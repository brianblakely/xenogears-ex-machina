#!/usr/bin/env python3
"""Set up a decomp-permuter directory for one NON_MATCHING function (debugging aid).

    permuter_import.py decomp/targets/overlays/field.mk func_8007XXXX [--out DIR]
    permuter -j8 --best-only DIR

The directory holds base.c (the unit preprocessed with -DNON_MATCHING, retaining
inline helpers and reducing other bodies to prototypes), compile.sh (the unit's qualified GCC
version, -G value and maspsx flags, exactly as decomp/Makefile builds it) and
target.o (the original function's private assembly). A permuter score of zero is
only a candidate: copy the source back and accept it with `make verify`.
Run inside the matching shell from the repository root.
"""

from __future__ import annotations

import argparse
import re
import shutil
import stat
import subprocess
import sys
from pathlib import Path

# The checkout being worked on: the current directory when it is one (worktrees).
ROOT = Path.cwd() if (Path.cwd() / "decomp/Makefile").exists() else Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

# Kept identical to decomp/Makefile CPPFLAGS/CC1FLAGS/ASFLAGS.
CPPFLAGS = (
    "-undef -D__GNUC__=2 -Dmips -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx -D_PSYQ "
    "-D__EXTENSIONS__ -D_MIPSEL -D__CHAR_UNSIGNED__ -D_LANGUAGE_C -DLANGUAGE_C "
    "-lang-c -nostdinc"
).split()
CC1FLAGS = "-quiet -mcpu=3000 -fgnu-linker -mgas -msoft-float -O2".split()
ASFLAGS = "-EL -march=r3000 -mtune=r3000 -msoft-float -no-pad-sections".split()
ABSOLUTE_FILTER = (
    r"sed -E -e '/^\.extern/d' "
    r"-e 's/^la\t(\$[0-9a-z]+),([A-Za-z_][A-Za-z0-9_]*(\+[0-9]+)?)$/lui\t\1,%hi(\2)\naddiu\t\1,\1,%lo(\2)/'"
)


def permuter_lib() -> Path:
    exe = shutil.which("permuter")
    if not exe:
        raise SystemExit("permuter not found: enter the matching Nix shell")
    text = Path(exe).read_text()
    match = re.search(r"(\S+)/permuter\.py", text)
    if not match:
        raise SystemExit("cannot locate decomp-permuter from its wrapper")
    return Path(match.group(1))


def strip_other_functions(text: str, keep: str) -> str:
    """Keep the selected body and inline definitions; reduce other bodies to prototypes."""
    out, depth, i, start, n = [], 0, 0, 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            continue
        if c == "{":
            if depth == 0:
                head = text[start:i].rstrip()
                match = re.search(r"(\w+)\s*\((?:[^()]|\([^()]*\))*\)$", head)
                if match and not re.search(r"=\s*$", head[: match.start()]):
                    end, level = i + 1, 1
                    while level:
                        level += {"{": 1, "}": -1}.get(text[end], 0)
                        end += 1
                    # Inline definitions participate in the selected function's
                    # code generation. Earlier prototypes are not its qualifiers.
                    qualifiers = head[: match.start()].rsplit(";", 1)[-1]
                    inline = re.search(r"\b(?:inline|__inline|__inline__)\b", qualifiers)
                    if match.group(1) == keep or inline:
                        out.append(text[start:end])
                    else:
                        out.append(head + ";")
                    start = i = end
                    continue
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    out.append(text[start:])
    return "".join(out)


def unit_setting(values: dict[str, str], prefix: str, unit: str, default: str) -> str:
    return values.get(f"{prefix}_{unit}", default)


def main() -> None:
    from matching_diff import config

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("config", type=Path)
    parser.add_argument("function")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    values = config(args.config)
    name = args.function
    pattern = re.compile(r"INCLUDE_ASM\(\s*\"([^\"]*)\"\s*,\s*%s\s*\)" % re.escape(name))
    source = asm_dir = None
    for directory in values["SOURCE_DIRS"].split():
        for path in sorted((ROOT / directory).rglob("*.c")):
            match = pattern.search(path.read_text())
            if match:
                source, asm_dir = path, match.group(1)
                break
        if source:
            break
    if source is None:
        raise SystemExit(f"{name}: no INCLUDE_ASM in {values['SOURCE_DIRS']}")

    unit = source.stem
    version = unit_setting(values, "CC", unit, values["CC_VERSION"])
    gp = unit_setting(values, "GP", unit, "0")
    maspsx = unit_setting(values, "MASPSX", unit, values.get("MASPSX_FLAGS", "--aspsx-version=2.34"))
    absolute = unit_setting(values, "EXTERN", unit, "") == "absolute"

    out = (args.out or ROOT / ".local/permuter" / name).resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    includes = ["-I" + str(ROOT / "decomp/include")] + values.get("TARGET_CPPFLAGS", "").split()
    pre = subprocess.run(
        [f"psx-cpp-{version}", *CPPFLAGS, *includes, "-DNON_MATCHING", "-P", str(source)],
        check=True, capture_output=True, text=True, cwd=ROOT,
    ).stdout
    # File-scope asm (macro.inc, INCLUDE_RODATA) is irrelevant to the function's code.
    pre = re.sub(r"^\s*__asm__\s*\(.*?\)\s*;\s*$", "", pre, flags=re.M)

    permuter_lib()
    (out / "base.c").write_text(strip_other_functions(pre, name))

    filter_ = f" | {ABSOLUTE_FILTER}" if absolute else ""
    compile_sh = out / "compile.sh"
    compile_sh.write_text(
        "#!/usr/bin/env bash\n"
        "# Same pipeline as decomp/Makefile for this unit.\n"
        "set -eo pipefail\n"
        'IN="$1"; OUT="$3"\n'
        f"psx-cc1-{version} {' '.join(CC1FLAGS)} -G{gp} -o \"$OUT.cc1.s\" \"$IN\"\n"
        f"maspsx {maspsx} -G{gp} < \"$OUT.cc1.s\"{filter_} > \"$OUT.s\"\n"
        f"psx-as {' '.join(ASFLAGS)} -G{gp} -o \"$OUT\" \"$OUT.s\"\n"
        'rm -f "$OUT.cc1.s" "$OUT.s"\n'
    )
    compile_sh.chmod(compile_sh.stat().st_mode | stat.S_IXUSR)

    asm = ROOT / asm_dir / f"{name}.s"
    (out / "target.s").write_text(
        '.include "macro.inc"\n.set noat\n.set noreorder\n'
        f'.include "{asm}"\n'
    )
    subprocess.run(
        ["psx-as", *ASFLAGS, "-G0", "-I", str(ROOT / "decomp/include"), "-I", str(ROOT),
         "-o", str(out / "target.o"), str(out / "target.s")],
        check=True, cwd=ROOT,
    )
    (out / "settings.toml").write_text(
        f'func_name = "{name}"\ncompiler_type = "gcc"\n'
        'objdump_command = "psx-objdump -drz -m mips:3000"\n'
    )
    base = subprocess.run([str(compile_sh), str(out / "base.c"), "-o", str(out / "base.o")],
                          capture_output=True, text=True)
    if base.returncode:
        sys.stderr.write(base.stderr)
        raise SystemExit(f"{out}: base.c does not compile")
    print(f"{out} ({source.relative_to(ROOT)}, GCC {version}, -G{gp}, maspsx {maspsx})")


if __name__ == "__main__":
    main()
