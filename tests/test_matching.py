"""Exact comparison failures and an authored MIPS-I assemble/link smoke test."""
from __future__ import annotations

import hashlib
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
        self.original = self.root / 'original.bin'
        self.rebuilt = self.root / 'rebuilt.bin'
        self.original.write_bytes(b'\x01\x02\x03\x04')
        self.rebuilt.write_bytes(self.original.read_bytes())
        self.digest = hashlib.sha256(self.original.read_bytes()).hexdigest()

    def test_equal_is_only_binary_agreement(self):
        result = compare(self.original, self.rebuilt, self.digest)
        self.assertTrue(result['matched'])
        self.assertEqual(result['source_coverage'], 'not_measured')
        self.assertIsNone(result['first_difference'])

    def test_one_byte_difference_is_not_masked(self):
        self.rebuilt.write_bytes(b'\x01\x02\x00\x04')
        result = compare(self.original, self.rebuilt, self.digest)
        self.assertFalse(result['matched'])
        self.assertEqual(result['first_difference'], 2)

    def test_both_length_mismatches_fail(self):
        for data in (b'\x01\x02', b'\x01\x02\x03\x04\x00'):
            with self.subTest(data=data):
                self.rebuilt.write_bytes(data)
                result = compare(self.original, self.rebuilt, self.digest)
                self.assertFalse(result['matched'])
                self.assertEqual(result['first_difference'], min(4, len(data)))

    def test_wrong_original_rejects_even_equal_outputs(self):
        with self.assertRaisesRegex(ValueError, 'fingerprint mismatch'):
            compare(self.original, self.rebuilt, '0' * 64)

    def test_same_file_and_hardlink_reject(self):
        with self.assertRaisesRegex(ValueError, 'distinct'):
            compare(self.original, self.original, self.digest)
        self.rebuilt.unlink()
        self.rebuilt.hardlink_to(self.original)
        with self.assertRaisesRegex(ValueError, 'distinct'):
            compare(self.original, self.rebuilt, self.digest)

    def test_invalid_hash_and_missing_input_reject(self):
        with self.assertRaises(ValueError):
            compare(self.original, self.rebuilt, 'not-a-hash')
        self.rebuilt.unlink()
        with self.assertRaises(OSError):
            compare(self.original, self.rebuilt, self.digest)

    def test_empty_image_cannot_pass(self):
        self.original.write_bytes(b'')
        self.rebuilt.write_bytes(b'')
        with self.assertRaisesRegex(ValueError, 'must not be empty'):
            compare(self.original, self.rebuilt, hashlib.sha256(b'').hexdigest())

    @unittest.skipUnless(shutil.which('make'), 'GNU make is unavailable')
    def test_unconfigured_game_target_fails(self):
        root = Path(__file__).resolve().parents[1]
        result = subprocess.run(['make', '-C', str(root / 'decomp'), 'verify'],
                                text=True, capture_output=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('qualifying the original toolchain', result.stderr)

    def test_cli_exit_codes(self):
        args = [str(self.original), str(self.rebuilt), '--sha256', self.digest]
        self.assertEqual(main(args), 0)
        self.rebuilt.write_bytes(b'wrong')
        self.assertEqual(main(args), 1)
        self.rebuilt.unlink()
        self.assertEqual(main(args), 2)

    @unittest.skipUnless(shutil.which('psx-as') and shutil.which('psx-ld') and shutil.which('psx-objcopy'),
                         'enter the matching Nix shell to test MIPS binutils')
    def test_mips_assemble_link_and_exact_bytes(self):
        assembly = self.root / 'fixture.s'
        linker = self.root / 'fixture.ld'
        obj = self.root / 'fixture.o'
        elf = self.root / 'fixture.elf'
        assembly.write_text('.section .text,"ax"\n.set noreorder\n.globl fixture\nfixture:\n'
                            'addiu $2,$0,7\njr $31\nnop\n')
        linker.write_text('SECTIONS { .text 0x80010000 : { *(.text) } '
                          '/DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) } }\n')
        subprocess.run(['psx-as', '-EL', '-mips1', '-mabi=32', '-G0', '-o', str(obj), str(assembly)], check=True)
        subprocess.run(['psx-ld', '-EL', '-T', str(linker), '-o', str(elf), str(obj)], check=True)
        subprocess.run(['psx-objcopy', '-O', 'binary', '-j', '.text', str(elf), str(self.rebuilt)], check=True)
        self.original.write_bytes(struct.pack('<4I', 0x24020007, 0x03e00008, 0, 0))
        digest = hashlib.sha256(self.original.read_bytes()).hexdigest()
        self.assertTrue(compare(self.original, self.rebuilt, digest)['matched'])


if __name__ == '__main__':
    unittest.main()
