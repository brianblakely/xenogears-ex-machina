#ifndef BATTLE_FLOW_H
#define BATTLE_FLOW_H

#include "common.h"
#include "battle/area.h"
#include "battle/frame.h"

/* The battle's flow (800B8098's unit, 800B8098-800B9F78): its start and
 * closing, the loads it waits for, and the acting slot's turn run through
 * the battle menu (battle_current_menu): sound playback that waits for its end and
 * the acting sprite's walks. */

/* A sound of the battle's table (4 bytes): its sound bank (the wave bank
 * follows it) and its sound number in the bank. */
typedef struct {
    u16 bank;
    s16 sound;
} BattleSound;

extern BattleSound battle_sound_table[];

extern void *battle_enemy_set_copy; /* the enemy set data copy */

/* The acting slot's turn (800B89FC-800B9F78). */
extern u8 battle_sprite_script_finished;

/* battle_acting_with_partner (set when the actor acts with its partner, 8008B478), as the
 * late unit addresses it inside the area. */
#define AREA_PARTNER_ACTION (((u8 *)&BATTLE_AREA)[0xA78])

/* D_800C4923, after the acting slot, as the late unit addresses it. */
#define AREA_BYTE_A73 (((u8 *)&BATTLE_AREA)[0xA73])

extern u16 battle_requested_single_action; /* the single action to request (effect VM 71, 73) */

extern s32 battle_menu_update_running; /* the battle menu update is running */
extern s16 battle_area_event_delay_timer; /* frames before the next event */
extern u8 battle_single_action_keeps_pose_13;
extern u16 battle_gear_sound_played_slots; /* slots whose gear sound played */
extern s16 battle_single_action_base_table[]; /* per target code: its first command */
extern s16 battle_single_action_split_table[]; /* per target code: its commands from here play motion 0x11 */

void battle_start_intro(s32 kind);  /* start the battle in a mode */
void battle_enter(s32 a);     /* enter the battle */
void battle_wait_for_disc(void);      /* run frames while the disc is busy */
void battle_close(s32 mode);  /* close the battle */
void battle_menu_open_turn(s32 mode, s32 slot, s32 targets, s32 next_slot); /* open the battle menu for a turn */
void battle_finish_loads(void);      /* finish the battle's loads */
void battle_stop_reads_finish_loads(void);      /* stop the resident transfer and finish the loads */
void battle_menu_add_step(void);      /* count a step of the battle menu */
/* Mark the battle menu (field48) with its state; a sprite callback (also the
 * event script overlay's). Declared without a prototype: 800B9508 also calls
 * it with the sprite (defined (void)). */
void battle_menu_mark_sprite_done();
/* Put the sprite at its target, idle, facing the other; defined without a
 * prototype (800BF0C4 calls it with the sprite alone). */
void battle_sprite_arrive_at_target();
void battle_menu_update(BattleMenu *menu); /* the battle menu's update */

#endif
