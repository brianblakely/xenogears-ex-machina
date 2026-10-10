#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"
#include "resident/gamedata.h"
#include "battle/work.h"

/* The combatants: their records in the battle work area (battle/work.h) and
 * attributes (80079ED8's unit, 80079ED8-8007A628), the attacker and target
 * the resolver works on, and the records' conditions (8008CCCC's and
 * 8009E53C's units, 80096824-8009E868). */

extern BattleWork *battle_work_ptr;
extern u8 battle_party_character_ids[3]; /* party character ids, 0x7F none */

/* The attacker and the target. */
extern u8 battle_party_member_count;                 /* formation mode */
extern CommandDescriptor *battle_current_command;    /* current command descriptor */
extern Combatant *battle_attacker_record;            /* attacker record */
extern GearRecord *battle_attacker_gear;             /* attacker's gear record */
extern u8 battle_attacker_slot;                      /* attacker slot */
extern Combatant *battle_target_record;              /* target record */
extern u8 *battle_target_attack_level;               /* the target's field 0x148 */
extern u8 *battle_attacker_attack_level;             /* the attacker's attack level and maximum (+0x148) */
extern u8 battle_ether_check_failed;                 /* an ether check failed */
extern void (*battle_gear_formula_table[])(void);    /* gear formula table */
extern u8 battle_target_slot;                        /* target slot */
extern GearRecord *battle_target_gear;               /* target's gear record */
extern u8 battle_gear_hud_attack_level;

u8 battle_access_combatant_attr8(u8 slot, u8 attribute, u8 value, u8 read);    /* a byte attribute: store or read */
u16 battle_access_combatant_attr16(u8 slot, u8 attribute, u16 value, u8 read); /* a halfword attribute */
s32 battle_get_gear_warning_flags(u8 slot);                                    /* gear warning flags of a slot */
void battle_wear_down_attacker_gear_parts(void);                               /* take a round of the attacker gear's ammo for the command */

#endif
