/* Actor and model helpers the battle event script opcodes call: actor
 * animations and actions (with completion reported to the interpreter),
 * the scripted attack, and the script slot models. */
#include "ovl3087.h"
#include "script_actor.h"

#ifdef NON_MATCHING
/* Action completion callback: clear the actor's action-running flag and
 * remove the callback. */
void func_801E93E8(BattleActor *actor) {
    D_800D3278->actionRunning[actor->slotLow | (actor->slotHigh << 2)] = 0;
    func_80021BF8(actor, 0);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E93E8);
#endif

#ifdef NON_MATCHING
/* Start an animation on actor n and report its completion; a negative
 * animation first restores the actor's battle pose when one is pending. */
void func_801E9430(s32 actor, s32 animation) {
    BattleActor *self;

    D_800C360C = 1;
    self = D_800C3EB0.actors[actor];
    if (animation < 0 && D_800D3350 == 0 && D_800C3618 != 0) {
        func_800B8354();
        self->unk50 = func_800BF354();
        func_80021BF0(self, D_800C3618);
    }
    func_800245D8(self, animation);
    func_80021BF8(self, func_801E93E8);
    if (animation == 1) {
        self->nextAnimation = animation;
    } else {
        self->nextAnimation = -1;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9430);
#endif

#ifdef NON_MATCHING
/* Return actor n to its idle animation. */
void func_801E950C(s32 actor) {
    BattleActor *self = D_800C3EB0.actors[actor];

    func_800245D8(self, self->idleAnimation);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E950C);
#endif

#ifdef NON_MATCHING
void func_801E9550(s32 actor) {
    BattleActor *self = D_800C3EB0.actors[actor];

    self->unk9E = 0;
    self->unk34 = 0;
    self->flags40 &= ~0xFC;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9550);
#endif

#ifdef NON_MATCHING
void func_801E958C(s32 actor) {
    D_800C3EB0.actors[actor]->unk9E = 0;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E958C);
#endif

#ifdef NON_MATCHING
/* Movement end callback: play the animation queued after the action (1
 * when none). */
void func_801E95B0(BattleActor *actor) {
    s32 animation = actor->nextAnimation;

    if (animation < 0) {
        animation = 1;
    }
    func_800245D8(actor, animation);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E95B0);
#endif

#ifdef NON_MATCHING
/* Move actor n to (x, y, z) with animation 2 and report the completion. */
void func_801E95E4(s16 actor, s16 x, s16 y, s16 z) {
    BattleActor *self = D_800C3EB0.actors[actor];

    self->argA0 = x;
    self->argA2 = y;
    self->argA4 = z;
    func_80021FE0(self, func_800BEF8C(self));
    func_800223B0(self, func_800BEF8C(self));
    func_800245D8(self, 2);
    func_800BF7C8(self, 8, func_801E95B0);
    func_80021BF8(self, func_801E93E8);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E95E4);
#endif

#ifdef NON_MATCHING
/* Run actor n's action 3 with (x, y, z) and report the completion. */
void func_801E9694(s16 actor, s16 x, s16 y, s16 z) {
    BattleActor *self = D_800C3EB0.actors[actor];

    self->argA0 = x;
    self->argA2 = y;
    self->argA4 = z;
    func_800245D8(self, 3);
    func_80021BF8(self, func_801E93E8);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9694);
#endif

#ifdef NON_MATCHING
void func_801E9700(s32 actor, s32 arg1) {
    BattleActor *self = D_800C3EB0.actors[actor];

    if (self->unk34 == 0) {
        self->idleAnimation = 0;
    } else {
        self->idleAnimation = 0x11;
    }
    func_800BF600(arg1);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9700);
#endif

#ifdef NON_MATCHING
/* Actor n attacks the target the battle search picks (itself when none):
 * wait for the next frame, run the attack, wait again. */
void func_801E9760(s32 actor, s32 target) {
    BattleActor *self = D_800C3EB0.actors[actor];

    D_800D3634 = 0xFFFF;
    D_800D3678 = func_800BEEB4(0xFFFF, &D_800D363C, self);
    if (D_800D3678 == 0) {
        D_800D363C.target = self;
    }
    self->target = D_800D363C.target;
    while (func_800BF720() != D_80059464) {
        func_800BE790();
    }
    D_80059464 = 0;
    D_800591AC = 1;
    if (func_800B7E94() != 0) {
        func_80021BF8(self, D_800BAB30);
    } else {
        if (self->unk34 != 0) {
            func_800245D8(self, 0x12);
        }
        func_800BC404(D_800D3634);
    }
    while (func_800BF720() != D_80059464) {
        func_800BE790();
    }
    D_80059464 = 0;
    D_800591AC = 0;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9760);
#endif

#ifdef NON_MATCHING
/* Turn actor n towards actor m and make m its target. */
void func_801E9894(s32 actor, u16 target) {
    BattleActor *self = D_800C3EB0.actors[actor];
    BattleActor *other = D_800C3EB0.actors[target];

    D_800D363C.unk4 = 0;
    D_800D363C.target = other;
    self->target = other;
    func_800223B0(self, func_800BEF24(self, other));
    func_80021FE0(self, func_800BEF24(self, other));
    if (actor < 3) {
        func_800BF2B8(self);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9894);
#endif

#ifdef NON_MATCHING
/* Play an animation on a script slot model. */
void func_801E9958(BattleModel *model, s32 animation) {
    func_800245D8(&model->body, animation);
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9958);
#endif

#ifdef NON_MATCHING
/* Create a script slot model from model data at a position. */
BattleModel *func_801E9978(void *file, s16 *position) {
    BattleModel *model;
    BattleActor *body;
    s32 frame = D_80059464;

    model = func_8001D1D8(0x19C, 0, D_800BAC50, D_800BAB0C, D_800BABDC);
    body = &model->body;
    D_80059464 = frame;
    model->flags &= 0x7FFFFFFF;
    body->owner = model;
    model->actor = body;
    model->actor2 = body;
    func_80023804(body);
    func_800239A0(body);
    body->file = file;
    body->flags40 = (body->flags40 & 0xFFFE1FFF) | 0x8000;
    body->flags3C &= ~3;
    func_800222BC(body, file);
    model->body.x = position[0] << 16;
    body->y = position[1] << 16;
    body->z = position[2] << 16;
    body->nextAnimation = 0;
    body->unk32 = 0;
    func_80022000(body, 0x2000);
    body->unk82 = 0x2000;
    body->unk9E = 0;
    func_800BC3F8(0);
    func_800BC2F0(0);
    D_800C37C8 = 1;
    return model;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9978);
#endif

#ifdef NON_MATCHING
/* Release a script slot model. */
void func_801E9AD4(BattleModel *model) {
    func_8001CE74(model);
    func_8001CB48(model->unk1C);
    func_8001CD94(model);
    func_800320E8(model);
    func_800BC3F8(1);
    func_800BC2F0(1);
    D_800C37C8 = 0;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9AD4);
#endif

#ifdef NON_MATCHING
void func_801E9B2C(void) {
    func_800BC3F8(0);
    func_800BC2F0(0);
    D_800C37C8 = 1;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/script_actor", func_801E9B2C);
#endif
