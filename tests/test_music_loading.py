"""Authored boundary/state cases for original music-loading policy."""

import unittest
from dataclasses import replace

from tools.analysis.events import EventError
from tools.analysis.music_loading import (
    PENDING,
    MusicPollInputs,
    MusicSelection,
    correlate_music_state,
    poll_music,
    read_music_state,
)


def state(**changes):
    base = read_music_state(bytes(176), bytes(4))
    return replace(base, **changes)


class MusicLoadingTests(unittest.TestCase):
    def test_pending_wave_prevents_sequence_read_and_requires_observed_result(self):
        before = state(wave_pending=1, deferred_sequence_read=1)
        row = MusicSelection(7, 3, 1)
        result = poll_music(before, row, MusicPollInputs(wave_result=PENDING))
        self.assertEqual((result.state, result.result), (before, PENDING))
        self.assertEqual(result.operations, (("finish_wave_chunks", 5),))
        with self.assertRaises(EventError):
            poll_music(before, row, MusicPollInputs())

    def test_wave_completion_records_bank_before_starting_sequence_read(self):
        before = state(wave_pending=1, deferred_sequence_read=1, loaded_sequence=PENDING)
        result = poll_music(before, MusicSelection(7, 3, 1), MusicPollInputs(wave_result=0))
        self.assertEqual(
            (
                result.state.loaded_wave_bank,
                result.state.wave_pending,
                result.state.wave_loaded_now,
                result.state.sequence_pending,
            ),
            (3, 0, 1, 1),
        )
        self.assertEqual(
            result.operations,
            (
                ("finish_wave_chunks", 5),
                ("wait_audio_service", 16),
                ("free_wave_staging",),
                ("read_sequence", 34),
            ),
        )
        self.assertEqual(result.result, PENDING)

    def test_shared_wave_requires_a_separate_completion_poll(self):
        row = MusicSelection(7, 255, 0)
        result = poll_music(state(), row, MusicPollInputs())
        self.assertEqual(result.state.shared_wave_state, 128)
        self.assertEqual(result.operations, (("start_shared_wave_read", 3),))
        waiting = poll_music(result.state, row, MusicPollInputs(shared_result=PENDING))
        self.assertEqual(waiting.result, PENDING)
        result = poll_music(result.state, row, MusicPollInputs(shared_result=0, disc_busy=0))
        self.assertEqual((result.state.shared_wave_state, result.result), (1, 0))

    def test_deferred_read_yields_even_when_selected_sequence_is_already_loaded(self):
        before = state(deferred_sequence_read=1, loaded_sequence=7)
        result = poll_music(before, MusicSelection(7, 255, 1), MusicPollInputs())
        self.assertEqual(result.state, replace(before, deferred_sequence_read=0))
        self.assertEqual((result.result, result.operations), (PENDING, ()))

    def test_disc_completion_precedes_sequence_creation_and_start(self):
        before = state(sequence_pending=1, start_parameter=PENDING)
        row = MusicSelection(7, 255, 1)
        waiting = poll_music(before, row, MusicPollInputs(disc_busy=2))
        self.assertEqual((waiting.state, waiting.result), (before, PENDING))
        result = poll_music(before, row, MusicPollInputs(disc_busy=0))
        self.assertEqual(
            result.operations,
            (("query_disc_busy",), ("create_sequence",), ("start_sequence", 127, 0)),
        )
        self.assertEqual(
            (
                result.state.loaded_sequence,
                result.state.sequence_active,
                result.state.sequence_pending,
                result.state.completed,
            ),
            (7, 1, 0, 1),
        )
        self.assertEqual(result.state.gate, before.gate)

    def test_reuse_and_alternate_start_parameter_are_separate_paths(self):
        row = MusicSelection(7, 255, 1)
        result = poll_music(
            state(sequence_pending=1, reuse_sequence=2, cached_sequence=123),
            row,
            MusicPollInputs(disc_busy=0),
        )
        self.assertEqual(result.operations[-1], ("resume_sequence", 123, 127, 240))
        self.assertEqual((result.state.reuse_sequence, result.state.cached_sequence), (0, 0))
        result = poll_music(
            state(sequence_pending=1, start_parameter=0), row, MusicPollInputs(disc_busy=0)
        )
        self.assertEqual(
            result.operations[-2:], (("start_sequence", 0, 0), ("configure_sequence", 0, 0))
        )

    def test_original_memory_codec_preserves_opaque_bytes(self):
        opaque = bytes(range(176))
        value = read_music_state(opaque, b"\x01\0\0\0")
        self.assertEqual(correlate_music_state(opaque, value), (opaque, b"\x01\0\0\0"))
        changed, deferred = correlate_music_state(opaque, replace(value, deferred_sequence_read=0))
        self.assertEqual((changed, deferred), (opaque, bytes(4)))
        with self.assertRaises(EventError):
            read_music_state(bytes(175), bytes(4))


if __name__ == "__main__":
    unittest.main()
