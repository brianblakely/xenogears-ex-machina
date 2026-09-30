#ifndef BATTLE_SCENE_H
#define BATTLE_SCENE_H

#include "common.h"
#include "model.h"

/* Presentation event queue slot (0x48 bytes, from D_800C3FE8). */
typedef struct {
    u16 amounts[11];
    u16 targetMask;          /* 0x16 */
    u8 codes[11];            /* 0x18 */
    u8 actor;                /* 0x23 */
    u16 accumulated[11];     /* 0x24 */
    u16 parameter;           /* 0x3A */
    u8 accumulatedCodes[11]; /* 0x3C */
    u8 type;                 /* 0x47: 0xF7 continues, 0xFF ends */
} BattleEvent;

/* A battle object: a stage object or an effect (fields as far as
 * recovered). */
typedef struct {
    s32 field0;           /* 0x00: nonzero in use */
    ModelPart *hierarchy; /* 0x04 */
    s32 field8;           /* 0x08 */
    void *packets;        /* 0x0C */
    s32 field10;          /* 0x10 */
    u8 **animations;      /* 0x14: count, then animations 0-63 */
    u8 **moreAnimations;  /* 0x18: animations from 64 */
    s16 scale1C;          /* 0x1C */
    s16 field1E;          /* 0x1E */
    u8 slot;              /* 0x20 */
    u8 slot2;             /* 0x21 */
    u8 pad22;
    u8 field23;  /* 0x23 */
    s16 scale24; /* 0x24 */
    s16 scale26; /* 0x26 */
    s16 scale28; /* 0x28 */
    u8 field2A;  /* 0x2A */
    u8 field2B;  /* 0x2B */
    u8 pad2C[0x34 - 0x2C];
    u8 active;   /* 0x34 */
    u8 field35;  /* 0x35 */
    u8 field36;  /* 0x36 */
    u8 field37;  /* 0x37 */
    u8 field38;  /* 0x38 */
    u8 field39;  /* 0x39 */
    s16 field3A; /* 0x3A */
    u16 field3C; /* 0x3C */
    u8 pad3E[0x4A - 0x3E];
    u16 flags4A; /* 0x4A */
    u8 pad4C[0x58 - 0x4C];
    s16 field58;    /* 0x58: target code (0xFA-0xFF special, 1-127 a slot + 1) */
    s16 targetPart; /* 0x5A: part of the target's hierarchy, 0 its root */
    u8 field5C;     /* 0x5C: parent object, 0xFF none */
    u8 field5D;     /* 0x5D: turn with the parent */
    s16 parentPart; /* 0x5E */
    s16 groundY;    /* 0x60 */
    u8 pad62;
    u8 hasTexture;   /* 0x63 */
    s16 offset[3];   /* 0x64: position relative to the target */
    s16 offset2[3];  /* 0x6A */
    s16 motion[12];  /* 0x70 */
    s16 position[3]; /* 0x88 */
    s16 field8E;     /* 0x8E */
    u8 pad90[0x98 - 0x90];
    s16 animation;       /* 0x98: -1 none */
    s16 animationLoop;   /* 0x9A: -1 none */
    u16 animationFrame;  /* 0x9C */
    u16 animationLength; /* 0x9E */
    u8 *animationStart;  /* 0xA0 */
    u8 *animationCursor; /* 0xA4 */
    u8 padA8[0xB0 - 0xA8];
    u8 *model;       /* 0xB0 */
    u8 *textureInfo; /* 0xB4 */
    u8 padB8[0x10A - 0xB8];
    u16 slotMask; /* 0x10A */
} BattleObject;

/* An animation header (fields as far as recovered). */
typedef struct {
    u8 pad0[2];
    u16 loop; /* 0x02 */
    u8 pad4[0x12 - 0x4];
    u16 length;     /* 0x12 */
    u32 dataOffset; /* 0x14 */
} Animation;

/* The battle scene data (fields as far as recovered). */
typedef struct {
    u8 pad0[0x348];
    s16 effectCount; /* 0x348 */
    s16 spriteCount; /* 0x34A */
    s16 maxX;        /* 0x34C */
    s16 minX;        /* 0x34E */
    s16 minZ;        /* 0x350 */
    s16 maxZ;        /* 0x352 */
} BattleSceneData;

/* An effect sprite record (0x7C bytes) of a sprite pool. */
typedef struct {
    u8 pad0[0x16];
    s16 id; /* 0x16: -1 free */
    u8 pad18[0x1E - 0x18];
    s16 field1E; /* 0x1E */
    u8 pad20[0x2C - 0x20];
    POLY_FT4 packets[2]; /* 0x2C: one per frame buffer */
} SpriteRecord;

/* A pool of sprite records; next is the first record that may be free. */
typedef struct {
    SpriteRecord *records;
    s16 count;
    s16 next;
} SpritePool;

/* A triangle of the scene's light geometry (0xE bytes). */
typedef struct {
    s16 vertices[3];   /* indices into the scene's points */
    s16 neighbours[3]; /* 0x06: adjacent triangles, -1 none */
    u8 id;             /* 0x0C */
    u8 visited;        /* 0x0D: visit stamp */
} SceneTriangle;

/* A light slot (6 bytes). */
typedef struct {
    u8 active;
    u8 r;
    u8 g;
    u8 b;
    u8 field4;
    u8 field5;
} LightSlot;

/* An 8-byte entry of D_800C3ACC. */
typedef struct {
    s32 value;
    s32 field4;
} SceneEntry8;

/* A position tracker (0x14 bytes, D_800D3304): an offset from a part of a
 * stage object. */
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 active;     /* 0x06 */
    SVector offset; /* 0x08 */
    s16 object;     /* 0x10: stage object, negative none */
    s16 part;       /* 0x12: its part less one */
} Tracker;

/* The battle's two loaded sound banks (0x18 bytes). */
typedef struct {
    s16 bank0;
    void *data0;
    s16 bank1;
    void *data1;
    s16 field10;
    s32 field14;
} SoundBanks;

/* A 0x18-byte resident record of D_800C3DA0. */
typedef struct {
    u8 pad[0x18];
} ResidentRecord18;

/* Per-slot formation information and placement (0x1C bytes, D_800C3EB4). */
typedef struct {
    u8 group;  /* formation group */
    u8 member;
    u8 field2; /* 0x7F none */
    u8 hidden; /* 0x03 */
    u8 gear;   /* 0x04: fights in a gear */
    u8 pad5;
    u8 targetCode; /* 0x06: from the default target (80085310) */
    u8 pad7[0xA - 0x7];
    s16 x; /* 0x0A */
    s16 z; /* 0x0C */
    s16 y; /* 0x0E */
    u8 pad10[0x1C - 0x10];
} BattleSlot;

/* Battle scene and effect state. */
extern SVector *D_800D3344;       /* scene points */
extern SceneTriangle *D_800D39CC; /* scene triangles */
extern s32 D_800D3348;            /* scene triangle count */
extern u8 D_800D2F64;             /* triangle visit stamp */
extern u8 D_800C37C8;             /* effects disabled */
extern LightSlot D_800C3AAC[4];
extern s16 D_800D2FC8;  /* point count of D_800D2FD0 */
extern u16 *D_800D2FD0; /* (x, z, y) points */
extern u8 D_800D3611;   /* a light slot changed */
extern u8 D_800C3B74;
extern u8 D_800C3D6C;
extern u8 D_800C3D68;
extern s32 D_800C3E88;
extern s16 D_800C3CF0;
extern s16 D_800C3B7C;
extern SceneEntry8 D_800C3ACC[20];
extern Tracker D_800D3304[2];
extern s32 D_800D2D40;
extern s32 D_800D2D48;
extern EffectEntry *D_800C3BAC[9];
extern BattleObject *D_800D3368[]; /* stage objects */
extern BattleEvent D_800C3FE8[];   /* presentation events */
extern u8 *D_800C3BEC;             /* effect script cursor */
extern s32 D_800C3BF0;             /* effect script step count */
extern u16 D_800C3E30;             /* slot mask */
extern u16 D_800C3D40;
extern EffectPool D_800C3D0C;
extern SpritePool D_800C3D04;
extern BattleSceneData *D_800658C8;
extern s32 D_800C3E38;
extern void *D_800C3D50[2];
extern void *D_800C3EA0;
extern ResidentRecord18 D_800C3DA0[2];
extern SoundBanks *D_800C3B78;
extern s32 D_80059464;
extern u8 D_800591AC;
extern s32 D_800C360C;
extern u8 D_800C4000[]; /* per slot */
extern BattleSlot D_800C3EB4[11];

void func_800BF85C(s32 index, s32 slot);
void func_800BE790(void);
s32 func_800BF6F8(void);
void func_8002A498(s32 a);
s32 func_800286CC(void);
void func_800B0060(BattleObject *object);

/* Resident services. */
void func_80027D40(void *handle);
void func_8002800C(ResidentRecord18 *record);
void func_800284B4(s32 *a, s32 *b);
void func_80028470(s32 a, s32 b);
void func_80028998(s32 bank);
s32 func_800288EC(s32 bank);
void func_80029AFC(SoundBanks *banks, s32 a, s32 b);

void func_8003852C(u8 *texture);
void func_800AA934(BattleObject *object, BattleObject *target, EffectPool *pool, s32 arg3);
void func_800B00D0(void);
void func_800AFF9C(BattleObject *object);
s32 func_800A0838(ModelList *models, ModelPart *root, u16 animation, s16 scale);
void func_800AE2A4(BattleObject *object, ModelList *models, s32 arg2);
void func_800AAD54(BattleObject *object, ModelList *models, s32 flags, s32 steps, s32 arg4);
void func_800AF180(EffectPool *pool, s32 index, ModelPart *from, ModelPart *to);
EffectPool *func_800A2234(EffectPool *pool, s32 count);
SpritePool *func_800A2CA4(SpritePool *pool, s32 count);
void func_800A9FF0(s32 index);
void func_800A22A8(EffectPool *pool);
void func_800A2D1C(SpritePool *pool);
s32 func_800AF400(void);
void func_800AFA98(BattleObject *object, ModelPart *part, s32 flags);
void func_800A5BE8(SVector *a, SVector *b, SVector *c, SVector *point, void *out);
s32 func_800A5870(SVector *point, s32 index, void *out);
s32 func_800A579C(SVector *point);
s32 func_800A5A48(SVector *a, SVector *b, SVector *c, SVector *point);
s32 func_800A5914(SVector *point, s32 triangle, s32 depth);
s32 func_800A5D54(SVector *point, s32 triangle, s32 depth);
s32 func_800AA650(s32 index);
void func_800B10EC(s32 index, s16 x, s16 z, s32 y);
void func_800A2D5C(SpritePool *pool);
void func_800A3490(void);
void func_800A3514(void);
void func_800A3578(void);
void func_800A35C8(void);

#endif
