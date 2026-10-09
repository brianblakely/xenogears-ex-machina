#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"
#include "resident/gamedata.h"
#include "battle/work.h"

/* The combatants: their records in the battle work area (battle/work.h) and
 * attributes (80079ED8's unit, 80079ED8-8007A628), the attacker and target
 * the resolver works on, and the records' conditions (8008CCCC's and
 * 8009E53C's units, 80096824-8009E868). */

extern BattleWork *D_800C34B0;
extern u8 D_800D2D24[3]; /* party character ids, 0x7F none */

/* The attacker and the target. */
extern u8 D_800C34AD;                 /* formation mode */
extern CommandDescriptor *D_800C3DFC; /* current command descriptor */
extern Combatant *D_800C3E00;         /* attacker record */
extern GearRecord *D_800D2D6C;        /* attacker's gear record */
extern u8 D_800C3E04;                 /* attacker slot */
extern Combatant *D_800C3E34;         /* target record */
extern u8 *D_800C3D60;                /* the target's field 0x148 */
extern u8 *D_800C3D3C;                /* the attacker's attack level and maximum (+0x148) */
extern u8 D_800D2DC4;                 /* an ether check failed */
extern void (*D_800C34DC[])(void);    /* gear formula table */
extern u8 D_800C3E50;                 /* target slot */
extern GearRecord *D_800D2DC8;        /* target's gear record */
extern u8 D_800D2C34;

u8 func_80079ED8(u8 slot, u8 attribute, u8 value, u8 read);   /* a byte attribute: store or read */
u16 func_8007A280(u8 slot, u8 attribute, u16 value, u8 read); /* a halfword attribute */
s32 func_8009C050(u8 slot);   /* gear warning flags of a slot */
void func_8009E788(void);     /* wear the attacker gear's parts for the command */

#endif
