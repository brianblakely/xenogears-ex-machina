#ifndef FIELD_FIELD_MODE_H
#define FIELD_FIELD_MODE_H

/* The field mode (field.c): its frame, the requests that leave it (menus,
 * battles, map changes) and random encounters, the play record, and the
 * control words the events test before they let such a request through
 * (field_event.c). */

#include "common.h"
#include "field/monitor.h"
#include "field.h"

/* The field loop's control words; an event that requests a transition waits
 * until 800adbdc and 800adbe4 are set (with 800adbec for map changes and
 * battles) and 800adb2c and 800adb90 are clear. */
extern s32 field_mode_exit_not_requested;
extern s32 field_battle_not_requested;
extern s32 field_scripted_battle_not_requested;
extern s32 field_worldmap_exit_not_requested;
extern s32 field_arena_exit_not_requested;
extern s32 field_map_change_not_requested;         /* publish the field id on the next walk */
extern s32 field_skip_exit_snapshot;         /* set by event 800933f8: the exit skips the state save */
extern s32 field_encounter_battle_pending;         /* 1 when the field may leave (80077e10) */
extern s32 field_encounter_music_started;         /* set once the field left for a battle */
extern s32 field_menu_request;         /* requested menu (scripts 0-6, menu button 0x80), 0xff none */
extern u16 field_menu_parameter;         /* menu parameter set by ext 99; the field loop and ext 55
                                * pass it to the menu (80059171) */
extern s32 field_movie_requested;         /* movie requested */
extern s32 field_exit_request_pending;         /* an event's wait (8008a244 yields while it is set) */
extern s32 field_party_rebuilding;         /* set while the party is rebuilt: no effects or scrolls start */
extern s32 field_actor_block_loading;         /* an actor block is being read */
extern s32 field_map_change_frames;         /* map change: the transition's frames */
extern s32 field_map_change_kind;         /* map change: the transition kind */
extern s32 field_exit_game_mode;         /* the kind of the next exit (8007954c) */
extern s32 field_music_saved_for_battle;         /* 8004f324 as the field was left, restored on return */
extern s32 field_no_panorama_after_return;         /* after a movie or a menu, nonzero sets 800adb50 (80089f94) */
extern u8 field_encounters_enabled;          /* random encounters enabled */
extern u8 field_characters_hidden;          /* 1 while character drawing is off */
extern s16 field_wide_overlay_shown;         /* 1 once an event switched to the 640-wide screen */
extern s32 field_draw_second_ot_enabled;         /* set while the second ordering tables are drawn */
/* The frame times field_frame_start_time-field_frame_gpu_time are in field/monitor.h. */
extern s32 field_unread_cleared_word;         /* only cleared (800705dc) */

void field_enter_map(void);      /* the field entry */
void field_run_pre_frame(void);      /* the pre-frame work */
s32 field_run_post_frame(void);       /* the post-frame work */
void field_run_frame(void);      /* one field frame */
void field_update_events_and_actors(void);      /* the field update: events, then every actor's motion */
void field_draw_set_clip_areas(s32 x, s32 y, s32 w, s32 h); /* both draw buffers' clip areas */
void field_draw_set_display_areas(void);      /* both draw blocks' display areas */
void field_sync_draw_and_vsync(void);      /* DrawSync, then VSync */
void field_sync_and_flush_cache(void);      /* sync, then flush the instruction cache */
void field_brighten_text_strip(void);      /* brighten the text strip (with 800b2344 set) */
s32 field_encounter_count_down(void);       /* count down the random-encounter steps */
void field_update_gear_riding_lock(void);      /* set the battle-entry flag (80059179) */
s32 field_is_exit_blocked(void);       /* 0 when nothing keeps the field from leaving */
void field_run_menu(void);      /* run a menu over the field */
void field_exit_to_mode(s32 kind);  /* leave the field for another mode */
void field_reload_actor_blocks(void);      /* reload the actors' extra blocks after a return */
void field_event_empty_map_change_hook(void);      /* empty; the map-change events call it */
void field_finish_return_to_field(void);      /* after a return to the field */
void field_event_init_actors(void);      /* rebuild the actors after a return (mode_field_return_pending set) */

/* The field state the field writes to the resident's snapshot block
 * (mode_snapshot_block) when it leaves (800a3f4c) and reads back on return
 * (800a3474), through a cursor. */
extern u8 *field_snapshot_cursor;         /* the snapshot cursor */
void field_restore_snapshot(void);      /* read it back (the write, field_save_snapshot, is in
                                * field/monitor.h) */

/* The view's world block (800afa54, 0x74 bytes) as copied byte-wise. */
typedef struct {
    u8 bytes[0x74];
} ViewSnapshot;

/* Copy `size` bytes as one unaligned block (a byte-struct assignment). */
#define COPY_BLOCK(destination, source, size)                    \
    {                                                            \
        typedef struct {                                         \
            u8 bytes[size];                                      \
        } Block;                                                 \
        *(Block *)(destination) = *(Block *)(source);            \
    }

/* The play record (800a31e8). */
extern u8 field_play_record_stopped;          /* 1 stops the record */
extern u16 field_play_record_buttons;         /* buttons held since the last record */
void field_update_play_record(void);      /* update the play record */
void field_event_save_map_and_variables(void);      /* record the map and camera, save the event variables */

#endif
