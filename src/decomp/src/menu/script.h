#ifndef MENU_SCRIPT_H
#define MENU_SCRIPT_H

#include "common.h"
#include "resident/window.h"
#include "actor.h"
#include "node.h"
#include "mode.h"

/* The arena's scenes (arena_camera_and_scenes 80070F80-80072D18): the scene script
 * interpreter (its scripts are assets, docs/scripts/arena-scene.md), the
 * opening and the scene list, the message window, the bout-end sequence
 * and the winner screen. */

/* A step in one of eight directions on the floor plane. */
typedef struct {
    s32 x;
    s32 z;
} FloorStep;

extern u8 arena_scene_opening_script[];           /* the opening's scene script */
extern u8 *arena_scene_scripts[];                 /* scene scripts */
extern FloorStep arena_scene_floor_steps[8];
extern u8 arena_scene_bout_end_script[];          /* the setup script */
extern LightRig *arena_winner_light_rig;          /* the scene's lights */
extern Actor *arena_scene_driven_actor;           /* actor the scene script drives */
extern s32 arena_scene_bout_end_step;             /* bout-end sequence step */
extern u8 arena_scene_bout_end_active;            /* the one-time scene setup ran */
extern Window arena_scene_message_window;         /* message window */

void arena_scene_start_script(u8 *script);
s32 arena_scene_run_script(void);
void arena_scene_draw_marker(u32 *ot);
void arena_scene_load_sprite_sheet(MenuImageFile *files);
void arena_scene_open_message_window(void);
void arena_scene_enter(s32 scene);
void arena_scene_start_tutorial(void);
void arena_scene_update(void);
void arena_scene_settle_actor_on_floor(Actor *actor);
s32 arena_scene_run_callback(s32 command); /* the scene callbacks */
void arena_scene_allow_bout_end(void);
void arena_scene_start_bout_end(void);
void arena_scene_update_bout_end(void);
void arena_winner_open_screen(Actor *scene);
void arena_winner_update_screen(LightRig *rig);

#endif
