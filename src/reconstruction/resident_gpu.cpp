// Resident libgpu calls reached from interrupt context, where no frame
// services exist: their hardware polls are recorded platform reads. Also the
// environment defaults (SetDefDrawEnv, SetDefDispEnv) and SetDispMask.
#include "xem/reconstruction/program.hpp"

#include <bit>

namespace xem::reconstruction {

// DrawSync(0) (800445d0 -> _sync 80046db4) inside an interrupt handler: the
// alarm, the queue drained, then polls of the GPU DMA (channel 2 busy,
// 80046e2c) and GPUSTAT (ready for commands, 80046e4c), each unfinished
// poll counting against the alarm (80046f30).
void Program::draw_sync_polled() {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        throw MissingDependency({"draw_sync", 0x800445f0, {}, {}}, "symbol:printf-80019964", false,
                                "libgpu request checking messages are not reconstructed");
    gpu_alarm(nullptr);
    if (gpu.head != gpu.tail)
        throw MissingDependency({"draw_sync", 0x80046dd4, {}, {}}, "symbol:libgpu-queued-call",
                                false, "Draining queued libgpu calls is not recovered");
    const auto alarm = [&] {
        if (std::bit_cast<std::int32_t>(gpu.deadline) <
                std::bit_cast<std::int32_t>(resident.vsync_counter) ||
            0xf0000 < std::bit_cast<std::int32_t>(gpu.polls++))
            throw MissingDependency({"draw_sync", 0x80046f80, {}, {}},
                                    "symbol:libgpu-timeout-reset", false,
                                    "The libgpu timeout message and reset are not reconstructed");
    };
    for (;;) {
        if ((platform_read(resident.platform, 0x80046e2c, 4) & 0x01000000U) != 0) {
            alarm();
            continue;
        }
        if ((platform_read(resident.platform, 0x80046e4c, 4) & 0x04000000U) == 0) {
            alarm();
            continue;
        }
        return;
    }
}

// 80043928 SetDefDrawEnv: drawing area and offset at (x, y), no texture
// window, texture page 10, dithering on, drawing to the displayed area
// only below 257 rows (289 in PAL), no background clear.
void Program::set_def_draw_env(std::uint32_t environment, std::uint32_t x, std::uint32_t y,
                               std::uint32_t w, std::uint32_t h) {
    const auto e = environment;
    for (const auto [offset, value] : {std::pair{0U, x},
                                       {2U, y},
                                       {4U, w},
                                       {6U, h},
                                       {8U, x},
                                       {10U, y},
                                       {12U, 0U},
                                       {14U, 0U},
                                       {16U, 0U},
                                       {18U, 0U},
                                       {20U, 10U}})
        set_memory(e + offset, value & 0xffffU, 2);
    set_memory(e + 22, 1, 1);
    const auto rows = static_cast<std::int32_t>(h);
    set_memory(e + 23, rows < ((resident.video_mode & 0xffU) != 0 ? 289 : 257) ? 1 : 0, 1);
    for (const auto offset : {24U, 25U, 26U, 27U})
        set_memory(e + offset, 0, 1);
}

// 800439e0 SetDefDispEnv: display area (x, y, w, h), full screen range,
// no interlace or 24-bit flags.
void Program::set_def_disp_env(std::uint32_t environment, std::uint32_t x, std::uint32_t y,
                               std::uint32_t w, std::uint32_t h) {
    const auto e = environment;
    for (const auto [offset, value] :
         {std::pair{0U, x}, {2U, y}, {4U, w}, {6U, h}, {8U, 0U}, {10U, 0U}, {12U, 0U}, {14U, 0U}})
        set_memory(e + offset, value & 0xffffU, 2);
    for (const auto offset : {16U, 17U, 18U, 19U})
        set_memory(e + offset, 0, 1);
}

// 80044534 SetDispMask: GP1 03 (display off for zero); turning the display
// off forgets the last display environment (8005693c filled with ff).
void Program::set_disp_mask(std::uint32_t mask) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        throw MissingDependency({"set_disp_mask", 0x80044574, {}, {}}, "symbol:printf-80019964",
                                false, "libgpu request checking messages are not reconstructed");
    if (gpu.services != 0x80056888)
        throw MissingDependency({"set_disp_mask", 0x800445b0, {}, {}}, "symbol:gpu-services", false,
                                "Another libgpu service table is not reconstructed");
    if (mask == 0)
        gpu.display_environment.fill(0xff); // 80047178
    // _ctl through the service table: the command is remembered.
    const auto command = 0x03000000U | (mask == 0 ? 1U : 0U);
    gpu.control[command >> 24U] = static_cast<std::uint8_t>(command);
    gpu.commands.push_back({GpuCommand::Kind::control, {}, 0, command});
}

} // namespace xem::reconstruction
