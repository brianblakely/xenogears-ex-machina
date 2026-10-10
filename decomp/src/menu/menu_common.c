/* The menu overlay's common (uninitialized global) variables. The menu's
 * assembler put those of up to eight bytes in .sbss and the larger ones in
 * .bss (menu.mk), and the original linker allocated each group's commons
 * after every unit's own, in an order of its own: the small ones at the end
 * of the file (zero there), the larger ones past it up to the end of the BSS
 * the resident's mode table clears (menu.bss.ld). This unit, linked last,
 * defines them in that order, each in a slot of whole words (decomp/Makefile,
 * uninitialized variables). GCC emits tentative definitions in the order of
 * their first declaration, so they are defined ahead of the headers that
 * declare them, structures by their tags; the headers then complete and
 * check the types. The SDK and resident headers ahead of them (for VECTOR,
 * MATRIX, POLY_FT3 and the resident's message Window) declare none of them. */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/window.h"

s32 arena_actors_flat_distance; /* 8009284C: horizontal distance between the actors */
s32 arena_actors_distance; /* 80092850: distance between the actors */
POLY_FT3 *arena_stage_ground_triangles[2]; /* 80092854: map triangle pool per draw buffer */
s16 arena_display_width; /* 8009285C: display width */
u8 arena_hud_left_charge_bar_v; /* 80092860: left bar texel row */
u8 arena_hud_unread_right_charge_bar_v; /* 80092864: right bar texel row */
struct DisplayBuffer *arena_current_draw_buffer; /* 80092868 */
s16 arena_display_height; /* 8009286C: display height */
struct DisplayBuffer *arena_unread_shown_buffer; /* 80092870 */
struct MoveList *arena_actor_move_lists; /* 80092874: per model id */
s32 arena_unused_common_word_1; /* 80092878: unreferenced */
u8 arena_camera_ease_step_count; /* 8009287C */
s32 arena_text_message_table; /* 80092880 */
u8 arena_rubber_band_enabled; /* 80092884 */
s32 arena_select_entry_count; /* 80092888 */
struct Environment *arena_current_stage_colors; /* 8009288C: current stage colours */
s32 arena_bout_round_winner; /* 80092890 */
struct Actor *arena_scene_driven_actor; /* 80092894: actor the scene script drives */
s16 arena_mode_vblanks_per_frame; /* 80092898 */
s32 arena_node_compose_parent_view; /* 8009289C */
u8 arena_draw_buffer_index; /* 800928A0: buffer being built */
s32 arena_unused_common_pair_1[2]; /* 800928A4: unreferenced */
s32 arena_bout_replay_timer; /* 800928AC */
s32 arena_stage_uses_narrow_view; /* 800928B0: the ground cells in view take the narrow wedge (the replay), else the wide one */
u8 arena_stage_index; /* 800928B4: stage */
s32 arena_unused_common_pair_2[2]; /* 800928B8: unreferenced */
u8 arena_bout_pose_ring_index; /* 800928C0 */
u8 arena_retreat_rule_enabled; /* 800928C4: enables the retreat rule */
s32 arena_play_mode; /* 800928C8: menu mode */
s32 arena_mode_unread_disc_mode_kind; /* 800928CC */
u16 arena_debug_display_flags; /* 800928D0: debug display switches */
u8 arena_bout_fight_active; /* 800928D4 */
u8 *arena_select_portraits; /* 800928D8: the 49 portraits, 0x1000 bytes each */
struct GroundSquare *arena_stage_height_map; /* 800928DC */
s32 arena_unused_common_word_2; /* 800928E0: unreferenced */
u32 *arena_current_layer_ot; /* 800928E4: ordering table primitives are added to */
s32 arena_frame_count; /* 800928E8: owner of the segments started now */
struct ListEntry **arena_select_entries; /* 800928EC */
u8 arena_bout_unread_byte; /* 800928F0 */
u8 arena_camera_side_flipped; /* 800928F4 */
s32 arena_debug_path_marker_count; /* 800928F8: recorded path points */
u8 arena_menu_driving_pad_port; /* 800928FC: pad port driving the menus */
s32 arena_scene_bout_end_step; /* 80092900: bout-end sequence step */
s32 arena_camera_view_mode; /* 80092904: camera view */
s32 arena_stage_back_color_blue; /* 80092908 */
s32 arena_camera_side_angle; /* 8009290C */
s32 arena_stage_back_color_green; /* 80092910 */
s32 arena_node_color_changed; /* 80092914: colour changed this frame */
s32 arena_bout_unread_draw_count; /* 80092918 */
s32 arena_stage_back_color_red; /* 8009291C */
u8 arena_menu_screen_flags; /* 80092920 */
s32 arena_menu_screen_done; /* 80092924 */
s32 arena_unused_common_word_3; /* 80092928: unreferenced */
s32 arena_bout_motion_speed; /* 8009292C */
void (*arena_debug_frame_hook)(void *block); /* 80092930 */
s32 arena_actors_heading; /* 80092934 */
u32 *arena_current_ot; /* 80092938: ordering table of the buffer being built */
u8 arena_scene_bout_end_active; /* 8009293C */
s32 arena_select_portraits_in_vram; /* 80092940 */
s32 arena_bout_fight_frame_count; /* 80092944 */
struct SoundSeq *arena_mode_music_seq; /* 80092948 */
s32 arena_bout_round_frame_count; /* 8009294C */
s32 arena_bout_round_number; /* 80092950 */

VECTOR arena_view_origin; /* 80096FA8: the focus the view was last aimed at: the scene origin */
struct SideHits arena_actor_side_move_info[2]; /* 80096FB8 */
MATRIX arena_display_screen_scale; /* 80096FE0: screen scale */
VECTOR arena_look_at_axis_x; /* 80097000: look-at work: third axis */
struct Actor arena_second_actor; /* 80097010: scene actor */
VECTOR arena_camera_focus; /* 8009867C: camera look-at point */
Window arena_scene_message_window; /* 8009868C: message window */
VECTOR arena_camera_position; /* 8009871C: camera eye */
struct Actor arena_first_actor; /* 8009872C: scene actor */
struct Settings arena_settings; /* 80099D98: current option settings */
struct PolyFT4Words arena_select_wheel_quads[2][10]; /* 80099DA8 */
VECTOR arena_look_at_forward; /* 8009A0C8: look-at work: forward */
struct DisplayBuffer arena_display_buffers[2]; /* 8009A0D8: the display buffers */
VECTOR arena_mesh_light_direction; /* 8009A2C8: mesh light direction */
MATRIX arena_display_unread_identity; /* 8009A2D8 */
struct OverlayBuffer arena_hud_overlay_buffers[2]; /* 8009A2F8 */
VECTOR arena_look_at_axis_y; /* 8009A918: look-at work: up */
struct PathMarker arena_debug_path_markers[30]; /* 8009A928: recorded path points */

#include "actor.h"
#include "bout.h"
#include "brain.h"
#include "camera.h"
#include "debug.h"
#include "display.h"
#include "effects.h"
#include "hud.h"
#include "menus.h"
#include "mode.h"
#include "node.h"
#include "script.h"
#include "select.h"
#include "stage.h"
#include "text.h"
