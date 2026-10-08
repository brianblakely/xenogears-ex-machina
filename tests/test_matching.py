"""Exact comparison failures and an authored MIPS-I assemble/link smoke test."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.matching import compare, main


class MatchingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.original = self.root / "original.bin"
        self.rebuilt = self.root / "rebuilt.bin"
        self.original.write_bytes(b"\x01\x02\x03\x04")
        self.rebuilt.write_bytes(self.original.read_bytes())
        self.digest = hashlib.sha256(self.original.read_bytes()).hexdigest()

    def test_equal_is_only_binary_agreement(self):
        result = compare(self.original, self.rebuilt, self.digest)
        self.assertTrue(result["matched"])
        self.assertEqual(result["source_coverage"], "not_measured")
        self.assertIsNone(result["first_difference"])

    def test_one_byte_difference_is_not_masked(self):
        self.rebuilt.write_bytes(b"\x01\x02\x00\x04")
        result = compare(self.original, self.rebuilt, self.digest)
        self.assertFalse(result["matched"])
        self.assertEqual(result["first_difference"], 2)

    def test_both_length_mismatches_fail(self):
        for data in (b"\x01\x02", b"\x01\x02\x03\x04\x00"):
            with self.subTest(data=data):
                self.rebuilt.write_bytes(data)
                result = compare(self.original, self.rebuilt, self.digest)
                self.assertFalse(result["matched"])
                self.assertEqual(result["first_difference"], min(4, len(data)))

    def test_wrong_original_rejects_even_equal_outputs(self):
        with self.assertRaisesRegex(ValueError, "fingerprint mismatch"):
            compare(self.original, self.rebuilt, "0" * 64)

    def test_same_file_and_hardlink_reject(self):
        with self.assertRaisesRegex(ValueError, "distinct"):
            compare(self.original, self.original, self.digest)
        self.rebuilt.unlink()
        self.rebuilt.hardlink_to(self.original)
        with self.assertRaisesRegex(ValueError, "distinct"):
            compare(self.original, self.rebuilt, self.digest)

    def test_invalid_hash_and_missing_input_reject(self):
        with self.assertRaises(ValueError):
            compare(self.original, self.rebuilt, "not-a-hash")
        self.rebuilt.unlink()
        with self.assertRaises(OSError):
            compare(self.original, self.rebuilt, self.digest)

    def test_empty_image_cannot_pass(self):
        self.original.write_bytes(b"")
        self.rebuilt.write_bytes(b"")
        with self.assertRaisesRegex(ValueError, "must not be empty"):
            compare(self.original, self.rebuilt, hashlib.sha256(b"").hexdigest())

    @unittest.skipUnless(shutil.which("make"), "GNU make is unavailable")
    def test_unconfigured_game_target_fails(self):
        root = Path(__file__).resolve().parents[1]
        result = subprocess.run(
            ["make", "-C", str(root / "decomp"), "verify"],
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Set CONFIG=targets/", result.stderr)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2",
            "psx-cpp-2.6.3", "psx-cc1-2.6.3", "maspsx", "psx-as",
        )),
        "enter the matching Nix shell to test cached compiler settings",
    )
    def test_c_object_rebuilds_when_settings_change_and_restore(self):
        repo = Path(__file__).resolve().parents[1]
        (self.root / "cache.c").write_text("int cache(int value) { return value + 1; }\n")
        (self.root / "fixture.ld").write_text("SECTIONS { .text : { *(.text) } }\n")
        (self.root / "fixture.mk").write_text(
            "ORIGINAL := original.bin\nORIGINAL_SHA256 := " + self.digest + "\n"
            "IMAGE := image.bin\nLINKER_SCRIPT := fixture.ld\n"
            "SPLAT_CONFIG := unused.yaml\nBUILD := build\nCC_VERSION := 2.7.2\n"
        )
        obj = self.root / "build/cache.o"
        stamp = obj.with_suffix(".cflags")

        def build(settings):
            result = subprocess.run(
                ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
                 "ROOT=" + str(self.root), "CONFIG=fixture.mk", *settings, str(obj)],
                cwd=self.root, text=True, capture_output=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return obj.stat().st_mtime_ns

        previous = build([])
        self.assertEqual(build([]), previous)
        changes = (
            (["CC_cache=2.6.3"], "CC=2.6.3"),
            (["CC_cache=2.6.3", "GP_cache=8"], "GP=8"),
            ([], "CC=2.7.2"),
            (["MASPSX_cache=--aspsx-version=2.56"], "MASPSX_FLAGS=--aspsx-version=2.56"),
            (["EXTERN_cache=absolute"], "EXTERN=absolute"),
            (["TARGET_CPPFLAGS=-DQUOTED='1'"], "-DQUOTED='1'"),
            (["CC1FLAGS=-quiet -mcpu=3000 -fgnu-linker -mgas -msoft-float -O1"], "-O1"),
            (["ASFLAGS=-EL -march=r3000 -mtune=r3000 -msoft-float -G0"], "ASFLAGS=-EL"),
            ([], "GP=0"),
        )
        for settings, signature in changes:
            with self.subTest(settings=settings):
                current = build(settings)
                self.assertGreater(current, previous)
                self.assertIn(signature, stamp.read_text())
                self.assertEqual(build(settings), current)
                previous = current

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "maspsx", "psx-as", "psx-objdump",
            "psx-cpp-2.6.3", "psx-cc1-2.6.3",
            "psx-cpp-2.7.2", "psx-cc1-2.7.2",
            "psx-cpp-2.7.2-cdk", "psx-cc1-2.7.2-cdk",
        )),
        "enter the matching Nix shell to test GTE memory inputs",
    )
    def test_gte_loads_consume_stores_and_evaluate_pointer_once(self):
        repo = Path(__file__).resolve().parents[1]
        worldmap = repo / "decomp/src/worldmap"
        menu = repo / "decomp/src/menu"
        menu_includes = (menu / "menu2.c").read_text().split("\n\n", 1)[0]
        menu_includes = re.sub(
            r'#include "([^"]+)"',
            lambda match: '#include "' + str(menu / match[1]) + '"', menu_includes,
        ) + "\n"
        # Compile the authored local macro without compiling unrelated game code.
        lines = (worldmap / "worldmap_80083A00.c").read_text().splitlines(True)
        start = next(i for i, line in enumerate(lines)
                     if line.startswith("#define gte_ldv3c("))
        end = start
        while lines[end].rstrip().endswith("\\"):
            end += 1
        ldv3c = "".join(lines[start:end + 1])
        (self.root / "decomp").mkdir()
        (self.root / "decomp/include").symlink_to(repo / "decomp/include")
        (self.root / "fixture.ld").write_text("SECTIONS { .text : { *(.text) } }\n")

        def instructions(obj, function):
            result = subprocess.run(
                ["psx-objdump", "-dr", "--disassemble=" + function, str(obj)],
                text=True, capture_output=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            words = [int(word, 16) for word in re.findall(
                r"^\s*[0-9a-f]+:\s+([0-9a-f]{8})\s", result.stdout, re.MULTILINE,
            )]
            self.assertTrue(words, result.stdout)
            return words, result.stdout

        # type, consumed component, direct call, side-effect call, store opcode,
        # component offset, memory-read opcodes and their count.
        v0 = ("SVECTOR", "point->vx", "gte_ldv0(point)",
              "gte_ldv0(get_point())", 0x29, 0, (0x32,), 2)
        cases = (
            ("worldmap_80083A00", "worldmap",
             '#include "' + str(worldmap / "worldmap.h") + '"\n' + ldv3c,
             (
                 ("v0", v0),
                 ("v3", ("SVECTOR", "point[2].vz",
                         "gte_ldv3(point, point + 1, point + 2)",
                         "gte_ldv3(get_point(), get_second(), get_third())",
                         0x29, 20, (0x32,), 6)),
                 ("lv0", ("VECTOR", "point->vx", "gte_ldlv0(point)",
                          "gte_ldlv0(get_point())", 0x2b, 0, (0x25, 0x32), 3)),
                 ("v3c", ("SVECTOR", "point[2].vz", "gte_ldv3c(point)",
                          "gte_ldv3c(get_point())", 0x29, 20, (0x32,), 6)),
             )),
            ("gte_shared", "worldmap",
             '#include "common.h"\n#include "psyq/libgte.h"\n'
             '#include "psyq/inline_c.h"\n', (("v0", v0),)),
            ("menu2", "menu", menu_includes,
             (
                 ("rot", ("Matrix", "point->m[2][2]", "gte_SetRotMatrix(point)",
                          "gte_SetRotMatrix(get_point())", 0x29, 16, (0x23,), 5)),
                 ("trans", ("Matrix", "point->t[0]", "gte_SetTransMatrix(point)",
                            "gte_SetTransMatrix(get_point())", 0x2b, 20, (0x23,), 3)),
             )),
        )
        for unit, target, includes, macros in cases:
            # Inherit each real unit's settings and the Makefile compiler recipe.
            (self.root / "fixture.mk").write_text(
                "include " + str(repo / ("decomp/targets/overlays/" + target + ".mk"))
                + "\nORIGINAL := original.bin\nORIGINAL_SHA256 := " + self.digest + "\n"
                + "IMAGE := image.bin\nLINKER_SCRIPT := fixture.ld\nBUILD := build\n"
            )
            source = includes + (
                "extern void *get_point(void), *get_second(void), *get_third(void);\n"
            )
            for name, (kind, component, direct, once, *_) in macros:
                source += (
                    f"void load_{name}({kind} *point) {{\n"
                    + f"    {component} = 1; {direct}; {component} = 2;\n}}\n"
                    + f"void once_{name}(void) {{ {once}; }}\n"
                )
            (self.root / (unit + ".c")).write_text(source)
            obj = self.root / ("build/" + unit + ".o")
            for compiler in ("2.6.3", "2.7.2", "2.7.2-cdk"):
                result = subprocess.run(
                    ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
                     "ROOT=" + str(self.root), "CONFIG=fixture.mk",
                     "CC_" + unit + "=" + compiler, str(obj)],
                    cwd=self.root, text=True, capture_output=True, check=False,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                for name, (_, _, _, once, store_op, offset, read_ops, count) in macros:
                    with self.subTest(unit=unit, compiler=compiler, macro=name):
                        words, assembly = instructions(obj, "once_" + name)
                        self.assertEqual(sum(
                            word >> 26 in read_ops and (word >> 21) & 31 != 29
                            for word in words
                        ), count, assembly)  # Exclude the getter's stack restores.
                        for getter in ("get_point", "get_second", "get_third"):
                            self.assertEqual(len(re.findall(
                                r"R_MIPS_26\s+" + getter + r"\b", assembly,
                            )), int(getter in once), assembly)
                        words, assembly = instructions(obj, "load_" + name)
                        # These two stores write 1 then 2 to the consumed
                        # component. Neither may be deleted or cross the GTE.
                        stores = [i for i, word in enumerate(words)
                                  if word >> 26 == store_op and (word >> 21) & 31 != 29]
                        loads = [i for i, word in enumerate(words)
                                 if word >> 26 in read_ops and (word >> 21) & 31 != 29]
                        self.assertEqual(len(stores), 2, assembly)
                        self.assertEqual(len(loads), count, assembly)
                        self.assertEqual([words[i] & 0xffff for i in stores],
                                         [offset, offset], assembly)
                        self.assertLess(stores[0], loads[0], assembly)
                        self.assertGreater(stores[1], loads[-1], assembly)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx",
            "psx-as", "psx-objdump",
        )),
        "enter the matching Nix shell to test the return-address memory output",
    )
    def test_return_address_capture_reloads_output_and_evaluates_pointer_once(self):
        repo = Path(__file__).resolve().parents[1]
        (self.root / "decomp").mkdir()
        (self.root / "decomp/include").symlink_to(repo / "decomp/include")
        (self.root / "heap.c").write_text(
            '#include "' + str(repo / "decomp/src/resident/heap.h") + '"\n'
            + "u32 read_caller(void) {\n"
            + "    u32 caller = 0; GET_RA(&caller); return caller;\n}\n"
            + "extern u32 *caller_slot(void);\n"
            + "void write_caller(void) { GET_RA(caller_slot()); }\n"
        )
        (self.root / "fixture.ld").write_text("SECTIONS { .text : { *(.text) } }\n")
        (self.root / "fixture.mk").write_text(
            "include " + str(repo / "decomp/targets/resident/slus_006.64.mk") + "\n"
            + "ORIGINAL := original.bin\nORIGINAL_SHA256 := " + self.digest + "\n"
            + "IMAGE := image.bin\nLINKER_SCRIPT := fixture.ld\nBUILD := build\n"
        )
        obj = self.root / "build/heap.o"
        result = subprocess.run(
            ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
             "ROOT=" + str(self.root), "CONFIG=fixture.mk", str(obj)],
            cwd=self.root, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run(
            ["psx-objdump", "-dr", "--disassemble=read_caller", str(obj)],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        words = [int(word, 16) for word in re.findall(
            r"^\s*[0-9a-f]+:\s+([0-9a-f]{8})\s", result.stdout, re.MULTILINE,
        )]
        # The assembly stores $ra; returning the initialized zero would hide
        # that write. The return register must reload the captured word.
        stores = [i for i, word in enumerate(words)
                  if word >> 26 == 0x2b and (word >> 16) & 31 == 31]
        reloads = [i for i, word in enumerate(words)
                   if word >> 26 == 0x23 and (word >> 16) & 31 == 2]
        self.assertEqual(len(stores), 1, result.stdout)
        self.assertEqual(len(reloads), 1, result.stdout)
        self.assertGreater(reloads[0], stores[0], result.stdout)
        result = subprocess.run(
            ["psx-objdump", "-dr", "--disassemble=write_caller", str(obj)],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(re.findall(r"R_MIPS_26\s+caller_slot\b", result.stdout)),
                         1, result.stdout)

    def test_cli_exit_codes(self):
        args = [str(self.original), str(self.rebuilt), "--sha256", self.digest]
        self.assertEqual(main(args), 0)
        self.rebuilt.write_bytes(b"wrong")
        self.assertEqual(main(args), 1)
        self.rebuilt.unlink()
        self.assertEqual(main(args), 2)

    @unittest.skipUnless(shutil.which("cc"), "a host C compiler is unavailable")
    def test_permuter_import_keeps_inline_helpers_for_selected_function(self):
        from tools.permuter_import import strip_other_functions

        source = (
            "extern int unavailable(void);\n"
            "static inline int add_one(int value) { return value + 1; }\n"
            "extern inline int declared_only(int value);\n"
            "int unrelated(void) { return unavailable(); }\n"
            "static __inline__ int twice(int value) { return value * 2; }\n"
            "int selected(void) { return twice(add_one(20)); }\n"
        )
        candidate = self.root / "candidate.c"
        executable = self.root / "candidate"
        candidate.write_text(
            strip_other_functions(source, "selected")
            + "int main(void) { return selected() != 42; }\n"
        )
        result = subprocess.run(
            ["cc", "-O2", str(candidate), "-o", str(executable)],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(subprocess.run([str(executable)], check=False).returncode, 0)

    @unittest.skipUnless(
        shutil.which("psx-as") and shutil.which("psx-ld") and shutil.which("psx-objcopy"),
        "enter the matching Nix shell to test MIPS binutils",
    )
    def test_mips_assemble_link_and_exact_bytes(self):
        assembly = self.root / "fixture.s"
        linker = self.root / "fixture.ld"
        obj = self.root / "fixture.o"
        elf = self.root / "fixture.elf"
        assembly.write_text(
            '.section .text,"ax"\n.set noreorder\n.globl fixture\nfixture:\n'
            "addiu $2,$0,7\njr $31\nnop\n"
        )
        linker.write_text(
            "SECTIONS { .text 0x80010000 : { *(.text) } "
            "/DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) "
            "*(.comment) *(.gnu.attributes) } }\n"
        )
        subprocess.run(
            ["psx-as", "-EL", "-mips1", "-mabi=32", "-G0", "-o", str(obj), str(assembly)],
            check=True,
        )
        subprocess.run(["psx-ld", "-EL", "-T", str(linker), "-o", str(elf), str(obj)], check=True)
        subprocess.run(
            ["psx-objcopy", "-O", "binary", "-j", ".text", str(elf), str(self.rebuilt)], check=True
        )
        self.original.write_bytes(struct.pack("<4I", 0x24020007, 0x03E00008, 0, 0))
        digest = hashlib.sha256(self.original.read_bytes()).hexdigest()
        self.assertTrue(compare(self.original, self.rebuilt, digest)["matched"])

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("psx-as", "psx-ld", "psx-readelf")),
        "enter the matching Nix shell to test coverage counts",
    )
    def test_coverage_counts_remaining_functions_and_instructions(self):
        repo = Path(__file__).resolve().parents[1]
        assembly = self.root / "coverage.s"
        obj = self.root / "coverage.o"
        elf = self.root / "coverage.elf"
        linker = self.root / "coverage.ld"
        source = self.root / "coverage.c"
        classification = self.root / "classification.txt"
        cases = {"c": 3, "asm": 6, "nonmatching": 4, "sdk": 5, "handwritten": 2}
        lines = ['.section .text,"ax"', ".set noreorder",
                 ".globl fixture_TEXT_START", "fixture_TEXT_START:"]
        ranges = []
        address = 0x80010000
        for cls, count in cases.items():
            name = "covered_" + cls
            lines += [".globl " + name, ".type " + name + ", @function", name + ":"]
            lines += ["nop"] * (count - 2) + ["jr $31", "nop"]
            lines += [".size " + name + ", .-" + name, ".space 4"]
            if cls in ("sdk", "handwritten"):
                ranges.append(f"{address:08x} {address + count * 4:08x} {cls}")
            address += (count + 1) * 4
        lines += [
            ".globl fixture_TEXT_END", "fixture_TEXT_END:",
            ".globl __maspsx_include_asm_hack_fixture",
            ".type __maspsx_include_asm_hack_fixture, @function",
            ".set __maspsx_include_asm_hack_fixture, covered_c",
            '.section .rodata,"a"', ".type outside_text, @function",
            "outside_text:", ".word 42", ".size outside_text, .-outside_text",
        ]
        assembly.write_text("\n".join(lines) + "\n")
        linker.write_text("SECTIONS { .text 0x80010000 : { *(.text) } .rodata : { *(.rodata) } }\n")
        source.write_text(
            'int covered_c(void) { return 0; }\n'
            'INCLUDE_ASM("fixture", covered_asm);\n'
            '#ifdef NON_MATCHING\nint covered_nonmatching(void) { return 1; }\n'
            '#else\nINCLUDE_ASM("fixture", covered_nonmatching);\n#endif\n'
            'INCLUDE_ASM("fixture", covered_sdk);\n'
            'INCLUDE_ASM("fixture", covered_handwritten);\n'
        )
        classification.write_text("\n".join(ranges) + "\n")
        subprocess.run(["psx-as", "-EL", "-mips1", "-o", str(obj), str(assembly)], check=True)
        subprocess.run(["psx-ld", "-EL", "-T", str(linker), "-o", str(elf), str(obj)], check=True)
        result = subprocess.run(
            [sys.executable, str(repo / "tools/matching_coverage.py"), str(elf),
             "--src", str(self.root), "--classification", str(classification)],
            text=True, capture_output=True, check=True,
        )
        report = json.loads(result.stdout)
        self.assertEqual(report["binary_agreement"], "not_measured")
        self.assertEqual(report["text_bytes"], 80)
        self.assertEqual(report["text_instructions"], 20)
        self.assertEqual(report["remaining_asm_functions"], 2)
        self.assertEqual(report["remaining_asm_bytes"], 40)
        self.assertEqual(report["remaining_asm_instructions"], 10)
        self.assertEqual(report["classes"], {
            cls: {"functions": 1, "bytes": count * 4, "instructions": count}
            for cls, count in cases.items()
        })

    def test_data_coverage_attributes_map_sections_by_object(self):
        from tools.matching_coverage import data_coverage, map_sections

        build = ".local/decomp/build/t"
        (self.root / "decomp/src/t").mkdir(parents=True)
        (self.root / "decomp/src/t/unit.c").write_text("int x = 1;\n")
        mapfile = self.root / "image.map"
        mapfile.write_text(
            f" .rodata        0x80010000       0x20 {build}/decomp/src/t/unit.o\n"
            f" .rodata.str1.4\n                0x80010020        0x8 {build}/decomp/src/t/unit.o\n"
            f" .text          0x80010028       0x40 {build}/decomp/src/t/unit.o\n"
            f" .data          0x80010068        0x0 {build}/decomp/src/t/unit.o\n"
            f" .data          0x80010068       0x30 {build}/.local/decomp/t/asm/data/t.data.o\n"
            f" .sdata         0x80010098        0x8 {build}/decomp/src/t/unit.o\n"
        )
        sections = map_sections(mapfile)
        self.assertEqual([(n, a, s) for n, a, s, _ in sections], [
            (".rodata", 0x80010000, 0x20), (".rodata.str1.4", 0x80010020, 8),
            (".data", 0x80010068, 0x30), (".sdata", 0x80010098, 8),
        ])
        ranges = [(0x80010070, 0x80010078, "sdk", ""), (0x80010090, 0x800100a0, "asset", "")]
        totals = data_coverage(sections, [(0x80010004, 0x80010010)], ranges, self.root)
        self.assertEqual(totals, {
            "c": 0x20 - 12 + 8 + 8 - 8, "included": 12,
            "placeholder": 0x30 - 8 - 8, "sdk": 8, "asset": 8 + 8,
        })

    def build_fixture_unit(self, source, settings=()):
        """Compile decomp/src/t/unit.c with the target Makefile; returns the object."""
        repo = Path(__file__).resolve().parents[1]
        include = self.root / "decomp/include"
        include.mkdir(parents=True, exist_ok=True)
        for name in ("include_asm.h", "macro.inc"):
            shutil.copy(repo / "decomp/include" / name, include / name)
        unit = self.root / "decomp/src/t/unit.c"
        unit.parent.mkdir(parents=True, exist_ok=True)
        unit.write_text(source)
        (self.root / "fixture.ld").write_text("SECTIONS { .text : { *(.text) } }\n")
        (self.root / "fixture.mk").write_text(
            "ORIGINAL := original.bin\nORIGINAL_SHA256 := " + self.digest + "\n"
            "IMAGE := image.bin\nLINKER_SCRIPT := fixture.ld\n"
            "SPLAT_CONFIG := unused.yaml\nBUILD := build\nCC_VERSION := 2.7.2\n"
        )
        obj = self.root / "build/decomp/src/t/unit.o"
        result = subprocess.run(
            ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
             "ROOT=" + str(self.root), "CONFIG=fixture.mk", *settings, str(obj)],
            cwd=self.root, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return obj

    def section_bytes(self, obj, section):
        out = self.root / (section.strip(".") + ".bin")
        subprocess.run(["psx-objcopy", "-O", "binary", "-j", section, str(obj), str(out)], check=True)
        return out.read_bytes()

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test original data objects",
    )
    def test_original_object_links_in_place_and_counts_as_included(self):
        from tools.matching_coverage import data_coverage, source_names

        self.original.write_bytes(bytes(range(16)))
        self.digest = hashlib.sha256(self.original.read_bytes()).hexdigest()
        obj = self.build_fixture_unit(
            '#include "include_asm.h"\n'
            "int before = 0x11111111;\n"
            '/* a byte object whose padding holds 05 06 07 */\n'
            'INCLUDE_ORIGINAL(".data", D_80010004, 0x80010004, 4);\n'
            "int after = 0x22222222;\n",
            ["TARGET_CPPFLAGS=-DORIGINAL_BASE=0x80010000"],
        )
        self.assertEqual(
            self.section_bytes(obj, ".data"),
            struct.pack("<I", 0x11111111) + bytes([4, 5, 6, 7]) + struct.pack("<I", 0x22222222),
        )
        symbols = subprocess.run(["psx-readelf", "-sW", str(obj)], check=True,
                                 capture_output=True, text=True).stdout
        self.assertRegex(symbols, r"\s4 NOTYPE\s+GLOBAL\s+DEFAULT\s+\d+ D_80010004\n")
        _asm, _nonmatching, included = source_names([self.root / "decomp/src"])
        self.assertEqual(included, {"D_80010004"})
        sections = [(".data", 0x80010000, 12, "build/decomp/src/t/unit.o")]
        totals = data_coverage(sections, [(0x80010004, 0x80010008)], [], self.root)
        self.assertEqual(totals, {"c": 8, "included": 4})

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as", "psx-readelf",
        )),
        "enter the matching Nix shell to test uninitialized variable slots",
    )
    def test_bss_slots_give_each_uninitialized_variable_whole_words(self):
        source = (
            '#include "include_asm.h"\n'
            "static unsigned char flag;\n"
            "unsigned char pairs[3][2];\n"
            "static short half;\n"
            "int word;\n"
            "int use(void) { return flag + pairs[1][1] + half + word; }\n"
        )

        def layout(settings):
            obj = self.build_fixture_unit(source, settings)
            symbols = subprocess.run(["psx-readelf", "-sW", str(obj)], check=True,
                                     capture_output=True, text=True).stdout
            sections = subprocess.run(["psx-readelf", "-SW", str(obj)], check=True,
                                      capture_output=True, text=True).stdout
            bss = re.search(r"\] \.bss\s+NOBITS\s+\w+\s+\w+\s+(\w+)", sections)
            offsets = {
                name: int(value, 16)
                for value, name in re.findall(r"\d+: ([0-9a-f]{8})\s+\d+ \w+\s+\w+\s+\w+\s+\d+ (\w+)\n", symbols)
                if name in ("flag", "pairs", "half", "word")
            }
            return offsets, int(bss.group(1), 16)

        # GCC emits the file-scope tentative definitions in declaration order;
        # maspsx packs them, ASPSX's slots keep every one word-aligned.
        self.assertEqual(layout([]), ({"flag": 0, "pairs": 1, "half": 7, "word": 9}, 13))
        self.assertEqual(layout(["BSS=slots"]), ({"flag": 0, "pairs": 4, "half": 12, "word": 16}, 20))
        self.assertIn("BSS=slots", (self.root / "build/decomp/src/t/unit.cflags").read_text())

    @unittest.skipUnless(importlib.util.find_spec("rabbitizer"), "enter the matching Nix shell")
    def test_service_scan_tracks_constant_bases_calls_and_cop2(self):
        from tools.service_calls import scan_function

        words = [
            0x3C041F80,  # lui   a0, 0x1f80
            0x34841814,  # ori   a0, a0, 0x1814  (GPU status port)
            0x8C850000,  # lw    a1, 0(a0)
            0xA0850010,  # sb    a1, 0x10(a0)   (a0 still holds 0x1f801814)
            0x3C061F80,  # lui   a2, 0x1f80
            0xACC00100,  # sw    zero, 0x100(a2)  (scratchpad)
            0x3C028005,  # lui   v0, 0x8005
            0x8C4208E4,  # lw    v0, 0x8e4(v0)   (hardware pointer global)
            0x0C010000,  # jal   0x80040000
            0x4A280030,  # rtpt
            0x0040F809,  # jalr  v0
            0x03E00008,  # jr    ra
        ]
        blob = b"".join(w.to_bytes(4, "little") for w in words)
        found = scan_function(blob, 0x80010000, {0x800508E4: 0x1F801C00})
        self.assertEqual(found["io"], [0x1F801814, 0x1F801824])
        self.assertEqual(found["scratchpad"], [0x1F800100])
        self.assertEqual(found["hardware_pointers"], [0x800508E4])
        self.assertEqual(found["calls"], [0x80040000])
        self.assertEqual((found["gte"], found["indirect"]), (1, 1))

    @unittest.skipUnless(importlib.util.find_spec("rabbitizer"), "enter the matching Nix shell")
    def test_instruction_differences_keep_immediates_and_absent_words(self):
        from tools.nonmatching_score import instruction_differences

        original = struct.pack("<3I", 0x24020007, 0x03E00008, 0)
        self.assertEqual(instruction_differences(original, original), 0)
        self.assertEqual(instruction_differences(
            original, struct.pack("<3I", 0x24020008, 0x03E00008, 0)), 1)
        self.assertEqual(instruction_differences(original, original + bytes(4)), 1)
        self.assertEqual(instruction_differences(original, original[:-4]), 1)
        self.assertEqual(instruction_differences(
            original, original[:4] + bytes(4) + original[4:]), 3)
        with self.assertRaisesRegex(ValueError, "word-aligned"):
            instruction_differences(original, b"bad")

    @unittest.skipUnless(
        importlib.util.find_spec("rabbitizer") and all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )), "enter the matching Nix shell to test linked draft differences",
    )
    def test_draft_audit_counts_relocated_field_offset_and_preserves_baseline(self):
        from tools import nonmatching_score

        repo = Path(__file__).resolve().parents[1]
        decomp = self.root / "decomp"
        source_dir = decomp / "src/fixture"
        source_dir.mkdir(parents=True)
        (decomp / "include").symlink_to(repo / "decomp/include")
        shutil.copyfile(repo / "decomp/Makefile", decomp / "Makefile")
        source = source_dir / "fixture.c"
        source.write_text(
            "extern int external_global[];\nint draft(void) { return external_global[0]; }\n"
        )
        linker = self.root / "fixture.ld"
        linker.write_text(
            "external_global = 0x80020000; _gp = 0x80059170;\n"
            "SECTIONS { .text 0x80010000 : AT(0) { fixture_TEXT_START = .;\n"
            ".local/build/decomp/src/fixture/fixture.o(.text)\n"
            "fixture_TEXT_END = .; } /DISCARD/ : { *(*) } }\n"
        )
        config = decomp / "fixture.mk"
        contents = (
            "ORIGINAL := original.bin\nORIGINAL_SHA256 := " + self.digest + "\n"
            "IMAGE := baseline.bin\nLINKER_SCRIPT := fixture.ld\n"
            "SPLAT_CONFIG := unused.yaml\nBUILD := .local/build\nCC_VERSION := 2.7.2\n"
            "SOURCE_DIRS := decomp/src/fixture\n"
        )
        config.write_text(contents)
        baseline = self.root / "baseline.bin"
        subprocess.run(
            ["make", "-s", "-C", str(decomp), "CONFIG=fixture.mk", str(baseline)], check=True,
        )
        original = baseline.read_bytes()
        self.original.write_bytes(original)
        config.write_text(contents.replace(self.digest, hashlib.sha256(original).hexdigest()))
        shutil.copyfile(
            self.root / ".local/build/decomp/src/fixture/fixture.o.s", source_dir / "draft.s",
        )
        source.write_text(
            '#include "include_asm.h"\nextern int external_global[];\n'
            '#ifdef NON_MATCHING\nint draft(void) { return external_global[1]; }\n'
            '#else\nINCLUDE_ASM("decomp/src/fixture", draft);\n#endif\n'
        )
        with patch.object(nonmatching_score, "ROOT", self.root):
            report = nonmatching_score.audit_drafts(config, nonmatching_score.config(config), [])
            self.assertEqual(report["differing_functions"], 1)
            self.assertEqual(report["differing_instructions"], 1)
            self.assertEqual(report["original_instructions"], report["candidate_instructions"])
            self.assertEqual(baseline.read_bytes(), original)
            baseline.write_bytes(bytes(len(original)))
            with self.assertRaisesRegex(ValueError, "baseline image does not match"):
                nonmatching_score.audit_drafts(config, nonmatching_score.config(config), [])


if __name__ == "__main__":
    unittest.main()
