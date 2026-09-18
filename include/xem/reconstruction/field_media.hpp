#pragma once

#include "xem/reconstruction/field_battle.hpp"

#include <cstdint>

namespace xem::reconstruction::field {

inline constexpr std::uint32_t music_pending = 0xffffffffU;

struct MusicSelection {
    // Original selected ID and the two-byte row at field 800adfcc + 2*ID.
    std::uint8_t sequence{};
    std::uint8_t wave_bank{};
    std::uint8_t shared_wave_flag{};
};

// Resource tokens are owned by the caller's resource implementation. They are
// never dereferenced or converted to host pointers by recovered field code.
using MusicResource = std::uint32_t;

struct MusicStreamState {
    MusicResource descriptor{}; // field 800adbb8
    MusicResource next_chunk{}; // field 800adbbc
    // Original indirect callback at 800afea4 takes the chunk in A0. The caller
    // registers its actual consumer; missing callbacks fail explicitly.
    std::function<void(MusicResource)> consume_chunk;
};

struct MusicLoadState {
    std::uint32_t gate{};                   // 8004f308; committed by the caller
    std::uint32_t loaded_sequence{};        // 8004f338
    std::uint32_t loaded_wave_bank{};       // 8004f33c
    std::uint32_t start_parameter{};        // 8004f340
    std::uint32_t reuse_sequence{};         // 8004f348
    MusicResource cached_sequence{};        // 8004f2fc
    std::uint32_t wave_pending{};           // 8004f354
    std::uint32_t sequence_pending{};       // 8004f358
    std::uint32_t sequence_active{};        // 8004f35c
    std::uint32_t wave_loaded_now{};        // 8004f360
    std::uint32_t shared_wave_state{};      // 8004f364
    std::uint32_t shared_release_started{}; // 8004f368
    std::uint32_t completed{};              // 8004f36c
    std::uint16_t shared_release_flag{};    // 8004f384
    std::uint32_t deferred_sequence_read{}; // 800afc54

    MusicResource sequence_input{};     // original fixed buffer 80062648
    MusicResource current_sequence{};   // 80062528
    MusicResource wave_staging{};       // 800c3a1c
    MusicResource shared_staging{};     // 800b00e0
    MusicResource shared_wave{};        // 80059560
    MusicResource active_shared_wave{}; // 8006251c
    std::int32_t wave_chunk_index{};    // 800b2370
    MusicResource wave_transfer{};      // 8006258c
    MusicStreamState stream;
};

// These are unresolved original *game calls*, not interchangeable PS1 platform
// services. An actual runtime must recover their implementations. A test double
// validates only the caller's policy; it cannot establish media completion.
// Pure virtual functions deliberately prevent silent successful defaults.
class UnrecoveredMusicCalls {
  public:
    virtual ~UnrecoveredMusicCalls() = default;
    virtual MusicResource next_stream_chunk() = 0; // resident 80028b14
    virtual MusicResource allocate_stream_buffer(std::uint32_t blocks,
                                                 std::uint32_t allocation_mode) = 0; // 8002a260
    virtual void release_stream_chunk(MusicResource chunk) = 0; // resident 8002945c
    virtual MusicResource start_wave_transfer(MusicResource staging, std::uint32_t bytes,
                                              std::uint32_t mode) = 0; // resident 800380d0
    virtual void continue_wave_transfer(MusicResource staging, std::uint32_t bytes) = 0; // 8003827c
    virtual void wait_audio_service(std::uint32_t flags) = 0; // resident 8003bdfc
    virtual void release_buffer(MusicResource buffer) = 0;    // resident 800320e8
    virtual void select_directory(std::uint32_t index, std::uint32_t offset) = 0;       // 80028470
    virtual std::uint32_t file_size(std::uint32_t file) = 0;                            // 800288ec
    virtual MusicResource allocate_buffer(std::uint32_t bytes, std::uint32_t mode) = 0; // 80031bdc
    virtual void read_file(std::uint32_t file, MusicResource destination, std::uint32_t offset,
                           std::uint32_t mode) = 0; // resident 800295d8
    virtual std::uint32_t disc_busy() = 0;          // resident 800286cc
    virtual MusicResource load_shared_wave(MusicResource input, std::uint32_t mode) = 0; // 80037fd8
    virtual MusicResource create_sequence(MusicResource input) = 0; // resident 80039850
    virtual void start_sequence(MusicResource sequence, std::uint32_t volume,
                                std::uint32_t parameter) = 0; // resident 80039a80
    virtual void configure_sequence(MusicResource sequence, std::uint32_t first,
                                    std::uint32_t second) = 0; // resident 8003a89c
    virtual void resume_sequence(MusicResource sequence, std::uint32_t volume,
                                 std::uint32_t parameter) = 0; // resident 80039b68
};

// Original 80085560 and 800854d0. The allocation mode is forwarded in A1
// through 8002a260 into 80031bdc; the callback remains owned by stream state.
// BattleRequestState owns the shared activity gate at 800adb2c.
void start_music_stream(MusicStreamState &state, BattleRequestState &request, std::uint32_t file,
                        std::uint32_t allocation_mode,
                        std::function<void(MusicResource)> consume_chunk,
                        UnrecoveredMusicCalls &calls);
[[nodiscard]] std::uint32_t poll_music_stream(MusicStreamState &state, BattleRequestState &request,
                                              UnrecoveredMusicCalls &calls);
// Original field 80085c3c: up to five streaming steps; inverts the stream's
// finished sentinel into the poller's pending/complete convention.
[[nodiscard]] std::uint32_t finish_music_wave_chunks(MusicStreamState &state,
                                                     BattleRequestState &request,
                                                     UnrecoveredMusicCalls &calls);
// Original callback 800859dc. Views are the caller's actual resource storage;
// token values are not cast to host pointers. The original copies 16 bytes at
// a time, including when the input and staging ranges overlap.
void consume_music_wave_chunk(MusicLoadState &state, MusicResource chunk,
                              std::span<const std::uint8_t, 2048> input,
                              std::span<std::uint8_t, 8192> staging, UnrecoveredMusicCalls &calls);
void start_shared_music_wave(MusicLoadState &state, UnrecoveredMusicCalls &calls);
[[nodiscard]] std::uint32_t finish_shared_music_wave(MusicLoadState &state,
                                                     UnrecoveredMusicCalls &calls);
// Original field 80085c90..80085ee8. Does not commit state.gate.
[[nodiscard]] std::uint32_t poll_music_load(MusicLoadState &state, BattleRequestState &request,
                                            MusicSelection selection, UnrecoveredMusicCalls &calls);
// Only the music statement at 80078b6c..80078b94, not the whole update function
// (whose RNG advance and cooldown decrement have separate owners).
void update_music_load_gate(MusicLoadState &state, BattleRequestState &request,
                            MusicSelection selection, UnrecoveredMusicCalls &calls);
// The recovered FE/A2 route consumes this same authoritative state.
void execute_music_extended_event(EventContext &context, std::uint8_t opcode,
                                  const MusicLoadState &state);

} // namespace xem::reconstruction::field
