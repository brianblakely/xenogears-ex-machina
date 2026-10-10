/* The world map's uninitialized variables. The BSS starts at the program's
 * end (8009bbb4) and runs through the word at 8009d80c, the span the
 * resident's mode table clears. No unit has statics, which would come first
 * in unit order: the first variable (seven units) and the last (two) are
 * shared, and those one unit reaches lie among shared ones throughout
 * (tools/data_users.py --range 8009bbb4:8009d810). All of them are commons,
 * which the original linker allocated in an order of its own; this unit,
 * linked last, defines them in that order. Each takes a slot of whole words
 * (decomp/Makefile, uninitialized variables): consecutive halfwords lie a
 * word apart (8009bd10-8009bd1c, 8009bd24/8009bd28, 8009cd4c/8009cd50),
 * where a size-aligned allocation would leave two bytes. Nothing addresses
 * the words marked unreferenced. GCC emits tentative definitions in the
 * order of their first declaration, so they are defined ahead of the world
 * map headers that declare them, the world map's own types by their tags;
 * the headers then complete the types. */
#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/window.h"

VECTOR worldmap_terrain_origin; /* 8009BBB4: terrain origin (TERRAIN_ORIGIN, GROUND_SCROLL) */
s32 worldmap_next_scene_chosen; /* 8009BBC4 */
struct DisplayBuffer worldmap_display_buffers[2]; /* 8009BBC8 */
s32 worldmap_stream_read_slot; /* 8009BCB8 */
s32 worldmap_unreferenced_word_01; /* 8009BCBC: unreferenced */
struct AreaObject *worldmap_effect_emitters; /* 8009BCC0 */
s32 worldmap_encounter_timer_count; /* 8009BCC4 */
s32 worldmap_battle_music_file; /* 8009BCC8 */
s32 worldmap_stream_sector_header[3]; /* 8009BCCC: sector header */
s32 worldmap_terrain_row_file; /* 8009BCD8 */
s32 worldmap_projection_distance; /* 8009BCDC */
u16 worldmap_area_image_cluts[16]; /* 8009BCE0: faded CLUT ids */
s32 *worldmap_path_tables; /* 8009BD00 */
s16 worldmap_actor_cylinder_count; /* 8009BD04 */
s32 worldmap_terrain_column_file; /* 8009BD08 */
s32 worldmap_entry_map_offset; /* 8009BD0C */
u16 worldmap_pad_port0_pressed; /* 8009BD10 */
u16 worldmap_pad_unread_port1_pressed; /* 8009BD14 */
u16 worldmap_pad_unread_port0_repeated; /* 8009BD18 */
u16 worldmap_pad_unread_port1_repeated; /* 8009BD1C */
void *worldmap_area_image; /* 8009BD20 */
s16 worldmap_path_name_id; /* 8009BD24 */
s16 worldmap_object_model_count; /* 8009BD28: animation count */
s32 worldmap_stream_wait; /* 8009BD2C */
void *worldmap_object_placement_list; /* 8009BD30 */
s32 worldmap_button_combo_pressed; /* 8009BD34 */
SVECTOR worldmap_camera_angle; /* 8009BD38: camera angle */
struct ViewSetup worldmap_view_setup; /* 8009BD40: view setup: eye, look-at point and up vector (VIEW) */
u8 worldmap_cylinder_hit_actor; /* 8009BD60 */
Window worldmap_destination_window; /* 8009BD64: destination name window */
struct EffectSlot *worldmap_effect_slots; /* 8009BDF4 */
void *worldmap_gear_models[3]; /* 8009BDF8: gear model buffers */
s16 worldmap_billboard_quad_count; /* 8009BE04: quads used this frame; a word in 80099BFC */
void *worldmap_stream_disc_buffer; /* 8009BE08: disc request buffer */
s32 worldmap_view_center_y; /* 8009BE0C */
s32 worldmap_movement_mode; /* 8009BE10: movement mode */
void *worldmap_footprint_quads0; /* 8009BE14 */
void *worldmap_footprint_quads1; /* 8009BE18 */
void *worldmap_effect_quads[2]; /* 8009BE1C: effect quads, per display buffer */
struct WorldmapActor *worldmap_actor_slots; /* 8009BE24 */
struct Camera worldmap_camera; /* 8009BE28 */
s32 worldmap_footprint_count; /* 8009BE38 */
struct DisplayBuffer *worldmap_current_display_buffer; /* 8009BE3C */
s32 worldmap_encounter_period; /* 8009BE40 */
s32 worldmap_stream_write_slot; /* 8009BE44 */
s32 worldmap_stream_unread_cleared_word_1; /* 8009BE48 */
MATRIX worldmap_unread_mode_matrix; /* 8009BE4C */
struct PlaceRequest worldmap_actor_cylinders[32]; /* 8009BE6C */
s32 worldmap_walker_face; /* 8009C16C */
s32 worldmap_party_count; /* 8009C170: loaded party members */
s32 worldmap_area_image_file; /* 8009C174 */
s32 worldmap_screen_fade_active; /* 8009C178 */
s32 worldmap_terrain_image_file; /* 8009C17C */
void *worldmap_area_data; /* 8009C180 */
void *worldmap_terrain_blocks[0x100]; /* 8009C184: terrain block buffers */
s32 worldmap_player_heading; /* 8009C584: player heading */
s8 worldmap_cd_sync_result[8]; /* 8009C588 */
u8 *worldmap_stream_destination; /* 8009C590: destination of the next sector's data */
s32 worldmap_unreferenced_pair[2]; /* 8009C594: unreferenced */
void *worldmap_terrain_image; /* 8009C59C */
DR_TPAGE worldmap_map_overlay_tpage; /* 8009C5A0 */
s32 worldmap_mode_index; /* 8009C5A8: arrival kind */
VECTOR worldmap_player_position; /* 8009C5AC: player position (20.12) */
s32 worldmap_wave_phase_x; /* 8009C5BC */
POLY_FT4 worldmap_map_overlay_quads[2]; /* 8009C5C0: overlay picture, per buffer */
s32 worldmap_area_index; /* 8009C610 */
void *worldmap_battle_music; /* 8009C614 */
s32 worldmap_wave_phase_z; /* 8009C618 */
s32 worldmap_unreferenced_word_02; /* 8009C61C: unreferenced */
struct SceneObject *worldmap_objects; /* 8009C620: scene objects */
struct HostReadRequest *worldmap_stream_host_lists[16]; /* 8009C624: submitted host-file request lists */
POLY_G3 worldmap_map_marker_polys[8]; /* 8009C664 */
POLY_FT4 worldmap_horizon_quads[2][2]; /* 8009C744: textured horizon quads, per buffer */
void *worldmap_suspend_memory; /* 8009C7E4: free memory block kept while away */
s32 worldmap_button_combo_held_last; /* 8009C7E8 */
struct BillboardList *worldmap_billboard_lists; /* 8009C7EC */
VECTOR worldmap_terrain_cull_normal_3; /* 8009C7F0 */
void *worldmap_saved_vram_page; /* 8009C800: saved VRAM area */
s32 worldmap_unreferenced_word_03; /* 8009C804: unreferenced */
MATRIX worldmap_camera_matrix; /* 8009C808: camera matrix */
VECTOR worldmap_terrain_cull_normal_0; /* 8009C828 */
SVECTOR worldmap_camera_block_cell; /* 8009C838: block cell */
s32 worldmap_walker_object; /* 8009C840 */
VECTOR worldmap_terrain_cull_normal_1; /* 8009C844 */
s16 worldmap_encounter_timers[16]; /* 8009C854 */
VECTOR worldmap_terrain_cull_normal_2; /* 8009C874 */
void *worldmap_music; /* 8009C884 */
void *worldmap_flight_music; /* 8009C888 */
void *worldmap_wave_bank; /* 8009C88C */
void *worldmap_saved_vram_cluts; /* 8009C890: saved VRAM area */
s32 worldmap_resuming; /* 8009C894: nonzero when resuming a saved state */
TILE worldmap_map_dot_tiles[0x40]; /* 8009C898 */
s32 worldmap_wave_bank_file; /* 8009CC98 */
s32 worldmap_texture_anim_count; /* 8009CC9C */
s32 worldmap_stream_read_error_count; /* 8009CCA0 */
s32 worldmap_screen_fade_rate; /* 8009CCA4 */
s32 worldmap_stream_status_error_count; /* 8009CCA8 */
s32 worldmap_unreferenced_word_04; /* 8009CCAC: unreferenced */
s32 worldmap_stream_unread_cleared_word_2; /* 8009CCB0 */
u16 worldmap_terrain_cluts[0x40]; /* 8009CCB4: terrain palette CLUT ids */
void *worldmap_character_models[3]; /* 8009CD34: character model buffers */
void (*worldmap_cloud_draw_hook)(void); /* 8009CD40: per-frame hook */
s32 worldmap_stream_state; /* 8009CD44 */
void *worldmap_object_models; /* 8009CD48 */
u16 worldmap_pad_port0_held; /* 8009CD4C: pad buttons held */
u16 worldmap_pad_unread_port1_held; /* 8009CD50 */
u16 worldmap_terrain_tpages[7]; /* 8009CD54: terrain texture pages */
s32 worldmap_texture_anim2_count; /* 8009CD64 */
struct FerryHeading worldmap_ferry_headings[32]; /* 8009CD68 */
s16 worldmap_destination_name_id; /* 8009CE68: destination id, -1 none */
POLY_G4 worldmap_screen_fade_quads[2]; /* 8009CE6C: full-screen fade, per display buffer */
struct DriftVelocity *worldmap_cloud_velocities; /* 8009CEB4 */
s32 worldmap_stream_bytes_left; /* 8009CEB8 */
CdlLOC worldmap_stream_seek_position; /* 8009CEBC: request position */
s32 worldmap_button_combo_held; /* 8009CEC0 */
struct TrailPoint worldmap_trail_points[32]; /* 8009CEC4 */
s32 worldmap_view_kind; /* 8009D144 */
u16 *worldmap_scene16_haze_spreads; /* 8009D148: per-row wobble spread */
s32 worldmap_saved_gear_riding_lock; /* 8009D14C */
struct Drift *worldmap_cloud_positions; /* 8009D150 */
s16 worldmap_trail_index; /* 8009D154: trail index */
struct QuadBuffer *worldmap_scene16_haze_quads[2]; /* 8009D158 */
s32 worldmap_area_blocks_x; /* 8009D160 */
DR_MOVE worldmap_scene16_haze_copies[2]; /* 8009D164: haze copy-back, per display buffer */
struct PolyG4 worldmap_sky_bands[4][2]; /* 8009D194: sky gradient bands, per buffer */
s32 worldmap_area_blocks_z; /* 8009D2B4 */
POLY_FT4 worldmap_destination_marker[2]; /* 8009D2B8: destination marker, per display buffer */
void *worldmap_object_meshes; /* 8009D308 */
struct WorldmapSpot *worldmap_footprints; /* 8009D30C: ring of 16 recent positions */
DR_TPAGE worldmap_screen_fade_blend; /* 8009D310: fade blend mode */
struct BlockGrid worldmap_terrain_previous_grid; /* 8009D318: previous terrain blocks */
struct DiscReadRequest *volatile worldmap_stream_next_request; /* 8009D3BC: next disc request */
void *worldmap_stream_host_buffer; /* 8009D3C0: host-file request buffer */
s32 worldmap_area_data_file; /* 8009D3C4 */
s32 worldmap_sound_bank_file; /* 8009D3C8 */
s32 worldmap_screen_fade_step; /* 8009D3CC */
s32 worldmap_music_file; /* 8009D3D0 */
s32 worldmap_entry_index; /* 8009D3D4 */
DR_TWIN worldmap_horizon_texture_windows[2]; /* 8009D3D8 */
s32 worldmap_camera_distance; /* 8009D3F0: camera distance */
struct WorldmapSpot *worldmap_arrival_points; /* 8009D3F4 */
FileRequest worldmap_read_list[16]; /* 8009D3F8: shared read list */
u16 worldmap_billboard_cluts[16]; /* 8009D478: terrain CLUTs */
Window worldmap_path_window; /* 8009D498: path name window */
void *worldmap_packed_menu_overlay; /* 8009D528 */
u16 worldmap_camera_follow_heading; /* 8009D52C */
s32 worldmap_unreferenced_word_05; /* 8009D530: unreferenced */
MATRIX worldmap_terrain_matrix; /* 8009D534 */
s32 worldmap_loop_running; /* 8009D554 */
s16 worldmap_terrain_crossed_edges; /* 8009D558 */
struct Camera worldmap_camera_follow_target; /* 8009D55C: saved camera */
u32 worldmap_stream_sectors_left; /* 8009D56C: sectors left */
struct BlockGrid worldmap_terrain_grid; /* 8009D570: current terrain blocks */
s32 worldmap_stream_wanted_sector; /* 8009D614 */
s16 worldmap_terrain_visible_blocks[25]; /* 8009D618: 5x5 visible blocks; -1 empty */
s32 worldmap_encounter_reroll_timer; /* 8009D64C */
s16 worldmap_terrain_quarter_visibility[25][4]; /* 8009D650: per block: visibility of its 4 quarters */
s16 worldmap_mesh_probe_hits[16]; /* 8009D718: probe hits: face and kind pairs */
u8 worldmap_cylinder_hit; /* 8009D738 */
void *worldmap_encounter_sets[16]; /* 8009D73C */
s32 *worldmap_texture_anim_section; /* 8009D77C */
struct TexAnim *worldmap_texture_anims; /* 8009D780 */
void *worldmap_name_table; /* 8009D784 */
struct DiscReadRequest *worldmap_stream_disc_lists[16]; /* 8009D788: submitted disc request lists */
s32 *worldmap_texture_anim2_section; /* 8009D7C8 */
s32 worldmap_loop_result; /* 8009D7CC */
struct TexAnim *worldmap_texture_anims2; /* 8009D7D0 */
void *worldmap_stream_drain_buffer; /* 8009D7D4: unused bytes drained from the final CD sector */
struct PathRegion *worldmap_current_path; /* 8009D7D8: the current path, -1 none */
s32 worldmap_terrain_packet_count; /* 8009D7DC: terrain POLY_FT3 packets used this frame */
s16 worldmap_object_count; /* 8009D7E0: scene object count */
s32 worldmap_unreferenced_word_06; /* 8009D7E4: unreferenced */
void *worldmap_billboard_quads[2]; /* 8009D7E8 */
s32 worldmap_display_buffer_index; /* 8009D7F0: current buffer */
s32 worldmap_stream_drive_sector; /* 8009D7F4 */
void *worldmap_cloud_quads[2]; /* 8009D7F8 */
s32 worldmap_flight_music_file; /* 8009D800 */
s32 worldmap_menu_requested; /* 8009D804 */
s32 worldmap_stream_request_count; /* 8009D808 */
s32 worldmap_encounter_expired_count; /* 8009D80C */

#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"
