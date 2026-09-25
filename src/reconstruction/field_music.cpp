// Field music load of overlay 38a1ce82...: the load 80085b20, the poll
// 80085c90 the main loop calls while 8004f308 is -1, the wave stream
// (80085560, 800854d0, 80085c3c) with its chunk callback 800859dc, and the
// release of the shared wave bank 80086024. field_media.cpp holds their
// control flow; Program::Music connects each call it makes to the recovered
// resident code: the stream ring (80028b14, 8002945c, 8002a260), the heap
// (80031bdc, 800320e8), disc reads (80028470, 800286cc, 800295d8) and the
// sound driver (800380d0, 8003827c, 8003bdfc, 80039850, 80039a80,
// 8003a89c). Calls the frozen route does not reach stop with
// MissingDependency naming their address.
#include "xem/reconstruction/disc_stream.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_heap.hpp"

#include <algorithm>
#include <string>

namespace xem::reconstruction {
namespace {
std::string hex(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(8, '0');
    for (int i = 7; i >= 0; --i, value >>= 4)
        text[static_cast<std::size_t>(i)] = digits[value & 15U];
    return text;
}
[[noreturn]] void music_call(std::uint32_t address) {
    throw MissingDependency({"music_call", address, {}, {}}, "symbol:music-call-" + hex(address),
                            false, "This music call is not reconstructed");
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value) {
    if (offset + 4 > data.size())
        throw field::FieldFormatError("Music block write exceeds owned storage");
    for (std::size_t i = 0; i < 4; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
constexpr std::uint32_t chunk_callback = 0x800859dc;
} // namespace

class Program::Music final : public field::MusicCalls {
  public:
    explicit Music(Program &owner) : program(owner) {}

    void consume_stream_chunk(std::uint32_t consumer, field::MusicResource chunk) override {
        if (consumer != chunk_callback)
            music_call(consumer);
        program.consume_music_chunk(chunk);
    }
    // Resident 8002a260: allocate count * 808 + 24 bytes, store the count,
    // then select (80028a94) and reset (80028aac) the ring. The header is
    // the ring the disc reads fill; the payload after it holds the chunks.
    field::MusicResource allocate_stream_buffer(std::uint32_t blocks, std::uint32_t mode) override {
        auto &state = program.resident;
        if (static_cast<std::int32_t>(blocks) < 1)
            return 0;
        auto block = resident::heap_allocate(state.heap, blocks * 0x808U + 0x24U, mode, 0x8002a284);
        if (!block)
            return 0;
        auto &read = state.disc_read;
        if (!read.ring.bytes.empty())
            throw MissingDependency({"music_stream_ring", 0x8002a29c, {}, {}},
                                    "state:disc-ring-replacement", false,
                                    "Replacing a Program-owned disc ring is not connected");
        put(block->bytes, 0, blocks);
        const auto header = static_cast<std::ptrdiff_t>(blocks * 8U + 0x24U);
        read.ring = {block->address, {block->bytes.begin(), block->bytes.begin() + header}};
        read.ring_payload = {block->address + static_cast<std::uint32_t>(header),
                             {block->bytes.begin() + header, block->bytes.end()}};
        static_cast<void>(field::select_disc_stream_ring(state.disc_stream, block->address));
        static_cast<void>(field::reset_disc_stream_ring(state.disc_stream, read.ring.bytes));
        return block->address;
    }
    // Resident 80028b14 and 8002945c on the owned ring header. A stream
    // step (800854d0) starts with 80028b14: arrivals recorded since the poll
    // began or during the previous step precede it.
    field::MusicResource next_stream_chunk() override {
        program.deliver_arrivals(0x80085c90);
        program.deliver_arrivals(0x800854d0);
        return field::next_disc_stream_chunk(program.resident.disc_stream, ring());
    }
    void release_stream_chunk(field::MusicResource chunk) override {
        static_cast<void>(
            field::release_disc_stream_chunk(program.resident.disc_stream, ring(), chunk));
    }
    field::MusicResource start_wave_transfer(field::MusicResource staging, std::uint32_t bytes,
                                             std::uint32_t mode) override {
        return program.load_wave_bank(staging, bytes, mode);
    }
    void continue_wave_transfer(field::MusicResource staging, std::uint32_t bytes) override {
        static_cast<void>(program.continue_wave_upload(staging, bytes));
    }
    void wait_audio_service(std::uint32_t flags) override {
        static_cast<void>(program.sound_wait(flags));
    }
    // 800320e8 of the stream buffer (from 800854d0) or the wave staging
    // (from 80085c90).
    void release_buffer(field::MusicResource buffer) override {
        const auto &music = program.resident.music;
        if (buffer == music.stream.descriptor)
            program.release_music_buffer(buffer, 0x8008553c);
        else if (buffer == music.wave_staging)
            program.release_music_buffer(buffer, 0x80085cd8);
        else
            music_call(0x800320e8);
    }
    void select_directory(std::uint32_t index, std::uint32_t offset) override {
        static_cast<void>(program.select_directory(index, offset));
    }
    void read_file(std::uint32_t file, field::MusicResource destination, std::uint32_t offset,
                   std::uint32_t mode) override {
        static_cast<void>(
            program.read_file(static_cast<std::int32_t>(file), destination, offset, mode));
    }
    std::uint32_t disc_busy() override { return program.disc_busy(); }
    field::MusicResource create_sequence(field::MusicResource input) override {
        return program.open_sequence(input);
    }
    void start_sequence(field::MusicResource sequence, std::uint32_t volume,
                        std::uint32_t frames) override {
        program.start_sequence(sequence, volume, frames);
    }
    void configure_sequence(field::MusicResource sequence, std::uint32_t volume,
                            std::uint32_t frames) override {
        program.sequence_volume(sequence, volume, frames);
    }
    // The shared wave bank (80085fb8, 80085f30) and a kept sequence
    // (80039b68) are not on the frozen route.
    std::uint32_t file_size(std::uint32_t) override { music_call(0x800288ec); }
    field::MusicResource allocate_buffer(std::uint32_t, std::uint32_t) override {
        music_call(0x80031bdc);
    }
    field::MusicResource load_shared_wave(field::MusicResource, std::uint32_t) override {
        music_call(0x80037fd8);
    }
    void resume_sequence(field::MusicResource, std::uint32_t, std::uint32_t) override {
        music_call(0x80039b68);
    }

  private:
    Program &program;

    // The selected ring's header: the block count, then a state and a
    // sequence halfword per slot.
    std::span<std::uint8_t> ring() {
        auto &state = program.resident;
        const auto address = state.disc_stream.ring_buffer;
        if (address == 0)
            return {};
        if (state.disc_read.ring.address != address || state.disc_read.ring.bytes.empty())
            throw field::FieldFormatError("The selected stream ring is not owned");
        return state.disc_read.ring.bytes;
    }
};

// Field 80086024: release the shared wave bank once; mark it unloaded.
void Program::release_shared_wave() {
    auto &music = resident.music;
    if (music.shared_release_started == 0) {
        music.shared_release_flag = 1;
        resident::release_wave_bank(resident.sound, music.active_shared_wave); // 80038310
        music.shared_release_started = 1;
    }
    music.shared_wave_state = 0;
}

// Field 80085b20: stop the music and start loading `id` (ff only stops).
void Program::load_music(std::uint32_t id) {
    auto &music = resident.music;
    disc_wait(0);
    stop_music();
    if (id == 0xff) {
        music.gate = 0;
        return;
    }
    static_cast<void>(select_directory(0x1c, 0));
    // The row at 800adfcc + 2 * id: wave bank, then the shared-wave flag.
    const auto row = 0x800adfccU + id * 2U;
    if (overlay_byte(row + 1) == 1)
        release_shared_wave();
    const std::uint32_t bank = overlay_byte(row);
    if (bank != 0xff && bank != music.loaded_wave_bank) {
        Music calls(*this);
        // 80085560 with the chunk callback 800859dc.
        field::start_music_stream(music.stream, resident.battle_request, bank * 2U + 0x13U, 1,
                                  chunk_callback, calls);
        music.wave_pending = 1;
        music.wave_chunk_index = 0;
        auto staging = resident::heap_allocate(resident.heap, 0x2000, 1, 0x80085be8);
        music.wave_staging = staging ? staging->address : 0;
        if (staging)
            resident.music_blocks.push_back(std::move(*staging));
    }
    static_cast<void>(select_directory(4, 0));
    music.gate = 0xffffffffU;
    music.deferred_sequence_read = 1;
}

std::uint32_t Program::poll_music(std::uint32_t id) {
    if (id >= 0xff)
        throw field::FieldFormatError("The music poll requires a music id below ff");
    // The row at 800adfcc + 2 * id: wave bank, then the shared-wave flag.
    const auto row = 0x800adfccU + id * 2U;
    const field::MusicSelection selection{static_cast<std::uint8_t>(id), overlay_byte(row),
                                          overlay_byte(row + 1)};
    Music calls(*this);
    const auto result =
        field::poll_music_load(resident.music, resident.battle_request, selection, calls);
    // Arrivals recorded since the poll's last position end with it.
    for (const auto point : {0x80085c90U, 0x800854d0U, 0x8003b424U})
        deliver_arrivals(point);
    return result;
}

void Program::consume_music_chunk(std::uint32_t chunk) {
    auto &music = resident.music;
    // Indices outside 0..4 return before reading either buffer.
    if (music.wave_chunk_index < 0 || music.wave_chunk_index > 4)
        return;
    const auto input = record_bytes(chunk, 2048);
    const auto staging = record_bytes(music.wave_staging, 8192);
    if (input.empty() || staging.empty())
        throw field::FieldFormatError("The wave chunk or its staging is not owned");
    Music calls(*this);
    field::consume_music_wave_chunk(music, chunk, std::span<const std::uint8_t, 2048>(input),
                                    std::span<std::uint8_t, 8192>(staging), calls);
}

// 800320e8 of the stream buffer (its ring header and payload) or of the
// wave staging, which the heap takes back.
void Program::release_music_buffer(std::uint32_t address, std::uint32_t call_site) {
    auto &read = resident.disc_read;
    resident::HeapBlock block;
    if (address == read.ring.address && !read.ring.bytes.empty()) {
        if (read.ring_payload.address != address + read.ring.bytes.size())
            throw field::FieldFormatError("The released stream buffer's payload is not owned");
        block = std::move(read.ring);
        block.bytes.insert(block.bytes.end(), read.ring_payload.bytes.begin(),
                           read.ring_payload.bytes.end());
        read.ring = {};
        read.ring_payload = {};
    } else {
        auto &blocks = resident.music_blocks;
        const auto found = std::ranges::find(blocks, address, &resident::HeapBlock::address);
        if (found == blocks.end())
            throw field::FieldFormatError("The released music buffer is not owned");
        block = std::move(*found);
        blocks.erase(found);
    }
    if (resident::heap_release(resident.heap, block, call_site) != 0)
        throw MissingDependency({"music_release", call_site, {}, {}}, "state:kept-music-buffer",
                                false, "A kept music buffer stays with its caller");
}

} // namespace xem::reconstruction
