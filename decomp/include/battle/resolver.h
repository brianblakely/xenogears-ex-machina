#ifndef BATTLE_RESOLVER_H
#define BATTLE_RESOLVER_H

#include "common.h"

/* The action resolver's rolls, formulas, statuses and messages (8008CCCC's
 * unit, 80098C6C-8009E53C, and 8009E53C's 8009E868). */

extern u8 battle_attack_item_broken;   /* a weapon-using attack found its item broken */
extern u8 battle_scene_part_speeds[4]; /* speeds 8009892c replaces by each gear part speed */

void battle_resolve_item_effect(u16 param);                 /* resolve an item or effect on its targets */
u16 battle_status_count_down(u8 slot);                      /* count down a slot's timed statuses */
s32 battle_get_status_drain_amounts(u8 slot, s32 *amounts); /* a slot's regeneration amounts */
void battle_revive_slot_record(u8 slot);                    /* revive a slot's record */
void battle_status_show_gear_message(u8 kind, u16 flag);    /* show an applied gear status's message */

#endif
