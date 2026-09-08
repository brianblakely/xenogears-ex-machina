"""Invented packet/header cases establish tooling behavior, never original gameplay."""

import struct
import unittest

from tools.reference.inspect_disc import SYNC
from tools.reference.media_survey import MovieHeaders, audio_header


def packet(chunk: int, *, chunks: int = 2, frame: int = 1, width: int = 32) -> bytes:
    raw = bytearray(2352)
    raw[:12] = SYNC
    raw[15] = 2
    raw[16:20] = raw[20:24] = bytes((1, 2, 0x42, 0x80))
    struct.pack_into("<IHHIIHH", raw, 24, 0x80010160, chunk, chunks, frame, 24, width, 16)
    return bytes(raw)


class MediaSurveyTests(unittest.TestCase):
    def test_complete_packet_groups_and_missing_chunks_are_distinct(self):
        headers = MovieHeaders()
        headers.consume(40, packet(0))
        self.assertEqual(len(headers.summary()["missing_or_incomplete_frames"]), 1)
        headers.consume(41, packet(1))
        summary = headers.summary()
        self.assertEqual(summary["missing_or_incomplete_frames"], [])
        self.assertEqual(summary["video_frame_header_count"], 1)
        self.assertEqual(summary["xa_role_counts"], {"video": 2})

    def test_duplicates_inconsistent_headers_and_invalid_indices_remain_visible(self):
        headers = MovieHeaders()
        headers.consume(40, packet(0))
        headers.consume(41, packet(0, width=48))
        headers.consume(42, packet(9))
        summary = headers.summary()
        self.assertEqual(summary["duplicate_video_chunk_lbas"], [41])
        self.assertEqual(summary["invalid_chunk_lbas"], [42])
        self.assertTrue(summary["disagreeing_frame_header_lbas"])
        self.assertTrue(summary["missing_or_incomplete_frames"])

    def test_invalid_raw_sector_cannot_be_interpreted_as_media(self):
        corrupt = bytearray(packet(0))
        corrupt[20] ^= 1
        for data in (packet(0)[:-1], bytes(corrupt), bytes(2352)):
            with self.subTest(size=len(data)), self.assertRaisesRegex(ValueError, "sector"):
                MovieHeaders().consume(40, data)

    def test_audio_checksum_bounds_and_old_versions_are_explicit(self):
        data = bytearray(48)
        data[:4] = b"smds"
        struct.pack_into("<I", data, 8, 48)
        struct.pack_into("<H", data, 12, 0x100)
        total = sum(value[0] for value in struct.iter_unpack("<I", data))
        struct.pack_into("<I", data, 4, -total & 0xFFFFFFFF)
        result = audio_header(bytes(data))
        self.assertEqual(result["header_word_sum_u32"], 0)
        self.assertEqual(result["header_version_u16"], 0x100)
        data[4] ^= 1
        self.assertNotEqual(audio_header(bytes(data))["header_word_sum_u32"], 0)
        struct.pack_into("<I", data, 8, 0xFFFFFFFF)
        with self.assertRaisesRegex(ValueError, "bounds"):
            audio_header(bytes(data))
        with self.assertRaisesRegex(ValueError, "truncated"):
            audio_header(b"smds")


if __name__ == "__main__":
    unittest.main()
