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
// Supplied storage is the ring header at state.ring_buffer: the block count,
// then eight bytes per slot (state, sequence). Chunk addresses name the
// payload after it, which these calls never read.
[[nodiscard]] MusicResource select_disc_stream_ring(DiscStreamState &state, MusicResource resource);
[[nodiscard]] std::int32_t reset_disc_stream_ring(const DiscStreamState &state,
                                                  std::span<std::uint8_t> storage);
[[nodiscard]] MusicResource next_disc_stream_chunk(DiscStreamState &state,
                                                   std::span<const std::uint8_t> storage);
[[nodiscard]] std::uint16_t release_disc_stream_chunk(const DiscStreamState &state,
                                                      std::span<std::uint8_t> storage,
                                                      MusicResource chunk);

} // namespace xem::reconstruction::field
