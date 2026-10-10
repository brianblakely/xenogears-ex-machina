"""tools/names.py on a synthetic repository: four linked targets (a resident,
two overlays at one address and a third that imports from the resident and
the first overlay), their sources, configuration, a doc and the package list."""

from __future__ import annotations

import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "tools/names.py"

ASSEMBLY = {
    # target: (load address, unit, assembly)
    "r": (
        0x80010000,
        "decomp/src/resident/main_80010000",
        ".globl func_80010000\n.type func_80010000, @function\nfunc_80010000:\njr $31\nnop\n"
        ".size func_80010000, . - func_80010000\n.space 0x18\n"
        ".globl func_80010020\n.type func_80010020, @function\nfunc_80010020:\njr $31\nnop\n"
        ".size func_80010020, . - func_80010020\n"
        ".globl func_80010028\n.type func_80010028, @function\nfunc_80010028:\njr $31\nnop\n"
        ".size func_80010028, . - func_80010028\n"
        ".globl D_80010030\n.type D_80010030, @object\nD_80010030:\n.word 0\n"
        ".size D_80010030, 4\n.space 0x4\n.globl D_80010038\nD_80010038:\n.space 0x8\n"
        ".globl D_80010040\n.type D_80010040, @object\nD_80010040:\n.word 1, 2\n"
        ".size D_80010040, 8\n",
    ),
    "ovl1": (
        0x80100000,
        "decomp/src/ovl1/ovl1",
        ".globl func_80100000\n.type func_80100000, @function\nfunc_80100000:\n"
        "jal func_80010000\nnop\njr $31\nnop\n.size func_80100000, . - func_80100000\n"
        ".globl func_80100010\n.type func_80100010, @function\nfunc_80100010:\njr $31\nnop\n"
        ".size func_80100010, . - func_80100010\n",
    ),
    "ovl2": (
        0x80100000,
        "decomp/src/ovl2/ovl2",
        ".globl func_80100000\n.type func_80100000, @function\nfunc_80100000:\njr $31\nnop\n"
        ".size func_80100000, . - func_80100000\n",
    ),
    "ovl3": (
        0x80200000,
        "decomp/src/ovl3/ovl3",
        ".globl func_80200000\n.type func_80200000, @function\nfunc_80200000:\n"
        "jal func_80100010\nnop\nlui $8, %hi(D_80010044)\nlw $8, %lo(D_80010044)($8)\n"
        "lui $9, %hi(D_80010040)\nlw $9, %lo(D_80010040)($9)\n"
        "lui $10, %hi(D_80010000)\nlw $10, %lo(D_80010000)($10)\n"
        "lui $11, %hi(D_80010004)\nlw $11, %lo(D_80010004)($11)\njr $31\nnop\n"
        ".size func_80200000, . - func_80200000\n",
    ),
}
# Each target's splat list (an import by address) and fragments.
LISTS = {
    "ovl1": "func_80010000 = 0x80010000;\n",
    "ovl3": "func_80100010 = 0x80100010;\nD_80010000 = 0x80010000;\nD_80010004 = 0x80010004;\n",
}
# A generated file a unit includes, with a label splat keeps inside it.
GENERATED = {
    ".local/decomp/resident/asm/nonmatchings/main_80010000/func_80010028.s": (
        "glabel func_80010028\n    nop\n  alabel D_80010030\n    nop\n"
        "endlabel func_80010028\n  alabel D_80010038\n    nop\n"
    ),
}
FRAGMENTS = {"ovl3": "decomp/targets/overlays/ovl3.resident.ld"}

SOURCES = {
    ".gitignore": ".local/\n",
    "decomp/include/resident/r.h": (
        "#ifndef RESIDENT_R_H\n#define RESIDENT_R_H\n\n/* The resident's calls and data. */\n"
        "#define cd_flag 1\nvoid func_80010000(void);\n"
        "extern int D_80010040[2]; /* a pair of words */\n\n#endif\n"
    ),
    "decomp/src/resident/main_80010000.c": (
        '/* The resident unit. */\n#include "resident/r.h"\n\n/* Return at once. */\n'
        'void func_80010000(void) {\n}\n\nINCLUDE_ASM("decomp/src/resident", func_80010020);\n\n'
        'INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_80010000", func_80010028);\n\n'
        "int D_80010040[2] = {1, 2};\n"
    ),
    "decomp/src/resident/func_80010020.s": (
        "# Return at once, by hand.\nglabel func_80010020\n    jr      $ra\n    nop\n"
    ),
    "decomp/src/ovl1/ovl1.c": (
        '/* Overlay 1. */\n#include "resident/r.h"\n\nstruct Pair {\n    int arg0;\n};\n\n'
        "void func_80100000(int value, struct Pair *pair); /* named already */\n\n"
        "/* Call the resident; keep `arg0` in the pair. */\n"
        "void func_80100000(int arg0, struct Pair *pair) {\n    func_80010000();\n"
        "    pair->arg0 = arg0;\n}\n\nvoid func_80100010(void) {\n}\n"
    ),
    "decomp/src/ovl2/ovl2.c": (
        "/* Overlay 2. */\n\n/* Return at once. */\nvoid func_80100000(void) {\n}\n"
    ),
    "decomp/src/ovl3/ovl3.c": (
        '/* Overlay 3. */\n#include "resident/r.h"\n\n'
        "extern void func_80100010(void); /* overlay 1's */\n"
        "extern int D_80010044; /* the pair's second word */\n"
        "extern int D_80010000[]; /* the resident's first word, by number */\n\n"
        "void func_80200000(void) {\n    func_80100010();\n}\n"
    ),
    "decomp/targets/resident/symbol_addrs.txt": "// The resident's names.\n",
    "decomp/targets/resident/classification.txt": "80010028 80010030 sdk a library routine\n",
    "decomp/targets/overlays/ovl3.resident.ld": (
        "D_80010040 = 0x80010040; /* the pair */\nD_80010044 = D_80010040 + 0x4;\n"
    ),
    "docs/notes.md": (
        "# Notes\n\nfunc_80010000 starts the resident; func_80100000 is in ovl1 and ovl2.\n"
        "decomp/src/ovl1/ovl1.c calls it; its assembly was\n"
        ".local/decomp/ovl2/asm/nonmatchings/ovl2/func_80100000.s.\n"
    ),
}

MAPPING = """image\tkind\told\tnew\tunit\tconfidence\tevidence
resident\tprefix\tcd\tcd_\tdecomp/include/resident/r.h\thigh\tthe resident header
ovl1\tprefix\tone\tone_\t\thigh\toverlay 1
ovl2\tprefix\ttwo\ttwo_\t\thigh\toverlay 2
resident\tfunc\tfunc_80010000\tcd_start\t\thigh\treturns at once
resident\tfunc\tfunc_80010020\tcd_return\t\thigh\treturns at once
resident\tasm\tdecomp/src/resident/func_80010020.s\tcd_return.s\t\thigh\tfollows it
resident\tdata\tD_80010040\tcd_pair\t\thigh\ta pair of words
resident\tdata\tD_80010044\tcd_pair_second\t\thigh\tits second word
resident\tdata\tD_80010030\tlibtest_word\t\thigh\ta label inside func_80010028's file
resident\tunit\tdecomp/src/resident/main_80010000.c\tcd_main.c\t\thigh\tthe resident unit
ovl1\tfunc\tfunc_80100000\tone_call_resident\t\thigh\tcalls the resident
ovl1\tfunc\tfunc_80100010\tone_return\t\thigh\treturns
ovl1\tunit\tdecomp/src/ovl1/ovl1.c\tone_main.c\t\thigh\tthe overlay's unit
ovl1\tparam\tfunc_80100000.arg0\tvalue\t\thigh\tkept in the pair
ovl2\tfunc\tfunc_80100000\ttwo_return\t\thigh\treturns
resident\tdata\tD_80010004\tcd_start_word\t\thigh\tthe start's second word
"""


@unittest.skipUnless(
    all(shutil.which(tool) for tool in ("psx-as", "psx-ld", "psx-cpp-2.7.2", "git"))
    and importlib.util.find_spec("rabbitizer"),
    "enter the matching Nix shell to test the renaming tool",
)
class NamesTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        files = dict(SOURCES)
        for target, (_address, unit, _asm) in ASSEMBLY.items():
            kind = "resident" if target == "r" else "overlays"
            image = Path(unit).parent.name
            config = Path("decomp/targets") / kind
            symbols = [f"{config}/{target}.symbols.txt"] if target != "r" else []
            symbols.append("decomp/targets/resident/symbol_addrs.txt")
            files[f"{config}/{target}.yaml"] = (
                f"name: {target}\noptions:\n  basename: {target}\n  symbol_addrs_path:\n"
                + "".join(f"    - {s}\n" for s in symbols)
                + f"segments:\n  - name: {target}\n    subsegments:\n"
                f"      - [0x0, c, {Path(unit).name}]\n"
            )
            if target != "r":
                files[f"{config}/{target}.symbols.txt"] = ""
            extra = ([f".local/{target}/undefined_funcs_auto.txt"] if target in LISTS else []) + (
                [FRAGMENTS[target]] if target in FRAGMENTS else []
            )
            files[f"{config}/{target}.mk"] = (
                f"CC_VERSION := 2.7.2\nSPLAT_CONFIG := {config}/{target}.yaml\n"
                f"IMAGE := .local/build/{target}.bin\nBUILD := .local/build/{target}\n"
                f"LINKER_SCRIPT := .local/{target}/{target}.ld\nSOURCE_DIRS := decomp/src/{image}\n"
                + (f"LINKER_EXTRA := {' '.join(extra)}\n" if extra else "")
                + (
                    "GP_main_80010000 := 8\n"
                    "CLASSIFICATION := decomp/targets/resident/classification.txt\n"
                    if target == "r"
                    else ""
                )
            )
        files["packaging/source-files.txt"] = "\n".join(
            sorted([*files, "packaging/source-files.txt"])
        )
        for name, text in [*files.items(), *GENERATED.items()]:
            (self.root / name).parent.mkdir(parents=True, exist_ok=True)
            (self.root / name).write_text(text)
        self.link()
        self.git("init", "-q")
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "fixture")

    def git(self, *arguments):
        return subprocess.run(
            ["git", "-c", "user.name=t", "-c", "user.email=t@t", *arguments],
            cwd=self.root,
            check=True,
            capture_output=True,
            text=True,
        ).stdout

    def link(self):
        for target, (address, unit, asm) in ASSEMBLY.items():
            build = Path(f".local/build/{target}")
            source = self.root / f".local/{target}/{target}.s"
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text(".set noreorder\n.text\n" + asm)
            obj = build / f"{unit}.o"
            (self.root / obj).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(
                ["psx-as", "-EL", "-march=r3000", "-o", str(obj), str(source)],
                cwd=self.root,
                check=True,
            )
            script = self.root / f".local/{target}/{target}.ld"
            script.write_text(
                f"SECTIONS {{\n  .{target} 0x{address:08X} : SUBALIGN(4) {{\n"
                f"    {obj}(.text)\n  }}\n  /DISCARD/ : {{ *(*) }}\n}}\n"
            )
            scripts = []
            if target in LISTS:
                (self.root / f".local/{target}/undefined_funcs_auto.txt").write_text(LISTS[target])
                provide = self.root / build / f".local/{target}/undefined_funcs_auto.ld"
                provide.parent.mkdir(parents=True, exist_ok=True)
                provide.write_text(
                    "".join(
                        f"PROVIDE({line.rstrip(';')});\n" for line in LISTS[target].splitlines()
                    )
                )
                scripts.append(str(provide.relative_to(self.root)))
            if target in FRAGMENTS:
                scripts.append(FRAGMENTS[target])
            subprocess.run(
                [
                    "psx-ld",
                    "-nostdlib",
                    "--no-check-sections",
                    "-Map",
                    f".local/build/{target}.bin.map",
                    "-T",
                    str(script.relative_to(self.root)),
                    *(f"-T{s}" for s in scripts),
                    "-o",
                    f".local/build/{target}.bin.elf",
                ],
                cwd=self.root,
                check=True,
            )

    def names(self, *arguments):
        return subprocess.run(
            [sys.executable, str(TOOL), *arguments],
            cwd=self.root,
            text=True,
            capture_output=True,
            check=False,
        )

    def read(self, name):
        return (self.root / name).read_text()

    def test_inventory_lists_each_image_s_placeholders(self):
        result = self.names("inventory")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = {
            tuple(line.split("\t")[:3]): line.split("\t")
            for line in self.read(".local/names/all.tsv").splitlines()[1:]
        }
        start = rows[("resident", "func", "func_80010000")]
        self.assertEqual(
            start[3:],
            [
                "80010000",
                "0x8",
                "global",
                "decomp/src/resident/main_80010000.c",
                "decomp/src/resident/main_80010000.c",
                "ovl1",
                "Return at once.",
            ],
        )
        self.assertEqual(rows[("resident", "data", "D_80010040")][8:], ["ovl3", "a pair of words"])
        # A view another image's fragment gives, listed under the image holding it.
        view = rows[("resident", "data", "D_80010044")]
        self.assertEqual((view[5], view[8]), ("view:D_80010040 + 0x4", "ovl3"))
        # The same placeholder in both overlays at 0x80100000, and the first's import.
        self.assertIn(("ovl2", "func", "func_80100000"), rows)
        self.assertEqual(rows[("ovl1", "func", "func_80100010")][8], "ovl3")
        # Units named by an address or the image number, an INCLUDE_ASM'd .s and a parameter.
        self.assertIn(("resident", "unit", "decomp/src/resident/main_80010000.c"), rows)
        self.assertIn(("ovl1", "unit", "decomp/src/ovl1/ovl1.c"), rows)
        self.assertEqual(
            rows[("resident", "asm", "decomp/src/resident/func_80010020.s")][5],
            "follows:func_80010020",
        )
        self.assertEqual(rows[("ovl1", "param", "func_80100000.arg0")][3], "80100000")
        label = rows[("resident", "data", "D_80010030")]
        self.assertEqual((label[5], label[7]), ("label", list(GENERATED)[0]))
        alias = rows[("resident", "func", "D_80010000")]
        self.assertEqual((alias[5], alias[8]), ("alias:func_80010000", "ovl3"))
        self.assertEqual(rows[("resident", "data", "D_80010004")][5:9:3], ["member", "ovl3"])

    def test_check_reports_each_rule(self):
        bad = self.root / "bad.tsv"
        bad.write_text(
            "resident\tprefix\tcd\tcd_\t\thigh\tx\n"
            "ovl1\tprefix\tone\tone_\t\thigh\tx\n"
            "ovl2\tprefix\tone\tone_\t\thigh\tx\n"
            "resident\tfunc\tfunc_80010000\tCdStart\t\thigh\tx\n"
            "resident\tfunc\tfunc_80010020\tone_return\t\thigh\tx\n"
            "resident\tdata\tD_80010040\tcd_misc_pair\t\thigh\tx\n"
            "resident\tdata\tD_80010044\tcd_flag\t\thigh\tx\n"
            "ovl1\tfunc\tfunc_80100000\tone_handle\t\thigh\tx\n"
            "ovl1\tfunc\tfunc_80100000\tone_again\t\thigh\tx\n"
            "ovl1\tfunc\tfunc_80100010\tone_handle\t\thigh\tx\n"
            "ovl2\tfunc\tfunc_80100000\tfunc_80100000_x\t\thigh\tx\n"
            "ovl3\tfunc\tfunc_80200000\tthree_entry\t\thigh\tx\n"
            "resident\tfunc\tfunc_8001DEAD\tcd_none\t\thigh\tx\n"
            "resident\tasm\tdecomp/src/resident/func_80010020.s\tcd_other.s\t\thigh\tx\n"
            "resident\tunit\tdecomp/src/resident/main_80010000.c\tmain_80010000.c\t\thigh\tx\n"
            "ovl1\tparam\tfunc_80100000.arg0\tpair\t\thigh\tx\n"
            "nowhere\tfunc\tfunc_80010000\tcd_x\t\thigh\tx\n"
            "resident\tthing\tfunc_80010000\tcd_x\t\thigh\tx\n"
            "resident\tfunc\tfunc_80010000\n"
            "resident\tdata\tD_80010030\tcd_word\t\thigh\tx\n"
            "resident\tdata\tD_80010038\tcd_tail\t\thigh\tx\n"
            "resident\tfunc\tD_80010000\tCdStart\t\thigh\tx\n"
        )
        result = self.names("check", "bad.tsv")
        self.assertEqual(result.returncode, 1)
        for message in (
            "bad.tsv:3: prefix one_ is also ovl1's",
            "bad.tsv:4: CdStart is not lower snake_case",
            "bad.tsv:5: one_return does not start with a prefix of resident (cd_): one_ is ovl1's",
            "bad.tsv:6: cd_misc_pair: says nothing (misc)",
            "bad.tsv:7: cd_flag is already a name in decomp/include/resident/r.h",
            "bad.tsv:8: one_handle: a bare 'handle' says nothing",
            "bad.tsv:9: ovl1 func func_80100000 is named twice",
            "one_handle is given to 2 symbols: ovl1 func_80100000, ovl1 func_80100010",
            "bad.tsv:11: func_80100000_x is still a placeholder",
            "bad.tsv:12: ovl3 declares no prefix (a prefix row)",
            "bad.tsv:13: func_8001DEAD is no placeholder of resident",
            "bad.tsv:14: decomp/src/resident/func_80010020.s follows its function: "
            "one_return.s, not cd_other.s",
            "bad.tsv:15: decomp/src/resident/main_80010000.c is already a file, a unit or an image",
            "bad.tsv:16: parameter pair is already a name in func_80100000",
            "bad.tsv:17: no image 'nowhere'",
            "bad.tsv:18: kind 'thing' is none of",
            "bad.tsv:19: 3 columns, not the seven",
            "bad.tsv:22: D_80010000 and func_80010000 (resident) meet in decomp/src/ovl3/ovl3.c",
            "bad.tsv:20: cd_word: an SDK member keeps its PsyQ name or takes its library's prefix",
            "bad.tsv:21: D_80010038 follows the function's end in .local/decomp/resident/asm/"
            "nonmatchings/main_80010000/func_80010028.s",
        ):
            self.assertIn(message, result.stderr)
        (self.root / "good.tsv").write_text(MAPPING)
        result = self.names("check", "good.tsv")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            "check: 16 rows (9 symbols, 2 units, 1 .s files, 1 parameters, 3 prefixes)",
            result.stdout,
        )
        self.assertIn("unnamed: 6 placeholders", result.stdout)
        unnamed = self.read(".local/names/unnamed.tsv")
        for name in (
            "func_80200000",
            "decomp/src/ovl2/ovl2.c",
            "decomp/src/ovl3/ovl3.c",
            "D_80010000",
            "D_80010038",
            "func_80010028",
        ):
            self.assertIn(name, unnamed)
        self.assertNotIn("D_80010030", unnamed)

    def test_apply_renames_by_scope_and_is_idempotent(self):
        (self.root / "map.tsv").write_text(MAPPING)
        result = self.names("apply", "map.tsv", "--dry-run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.git("status", "--porcelain"), "?? map.tsv\n")
        # The doc's first func_80100000 names neither overlay; the second lies in
        # a path naming ovl2.
        self.assertIn(
            "ambiguous: docs/notes.md:3: func_80100000: ovl1, ovl2 hold it", result.stderr
        )
        (self.root / "overrides.tsv").write_text("docs/notes.md\t3\tfunc_80100000\tovl1\n")
        result = self.names("apply", "map.tsv", "--overrides", "overrides.tsv")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("ambiguous", result.stderr)
        status = self.git("status", "--porcelain")
        self.assertIn("RM decomp/src/ovl1/ovl1.c -> decomp/src/ovl1/one_main.c", status)
        self.assertIn(
            "RM decomp/src/resident/main_80010000.c -> decomp/src/resident/cd_main.c", status
        )
        self.assertIn(
            "RM decomp/src/resident/func_80010020.s -> decomp/src/resident/cd_return.s", status
        )
        # Each overlay's func_80100000 by its own target; the parameter only
        # inside its function (not the member), its address in its comment.
        self.assertEqual(
            self.read("decomp/src/ovl1/one_main.c"),
            (
                '/* Overlay 1. */\n#include "resident/r.h"\n\nstruct Pair {\n    int arg0;\n};\n\n'
                "void one_call_resident(int value, struct Pair *pair); /* named already */\n\n"
                "/* 80100000: Call the resident; keep `value` in the pair. */\n"
                "void one_call_resident(int value, struct Pair *pair) {\n    cd_start();\n"
                "    pair->arg0 = value;\n}\n\n/* 80100010 */\nvoid one_return(void) {\n}\n"
            ),
        )
        self.assertEqual(
            self.read("decomp/src/ovl2/ovl2.c"),
            ("/* Overlay 2. */\n\n/* 80100000: Return at once. */\nvoid two_return(void) {\n}\n"),
        )
        self.assertEqual(
            self.read("decomp/src/resident/cd_main.c"),
            (
                '/* 80010000: The resident unit. */\n#include "resident/r.h"\n\n'
                "/* 80010000: Return at once. */\nvoid cd_start(void) {\n}\n\n"
                '/* 80010020 */\nINCLUDE_ASM("decomp/src/resident", cd_return);\n\n'
                # the generated assembly's folder follows the unit
                'INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/cd_main", func_80010028);\n\n'
                "int cd_pair[2] = {1, 2}; /* 80010040 */\n"
            ),
        )
        self.assertIn("glabel cd_return\n", self.read("decomp/src/resident/cd_return.s"))
        self.assertIn(
            "void cd_start(void);\nextern int cd_pair[2]; /* a pair of words */",
            self.read("decomp/include/resident/r.h"),
        )
        self.assertIn(
            "extern void one_return(void); /* overlay 1's */\n"
            "extern int cd_pair_second; /* the pair's second word */",
            self.read("decomp/src/ovl3/ovl3.c"),
        )
        # Configuration: the per-unit setting, yaml subsegments (not the
        # segment or basename), the fragment and the symbol files.
        self.assertIn("GP_cd_main := 8", self.read("decomp/targets/resident/r.mk"))
        self.assertIn("- [0x0, c, cd_main]", self.read("decomp/targets/resident/r.yaml"))
        yaml = self.read("decomp/targets/overlays/ovl1.yaml")
        self.assertIn("name: ovl1\noptions:\n  basename: ovl1\n", yaml)
        self.assertIn("- [0x0, c, one_main]", yaml)
        self.assertEqual(
            self.read("decomp/targets/overlays/ovl3.resident.ld"),
            (
                "cd_pair = 0x80010040; /* the pair */\n"
                "cd_pair_second = cd_pair + 0x4; /* 80010044 */\n"
            ),
        )
        header = "// Names tools/names.py gave these addresses (docs/matching.md, Names).\n"
        self.assertEqual(
            self.read("decomp/targets/resident/symbol_addrs.txt"),
            (
                "// The resident's names.\n" + header + "cd_start = 0x80010000; // type:func\n"
                "cd_return = 0x80010020; // type:func\nlibtest_word = 0x80010030; // type:label\n"
                "cd_pair = 0x80010040;\n"
            ),
        )
        self.assertEqual(
            self.read("decomp/targets/overlays/ovl1.symbols.txt"),
            (
                header + "one_call_resident = 0x80100000; // type:func\n"
                "one_return = 0x80100010; // type:func\n"
            ),
        )
        # ovl3 takes ovl1's function and a part of the resident's first word
        # (only its assembly names it) from its splat list: its own file names them.
        self.assertEqual(
            self.read("decomp/targets/overlays/ovl3.symbols.txt"),
            (header + "cd_start_word = 0x80010004;\none_return = 0x80100010; // type:func\n"),
        )
        self.assertEqual(
            self.read("docs/notes.md"),
            (
                "# Notes\n\ncd_start starts the resident; one_call_resident is in ovl1 and ovl2.\n"
                "decomp/src/ovl1/one_main.c calls it; its assembly was\n"
                ".local/decomp/ovl2/asm/nonmatchings/ovl2/two_return.s.\n"
            ),
        )
        listed = self.read("packaging/source-files.txt").splitlines()
        self.assertEqual(listed, sorted(listed))
        self.assertIn("decomp/src/resident/cd_return.s", listed)
        # A second run changes nothing (once a split and a link name the
        # generated folder and the object after the renamed unit).
        generated = self.root / ".local/decomp/resident/asm/nonmatchings"
        (generated / "main_80010000").rename(generated / "cd_main")
        built = self.root / ".local/build/r/decomp/src/resident"
        (built / "main_80010000.o").rename(built / "cd_main.o")
        script = self.root / ".local/r/r.ld"
        script.write_text(script.read_text().replace("main_80010000", "cd_main"))
        result = self.names("apply", "map.tsv", "--overrides", "overrides.tsv")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("apply: changed 0 files;", result.stdout)
        self.assertIn("5 applied already", result.stdout)

    def test_apply_refuses_a_name_its_readers_bind_differently(self):
        # A header both overlays include names func_80100000, each its own.
        (self.root / "decomp/include/shared.h").write_text("void func_80100000();\n")
        for unit in ("decomp/src/ovl1/ovl1.c", "decomp/src/ovl2/ovl2.c"):
            text = self.read(unit)
            (self.root / unit).write_text(text.replace("\n\n", '\n#include "shared.h"\n\n', 1))
        self.git("add", "-A")
        (self.root / "map.tsv").write_text(
            "ovl1\tprefix\tone\tone_\t\thigh\tx\n"
            "ovl1\tfunc\tfunc_80100000\tone_call_resident\t\thigh\tx\n"
        )
        result = self.names("apply", "map.tsv")
        self.assertEqual(result.returncode, 1)
        self.assertIn(
            "error: decomp/include/shared.h:1: func_80100000: the targets reading it"
            " bind it to ovl1's, ovl2's",
            result.stderr,
        )
        self.assertNotIn("one_call_resident", self.read("decomp/src/ovl1/ovl1.c"))


if __name__ == "__main__":
    unittest.main()
