/* The field overlay's common (uninitialized global) variables. The field
 * BSS starts at the program's end (800af5e8) with the units' own variables,
 * field_event's and field_effect's statics; the original linker then
 * allocated the commons, which every unit reaches, in an order of its own up
 * to the end the resident's mode table clears (the word at 800c426c). This
 * unit, linked last, defines them in that order, each in a slot of whole
 * words (decomp/Makefile, uninitialized variables). Nothing addresses the
 * words marked unreferenced. GCC emits tentative definitions in the order of
 * their first declaration, so they are defined ahead of the field headers
 * that declare them, the field's own and the resident's structures by their
 * tags (resident/gpu.h, which declares none of them, gives the untagged
 * Panorama); the headers then complete and check the types. */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"

s32 field_layer_next_index; /* 800AF858 */
MATRIX field_camera_next_view; /* 800AF85C */
u16 *field_screen_band_work_pixels; /* 800AF87C: the band's working pixels */
struct FieldView field_view; /* 800AF880 */
s32 field_unreferenced_block_01[44]; /* 800AFB58: unreferenced */
u16 field_compass_colors[16]; /* 800AFC08: compass colours read back from VRAM */
RECT field_vram_column_rect; /* 800AFC28 */
MATRIX field_sprite_view_matrix; /* 800AFC30: sprite view rotation matrix */
u8 *field_snapshot_cursor; /* 800AFC50 */
s32 field_music_seq_read_pending; /* 800AFC54 */
RECT field_screen_band_rect; /* 800AFC58: screen band saved by event op dd */
POLY_FT4 *field_status_panel_quads[2]; /* 800AFC60 */
struct FieldSprites *field_overlay_sprites; /* 800AFC68 */
u16 field_play_record_buttons; /* 800AFC6C: buttons held since the last record */
struct ScreenColumn *field_vram_column_copy; /* 800AFC70: saved screen column */
s32 field_sprite_created_count; /* 800AFC74: sprites created */
s32 field_music_saved_for_battle; /* 800AFC78 */
s32 field_event_batch_limit; /* 800AFC7C: batch limit */
RECT field_texture_windows[16]; /* 800AFC80: text texture windows */
s32 field_unreferenced_word_01; /* 800AFD00: unreferenced */
s32 field_dialogue_open_blocked; /* 800AFD04: dialogue gate */
void *field_sound_bank_load_buffer; /* 800AFD08: bank file being loaded */
s32 field_sound_bank_load_file; /* 800AFD0C: bank file number */
s32 field_unread_collision_attribute_count; /* 800AFD10: attributes before the first triangle */
s32 field_map_change_frames; /* 800AFD14 */
s32 field_sound_bank_load_slot; /* 800AFD18: bank slot being loaded */
s32 field_current_event_actor_index; /* 800AFD1C: current actor index */
s16 field_unread_party_sprite_take_mark; /* 800AFD20 */
u16 field_compass_palette[128]; /* 800AFD24: compass palette */
DR_MODE field_menu_fade_draw_modes[2]; /* 800AFE24: fade draw mode per buffer */
RECT field_fade_texture_windows[2]; /* 800AFE3C: fade texture windows */
RECT field_menu_fade_source_rect; /* 800AFE4C: fade copy source */
TILE field_menu_fade_tiles[2]; /* 800AFE54: fade tile per buffer */
s32 field_staff_roll_drawing; /* 800AFE74 */
s32 field_picture_marker_origin_x; /* 800AFE78 */
s32 field_picture_marker_origin_y; /* 800AFE7C */
void *field_block_pair_first; /* 800AFE80 */
s32 field_no_panorama_after_return; /* 800AFE84 */
struct EmitterSlot field_sound_emitter_slots[3]; /* 800AFE88 */
u16 field_pad_port0_held; /* 800AFE9C: held pad buttons */
u16 field_pad_port1_held; /* 800AFEA0 */
void (*field_music_stream_callback)(s32); /* 800AFEA4: stream chunk callback */
struct WindowList field_texture_scrolls; /* 800AFEA8 */
s32 field_event_yield_ends_run; /* 800AFFEC */
s32 field_unreferenced_block_02[21]; /* 800AFFF0: unreferenced */
s32 field_effect_edited_template; /* 800B0044 */
s32 field_map_change_kind; /* 800B0048 */
RECT field_compass_palette_rect; /* 800B004C: compass colour strip */
s8 *field_pointer_pads[2]; /* 800B0054: pointer pad buffers */
u16 field_pointer_x_divisor; /* 800B005C: pointer X divisor */
u16 field_pointer_y_divisor; /* 800B0060: pointer Y divisor */
s32 field_exit_game_mode; /* 800B0064 */
s32 field_pointer_x[2]; /* 800B0068: pointer X per port */
s32 field_pointer_y[2]; /* 800B0070: pointer Y per port */
struct FieldActor *field_current_event_actor; /* 800B0078: current event actor */
Panorama *field_panorama; /* 800B007C */
struct FieldEventParams field_panorama_parameters; /* 800B0080 */
s32 field_camera_pitch; /* 800B00B4: camera pitch */
SVECTOR field_screen_pieces_rotation; /* 800B00B8: piece rotation */
s32 field_event_yield_requested; /* 800B00C0: yield */
struct ScreenGrid *field_screen_grid; /* 800B00C4 */
struct FileRequest field_dialogue_portrait_file_requests[3]; /* 800B00C8: file list read by 80029afc */
void *field_music_shared_wave_bank; /* 800B00E0: shared wave bank buffer */
s32 field_movie_awaiting_frame; /* 800B00E4 */
MATRIX field_instance_cull_matrix; /* 800B00E8: instance view: the rotation with its translation */
s16 field_effect_slot_owners[64]; /* 800B0108: effect slot owners, -1 free */
struct OverlaySprites field_wide_overlay_sprites; /* 800B0188 */
u8 field_play_record_stopped; /* 800B02C8 */
struct Record78 field_effect_templates[8]; /* 800B02CC: the eight particle emitters */
s32 field_dialogue_message_slots[4]; /* 800B068C */
void *field_block_pair_second; /* 800B069C */
s32 field_movie_frame; /* 800B06A0 */
struct FieldSlot6 field_dialogue_portrait_slots[3]; /* 800B06A4 */
struct FieldDescriptor *field_current_event_descriptor; /* 800B06B8: descriptor of the running actor */
struct FieldMarker field_compass_markers[25]; /* 800B06BC: ring, letters, needle and pointer quads */
struct ScreenPieces field_screen_pieces; /* 800B11AC */
s32 field_unread_cleared_word; /* 800B14A4 */
s32 field_screen_convert_nibble_count; /* 800B14A8: nibble counter */
u16 field_unread_jump_frame_count; /* 800B14AC */
u8 field_effect_slot_states[64]; /* 800B14B0: effect slot states */
struct FieldHistory field_movement_history_records[32]; /* 800B14F0 */
struct PictureMarks *field_picture_marker_sprites; /* 800B1DF0 */
/* Draw modes per buffer and texture window: 0 text, 1 compass, 3 distortion, 4 grid. */
DR_MODE field_texture_window_modes[2][16]; /* 800B1DF4 */
u32 *field_event_loaded_tim; /* 800B1F74: TIM image held by instruction 0x77 */
struct SpriteSlotTable field_sprite_slots; /* 800B1F78 */
struct FieldWork field_work; /* 800B2078 */
struct SoundBank *field_movie_sound_bank; /* 800B235C: movie sound-effect bank */
s32 field_movement_history_indices[3]; /* 800B2360: movement history index per party slot */
u16 field_menu_parameter; /* 800B236C: menu parameter set by ext 99 */
s32 field_music_chunk_count; /* 800B2370: music-wave chunks gathered */
struct FieldLaunch field_effect_launch; /* 800B2374: effect launch for ext 90 and 93 */
s32 field_unreferenced_block_03[3]; /* 800B2388: unreferenced */
/* The 801e module's file list: two files per layer (at most four), the
 * module file and the zero end. */
struct FileRequest field_layer_file_requests[10]; /* 800B2394 */
s32 field_unreferenced_block_04[46]; /* 800B23E4: unreferenced */
struct FieldDrawBlock field_draw_blocks[2]; /* 800B249C */
s32 field_screen_pieces_scale; /* 800C2684: piece scale, 0x1000 = 1 */
u32 field_screen_convert_word; /* 800C2688: current word */
s32 field_monitor_absent; /* 800C268C */
s16 field_unread_text_pair1[2]; /* 800C2690: only cleared (80077620) */
u16 field_pad_port0_pressed; /* 800C2694: newly pressed pad buttons */
struct DialogueWindow field_dialogue_windows[4]; /* 800C2698 */
u16 field_pad_unread_port1_pressed; /* 800C38F8 */
s16 field_unread_text_pair2[2]; /* 800C38FC: only cleared (80077620) */
u16 field_pad_port0_repeated; /* 800C3900: pad buttons held */
u32 *field_screen_convert_stream; /* 800C3904: packed stream */
u16 field_pad_port1_repeated; /* 800C3908: pad buttons pressed */
u32 *field_screen_convert_pixels; /* 800C390C: converted pixels */
s32 field_movement_history_not_recorded; /* 800C3910: history reset */
s32 field_picture_marker_scale_x; /* 800C3914 */
struct Record78 *field_effect_slot_emitters[64]; /* 800C3918: effect slot emitters */
s32 field_picture_marker_scale_z; /* 800C3A18 */
void *field_music_gather_buffer; /* 800C3A1C: music-wave gather buffer */
struct FieldMovieRequest field_movie_request; /* 800C3A20 */
struct ScreenPieces *field_picture_pieces; /* 800C3A3C: the picture's three pieces */
s32 field_screen_grid_fade_radius; /* 800C3A40: fade radius */
s32 field_pointer_left; /* 800C3A44: pointer bounds */
u16 *field_screen_band_saved_pixels; /* 800C3A48: the band's saved pixels */
s32 field_pointer_top; /* 800C3A4C */
s32 field_pointer_right; /* 800C3A50 */
s32 field_pointer_bottom; /* 800C3A54 */
s32 field_unreferenced_word_02; /* 800C3A58: unreferenced */
s32 field_model_cull_margin_x; /* 800C3A5C */
s32 field_model_cull_margin_y; /* 800C3A60 */
s32 field_movie_sound_timeline_index; /* 800C3A64: movie sound timeline position */
/* Event variables: the game state's 0x200, then 0x200 cleared on entry. */
s16 field_event_variables[0x400]; /* 800C3A68 */
s32 field_dialogue_pass_open_count; /* 800C4268: dialogue windows opened this pass */
struct FieldDrawBlock *field_current_draw_block; /* 800C426C: current draw block */

#include "field/monitor.h"
#include "field.h"
#include "field_camera.h"
#include "field_dialogue.h"
#include "field_draw.h"
#include "field_effect.h"
#include "field_event.h"
#include "field_layer.h"
#include "field_load.h"
#include "field_mode.h"
#include "field_motion.h"
#include "field_movie.h"
#include "field_pad.h"
#include "field_panel.h"
#include "field_party.h"
#include "field_picture.h"
#include "field_screen.h"
#include "field_sound.h"
