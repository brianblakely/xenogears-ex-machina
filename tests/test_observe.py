"""Synthetic checks for reference-capture fidelity and checkpoint rejection."""

from __future__ import annotations

import hashlib
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

from tools.reference.observe import validate_inputs, validate_reference_state, write_png


class CaptureImageTests(unittest.TestCase):
    def decode_png(self, path: Path) -> tuple[tuple[int, int], bytes]:
        data = path.read_bytes()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        offset, compressed, dimensions = 8, bytearray(), None
        while offset < len(data):
            size = struct.unpack_from(">I", data, offset)[0]
            kind = data[offset + 4 : offset + 8]
            payload = data[offset + 8 : offset + 8 + size]
            checksum = struct.unpack_from(">I", data, offset + 8 + size)[0]
            self.assertEqual(checksum, zlib.crc32(kind + payload))
            if kind == b"IHDR":
                width, height, depth, color, compression, filtering, interlace = struct.unpack(
                    ">IIBBBBB", payload
                )
                self.assertEqual((depth, color, compression, filtering, interlace), (8, 2, 0, 0, 0))
                dimensions = (width, height)
            elif kind == b"IDAT":
                compressed.extend(payload)
            offset += size + 12
        self.assertIsNotNone(dimensions)
        return dimensions, zlib.decompress(compressed)

    def test_primary_colors_and_row_padding_in_each_supported_pixel_format(self) -> None:
        expected = b"\x00" + bytes((255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255))
        cases = [
            (0, 2, (0x7C00, 0x03E0, 0x001F, 0x7FFF)),
            (2, 2, (0xF800, 0x07E0, 0x001F, 0xFFFF)),
            (1, 4, (0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF)),
        ]
        with tempfile.TemporaryDirectory() as directory:
            for pixel_format, size, pixels in cases:
                with self.subTest(pixel_format=pixel_format):
                    row = b"".join(pixel.to_bytes(size, sys.byteorder) for pixel in pixels)
                    row += b"\xde\xad\xbe\xef"
                    path = Path(directory) / "capture.png"
                    write_png(path, row * 2, 4, 2, len(row), pixel_format)
                    dimensions, content = self.decode_png(path)
                    self.assertEqual(dimensions, (4, 2))
                    self.assertEqual(content, expected * 2)

    def test_invalid_format_geometry_pitch_and_truncated_buffers_are_rejected(self) -> None:
        cases = [
            (b"\x00\x00", 1, 1, 2, 3),
            (b"", 0, 1, 0, 0),
            (b"", 2049, 1, 4098, 0),
            (b"\x00", 1, 1, 2, 0),
            (b"\x00\x00", 2, 1, 2, 0),
        ]
        with tempfile.TemporaryDirectory() as directory:
            for case in cases:
                with self.subTest(case=case), self.assertRaises(ValueError):
                    write_png(Path(directory) / "invalid.png", *case)


class InputProtocolTests(unittest.TestCase):
    def test_explicit_intervals_and_simultaneous_buttons(self) -> None:
        schedule = [{"start": 0, "end": 10, "buttons": ["up", "cross"]}]
        self.assertEqual(validate_inputs(schedule, 10), schedule)
        self.assertEqual(validate_inputs([], 10), [])

    def test_ambiguous_or_out_of_budget_inputs_are_rejected(self) -> None:
        invalid = [
            {},
            [None],
            [{"start": 0, "end": 1, "buttons": [], "repeat": 20}],
            [{"start": False, "end": 1, "buttons": []}],
            [{"start": -1, "end": 1, "buttons": []}],
            [{"start": 0, "end": 11, "buttons": []}],
            [{"start": 1, "end": 1, "buttons": []}],
            [{"start": 0, "end": 1, "buttons": "cross"}],
            [{"start": 0, "end": 1, "buttons": ["guess"]}],
            [{"start": 0, "end": 1, "buttons": ["cross", "cross"]}],
        ]
        for schedule in invalid:
            with self.subTest(schedule=schedule), self.assertRaises(ValueError):
                validate_inputs(schedule, 10)


class ReferenceCheckpointTests(unittest.TestCase):
    def test_matching_synthetic_checkpoint_is_read_and_tampering_rejected(self) -> None:
        identity = {
            "schema_version": 1,
            "source_profile": "synthetic-profile",
            "content_sha256": "synthetic-content",
            "core_sha256": "synthetic-core",
            "bios_sha256": None,
            "effective_options": {"synthetic-option": "selected"},
        }
        payload = b"Invented external-reference state for a rejection test."
        manifest = identity | {"final_state_sha256": hashlib.sha256(payload).hexdigest()}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "final.state"
            report_path = path.parent / "observation.json"
            path.write_bytes(payload)
            report_path.write_text(json.dumps(manifest))
            self.assertEqual(validate_reference_state(path, identity), payload)
            for key in identity:
                with self.subTest(changed=key):
                    changed = manifest | {key: "incompatible"}
                    report_path.write_text(json.dumps(changed))
                    with self.assertRaisesRegex(ValueError, "incompatible"):
                        validate_reference_state(path, identity)
            report_path.write_text(json.dumps(manifest))
            path.write_bytes(payload + b"modified")
            with self.assertRaisesRegex(ValueError, "checksum"):
                validate_reference_state(path, identity)
            path.write_bytes(b"")
            with self.assertRaisesRegex(ValueError, "empty"):
                validate_reference_state(path, identity)


if __name__ == "__main__":
    unittest.main()
