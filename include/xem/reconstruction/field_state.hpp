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
    std::uint32_t transition{}; // 800adb38: nonzero runs the 800a5924 transition
    std::uint32_t w_adbd0{};    // 800adbd0: read by the branch after a battle request
    std::uint32_t w_adc10{};    // 800adc10: cleared by the field mode's start
    std::uint32_t w_adb7c{};    // 800adb7c: cleared by the field mode's start
    // 800c3a44, 800c3a4c, 800c3a50, 800c3a54: the pointer's bounds (left, top,
    // right, bottom), scaled by its divisors.
    std::array<std::int32_t, 4> pointer_bounds{};
    // 800c2690, 800c2692: cleared by the text image load.
    std::uint16_t w_c2690{};
    std::uint16_t w_c2692{};
    std::uint32_t saved_music{}; // 800afc78: music the battle branch saved (8004f324)
    std::uint32_t
        w_adb30{}; // 800adb30: a block the loop's exit releases; a movie's library ends there
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
    // The movie player 800a7c58 (field_movie_player.cpp).
    std::uint32_t movie_frame_pending{};  // 800b00e4: the first frame is not loaded yet
    std::uint32_t movie_display{};        // 800adb78: 1 when the last frame went to y 0
    std::uint32_t movie_overlay_active{}; // 800afe74: 800a7948 draws over the movie
    std::uint32_t movie_component_word{}; // 800c2688: 800a7744's current word
    std::uint32_t movie_bits_read{};      // 800b14a8: components 800a7744 returned
    // The library block the player holds in s2 across its loop; a host
    // resuming the player takes it from that register.
    std::uint32_t movie_library{};
    ReloadState reload;
};

} // namespace xem::reconstruction
