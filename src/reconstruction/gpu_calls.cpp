// Resident libgpu calls (executable dc0b2dd7...) on the path the field frame
// takes: every call runs immediately through the command queue (8004668c)
// because the queue is empty and the GPU DMA channel idle. Commands that reach
// the GPU become GpuCommand records; the libgpu statics and the packets and
// rectangles the calls write in RAM are Program state. Hardware reads that
// enter RAM (alarm counters, GPUSTAT, the interrupt mask) come from
// FrameServices.
#include "xem/reconstruction/program.hpp"

#include <bit>
#include <vector>

namespace xem::reconstruction {
namespace {
std::int16_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
constexpr std::uint32_t function_clear = 0x80045e44; // _clr
constexpr std::uint32_t function_load = 0x800460a0;  // _dws
constexpr std::uint32_t function_send = 0x800465ec;  // _cwc
constexpr std::uint32_t interrupt_mask_register = 0x1f801074;
// Clamp a rectangle extent to [0, limit], or [0, limit - 1] when inclusive.
std::int16_t clamp_extent(std::int16_t value, std::int16_t limit, bool inclusive) {
    const auto top = static_cast<std::int16_t>(inclusive ? limit - 1 : limit);
    if (value < 0)
        return 0;
    return value > top ? top : value;
}
} // namespace

// libgpu set_alarm (80046efc): a deadline 240 vertical blanks ahead.
void Program::gpu_alarm(FrameServices &services) {
    auto &gpu = resident.gpu;
    gpu.alarm = take_service(services.vblank_counts, "VSync(-1) for a libgpu alarm") + 0xf0U;
    gpu.alarm_polls = 0;
}

void Program::gpu_check_rect(std::uint32_t rect) const {
    // checkRECT (8004463c) prints only at debug levels 1 and 2.
    static_cast<void>(rect);
    if (resident.gpu.debug != 0)
        throw MissingDependency({"gpu_check_rect", 0x8004463c, {}, {}},
                                "symbol:libgpu-debug-output", false,
                                "libgpu debug output is not recovered");
}

// libgpu _addque2 (8004668c) on its immediate path: the call runs at once with
// interrupts masked and is remembered as the last call.
void Program::gpu_call(FrameServices &services, std::uint32_t function, std::uint32_t parameter,
                       std::uint32_t argument, const std::function<void()> &run) {
    auto &gpu = resident.gpu;
    gpu_alarm(services);
    if (((gpu.queue_in + 1U) & 0x3fU) == gpu.queue_out)
        throw MissingDependency({"gpu_call", 0x800466c0, {}, {}}, "symbol:libgpu-queue-full", false,
                                "Waiting for a full libgpu queue is not recovered");
    gpu.saved_mask = take_service(services.interrupt_masks, "SetIntrMask(0) result");
    resident.hardware_writes.push_back({interrupt_mask_register, 0, 2});
    gpu.started = 1;
    if (gpu.queue_check != 0) {
        const bool empty = gpu.queue_in == gpu.queue_out;
        const bool busy =
            empty && (take_service(services.dma_busy, "libgpu DMA busy check") & 0x1000000U) != 0;
        if (!empty || busy || gpu.force_queue != 0)
            throw MissingDependency({"gpu_call", 0x800467d4, {}, {}}, "symbol:libgpu-queued-call",
                                    false, "Queued libgpu calls are not recovered");
    }
    run();
    gpu.last_call = {function, parameter, argument};
    resident.hardware_writes.push_back({interrupt_mask_register, gpu.saved_mask, 2});
}

// LoadImage (80044894) -> _dws (800460a0): clamp the rectangle in place and
// send the image through GP0 and DMA.
void Program::load_image(FrameServices &services, std::uint32_t rect, std::uint32_t source) {
    gpu_check_rect(rect);
    gpu_call(services, function_load, rect, source, [&] {
        auto &gpu = resident.gpu;
        gpu_alarm(services);
        const auto w = clamp_extent(s16(memory(rect + 4, 2)), gpu.vram_width, false);
        set_memory(rect + 4, static_cast<std::uint16_t>(w), 2);
        const auto h = clamp_extent(s16(memory(rect + 6, 2)), gpu.vram_height, false);
        set_memory(rect + 6, static_cast<std::uint16_t>(h), 2);
        if (w * h + 1 < 2)
            throw MissingDependency({"load_image", 0x80046164, {}, {}}, "symbol:libgpu-empty-image",
                                    false, "An empty LoadImage rectangle is not recovered");
        // Waiting for GPUSTAT bit 26 polls the alarm.
        gpu.alarm_polls = take_service(services.alarm_polls, "LoadImage alarm polls");
        gpu.commands.push_back({GpuCommand::Kind::load_image,
                                {s16(memory(rect, 2)), s16(memory(rect + 2, 2)), w, h},
                                source,
                                0});
    });
}

// ClearImage (80044764) -> _clr (80045e44): clamp the rectangle in place and
// send a fill packet built in libgpu's packet buffer (8005a238).
void Program::clear_image(FrameServices &services, std::uint32_t rect, std::uint32_t color) {
    gpu_check_rect(rect);
    gpu_call(services, function_clear, rect, color, [&] {
        auto &gpu = resident.gpu;
        const auto w = clamp_extent(s16(memory(rect + 4, 2)), gpu.vram_width, true);
        set_memory(rect + 4, static_cast<std::uint16_t>(w), 2);
        const auto h = clamp_extent(s16(memory(rect + 6, 2)), gpu.vram_height, true);
        set_memory(rect + 6, static_cast<std::uint16_t>(h), 2);
        const auto status = take_service(services.gpu_status, "GPUSTAT read by ClearImage");
        const auto mode = 0xe1000000U | (color >> 31U) << 10U | (status & 0x7ffU);
        std::vector<std::uint32_t> packet;
        if ((memory(rect, 2) & 0x3fU) == 0 && (static_cast<std::uint32_t>(w) & 0x3fU) == 0) {
            // Aligned: a VRAM fill.
            packet = {0x05ffffffU,  0xe6000000U,     mode, 0x02000000U | (color & 0xffffffU),
                      memory(rect), memory(rect + 4)};
        } else {
            // Unaligned: open the drawing area, draw a rectangle, then restore
            // the area and offset read back through _param (80046638): GP1
            // 10000003..5, then GPUREAD.
            const auto info = [&](std::uint32_t index) {
                gpu.commands.push_back({GpuCommand::Kind::control, {}, 0, 0x10000000U | index});
                return take_service(services.gpu_info, "GPUREAD for ClearImage") & 0xffffffU;
            };
            packet = {0x0805a25cU,
                      0xe3000000U,
                      0xe4ffffffU,
                      0xe5000000U,
                      0xe6000000U,
                      mode,
                      0x60000000U | (color & 0xffffffU),
                      memory(rect),
                      memory(rect + 4),
                      0x03ffffffU};
            for (std::uint32_t index = 3; index <= 5; ++index)
                packet.push_back(info(index) | (0xe0000000U + (index << 24U)));
        }
        for (std::size_t i = 0; i < packet.size(); ++i)
            for (std::size_t b = 0; b < 4; ++b)
                gpu.packet[i * 4 + b] = static_cast<std::uint8_t>(packet[i] >> (8U * b));
        gpu.commands.push_back({GpuCommand::Kind::clear_image,
                                {s16(memory(rect, 2)), s16(memory(rect + 2, 2)), w, h},
                                0x8005a238,
                                color});
    });
}

// DrawOTag (80044bd0) -> _cwc (800465ec): send the packet list by DMA.
void Program::draw_otag(FrameServices &services, std::uint32_t table) {
    if (resident.gpu.debug >= 2)
        throw MissingDependency({"draw_otag", 0x80044bf0, {}, {}}, "symbol:libgpu-debug-output",
                                false, "libgpu debug output is not recovered");
    gpu_call(services, function_send, table, 0, [&] {
        resident.gpu.commands.push_back({GpuCommand::Kind::draw_packets, {}, table, 0});
    });
}

// PutDrawEnv (80044c44): build the environment's packet with SetDrawEnv2
// (8004574c), send it, and keep a copy of the environment (800568e0).
void Program::put_draw_env(FrameServices &services, std::uint32_t environment) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        throw MissingDependency({"put_draw_env", 0x80044c74, {}, {}}, "symbol:libgpu-debug-output",
                                false, "libgpu debug output is not recovered");
    const auto type = resident.gpu_type;
    const bool low = static_cast<std::uint8_t>(type - 1U) < 2;
    const auto coordinate = [&](std::int32_t value, std::int16_t limit) {
        return clamp_extent(static_cast<std::int16_t>(value), limit, true);
    };
    // get_cs/get_ce (80045a34/80045b00): a clamped drawing-area corner.
    const auto corner = [&](std::uint32_t command, std::int32_t x, std::int32_t y) {
        const auto cx = static_cast<std::uint32_t>(coordinate(x, gpu.vram_width));
        const auto cy = static_cast<std::uint32_t>(coordinate(y, gpu.vram_height));
        return command |
               (low ? (cy & 0xfffU) << 12U | (cx & 0xfffU) : (cy & 0x3ffU) << 10U | (cx & 0x3ffU));
    };
    const auto e = environment;
    const auto clip_x = s16(memory(e, 2));
    const auto clip_y = s16(memory(e + 2, 2));
    const auto clip_w = s16(memory(e + 4, 2));
    const auto clip_h = s16(memory(e + 6, 2));
    const auto packet = e + 0x1c;
    set_memory(packet + 4, corner(0xe3000000U, clip_x, clip_y));
    set_memory(packet + 8,
               corner(0xe4000000U, s16(u32(clip_x + clip_w - 1)), s16(u32(clip_y + clip_h - 1))));
    // get_ofs (80045bcc).
    const auto ox = memory(e + 8, 2);
    const auto oy = memory(e + 0xa, 2);
    set_memory(packet + 0xc, 0xe5000000U | (low ? (oy & 0xfffU) << 12U | (ox & 0xfffU)
                                                : (oy & 0x7ffU) << 11U | (ox & 0x7ffU)));
    set_memory(packet + 0x10, gpu::draw_mode(type, memory(e + 0x17, 1) != 0,
                                             memory(e + 0x16, 1) != 0, memory(e + 0x14, 2)));
    const std::array<std::int16_t, 4> window{s16(memory(e + 0xc, 2)), s16(memory(e + 0xe, 2)),
                                             s16(memory(e + 0x10, 2)), s16(memory(e + 0x12, 2))};
    set_memory(packet + 0x14, gpu::texture_window(&window));
    set_memory(packet + 0x18, 0xe6000000U);
    if (memory(e + 0x18, 1) != 0)
        throw MissingDependency({"put_draw_env", 0x80045800, {}, {}},
                                "symbol:libgpu-environment-background", false,
                                "Drawing environments with a background fill are not recovered");
    set_memory(packet + 3, 6, 1);
    set_memory(packet, memory(packet) | 0xffffffU);
    gpu_call(services, function_send, packet, 0,
             [&] { gpu.commands.push_back({GpuCommand::Kind::draw_packets, {}, packet, 0}); });
    for (std::uint32_t i = 0; i < gpu.draw_environment.size(); ++i)
        gpu.draw_environment[i] = static_cast<std::uint8_t>(memory(e + i, 1));
}

// PutDispEnv (80044e9c): display start, then display ranges and mode when
// they changed since the copy kept at 8005693c.
void Program::put_disp_env(FrameServices &services, std::uint32_t environment) {
    static_cast<void>(services);
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        throw MissingDependency({"put_disp_env", 0x80044ecc, {}, {}}, "symbol:libgpu-debug-output",
                                false, "libgpu debug output is not recovered");
    if (static_cast<std::uint8_t>(resident.gpu_type - 1U) < 2)
        throw MissingDependency({"put_disp_env", 0x80044f04, {}, {}}, "symbol:libgpu-display-type",
                                false,
                                "Display commands for libgpu types 1 and 2 are not recovered");
    const auto e = environment;
    // _ctl (80046560): a GP1 command, remembered per command number.
    const auto control = [&](std::uint32_t command) {
        gpu.control[command >> 24U] = static_cast<std::uint8_t>(command);
        gpu.commands.push_back({GpuCommand::Kind::control, {}, 0, command});
    };
    control(0x05000000U | (memory(e + 2, 2) & 0x3ffU) << 10U | (memory(e, 2) & 0x3ffU));
    const auto previous = [&](std::uint32_t offset) {
        return static_cast<std::uint32_t>(gpu.display_environment[offset]) |
               static_cast<std::uint32_t>(gpu.display_environment[offset + 1]) << 8U;
    };
    bool screen_same = true;
    for (std::uint32_t i = 8; i < 0x10; i += 2)
        screen_same = screen_same && previous(i) == memory(e + i, 2);
    if (!screen_same)
        throw MissingDependency({"put_disp_env", 0x80044fd8, {}, {}}, "symbol:libgpu-display-range",
                                false, "Changing display ranges are not recovered");
    bool mode_same = previous(0x10) == memory(e + 0x10, 2) && previous(0x12) == memory(e + 0x12, 2);
    for (std::uint32_t i = 0; i < 8; i += 2)
        mode_same = mode_same && previous(i) == memory(e + i, 2);
    if (!mode_same) {
        // GetVideoMode (8004c308) is stored in the environment's +12.
        set_memory(e + 0x12, resident.video_mode, 1);
        std::uint32_t mode = 0x08000000U;
        if ((resident.video_mode & 0xffU) == 1)
            mode |= 8U;
        if (memory(e + 0x11, 1) != 0)
            mode |= 0x10U;
        if (memory(e + 0x10, 1) != 0)
            mode |= 0x20U;
        if (gpu.interlace != 0)
            mode |= 0x80U;
        const auto width = s16(memory(e + 4, 2));
        if (width >= 0x119)
            mode |= width < 0x161 ? 1U : width < 0x191 ? 0x40U : width < 0x231 ? 2U : 3U;
        const auto height = s16(memory(e + 6, 2));
        if (height >= (memory(e + 0x12, 1) != 0 ? 0x121 : 0x101))
            mode |= 0x24U;
        control(mode);
    }
    for (std::uint32_t i = 0; i < gpu.display_environment.size(); ++i)
        gpu.display_environment[i] = static_cast<std::uint8_t>(memory(e + i, 1));
}

// DrawSync(0) (800445d0) -> _sync (80046db4): wait for an empty queue and an
// idle GPU; the waits poll the alarm.
void Program::draw_sync(FrameServices &services) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        throw MissingDependency({"draw_sync", 0x800445f0, {}, {}}, "symbol:libgpu-debug-output",
                                false, "libgpu debug output is not recovered");
    gpu_alarm(services);
    if (gpu.queue_in != gpu.queue_out)
        throw MissingDependency({"draw_sync", 0x80046dd4, {}, {}}, "symbol:libgpu-queued-call",
                                false, "Draining queued libgpu calls is not recovered");
    gpu.alarm_polls = take_service(services.alarm_polls, "DrawSync alarm polls");
}

} // namespace xem::reconstruction
