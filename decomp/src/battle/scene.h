#ifndef BATTLE_SCENE_H
#define BATTLE_SCENE_H

#include "common.h"
#include "model.h"

/* Presentation event queue slot (0x48 bytes, from D_800C3FE8). */
typedef struct {
    u16 amounts[11];
    u16 targetMask;         /* 0x16 */
    u8 codes[11];           /* 0x18 */
    u8 actor;               /* 0x23 */
    u16 accumulated[11];    /* 0x24 */
    u16 parameter;          /* 0x3A */
    u8 accumulatedCodes[11]; /* 0x3C */
    u8 type;                /* 0x47: 0xF7 continues, 0xFF ends */
} BattleEvent;

/* A stage object (fields as far as recovered). */
typedef struct {
    u8 pad0[4];
    ModelPart *hierarchy;   /* 0x04 */
    u8 pad8[0x1C - 0x8];
    s16 scale1C;            /* 0x1C */
    u8 pad1E[0x24 - 0x1E];
    s16 scale24;            /* 0x24 */
    u8 pad26[0x2A - 0x26];
    u8 field2A;             /* 0x2A */
    u8 pad2B[0x34 - 0x2B];
    u8 active;              /* 0x34 */
} StageObject;

/* An effect sprite record (0x7C bytes) of a sprite pool. */
typedef struct {
    u8 pad0[0x16];
    s16 id;                 /* 0x16: -1 free */
    u8 pad18[0x7C - 0x18];
} SpriteRecord;

/* A pool of sprite records; next is the first record that may be free. */
typedef struct {
    SpriteRecord *records;
    s16 count;
    s16 next;
} SpritePool;

/* An effect object (fields as far as recovered). */
typedef struct {
    u8 pad0[0x98];
    s16 field98;            /* 0x98: -1 unset */
} EffectObject;

/* Battle scene and effect state. */
extern void *D_800D3344;                /* scene actor records */
extern void *D_800D39CC;                /* scene light entries */
extern u8 D_800C3B74;
extern u8 D_800C3D6C;
extern s32 D_800D2D40;
extern s32 D_800D2D48;
extern s32 D_800C3BAC[9];
extern StageObject *D_800D3368[];        /* stage objects */
extern BattleEvent D_800C3FE8[];        /* presentation events */
extern u8 *D_800C3BEC;                  /* effect script cursor */
extern s32 D_800C3BF0;                  /* effect script step count */
extern u16 D_800C3E30;                  /* slot mask */

#endif
