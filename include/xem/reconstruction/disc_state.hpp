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

// Resident file reads (800295d8 and its setup 80029690). Globals whose
// meaning is not recovered keep their original addresses as names.
struct DiscReadState {
    std::uint32_t file_table{};             // 8004fdf0: 7-byte records, sector then size
    std::uint32_t directory_table{};        // 8004fdf4: u16 first file + 1 per directory
    std::uint32_t size{};                   // 8004fdf8: bytes to read, rounded to words
    std::uint32_t w_fe00{};                 // 8004fe00: file count of a list read
    std::uint32_t sector{};                 // 8004fe04
    std::uint32_t destination{};            // 8004fe08
    std::uint32_t w_fe0c{};                 // 8004fe0c
    std::uint32_t w_fe10{};                 // 8004fe10
    std::uint32_t directory{};              // 8004fe14: first file - 1 of the selected directory
    std::uint32_t read_directory{};         // 8004fe18: directory of the current read
    std::uint16_t h_fe26{};                 // 8004fe26
    std::uint16_t h_fe28{};                 // 8004fe28
    std::uint32_t ring_slots{};             // 8004fe2c: first slot of the selected ring
    std::uint32_t w_fe34{};                 // 8004fe34
    std::uint32_t offset{};                 // 8004fe38: low halfword of the read offset
    std::uint32_t w_fe3c{};                 // 8004fe3c
    std::uint32_t host_file{};              // 8004fe4c: host file handle
    std::array<std::uint32_t, 3> w_59ef8{}; // 80059ef8..80059f03, cleared per read
    std::uint32_t file{};                   // 80059f0c
    std::array<std::uint8_t, 4> location{}; // 80059f10: minute, second, sector (BCD), unused
    std::array<std::uint8_t, 4> b_59f18{};  // 80059f18: ring-mode control bytes
    std::uint16_t h_59f60{};                // 80059f60
    std::uint32_t requests{};               // 8005a488: CD reads issued
    std::uint32_t w_5a4dc{};                // 8005a4dc: failed data interrupts in a row
    // Interrupt-side read state (callbacks 8002a68c, 8002ac24, 8002b084,
    // 8002b2f0, 8002b5d0, DMA callbacks 8002ba40, 8002ba58, 8002bb50).
    std::uint32_t retry_reason{};           // 8004fe20
    std::uint32_t w_fde0{};                 // 8004fde0
    std::array<std::uint32_t, 3> skipped{}; // 8004fde4, 8004fde8, 8004fdec: unexpected sectors
    std::uint32_t saved_callback{};         // 80059f08: callback 80040fcc replaced
    std::array<std::uint8_t, 2> b_59f14{};  // 80059f14
    std::array<std::uint32_t, 2> w_5a48c{}; // 8005a48c, 8005a490
    std::array<std::uint32_t, 2> w_5a494{}; // 8005a494, 8005a498
    std::array<std::uint32_t, 2> w_5a4a4{}; // 8005a4a4, 8005a4a8
    std::uint32_t w_5a4b4{};                // 8005a4b4
    std::uint16_t h_5a4b8{};                // 8005a4b8: frame of a stalled movie stream
    // Payload of the active stream ring (after its header): count sectors of
    // 800h bytes. Attached with the ring header.
    resident::HeapBlock ring_payload;
    // Image stream state (8002bb50), words at 80059f24..80059f50: record
    // 1200 mode, x, y; record 1201 mode, x, y; records left; current x, y,
    // width; next height pointer; strips left. Halfword fields keep their
    // upper halves.
    std::array<std::uint32_t, 12> image{};
    // The tables at 8004fdf0 (8000 bytes) and 8004fdf4 (7a bytes), the sizes
    // 80028230 reads them with; empty before they are loaded.
    std::vector<std::uint8_t> files;
    std::vector<std::uint8_t> directories;
    // Header of the ring a stream read selects: count, eight-byte slots, then
    // the 24-byte tail before the payload (0x24 + 8 * count bytes).
    resident::HeapBlock ring;
    // The caller's list a list read (80029afc) sorts in place and keeps
    // (8004fe0c): 8-byte entries of u16 file, u16 unused, u32 destination,
    // then the halfword of the terminating zero file. The list-read data
    // callback 8002ac24 walks it.
    resident::HeapBlock list;
};

// CD library state reached by read setup: CdControl 8004111c, the command
// writer 80042088, the sync wait 80041b3c and the DMA callback 8004c21c.
// The interrupt handler 80042ca8 calls ready_callback for a completed command
// and sync_callback for delivered data.
struct CdState {
    std::uint32_t ready_callback{};         // 800564a8
    std::uint32_t sync_callback{};          // 800564ac
    std::uint32_t read_callback{};          // 80056844: CdReadCallback (8004373c)
    std::int32_t debug{};                   // 800564b4: nonzero levels print
    std::uint32_t status{};                 // 800564b8: last drive status (first response byte)
    std::uint32_t status2{};                // 800564bc: second response byte
    std::uint32_t shell_opened{};           // 800564c0: status bit 10 transitions to set
    std::array<std::uint8_t, 4> position{}; // 800564c4: last Setloc parameter
    std::uint8_t mode{};                    // 800564c8: last Setmode parameter
    std::uint8_t command{};                 // 800564c9: last command
    std::uint8_t sync_status{};             // 80056788: interrupt status of the last command
    std::uint8_t ready_status{};            // 80056789: interrupt status of the last data
    // Per-command tables, one word for each command 0-31.
    std::array<std::uint32_t, 32> setloc_first{};     // 80056420: send Setloc first
    std::array<std::uint32_t, 32> clear_ready{};      // 800565f0: clear 80056789
    std::array<std::uint32_t, 32> parameter_counts{}; // 800566f0
    // Controller register addresses: 80056770 index, 80056774 command,
    // 80056778 parameter.
    std::array<std::uint32_t, 3> registers{};
    std::uint16_t interrupt_poll{}; // 800578a6: nonzero makes waits poll the controller
    std::uint32_t dma_services{};   // 8005892c: service table
    std::optional<std::uint32_t> dma_set_callback; // *8005892c + 4
    std::uint32_t dma_interrupt_register{};        // 80058968: address of DICR
    std::uint32_t dma_callback{}; // 80058978: DMA channel 3 callback (table 8005896c)
    // Interrupt side (80042ca8, 800415b4).
    std::uint8_t end_status{};                  // 8005678a
    std::array<std::uint8_t, 8> sync_result{};  // 8005a210: response of the last command
    std::array<std::uint8_t, 8> ready_result{}; // 8005a218: response of the last data
    std::array<std::uint8_t, 8> end_result{};   // 8005a220
    std::array<std::uint32_t, 32>
        completes{}; // 80056570: acknowledged commands that complete later
    std::array<std::uint32_t, 32>
        ack_updates{};             // 80056670: acknowledgements that update the status
    std::uint32_t flag_register{}; // 8005677c: request/interrupt-flag register address
    // Register addresses of the sector transfer 80042aa8: 800567a4 (delay),
    // 80056780 (size), 800567a8 (DPCR), 800567ac (DMA3 address), 800567b0
    // (DMA3 block).
    std::array<std::uint32_t, 5> transfer_registers{};
    // 800564cc: 1 selects the alternate callback reset of the movie stream
    // ring (80040ce4, 80040cd0).
    std::uint32_t w_564cc{};
    // 8005a470: 1 while stream sectors arrive without their headers (a
    // Setmode without whole sectors, 801d586c).
    std::uint32_t stream_header_mode{};
};

// A hardware register store, in program order. Not RAM: a native service
// receives these as commands.
struct HardwareWrite {
    std::uint32_t address;
    std::uint32_t value;
    std::uint32_t width;
    bool operator==(const HardwareWrite &) const = default;
};

// Party sprite files (8001b044, 8001b3a8): 8004f374 marks files read ahead
// for decoding, 8004f31c the kind of set loaded, 8004f320 the kind the map
// needs (1 when map bits 0e-0f are set).
struct PartySpriteLoad {
    std::uint32_t pending{}; // 8004f374
    std::uint32_t loaded{};  // 8004f31c
    std::uint32_t needed{};  // 8004f320
};

} // namespace xem::reconstruction
