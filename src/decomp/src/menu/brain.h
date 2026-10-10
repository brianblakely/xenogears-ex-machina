#ifndef MENU_BRAIN_H
#define MENU_BRAIN_H

#include "common.h"
#include "actor.h"

/* The computer opponent (arena_scene_graph_and_opponent 8008EE1C-80090F38): a brain per side that
 * picks commands by mode (idle, attack, distance, approach) from its
 * tendencies and the actors' distance, charge and hp, and enters them as
 * the actor's pad inputs. */

/* The computer opponent's decision state, handed to its command handlers. */
typedef struct Brain {
    Actor *owner;
    s16 timer;          /* 0x04: frames until the next decision */
    u8 unk6;
    u8 unk7;
    u8 mode;            /* 0x08: 0 idle, 1 attack, 2 distance, 3 approach */
    u8 unk9;
    s16 unkA;
    s16 unkC;
    u8 unkE;
    u8 unkF;
    s32 unk10;          /* 0x10: attack eagerness */
    s32 unk14;
    s32 unk18;          /* 0x18: chance to press an attack */
    s32 unk1C;          /* 0x1C: eagerness when not keen */
    s32 unk20;
    s32 unk24;          /* 0x24: charge it waits for */
    s32 unk28;
    u32 roll : 8;       /* 0x2C: a random byte for this round */
    u32 unk2C_8 : 1;
    u32 unk2C_9 : 1;
    u32 unk2C_10 : 1;
    u32 unk2C_11 : 1;
    u32 unk2C_12 : 1;
    u32 defending : 1;  /* 0x2C bit 13 */
    u32 unk2C_14 : 2;
    u32 unk2E : 8;      /* 0x2E */
    u32 unk2C_24 : 8;
    s16 unk30;
} Brain;

extern u8 arena_retreat_rule_enabled; /* enables the retreat rule */

void arena_brain_run_practice_command(Actor *actor);
s32 arena_brain_is_hp_above_fraction(Actor *actor, s32 fraction);
s32 arena_brain_check_special_charge(Actor *actor, s32 check);
s32 arena_brain_get_charge_after_special(Actor *actor, Brain *brain);
s32 arena_brain_decide_close_in(Actor *actor, s32 eager);
void arena_brain_roll_choices(Brain *brain);
s32 arena_brain_is_in_far_quadrant(Actor *actor);
s32 arena_brain_apply_retreat_rule(Actor *actor, Brain *brain);
void arena_brain_react_with_guard(Actor *actor, Brain *brain);
void arena_brain_enter_idle_mode(Actor *actor);
void arena_brain_queue_random_attacks(Actor *actor);
void arena_brain_take_attack_step(Actor *actor, Brain *brain);
s32 arena_brain_use_special_move(Actor *actor, Brain *brain);
void arena_brain_enter_attack_mode(Actor *actor);
s32 arena_brain_choose_attack(Actor *actor, Brain *brain);
void arena_brain_enter_approach_mode(Actor *actor, s32 kind);
void arena_brain_enter_distance_mode(Actor *actor, s32 kind);
void arena_brain_attach(Actor *actor);
void arena_brain_update(Actor *actor);

#endif
