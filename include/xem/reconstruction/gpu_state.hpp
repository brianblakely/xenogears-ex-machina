#pragma once

#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/disc_stream.hpp"
#include "xem/reconstruction/field_control.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_movie.hpp"
#include "xem/reconstruction/field_return.hpp"
#include "xem/reconstruction/field_script.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/field_sprite_model.hpp"
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/gte.hpp"
#include "xem/reconstruction/interrupts.hpp"
#include "xem/reconstruction/menu.hpp"
#include "xem/reconstruction/menu_save.hpp"
#include "xem/reconstruction/movie.hpp"
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>


namespace xem::reconstruction {

// Resident libgpu state: the request queue (enqueue 8004668c, runner
// 8004696c), the statics of the calls through it (gpu_queue.cpp) and the
// commands that reach the GPU.
struct GpuState {
    std::uint32_t services{};                  // 800568c8: service table address
    std::array<std::uint32_t, 12> functions{}; // 80056888: the service table
    std::uint8_t queued{};                     // 800568d1: zero runs requests at once
    std::uint8_t debug{};                      // 800568d2: request checking level
    std::uint8_t interlace{};                  // 800568d3: adds the interlace bit to display modes
    std::int16_t width{};                      // 800568d4: VRAM width
    std::int16_t height{};                     // 800568d6: VRAM height
    std::uint32_t sync_pending{};              // 800568d8: set by each request
    std::uint32_t sync_callback{};             // 800568dc: DrawSync callback
    std::array<std::uint8_t, 0x5c> draw_environment{};    // 800568e0: last PutDrawEnv
    std::array<std::uint8_t, 0x14> display_environment{}; // 8005693c: last PutDispEnv
    // 800569a0 GP0, 800569a4 GP1/GPUSTAT, 800569a8 DMA2 address, 800569ac
    // DMA2 block, 800569b0 DMA2 control.
    std::array<std::uint32_t, 5> registers{};
    // 800569b4 DMA6 address, 800569b8 DMA6 block, 800569bc DMA6 control,
    // 800569c0 DPCR: the registers ClearOTagR's ordering-table DMA uses.
    std::array<std::uint32_t, 4> otc_registers{};
    std::array<std::uint32_t, 3> current{}; // 800569c4: last operation, parameter, argument
    std::uint32_t head{};                   // 800569d4
    std::uint32_t tail{};                   // 800569d8
    std::uint32_t enqueue_mask{};           // 800569dc: interrupt mask saved by 8004668c
    std::uint32_t execute_mask{};           // 800569e0: interrupt mask saved by 8004696c
    std::uint32_t deadline{};               // 800569e8
    std::uint32_t polls{};                  // 800569ec
    // 8006be34: 64 requests of 60h bytes: operation, parameter pointer,
    // argument, then the copied parameter.
    std::array<std::uint8_t, 64 * 0x60> queue{};
    std::array<std::uint8_t, 0x44> packet{};   // 8005a238: packet built by _clr
    std::array<std::uint8_t, 0x100> control{}; // 8005a27c: last GP1 value per command
    std::vector<GpuCommand> commands;          // In program order
    // Platform input: the VRAM words each StoreImage transfer (_drs
    // 800462dc) delivers, in transfer order, however the queue runs it.
    std::deque<std::vector<std::uint8_t>> vram_reads;
    std::array<std::uint8_t, 0x14> move_packet{}; // 80056978: MoveImage's packet
    std::uint32_t reset_mask{}; // 800569e4: interrupt mask saved by _reset (80046c58)
    std::uint32_t tim_cursor{}; // 8005a37c: the next TIM of OpenTIM/ReadTIM
};

} // namespace xem::reconstruction
