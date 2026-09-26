// Resident libgpu calls reached from interrupt context, where no frame
// services exist: their hardware polls are recorded platform reads. Also
// SetDispMask.
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
