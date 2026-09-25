#include "xem/reconstruction/disc_stream.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void half(std::span<std::uint8_t> data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}
std::uint16_t half(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset] |
                                      (static_cast<std::uint32_t>(data[offset + 1]) << 8U));
}
// Authored call doubles around the actual ring operations (the resident
// 8002a260 allocation shape, 80028b14 and 8002945c), so the connected source
// test cannot substitute their results.
struct Calls final : field::MusicCalls {
    static constexpr field::MusicResource ring_token = 0x1000;
    std::array<std::uint8_t, 8 * 0x808 + 0x24> ring;
    std::vector<std::string> operations;
    field::MusicResource allocated{ring_token};
    field::DiscStreamState &disc_stream;
    std::function<void(field::MusicResource)> consume;
    explicit Calls(field::DiscStreamState &state) : disc_stream(state) { ring.fill(0xaa); }
    static void unsupported() { throw std::runtime_error("Unexpected unresolved test dependency"); }
    std::span<std::uint8_t> storage(field::MusicResource resource) {
        check(resource == ring_token, "Host lookup requires the allocated token");
        return ring;
    }
    field::MusicResource allocate_stream_buffer(std::uint32_t blocks, std::uint32_t mode) override {
        if (static_cast<std::int32_t>(blocks) < 1)
            return 0;
        operations.push_back("allocate " + std::to_string(blocks * 0x808U + 0x24U) + " " +
                             std::to_string(mode));
        if (allocated == 0)
            return 0;
        const auto bytes = storage(allocated);
        std::fill_n(bytes.begin(), 4, 0);
        bytes[0] = static_cast<std::uint8_t>(blocks);
        (void)field::select_disc_stream_ring(disc_stream, allocated);
        (void)field::reset_disc_stream_ring(disc_stream, bytes);
        return allocated;
    }
    field::MusicResource next_stream_chunk() override {
        return field::next_disc_stream_chunk(disc_stream, disc_stream.ring_buffer == 0
                                                              ? std::span<std::uint8_t>{}
                                                              : storage(disc_stream.ring_buffer));
    }
    void release_stream_chunk(field::MusicResource chunk) override {
        (void)field::release_disc_stream_chunk(disc_stream, storage(disc_stream.ring_buffer),
                                               chunk);
    }
    void consume_stream_chunk(std::uint32_t consumer, field::MusicResource chunk) override {
        check(consumer == 0x800859dc && consume, "Chunk callback address");
        consume(chunk);
    }
    field::MusicResource allocate_buffer(std::uint32_t, std::uint32_t) override {
        unsupported();
        return 0;
    }
    void read_file(std::uint32_t file, field::MusicResource destination, std::uint32_t offset,
                   std::uint32_t mode) override {
        check(file == 31 && destination == ring_token && offset == 0 && mode == 256,
              "Original stream read arguments");
        operations.push_back("read");
        // Read setup and asynchronous CD delivery are explicitly synthetic here.
        disc_stream.active_block_count = 8;
        disc_stream.expected_sequence = 10;
        for (std::size_t i = 0; i < 5; ++i) {
            half(ring, 4 + i * 8, 3);
            half(ring, 6 + i * 8, static_cast<std::uint16_t>(10 + i));
            std::fill_n(ring.begin() + static_cast<std::ptrdiff_t>(100 + i * 2048), 2048,
                        static_cast<std::uint8_t>(i + 1));
        }
    }
    std::uint32_t disc_busy() override {
        operations.push_back("busy");
        return 0;
    }
    void release_buffer(field::MusicResource resource) override {
        check(resource == ring_token, "Stream completion releases the allocated descriptor");
        operations.push_back("free");
    }
    void wait_audio_service(std::uint32_t flags) override {
        check(flags == 16, "Wave wait flags");
        operations.push_back("wait");
    }
    field::MusicResource start_wave_transfer(field::MusicResource staging, std::uint32_t bytes,
                                             std::uint32_t mode) override {
        check(staging == 0x8000 && bytes == 8192 && mode == 0, "Initial wave transfer arguments");
        operations.push_back("wave_start");
        return 77;
    }
    void continue_wave_transfer(field::MusicResource staging, std::uint32_t bytes) override {
        check(staging == 0x8000 && bytes == 2048, "Continuation wave transfer arguments");
        operations.push_back("wave_continue");
    }
    void select_directory(std::uint32_t, std::uint32_t) override { unsupported(); }
    std::uint32_t file_size(std::uint32_t) override {
        unsupported();
        return 0;
    }
    field::MusicResource load_shared_wave(field::MusicResource, std::uint32_t) override {
        unsupported();
        return 0;
    }
    field::MusicResource create_sequence(field::MusicResource) override {
        unsupported();
        return 0;
    }
    void start_sequence(field::MusicResource, std::uint32_t, std::uint32_t) override {
        unsupported();
    }
    void configure_sequence(field::MusicResource, std::uint32_t, std::uint32_t) override {
        unsupported();
    }
    void resume_sequence(field::MusicResource, std::uint32_t, std::uint32_t) override {
        unsupported();
    }
};

template <typename Function> void rejects(Function function, const char *message) {
    try {
        function();
    } catch (const field::EventError &) {
        return;
    }
    throw std::runtime_error(message);
}

void allocation_and_reset() {
    field::DiscStreamState state;
    state.expected_sequence = 73;
    state.active_block_count = 17;
    Calls calls(state);
    check(calls.allocate_stream_buffer(0, 2) == 0 &&
              calls.allocate_stream_buffer(0x80000000U, 2) == 0 && calls.operations.empty(),
          "Original nonpositive block count returns null before allocating");
    check(calls.allocate_stream_buffer(8, 2) == Calls::ring_token &&
              calls.operations == std::vector<std::string>{"allocate 16484 2"} &&
              state.ring_buffer == Calls::ring_token && state.expected_sequence == 73 &&
              state.active_block_count == 17,
          "Allocation forwards mode and selects ring while preserving separate stream globals");
    check(calls.ring[0] == 8 && calls.ring[1] == 0 && calls.ring[2] == 0 && calls.ring[3] == 0,
          "Allocation writes the original 32-bit block count");
    for (std::size_t i = 4; i < 68; ++i)
        check(calls.ring[i] == (i == 8 ? 8 : 0),
              "Reset clears only the slot records and count field");
    for (std::size_t i = 68; i < calls.ring.size(); ++i)
        check(calls.ring[i] == 0xaa, "Ring reset preserves opaque header tail and payload bytes");
    calls.allocated = 0;
    const auto before = calls.ring;
    check(calls.allocate_stream_buffer(5, 1) == 0 && state.ring_buffer == Calls::ring_token &&
              calls.ring == before,
          "Allocation failure preserves the previously selected ring");
    check(field::select_disc_stream_ring(state, 0) == Calls::ring_token &&
              field::reset_disc_stream_ring(state, {}) == -1 &&
              field::next_disc_stream_chunk(state, {}) == 0 &&
              field::release_disc_stream_chunk(state, {}, 17) == 0xffff,
          "Null ring operations preserve original sentinel returns");
}

void selection_release_and_boundaries() {
    field::DiscStreamState state;
    Calls calls(state);
    (void)calls.allocate_stream_buffer(8, 1);
    state.active_block_count = 8;
    state.expected_sequence = 0xffff;
    half(calls.ring, 4 + 3 * 8, 3);
    half(calls.ring, 6 + 3 * 8, 0xffff);
    const auto before = calls.ring;
    const auto selected = calls.next_stream_chunk();
    check(
        selected == Calls::ring_token + 100 + 3 * 2048 && state.expected_sequence == 0 &&
            calls.ring == before,
        "Retail ring scans ready flags/sequence, wraps the u16 sequence, and preserves slot bytes");
    check(calls.next_stream_chunk() == 0 && state.expected_sequence == 0,
          "Exhausted matching scan returns null without advancing sequence");
    check(field::release_disc_stream_chunk(state, calls.ring, selected + 2047) == 3,
          "Interior chunk pointers retain original slot-alias arithmetic");
    for (std::size_t i = 0; i < calls.ring.size(); ++i)
        check(calls.ring[i] == ((i == 28 || i == 29) ? 0 : before[i]),
              "Release clears exactly the selected slot state halfword");
    check(field::release_disc_stream_chunk(state, {}, 0) == 0,
          "Null chunk release succeeds without reading storage");
    state.active_block_count = 7;
    check(calls.next_stream_chunk() == Calls::ring_token + 100 + 8 * 2048 &&
              state.expected_sequence == 1,
          "Mismatched active/header counts return the original address past the payload");
    state.host_file_table = 1;
    rejects([&] { (void)calls.next_stream_chunk(); },
            "Unrecovered debugger-file path must fail explicitly");
    check(state.expected_sequence == 1,
          "Unsupported host path fails before retail state mutations");
    state.host_file_table = 0;
    rejects([&] { (void)field::release_disc_stream_chunk(state, calls.ring, Calls::ring_token); },
            "Out-of-range ring slot cannot silently release unrelated storage");
}

void connected_ring_stream_wave() {
    field::DiscStreamState disc;
    Calls calls(disc);
    field::MusicLoadState music;
    music.wave_staging = 0x8000;
    field::BattleRequestState request;
    std::array<std::uint8_t, 8192> staging{};
    calls.consume = [&](field::MusicResource chunk) {
        const auto offset = chunk - Calls::ring_token;
        const std::span<const std::uint8_t, 2048> input(calls.ring.data() + offset, 2048);
        field::consume_music_wave_chunk(music, chunk, input, staging, calls);
    };
    field::start_music_stream(music.stream, request, 31, 1, 0x800859dc, calls);
    check(field::finish_music_wave_chunks(music.stream, request, calls) == field::music_pending &&
              disc.expected_sequence == 15 && music.wave_transfer == 77 && request.menu_gate == 1,
          "Actual ring selection and release feed the recovered five-step wave callback");
    for (std::size_t i = 0; i < 5; ++i)
        check(half(std::span<const std::uint8_t>(calls.ring), 4 + i * 8) == 0,
              "Each actual callback releases its actual selected ring slot");
    check(field::finish_music_wave_chunks(music.stream, request, calls) == 0 &&
              request.menu_gate == 0,
          "Actual empty ring and idle disc release the field's shared activity gate");
    check(calls.operations == std::vector<std::string>{"allocate 16484 1", "read", "wave_start",
                                                       "wait", "wave_continue", "busy", "free"},
          "Connected source crosses only the declared heap/CD/audio dependencies");
    for (std::size_t i = 0; i < staging.size(); ++i)
        check(staging[i] == (i < 2048 ? 5 : i / 2048 + 1), "Full connected ring/staging bytes");
}
} // namespace
int main() {
    try {
        allocation_and_reset();
        selection_release_and_boundaries();
        connected_ring_stream_wave();
        std::cout << "Resident disc ring and connected field wave source passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
