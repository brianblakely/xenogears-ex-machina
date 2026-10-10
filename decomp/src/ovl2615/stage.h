#ifndef OVL2615_STAGE_H
#define OVL2615_STAGE_H

/* The stage setup (stage.c): the stage file, model, scene geometry, part
 * animations and backdrop. The battle overlay's objects and calls it uses
 * come from the shared battle headers (the stage objects and their model
 * parts, the scene's points and triangles, the effect pool, the stage's
 * images and colours); the scene data from scene.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/model.h"
#include "battle/objects.h"
#include "battle/resolver.h"
#include "battle/scene.h"
#include "battle/stage.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "resident/text.h"
#include "ovl2615.h"
#include "scene.h"

/* Callers convert arguments/result differently from the resident definition:
 * a panorama (u16 arguments where it takes words and the reverse, and the
 * stage object, which starts with its VECTOR position), and a texture scroll
 * of `count` bands (words where it takes s16 and u16). */
Panorama *gpu_create_panorama(u16 tex_x, u16 tex_y, u16 width, u16 height, u16 clut_x, u16 clut_y,
                        u16 mode, u16 turn, StageObject *object, void *colours, s32 fill_scale,
                        s32 fade_range, s32 fade_start);
void gpu_init_texture_scroll(TextureScroll *scroll, s32 x, s32 y, s32 w, s32 h, s32 count, s32 source_x,
                   s32 source_y, void *speeds);

/* The battle object list's last entry (battle_objects) is the stage model. */
#define STAGE_MODEL 31

/* The stage file: its texture image list and part positions. */
typedef struct {
    s16 x, y, z;
    u16 rotation;
} PartPosition;

typedef struct {
    u8 pad0[4];
    s32 *images;          /* 0x04 */
    u8 pad8[0x14 - 8];
    PartPosition *positions; /* 0x14 */
} StageFile;

extern s16 battle_stage_image_anim_active;
/* The battle's calls with the stage's conversions: start the stage model's
 * effect script list, and pose its hierarchy (no result). */
void battle_reset_object(BattleObject *object, EffectPool *pool, void *motion, s32 animations);
void battle_pose_model_hierarchy(ModelPart *parts, s32 scale);

/* The stage backdrop (func_801E7914, 0x17cc bytes): a floor grid of 9 x 9
 * vertices and 128 tiles, and the fills and fades around it. */
typedef struct {
    s16 x;                    /* 0x00 */
    s16 y;                    /* 0x02 */
    s16 width;                /* 0x04 */
    s16 height;               /* 0x06 */
    s16 v08;                  /* 0x08 */
    s16 v0A;                  /* 0x0A */
    s16 v0C;                  /* 0x0C */
    s16 v0E;                  /* 0x0E */
    s16 v10;                  /* 0x10 */
    s16 v12;                  /* 0x12 */
    s16 position[3];          /* 0x14: the object's position */
    s16 pad1A;
    CVECTOR colours[2];       /* 0x1C */
    DR_MODE modes[4];         /* 0x24 */
    SVECTOR grid[81];         /* 0x54 */
    POLY_FT4 tiles[128];      /* 0x2DC */
    POLY_F4 fills[4];         /* 0x16DC */
    POLY_G4 fades[4];         /* 0x173C */
} StageBackdrop;

StageBackdrop *func_801E7914(s16 texX, s16 texY, s16 width, s16 height, s16 size, s16 step,
                             s16 v0A, s16 clutX, s16 clutY, s16 v10, s16 v12, VECTOR *position,
                             CVECTOR *colour, s16 v0C, s16 v0E);
void func_801E7EC4(SVECTOR *points, SceneTriangle *triangles, s32 count);

#endif
