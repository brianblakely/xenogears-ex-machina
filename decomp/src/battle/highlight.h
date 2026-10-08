#ifndef BATTLE_HIGHLIGHT_H
#define BATTLE_HIGHLIGHT_H

/* Slot highlights (800BCB54-800BD098): the acting slot's sprite pulses in
 * colour, highlighted slots (D_800C3D14) carry a spinning effect-script
 * ring above their sprite. */

#include "common.h"
#include "psyq.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
#include "frame.h"

/* The acting slot's pulse (D_800C3748), a child task of its actor task. */
typedef struct {
    BattleTask task;
    BattleSprite *sprite; /* 0x1C */
    s32 tick;             /* 0x20 */
    s32 slot;             /* 0x24 */
} SlotPulse;

extern SlotPulse *D_800C3748;

/* A highlighted slot's ring (0x68 bytes): a task and a draw task, drawn
 * from resident effect script 0 of 8001C76C with double-buffered vertices. */
typedef struct {
    ActorTask actor;      /* 0x00: its draw task at 0x1C */
    BattleSprite *sprite; /* 0x38 */
    VECTOR pos;           /* 0x3C: above the sprite */
    SVECTOR angle;        /* 0x4C: spins about y */
    s32 height;           /* 0x54: the sprite's size */
    s32 tick;             /* 0x58 */
    u8 *vertices[2];      /* 0x5C: per display buffer */
    u8 *script;           /* 0x64 */
} SlotRing;

/* D_800C4922, the acting slot, as the late unit addresses it inside the
 * area (the menu's path points overlap it). */
#define AREA_ACTING_SLOT (((u8 *)&BATTLE_AREA)[0xA72])

extern MATRIX D_8004FBB8;   /* sprite camera */
extern u8 D_8001C76C[];  /* resident effect script table (a TMD model, objects.h) */

void *func_8001D0A4(void *owner, void (*update)()); /* the owner's child task running update */
u8 *func_800B168C(u8 *table, s32 index);
s32 func_800B16A4(ScriptEntry *entry);
void func_800B1720();
void func_800B1F6C();

void func_800BCB54(BattleTask *task); /* also the pulse destroy; ends D_800C3748 */
void func_800BCBB4(BattleTask *task);
void func_800BCC60(void);
void func_800BCEAC(BattleTask *draw);
void func_800BCFAC(ActorTask *task);
void func_800BD024(ActorTask *task);
void func_800BD098(ActorTask *owner);

#endif
