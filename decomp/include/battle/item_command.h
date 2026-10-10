#ifndef BATTLE_ITEM_COMMAND_H
#define BATTLE_ITEM_COMMAND_H

#include "common.h"

/* The technique and item menus and committing their choice (8008B478's unit,
 * 8008B478-8008C81C; battle.c 80085C48, 8008AC88-8008B224; the enemy and
 * event queue sides in 800792F8's and 80079ED8's units; character 4's gear
 * items in 8008CCCC's, 8009A7E4-8009A854). */
extern u8 battle_applying_item_results;  /* results are being applied for a committed item (read with lbu) */
extern u8 battle_item_menu_chosen_row;  /* item list row of the chosen item */
extern u8 battle_item_menu_chosen_column;  /* item list column of the chosen item */

extern s8 battle_acting_with_partner;
extern u8 battle_gear_part_counts[0x30]; /* gear part counts */

void battle_ai_tell_target_about_actor(u8 actor, u8 target);        /* tell an enemy who acts on it */
void battle_close_actor_event_queue(u8 actor);                   /* close the actor's event queue */
void battle_commit_item_targets(); /* commit an item's targets; K&R, callers pass them unconverted */
void battle_queue_reacting_enemies_event(u16 mask, u8 actor);         /* queue event 0xf3 for the reacting enemies */
void battle_hide_command_windows(u8 keep);                    /* hide the command windows */
u8 battle_confirm_art(u8 member, u8 column, u8 row); /* confirm a technique */
u8 battle_art_menu_run(u8 member);                    /* run the technique menu; 1 when chosen */
void battle_execute_chosen_item(u8 member);                  /* execute the chosen item */
u8 battle_item_menu_run(u8 member);                    /* run the item menu; 1 when chosen */
s32 battle_is_character4_entry_item(u8 index);                    /* whether a gear item is one of character 4's */
void battle_put_item_in_character4_entry(u8 index, u8 k);             /* put a battle item into character 4's entries */

#endif
