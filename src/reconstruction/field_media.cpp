#include "xem/reconstruction/field_media.hpp"

#include <algorithm>
#include <utility>

namespace xem::reconstruction::field {

// EVID-REF-018; qualified field overlay identity is event_source_overlay_sha256.
void start_music_stream(MusicStreamState &state, BattleRequestState &request, std::uint32_t file,
                        std::uint32_t allocation_mode,
                        std::function<void(MusicResource)> consume_chunk,
                        UnrecoveredMusicCalls &calls) {
    if (!consume_chunk)
        throw EventError("Music stream requires its recovered chunk consumer");
    request.menu_gate = 1;
    state.descriptor = calls.allocate_stream_buffer(8, allocation_mode);
    calls.read_file(file, state.descriptor, 0, 0x100);
    state.consume_chunk = std::move(consume_chunk);
}

std::uint32_t poll_music_stream(MusicStreamState &state, BattleRequestState &request,
                                UnrecoveredMusicCalls &calls) {
    state.next_chunk = calls.next_stream_chunk();
    if (state.next_chunk == 0) {
        // The original rechecks next_chunk after the disc call. Preserve that
        // shared-state dependency instead of caching the pre-call observation.
        if (calls.disc_busy() == 0 && state.next_chunk == 0) {
            calls.release_buffer(state.descriptor);
            request.menu_gate = 0;
            return music_pending;
        }
    } else {
        if (!state.consume_chunk)
            throw EventError("Music stream chunk consumer is unrecovered");
        state.consume_chunk(state.next_chunk);
    }
    return 0;
}

std::uint32_t finish_music_wave_chunks(MusicStreamState &state, BattleRequestState &request,
                                       UnrecoveredMusicCalls &calls) {
    for (unsigned i = 0; i < 5; ++i) {
        if (poll_music_stream(state, request, calls) == music_pending)
            return 0;
    }
    return music_pending;
}

void consume_music_wave_chunk(MusicLoadState &state, MusicResource chunk,
                              std::span<const std::uint8_t, 2048> input,
                              std::span<std::uint8_t, 8192> staging, UnrecoveredMusicCalls &calls) {
    const auto copy_chunk = [&](std::size_t offset) {
        for (std::size_t i = 0; i < input.size(); i += 16) {
            // Four word loads precede four stores in each original iteration.
            std::array<std::uint8_t, 16> words;
            std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(i), 16, words.begin());
            std::copy(words.begin(), words.end(),
                      staging.begin() + static_cast<std::ptrdiff_t>(offset + i));
        }
    };
    if (state.wave_chunk_index < 0)
        return;
    if (state.wave_chunk_index < 4) {
        copy_chunk(static_cast<std::size_t>(state.wave_chunk_index) * 2048);
        ++state.wave_chunk_index;
        calls.release_stream_chunk(chunk);
        if (state.wave_chunk_index == 4)
            state.wave_transfer = calls.start_wave_transfer(state.wave_staging, 8192, 0);
    } else if (state.wave_chunk_index == 4) {
        calls.wait_audio_service(0x10);
        copy_chunk(0);
        calls.continue_wave_transfer(state.wave_staging, 2048);
        calls.release_stream_chunk(chunk);
    }
}

// Original 80085fb8..80086020, including directory restoration after the read.
void start_shared_music_wave(MusicLoadState &state, UnrecoveredMusicCalls &calls) {
    calls.select_directory(0x1c, 0);
    const auto size = calls.file_size(3);
    state.shared_staging = calls.allocate_buffer(size, 1);
    calls.read_file(3, state.shared_staging, 0, 0x80);
    calls.select_directory(4, 0);
    state.shared_wave_state = 0x80;
}

// Original 80085f30..80085fb4. The two shared-wave owners receive the same object.
std::uint32_t finish_shared_music_wave(MusicLoadState &state, UnrecoveredMusicCalls &calls) {
    if (calls.disc_busy() != 0)
        return music_pending;
    const auto wave = calls.load_shared_wave(state.shared_staging, 0);
    state.active_shared_wave = wave;
    state.shared_wave = wave;
    calls.wait_audio_service(0x10);
    calls.release_buffer(state.shared_staging);
    state.shared_wave_state = 1;
    state.shared_release_flag = 0;
    state.shared_release_started = 0;
    return 0;
}

std::uint32_t poll_music_load(MusicLoadState &state, BattleRequestState &request,
                              MusicSelection selection, UnrecoveredMusicCalls &calls) {
    if (selection.sequence == 255)
        throw EventError("Music poll requires a source-qualified selector, not the stop sentinel");
    if (state.wave_pending == 1) {
        if (finish_music_wave_chunks(state.stream, request, calls) == music_pending)
            return music_pending;
        calls.wait_audio_service(0x10);
        calls.release_buffer(state.wave_staging);
        state.wave_pending = 0;
        state.wave_loaded_now = 1;
        state.loaded_wave_bank = selection.wave_bank;
    }
    if (selection.shared_wave_flag == 0) {
        if (state.shared_wave_state == 0) {
            start_shared_music_wave(state, calls);
            return music_pending;
        }
        if ((state.shared_wave_state & 0x80U) != 0 &&
            finish_shared_music_wave(state, calls) == music_pending)
            return music_pending;
    }
    if (state.deferred_sequence_read == 1) {
        if (state.loaded_sequence != selection.sequence) {
            calls.select_directory(0x1c, 0);
            calls.read_file(0x14U + 2U * selection.sequence, state.sequence_input, 0, 0x80);
            state.sequence_pending = 1;
            calls.select_directory(4, 0);
        }
        state.deferred_sequence_read = 0;
        return music_pending;
    }
    if (calls.disc_busy() != 0)
        return music_pending;
    if (state.sequence_pending == 1) {
        if (state.reuse_sequence == 0) {
            state.current_sequence = calls.create_sequence(state.sequence_input);
            if (state.start_parameter == music_pending) {
                calls.start_sequence(state.current_sequence, 127, 0);
            } else {
                calls.start_sequence(state.current_sequence, 0, 0);
                calls.configure_sequence(state.current_sequence, 0, 0);
            }
        } else {
            state.current_sequence = state.cached_sequence;
            calls.resume_sequence(state.cached_sequence, 127, 240);
            state.reuse_sequence = 0;
            state.cached_sequence = 0;
        }
        state.sequence_pending = 0;
        state.sequence_active = 1;
        state.loaded_sequence = selection.sequence;
    }
    state.start_parameter = music_pending;
    state.completed = 1;
    return 0;
}

void update_music_load_gate(MusicLoadState &state, BattleRequestState &request,
                            MusicSelection selection, UnrecoveredMusicCalls &calls) {
    if (state.gate == music_pending)
        state.gate = poll_music_load(state, request, selection, calls);
}

void execute_music_extended_event(EventContext &context, std::uint8_t opcode,
                                  const MusicLoadState &state) {
    if (context.current_actor == nullptr)
        throw EventError("Extended music event requires the current actor");
    if (context.program.byte(context.current_actor->pc) != opcode)
        throw EventError("Extended opcode disagrees with the working PC");
    if (opcode != 0xa2)
        throw UnsupportedExtendedInstruction(context.current_actor->pc, opcode);
    wait_music_load_extended(context, state.gate);
}

} // namespace xem::reconstruction::field
