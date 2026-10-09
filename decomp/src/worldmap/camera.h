#ifndef WORLDMAP_CAMERA_H
#define WORLDMAP_CAMERA_H

/* The world map camera: its target, angle and distance, the view it is built
 * from (an orbit by angle, func_80097440, or a look-at view, func_80097244),
 * the scripted camera helpers of the scene modes and the open map's camera
 * actors (worldmap_80090A84). */

#include "worldmap.h"

/* Camera: its target (20.12). */
typedef struct Camera {
    VECTOR target;
} Camera;

extern Camera D_8009BE28; /* the camera */
extern Camera D_8009D55C; /* the target the camera follows (the player's) */
extern SVECTOR D_8009BD38; /* camera angle */
extern s32 D_8009D3F0;    /* camera distance */
extern s32 D_8009BE0C;    /* screen y of the view's centre (SetGeomOffset) */
extern s32 D_8009D144;    /* view kind: 0 by angle, 1 look-at */
extern MATRIX D_8009C808; /* camera matrix */

/* The view setup at D_8009BD40: eye and look-at points and the up vector,
 * which the orbit placement (func_80096F18) writes and the look-at camera
 * (func_80097244) reads; some camera actors swap its two view vectors per
 * frame. */
typedef struct ViewSetup {
    SVECTOR eye;
    SVECTOR at;
    VECTOR up;
} ViewSetup;

extern ViewSetup D_8009BD40;
#define VIEW D_8009BD40
#define VIEW_VECTORS ((SVECTOR *)&D_8009BD40) /* two view vectors, swapped per frame */

/* Scratchpad work area of the camera steering. */
typedef struct {
    VECTOR delta;     /* 0x00 */
    u8 pad10[0x90];
    SVECTOR view;     /* 0xA0: swap space */
} CameraScratch;

void func_80096F18(ViewSetup *view, Camera *camera, s32 distance, SVECTOR *angle);
void func_80097070(MATRIX *m, SVECTOR *angle); /* matrix to angles */
void func_80097244(void *);                    /* look-at camera matrix */
void func_80097440(void *);                    /* camera matrix by angle */
void func_80076858(s32 t, SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, VECTOR *out);

/* Scripted camera easing (worldmap_80072238): an eighth of the way to the
 * actor's target angle, distance or position at a time. Declared without
 * parameters: the callers pass each the actor and their work vectors,
 * func_80076F54 too, which takes the actor alone. */
void func_80076DA4(), func_80076F54(), func_80076FA8();
s32 func_800771D8(s32 value, s32 target, s32 delta);

/* The open map's camera actors (worldmap_80090A84), start and update: the
 * follower of the player, the orbit distance and pitch, and the screen
 * height of the view's centre (D_8009BE0C). */
s32 func_80091430(s32 index), func_800914D0(s32 index);
s32 func_80091B54(s32 index), func_80091C18(s32 index);
s32 func_80092234(s32 index), func_800922AC(s32 index);

#endif
