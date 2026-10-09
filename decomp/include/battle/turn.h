#ifndef BATTLE_TURN_H
#define BATTLE_TURN_H

#include "common.h"

/* The battle's turns: the turn and menu state, the turn queue and the
 * slots' turn order, their flags, masks and running results, and the battle
 * frame that every wait runs (battle overlay; the event script overlay
 * ovl3087 reads the turn state too). */

/* Per-slot block of the turn state (0x40 bytes). */
typedef struct {
    u8 unk0[8];
    u8 digits[5][4];   /* +0x08 five four-glyph number strings */
    u16 items[16];     /* menu item availability, 0 = available */
    u8 defaultTarget;  /* +0x3C */
    u8 unk3D[3];
} TurnSlot;

/* Turn and menu state: the heap block at *800c3eac. */
typedef struct TurnState {
    TurnSlot slots[11];
    u8 unk2C0[4];
    u8 combo[8];       /* +0x2C4 entered combo steps, 0xff ends */
    u8 unk2CC[7];
    u8 actor;          /* +0x2D3 acting slot */
    u8 unk2D4[2];
    u8 unk2D6;
    u8 unk2D7[0x2DA - 0x2D7];
    u8 eventCount;     /* +0x2DA queued presentation events */
    u8 eventsDone;     /* +0x2DB */
    u8 unk2DC;         /* action index + 1 */
    u8 page;           /* +0x2DD command page */
    u8 menuDone;       /* +0x2DE */
    u8 unk2DF;
    u8 unk2E0;         /* shade step of the fading list quads (x16) */
    u8 unk2E1[0x2E6 - 0x2E1];
    u8 unk2E6;         /* chosen gear command + 0x10 */
    u8 unk2E7;
    u8 unk2E8;         /* attack page target */
    u8 unk2E9;
    u8 unk2EA;
    u8 reaction[3];    /* +0x2EB per party member */
    u8 unk2EE[0x2F6 - 0x2EE];
    u8 repeatArmed;    /* +0x2F6 */
} TurnState;

extern TurnState *D_800C3EAC;

/* Turn queue (800d2dcc). */
typedef struct TurnQueue {
    u8 present[11];    /* slot takes part */
    u8 cursor;         /* +0x0B turn order position */
    u8 order[11];      /* +0x0C slots in turn order */
    u8 unk17;
    u8 ready[11];      /* +0x18 slot ready to act (0xff: out) */
    u8 unk23;
    s16 timers[3][11]; /* +0x24 [0] reload values, [1] counters,
                        * [2] slow-status alternation */
} TurnQueue;

extern TurnQueue D_800D2DCC;
extern u8 D_800D3298;      /* ATB enabled */
extern u8 D_800D2DC0;      /* forced next turn: slot + 1 */
extern u8 D_800C4922;      /* acting slot (the battle area's +0xa72) */
extern u8 D_800D36C0;      /* the party member whose menu is open */

/* Per-slot flags (8 bytes from 800d32a0). */
typedef struct SlotFlags {
    u8 unk0;
    u8 unk1;
    u8 unk2[6];
} SlotFlags;

extern SlotFlags D_800D32A0[11];

/* The slots' masks and running results. */
extern u16 D_800D39DC;     /* alive mask */
extern u16 D_800C48E8;     /* knocked-out mask (the battle area's knockedOut) */
extern u16 D_800C3608;     /* slots that still count while down */
extern u16 D_800C3448[16]; /* slot bits */
extern u16 D_800C3468[16]; /* flag bits */
extern u16 D_800C3234[16]; /* single-bit masks, 0x8000 down to 1 */
extern u8 D_800D2D5C[11];  /* running result code per slot */
extern s16 D_800D2D70[11]; /* running result amount per slot */

/* The battle frame and the turn order (80070e2c's unit, 80079ed8's). */
s32 func_800716D8(void);
void func_8007171C(void);
void func_80078508(u8 *order);
s32 func_80080AE4(u8 actor);
/* Slot masks and random values (battle.c). */
u16 func_80089B50(u16 low, u16 high);
u16 func_80089BEC(u8 bit);
u16 func_80089C08(u8 slot);
u16 func_80089C48(u8 slot);
u16 func_80089C6C(u16 mask, u8 bit);
u16 func_80089C9C(u16 mask, u8 slot);
s32 func_80098AF8(s32 slot, s32 mode);

#endif
