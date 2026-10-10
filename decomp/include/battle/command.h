#ifndef BATTLE_COMMAND_H
#define BATTLE_COMMAND_H

#include "common.h"
#include "battle/scene.h"
#include "battle/work.h"

/* The battle command menu: a party member's turn in the menu, the attack
 * page and its combos, target selection, automatic turns and the gear, item
 * and escape commands (80079ED8's unit 8007FB70-80080C94, battle.c
 * 800826CC-8008B108, 8008B478's 8008C4A8, 8008CCCC's 8008CFB8-8009BAC4 and
 * 800B8098's turn cancel 800B8DA4). */

/* The menu's state. */
extern u8 battle_command_menu_sounds_enabled;      /* menu effects enabled */
extern void *battle_command_menu_module_block;     /* menu module block */
extern void *battle_command_menu_file3_block;      /* file 3 block */
extern u8 battle_target_candidates[12];            /* default-target candidates */
extern u8 battle_target_candidate_count;           /* candidate count */
extern u16 battle_target_candidate_mask;
extern u8 battle_target_cursor_slot;
extern u8 battle_turns_active;
extern s32 battle_list_page_scroll;
extern u8 battle_list_page_scroll_request;
extern u8 battle_combo_step_flags[15];             /* combo step flags, by combo step index 0-14 */
extern u16 battle_gear_hud_charge;                 /* fuel gained by charging */

/* The timer reload by maximum and remaining AP: battle_ap_timer_reload_table (maximum 3-7) from
 * three rows before (battle.data.ld). */
extern u8 battle_ap_timer_reload_table_by_max_ap[][8];
extern u8 battle_unread_gear_attack_step_flag;       /* healing ignores the gear */
extern u8 *battle_combo_patterns[13];                /* combo input patterns (seven inputs each) */
/* The next combo step by step and AP paid: battle_combo_next_step_table from one byte before
 * (battle.data.ld). */
extern u8 battle_combo_next_step_table_by_paid[8][3];
extern u8 *battle_combo_deathblows_by_character[];   /* per character: the deathblow of each combo */

/* The fuel cost of each combo step (1-based, indexed like the combo flags
 * 800c34cc) at the gear HUD of the battle work area. */
#define STEP_FUEL (&battle_work_area.gearHud.commands[-1])

/* A slot's ground position, read unsigned. */
#define SLOT_X(slot) ((u16)battle_area_slots[slot].x)
#define SLOT_Z(slot) ((u16)battle_area_slots[slot].z)
/* The square of a difference, taken of its magnitude. */
#define SQUARE(x) ((x) < 0 ? (-(x)) * (-(x)) : (x) * (x))

/* A member's turn in the menu (80079ED8's unit). */
void battle_ammo_window_release(u8 member);
void battle_command_menu_release_module_block(void);   /* release the menu module block */
void battle_command_menu_load_module_block(u8 member); /* load the menu module block */
void battle_command_menu_release_file3_block(void);    /* release the file 3 block */
void battle_command_menu_load_file3_block(void);       /* load the file 3 block */
void battle_command_menu_run(u8 member);               /* run a party member's command menu */
void battle_mark_actor_events_done(void);              /* events done: refresh the actor's menu state */
void battle_take_automatic_turn(u8 member);            /* an automatic turn */

/* The attack page, targets and commands (battle.c). */
void battle_board_gear(u8 member);                        /* the member boards its gear */
u8 battle_can_attack_slot(u8 member, u8 slot);            /* whether the member can attack slot */
u8 battle_order_attack_candidates(u8 member);             /* order the member's attack candidates */
u8 battle_choose_target(u16 target, u8 member, s32 mode); /* select a target */
void battle_attack_page_enter(u8 member);                 /* enter the attack page */
u8 battle_execute_attack_step(u8 member, u8 cost);        /* execute the attack; the target reacted */
void battle_play_menu_sound(u8 id);                       /* play a menu sound */
void battle_execute_chosen_art(u8 member);                /* execute the chosen technique */

/* The combo, gear, item and escape commands (8008B478's and 8008CCCC's
 * units). */
u8 battle_combo_command_run(u8 member);                              /* run the member's combo; 1 when cancelled */
u8 battle_gear_menu_run(u8 member);                                  /* run the gear command menu; 1 when committed */
void battle_ammo_window_open(u8 member);
void battle_ammo_window_hide(u8 member, u8 release);
s32 battle_try_escape(void);                                         /* the escape succeeds */
void battle_start_defending(u8 member);                              /* the Defense command */
void battle_end_defending(u8 member);                                /* end the member's defending */
void battle_choose_automatic_action(u8 slot, u8 *choice, s16 *busy); /* choose an automatic action */
void battle_cancel_turn(void);                                       /* cancel the turn */

#endif
