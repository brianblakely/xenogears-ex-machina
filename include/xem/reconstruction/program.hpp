#pragma once

#include "xem/reconstruction/call_state.hpp"
#include "xem/reconstruction/disc_state.hpp"
#include "xem/reconstruction/field_state.hpp"
#include "xem/reconstruction/gpu_state.hpp"
#include "xem/reconstruction/input_state.hpp"
#include "xem/reconstruction/interrupt_state.hpp"
#include "xem/reconstruction/resident_state.hpp"
#include "xem/reconstruction/source_point.hpp"

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

namespace menu {
class CardBios;
}
namespace field {
struct ViewLocals;
}

inline constexpr std::uint32_t game_data_bytes = 0x2358;
// The resident block the game state pointer (8005a39c) names once the field
// mode's start (80077e88) or boot publishes it.
inline constexpr std::uint32_t game_data_block = 0x8006d634;
// Load address of the field overlay image.
inline constexpr std::uint32_t field_overlay_base = 0x8006faf0;
// The field main loop 80077e88's stack pointer (the SP at its calls of
// 8007554c and 800a5c40): the bottom of the stack its callees' frames grow from.
inline constexpr std::uint32_t field_loop_stack = 0x801fffc8;

inline constexpr std::string_view resident_executable_sha256 =
    "dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119";

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
    // The codec a movie the field plays decodes with (movie.hpp).
    movie::MdecCodec *mdec{};
    // BIOS services reached by a field menu (including its critical section).
    menu::CardBios *menu_card{};
    // BIOS FlushCache reached after EnterCriticalSection, before Exit.
    std::function<void()> flush_instruction_cache;
};

// A platform result the host did not supply: invalid input, not a game result.
class ServiceUnavailable : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
// Consumes the next result of one service queue.
std::uint32_t take_service(std::deque<std::uint32_t> &results, const char *what);

// 80041430 CdIntToPos: a logical sector as BCD minute, second and sector
// after the 150 lead-in sectors (byte 3 zero); 80041534 CdPosToInt back.
[[nodiscard]] std::array<std::uint8_t, 4> cd_position(std::uint32_t sector);
[[nodiscard]] std::uint32_t cd_sector(const std::array<std::uint8_t, 4> &location);

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
namespace menu {
class Overlay;
}
// Resumable points of the mode dispatcher 80019acc(0).
enum class DispatchStep : std::uint8_t {
    start,  // entry: ResetGraph, DrawSync, VSync(2)
    heap,   // 80019b3c: the heap restart, BSS clear and the mode block load
    wait,   // 80019b80: the disc wait, then the decode
    sync,   // 80019b98: after the decode: DrawSync, VSync, FlushCache
    reinit, // 80019bdc: the second heap restart up to the row call
};
// Read-only observation. Hosts may interrupt at a boundary; no callback supplies
// a computed game result. References expire when the callback returns.
using ProgramObserver = std::function<void(const Program &, SourcePoint, bool completed)>;

// What the battle's start draws and the reconstruction does not: each is
// observed where the original runs it (battle_start.cpp).
enum class BattleStartPresentation : std::uint8_t {
    swirl_capture, // 800b7870: the screen through a 30000h block (StoreImage
                   // 800448f8, bit 15 on each pixel, LoadImage 80044894)
    swirl_open,    // 800b7424: the swirl block's geometry
    swirl_draw,    // 800b6f0c, 800b7160: one frame of the swirl's primitives
    swirl_show,    // PutDrawEnv, PutDispEnv and DrawOTag of the frame
    camera,        // 800bc404: frame the camera (camera state and GTE only)
};
using BattleStartPresent = std::function<void(BattleStartPresentation)>;

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
    std::optional<MenuCallState> menu_call_state;

    void qualify_menu_call(std::uint32_t entry_sp, std::uint32_t stack_base,
                           std::span<const std::uint8_t> stack,
                           std::span<const std::uint32_t> saved_registers,
                           std::uint32_t frame_pointer, std::uint32_t return_address);
    // Field 800799d4: menu loading, resident initialization, the shared menu
    // overlay and field restoration, keeping the live field and heap.
    void field_menu(FrameServices &services, std::uint32_t entry_sp,
                    const ProgramObserver &observe = {});
    // Resident 8001c634, with the source frame at entry_sp - 18h.
    void initialize_field_menu(FrameServices &services, std::uint32_t entry_sp,
                               const ProgramObserver &observe = {});
    // Mode 6 (the movie mode, movie_mode.cpp) while its overlay is loaded:
    // the overlay image with its statics (8006faf0..80077458) and the
    // player's stack frame (80076488), whose rectangle ClearImage reads.
    struct MovieModeMemory {
        resident::HeapBlock overlay;
        resident::HeapBlock frame;
    };
    std::optional<MovieModeMemory> movie_mode_memory;

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
    void field_move(const ProgramObserver &observe = {}, std::optional<FrameCallAbi> caller = {});
    // Field overlay 8007554c: one field frame (move phase, drawing, buffer
    // presentation and the frame-rate wait). Services supply platform timing
    // and GPU status results; see FrameServices.
    void field_frame(FrameServices &services, const ProgramObserver &observe = {},
                     FrameStep from = FrameStep::start, std::optional<FrameCallAbi> caller = {});
    // Field main loop 80077e88 from a frame's return (800782e4) up to the
    // call of the next frame (800782dc): 800a5924, the exit, map-change and
    // menu checks, 80078b5c, the pause and reset checks, then 80077dac
    // (buffer swap, ordering tables, the pad drain 80074700 and 800a31e8).
    // `services` supplies 80077dac's VSync(1). Branches whose callees are not
    // recovered stop with MissingDependency.
    // Returns false where the loop leaves the field for battle mode: the
    // original then calls the mode dispatcher 80019acc(0) (8007954c).
    bool field_between_frames(FrameServices &services, const ProgramObserver &observe = {});
    // One main-loop iteration: the code between frames, then the frame;
    // false (and no frame) when the loop left the field.
    bool field_loop_step(FrameServices &services, const ProgramObserver &observe = {});
    // The loop's battle branch (80078334..80078494): true when it leaves
    // (80078abc), false after the battle music's first step (800adbd0 1).
    bool field_battle_start();
    // Field 800700b0: reset the GPU, destroy the sprite tasks, flush both
    // sprite buffers and release the field's actors, models and components.
    void field_teardown(FrameServices &services);
    // 80078abc up to the teardown 800700b0 (field_battle_exit.cpp).
    void field_battle_leave(FrameServices &services);
    // 80078b04..80078b2c after the teardown; `block` is *800adb30.
    void field_battle_release(std::uint32_t block);
    // 80078abc..80078b34: leave, tear down, release and exit (8007954c(0));
    // true where the original calls the mode dispatcher 80019acc(0).
    bool leave_field_for_battle(FrameServices &services, const ProgramObserver &observe = {});
    // Field 800a3f4c: save the field-return snapshot.
    void save_field_return();
    // 80078d44, the field entry, from its caller's stack frame (the main
    // loop's SP - 30h), then the main loop up to its first frame.
    void field_entry(FrameServices &services, std::uint32_t frame,
                     const ProgramObserver &observe = {});
    void field_loop_start(FrameServices &services, const ProgramObserver &observe = {});
    // 80077e88..80078154: the field mode's start before its entry.
    void field_mode_start(FrameServices &services, const ProgramObserver &observe = {});
    void init_pointer(); // 80071ee8
    void field_loop_top(FrameServices &services, const ProgramObserver &observe = {});
    void show_reassigned_party(); // 800ad898
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
    // Resident 8002a260: a heap block of `blocks` stream sectors that becomes
    // the owned disc ring (disc_read.ring and ring_payload); 0 when none.
    std::uint32_t allocate_stream_ring(std::uint32_t blocks, std::uint32_t mode);
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
    // Deliver the unpositioned interrupt arrivals at the front of the
    // platform input: a call's code about to make a recorded hardware read
    // that the original made after them. True if any was delivered.
    bool deliver_leading_arrivals();
    // The same for leading arrivals that serve the vertical blank alone: a
    // call-level VSync(-1) read counts them.
    void deliver_leading_vblanks();
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
    // Resident mode dispatcher 80019acc(0) (mode_dispatch.cpp) from `from` up
    // to its call of the next mode's function (mode table 8001808c); returns
    // that function. Observed boundaries: mode_heap (the first heap restart
    // 80031b10), mode_loaded (800199cc returned), mode_decode (80032eb4 is
    // called), mode_reinit (the second 80031b10) and mode_row (the call).
    // The battle overlay's BSS and decoded image become Program::battle.
    std::uint32_t mode_dispatch(FrameServices &services, DispatchStep from,
                                const ProgramObserver &observe = {});
    // Resident 8001b6c4 (battle mode) up to its call of 80070f40: the disc
    // wait, directory 12 and the graphics setup 8001b844.
    void battle_mode_start();
    // Resident libgpu/libgte setup calls (graphics_setup.cpp). Environments
    // are addressed as the original addresses them: battle memory when it
    // holds them, else other owned memory.
    void set_default_display_environment(std::uint32_t environment, std::int32_t x, std::int32_t y,
                                         std::int32_t width,
                                         std::int32_t height); // SetDefDispEnv 800439e0
    void set_default_draw_environment(std::uint32_t environment, std::int32_t x, std::int32_t y,
                                      std::int32_t width,
                                      std::int32_t height); // SetDefDrawEnv 80043928
    // InitGeom 80048bc4 called from `return_address - 8`.
    void init_geometry(std::uint32_t return_address);
    void set_geometry_offset(std::int32_t x, std::int32_t y); // SetGeomOffset 8004a12c
    void set_geometry_screen(std::int32_t h);                 // SetGeomScreen 8004a14c
    // 8001b94c: a battle draw environment's dither, background and colour.
    void set_battle_draw_modes(std::uint32_t environment);
    void set_display_mask(std::uint32_t mask); // SetDispMask 80044534
    // Field extended event handler that the FE handler's table reaches for
    // actor `index`, whose working PC is the extended byte.
    void event_extended(std::size_t index, const ProgramObserver &observe = {});
    // Field 800a7f78: whether the movie loop drains the pad before deciding.
    [[nodiscard]] field::MoviePad movie_pad() const;
    // Field 800a7f78..800a80b0 once that drain has run: the loop's decision.
    // A skip applies the CD fade and the five waits before returning.
    field::MovieStep movie_decision(field::MovieServices &services);
    // The movie library at 801d3000 (movie.hpp, movie.cpp, movie_stream.cpp).
    // 801d3538: open it for a `width` x `height` movie; 0, or -1 without a ring.
    std::int32_t movie_open(std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                            std::uint32_t slice, std::uint32_t sectors, std::uint32_t limit,
                            std::uint32_t mode);
    // 801d37cc's arguments: the movie file of the selected directory, its
    // first sector, first and last frames, CD-XA channel, select bits (1: XA
    // audio of `channel`, 2: no real-time audio), hold (loop at the end),
    // the display buffers' corners (x, y of each), the most rows a slice
    // loads and the frame callback.
    struct MovieStart {
        std::uint32_t file{};
        std::uint32_t sector{};
        std::uint32_t first_frame{};
        std::uint32_t last_frame{};
        std::uint32_t channel{};
        std::uint32_t select{};
        std::uint32_t hold{};
        std::array<std::uint32_t, 4> area{};
        std::uint32_t rows{};
        std::uint32_t callback{};
    };
    void movie_start(const MovieStart &start);
    // 801d3f7c: one decode step; the frame's variable-length decode is the
    // codec service.
    void movie_poll(movie::MdecCodec &codec);
    void movie_close(); // 801d43b0
    // Interrupt-context callbacks of the library.
    void movie_slice_decoded();   // 801d30c4: MDEC output DMA done
    void stream_interrupt();      // 801d5900 -> 801d5d54: a stream sector
    void stream_frame_complete(); // 801d5a04: a frame's last sector transferred
    // The field movie player 800a7c58 (field_movie_player.cpp) and its
    // stages. `frame` is the player's stack frame (its entry SP - 28h).
    void play_movie(FrameServices &services, std::uint32_t frame, movie::MdecCodec &codec,
                    const ProgramObserver &observe = {}, std::optional<FrameCallAbi> caller = {});
    void movie_prepare(FrameServices &services, std::uint32_t frame,
                       const ProgramObserver &observe = {},
                       std::optional<FrameCallAbi> caller = {});
    void movie_first_frame(FrameServices &services, movie::MdecCodec &codec);
    [[nodiscard]] field::MovieStep movie_pass(FrameServices &services, movie::MdecCodec &codec,
                                              const ProgramObserver &observe = {},
                                              std::optional<FrameCallAbi> caller = {});
    void movie_finish(FrameServices &services, std::uint32_t frame,
                      const ProgramObserver &observe = {});
    void movie_open_display();                                             // 800a708c
    void movie_start_request();                                            // 800a7218
    void movie_decode_steps(std::uint32_t count, movie::MdecCodec &codec); // 800a732c
    // Mode 6, the movie mode (movie_mode.cpp): 800737ec plays the movie the
    // request bytes 8004fe44..47 name, then selects the next mode (8004fe46).
    // The mode dispatcher it enters last (80019acc) is not reconstructed;
    // the call returns before it. `frame` is 80076488's stack frame (its
    // entry SP - 308h). The observer sees "movie_mode_head" at each pass of
    // the player loop (8007670c).
    void movie_mode(FrameServices &services, movie::MdecCodec &codec, std::uint32_t frame,
                    const ProgramObserver &observe = {});
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
    void field_reload_fade_in(FrameServices &services, const ProgramObserver &observe = {},
                              std::optional<std::uint32_t> frame = {});
    // One pass of that loop (800a6148..800a61c8): `shade` is the quads'
    // 8.16 color, returned for the next pass.
    std::int32_t field_reload_fade_frame(FrameServices &services, std::int32_t shade,
                                         const ProgramObserver &observe = {},
                                         std::optional<FrameCallAbi> caller = {});
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
    // Setup module 801e5840: one phase of the setup the intro swirl 800b7870
    // runs: 0 the party records, archive contents, VRAM uploads and the
    // enemy data read (801e5384), 1 participants and formation, 2 items and
    // turns (battle.hpp). `services` supplies the uploads' platform results;
    // `stack` is the stack pointer at 801e5840, below which the original's
    // callees keep the rectangles they pass to LoadImage.
    void setup_battle_phase(std::uint32_t phase, FrameServices &services, std::uint32_t stack);
    // Battle 80070f40 up to its call of 800b8098: the turn, UI and graphics
    // state blocks (800c3eac, 800d2d28, 800c3ea4, through 8008abb8), the menu
    // input and music globals, then the formation record 8006f9dc from the
    // field's formation table (800658dc) at the selector 80059508. The event
    // battle (8005947c), the post-battle module (800594f8) and debug paths
    // stop with MissingDependency.
    void battle_prologue();
    // 80071168..80071184: the alive mask (8007252c), then 800c3e4c = 2.
    void battle_after_load();
    // 800b8190..800b81b4: 801e7210's result into 800c4a38 and 800a5e9c.
    void battle_after_scene(std::uint32_t result);
    // 8009892c: the party adjustments (battle.hpp).
    void battle_adjust_party();
    // 8001bb0c (800379d8): allocate the formation scene's stage and scene
    // data files and start their list read.
    void battle_scene_files();
    // 8001bbac: the setup files (directory 12 files 2-4) by a list read, the
    // effect bank once its file has arrived, and the members' effects.
    void battle_setup_files();
    // 800b8098 up to 8001bbac: the mode (800d36b8) and 800b8284 (geometry
    // offset, both display and draw environments).
    void battle_load_prologue(std::uint32_t mode);
    // 800b8098 after the swirl up to 801e7210: the disc wait (80028a60) and
    // 800a8b0c (effect globals and lists).
    void battle_effect_lists();
    // 80071278 up to 8009892c: release the setup's effect bank, marker and
    // setup module blocks once the setup frames end.
    void battle_release_setup();
    // 800b81bc: the battle renderer and task setup before the main loop;
    // the setup module's task keeps `task_argument` (A0).
    void battle_renderer_setup(std::uint32_t task_argument);
    // Setup module 801e7210: the stage model, the scene data's new block and
    // its lights (battle_setup.cpp). `stack` is the stack pointer at the
    // call; `origin`, `colors` and `tint` are its fourth to sixth arguments
    // (800ccb94, 800ccbb4, 800c4a39). Returns the scene data's +35e.
    std::uint32_t battle_stage_setup(FrameServices &services, std::uint32_t stack,
                                     std::uint32_t stage, std::uint32_t origin,
                                     std::uint32_t colors, std::uint32_t tint);
    // One battle frame's run of the loading task `node` (battle_loader.cpp):
    // its state 801e6fec, 801e6f00, 801e6e48, 801e6d6c, 801e6d34 or 801e6c80.
    void battle_loader_step(std::uint32_t node, FrameServices &services);
    // 80071310 up to the main loop's first 800723e0: the party positions.
    void battle_place_party();
    // Battle 80070f40 up to its first call of the turn procedure 800723e0,
    // the steps above connected (battle_start.cpp). `stack` is the stack
    // pointer at 80070f40; `present` observes what the start draws.
    void battle_start(FrameServices &services, std::uint32_t stack,
                      const BattleStartPresent &present, const ProgramObserver &observe);
    // 800b8098: the load, from 800b8284 to the stage's result.
    void battle_load(std::uint32_t mode, FrameServices &services, std::uint32_t stack,
                     const BattleStartPresent &present, const ProgramObserver &observe);
    // 800b7870: the intro swirl, which runs the setup phases and the scene
    // files as the disc allows. `stack` is the stack pointer in 800b7870's
    // body (the phases' 801e5840 stack pointer).
    void battle_swirl(FrameServices &services, std::uint32_t stack,
                      const BattleStartPresent &present);
    // The other draw environment becomes current and its ordering table is
    // cleared (800b7870, 800b88c4).
    void swap_battle_draw_buffer();
    // 80071188..80071278: the opening and the setup frames.
    void battle_setup_frames(FrameServices &services, const BattleStartPresent &present,
                             const ProgramObserver &observe);
    // 80070f40 after 800b81bc up to 80077990: the fade tasks (800b39c0), the
    // sequence of 800694f8 (800397fc) and the scene data pointers.
    void battle_opening();
    // 80077990: the palette rows read back, window glyphs and draw modes.
    void battle_opening_images(FrameServices &services);
    // 8007819c: the two message windows and the four pairs of text quads.
    void battle_opening_windows();
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
    // Original 800a28d4 frame SP while load_field runs its initialization.
    std::optional<std::uint32_t> field_init_stack_{};
    // Original 8007554c body SP lent to source callees for this frame only.
    std::optional<std::uint32_t> field_frame_stack_{};
    // Source caller contexts lent only while the recovered GPU call runs.
    std::optional<FrameCallAbi> gpu_enqueue_caller_{};
    std::optional<FrameCallAbi> gpu_operation_caller_{};
    void retain_view_locals(const field::ViewLocals &locals, std::uint32_t sp,
                            std::uint32_t return_address);
    // Outside interrupt code, the arrivals recorded before the next
    // hardware read: code that polls hardware observes them first.
    void deliver_due_arrivals();
    // The menu overlay (menu_overlay.hpp) runs on the Program's services.
    friend class menu::Overlay;
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
    [[nodiscard]] std::uint32_t
    rot_average4(std::uint32_t record, std::uint32_t packet,
                 std::optional<std::array<std::uint32_t, 2>> local_outputs = {}); // 8004a7bc
    [[nodiscard]] field::GteLong vector_normal(const field::GteLong &vector);     // 80048d7c
    void frame_shadows(std::uint32_t table, std::uint32_t buffer);                // 800764b4
    void compass_quad(std::uint32_t table, std::uint32_t record, const field::GteMatrix &m,
                      bool label);
    [[nodiscard]] std::uint16_t overlay_half(std::uint32_t address) const;
    // Original addresses of Program-owned packets and globals, for the
    // drawing code that links packets by address.
    [[nodiscard]] std::uint32_t memory(std::uint32_t address, std::size_t width = 4) const;
    [[nodiscard]] std::span<std::uint8_t> resource_bytes(std::uint32_t address,
                                                         std::size_t width) const;
    void set_memory(std::uint32_t address, std::uint32_t value, std::size_t width = 4);
    // A store to battle memory when it holds it, else set_memory.
    void store_owned(std::uint32_t address, std::uint32_t value, std::uint32_t width);
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
    std::int32_t read_setup(std::uint32_t file, std::uint32_t destination, std::uint32_t offset,
                            std::uint32_t mode);         // Resident 80029690
    std::int32_t select_ring(std::uint32_t destination); // 80029740..800297a4, 80029858..800298c4
    std::int32_t cd_control(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter);
    std::int32_t cd_command(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter,
                            bool nowait, std::array<std::uint8_t, 8> *result = nullptr); // 80042088
    std::int32_t cd_sync(std::array<std::uint8_t, 8> *result = nullptr); // 80041b3c(0, result)
    void cd_dma_callback(std::uint32_t function);                        // 800413ec
    // Resident disc and libcd calls of the movie library (resident_disc.cpp).
    [[nodiscard]] std::array<std::uint32_t, 2> current_directory() const; // 800284b4
    void current_directory(std::uint32_t group, std::uint32_t index);     // 800284b4
    std::uint32_t file_sector(std::uint32_t file);                        // 800289d0
    std::uint32_t file_words(std::uint32_t file);                         // 800288ec
    void cancel_disc_read(std::uint32_t offset);                          // 8002a498
    void disc_set_mode(std::uint32_t mode);                               // 8002a428
    void seek_file(std::int32_t file);                                    // 8002a2d0
    void cd_datasync_wait();                                              // 8004293c(0)
    std::int32_t cd_control_wait(std::uint8_t command, const std::array<std::uint8_t, 4> *parameter,
                                 std::array<std::uint8_t, 8> *result); // 80040fe4
    std::int32_t cd_control_blocking(std::uint8_t command,
                                     const std::array<std::uint8_t, 4> *parameter,
                                     std::array<std::uint8_t, 8> *result); // 80041248
    std::int32_t cd_command_wait(std::array<std::uint8_t, 8> *result);     // 80042250
    std::uint32_t cd_ready(std::array<std::uint8_t, 8> &result);           // 80041dbc(1)
    void draw_sync_polled(); // 800445d0 in interrupt context (resident_gpu.cpp)
    // The movie library (movie.cpp, movie_stream.cpp).
    void movie_decode(movie::MdecCodec &codec);                              // 801d3d54
    std::uint32_t movie_next_frame(std::uint32_t end, std::uint32_t header); // 801d3b00
    void movie_restart(std::uint32_t file, std::uint32_t sector, std::uint32_t channel,
                       std::uint32_t mode, const std::array<std::uint8_t, 4> *location); // 801d41ac
    void movie_stop();                                                                   // 801d4318
    void mdec_reset(std::uint32_t mode);                                                 // 801d4534
    void mdec_hardware_reset(std::uint32_t mode);                                        // 801d47fc
    void verify_mdec_registers();
    void mdec_in(std::uint32_t buffer, std::uint32_t mode);        // 801d46a0
    void mdec_in_words(std::uint32_t buffer, std::uint32_t words); // 801d48f8
    void mdec_out(std::uint32_t buffer, std::uint32_t words);      // 801d471c
    std::int32_t mdec_in_sync();                                   // 801d4a1c
    std::int32_t mdec_out_sync();                                  // 801d4ab4
    void mdec_out_callback(std::uint32_t function);                // 801d47d8
    std::uint32_t vlc_size(std::uint32_t halfwords);               // 801d4c98
    bool decode_vlc(movie::MdecCodec &codec, std::uint32_t bitstream,
                    std::uint32_t output); // 801d4cc8
    void verify_stream_registers();
    void stream_set_ring(std::uint32_t ring, std::uint32_t count);     // 801d583c
    void stream_clear_ring();                                          // 801d5920
    void stream_clear_slots(std::uint32_t first, std::uint32_t count); // 801d5c34
    void stream_set_stream(std::uint32_t mode, std::uint32_t start, std::uint32_t end,
                           std::uint32_t complete, std::uint32_t ended);   // 801d5af4
    std::uint32_t stream_next(std::uint32_t &data, std::uint32_t &header); // 801d5c70
    std::uint32_t stream_free(std::uint32_t data);                         // 801d5b7c
    void stream_unset_ring();                                              // 801d5980
    std::int32_t stream_read(std::uint32_t mode);                          // 801d586c
    std::uint32_t stream_position(std::array<std::uint8_t, 4> &location);  // 801d5a94
    void stream_dma(std::uint32_t address, std::uint32_t words, std::uint32_t blocks,
                    std::uint32_t control, std::uint32_t interrupt); // 801d66f8
    // The field movie player's helpers (field_movie_player.cpp).
    void movie_stack_word(std::uint32_t address, std::uint32_t value);
    void movie_frame_ready(std::uint32_t callback, std::uint32_t frame, std::uint32_t x,
                           std::uint32_t y); // 800a7120
    // Mode 6's helpers (movie_mode.cpp).
    void movie_mode_play(FrameServices &services, movie::MdecCodec &codec, std::uint32_t select,
                         std::uint32_t frame, const ProgramObserver &observe); // 800763bc
    void movie_mode_run(FrameServices &services, movie::MdecCodec &codec, std::uint32_t frame,
                        const ProgramObserver &observe);               // 80076488
    void movie_mode_frame_ready(std::uint32_t frame, std::uint32_t y); // 800768d8
    void movie_mode_pad();                                             // 800769a4
    // Resident libgpu environment setup (resident_gpu.cpp).
    void set_disp_mask(std::uint32_t mask); // 80044534
    // 8003569c(port): the port's buttons from its controller buffer.
    [[nodiscard]] std::uint32_t pad_buttons(std::size_t port);
    // 80028928(file): a directory entry's negative size as a count; 0 when
    // the size is not negative.
    [[nodiscard]] std::int32_t file_count(std::uint32_t file);
    void movie_wait_disc(FrameServices &services, const ProgramObserver &observe,
                         std::optional<FrameCallAbi> caller);                     // 800a7394
    void movie_release_parked(FrameServices &services, std::uint32_t frame);      // 800a73e8
    void movie_restore_parked(FrameServices &services, std::uint32_t frame);      // 800a74f8
    void movie_sound_step();                                                      // 80085678
    void movie_sound_load();                                                      // 80085788
    void movie_sound_release();                                                   // 80085738
    void movie_overlay_step();                                                    // 800a7948
    void movie_overlay_load();                                                    // 800acc58
    void draw_and_vertical_sync(FrameServices &services);                         // 800775f8
    void movie_last_frame_to_15bit(FrameServices &services, std::uint32_t frame); // 800a77c4(0)
    std::uint32_t movie_next_component();                                         // 800a7744
    void start_field_stream();                                                    // 80070488
    void finish_field_stream(FrameServices &services);                            // 80070508
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
    // `services` supply the results of a request run from the main flow
    // (DrawSync's drain); a request run from the DMA2 interrupt has none.
    void gpu_stack_store(std::uint32_t address, std::uint32_t value, std::size_t width = 4);
    std::uint32_t gpu_execute(FrameServices *services = nullptr,
                              std::optional<FrameCallAbi> caller = {}); // 8004696c
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
    // `arrivals` delivers the interrupts that arrive while the queue drains.
    void draw_sync(FrameServices &services, const std::function<void()> &arrivals = {},
                   std::optional<FrameCallAbi> caller = {}); // 800445d0
    // Mode dispatch (mode_dispatch.cpp).
    void release_heap_blocks();                        // 8003223c
    void restart_heap(std::uint32_t address);          // 80031b10
    std::uint32_t load_mode_block(std::uint32_t mode); // 800199cc
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
    void save_particle_vram(FrameServices &services);                          // 800a915c
    void load_text_palette(FrameServices &services, std::uint32_t caller);     // 80077544
    void copy_screen(FrameServices &services, std::int32_t x, std::int32_t y); // 800a476c
    void release_named_block();                                                // 8003748c
    // 80078c5c; `frame` is its stack frame (its entry SP - 20h).
    void brighten_text_strip(FrameServices &services, std::uint32_t frame);
    void stop_particles(FrameServices &services);                          // 800a9460
    void stop_emitters();                                                  // 800864f0
    void close_dialogues();                                                // 8007ffe8
    void restore_particle_vram(FrameServices &services);                   // 800a91f0
    void reload_transition_setup();                                        // 800a663c(1, 1)
    void reload_present(FrameServices &services);                          // 800a6924
    void reload_screen_fade(FrameServices &services, std::uint32_t frame); // 800a5884(1, 1)
    void prepare_party_sprites();                                          // 8001b044
    void decode_party_sprites();
    void wait_disc_idle(); // 8001ad1c                                           // 8001b3a8
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
    void load_field_effect_bank();                             // 80085890
    void allocate_party_sprites();                             // 80077c88
    void release_party_sprites();                              // 80077d2c
    void field_entry_flag();                                   // 800798bc
    void field_menu_fade(FrameServices &services, std::int32_t shade, std::uint32_t caller_sp,
                         std::uint32_t return_address); // 80079784
    void field_flush_cache(FrameServices &services, std::optional<std::uint32_t> caller_sp,
                           std::uint32_t return_address);   // 8007999c
    void field_menu_return(const ProgramObserver &observe); // 800a2488
    void load_tim_at(FrameServices &services, std::uint32_t tim, std::int32_t x, std::int32_t y,
                     std::int32_t clut_x, std::int32_t clut_y, std::int32_t clut_w,
                     std::int32_t clut_h);                                       // 80070340
    void load_text_images(FrameServices &services);                              // 80077620
    void read_map_ahead();                                                       // 800777dc
    void init_display(FrameServices &services);                                  // 80071fb0
    void run_actor0_script(std::uint32_t entry, const ProgramObserver &observe); // 800a22ac
    void adjust_after_return(const ProgramObserver &observe);                    // 800a24c4
    void release_cached_sequence();                                              // 80085eec
    // Event initialization and actor setup (field_init.cpp).
    void create_actor_sprite(std::size_t index, const field::FieldSpriteArguments &arguments,
                             std::optional<std::uint32_t> caller_sp = {}); // 80076ac0
    void sync_actor_position(std::size_t index);                           // 800a0c94
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
    void release_actor(std::uint32_t index); // 8008083c
    // Battle setup (battle_setup.cpp).
    void setup_battle_party(battle::Battle &battle, FrameServices &services,
                            std::uint32_t stack);     // 801e5384
    void setup_battle_panels(battle::Battle &battle); // 801e6290, 801e62b8
    // 800b39c0(time, mode, r, g, b): start or retarget the fade task.
    void start_battle_fade(battle::Battle &battle, std::uint32_t time, std::uint32_t mode,
                           std::uint32_t red, std::uint32_t green, std::uint32_t blue);
    void step_battle_fade(battle::Battle &battle, std::uint32_t task); // 800b36bc
    // 8008f8f4(id, x, y, w, h, deferred, frame): window `id` (0-7): its block
    // (800d2e38, 5a8h bytes) and placement record (800d2d90, eh bytes) when
    // UI +b0+id is clear, with their primitives (80077454); then its frame
    // built at once (8008f6e4) or, when deferred, its placement recorded.
    void open_battle_window(battle::Battle &battle, std::uint32_t id, std::int16_t x,
                            std::int16_t y, std::int16_t w, std::int16_t h, bool deferred,
                            bool frame);
    std::uint32_t unpack_battle_item(battle::Battle &battle, std::uint32_t item,
                                     std::uint32_t mode); // 80032e88
    void release_battle_block(battle::Battle &battle, std::uint32_t address,
                              std::uint32_t call_site); // 800320e8
    // The battle loading task (battle_loader.cpp).
    std::uint32_t battle_block(std::uint32_t size, std::uint32_t mode, std::uint32_t site);
    void release_battle_heap(std::uint32_t address, std::uint32_t site);
    std::vector<std::uint8_t> take_task_bytes(std::uint32_t address, std::uint32_t size);
    void return_task_list();
    void register_task(std::uint32_t owner, std::uint32_t node);         // 8001cc18
    void register_pending_task(std::uint32_t owner, std::uint32_t node); // 8001ca58
    void unlink_task(std::uint32_t node);                                // 8001cd94
    std::uint32_t create_task(std::uint32_t size, std::uint32_t owner, std::uint32_t update,
                              std::uint32_t draw, std::uint32_t destroy); // 8001d1d8
    void with_battle_sprites(const std::function<void(const field::SpriteSources &)> &call);
    std::uint32_t create_battle_sprite_task(std::uint32_t data, std::uint32_t palette,
                                            std::int16_t x, std::int16_t y, std::uint32_t animation,
                                            std::uint32_t variant); // 800ba984
    void create_slot_sprite(std::uint32_t slot, std::uint32_t index,
                            std::uint32_t animation); // 801e67a4
    void place_enemy_rows(FrameServices &services, std::uint32_t data,
                          std::uint32_t frame); // 801e6314
    void read_member_files(std::uint32_t list); // 801e693c
    void create_member_sprites();               // 801e6ac4
    void upload_image_sections(FrameServices &services, std::uint32_t data,
                               std::uint32_t rect); // 8002dde4
    // LoadImage of the rectangle at `rect` in battle memory, clamped in place.
    void load_battle_image(battle::Battle &battle, FrameServices &services, std::uint32_t rect,
                           std::uint32_t source);
    void upload_battle_images(battle::Battle &battle, FrameServices &services, std::uint32_t images,
                              std::uint32_t frame); // 8002dde4
    void register_stage_model(battle::Battle &battle, FrameServices &services, std::uint32_t frame,
                              std::uint32_t stage);                // 800a8bf0
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
                                std::uint32_t link_status,
                                std::optional<std::uint32_t> contact_sp = {});
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
    // Set while a delivered arrival's interrupt code runs.
    bool in_interrupt_{};
};

} // namespace xem::reconstruction
