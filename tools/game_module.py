#!/usr/bin/env python3
"""Build the wasm32 game module from the recovered C (docs/runtime.md).

Every unit the matched PS1 links compiled (the FILE symbols of their ELFs),
unchanged, plus the port layer in port/, is compiled to LLVM IR by clang's MIPS
front end, retargeted to wasm32 and rewritten so that the module keeps the
original memory layout:

- a global the C defines or declares resolves to its original address, read
  from the matched links (locals by their unit); its bytes come from the images
  the game loads, so the C's initializers are dropped;
- a function's address, wherever C takes it, is its original address;
- an indirect call goes through a dispatcher per wasm signature that selects the
  function by address and checks a fingerprint of its original first code
  bytes in game memory (an overlay function shares its address with other
  images' functions), trapping on an absent or overwritten image;
- an inline assembly statement becomes a call to the port function that
  port/asm_map*.json gives for its template and constraints;
- a direct call whose argument or result types differ from the definition (an
  unprototyped call) goes through an adapter that truncates or extends integer
  arguments, passes 0 for a missing one and drops an extra one; adapters.txt
  lists each, since a missing argument read a leftover register on the PS1.

The rewritten units are compiled with -O2, linked by wasm-ld and instrumented
by wasm-opt --asyncify at the yield imports. The decomp itself is not
changed. Run inside nix/runtime after `make -C decomp all-verify`.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGETS = ROOT / "decomp" / "targets"
# The development images load at 0x80280000, outside retail RAM, and only when
# the boot word enables the development paths; Disc 2's executable shares
# Disc 1's code and addresses (only the embedded disc index differs).
SKIPPED_TARGETS = {"debug595", "debug2611", "slus_006.69"}
PORT_DIR = ROOT / "port"
# Game memory: RAM at KSEG0 and the scratchpad keep their PS1 addresses.
MEMORY_BYTES = 0x80200000
# The port's shadow stack, then its own data, lie below the scratchpad.
STACK_BYTES = 0x100000
FINGERPRINT_BYTES = 32
# Host imports that suspend the game or abandon its stack (asyncify unwinds
# through them).
YIELD_IMPORTS = ["xem.yield", "xem.restart"]
# The module's exports, defined in port/.
EXPORTS = ["xem_run", "xem_call", "xem_unwind_area"]
HOST_PREFIX = "xem_host_"
# Game functions the port wraps (decomp/port): the game's definition is
# renamed xem_original_<name> and the port's definition calls it.
WRAPPED = {"mode_dispatch"}

CFLAGS = [
    # The MIPS front end accepts the original inline assembly; the IR is then
    # retargeted to wasm32, whose data layout (ILP32, little-endian, the same
    # ABI alignments) gives every structure the same layout.
    "--target=mipsel-unknown-unknown", "-std=gnu89", "-undef", "-nostdinc", "-ffreestanding",
    "-include", "port/include/xem/prelude.h", "-Iport/include", "-Idecomp/include",
    "-Dmips", "-D__mips__", "-D__mips", "-Dpsx", "-D__psx__", "-D__psx", "-D_PSYQ", "-D_MIPSEL",
    "-D__CHAR_UNSIGNED__", "-D_LANGUAGE_C", "-DLANGUAGE_C",
    "-funsigned-char", "-fwrapv", "-fno-strict-aliasing", "-fno-builtin", "-w",
    # `return;` in a non-void function (docs/later-phases.md, Portability
    # hazards: values left in $v0) compiles; its callers are audited separately.
    "-Wno-return-mismatch",
]

WASM_LAYOUT = 'target datalayout = "e-m:e-p:32:32-p10:8:8-p20:8:8-i64:64-i128:128-n32:64-S128-ni:1:10:20"'
WASM_TRIPLE = 'target triple = "wasm32-unknown-unknown"'
ASM_RE = re.compile(r'^(\s*(?:(%[-\w.]+) = )?)(?:tail )?call (\S+|\{[^}]*\}) asm (?:sideeffect )?(?:alignstack )?"((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)"\((.*)\)((?: #\d+)?)(?:, !srcloc !\d+)?$')
NAME = r'@(?:"(?:[^"\\]|\\.)*"|[-\w.$]+)'
NAME_RE = re.compile(NAME)
GLOBAL_DEF_RE = re.compile(r"^(" + NAME + r") = (.*?)\b(global|constant) (.*)$")
FUNC_HEAD_RE = re.compile(r"^(define|declare) (.*?)(" + NAME + r")\((.*)$")
CALL_RE = re.compile(
    r"^(\s*(?:%[-\w.]+ = )?)((?:tail |musttail |notail )?call )((?:(?:noundef|signext|zeroext|inreg|fastcc|ccc) )*)"
    r"(\S+) (\([^()]*\) )?(%[-\w.]+|inttoptr \(i32 -?\d+ to ptr\)|" + NAME + r")\((.*)\)((?: #\d+)?(?:, !.*)?)$"
)
TYPE_ATTRS = {"noundef", "signext", "zeroext", "inreg", "nonnull", "noalias", "nocapture", "readonly", "writeonly", "returned", "immarg"}


def name_of(token):
    """The symbol name of an IR global token (quotes and asm-label \\01 stripped)."""
    name = token[1:]
    if name.startswith('"'):
        name = name[1:-1]
        if name.startswith("\\01"):
            name = name[3:]
    return name


def run(cmd, **kw):
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, **kw)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"failed: {' '.join(map(str, cmd))}")
    return result


# ---------------------------------------------------------------- matched links

def read_targets():
    targets = []
    for mk in sorted(TARGETS.glob("*/*.mk")):
        name = mk.stem
        if name in SKIPPED_TARGETS:
            continue
        text = mk.read_text()
        image = re.search(r"^IMAGE\s*:=\s*(\S+)", text, re.M).group(1)
        targets.append((name, ROOT / (image + ".elf")))
    return targets


def elf_symbols(path):
    """Sections and symbols of a 32-bit little-endian ELF, locals scoped by FILE."""
    data = path.read_bytes()
    shoff, = struct.unpack_from("<I", data, 0x20)
    shentsize, shnum, _ = struct.unpack_from("<HHH", data, 0x2E)
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    symbols = []
    unit = None
    for sec in sections:
        if sec[1] != 2:
            continue
        strtab = sections[sec[6]]
        for off in range(sec[4], sec[4] + sec[5], 16):
            name, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", data, off)
            start = strtab[4] + name
            text = data[start:data.index(b"\0", start)].decode()
            bind, kind = info >> 4, info & 15
            if kind == 4:
                unit = os.path.relpath(text, ROOT) if text.startswith("/") else text
                continue
            if not text or kind == 3 or shndx == 0:
                continue
            symbols.append((text, value, size, bind, kind, shndx, unit if bind == 0 else None))
    return data, sections, symbols


def section_bytes(data, sections, address, length):
    for sec in sections:
        addr, off, size, kind = sec[3], sec[4], sec[5], sec[1]
        if kind == 1 and addr <= address < addr + size:
            end = min(address + length, addr + size)
            return data[off + address - addr: off + end - addr]
    return b""


def collect_symbols(out):
    """The original address of every symbol of every image, and fingerprints."""
    images = {}
    for target, elf in read_targets():
        if not elf.exists():
            raise SystemExit(f"{elf} is missing: run make -C decomp all-verify first")
        data, sections, symbols = elf_symbols(elf)
        exec_sections = {i for i, s in enumerate(sections) if s[2] & 4}
        globals_, locals_, units = {}, {}, set()
        code_starts = sorted({v for _, v, _, _, _, shndx, _ in symbols if shndx in exec_sections})
        functions = {}
        for name, value, size, bind, kind, shndx, unit in symbols:
            if unit:
                units.add(unit)
            is_code = shndx in exec_sections and not name.startswith(("gcc2_compiled", "__gnu_compiled"))
            if bind == 0:
                locals_.setdefault(unit, {})[name] = value
            else:
                globals_[name] = value
            if is_code:
                # The fingerprint covers the function's first bytes, up to the
                # next code symbol.
                index = code_starts.index(value)
                limit = code_starts[index + 1] - value if index + 1 < len(code_starts) else FINGERPRINT_BYTES
                code = section_bytes(data, sections, value, min(FINGERPRINT_BYTES, limit))
                key = name if bind else f"{unit}:{name}"
                functions[key] = [value, len(code), fnv1a64(code)]
        images[target] = {"globals": globals_, "locals": {k: v for k, v in locals_.items() if k},
                          "functions": functions, "units": sorted(u for u in units if u.endswith(".c"))}
    (out / "symbols.json").write_text(json.dumps(images, indent=1, sort_keys=True))
    return images


def fnv1a64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


class Addresses:
    """Original addresses by global name and by (unit, local name)."""

    def __init__(self, images):
        self.globals, self.locals, self.functions = {}, {}, {}
        self.unit_image = {}
        for image, info in images.items():
            for name, value in info["globals"].items():
                previous = self.globals.get(name)
                if previous is not None and previous != value:
                    raise SystemExit(f"{name} has two addresses ({previous:#x}, {value:#x})")
                self.globals[name] = value
            for unit, names in info["locals"].items():
                self.locals.setdefault(unit, {}).update(names)
            for unit in info["units"]:
                self.unit_image[unit] = image
            for key, value in info["functions"].items():
                self.functions[key] = (image, *value)

    def data(self, unit, name, internal):
        if internal:
            local = self.locals.get(unit, {})
            if name in local:
                return local[name]
            # A function-local static: clang names it function.variable, GCC
            # variable.N in the same unit.
            base = name.rsplit(".", 1)[-1]
            candidates = [v for k, v in local.items() if re.fullmatch(re.escape(base) + r"\.\d+", k)]
            if len(candidates) == 1:
                return candidates[0]
            # A static whose address the target's symbol file defines (an
            # .lcomm object keeps no local symbol).
        return self.globals.get(name)

    def function(self, unit, name, internal):
        if internal:
            entry = self.functions.get(f"{unit}:{name}")
            return entry
        return self.functions.get(name)


# ---------------------------------------------------------------- IR handling

def split_top(text):
    """Split on commas outside parentheses, brackets and braces."""
    parts, depth, start = [], 0, 0
    for i, c in enumerate(text):
        if c in "([{<":
            depth += 1
        elif c in ")]}>":
            depth -= 1
        elif c == "," and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    tail = text[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def first_type(text):
    """The leading type of an argument or parameter, and the rest."""
    text = text.strip()
    if text[0] in "{[<":
        close = {"{": "}", "[": "]", "<": ">"}[text[0]]
        depth = 0
        for i, c in enumerate(text):
            if c == text[0]:
                depth += 1
            elif c == close:
                depth -= 1
                if depth == 0:
                    return text[: i + 1], text[i + 1:].strip()
    head, _, rest = text.partition(" ")
    return head, rest


def wasm_type(ir_type):
    if ir_type in ("i1", "i8", "i16", "i32", "ptr"):
        return "i32"
    if ir_type == "i64":
        return "i64"
    if ir_type == "float":
        return "f32"
    if ir_type == "double":
        return "f64"
    if ir_type == "void":
        return "void"
    raise ValueError(f"no wasm type for {ir_type}")


def param_types(params_text):
    """IR parameter list text (without the closing parenthesis) -> types and attrs."""
    if params_text.strip() in ("", "..."):
        return [], params_text.strip() == "..."
    out, variadic = [], False
    for part in split_top(params_text):
        if part == "...":
            variadic = True
            continue
        ty, rest = first_type(part)
        attrs = [a for a in rest.split() if a in ("signext", "zeroext")]
        out.append((ty, attrs))
    return out, variadic


def parse_function_head(line):
    """(kind, name, ret type, ret attrs, params, variadic, linkage words) of a define/declare."""
    m = FUNC_HEAD_RE.match(line)
    kind, before, token, after = m.groups()
    depth, end = 1, None
    for i, c in enumerate(after):
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                end = i
                break
    params, variadic = param_types(after[:end])
    words = before.split()
    ret = words[-1]
    ret_attrs = [w for w in words[:-1] if w in ("signext", "zeroext")]
    return kind, token, ret, ret_attrs, params, variadic, words[:-1]


def signature_key(ret, params):
    return wasm_type(ret) + "(" + ",".join(wasm_type(t) for t, _ in params) + ")"


def key_name(key):
    return key.replace("(", "_").replace(")", "").replace(",", "")


class Unit:
    def __init__(self, source, ll_path, image):
        self.source, self.ll_path, self.image = source, ll_path, image
        self.lines = ll_path.read_text().split("\n")
        self.defined = {}   # name -> head
        self.declared = {}  # name -> head
        self.internal_functions = set()
        for line in self.lines:
            if line.startswith(("define ", "declare ")):
                head = parse_function_head(line)
                name = name_of(head[1])
                if head[0] == "define" and image != "port" and name in WRAPPED:
                    name = "xem_original_" + name
                    head = (head[0], "@" + name, *head[2:])
                (self.defined if head[0] == "define" else self.declared)[name] = head
                if head[0] == "define" and "internal" in head[6]:
                    self.internal_functions.add(name)


def cast_value(value, from_type, to_type, signed):
    """IR instructions converting `value` between integer/pointer types."""
    if from_type == to_type:
        return [], value
    tmp = f"%xem.cast{cast_value.counter}"
    cast_value.counter += 1
    ints = ("i1", "i8", "i16", "i32", "i64")
    if from_type == "ptr" and to_type in ints:
        return [f"  {tmp} = ptrtoint ptr {value} to {to_type}"], tmp
    if to_type == "ptr" and from_type in ints:
        return [f"  {tmp} = inttoptr {from_type} {value} to ptr"], tmp
    fw, tw = int(from_type[1:]), int(to_type[1:])
    op = "trunc" if fw > tw else ("sext" if signed else "zext")
    return [f"  {tmp} = {op} {from_type} {value} to {to_type}"], tmp


cast_value.counter = 0


def zero_of(ty):
    return "null" if ty == "ptr" else ("0.0" if ty in ("float", "double") else "0")


def adapter_body(target, call_ret, call_params, def_ret, def_ret_attrs, def_params):
    """Body lines calling `target` (defined with def types) from params %a0.. of call types."""
    lines, args = [], []
    for i, (ty, attrs) in enumerate(def_params):
        if i < len(call_params):
            src_ty = call_params[i][0]
            pre, value = cast_value(f"%a{i}", src_ty, ty, "signext" in attrs or "signext" in call_params[i][1])
            lines += pre
            args.append(f"{ty} {' '.join(attrs) + ' ' if attrs else ''}{value}")
        else:
            args.append(f"{ty} {zero_of(ty)}")
    call = f"call {def_ret} {target}({', '.join(args)})"
    if def_ret == "void":
        lines.append(f"  {call}")
        lines.append("  ret void" if call_ret == "void" else f"  ret {call_ret} {zero_of(call_ret)}")
    else:
        result = f"%xem.r{cast_value.counter}"
        cast_value.counter += 1
        lines.append(f"  {result} = {call}")
        if call_ret == "void":
            lines.append("  ret void")
        else:
            pre, value = cast_value(result, def_ret, call_ret, "signext" in def_ret_attrs)
            lines += pre
            lines.append(f"  ret {call_ret} {value}")
    return lines


def const_address(value):
    signed = value - (1 << 32) if value >= 1 << 31 else value
    return f"inttoptr (i32 {signed} to ptr)"


class Rewriter:
    def __init__(self, addresses, units):
        self.addresses = addresses
        self.units = units
        # Every function definition in the module, by its global or unit-scoped name.
        self.definitions = {}
        for unit in units:
            for name, head in unit.defined.items():
                key = f"{unit.source}:{name}" if name in unit.internal_functions else name
                self.definitions[key] = (unit, head)
        self.taken = {}        # definition key -> address
        self.dispatch_keys = set()
        self.adapters = {}     # adapter name -> (target token, call ret, call params, def head)
        self.missing_data = []
        self.unit_decls = {}   # unit -> declarations of the dispatchers and adapters it calls
        self.asm_map = {}
        for path in sorted(PORT_DIR.glob("asm_map*.json")):
            for entry in json.loads(path.read_text()):
                self.asm_map[(entry["template"], entry["constraints"])] = entry["function"]
        self.unmapped_asm, self.unmapped_users = {}, {}

    def function_entry(self, unit, name):
        internal = name in unit.internal_functions
        entry = self.addresses.function(unit.source, name, internal)
        return entry

    def rewrite(self, unit):
        is_port = unit.image == "port"
        replacements = {}
        kept = set()
        out = []
        # Globals: drop every definition or declaration with an original address.
        for line in unit.lines:
            m = GLOBAL_DEF_RE.match(line)
            if m and not line.startswith(("@.", "@__const.", '@".')):
                name = name_of(m.group(1))
                words = m.group(2).split()
                internal = "internal" in words or "private" in words
                external = "external" in words
                address = self.addresses.data(unit.source, name, internal and not is_port)
                if address is None and not internal and not is_port and not external:
                    self.missing_data.append(f"{unit.source}: {name}")
                if address is not None:
                    replacements[m.group(1)] = const_address(address)
                    # Function addresses stored in the dropped initializer are taken.
                    for token in NAME_RE.findall(m.group(4)):
                        self.take(unit, name_of(token))
                    continue
                kept.add(m.group(1))
            out.append(line)
        lines, host_groups = [], []
        for line in out:
            if line.startswith("declare ") and is_port:
                name = name_of(parse_function_head(line)[1])
                if name.startswith(HOST_PREFIX):
                    # A host import: the attribute group follows the unit's own.
                    import_name = name[len(HOST_PREFIX):]
                    line = re.sub(r" #\d+$", "", line) + f" #{900 + len(host_groups)}"
                    host_groups.append(f'attributes #{900 + len(host_groups)} = {{ "wasm-import-module"="xem" '
                                       f'"wasm-import-name"="{import_name}" }}')
                    lines.append(line)
                    continue
            if line.startswith(("define ", "declare ")):
                line = self.rename_static_head(unit, line)
                if line.startswith("define ") and not is_port:
                    name = name_of(parse_function_head(line)[1])
                    if name in WRAPPED:
                        line = line.replace("@" + name + "(", "@xem_original_" + name + "(", 1)
                        # The unit's own calls reach the port's definition.
                        head = parse_function_head(line)
                        self.declare(unit, name, head[2], [t for t, _ in head[4]])
                lines.append(line)
                continue
            if line.startswith("target datalayout"):
                lines.append(WASM_LAYOUT)
                continue
            if line.startswith("target triple"):
                lines.append(WASM_TRIPLE)
                continue
            if line.startswith("attributes "):
                lines.append(re.sub(r' "target-(cpu|features)"="[^"]*"', "", line))
                continue
            if line.startswith(("!", "source_filename")):
                lines.append(line)
                continue
            if " asm " in line:
                asm = self.rewrite_asm(unit, line)
                if asm is not None:
                    lines.extend(asm)
                    continue
            line = self.substitute(unit, line, replacements, kept)
            lines.extend(self.rewrite_call(unit, line))
        lines += host_groups
        # Declarations of the dispatchers and adapters this unit calls.
        for name, decl in sorted(self.unit_decls.pop(unit.source, {}).items()):
            lines.append(decl)
        text = "\n".join(lines)
        for name, mangled in self.static_renames(unit).items():
            text = re.sub(re.escape(name) + r"(?=[\s(),])", mangled, text)
        return text

    def static_renames(self, unit):
        renames = {}
        for name in unit.internal_functions:
            key = f"{unit.source}:{name}"
            if key in self.taken:
                renames["@" + name] = "@" + self.mangle(unit, name)
        return renames

    def mangle(self, unit, name):
        return '"xem.static.' + unit.source.replace("/", ".") + "." + name + '"'

    def rename_static_head(self, unit, line):
        head = parse_function_head(line)
        name = name_of(head[1])
        if head[0] == "define" and name in unit.internal_functions and f"{unit.source}:{name}" in self.taken:
            # An address-taken static is reachable from the dispatchers.
            line = line.replace(" internal ", " hidden ", 1)
        return line

    def take(self, unit, name):
        """Record `name` as a function whose address the code or data takes."""
        key = self.definition_key(unit, name)
        if key is None:
            return None
        entry = self.addresses.function(unit.source, name, key.startswith(unit.source + ":")) \
            if key.startswith(unit.source + ":") else self.addresses.functions.get(name)
        if entry is None:
            return None
        self.taken[key] = entry
        return entry

    def definition_key(self, unit, name):
        if name in unit.internal_functions:
            return f"{unit.source}:{name}"
        if name in self.definitions:
            return name
        if name in self.addresses.functions:
            return name
        return None

    def substitute(self, unit, line, replacements, kept):
        def replace(m):
            token = m.group(0)
            if token in replacements:
                return replacements[token]
            if token in kept:
                return token
            name = name_of(token)
            # A direct call keeps the symbol: the call's callee is followed by '('.
            if line[m.end():m.end() + 1] == "(" and re.search(r"call [^@%]*" + re.escape(token) + r"\($", line[: m.end() + 1]):
                return token
            if name in unit.defined or name in unit.declared:
                entry = self.take(unit, name)
                if entry is not None:
                    return const_address(entry[1])
            data = self.addresses.data(unit.source, name, False)
            if data is not None and not (name in unit.defined or name in unit.declared):
                return const_address(data)
            return token
        return NAME_RE.sub(replace, line)

    def rewrite_call(self, unit, line):
        m = CALL_RE.match(line)
        if not m:
            return [line]
        lhs, call_kw, ret_attrs, ret, fnty, callee, args_text, tail = m.groups()
        args = split_top(args_text) if args_text.strip() else []
        arg_types = []
        for a in args:
            ty, rest = first_type(a)
            arg_types.append((ty, [w for w in rest.split() if w in ("signext", "zeroext")]))
        if fnty:
            # A variadic call: the dispatchers take fixed arguments only.
            if "..." in fnty:
                if callee.startswith("@"):
                    return [line]
                raise SystemExit(f"{unit.source}: variadic indirect call: {line.strip()}")
        if callee.startswith("@"):
            name = name_of(callee)
            target = self.lookup_definition(unit, name)
            if target is None:
                return [line]
            tunit, head = target
            _, _, def_ret, def_ret_attrs, def_params, def_variadic, _ = head
            if def_variadic:
                return [line]
            call_key = signature_key(ret, arg_types)
            if call_key == signature_key(def_ret, def_params) and len(arg_types) == len(def_params) \
                    and all(a[0] == p[0] for a, p in zip(arg_types, def_params)) and ret == def_ret:
                return [line]
            adapter = f"xem.adapt.{name}.{key_name(call_key)}"
            self.adapters[adapter] = (callee if tunit is unit else "@" + name, ret, arg_types, head, unit.source)
            self.declare(unit, adapter, ret, [t for t, _ in arg_types])
            return [f"{lhs}{call_kw}{ret_attrs}{ret} @\"{adapter}\"({args_text}){tail}"]
        key = signature_key(ret, arg_types)
        self.dispatch_keys.add((key, ret, tuple(t for t, _ in arg_types)))
        self.declare(unit, f"xem.icall.{key_name(key)}.{dispatch_variant(ret, arg_types)}", ret,
                     ["ptr"] + [t for t, _ in arg_types])
        callee_arg = f"ptr {callee}"
        new_args = callee_arg + (", " + args_text if args_text.strip() else "")
        return [f"{lhs}{call_kw}{ret_attrs}{ret} @\"xem.icall.{key_name(key)}.{dispatch_variant(ret, arg_types)}\"({new_args}){tail}"]

    def rewrite_asm(self, unit, line):
        """An inline assembly call as a call to its port function (port/asm_map*.json)."""
        m = ASM_RE.match(line)
        if not m:
            return None
        lhs, result, ret, template, constraints, args_text, tail = m.groups()
        outputs = [c for c in constraints.split(",") if c.startswith("=")]
        function = self.asm_map.get((template, constraints))
        if function is None:
            number = self.unmapped_asm.setdefault((template, constraints), len(self.unmapped_asm))
            self.unmapped_users.setdefault((template, constraints), set()).add(unit.source)
            self.declare(unit, "xem_unmapped_asm", "void", ["i32"])
            out = [f'  call void @"xem_unmapped_asm"(i32 {number})']
            if result:
                out.append(f"  {lhs}{'zeroinitializer' if ret.startswith('{') else '0'}"
                           .replace(" = zeroinitializer", f" = select i1 true, {ret} zeroinitializer, {ret} zeroinitializer")
                           .replace(" = 0", f" = add {ret} 0, 0"))
            return out
        args = split_top(args_text) if args_text.strip() else []
        types = [first_type(a)[0] for a in args]
        if len(outputs) <= 1:
            self.declare(unit, function, ret, types)
            return [f'{lhs}call {ret} @"{function}"({args_text}){tail}']
        n = len(outputs)
        tmp = f"%xem.asm{cast_value.counter}"
        cast_value.counter += 1
        self.declare(unit, function, "void", ["ptr"] + types)
        out = [f"  {tmp} = alloca [{n} x i32], align 4",
               f'  call void @"{function}"(ptr {tmp}{", " + args_text if args_text.strip() else ""}){tail}']
        value = "undef"
        for i in range(n):
            out.append(f"  {tmp}.p{i} = getelementptr [{n} x i32], ptr {tmp}, i32 0, i32 {i}")
            out.append(f"  {tmp}.v{i} = load i32, ptr {tmp}.p{i}, align 4")
            target = f"%{result}" if i == n - 1 else f"{tmp}.s{i}"
            if result and not result.startswith("%"):
                target = f"%{result}" if i == n - 1 else target
            out.append(f"  {target if i < n - 1 else '%' + result.lstrip('%')} = insertvalue {ret} {value}, i32 {tmp}.v{i}, {i}")
            value = f"{tmp}.s{i}" if i < n - 1 else value
        return out

    def declare(self, unit, name, ret, types):
        self.unit_decls.setdefault(unit.source, {})[name] = f'declare {ret} @"{name}"({", ".join(types)})'

    def lookup_definition(self, unit, name):
        if name in unit.internal_functions:
            return unit, unit.defined[name]
        found = self.definitions.get(name)
        return found


def dispatch_variant(ret, arg_types):
    """Distinguish IR-level signatures sharing a wasm signature (i8 vs i32)."""
    text = ret + "(" + ",".join(t for t, _ in arg_types) + ")"
    return hashlib.sha1(text.encode()).hexdigest()[:8]


def generate_dispatchers(rewriter, addresses):
    """IR for the dispatchers and adapters."""
    lines = ['target datalayout = "e-m:e-p:32:32-p10:8:8-p20:8:8-i64:64-i128:128-n32:64-S128-ni:1:10:20"',
             'target triple = "wasm32-unknown-unknown"', "",
             "declare i32 @xem_code_matches(i32, i32, i64)",
             "declare void @xem_bad_call(i32, i32) noreturn", ""]
    declared = set()
    candidates = {}
    for key, entry in rewriter.taken.items():
        image, address, length, fingerprint = entry
        found = rewriter.definitions.get(key)
        if found is None:
            continue
        unit, head = found
        _, token, def_ret, def_ret_attrs, def_params, variadic, _ = head
        if variadic:
            continue
        name = name_of(token)
        target = "@" + (rewriter.mangle(unit, name) if ":" in key else name)
        candidates.setdefault(signature_key(def_ret, def_params), []).append(
            (address, length, fingerprint, target, def_ret, def_ret_attrs, def_params, image))
    sharers = {}
    for items in candidates.values():
        for item in items:
            sharers.setdefault(item[0], set()).add(item[3])
    for number, (key, ret, arg_tys) in enumerate(sorted(rewriter.dispatch_keys)):
        call_params = [(t, []) for t in arg_tys]
        variant = dispatch_variant(ret, call_params)
        params = ", ".join(["ptr %f"] + [f"{t} %a{i}" for i, t in enumerate(arg_tys)])
        lines.append(f'define hidden {ret} @"xem.icall.{key_name(key)}.{variant}"({params}) {{')
        lines.append("entry:")
        lines.append("  %addr = ptrtoint ptr %f to i32")
        items = sorted(candidates.get(key, []))
        by_address = {}
        for item in items:
            by_address.setdefault(item[0], []).append(item)
        cases = " ".join(f"i32 {a - (1 << 32) if a >= 1 << 31 else a}, label %at{a:x}" for a in by_address)
        lines.append(f"  switch i32 %addr, label %miss [ {cases} ]")
        for address, group in by_address.items():
            lines.append(f"at{address:x}:")
            for index, (_, length, fingerprint, target, def_ret, def_ret_attrs, def_params, image) in enumerate(group):
                if index:
                    lines.append(f"c{address:x}_{index}:")
                label = f"b{address:x}_{index}"
                nxt = f"c{address:x}_{index + 1}" if index + 1 < len(group) else "miss"
                always = len(sharers[address]) == 1 and image == "slus_006.64"
                if always:
                    lines.append(f"  br label %{label}")
                else:
                    fp = fingerprint - (1 << 64) if fingerprint >= 1 << 63 else fingerprint
                    lines.append(f"  %m{label} = call i32 @xem_code_matches(i32 %addr, i32 {length}, i64 {fp})")
                    lines.append(f"  %t{label} = icmp ne i32 %m{label}, 0")
                    lines.append(f"  br i1 %t{label}, label %{label}, label %{nxt}")
                lines.append(f"{label}:")
                lines += adapter_body(target, ret, call_params, def_ret, def_ret_attrs, def_params)
        lines.append("miss:")
        lines.append(f"  call void @xem_bad_call(i32 %addr, i32 {number})")
        lines.append("  unreachable")
        lines.append("}")
        lines.append("")
        for item in items:
            declared.add((item[3], item[4], tuple(t for t, _ in item[6])))
    report = []
    for adapter, (target, call_ret, call_params, head, source) in sorted(rewriter.adapters.items()):
        _, token, def_ret, def_ret_attrs, def_params, _, _ = head
        params = ", ".join(f"{t} %a{i}" for i, (t, _) in enumerate(call_params))
        lines.append(f'define hidden {call_ret} @"{adapter}"({params}) {{')
        lines.append("entry:")
        lines += adapter_body(target, call_ret, call_params, def_ret, def_ret_attrs, def_params)
        lines.append("}")
        declared.add((target, def_ret, tuple(t for t, _ in def_params)))
        report.append(f"{source}: {name_of(token)} called as {call_ret}({', '.join(t for t, _ in call_params)}),"
                      f" defined {def_ret}({', '.join(t for t, _ in def_params)})")
    for target, def_ret, def_params in sorted(declared, key=str):
        lines.append(f"declare {def_ret} {target}({', '.join(def_params)})")
    return "\n".join(lines) + "\n", report


# ---------------------------------------------------------------- build

def compile_ir(source, out_ll):
    """Compile one unit to IR; the compiler's errors, or None."""
    result = subprocess.run([os.environ["XEM_CLANG"], *CFLAGS, "-S", "-emit-llvm", "-O0", "-Xclang",
                             "-disable-O0-optnone", "-ferror-limit=0", str(source), "-o", str(out_ll)],
                            cwd=ROOT, capture_output=True, text=True)
    return result.stderr if result.returncode else None


def compile_object(ll, obj):
    """Compile rewritten IR to an object; the compiler's errors, or None."""
    result = subprocess.run([os.environ["XEM_CLANG"], "--target=wasm32-unknown-unknown", "-O2", "-c", str(ll),
                             "-o", str(obj), "-w"], cwd=ROOT, capture_output=True, text=True)
    return result.stderr if result.returncode else None


def build(args):
    out = (ROOT / args.out).resolve()
    (out / "ir").mkdir(parents=True, exist_ok=True)
    (out / "obj").mkdir(parents=True, exist_ok=True)
    images = collect_symbols(out)
    addresses = Addresses(images)
    sources = []
    for image, info in images.items():
        for unit in info["units"]:
            sources.append((unit, image))
    for path in sorted(PORT_DIR.glob("*.c")):
        sources.append((str(path.relative_to(ROOT)), "port"))
    seen = set()
    sources = [s for s in sources if not (s[0] in seen or seen.add(s[0]))]

    def ll_of(source):
        return out / "ir" / (source.replace("/", ".") + ".ll")

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        errors = list(pool.map(lambda s: compile_ir(ROOT / s[0], ll_of(s[0])), sources))
    failed = [(s, e) for s, e in zip(sources, errors) if e]
    if failed:
        log = out / "compile-errors.txt"
        log.write_text("".join(f"== {s[0]}\n{e}" for s, e in failed))
        print(f"{len(failed)} units failed to compile: {log}")
        if not args.stubs:
            raise SystemExit(1)
        sources = [s for s, e in zip(sources, errors) if not e]
    units = [Unit(source, ll_of(source), image) for source, image in sources]
    rewriter = Rewriter(addresses, units)
    rewritten = {}
    for unit in units:
        rewritten[unit.source] = rewriter.rewrite(unit)
    # Second pass: a static's address may be taken after its unit was rewritten.
    for unit in units:
        if rewriter.static_renames(unit):
            rewritten[unit.source] = rewriter.rewrite(unit)
    if rewriter.unmapped_asm:
        report = out / "unmapped-asm.txt"
        report.write_text("".join(
            f"{n}\t{t}\t{c}\t{' '.join(sorted(rewriter.unmapped_users[(t, c)]))}\n"
            for (t, c), n in sorted(rewriter.unmapped_asm.items(), key=lambda x: x[1])))
        print(f"{len(rewriter.unmapped_asm)} inline assembly statements have no port function: {report}")
        if not args.stubs:
            raise SystemExit(1)
    if rewriter.missing_data:
        (out / "missing-data.txt").write_text("\n".join(rewriter.missing_data) + "\n")
        print(f"{len(rewriter.missing_data)} defined globals without an original address: {out / 'missing-data.txt'}")
    ll_files = []
    for unit in units:
        path = out / "ir" / (unit.source.replace("/", ".") + ".x.ll")
        path.write_text(rewritten[unit.source])
        ll_files.append(path)
    dispatch, adapters = generate_dispatchers(rewriter, addresses)
    (out / "ir" / "dispatch.ll").write_text(dispatch)
    (out / "adapters.txt").write_text("\n".join(adapters) + "\n")
    ll_files.append(out / "ir" / "dispatch.ll")
    objects = [out / "obj" / (p.stem + ".o") for p in ll_files]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        errors = list(pool.map(lambda p: compile_object(*p), zip(ll_files, objects)))
    failed = [(p, e) for p, e in zip(ll_files, errors) if e]
    if failed:
        log = out / "codegen-errors.txt"
        log.write_text("".join(f"== {p.name}\n{e}" for p, e in failed))
        print(f"{len(failed)} units failed code generation: {log}")
        if not args.stubs:
            raise SystemExit(1)
        objects = [o for o, e in zip(objects, errors) if not e]
    link(out, objects, args, units)


def stub_ir(missing, units):
    """IR defining each missing function as a call to the host's xem_missing."""
    heads = {}
    for unit in units:
        for name, head in unit.declared.items():
            heads.setdefault(name, head)
    lines = ['target triple = "wasm32-unknown-unknown"', "",
             'declare void @xem_host_missing(i32) #0', 'attributes #0 = { "wasm-import-module"="xem" "wasm-import-name"="missing" }', ""]
    for number, name in enumerate(missing):
        head = heads.get(name)
        if head is None:
            lines.append(f"define hidden void @{name}() {{\n  call void @xem_host_missing(i32 {number})\n  unreachable\n}}")
            continue
        _, token, ret, _, params, variadic, _ = head
        plist = ", ".join(t for t, _ in params) + (", ..." if variadic and params else ("..." if variadic else ""))
        lines.append(f"define hidden {ret} {token}({plist}) {{\n  call void @xem_host_missing(i32 {number})\n  unreachable\n}}")
    return "\n".join(lines) + "\n"


def link(out, objects, args, units):
    raw = out / "game.raw.wasm"
    cmd = [os.environ["XEM_WASM_LD"], "--no-entry", "--error-limit=0",
           f"--initial-memory={MEMORY_BYTES}", f"--max-memory={MEMORY_BYTES}",
           "-z", f"stack-size={STACK_BYTES}", "--stack-first",
           "--export=__stack_pointer", *(f"--export={name}" for name in EXPORTS), "-o", str(raw)]
    if args.stubs:
        # wasm-ld reports some undefined symbols only once others resolve.
        missing = set()
        defined = {name for unit in units for name in unit.defined}
        stub_ll, stub_obj = out / "ir" / "stubs.ll", out / "obj" / "stubs.o"
        for _ in range(8):
            extra = [stub_obj] if missing else []
            result = subprocess.run(cmd + list(map(str, objects + extra)), cwd=ROOT, capture_output=True, text=True)
            found = set(re.findall(r"undefined symbol: (\S+)", result.stderr))
            # References that disagree on the signature of a function nothing defines.
            found |= {n for n in re.findall(r"function signature mismatch: (\S+)", result.stderr) if n not in defined}
            if found <= missing:
                break
            missing |= found
            stub_ll.write_text(stub_ir(sorted(missing), units))
            error = compile_object(stub_ll, stub_obj)
            if error:
                raise SystemExit(error)
        (out / "stubs.txt").write_text("".join(f"{i} {name}\n" for i, name in enumerate(sorted(missing))))
        if missing:
            print(f"{len(missing)} undefined functions trap through xem.missing: {out / 'stubs.txt'}")
            objects = objects + [stub_obj]
    run(cmd + list(map(str, objects)))
    final = out / "game.wasm"
    imports = ",".join(YIELD_IMPORTS)
    run(["wasm-opt", "-O2", "--asyncify", f"--pass-arg=asyncify-imports@{imports}",
         "--enable-bulk-memory", "--enable-sign-ext", "--enable-mutable-globals", "--enable-nontrapping-float-to-int",
         str(raw), "-o", str(final)])
    print(f"wrote {final} ({final.stat().st_size} bytes)")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default="build/game")
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    parser.add_argument("--stubs", action="store_true",
                        help="define undefined functions as traps into the host (xem.missing) and list them")
    build(parser.parse_args())


if __name__ == "__main__":
    main()
