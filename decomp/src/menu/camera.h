#ifndef MENU_CAMERA_H
#define MENU_CAMERA_H

#include "common.h"
#include "psyq/libgte.h"
#include "actor.h"

/* The camera (arena_camera_and_scenes 800707A8-80070F80; arena_fighters_bout_and_effects 800796B8, 8007A768,
 * 8007A958; arena_stage_views_and_hud 800831C8-80083CE8 but 80083BB4): its eye and look-at
 * point, the view modes, the idle orbit and the camera/scene modes. */

extern u8 arena_camera_ease_step_count;      /* idle camera */
extern s32 arena_camera_view_mode;           /* camera view */
extern s32 arena_camera_side_angle;          /* side of the actors' line the eye takes (+-0x400) */
extern VECTOR arena_camera_focus;            /* the look-at point */
extern VECTOR arena_camera_position;         /* the eye */

void arena_scene_close_choice(void);
s32 arena_camera_ease_step(s32 target, s32 current, s32 steps);
void arena_camera_ease_focus(VECTOR *target, s32 steps);
void arena_camera_ease_position(VECTOR *target, s32 steps);
void arena_camera_apply_view_mode(u32 mode);
void arena_camera_frame_actors_for_bout(Actor *first, Actor *second);
void arena_camera_start_victory_view(Actor *actor);
void arena_camera_place_at_random(void);
void arena_camera_turn_orbit(s32 buttons);
void arena_camera_update_orbit(s32 smooth);
void arena_camera_start_orbit(void);
void arena_camera_frame_actors_for_scene(Actor *first, Actor *second);
void arena_mode_set_state(s32 mode);
s32 arena_mode_get_state(void);

#endif
