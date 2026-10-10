#ifndef WORLDMAP_CAMERA_H
#define WORLDMAP_CAMERA_H

/* The world map camera: its target, angle and distance, the view it is built from
 * (an orbit by angle, worldmap_camera_build_from_angles, or a look-at view,
 * worldmap_camera_build_look_at), the scripted camera helpers of the scene modes
 * and the open map's camera actors (worldmap_steering_camera_terrain). */

#include "worldmap.h"

/* Camera: its target (20.12). */
typedef struct Camera {
    VECTOR target;
} Camera;

extern Camera worldmap_camera;               /* the camera */
extern Camera worldmap_camera_follow_target; /* the target the camera follows (the player's) */
extern SVECTOR worldmap_camera_angle;        /* camera angle */
extern s32 worldmap_camera_distance;         /* camera distance */
extern s32 worldmap_view_center_y;           /* screen y of the view's centre (SetGeomOffset) */
extern s32 worldmap_view_kind;               /* view kind: 0 by angle, 1 look-at */
extern MATRIX worldmap_camera_matrix;        /* camera matrix */

/* The view setup at worldmap_view_setup: eye and look-at points and the up vector,
 * which the orbit placement (worldmap_camera_place_orbit) writes and the look-at camera
 * (worldmap_camera_build_look_at) reads; some camera actors swap its two view vectors per
 * frame. */
typedef struct ViewSetup {
    SVECTOR eye;
    SVECTOR at;
    VECTOR up;
} ViewSetup;

extern ViewSetup worldmap_view_setup;
#define VIEW worldmap_view_setup
#define VIEW_VECTORS ((SVECTOR *)&worldmap_view_setup) /* two view vectors, swapped per frame */

/* Scratchpad work area of the camera steering. */
typedef struct {
    VECTOR delta;     /* 0x00 */
    u8 pad10[0x90];
    SVECTOR view;     /* 0xA0: swap space */
} CameraScratch;

void worldmap_camera_place_orbit(ViewSetup *view, Camera *camera, s32 distance, SVECTOR *angle);
void worldmap_get_matrix_angles(MATRIX *m, SVECTOR *angle); /* matrix to angles */
void worldmap_camera_build_look_at(void *);                 /* look-at camera matrix */
void worldmap_camera_build_from_angles(void *);             /* camera matrix by angle */
void worldmap_eval_quadratic_bspline(s32 t, SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, VECTOR *out);

/* Scripted camera easing (worldmap_open_map): an eighth of the way to the
 * actor's target angle, distance or position at a time. Declared without
 * parameters: the callers pass each the actor and their work vectors,
 * worldmap_camera_ease_distance too, which takes the actor alone. */
void worldmap_camera_ease_angle(), worldmap_camera_ease_distance(), worldmap_camera_ease_position();
s32 worldmap_step_value_toward(s32 value, s32 target, s32 delta);

/* The open map's camera actors (worldmap_steering_camera_terrain), start and update: the
 * follower of the player, the orbit distance and pitch, and the screen
 * height of the view's centre (worldmap_view_center_y). */
s32 worldmap_camera_follow_start(s32 index), worldmap_camera_follow_update(s32 index);
s32 worldmap_camera_orbit_start(s32 index), worldmap_camera_orbit_update(s32 index);
s32 worldmap_view_center_start(s32 index), worldmap_view_center_update(s32 index);

#endif
