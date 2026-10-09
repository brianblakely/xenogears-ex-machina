#ifndef BATTLE_ACTIONS_H
#define BATTLE_ACTIONS_H

#include "common.h"

/* The battle's actions: the action list a turn runs (its entries' handlers,
 * 80078658-80079270 in 80070E2C's unit, and its execution, 80079778), the
 * committed action with its item list, the per-slot results and the
 * presentation events that apply them (battle.c 80085350-800879A8), and the
 * item effects. */

/* Action list entry (8 bytes, 32 from 800d2e5c). The debug overlay's state
 * page prints the first 23 under the columns Cd, Cl, An, P1, P2, P3 and Tg. */
typedef struct BattleAction {
    u8 type;
    u8 arg1;
    u8 animation;
    u8 named;
    u8 param;
    u8 unk5;
    u16 targets;
} BattleAction;

extern BattleAction D_800D2E5C[32];
extern u16 D_800D39E0;     /* mask of slots that act together */

/* The committed action (800d2c94). */
typedef struct {
    u16 targets;      /* +0x00 target mask */
    u16 alive;        /* +0x02 alive mask at commit */
    s16 animation;    /* +0x04 */
    u8 pad6[0xA - 0x6];
    u16 held;         /* +0x0A party members whose timers are held */
    u8 padC[0x10 - 0xC];
    u8 enemyBytes[5]; /* +0x10 copied to an enemy's AI bytes 9-13 */
    u8 actor;         /* +0x15 committing actor */
    u8 action;        /* +0x16 committed action index */
    u8 pad17[0x1B - 0x17];
    u8 message;       /* +0x1B pending battle message id */
    u8 itemCounts[0x30]; /* +0x1C the item list (also D_800D2CB0) */
    u8 itemIds[0x30];    /* +0x4C (also D_800D2CE0) */
} ActionCommit;

extern ActionCommit D_800D2C94;
extern u16 D_800D2C9E;            /* D_800D2C94.held addressed on its own */
extern u8 D_800D2CB0[0x30];       /* D_800D2C94.itemCounts: item counts */
extern u8 D_800D2CE0[0x30];       /* D_800D2C94.itemIds: item ids */
extern u8 D_800C204C;
extern u8 D_800C3E18;
extern u8 D_800C3D70[0x30];
extern u8 D_800D2FE4[0x30];       /* battle item ids: the setup (ovl2615) lists them, the results (ovl2596) read them */

/* The per-slot results of the battle work area (battle.data.ld). */
extern s32 D_800D2C54[12]; /* per-slot damage */
extern s32 D_800D2C60[8];  /* the enemies' damage */
extern u8 D_800D2C88[12];  /* per-slot result code */
extern u8 D_800D2C8B[8];   /* the enemies' result codes */

/* Item effect table (0x10 bytes from 800d2200, in the battle work area's item
 * lists). */
typedef struct {
    u8 unk0[4];
    u16 target;    /* +0x4 target selection */
    u8 unk6[0x8 - 0x6];
    u8 amount;     /* +0x8 */
    u8 duration;   /* +0x9 */
    s16 flags;     /* +0xA effect bits; 1 selects the special effect amount */
    s16 status;    /* +0xC status bits */
    u16 animation; /* +0xE */
} ItemEffect;

extern ItemEffect D_800D2200[];
extern void *D_800D329C;   /* item name table */

/* Queueing presentation events (80070E2C's unit). */
void func_80078658(u8 index, u8 actor); /* show action index's name, queue its event */
void func_800787E0(u8 value, u8 actor);
void func_8007887C(u8 actor);

/* Action list handlers (800793f0's table). */
void func_80078998(u8 actor, u8 index, u8 target);
void func_80078B34(u8 actor, u8 index, u8 target);
void func_80078C9C(u8 actor, u8 index, u8 target);
void func_80078CEC(u8 actor, u8 index, u8 target);
void func_80078D48(u8 actor, u8 index, u8 target);
void func_80078D6C(u8 actor, u8 index, u8 target);
void func_80078E24(u8 actor, u8 index, u8 target);
void func_80079054(u8 actor, u8 index, u8 target);
void func_80079098(u8 actor, u8 index, u8 target);
void func_80079114(u8 actor, u8 index, u8 target);
void func_8007916C(u8 actor, u8 index, u8 target);
void func_800791FC(u8 actor, u8 index, u8 target);
void func_80079270(u8 actor, u8 index, u8 target);
void func_80079778(u8 actor); /* execute the actor's action list */

/* Committing, resolving and presenting results (battle.c). */
void func_80085350(void);    /* reset the running results */
void func_80085388(void);    /* clear the current event's results */
void func_80085AC4(u8 slot); /* revive slot at full HP */
void func_80085B58(u8 slot); /* apply and show slot's recovery amounts */
void func_80085C88(u8 queue); /* accumulate and apply event queue's results */
void func_80085CCC(u8 actor, u16 targets, u16 animation); /* commit an action and resolve it */
void func_800879A8(u8 actor, u8 target); /* reset the events for actor against target */

#endif
