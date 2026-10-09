#ifndef BATTLE_HIGHLIGHT_H
#define BATTLE_HIGHLIGHT_H

/* Slot highlights (800BCB54-800BD098): the acting slot's sprite pulses in
 * colour, highlighted slots (D_800C3D14) carry a spinning effect-script
 * ring above their sprite. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "battle/area.h"

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

void func_800BCD98(u16 mask);
void func_800BD1FC(s32 slot);
void func_800BD2E4(void);

#endif
