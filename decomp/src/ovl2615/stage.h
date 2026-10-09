#ifndef OVL2615_STAGE_H
#define OVL2615_STAGE_H

/* The stage setup (stage.c): the stage file, model, lights, part animations
 * and backdrop, and the battle overlay objects and calls they use. The
 * scene data comes from scene.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
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
Panorama *func_8002709C(u16 tex_x, u16 tex_y, u16 width, u16 height, u16 clut_x, u16 clut_y,
                        u16 mode, u16 turn, StageObject *object, void *colours, s32 fill_scale,
                        s32 fade_range, s32 fade_start);
void func_80027D64(TextureScroll *scroll, s32 x, s32 y, s32 w, s32 h, s32 count, s32 source_x,
                   s32 source_y, void *speeds);

/* Stage light entry (0x0E bytes). */
typedef struct {
    u8 pad0[0xD];
    u8 active;
} StageLight;

extern void *D_800D3344;        /* stage actors */
extern StageLight *D_800D39CC;  /* stage light entries */
extern s32 D_800D3348;          /* stage light count */
extern u8 D_800D2F64;

extern s16 D_800D2D30; /* stage texture bounds: left */
extern s16 D_800D2D34; /* top */
extern s16 D_800D2D2C; /* width */
extern s16 D_800C3EA8; /* height */

/* A model part (0x7c bytes); the first part heads the model and holds the
 * part count. */
typedef struct {
    u8 pad0[0xA];
    u16 count;            /* 0x0A */
    u8 padC[0x52 - 0xC];
    u16 rotation;         /* 0x52 */
    u8 pad54[0x5C - 0x54];
    s32 x;                /* 0x5C */
    s32 y;                /* 0x60 */
    s32 z;                /* 0x64 */
    u8 pad68[0x7C - 0x68];
} ModelPart;

/* The stage model record. */
typedef struct {
    void *model;          /* 0x00 */
    ModelPart *parts;     /* 0x04 */
    u8 pad8[0x1C - 8];
    s16 pose;             /* 0x1C */
} StageModel;
/* The battle object list (800d3368); its last entry is the stage model. */
extern StageModel *D_800D3368[32];
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

extern ModelPart *D_800C3E38;   /* the stage model's parts */
extern void *D_800C3E48;
extern void *D_800C3EA0;        /* the stage backdrop */
extern Panorama *D_800C3D50[2]; /* the stage's panoramas (object types 1, 2) */
extern TextureScroll D_800C3DA0[2]; /* texture scrolls of stage objects of type 7 */
extern s16 D_800D361A;
extern u8 *D_800D2FD0;
extern u16 D_800D2FC8;
extern s16 *D_800D2FC0;         /* the stage colour matrix */
extern u8 D_800D2D10[4];
/* The slots' AI flags (battle_setup.h), passed to the stage model calls. */
extern struct BattleAiFlags D_800C3D0C;
void func_800AA898(StageModel *model, void *state, void *motion, s32 a3);
void func_800AA934(StageModel *model, StageModel *model2, void *state, s32 a3);
void func_8009EF3C(ModelPart *parts, s32 pose);

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
void func_801E7EC4(void *actors, StageLight *lights, s32 count);

#endif
