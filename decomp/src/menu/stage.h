#ifndef MENU_STAGE_H
#define MENU_STAGE_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "actor.h"
#include "node.h"

/* The arena (menu5 80081ECC-800831C8, 80083DCC-80084BEC, 800875EC-80087E38;
 * menu2's handwritten 80072D18): the stage colours, the floor and its
 * height map, the backdrop, the wall and the actors' shadows, the map
 * triangles and row spans, and the drawing of the 3D views. */

/* Stage colours (17 bytes each). */
typedef struct Environment {
    u8 top[3];         /* sky gradient top */
    u8 unk3;
    u8 unk4, unk5, unk6;
    u8 unk7;
    u8 bottom[3];      /* sky gradient bottom, far and fade colour */
    u8 unkB;
    u8 back[3];        /* back colour */
    u8 unkF;
    u8 dim;            /* 0x10: halve the actor glow */
} Environment;

/* Ground height map: 128 columns of 256-unit squares, 4 bytes each. */
typedef struct GroundSquare {
    u16 height;
    u16 unk2; /* map renderer: UV high nibbles 0xF0F0, orientation bits 0..1,
               * texture-page/CLUT selector bits 2..3 */
} GroundSquare;

/* The stage file: its TIM images (floor, backdrop, icons, name and bars). */
typedef struct {
    u8 unk0[0x38];
    void *backdrop_tim;   /* 0x38 */
    u8 unk3C[0xC];
    void *icon_tims[4];   /* 0x48 */
    void *name_tim;       /* 0x58 */
    void *bar_tim;        /* 0x5C */
    u8 unk60[0xC];
    void *floor_tim;      /* 0x6C */
    void *extra_tims[9];  /* 0x70 */
} StageFiles;

/* Map drawing table copied into the scratchpad; ends with the icons. */
typedef struct {
    u16 uv[4][4];      /* four orientations: upper-left/right, lower-left/right */
    u16 icons[8];      /* 0x20: per icon its texture page, then its palette */
} MapTable;

extern Environment D_8009178C[];
extern u8 D_80091834[];          /* per map row: leftmost allowed column */
extern u8 D_800918B4[];          /* per map row: rightmost allowed column */
extern MapTable D_80091934;
extern POLY_FT3 *D_80092854[2];  /* map triangle pool per draw buffer */
extern Environment *D_8009288C;  /* current stage colours */
extern s32 D_800928B0;           /* selects the look-at marker (func_80082300 or func_80082178) */
extern u8 D_800928B4;            /* stage */
extern GroundSquare *D_800928DC; /* the height map */
extern s32 D_80092908;           /* back colour blue */
extern s32 D_80092910;           /* back colour green */
extern s32 D_8009291C;           /* back colour red */

/* Draw selected map cells using scratchpad row spans and MapTable.
 * Return the emitted triangle count; loaded GTE view/depth-cue state is used. */
u32 func_80072D18(u32 *ot, s32 originX, s32 originZ);
void func_80081ECC(void);
void func_80082458(SVECTOR *out);
s32 func_80082488(VECTOR *pos, s32 lift);
s32 func_800828C4(VECTOR *pos);
void func_800828F8(VECTOR *pos, VECTOR *step, s32 radius);
void func_80082C4C(StageFiles *files);
void func_800875EC(void);
void func_80087698(s32 x0, s32 y0, s32 x1, s32 y1); /* widen the map's row spans along a line */
void func_8008779C(u32 *ot, s32 originX, s32 originZ);
void func_80087830(void);
void func_80087AB0(Actor *actor);

#endif
