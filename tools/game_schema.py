#!/usr/bin/env python3
"""The game's data schema for introspection (docs/runtime.md, Introspection).

Every unit the matched PS1 links compiled is compiled again, unchanged, with
debug information by clang's MIPS front end (the layout the game module keeps).
Its DWARF gives every global the C defines with its full type; the matched
links give its original address. The result, build/game/schema.json, lets the
runtime decode any global from game memory by name and path:

    {"globals": {name: {"address", "image", "type"}},
     "types": [{"kind": "base"|"typedef"|"pointer"|"array"|"struct"|"union"|"enum"|"function"|"opaque", ...}],
     "records": {struct or union name: type index}}

A pointer to a named struct or union refers to it as "record <name>".

Run by tools/game_module.py; needs pyelftools (nix/runtime).
"""

import concurrent.futures
import json
import os
import subprocess
import tempfile
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parent.parent


class Types:
    """Types interned by structure, so units' identical types share one entry."""

    def __init__(self):
        self.entries, self.index, self.records = [], {}, {}

    def add(self, entry):
        key = json.dumps(entry, sort_keys=True)
        if key not in self.index:
            self.index[key] = len(self.entries)
            self.entries.append(entry)
        number = self.index[key]
        # Pointers name records ("record Name"); the first full definition wins.
        if entry["kind"] in ("struct", "union") and entry.get("name"):
            self.records.setdefault(entry["name"], number)
        return number


def attr(die, name, default=None):
    value = die.attributes.get(name)
    if value is None:
        return default
    value = value.value
    return value.decode() if isinstance(value, bytes) else value


def type_of(die, types, busy):
    """The interned type id of a DIE's DW_AT_type (void is None)."""
    if "DW_AT_type" not in die.attributes:
        return None
    return convert(die.get_DIE_from_attribute("DW_AT_type"), types, busy)


def convert(die, types, busy):
    tag = die.tag
    if tag in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type", "DW_TAG_restrict_type"):
        inner = type_of(die, types, busy)
        if tag == "DW_TAG_typedef":
            return types.add({"kind": "typedef", "name": attr(die, "DW_AT_name"), "type": inner})
        return inner
    if tag == "DW_TAG_base_type":
        encoding = {5: "signed", 6: "signed", 7: "unsigned", 8: "unsigned", 2: "bool", 4: "float"}
        return types.add({"kind": "base", "name": attr(die, "DW_AT_name"), "size": attr(die, "DW_AT_byte_size"),
                          "encoding": encoding.get(attr(die, "DW_AT_encoding"), "unsigned")})
    if tag == "DW_TAG_pointer_type":
        # Pointers to records refer to them by name, which breaks cycles.
        target = die.get_DIE_from_attribute("DW_AT_type") if "DW_AT_type" in die.attributes else None
        pointee = None
        if target is not None:
            pointee = ("record " + attr(target, "DW_AT_name")) if target.tag in (
                "DW_TAG_structure_type", "DW_TAG_union_type") and attr(target, "DW_AT_name") else None
            if pointee is None and target.offset not in busy:
                busy.add(target.offset)
                pointee = convert(target, types, busy)
                busy.discard(target.offset)
        return types.add({"kind": "pointer", "size": 4, "to": pointee})
    if tag == "DW_TAG_array_type":
        dims = []
        for child in die.iter_children():
            if child.tag == "DW_TAG_subrange_type":
                count = attr(child, "DW_AT_count")
                upper = attr(child, "DW_AT_upper_bound")
                dims.append(count if count is not None else (upper + 1 if upper is not None else 0))
        element = type_of(die, types, busy)
        for count in reversed(dims or [0]):
            element = types.add({"kind": "array", "of": element, "count": count})
        return element
    if tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
        name = attr(die, "DW_AT_name")
        if attr(die, "DW_AT_declaration"):
            return types.add({"kind": "opaque", "name": name})
        members = []
        for child in die.iter_children():
            if child.tag != "DW_TAG_member":
                continue
            member = {"name": attr(child, "DW_AT_name"), "offset": attr(child, "DW_AT_data_member_location", 0),
                      "type": type_of(child, types, busy)}
            if "DW_AT_bit_size" in child.attributes:
                member["bit_size"] = attr(child, "DW_AT_bit_size")
                member["bit_offset"] = attr(child, "DW_AT_data_bit_offset", 0)
                member["offset"] = 0
            members.append(member)
        kind = "struct" if tag == "DW_TAG_structure_type" else "union"
        return types.add({"kind": kind, "name": name, "size": attr(die, "DW_AT_byte_size", 0), "members": members})
    if tag == "DW_TAG_enumeration_type":
        values = {attr(c, "DW_AT_name"): attr(c, "DW_AT_const_value") for c in die.iter_children()
                  if c.tag == "DW_TAG_enumerator"}
        return types.add({"kind": "enum", "name": attr(die, "DW_AT_name"), "size": attr(die, "DW_AT_byte_size", 4),
                          "values": values})
    if tag == "DW_TAG_subroutine_type":
        return types.add({"kind": "function"})
    return types.add({"kind": "opaque", "name": attr(die, "DW_AT_name") or tag})


def unit_globals(obj, types):
    """(name, is_static, type id) of every variable a compiled unit defines."""
    out = []
    with open(obj, "rb") as stream:
        dwarf = ELFFile(stream).get_dwarf_info()
        for cu in dwarf.iter_CUs():
            for die in cu.get_top_DIE().iter_children():
                if die.tag != "DW_TAG_variable" or attr(die, "DW_AT_declaration"):
                    continue
                name = attr(die, "DW_AT_name")
                if not name:
                    continue
                out.append((name, not attr(die, "DW_AT_external"), type_of(die, types, set())))
            # Function-local statics.
            for die in cu.get_top_DIE().iter_children():
                if die.tag != "DW_TAG_subprogram":
                    continue
                for child in die.iter_children():
                    if child.tag == "DW_TAG_variable" and "DW_AT_location" in child.attributes and attr(child, "DW_AT_name"):
                        location = child.attributes["DW_AT_location"].value
                        # A static has a DW_OP_addr location; locals have frame offsets.
                        if isinstance(location, list) and location and location[0] == 0x03:
                            out.append((f"{attr(die, 'DW_AT_name')}.{attr(child, 'DW_AT_name')}", True,
                                        type_of(child, types, set())))
    return out


def headers_first(source, directory):
    """A wrapper unit that includes the target's headers before `source`.

    The commons units define arrays of types their headers complete later
    (GCC accepts it, clang does not); with the headers first the types are
    complete, and the unit itself is unchanged."""
    wrapper = Path(directory) / (source.replace("/", ".") + ".wrapper.c")
    target = Path(source).parts[2]
    headers = sorted((ROOT / "decomp/include" / target).glob("*.h")) + sorted((ROOT / "decomp/src" / target).glob("*.h"))
    includes = "".join(f'#include "{h}"\n' for h in [ROOT / "decomp/include/common.h", *headers])
    wrapper.write_text(includes + f'#include "{ROOT / source}"\n')
    return wrapper


def build_schema(out, units, addresses, cflags, jobs):
    """Write out/schema.json for the game units [(source, image)]."""
    types = Types()
    schema = {}
    with tempfile.TemporaryDirectory(prefix="xem-schema-") as tmp:
        def compile_unit(item):
            source, _ = item
            obj = Path(tmp) / (source.replace("/", ".") + ".o")

            def attempt(path):
                return subprocess.run([os.environ["XEM_CLANG"], *cflags, "-c", "-g", "-O0",
                                       "-fno-eliminate-unused-debug-types", "-ferror-limit=0", str(path), "-o", str(obj)],
                                      cwd=ROOT, capture_output=True, text=True).returncode == 0

            if attempt(source):
                return obj
            return obj if attempt(headers_first(source, Path(tmp))) else None

        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            objects = list(pool.map(compile_unit, units))
        for (source, image), obj in zip(units, objects):
            if obj is None:
                continue
            for name, static, type_id in unit_globals(obj, types):
                address = addresses.data(source, name, static)
                if address is None:
                    continue
                schema[name if not static or name not in schema else f"{source}:{name}"] = {
                    "address": address, "image": image, "type": type_id}
    # Symbols without a C definition (SDK data, linker-script views) are kept
    # by address with no type; they read as raw words.
    for name, address in addresses.globals.items():
        if name not in schema and not name.startswith(("_", "gcc2_compiled", "__gnu_compiled")) \
                and name not in addresses.functions:
            schema[name] = {"address": address, "image": None, "type": None}
    path = out / "schema.json"
    path.write_text(json.dumps({"globals": schema, "types": types.entries, "records": types.records}, sort_keys=True))
    return path, len(schema), len(types.entries)
