// The movie library (movie.hpp) loaded at 801d3000: the game's streaming
// player and Sony's libpress MDEC interface. The player decodes one frame
// ahead: a poll (801d3f7c) starts the MDEC on the frame decoded last,
// decodes the next frame's bitstream (MdecCodec) and frees its ring sectors;
// the MDEC's output DMA completion (801d30c4, interrupt context) loads each
// decoded slice into VRAM and reports a finished frame to the caller's
// callback. Statics live in the library image, a heap block of the loader,
// and are addressed as the original addresses them.
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/program.hpp"

#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s8(std::uint32_t value) {
    return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
}
[[noreturn]] void unrecovered(std::string_view operation, std::uint32_t address, const char *id,
                              const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}
// The host-file stream (8004fe48 nonzero) uses the resident disc ring
// instead of the library's; it is a development configuration.
[[noreturn]] void host_file_stream(std::uint32_t address) {
    unrecovered("movie_host_stream", address, "symbol:movie-host-stream",
                "The movie library's host-file stream (8002a260, 80028f30, 800294b4) is not "
                "reconstructed");
}
} // namespace

using namespace movie;

// 801d3538: open the library for a `width` x `height` movie in 16-bit VRAM
// units. `scale` sizes the two run-level buffers (width * height * scale /
// 128 bytes), `slice` is the macroblock column width in pixels, `sectors`
// the stream ring's length, `limit` the decode-call limit and `mode` bit 0
// selects 24-bit output. Returns 0, or -1 when no ring was allocated.
std::int32_t Program::movie_open(std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                                 std::uint32_t slice, std::uint32_t sectors, std::uint32_t limit,
                                 std::uint32_t mode) {
    const bool host = resident.disc_stream.host_file_table != 0; // 8002c3d8
    set_memory(host_stream, host ? 1 : 0, 1);
    set_memory(player_state, 0, 1);
    if (memory(split_display) != 0)
        slice = width;
    mdec_reset(0);
    const auto columns = width & 0xffffU;
    const auto rows = height & 0xffffU;
    const auto product = s32(columns * rows * ((scale & 0xffffU) << 1U));
    const auto buffer_bytes =
        static_cast<std::uint32_t>((product < 0 ? product + 255 : product) >> 8);
    set_memory(vlc_buffers, load_block(buffer_bytes, 0, 0x801d360c));
    set_memory(vlc_buffers + 4, load_block(buffer_bytes, 0, 0x801d3620));
    set_memory(vlc_limit, limit & 0xffffU);
    set_memory(color_mode, mode & 3U);
    auto units = width;
    if ((memory(color_mode) & 1U) != 0) { // Three bytes per pixel.
        slice = ((slice & 0xffffU) * 3U) >> 1U;
        units = (columns * 3U) >> 1U;
    }
    set_memory(image_width, units, 2);
    set_memory(image_height, height, 2);
    const auto slice_bytes = ((slice & 0xffffU) * rows) << 1U;
    set_memory(slice_buffers, load_block(slice_bytes, 0, 0x801d3680));
    set_memory(slice_buffers + 4, load_block(slice_bytes, 0, 0x801d3694));
    for (std::uint32_t k = 0; k < 2; ++k) {
        const auto display = display_buffers + 8 * k;
        set_memory(display, 0, 2);
        set_memory(display + 2, 0, 2);
        set_memory(display + 4, units, 2);
        set_memory(display + 6, height, 2);
        const auto rect = slice_rects + 8 * k;
        set_memory(rect, 0, 2);
        set_memory(rect + 2, 0, 2);
        set_memory(rect + 4, slice, 2);
        set_memory(rect + 6, height, 2);
    }
    if (host)
        host_file_stream(0x801d3734);
    const auto count = sectors & 0xffffU;
    const auto ring = load_block(count << 11U, 0, 0x801d3754);
    set_memory(ring_buffer, ring);
    stream_set_ring(ring, count); // 801d583c
    if (memory(ring_buffer) == 0)
        return -1;
    set_memory(player_state, 1, 1);
    return 0;
}

// 801d37cc: start streaming.
void Program::movie_start(const MovieStart &start) {
    if (memory(player_state, 1) == 0)
        return;
    current_directory(saved_directory, saved_directory + 4); // 800284b4
    mdec_out_callback(0x801d30c4);                           // 801d47d8
    set_memory(player_state, start.hold != 0 ? 2 : 1, 1);
    set_memory(restarted, 0, 1);
    set_memory(row_limit, start.rows, 2);
    set_memory(movie_file, start.file, 2);
    set_memory(start_sector, start.sector);
    if (memory(host_stream, 1) != 0)
        host_file_stream(0x801d38a4);
    if ((start.select & 1U) != 0) {
        // Real-time CD-XA audio of the movie's file 1, channel `channel`:
        // the drive plays it into the SPU; its sectors never reach RAM.
        set_memory(xa_channel, start.channel, 2);
        set_memory(cd_mode, 0x148);
        const std::array<std::uint8_t, 4> filter{
            1, static_cast<std::uint8_t>(memory(xa_channel, 1)), 0, 0};
        do
            deliver_arrivals(0x801d3930);
        while (cd_control_blocking(0x0d, &filter, nullptr) == 0); // 80041248 Setfilter
    } else {
        set_memory(cd_mode, 0x100);
    }
    stream_set_stream(memory(color_mode) & 1U, start.first_frame & 0xffffU, 0xffffffffU, 0, 0);
    if ((start.select & 2U) != 0)
        set_memory(cd_mode, memory(cd_mode) & ~0x40U);
    set_memory(frame_callback, start.callback);
    const auto three_halves = [](std::uint32_t value) {
        return static_cast<std::uint32_t>(s32(value + (value << 1U)) >> 1);
    };
    std::uint32_t width = 0x10;
    if ((memory(color_mode) & 1U) != 0) {
        set_memory(display_buffers, three_halves(start.area[0]), 2);
        set_memory(display_buffers + 8, three_halves(start.area[2]), 2);
        set_memory(display_buffers + 2, start.area[1], 2);
        width = 0x18;
    } else {
        set_memory(display_buffers, start.area[0], 2);
        set_memory(display_buffers + 2, start.area[1], 2);
        set_memory(display_buffers + 8, start.area[2], 2);
    }
    set_memory(display_buffers + 10, start.area[3], 2);
    set_memory(slice_width, width, 2);
    set_memory(mdec_idle, 1, 1);
    set_memory(frame_waiting, 1, 1);
    set_memory(shown_frame, 0xffffffffU);
    set_memory(load_restart, 1);
    set_memory(load_enabled, 1);
    set_memory(first_frame, start.first_frame & 0xffffU);
    set_memory(vlc_buffer_index, 0);
    set_memory(slice_buffer_index, 0);
    set_memory(decode_display, 0);
    set_memory(load_display, 0);
    set_memory(frame_width, 0, 2);
    set_memory(frame_height, 0, 2);
    set_memory(stall_count, 0);
    set_memory(vlc_pending, 0);
    set_memory(skipped_frames, 0);
    set_memory(end_frame, start.last_frame & 0xffffU);
    deliver_arrivals(0x801d3ac4);
    movie_restart(start.file, start.sector, memory(xa_channel, 2), memory(cd_mode), 0);
}

// 801d3f7c: one step of playback. Fades the CD audio in once past the first
// frame and out three frames before the end, ends or loops the stream at the
// end, decodes, and restarts a stream that delivered nothing for 2161 polls.
void Program::movie_poll(MdecCodec &codec) {
    if (s8(memory(player_state, 1)) <= 0)
        return;
    const auto shown = s32(memory(shown_frame));
    if (s32(memory(first_frame)) < shown && memory(fade_in_pending) != 0) {
        set_memory(fade_in_pending, 0);
        deliver_arrivals(0x801d3fd0);
        resident::set_cd_volume(resident.sound, 0x7fff, 0x28);
    }
    if (s32(memory(shown_frame)) >= s32(memory(end_frame)) - 3 && memory(fade_out_pending) != 0) {
        set_memory(fade_out_pending, 0);
        deliver_arrivals(0x801d4014);
        resident::set_cd_volume(resident.sound, 0, 0x28);
    }
    if (s32(memory(shown_frame)) >= s32(memory(end_frame))) {
        if (s8(memory(player_state, 1)) == 1) {
            deliver_arrivals(0x801d4050);
            movie_stop(); // 801d4318
        } else {
            set_memory(restarted, 0, 1);
            set_memory(shown_frame, 0xffffffffU);
            deliver_arrivals(0x801d4090);
            movie_restart(memory(movie_file, 2), memory(start_sector), memory(xa_channel, 2),
                          memory(cd_mode), 0);
        }
    }
    if (memory(mdec_idle, 1) != 0 || memory(frame_waiting, 1) != 0 || memory(vlc_pending) != 0) {
        deliver_arrivals(0x801d40d4);
        movie_decode(codec); // 801d3d54
    }
    if (s32(memory(stall_count)) < 0x871)
        return;
    // The ring delivered no frame for 2161 polls: seek again from the last
    // sector the stream saw, unless that lies past the end.
    set_memory(stall_count, 0);
    if (resident.cd.stream_header_mode != 0)
        unrecovered("movie_poll", 0x801d40fc, "state:movie-stall-location",
                    "A stalled stream without sector headers seeks from an uninitialized location");
    std::array<std::uint8_t, 4> location{};
    const auto frame = stream_position(location); // 801d5a94
    auto &read = resident.disc_read;
    read.h_5a4b8 = static_cast<std::uint16_t>(frame);
    ++read.w_5a4dc;
    read.w_5a4a4[1] = cd_sector(location); // 80041534
    read.w_5a4b4 = memory(start_sector);
    const bool resume = !(s32(memory(end_frame)) < s32(frame)) && s32(frame) > 0;
    set_memory(shown_frame, 0xffffffffU);
    movie_restart(memory(movie_file, 2), memory(start_sector), memory(xa_channel, 2),
                  memory(cd_mode), resume ? &location : nullptr);
}

// 801d3d54: start the MDEC on the frame decoded last (when it is idle), then
// decode the next frame's bitstream, or continue a partial decode.
void Program::movie_decode(MdecCodec &codec) {
    std::uint32_t bitstream = 0;
    std::uint32_t output = 0;
    if (memory(vlc_pending) == 0) {
        if (memory(frame_waiting, 1) == 0) {
            const auto k = memory(decode_display);
            // The slice rectangle starts at the display buffer's corner.
            set_memory(slice_rects + 8 * k, memory(display_buffers + 8 * k, 2), 2);
            set_memory(slice_rects + 8 * k + 2, memory(display_buffers + 8 * k + 2, 2), 2);
            set_memory(loaded_frame, memory(ring_frame));
            deliver_arrivals(0x801d3dfc);
            mdec_in(memory(vlc_buffers + 4 * memory(vlc_buffer_index)), memory(color_mode));
            const auto k2 = memory(decode_display);
            const auto size =
                s16(memory(slice_rects + 8 * k2 + 4, 2)) * s16(memory(slice_rects + 8 * k2 + 6, 2));
            deliver_arrivals(0x801d3e54);
            mdec_out(memory(slice_buffers + 4 * memory(slice_buffer_index)),
                     static_cast<std::uint32_t>((size + (size < 0 ? 1 : 0)) >> 1));
            set_memory(mdec_idle, 0, 1);
            set_memory(decode_display, 1U - memory(decode_display));
            set_memory(vlc_buffer_index, 1U - memory(vlc_buffer_index));
        }
        deliver_arrivals(0x801d3e98);
        const auto data = movie_next_frame(memory(end_frame), decoded_bitstream); // 801d3b00
        set_memory(frame_bitstream, data);
        if (data == 0) {
            set_memory(frame_waiting, 1, 1);
            return;
        }
        set_memory(frame_waiting, 0, 1);
        static_cast<void>(vlc_size(memory(vlc_limit))); // 801d4c98
        bitstream = memory(frame_bitstream);
        output = memory(vlc_buffers + 4 * memory(vlc_buffer_index));
    }
    deliver_arrivals(0x801d3f08);
    set_memory(vlc_pending, decode_vlc(codec, bitstream, output) ? 1U : 0U); // 801d4cc8
    if (memory(vlc_pending) != 0)
        return;
    if (memory(host_stream, 1) != 0)
        host_file_stream(0x801d3f48);
    deliver_arrivals(0x801d3f60);
    static_cast<void>(stream_free(memory(frame_bitstream))); // 801d5b7c
}

// 801d3b00: the next frame's bitstream from the ring, or 0 when none is
// complete; its first sector's header goes to `header`. A frame whose size
// differs from the last one moves the display buffers' far corners and the
// rows each slice loads.
std::uint32_t Program::movie_next_frame(std::uint32_t, std::uint32_t header) {
    if (memory(host_stream, 1) != 0)
        host_file_stream(0x801d3b1c);
    std::uint32_t data = 0;
    std::uint32_t sector = 0;
    if (stream_next(data, sector) != 0) { // 801d5c70
        set_memory(stall_count, memory(stall_count) + 1);
        return 0;
    }
    const auto previous = memory(ring_frame);
    const auto frame = memory(sector + 8);
    set_memory(stall_count, 0);
    set_memory(previous_frame, previous);
    set_memory(ring_frame, frame);
    if (s32(previous + 1) < s32(frame))
        set_memory(skipped_frames, memory(skipped_frames) + 1);
    const auto width = memory(sector + 0x10, 2);
    const auto height = memory(sector + 0x12, 2);
    if (memory(frame_width, 2) != width || memory(frame_height, 2) != height) {
        set_memory(frame_width, width, 2);
        set_memory(frame_height, height, 2);
        auto columns = memory(frame_width, 2);
        if ((memory(color_mode) & 1U) != 0)
            columns = static_cast<std::uint32_t>(s32(columns + (columns << 1U)) >> 1);
        set_memory(display_buffers + 8 + 4, memory(display_buffers + 8, 2) + columns, 2);
        set_memory(display_buffers + 4, memory(display_buffers, 2) + columns, 2);
        const auto rows = memory(frame_height, 2);
        set_memory(display_buffers + 6, memory(display_buffers + 2, 2) + rows, 2);
        set_memory(display_buffers + 8 + 6, memory(display_buffers + 8 + 2, 2) + rows, 2);
        const auto most = memory(image_height, 2);
        const auto loaded = most < rows ? most : rows;
        set_memory(slice_rects + 6, loaded, 2);
        set_memory(slice_rects + 8 + 6, loaded, 2);
    }
    set_memory(header, sector);
    return data;
}

// 801d41ac: seek the stream to `sector` of `file` (or to `location` when
// given) and read from there in `mode`. The current directory is kept.
void Program::movie_restart(std::uint32_t file, std::uint32_t sector, std::uint32_t,
                            std::uint32_t mode, const std::array<std::uint8_t, 4> *location) {
    deliver_arrivals(0x801d41e0);
    resident::set_cd_volume(resident.sound, 0, 0);
    cancel_disc_read(0); // 8002a498
    deliver_arrivals(0x801d41f0);
    disc_wait(0); // 80028a60
    const auto kept = current_directory();
    static_cast<void>(select_directory(memory(saved_directory), memory(saved_directory + 4)));
    set_memory(fade_in_pending, 1);
    set_memory(fade_out_pending, 1);
    if (memory(host_stream, 1) != 0)
        host_file_stream(0x801d4238);
    mode |= 0x80U; // Double speed.
    std::array<std::uint8_t, 4> start{};
    if (location == nullptr) {
        start = cd_position(file_sector(file) + sector); // 800289d0, 80041430
        location = &start;
    }
    do
        deliver_arrivals(0x801d42c4);
    while (cd_control_blocking(0x02, location, nullptr) == 0); // Setloc
    do
        deliver_arrivals(0x801d42d4);
    while (stream_read(mode) == 0); // 801d586c
    static_cast<void>(select_directory(kept[0], kept[1]));
}

// 801d4318: stop the stream: silence the CD input, stop the MDEC, unset the
// ring's callbacks, pause the drive and restore the resident read mode.
void Program::movie_stop() {
    deliver_arrivals(0x801d4324);
    resident::set_cd_volume(resident.sound, 0, 0);
    cancel_disc_read(0);  // 8002a498
    mdec_out_callback(0); // 801d47d8
    mdec_reset(0);        // 801d4534
    set_memory(player_state, 0xff, 1);
    if (memory(host_stream, 1) != 0)
        host_file_stream(0x801d4360);
    deliver_arrivals(0x801d4370);
    stream_unset_ring(); // 801d5980
    do
        deliver_arrivals(0x801d4380);
    while (cd_control_blocking(0x09, nullptr, nullptr) == 0); // Pause
    deliver_arrivals(0x801d4390);
    disc_set_mode(0xa0); // 8002a428
    deliver_arrivals(0x801d4398);
    disc_wait(0); // 80028a60
}

// 801d43b0: stop, then release the buffers and the ring.
void Program::movie_close() {
    deliver_arrivals(0x801d43b8);
    movie_stop();
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 5> blocks{{
        {vlc_buffers, 0x801d43cc},
        {vlc_buffers + 4, 0x801d43dc},
        {slice_buffers, 0x801d43ec},
        {slice_buffers + 4, 0x801d43fc},
        {ring_buffer, 0x801d440c},
    }};
    for (const auto &[pointer, site] : blocks)
        static_cast<void>(release_owned_block(memory(pointer), site));
    for (const auto &[pointer, site] : blocks)
        set_memory(pointer, 0);
}

// 801d30c4, the MDEC output DMA's completion callback (interrupt context):
// load the slice just decoded into VRAM, then decode the next slice or, at
// the frame's last column, report the frame to the frame callback.
void Program::movie_slice_decoded() {
    if (memory(host_stream, 1) == 0 && (memory(color_mode) & 1U) != 0 &&
        memory(stream_deferred) != 0) {
        stream_interrupt(); // 801d5d54, deferred while the MDEC output DMA ran
        set_memory(stream_deferred, 0);
    }
    const auto k = memory(load_display);
    const auto rect = slice_rects + 8 * k;
    const auto limit = s16(memory(row_limit, 2));
    const auto rows = memory(rect + 6, 2);
    if (limit >= 0 && limit < s16(rows))
        set_memory(rect + 6, static_cast<std::uint32_t>(limit), 2);
    if (memory(split_display) != 0)
        unrecovered("movie_slice_split", 0x801d3188, "symbol:movie-split-display",
                    "Loading slices into a split display (801d68b4 set) is not reconstructed");
    // The MDEC's output for this slice arrived by DMA1 before its completion
    // interrupt: a hardware result the platform supplies.
    const auto size =
        static_cast<std::size_t>(s16(memory(rect + 4, 2)) * s16(memory(rect + 6, 2)) * 2);
    auto &outputs = resident.mdec_output;
    if (outputs.empty())
        throw PlatformInputError("No MDEC output is supplied for a decoded slice");
    // A recording may cover more than the slice (its buffer's full size).
    if (outputs.front().size() < size)
        throw PlatformInputError("The supplied MDEC output is shorter than the slice");
    dma_store(memory(slice_buffers + 4 * memory(slice_buffer_index)),
              std::span<const std::uint8_t>(outputs.front()).first(size));
    outputs.pop_front();
    // The completed transfer left DMA1 idle (CHCR 1f801098 bit 24).
    resident.io[0x98 + 3] &= 0xfeU;
    if (memory(load_enabled) != 0) {
        std::array<std::int16_t, 4> area{static_cast<std::int16_t>(memory(rect, 2)),
                                         static_cast<std::int16_t>(memory(rect + 2, 2)),
                                         static_cast<std::int16_t>(memory(rect + 4, 2)),
                                         static_cast<std::int16_t>(memory(rect + 6, 2))};
        static_cast<void>(load_image(
            area, rect, memory(slice_buffers + 4 * memory(slice_buffer_index)), nullptr));
    }
    set_memory(rect, memory(rect, 2) + memory(rect + 4, 2), 2);
    set_memory(rect + 6, rows, 2);
    const auto next = 1U - memory(slice_buffer_index);
    set_memory(slice_buffer_index, next);
    if (s16(memory(rect, 2)) < s16(memory(display_buffers + 8 * k + 4, 2))) {
        const auto words = s16(memory(rect + 4, 2)) * s16(memory(rect + 6, 2));
        mdec_out(memory(slice_buffers + 4 * next),
                 static_cast<std::uint32_t>((words + (words < 0 ? 1 : 0)) >> 1));
        return;
    }
    if (const auto callback = memory(frame_callback); callback != 0) {
        const auto display = display_buffers + 8 * k;
        auto x = memory(display, 2);
        if ((memory(color_mode) & 1U) != 0) // Back to pixels: two thirds.
            x = static_cast<std::uint32_t>((static_cast<std::int64_t>(s16(x) * 2) * 0x55555556LL) >>
                                           32) &
                0xffffU;
        movie_frame_ready(callback, memory(loaded_frame, 2), x, memory(display + 2, 2));
    }
    set_memory(mdec_idle, 1, 1);
    set_memory(shown_frame, memory(loaded_frame));
    set_memory(load_enabled, memory(load_restart));
    set_memory(load_display, 1U - memory(load_display));
}

// --- libpress ---------------------------------------------------------------

namespace {
constexpr std::uint32_t mdec_command = 0x1f801820;
constexpr std::uint32_t mdec_control = 0x1f801824;
constexpr std::uint32_t dma_control = 0x1f8010f0;
constexpr std::uint32_t dma0_address = 0x1f801080;
constexpr std::uint32_t dma0_block = 0x1f801084;
constexpr std::uint32_t dma0_channel = 0x1f801088;
constexpr std::uint32_t dma1_address = 0x1f801090;
constexpr std::uint32_t dma1_block = 0x1f801094;
constexpr std::uint32_t dma1_channel = 0x1f801098;
} // namespace

// 801d4534 DecDCTReset(mode): mode 0 also resets the interrupt callbacks
// (8004b740, the DMA service at +0c, which returns at once once the
// dispatcher is initialized).
void Program::mdec_reset(std::uint32_t mode) {
    if (mode == 0 && resident.interrupts.initialized == 0)
        unrecovered("mdec_reset", 0x8004b8f4, "symbol:reset-callback-8004b8d8",
                    "Initializing the interrupt callbacks (8004b8d8) is not reconstructed");
    mdec_hardware_reset(mode); // 801d47fc
}

// 801d47fc MDEC_reset: reset the MDEC, stop both DMA channels, and with mode
// 0 load the quantization and scale tables (801d76e0, 801d7764).
void Program::mdec_hardware_reset(std::uint32_t mode) {
    if (mode > 1)
        unrecovered("mdec_hardware_reset", 0x801d48d8, "symbol:printf-80019964",
                    "The bad-option message of MDEC_reset is not reconstructed");
    verify_mdec_registers();
    io_write(mdec_control, 0x80000000U, 4);
    io_write(dma0_channel, 0, 4);
    if ((io_latch(dma1_channel, 4) & 0x01000000U) != 0) {
        // An output transfer is still running: the reset stops it after
        // the MDEC wrote part of the slice, bytes the platform supplies.
        const auto buffer = 0x80000000U | io_latch(dma1_address, 4);
        const auto bytes = (io_latch(dma1_block, 4) >> 16U) * 0x80U;
        const auto found = resident.mdec_aborted.find(buffer);
        if (found == resident.mdec_aborted.end() || found->second.size() < bytes)
            throw PlatformInputError("No MDEC output is supplied for the stopped transfer");
        dma_store(buffer, std::span<const std::uint8_t>(found->second).first(bytes));
        resident.mdec_aborted.erase(found);
    }
    io_write(dma1_channel, 0, 4);
    if (mode == 1)
        static_cast<void>(io_latch(dma1_channel, 4)); // A read with no effect.
    io_write(mdec_control, 0x60000000U, 4);
    if (mode == 0) {
        mdec_in_words(0x801d76e0, 0x20);
        mdec_in_words(0x801d7764, 0x20);
    }
}

// The register pointers of the image must be the ones the library was
// linked with; the reconstruction writes those registers directly.
void Program::verify_mdec_registers() {
    constexpr std::array<std::uint32_t, 15> expected{
        dma0_address, dma0_block, dma0_channel, dma1_address, dma1_block,
        dma1_channel, 0x1f8010a0, 0x1f8010a4,   0x1f8010a8,   0x1f8010b0,
        0x1f8010b4,   0x1f8010b8, mdec_command, mdec_control, dma_control};
    for (std::uint32_t i = 0; i < expected.size(); ++i)
        if (memory(mdec_registers + 4 * i) != expected[i])
            throw field::FieldFormatError("The movie library's MDEC register table differs");
}

// 801d46a0 DecDCTin(buffer, mode): set the run-level stream's output depth
// (bit 27: 15-bit unless mode bit 0) and sign (bit 25, mode bit 1) in its
// header word, then send it (MDEC_in, its low halfword counting words).
void Program::mdec_in(std::uint32_t buffer, std::uint32_t mode) {
    auto header = memory(buffer);
    header = (mode & 1U) != 0 ? header & ~0x08000000U : header | 0x08000000U;
    set_memory(buffer, header);
    header = (mode & 2U) != 0 ? header | 0x02000000U : header & ~0x02000000U;
    set_memory(buffer, header);
    mdec_in_words(buffer, memory(buffer, 2));
}

// 801d48f8 MDEC_in: after the input is idle, the header word goes to the
// command register and DMA0 sends the rest in blocks of 32 words.
void Program::mdec_in_words(std::uint32_t buffer, std::uint32_t words) {
    static_cast<void>(mdec_in_sync());
    io_write(dma_control, io_latch(dma_control, 4) | 0x88U, 4);
    io_write(dma0_address, buffer + 4, 4);
    io_write(dma0_block, ((words >> 5U) << 16U) | 0x20U, 4);
    io_write(mdec_command, memory(buffer), 4);
    io_write(dma0_channel, 0x01000201U, 4);
}

// 801d498c MDEC_out (through DecDCTout 801d471c): after the output DMA is
// idle, DMA1 receives `words` words of decoded pixels into `buffer`.
void Program::mdec_out(std::uint32_t buffer, std::uint32_t words) {
    static_cast<void>(mdec_out_sync());
    io_write(dma_control, io_latch(dma_control, 4) | 0x88U, 4);
    io_write(dma1_channel, 0, 4);
    io_write(dma1_address, buffer, 4);
    io_write(dma1_block, ((words >> 5U) << 16U) | 0x20U, 4);
    io_write(dma1_channel, 0x01000200U, 4);
}

// 801d4a1c MDEC_in_sync: wait while the MDEC reports its input busy (status
// bit 29). The polls are platform reads; 0x100000 of them time out.
std::int32_t Program::mdec_in_sync() {
    static_cast<void>(deliver_leading_arrivals());
    if ((platform_read(resident.platform, 0x801d4a34, 4) & 0x20000000U) == 0)
        return 0;
    for (std::uint32_t polls = 0; polls < 0x100000; ++polls) {
        static_cast<void>(deliver_leading_arrivals());
        if ((platform_read(resident.platform, 0x801d4a90, 4) & 0x20000000U) == 0)
            return 0;
    }
    unrecovered("mdec_in_sync", 0x801d4a74, "symbol:mdec-timeout-801d4b64",
                "The MDEC input timeout report (801d4b64) is not reconstructed");
}

// 801d4ab4 MDEC_out_sync: wait while DMA1 is busy (control bit 24).
std::int32_t Program::mdec_out_sync() {
    static_cast<void>(deliver_leading_arrivals());
    if ((platform_read(resident.platform, 0x801d4acc, 4) & 0x01000000U) == 0)
        return 0;
    for (std::uint32_t polls = 0; polls < 0x100000; ++polls) {
        static_cast<void>(deliver_leading_arrivals());
        if ((platform_read(resident.platform, 0x801d4b28, 4) & 0x01000000U) == 0)
            return 0;
    }
    unrecovered("mdec_out_sync", 0x801d4b0c, "symbol:mdec-timeout-801d4b64",
                "The MDEC output timeout report (801d4b64) is not reconstructed");
}

// 801d47d8 DecDCToutCallback: DMACallback(1, function) (8004b7a0).
void Program::mdec_out_callback(std::uint32_t function) { set_dma_callback(1, function); }

// 801d4c98 DecDCTvlcSize(halfwords): the decode-call limit, or unbounded
// (ffffff) for 1 or less; returns the previous one.
std::uint32_t Program::vlc_size(std::uint32_t halfwords) {
    const auto previous = memory(vlc_call_limit);
    set_memory(vlc_call_limit, s32(halfwords - 1) > 0 ? halfwords << 1U : 0x00ffffffU);
    return previous;
}

// 801d4cc8 DecDCTvlc2: the codec service with the library's call limit and
// its context bytes.
bool Program::decode_vlc(MdecCodec &codec, std::uint32_t bitstream, std::uint32_t output) {
    auto context = owned_span(vlc_context);
    if (context.size() < vlc_context_bytes)
        throw field::FieldFormatError("The variable-length decode context is not owned");
    const MdecCodec::Memory ram = [this](std::uint32_t address, std::uint32_t size) {
        auto bytes = owned_span(address);
        if (bytes.size() < size)
            throw field::FieldFormatError("The variable-length decode reaches unowned memory");
        return bytes.first(size);
    };
    return codec.decode_vlc(ram, bitstream, output, memory(vlc_call_limit),
                            context.first(vlc_context_bytes));
}

} // namespace xem::reconstruction
