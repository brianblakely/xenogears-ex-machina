#ifndef BATTLE_ACTOR_H
#define BATTLE_ACTOR_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* The slots' battle sprites (resident sprite-engine objects) and their tasks,
 * the gear objects that stand in for them, and the battle camera (800B8098's
 * unit, 800B9F7C-800BCB54). */

extern u8 battle_sprites_paused;           /* sprite updates paused */
extern s32 battle_camera_resume_mode;
extern u16 battle_gear_image_places_taken; /* the gear image places taken (battle_gear_image_places) */
extern s32 battle_gear_object_load_count;  /* gear object loads running */
extern u8 battle_gear_objects_loaded;      /* the gear objects are loaded */
extern u8 battle_camera_skip_gear_heights; /* frame the sprites without their gear heights */

/* The battle camera (800d309c); its view matrix is also named
 * battle_camera_view_matrix, its eye and look-at points
 * battle_camera_wanted_points, its angles battle_camera_angles and its range
 * battle_camera_distance. */
typedef struct BattleCamera {
    s32 field0;
    SVECTOR eye;    /* +04 */
    SVECTOR target; /* +0C */
    SVECTOR rot;    /* +14 */
    s32 range;      /* +1C */
    MATRIX matrix;  /* +20 */
    s32 cpu;        /* +40: the frame's CPU time: VSync(1) once its drawing is queued */
    s32 gpu;        /* +44: its GPU time: VSync(1) once the GPU finished */
    s32 start;      /* +48: VSync(-1) at the frame's start */
} BattleCamera;

extern BattleCamera battle_camera;
extern SVECTOR battle_camera_wanted_points[2]; /* the camera's wanted eye and look-at points */
extern SVECTOR battle_camera_angles;           /* the camera's angles (the debug overlay's 80280960 too) */
extern s32 battle_camera_distance;             /* the camera's distance */
extern MATRIX battle_camera_view_matrix;       /* the battle view matrix */
extern s32 battle_camera_ease_fraction;
extern s32 battle_camera_framed_slots;
extern SpriteTask *battle_camera_eye_task;     /* the eye sprite's task */
extern SpriteTask *battle_camera_target_task;  /* the look-at sprite's task */
extern SVECTOR battle_camera_framing_angles;   /* the camera's framing angles */
extern u8 battle_stage_drawing_off;            /* stage drawing off */
extern SVECTOR battle_camera_up_vector;        /* the camera's up vector */
extern Sprite *battle_camera_circled_sprite;   /* the sprite the camera circles */
extern s32 battle_camera_circle_distance;      /* its distance from it */
extern s16 battle_camera_circle_angle;         /* its angle round it */

/* The slots' sprites. */
void battle_end_turn_and_preload_slot(s32 slot);       /* end a slot's turn presentation */
void battle_sprite_aim_jump(Sprite *sprite);            /* aim a sprite's jump at its target */
void battle_sprite_aim_jump_keep_rise(Sprite *sprite);  /* the same, keeping its rising speed */
void battle_sprite_update_ground(Sprite *sprite);       /* put a sprite on the scene's ground */
/* A sprite task's callbacks (battle_sprite_task_create creates one; the event script
 * overlay its script slots' models): task_alloc_two_node_task's update, second update
 * and destroy. */
void battle_sprite_task_draw(Task *task);        /* second update: depth in the view, draw the parts */
void battle_sprite_task_destroy(Task *node);     /* destroy: part block, children, sprite and node */
void battle_sprite_task_update(Task *task);      /* update (twice with double steps) unless paused */
void battle_slot_sprite_face_side(s32 slot);     /* face a slot's sprite along its side */
void battle_slot_swap_sprite_for_gear(s32 slot); /* send a party slot's sprite off for its gear */
void battle_gear_load_start(s32 slot);           /* start loading a slot's gear object */

/* The camera. */
void battle_set_view_and_draw_stage(void); /* set the battle view and draw the stage */
void battle_camera_step(void);             /* step the battle camera */
void battle_camera_set_mode(s32 mode);     /* set the camera mode */
void battle_camera_set_resume_mode(s32 value);
void battle_camera_start_move(s32 mask);   /* start a camera move */
void battle_camera_hold(void);             /* camera mode 4 */
void battle_camera_track_slots(void);      /* camera mode 1 */

#endif
