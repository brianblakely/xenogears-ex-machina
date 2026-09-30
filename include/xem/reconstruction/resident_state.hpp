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

#include "xem/reconstruction/disc_state.hpp"
#include "xem/reconstruction/gpu_state.hpp"
#include "xem/reconstruction/input_state.hpp"
#include "xem/reconstruction/interrupt_state.hpp"
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace xem::reconstruction {

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
    std::uint8_t b_5942c{}; // 8005942c: cleared on every exit
    // Leaving a field for battle (field_battle_exit.cpp): the battle-entry
    // flag 800798bc sets, the field's effect bank 80085988 unlinks (8006259c),
    // 8004f32c it resets and the party sprite blocks 80077d2c releases
    // (8005a414).
    std::uint8_t b_59179{};            // 80059179
    std::uint32_t field_effect_bank{}; // 8006259c: the field (and menu) effect bank block
    std::uint32_t w_4f32c{};           // 8004f32c
    // The movie player parks slots 1 and 2 in VRAM while a movie plays.
    std::array<std::uint32_t, 3> party_sprite_blocks{}; // 8005a414
    std::uint32_t w_4f30c{};                            // 8004f30c
    std::uint32_t w_4f310{};                            // 8004f310
    // 8006fabc..8006fac7: three words the first field start sets to FF
    // (80077f98) and field extended handlers near 8008c4xx shift; meaning
    // not recovered.
    std::array<std::uint32_t, 3> w_6fabc{};
    std::uint32_t w_4f370{}; // 8004f370: nonzero keeps a map change from reaching the dispatcher
    // Field entry 80078d44 globals whose meaning is not recovered.
    std::uint32_t w_4f2f8{}; // 8004f2f8: zero converts the screen first (800a77c4); set after
    std::uint32_t w_4f304{}; // 8004f304: nonzero restores a saved sound state
    std::uint32_t effect_bank_cache{}; // 8005a4bc: a copy of the effect bank a battle kept
    std::uint32_t text_images{};       // 8005a4a0: the field's text image file
    std::uint32_t w_4f344{};           // 8004f344: nonzero while the text images are read
    std::uint32_t w_62524{};           // 80062524: the field start's copy of 800595ac
    // 80065afc: each party slot's sprite file read ahead (packed, 8001aeb8).
    std::array<std::uint32_t, 3> party_sprite_files{};
    std::uint32_t w_4f300{}; // 8004f300: field drawing over a movie (800a7948, 800acc58)
    // 8004fe44..47: the movie mode's request (kind and flag, movie, next
    // mode, keep playing through Circle and Start).
    std::uint32_t movie_request{};
    // 8005947c: nonzero keeps the battle epilogue on mode 2 and 800594f8 clear.
    std::uint8_t b_5947c{};
    // Battle setup files (8001bbac, 800379d8): the formation data the scene
    // file holds (8005949c), the setup archive (directory 12 file 3,
    // 800595a8), the effect header (file 2, 800595d0) and the two blocks that
    // place the setup module at 801e4000 (80059480, 800594ac).
    std::uint32_t battle_scene{};   // 8005949c
    std::uint32_t battle_archive{}; // 800595a8
    std::uint32_t battle_effects{}; // 800595d0
    std::uint32_t battle_marker{};  // 80059480
    std::uint32_t battle_spacer{};  // 800594ac
    std::uint8_t b_5959c{};         // 8005959c: 8001b6c4 sets it, 80070f40 clears it
    std::uint32_t battle_wave{};    // 800595ac: the wave bank 800b853c loads
    // 80000010, low RAM: 8001cc18 reads it as the generation of a null owner.
    std::uint32_t null_owner_generation{};
    // Battle windows (8008f8f4): a word that selects blending for their
    // glyphs (800595a0: 80076b00 texture page bit 40; 80077454 GetTPage's
    // abr). Their backing quads take window_color.
    std::uint32_t window_blend{};
    // The scene files 800379d8 reads (directory 15 files 2n+6 and 2n+7): the
    // stage file (80059470), a word it clears (80059520), and the scene data
    // after the second file's first word (800658c8 and 8005949c); the list
    // read's zero destination word after its terminator (8005a1f0).
    std::uint32_t battle_stage{};      // 80059470
    std::uint32_t battle_stage_b{};    // 80059520
    std::uint32_t battle_scene_data{}; // 800658c8
    std::uint32_t scene_list_tail{};   // 8005a1f0
    // Field main loop (field_loop.cpp) globals whose meaning is not recovered.
    std::uint32_t w_4f2f4{}; // 8004f2f4: cleared by 800a31e8
    std::uint32_t w_4f318{}; // 8004f318: 800a31e8 frames since variable 10 last stepped
    std::uint32_t w_4f328{}; // 8004f328: bit 80 stops variable 10, bit 4 counts it down
    std::uint8_t b_59171{};  // 80059171: 800b236c when triangle opens the menu
    // Menu overlay (menu_overlay.hpp) globals.
    std::uint8_t menu_mode{};       // 80059460: 0 field menu, 2 title file screen, 6 other
    std::uint8_t menu_cursor{};     // 800594cc: field menu cursor kept between openings
    std::uint8_t menu_effects{};    // 80059178: nonzero loads the menu's effect bank
    std::uint32_t menu_resources{}; // 8005945c: resource block the menu unpacks and frees
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
    // The MDEC's decoded output that DMA1 delivers into RAM, one transfer
    // each, in order: a hardware result (movie.hpp), consumed by the
    // transfer's completion callback.
    std::deque<std::vector<std::uint8_t>> mdec_output;
    // The slice buffers' bytes after an MDEC reset stopped a running output
    // transfer, by buffer address: what the MDEC wrote before it stopped.
    std::map<std::uint32_t, std::vector<std::uint8_t>> mdec_aborted;
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
    // RAM a heap restart (80031b10) left outside the list, as it was, until
    // another owner takes it (a restart below it, a mode's BSS).
    resident::ByteRuns heap_outside;
    // Written by 80031a30 after a mode's heap restart; meaning not recovered.
    std::uint32_t w_59334{}; // 80059334
    std::uint32_t w_59338{}; // 80059338
    // Return addresses InitGeom (800569f0) and its 8004b4ac (800593d4) save.
    std::uint32_t geometry_return{};
    std::uint32_t geometry_inner_return{};
};

} // namespace xem::reconstruction
