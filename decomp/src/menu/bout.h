#ifndef MENU_BOUT_H
#define MENU_BOUT_H

#include "common.h"
#include "actor.h"

/* The bout (menu3 80075060-80075748, 80079A8C-8007B210 but the camera's
 * functions; menu5 80083CE8): rounds, the referee, the replay of a round's
 * end and the result view. */

extern s32 arena_debug_enabled;          /* debug: pad camera tuning, both sides' move names */
extern u8 arena_rubber_band_enabled;     /* rubber band battle (an options page setting) */
extern s32 arena_bout_round_winner;      /* who was knocked out: 0 the second actor, 1 the first, 2 both */
extern s32 arena_bout_replay_timer;      /* replay frames left (0xFF: replay over) */
extern u8 arena_bout_pose_ring_index;    /* frame of the recorded poses */
extern u8 arena_bout_fight_active;       /* the fight is on (from "FIGHT!!" to the knock-out) */
extern u8 arena_bout_unread_byte;
extern u8 arena_camera_side_flipped;     /* the actors face the other way */
extern s32 arena_bout_unread_draw_count; /* draws */
extern s32 arena_bout_motion_speed;      /* 0x100 at each round's start */
extern s32 arena_bout_fight_frame_count; /* frames the bout has run */
extern s32 arena_bout_round_frame_count; /* frames since the round started (the replay's length) */
extern s32 arena_bout_round_number;      /* round number */

void arena_bout_init(void);
void arena_bout_clear_counters(void);
s32 arena_bout_start_round(void);
void arena_bout_update(Actor *first, Actor *second);
void arena_bout_start_replay(s32 frames);
void arena_bout_update_replay(Actor *first, Actor *second);
void arena_bout_start_result_view(void);
void arena_bout_update_result_view(Actor *first, Actor *second);
void arena_bout_draw_elapsed_time(void);

#endif
