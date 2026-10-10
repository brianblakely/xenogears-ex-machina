#ifndef RESIDENT_MODE_H
#define RESIDENT_MODE_H

#include "common.h"
#include "gpu.h"
#include "cd.h"
#include "formation.h"

/* Resident startup and the mode dispatcher (0x80019524-0x80019d48). */

/* Mode table 8001808c: the mode's entry, the BSS it clears and whether its
 * overlay file is loaded into the mode block before the call. */
typedef struct {
    void (*entry)(void);
    u8 *bss_start;
    u8 *bss_end;
    s32 loaded;
} ModeEntry;

/* Kernel menu (mode 0) double buffer, 8005 95e8. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u_long ot[1];
    POLY_F3 cursor;
} KernelBuffer;

/* An 8x8 tile of the debug Game of Life screen (a TILE_8 primitive). */
typedef struct {
    u32 tag;
    u32 rgbc;
    u32 xy;
} LifeTile;

extern s32 mode_kernel_menu_cursor;           /* kernel menu cursor */
extern s32 *mode_disc_mode_pointer;
extern u8 *mode_battle_stage_file;  /* the battle stage file (mode_load_battle_stage; ovl2615 func_801E7210) */
extern s32 mode_battle_stage_unused_word;
extern u8 *mode_battle_scene_file;  /* the battle scene data (mode_load_battle_stage; ovl2615 func_801E7210) */

/* Game state reset by 8001aadc. */
extern s32 mode_unread_play_record_word, mode_field_entered_once, mode_music_cached_seq, mode_staff_roll_enabled, mode_worldmap_area_load_count, mode_music_load_pending;
extern s32 mode_field_return_pending, mode_unread_arena_departure_count, mode_unread_reset_word, mode_play_clock_frame_count, mode_party_file_kind, mode_party_uses_gear_files;
extern s32 mode_music_selected_track, mode_play_clock_flags, mode_effect_bank_not_preloaded, mode_read_ahead_map, mode_read_ahead_slot, mode_music_loaded_track;
extern s32 mode_music_loaded_wave, mode_music_start_full_volume, mode_text_images_preloaded, mode_music_reuse_seq, mode_field_map_id, mode_menu_request_count;
extern s32 mode_music_wave_streaming, mode_music_seq_read_pending, mode_music_seq_active, mode_music_wave_bank_loaded, mode_shared_wave_bank_state, mode_shared_wave_bank_released;
extern s32 mode_music_started, mode_field_standalone, mode_party_files_pending, mode_debug_hide_compass, mode_debug_hide_sprites, mode_debug_hide_layer;
extern s16 mode_shared_wave_bank_needs_reload;
extern u8 mode_battle_return_fade;
extern u8 mode_result_code;
extern s32 mode_party_actors[3];
extern s32 mode_wave_bank_slots[4]; /* loaded wave bank per slot */
extern s32 mode_party_members[3];
extern s32 mode_party_stand_in_actors[3];
extern s32 mode_party_file_ids[3];
extern FileRequest mode_party_file_list[4]; /* party file list, zero-terminated */
extern FileRequest mode_battle_file_list[4]; /* the battle mode's sound files (8001bbac) */
extern void *mode_party_file_blocks[3];       /* party character file blocks */
extern void *mode_preloaded_text_images;          /* file 0xa7 block */
extern void *mode_preloaded_effect_bank;          /* file 0xa8 block */
extern void *mode_party_sprite_blocks[3];       /* party field sprite blocks */
extern s32 mode_read_ahead_size;            /* map read-ahead size */
extern void *mode_read_ahead_block;          /* map read-ahead block */
extern s32 mode_music_seq;            /* the active sequence */
extern struct SoundSequence *mode_music_wave_bank; /* the transferred wave bank */

extern u8 *const mode_overlay_decode_destination; /* overlay decode destination */
extern u8 boot_bss_last_word[];      /* the last word below the overlay area */
extern s32 mode_next_mode;       /* next mode */
extern ModeEntry mode_table[];
extern s32 mode_overlay_files[];     /* each mode's overlay file in directory 1 */
extern struct SoundSequence *mode_shared_wave_bank; /* resident wave banks */
extern struct SoundSequence *mode_wave_bank_5;
extern s32 mode_disc_mode;
extern u8 boot_packed_logo[];      /* compressed boot logo image */
extern char *mode_fatal_error_messages[];   /* messages of the fatal errors 0x80-0x85 */
extern s32 mode_fatal_error_count;       /* fatal error count */
extern u8 cd_disc_files[];
extern u16 cd_disc_directories[];

/* Original hand-written startup code. */
void boot_entry_point(void);               /* entry: clear the BSS, reset the stack, boot */
void boot_reset_stack_and_gp(void);               /* sp = fp = 0x80200000, gp = _gp */
void boot_clear_bss_range(u8 *start, u8 *end); /* zero the words after start through end */

void mode_run_kernel_menu(void); /* mode 0, the kernel menu */
void mode_run_battle(void); /* mode 2 */
void mode_battle_init_display(void); /* the battle's display buffers and projection */
void mode_run_menu(void); /* mode 5 */
void mode_select_next_mode(s32 mode);
void *mode_load_overlay_block(s32 mode);
void mode_dispatch(s32 error) __attribute__((noreturn));
void mode_select_default_heap_tag(void);
void boot_restart(void);
void boot_show_logo(void);
void mode_show_fatal_error(s32 error, u32 caller);
void mode_kernel_menu_init(void);
void mode_reset_game_state(void);
void boot_empty_step(void);
void mode_reload_party_files(s32 extra);
void mode_unpack_party_files(void);
void mode_start_map_read(s32 map);
void mode_load_party_character_files(void);
void mode_load_party_gear_files(void);
void mode_wait_for_disc_idle(void);
s32 mode_get_character_gear_id(s32 index);
void mode_init_game_data(void);
void sprite_reset_engine(void);

/* More of the mode dispatcher's calls and state. */
/* An empty debug print: the menu passes it a message. */
void mode_empty_debug_print();
void boot_check_soft_reset(void);
void mode_clear_field_return(void);
void mode_preload_field_files(void);
void mode_sync_party_files(void);
s32 mode_read_map_ahead(s32 map, s32 slot);
void mode_stop_music(void);
void mode_load_initial_game_data(void);
void mode_load_current_battle_stage(void);
void mode_battle_load_files(void);
extern u8 pad_port0_left_stick_x;
extern u8 pad_port1_left_stick_x;
extern u8 pad_port0_left_stick_y;
extern u8 pad_port1_left_stick_y;
extern s32 *mode_battle_action_command_file;
extern u8 mode_battle_standalone;
extern u8 mode_battle_debug_page;
extern void *mode_field_layer_script_files[4];
extern void *mode_field_layer_model_files[4];
extern s32 mode_field_last_moved_actor;
extern u8 mode_music_buffer[0x3200]; /* a work buffer of the field, the world map, battle and its overlays */
extern s32 mode_field_pointer_state[5];
extern u8 mode_pending_battle_formation; /* the next battle's formation + 1 (formation.h) */
extern u8 mode_gear_riding_lock; /* the battle-entry flag (the field and world map set it) */
extern s16 mode_party_gear_refresh_flags[3]; /* per party slot (the field) */
extern u8 mode_result_fanfare_started; /* battle music playing */

#endif
