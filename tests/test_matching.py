"""Exact comparison failures and an authored MIPS-I assemble/link smoke test."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
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
            (["SBSS_cache=8"], "SBSS=8"),
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

    def test_map_sections_skip_discarded_input_and_join_long_names(self):
        from tools.matching_coverage import map_sections

        unit = ".local/decomp/build/t/decomp/src/t/unit.o"
        data = ".local/decomp/build/t/.local/decomp/t/asm/data/t.data.o"
        mapfile = self.root / "image.map"
        mapfile.write_text(
            "Discarded input sections\n\n"
            f" .reginfo       0x00000000       0x18 {unit}\n"
            f" .rodata        0x00000000       0x10 {unit}\n\n"
            "Memory Configuration\n\n"
            "Linker script and memory map\n\n"
            f" .rodata        0x80010000       0x20 {unit}\n"
            "                0x80010000                D_80010000\n"
            f" .rodata.str1.4\n                0x80010020        0x8 {unit}\n"
            f" .text          0x80010028       0x40 {unit}\n"
            f" .data          0x80010068        0x0 {unit}\n"
            " *fill*         0x80010068        0x8 \n"
            f" .data          0x80010070       0x30 {data}\n"
        )
        self.assertEqual(map_sections(mapfile), [
            (".rodata", 0x80010000, 0x20, unit), (".rodata.str1.4", 0x80010020, 8, unit),
            (".text", 0x80010028, 0x40, unit), (".data", 0x80010070, 0x30, data),
        ])

    def test_only_a_string_with_a_stray_padding_byte_needs_no_reason(self):
        from tools.matching_coverage import stray_padding

        for data, stray in (
            (b"ply_01\0o", True),  # menu6's gear list: a stray 'o'
            (b"Size%9d\n\0\0\x94\x08", True),
            (b"ab\0\0", False),  # zero padding, as C emits it
            (b"abc\0", False),  # no padding
            (b"ab\0\0c\0\0\0", False),  # two strings
            (b"\x01\0\x04\0", False),  # a data object: its size is not in its bytes
            (b"\0\x04\0\0", False),
            (b"abcd", False),  # no terminator
        ):
            with self.subTest(data):
                self.assertEqual(stray_padding(data), stray)

    def test_mark_asm_labels_the_text_of_each_asm_statement(self):
        from tools.matching_coverage import mark_asm

        source = (
            '# 1 "decomp/src/t/unit.c"\n'
            '__asm__(".include \\"macro.inc\\"\\n");\n'
            'register int pinned asm("$14");\n'
            'int renamed(void) asm("other");\n'
            "void __maspsx_include_asm_hack_f() {\n"
            '    __asm__(".text # maspsx-keep\\n" "\\t.set at # maspsx-keep\\n");\n'
            "}\n"
            "int g(int *p) {\n"
            '    register int (*call)(void) asm("$15");\n'
            '    char *s = "asm(\\"no\\")";\n'
            '    if (p) __asm__ volatile("lwc2 $0, 0(%0)" : : "r"(p));\n'
            '    else asm("nop");\n'
            '    __asm__ const("break 1");\n'
            "    return ';';\n"
            "}\n"
        )
        self.assertEqual(mark_asm(source), (
            '# 1 "decomp/src/t/unit.c"\n'
            '__asm__("Lcovb_0:\\n\\t" ".include \\"macro.inc\\"\\n" "\\nLcove_0:");\n'
            'register int pinned asm("$14");\n'
            'int renamed(void) asm("other");\n'
            "void __maspsx_include_asm_hack_f() {\n"
            '    __asm__("Lcovb_1: # maspsx-keep\\n\\t" ".text # maspsx-keep\\n" '
            '"\\t.set at # maspsx-keep\\n" "\\nLcove_1: # maspsx-keep");\n'
            "}\n"
            "int g(int *p) {\n"
            '    register int (*call)(void) asm("$15");\n'
            '    char *s = "asm(\\"no\\")";\n'
            '    if (p) __asm__ volatile("Lcovb_2:\\n\\t" "lwc2 $0, 0(%0)" "\\nLcove_2:"'
            ' : : "r"(p));\n'
            '    else asm("Lcovb_3:\\n\\t" "nop" "\\nLcove_3:");\n'
            '    __asm__ const("Lcovb_4:\\n\\t" "break 1" "\\nLcove_4:");\n'
            "    return ';';\n"
            "}\n"
        ))

    def test_marking_refuses_text_cc1_copies_and_macros_take_registers(self):
        from tools.matching_coverage import ORIGINAL_ASM, mark_asm, original_pattern

        # cc1 copies a declaration's asm name, a section or alias attribute
        # and a line marker's file name into its output as they are: the
        # coverage build accepts only plain names, which carry no lines.
        plain = (
            '# 1 "decomp/src/t/unit.c"\n'
            '# 1 "decomp/include/include_asm.h" 1\n'
            'register int pinned asm("$14");\n'
            'extern int word __asm__("D_800CCB34");\n'
            'int table[] __attribute__((section(".text"))) = { 1 };\n'
        )
        self.assertEqual(mark_asm(plain), plain)
        for text, message in (
            ('extern int v asm("D_1\\n\\t.word 0x24020001\\n\\t#");\n', "asm name"),
            ('extern int v asm("D_1;.word 0x24020001");\n', "asm name"),
            ('int t[] __attribute__((section(".text\\n\\t.word 1\\n\\t#"))) = { 1 };\n',
             "section attribute"),
            ('void g(void) __attribute__((__alias__("f\\n\\t.word 1")));\n', "__alias__ attribute"),
            ('# 1 "x\\n\\t.word 0x24020001\\n\\t#"\n', "not a line marker"),
            ("#pragma weak f\n", "not a line marker"),
        ):
            with self.subTest(text), self.assertRaises(SystemExit) as caught:
                mark_asm(text)
            self.assertIn(message, str(caught.exception))
        # An original-style macro's text in the cc1 output has a register for
        # each operand, the same one at each use (cc1 names $29/$30 $sp/$fp).
        ldv0 = original_pattern("lwc2 $0, 0(%0);lwc2 $1, 4(%0)")
        self.assertIn("lwc2 $0, 0(%0);lwc2 $1, 4(%0)", ORIGINAL_ASM)
        self.assertTrue(ldv0.fullmatch("lwc2 $0, 0($4);lwc2 $1, 4($4)"))
        self.assertTrue(ldv0.fullmatch("lwc2 $0, 0($sp);lwc2 $1, 4($sp)"))
        self.assertFalse(ldv0.fullmatch("lwc2 $0, 0($4);lwc2 $1, 4($5)"))
        self.assertFalse(ldv0.fullmatch("lwc2 $0, 0($4);lwc2 $1, 4($4);.word 0"))
        self.assertFalse(original_pattern("mtc2 %0, $8").fullmatch("mtc2 5, $8"))
        enter = original_pattern("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8")
        self.assertTrue(
            enter.fullmatch("move $8, $9\nsw $29, 0($8)\naddiu $8, $8, -4\nmove $29, $8"))

    def write_fixture(self, source, root=None):
        """decomp/src/t/unit.c, the include files it may use and a target
        configuration for it at root (the test's own by default)."""
        repo = Path(__file__).resolve().parents[1]
        root = root or self.root
        include = root / "decomp/include"
        include.mkdir(parents=True, exist_ok=True)
        for name in ("include_asm.h", "macro.inc"):
            shutil.copy(repo / "decomp/include" / name, include / name)
        unit = root / "decomp/src/t/unit.c"
        unit.parent.mkdir(parents=True, exist_ok=True)
        unit.write_text(source)
        digest = hashlib.sha256((root / "original.bin").read_bytes()).hexdigest()
        (root / "fixture.ld").write_text("SECTIONS { .text : { *(.text) } }\n")
        (root / "fixture.mk").write_text(
            "ORIGINAL := original.bin\nORIGINAL_SHA256 := " + digest + "\n"
            "IMAGE := image.bin\nLINKER_SCRIPT := fixture.ld\n"
            "SPLAT_CONFIG := unused.yaml\nBUILD := build\nCC_VERSION := 2.7.2\n"
        )

    def build_fixture_unit(self, source, settings=()):
        """Compile decomp/src/t/unit.c with the target Makefile; returns the object."""
        obj, result = self.make_fixture_unit(source, settings)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return obj

    def make_fixture_unit(self, source, settings=()):
        """Run the target Makefile's object rule on decomp/src/t/unit.c."""
        repo = Path(__file__).resolve().parents[1]
        self.write_fixture(source)
        obj = self.root / "build/decomp/src/t/unit.o"
        result = subprocess.run(
            ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
             "ROOT=" + str(self.root), "CONFIG=fixture.mk", *settings, str(obj)],
            cwd=self.root, text=True, capture_output=True, check=False,
        )
        return obj, result

    def section_bytes(self, obj, section):
        out = self.root / (section.strip(".") + ".bin")
        subprocess.run(["psx-objcopy", "-O", "binary", "-j", section, str(obj), str(out)],
                       check=True)
        return out.read_bytes()

    def cover_linked_fixture(self, image, settings=(), sections=(".data", ".bss"), arguments=(),
                             root=None, extra=(), noload=()):
        """Link the fixture unit's `sections` (loaded) at 0x80010000, then the
        `extra` input sections, and the `noload` input sections after them in
        a NOLOAD section (4-byte aligned, as splat's scripts), and build its
        coverage object with the target Makefile; then run the coverage
        report on the ELF and map."""
        repo = Path(__file__).resolve().parents[1]
        root = root or self.root
        inputs = "".join(f"    build/decomp/src/t/unit.o({section})\n" for section in sections)
        inputs += "".join(f"    {line}\n" for line in extra)
        unloaded = "".join(f"    {line}\n" for line in noload)
        (root / "fixture.ld").write_text(
            "SECTIONS {\n  .fixture 0x80010000 : AT(0) {\n" + inputs + "  }\n"
            + (f"  .fixture_bss (NOLOAD) : SUBALIGN(4) {{\n{unloaded}  }}\n" if noload else "")
            + "  /DISCARD/ : { *(*) }\n}\n"
        )
        build = subprocess.run(
            ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
             "ROOT=" + str(root), "CONFIG=fixture.mk", "IMAGE=" + image,
             "TARGET_CPPFLAGS=-DORIGINAL_BASE=0x80010000", *settings,
             str(root / image), str(root / "build/decomp/src/t/unit.cov.o")],
            cwd=root, text=True, capture_output=True, check=False,
        )
        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
        return subprocess.run(
            [sys.executable, str(repo / "tools/matching_coverage.py"), image + ".elf",
             "--map", image + ".map", "--src", "decomp/src", *arguments],
            cwd=root, text=True, capture_output=True, check=False,
        )

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test original data objects",
    )
    def test_original_object_links_in_place_and_counts_as_included(self):
        self.original.write_bytes(bytes(range(16)))
        obj = self.build_fixture_unit(
            '#include "include_asm.h"\n'
            "int before = 0x11111111;\n"
            '/* a byte object whose padding holds 05 06 07 */\n'
            'INCLUDE_ORIGINAL(".data", D_80010004, 0x80010004, 4);\n'
            "int after = 0x22222222;\n"
            "static int counter;\n"
            "int *count(void) { return &counter; }\n",
            ["TARGET_CPPFLAGS=-DORIGINAL_BASE=0x80010000"],
        )
        self.assertEqual(
            self.section_bytes(obj, ".data"),
            struct.pack("<I", 0x11111111) + bytes([4, 5, 6, 7]) + struct.pack("<I", 0x22222222),
        )
        symbols = subprocess.run(["psx-readelf", "-sW", str(obj)], check=True,
                                 capture_output=True, text=True).stdout
        self.assertRegex(symbols, r"\s4 NOTYPE\s+GLOBAL\s+DEFAULT\s+\d+ D_80010004\n")
        # An included object that is not a string with a stray byte in its
        # padding stays original only with a reviewed reason: an `included`
        # line of the classification naming its range and label.
        result = self.cover_linked_fixture("image.bin")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("D_80010004 (80010004-80010008) has no stray byte", result.stderr)
        classification = self.root / "classification.txt"
        reviewed = ["--classification", "classification.txt"]
        for line, message in (
            ("80010004 80010008 included D_80010005 a byte flag\n", "has no stray byte"),
            ("80010004 8001000c included D_80010004 a byte flag\n", "has no stray byte"),
            ("80010004 80010008 included D_80010004\n", "names its object and the reason"),
        ):
            with self.subTest(line):
                classification.write_text(line)
                result = self.cover_linked_fixture("image.bin", arguments=reviewed)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
        classification.write_text(
            "80010004 80010008 included D_80010004 a byte object whose padding holds 05 06 07\n")
        # The report counts every loaded byte by where GAS put it, the loaded
        # .bss included.
        result = self.cover_linked_fixture("image.bin", arguments=reviewed)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["data_classes"], {"bss": 4, "c": 8, "included": 4})
        self.assertEqual(report["remaining_data_placeholder_bytes"], 0)
        # A data class lists its ranges with their section and object.
        for cls, ranges in (("included", ["80010004      4 D_80010004 (.data unit.o)"]),
                            ("c", ["80010000      4 (.data unit.o)",
                                   "80010008      4 (.data unit.o)"]),
                            ("bss", ["80010010      4 (.bss unit.o)"])):
            listing = self.cover_linked_fixture("image.bin", arguments=reviewed + ["--list", cls])
            self.assertEqual(listing.stdout.splitlines(), ranges, listing.stderr)
        # A linker-script assignment shadowing the name changes nothing.
        (self.root / "shadow.ld").write_text("D_80010004 = 0x80010004;\n")
        result = self.cover_linked_fixture("shadow.bin", ["LINKER_EXTRA=shadow.ld"],
                                           arguments=reviewed)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["data_classes"],
                         {"bss": 4, "c": 8, "included": 4})
        # A line must name an included object.
        classification.write_text(classification.read_text()
                                  + "80010008 8001000c included after not original\n")
        result = self.cover_linked_fixture("image.bin", arguments=reviewed)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("included line after (80010008-8001000c): no included object there",
                      result.stderr)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf", "psx-nm",
        )),
        "enter the matching Nix shell to test the link's symbol files",
    )
    def test_splat_symbol_files_never_override_a_definition(self):
        # splat assigns an address to every name its assembly used; the link
        # reads its files as PROVIDE, so the unit's own definition stands and
        # only a name that no object defines takes the file's address.
        self.build_fixture_unit("int defined = 1;\nextern int elsewhere;\n"
                                "int *use(void) { return &elsewhere; }\n")
        auto = self.root / "auto/undefined_syms_auto.txt"
        auto.parent.mkdir()
        auto.write_text("defined = 0x80020000;\nelsewhere = 0x80030000;\nunused = 0x80040000;\n")
        (self.root / "pad.ld").write_text("__file_end = 0x40;\n")
        settings = ["LINKER_EXTRA=auto/undefined_syms_auto.txt pad.ld", "PAD_TO_SYMBOL=__file_end"]
        self.cover_linked_fixture("image.bin", settings, sections=(".text", ".data"))
        symbols = {
            fields[2]: (int(fields[0], 16), fields[1])
            for fields in map(str.split, subprocess.run(
                ["psx-nm", str(self.root / "image.bin.elf")], check=True, capture_output=True,
                text=True).stdout.splitlines())
            if len(fields) == 3
        }
        self.assertNotEqual(symbols["defined"][1], "A")  # the unit's, in the image
        self.assertNotEqual(symbols["defined"][0], 0x80020000)
        self.assertEqual(symbols["elsewhere"], (0x80030000, "A"))
        self.assertNotIn("unused", symbols)
        # PAD_TO_SYMBOL pads the file to that symbol of the link.
        self.assertEqual((self.root / "image.bin").stat().st_size, 0x40)
        repo = Path(__file__).resolve().parents[1]
        for setting, message in (("PAD_TO_SYMBOL=__missing", "--pad-to"),
                                 ("LINKER_EXTRA=auto/undefined_syms_auto.txt", "not a symbol")):
            with self.subTest(setting):
                if setting.startswith("LINKER_EXTRA"):
                    auto.write_text("INCLUDE other.ld\n")
                result = subprocess.run(
                    ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
                     "ROOT=" + str(self.root), "CONFIG=fixture.mk", "IMAGE=bad.bin", *settings,
                     setting, str(self.root / "bad.bin")],
                    cwd=self.root, text=True, capture_output=True, check=False,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test linker-script names inside the target",
    )
    def test_script_names_inside_the_target_are_reported_unless_views(self):
        # The image is the unit's .data, 80010000-80010028: the table, then
        # the pointers that make the link resolve each name. The declared
        # uninitialized data follows up to 80010040.
        self.build_fixture_unit(
            "int table[4] = {1, 2, 3, 4};\n"
            "extern int inner[], outside, late, view[], chained, fixed;\n"
            "int *refs[] = {inner, &outside, &late, view, &chained, &fixed};\n")
        auto = self.root / "auto/undefined_syms_auto.txt"
        auto.parent.mkdir()
        auto.write_text("table = 0x80010000;\ninner = 0x80010004;\noutside = 0x80030000;\n"
                        "late = 0x80010030;\nunused = 0x80010008;\n")
        (self.root / "views.ld").write_text(
            "/* Views of the table. */\nview = table + 8;\nchained = view + 4;\n"
            "fixed = 0x8001000C;\n")
        (self.root / "other.ld").write_text("elsewhere = table + 0x10000;\n")
        settings = ["LINKER_EXTRA=auto/undefined_syms_auto.txt views.ld other.ld"]
        self.cover_linked_fixture("image.bin", settings, sections=(".data",))
        tool = Path(__file__).resolve().parents[1] / "tools/matching_coverage.py"
        scripts = ["--script", "build/auto/undefined_syms_auto.ld", "--script", "views.ld",
                   "--script", "other.ld"]

        def check(*arguments):
            return subprocess.run(
                [sys.executable, str(tool), "image.bin.elf", "--map", "image.bin.map", *scripts,
                 *arguments], cwd=self.root, text=True, capture_output=True, check=False)

        # Splat's PROVIDE of an object's own name is unused; a used one inside
        # the image or the declared uninitialized data is reported, as is a
        # number a views script assigns there. Views of linked symbols are not.
        result = check("--script-symbols", "strict", "--views", "views.ld",
                       "--bss-end", "0x80010040")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(sorted(result.stderr.splitlines()), [
            "error: image.bin.elf: 1 name(s) that build/auto/undefined_syms_auto.ld assigns lie"
            " inside the target's own image (80010000-80010028): inner",
            "error: image.bin.elf: 1 name(s) that build/auto/undefined_syms_auto.ld assigns lie"
            " inside the target's own uninitialized data (80010028-80010040): late",
            "error: image.bin.elf: 1 name(s) that views.ld assigns lie inside the target's own"
            " image (80010000-80010028): fixed",
        ])
        warned = check("--script-symbols", "warn", "--views", "views.ld", "--bss-end",
                       "0x80010040")
        self.assertEqual(warned.returncode, 0)
        self.assertEqual(warned.stderr.replace("warning:", "error:"), result.stderr)
        # Without a declared end only the image counts; without the views
        # allowance the views are names like any other.
        result = check("--script-symbols", "strict", "--views", "views.ld")
        self.assertNotIn("late", result.stderr)
        result = check("--script-symbols", "strict")
        self.assertIn("3 name(s) that views.ld assigns lie inside the target's own image"
                      " (80010000-80010028): view, chained, fixed", result.stderr)
        # An allowance must be one of the link's scripts and define a view.
        result = check("--script-symbols", "warn", "--views", "views.ld", "--views", "other.ld")
        self.assertEqual(result.returncode, 0)
        self.assertIn("warning: image.bin.elf: views script other.ld defines no view inside the"
                      " target's own image or uninitialized data", result.stderr)
        result = check("--script-symbols", "warn", "--views", "missing.ld")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing.ld: a views script that is not among the link's scripts",
                      result.stderr)
        result = check("--script-symbols", "warn", "--bss-end", "0x80010020")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("up to 80010028, past its declared end 80010020", result.stderr)
        # verify runs the check after the exact comparison: strict, the
        # default, fails; warn reports and passes, marked as no acceptance.
        image = (self.root / "image.bin").read_bytes()
        self.original.write_bytes(image)
        fixture = self.root / "fixture.mk"
        fixture.write_text(fixture.read_text().replace(
            self.digest, hashlib.sha256(image).hexdigest()))
        repo = Path(__file__).resolve().parents[1]
        environment = {k: v for k, v in os.environ.items() if k != "SCRIPT_SYMBOLS"}
        for mode, status in ((None, 2), ("strict", 2), ("warn", 0)):
            with self.subTest(mode):
                result = subprocess.run(
                    ["make", "--no-print-directory", "-f", str(repo / "decomp/Makefile"),
                     "ROOT=" + str(self.root), "CONFIG=fixture.mk", "IMAGE=image.bin",
                     "TARGET_CPPFLAGS=-DORIGINAL_BASE=0x80010000", *settings,
                     "LINK_VIEWS=views.ld", "BSS_END=0x80010040",
                     *(["SCRIPT_SYMBOLS=" + mode] if mode else []), "verify"],
                    cwd=self.root, env=environment, text=True, capture_output=True,
                    check=False)
                self.assertEqual(result.returncode, status, result.stdout + result.stderr)
                self.assertIn('"matched": true', result.stdout)
                self.assertIn(f"{'error' if status else 'warning'}: image.bin.elf: 1 name(s)"
                              " that views.ld assigns", result.stderr)
                self.assertEqual("NOT ACCEPTANCE: SCRIPT_SYMBOLS=warn" in result.stderr,
                                 mode == "warn")
        # all-verify gives each target SCRIPT_SYMBOLS=strict on its command
        # line: warn from the environment or all-verify's own command line
        # does not weaken it.
        targets = self.root / "targets/overlays"
        targets.mkdir(parents=True)
        (targets / "fixture.mk").write_text(
            fixture.read_text() + "TARGET_CPPFLAGS := -DORIGINAL_BASE=0x80010000\n"
            "LINKER_EXTRA := auto/undefined_syms_auto.txt views.ld other.ld\n"
            "LINK_VIEWS := views.ld\nBSS_END := 0x80010040\n")
        (self.root / "Makefile").write_text(f"include {repo / 'decomp/Makefile'}\n")
        for where in ("environment", "command line"):
            with self.subTest(where):
                result = subprocess.run(
                    ["make", "--no-print-directory", "ROOT=" + str(self.root),
                     *(["SCRIPT_SYMBOLS=warn"] if where == "command line" else []),
                     "all-verify"],
                    cwd=self.root, text=True, capture_output=True, check=False,
                    env={**environment, **({"SCRIPT_SYMBOLS": "warn"}
                                           if where == "environment" else {})})
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn('"matched": true', result.stdout)
                self.assertIn("error: image.bin.elf: 1 name(s) that views.ld assigns",
                              result.stderr)
                self.assertNotIn("NOT ACCEPTANCE", result.stderr)

    def link_assembly(self, name, source, script, scripts=()):
        """Assemble NAME.s as the target Makefile does and link NAME.bin.elf
        (with its map) from the script and the further scripts."""
        (self.root / f"{name}.s").write_text(source)
        (self.root / f"{name}.ld").write_text(script)
        subprocess.run(["psx-as", "-EL", "-march=r3000", "-mtune=r3000", "-msoft-float",
                        "-no-pad-sections", "-G0", "-o", f"{name}.o", f"{name}.s"],
                       cwd=self.root, check=True)
        subprocess.run(["psx-ld", "-nostdlib", "--no-check-sections", "-Map", f"{name}.bin.map",
                        "-T", f"{name}.ld", *(f"-T{path}" for path in scripts),
                        "-o", f"{name}.bin.elf"], cwd=self.root, check=True)

    @unittest.skipUnless(shutil.which("psx-as") and shutil.which("psx-ld"),
                         "enter the matching Nix shell to test the relocation scan")
    def test_own_addresses_without_their_relocation_fail(self):
        # The image is func (80010000-80010024), a word after it in .text and
        # the .data table (80010028-80010038). Four of its own addresses are
        # numbers: a lui of their %hi, a jal, a word in .text outside every
        # function and one in .data; relocated ones and addresses outside
        # the image (lui 0x8020, 80300000) are not reported.
        self.link_assembly("image", (
            ".set noreorder\n.set noat\n.text\n.globl func\n.type func, @function\nfunc:\n"
            "lui $8, %hi(table)\naddiu $8, $8, %lo(table)\njal func\nnop\n"
            "lui $9, 0x8001\n.word 0x0C004000\nlui $10, 0x8020\njr $31\nnop\n"
            ".size func, . - func\n.word 0x80010008\n"
            ".data\n.globl table\ntable:\n.word table\n.word 0x80010004\n.word 0x80300000\n"
            ".word 1\n"),
            "SECTIONS {\n  .image 0x80010000 : SUBALIGN(4) { image.o(.text) image.o(.data) }\n"
            "  /DISCARD/ : { *(*) }\n}\n")
        tool = Path(__file__).resolve().parents[1] / "tools/matching_coverage.py"
        classification = self.root / "classification.txt"

        def check(lines=None):
            if lines is not None:
                classification.write_text(lines)
            return subprocess.run(
                [sys.executable, str(tool), "image.bin.elf", "--map", "image.bin.map",
                 "--relocations",
                 *(["--classification", "classification.txt"] if lines is not None else [])],
                cwd=self.root, text=True, capture_output=True, check=False)

        result = check()
        self.assertEqual(result.returncode, 1, result.stderr)
        span = "an address in 80010000-80010038"
        self.assertEqual(result.stderr.splitlines(), [
            "error: image.bin.elf: 80010010 (.text of image.o): lui 3c098001: the %hi of"
            f" {span} without R_MIPS_HI16",
            "error: image.bin.elf: 80010014 (.text of image.o): jal 0c004000 without R_MIPS_26",
            f"error: image.bin.elf: 80010024 (.text of image.o): word 80010008, {span},"
            " without R_MIPS_32",
            f"error: image.bin.elf: 8001002c (.data of image.o): word 80010004, {span},"
            " without R_MIPS_32",
        ])
        # Bytes classified asset or included, original data, are exempt; any
        # other, authored (handwritten) assembly too, only by an unrelocated
        # line with its reason, also inside a class's range.
        reviewed = ("80010010 80010018 handwritten a routine the compiler does not emit\n"
                    "80010010 80010018 unrelocated a patcher's literal targets\n"
                    "80010024 80010028 asset an embedded file's bytes\n")
        for lines in (reviewed + "8001002c 80010030 unrelocated a count, not an address\n",
                      reviewed.replace("asset an", "included D_80010024 an")
                      + "80010028 80010038 sdk a library's data\n"
                      "8001002c 80010030 unrelocated a count, not an address\n"):
            with self.subTest(lines):
                result = check(lines)
                self.assertEqual(result.returncode, 0, result.stderr)
        unreviewed = reviewed.replace("80010010 80010018 unrelocated a patcher's literal targets\n",
                                      "")
        for lines, message in (
            (unreviewed + "8001002c 80010030 unrelocated a count, not an address\n",
             "80010010 (.text of image.o): lui 3c098001"),
            (unreviewed.replace("handwritten", "sdk")
             + "8001002c 80010030 unrelocated a count, not an address\n",
             "80010014 (.text of image.o): jal 0c004000"),
            (reviewed + "8001002c 80010030 unrelocated a count\n"
             "80010030 80010034 unrelocated a word outside the image\n",
             "unrelocated line 80010030-80010034: no word there that the relocation scan"
             " reports"),
            (reviewed + "8001002c 80010030 unrelocated\n", "gives the reason"),
        ):
            with self.subTest(lines):
                result = check(lines)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)

    @unittest.skipUnless(
        shutil.which("psx-as") and shutil.which("psx-ld")
        and importlib.util.find_spec("rabbitizer"),
        "enter the matching Nix shell to test the cross-image check")
    def test_cross_image_names_and_mode_table_agree_with_the_other_links(self):
        # A resident r (80010000-80010028: res_func, fill, the mode table,
        # res_var), an overlay o (80020000-80020018, its .bss to 80020028)
        # that takes res_func from a PROVIDE list and other names from a
        # fragment, and a module p (80030000-8003000c: p_func, which loads
        # res_var's second word by number): by name, by view (member:
        # res_var + 4), by address (alias: the table, and D_80030000: p_func)
        # and inside an object (inner: res_var's second word).
        def resident(entry=0x80020000, bss=(0x80020014, 0x80020024)):
            self.link_assembly("r", (
                ".set noreorder\n.text\n.globl res_func\n.type res_func, @function\n"
                "res_func:\njr $31\nnop\n.size res_func, . - res_func\n"
                f".section .rodata\n.globl table\ntable:\n.word {entry}, {bss[0]}, {bss[1]}, 1\n"
                ".data\n.globl res_var\nres_var:\n.word 0, 0\n"),
                "SECTIONS {\n  .r 0x80010000 : SUBALIGN(4) { r.o(.text) . = ALIGN(16); r.o(.rodata)"
                " r.o(.data) }\n  /DISCARD/ : { *(*) }\n}\n")

        def overlay(names):
            (self.root / "o.resident.ld").write_text(names)
            self.link_assembly("o", (
                ".set noreorder\n.text\n.globl ov_entry\n.type ov_entry, @function\n"
                "ov_entry:\nlui $8, %hi(res_var)\nlw $8, %lo(res_var)($8)\njal res_func\nnop\n"
                "jr $31\nnop\n.size ov_entry, . - ov_entry\n"
                ".bss\n.globl ov_bss\nov_bss:\n.space 0x10\n"),
                "SECTIONS {\n  .o 0x80020000 : SUBALIGN(4) { o.o(.text) }\n"
                "  .o_bss (NOLOAD) : SUBALIGN(4) { o.o(.bss) }\n  /DISCARD/ : { *(*) }\n}\n",
                ["build/o/auto/undefined_funcs_auto.ld", "o.resident.ld"])

        provide = self.root / "build/o/auto/undefined_funcs_auto.ld"
        provide.parent.mkdir(parents=True)
        provide.write_text("PROVIDE(res_func = 0x80010000);\nPROVIDE(unused = 0x80010004);\n")
        (self.root / "r.mk").write_text(
            "IMAGE := r.bin\nBUILD := build/r\nLINKER_SCRIPT := r.ld\nMODE_TABLE := table\n")
        (self.root / "o.mk").write_text(
            "IMAGE := o.bin\nBUILD := build/o\nLINKER_SCRIPT := o.ld\n"
            "LINKER_EXTRA := auto/undefined_funcs_auto.txt o.resident.ld\n"
            "MODE := 0\nMODE_ENTRY := ov_entry\n")
        (self.root / "p.mk").write_text("IMAGE := p.bin\nBUILD := build/p\nLINKER_SCRIPT := p.ld\n")
        self.link_assembly("p", (
            ".set noreorder\n.text\n.globl p_func\n.type p_func, @function\n"
            "p_func:\nlui $8, 0x8001\njr $31\nlw $8, 0x24($8)\n.size p_func, . - p_func\n"),
            "SECTIONS {\n  .p 0x80030000 : SUBALIGN(4) { p.o(.text) }\n  /DISCARD/ : { *(*) }\n}\n")
        names = ("res_var = 0x80010020;\nalias = 0x80010010;\ninner = 0x80010024;\n"
                 "member = res_var + 4;\nD_80030000 = 0x80030000;\ntimer = 0x1F801100;\n")
        resident()
        overlay(names)
        tool = Path(__file__).resolve().parents[1] / "tools/cross_image.py"

        def check():
            return subprocess.run([sys.executable, str(tool), "r.mk", "o.mk", "p.mk"],
                                  cwd=self.root, text=True, capture_output=True, check=False)

        result = check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {
            "claim": "cross_image_agreement", "targets": 3, "names": 6, "by_name": 2,
            "by_view": 1, "by_address": 2, "inside_object": 1, "mode_entries": 1})
        # --numbers lists the other links' addresses a link holds as numbers:
        # p's lui/lw of res_var's second word, not o's relocated ones.
        listed = subprocess.run([sys.executable, str(tool), "r.mk", "o.mk", "p.mk", "--numbers"],
                                cwd=self.root, text=True, capture_output=True, check=True)
        self.assertEqual(listed.stdout, "p 80030000 lui 80010024 (p.o): r\n")
        # A name another link places elsewhere, also where its copied value
        # points into a third link (r exports table, which p does not
        # define), a view outside the object holding its base, a name whose
        # value is not the address it gives, one in no object (the fill
        # after res_func) and a mode table that does not hold the overlay's
        # entry and uninitialized data all fail.
        for change, message in (
            (lambda: overlay(names.replace("0x80010020", "0x80010024")),
             "o: res_var = 80010024 (o.resident.ld), but r defines res_var at 80010020"),
            (lambda: overlay(names + "table = 0x80030000;\n"),
             "o: table = 80030000 (o.resident.ld), but r defines table at 80010010"),
            (lambda: overlay(names + "table = 0x80010010;\npast = table + 0x10;\n"),
             "o: past = 80010020 (o.resident.ld) is table + 0x10, outside the object holding"
             " table in r (80010010-80010020)"),
            (lambda: overlay(names + "D_80010004 = 0x80010000;\n"),
             "o: D_80010004 = 80010000 (o.resident.ld), but its name gives 80010004"),
            (lambda: overlay(names + "gap = 0x8001000C;\n"),
             "o: gap = 8001000c (o.resident.ld) lies in r but in no input section they place"),
            (lambda: resident(entry=0x80020004),
             "r: table[0] enters 80020004, not o's ov_entry (80020000)"),
            (lambda: resident(bss=(0x80020014, 0x80020020)),
             "r: table[0] clears 80020018-80020024, but o links its uninitialized data at"
             " 80020018-80020028"),
        ):
            with self.subTest(message):
                change()
                result = check()
                self.assertEqual(result.returncode, 1, result.stdout)
                self.assertEqual(result.stderr, f"error: {message}\n")
                resident()
                overlay(names)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test the uninitialized data past the image",
    )
    def test_uninitialized_data_past_the_image_counts_apart(self):
        # The image is the unit's word at 80010000. Past it, not loaded: the
        # unit's 8-byte static, a generated unit's two labelled variables (6
        # bytes), alignment fill, another generated unit's word and, up to the
        # declared end, bytes no object holds, one variable named only by a
        # linker script.
        self.build_fixture_unit("int c_data = 0x11111111;\nstatic int counter[2];\n"
                                "int *count(void) { return counter; }\n")
        (self.root / "asm/data").mkdir(parents=True)
        (self.root / "asm/data/generated.bss.s").write_text(
            ".section .bss\n.globl D_8001000C\nD_8001000C:\n.space 4\n"
            ".globl D_80010010\nD_80010010:\n.space 2\n")
        (self.root / "asm/data/tail.bss.s").write_text(".section .bss\n.space 4\n")
        names = self.root / "names.ld"
        names.write_text("D_8001001C = 0x8001001C;\n")
        noload = ("build/decomp/src/t/unit.o(.bss)", "build/asm/data/generated.bss.o(.bss)",
                  "build/asm/data/tail.bss.o(.bss)")

        def cover(*arguments):
            result = self.cover_linked_fixture("image.bin", ["LINKER_EXTRA=names.ld"],
                                               sections=(".data",), noload=noload,
                                               arguments=arguments)
            self.assertEqual(result.returncode, 0, result.stderr)
            return result

        report = json.loads(cover().stdout)
        self.assertEqual(report["data_classes"], {"c": 4})
        self.assertEqual((report["bss_noload_bytes"], report["bss_noload_classes"]),
                         (18, {"bss": 8, "bss_placeholder": 10}))
        report = json.loads(cover("--bss-end", "0x80010020").stdout)
        self.assertEqual((report["bss_noload_bytes"], report["bss_noload_classes"],
                          report["remaining_bss_placeholder_bytes"]),
                         (26, {"bss": 8, "bss_placeholder": 18}, 18))
        self.assertEqual(report["remaining_data_placeholder_bytes"], 0)
        # Each placeholder range shows with the symbol at its start.
        listing = cover("--bss-end", "0x80010020", "--list", "bss_placeholder")
        self.assertEqual(listing.stdout.splitlines(), [
            "8001000c      4 D_8001000C (.bss generated.bss.o)",
            "80010010      2 D_80010010 (.bss generated.bss.o)",
            "80010014      4 (.bss tail.bss.o)",
            "80010018      4 (no object)",
            "8001001c      4 D_8001001C (no object)",
        ])
        listing = cover("--bss-end", "0x80010020", "--list", "bss")
        self.assertEqual(listing.stdout.splitlines(), ["80010004      8 (.bss unit.o)"])
        # The fill between input sections counts once a name places a variable there.
        names.write_text("D_8001001C = 0x8001001C;\nD_80010012 = 0x80010012;\n")
        report = json.loads(cover("--bss-end", "0x80010020").stdout)
        self.assertEqual(report["bss_noload_classes"], {"bss": 8, "bss_placeholder": 20})
        # A declared end short of what the link places fails.
        result = self.cover_linked_fixture("image.bin", ["LINKER_EXTRA=names.ld"],
                                           sections=(".data",), noload=noload,
                                           arguments=["--bss-end", "0x80010010"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the link places the target up to 80010018, past its declared end"
                      " 80010010 (BSS_END)", result.stderr)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test data carried by assembly",
    )
    def test_functions_and_data_count_by_how_they_were_built(self):
        # splat moves rodata that only one function uses into the function's
        # .s file, so INCLUDE_ASM carries it into the C unit's .rodata.
        asm = self.root / "asm"
        asm.mkdir()

        def function(name, rodata=""):
            (asm / f"{name}.s").write_text(
                ".section .rodata\n" + rodata + ".section .text\n"
                f"glabel {name}\n.L{name}:\n    jr $ra\n    nop\nendlabel {name}\n"
            )

        def table(name, target, align):
            return (f".align {align}\ndlabel {name}\n    .word {target}\n    .word {target}\n"
                    f"enddlabel {name}\n")

        function("func_sdk", '.align 2\nnonmatching D_sdk\ndlabel D_sdk\n    .asciz "sdk"\n'
                             "enddlabel D_sdk\n" + table("jtbl_sdk", ".Lfunc_sdk", 3))
        function("func_hand")
        function("func_draft", table("jtbl_draft", ".Lfunc_draft", 2))
        function("func_asm", ".align 2\ndlabel D_asm\n    .word 0\nenddlabel D_asm\n")
        (asm / "D_shared.s").write_text(
            '.section .rodata\n.align 2\ndlabel D_shared\n    .asciz "both"\n.align 2\n'
            "enddlabel D_shared\n"
        )
        self.build_fixture_unit(
            '#include "include_asm.h"\n'
            'INCLUDE_ASM("asm", func_sdk);\n'
            'INCLUDE_ASM("asm", func_hand);\n'
            'const char *name(void) { return "c strin"; }\n'
            '#ifdef NON_MATCHING\nint func_draft(int i) { return i; }\n'
            '#else\nINCLUDE_ASM("asm", func_draft);\n#endif\n'
            'INCLUDE_ASM("asm", func_asm);\n'
            'INCLUDE_RODATA("asm", D_shared);\n'
        )
        # .text is linked first: func_sdk is at 0x80010000, func_hand follows.
        # The string several functions share stays original with a reason.
        (self.root / "classification.txt").write_text(
            "80010000 80010008 sdk a library function\n"
            "80010008 80010010 handwritten an authored routine\n"
            "80010054 8001005c included D_shared a string two library functions share\n"
        )
        result = self.cover_linked_fixture(
            "image.bin", sections=(".text", ".rodata"),
            arguments=["--classification", "classification.txt"],
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["binary_agreement"], "not_measured")
        sizes = {"c": 16, "sdk": 8, "handwritten": 8, "nonmatching": 8, "asm": 8}
        self.assertEqual(report["classes"], {
            cls: {"functions": 1, "bytes": size, "instructions": size // 4}
            for cls, size in sizes.items()
        })
        self.assertEqual((report["text_bytes"], report["text_instructions"]), (48, 12))
        self.assertEqual((report["remaining_asm_functions"], report["remaining_asm_bytes"],
                          report["remaining_asm_instructions"]), (2, 16, 4))
        # Only the compiler's string counts as C. The SDK function's string,
        # the padding before its 8-aligned jump table and the table count as
        # sdk; the draft's table, the unrecovered function's word and the
        # INCLUDE_RODATA'd string under their classes.
        self.assertEqual(report["data_classes"],
                         {"c": 8, "sdk": 16, "nonmatching": 8, "asm": 4, "included": 8})
        self.assertEqual(report["remaining_data_asm_bytes"], 12)
        # The report reads the coverage build as made from this object: one
        # changed after it fails.
        side = self.root / "build/decomp/src/t/unit.cov.o.s"
        side.write_text(side.read_text() + "\tnop\n")
        tool = Path(__file__).resolve().parents[1] / "tools/matching_coverage.py"
        result = subprocess.run(
            [sys.executable, str(tool), "image.bin.elf", "--map", "image.bin.map"],
            cwd=self.root, text=True, capture_output=True, check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not the unit's GAS input", result.stderr)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test data classes",
    )
    def test_data_counts_by_unit_and_classified_range(self):
        # The C unit's word, a generated data unit's four words (GAS aligns the
        # section to 16: at 80010010) and an authored assembly unit's word,
        # linked in that order; a classified range takes precedence over the
        # class of each.
        self.build_fixture_unit("int c_data = 0x11111111;\n")
        (self.root / "asm/data").mkdir(parents=True)
        (self.root / "asm/data/generated.s").write_text(".section .data\n.word 1, 2, 3, 4\n")
        (self.root / "decomp/src/t/authored.s").write_text(".section .data\n.word 5\n")
        ranges = "80010000 80010002 sdk half the C word\n80010014 80010018 asset a generated word\n"
        classification = self.root / "classification.txt"
        classification.write_text(ranges)
        arguments = ["--classification", "classification.txt"]
        extra = ("build/asm/data/generated.o(.data)", "build/decomp/src/t/authored.o(.data)")
        # Handwritten bytes count only inside a handwritten range.
        result = self.cover_linked_fixture("image.bin", sections=(".data",), arguments=arguments,
                                           extra=extra)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("(80010020-80010024): handwritten bytes outside every handwritten range",
                      result.stderr)
        classification.write_text(ranges + "80010020 80010024 handwritten an authored word\n")
        result = self.cover_linked_fixture("image.bin", sections=(".data",), arguments=arguments,
                                           extra=extra)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["data_classes"],
                         {"c": 2, "sdk": 2, "placeholder": 12, "asset": 4, "handwritten": 4})
        self.assertEqual(report["remaining_data_placeholder_bytes"], 12)
        listing = self.cover_linked_fixture("image.bin", sections=(".data",), extra=extra,
                                            arguments=arguments + ["--list", "placeholder"])
        self.assertEqual(listing.stdout.splitlines(), ["80010010      4 (.data generated.o)",
                                                       "80010018      8 (.data generated.o)"])

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test the .text outside functions",
    )
    def test_text_outside_functions_counts_under_its_owner(self):
        # The C unit's .text, from 80010000: an SDK data tag an INCLUDE_ASM'd
        # file holds (no function of its name), a function its file follows
        # with padding and a data word, an INCLUDE_RODATA'd word and a table
        # cc1 places in .text before its function (8001001c, allowed by its
        # classification line). Then a generated and an authored assembly
        # unit (each GAS aligns to 16), each a function and a padding word.
        asm = self.root / "asm"
        asm.mkdir()
        (asm / "D_tag.s").write_text(
            "dlabel D_tag\n    .word 0x15007350, 0x0040809C\nenddlabel D_tag\n")
        (asm / "func_pad.s").write_text("glabel func_pad\n    jr $ra\n    nop\nendlabel func_pad\n"
                                        "    nop\n    .word 0x12345678\n")
        (asm / "D_words.s").write_text("dlabel D_words\n    .word 1\nenddlabel D_words\n"
                                       ".section .text\n    .word 0x22222222\n")
        self.build_fixture_unit(
            '#include "include_asm.h"\n'
            'INCLUDE_ASM("asm", D_tag);\n'
            'INCLUDE_ASM("asm", func_pad);\n'
            'INCLUDE_RODATA("asm", D_words);\n'
            'int table[] __attribute__((section(".text"))) = { 1 };\n'
            "int f(void) { return table[0]; }\n"
        )
        unit = (".include \"macro.inc\"\n.set noreorder\n.section .text\n"
                "glabel {0}\n    jr $ra\n    nop\nendlabel {0}\n    .word 0\n")
        (asm / "gen.s").write_text(unit.format("func_gen"))
        (self.root / "decomp/src/t/authored.s").write_text(unit.format("func_auth"))
        # The INCLUDE_RODATA'd words (in .text, unlabelled, and in .rodata)
        # stay original with a reviewed reason; the authored unit's function
        # and padding are handwritten only inside a handwritten range.
        ranges = ("80010000 80010008 sdk a library tag\n"
                  "80010018 8001001c included - a word in .text\n"
                  "8001001c 80010020 text_data table the original keeps it among the code\n"
                  "80010030 80010034 included D_words a word\n")
        classification = self.root / "classification.txt"
        classification.write_text(ranges)
        arguments = ["--classification", "classification.txt"]
        extra = ("build/asm/gen.o(.text)", "build/decomp/src/t/authored.o(.text)")
        result = self.cover_linked_fixture(
            "image.bin", sections=(".text", ".rodata"), arguments=arguments, extra=extra)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("func_auth (80010050-80010058): handwritten bytes outside every"
                      " handwritten range", result.stderr)
        classification.write_text(ranges + "80010050 80010058 handwritten an authored routine\n")
        result = self.cover_linked_fixture(
            "image.bin", sections=(".text", ".rodata"), arguments=arguments, extra=extra)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("text outside every function (80010058-8001005c): handwritten bytes",
                      result.stderr)
        classification.write_text(ranges + "80010050 8001005c handwritten an authored routine\n")
        result = self.cover_linked_fixture(
            "image.bin", sections=(".text", ".rodata"), arguments=arguments, extra=extra)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        # Functions, bytes and instructions per class: the tag by its range,
        # the table as text_data, each other byte outside the functions under
        # its owner's class.
        self.assertEqual(
            {cls: (v["functions"], v["bytes"], v["instructions"])
             for cls, v in report["classes"].items()},
            {"sdk": (0, 8, 0), "asm": (2, 28, 4), "included": (0, 4, 0), "c": (1, 16, 4),
             "text_data": (0, 4, 0), "handwritten": (1, 12, 2)},
        )
        # The classes add up to the .text input sections (48 + 12 + 12; the
        # unit's .rodata follows its .text, and the alignment gaps before the
        # 16-aligned assembly units belong to no input section).
        self.assertEqual((report["text_bytes"], report["text_instructions"]), (72, 10))
        self.assertEqual((report["remaining_asm_functions"], report["remaining_asm_bytes"],
                          report["remaining_asm_instructions"]), (2, 28, 4))
        self.assertEqual(report["data_classes"], {"included": 4})
        tool = Path(__file__).resolve().parents[1] / "tools/matching_coverage.py"
        listing = subprocess.run(
            [sys.executable, str(tool), "image.bin.elf", "--map", "image.bin.map", *arguments,
             "--list", "asm"],
            cwd=self.root, text=True, capture_output=True, check=True,
        ).stdout
        self.assertEqual(listing.splitlines(), [
            "80010008      8 func_pad", "80010010      8 (outside every function)",
            "80010040      8 func_gen", "80010048      4 (outside every function)",
        ])
        listing = subprocess.run(
            [sys.executable, str(tool), "image.bin.elf", "--map", "image.bin.map", *arguments,
             "--list", "text_data"],
            cwd=self.root, text=True, capture_output=True, check=True,
        ).stdout
        self.assertEqual(listing.splitlines(), ["8001001c      4 table"])
        listing = subprocess.run(
            [sys.executable, str(tool), "image.bin.elf", "--map", "image.bin.map", *arguments,
             "--list", "included"],
            cwd=self.root, text=True, capture_output=True, check=True,
        ).stdout
        self.assertEqual(listing.splitlines(), [
            "80010018      4 (outside every function)", "80010030      4 D_words (.rodata unit.o)",
        ])

    def cover_probe(self, name, source, files, headers, original, settings=(), arguments=()):
        """cover_linked_fixture on a unit of its own (.text, .rodata, .data)."""
        root = self.root / name
        root.mkdir()
        (root / "original.bin").write_bytes(original)
        self.write_fixture(source, root)
        headers = {f"decomp/include/{path}": text for path, text in headers.items()}
        for path, text in {**files, **headers}.items():
            (root / path).parent.mkdir(parents=True, exist_ok=True)
            (root / path).write_text(text)
        return self.cover_linked_fixture("image.bin", settings, (".text", ".rodata", ".data"),
                                         arguments=arguments, root=root)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as",
            "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test coverage attribution",
    )
    def test_bytes_the_compiler_did_not_emit_never_count_as_c(self):
        """Each case links original or hand-written bytes into a C unit; the
        report counts them as what they are, or fails. Expected: the text and
        data bytes per class, or the failure."""

        def function(name, body="    jr $ra\n    nop\n"):
            return f"glabel {name}\n{body}endlabel {name}\n"

        def asm_unit(name):
            return include + f'INCLUDE_ASM("asm", {name});\n'

        def data_unit(line, prefix=""):
            return (include + prefix + "int before = 0x11111111;\n" + line
                    + "\nint after = 0x22222222;\n")

        include = '#include "include_asm.h"\n'
        three = "    addiu $v0, $zero, 0x1234\n    jr $ra\n    nop\n"
        place = 'INCLUDE_ORIGINAL(".data", D_80010004, 0x80010004, 4);'
        c_func = "int c_func(int x) { return x + 1; }\n"
        in_data = ({}, {"c": 8, "included": 4})
        # The original object at 0x80010004: "ab", its terminator and a stray
        # byte in its padding, which needs no reviewed reason.
        stray = bytes(range(4)) + b"ab\0\x07" + bytes(range(8, 16))
        in_text = "original bytes into .text"
        other = "neither one of include_asm.h's statements nor an original-style macro"
        ldv0 = ("int f(int *p) {\n"  # psyq/inline_c.h's gte_ldv0
                '    __asm__ volatile("lwc2 $0, 0(%0);" "lwc2 $1, 4(%0)" : : "r"(p) : "memory");\n'
                "    return 0;\n}\n")
        cases = [
            # Whatever an INCLUDE_ASM'd file emits is its function's: a `;`
            # statement separator, a nested include, a redefined macro, a later
            # .size, alignment fill after its last object.
            ("statement_separator", asm_unit("func_f"), {"asm/func_f.s":
                ".section .rodata\n.align 2; .word 0x11223344, 0x55667788\n.align 2\n"
                "dlabel D_f\n    .word 0x99AABBCC\nenddlabel D_f\n"
                ".section .text\n" + function("func_f")},
             ({"asm": 8}, {"asm": 12})),
            ("nested_include", asm_unit("func_b"), {
                "asm/func_b.s": '.include "asm/extra.s"\n' + function("func_b"),
                "asm/extra.s": ".section .rodata\ndlabel D_hidden\n"
                               "    .word 0x11223344, 0x55667788\nenddlabel D_hidden\n"
                               ".section .text\n"},
             ({"asm": 8}, {"asm": 8})),
            ("macro_redefinition", asm_unit("func_g"), {"asm/func_g.s":
                ".purgem nonmatching\n.macro nonmatching label, size=1\n.word 0x5A5A5A5A\n.endm\n"
                ".section .rodata\n.align 2\nnonmatching D_g\ndlabel D_g\n    .word 1\n"
                "enddlabel D_g\n.section .text\n" + function("func_g")},
             ({"asm": 8}, {"asm": 8})),
            ("size_override", asm_unit("func_d"), {"asm/func_d.s":
                ".section .rodata\n.align 2\ndlabel D_big\n"
                "    .word 0x11111111, 0x22222222, 0x33333333, 0x44444444\nenddlabel D_big\n"
                ".section .text\n.size D_big, 4\n" + function("func_d")},
             ({"asm": 8}, {"asm": 16})),
            ("align_fill", asm_unit("func_c"), {"asm/func_c.s":
                ".section .rodata\n.align 2\ndlabel D_obj\n    .byte 1\nenddlabel D_obj\n"
                ".balign 4, 0xAB\n.balign 8, 0xCD\n.balign 16, 0xEF\n"
                ".section .text\n" + function("func_c")},
             ({"asm": 8}, {"asm": 16})),
            # However the source spells an INCLUDE_* use, the report reads the
            # statement cc1 emitted.
            ("token_paste", data_unit(
                'CAT(INCLUDE_, ORIGINAL)(".data", D_80010004, 0x80010004, 4);',
                "#define CAT(a, b) a##b\n"), {}, in_data),
            ("splice_original", data_unit(
                'INCLUDE_ORIG\\\nINAL(".data", D_80010004, 0x80010004, 4);'), {}, in_data),
            ("other_extension", data_unit('#include "orig.inc"'),
             {"decomp/src/t/orig.inc": place + "\n"}, in_data),
            ("header_wrapper", data_unit("ORIG(D_80010004, 0x80010004, 4);",
                                         '#include "wrap.h"\n'), {}, in_data,
             {"wrap.h": '#define ORIG(name, vram, size) '
                        'INCLUDE_ORIGINAL(".data", name, vram, size)\n'}),
            ("splice_asm", include + 'INCLUDE_\\\nASM("asm", func_s);\n',
             {"asm/func_s.s": function("func_s", three)}, ({"asm": 12}, {})),
            # A function counts as C only where cc1 emitted it.
            ("rodata_function", include + c_func + 'INCLUDE_RODATA("asm", func_rod);\n',
             {"asm/func_rod.s": ".section .text\n.set noreorder\n" + function("func_rod", three)
                                + ".set reorder\n"},
             ({"c": 8, "asm": 12}, {})),
            ("extra_glabel", asm_unit("func_a"),
             {"asm/func_a.s": function("func_a") + function("func_hidden", three)},
             ({"asm": 20}, {})),
            ("original_in_function", include + "int func_e(void) {\n"
             '    INCLUDE_ORIGINAL(".text", D_body, 0x80010000, 16);\n    return 0;\n}\n',
             {}, in_text),
            ("original_text_filescope", include + c_func
             + 'INCLUDE_ORIGINAL(".text", func_orig, 0x80010000, 24);\n', {}, in_text),
            # Any other asm statement must be an original-style macro
            # (ORIGINAL_ASM) written inside a compiled function, exactly its
            # template with registers for the operands and no GAS macro.
            ("inline_data", include + "int before = 1;\n"
             '__asm__(".section .data\\n\\t.word 0x12345678\\n.previous");\n', {}, other),
            ("inline_text", include + '__asm__(".text\\n\\t.word 0x24020001");\n'
             "int f(void) { return 0; }\n", {}, other),
            ("macro_at_file_scope", include + '__asm__("break 1024");\n'
             "int f(void) { return 0; }\n", {}, other),
            ("inline_word", include + "int f(void) {\n"
             '    __asm__ volatile(".word 0x24020001");\n    return 0;\n}\n', {}, other),
            ("inline_incbin", include + "int f(void) {\n"
             '    __asm__ volatile(".incbin \\"original.bin\\", 0, 4");\n    return 0;\n}\n',
             {}, other),
            ("inline_macro", include
             + 'int f(void) {\n    __asm__ volatile("rtps");\n    return 0;\n}\n', {}, other),
            ("redefined_instruction", asm_unit("func_l") + ldv0,
             {"asm/func_l.s": function("func_l") + ".macro lwc2 a, b\n.word 0x5A5A5A5A\n.endm\n"},
             "an original-style macro expands a GAS macro"),
            ("instruction_macro", asm_unit("func_m") + "int g(int x) { return x + 1; }\n",
             {"asm/func_m.s": function("func_m") + ".macro j target\n.word 0x0000000D\n.endm\n"},
             "cc1's lines expanded a GAS macro"),
            # An original-style macro's code counts with its compiled function
            # (data cc1 puts in .text: the test after this one).
            ("inline_code", include + ldv0, {}, ({"c": 16}, {})),
            # Every .text byte counts once: a function lies inside its input
            # section and overlaps no other.
            ("function_overlap", asm_unit("func_o"),
             {"asm/func_o.s": "glabel func_o\n    nop\n" + function("func_i")
                              + ".size func_o, 12\n"}, "overlaps func_o"),
            ("function_past_section", asm_unit("func_p"),
             {"asm/func_p.s": function("func_p") + ".size func_p, 64\n"},
             "runs past its input section"),
        ]
        original = struct.pack("<8I", 0x24020011, 0x24030022, 0x24040033, 0x24050044,
                               0x03E00008, 0, 0, 0)
        for name, source, files, expected, *headers in cases:
            with self.subTest(name):
                result = self.cover_probe(name, source, files, headers[0] if headers else {},
                                          stray if expected is in_data else original)
                if isinstance(expected, str):
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn(expected, result.stderr)
                    continue
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout)
                text = {k: v["bytes"] for k, v in report["classes"].items()}
                self.assertEqual((text, report["data_classes"]), expected)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "psx-cpp-2.7.2-cdk", "psx-cc1-2.7.2-cdk",
            "maspsx", "psx-as", "psx-ld", "psx-objcopy", "psx-readelf",
        )),
        "enter the matching Nix shell to test data in .text",
    )
    def test_data_cc1_puts_in_text_counts_only_where_allowed(self):
        """Bytes cc1 puts in .text outside its functions are data objects, never
        C code: each counts as text_data only where a classification line names
        it with the evidence that the original keeps it there. Expected: the
        (functions, bytes, instructions) of each text class, or the failure."""
        include = '#include "include_asm.h"\n'
        unlisted = "a data object in .text, which counts only as a text_data line"
        f = "int f(int i) { return i + 1; }\n"
        # 80010000: a two-entry table, then target and f (8 bytes each).
        table = ("extern void target(void);\n"
                 'void (*table[])(void) __attribute__((section(".text"))) = { target, target };\n'
                 "void target(void) {}\n" + f)
        reason = "the original keeps the table among its code"
        listed = f"80010000 80010008 text_data table {reason}\n"
        switch = ("int g(int i) {\n    switch (i) {\n    case 0: return 5;\n    case 1: return 7;\n"
                  "    case 2: return 9;\n    case 3: return 11;\n    }\n    return 0;\n}\n")
        cases = [
            # Authored machine words in a .text array: E1 calls three of them
            # (jr $ra; nop; li $v0, 1), E2's are a whole function body, which
            # could replace an INCLUDE_ASM'd function and still match.
            ("E1_code_array", 'unsigned code[] __attribute__((section(".text"))) = {'
             " 0x03E00008, 0x00000000, 0x24020001 };\n"
             "int call(void) { return ((int (*)(void))code)(); }\n", {}, (), "", unlisted),
            ("E2_function_body_array",
             'unsigned func_80010000[] __attribute__((section(".text"))) = {\n'
             "    0x27BDFFE8, 0xAFBF0010, 0x0C004000, 0x00000000,\n"
             "    0x8FBF0010, 0x27BD0018, 0x03E00008, 0x00000000,\n};\n", {}, (), "", unlisted),
            # Every spelling of the attribute, also on a static or const object.
            ("dunder_section", 'int t[] __attribute__((__section__(".text"))) = { 1 };\n' + f,
             {}, (), "", unlisted),
            ("attribute_spacing", 'int t[] __attribute((section (".text"))) = { 1 };\n' + f,
             {}, (), "", unlisted),
            ("static_const", 'static const int t[] __attribute__((section(".text"))) = { 1, 2 };\n'
             "int f(int i) { return t[i]; }\n", {}, (), "", unlisted),
            # 2.7.2-cdk also takes the attribute on a function's static and on
            # an uninitialized variable (2.6.3/2.7.2 refuse or ignore it).
            ("cdk_function_static", "int f(int i) {\n"
             '    static int t[] __attribute__((section(".text"))) = { 1, 2 };\n'
             "    return t[i];\n}\n", {}, ("CC_VERSION=2.7.2-cdk",), "", unlisted),
            ("cdk_uninitialized", 'int t __attribute__((section(".text")));\n' + f,
             {}, ("CC_VERSION=2.7.2-cdk",), "", unlisted),
            # No attribute: after a definition cc1 believes it is in .data, but
            # INCLUDE_RODATA leaves the assembler in .text for the next one.
            ("after_include_rodata", "int a = 1;\n" + 'INCLUDE_RODATA("asm", D_r);\n'
             "int b = 2;\nint f(void) { return a + b; }\n",
             {"asm/D_r.s": "dlabel D_r\n    .word 7\nenddlabel D_r\n"}, (), "", unlisted),
            # -membedded-pic puts a switch's jump table among its code.
            ("embedded_pic_jump_table", switch, {}, ("CC1FLAGS_unit=-membedded-pic",), "",
             "a jump table or constant among its code"),
            # A function's code belongs in .text, attribute or not.
            ("function_in_text", 'void fn(void) __attribute__((section(".text")));\n'
             "void fn(void) {}\n", {}, (), "", {"c": (1, 8, 2)}),
            ("function_in_data", 'void fd(void) __attribute__((section(".data")));\n'
             "void fd(void) {}\n", {}, (), "", "a function in .data"),
            # A listed object counts as text_data, bytes only.
            ("listed", table, {}, (), listed, {"text_data": (0, 8, 0), "c": (2, 16, 4)}),
            # Each line names one object cc1 defined at its start, spans it to
            # the next symbol, tiles cc1's bytes with the others, is used and
            # gives a reason; no classified range overlaps another or counts
            # cc1's bytes.
            ("other_name", table, {}, (), listed.replace(" table ", " target "),
             "is not one data object cc1 defined there"),
            ("too_long", table, {}, (), listed.replace("80010008", "8001000c"), unlisted),
            ("too_short", table, {}, (), listed.replace("80010008", "80010004"), unlisted),
            ("two_objects", table.replace(
                "target, target };\n", "target };\n"
                'void (*more[])(void) __attribute__((section(".text"))) = { target };\n'),
             {}, (), listed, "is not one data object cc1 defined there"),
            ("unused", table, {}, (), listed + f"80020000 80020004 text_data ghost {reason}\n",
             "no data object cc1 placed in .text there"),
            ("no_reason", table, {}, (), "80010000 80010008 text_data table\n",
             "names its object and the evidence"),
            ("empty_range", table, {}, (), listed.replace("80010008", "80010000"),
             "an empty range"),
            ("range_overlap", table, {}, (), listed + "80010004 80010008 sdk a library word\n",
             "sdk at 80010004 overlaps text_data"),
            ("range_only", table, {}, (), "80010000 80010008 sdk a library table\n", unlisted),
        ]
        for name, source, files, settings, lines, expected in cases:
            with self.subTest(name):
                arguments = ["--classification", "classification.txt"] if lines else []
                files = {**files, "classification.txt": lines}
                result = self.cover_probe(name, include + source, files, {}, bytes(16),
                                          settings, arguments)
                if isinstance(expected, str):
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn(expected, result.stderr)
                    continue
                self.assertEqual(result.returncode, 0, result.stderr)
                classes = json.loads(result.stdout)["classes"]
                self.assertEqual({k: (v["functions"], v["bytes"], v["instructions"])
                                  for k, v in classes.items()}, expected)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as", "psx-readelf",
        )),
        "enter the matching Nix shell to test uninitialized variable slots",
    )
    def test_bss_slots_give_each_uninitialized_variable_whole_words(self):
        def layout(source, section, settings=()):
            obj = self.build_fixture_unit(source, settings)
            symbols = subprocess.run(["psx-readelf", "-sW", str(obj)], check=True,
                                     capture_output=True, text=True).stdout
            sections = subprocess.run(["psx-readelf", "-SW", str(obj)], check=True,
                                      capture_output=True, text=True).stdout
            size = re.search(r"\] " + re.escape(section) + r"\s+NOBITS\s+\w+\s+\w+\s+(\w+)", sections)
            offsets = {
                name: int(value, 16)
                for value, name in re.findall(r"\d+: ([0-9a-f]{8})\s+\d+ \w+\s+\w+\s+\w+\s+\d+ (\w+)\n", symbols)
                if name in ("flag", "pairs", "half", "word", "point")
            }
            return offsets, int(size.group(1), 16)

        # Statics (.lcomm), so the unit's own allocation order is tested. A
        # tentative definition (.comm) maspsx would allocate among them in
        # GCC's order, where the original linker placed commons after every
        # unit's own; the commons units reproduce that by their link order.
        source = (
            '#include "include_asm.h"\n'
            "static unsigned char flag;\n"
            "static unsigned char pairs[3][2];\n"
            "static short half;\n"
            "static int word;\n"
            "int use(void) { return flag + pairs[1][1] + half + word; }\n"
        )
        # Declaration order, each object in whole words (ASPSX 2.34, the
        # fixture's default), so every one is naturally aligned.
        offsets, size = layout(source, ".bss")
        self.assertEqual((offsets, size), ({"flag": 0, "pairs": 4, "half": 12, "word": 16}, 20))
        for name, alignment in {"flag": 1, "pairs": 1, "half": 2, "word": 4}.items():
            self.assertEqual(offsets[name] % alignment, 0, name)
        # Small data takes the same slots: an 8-byte object follows a byte at
        # 4 mod 8, as in the original images, where maspsx would 8-align it.
        small = (
            '#include "include_asm.h"\n'
            "static unsigned char flag;\n"
            "static struct { int x, y; } point;\n"
            "int use(void) { return flag + point.y; }\n"
        )
        self.assertEqual(layout(small, ".sbss", ["GP_unit=8"]), ({"flag": 0, "point": 4}, 12))
        # ASPSX 2.56 keeps each object's size and aligns it by that size up
        # to a word (battle 800c3ca4-800c3cb4): the 6-byte array follows the
        # byte at the next word and the halfword packs against it.
        sized = ["MASPSX_FLAGS=--aspsx-version=2.56"]
        self.assertEqual(layout(source, ".bss", sized), ({"flag": 0, "pairs": 4, "half": 10, "word": 12}, 16))
        self.assertEqual(layout(small, ".sbss", sized + ["GP_unit=8"]), ({"flag": 0, "point": 4}, 12))
        # No slot rule is evidenced for other ASPSX versions: a sub-word
        # variable fails the build instead of keeping maspsx's packing.
        _obj, result = self.make_fixture_unit(source, ["MASPSX_FLAGS=--aspsx-version=2.86"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no slot rule", result.stderr)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in (
            "make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as", "psx-readelf",
        )),
        "enter the matching Nix shell to test the small-data split",
    )
    def test_sbss_divides_a_g0_units_variables_by_declared_size(self):
        source = (
            '#include "include_asm.h"\n'
            "static unsigned char flag;\n"
            "static unsigned char pairs[3][2];\n"
            "static int big[3];\n"
            "static short half;\n"
            "static int word;\n"
            "int use(void) { return flag + pairs[1][1] + big[2] + half + word; }\n"
        )

        def layout(settings):
            obj = self.build_fixture_unit(source, settings)
            symbols = subprocess.run(["psx-readelf", "-sW", str(obj)], check=True,
                                     capture_output=True, text=True).stdout
            sections = subprocess.run(["psx-readelf", "-SW", str(obj)], check=True,
                                      capture_output=True, text=True).stdout
            index = {number: name for number, name in re.findall(r"\[\s*(\d+)\] (\S+)", sections)}
            placed = {}
            for value, number, name in re.findall(
                    r"\d+: ([0-9a-f]{8})\s+\d+ \w+\s+\w+\s+\w+\s+(\d+) (\w+)\n", symbols):
                if name in ("flag", "pairs", "big", "half", "word"):
                    placed.setdefault(index[number], {})[name] = int(value, 16)
            relocations = subprocess.run(["psx-readelf", "-rW", str(obj)], check=True,
                                         capture_output=True, text=True).stdout
            # The code addresses every variable absolutely, as at -G0.
            self.assertNotIn("GPREL", relocations)
            return placed

        # Without the setting maspsx keeps one .bss in declaration order.
        self.assertEqual(layout([]), {".bss": {"flag": 0, "pairs": 4, "big": 12, "half": 24,
                                               "word": 28}})
        # Objects of up to SBSS_<unit> bytes move to .sbss and the others stay,
        # each group in declaration order and in whole-word slots.
        self.assertEqual(layout(["SBSS_unit=8"]), {
            ".sbss": {"flag": 0, "pairs": 4, "half": 12, "word": 16}, ".bss": {"big": 0}})
        # The threshold compares the size GCC declares, not the slot: the
        # 6-byte array moves at 6 although its slot takes 8 bytes, not at 5.
        self.assertEqual(layout(["SBSS_unit=6"]), {
            ".sbss": {"flag": 0, "pairs": 4, "half": 12, "word": 16}, ".bss": {"big": 0}})
        self.assertEqual(layout(["SBSS_unit=5"]), {
            ".sbss": {"flag": 0, "half": 4, "word": 8}, ".bss": {"pairs": 0, "big": 8}})
        # The slot rule of the unit's ASPSX applies to both sections.
        sized = ["MASPSX_FLAGS=--aspsx-version=2.56", "SBSS_unit=8"]
        self.assertEqual(layout(sized), {
            ".sbss": {"flag": 0, "pairs": 4, "half": 10, "word": 12}, ".bss": {"big": 0}})

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("make", "psx-cpp-2.7.2", "psx-cc1-2.7.2", "maspsx", "psx-as")),
        "enter the matching Nix shell to test the object rule",
    )
    def test_a_failing_stage_of_the_object_pipeline_fails_the_rule(self):
        # cpp feeds cc1 and maspsx feeds the BSS slot filter through pipes:
        # either failing must fail the object rather than leave a partial one.
        source = '#include "include_asm.h"\nint value = 1;\n'
        _obj, result = self.make_fixture_unit(source, ["MASPSX=false"])
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        _obj, result = self.make_fixture_unit('#include "missing.h"\n' + source)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("missing.h", result.stderr)
        # The coverage build's marking stage refuses an asm name that would
        # carry lines into cc1's own, which the object itself assembles.
        obj = self.build_fixture_unit(
            '#include "include_asm.h"\n'
            'extern int v asm("D_1\\n\\t.word 0x24020001\\n\\t#");\n'
            "int f(void) { return v; }\n"
        )
        side = obj.with_name("unit.cov.o")
        result = subprocess.run(
            ["make", "--no-print-directory", "-f",
             str(Path(__file__).resolve().parents[1] / "decomp/Makefile"),
             "ROOT=" + str(self.root), "CONFIG=fixture.mk", str(side)],
            cwd=self.root, text=True, capture_output=True, check=False,
        )
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("asm name", result.stderr)
        self.assertFalse(side.exists())

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
            0x4A280030,  # rtpt
            0x0C010000,  # jal   0x80040000
            0xACC00200,  # sw    zero, 0x200(a2)  (the delay slot runs before the call)
            0xACC00300,  # sw    zero, 0x300(a2)  (a2 clobbered by the call)
            0x0040F809,  # jalr  v0
            0x03E00008,  # jr    ra
        ]
        blob = b"".join(w.to_bytes(4, "little") for w in words)
        found = scan_function(blob, 0x80010000, {0x800508E4: 0x1F801C00})
        self.assertEqual(found["io"], [0x1F801814, 0x1F801824])
        self.assertEqual(found["scratchpad"], [0x1F800100, 0x1F800200])
        self.assertEqual(found["hardware_pointers"], [0x800508E4])
        self.assertEqual(found["calls"], [0x80040000])
        self.assertEqual((found["gte"], found["indirect"]), (1, 1))

    @unittest.skipUnless(importlib.util.find_spec("rabbitizer"), "enter the matching Nix shell")
    def test_service_scan_names_bios_stubs_and_entries_inside_symbols(self):
        from tools.service_calls import beside, bios_call, sdk_entries

        words = [
            0x240A00B0,  # addiu t2, zero, 0xb0   close: B(36h)
            0x01400008,  # jr    t2
            0x24090036,  # addiu t1, zero, 0x36
            0x00000000,
            0x240A00B0,  # the next stub, merged into the same symbol: B(41h)
            0x01400008,
            0x24090041,
            0x00000000,
            0x03E00008,  # jr    ra               GetGp, an entry inside the symbol
            0x03801021,  # addu  v0, gp, zero
        ]
        blob = b"".join(w.to_bytes(4, "little") for w in words)
        self.assertEqual(bios_call(blob, 0), "B(36h)")
        self.assertEqual(bios_call(blob, 16), "B(41h)")
        self.assertIsNone(bios_call(blob, 4))
        self.assertIsNone(bios_call(blob, 32))  # runs past the blob
        names = sdk_entries(blob, 0x80040564, [(0x80040564, len(blob), "close")])
        self.assertEqual(names[0x80040564], "close")
        self.assertEqual(names[0x80040574], "B(41h)")
        self.assertEqual(names[0x80040584], "close+0x20")
        self.assertEqual(len(names), len(words))

        resident = {"span": (0x8000F800, 0x80059800)}
        menu = {"span": (0x801C5000, 0x801D9070)}
        movie_library = {"span": (0x801D3000, 0x801E8A1C)}
        self.assertTrue(beside(menu, resident))
        self.assertTrue(beside(menu, menu))
        self.assertFalse(beside(menu, movie_library))

    @unittest.skipUnless(importlib.util.find_spec("rabbitizer"), "enter the matching Nix shell")
    def test_data_users_resolve_bases_gp_and_indexed_accesses(self):
        from tools.data_users import formed_addresses

        words = [
            0x3C1C8006,  # lui   gp, 0x8006
            0x279C9170,  # addiu gp, gp, -0x6e90  (the start code's $gp: no data address)
            0x3C048009,  # lui   a0, 0x8009
            0x248425D4,  # addiu a0, a0, 0x25d4   (formed only)
            0x8C820004,  # lw    v0, 4(a0)
            0x00852021,  # addu  a0, a0, a1       (an indexed access keeps the base)
            0x90830002,  # lbu   v1, 2(a0)
            0x83820010,  # lb    v0, 0x10(gp)
            0x0C010000,  # jal   0x80040000
            0x00000000,  # nop
            0x8C850000,  # lw    a1, 0(a0)        (a0 clobbered by the call)
            0x03E00008,  # jr    ra
        ]
        blob = b"".join(w.to_bytes(4, "little") for w in words)
        self.assertEqual(formed_addresses(blob, 0x80010000), {
            0x800925D4: {"addiu"}, 0x800925D8: {"lw"}, 0x800925D6: {"lbu"}, 0x80059180: {"lb@gp"},
        })

    @unittest.skipUnless(importlib.util.find_spec("rabbitizer"), "enter the matching Nix shell")
    def test_stray_padding_flags_tails_unreferenced_bytes_and_strings(self):
        from tools.stray_padding import c_objects, flags

        gas = "\n".join([
            ".section .data", ".align 2", ".globl D_1", "D_1:", ".byte\t0", ".byte\t1,2",
            ".byte\t53", ".section .data", ".align 2", ".globl D_2", "D_2:",
            '.incbin ".local/x.bin", 0x10 - 0x0, 4', ".size D_2, 4", ".previous",
            ".section .rodata", ".align 2", "$LC0:", '.ascii "ab\\000k\\000"', ".text", "func:",
            "jr\t$31",
        ])
        objects = c_objects(gas)
        self.assertEqual(sorted(objects), ["$LC0", "D_1"])  # INCLUDE_* labels are not C
        self.assertEqual(objects["D_1"]["elements"], [1, 1, 1, 1])
        self.assertEqual(objects["$LC0"]["strings"], [b"ab\0k\0"])

        # battle's combo flags as they were: 15 indexed bytes, then '5' in the fill
        table = {"bytes": bytes(range(15)) + b"5", "elements": [1] * 16, "strings": [], "size": 1,
                 "start": 0x800C34CC, "next": 0x800C34DC, "pointers": 0,
                 "access": [(0, "indexed", "lbu", "func_80086B88")]}
        self.assertEqual(flags(table)[0], ("tail", 1, "35", ["text", "outlier"]))
        table["access"] = [(15, "exact", "lbu", "f")]  # a constant-offset read dismisses it
        self.assertTrue(all("read" in f[3] for f in flags(table)))
        # A struct object's last members count by their directive widths: its
        # trailing bytes are tails, its trailing words are not.
        table.update(size=16, access=[], pointers=1)
        self.assertEqual(flags(table), [("tail", 1, "35", ["text"]), ("tail", 2, "0e 35", []),
                                        ("tail", 3, "0d 0e 35", [])])
        table.update(elements=[4] * 4)
        self.assertEqual(flags(table), [])
        byte = {"bytes": b"\x08", "elements": [1], "strings": [], "size": 1, "start": 0x801E96A6,
                "next": 0x801E96A8, "pointers": 0, "access": []}
        self.assertEqual(flags(byte), [("unref", 1, "08", ["slot"])])
        string = {"bytes": b"ab\0k\0", "elements": [1] * 5, "strings": [b"ab\0k\0"], "size": 1,
                  "start": 0x80010000, "next": 0x80010008, "pointers": 0,
                  "access": [(0, "formed", "addiu", "f")]}
        self.assertEqual(flags(string), [("string", 2, "6b 00", [])])

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
