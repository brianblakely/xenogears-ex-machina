#include "xem/reconstruction/disc_stream.hpp"

#include <bit>

namespace xem::reconstruction::field {
namespace {
std::uint32_t read(std::span<const std::uint8_t> storage, std::uint32_t at, std::uint32_t width) {
    if (at > storage.size() || width > storage.size() - at)
        throw EventError("Disc stream ring read exceeds its live allocation");
    std::uint32_t result = 0;
    for (std::uint32_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(storage[at + i]) << (8U * i);
    return result;
}

void write(std::span<std::uint8_t> storage, std::uint32_t at, std::uint32_t value,
           std::uint32_t width) {
    if (at > storage.size() || width > storage.size() - at)
        throw EventError("Disc stream ring write exceeds its live allocation");
    for (std::uint32_t i = 0; i < width; ++i)
        storage[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
} // namespace

// Resident 80028a94: replacing the ring does not reset other stream globals.
MusicResource select_disc_stream_ring(DiscStreamState &state, MusicResource resource) {
    const auto previous = state.ring_buffer;
    state.ring_buffer = resource;
    return previous;
}

// Resident 80028aac: preserve all bytes outside the slot records and count store.
std::int32_t reset_disc_stream_ring(const DiscStreamState &state, std::span<std::uint8_t> storage) {
    if (state.ring_buffer == 0)
        return -1;
    const auto count = std::bit_cast<std::int32_t>(read(storage, 0, 4));
    for (std::int32_t index = 0; index < count; ++index) {
        const auto offset = static_cast<std::uint32_t>(index) * 8U + 4U;
        write(storage, offset, 0, 2);
        write(storage, offset + 2U, 0, 2);
        write(storage, offset + 4U, 0, 2);
        write(storage, offset + 6U, 0, 2);
    }
    write(storage, 8, static_cast<std::uint16_t>(count), 2);
    return count;
}

// Retail path of resident 80028b14. Host debugger-file reads and their fatal
// diagnostics remain explicit unsupported game logic, not native CD services.
MusicResource next_disc_stream_chunk(DiscStreamState &state,
                                     std::span<const std::uint8_t> storage) {
    if (state.ring_buffer == 0)
        return 0;
    if (state.host_file_table != 0)
        throw EventError("Unrecovered host-file branch of original disc stream reader");
    const auto count_bits = read(storage, 0, 4);
    const auto count = std::bit_cast<std::int32_t>(count_bits);
    std::int32_t index = 0;
    for (; index < count; ++index) {
        const auto offset = static_cast<std::uint32_t>(index) * 8U + 4U;
        if (read(storage, offset, 2) == 3 &&
            read(storage, offset + 2U, 2) == state.expected_sequence)
            break;
    }
    // The original compares with the separate active count, not the header.
    if (index == state.active_block_count)
        return 0;
    state.expected_sequence = static_cast<std::uint16_t>(state.expected_sequence + 1U);
    return state.ring_buffer + count_bits * 8U + static_cast<std::uint32_t>(index) * 2048U + 36U;
}

// Resident 8002945c. Interior chunk pointers alias their containing 2048-byte slot.
std::uint16_t release_disc_stream_chunk(const DiscStreamState &state,
                                        std::span<std::uint8_t> storage, MusicResource chunk) {
    if (state.ring_buffer == 0)
        return 0xffff;
    if (chunk == 0)
        return 0;
    const auto payload = state.ring_buffer + read(storage, 0, 4) * 8U + 36U;
    const auto index = (chunk - payload) >> 11U;
    const auto offset = index * 8U + 4U;
    const auto previous = static_cast<std::uint16_t>(read(storage, offset, 2));
    write(storage, offset, 0, 2);
    return previous;
}

} // namespace xem::reconstruction::field
