#pragma once

#include <array>
#include <cstdint>

namespace xem::reconstruction {

// Interrupt environment of the dispatcher 8004b9b4 and its handlers.
struct InterruptState {
    std::uint16_t initialized{};              // 800578a4
    std::array<std::uint32_t, 11> handlers{}; // 800578a8: one per I_STAT bit
    std::uint16_t mask{};                     // 800578d4: bits the dispatcher serves
    std::array<std::uint32_t, 3> registers{}; // 80058930: I_STAT, I_MASK, DPCR addresses
    std::uint32_t unexpected{};               // 8005893c: dispatches ending with a pending bit
    std::array<std::uint32_t, 8> vsync_callbacks{}; // 80058940
    // 8005896c: DMA completion callbacks; channel 3 is CdState::dma_callback.
    std::array<std::uint32_t, 7> dma_callbacks{};
    // 800578e0: stack pointer of the exception hook's context (the setjmp
    // buffer at 800578dc that the hook resumes, then calls 8004b9b4).
    std::uint32_t hook_stack{};
    std::uint32_t spu_callback{}; // 8005950c: SPU interrupt callback
    std::uint32_t spu_count{};    // 80059514: SPU interrupts served
};

} // namespace xem::reconstruction
