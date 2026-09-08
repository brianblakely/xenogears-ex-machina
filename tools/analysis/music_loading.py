"""Recovered field music-poll policy from original 0x80085c90..0x80085ee8.

This expresses the game-owned loading state and requested service operations.
Wave transfer, disc completion and sequence/audio implementations are explicit
external inputs/operations; none are supplied by no-op substitutes here.
"""

import struct
from dataclasses import dataclass, replace

from tools.analysis.events import EventError

PENDING = 0xFFFFFFFF


@dataclass(frozen=True)
class MusicSelection:
    sequence: int
    wave_bank: int
    shared_wave_flag: int

    def __post_init__(self):
        if not (
            0 <= self.sequence < 255
            and 0 <= self.wave_bank <= 255
            and 0 <= self.shared_wave_flag <= 255
        ):
            raise EventError("music selection requires a qualified byte-sized source-table row")


@dataclass(frozen=True)
class MusicLoadState:
    gate: int
    loaded_sequence: int
    loaded_wave_bank: int
    start_parameter: int
    reuse_sequence: int
    cached_sequence: int
    wave_pending: int
    sequence_pending: int
    sequence_active: int
    wave_loaded_now: int
    shared_wave_state: int
    shared_release_started: int
    completed: int
    shared_release_flag: int
    deferred_sequence_read: int


@dataclass(frozen=True)
class MusicPollInputs:
    # Values are observations of the original services on the chosen path.
    wave_result: int | None = None
    shared_result: int | None = None
    disc_busy: int | None = None


@dataclass(frozen=True)
class MusicPollEffect:
    state: MusicLoadState
    result: int
    operations: tuple[tuple, ...]


def required(value: int | None, name: str) -> int:
    if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
        raise EventError(f"original {name} result is required as a u32 value")
    return value


def poll_music(
    state: MusicLoadState, selection: MusicSelection, inputs: MusicPollInputs
) -> MusicPollEffect:
    """One original field poll, preserving exact sentinel/flag comparisons."""
    operations = []

    def result(value):
        return MusicPollEffect(state, value, tuple(operations))

    if state.wave_pending == 1:
        operations.append(("finish_wave_chunks", 5))
        if required(inputs.wave_result, "wave transfer") == PENDING:
            return result(PENDING)
        # Original 0x8003bdfc waits on audio-service flag 0x10; its internal
        # completion is outside this poll policy and is not a buffer flush.
        operations += [("wait_audio_service", 16), ("free_wave_staging",)]
        state = replace(
            state, wave_pending=0, wave_loaded_now=1, loaded_wave_bank=selection.wave_bank
        )
    if selection.shared_wave_flag == 0:
        if state.shared_wave_state == 0:
            operations.append(("start_shared_wave_read", 3))
            state = replace(state, shared_wave_state=0x80)
            return result(PENDING)
        if state.shared_wave_state & 0x80:
            operations.append(("finish_shared_wave",))
            if required(inputs.shared_result, "shared wave") == PENDING:
                return result(PENDING)
            state = replace(
                state, shared_wave_state=1, shared_release_flag=0, shared_release_started=0
            )
    if state.deferred_sequence_read == 1:
        if state.loaded_sequence != selection.sequence:
            operations.append(("read_sequence", 0x14 + 2 * selection.sequence))
            state = replace(state, sequence_pending=1)
        state = replace(state, deferred_sequence_read=0)
        # The original returns pending even if no read was necessary.
        return result(PENDING)
    operations.append(("query_disc_busy",))
    if required(inputs.disc_busy, "disc busy") != 0:
        return result(PENDING)
    if state.sequence_pending == 1:
        if state.reuse_sequence:
            operations.append(("resume_sequence", state.cached_sequence, 127, 240))
            state = replace(state, reuse_sequence=0, cached_sequence=0)
        else:
            operations.append(("create_sequence",))
            operations.append(("start_sequence", 127 if state.start_parameter == PENDING else 0, 0))
            if state.start_parameter != PENDING:
                operations.append(("configure_sequence", 0, 0))
        state = replace(
            state, sequence_pending=0, sequence_active=1, loaded_sequence=selection.sequence
        )
    state = replace(state, start_parameter=PENDING, completed=1)
    return result(0)


# External analysis correlation, not native state/API offsets. The original
# load-result gate is committed by the caller after the poll returns.
MEDIA_FIELDS = {
    "cached_sequence": 0x1C,
    "gate": 0x28,
    "loaded_sequence": 0x58,
    "loaded_wave_bank": 0x5C,
    "start_parameter": 0x60,
    "reuse_sequence": 0x68,
    "wave_pending": 0x74,
    "sequence_pending": 0x78,
    "sequence_active": 0x7C,
    "wave_loaded_now": 0x80,
    "shared_wave_state": 0x84,
    "shared_release_started": 0x88,
    "completed": 0x8C,
}


def read_music_state(media: bytes, deferred: bytes) -> MusicLoadState:
    if len(media) != 0xB0 or len(deferred) != 4:
        raise EventError("original music-state correlation needs 176 and 4 bytes")
    values = {
        name: struct.unpack_from("<I", media, offset)[0] for name, offset in MEDIA_FIELDS.items()
    }
    values["shared_release_flag"] = struct.unpack_from("<H", media, 0xA4)[0]
    values["deferred_sequence_read"] = int.from_bytes(deferred, "little")
    return MusicLoadState(**values)


def correlate_music_state(media: bytes, state: MusicLoadState) -> tuple[bytes, bytes]:
    if len(media) != 0xB0:
        raise EventError("original music-state prefix must have 176 bytes")
    output = bytearray(media)
    for name, offset in MEDIA_FIELDS.items():
        struct.pack_into("<I", output, offset, getattr(state, name))
    struct.pack_into("<H", output, 0xA4, state.shared_release_flag)
    return bytes(output), state.deferred_sequence_read.to_bytes(4, "little")
