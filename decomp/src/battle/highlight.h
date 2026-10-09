#ifndef BATTLE_HIGHLIGHT_H
#define BATTLE_HIGHLIGHT_H

/* Slot highlights (800BCB54-800BD098): the acting slot's sprite pulses in
 * colour, highlighted slots (D_800C3D14) carry a spinning effect-script
 * ring above their sprite. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "objects.h"
#include "screen.h"
#include "sprite_effect.h"
#include "frame.h"

/* The acting slot's pulse (D_800C3748), a child task of its actor task. */
typedef struct {
    Task task;
    Sprite *sprite; /* 0x1C */
    s32 tick;             /* 0x20 */
    s32 slot;             /* 0x24 */
} SlotPulse;

extern SlotPulse *D_800C3748;

/* A highlighted slot's ring (0x68 bytes): a task and a draw task, drawn
 * from object 0 of the resident TMD model D_8001C76C with double-buffered
 * packets. */
typedef struct {
    Task task;            /* 0x00 */
    Task draw;            /* 0x1C */
    Sprite *sprite;       /* 0x38 */
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

extern u8 D_8001C76C[];  /* resident TMD model (objects.h's effect script file format) */

u8 *func_800B168C(u8 *table, s32 index);
s32 func_800B16A4(ScriptEntry *entry);
void func_800B1720();
void func_800B1F6C();

void func_800BCB54(Task *task); /* also the pulse destroy; ends D_800C3748 */
void func_800BCBB4(Task *task);
void func_800BCC60(void);
void func_800BCEAC(Task *draw);
void func_800BCFAC(Task *task);
void func_800BD024(Task *task);
void func_800BD098(SpriteTask *owner);

#endif
