#ifndef OVL3087_SCRIPT_ACTOR_H
#define OVL3087_SCRIPT_ACTOR_H

#include "common.h"

/* Actor and model helpers of the battle event scripts (801e93e8-801e9b58),
 * a separate unit built by a later compiler (see ovl3087.mk). The
 * interpreter (ovl3087.c) declares these helpers with its own prototypes. */

/* A battle actor (the battle overlay's animated object), only the fields
 * these helpers use. */
typedef struct BattleActor {
    s32 x, y, z; /* 0x00 position, 16.16 */
    u8 pad0C[0x32 - 0x0C];
    s16 unk32;
    u16 unk34;
    u8 pad36[0x3C - 0x36];
    u32 flags3C;
    u32 flags40;
    u8 pad44[0x48 - 0x44];
    void *file; /* 0x48 model data */
    u8 pad4C[0x50 - 0x4C];
    s32 unk50;
    u8 pad54[0x6C - 0x54];
    struct BattleModel *owner; /* 0x6c */
    u8 pad70[0x74 - 0x70];
    struct BattleActor *target; /* 0x74 */
    u8 pad78[0x82 - 0x78];
    s16 unk82;
    u8 pad84[0x9E - 0x84];
    s16 unk9E;
    s16 argA0, argA2, argA4; /* 0xa0 action arguments */
    u8 padA6[0xA8 - 0xA6];
    u32 unkA8 : 30;
    u32 slotLow : 2; /* 0xa8 bits 30-31: low bits of the actor slot */
    u32 slotHigh : 2; /* 0xac bits 0-1: high bits of the actor slot */
    u32 unkAC : 22;
    s32 idleAnimation : 8; /* 0xaf animation to return to */
    s32 nextAnimation : 8; /* 0xb0 animation after an action (-1 none) */
    u32 unkB0 : 24;
} BattleActor;

/* A model object created for a script slot: its actor follows a 0x38-byte
 * header. */
typedef struct BattleModel {
    s32 unk0;
    BattleActor *actor; /* 0x04 */
    u8 pad8[0x14 - 0x08];
    u32 flags; /* 0x14 */
    u8 pad18[0x1C - 0x18];
    u8 unk1C[4]; /* 0x1c */
    BattleActor *actor2; /* 0x20 */
    u8 pad24[0x38 - 0x24];
    BattleActor body; /* 0x38 */
} BattleModel;

/* The battle work area at 800c3eb0; the actor table is at 0x8c8c. */
typedef struct {
    u8 pad0[0x8C8C];
    BattleActor *actors[16];
} BattleWork;
extern BattleWork D_800C3EB0;

/* The actor chosen by the attack target search (800bdeb4). */
typedef struct {
    BattleActor *target;
    s32 unk4;
} TargetSearch;
extern TargetSearch D_800D363C;
extern u16 D_800D3634;
extern s16 D_800D3678;
extern s32 D_800C360C;
extern s32 D_800C3618;
extern u8 D_800D3350;
extern s32 D_80059464; /* frame counter */
extern u8 D_800591AC;
extern u8 D_800C37C8;
extern void D_800B9B30(BattleActor *actor);
extern u8 D_800BABDC[];
extern u8 D_800BAC50[];
extern u8 D_800BAB0C[];

/* Resident sprite/actor services. */
void func_80021BF0(BattleActor *actor, s32 arg);
void func_80021BF8(BattleActor *actor, void (*callback)(BattleActor *actor));
void func_80021FE0(BattleActor *actor, s32 arg);
void func_80022000(BattleActor *actor, s32 scale);
void func_800222BC(BattleActor *actor, void *file);
void func_800223B0(BattleActor *actor, s32 arg);
void func_80023804(BattleActor *actor);
void func_800239A0(BattleActor *actor);
void func_800245D8(BattleActor *actor, s32 animation);
BattleModel *func_8001D1D8(s32 size, s32 arg1, void *arg2, void *arg3, void *arg4);
void func_8001CB48(void *arg);
void func_8001CD94(BattleModel *model);
void func_8001CE74(BattleModel *model);
void func_800B8354(void);
s32 func_800B7E94(void);
void func_800BC2F0(s32 arg);
void func_800BC3F8(s32 arg);
void func_800BC404(u16 arg);
void func_800BE790(void);
s16 func_800BEEB4(s32 arg0, TargetSearch *search, BattleActor *actor);
s16 func_800BEF24(BattleActor *actor, BattleActor *target);
s16 func_800BEF8C(BattleActor *actor);
void func_800BF2B8(BattleActor *actor);
s32 func_800BF354(void);
void func_800BF600(s32 command, BattleActor *actor); /* battle: run command during the actor's motion */
s32 func_800BF720(void);
void func_800BF7C8(BattleActor *actor, s32 arg1, void (*callback)(BattleActor *actor));

void func_801E93E8(BattleActor *actor);
void func_801E9430(s32 actor, s32 animation);
void func_801E950C(s32 actor);
void func_801E9550(s32 actor);
void func_801E958C(s32 actor);
void func_801E95B0(BattleActor *actor);
void func_801E95E4(s16 actor, s16 x, s16 y, s16 z);
void func_801E9694(s16 actor, s16 x, s16 y, s16 z);
void func_801E9700(s32 actor, s32 arg1);
void func_801E9760(s32 actor, s32 target);
void func_801E9894(s32 actor, u16 target);
void func_801E9958(BattleModel *model, s32 animation);
BattleModel *func_801E9978(void *file, s16 *position);
void func_801E9AD4(BattleModel *model);
void func_801E9B2C(void);

#endif
