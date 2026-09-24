#pragma once

#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/disc_stream.hpp"
#include "xem/reconstruction/field_control.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_return.hpp"
#include "xem/reconstruction/field_script.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/field_sprite_model.hpp"
#include "xem/reconstruction/interrupts.hpp"
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace xem::reconstruction {

inline constexpr std::uint32_t game_data_bytes = 0x2358;
// Load address of the field overlay image.
inline constexpr std::uint32_t field_overlay_base = 0x8006faf0;

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
    // Ring storage, one 16-entry halfword array per entry field: 8005a0fc,
    // 8005a11c, 8005a13c, 8005a15c, 8005a17c, 8005a19c.
    std::array<std::array<std::uint16_t, 16>, 6> ring{};
    // 80035db0.
    void reset() { *this = {.w50200 = 1, .ring = ring}; }
};

// Per-VSync controller and clock state (VSync callback 8003634c and callees
// 800358bc, 80035e44, 80036220). The pad buffers are the BIOS pad driver's
// receive buffers: platform input, read only.
struct PadState {
    std::uint32_t vsyncs{};                                // 80059488: VSync callbacks run
    std::uint32_t hook{};                                  // 800501fc: optional per-VSync call
    std::uint32_t debugger{};                              // 80059390
    std::uint32_t text_word{};                             // 80010000, compared with -1
    std::uint8_t type{};                                   // 80059388: last controller type
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

// libgpu request queue (enqueue 8004668c, execute 8004696c) and the image
// operations the interrupt side reaches (LoadImage 80044894 / 800460a0,
// StoreImage 800462dc, DrawOTag 800465ec).
struct GpuState {
    std::uint32_t services{};                  // 800568c8: service table address
    std::array<std::uint32_t, 12> functions{}; // 80056888: the service table
    std::uint8_t queued{};                     // 800568d1: zero runs requests at once
    std::uint8_t debug{};                      // 800568d2: request checking level
    std::int16_t width{};                      // 800568d4: VRAM width
    std::int16_t height{};                     // 800568d6: VRAM height
    std::uint32_t sync_pending{};              // 800568d8
    std::uint32_t sync_callback{};             // 800568dc: DrawSync callback
    // 800569a0 GP0, 800569a4 GP1/GPUSTAT, 800569a8 DMA2 address, 800569ac
    // DMA2 block, 800569b0 DMA2 control.
    std::array<std::uint32_t, 5> registers{};
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
    // Image data of queued LoadImage requests: caller memory the queue
    // refers to. Read-only input, attached by the host.
    std::vector<resident::HeapBlock> sources;
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
    std::uint32_t w_fe00{};                 // 8004fe00
    std::uint32_t w_fe3c{};                 // 8004fe3c
    std::uint32_t w_fde0{};                 // 8004fde0
    std::array<std::uint32_t, 3> skipped{}; // 8004fde4, 8004fde8, 8004fdec: unexpected sectors
    std::uint32_t saved_callback{};         // 80059f08: callback 80040fcc replaced
    std::array<std::uint8_t, 2> b_59f14{};  // 80059f14
    std::array<std::uint32_t, 2> w_5a48c{}; // 8005a48c, 8005a490
    std::array<std::uint32_t, 2> w_5a494{}; // 8005a494, 8005a498
    std::array<std::uint32_t, 2> w_5a4a4{}; // 8005a4a4, 8005a4a8
    std::uint32_t w_5a4b4{};                // 8005a4b4
    // The list a list read walks (8004fe0c): halfword file, word destination
    // per eight-byte entry. Read-only input; empty unless attached.
    resident::HeapBlock list;
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
    std::uint32_t field_return_mode{};
    // Original snapshot and original resource identities, not native persistence.
    // The resident snapshot storage is at 8005a4e4 (0x3804 bytes).
    field::original::Bytes field_snapshot;
    std::array<std::uint32_t, 3> saved_party_modes{}; // 8005a408, written with a snapshot
    std::uint32_t game_state{};                       // 8005a39c: resident game-state pointer
    // Persistent game data at *8005a39c: the 2358-byte block resident 8001b9d8
    // initializes for a new game (empty before boot allocates it).
    std::vector<std::uint8_t> game_data;
    [[nodiscard]] std::array<std::uint8_t, 3> party_modes() const; // game data + 22b1
    std::vector<std::uint32_t> party_sprite_resources;
    field::MathTables math;
    InputQueue input_queue;
    // Loaded GTE rotation/translation (control registers 0-7). Machine state
    // that survives calls: PushMatrix stores it to memory.
    field::GteMatrix gte{};
    field::GteScreen gte_screen{}; // GTE OFX/OFY/H (control 24-26)
    std::uint8_t gpu_type{};       // 800568d0: libgpu draw-mode encoding
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
    std::uint32_t cd_sync_deadline{};           // 8005a228
    std::uint32_t cd_sync_polls{};              // 8005a22c
    std::uint32_t cd_sync_label{};              // 8005a230: diagnostic string for a timeout
    std::uint32_t cd_dma_register{};            // 800567b4: address of DMA3 CHCR
    std::array<std::uint16_t, 2> text_cluts{};  // 800595d4 (even rows), 80059414 (odd rows)
    field::MatrixStack matrix_stack{};          // 80056d2c depth, 80056d30 records
    InterruptState interrupts;
    PadState pad;
    GpuState gpu;
    // RAM that disc DMA filled and no other Program value owns (file
    // destinations, the sector tail buffer 800596f8), by address.
    std::vector<resident::HeapBlock> disc_transfers;
    // Platform inputs, consumed in order; see interrupts.hpp.
    std::deque<PlatformInput> platform;
    DiscDrive drive;
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
    // Resident 800295d8: start reading `file` of the selected directory into
    // `destination`; returns 0, or -3 (no such file) and -4 (empty ring).
    // Waiting for an earlier read, host-file reads and CD waits that need an
    // interrupt stop with MissingDependency.
    std::int32_t read_file(std::int32_t file, std::uint32_t destination, std::uint32_t offset,
                           std::uint32_t mode);
    // Resident 80028470: select directory `base + index`; -1 when it is empty.
    std::int32_t select_directory(std::uint32_t base, std::uint32_t index);
    // Resident 8004b9b4, entered from the BIOS exception hook: serve every
    // pending interrupt the dispatcher enables, until none is pending.
    // Asynchronous register reads come from resident.platform.
    void interrupt_dispatch();
    // Resident 8003c028, the sound driver tick; `event` is V0 at entry (the
    // driver flags the event handler loaded). Returns 0.
    std::uint32_t sound_tick(std::uint32_t event);
    // Resident 8001b66c: stop the playing sequence and forget the loaded pair.
    void stop_music();
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
    // Post-battle 801e2794: victory rewards and write-back.
    void grant_battle_rewards();
    // Post-battle 801e2280 up to 801e23d4: experience pool and gold.
    void total_battle_rewards();
    // Post-battle 801e1444: add drops to the inventory.
    void add_battle_drops(std::uint32_t ids, std::uint32_t counts, std::uint32_t categories);

  private:
    void dispatch(field::EventContext &context, std::uint8_t opcode,
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
    std::uint32_t file_size(std::int32_t file); // Resident 80028738
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
    // libgpu (gpu_queue.cpp). A rectangle is x, y, width, height.
    // `address` is the rectangle's original (stack) address.
    std::int32_t load_image(std::array<std::int16_t, 4> &rect, std::uint32_t address,
                            std::uint32_t data); // 80044894
    std::int32_t gpu_enqueue(std::uint32_t operation, std::array<std::int16_t, 4> &rect,
                             std::uint32_t address, std::uint32_t size,
                             std::uint32_t argument); // 8004668c
    std::uint32_t gpu_execute();                      // 8004696c
    // Run a queued or immediate operation; `rect` is the rectangle parameter
    // when the caller owns it, else it lives in the queue at `parameter`.
    std::int32_t gpu_operation(std::uint32_t operation, std::uint32_t parameter,
                               std::array<std::int16_t, 4> *rect, std::uint32_t argument);
    void gpu_wait_ready(std::uint32_t first, std::uint32_t again); // GPUSTAT bit 26 poll
    [[nodiscard]] std::uint32_t ram_word(std::uint32_t address) const;
    void cd_get_sector(std::uint32_t buffer, std::uint32_t words); // 800413ac / 80042aa8
    // RAM that DMA fills: owned globals, else a disc transfer block.
    void dma_store(std::uint32_t address, std::span<const std::uint8_t> bytes);
    // A register only software changes: its last recorded write, else the
    // observed I/O page.
    [[nodiscard]] std::uint32_t io_latch(std::uint32_t address, std::uint32_t width) const;
    void io_write(std::uint32_t address, std::uint32_t value, std::uint32_t width);
    std::int32_t disc_idle_query();                  // Field 8008a558
    void change_music(field::EventContext &context); // Field 8008f76c (primary 75)
    void load_music(std::uint32_t id);               // Field 80085b20
    void release_shared_wave();                      // Field 80086024
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
    // Leaves the composed model transform loaded in resident.gte, as the original.
    [[nodiscard]] std::optional<PolygonHit> polygon_contact(std::size_t index, std::int32_t x,
                                                            std::int32_t z);
    // Resident sprite routines on an actor's descriptor sprite with the owned
    // resources, frames and tables.
    void
    sprite_call(std::size_t index,
                const std::function<void(field::SpriteWindow, const field::SpriteSources &)> &call);
};

} // namespace xem::reconstruction
