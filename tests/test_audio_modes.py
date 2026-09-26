"""Synthetic recordings exercise the sound-mode signal comparison. They
describe no original observation."""

import random
import tempfile
import unittest
import wave
from array import array
from pathlib import Path

from tools.analysis.audio_modes import (
    AudioModeError,
    analyse,
    first_difference,
    parse_window,
    read_wav,
)

RATE = 1200  # 20 samples per frame at 60 fps
DELAY = 37


def write(path: Path, left: list[int], right: list[int], channels: int = 2) -> Path:
    samples = array("h", [v for pair in zip(left, right, strict=True) for v in pair])
    with wave.open(str(path), "wb") as sink:
        sink.setnchannels(channels)
        sink.setsampwidth(2)
        sink.setframerate(RATE)
        sink.writeframes(samples.tobytes())
    return path


class AudioModeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        generator = random.Random(7)
        n = RATE * 20
        self.dry_left = [generator.randint(-4000, 4000) for _ in range(n)]
        self.dry_right = [generator.randint(-4000, 4000) for _ in range(n)]
        # A delayed, attenuated copy stands in for a reverb return.
        self.echo = [0] * DELAY + [v // 2 for v in self.dry_left[:-DELAY]]
        self.switch = RATE * 2  # the modes agree for the first two seconds
        stereo_left = [d + e for d, e in zip(self.dry_left, self.echo, strict=True)]
        stereo_right = list(self.dry_right)
        wide_left = [
            s if i < self.switch else d - e
            for i, (s, d, e) in enumerate(zip(stereo_left, self.dry_left, self.echo, strict=True))
        ]
        wide_right = [s if i < self.switch else -s - 1 for i, s in enumerate(stereo_right)]
        self.paths = {
            "stereo": write(self.root / "stereo.wav", stereo_left, stereo_right),
            "wide": write(self.root / "wide.wav", wide_left, wide_right),
            "mono": write(self.root / "mono.wav", stereo_left, stereo_right),
        }

    def tearDown(self):
        self.directory.cleanup()

    def test_matched_recordings_report_inversion_identity_and_delay(self):
        report = analyse(self.paths, [("after", 180, 1200)], 60.0, 10.0, 60.0)
        self.assertEqual(
            report["first_difference"]["stereo/wide"], {"sample": self.switch, "frame": 120.0}
        )
        self.assertIsNone(report["first_difference"]["stereo/mono"])
        window = report["windows"]["after"]
        relation = window["wide_vs_stereo"]
        self.assertEqual(relation["right_inversion"]["max_abs_residual"], 1)
        self.assertEqual(relation["right_inversion"]["common_residuals"], [[-1, 20400]])
        self.assertEqual(relation["right_lag_correlation"]["0"], -1.0)
        self.assertEqual(window["mono_vs_stereo"]["left_identity"]["exact_fraction"], 1.0)
        flipped = window["wide_left_flipped_part"]
        self.assertEqual(flipped["delay_after_kept"]["lag_samples"], DELAY)
        self.assertLess(abs(flipped["delay_after_kept"]["correlation_at_zero"]), 0.05)

    def test_identical_windows_have_no_flipped_part(self):
        report = analyse(self.paths, [("before", 0, 100)], 60.0, 1.0, 10.0)
        window = report["windows"]["before"]
        self.assertNotIn("wide_left_flipped_part", window)
        self.assertEqual(window["wide_vs_stereo"]["left_identity"]["exact_fraction"], 1.0)

    def test_first_difference_of_equal_recordings(self):
        stereo = read_wav(self.paths["stereo"])
        self.assertIsNone(first_difference(stereo, stereo))

    def test_rejects_bad_inputs(self):
        with self.assertRaises(AudioModeError):
            parse_window("empty:10:10")
        with self.assertRaises(AudioModeError):
            analyse(self.paths, [("late", 5000, 6000)], 60.0, 1.0, 10.0)
        single = self.root / "single.wav"
        with wave.open(str(single), "wb") as sink:
            sink.setnchannels(1)
            sink.setsampwidth(2)
            sink.setframerate(RATE)
            sink.writeframes(bytes(4))
        with self.assertRaises(AudioModeError):
            read_wav(single)


if __name__ == "__main__":
    unittest.main()
