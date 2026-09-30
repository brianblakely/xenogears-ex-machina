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

/* A battle object: a stage object or an effect (fields as far as
 * recovered). */
typedef struct {
    u8 pad0[4];
    ModelPart *hierarchy;   /* 0x04 */
    u8 pad8[0xC - 0x8];
    void *packets;          /* 0x0C */
    u8 pad10[0x18 - 0x10];
    s32 field18;            /* 0x18 */
    s16 scale1C;            /* 0x1C */
    u8 pad1E[0x24 - 0x1E];
    s16 scale24;            /* 0x24 */
    u8 pad26[0x2A - 0x26];
    u8 field2A;             /* 0x2A */
    u8 pad2B[0x34 - 0x2B];
    u8 active;              /* 0x34 */
    u8 field35;             /* 0x35 */
    u8 pad36[0x63 - 0x36];
    u8 hasTexture;          /* 0x63 */
    u8 pad64[0x88 - 0x64];
    s16 position[3];        /* 0x88 */
    u8 pad8E[0x98 - 0x8E];
    s16 animation;          /* 0x98: -1 none */
    s16 animationLoop;      /* 0x9A: -1 none */
    u16 animationFrame;     /* 0x9C */
    u16 animationLength;    /* 0x9E */
    u8 *animationStart;     /* 0xA0 */
    u8 *animationCursor;    /* 0xA4 */
    u8 padA8[0xB4 - 0xA8];
    u8 *textureInfo;        /* 0xB4 */
} BattleObject;

/* An animation header (fields as far as recovered). */
typedef struct {
    u8 pad0[2];
    u16 loop;               /* 0x02 */
    u8 pad4[0x12 - 0x4];
    u16 length;             /* 0x12 */
    u32 dataOffset;         /* 0x14 */
} Animation;

/* The battle scene data (fields as far as recovered). */
typedef struct {
    u8 pad0[0x348];
    s16 effectCount;        /* 0x348 */
    s16 spriteCount;        /* 0x34A */
    s16 maxX;               /* 0x34C */
    s16 minX;               /* 0x34E */
    s16 minZ;               /* 0x350 */
    s16 maxZ;               /* 0x352 */
} BattleSceneData;

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

/* Battle scene and effect state. */
extern void *D_800D3344;                /* scene actor records */
extern void *D_800D39CC;                /* scene light entries */
extern u8 D_800C3B74;
extern u8 D_800C3D6C;
extern s32 D_800D2D40;
extern s32 D_800D2D48;
extern EffectEntry *D_800C3BAC[9];
extern BattleObject *D_800D3368[];      /* stage objects */
extern BattleEvent D_800C3FE8[];        /* presentation events */
extern u8 *D_800C3BEC;                  /* effect script cursor */
extern s32 D_800C3BF0;                  /* effect script step count */
extern u16 D_800C3E30;                  /* slot mask */
extern s16 D_800C3D40;
extern EffectPool D_800C3D0C;
extern BattleSceneData *D_800658C8;

s32 func_80048C4C(s32 value);           /* square root */
void func_8003852C(u8 *texture);
void func_800AA934(BattleObject *object, BattleObject *target, EffectPool *pool, s32 arg3);
void func_800B00D0(void);

#endif
