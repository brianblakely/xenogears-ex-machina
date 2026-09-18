#pragma once

#include "xem/reconstruction/field_media.hpp"

namespace xem::reconstruction::field {

struct DiscStreamState {
    MusicResource ring_buffer{};       // resident 8004fe30
    std::int32_t active_block_count{}; // resident 8004fe40, assigned by read setup
    std::uint16_t expected_sequence{}; // resident 8004fe24
    std::uint32_t host_file_table{};   // resident 8004fe48; nonzero debugger path unsupported
};

// Resource values identify byte-addressable 32-bit allocations. Arithmetic is
// unsigned original address arithmetic; no resource value becomes a host pointer.
// Supplied storage must belong to state.ring_buffer and remain live for the call.
[[nodiscard]] MusicResource select_disc_stream_ring(DiscStreamState &state, MusicResource resource);
[[nodiscard]] std::int32_t reset_disc_stream_ring(const DiscStreamState &state,
                                                  std::span<std::uint8_t> storage);
[[nodiscard]] MusicResource next_disc_stream_chunk(DiscStreamState &state,
                                                   std::span<const std::uint8_t> storage);
[[nodiscard]] std::uint16_t release_disc_stream_chunk(const DiscStreamState &state,
                                                      std::span<std::uint8_t> storage,
                                                      MusicResource chunk);

// Concrete recovered ring calls connect the field stream/poll/callback source
// to the actual resident ring algorithms. Remaining pure virtual methods are
// still the explicit unresolved game dependencies in UnrecoveredMusicCalls.
class DiscStreamMusicCalls : public UnrecoveredMusicCalls {
  public:
    explicit DiscStreamMusicCalls(DiscStreamState &state) : disc_stream(state) {}
    MusicResource allocate_stream_buffer(std::uint32_t blocks, std::uint32_t allocation_mode) final;
    MusicResource next_stream_chunk() final;
    void release_stream_chunk(MusicResource chunk) final;

  protected:
    DiscStreamState &disc_stream;
    // Host storage lookup, not an original game call or a heap implementation.
    // Reject missing/released tokens. The original heap at 80031bdc remains the
    // inherited allocate_buffer dependency, including its allocation-mode input.
    virtual std::span<std::uint8_t> stream_buffer_storage(MusicResource resource) = 0;
};

} // namespace xem::reconstruction::field
