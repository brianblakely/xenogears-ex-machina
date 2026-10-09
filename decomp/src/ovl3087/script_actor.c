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
    D_800D3278->actionRunning[SPRITE_SLOT(actor)] = 0;
    func_80021BF8(actor, 0);
}

/* Start an animation on actor n and report its completion; a negative
 * animation first restores the actor's battle pose when one is pending. */
void func_801E9430(s32 actor, s32 animation) {
    Sprite *self;

    D_800C360C = 1;
    self = D_800C3EB0.sprites[actor];
    if (animation < 0 && D_800D3350 == 0 && D_800C3618 != 0) {
        func_800B8354();
        self->word50 = func_800BF354();
        func_80021BF0(self, (s32)D_800C3618);
    }
    func_800245D8(self, animation);
    func_80021BF8(self, func_801E93E8);
    if (animation == 1) {
        SPRITE_NEXT_MOTION(self) = animation;
    } else {
        SPRITE_NEXT_MOTION(self) = -1;
    }
}

/* Return actor n to its idle animation. */
void func_801E950C(s32 actor) {
    Sprite *self = D_800C3EB0.sprites[actor];

    func_800245D8(self, (s8)self->motion.bytes[3]);
}

/* Stop actor n's commands: clear its command countdown, its pending frame
 * and flag bits 2-7. */
void func_801E9550(s32 actor) {
    Sprite *self = D_800C3EB0.sprites[actor];

    self->countdown = 0;
    self->frame = 0;
    self->flags &= ~0xFC;
}

/* Clear actor n's command countdown. */
void func_801E958C(s32 actor) {
    D_800C3EB0.sprites[actor]->countdown = 0;
}

/* Movement end callback: play the animation queued after the action (1
 * when none). */
void func_801E95B0(Sprite *actor) {
    s32 animation = (s8)actor->b0.byteb0;

    if (animation < 0) {
        animation = 1;
    }
    func_800245D8(actor, animation);
}

/* Move actor n to (x, y, z) with animation 2 and report the completion. */
void func_801E95E4(s16 actor, s16 x, s16 y, s16 z) {
    Sprite *self = D_800C3EB0.sprites[actor];

    self->target_x = x;
    self->target_y = y;
    self->target_z = z;
    func_80021FE0(self, func_800BEF8C(self));
    func_800223B0(self, func_800BEF8C(self));
    func_800245D8(self, 2);
    func_800BF7C8(self, 8, func_801E95B0);
    func_80021BF8(self, func_801E93E8);
}

/* Run actor n's action 3 with (x, y, z) and report the completion. */
void func_801E9694(s16 actor, s16 x, s16 y, s16 z) {
    Sprite *self = D_800C3EB0.sprites[actor];

    self->target_x = x;
    self->target_y = y;
    self->target_z = z;
    func_800245D8(self, 3);
    func_80021BF8(self, func_801E93E8);
}

/* Set actor n's idle animation (0, or 0x11 while a frame is pending) and run
 * command `arg1` during its motion. */
void func_801E9700(s32 actor, s32 arg1) {
    Sprite *self = D_800C3EB0.sprites[actor];

    if (self->frame == 0) {
        self->motion.bytes[3] = 0;
    } else {
        self->motion.bytes[3] = 0x11;
    }
    func_800BF600(arg1, self);
}

/* Actor n attacks the target the battle search picks (itself when none):
 * wait for the next frame, run the attack, wait again. */
void func_801E9760(s32 actor, s32 target) {
    Sprite *self = D_800C3EB0.sprites[actor];
    s32 any = 0xFFFF;

    D_800D3634 = any;
    D_800D3678 = func_800BEEB4(any, D_800D363C, self);
    if (D_800D3678 == 0) {
        D_800D363C[0] = self;
    }
    self->partner = D_800D363C[0];
    while (D_80059464 != func_800BF720()) {
        func_800BE790();
    }
    D_80059464 = 0;
    D_800591AC = 1;
    if (func_800B7E94() != 0) {
        func_80021BF8(self, D_800B9B30);
    } else {
        if (self->frame != 0) {
            func_800245D8(self, 0x12);
        }
        func_800BC404(D_800D3634);
    }
    while (D_80059464 != func_800BF720()) {
        func_800BE790();
    }
    D_80059464 = 0;
    D_800591AC = 0;
}

/* Turn actor n towards actor m and make m its target. */
void func_801E9894(s32 actor, u16 target) {
    Sprite *self = D_800C3EB0.sprites[actor];
    Sprite **search = D_800D363C;
    Sprite *other = D_800C3EB0.sprites[target];

    search[1] = NULL;
    search[0] = other;
    self->partner = other;
    func_800223B0(self, func_800BEF24(self, other));
    func_80021FE0(self, func_800BEF24(self, other));
    if (actor < 3) {
        func_800BF2B8(self);
    }
}

/* Play an animation on a script slot model. */
void func_801E9958(SpriteTask *model, s32 animation) {
    func_800245D8(&model->sprite, animation);
}

/* Create a script slot model from model data at a position. */
SpriteTask *func_801E9978(void *file, s16 *position) {
    SpriteTask *model;
    Sprite *body;
    s32 frame = D_80059464;

    model = (SpriteTask *)func_8001D1D8(0x19C, 0, D_800BAC50, D_800BAB0C, D_800BABDC);
    body = &model->sprite;
    D_80059464 = frame;
    model->task.link.word &= 0x7FFFFFFF;
    body->block = model;
    model->task.data = body;
    model->auxiliary.data = body;
    func_80023804(body);
    func_800239A0(body);
    body->animations = file;
    body->flags = (body->flags & 0xFFFE1FFF) | 0x8000;
    body->render.word &= ~3;
    func_800222BC(body, file);
    model->sprite.x = position[0] << 16;
    body->y = position[1] << 16;
    body->z = position[2] << 16;
    body->b0.byteb0 = 0;
    body->direction = 0;
    func_80022000(body, 0x2000);
    body->word82 = 0x2000;
    body->countdown = 0;
    func_800BC3F8(0);
    func_800BC2F0(0);
    D_800C37C8 = 1;
    return model;
}

/* Release a script slot model. */
void func_801E9AD4(SpriteTask *model) {
    func_8001CE74(&model->task);
    func_8001CB48(&model->auxiliary);
    func_8001CD94(&model->task);

    func_800320E8(model);
    func_800BC3F8(1);
    func_800BC2F0(1);
    D_800C37C8 = 0;
}

/* Camera value and mode 0 (800bc3f8, 800bc2f0) and effects disabled, as a
 * script slot model's creation leaves them. */
void func_801E9B2C(void) {

    func_800BC3F8(0);
    func_800BC2F0(0);
    D_800C37C8 = 1;
}
