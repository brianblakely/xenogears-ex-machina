#ifndef BATTLE_ACTIONS_H
#define BATTLE_ACTIONS_H

#include "common.h"

/* The battle's actions: the action list a turn runs (its entries' handlers,
 * 80078658-80079270 in 80070E2C's unit, and its execution, 80079778), the
 * committed action with its item list, the per-slot results and the
 * presentation events that apply them (battle.c 80085350-800879A8), and the
 * item effects. */

/* Action list entry (8 bytes, 32 from 800d2e5c). The debug overlay's state
 * page prints the first 23 under the columns Cd, Cl, An, P1, P2, P3 and Tg. */
typedef struct BattleAction {
    u8 type;
    u8 arg1;
    u8 animation;
    u8 named;
    u8 param;
    u8 unk5;
    u16 targets;
} BattleAction;

extern BattleAction battle_action_list[32];
extern u16 battle_joint_action_slots;     /* mask of slots that act together */

/* The committed action (800d2c94). */
typedef struct {
    u16 targets;         /* +0x00 target mask */
    u16 alive;           /* +0x02 alive mask at commit */
    s16 animation;       /* +0x04 */
    u8 pad6[0xA - 0x6];
    u16 held;            /* +0x0A party members whose timers are held */
    u8 padC[0x10 - 0xC];
    u8 enemyBytes[5];    /* +0x10 copied to an enemy's AI bytes 9-13 */
    u8 actor;            /* +0x15 committing actor */
    u8 action;           /* +0x16 committed action index */
    u8 pad17[0x1B - 0x17];
    u8 message;          /* +0x1B pending battle message id */
    u8 itemCounts[0x30]; /* +0x1C the item list (also battle_item_counts) */
    u8 itemIds[0x30];    /* +0x4C (also battle_item_ids) */
} ActionCommit;

extern ActionCommit battle_committed_action;
extern u16 battle_held_slot_mask;            /* battle_committed_action.held addressed on its own */
extern u8 battle_item_counts[0x30];          /* battle_committed_action.itemCounts: item counts */
extern u8 battle_item_ids[0x30];             /* battle_committed_action.itemIds: item ids */
extern u8 battle_in_automatic_turn;
extern u8 battle_attack_approach_done;
extern u8 battle_gear_part_ids[0x30];
extern u8 battle_item_inventory_ids[0x30];   /* battle item ids: the setup (ovl2615) lists them, the results (ovl2596) read them */

/* The per-slot results of the battle work area (battle.data.ld). */
extern s32 battle_slot_damages[12];     /* per-slot damage */
extern s32 battle_enemy_damages[8];     /* the enemies' damage */
extern u8 battle_slot_result_codes[12]; /* per-slot result code */
extern u8 battle_enemy_result_codes[8]; /* the enemies' result codes */

/* Item effect table (0x10 bytes from 800d2200, in the battle work area's item
 * lists). */
typedef struct {
    u8 unk0[4];
    u16 target;    /* +0x4 target selection */
    u8 unk6[0x8 - 0x6];
    u8 amount;     /* +0x8 */
    u8 duration;   /* +0x9 */
    s16 flags;     /* +0xA effect bits; 1 selects the special effect amount */
    s16 status;    /* +0xC status bits */
    u16 animation; /* +0xE */
} ItemEffect;

extern ItemEffect battle_item_effects[];
extern void *battle_item_name_table;   /* item name table */

/* Queueing presentation events (80070E2C's unit). */
void battle_action_list_name(u8 index, u8 actor); /* show action index's name, queue its event */
void battle_action_list_event_f7(u8 value, u8 actor);
void battle_action_list_message_f8(u8 actor);

/* Action list handlers (800793f0's table). */
void battle_action_list_act(u8 actor, u8 index, u8 target);
void battle_action_list_approach(u8 actor, u8 index, u8 target);
void battle_action_list_event_fc(u8 actor, u8 index, u8 target);
void battle_action_list_event(u8 actor, u8 index, u8 target);
void battle_action_list_together(u8 actor, u8 index, u8 target);
void battle_action_list_leave(u8 actor, u8 index, u8 target);
void battle_action_list_split(u8 actor, u8 index, u8 target);
void battle_action_list_set_attr8(u8 actor, u8 index, u8 target);
void battle_action_list_add_attr8(u8 actor, u8 index, u8 target);
void battle_action_list_set_attr16(u8 actor, u8 index, u8 target);
void battle_action_list_add_attr16(u8 actor, u8 index, u8 target);
void battle_action_list_named_f4(u8 actor, u8 index, u8 target);
void battle_action_list_event_f6(u8 actor, u8 index, u8 target);
void battle_action_list_play(u8 actor); /* execute the actor's action list */

/* Committing, resolving and presenting results (battle.c). */
void battle_reset_running_results(void);                         /* reset the running results */
void battle_clear_event_results(void);                           /* clear the current event's results */
void battle_revive_slot(u8 slot);                                /* revive slot at full HP */
void battle_apply_status_drains(u8 slot);                        /* apply and show slot's recovery amounts */
void battle_accumulate_and_apply_results(u8 queue);              /* accumulate and apply event queue's results */
void battle_commit_action(u8 actor, u16 targets, u16 animation); /* commit an action and resolve it */
void battle_reset_events_for_target(u8 actor, u8 target);        /* reset the events for actor against target */

#endif
