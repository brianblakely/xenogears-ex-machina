#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"

/* A status word pair: the statuses in effect, and those that are permanent
 * (they do not wear off). Timed statuses are tested on the whole word. */
typedef union {
    u32 word;
    struct {
        u16 active;
        u16 permanent;
    } half;
} StatusPair;

/* Status block of a combatant: the pilot's at record +0, the gear's at
 * record +0xA4. Fields are named as their uses are recovered; unknown bytes
 * stay padding. */
typedef struct {
    u8 pad0[0x32];
    u16 flags32;            /* 0x32: bit 0x40 doubles status durations */
    u8 pad34[0x7A - 0x34];
    u16 status7A;
    u16 status7C;           /* bits 0xC002 mark a member out of action */
    u8 pad7E[0x80 - 0x7E];
    u16 status80;
    u8 pad82[0x84 - 0x82];
    StatusPair status84;
    StatusPair status88;
    StatusPair status8C;
    u8 pad90[0xA4 - 0x90];
} UnitStatus;

/* Combatant record: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes in the
 * static array D_800CCCE8; D_800C34B0 points at it. */
typedef struct {
    UnitStatus pilot;
    UnitStatus gear;
    u8 pad148[0x15A - 0x148];
    u8 flags15A;            /* bit 0x80: fighting in a gear */
    u8 pad15B;
    volatile u8 statusTimers[0x10]; /* remaining turns per timed status */
    u8 pad16C[0x170 - 0x16C];
} Combatant;

extern Combatant D_800CCCE8[11];
extern Combatant *D_800C34B0;

extern u8 D_800C34AD;       /* formation mode */
extern u8 D_800C3E04;       /* attacker slot */
extern Combatant *D_800C3E34; /* target record */

void func_80099CF0(UnitStatus *gear, Combatant *record, volatile u8 *timers);

#endif
