/* Actor and model helpers the battle event script opcodes call
 * (801e93e8-801e9b5c): actor animations and actions (with completion reported
 * to the interpreter), the scripted attack, and the script slot models. The
 * unit is built by the Cygnus CDK GCC 2.7.2, which alone reproduces its
 * functions (ovl3087.mk); it starts at the first of them, after the
 * interpreter's last opcode handler, and ends at the overlay's data
 * (801e9b5c, ovl3087.c's). */
#include "common.h"
#include "resident/heap.h"
#include "resident/sprite.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/scene.h"
#include "battle/sprite.h"
#include "script_actor.h"

/* Action completion callback: clear the actor's action-running flag and
 * remove the callback. */
void func_801E93E8(Sprite *actor) {
    battle_state_of_event_script->actionRunning[SPRITE_SLOT(actor)] = 0;
    sprite_set_completion_callback(actor, 0);
}

/* Start an animation on actor n and report its completion; a negative
 * animation first restores the actor's battle pose when one is pending. */
void func_801E9430(s32 actor, s32 animation) {
    Sprite *self;

    battle_area_event_index = 1;
    self = battle_area.sprites[actor];
    if (animation < 0 && battle_command_file_started == 0 && battle_command_file != 0) {
        battle_wait_for_disc();
        self->word50 = battle_start_command_file_once();
        sprite_set_alternate_resource(self, (s32)battle_command_file);
    }
    sprite_start_animation(self, animation);
    sprite_set_completion_callback(self, func_801E93E8);
    if (animation == 1) {
        SPRITE_NEXT_MOTION(self) = animation;
    } else {
        SPRITE_NEXT_MOTION(self) = -1;
    }
}

/* Return actor n to its idle animation. */
void func_801E950C(s32 actor) {
    Sprite *self = battle_area.sprites[actor];

    sprite_start_animation(self, (s8)self->motion.bytes[3]);
}

/* Stop actor n's commands: clear its command countdown, its pending frame
 * and flag bits 2-7. */
void func_801E9550(s32 actor) {
    Sprite *self = battle_area.sprites[actor];

    self->countdown = 0;
    self->frame = 0;
    self->flags &= ~0xFC;
}

/* Clear actor n's command countdown. */
void func_801E958C(s32 actor) {
    battle_area.sprites[actor]->countdown = 0;
}

/* Movement end callback: play the animation queued after the action (1
 * when none). */
void func_801E95B0(Sprite *actor) {
    s32 animation = (s8)actor->b0.byteb0;

    if (animation < 0) {
        animation = 1;
    }
    sprite_start_animation(actor, animation);
}

/* Move actor n to (x, y, z) with animation 2 and report the completion. */
void func_801E95E4(s16 actor, s16 x, s16 y, s16 z) {
    Sprite *self = battle_area.sprites[actor];

    self->target_x = x;
    self->target_y = y;
    self->target_z = z;
    sprite_set_direction(self, battle_get_target_direction(self));
    sprite_set_facing(self, battle_get_target_direction(self));
    sprite_start_animation(self, 2);
    battle_distance_watch_start(self, 8, func_801E95B0);
    sprite_set_completion_callback(self, func_801E93E8);
}

/* Run actor n's action 3 with (x, y, z) and report the completion. */
void func_801E9694(s16 actor, s16 x, s16 y, s16 z) {
    Sprite *self = battle_area.sprites[actor];

    self->target_x = x;
    self->target_y = y;
    self->target_z = z;
    sprite_start_animation(self, 3);
    sprite_set_completion_callback(self, func_801E93E8);
}

/* Set actor n's idle animation (0, or 0x11 while a frame is pending) and run
 * command `arg1` during its motion. */
void func_801E9700(s32 actor, s32 arg1) {
    Sprite *self = battle_area.sprites[actor];

    if (self->frame == 0) {
        self->motion.bytes[3] = 0;
    } else {
        self->motion.bytes[3] = 0x11;
    }
    battle_single_action_load_during_motion(arg1, self);
}

/* Actor n attacks the target the battle search picks (itself when none):
 * wait for the next frame, run the attack, wait again. */
void func_801E9760(s32 actor, s32 target) {
    Sprite *self = battle_area.sprites[actor];
    s32 any = 0xFFFF;

    battle_area_event_target_mask = any;
    battle_area_event_target_count = battle_list_slot_sprites(any, battle_area_event_target_sprites, self);
    if (battle_area_event_target_count == 0) {
        battle_area_event_target_sprites[0] = self;
    }
    self->partner = battle_area_event_target_sprites[0];
    while (task_active_main_count != battle_is_total_popup_shown()) {
        battle_run_frame();
    }
    task_active_main_count = 0;
    task_new_tasks_active = 1;
    if (battle_single_action_start() != 0) {
        sprite_set_completion_callback(self, battle_menu_mark_sprite_done);
    } else {
        if (self->frame != 0) {
            sprite_start_animation(self, 0x12);
        }
        battle_camera_start_move(battle_area_event_target_mask);
    }
    while (task_active_main_count != battle_is_total_popup_shown()) {
        battle_run_frame();
    }
    task_active_main_count = 0;
    task_new_tasks_active = 0;
}

/* Turn actor n towards actor m and make m its target. */
void func_801E9894(s32 actor, u16 target) {
    Sprite *self = battle_area.sprites[actor];
    Sprite **search = battle_area_event_target_sprites;
    Sprite *other = battle_area.sprites[target];

    search[1] = NULL;
    search[0] = other;
    self->partner = other;
    sprite_set_facing(self, battle_get_sprite_direction(self, other));
    sprite_set_direction(self, battle_get_sprite_direction(self, other));
    if (actor < 3) {
        battle_load_slot_command_file(self);
    }
}

/* Play an animation on a script slot model. */
void func_801E9958(SpriteTask *model, s32 animation) {
    sprite_start_animation(&model->sprite, animation);
}

/* Create a script slot model from model data at a position. */
SpriteTask *func_801E9978(void *file, s16 *position) {
    SpriteTask *model;
    Sprite *body;
    s32 frame = task_active_main_count;

    model = (SpriteTask *)task_alloc_two_node_task(0x19C, 0, battle_sprite_task_update, battle_sprite_task_draw, battle_sprite_task_destroy);
    body = &model->sprite;
    task_active_main_count = frame;
    model->task.link.word &= 0x7FFFFFFF;
    body->block = model;
    model->task.data = body;
    model->auxiliary.data = body;
    sprite_reset_defaults(body);
    sprite_attach_inline_storage(body);
    body->animations = file;
    body->flags = (body->flags & 0xFFFE1FFF) | 0x8000;
    body->render.word &= ~3;
    sprite_bind_resource(body, file);
    model->sprite.x = position[0] << 16;
    body->y = position[1] << 16;
    body->z = position[2] << 16;
    body->b0.byteb0 = 0;
    body->direction = 0;
    sprite_set_scale(body, 0x2000);
    body->word82 = 0x2000;
    body->countdown = 0;
    battle_camera_set_resume_mode(0);
    battle_camera_set_mode(0);
    battle_effects_disabled = 1;
    return model;
}

/* Release a script slot model. */
void func_801E9AD4(SpriteTask *model) {
    task_destroy_owned_by(&model->task);
    task_unlink_draw_node(&model->auxiliary);
    task_unlink_main_node(&model->task);
    heap_free(model);
    battle_camera_set_resume_mode(1);
    battle_camera_set_mode(1);
    battle_effects_disabled = 0;
}

/* Camera value and mode 0 (800bc3f8, 800bc2f0) and effects disabled, as a
 * script slot model's creation leaves them. */
void func_801E9B2C(void) {
    battle_camera_set_resume_mode(0);
    battle_camera_set_mode(0);
    battle_effects_disabled = 1;
}
