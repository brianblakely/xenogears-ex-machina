#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"

/* Status block of a combatant: the pilot's at record +0, the gear's at
 * record +0xA4. The status words come in pairs; the second of a pair holds
 * statuses that do not wear off. Fields are named as their uses are recovered;
 * unknown bytes stay padding. */
typedef struct {
    u8 pad0[0x32];
    u16 flags32;            /* 0x32: bit 0x40 doubles status durations */
    u8 pad34[0x7A - 0x34];
    u16 status7A;
    u16 status7C;           /* bits 0xC002 mark a member out of action */
    u8 pad7E[0x80 - 0x7E];
    u16 status80;
    u8 pad82[0x84 - 0x82];
    u16 status84;
    u16 status86;
    u16 status88;
    u16 status8A;
    u16 status8C;
    u16 status8E;
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
    u8 statusTimers[0x10];  /* remaining turns per timed status */
    u8 pad16C[0x170 - 0x16C];
} Combatant;

extern Combatant D_800CCCE8[11];
extern Combatant *D_800C34B0;

extern u8 D_800C34AD;       /* formation mode */
extern u8 D_800C3E04;       /* attacker slot */
extern Combatant *D_800C3E34; /* target record */

#endif
