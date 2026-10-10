#ifndef OVL3087_SCRIPT_ACTOR_H
#define OVL3087_SCRIPT_ACTOR_H

#include "common.h"
#include "resident/sprite.h"

/* Actor and model helpers of the battle event scripts (801e93e8-801e9b58),
 * a separate unit built by a later compiler (see ovl3087.mk), on the battle
 * slots' sprites (the resident's Sprite) and the script slots' model sprites
 * (SpriteTasks). The interpreter (battle_event_script_vm.c) declares these helpers with its
 * own prototypes. */

/* Resident functions whose callers convert arguments/result differently
 * from the resident definition (decomp/src/resident/own_declarations.h). */
void sprite_set_direction(Sprite *sprite, s32 arg);
void sprite_set_facing(Sprite *sprite, s32 arg);

/* Defined u8 in the battle (own_declarations.h); u8 here adds andi 0xff to the result's test. */
s32 battle_single_action_start(void);

void battle_event_script_complete_actor_action(Sprite *actor);
void battle_event_script_start_actor_animation(s32 actor, s32 animation);
void battle_event_script_return_actor_to_idle(s32 actor);
void battle_event_script_stop_actor_commands(s32 actor);
void battle_event_script_clear_actor_countdown(s32 actor);
void battle_event_script_play_queued_animation(Sprite *actor);
void battle_event_script_start_actor_move(s16 actor, s16 x, s16 y, s16 z);
void battle_event_script_start_actor_animation3(s16 actor, s16 x, s16 y, s16 z);
void battle_event_script_give_actor_command(s32 actor, s32 command);
void battle_event_script_run_actor_attack(s32 actor, s32 target);
void battle_event_script_turn_actor_to_target(s32 actor, u16 target);
void battle_event_script_play_model_animation(SpriteTask *model, s32 animation);
SpriteTask *battle_event_script_create_model(void *file, s16 *position);
void battle_event_script_destroy_model(SpriteTask *model);
void battle_event_script_reset_camera(void);

#endif
