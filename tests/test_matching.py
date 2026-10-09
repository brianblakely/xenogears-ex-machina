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
                             root=None, extra=()):
        """Link the fixture unit's `sections` (loaded) at 0x80010000, then the
        `extra` input sections, and build its coverage object with the target
        Makefile; then run the coverage report on the ELF and map."""
        repo = Path(__file__).resolve().parents[1]
        root = root or self.root
        inputs = "".join(f"    build/decomp/src/t/unit.o({section})\n" for section in sections)
        inputs += "".join(f"    {line}\n" for line in extra)
        (root / "fixture.ld").write_text(
            "SECTIONS {\n  .fixture 0x80010000 : AT(0) {\n" + inputs
            + "  }\n  /DISCARD/ : { *(*) }\n}\n"
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
        # The report counts every loaded byte by where GAS put it, the loaded
        # .bss included.
        result = self.cover_linked_fixture("image.bin")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["data_classes"], {"bss": 4, "c": 8, "included": 4})
        self.assertEqual(report["remaining_data_placeholder_bytes"], 0)
        # It looks no object up by name, so a linker-script assignment
        # shadowing the name changes nothing.
        (self.root / "shadow.ld").write_text("D_80010004 = 0x80010004;\n")
        result = self.cover_linked_fixture("shadow.bin", ["LINKER_EXTRA=shadow.ld"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["data_classes"],
                         {"bss": 4, "c": 8, "included": 4})

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
        (self.root / "classification.txt").write_text(
            "80010000 80010008 sdk a library function\n"
            "80010008 80010010 handwritten an authored routine\n"
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
        (self.root / "classification.txt").write_text(
            "80010000 80010002 sdk half the C word\n80010014 80010018 asset a generated word\n"
        )
        result = self.cover_linked_fixture(
            "image.bin", sections=(".data",), arguments=["--classification", "classification.txt"],
            extra=("build/asm/data/generated.o(.data)", "build/decomp/src/t/authored.o(.data)"),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["data_classes"],
                         {"c": 2, "sdk": 2, "placeholder": 12, "asset": 4, "handwritten": 4})
        self.assertEqual(report["remaining_data_placeholder_bytes"], 12)

    def cover_probe(self, name, source, files, headers, original):
        """cover_linked_fixture on a unit of its own (.text, .rodata, .data)."""
        root = self.root / name
        root.mkdir()
        (root / "original.bin").write_bytes(original)
        self.write_fixture(source, root)
        headers = {f"decomp/include/{path}": text for path, text in headers.items()}
        for path, text in {**files, **headers}.items():
            (root / path).parent.mkdir(parents=True, exist_ok=True)
            (root / path).write_text(text)
        return self.cover_linked_fixture("image.bin", sections=(".text", ".rodata", ".data"),
                                         root=root)

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
        in_text = "original bytes into .text"
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
            # Other inline asm may emit only code into the compiled function
            # it is written in, with no GAS macro or file.
            ("inline_data", include + "int before = 1;\n"
             '__asm__(".section .data\\n\\t.word 0x12345678\\n.previous");\n',
             {}, "inline asm emits .data bytes"),
            ("inline_text", include + '__asm__(".text\\n\\t.word 0x24020001");\n'
             "int f(void) { return 0; }\n", {}, "inline asm emits .text bytes"),
            ("inline_incbin", include + "int f(void) {\n"
             '    __asm__ volatile(".incbin \\"original.bin\\", 0, 4");\n    return 0;\n}\n',
             {}, "expands a GAS macro or reads a file"),
            ("inline_macro", include
             + 'int f(void) {\n    __asm__ volatile("rtps");\n    return 0;\n}\n',
             {}, "expands a GAS macro or reads a file"),
            ("instruction_macro", asm_unit("func_m") + "int g(int x) { return x + 1; }\n",
             {"asm/func_m.s": function("func_m") + ".macro j target\n.word 0x0000000D\n.endm\n"},
             "cc1's lines expanded a GAS macro"),
            # GTE code in a compiled function counts with it; data a unit
            # places in .text is attributed but not counted.
            ("inline_code", include + "int f(int *p) {\n"
             '    __asm__ volatile("lwc2 $0, 0(%0)" : : "r"(p));\n    return 0;\n}\n',
             {}, ({"c": 12}, {})),
            ("text_data", include + 'int table[] __attribute__((section(".text"))) = { 1 };\n'
             "int f(void) { return table[0]; }\n", {}, ({"c": 16}, {})),
        ]
        original = struct.pack("<8I", 0x24020011, 0x24030022, 0x24040033, 0x24050044,
                               0x03E00008, 0, 0, 0)
        for name, source, files, expected, *headers in cases:
            with self.subTest(name):
                result = self.cover_probe(name, source, files, headers[0] if headers else {},
                                          bytes(range(16)) if expected is in_data else original)
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
        # No slot rule is evidenced for other ASPSX versions: a sub-word
        # variable fails the build instead of keeping maspsx's packing.
        _obj, result = self.make_fixture_unit(source, ["MASPSX_FLAGS=--aspsx-version=2.56"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no slot rule", result.stderr)

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
