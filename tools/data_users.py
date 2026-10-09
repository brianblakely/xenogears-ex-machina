#!/usr/bin/env python3
"""Which code forms each data address (docs/matching.md, Recovering data).

    data_users.py CONFIG.mk [--end ADDR]     # a target's uninitialized variables
    data_users.py --range START:END [CONFIG.mk ...]   # users across images

Run after `make -C decomp all-verify`: it reads each target's rebuilt image
(byte-identical to the original), link map and ELF. Every function is
scanned for the data addresses it forms: `lui` bases (and `$gp`, 0x80059170)
resolved by `addiu`/`ori` or by a load/store offset, kept through `addu` (an
indexed access) and dropped by any other write or a call's clobber. This is
a static, linear lower bound, not an execution trace: data reached only
through pointers is not seen.

With a target, each .bss/.sbss input section lists the units forming
addresses in it. A unit with code whose own variables another unit's code
reaches is a FOREIGN reference (units without code, like the commons units,
are shared by design). `--end` extends the scan past the linked sections to
variables the units still leave extern (the menu's larger ones): there it
prints each run of addresses formed by one unit, SHARED addresses with their
users, and every INVERSION, an address formed only by a unit linked before
the owner of a lower address. `+` marks an address only formed (`addiu`,
`ori`; a loop's end bound, an argument), not loaded or stored.

With `--range`, every function of the given targets (default: all) that
forms an address in [START, END) is listed with its instructions (`@gp`:
through `$gp`): the check to run before calling shared data unreferenced.
"""

from __future__ import annotations

import argparse
import bisect
import re
import sys
from pathlib import Path

import rabbitizer

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from matching_diff import config, functions, load_offset  # noqa: E402

GP = 0x80059170
BSS_SECTION = re.compile(r"\.s?bss\b|COMMON\b|\.scommon\b")
LOADS_STORES = {
    "lb", "lbu", "lh", "lhu", "lw", "lwl", "lwr", "sb", "sh", "sw", "swl", "swr", "lwc2", "swc2",
}
VOLATILE = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31)


def input_sections(map_path: Path) -> list[tuple[str, int, int, str]]:
    """(section, address, size, unit) of every nonempty input section in the map."""
    result, pending = [], None
    for line in map_path.read_text().splitlines():
        parts = line.split()
        if line.startswith(" ") and len(parts) == 1 and not parts[0].startswith("0x"):
            pending = parts[0]  # a long section name on a line of its own
            continue
        if pending and len(parts) == 3:
            parts = [pending] + parts
        pending = None
        if len(parts) != 4 or not parts[1].startswith("0x") or not parts[2].startswith("0x"):
            continue
        name, address, size, obj = parts
        if int(size, 16) and obj.endswith(".o"):
            if "/decomp/src/" in obj:
                unit = obj.split("/decomp/src/", 1)[1].removesuffix(".o")
            else:
                unit = Path(obj).name.split(".")[0]  # generated: mdec_sdk_bss.bss.o
            result.append((name, int(address, 16), int(size, 16), unit))
    return result


def formed_addresses(code: bytes, address: int) -> dict[int, set[str]]:
    """Data addresses one function forms: {address: instructions}."""
    regs: dict[int, int] = {28: GP}
    found: dict[int, set[str]] = {}
    clobber = 0
    for offset in range(0, len(code) - 3, 4):
        word = int.from_bytes(code[offset : offset + 4], "little")
        ins = rabbitizer.Instruction(word, vram=address + offset)
        name = ins.getOpcodeName()
        if clobber:
            clobber -= 1
            if not clobber:
                for reg in VOLATILE:
                    regs.pop(reg, None)
        if name in LOADS_STORES and ins.rs.value in regs:
            target = (regs[ins.rs.value] + ins.getProcessedImmediate()) & 0xFFFFFFFF
            found.setdefault(target, set()).add(name + ("@gp" if ins.rs.value == 28 else ""))
        if name == "lui":
            regs[ins.rt.value] = (ins.getProcessedImmediate() << 16) & 0xFFFFFFFF
            continue
        if name in ("addiu", "ori") and ins.rs.value in regs:
            value, immediate = regs[ins.rs.value], ins.getProcessedImmediate()
            value = (value + immediate if name == "addiu" else value | immediate) & 0xFFFFFFFF
            if ins.rt.value != 28:  # setting $gp itself (the start code) forms no data address
                found.setdefault(value, set()).add(name + ("@gp" if ins.rs.value == 28 else ""))
            regs[ins.rt.value] = value
            continue
        if name == "addu" and (ins.rs.value in regs) != (ins.rt.value in regs):
            regs[ins.rd.value] = regs.get(ins.rs.value, regs.get(ins.rt.value))
            continue
        if ins.modifiesRt():
            regs.pop(ins.rt.value, None)
        if ins.modifiesRd():
            regs.pop(ins.rd.value, None)
        if name in ("jal", "jalr"):
            clobber = 2  # after the delay slot
        regs[28] = GP
    return found


def scan(config_path: Path, lo: int, hi: int):
    """The target's link and {address: {unit: {function: instructions}}} in [lo, hi)."""
    values = config(config_path)
    image = ROOT / values["IMAGE"]
    elf = Path(str(image) + ".elf")
    sections = input_sections(Path(str(image) + ".map"))
    texts = sorted((a, a + n, u) for s, a, n, u in sections if s.startswith(".text"))
    starts = [a for a, _, _ in texts]
    data, delta = image.read_bytes(), load_offset(elf)
    users: dict[int, dict[str, dict[str, set[str]]]] = {}
    for address, size, name in functions(elf):
        i = bisect.bisect_right(starts, address) - 1
        if i < 0 or address >= texts[i][1] or not size:
            continue
        code = data[address + delta : address + delta + size]
        for target, how in formed_addresses(code, address).items():
            if lo <= target < hi:
                users.setdefault(target, {}).setdefault(texts[i][2], {}).setdefault(name, set()).update(how)
    return sections, texts, users


def label(names: dict[str, set[str]]) -> str:
    """Functions, `+` on those that only form the address."""
    return ", ".join(n + ("" if any(h.split("@")[0] in LOADS_STORES for h in how) else "+")
                     for n, how in sorted(names.items()))


def ownership(config_path: Path, end: int | None) -> list[str]:
    values = config(config_path)
    sections = input_sections(Path(str(ROOT / values["IMAGE"]) + ".map"))
    bss = sorted((a, a + n, u) for s, a, n, u in sections if BSS_SECTION.match(s))
    lo = bss[0][0] if bss else (end or 0)
    hi = max([e for _, e, _ in bss] + [end or 0])
    _sections, texts, users = scan(config_path, lo, hi)
    order = list(dict.fromkeys(u for _, _, u in texts))
    lines = [f"== {config_path.stem}: units {' '.join(order)}"]
    foreign = []
    for start, stop, owner in bss:
        units = sorted({u for a, by in users.items() if start <= a < stop for u in by}, key=order.index)
        code = owner in order
        lines.append(f"  {owner} .bss {start:08x}-{stop:08x}{'' if code else ' (no code)'}: {' '.join(units) or '-'}")
        for a in sorted(a for a in users if start <= a < stop and code):
            foreign.extend(f"{a:08x} ({owner}) formed by {u} {label(n)}" for u, n in users[a].items() if u != owner)
    lines.append("  FOREIGN: " + ("; ".join(foreign) if foreign else "none"))
    if end is not None:
        linked = bss[-1][1] if bss else lo
        runs, inversions, latest = [], [], None
        for a in sorted(x for x in users if x >= linked):
            by = users[a]
            owner = next(iter(by)) if len(by) == 1 else "SHARED (" + "; ".join(
                f"{u} {label(n)}" for u, n in by.items()) + ")"
            if runs and runs[-1][0] == owner:
                runs[-1][2] = a
            else:
                runs.append([owner, a, a])
            if owner.startswith("SHARED"):
                continue
            if latest and order.index(owner) < order.index(latest[1]):
                inversions.append(f"{a:08x} ({owner} {label(by[owner])}) after {latest[0]:08x} ({latest[1]})")
            else:
                latest = (a, owner)
        lines.append(f"  past the linked .bss ({linked:08x}-{end:08x}):")
        lines.extend(f"    {owner} {a:08x}-{b:08x}" for owner, a, b in runs)
        lines.append("  INVERSIONS: " + ("; ".join(inversions) if inversions else "none"))
    return lines


def users_in(configs: list[Path], lo: int, hi: int) -> list[str]:
    lines = []
    for config_path in configs:
        _sections, _texts, users = scan(config_path, lo, hi)
        for a in sorted(users):
            for unit, names in users[a].items():
                uses = "; ".join(f"{n} {' '.join(sorted(h))}" for n, h in sorted(names.items()))
                lines.append(f"{a:08x} {config_path.stem}:{unit}: {uses}")
    return lines or [f"no function forms an address in {lo:08x}-{hi:08x}"]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("configs", type=Path, nargs="*")
    parser.add_argument("--end", type=lambda s: int(s, 16), help="end (hex) of variables past the linked .bss")
    parser.add_argument("--range", help="START:END (hex): list every user across the targets")
    args = parser.parse_args()
    configs = [c.resolve() for c in args.configs]
    if args.range:
        lo, hi = (int(x, 16) for x in args.range.split(":"))
        configs = configs or sorted((ROOT / "decomp/targets").glob("*/*.mk"))
        print("\n".join(users_in(configs, lo, hi)))
    elif len(configs) == 1:
        print("\n".join(ownership(configs[0], args.end)))
    else:
        parser.error("give one target, or --range")


if __name__ == "__main__":
    main()
