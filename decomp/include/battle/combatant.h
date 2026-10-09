#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"
#include "resident/gamedata.h"
#include "battle/work.h"

/* Views of the game data D_8006D634 (resident/gamedata.h) the battle indexes
 * by slot: the item durabilities from +0x2286 and the gear part durabilities
 * from its flags at +0x22B6 (word 0 the option flags). Their extent is not
 * settled, so they stay names of their own. */
extern u8 D_8006F8BA[]; /* item durability by slot */
extern u8 D_8006F8EA[]; /* gear part durability by slot */

extern BattleWork *D_800C34B0;

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
extern u8 D_800D2D24[3]; /* party character ids, 0x7F none */

u8 func_80079ED8(u8 slot, u8 attribute, u8 value, u8 read);
u16 func_8007A280(u8 slot, u8 attribute, u16 value, u8 read);
s32 func_8009C050(u8 slot);
void func_8009E788(void);

#endif
