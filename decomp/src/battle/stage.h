#ifndef BATTLE_STAGE_H
#define BATTLE_STAGE_H

#include "common.h"
#include "psyq.h"
#include "scene.h"
#include "battle_core.h"

/* An object's model or extra data (fields as far as recovered). */
struct ObjectData {
    u8 pad0[8];
    SoundSystem *sounds; /* 0x08: its sound bank */
};

/* Per-frame update and drawing of the stage objects. */
extern s32 D_800CCC5C;     /* frames skipped by the last frame */
extern s16 D_800C3B80;     /* pulse level of the highlight colour */
extern s16 D_800D39E8;     /* a slow wave (4..9) */
extern u16 D_800C3D14;     /* highlighted slots */
extern u8 D_800C3DF8;      /* effects run */
extern Matrix *D_800D2FC0; /* the stage colour matrix */
extern s32 D_80050104;     /* resident: drawing with lighting */
extern SoundSystem *D_800C4924;
extern SVector D_800D3354; /* camera position */
extern SVector D_800D335C; /* camera look-at point */
extern s16 D_800C3542;     /* last scene triangle under the camera's view point */
extern s16 D_800C3544;     /* its ground height */
extern s16 D_800C3546;     /* key of the last update */

/* Resident services. */
s32 func_8003F8B0(s32 angle); /* rcos (4096 = 1.0) */
s32 func_8003F8CC(s32 angle); /* rsin */
s32 ratan2(s32 y, s32 x);
void func_8004A480(Vector *a, Vector *b, Vector *out); /* OuterProduct12 */

void func_8009F844(BattleObject *object, s32 arg1, s32 arg2, s32 arg3, s32 skipped, s32 arg5, s32 arg6);
void func_800A2FD8(SpritePool *pool, s32 arg1, s32 steps, s32 arg3, s32 arg4);
void func_800A429C(u8 *anim);
void func_800A44C0(BattleObject **objects);
void func_800A4CF8(s32 index);
s32 func_800AAA20(BattleObject *object, EffectPool *pool, s32 steps, s32 arg3, s32 arg4);
void func_800AAB34(BattleObject *object);
u8 func_800AA514(s16 a, s16 b, s32 c);
s32 func_800AA600(s32 index);
void func_8009F794(ModelList *list, s32 release);
void func_800A2ACC(EffectPool *pool, ModelPart *part);
void func_800A2BB8(EffectPool *pool, ModelPart *part, u8 kind);
void func_800B026C(EffectPool *pool, s32 steps, s32 arg2, s32 arg3);

/* A resident sprite task (fields as far as the battle uses them): its
 * sprite's position from +0x38, and at +link its caller block. */
typedef struct EffectSprite {
    u8 pad0[0x38];
    s32 x, y, z; /* 0x38: 16.16 */
    u8 pad44[0xBE - 0x44];
    s16 link; /* 0xBE */
} EffectSprite;

/* The battle's block of a sprite following an object part (0x18 bytes). */
typedef struct {
    u8 pad0[4];
    void (*update)(EffectSprite *sprite); /* 0x04: the sprite's own update */
    BattleObject *object;                 /* 0x08 */
    s16 part;                             /* 0x0C: 0 the root */
    s16 onGround;                         /* 0x0E: keep the object's ground height */
    SVector offset;                       /* 0x10: from the part */
} SpriteFollow;

/* An animation script command creating a sprite (fields as far as used). */
typedef struct {
    u8 pad0[5];
    u8 part;      /* 0x05 */
    s16 offset[3]; /* 0x06 */
    u8 onGround;  /* 0x0C */
    u8 padD[0x13 - 0xD];
    u8 follow;    /* 0x13 */
} SpriteCommand;

/* A camera channel: an effect entry of D_800C3BAC seen as signed values. */
typedef struct {
    u8 used;
    u8 field1;
    u8 mode;         /* 0x02: bit 0 ease, low nibble < 2 follows objects */
    u8 kind;         /* 0x03 */
    s16 current[3];  /* 0x04 */
    s16 slot;        /* 0x0A: the followed object (or the target x) */
    s16 height;      /* 0x0C: subtracted from its height (or the target y) */
    s16 slot2;       /* 0x0E: a second object, -1 none (or the target z) */
    u16 progress;    /* 0x10 */
    s16 duration;    /* 0x12 */
} CameraChannel;

/* The camera. */
extern u8 D_800C3B84;  /* the channel kind reported in D_800C3B88 */
extern u8 D_800C3B88;  /* bit 0: that channel runs, bit 1: it finished */
extern u8 D_800C3B8C;  /* snap: channels 7 and 8 start at their targets */
extern s16 D_800C3B90; /* orbit yaw */
extern s16 D_800C3B94; /* orbit pitch */
extern s16 D_800C3B98; /* orbit distance */
extern s16 D_800C3B9C; /* orbit height */
extern s16 D_800C3BA0; /* look-at yaw */
extern s16 D_800C3BA4; /* look-at distance */
extern s16 D_800C3BA8; /* look-at height */

s32 func_800B0FF4(SVector *from, SVector *point);
s16 func_800B0B14(s32 key);

/* An entry of an effect script file (0x1C bytes); the offsets are from the
 * entry until relocated. */
typedef struct {
    u8 *data0;    /* 0x00 */
    s32 field4;   /* 0x04 */
    u8 *data8;    /* 0x08 */
    s32 fieldC;   /* 0x0C */
    u8 *commands; /* 0x10: 4-byte aligned commands; byte 1 is the length in words less one */
    s32 count;    /* 0x14: commands */
    s32 field18;  /* 0x18 */
} ScriptEntry;

/* An effect script file. */
typedef struct {
    u8 pad0[4];
    u32 flags; /* 0x04: bit 0 relocated */
    u8 pad8[4];
    ScriptEntry entries[1]; /* 0x0C */
} ScriptFile;

extern ScriptEntry D_800C3BD0; /* the selected script */

/* Resident sprites. */
EffectSprite *func_80023FD8(s32 kind, void *resource, SVector *position, s32 size);
void func_80021FE0(s32 *body, s32 direction);
void func_800223B0(s32 *body, s32 direction);
void func_80022000(s32 *body, s32 scale);
void *func_8001CD7C(EffectSprite *sprite); /* the task's update */
void func_8001CD6C(EffectSprite *sprite, void (*update)(EffectSprite *sprite)); /* set it */

void func_800AFC68(EffectSprite *sprite);

#endif
