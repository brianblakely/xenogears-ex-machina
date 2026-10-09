#!/usr/bin/env python3
"""Initialized C data that may spell stray assembler padding as elements
(docs/matching.md, Recovering data).

    stray_padding.py [CONFIG.mk ...]     # default: every target

Run inside the matching Nix shell after `make -C decomp all-verify`. The
original assembler left stray bytes in some alignment fill; C cannot emit
them, so an object whose fill holds them is linked with INCLUDE_ORIGINAL. A C
initializer can still spell such bytes as invented trailing elements. This
lists every linked C data object of the targets (each C unit's .data, .rodata
and .sdata input section in the link map) that could hide them, for review
against its readers:

* each unit's GAS input (``<unit>.o.s``) is reassembled with ``-L``, so every
  label has its offset, string literals included; the directives cc1 emitted
  after a label give the object's declared bytes and elements (the labels of
  INCLUDE_* statements are not C and are skipped);
* the object's definition in its unit's source folder gives the size R of its
  outermost elements, printed and used for the ``outlier`` note: one more
  initializer element adds R bytes (a scalar or struct object has none;
  without a definition, the directive width);
* the code of every target that can be resident beside the object's own is
  scanned for the addresses its instructions form (``lui`` bases resolved by
  ``addiu``/``ori`` or a load/store offset): an access is ``exact`` at a
  constant address, ``indexed`` through an ``addu`` with the base, or
  ``formed`` (an address computed, not loaded); the data words of those
  targets that point into the object are counted too.

Flags (an object can carry several):

  tail    its last 1-3 bytes, whole byte or halfword elements as cc1 emitted
          them, whatever the declared shape (the last elements of a flat
          table, the end of a 2-D table's last row, a structure's last
          members), would be alignment fill if the object ended before them
          (the next object or input section starts at the first address at
          or after them aligned as it is) and hold a non-zero byte. Notes: ``read`` (an exact access reads
          them: not listed, counted as ``tail read``), ``unread`` (every access
          is exact and none reaches them), ``text`` (every non-zero byte is
          printable ASCII), ``outlier`` (a value outside the range of the
          object's other elements).
  unread  every access is exact and the unread rest of the object lies in an
          alignment slot with a non-zero byte (also for word elements).
  wide    every load or store is narrower than the declared elements and a
          byte no access touches is non-zero.
  unref   no code forms an address in the object and no data word points
          into it, and it holds a non-zero byte; ``slot``: it starts less than
          4 bytes before the next object.
  string  a string initializer holds bytes after its terminator.

The scan is a review list, not a gate: an index's range, a base formed
before the object (``D_x[i - 4]``) or a pointer kept elsewhere needs the
source, and ``text`` or ``outlier`` values are often real data.
"""

from __future__ import annotations

import bisect
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from matching_coverage import classification, map_sections, read_elf  # noqa: E402
from matching_diff import config, functions, load_offset  # noqa: E402
from service_calls import LOADS_STORES, VOLATILE  # noqa: E402

DATA = (".data", ".rodata", ".sdata")
GP = 0x80059170
WIDTH = {"lb": 1, "lbu": 1, "sb": 1, "lh": 2, "lhu": 2, "sh": 2}
SECTIONS = {".data": ".data", ".rdata": ".rodata", ".sdata": ".sdata", ".text": ".text",
            ".bss": ".bss", ".sbss": ".sbss"}
SIZES = {".byte": 1, ".half": 2, ".short": 2, ".hword": 2, ".word": 4, ".int": 4, ".long": 4}


def operands(text: str) -> list[str]:
    """Top-level comma-separated operands, string literals kept whole."""
    out, current, quoted, escaped = [], "", False, False
    for c in text:
        if quoted:
            current += c
            quoted = escaped or c != '"'
            escaped = not escaped and c == "\\"
        elif c == '"':
            quoted, current = True, current + c
        elif c == ",":
            out.append(current.strip())
            current = ""
        else:
            current += c
    return out + ([current.strip()] if current.strip() else [])


def string_bytes(literal: str) -> bytes:
    """The bytes of a GAS string literal."""
    body, out, i = literal.strip()[1:-1], bytearray(), 0
    while i < len(body):
        if body[i] != "\\":
            out.append(ord(body[i]))
            i += 1
            continue
        digits = re.match(r"[0-7]{1,3}", body[i + 1:])
        if digits:
            out.append(int(digits.group(), 8) & 0xFF)
            i += 1 + len(digits.group())
            continue
        hexa = re.match(r"x[0-9A-Fa-f]+", body[i + 1:])
        if hexa:
            out.append(int(hexa.group()[1:], 16) & 0xFF)
            i += 1 + len(hexa.group())
            continue
        out.append({"n": 10, "t": 9, "r": 13, "b": 8, "f": 12}.get(body[i + 1], ord(body[i + 1])))
        i += 2
    return bytes(out)


def c_objects(gas_input: str) -> dict[str, dict]:
    """The labels cc1 emitted in data sections: {label: {"elements": [width,
    ...] (0 for a .space byte), "strings": [bytes, ...]}}, without the labels
    of INCLUDE_* statements (an .incbin or .include follows them)."""
    objects: dict[str, dict] = {}
    section, previous, current = ".text", ".text", None
    for raw in gas_input.splitlines():
        line = raw.strip()
        if not line.startswith((".ascii", ".asciz", ".string")):
            line = line.split("#", 1)[0].strip()
        label = re.match(r"^([A-Za-z_$.][\w$.]*):\s*(.*)$", line)
        if label:
            current = None
            if section in DATA:
                current = {"elements": [], "strings": [], "included": False}
                objects[label.group(1)] = current
            line = label.group(2)
        if not line:
            continue
        op, rest = (line.split(None, 1) + [""])[:2]
        if op == ".section":
            previous, section, current = section, rest.split(",")[0].strip(), None
        elif op in SECTIONS:
            previous, section, current = section, SECTIONS[op], None
        elif op == ".previous":
            section, previous, current = previous, section, None
        elif op in (".align", ".balign"):
            current = None
        elif current is None:
            continue
        elif op in SIZES:
            current["elements"] += [SIZES[op]] * len(operands(rest))
        elif op in (".ascii", ".asciz", ".string"):
            for literal in operands(rest):
                data = string_bytes(literal) + (b"\0" if op != ".ascii" else b"")
                current["elements"] += [1] * len(data)
                current["strings"].append(data)
        elif op == ".space":
            current["elements"] += [0] * int(operands(rest)[0], 0)
        elif op in (".incbin", ".include"):
            current["included"] = True
    return {k: v for k, v in objects.items() if v["elements"] and not v["included"]}


def unit_labels(obj: Path, tmp: Path) -> dict[str, tuple[str, int]]:
    """{label: (section, offset)} of a C unit's object, reassembled from its
    GAS input with -L so that local labels keep their offsets."""
    flags = dict(line.split("=", 1) for line in obj.with_suffix(".cflags").read_text().splitlines()
                 if "=" in line and not line.startswith("/"))
    out = tmp / (obj.stem + ".o")
    command = ["psx-as", *flags["ASFLAGS"].split(), f"-G{flags['GP']}", "-L", "-o", str(out)]
    run = subprocess.run([*command, f"{obj}.s"], cwd=ROOT, capture_output=True, text=True)
    if run.returncode:
        raise SystemExit(f"{obj}.s: {run.stderr}")
    sections, symbols = read_elf(out)
    original = {s.name: s.data for s in read_elf(obj)[0]}
    for s in sections:
        if s.name in DATA and s.data != original.get(s.name):
            raise SystemExit(f"{obj}: the reassembled {s.name} differs from the object's")
    return {s.name: (sections[s.section].name, s.value) for s in symbols
            if s.name and 0 < s.section < len(sections) and s.kind != 3}


def outer_count(label: str, folder: Path, cache: dict) -> int | None:
    """The outermost element count of `label`'s definition in its unit's source
    folder: 0 for a scalar or struct object, None when no definition or count
    is found (an initializer's top-level elements count for `[]`)."""
    head = re.compile(r"^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w ]*?[\s*]+" + re.escape(label)
                      + r"\s*((?:\[[^\]]*\])*)\s*[=;]")
    for path in sorted(folder.glob("*.c")) + sorted(folder.glob("*.h")):
        lines = cache.setdefault(path, path.read_text(errors="replace").splitlines())
        for i, line in enumerate(lines):
            match = head.match(line.strip()) if label in line else None
            if not match:
                continue
            dims = re.findall(r"\[([^\]]*)\]", match.group(1))
            if not dims:
                return 0
            if dims[0].strip():
                return _product(dims[0])
            text = re.sub(r"/\*.*?\*/", " ", " ".join(lines[i:i + 600]))
            text = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)  # commas inside strings
            depth, count, item = 0, 0, False
            for c in text[text.find("{", text.index(label)):]:
                if c == "{":
                    depth += 1
                    item = item or depth == 2
                elif c == "}":
                    depth -= 1
                    if depth == 0:
                        return count + item
                elif depth == 1 and c == ",":
                    count, item = count + item, False
                elif depth == 1 and not c.isspace():
                    item = True
            return None
    return None


def _product(expression: str) -> int | None:
    """A dimension written as a product of integers (`13 * 5`)."""
    factors = [f.strip() for f in expression.split("*")]
    if not all(re.fullmatch(r"0x[0-9A-Fa-f]+|[1-9]\d*", f) for f in factors):
        return None
    result = 1
    for f in factors:
        result *= int(f, 0)
    return result


def accesses(image: bytes, delta: int, funcs: list[tuple[int, int, str]]) -> list[tuple]:
    """(address, kind, opcode, function) of every data address the code forms
    from a constant base: kind is exact, indexed or formed (see the module)."""
    import rabbitizer

    found = []
    for address, size, name in funcs:
        blob = image[address + delta:address + delta + size]
        regs: dict[int, tuple[int, bool]] = {28: (GP, False)}
        clobber = 0
        for offset in range(0, len(blob) - 3, 4):
            word = int.from_bytes(blob[offset:offset + 4], "little")
            ins = rabbitizer.Instruction(word, vram=address + offset)
            op = ins.getOpcodeName()
            if clobber:
                clobber -= 1
                if not clobber:
                    for reg in VOLATILE:
                        regs.pop(reg, None)
            if op in LOADS_STORES and ins.rs.value in regs:
                base, indexed = regs[ins.rs.value]
                found.append(((base + ins.getProcessedImmediate()) & 0xFFFFFFFF,
                              "indexed" if indexed else "exact", op, name))
            if op == "lui":
                regs[ins.rt.value] = ((ins.getProcessedImmediate() << 16) & 0xFFFFFFFF, False)
            elif op in ("addiu", "ori") and ins.rs.value in regs:
                (value, indexed), immediate = regs[ins.rs.value], ins.getProcessedImmediate()
                value = (value + immediate if op == "addiu" else value | immediate) & 0xFFFFFFFF
                regs[ins.rt.value] = (value, indexed)
                if ins.rt.value != 28:
                    found.append((value, "formed", op, name))
            elif op == "addu" and (ins.rs.value in regs) != (ins.rt.value in regs):
                regs[ins.rd.value] = (regs.get(ins.rs.value, regs.get(ins.rt.value))[0], True)
            else:
                if ins.modifiesRt():
                    regs.pop(ins.rt.value, None)
                if ins.modifiesRd():
                    regs.pop(ins.rd.value, None)
                if op in ("jal", "jalr"):
                    clobber = 2
                regs[28] = (GP, False)
    return found


def flags(o: dict) -> list[tuple[str, int, str, list[str]]]:
    """(flag, byte count, bytes, notes) of one object: `bytes` its declared
    bytes, `elements` their directive widths (0: .space), `size` its outermost
    element size, `start`/`next` its address and the next object's, `access`
    (offset, kind, opcode, function) tuples, `pointers` the data words into
    it, `strings` its string literals."""
    data, start, nxt, size = o["bytes"], o["start"], o["next"], o["size"]
    n, end = len(data), o["start"] + len(o["bytes"])
    exact = [a for a in o["access"] if a[1] == "exact"]
    only_exact = bool(exact) and len(exact) == len(o["access"]) and not o["pointers"]
    reach = max((a[0] + WIDTH.get(a[2], 4) for a in exact), default=0)
    result = []
    align = min(nxt & -nxt, 4) if nxt else 4
    widths, at = {}, 0  # each directive element's offset: its width (0: a .space byte)
    for w in o["elements"]:
        widths[at] = w
        at += w or 1
    for k in (1, 2, 3) if not o["strings"] else ():
        off = n - k
        whole = off in widths and all(w < 4 for a, w in widths.items() if a >= off)
        if off <= 0 or not whole or not (end - k < nxt and -(-(end - k) // align) * align == nxt):
            continue
        tail = data[off:]
        if not any(tail):
            continue
        notes = []
        if any(a[0] < n and a[0] + WIDTH.get(a[2], 4) > off for a in exact):
            notes.append("read")
        elif only_exact:
            notes.append("unread")
        if all(0x20 <= b < 0x7F for b in tail if b):
            notes.append("text")
        if size <= 2:
            values = struct.unpack(f"<{n // size}{'BH'[size - 1]}", data[:n // size * size])
            rest, trailing = values[:off // size], values[off // size:]
            if rest and any(v < min(rest) or v > max(rest) for v in trailing):
                notes.append("outlier")
        result.append(("tail", k, tail.hex(" "), notes))
    if only_exact and reach < n and nxt - (start + reach) <= 3 and any(data[reach:]):
        result.append(("unread", n - reach, data[reach:].hex(" "), []))
    widths = {w for w in o["elements"] if w}
    used = [WIDTH.get(a[2], 4) for a in o["access"] if a[1] != "formed"]
    if used and len(widths) == 1 and max(used) < min(widths) and not o["strings"]:
        w, u = min(widths), max(used)
        if any(any(data[i + u:i + w]) for i in range(0, n, w)):
            result.append(("wide", w, f"read {u}", []))
    if not o["access"] and not o["pointers"] and any(data):
        result.append(("unref", n, data[:16].hex(" "), ["slot"] if nxt - start <= 3 else []))
    for s in o["strings"]:
        z = s.find(0)
        if 0 <= z < len(s) - 1 and any(s[z + 1:]):
            result.append(("string", len(s) - z - 1, s[z + 1:].hex(" "), []))
    return result


def load(path: Path) -> dict:
    """A target's linked image, link map, functions and classified ranges."""
    values = config(path)
    image_path = ROOT / values["IMAGE"]
    elf = Path(f"{image_path}.elf")
    image, delta = image_path.read_bytes(), load_offset(elf)
    ranges = classification(ROOT / values["CLASSIFICATION"]) if values.get("CLASSIFICATION") else []
    return {"name": path.stem, "image": image, "delta": delta, "span": (-delta, len(image) - delta),
            "map": map_sections(Path(f"{image_path}.map")), "functions": functions(elf),
            "classified": ranges}


def element_size(label: str, unit: str, elements: list[int], cache: dict) -> int:
    """The bytes one more initializer element adds (see the module)."""
    n = sum(w or 1 for w in elements)
    count = outer_count(label, (ROOT / "decomp/src" / unit).parent, cache)
    if count is None:  # no definition found: by directive width
        return min([w for w in elements if w] or [1])
    if count == 0:  # a scalar or struct object
        return n
    return n // count if n % count == 0 else 1


def target_objects(t: dict, tmp: Path, cache: dict) -> list[dict]:
    """The C data objects of one target, in link map order."""
    objects = []
    bounds = sorted({a for _n, a, _s, _o in t["map"]} | {a + s for _n, a, s, _o in t["map"]})
    units: dict[str, tuple] = {}
    for name, address, _size, obj in t["map"]:
        unit = obj.split("/decomp/src/", 1)[-1]
        source = (ROOT / "decomp/src" / unit).with_suffix(".c")
        if name not in DATA or "/decomp/src/" not in obj or not source.exists():
            continue
        if obj not in units:
            folder = tmp / t["name"] / str(len(units))
            folder.mkdir(parents=True)
            units[obj] = (unit_labels(ROOT / obj, folder),
                          c_objects(Path(f"{ROOT / obj}.s").read_text()))
        labels, parsed = units[obj]
        here = sorted((off, label) for label, (section, off) in labels.items() if section == name)
        for i, (off, label) in enumerate(here):
            if label not in parsed:
                continue
            elements = parsed[label]["elements"]
            start, n = address + off, sum(w or 1 for w in elements)
            later = [address + o for o, _l in here[i + 1:] if address + o >= start + n]
            after = bounds[bisect.bisect_left(bounds, start + n):]
            if any(a < start + n and start < b for a, b, _c, _note in t["classified"]):
                raise SystemExit(f"{t['name']} {label}: a C object inside a classified range")
            objects.append({
                "target": t["name"], "unit": unit, "label": label, "start": start,
                "next": min(later[:1] + after[:1] or [start + n]),
                "bytes": t["image"][start + t["delta"]:start + t["delta"] + n],
                "elements": elements, "strings": parsed[label]["strings"],
                "size": element_size(label, unit, elements, cache), "access": [], "pointers": 0,
            })
    return objects


def attribute(targets: list[dict], objects: list[dict]) -> None:
    """Add each target's accesses and data words to the objects of the targets
    that can be resident beside it (an image overlapping another's span is
    never loaded with it)."""
    index: dict[str, list[dict]] = {}
    for o in sorted(objects, key=lambda o: o["start"]):
        index.setdefault(o["target"], []).append(o)
    spans = {t["name"]: t["span"] for t in targets}
    for t in targets:
        image, delta = t["image"], t["delta"]
        items = [(a, (kind, op, fn)) for a, kind, op, fn in accesses(image, delta, t["functions"])]
        items += [(int.from_bytes(image[a + delta + k:a + delta + k + 4], "little"), None)
                  for name, a, s, _o in t["map"] if name in DATA for k in range(0, s - 3, 4)]
        for owner, objs in index.items():
            span = spans[owner]
            if owner != t["name"] and not (span[1] <= t["span"][0] or t["span"][1] <= span[0]):
                continue
            starts = [o["start"] for o in objs]
            for address, entry in items:
                i = bisect.bisect_right(starts, address) - 1
                o = objs[i] if i >= 0 else None
                if o is None or address >= max(o["next"], o["start"] + len(o["bytes"])):
                    continue
                if entry is None:
                    o["pointers"] += 1
                else:
                    o["access"].append((address - o["start"], *entry))


def main() -> None:
    paths = sorted((ROOT / "decomp/targets").glob("*/*.mk"))
    wanted = {Path(a).stem for a in sys.argv[1:]} or {p.stem for p in paths}
    targets = [load(path) for path in paths]
    objects, cache = [], {}
    with tempfile.TemporaryDirectory() as tmp:
        for t in targets:
            objects += target_objects(t, Path(tmp), cache)
    attribute(targets, objects)
    counts: dict[str, int] = {}
    shown = [o for o in objects if o["target"] in wanted]
    flagged = 0
    for o in shown:
        result = flags(o)
        reviewed = [f for f in result if "read" not in f[3]]
        for kind in {f[0] for f in reviewed} | {"tail read" for f in result if "read" in f[3]}:
            counts[kind] = counts.get(kind, 0) + 1
        if not reviewed:
            continue
        flagged += 1
        readers = sorted({f"{a[3]}:{a[1]}" for a in o["access"]})
        notes = "; ".join(f"{f[0]} {f[1]} [{f[2]}]" + "".join(" " + x for x in f[3])
                          for f in reviewed)
        print(f"{o['target']} {o['start']:08x}-{o['start'] + len(o['bytes']):08x}"
              f" next {o['next']:08x} {o['label']} ({o['unit']}, element {o['size']} B,"
              f" {o['pointers']} pointers): {notes} | {' '.join(readers[:8]) or 'no reader'}"
              + (" ..." if len(readers) > 8 else ""))
    print(f"{len(shown)} C data objects, {flagged} to review; objects per flag: "
          + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))


if __name__ == "__main__":
    main()


