/* The battle overlay's common (uninitialized global) variables. The original
 * linker allocated them after every unit's own variables, from 800c3cec, in
 * an order of its own, up to the end of the BSS that the resident's mode
 * table clears for the overlay (800d39f4), so this unit, linked last, defines
 * them, each in a slot of whole words (decomp/Makefile). GCC emits tentative
 * definitions in the order of their first declaration, so they are defined
 * ahead of the headers that declare them, structures by their tags; the
 * headers then complete the types and check the declarations. The
 * names the battle code uses for parts of these objects are in
 * battle.data.ld. Variables no battle code addresses are marked
 * unreferenced, or with the battle-time modules that use them. */
#include "common.h"
#include "psyq/libgte.h"

u8 battle_unread_command_file_loaded;    /* 800C3CEC: a command file is loaded */
s16 battle_surface_wind_phase; /* 800C3CF0 */
u8 battle_decimal_digits[9]; /* 800C3CF4: decimal digits */
u8 battle_item_menu_chosen_row;    /* 800C3D00: item list row of the chosen item */
struct SpritePool battle_effect_sprite_pool; /* 800C3D04 */
struct EffectPool battle_effect_pool; /* 800C3D0C */
u16 battle_highlight_slot_mask; /* 800C3D14: highlighted slots */
struct EnemyReaction battle_enemy_reactions[8]; /* 800C3D18 */
s32 battle_running_total; /* 800C3D38: the running total */
u8 *battle_attacker_attack_level; /* 800C3D3C: the attacker's attack level and maximum (+0x148) */
u16 battle_selected_object_index; /* 800C3D40 */
u8 battle_resume_event_script_at_end; /* 800C3D44 */
u8 battle_uses_event_script;  /* 800C3D48: the 801e5000 module is loaded */
s16 battle_trail_blend; /* 800C3D4C: the trail's blend */
struct Panorama *battle_stage_backdrops[2]; /* 800C3D50: the stage backdrops */
s32 battle_gear_enemy_count; /* 800C3D58: gear enemies present */
u8 battle_defeat_allowed_by_event_script; /* 800C3D5C */
u8 *battle_target_attack_level; /* 800C3D60: the target's field 0x148 */
u16 battle_target_candidate_mask; /* 800C3D64 */
u8 battle_unread_acting_object_started; /* 800C3D68 */
u8 battle_shadows_enabled; /* 800C3D6C */
u8 battle_gear_part_ids[0x30]; /* 800C3D70 */
struct TextureScroll battle_stage_texture_scrolls[2]; /* 800C3DA0: the stage's texture scrolls */
u8 *battle_enemy_data_file;    /* 800C3DD0: the enemy data file (ovl2615) */
s32 battle_unreferenced_pair[2]; /* 800C3DD4: unreferenced */
void *battle_enemy_name_table;  /* 800C3DDC: enemy name table */
u8 battle_combo_step_buttons[8];  /* 800C3DE0: the entered combo steps' buttons */
void *battle_command_menu_file3_block;  /* 800C3DE8: file 3 block */
s32 battle_enemy_set_file; /* 800C3DEC */
s16 battle_acting_sprite_command_motion; /* 800C3DF0: the acting sprite's command motion */
s32 battle_unreferenced_word_01; /* 800C3DF4: unreferenced */
u8 battle_camera_channels_active;  /* 800C3DF8: effects run */
struct CommandDescriptor *battle_current_command; /* 800C3DFC: current command descriptor */
struct Combatant *battle_attacker_record;         /* 800C3E00: attacker record */
u8 battle_attacker_slot;                        /* 800C3E04: attacker slot */
u8 battle_panel_hp_digits[3];                     /* 800C3E08: panel value digits */
struct KnownSkills battle_known_skills_at_start[3]; /* 800C3E0C */
u8 battle_attack_approach_done; /* 800C3E18 */
struct Sprite *battle_acting_sprite; /* 800C3E1C */
s32 battle_unread_menu_word; /* 800C3E20 */
struct DirectionArrows *battle_direction_arrows; /* 800C3E24 */
u8 battle_direction_input[2]; /* 800C3E28: direction input: [0] the previous, [1] the current */
u8 battle_target_cursor_slot; /* 800C3E2C */
u16 battle_selected_slot_mask;               /* 800C3E30: slot mask */
struct Combatant *battle_target_record; /* 800C3E34: target record */
struct ModelPart *battle_stage_model_parts; /* 800C3E38 */
s32 battle_unreferenced_word_02;   /* 800C3E3C: unreferenced */
u8 battle_enemy_name_indices[8]; /* 800C3E40: enemy name per enemy slot (3-10) */
struct ModelTable *battle_stage_model_table; /* 800C3E48: the stage's models (hierarchy battle_stage_model_parts) */
u8 battle_frame_mode;                /* 800C3E4C: battle end state */
u8 battle_target_slot;                /* 800C3E50: target slot */
s32 battle_music_seq; /* 800C3E54 */
s32 battle_unreferenced_word_03; /* 800C3E58: unreferenced */
struct TextImage battle_digit_text_images[10]; /* 800C3E5C: text images of battle messages 0-9: the decimal digits */
s32 battle_unreferenced_word_04; /* 800C3E84: unreferenced */
s32 battle_stage_frame_remainder; /* 800C3E88 */
u8 battle_pending_message;     /* 800C3E8C: pending battle message + 1 */
u8 battle_target_candidates[12]; /* 800C3E90: default-target candidates */
s16 battle_trail_color_count;    /* 800C3E9C: the trail's colour count */
void *battle_stage_sky; /* 800C3EA0 */
struct BattleGraphics *battle_graphics; /* 800C3EA4 */
s16 battle_stage_image_height; /* 800C3EA8: stage image height */
struct TurnState *battle_turn_state; /* 800C3EAC */
struct BattleArea battle_area; /* 800C3EB0 */
u8 battle_item_counts[0x30]; /* 800D2CB0: item counts */
u8 battle_item_ids[0x30]; /* 800D2CE0: item ids */
u8 battle_scene_part_speeds[4];    /* 800D2D10: speeds 8009892c replaces by each gear part speed */
s32 battle_unreferenced_quad[4];   /* 800D2D14: unreferenced */
u8 battle_party_character_ids[3];    /* 800D2D24: party character ids, 0x7F none */
struct BattleUi *battle_ui; /* 800D2D28 */
s16 battle_stage_image_width; /* 800D2D2C: stage image width */
s16 battle_stage_image_x; /* 800D2D30: stage image x */
s16 battle_stage_image_y; /* 800D2D34: stage image y */
s16 battle_panel_hp_remainder; /* 800D2D38: the panel member HP's digits' remainder */
s32 battle_heap_mark_for_post_battle_module; /* 800D2D3C: 801de000 module blocks */
s32 battle_buffer0_background_color_ptr; /* 800D2D40 */
s32 battle_unread_setup_flag; /* 800D2D44: unreferenced */
s32 battle_buffer1_background_color_ptr; /* 800D2D48 */
s16 battle_effect_hit_count; /* 800D2D4C: effect hits */
u8 battle_skip_result_screens; /* 800D2D50 */
u8 battle_file2_block_and_max_hp_digits[7];  /* 800D2D54: panel maximum digits */
u8 battle_running_result_codes[11]; /* 800D2D5C: running result code per slot */
struct TotalPopup *battle_current_total_popup; /* 800D2D68: the running total's task, if shown */
struct GearRecord *battle_attacker_gear; /* 800D2D6C: attacker's gear record */
s16 battle_running_result_amounts[11];            /* 800D2D70: running result amount per slot */
u8 battle_panel_gear_hp_digits[5];              /* 800D2D88: name glyph codes */
struct WindowRect *battle_window_rects[7]; /* 800D2D90 */
struct Window *battle_message_text_window; /* 800D2DAC: the message text window */
u32 *battle_blank_text_image; /* 800D2DB0: blank text image */
struct ListPrims *battle_hud_primitive_lists; /* 800D2DB4 */
u8 battle_resolve_status;  /* 800D2DB8: resolve status returned to the caller */
s32 battle_unreferenced_word_05; /* 800D2DBC: unreferenced */
u8 battle_forced_next_turn;  /* 800D2DC0: forced next turn: slot + 1 */
u8 battle_ether_check_failed;  /* 800D2DC4: an ether check failed */
struct GearRecord *battle_target_gear; /* 800D2DC8: target's gear record */
struct TurnQueue battle_turn_queue; /* 800D2DCC */
s32 battle_unreferenced_word_06; /* 800D2E34: unreferenced */
struct WindowBlock *battle_window_blocks[7]; /* 800D2E38 */
s16 battle_knocked_down_mask; /* 800D2E54 */
s16 battle_panel_hp; /* 800D2E58: panel member HP */
struct BattleAction battle_action_list[32]; /* 800D2E5C */
void *battle_glyph_table; /* 800D2F5C: glyph table */
s32 battle_heap_reserve_for_post_battle_module; /* 800D2F60 */
u8 battle_triangle_visit_stamp; /* 800D2F64: triangle visit stamp */
struct IconCell battle_icon_cells[22]; /* 800D2F68 */
MATRIX *battle_stage_color_matrix; /* 800D2FC0: the stage colour matrix */
u8 battle_exit_requested;      /* 800D2FC4: battle exit requested */
s16 battle_stage_circle_count;     /* 800D2FC8: point count of battle_stage_circles */
s32 battle_curve_segments_drawn;     /* 800D2FCC: segments drawn of the current curve */
u16 *battle_stage_circles;    /* 800D2FD0: (x, z, y) points */
s32 battle_unreferenced_word_07;     /* 800D2FD4: unreferenced */
u8 *battle_trail_colors;     /* 800D2FD8: the trail being drawn: its colours */
u8 battle_unread_single_action_loaded; /* 800D2FDC */
s16 battle_panel_max_hp_remainder;     /* 800D2FE0: the panel member maximum HP's digits' remainder */
u8 battle_item_inventory_ids[48];  /* 800D2FE4: battle item ids (ovl2596, ovl2615) */
u8 battle_pressed_key; /* 800D3014 */
s32 battle_panel_gear_hp_remainder; /* 800D3018: the panel gear HP's digits' remainder */
struct GroupEntry battle_formation_groups[32]; /* 800D301C */
struct BattleCamera battle_camera; /* 800D309C */
s32 battle_unreferenced_word_08; /* 800D30E8: unreferenced */
struct TotalPopup battle_total_popup; /* 800D30EC */
s32 battle_unreferenced_block[66]; /* 800D316C: unreferenced */
u8 battle_target_candidate_count;      /* 800D3274: candidate count */
struct ScriptState *battle_state_of_event_script; /* 800D3278 */
s32 battle_unreferenced_word_09; /* 800D327C: unreferenced */
u8 battle_party_panel_layout;  /* 800D3280: party panel layout */
s32 battle_heap_mark_for_event_script; /* 800D3284 */
s32 battle_list_page_scroll; /* 800D3288 */
s32 battle_heap_reserve_for_event_script; /* 800D328C */
s32 battle_unreferenced_word_10; /* 800D3290: unreferenced */
u8 battle_uses_fixed_party; /* 800D3294 */
u8 battle_atb_enabled;    /* 800D3298: ATB enabled */
void *battle_item_name_table; /* 800D329C: item name table */
struct SlotFlags battle_slot_flags[11]; /* 800D32A0 */
struct MemberCard *battle_member_cards[3]; /* 800D32F8 */
struct Tracker battle_light_trackers[2]; /* 800D3304 */
s32 battle_unreferenced_word_11; /* 800D332C: unreferenced */
s16 battle_panel_max_hp; /* 800D3330: panel member maximum HP */
s16 battle_trail_depth; /* 800D3334 */
u8 battle_continue_to_movie_mode;       /* 800D3338: (the resident's battle mode, ovl2596, ovl2615, ovl3087) */
s32 battle_panel_gear_hp;      /* 800D333C: panel gear HP */
void *battle_messages_of_event_script;    /* 800D3340: (ovl3087) */
SVECTOR *battle_scene_points; /* 800D3344: scene points */
s32 battle_scene_triangle_count;      /* 800D3348: scene triangle count */
void *battle_summary_window_prims;    /* 800D334C: the post-battle module's result summary (ovl2596) */
u8 battle_command_file_started;       /* 800D3350: the command file is started */
SVECTOR battle_camera_view_eye;  /* 800D3354: camera position */
SVECTOR battle_camera_view_target;  /* 800D335C: camera look-at point */
struct Formation *battle_formation; /* 800D3364 */
struct BattleObject *battle_objects[32]; /* 800D3368: stage objects */
/* The enemy files' disc read list (ovl2615): a file number and a destination
 * per entry, ended by file 0. */
u16 battle_enemy_read_list; /* 800D33E8 */
void *battle_enemy_read_list_destination0; /* 800D33EC */
u16 battle_enemy_read_list_file1; /* 800D33F0 */
void *battle_enemy_read_list_destination1; /* 800D33F4 */
u16 battle_enemy_read_list_end; /* 800D33F8 */
void *battle_enemy_read_list_end_destination; /* 800D33FC */
struct EnemyAi battle_enemy_ai_blocks[8]; /* 800D3400 */
struct ImageAnim battle_stage_image_anim; /* 800D3600: the stage's image animation */
s32 battle_popup_color_kind; /* 800D3630: the popup colour kind */
u16 battle_area_event_target_mask; /* 800D3634: the current event's targets */
u8 battle_screen_fade_blocked; /* 800D3638 */
struct Sprite *battle_area_event_target_sprites[11]; /* 800D363C: the current event's target sprites, NULL ended */
s32 battle_panel_gear_max_hp; /* 800D3668: panel gear maximum HP */
u8 battle_command_menu_sounds_enabled;  /* 800D366C: menu effects enabled */
u8 battle_item_menu_chosen_column;  /* 800D3670: item list column of the chosen item */
s32 battle_unreferenced_word_12; /* 800D3674: unreferenced */
s16 battle_area_event_target_count; /* 800D3678: the current event's target count */
void *battle_command_menu_module_block; /* 800D367C: menu module block */
s32 battle_total_popup_shown_value; /* 800D3680: the total shown, -1 none */
s32 battle_unreferenced_word_13; /* 800D3684: unreferenced */
u8 battle_gear_part_counts[0x30]; /* 800D3688: gear part counts */
u8 battle_start_mode;       /* 800D36B8: the battle's start mode */
s16 battle_pending_hit_count; /* 800D36BC */
u8 battle_unread_open_menu_member;  /* 800D36C0: the party member whose menu is open */
s32 battle_unreferenced_word_14; /* 800D36C4: unreferenced */
struct BattleMessage battle_message_entries[8]; /* 800D36C8 */
void *battle_enemy_set_copy; /* 800D39C8: the enemy set data copy */
struct SceneTriangle *battle_scene_triangles; /* 800D39CC: scene triangles */
struct EventScriptFile *battle_file_of_event_script; /* 800D39D0: (ovl3087) */
u8 battle_list_page_scroll_request; /* 800D39D4 */
void *battle_music_file_block; /* 800D39D8: (ovl2606) */
u16 battle_alive_mask;   /* 800D39DC: alive mask */
u16 battle_joint_action_slots;   /* 800D39E0: mask of slots that act together */
u16 battle_requested_single_action;   /* 800D39E4: the single action to request (71, 73) */
s16 battle_surface_wind_strength;   /* 800D39E8: a slow wave (4..9) */
struct Sprite *battle_camera_circled_sprite; /* 800D39EC: the sprite the camera circles */
void *battle_message_table; /* 800D39F0: battle message table */

#include "battle/action_file.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/command.h"
#include "battle/effect.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/groups.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/objects.h"
#include "battle/resolver.h"
#include "battle/scene.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/stage.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "action_resolve.h"
#include "curve.h"
#include "overlays.h"
#include "popup.h"
#include "sprite_effect.h"
