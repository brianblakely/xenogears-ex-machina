#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"
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


void func_80099CF0(GearRecord *gear, Combatant *record, volatile u8 *timers);
void func_8009B104(u8 slot, Combatant *chuchu);
void func_8009BE0C(void);
s32 func_8009C050(u8 slot);
void func_80096824(void);
void func_8009AC48(u8 member, u8 checked);
void func_8009C4B4(void);
void func_8009C9C4(void);
void func_800995A0(u8 slot, u8 kind, u16 flag, u8 amount);
void func_8009CA90(void);
void func_8009CB68(u8 slot);
void func_8009E788(void);
s8 func_8009DBFC(u8 fromGear);
s16 func_80096FBC(void);
s16 func_80097610(void);

#endif
