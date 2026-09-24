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
#include "xem/reconstruction/menu.hpp"
#include "xem/reconstruction/menu_save.hpp"
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
    // 80035db0.
    void reset() { *this = {.w50200 = 1}; }
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
    // Stream reads (80029eb0): six halfword parameters at 80059f24 + 4 * i,
    // then state cleared per stream: 80059f3c, halfwords 80059f40/44/48 and
    // words 80059f4c/50.
    std::array<std::uint16_t, 6> h_59f24{};
    std::uint32_t w_59f3c{};
    std::array<std::uint16_t, 3> h_59f40{};
    std::array<std::uint32_t, 2> w_59f4c{};
    std::uint16_t h_59f60{};  // 80059f60
    std::uint32_t requests{}; // 8005a488: CD reads issued
    std::uint32_t w_5a4dc{};  // 8005a4dc
    // The tables at 8004fdf0 (8000 bytes) and 8004fdf4 (7a bytes), the sizes
    // 80028230 reads them with; empty before they are loaded.
    std::vector<std::uint8_t> files;
    std::vector<std::uint8_t> directories;
    // Header of the ring a stream read selects: count, eight-byte slots, then
    // the 24-byte tail before the payload (0x24 + 8 * count bytes).
    resident::HeapBlock ring;
    // The caller's list a list read (80029afc) sorts in place and keeps
    // (8004fe0c): 8-byte entries of u16 file, u16 unused, u32 destination,
    // then the halfword of the terminating zero file.
    resident::HeapBlock list;
};

// CD library state reached by read setup: CdControl 8004111c, the command
// writer 80042088, the sync wait 80041b3c and the DMA callback 8004c21c.
struct CdState {
    std::uint32_t ready_callback{};         // 800564a8
    std::uint32_t sync_callback{};          // 800564ac
    std::int32_t debug{};                   // 800564b4: nonzero levels print
    std::uint8_t status{};                  // 800564b8: last drive status
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
    // 800adb70 is also the movie request: extended 60 sets it and the field
    // loop plays the movie (800a7c58) and clears it.
    field::MovieState movie{};
    std::uint32_t exit_mode{}; // 800b0064: bits 0-6 the next game mode, bit 80 calls 8001bb50
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
    // Resident 8001b66c: stop the playing sequence and forget the loaded pair.
    void stop_music();
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
    std::uint32_t disc_busy();                  // Resident 800286cc
    void disc_wait(std::uint32_t once);         // Resident 80028a60
    std::uint32_t file_size(std::int32_t file); // Resident 80028738
    std::uint32_t read_size(std::int32_t file); // Resident 80028808
    void seek_file(std::int32_t file);          // Resident 8002a394
    std::int32_t read_setup(std::uint32_t file, std::uint32_t destination, std::uint32_t offset,
                            std::uint32_t mode);         // Resident 80029690
    std::int32_t select_ring(std::uint32_t destination); // 80029740..800297a4, 80029858..800298c4
    std::int32_t cd_control(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter);
    std::int32_t cd_command(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter,
                            bool nowait);            // 80042088
    std::int32_t cd_sync();                          // 80041b3c(0, 0)
    void cd_dma_callback(std::uint32_t function);    // 800413ec
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
