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
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace xem::reconstruction {

inline constexpr std::uint32_t game_data_bytes = 0x2358;
// Load address of the field overlay image.
inline constexpr std::uint32_t field_overlay_base = 0x8006faf0;
// The field main loop 80077e88's stack pointer (the SP at its calls of
// 8007554c and 800a5c40): the bottom of the stack its callees' frames grow from.
inline constexpr std::uint32_t field_loop_stack = 0x801fffc8;

inline constexpr std::string_view resident_executable_sha256 =
    "dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119";

// Source correlation for debugging. A script PC is never a machine-code address.
struct SourcePoint {
    std::string_view operation;
    std::uint32_t machine_address{};
    std::optional<std::size_t> actor;
    std::optional<std::uint32_t> event_pc;
    std::optional<field::FieldSpriteArguments> sprite_arguments{};
    std::optional<std::uint32_t> sprite_pc{};
    // Event opcode being executed; extended opcodes are fe00 | extended byte.
    std::optional<std::uint16_t> event_opcode{};
};

class MissingDependency : public std::runtime_error {
  public:
    MissingDependency(SourcePoint source, std::string id, bool available, const char *reason)
        : std::runtime_error(reason), point(source), dependency(std::move(id)),
          recovered(available) {}
    SourcePoint point;
    std::string dependency;
    bool recovered;
};

struct FieldActor {
    // Original-layout correlations used by the recovered sprite/return source.
    // These bytes are owned, not pointers into an emulator or a native save format.
    // Original addresses identify the correlated records; they are never host pointers.
    std::uint32_t address{};
    std::uint32_t descriptor_address{};
    field::original::Block<0x138> storage{};
    field::original::Block<0x5c> descriptor{};
    field::SpriteConstruction sprite{};
    field::original::Block<48> checkpoint{};
    std::optional<field::original::Block<12>> extension_110;
    std::optional<field::original::Block<16>> extension_114;
    // Collision model pointer of the descriptor's model instance (+0 record,
    // field +4); zero when the descriptor has no instance.
    std::uint32_t model{};

    [[nodiscard]] field::EventActor events() const;
    [[nodiscard]] field::ControlActor control() const;
};

// Resident pad-input queue (80035c0c enqueue, 80035cdc dequeue): a 16-entry ring
// of six halfwords. 80035db0 clears it along with other input halfwords whose
// readers are not recovered; those keep their original addresses as names.
struct InputQueue {
    std::uint32_t count{};    // 8005937c
    std::uint32_t write{};    // 80059380
    std::uint32_t read{};     // 80059384
    std::uint32_t overflow{}; // 80050208, set when a seventeenth entry is refused
    std::uint32_t w50200{};   // 80050200, set to 1 by the reset
    // 80059570, 80059574, 8005948c, 80059490, 800594a4, 800594a8: last dequeued entry.
    std::array<std::uint16_t, 6> current{};
    // 800594dc, 800594e0, 800594e8, 800594ec, 800595c8, 800595cc.
    std::array<std::uint16_t, 6> other{};
    // The ring: 16 entries of each field in its own array (8005a0fc + 20 *
    // field), indexed by the low 4 bits of the counters.
    std::array<std::array<std::uint16_t, 16>, 6> ring{};
    // 80035db0: clear the counters and entries; the ring keeps its contents.
    void reset() {
        const auto kept = ring;
        *this = {.w50200 = 1};
        ring = kept;
    }
    // 80035cdc: move the oldest entry into `current`; false when empty.
    bool dequeue();
};

// Per-VSync controller and clock state (VSync callback 8003634c and callees
// 800358bc, 80035e44, 80036220). The pad buffers are the BIOS pad driver's
// receive buffers: platform input, read only.
struct PadState {
    std::uint32_t vsyncs{};   // 80059488: VSync callbacks run; the play counter saves keep
    std::uint32_t hook{};     // 800501fc: optional per-VSync call
    std::uint32_t debugger{}; // 80059390
    std::uint8_t type{};      // 80059388: last controller type
    std::array<std::array<std::uint8_t, 34>, 2> buffers{}; // 800625fc, 8006261e
    std::array<std::uint32_t, 2> held{};                   // 80059374: last buttons per port
    std::array<std::uint32_t, 2> repeat_delay{};           // 8005022c: VSyncs since a new press
    // Analog bytes per port: 80059444, 8005944c, 80059430, 80059438 and
    // 80059448, 80059450, 80059434, 8005943c.
    std::array<std::array<std::uint8_t, 4>, 2> analog{};
    std::array<std::uint16_t, 8> remap_bits{};  // 800501e8
    std::array<std::uint8_t, 8> remap_index{};  // 80050238
    std::array<std::uint8_t, 16> direction_x{}; // 8005020c: by d-pad nibble
    std::array<std::uint8_t, 16> direction_y{}; // 8005021c
    std::uint8_t clock_stopped{};               // 800501f8
    // Play clock: 80059370 frames, 80059418 seconds, 80059420 minutes, 80059484 hours.
    std::array<std::uint8_t, 4> clock{};
    std::array<std::array<std::uint8_t, 8>, 2> actuators{}; // 8005a1bc
};

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
};

// A hardware register store, in program order. Not RAM: a native service
// receives these as commands.
struct HardwareWrite {
    std::uint32_t address;
    std::uint32_t value;
    std::uint32_t width;
    bool operator==(const HardwareWrite &) const = default;
};

// Platform results one field frame (8007554c) consumes, in call order. The
// analysis host supplies the values observed at the original service returns;
// a native host supplies its own platform's. A missing value stops the frame.
struct FrameServices {
    // VSync(1): horizontal blanks counted since the last VSync(0).
    std::deque<std::uint32_t> hblank_counts;
    // VSync(0) on return: root counter 1 (80057844) and the vertical-blank
    // counter (80057848).
    std::deque<std::array<std::uint32_t, 2>> vblank_waits;
    // VSync(-1) read by each libgpu alarm (80046efc).
    std::deque<std::uint32_t> vblank_counts;
    // libgpu alarm polls counted while DrawSync or LoadImage waited (800569ec).
    std::deque<std::uint32_t> alarm_polls;
    // GPUSTAT as read by ClearImage's packet builder (80045e44).
    std::deque<std::uint32_t> gpu_status;
    // DMA channel 2 busy bit as read by libgpu's command queue (80046758).
    std::deque<std::uint32_t> dma_busy;
    // SetIntrMask(0) results: the interrupt mask replaced around a call.
    std::deque<std::uint32_t> interrupt_masks;
    // GPU information reads (GP1 10h, then GPUREAD) by libgpu _param (80046638).
    std::deque<std::uint32_t> gpu_info;
};

// A platform result the host did not supply: invalid input, not a game result.
class ServiceUnavailable : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
// Consumes the next result of one service queue.
std::uint32_t take_service(std::deque<std::uint32_t> &results, const char *what);

// Party sprite files (8001b044, 8001b3a8): 8004f374 marks files read ahead
// for decoding, 8004f31c the kind of set loaded, 8004f320 the kind the map
// needs (1 when map bits 0e-0f are set).
struct PartySpriteLoad {
    std::uint32_t pending{}; // 8004f374
    std::uint32_t loaded{};  // 8004f31c
    std::uint32_t needed{};  // 8004f320
};

struct ResidentState {
    field::EventVariables variables;
    std::uint32_t random_seed{};
    field::BattleRequestState battle_request;
    field::MusicLoadState music;
    // Heap blocks field 80085b20 allocates: the stream buffer after its ring
    // header (8002a260) and the wave staging 800c3a1c.
    std::vector<resident::HeapBlock> music_blocks;
    resident::SoundDriver sound;
    field::DiscStreamState disc_stream;
    field::SpriteEnvironment sprite{};
    field::SpriteTaskState sprite_tasks{};
    field::SpriteUploadState sprite_upload{};
    field::SpriteHeapControls sprite_heap{};
    resident::Heap heap;
    field::SpriteModelState sprite_models{};
    // Original snapshot and original resource identities, not native persistence.
    // The resident snapshot storage is at 8005a4e4 (0x3804 bytes).
    field::original::Bytes field_snapshot;
    std::array<std::uint32_t, 3> saved_party_modes{}; // 8005a408, written with a snapshot
    std::uint32_t game_state{};                       // 8005a39c: resident game-state pointer
    // 8004f34c: the field map; primary 98 stores the requested one.
    std::uint32_t field_map{};
    // Map data read ahead of a map change (8001b484): file id + b8 of the
    // selected directory, kept in its own heap block.
    std::uint32_t preload_id{0xffffffffU};   // 8004f330
    std::uint32_t preload_slot{0xffffffffU}; // 8004f334; -1 while nothing is read ahead
    std::uint32_t preload_size{};            // 8005a4c0
    resident::HeapBlock preload_block;       // Address at 8005a4e0; bytes while allocated
    // Saved to variables 44 and 46 by 800a30fc; their producers are not recovered.
    std::uint16_t departure_5941c{}; // 8005941c
    std::uint8_t departure_594d0{};  // 800594d0
    // Persistent game data at *8005a39c: the 2358-byte block resident 8001b9d8
    // initializes for a new game (empty before boot allocates it).
    std::vector<std::uint8_t> game_data;
    [[nodiscard]] std::array<std::uint8_t, 3> party_modes() const; // game data + 22b1
    // 8005a414: the sprite resource each party slot's files were read into.
    std::array<std::uint32_t, 3> party_sprite_resources{};
    field::MathTables math;
    InputQueue input_queue;
    // 8005917c points at the word at 80010000; -1 there disables the debug
    // input and drawing paths.
    std::uint32_t debug_pointer{}; // 8005917c
    std::uint32_t debug_word{};    // 80010000
    // Geometry coprocessor registers: machine state that survives calls
    // (PushMatrix stores the loaded matrix to memory).
    Gte gte{};
    std::uint8_t gpu_type{}; // 800568d0: libgpu draw-mode encoding
    // Hardware I/O registers (1f801000..1f801fff) as observed; a platform input
    // that recovered code reads, never original RAM.
    std::array<std::uint8_t, 0x1000> io{};
    // Resident disc status (800286cc) and libcd CD_datasync (8004293c).
    // The host-file flag 8004fe48 is disc_stream.host_file_table.
    std::uint32_t disc_error{};   // 8004fdfc: set while a read is active
    std::uint32_t disc_pending{}; // 8004fe1c
    DiscReadState disc_read;
    CdState cd;
    std::vector<HardwareWrite> hardware_writes; // In program order
    std::uint32_t vsync_counter{};              // 80058960: VSync(-1)
    std::uint32_t vsync_hcount{};               // 80057844: root counter 1 at the last VSync(0)
    std::uint32_t vsync_previous{};             // 80057848: vertical blanks at the last VSync(0)
    std::uint32_t video_mode{};                 // 80058990: GetVideoMode (1 PAL)
    std::uint32_t w_4f378{};                    // 8004f378: nonzero hides the field compass
    std::uint32_t w_4f37c{};                    // 8004f37c: nonzero skips actor billboards
    std::uint32_t w_4f380{};                    // 8004f380: nonzero skips 8007520c and party models
    std::uint32_t sprite_buffer{};              // 800592f8: draw buffer of the sprite system
    std::array<std::uint32_t, 2> sprite_uploads{}; // 800594c4: pending uploads per buffer
    // Per-buffer sprite arenas (800594b4, 800592fc bytes each) and the bump
    // allocation within the current one.
    std::array<std::uint32_t, 2> sprite_arenas{};   // 800594b4
    std::uint32_t sprite_arena_bytes{};             // 800592fc
    std::uint32_t sprite_arena_cursor{};            // 80059580
    std::uint32_t sprite_arena_start{};             // 80059524
    std::uint32_t sprite_arena_end{};               // 80059534
    std::array<std::uint32_t, 2> sprite_releases{}; // 80059300: blocks freed per buffer
    std::uint32_t sprite_table{};                   // 8005956c: ordering table sprites draw into
    field::GteMatrix sprite_view{}; // 8004fbb8: camera matrix sprites are placed with
    std::array<std::array<std::int16_t, 4>, 4>
        sprite_quad{};                // 8004fb98: projected corners (SVECTOR)
    std::uint8_t sprite_platform_b{}; // 800591ae: with 800591ad, forces sprite matrix updates
    std::uint32_t cd_sync_deadline{}; // 8005a228
    std::uint32_t cd_sync_polls{};    // 8005a22c
    std::uint32_t cd_sync_label{};    // 8005a230: diagnostic string for a timeout
    std::uint32_t cd_dma_register{};  // 800567b4: address of DMA3 CHCR
    std::array<std::uint16_t, 2> text_cluts{}; // 800595d4 (even rows), 80059414 (odd rows)
    field::MatrixStack matrix_stack{};         // 80056d2c depth, 80056d30 records
    // Game-mode selection (8001996c) for the mode dispatcher 80019acc.
    std::uint32_t next_mode{}; // 80018088
    // 800592bc: the heap block 800199cc loaded for mode_loaded (its address,
    // or zero) with its bytes.
    resident::HeapBlock mode_block;
    std::uint32_t mode_loaded{}; // 800592c0: -1 once the next mode differs
    // Field exit 8007954c globals whose meaning is not recovered.
    std::uint8_t b_5942c{};  // 8005942c: cleared on every exit
    std::uint32_t w_4f30c{}; // 8004f30c
    std::uint32_t w_4f310{}; // 8004f310
    std::uint32_t w_4f370{}; // 8004f370: nonzero keeps a map change from reaching the dispatcher
    // Field entry 80078d44 globals whose meaning is not recovered.
    std::uint32_t w_4f2f8{}; // 8004f2f8: zero converts the screen first (800a77c4); set after
    std::uint32_t w_4f304{}; // 8004f304: nonzero restores a saved sound state
    // Return addresses InitGeom (80048bc4) and its BIOS setup (8004b4ac) keep.
    std::uint32_t geometry_return{};   // 800569f0
    std::uint32_t bios_setup_return{}; // 800593d4
    // 8005947c: nonzero keeps the battle epilogue on mode 2 and 800594f8 clear.
    std::uint8_t b_5947c{};
    // Field main loop (field_loop.cpp) globals whose meaning is not recovered.
    std::uint32_t w_4f2f4{}; // 8004f2f4: cleared by 800a31e8
    std::uint32_t w_4f318{}; // 8004f318: 800a31e8 frames since variable 10 last stepped
    std::uint32_t w_4f328{}; // 8004f328: bit 80 stops variable 10, bit 4 counts it down
    std::uint8_t b_59171{};  // 80059171: 800b236c when triangle opens the menu
    // 80065848: 8007ae78's pointer record for port 2 (x, y, buttons, dx, dy).
    std::array<std::int32_t, 5> pointer{};
    InterruptState interrupts;
    PadState pad;
    GpuState gpu;
    // RAM that disc DMA filled and no other Program value owns (file
    // destinations, the sector tail buffer 800596f8), by address.
    std::vector<resident::HeapBlock> disc_transfers;
    // Platform inputs, consumed in order; see interrupts.hpp.
    std::deque<PlatformInput> platform;
    DiscDrive drive;
    // 8003748c releases the block 80059394 names unless 800593a0 is set;
    // their producers are not recovered.
    std::uint32_t w_59394{};
    std::uint32_t w_593a0{};
    PartySpriteLoad party_sprite_load;
    // 80059f84: the GTE light color matrix (LR1..LB3) 80030a30 builds, with
    // the halfword after it.
    std::array<std::uint16_t, 10> light_colors{};
    std::array<std::uint8_t, 3> window_color{}; // 800594d4: dialogue backing tile color
    // 80022a0c's rectangle and pixels (800592f0, 800592f4).
    std::array<std::uint32_t, 2> image_upload{};
    // Stack windows code ran on inside heap blocks (80022a0c): address and
    // size. Their bytes are callee frames, like the stack below an entry.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> switched_stacks;
    // Bytes of allocated heap blocks that no other Program value interprets,
    // by address; a field teardown releases them with their blocks.
    std::map<std::uint32_t, std::vector<std::uint8_t>> heap_contents;
};

// Field reload 800a5c40. Globals whose meaning is not recovered keep their
// original addresses as names.
struct ReloadState {
    // Transition quads (800a663c, 800a6408, 800a5600): zoom 800c2684 and
    // rotation 800b00b8; their packets are the region 800b11ac..800b14a4.
    std::int32_t zoom{};
    field::GteVector angles{};
    std::uint32_t fade_frames{};    // 800afd14: frames of the reload's fade-in
    std::uint32_t stream_pending{}; // 800adb60: the field stream 80070488 started
    std::uint32_t stream_ring{};    // 800adc14: its ring block
    // VRAM 800a915c saves while particles pause (rectangle 800afc28, block
    // *800afc70); 800a91f0 loads it back.
    std::array<std::int16_t, 4> vram_rect{};
    std::uint32_t vram_save_address{};
    resident::HeapBlock vram_save;
    std::array<std::int16_t, 64> particle_ids{}; // 800b0108: -1 per stopped slot
    std::uint16_t effects_kept{}; // 800b233c: bit per effect pair 800864f0 leaves playing
    // Loaded components (80070cc8): descriptor table 800afb10, event actor
    // count 800adbfc, zones 800adbf4 (component 8), events 800adbf8
    // (component 5) and geometry 800afb14 (component 2).
    std::uint32_t descriptor_table{};
    std::uint32_t event_actors{};
    std::uint32_t zones_address{};
    std::uint32_t events_address{};
    std::uint32_t geometry_address{};
    std::uint32_t event_bytecode{}; // 800adc00: the events' bytecode
    // Collision tables of the loaded component 1: attributes (800afb20),
    // triangles (800afb24) and vertices (800afb34) per layer, and the
    // attribute table's words (800afd10).
    std::uint32_t collision_attributes{};
    std::array<std::uint32_t, 4> collision_triangles{};
    std::array<std::uint32_t, 4> collision_vertices{};
    std::uint32_t attribute_words{};
    std::uint32_t w_adb24{}; // 800adb24: the distortion buffers 800b20b4..800b20c0 are held
};

struct FieldState {
    // Decoded field overlay image loaded at 8006faf0, a read-only source for
    // its constant tables. overlay_verified marks the bytes whose loaded copy
    // was checked equal to it; reading any other byte fails.
    std::vector<std::uint8_t> overlay;
    std::vector<bool> overlay_verified;
    std::vector<FieldActor> actors;
    field::EventPackage event_package;
    field::CollisionPackage collision;
    // Original bytes inside field-return snapshot regions that no semantic
    // field interprets yet, keyed by address. Owned so the regions round-trip.
    std::map<std::uint32_t, std::uint8_t> uninterpreted;
    std::uint32_t descriptor_count{};
    std::size_t snapshot_bytes_used{};
    std::uint32_t snapshot_cursor{}; // 800afc50: 800a3c8c's pointer into the snapshot
    std::uint32_t sprite_bundle_address{};
    std::int16_t sprite_gate{}; // 800b218e
    std::uint8_t b_b2357{};     // 800b2357: nonzero disables billboard fog
    std::uint32_t initialized_sprites{};
    std::uint32_t party_reassignment{}; // 800b2268
    std::vector<field::SpriteAllocation> resources;
    std::vector<field::SpriteAllocation> frame_list;
    std::vector<std::uint8_t> replay_widths;
    field::EventControl event_control;
    field::FieldPassState pass;
    field::ControlInputs control_inputs;
    field::ControlState control_state;
    std::uint8_t battle_mode_source{};
    std::int32_t single_actor_mode{};
    std::uint8_t party_processing_mode{};
    std::array<std::int32_t, 3> party_indices{255, 255, 255};
    // Scheduler publication (800afd1c/800b0078/800b06b8): the last eligible actor.
    std::optional<std::size_t> published_actor;
    std::int32_t controlled_actor{};               // 800b226c
    std::array<std::int32_t, 4> triangle_counts{}; // 800afb44: active triangles per layer
    std::int16_t layer_count{};                    // 800afb54
    std::vector<std::uint8_t> zones; // Component 8 trigger zones (800adbf4), 24-byte records
    std::array<field::DialogueWindow, 4> dialogue{};
    std::uint16_t script_flag_b236c{};
    std::uint32_t last_sound_effect{}; // 800b21b8: last effect id field 80085634 started
    field::FieldFade fade{};
    field::FieldCamera camera{};
    std::array<std::uint8_t, 2> script_flags_b21d0{};
    // Controlled-actor history ring 800b14f0 (32 records of 72 bytes), its
    // index 800b2360 and reset word 800c3910 (-1 per update, 0 after a store).
    std::array<std::uint8_t, 32 * 72> history_ring{};
    std::array<std::uint32_t, 3> history_indices{}; // 800b2360: leader, then followers
    std::uint32_t history_reset{};
    std::array<std::int32_t, 3> party_characters{}; // 80062590: character id per slot
    std::int16_t followers_idle{};                  // 800b234e
    std::int16_t talk_inhibited{};                  // 800b2174
    std::int16_t gather_override{};                 // 800b2348
    std::uint32_t touch_latch{};                    // 800adf64
    std::uint16_t position_result{};                // 800adb00
    std::uint8_t forced_position{};                 // 800b21cf
    std::int32_t linked_floor_override{};           // 800adb94, applied when collision mode is set
    std::uint32_t motion_counter{}; // 800af858; cleared per update, consumed by 80082620
    // Loaded component 1 (800afb18), including its live attribute table
    // (800afb20). The parsed collision package is derived from these bytes.
    std::uint32_t collision_address{};
    std::vector<std::uint8_t> collision_component;
    std::uint32_t collision_mode{};                // 800adb98
    std::uint32_t collision_enabled{};             // 800adc0c; set to one before followers
    std::int16_t animation_mode{};                 // 800b2346; jump/animation mode
    std::int32_t motion_actor{};                   // 80065b08; last actor entering 80082bb8
    std::uint16_t terrain_angle{};                 // 800b2178
    std::int16_t terrain_scale{};                  // 800b218c
    std::array<std::int16_t, 4> terrain_speeds{};  // 800adfc4
    std::array<std::uint16_t, 8> terrain_angles{}; // 800adfa8
    field::GteMatrix world_matrix{};               // 800afaa4
    // Move phase 800739c0.
    field::GteMatrix sprite_view{};                   // 800afc30, rotation of sprite_view_angles
    field::GteVector sprite_view_angles{};            // 800b2184
    std::int32_t elevation_angle{};                   // 800b00b4
    std::int32_t camera_settle{};                     // 800adbac
    std::int32_t camera_release{};                    // 800adbb0
    std::int32_t camera_floor_latched{};              // 800adba8
    std::int32_t camera_cut{};                        // 800adc18: snap facing and view
    std::array<std::uint8_t, 8> heading_octants{};    // 800adc1c: bit per octant
    std::uint8_t orientation_hold{};                  // 800adb05: skip sprite orientation
    std::int16_t party_turn_speed{};                  // 800b21b4
    std::uint8_t camera_floor_fixed{};                // 800b21cd
    std::uint16_t followed_actor{};                   // 800b233e
    std::array<std::uint16_t, 24> direction_tables{}; // 800aea34 facings, 800aea54 directions
    // Message windows (FC 8009bf8c and callees).
    std::uint32_t messages_address{};   // 800adbf0: loaded field component 7
    std::vector<std::uint8_t> messages; // Component 7, the message table
    std::array<std::vector<resident::HeapBlock>, 4> dialogue_blocks; // Text primitive buffers
    std::array<std::int32_t, 4> dialogue_slots{};                    // 800b068c: -1 when free
    std::uint32_t dialogue_slot_cursor{};                            // 800ade90
    std::array<std::uint16_t, 8> dialogue_vram{}; // 800adf54: text VRAM x/y per window
    std::int16_t text_speed{};                    // 800b21d6: window slide steps
    std::uint32_t dialogue_gate_afd04{};          // 800afd04: nonzero defers FC
    std::uint32_t disc_idle_known{};              // 800adb70: zero requires a disc query
    // 800adb70 is also the movie request: extended 60 sets it and the field
    // loop plays the movie (800a7c58) and clears it.
    field::MovieState movie{};
    std::uint32_t exit_mode{};  // 800b0064: bits 0-6 the next game mode, bit 80 calls 8001bb50
    std::uint32_t gate_adbc4{}; // 800adbc4: a requested map change waits unless ff
    // Field frame 8007554c.
    std::uint32_t frame_start_hcount{}; // 800adb9c: VSync(1) at frame start
    std::uint32_t frame_drawn_hcount{}; // 800adba0: VSync(1) after drawing
    std::uint32_t draw_buffer{};        // 800adb08: index of the buffer being built
    std::uint32_t draw_block{};         // 800c426c: its 80f4-byte block (800b249c + 80f4 * index)
    // Positional sound emitters (800afe88): actor, effect id and a word each.
    std::array<std::array<std::uint16_t, 3>, 3> emitters{};
    std::int16_t emitter_source{}; // 800b22e0: listener (0 controlled actor, 1 eye, 2 target)
    std::array<std::array<std::int16_t, 4>, 2> fade_windows{}; // 800afe3c: fade texture windows
    // Compass (80074108).
    std::array<std::uint16_t, 16> compass_colors{};     // 800afc08
    std::array<std::uint16_t, 128> compass_palette{};   // 800afd24: 8 rows, dark when blocked
    std::array<std::int16_t, 4> compass_palette_rect{}; // 800b004c: its VRAM rectangle
    std::int16_t compass_heading{};                     // 800adb48
    std::int16_t compass_target{};                      // 800adb4a
    field::GteMatrix matrix_afa84{}; // 800afa84: compass base times the camera rotation
    // Regions the frame's drawing code addresses: both draw buffer blocks
    // (800b249c), packet buffers and model instance records.
    OriginalRegions regions;
    // Descriptors after the event actors' (map pieces), 5c bytes each.
    struct Piece {
        std::uint32_t address{};
        field::original::Block<0x5c> descriptor{};
    };
    std::vector<Piece> pieces;
    // Model pass (800748e8).
    field::GteMatrix cull_view{};                    // 800b00e8: bounding centre in view
    std::array<std::uint32_t, 2> cull_margins{};     // 800c3a5c x, 800c3a60 y
    std::array<std::int16_t, 3> piece_drift{};       // 800b21ae x, 800b21b0 z, 800b21b2 y
    std::array<std::int32_t, 3> piece_drift_total{}; // 800b21bc x, y, z
    std::uint8_t piece_drift_mode{};                 // 800b21d2: 7f bits select, 80 draws always
    std::array<std::uint8_t, 3> fog_color{};         // 800b2190
    std::array<std::uint8_t, 3> far_color{};         // 800b2194
    std::array<std::int16_t, 2> fog_range{};         // 800b2198 near, 800b219a far
    std::array<std::uint16_t, 3> back_color{};       // 800afb04: lit models' background
    // Later frame steps. Gates whose drawing is not recovered keep their
    // original address as their name.
    std::uint32_t particles_paused{};              // 800adb34
    std::array<std::uint8_t, 64> particle_slots{}; // 800b14b0: 1 while an emitter runs
    std::int16_t distortion{};                     // 800b2078: screen distortion active
    std::uint32_t w_af278{};                       // 800af278: gates 800a84c0
    std::int16_t h_b00b2{};                        // 800b00b2: with 800adb50, gates 80075484
    std::uint32_t w_adb50{};                       // 800adb50
    std::uint32_t w_b2264{};                       // 800b2264: gates 8007520c
    std::uint32_t w_adb54{};                       // 800adb54: gates 800abec8
    std::uint16_t h_afd20{}; // 800afd20: ff40 once a party member is placed; no reader recovered
    std::uint32_t dialogue_ticks{};                  // 800ade98
    std::uint32_t dialogue_cursor{};                 // 800ade94: 0..4, every fourth tick
    std::uint32_t background_mode{};                 // 800b0048: 3 copies VRAM behind cuts
    std::array<std::uint8_t, 3> clear_color{};       // 800b219c
    std::int16_t h_afea8{};                          // 800afea8: calls of 800920d8
    std::uint32_t pending_load{};                    // 800adbb4
    std::uint32_t pending_load_source{};             // 800af87c
    std::array<std::int16_t, 4> pending_load_rect{}; // 800afc58
    std::uint32_t w_adb4c{};                         // 800adb4c: joins the second model table
    std::int16_t ot_depth{};                         // 800b21d4: model table entries joined
    // Field main loop 80077e88 between frames (field_loop.cpp).
    std::uint32_t transition{};     // 800adb38: nonzero runs the 800a5924 transition
    std::uint32_t w_adbd0{};        // 800adbd0: read by the branch after a battle request
    std::uint32_t gate_adbd8{};     // 800adbd8: zero leaves the field (exit kind 3)
    std::uint32_t gate_adbe8{};     // 800adbe8: zero leaves the field (exit kind 2)
    std::uint16_t input_mask{};     // 800b217a: buttons of port 1 the drain keeps
    std::uint16_t held_buttons_2{}; // 800afea0: port 2 buttons held
    std::uint16_t held_history{};   // 800afc6c: port 1 buttons held since cleared
    std::uint8_t b_b02c8{};         // 800b02c8: 1 skips 800a31e8
    std::uint8_t pause_inhibited{}; // 800b2358: nonzero ignores Start (800c3900 800)
    // 8007ae78: pad buffer per port (800b0054), divisors (800b005c, 800b0060)
    // and positions per port (800b0068 x, 800b0070 y).
    std::array<std::uint32_t, 2> pointer_pads{};
    std::array<std::uint16_t, 2> pointer_divisors{};
    std::array<std::int32_t, 2> pointer_x{};
    std::array<std::int32_t, 2> pointer_y{};
    // Main-loop registers that survive between frames: s4 latches the
    // L2+R2 combination (800798bc), s5 records that 800afc78 was saved.
    // A host takes them from the loop's registers at an imported frame.
    bool combination_latched{};
    bool music_saved{};
    ReloadState reload;
};

// Resumable points of field frame 8007554c: each names the call the frame
// makes next (original call site in comments). Analysis resumes a frame from
// an original snapshot taken at that call; a native frame starts at `start`.
enum class FrameStep : std::uint8_t {
    start,           // 8007554c
    emitters,        // 8007557c: 80086908
    fade,            // 800755a8: 80071cb4
    compass,         // 800755e4: 80074108
    models,          // 80075604: 800748e8
    characters,      // 8007560c: 800752c8
    particles,       // 80075614: 800a9688
    distortion,      // 80075638: 800a4dac
    call_800a84c0,   // 80075648
    call_80075484,   // 80075650
    call_8007520c,   // 80075658
    call_800abec8,   // 80075660
    drawn_time,      // 80075694: VSync(1)
    draw_sync,       // 800756a4: DrawSync(0)
    dialogue_timers, // 800756ac: 800805f4
    dialogue,        // 800756c4: 8008004c
    vertical_sync,   // 800756cc: VSync(0)
    timed_release,   // 800756d4: 80032cb8
    clear,           // 800756dc: ClearImage or MoveImage
    environments,    // 80075780: PutDispEnv, PutDrawEnv
    uploads,         // 800757c4: 80025044
    call_800920d8,   // 800757f0
    load,            // 800757f8: a pending LoadImage
    tables,          // 80075850: AddPrims
    draw,            // 800758bc: DrawOTag
};

class Program;
// Read-only observation. Hosts may interrupt at a boundary; no callback supplies
// a computed game result. References expire when the callback returns.
using ProgramObserver = std::function<void(const Program &, SourcePoint, bool completed)>;

// A single owner for reusable recovered behavior. No case files, expectations,
// host clocks, presentation, or CPU emulation belong here. All borrowed views are
// constructed for a call; moving this object cannot leave internal dangling spans.
class Program {
  public:
    ResidentState resident;
    std::unique_ptr<FieldState> field;
    // Battle-mode memory while the battle overlay is loaded.
    std::optional<battle::BattleMemory> battle;
    // Menu-mode memory while the menu overlay is loaded.
    std::optional<menu::MenuMemory> menu;

    [[nodiscard]] field::FieldSpriteEnvironment sprite_environment() const;
    void set_sprite_environment(const field::FieldSpriteEnvironment &environment);

    // Narrow entry and the broader original 800a28d4 return branch use this same
    // 800a3474 implementation. Existing live sprites require original cleanup.
    void restore_field_data(const field::original::RestoreAllocation &allocate = {},
                            const ProgramObserver &observe = {});
    // Data restore -> all factories, in original order -> optional reassignment.
    // The later 800a3c8c checkpoint pass is NOT part of this original function.
    void restore_field(const field::SpriteAllocator &allocate, const field::SpriteReleaser &release,
                       const field::original::RestoreAllocation &allocate_extension = {},
                       const ProgramObserver &observe = {},
                       const field::SpriteImageUploader &upload_image = {});

    // Field overlay 800a3c8c, after the factories: restore the transform block
    // and each actor's sprite checkpoint from the resident snapshot.
    void checkpoint_pass(const ProgramObserver &observe = {});

    // Semantic operations used by hosts and future native control. An event pass
    // is not a field update, a frame, or a guarantee of player-control readiness.
    [[nodiscard]] field::ScheduleResult event_pass(const ProgramObserver &observe = {});
    [[nodiscard]] field::BatchResult event_batch(std::size_t actor, std::int32_t limit,
                                                 const ProgramObserver &observe = {});
    // Field overlay 8008110c: events, previous positions, eligible motion,
    // controlled contact/position, other positions, encounter and followers.
    // Stops with MissingDependency at the first unrecovered callee it reaches.
    void field_update(const ProgramObserver &observe = {});
    // Field overlay 800739c0: the field update, then camera, view matrices,
    // actor facing and sprite orientation.
    void field_move(const ProgramObserver &observe = {});
    // Field overlay 8007554c: one field frame (move phase, drawing, buffer
    // presentation and the frame-rate wait). Services supply platform timing
    // and GPU status results; see FrameServices.
    void field_frame(FrameServices &services, const ProgramObserver &observe = {},
                     FrameStep from = FrameStep::start);
    // Field main loop 80077e88 from a frame's return (800782e4) up to the
    // call of the next frame (800782dc): 800a5924, the exit, map-change and
    // menu checks, 80078b5c, the pause and reset checks, then 80077dac
    // (buffer swap, ordering tables, the pad drain 80074700 and 800a31e8).
    // `services` supplies 80077dac's VSync(1). Branches whose callees are not
    // recovered stop with MissingDependency.
    void field_between_frames(FrameServices &services, const ProgramObserver &observe = {});
    // One main-loop iteration: the code between frames, then the frame.
    void field_loop_step(FrameServices &services, const ProgramObserver &observe = {});
    // 80078d44, the field entry, from its caller's stack frame (the main
    // loop's SP - 30h), then the main loop up to its first frame.
    void field_entry(FrameServices &services, std::uint32_t frame,
                     const ProgramObserver &observe = {});
    void field_loop_start(FrameServices &services, const ProgramObserver &observe = {});
    void field_loop_top(FrameServices &services, const ProgramObserver &observe = {});
    void default_draw_env(std::uint32_t env, std::int32_t x, std::int32_t y, std::int32_t w,
                          std::int32_t h); // 80043928 SetDefDrawEnv
    void default_disp_env(std::uint32_t env, std::int32_t x, std::int32_t y, std::int32_t w,
                          std::int32_t h); // 800439e0 SetDefDispEnv
    void show_reassigned_party();          // 800ad898
    // Resident 800295d8: start reading `file` of the selected directory into
    // `destination`; returns 0, or -3 (no such file) and -4 (empty ring).
    // Waiting for an earlier read, host-file reads and CD waits that need an
    // interrupt stop with MissingDependency.
    std::int32_t read_file(std::int32_t file, std::uint32_t destination, std::uint32_t offset,
                           std::uint32_t mode);
    // Resident 80028470: select directory `base + index`; -1 when it is empty.
    std::int32_t select_directory(std::uint32_t base, std::uint32_t index);
    // Resident 80029afc: sort the owned list (disc_read.list) into file order
    // and start reading its first file at `offset`; the interrupt callbacks
    // read the rest. When the first sorted entry has no destination,
    // `offset` instead names a file to seek to (8002a394; not positive:
    // pause). Returns 0, or -3 for a missing or empty list.
    std::int32_t read_files(std::int32_t offset);
    // Resident 80029eb0: stream `file` into the owned ring (disc_read.ring,
    // at least two slots) with six halfword stream parameters; the interrupt
    // callbacks deliver it. Returns 0, -4 (no ring) or -3 (no such file).
    std::int32_t read_stream(std::int32_t file, std::uint32_t ring, std::uint32_t offset,
                             const std::array<std::uint16_t, 6> &parameters);
    // Resident 8004b9b4, entered from the BIOS exception hook: serve every
    // pending interrupt the dispatcher enables, until none is pending.
    // Asynchronous register reads come from resident.platform.
    void interrupt_dispatch();
    // The end of a field reload or load stage, or a VSync(0) outside a
    // frame: the arrivals recorded since the previous one (site 8004b674).
    void deliver_stage_arrivals() { deliver_arrivals(0x8004b674); }
    // A stage completed at the return address `address`: its arrivals, then
    // the position that ends them when the platform input records one.
    void reach_position(std::uint32_t address);
    // Deliver the interrupt arrivals at the front of the platform input: a
    // host ending an imported call whose remaining arrivals all came before
    // its return.
    void deliver_pending_arrivals();
    // Resident 8003c028, the sound driver tick; `event` is V0 at entry (the
    // driver flags the event handler loaded). Returns 0.
    std::uint32_t sound_tick(std::uint32_t event);
    // Resident 8001b66c: stop the playing sequence and forget the loaded pair.
    void stop_music();
    // Field 80085c90: one step of loading music `id` (the main loop passes
    // 8004f324 while 8004f308 is -1 and stores the result there): -1 while
    // the wave bank streams, its sequence is read or the disc is busy, 0 once
    // the sequence plays. SPU uploads become HardwareWrite commands; waits
    // for SPU and disc transfers take the next interrupt arrivals.
    std::uint32_t poll_music(std::uint32_t id);
    // Field 800859dc: the wave stream's chunk callback (A0 the chunk).
    void consume_music_chunk(std::uint32_t chunk);
    // Resident 800380d0: create a wave bank from the header staged at
    // `header` (`bytes` staged) in `mode` (0 its own SPU address, else fixed
    // at that address, -1 dynamic), start uploading the staged samples and
    // link it last on the wave bank list; returns it.
    std::uint32_t load_wave_bank(std::uint32_t header, std::uint32_t bytes, std::uint32_t mode);
    // Resident 80039850: create and link a sequence over the event data at
    // `data`; returns it.
    std::uint32_t open_sequence(std::uint32_t data);
    // Resident 80039a80: (re)start a sequence at `volume`, fading over
    // `frames` ticks when nonzero (8003a89c).
    void start_sequence(std::uint32_t sequence, std::uint32_t volume, std::uint32_t frames);
    // Resident 800386c4: select a sound output mode (resident::SoundMode) and
    // reapply every volume it affects; the reverb output volume goes to the
    // SPU (libspu 8004e574) as hardware writes. The CD mix 8003885c (driver
    // flag 4000) stops with MissingDependency.
    void set_sound_mode(std::int32_t mode);
    // Resident 8001996c: select the next game mode for the mode dispatcher
    // 80019acc, dropping the cached mode block when the mode changes.
    void set_next_mode(std::uint32_t mode);
    // Field extended event handler that the FE handler's table reaches for
    // actor `index`, whose working PC is the extended byte.
    void event_extended(std::size_t index, const ProgramObserver &observe = {});
    // Field 800a7f78: whether the movie loop drains the pad before deciding.
    [[nodiscard]] field::MoviePad movie_pad() const;
    // Field 800a7f78..800a80b0 once that drain has run: the loop's decision.
    // A skip applies the CD fade and the five waits before returning.
    field::MovieStep movie_decision(field::MovieServices &services);
    // Field 8007954c: leave the field. Kind 3 (a map change to the mode in
    // 800b0064) returns true where the original calls the mode dispatcher
    // 80019acc(0), which the caller runs next; false when 8004f370 keeps the
    // field. Other kinds stop with MissingDependency.
    bool exit_field(std::uint32_t kind);
    // Resident 8001b758 up to 8001b82c, after the battle 80070f40 returns:
    // victory (1), escape (40) and 21 select the next mode (6 with the
    // battle's 800d3338 set, 2 with 8005947c set, otherwise 1 or 3 by the
    // persistent map selector 8006f94e); a defeat (81) clears 8004f30c
    // (8001ac94) and selects mode 1 with map selector 1ea and 8006f950..954
    // cleared. Unless 8005947c is set, 800594f8 becomes 1. `outcome`
    // (800c48ea) and `mode_flag` (800d3338) are battle overlay bytes.
    void finish_battle_mode(std::uint32_t outcome, std::uint32_t mode_flag);
    // Resident 8001b484: read map data `id` ahead into slot `slot`. 0 once that
    // data is the one read ahead; -1 while the disc is busy or after starting
    // the read.
    std::int32_t preload_field(std::uint32_t id, std::uint32_t slot);
    // Field 800a30fc: record the departure in the game data and variables,
    // then copy the first 400 bytes of the variable bank to game data +1930.
    void save_field_departure();
    // Field 80078494..80078558 in the main loop 80077e88: once a requested
    // map's data is read ahead and the disc and fade are idle, save the
    // departure, reload (800a5c40) and reset the input queue.
    void field_map_change_step(FrameServices &services, const ProgramObserver &observe = {});
    // The same step up to the reload call (80078540): true when it is due.
    bool start_map_change();
    // Field 80077dac, before each field frame of the main loop and of the
    // reload's fade-in: VSync(1), the next draw buffer, the pad drain and the
    // play record (800a31e8).
    void field_pre_frame(FrameServices &services);
    // Field 80078b5c, after each field frame of the main loop and of the
    // reload's fade-in: the RNG, the music load gate and the camera-cut
    // countdown.
    void field_post_frame();
    // Field 800a6408: place and link the reload's five transition quads.
    void reload_transition_draw();
    // Field 800a5600: the next buffer's transition quad color.
    void reload_transition_shade(std::uint32_t value);
    // Field 800a5c40 from the fade-in (800a6120) to its return: start the
    // fade, run the fade-in frames and restore what the reload suspended.
    // `frame` is the reload's stack frame (its entry SP - 48h).
    void field_reload_fade_in(FrameServices &services, const ProgramObserver &observe = {});
    // One pass of that loop (800a6148..800a61c8): `shade` is the quads'
    // 8.16 color, returned for the next pass.
    std::int32_t field_reload_fade_frame(FrameServices &services, std::int32_t shade,
                                         const ProgramObserver &observe = {});
    void field_reload_finish(FrameServices &services, std::uint32_t frame);
    // Field 800a5c40 up to its reload-type dispatch (800a5d74): stop the old
    // field's effects, save the screen, tear the field down (800700b0) and
    // move the read-ahead map data.
    void field_reload_teardown(FrameServices &services, const ProgramObserver &observe = {});
    // Field 800a5c40 from its entry, for reload type 2: the teardown, the
    // screen fade 800a5884, the party sprites (8001b044, 8001b3a8), then the
    // field load 80070cc8 onward. `frame` is the reload's stack frame (its
    // entry SP - 48h). Completed stages are observable boundaries.
    void field_reload(FrameServices &services, std::uint32_t frame,
                      const ProgramObserver &observe = {});
    // Field 80070cc8: load the map data read ahead (8005a4e0) as the field.
    // `frame` is its stack frame (its entry SP - a0h).
    void load_field(FrameServices &services, std::uint32_t frame,
                    const ProgramObserver &observe = {});
    // Battle 80085ccc: commit and resolve an action.
    void commit_battle_action(std::uint32_t attacker, std::uint32_t targets,
                              std::uint32_t animation);
    // Battle 80085618: apply queued results.
    void apply_battle_results(std::uint32_t queue);
    // Battle 8007252c: rebuild the alive mask and outcome.
    void update_battle_alive();
    // Battle 800799c8: an enemy's AI script.
    void run_battle_enemy_script(std::uint32_t slot, std::uint32_t flag);
    // Battle 8007171c / 800718bc: ATB tick and turn-timer reload.
    void tick_battle_timers();
    void reload_battle_timer();
    // A battle step over battle memory with the resident game data and rand
    // state (the turn procedure's steps in battle.hpp).
    void run_battle(const std::function<void(battle::Battle &)> &step);
    // Post-battle 801e2794: victory rewards and write-back.
    void grant_battle_rewards();
    // Post-battle 801e2280 up to 801e23d4: experience pool and gold.
    void total_battle_rewards();
    // Post-battle 801e1444: add drops to the inventory.
    void add_battle_drops(std::uint32_t ids, std::uint32_t counts, std::uint32_t categories);
    // Menu 801e31c0: apply a consumable to a character; nonzero when it
    // restored nothing.
    std::uint32_t apply_menu_item_effect(std::uint32_t tables, std::uint32_t character,
                                         std::uint32_t item);
    // Menu 801db920 from 801dbba4 to 801dbc90: use inventory entry `index`
    // on the party slots in `targets`.
    void use_menu_item(std::uint32_t index, std::uint32_t targets);
    // Menu 801df0d4: commit an equipment change.
    std::uint32_t swap_menu_equipment(std::uint32_t slot, std::uint32_t part, std::uint32_t special,
                                      std::uint32_t gear);
    // Menu 801e36d4 / 801e3a80: equipment bonuses and the shown stats.
    void menu_equipment_bonuses(std::uint32_t tables, std::uint32_t character);
    void menu_equipment_stats(std::uint32_t tables, std::uint32_t character);
    // Menu save and load (menu_save.hpp) with the resident game data.
    // `scratch` is 801cb184's name buffer as the caller's stack holds it.
    void serialize_menu_save(std::uint32_t payload, std::uint32_t digit,
                             menu::NameScratch &scratch);                     // 801cba4c
    std::uint32_t seal_menu_save(std::uint32_t payload);                      // 801cc424..801cc448
    void store_menu_game_data(std::uint32_t payload);                         // 801e4a28
    void decode_menu_names(menu::NameScratch &scratch);                       // 801cb184
    menu::LoadCheck check_menu_load(std::uint32_t buffer);                    // 801cb6f0..801cb71c
    void restore_menu_game_data(std::uint32_t payload, std::uint32_t tables); // 801e4d10
    void apply_menu_load(std::uint32_t payload, menu::NameScratch &scratch);  // 801cb28c
    bool menu_load_slot_valid(std::uint32_t mode);                            // 801c9bcc
    std::uint32_t find_menu_load_slot(std::uint32_t mode);                    // 801c9d34
    // Resident 80028530: the current disc (u16 at the directory table + 78).
    [[nodiscard]] std::uint32_t current_disc() const;

  private:
    // Platform services of the call now running events (the field load's
    // initialization); event instructions that reach libgpu use them.
    FrameServices *event_services_{};
    // Interrupt handlers now running (host bookkeeping, not original RAM).
    std::uint32_t interrupt_depth_{};
    // Outside interrupt code, the arrivals recorded before the next
    // hardware read: code that polls hardware observes them first.
    void deliver_due_arrivals();
    // Field frame steps (field_frame.cpp).
    void frame_emitters(std::uint32_t listener);                                    // 80086590
    void frame_fade();                                                              // 80071cb4
    void frame_compass(FrameServices &services);                                    // 80074108
    void frame_models();                                                            // 800748e8
    void frame_characters(FrameServices &services, const ProgramObserver &observe); // 800752c8
    void sprite_buffer_begin(std::uint32_t buffer);                                 // 800250e0
    void sprite_frames();                                                           // 8001d468
    void build_sprite_frame(std::uint32_t sprite, std::uint32_t frame);             // 8001dae8
    void build_sprite_cell_frame(std::uint32_t sprite, std::uint32_t frame);        // 8001d53c
    [[nodiscard]] std::uint32_t sprite_part_controls(std::uint32_t sprite, std::uint32_t part,
                                                     std::uint32_t stream, std::uint32_t &group);
    [[nodiscard]] std::uint32_t sprite_part_offsets(std::uint32_t part, std::uint32_t stream,
                                                    bool wide);
    void sprite_frame_scale(std::uint32_t sprite, std::uint32_t record);
    void sprite_upload(std::uint32_t source, std::int32_t x, std::int32_t y, std::uint32_t width,
                       std::uint32_t height);         // 800251c8
    void sprite_pending_tasks();                      // 8001c9f8
    void draw_task_model(std::uint32_t node);         // 80025718
    void refresh_sprite_matrix(std::uint32_t sprite); // 80022038
    void frame_billboards(std::uint32_t table);       // 80075b44
    void sprite_color(std::uint32_t sprite, std::uint32_t red, std::uint32_t green,
                      std::uint32_t blue);                                     // 80021b98
    void sprite_recolor_parts(std::uint32_t sprite);                           // 8001f6b0
    void sprite_billboard(std::uint32_t sprite, std::uint32_t slot);           // 8001e298
    void place_sprite(std::uint32_t sprite);                                   // 8001e148
    void emit_sprite_parts(std::uint32_t sprite, std::uint32_t slot);          // 8001e3d8
    [[nodiscard]] std::int32_t rot_trans_pers(const field::GteVector &vector); // 8004a64c
    // Sprite sources over the owned field state; `actor` lends that actor
    // to sprite callbacks that select it.
    void with_sprite_sources(const std::function<void(const field::SpriteSources &)> &call,
                             std::optional<std::uint32_t> actor = {});
    [[nodiscard]] field::GteMatrix memory_matrix(std::uint32_t address) const;
    void set_memory_matrix(std::uint32_t address, const field::GteMatrix &m);
    // Owned record bytes (actor records, descriptors and sprites, sprite task
    // blocks, music blocks, the disc read ring, payload, list and transfer
    // blocks, game data and sound driver objects) at an original address:
    // `record_block` spans to the end of the owning record.
    [[nodiscard]] std::span<std::uint8_t> record_block(std::uint32_t address) const;
    [[nodiscard]] std::span<std::uint8_t> record_bytes(std::uint32_t address,
                                                       std::size_t width) const;
    [[nodiscard]] std::span<std::uint8_t> descriptor_bytes(std::size_t index);
    [[nodiscard]] bool model_culled(std::uint32_t instance); // 800aaa74
    void draw_model(std::uint32_t model, std::uint32_t packets, std::uint32_t table,
                    std::int32_t mode); // 8002c700
    void draw_primitives(std::uint32_t routine, std::uint32_t record, std::int32_t count);
    // Field 8007ab6c/8007ac58: one compass quad (a letter when `label`).
    [[nodiscard]] std::uint32_t rot_average4(std::uint32_t record,
                                             std::uint32_t packet);           // 8004a7bc
    [[nodiscard]] field::GteLong vector_normal(const field::GteLong &vector); // 80048d7c
    void frame_shadows(std::uint32_t table, std::uint32_t buffer);            // 800764b4
    void compass_quad(std::uint32_t table, std::uint32_t record, const field::GteMatrix &m,
                      bool label);
    [[nodiscard]] std::uint16_t overlay_half(std::uint32_t address) const;
    // Original addresses of Program-owned packets and globals, for the
    // drawing code that links packets by address.
    [[nodiscard]] std::uint32_t memory(std::uint32_t address, std::size_t width = 4) const;
    [[nodiscard]] std::span<std::uint8_t> resource_bytes(std::uint32_t address,
                                                         std::size_t width) const;
    void set_memory(std::uint32_t address, std::uint32_t value, std::size_t width = 4);
    void add_primitive(std::uint32_t table_entry, std::uint32_t packet); // addPrim
    void add_primitives(std::uint32_t table, std::uint32_t first, std::uint32_t last);
    void frame_dialogue_timers();                                      // 800805f4
    void frame_dialogue(FrameServices &services, std::uint32_t table); // 8008004c
    void draw_dialogue_window(FrameServices &services, std::uint32_t table, std::uint32_t w,
                              bool first);
    void close_dialogue(std::uint32_t w);                               // 8007f6f8
    void queue_dialogue_page(std::uint32_t window, std::uint32_t text); // 80034714
    void dialogue_glyphs(std::uint32_t window);                         // 80033df0
    void release_dialogue_block(std::uint32_t window, std::uint32_t address,
                                std::uint32_t call_site);
    void draw_dialogue_text(FrameServices &services, std::uint32_t window, std::uint32_t table,
                            std::uint32_t buffer); // 80034888
    void draw_dialogue_frame(std::uint32_t table, std::uint32_t buffer,
                             std::uint32_t w); // 8007e1c0
    void dialogue_quad(std::uint32_t packet, std::int32_t x, std::int32_t y, std::int32_t w,
                       std::int32_t h, bool mirror);                    // 8007e16c
    void dialogue_choice(std::uint32_t w);                              // 8007dcf8
    [[nodiscard]] std::uint32_t dialogue_waiting(std::uint32_t window); // 80033cd0
    [[nodiscard]] std::int32_t dialogue_line_y(std::uint32_t window);   // 800347c0
    void link_text_packet(std::uint32_t table, std::uint32_t packet);   // 80031798
    void set_draw_mode(std::uint32_t packet, std::uint32_t tpage,
                       const std::array<std::int16_t, 4> &area); // 800454dc
    void dispatch(field::EventContext &context, std::uint8_t opcode,
                  const ProgramObserver &observe);
    void dispatch_extended(field::EventContext &context, std::uint8_t extended, SourcePoint point,
                           const ProgramObserver &observe);
    void script(field::EventContext &context,
                const std::function<void(field::FieldWorld &)> &handler);
    void field_pre_motion(std::size_t index);
    void field_actor_motion(std::size_t index);
    void field_animation(std::size_t index, std::int32_t animation);
    void select_animation(std::size_t index, std::int32_t animation);
    void camera_update(); // 80073230
    // Field 8009bf8c (primary FC) and its window opening chain.
    void show_message(field::EventContext &context);
    std::int32_t open_dialogue(field::FieldWorld &world, field::FieldPassState &pass,
                               std::uint32_t speaker, std::uint32_t mode);
    std::array<std::int32_t, 2> project_actor(std::size_t index, std::int16_t height);
    std::uint32_t disc_busy();          // Resident 800286cc
    void disc_wait(std::uint32_t once); // Resident 80028a60
    // A waiting loop polls again: run the next interrupt arrival, if any.
    bool deliver_interrupt();
    // Run the arrivals recorded at `point`, the original address of the code
    // they followed (see PlatformInput).
    void deliver_arrivals(std::uint32_t point);
    // Field main-loop steps (field_loop.cpp).
    std::int32_t loop_disc_busy();                                       // 80078bc8
    void clear_ordering_table(std::uint32_t table, std::uint32_t count); // 80044ad8
    void drain_pad();                                                    // 80074700
    void pointer_state();                                                // 8007ae78(1, 80065848)
    void record_play_state();                                            // 800a31e8
    void swap_draw_buffer();                                             // 80073fe0
    std::uint32_t file_size(std::int32_t file);                          // Resident 80028738
    std::uint32_t read_size(std::int32_t file);                          // Resident 80028808
    std::uint32_t file_bytes(std::int32_t file);                         // Resident 800288ec
    std::uint32_t allocate_disc_ring(std::uint32_t blocks, std::uint32_t mode); // Resident 8002a260
    std::int32_t read_setup(std::uint32_t file, std::uint32_t destination, std::uint32_t offset,
                            std::uint32_t mode);         // Resident 80029690
    std::int32_t select_ring(std::uint32_t destination); // 80029740..800297a4, 80029858..800298c4
    std::int32_t cd_control(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter);
    std::int32_t cd_command(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter,
                            bool nowait);         // 80042088
    std::int32_t cd_sync();                       // 80041b3c(0, 0)
    void cd_dma_callback(std::uint32_t function); // 800413ec
    // Interrupt context (interrupts.cpp, disc_read.cpp).
    void interrupt_handler(std::uint32_t address); // An 800578a8 entry
    void vsync_interrupt();                        // 8004bf78
    void vsync_update();                           // 8003634c
    void pad_update();                             // 800358bc
    void dma_interrupt();                          // 8004c098
    void dma_completed(std::uint32_t address);     // A 8005896c entry
    void spu_interrupt();                          // 8003bfa0
    void spu_transfer_completed();                 // 8004cb3c (sound_tick.cpp)
    void cd_interrupt();                           // 80042ca8
    std::uint32_t cd_getintr();                    // 800415b4
    void cd_poll();                                // 80041c80..80041d24
    void cd_callback(std::uint32_t address, std::uint8_t status,
                     const std::array<std::uint8_t, 8> &result);
    void disc_command_done(std::uint8_t status,
                           const std::array<std::uint8_t, 8> &result); // 8002a68c
    void disc_data(std::uint8_t status);                               // 8002b084
    void disc_ring_data(std::uint8_t status);                          // 8002b2f0
    void disc_list_data(std::uint8_t status);                          // 8002ac24
    void disc_data_failed(bool counted);                               // 8002b204 and its copies
    void disc_ring_transferred();                                      // 8002ba58
    void disc_continue(std::uint32_t file);                            // 8002a394
    void disc_image_transferred();                                     // 8002bb50
    // 8004c21c through 8004b7a0: set the DMA completion callback of `channel`.
    void set_dma_callback(std::uint32_t channel, std::uint32_t function);
    // libgpu (gpu_queue.cpp). A rectangle is x, y, width, height. Inside a
    // field frame `services` supplies the platform results; interrupt-side
    // calls (null services) read the recorded hardware loads.
    void gpu_alarm(FrameServices *services); // 80046efc
    void gpu_check_rect() const;             // 8004463c
    // 80044894; `address` is the rectangle's original address.
    std::int32_t load_image(std::array<std::int16_t, 4> &rect, std::uint32_t address,
                            std::uint32_t data, FrameServices *services = nullptr);
    // LoadImage of a rectangle in owned memory, clamped there.
    void load_image_at(FrameServices &services, std::uint32_t rect, std::uint32_t source);
    std::int32_t gpu_enqueue(std::uint32_t operation, std::uint32_t parameter,
                             std::array<std::int16_t, 4> *rect, std::uint32_t size,
                             std::uint32_t argument, FrameServices *services); // 8004668c
    std::uint32_t gpu_execute();                                               // 8004696c
    // Run a queued or immediate operation; `rect` is the rectangle parameter
    // when the caller holds it, else it lives in the queue at `parameter`.
    std::int32_t gpu_operation(std::uint32_t operation, std::uint32_t parameter,
                               std::array<std::int16_t, 4> *rect, std::uint32_t argument,
                               FrameServices *services);
    void clear_operation(std::uint32_t rect, std::uint32_t color,
                         FrameServices *services);                 // 80045e44
    void gpu_wait_ready(std::uint32_t first, std::uint32_t again); // GPUSTAT bit 26 poll
    void clear_image(FrameServices &services, std::uint32_t rect, std::uint32_t color); // 80044764
    void draw_otag(FrameServices &services, std::uint32_t table);                       // 80044bd0
    void put_draw_env(FrameServices &services, std::uint32_t environment);              // 80044c44
    void put_disp_env(std::uint32_t environment);                                       // 80044e9c
    void draw_sync(FrameServices &services);                                            // 800445d0
    // Reload steps (field_reload.cpp).
    void party_record(std::uint32_t slot); // 8009fee4
    // VSync(0) (8004b54c), then the arrivals since the previous stage.
    void vertical_sync(FrameServices &services);
    // StoreImage (800448f8) into owned bytes at `destination`, now or when
    // the request queue runs it.
    void store_image(FrameServices &services, std::array<std::int16_t, 4> &rect,
                     std::uint32_t address, std::uint32_t destination);
    void move_image(FrameServices &services, const std::array<std::int16_t, 4> &rect,
                    std::int32_t x, std::int32_t y);                           // 8004495c
    void save_screen_vram(FrameServices &services);                            // 800a915c
    void restore_screen_vram(FrameServices &services);                         // 800a91f0
    void load_text_palette(FrameServices &services, std::uint32_t caller);     // 80077544
    void copy_screen(FrameServices &services, std::int32_t x, std::int32_t y); // 800a476c
    void release_named_block();                                                // 8003748c
    void field_teardown(FrameServices &services);                              // 800700b0
    void reload_transition_setup();                                            // 800a663c(1, 1)
    void reload_present(FrameServices &services);                              // 800a6924
    void reload_screen_fade(FrameServices &services, std::uint32_t frame);     // 800a5884(1, 1)
    void start_field_stream();                                                 // 80070488
    // 80078c5c; `frame` is its stack frame (its entry SP - 20h).
    void brighten_text_strip(FrameServices &services, std::uint32_t frame);
    void prepare_party_sprites(); // 8001b044
    void decode_party_sprites();  // 8001b3a8
    // Field load steps (field_load.cpp).
    void store_original(std::uint32_t address, std::uint32_t value, std::size_t width);
    void identity_matrix(std::uint32_t address);                                   // 80070594
    void reset_field_state();                                                      // 800705dc
    void reset_camera();                                                           // 8007254c
    void set_quad_uv(std::uint32_t packet, const std::array<std::int32_t, 8> &uv); // 8007a44c
    void compass_record(std::uint32_t record, std::uint32_t column, std::uint32_t row,
                        std::uint32_t style); // 8007a7f4
    void compass_letters();                   // 8007a5c4
    std::uint32_t load_block(std::uint32_t size, std::uint32_t mode, std::uint32_t site);
    [[nodiscard]] std::uint8_t ram_byte(std::uint32_t address);
    void decode_component(std::uint32_t index, std::uint32_t destination); // 8007008c
    void load_tim_images(FrameServices &services, std::uint32_t tim);      // 800771f8
    void load_images_across(FrameServices &services, std::uint32_t data, std::uint32_t x,
                            std::uint32_t y, std::uint32_t frame);         // 80022a70
    void relocate_model_group(std::uint32_t group);                        // 8002c3e8
    void set_field_light(std::uint32_t index, std::uint32_t light);        // 80030a30
    void setup_field_view(std::uint32_t view);                             // 8006fdec
    void build_model_instance(std::uint32_t instance, std::uint32_t mode); // 8002cb54, 8002c8cc
    void trim_model(std::uint32_t model);                                  // 8002c644
    void build_shadow(std::uint32_t shadow);                               // 8007aa44
    void create_field_actor(std::uint32_t index);                          // 80080f44
    void load_descriptors();                                               // 80071318..800715a0
    void draw_mode_packet(std::uint32_t packet, std::uint32_t tpage,
                          const std::array<std::int16_t, 4> *area); // 800454dc
    void init_dialogue_packets(std::uint32_t w);                    // 8007ee0c
    void init_dialogue();                                           // 8007decc
    std::vector<std::uint8_t> take_contents(std::uint32_t address, std::uint32_t size);
    void adopt_loaded_field(const std::array<std::uint32_t, 9> &sizes);
    void init_field_events(const ProgramObserver &observe);    // 800a28d4
    void restore_field_events(const ProgramObserver &observe); // 800a28d4 after a return
    void restore_actor_data(const ProgramObserver &observe);   // 800a2714
    void scale_actor_rotation(std::uint32_t index);            // 80072254
    void finish_field_load(const ProgramObserver &observe);    // 80071770..80071a5c
    void prepare_model_instances();                            // 80073e38
    void place_party_at_leader();                              // 80077268
    void read_map_ahead();                                     // 800777dc
    void init_display(FrameServices &services);                // 80071fb0
    void run_actor0_script(std::uint32_t entry, const ProgramObserver &observe); // 800a22ac
    void adjust_after_return(const ProgramObserver &observe);                    // 800a24c4
    void release_cached_sequence();                                              // 80085eec
    // Event initialization and actor setup (field_init.cpp).
    void create_actor_sprite(std::size_t index,
                             const field::FieldSpriteArguments &arguments); // 80076ac0
    void sync_actor_position(std::size_t index);                            // 800a0c94
    [[nodiscard]] std::uint32_t bundle_sprite(std::uint32_t slot);
    void place_at_entry(std::size_t index, std::int32_t entry); // 8009fa54
    void event_default_sprite(field::EventContext &context);    // Primary bc
    void event_bundle_sprite(field::EventContext &context);     // Primary 0b
    void event_party_member(field::EventContext &context);      // Primary 16
    void event_place_height(field::EventContext &context);      // Primary 1d
    void event_descriptor_hidden(field::EventContext &context); // Primary 23
    void event_stop_actor(field::EventContext &context);        // Primary 27
    void event_camera_bounds(field::EventContext &context);     // Primary e6
    void event_branch_below(field::EventContext &context);      // Primary 85
    void event_face_direction(field::EventContext &context);    // Primary 69
    void event_encounter_table(field::EventContext &context);   // Primary f7
    // Extended actor-setup instructions; false when `extended` is not one.
    bool event_setup_extended(field::EventContext &context, std::uint8_t extended);
    [[nodiscard]] std::int32_t party_character(std::int32_t selector) const; // 8008cf3c
    void reset_graph(std::uint32_t mode);                                    // 80044110 ResetGraph
    void destroy_sprite_tasks();                                             // 8001c8dc
    void flush_sprite_uploads(FrameServices &services);                      // 80025044
    // Owned bytes from `address` to the end of their owner, whatever value
    // owns them (records, regions, resources, loaded components, allocated
    // heap contents); empty when none does.
    [[nodiscard]] std::span<std::uint8_t> owned_span(std::uint32_t address);
    // Release the allocated heap block at `address` (800320e8 at `site`)
    // with the owned bytes it holds; returns its size (zero if kept).
    std::uint32_t release_owned_block(std::uint32_t address, std::uint32_t site);
    void release_actor(std::uint32_t index);                       // 8008083c
    void cd_get_sector(std::uint32_t buffer, std::uint32_t words); // 800413ac / 80042aa8
    // RAM that DMA fills: owned globals, else a disc transfer block.
    void dma_store(std::uint32_t address, std::span<const std::uint8_t> bytes);
    // A register only software changes: its last recorded write, else the
    // observed I/O page.
    [[nodiscard]] std::uint32_t io_latch(std::uint32_t address, std::uint32_t width) const;
    void io_write(std::uint32_t address, std::uint32_t value, std::uint32_t width);
    std::int32_t disc_idle_query(); // Field 8008a558
    // Field 8008f76c (primary 75) and 8008f724 (72): 8008f7b8 with the
    // start parameter (8004f340) -1 or 0.
    void change_music(field::EventContext &context, std::uint32_t start_parameter);
    void load_music(std::uint32_t id); // Field 80085b20
    void release_shared_wave();        // Field 80086024
    // The music load's calls (field_music.cpp) and the resident sound calls
    // they reach (sound_load.cpp, sound_tick.cpp).
    class Music;
    void release_music_buffer(std::uint32_t address, std::uint32_t call_site);   // 800320e8
    std::uint32_t continue_wave_upload(std::uint32_t data, std::uint32_t bytes); // 8003827c
    void spu_transfer(std::uint32_t spu, std::uint32_t ram, std::uint32_t size,
                      std::uint32_t callback);                      // 8003bc10
    std::int32_t sound_wait(std::uint32_t flags);                   // 8003bdfc
    void spu_reverb_depth(std::uint16_t left, std::uint16_t right); // 8004e574
    void sequence_volume(std::uint32_t sequence, std::uint32_t volume,
                         std::uint32_t frames); // 8003a89c
    // Map changes: field 800932d0 (primary 98), 8009744c and 8009a514.
    void request_map_change(field::FieldWorld &world);
    [[nodiscard]] std::int32_t facing_octant() const;
    [[nodiscard]] std::int32_t camera_heading_octant() const;
    [[nodiscard]] std::uint8_t overlay_byte(std::uint32_t address) const;
    // Field 80085634 (extended 65): stop the effect voice pair of `channel`
    // and, for a nonzero id, start that effect at full volume, centered.
    void play_sound_effect(std::uint32_t id, std::uint32_t channel);
    void field_history(std::size_t index);
    std::int32_t field_position(std::size_t index, std::int32_t linked_floor,
                                std::uint32_t link_status);
    void field_contact(std::size_t index);
    void field_interactions(std::size_t index);
    [[nodiscard]] bool rectangle_contains(std::size_t index, std::int32_t x, std::int32_t z,
                                          std::int32_t margin) const;
    void field_followers();
    void snap_actor(std::size_t index, std::int32_t x, std::int32_t z);
    std::int32_t gather_member(std::size_t slot, std::int32_t x, std::int32_t z,
                               std::uint32_t facing);
    void party_gather(field::FieldWorld &world);
    struct PolygonHit {
        std::int32_t floor;
        field::FieldVector normal;
    };
    // Leaves the composed model transform loaded in resident.gte.transform, as the original.
    [[nodiscard]] std::optional<PolygonHit> polygon_contact(std::size_t index, std::int32_t x,
                                                            std::int32_t z);
    // Resident sprite routines on an actor's descriptor sprite with the owned
    // resources, frames and tables.
    void
    sprite_call(std::size_t index,
                const std::function<void(field::SpriteWindow, const field::SpriteSources &)> &call);
};

} // namespace xem::reconstruction
