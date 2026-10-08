"""Exact comparison failures and an authored MIPS-I assemble/link smoke test."""

from __future__ import annotations

import hashlib
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

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


if __name__ == "__main__":
    unittest.main()
