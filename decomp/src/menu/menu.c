#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"

/* Start the menu camera: mode 3 setup and its script block. */
void func_800707A8(void) {
    func_80083C0C(3);
    func_800346D4(&D_80092954);
}

/* One easing step from current toward target: the remaining distance
 * (rounded away from zero) divided by the number of steps. */
s32 func_800707D8(s32 target, s32 current, s32 steps) {
    s32 delta = target - current;

    if (delta < 0) {
        delta++;
        delta -= steps;
    } else {
        delta--;
        delta += steps;
    }
    return delta / steps;
}

/* Ease the camera eye toward target over the given number of steps; the
 * eye height is compared including the current lift. */
void func_80070808(Vector *target, s32 steps) {
    D_8009867C.vx += func_800707D8(target->vx, D_8009867C.vx, steps);
    D_8009867C.vz += func_800707D8(target->vz, D_8009867C.vz, steps);
    D_8009867C.vy += func_800707D8(target->vy, D_8009867C.vy + D_800925F4, steps);
}

/* Ease the camera look-at point toward target, limited by the collision
 * step check. */
void func_800708C4(Vector *target, s32 steps) {
    Vector step;

    step.vx = func_800707D8(target->vx, D_8009871C.vx, steps);
    step.vy = func_800707D8(target->vy, D_8009871C.vy, steps);
    step.vz = func_800707D8(target->vz, D_8009871C.vz, steps);
    func_800828F8(&D_8009871C, &step, 0x3D00);
    D_8009871C.vx += step.vx;
    D_8009871C.vy += step.vy;
    D_8009871C.vz += step.vz;
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FAF0);

#ifdef NON_MATCHING
/* Place the menu camera for one of the view modes. Does not match: GCC
 * 8-aligns the jump table (original at 0x8006faf4), and cases 3/4 are
 * cross-jumped after the look-at copy rather than before the lift store. */
void func_8007099C(u32 mode) {
    Vector target;

    switch (mode) {
    case 0:
        func_80083738(&D_8009872C, &D_80097010);
        break;
    case 1:
        target = D_8009872C.pos;
        D_800925F4 = 0;
        target.vy -= 0xA0;
        func_80070808(&target, 4);
        break;
    case 2:
        target = D_80097010.pos;
        D_800925F4 = 0;
        target.vy -= 0xA0;
        func_80070808(&target, 4);
        break;
    case 3:
        D_800925F4 = 0xA0;
        target = D_80099078;
        target.vy += D_800925F4;
        func_80070808(&target, 0x10);
        target.vx = D_8009872C.pos.vx + ((func_8003F8B0(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        target.vy = D_8009872C.pos.vy - 0x20 - D_800925F4;
        target.vz = D_8009872C.pos.vz + ((func_8003F8CC(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        func_800708C4(&target, 0x46);
        {
            s32 top = func_80082488(&D_8009871C, 0) - 0x40 - D_800925F4;
            if (top < D_8009871C.vy) {
                D_8009871C.vy = top;
            }
        }
        break;
    case 4:
        D_800925F4 = 0x80;
        target = D_80099078;
        target.vy += D_800925F4;
        func_80070808(&target, 0x10);
        target.vx = D_8009872C.pos.vx + ((func_8003F8B0(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        target.vy = D_8009872C.pos.vy - 0x20 - D_800925F4;
        target.vz = D_8009872C.pos.vz + ((func_8003F8CC(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        func_800708C4(&target, 0x46);
        {
            s32 top = func_80082488(&D_8009871C, 0) - 0x40 - D_800925F4;
            if (top < D_8009871C.vy) {
                D_8009871C.vy = top;
            }
        }
        break;
    case 5:
        D_8009867C.vx = D_80097010.pos.vx;
        D_8009867C.vy = D_80097010.pos.vy - 0xC0;
        D_8009867C.vz = D_80097010.pos.vz;
        D_8009871C.vx = D_8009867C.vx + ((func_8003F8B0(D_80097010.angle + 0x900) * 0xE0) >> 12);
        D_8009871C.vy = D_80097010.pos.vy - 0xD0;
        D_8009871C.vz = D_8009867C.vz + ((func_8003F8CC(D_80097010.angle + 0x900) * 0xE0) >> 12);
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007099C);
#endif

/* Re-centre the two actors and the look-at point on a fixed scene spot:
 * the midpoint of the actors moves to the layout's anchor, actors on the
 * floor and the look-at point at a fixed height. */
void func_80070C7C(s32 layout) {
    Vector first = D_8009872C.pos;
    Vector second = D_80097010.pos;
    Vector look = D_8009871C;
    Vector centre = first;

    centre.vx += second.vx;
    centre.vy += second.vy;
    centre.vz += second.vz;
    centre.vx /= 2;
    centre.vy /= 2;
    centre.vz /= 2;
    first.vx -= centre.vx;
    first.vy -= centre.vy;
    first.vz -= centre.vz;
    second.vx -= centre.vx;
    second.vy -= centre.vy;
    second.vz -= centre.vz;
    look.vx -= centre.vx;
    look.vy -= centre.vy;
    look.vz -= centre.vz;
    switch (layout) {
    case 0:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x4000;
        break;
    case 1:
        centre.vx = 0x6000;
        centre.vy = 0;
        centre.vz = 0x6000;
        break;
    case 2:
        centre.vx = 0x2000;
        centre.vy = 0;
        centre.vz = 0x2000;
        break;
    case 3:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x2400;
        break;
    }
    first.vx += centre.vx;
    first.vy += centre.vy;
    first.vz += centre.vz;
    second.vx += centre.vx;
    second.vy += centre.vy;
    second.vz += centre.vz;
    look.vx += centre.vx;
    look.vy += centre.vy;
    look.vz += centre.vz;
    first.vy = 0;
    second.vy = 0;
    look.vy = -0x300;
    D_8009872C.pos = first;
    D_80097010.pos = second;
    D_8009871C = look;
    func_8007E24C();
}

/* Start a scene script; reset both actors' states and clear their 0x8000
 * flag. */
void func_80070F80(u8 *script) {
    D_800925F8 = script;
    D_8009872C.state = 0;
    D_80097010.state = 0;
    D_800925FC = 0;
    D_8009872C.flags &= ~0x8000;
    D_80097010.flags &= ~0x8000;
}

/* Turn an actor toward one of two headings depending on which side of the
 * scene centre it stands, and reset its state. */
s32 func_80070FD8(Actor *actor) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((func_8004B32C(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        actor->target_angle = 0x800 - D_80092934;
    } else {
        actor->target_angle = 0xC00 - D_80092934;
    }
    actor->state = 0xFF;
    actor->unkCE = 0;
    return 0;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007107C);

/* Link the screen offset packet and this frame's texture page packet. */
void func_80071724(u32 *ot) {
    Window *frame = D_80092868;

    /* x0 and y0 of the offset sprite, stored as one word */
    *(u32 *)&frame->sprite.x0 = D_800925E0 | (D_800925E4 << 16);
    func_80043B48(ot, &frame->sprite);
    func_80043B48(ot, &D_800929E4[D_800928A0]);
}

/* Upload the menu's sprite sheet TIM (its first CLUT colour made
 * transparent), build both texture page packets and the sprite template. */
void func_80071794(u32 **resources) {
    TimImage image;
    Rect unused; /* the original frame reserves 8 more bytes */
    s16 *clut;

    func_800471B4(resources[0x60 / 4]);
    func_800471C4(&image);
    clut = (s16 *)image.caddr;
    clut[2] = -0x8000;
    clut[0] = 0;
    clut[3] = -1;
    func_80044894(image.crect, image.caddr);
    func_80044894(image.prect, image.paddr);
    func_80043E20(&D_800929E4[0], 0, 0, func_80043A1C(0, 1, image.prect->x, image.prect->y));
    D_800929E4[1] = D_800929E4[0];
    D_8009A14C.u0 = (image.prect->x & 0x3F) * 4;
    D_8009A14C.v0 = image.prect->y;
    D_8009A14C.clut = func_80043A58(image.crect->x, image.crect->y);
    D_8009A244 = D_8009A14C;
}

/* Open the menu message window. */
void func_800718C0(void) {
    D_800925D8 = -1;
    D_800925D4 = 0;
    func_80032F54(&D_8009868C, 0x140, 0x30, 0x1C, 0x9A, 0x40, 4);
}

/* Enter a menu scene: the first scene also starts sound 0x37 and uses a
 * taller window; restarts both actors at full HP and centres the screen
 * offset. */
void func_8007191C(s32 scene) {
    D_80092608 = scene == 0;
    if (scene == 0) {
        func_8008EB4C(0x37);
        D_8009868C.unkC = 2;
        D_8009868C.unk6 = 0xB4;
    } else {
        D_8009868C.unkC = 4;
        D_8009868C.unk6 = 0x9A;
    }
    func_80070F80(D_8009105C[scene]);
    D_800925E8 = 0xA0;
    D_800925E0 = 0xA0;
    D_800925EC = 0x6D;
    D_800925E4 = 0x6D;
    D_80092600 = 0;
    D_8009872C.hp = D_8009872C.max_hp;
    D_80097010.hp = D_80097010.max_hp;
}

/* Start the menu's opening: text window with message 0x42, the intro
 * script, then scene 9. */
void func_800719F0(void) {
    D_80099D9D = 0;
    D_80099D98.driven = 0;
    func_80083C0C(7);
    D_800928C8 = 5;
    D_80092884 = 0;
    func_80032F54(&D_80092954, 0x140, 0x70, 0xA2, 0x2A, 0x1C, 8);
    func_80034714(&D_80092954, func_80033728(D_80092880, 0x42));
    D_800929BC = 0x1E;
    D_800925DC = 0;
    func_80070F80(D_80090F38);
    D_80092604 = 0;
    D_80092904 = 0;
    D_80092900 = 0;
    func_8007191C(9);
}

/* Per-frame menu scene update: scene choice input, the scene script, the
 * screen offset easing, the message window and the camera. */
void func_80071AD0(void) {
    MenuWindow *message;

    if (D_800925F0 != 0 && (D_800928E8 & 4)) {
        func_80071724(D_80092938);
    }
    func_80036420();
    message = &D_8009868C;
    if (D_80092608 != 0) {
        if (D_800594A4 & 0x1000) {
            func_8008EB4C(0x1E);
            D_80092604--;
        }
        if (D_800594A4 & 0x4000) {
            func_8008EB4C(0x1E);
            D_80092604++;
        }
        if (D_80092604 >= 8) {
            D_80092604 = 0;
        }
        if (D_80092604 < 0) {
            D_80092604 = 7;
        }
        func_80034800(&D_80092954, (D_800928E8 * 7) & 0x3F, 0xC0, 0x10);
        func_80034874(&D_80092954, D_80092604);
        if (D_8005948C & 0x20) {
            func_8008EB4C(0x21);
            func_8007191C(D_80092604 + 1);
            func_800346A4(message);
        }
    }
    if (*D_800925F8 == 0) {
        func_8007191C(0);
    }
    func_8007107C();
    D_800925E0 += func_800707D8(D_800925E8, D_800925E0, 4);
    D_800925E4 += func_800707D8(D_800925EC, D_800925E4, 4);
    if (D_80092608 != 0) {
        func_80034888(&D_80092954, D_80092938, D_800928A0);
    }
    if (D_800925D4 != D_800925D8) {
        message->unk68 = 3;
        func_800346A4(message);
        func_80034714(message, func_80033728(D_80092880, D_800925D4));
        D_800925D8 = D_800925D4;
    }
    func_80079DF0(&D_8009872C, &D_80097010);
    func_8007099C(D_80092904);
}

#ifdef NON_MATCHING
/* Settle an actor on the floor: while a probe 0xC0 away in one of eight
 * directions finds the floor more than 0x40 higher, step away from it (at
 * most 20 times); then record the floor height and its attribute bits.
 * Does not match: the original strength-reduces the step table walk into
 * two pointers (x from a register base, z from the symbol + 4). */
void func_80071DA4(Actor *actor) {
    Vector *pos = &actor->pos;
    s32 tries = 0;
    s32 best;
    s32 highest;
    s32 dir;
    s32 floor;
    Vector probe;

    do {
        pos->vy = highest = func_80082488(pos, 1);
        for (dir = 0; dir < 8; dir++) {
            probe = *pos;
            probe.vx += D_80091084[dir].x * 0xC0;
            probe.vz += D_80091084[dir].z * 0xC0;
            floor = func_80082488(&probe, 1);
            if (floor < highest - 0x40) {
                best = dir;
                highest = floor;
            }
        }
        if (highest >= pos->vy - 0x40) {
            pos->vy = func_80082488(pos, 1);
            break;
        }
        pos->vx -= D_80091084[best].x * 0xC0;
        pos->vz -= D_80091084[best].z * 0xC0;
        tries++;
    } while (tries < 20);
    actor->floor_y = func_80082488(&actor->pos, 1);
    actor->flags = (actor->flags & 0x9FFFFFFF) | (((func_800828C4(actor) >> 24) & 3) << 29);
}

#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80071DA4);
#endif

/* Scene script callback: 0 plays the stored sound, 1/2 act on one actor
 * (1 also picks the message for whichever actor has more HP left), 3 sets
 * the look-at height, capped at -0x600. */
s32 func_80071F8C(s32 command) {
    switch (command) {
    case 0:
        func_80039C4C(D_80092948);
        func_80039FF8();
        break;
    case 1:
        func_80071DA4(&D_8009872C);
        if ((D_8009872C.hp << 8) / D_8009872C.max_hp > (D_80097010.hp << 8) / D_80097010.max_hp) {
            D_800925D4 = 0x43;
        } else {
            D_800925D4 = 0x44;
        }
        break;
    case 2:
        func_80071DA4(&D_80097010);
        break;
    case 3:
        D_8009871C.vy = -D_8009284C;
        if (D_8009871C.vy < -0x600) {
            D_8009871C.vy = -0x600;
        }
        break;
    }
}

/* Allow the next menu scene setup. */
void func_800720C4(void) {
    D_8009293C = 0;
}

/* One-time scene setup: start the scene script and clear the actors'
 * counters and the message state. */
void func_800720D4(void) {
    if (D_8009293C == 0) {
        func_80070F80(D_800910C4);
        D_8009293C = 1;
        D_800928D4 = 0;
        D_80099D9D = 0;
        D_80099D98.driven = 0;
        D_80092900 = 0;
        D_800925D4 = 0;
        D_800925D8 = 0;
        D_80092A00 = 1;
        D_80092A10 = 1;
        D_80092A20 = 1;
        D_80097010.unkE8 = 0;
        D_8009872C.unkE8 = 0;
    }
}
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80072170);

void func_800725A8(void) {
}

/* Set up the scene around an actor: graphics state, lights, its model
 * copy, the scene origin and the values from its move header. */
void func_800725B0(Actor *scene) {
    SceneHeader *header;

    func_80030988(5, 4, 0x40, 0x40);
    D_800910F0 = func_8008A3E0((Holder *)func_8008A2B8(0x10));
    D_80092610 = func_8008C2C0(scene->node);
    func_8008976C(0x280, 0xDA);
    func_8004A14C(0x400);
    D_800928D0 = 0;
    D_80092614 = scene;
    func_80078F00(scene);
    D_80096FA8.vz = 0;
    D_80096FA8.vy = 0;
    D_80096FA8.vx = 0;
    header = scene->unk8FC;
    D_80092618 = 1;
    D_8009261C = header->unk14;
    D_80092620 = header->unk16;
    D_80092624 = header->unk18;
    D_80092628 = header->unk1A;
    D_8009262C = header->unk1C;
    D_80092632 = header->unk1E;
}

/* Tear down the scene set up by func_800725B0. */
void func_800726B4(void) {
    func_8008BC04();
    func_8007F834();
    func_80030988(1, 1, 0x40, 0x40);
    func_8008A5BC(D_800910F0);
    func_80089D5C(D_80092610);
    func_8008976C(0x140, 0xDA);
    func_8004A14C(0xC0);
    func_80083C0C(3);
    func_8007E954(0x100);
    func_80080D10();
}

/* Copy a model's matrix to out, rotated by the base matrix, with its
 * translation set to the model position relative to the scene origin. */
void func_8007273C(Node *model, Matrix *matrix, Matrix *out) {
    Matrix local;

    *out = *matrix;
    local = D_80091C0C;
    local.t[0] = model->position.vx - D_80096FA8.vx;
    local.t[1] = -D_80096FA8.vy;
    local.t[2] = model->position.vz - D_80096FA8.vz;
    func_8004931C(matrix, &local, &local);
    out->t[0] = local.t[0];
    out->t[1] = local.t[1];
    out->t[2] = local.t[2];
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80072858);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80072D18);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073064);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800730AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800730F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007313C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800731F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800732AC);

/* Create the menu's glow emitter: 96 bluish tile sparks. */
void func_800732CC(void) {
    Emitter *emitter = func_8008D3F4(3, 0);

    emitter->r = 0x80;
    emitter->g = 0x80;
    emitter->b = 0xC0;
    func_8008D5C0(emitter, 0x60);
    emitter->gravity = 4;
    emitter->spread = 0x300;
    emitter->speed = 8;
    emitter->speed_range = 0x20;
    emitter->unk68 = 0;
    emitter->life = 0x20;
    D_80092644 = emitter;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007334C);

/* Fire a projectile of the given kind from a point toward the actor's
 * target (or away from origin when given), in the first free slot. */
void func_80073424(Vector *from, Vector *origin, Actor *actor, s32 kind, s32 arg4, s32 arg5) {
    Vector toward;
    Vector aim;
    SVector unused; /* the original frame reserves 8 more bytes */
    Actor *opponent;
    ShotKind *info;
    Shot *shot;
    s32 i;

    for (i = 0; i < 9; i++) {
        if (actor->shots[i].active == 0) {
            shot = &actor->shots[i];
            break;
        }
    }
    if (i == 8) {
        return;
    }
    opponent = actor->opponent;
    aim.vx = opponent->pos.vx - from->vx;
    aim.vy = opponent->pos.vy - from->vy - 0x90;
    aim.vz = opponent->pos.vz - from->vz;
    info = &D_800910F4[kind];
    if (origin != NULL) {
        toward.vx = from->vx - origin->vx;
        toward.vy = from->vy - origin->vy;
        toward.vz = from->vz - origin->vz;
    } else {
        toward = aim;
    }
    func_8008859C(&toward, &shot->dir);
    shot->homing = info->unk0;
    shot->speed = info->speed;
    shot->life = info->unk3;
    shot->look = info->unk2;
    shot->steer = info->unk5;
    shot->unk38 = arg5;
    shot->unk3C = arg4;
    if (info->sound != 0) {
        func_8008EBD0(actor, info->sound, shot, 2);
    }
    func_80073064(&shot->dir, &shot->velocity, shot->speed);
    shot->pos = *from;
    shot->prev = shot->pos;
    shot->active = 1;
}

/* Move an actor's shots: expire, hit the floor, home in on the opponent's
 * core, draw the trail for their look and update speed and homing. */
s32 func_80073644(Actor *actor) {
    SVector half;
    Vector toward;
    SVector dir;
    Color colour;
    Actor *opponent;
    Shot *shot;
    s32 dist;
    s32 i;

    opponent = actor->opponent;
    actor->nearest_dist = 0x10000;
    func_8007C100(&actor->colour);
    for (i = 0; i < 8; i++) {
        shot = &actor->shots[i];
        if (shot->active == 0) {
            continue;
        }
        if (--shot->life == -1) {
            shot->active = 0;
            continue;
        }
        if (func_80082488(&shot->pos, 0) < shot->pos.vy) {
            shot->active = 0;
            func_8007D190(&shot->pos, 1);
            continue;
        }
        shot->prev = shot->pos;
        toward.vx = opponent->core.vx - shot->pos.vx;
        toward.vy = opponent->core.vy - shot->pos.vy;
        toward.vz = opponent->core.vz - shot->pos.vz;
        dist = func_800886FC(&toward);
        shot->dist = dist;
        if (dist < actor->nearest_dist) {
            actor->nearest_dist = dist;
            actor->nearest_shot = shot;
        }
        func_80048D68(&toward, &dir);
        func_8004901C(&dir, &shot->dir, shot->homing, 0x1000 - shot->homing, &shot->dir);
        func_80073064(&shot->dir, &shot->velocity, shot->speed);
        switch (shot->look) {
        case 0:
            half.vx = shot->velocity.vx;
            half.vy = shot->velocity.vy;
            half.vz = shot->velocity.vz;
            half.vx /= 2;
            half.vy /= 2;
            half.vz /= 2;
            shot->pos.vx += half.vx;
            shot->pos.vy += half.vy;
            shot->pos.vz += half.vz;
            func_8007D190(&shot->pos, 0xA);
            shot->pos.vx += half.vx;
            shot->pos.vy += half.vy;
            shot->pos.vz += half.vz;
            func_8007D190(&shot->pos, 0xA);
            break;
        case 1:
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007D190(&shot->pos, 2);
            break;
        case 2:
            colour.r = 0xFF;
            colour.b = 0x40;
            colour.g = ((D_800928E8 + i) << 6) - 1;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, &colour);
            break;
        case 3:
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007C880((actor->flags >> 27) & 1, &shot->pos, shot->unk38, 2);
            break;
        case 4:
            colour.r = colour.g = func_8003FA38() % 191 + 0x40;
            colour.b = 0xFF;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, &colour);
            break;
        case 5:
            colour.r = colour.g = func_8003FA38() % 191 + 0x40;
            colour.b = 0xFF;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, &colour);
            func_8007C880((actor->flags >> 27) & 1, &shot->pos, shot->unk38, 2);
            break;
        }
        switch (shot->steer) {
        case 0:
            break;
        case 1:
            if (shot->speed < 0x70) {
                shot->speed += 0x10;
            }
            break;
        case 2:
            shot->homing = 0x600 - shot->life * 0x30;
            break;
        case 3:
            shot->homing = 0;
            break;
        }
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073B7C);

/* Age an actor's trail segments: new ones start fading, fading ones are freed. */
void func_80073CA4(Actor *actor) {
    s32 i;
    Trail *trail;

    for (i = 0; i < 16; i++) {
        trail = &actor->trails[i];
        if (trail->state == 2) {
            trail->state = 1;
        } else {
            trail->state = 0;
        }
    }
}

/* Record a new trail segment between two points. */
void func_80073CEC(Vector *a, Vector *b, s32 flip, HitSpec *hit, Trail *trail, s32 arg5, Actor *owner) {
    trail->a_prev = trail->a;
    trail->b_prev = trail->b;
    trail->a = *a;
    trail->b = *b;
    trail->unk43 = arg5;
    trail->flip = flip & 1;
    trail->state = 2;
    trail->unk47 = owner->unk644;
    trail->unk50 = owner->unk84;
    trail->unk46 = hit->type;
    D_80092650++;
}

#ifdef NON_MATCHING
/* Whether an actor can take amount more: always below 0x1000 total,
 * otherwise only while the excess / 20 is below its HP. Does not match:
 * the loaded field lands in v0 instead of v1. */
s32 func_80073DE4(Actor *actor, s32 amount) {
    amount += actor->unkB6;
    if (amount > 0x1000) {
        return (amount - 0xFF1) / 20 < actor->hp;
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073DE4);
#endif

/* Add charge to an actor. Past full charge the excess / 20 is spent from
 * its HP-bound reserve (and counted by kind); if it cannot be, the charge
 * is refused with a buzzer. Always accepted in modes 4 and 6. */
s32 func_80073E2C(Actor *actor, s32 amount, s32 kind) {
    s32 excess;

    if (D_800928C8 == 4) {
        return 1;
    }
    if (D_800928C8 == 6) {
        return 1;
    }
    actor->unkB6 += amount;
    if (actor->unkB6 > 0x1000) {
        excess = (actor->unkB6 - 0x1000) / 20;
        if (excess >= actor->hp) {
            actor->unkB6 -= amount;
            actor->unkBA = 0;
            if (func_80083CD8() != 4) {
                func_8008EB4C(0x2E);
            }
            return 0;
        }
        actor->unkB6 = 0x1000;
        actor->unkBA = excess;
        switch (kind) {
        case 1:
            actor->unk1654 += actor->unkBA;
            break;
        case 2:
            actor->unk1658 += actor->unkBA;
            break;
        default:
            return 1;
        }
    }
    return 1;
}

/* Spawn the effect of a hit: between two model points for line types,
 * otherwise at the midpoint of the given points. */
void func_80073F34(Actor *actor, HitSpec *hit) {
    Vector a;
    Vector b;
    s32 style;

    style = func_8007CD14((actor->flags >> 27) & 1, hit->part_a, hit->vertex_a, 1);
    func_80073B7C(actor, hit->part_a, hit->vertex_a, &a);
    if (func_8007D25C(hit->type) != 0) {
        func_80073B7C(actor, hit->part_b, hit->vertex_b, &b);
        if (hit->type == 0x10) {
            func_8007CD44((actor->flags >> 27) & 1, &a, &b, style);
        } else {
            func_8007D65C(&a, &b, hit->type);
        }
        return;
    }
    if (hit->part_a != hit->part_b || hit->vertex_a != hit->vertex_b) {
        func_80073B7C(actor, hit->part_b, hit->vertex_b, &b);
        a.vx = (a.vx + b.vx) / 2;
        a.vy = (a.vy + b.vy) / 2;
        a.vz = (a.vz + b.vz) / 2;
    }
    func_8007C100(&actor->colour);
    if (hit->type >= 0x20) {
        func_8007C880((actor->flags >> 27) & 1, &a, style, hit->type - 0x20);
    } else {
        func_8007D190(&a, hit->type);
    }
}

/* Resolve a hit on an actor's model: impact effects at the hit points and,
 * when it lands, a charged shot, a projectile or a trail segment. */
void func_800740E4(Actor *actor, HitSpec *hit, s32 lands) {
    Vector a;
    Vector b;
    Vector unused; /* the original frame reserves 16 more bytes */
    s32 style;
    s32 power;
    s32 single;
    s32 found;
    s32 i;
    Trail *trail;

    style = func_8007CD14(ACTOR_SIDE(actor), hit->part_a, hit->vertex_a, 0);
    if (hit->part_a == hit->part_b && hit->vertex_a == hit->vertex_b) {
        func_80073B7C(actor, hit->part_a, hit->vertex_a, &a);
        if (actor->unk84[2] != 0 && !(hit->type & 0x40)) {
            func_8007C880(ACTOR_SIDE(actor), &a, style, 0);
        }
        b = a;
        single = 1;
    } else {
        func_80073B7C(actor, hit->part_a, hit->vertex_a, &a);
        func_80073B7C(actor, hit->part_b, hit->vertex_b, &b);
        if (actor->unk84[2] != 0 && !(hit->type & 0x40)) {
            func_8007CD44(ACTOR_SIDE(actor), &a, &b, style);
        }
        single = 0;
    }
    if (!lands) {
        return;
    }
    if (hit->type == 0x20) {
        if ((func_80073E2C(actor, actor->unkBE, 1) && func_80083CD8() != 4)
            || (func_80083CD8() == 4 && (actor->move->flags & 0x8000))) {
            if (!single) {
                a.vx = (a.vx + b.vx) / 2;
                a.vy = (a.vy + b.vy) / 2;
                a.vz = (a.vz + b.vz) / 2;
            }
            func_80073424(&a, NULL, actor, 0, actor->moves->unk18, style);
            func_80076424(actor);
            actor->pose->flags |= 0x8000;
        } else {
            func_8007D190(&a, 9);
            func_80076424(actor);
            actor->pose->flags &= 0x7FFF;
        }
        D_80096FB8[ACTOR_SIDE(actor)].unkC = D_80096FB8[ACTOR_SIDE(actor)].unk10 = actor->unk99E;
        D_80096FB8[ACTOR_SIDE(actor)].unk0 = D_80096FB8[ACTOR_SIDE(actor)].unk8 = D_8009112C;
        D_80096FB8[ACTOR_SIDE(actor)].unk4 = actor->moves->unk18;
        return;
    }
    if (D_80096FB8[ACTOR_SIDE(actor)].unk8 != D_80096FB8[ACTOR_SIDE(actor)].unk0) {
        D_80096FB8[ACTOR_SIDE(actor)].unkC = actor->unk99E;
        D_80096FB8[ACTOR_SIDE(actor)].unk8 = D_80096FB8[ACTOR_SIDE(actor)].unk0;
    }
    D_80096FB8[ACTOR_SIDE(actor)].unk10 = actor->unk99E;
    power = actor->unk644;
    switch (hit->type) {
    case 4:
        func_80073424(&a, NULL, actor, 1, power, style);
        return;
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
        if (single) {
            func_80073424(&a, NULL, actor, hit->type - 0x20, power, style);
        } else {
            func_80073424(&a, &b, actor, hit->type - 0x20, power, style);
        }
        return;
    }
    found = 0;
    for (i = 0; i < 16; i++) {
        trail = &actor->trails[i];
        if (trail->state == 1 && trail->style == style && trail->frame != D_800928E8) {
            func_80073CEC(&a, &b, single, hit, trail, hit->part_a, actor);
            trail->unk44_0 = 0;
            trail->frame = D_800928E8;
            return;
        }
    }
    for (i = 0; i < 16 && !found; i++) {
        trail = &actor->trails[i];
        if (trail->state == 0) {
            func_80073CEC(&a, &b, single, hit, trail, hit->part_a, actor);
            trail->style = style;
            trail->unk44_0 = 1;
            trail->frame = D_800928E8;
            found = 1;
            break;
        }
    }
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074678);

#ifdef NON_MATCHING
/* Show the model objects of the current move: unhide every kind-1 object,
 * then hide the listed ones (and object 13 in mode 0xD). Does not match:
 * the original leaf keeps an empty 16-byte frame (likely from a call that
 * was optimised away). */
void func_80074998(Actor *actor) {
    Node **nodes = ((ModelSet *)actor->node->data)->nodes;
    s32 i;

    for (i = 0; i < ((ModelSet *)actor->node->data)->nodeCount; i++) {
        if (nodes[i]->type == 1) {
            ((Model *)nodes[i]->data)->flags &= ~1;
        }
    }
    for (i = 0; i < actor->visible_count; i++) {
        ((Model *)nodes[actor->visible[i]]->data)->flags |= 1;
    }
    if (actor->kind == 0xD) {
        ((Model *)nodes[13]->data)->flags |= 1;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074998);
#endif

/* Apply an actor's pose and start its move's animation. */
void func_80074AB4(Actor *actor) {
    Pose *pose = actor->pose;
    Move *move = actor->move;
    Player *anim;

    actor->pos.vx = pose->x;
    actor->pos.vy = pose->y;
    actor->pos.vz = pose->z;
    actor->anim = move->anim;
    actor->angle = (pose->flags << 20) >> 20;
    anim = &((ModelSet *)actor->node->data)->players[move->anim];
    if (move->flags & 0x1000) {
        func_80074998(actor);
        func_8008B0D8(anim);
    }
    func_80074678(actor, anim->frame, actor->move->unk9);
    if (move->unkA != 0) {
        func_8008B730(anim, move->unk9, move->unkA);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074BA4);

/* Record the outcome of a bout from the player's side: how it was lost,
 * or which limit the win stayed within and how the opponent ended. */
void func_80075060(s32 lost) {
    Actor *player = &D_8009872C;
    s32 limit;

    if (lost) {
        if (func_8008F4F4(player, 0xE0)) {
            D_80050622 = 2;
        } else if (func_8008F4F4(player, 0x10)) {
            D_80050622 = 1;
        } else {
            D_80050622 = 3;
        }
    } else if (D_80099D8C == 0 && D_80099D88 == 0) {
        D_80050622 = 0x88;
    } else if (D_80099D88 == 0) {
        D_80050622 = 0x82;
    } else {
        limit = player->max_hp * 0xB0 / 255;
        if (limit < player->unk1654) {
            D_80050622 = 0x83;
        } else if (player->max_hp * 0xA0 / 255 < player->unk1658) {
            D_80050622 = 0x84;
        } else if (limit < player->unk1658 + player->unk1654) {
            D_80050622 = 0x85;
        } else {
            player = player->opponent;
            if (func_8008F4F4(player, 0xE0)) {
                D_80050622 = 0x86;
            } else if (func_8008F4F4(player, 0x10)) {
                D_80050622 = 0x81;
            } else {
                D_80050622 = 0x87;
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800751C8);

/* The other actor's value 15EC scaled by amount / 32, less this actor's
 * value 15EE. */
s32 func_8007570C(Actor *actor, Actor *other, s32 amount) {
    return other->unk15EC * amount / 32 - actor->unk15EE;
}

/* The other actor's value 15EA less this actor's value 15E8. */
s32 func_80075738(Actor *actor, Actor *other) {
    return other->unk15EA - actor->unk15E8;
}

void func_80075748(void) {
}

/* Whether point (px, pz) lies within radius of the segment from (x0, z0)
 * to (x1, z1), on its forward side. */
s32 func_80075750(s32 x0, s32 z0, s32 x1, s32 z1, s32 px, s32 pz, s32 radius) {
    Vector d;
    Vector sq;
    s32 rx;
    s32 rz;
    s32 len;
    s32 ux;
    s32 uz;
    s32 along;
    s32 across;
    s32 hit;

    d.vx = x1 - x0;
    d.vy = z1 - z0;
    rx = px - x0;
    rz = pz - z0;
    d.vz = radius;
    func_8004A414(&d, &sq);
    len = func_80048C4C(sq.vx + sq.vy);
    if (len == 0) {
        return 0;
    }
    ux = (d.vx << 12) / len;
    uz = (d.vy << 12) / len;
    along = (rx * ux + rz * uz) / 4096;
    across = (-(rx * uz) + rz * ux) / 4096;
    hit = (u32)(across * across - sq.vz) >> 31;
    if (along < 0) {
        return 0;
    }
    return along <= len + radius ? hit : 0;
}

#ifdef NON_MATCHING
/* Like func_80075750, and on a hit store where the segment enters the
 * circle around the point in D_80092654/D_80092658. Does not match: the
 * segment length and the products are allocated to other registers. */
s32 func_80075888(s32 x0, s32 z0, s32 x1, s32 z1, s32 px, s32 pz, s32 radius) {
    Vector d;
    Vector sq;
    s32 rx;
    s32 rz;
    s32 len;
    s32 ux;
    s32 uz;
    s32 along;
    s32 across;
    s32 diff;
    s32 hit;

    d.vx = x1 - x0;
    d.vy = z1 - z0;
    rx = px - x0;
    rz = pz - z0;
    d.vz = radius;
    func_8004A414(&d, &sq);
    len = func_80048C4C(sq.vx + sq.vy);
    if (len == 0) {
        return 0;
    }
    ux = (d.vx << 12) / len;
    uz = (d.vy << 12) / len;
    along = (rx * ux + rz * uz) / 4096;
    across = (-(rx * uz) + rz * ux) / 4096;
    diff = across * across - sq.vz;
    hit = (u32)diff >> 31;
    if (along < 0) {
        hit = 0;
    }
    if (along > len + radius) {
        hit = 0;
    }
    if (hit) {
        if (diff < 0) {
            diff = -diff;
        }
        along -= func_80048C4C(diff);
        D_80092654 = along * ux / 4096 + x0;
        D_80092658 = along * uz / 4096 + z0;
    }
    return hit;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075888);
#endif

/* Whether a point comes within radius of any edge of a quad given as four
 * corner vectors; clears the crossing point to the point first. */
s32 func_80075A4C(Vector *quad, s32 px, s32 pz, s32 radius) {
    D_80092654 = px;
    D_80092658 = pz;
    if (func_80075750(quad[1].vx, quad[1].vz, quad[0].vx, quad[0].vz, px, pz, radius)
        || func_80075750(quad[3].vx, quad[3].vz, quad[2].vx, quad[2].vz, px, pz, radius)
        || func_80075750(quad[0].vx, quad[0].vz, quad[2].vx, quad[2].vz, px, pz, radius)) {
        return 1;
    }
    return func_80075750(quad[1].vx, quad[1].vz, quad[3].vx, quad[3].vz, px, pz, radius) != 0;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075B50);

/* Queue a pad input for an actor (dropped when 32 are pending). */
void func_8007639C(Actor *actor, u8 input) {
    if (actor->input_count < 32) {
        actor->inputs[actor->input_head++ & 0x1F] = input;
        actor->input_count++;
    }
}

/* Take the oldest queued input of an actor, 0 when none. */
u8 func_800763E4(Actor *actor) {
    u8 input;

    if (actor->input_count == 0) {
        return 0;
    }
    input = actor->inputs[actor->input_tail++ & 0x1F];
    actor->input_count--;
    return input;
}

/* Empty an actor's input queue. */
void func_80076424(Actor *actor) {
    actor->input_count = 0;
    actor->input_head = 0;
    actor->input_tail = 0;
    actor->unk9C3 = 0;
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC3C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC48);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC54);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC58);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC64);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC6C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC74);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC78);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC8C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC94);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FCA8);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FCB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076438);

/* Start an actor's turn: clear its per-turn state and flags, set its
 * gauge, place its gauge at its side and record the pending combo. */
void func_800764CC(Actor *actor) {
    s32 value;
    u32 flags;

    actor->unk40 = 0;
    actor->unk100 = 0;
    if (func_80083CD8() != 7) {
        actor->flags &= ~0x8000;
        actor->flags &= ~2;
    }
    value = actor->unk15F4 * D_8009292C;
    flags = actor->flags & ~0x80000;
    actor->flags = flags;
    actor->unk15F2 = value >> 8;
    func_8007E894(!(flags & 0x8000000) ? 0x28 : 0xF0, 0x28);
    if (actor->unk9C3 != 0) {
        D_80096FB8[ACTOR_SIDE(actor)].unk0 = D_80091198[actor->unk9C3];
        D_80096FB8[ACTOR_SIDE(actor)].unk4 = actor->moves->learned[actor->unk9C3 - 1] * actor->moves->base / 100;
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007661C);

/* Queue input 5 for an actor when both actors are in the 0x40000 state
 * near the ground, unless it is blocked (bit 24, move 4, or the 0x20000000
 * stance without permission bit 1). */
s32 func_800767C8(Actor *actor) {
    if (D_8009284C > 0x200 || actor->unkC5 == 4) {
        return;
    }
    if (actor->flags & 0x1000000) {
        return;
    }
    if ((actor->flags & 0x40000) && (actor->opponent->flags & 0x40000)
        && ((actor->flags & 0x60000000) != 0x20000000 || (actor->unk90A & 2))) {
        func_80076424(actor);
        func_8007639C(actor, 5);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077038);

/* Push an actor along an angle (scaled down by shift) and let it rise by
 * lift, never faster than 0x82 upward. */
s32 func_80077584(Actor *actor, s32 angle, s32 shift, s32 lift) {
    actor->push.vx += func_8003F8B0(angle) >> shift;
    actor->push.vz += func_8003F8CC(angle) >> shift;
    if (actor->velocity.vy >= -0x8B) {
        actor->velocity.vy -= lift;
    }
    if (actor->velocity.vy < -0x82) {
        actor->velocity.vy = -0x82;
    }
}

/* Put an actor into its jump: animation, timers and flags, then the
 * initial push. */
void func_8007762C(Actor *actor, s32 angle, s32 shift, s32 lift) {
    actor->anim = 8;
    actor->unk4E = 0xFF;
    actor->unkCA = 0x3C;
    actor->unk916 = 0x28;
    actor->unkE8 = 0x28;
    actor->unkC4 = 4;
    actor->unkC5 = 0;
    actor->flags |= 0x1000;
    actor->unkD4 &= ~0x10;
    actor->flags &= ~0x400;
    func_80077584(actor, angle, shift, lift);
}

/* Start pad vibration for an actor's side when enabled for it (the right
 * side only in modes 2 and 4) and it is not suppressed. */
s32 func_800776A8(Actor *actor, s32 arg) {
    if (func_80083CD8() == 4) {
        return;
    }
    if (actor->flags & 0x8000000) {
        if ((D_800928C8 == 2 || D_800928C8 == 4) && (D_80099D9C & 1) && !(actor->flags & 0x40)) {
            func_80036258(1, arg);
        }
    } else if ((D_80099D9B & 1) && !(actor->flags & 0x40)) {
        func_80036258(0, arg);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077770);

/* Advance an actor's combo with a button and return the new combo's
 * entry. */
MoveSlot *func_80077A38(Actor *actor, s32 button) {
    s32 i = actor->unk9C3 * 2;

    if (button != 0) {
        actor->unk9C3 = D_80091178[i];
    } else {
        actor->unk9C3 = D_80091178[i + 1];
    }
    return &actor->move_slots[actor->unk9C3];
}

/* Set an actor's 0x80000 flag. */
void func_80077A88(Actor *actor) {
    actor->flags |= 0x80000;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A9C);

/* Put an actor back at its home position, idle. */
void func_80078154(Actor *actor) {
    actor->pos = actor->home;
    actor->anim = 0;
    actor->unkC4 = 0;
    actor->unkC5 = 0;
    actor->flags |= 0x2000000;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078194);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078704);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078920);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078D20);

/* Place an actor's model at the actor's position and facing. */
void func_80078E94(Actor *actor) {
    actor->node->position.vx = actor->pos.vx;
    actor->node->position.vy = actor->pos.vy;
    actor->node->position.vz = actor->pos.vz;
    actor->node->unk44.vy = actor->angle;
}

/* Default values of a seven-entry parameter block. */
void func_80078ED4(s16 *params) {
    params[0] = 0x100;
    params[2] = 0x10;
    params[1] = 0;
    params[3] = 0;
    params[4] = 0;
    params[5] = 0x30;
    params[6] = 0x30;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078F00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007920C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800796B8);

/* Reset the bout: effects, glow and the round settings. */
void func_80079A8C(void) {
    func_800732CC();
    func_8008DC28();
    func_80088AF8();
    D_80099D9D = 1;
    D_80099DA1 = 3;
    D_8009292C = 0x100;
    D_80099D98.driven = 0;
    D_80099D9A = 0;
    D_80099DA2 = 0;
    D_80099DA4 = 0x100;
}

void func_80079B04(void) {
}

/* Clear the per-round counters. */
void func_80079B0C(void) {
    D_80092950 = 0;
    D_8009872C.unkF2 = 0;
    D_80097010.unkF2 = 0;
    D_80092918 = 0;
    D_80092944 = 0;
    D_800928FC = 0;
}

/* Start a round: face both actors in, reset effects, counters and the
 * message window; every fifth round from the third picks a special stage
 * when enabled. */
s32 func_80079B44(void) {
    D_8009872C.unkCC = 0x400;
    D_80097010.unkCC = -0x400;
    func_80078F00(&D_8009872C);
    func_80078F00(&D_80097010);
    func_8008D580(D_80092644);
    func_8007E24C();
    func_8007BB7C();
    func_800831C8();
    D_800928F4 = 1;
    D_8009290C = -1;
    func_8008DCA8(0);
    D_80092638 = 0;
    D_80092640 = 0;
    D_80092890 = 0;
    D_8009263C = 0x5A;
    D_8009294C = 0;
    D_80092648 = 0;
    func_8007F834();
    D_80050622 = 0;
    func_8008E620();
    func_800720C4();
    D_800928D4 = 0;
    D_800928F0 = 0;
    D_80091144 = 0;
    D_80091145 = 0;
    D_80092664 = 0;
    D_80092950++;
    func_800346A4(&D_8009868C);
    D_80099D9A = 0;
    if (D_8005061C != 0) {
        switch ((D_80092950 - 1) % 5) {
        case 3:
            D_800928B4 = 1;
            break;
        case 4:
            D_800928B4 = 2;
            break;
        default:
            D_800928B4 = 0;
            break;
        }
    }
}

/* Mirror actor flags 2, 15 and 19 into its pose. */
void func_80079D08(Actor *actor) {
    actor->pose->unkA = (actor->pose->unkA & ~0x100) | ((actor->flags << 6) & 0x100);
    actor->pose->flags = (actor->pose->flags & ~0x2000) | ((actor->flags >> 2) & 0x2000);
    actor->pose->flags = (actor->pose->flags & ~0x4000) | ((actor->flags >> 5) & 0x4000);
}

#ifdef NON_MATCHING
/* Restore actor flags 2, 15 and 19 from its pose. Does not match: the
 * original extracts pose bit 14 as a signed bit-field (sll 17; slti). */
void func_80079D6C(Actor *actor) {
    actor->flags = (actor->flags & ~4) | ((actor->pose->unkA >> 6) & 4);
    actor->flags = (actor->flags & ~0x8000) | ((actor->pose->flags << 2) & 0x8000);
    actor->flags = (actor->flags & ~0x80000) | (((actor->pose->flags >> 14) & 1) << 19);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079D6C);
#endif

/* Clear D_80092640. */
void func_80079DE0(void) {
    D_80092640 = 0;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079DF0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A21C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A344);

/* Put an actor into its round-end pose: a win pose when the round took
 * under two seconds. */
void func_8007A6D0(Actor *actor) {
    D_8009292C = 0x100;
    if (D_800928AC < 0x78) {
        actor->unk4F = 0x10;
        actor->unk52 = 0;
        actor->anim = 0x10;
        actor->flags |= 0x400;
    } else {
        actor->unk4F = 0x10;
        actor->unk52 = 0;
        actor->anim = 0;
        actor->flags |= 0x400;
    }
}

/* Put an actor into its knocked-down pose. */
void func_8007A730(Actor *actor) {
    D_8009292C = 0x100;
    actor->unk4F = 0x10;
    actor->unk52 = 0;
    actor->anim = 9;
    actor->flags |= 0x2000400;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A768);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A958);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007AC3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007AE10);

/* Set up a scene model with the given mode and place it. */
void func_8007B210(Actor *model, s32 mode) {
    model->pose = (Pose *)model->unk9CC;
    model->unk4F = 0x10;
    model->anim = mode;
    model->unk52 = 0;
    D_8009292C = 0x100;
    func_80074BA4(model);
    func_80074678(model, model->unk998, model->unk99A);
}

/* Build a four-entry palette from two colours (components biased by 0x80,
 * clamped at zero) and load it, keeping the returned CLUT id. */
void func_8007B270(u8 *first, u8 *second) {
    s32 r, g, b;

    r = first[0] - 0x80;
    g = first[1] - 0x80;
    b = first[2] - 0x80;
    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }
    D_800926A8[0] = (((r >> 2) & 0x1F) + ((g << 3) & 0x3E0) + ((b << 8) & 0x7C00)) | -0x8000;
    r = second[0] - 0x80;
    g = second[1] - 0x80;
    b = second[2] - 0x80;
    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }
    D_800926A8[1] = (((r >> 2) & 0x1F) + ((g << 3) & 0x3E0) + ((b << 8) & 0x7C00)) | -0x8000;
    D_800926A8[2] = 0;
    D_800926A8[3] = 0x1111;
    D_800926A0 = func_800438C0(D_800926A8, D_80092698, D_8009269C);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B388);

/* Advance every live sparkle one frame (expiring it after its last frame,
 * letting it fall otherwise) and latch the per-frame counters. */
void func_8007BACC(void) {
    Sparkle *sparkle;
    s32 i;
    s32 count;

    sparkle = D_80092AD8;
    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (sparkle->active) {
            if (sparkle->frame == sparkle->frame_count) {
                sparkle->active = 0;
            } else {
                sparkle->frame++;
                if (sparkle->type == 0) {
                    sparkle->u.fall.fall_speed += sparkle->u.fall.gravity;
                    sparkle->y += sparkle->u.fall.fall_speed;
                }
            }
        }
    }
    D_800926A4++;
    count = D_800926B0;
    D_800926B0 = 0;
    D_800926B4 = count;
}

/* Free every sparkle. */
void func_8007BB7C(void) {
    Sparkle *sparkle = D_80092AD8;
    s32 i;

    for (i = SPARKLE_COUNT - 1; i >= 0; i--, sparkle++) {
        sparkle->active = 0;
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BBA0);

/* Set the colour of kind-2 sparkles. */
void func_8007C100(Color *color) {
    D_800926B8 = *color;
}

/* Start a sparkle of the given kind at a position, in the first free slot. */
void func_8007C124(SVector *pos, s32 kind) {
    Sparkle *sparkle = D_80092AD8;
    SparkleKind *info;
    PolyFT4 *prim;
    s32 i;

    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active) {
            break;
        }
    }
    if (i == SPARKLE_COUNT) {
        return;
    }
    sparkle->active = 1;
    info = &D_80092A74[kind];
    sparkle->type = 0;
    sparkle->frame = 0;
    sparkle->u.fall.kind = info;
    sparkle->frame_count = info->frame_count;
    sparkle->u.fall.gravity = info->gravity;
    sparkle->u.fall.fall_speed = 0;
    sparkle->x = pos->vx;
    sparkle->y = pos->vy;
    sparkle->z = pos->vz;
    prim = sparkle->prim;
    prim->tpage = info->tpage;
    if (kind == 2) {
        prim->code &= ~1;
        prim->r0 = D_800926B8.r;
        prim->g0 = D_800926B8.g;
        prim->b0 = D_800926B8.b;
    } else {
        prim->code |= 1;
    }
    prim->clut = info->clut;
    sparkle->prim[1] = *prim;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C280);

#ifdef NON_MATCHING
/* Start a trail segment of a key at a position for the current owner
 * (once per key and owner), with the given texture column and size, linked
 * to the segment started on the previous frame.
 * Does not match: the texture and header stores are scheduled in a different order. */
void func_8007C880(s32 column, Vector *pos, s32 key, s32 size) {
    Sparkle *sparkle;
    Sparkle *other;
    PolyFT4 *prim;
    s32 i;

    for (i = 0, sparkle = D_80092AD8; i < SPARKLE_COUNT; i++, sparkle++) {
        if (sparkle->active && sparkle->u.trail.key == key && sparkle->u.trail.owner == D_800928E8) {
            return;
        }
    }
    sparkle = D_80092AD8;
    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active) {
            break;
        }
    }
    if (i == SPARKLE_COUNT) {
        return;
    }
    prim = sparkle->prim;
    prim->u0 = prim->u1 = prim->u2 = prim->u3 = (u8)D_80092698 * 4 + 8 + column * 4;
    prim->v0 = prim->v1 = prim->v2 = prim->v3 = D_8009269C;
    prim->code &= ~1;
    prim->tpage = D_80092694;
    prim->clut = D_800926A0;
    sparkle->prim[1] = *prim;
    sparkle->frame_count = 7;
    sparkle->type = 1;
    sparkle->active = 1;
    sparkle->frame = 0;
    sparkle->x = pos->vx;
    sparkle->y = pos->vy;
    sparkle->u.trail.owner = D_800928E8;
    sparkle->u.trail.key = key;
    sparkle->u.trail.prev = NULL;
    sparkle->u.trail.stamp = D_800926A4;
    sparkle->z = pos->vz;
    sparkle->u.trail.size = D_80091228[size];
    for (i = 0, other = D_80092AD8; i < SPARKLE_COUNT; i++, other++) {
        if (other->active && other->u.trail.key == key && other != sparkle && other->type == 1 &&
            other->u.trail.stamp == (u16)(D_800926A4 - 1)) {
            sparkle->u.trail.prev = other;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C880);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CAA4);

/* Pack four fields into one word: top byte, 16-bit middle, flag bit 7 and
 * a 7-bit low field. */
u32 func_8007CD14(s32 flag, s32 top, s32 middle, s32 low) {
    return (low & 0x7F) | ((flag << 7) & 0x80) | (top << 24) | ((middle << 8) & 0xFFFF00);
}

#ifdef NON_MATCHING
/* Start a line segment of a key between two positions for the current
 * owner (once per key and owner), with the given texture column, linked to
 * the segment started on the previous frame.
 * Does not match: loads are scheduled ahead of the original statement order. */
void func_8007CD44(s32 column, Vector *from, Vector *to, s32 key) {
    Sparkle *sparkle;
    Sparkle *other;
    PolyFT4 *prim;
    s32 i;

    for (i = 0, sparkle = D_80092AD8; i < SPARKLE_COUNT; i++, sparkle++) {
        if (sparkle->active && sparkle->u.line.key == key && sparkle->u.line.owner == D_800928E8) {
            return;
        }
    }
    sparkle = D_80092AD8;
    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active) {
            break;
        }
    }
    if (i == SPARKLE_COUNT) {
        return;
    }
    sparkle->frame_count = 7;
    sparkle->type = 2;
    sparkle->active = 1;
    sparkle->frame = 0;
    sparkle->x = from->vx;
    sparkle->y = from->vy;
    sparkle->u.line.owner = D_800928E8;
    sparkle->z = from->vz;
    sparkle->u.line.x = to->vx;
    sparkle->u.line.y = to->vy;
    prim = sparkle->prim;
    prim->u0 = prim->u1 = prim->u2 = prim->u3 = (u8)D_80092698 * 4 + 8 + column * 4;
    prim->v0 = prim->v1 = prim->v2 = prim->v3 = D_8009269C;
    sparkle->u.line.key = key;
    sparkle->u.line.prev = NULL;
    sparkle->u.line.stamp = D_800926A4;
    sparkle->u.line.z = to->vz;
    prim->tpage = D_80092694;
    prim->clut = D_800926A0;
    prim->code &= ~1;
    sparkle->prim[1] = *prim;
    for (i = 0, other = D_80092AD8; i < SPARKLE_COUNT; i++, other++) {
        if (other->active && other->u.line.key == key && other != sparkle && other->type == 2 &&
            other->u.line.stamp == (u16)(D_800926A4 - 1)) {
            sparkle->u.line.prev = other;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CD44);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CF78);

/* Copy the camera position to the scratchpad and run the scene pass. */
void func_8007D068(void *arg) {
    SCENE_SCRATCH->camera = D_80096FA8;
    func_8007E3CC(arg);
}

/* Jitter a short position by -24..23 on each axis. */
void func_8007D0B4(SVector *pos) {
    pos->vx += func_8003FA38() % 48 - 24;
    pos->vy += func_8003FA38() % 48 - 24;
    pos->vz += func_8003FA38() % 48 - 24;
}

/* Start a sparkle of kind 0..4 at a position; kinds 8..12 are the same
 * sparkles with the position jittered first. Declared int without a return
 * value, as the original's unfilled branch delay slot shows. */
s32 func_8007D190(Vector *pos, u32 kind) {
    SVector at;

    at.vx = pos->vx;
    at.vy = pos->vy;
    at.vz = pos->vz;
    switch (kind) {
    case 8:
        func_8007D0B4(&at);
    case 0:
        func_8007C124(&at, 0);
        break;
    case 9:
        func_8007D0B4(&at);
    case 1:
        func_8007C124(&at, 1);
        break;
    case 10:
        func_8007D0B4(&at);
    case 2:
        func_8007C124(&at, 2);
        break;
    case 11:
        func_8007D0B4(&at);
    case 3:
        func_8007C124(&at, 3);
        break;
    case 12:
        func_8007D0B4(&at);
    case 4:
        func_8007C124(&at, 4);
        break;
    }
}

/* Whether a code lies in 0x10..0x1f. */
s32 func_8007D25C(s32 code) {
    if (code < 0x10) {
        return 0;
    }
    return code < 0x20;
}

/* Jitter a position by -32..31 on each axis. */
void func_8007D274(Vector *from, Vector *to) {
    to->vx = from->vx + func_8003FA38() % 64 - 32;
    to->vy = from->vy + func_8003FA38() % 64 - 32;
    to->vz = from->vz + func_8003FA38() % 64 - 32;
}

#ifdef NON_MATCHING
/* Queue a three-strand bolt of jittered 7-segment lines between two points,
 * coloured by kind: 0 green-blue flicker, 1 random grey-yellow, 2
 * alternating white and red segments.
 * Does not match: the address of prev is kept in a saved register. */
void func_8007D334(Vector *from, Vector *to, s32 kind) {
    Vector point;
    Vector step;
    Vector prev;
    Vector next;
    Color color;
    s32 strand;
    s32 i;
    s32 value;

    step = *to;
    step.vx -= from->vx;
    step.vy -= from->vy;
    step.vz -= from->vz;
    func_800886FC(&step);
    step.vx /= 7;
    step.vy /= 7;
    step.vz /= 7;
    for (strand = 0; strand < 3; strand++) {
        point = *from;
        prev = *from;
        for (i = 0; i < 7; i++) {
            point.vx += step.vx;
            point.vy += step.vy;
            point.vz += step.vz;
            func_8007D274(&point, &next);
            switch (kind) {
            case 0:
                color.r = func_8003FA38() & 0x3F;
                color.g = func_8003FA38() % 191 + 0x40;
                color.b = 0xFF;
                break;
            case 1:
                value = func_8003FA38() % 256 + 0x40;
                if (value < 0x100) {
                    color.g = value;
                } else {
                    color.g = 0xFF;
                }
                color.r = color.g = color.g;
                color.b = value / 3;
                break;
            case 2:
                if (i & 1) {
                    color.b = 0xFF;
                    color.g = 0xFF;
                    color.r = 0xFF;
                } else {
                    color.r = 0xFF;
                    color.b = 0;
                    color.g = 0;
                }
                break;
            }
            func_8007E31C(&prev, i == 6 ? to : &next, &color);
            prev = next;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D334);
#endif

/* Map codes 0x11..0x13 to kinds 0..2 and forward them. */
void func_8007D65C(Vector *from, Vector *to, s32 code) {
    switch (code) {
    case 0x11:
        func_8007D334(from, to, 0);
        break;
    case 0x12:
        func_8007D334(from, to, 1);
        break;
    case 0x13:
        func_8007D334(from, to, 2);
        break;
    }
}

#ifdef NON_MATCHING
/* Allocate the scene cell table and both buffers' point primitives.
 * Does not match: the primitive pointer loads are hoisted over the
 * stores (the original keeps every access in order). */
void func_8007D6B8(void) {
    SceneCell10 *cell;
    s32 i;

    D_800926BC = func_80031BDC(0x9F6, 0);
    D_800926C0 = func_80031BDC(0xBF4, 0);
    D_800926C4 = func_80031BDC(0xBF4, 0);
    cell = D_800926BC;
    for (i = 0; i < 0xFF; i++) {
        D_800926C0[i].len = 2;
        D_800926C0[i].rgbc = 0x6880B0F0;
        D_800926C4[i].len = 2;
        D_800926C4[i].rgbc = 0x6880B0F0;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk0 = cell->unk4 = cell->unk6 = 0;
        cell++;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D6B8);
#endif

#ifdef NON_MATCHING
/* Spawn up to count ground particles in free cells around a position: on
 * the ground below a random point within 32 units, rising for 20 frames.
 * Does not match: the x coordinate is kept pre-shifted for the map index. */
void func_8007D7A8(Vector *pos, s32 count) {
    SceneCell10 *cell = D_800926BC;
    s32 i;
    s32 x;
    s32 z;

    for (i = 0; i < 251; i++, cell++) {
        if (count == 0) {
            break;
        }
        if (cell->unk6 == 0) {
            x = pos->vx + (func_8003FA38() % 64 - 32);
            cell->unk0 = x;
            z = pos->vz + (func_8003FA38() % 64 - 32);
            cell->unk4 = z;
            cell->unk2 = cell->unk8 = D_800928DC[((s16)z >> 8) * 128 + ((s16)x >> 8)].height;
            cell->unk7 = -(func_8003FA38() % 10 + 10);
            count--;
            cell->unk6 = 20;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D7A8);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D918);

/* Allocate the 540 scene cells and their small tiles (2..4 pixels square,
 * pale blue), with a copy of the tiles for the other draw buffer. */
void func_8007DB28(void) {
    SceneCell12 *cell;
    TileWords *tile;
    s32 i;

    D_800926C8 = func_80031BDC(0x1950, 0);
    tile = func_80031BDC(0x21C0, 0);
    D_800926CC = tile;
    D_800926D0 = func_80031BDC(0x21C0, 0);
    cell = D_800926C8;
    for (i = 0; i < 540; i++, cell++, tile++) {
        tile->len = 3;
        tile->rgbc = 0x60FFD0A0;
        tile->w = func_8003FA38() % 3 + 2;
        tile->h = func_8003FA38() % 3 + 2;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk8 = cell->unkA = cell->unk9 = cell->unk6 = 0;
    }
    func_800732AC(D_800926D0, D_800926CC, 0x21C0);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007DC74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E020);

/* Clear both scene cell tables and the actors' 0x90b bytes. */
void func_8007E24C(void) {
    SceneCell12 *cell;
    s32 i;

    cell = D_800926C8;
    for (i = 0; i < 0x21C; i++) {
        cell[i].unk6 = 0;
        cell[i].unk4 = 0;
        cell[i].unk2 = 0;
        cell[i].unk0 = 0;
    }
    D_80097010.unk90B = 0;
    D_8009872C.unk90B = 0;
    for (i = 0; i < 0xFF; i++) {
        D_800926BC[i].unk6 = 0;
        D_800926BC[i].unk0 = D_800926BC[i].unk2 = D_800926BC[i].unk4 = 0;
    }
}

/* Initialise the scene's line primitives and clear the per-frame counters. */
void func_8007E2D8(void) {
    SceneLine *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        line = &D_80094818[i];
        line->line.len = 3;
        line->line.code = 0x40;
    }
    D_800926B0 = 0;
    D_800926B4 = 0;
}

/* Queue a coloured 3D line segment for this frame (at most 100). */
void func_8007E31C(Vector *from, Vector *to, Color *color) {
    SceneLine *line;

    if (D_800926B0 < 100) {
        line = &D_80094818[D_800926B0];
        line->from.vx = from->vx;
        line->from.vy = from->vy;
        line->from.vz = from->vz;
        line->to.vx = to->vx;
        line->to.vy = to->vy;
        line->to.vz = to->vz;
        line->line.r0 = color->r;
        line->line.g0 = color->g;
        line->line.b0 = color->b;
        D_800926B0++;
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E3CC);

/* Set the scene state, playing sound 0x24 when state 10 starts from 0. */
void func_8007E528(s32 state) {
    if (D_80092708 == 0 && state == 10) {
        func_8008EB4C(0x24);
    }
    D_80092708 = state;
}

/* While the scene state counts down, draw its sprite (when flag 2 is set). */
void func_8007E574(void *ot) {
    if (D_80092708 != 0) {
        if (D_800928E8 & 2) {
            func_80043B48(ot, &D_800954D8[D_800928A0].sprite);
            func_80043B48(ot, &D_800954D8[D_800928A0].tpage);
        }
        D_80092708--;
    }
}

s32 func_8007E624(void) {
    return D_800926DC;
}

#ifdef NON_MATCHING
/* Allocate the text quads, load the font (with its palette's colours 0, 2
 * and 3 replaced) and the banner image, and build the banner sprite.
 * Does not match: the banner sprite address is taken from its length byte. */
void func_8007E634(MenuFiles *files) {
    TimImage image;
    s32 unused[2]; /* never used; the original frame keeps its slot */
    s16 *palette;
    s32 i;

    D_800926D4[0] = func_80031BDC(0xFA0, 0);
    D_800926D4[1] = func_80031BDC(0xFA0, 0);
    for (i = 0; i < 100; i++) {
        ((u8 *)&D_800926D4[0][i].tag)[3] = 0;
        ((u8 *)&D_800926D4[1][i].tag)[3] = 0;
    }
    func_800471B4(files->font);
    func_800471C4(&image);
    palette = image.caddr;
    palette[2] = -0x6F9D;
    palette[0] = 0;
    palette[3] = -1;
    func_80044894(&image.crect->x, image.caddr);
    func_80044894(&image.prect->x, image.paddr);
    D_800926E4 = func_80043A58(image.crect->x, image.crect->y);
    D_800926E0 = func_80043A1C(0, 1, image.prect->x, image.prect->y);
    D_800926DC = 0;
    func_800471B4(files->banner);
    func_800471C4(&image);
    palette = image.caddr;
    palette[0] = 0;
    func_80044894(&image.crect->x, image.caddr);
    func_80044894(&image.prect->x, image.paddr);
    D_800954D8[0].sprite.len = 4;
    D_800954D8[0].sprite.code = 0x65;
    func_80043E20(&D_800954D8[0].tpage, 0, 0, func_80043A1C(0, 1, image.prect->x, image.prect->y));
    D_800954D8[0].sprite.clut = func_80043A58(image.crect->x, image.crect->y);
    D_800954D8[0].sprite.x0 = 0x40;
    D_800954D8[0].sprite.y0 = 0xBE;
    D_800954D8[0].sprite.w = 0xC4;
    D_800954D8[0].sprite.h = 0xD;
    D_800954D8[0].sprite.u0 = image.prect->x * 4;
    D_800954D8[0].sprite.v0 = image.prect->y;
    D_800954D8[1] = D_800954D8[0];
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E634);
#endif

void func_8007E894(s32 x, s32 y) {
    D_800926E8 = x;
    D_800926EC = y;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E8AC);

void func_8007E954(s32 value) {
    D_800912DC = value;
}

#ifdef NON_MATCHING
/* Draw one character at the text cursor (at most 101 quads a frame; '('
 * only advances) and move the cursor right by its scaled width. Declared
 * int without a return value, as the original's unfilled delay slot shows.
 * Does not match: the original loads the glyph width before storing the
 * first vertex, and the texture page/CLUT before the packet length. */
s32 func_8007E964(s32 ch) {
    PolyFT4Words *quad;
    Glyph *glyph;
    s32 right;

    if (D_800926DC < 101) {
        quad = D_800926D4[D_800928A0];
        quad += D_800926DC;
        glyph = func_8007E8AC(ch);
        if (glyph != NULL) {
            if (ch != '(') {
                quad->xy0 = D_800926E8 | (D_800926EC << 16);
                ch = glyph->width | 3; /* the quad width, in the same variable */
                right = D_800926E8 + ((ch * D_800912DC) >> 8);
                quad->xy1 = right | (D_800926EC << 16);
                quad->xy2 = D_800926E8 | ((D_800926EC + glyph->height) << 16);
                quad->xy3 = right | ((D_800926EC + glyph->height) << 16);
                quad->uv0 = glyph->u | (glyph->v << 8);
                quad->uv1 = (glyph->u + ch) | (glyph->v << 8);
                quad->uv2 = glyph->u | ((glyph->v + (glyph->height + 1)) << 8);
                quad->uv3 = (glyph->u + ch) | ((glyph->v + (glyph->height + 1)) << 8);
                quad->len = 9;
                quad->rgbc = D_800926F0 | (D_800926F4 << 8) | (D_800926F8 << 16) | 0x2C000000;
                quad->tpage = D_800926E0;
                quad->clut = D_800926E4;
                D_800926DC++;
            }
            D_800926E8 += ((glyph->width * D_800912DC) >> 8) + 2;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E964);
#endif

/* Width of a text string in pixels at the current text scale. */
s32 func_8007EB6C(u8 *text) {
    s32 width = 0;

    while (*text != 0) {
        width += ((func_8007E8AC(*text++)->width * D_800912DC) >> 8) + 2;
    }
    return width;
}

/* Draw a line of text at the cursor and move the cursor to the next line. */
void func_8007EBE0(u8 *text) {
    s32 x = D_800926E8;

    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text centred on the cursor, then move to the next line. */
void func_8007EC54(u8 *text) {
    s32 x = D_800926E8;

    D_800926E8 -= func_8007EB6C(text) / 2;
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text ending at the cursor, then move to the next line. */
void func_8007ECF0(u8 *text) {
    s32 x = D_800926E8;

    D_800926E8 -= func_8007EB6C(text);
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text shifted left by an offset, then move to the next line. */
void func_8007ED84(u8 *text, s32 offset) {
    s32 unused[2]; /* never used; the original frame keeps its slot */
    s32 x = D_800926E8;

    D_800926E8 = x - offset;
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

#ifdef NON_MATCHING
/* Set the text colour: highlighted (fading red) or plain white.
 * Does not match: the original reloads 0xff in the highlight branch. */
void func_8007EE08(s32 highlight) {
    if (highlight) {
        D_800926F0 = D_80059488 * 20;
        D_800926F4 = 0xFF;
        D_800926F8 = 0;
    } else {
        D_800926F0 = 0xFF;
        D_800926F4 = 0xFF;
        D_800926F8 = 0xFF;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE08);
#endif

#ifdef NON_MATCHING
/* Set the text colour: highlighted (fading toward blue) or plain white.
 * Does not match: the original reloads 0xff in the highlight branch. */
void func_8007EE68(s32 highlight) {
    if (highlight) {
        D_800926F8 = 0xFF;
        D_800926F0 = 0xFF - D_80059488 * 20;
        D_800926F4 = 0xFF - D_80059488 * 20;
        return;
    }
    D_800926F0 = 0xFF;
    D_800926F4 = 0xFF;
    D_800926F8 = 0xFF;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE68);
#endif

#ifdef NON_MATCHING
/* Build the list of the 49 entries (or, when filtering, of those whose
 * required level the current level reaches) and order it when filtering.
 * Does not match: the source and entry pointers get swapped registers. */
void func_8007EEE8(s32 filter) {
    s32 level = D_8006EF64;
    ListEntry **list = func_80031BDC(0xC4, 1);
    ListSource *source;
    s32 i;

    source = D_80092874;
    D_800928EC = list;
    D_80092888 = 0;
    for (i = 0; i < 49; i++, source++) {
        if (!filter || source->level <= level) {
            list[D_80092888++] = &D_80091964[i];
        }
    }
    if (filter) {
        func_8008895C();
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EEE8);
#endif

#ifdef NON_MATCHING
/* Allocate and lay out the 49 portrait slots: palette rows 511 down and
 * a 7x7 grid of image areas.
 * Does not match: the x shift is scheduled after the first stores. */
void func_8007EFB4(void) {
    u8 unused[0x30]; /* never used; the original frame keeps its slot */
    GridCell *cell;
    s32 id;
    s32 row;
    s32 col;
    s16 top;

    cell = D_8009270C = func_80031BDC(0x3D4, 0);
    id = 0x1FF;
    for (row = 0; row < 7; row++) {
        top = row * 0x20 + 0x1A0;
        for (col = 0; col < 7; col++) {
            cell->clut_x = 0x200;
            cell->clut_y = id--;
            cell->clut_w = 0x80;
            cell->clut_h = 1;
            cell->image_x = top;
            cell->image_y = col << 6;
            cell->image_w = 0x1E;
            cell->image_h = 0x40;
            cell++;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EFB4);
#endif

#ifdef NON_MATCHING
/* Draw the portrait of a list entry (the index wraps around the list) on
 * the left or right side. At fade 64 it is shown full size and unshaded;
 * below, it is shaded and shrunk by fade / 16.
 * Does not match: the vertex arithmetic is scheduled differently. */
void func_8007F05C(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade) {
    GridCell *cell;
    s32 shrink;
    s32 top;
    s32 bottom;
    s32 left;
    s32 right;
    s32 u;

    if (right_side) {
        x += 0xD3;
    } else {
        x += 0x33;
    }
    if (index > D_80092888 - 1) {
        index -= D_80092888;
    }
    if (index < 0) {
        index += D_80092888;
    }
    cell = &D_8009270C[D_800928EC[index]->id];
    if (fade == 0x40) {
        quad->len = 9;
        ((u8 *)&quad->rgbc)[3] = 0x2D;
        quad->xy0 = x | 0x300000;
        right = x + 0x3C;
        quad->xy1 = right | 0x300000;
        quad->xy2 = x | 0x700000;
        quad->xy3 = right | 0x700000;
    } else {
        fade += 0x40;
        shrink = (fade - 0x40) >> 4;
        quad->len = 9;
        quad->rgbc = fade | (fade << 8) | (fade << 16) | 0x2C000000;
        left = x - (shrink - 4);
        top = 0x34 - shrink;
        quad->xy0 = left | (top << 16);
        right = left + 0x34 + shrink * 2;
        bottom = top + 0x38 + shrink * 2;
        quad->xy1 = right | (top << 16);
        quad->xy2 = left | (bottom << 16);
        quad->xy3 = right | (bottom << 16);
    }
    u = cell->image_x * 2;
    quad->uv0 = u | (cell->image_y << 8);
    quad->uv1 = (u + 0x3B) | (cell->image_y << 8);
    quad->uv2 = u | ((cell->image_y + 0x3F) << 8);
    quad->uv3 = (u + 0x3B) | ((cell->image_y + 0x3F) << 8);
    quad->clut = func_80043A58(cell->clut_x, cell->clut_y);
    quad->tpage = func_80043A1C(1, 0, cell->image_x & 0xFF80, cell->image_y);
    func_80043B48(D_80092938, quad);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F05C);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F258);

void func_8007F834(void) {
    D_80092740 = 0;
    D_8009273C = 0;
    D_80092744 = 0;
}

#ifdef NON_MATCHING
/* Leave the settings screen: camera mode 1 and flags 0xc on both actors.
 * Does not match: the original addresses both flag words through two
 * address registers in the opposite register order. */
void func_8007F854(void) {
    s32 unused[2]; /* never used; the original frame keeps its slot */

    D_800912F0 = 1;
    func_80083C0C(1);
    D_80092734 = NULL;
    func_8007F834();
    D_8009872C.unkD4 |= 0xC;
    D_80097010.unkD4 |= 0xC;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F854);
#endif

void func_8007F8B4(void) {
    s32 unused[2]; /* never used; the original frame keeps its slot */

    func_80083C0C(1);
    D_80092734 = NULL;
    func_8007F834();
}

void func_8007F8E4(void) {
    func_80080C48(0);
    if (D_80099D98.option6 != 0 || D_80092950 == 1) {
        func_80083C0C(3);
    } else {
        D_80092950--;
        func_80083C0C(6);
    }
}

/* Highlight the text of a page's entry when it is under the cursor. */
void func_8007F948(MenuPage *page, s32 entry) {
    if (page->cursor == entry) {
        func_8007EE08(1);
    } else {
        func_8007EE08(0);
    }
}

/* Name of the chosen first setting. */
char *func_8007F97C(void) {
    return D_800912F4[D_80099D98.level];
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF60);

/* Draw the values column of the settings page, right-aligned, applying the
 * chosen speed as it is shown. */
void func_8007F9A0(MenuPage *page) {
    char text[8];

    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007F948(page, 0);
    func_8007ECF0(func_8007F97C());
    func_8007F948(page, 1);
    func_8003FBF8(text, D_8006FF5C, D_80099D98.speed + 1);
    D_80099DA4 = D_8009292C = D_8009130C[D_80099D98.speed];
    func_8007ECF0(text);
    func_8007F948(page, 2);
    func_8003FBF8(text, D_8006FF60, D_80091300[D_80099D98.rate]);
    func_8007ECF0(text);
    func_8007F948(page, 3);
    func_8007ECF0(D_80099D98.com1 ? "COM" : "USER1");
    func_8007F948(page, 4);
    func_8007ECF0(D_80099D98.driven ? "COM" : "USER2");
    func_8007EE08(0);
}

/* Draw the values column of the second settings page; the chosen entry of
 * setting 10 is also passed to 80081100 as 0x15 + entry. */
void func_8007FB0C(MenuPage *page) {
    char text[8];

    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007ECF0(D_8006FF7C);
    func_8007F948(page, 1);
    func_8007ECF0(D_8009132C[D_80099D98.command]);
    func_80081100(D_80099D98.command + 0x15, 1);
    func_8007F948(page, 2);
    func_8003FBF8(text, D_8006FF60, D_80091300[D_80099D98.rate]);
    func_8007ECF0(text);
    func_8007EE08(0);
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF7C);

/* Draw the vibration page: per controller port, the vibration setting when
 * a type-4 controller without the "COM" setting is connected (the entry is
 * hidden otherwise). */
void func_8007FBEC(void) {
    MenuPage *page;
    s32 active;
    s32 unused[2]; /* never used; the original frame keeps its slot */

    func_8007E894(0xA0, 0x8C);
    active = D_80092710 ^ 1;
    active &= 1;
    page = &D_800915AC[5];
    if (active && D_800915AC[5].cursor == 0) {
        D_8009272C = 1;
    } else {
        D_8009272C = 0;
    }
    func_8007E894(0x50, 0x8C);
    if (func_80035734(0) == 4 && D_80099D98.com1 == 0) {
        if (active) {
            func_8007F948(page, 1);
        }
        func_8007EC54((D_80099D98.option4 & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        D_800915AC[5].item->flags &= ~4;
    } else {
        D_800915AC[5].item->flags |= 4;
    }
    func_8007EE08(0);

    active = D_80092710 >> 1;
    active ^= 1;
    active &= 1;
    if (active && D_80092754 == 0) {
        active = 0;
    }
    page = &D_800915AC[6];
    if (active && D_800915AC[6].cursor == 0) {
        D_80092730 = 1;
    } else {
        D_80092730 = 0;
    }
    func_8007E894(0xF0, 0x8C);
    if (func_80035734(1) == 4 && D_80099D98.driven == 0) {
        if (active) {
            func_8007F948(page, 1);
        }
        func_8007EC54((D_80099D98.option5 & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        D_800915AC[6].item->flags &= ~4;
    } else {
        D_800915AC[6].item->flags |= 4;
    }
    func_8007EE08(0);
    func_8007F258(D_80092938, 1);
}

/* Draw the values column of the options page. */
void func_8007FE48(MenuPage *page) {
    char text[16];
    char *value;

    func_8007EE08(0);
    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007F948(page, 4);
    if (D_80099D98.option6 != 0) {
        func_8003FBF8(text, D_8006FF5C, D_80099D98.option6);
        value = text;
    } else {
        value = "#";
    }
    func_8007ECF0(value);
    func_8007F948(page, 5);
    func_8007ECF0(D_800912F4[D_80099D98.level]);
    func_8007F948(page, 6);
    func_8007ECF0(D_80092884 ? "ON" : "OFF");
    func_8007EE08(0);
}

/* Step a settings value with left/right: flag 4 reverses the direction,
 * flag 2 uses the repeating buttons, flag 1 wraps around (else clamps
 * silently). Plays the cursor sound when moved. */
s32 func_8007FF70(s32 value, s32 max, s32 flags) {
    s32 step = 1;
    u32 buttons;
    s32 moved;

    if (flags & 4) {
        step = -1;
    }
    moved = 0;
    if (flags & 2) {
        buttons = D_8009274C;
    } else {
        buttons = D_80092750;
    }
    if (buttons & 0x2000) {
        value += step;
    }
    if (buttons & 0x8000) {
        value -= step;
    }
    if (buttons & 0xA000) {
        moved = 1;
    }
    if (flags & 1) {
        if (value == -1) {
            value = max;
        }
        if (value > max) {
            value = 0;
        }
    } else {
        if (value == -1) {
            moved = 0;
            value = 0;
        }
        if (value > max) {
            moved = 0;
            value = max;
        }
    }
    if (moved) {
        func_8008EB4C(0x20);
    }
    return value;
}

void func_80080054(void) {
    D_80099D98.level = func_8007FF70(D_80099D98.level, 2, 0);
}

void func_80080090(void) {
    D_80099D98.speed = func_8007FF70(D_80099D98.speed, 7, 2);
}

void func_800800CC(void) {
    D_80099D98.rate = func_8007FF70(D_80099D98.rate, 4, 2);
}

void func_80080108(void) {
    D_80099D98.option4 = func_8007FF70(D_80099D98.option4, 1, 1);
}

void func_80080144(void) {
    D_80099D98.option5 = func_8007FF70(D_80099D98.option5, 1, 1);
}

void func_80080180(void) {
    D_80099D98.com1 = func_8007FF70(D_80099D98.com1, 1, 1);
}

void func_800801BC(void) {
    D_80099D98.driven = func_8007FF70(D_80099D98.driven, 1, 1);
}

void func_800801F8(void) {
    D_80099D98.option6 = func_8007FF70(D_80099D98.option6, 3, 2);
}

void func_80080234(void) {
    D_80092884 = func_8007FF70(D_80092884, 1, 1);
}

void func_80080268(void) {
    D_80099D98.command = func_8007FF70(D_80099D98.command, 13, 3);
}

/* First side's selection: cancel, move (skipping the other side's pick
 * unless shared picks are allowed or both already coincide) and confirm. */
void func_800802A4(void) {
    s32 same;

    if (D_80091364 == 0 && (D_8005948C & 0x40)) {
        D_80092710 &= ~1;
        func_80085134(0);
        func_8008EB4C(0x22);
    }
    if (!(D_80092710 & 1)) {
        same = D_80092700 == D_80092704;
        do {
            D_80092700 = func_8007FF70(D_80092700, D_80092888 - 1, 3);
        } while (D_80092700 == D_80092704 && !(D_80092748 & 1) && !same);
        if (D_80091364 == 0 && (D_8005948C & 0x20)) {
            D_80092710 |= 1;
            func_8008EB4C(0x21);
            func_8008509C(0, D_800928EC[D_80092700]->id);
        }
    }
}

/* Second side's selection; cancelling outside mode 2 leaves the screen. */
void func_8008040C(void) {
    s32 same;

    if (D_80092750 & 0x40) {
        if (D_800928C8 != 2) {
            D_80092710 = 0;
            func_80085134(1);
            func_8008EB4C(0x22);
            return;
        }
        D_80092710 &= ~2;
    }
    if (!(D_80092710 & 2)) {
        same = D_80092700 == D_80092704;
        do {
            D_80092704 = func_8007FF70(D_80092704, D_80092888 - 1, 3);
        } while (D_80092700 == D_80092704 && !(D_80092748 & 1) && !same);
        if (D_80092750 & 0x20) {
            D_80092710 |= 2;
            func_8008EB4C(0x21);
            func_8008509C(1, D_800928EC[D_80092704]->id);
        }
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080570);

/* Load both picks' portraits (palette and image) into their VRAM slots and
 * mark both sides confirmed. */
void func_80080644(s32 first, s32 second) {
    u8 *data = func_80031BDC(0x2000, 0);
    u8 *other;
    GridCell *cell;

    func_8002954C((u16 *)func_800289D0(6) + first, data, 0x1000, 0, 0);
    other = data + 0x1000;
    func_8002954C((u16 *)func_800289D0(6) + second, other, 0x1000, 0, 0);
    D_80092700 = first;
    D_80092704 = second;
    D_80092714 = first;
    D_80092720 = second;
    D_80092710 = 3;
    func_80028A60(0);
    cell = &D_8009270C[first];
    func_80044894(&cell->clut_x, data);
    func_80044894(&cell->image_x, data + 0x100);
    cell = &D_8009270C[second];
    func_80044894(&cell->clut_x, other);
    func_80044894(&cell->image_x, data + 0x1100);
    func_80032C18(data, 2);
}

#ifdef NON_MATCHING
/* Enter the selection screen in a mode: upload every portrait once, set
 * the pages' entry counts and labels, and reset both sides.
 * Does not match: the original reloads 4 into the branch delay slot. */
void func_80080780(s32 mode) {
    GridCell *cell;
    s32 i;
    s32 count;

    if (D_80092940 == 0) {
        func_80028A60(0);
        cell = D_8009270C;
        for (i = 0; i < 49; i++, cell++) {
            func_80044894(&cell->clut_x, D_800928D8 + (i << 12));
            func_80044894(&cell->image_x, D_800928D8 + (i << 12) + 0x100);
        }
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
    D_800928C8 = mode;
    count = 4;
    if (mode == 4) {
        count = 3;
    }
    D_800915AC[5].count = count;
    D_800915AC[6].count = 5;
    if (mode == 3) {
        D_80091369 = 0x27;
        D_80091391 = 0x28;
    } else {
        D_80091369 = 0x25;
        D_80091391 = 0x26;
    }
    func_80083C0C(1);
    D_80092710 = 0;
    D_80092728 = 0;
    D_80092724 = 0;
    D_8009271C = 0;
    D_80092718 = 0;
    D_80092714 = D_80092700;
    D_80092720 = D_80092704;
    func_80080964(5);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080780);
#endif

void func_800808F4(void) {
    D_80092924 = 1;
    func_8007F834();
}

void func_80080920(void) {
    D_80092924 = 1;
    func_800719F0();
    func_8008509C(0, 0);
    func_8008509C(1, 1);
}

/* Show a page, remembering the current one; 0xff returns to it. */
void func_80080964(s32 page) {
    MenuPage *previous;

    if (page == 0xFF) {
        D_80092734 = D_80092738;
        return;
    }
    previous = D_80092734;
    D_80092734 = &D_800915AC[page];
    D_80092738 = previous;
}

/* Whether page 3 is shown. */
s32 func_800809BC(void) {
    return D_80092734 == &D_800915AC[3];
}

/* Enter the settings/system menu at page 3 with every state reset. */
void func_800809D8(void) {
    func_80039FF8();
    D_80092734 = NULL;
    func_80080964(3);
    D_800915AC[3].cursor = 0;
    D_800915AC[4].cursor = 0;
    D_800928C8 = 0;
    D_80092758 = 0;
    func_8007F834();
    D_80092924 = 0;
    func_80080AA0(0);
    D_80092940 = 0;
    D_800928D8 = func_800891C0(6);
}

void func_80080A58(void) {
    if (D_80092940 == 0) {
        func_80028A60(0);
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
}

/* Free the loaded image data (or just forget it). */
void func_80080AA0(s32 forget) {
    if (forget) {
        D_80092760 = NULL;
    }
    if (D_80092760 != NULL) {
        func_800320E8(D_80092760);
        D_80092760 = NULL;
    }
}

/* Unpack the loaded image data and upload it to VRAM (320,256)-(640,474). */
void func_80080AE8(void) {
    s16 rect[4];

    if (D_80092760 != NULL) {
        func_800445D0(0);
        rect[0] = 0x140;
        rect[1] = 0x100;
        rect[2] = 0x140;
        rect[3] = 0xDA;
        func_8007313C(D_80092760, (u8 *)D_80092760 + 0x21E80);
        func_80044894(rect, D_80092760);
    }
}

/* Keep a copy of the shown screen: allocate the image buffer once, copy
 * the displayed buffer's area to (320,256) and read it back. */
void func_80080B58(void) {
    Rect area;

    if (D_80092760 == NULL) {
        func_80031BB4(1);
        D_80092760 = func_80031BDC(0x22100, 0);
        func_80031BB4(0);
    }
    func_800445D0(0);
    area = D_8009A0D8[(D_800928A0 + 1) & 1].draw.clip;
    func_8004495C(&area, 0x140, 0x100);
    if (D_80092760 != NULL) {
        func_800448F8(&area, D_80092760);
    }
    func_800445D0(0);
}

#ifdef NON_MATCHING
/* Open the system menu: mode 1 at page 0, mode 2 at page 7, else close.
 * Does not match: the shared tail is cross-jumped one instruction early. */
void func_80080C48(s32 mode) {
    func_80039FF8();
    func_8008EB4C(0x1F);
    if (mode == 1) {
        D_80092734 = NULL;
        func_80080964(0);
        D_800915AC[0].cursor = 0;
        D_800915AC[2].cursor = 1;
    } else if (mode == 2) {
        D_80092734 = NULL;
        func_80080964(7);
        D_800915AC[7].cursor = 1;
    } else {
        func_8007F8B4();
        return;
    }
    func_80083C0C(0);
    D_80092758 = 1;
    D_800926FC = 0;
    D_8009275C = 1;
    func_80080B58();
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080C48);
#endif

void func_80080D10(void) {
    D_800926DC = 0;
}

/* Link this frame's text quads and the menu overlay: the shown page's box
 * with its texture page and, while a page or the copy request is active, a
 * move of the kept screen copy into the draw buffer. */
void func_80080D20(void *ot) {
    PolyFT4 *quad = D_800926D4[D_800928A0];
    Rect area;
    s32 i;

    for (i = 0; i < D_800926DC; i++, quad++) {
        func_80043B48(ot, quad);
    }
    D_800926DC = 0;
    func_800811AC(ot);
    if ((D_80092734 != NULL && D_80092758 != 0) || D_800912F0 != 0) {
        if (D_80092734 != NULL) {
            func_80043B48(ot, &D_80092734->frame[D_800928A0]);
            func_80043E20(&D_800954C8[D_800928A0], 0, 0, func_80043A1C(0, 2, 0, 0));
            func_80043B48(ot, &D_800954C8[D_800928A0]);
        }
        area.x = 0x140;
        area.y = 0x100;
        area.w = 0x140;
        area.h = 0xDA;
        func_80043E4C(&D_80095498[D_800928A0], &area, D_8009A0D8[D_800928A0].draw.clip.x,
                      D_8009A0D8[D_800928A0].draw.clip.y);
        func_80043B48(ot, &D_80095498[D_800928A0]);
    }
    D_800912F0 = 0;
}

/* Set up the two semi-transparent sprite strips (at y 180 and 195) sharing
 * one pixel buffer, and their texture page. */
void func_80080F04(void) {
    u8 *pixels = func_80031BDC(0x6B4, 0);

    D_80095510[0].pixels = D_80095510[1].pixels = pixels;
    D_80095510[0].sprite[0].xy0 = 0xB40000;
    D_80095510[0].sprite[0].uv0 = 0x3000;
    func_80043D14(&D_80095510[0].sprite[0]);
    func_80043C24(&D_80095510[0].sprite[0], 1);
    D_80095510[0].sprite[0].h = 0xD;
    D_80095510[0].sprite[0].clut = D_800595D4;
    D_80095510[0].sprite[1] = D_80095510[0].sprite[0];
    D_80095510[1].sprite[0].xy0 = 0xC30000;
    D_80095510[1].sprite[0].uv0 = 0x3000;
    func_80043D14(&D_80095510[1].sprite[0]);
    func_80043C24(&D_80095510[1].sprite[0], 1);
    D_80095510[1].sprite[0].h = 0xD;
    D_80095510[1].sprite[0].clut = D_80059414;
    D_80095510[1].sprite[1] = D_80095510[1].sprite[0];
    func_80043E20(&D_80095570[0], 0, 0, func_80043A1C(0, 0, 0x140, 0x30));
    D_80095570[1] = D_80095570[0];
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081094);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081100);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800811AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800812BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800814AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008151C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008162C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8007008C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081A44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081D2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081E00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081E6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081ECC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082178);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082300);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082458);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082488);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082880);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800828C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800828F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082A70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082C4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082E60);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800831C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800832C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083310);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008369C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083738);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083B54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083BB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083C0C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083CD8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083CE8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083DCC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800840CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800846A0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800849E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084A40);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084A64);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084AE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084B48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084BEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084C88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084FD0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085014);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085070);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008509C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085134);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008518C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800851D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085264);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800852C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085E34);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085E60);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085E90);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085EAC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085EC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800864B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800866D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800868E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086E24);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086E70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086FF8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087068);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800875EC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087650);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087698);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008779C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087830);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800878DC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087AB0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087B74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087E38);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087EA0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008820C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800882D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088308);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008832C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800884E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008859C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088658);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800886FC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088754);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800887A4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088838);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800888B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800888E4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088908);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088940);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008895C);

/* Once every progress flag 0..48 except 22 is set, mark the options
 * complete and apply the unlock. */
s32 func_800889C8(void) {
    s32 flag;

    for (flag = 0; flag < 49; flag++) {
        if (flag != 22 && !func_800888E4(flag)) {
            return 0;
        }
    }
    D_8006F978.options.complete = 1;
    func_8008895C();
    return 0;
}

/* Store the current option settings in the saved options word. */
void func_80088A40(void) {
    if (D_8005061C) {
        D_8006F978.options.version = 1;
        D_8006F978.options.option4 = D_80099D98.option4;
        D_8006F978.options.option5 = D_80099D98.option5;
        D_8006F978.options.option6 = D_80099D98.option6;
        D_8006F978.options.option13 = D_80099D98.level;
    }
}

/* Load the option settings from the saved options word, or write the
 * defaults when it was never written; a completed word clears the flags. */
void func_80088AF8(void) {
    s32 i;

    if (D_8005061C) {
        D_800927EC = 0;
        if (D_8006F978.options.version == 1) {
            D_80099D98.option4 = D_8006F978.options.option4;
            D_80099D98.option5 = D_8006F978.options.option5;
            D_80099D98.option6 = D_8006F978.options.option6;
            D_80099D98.level = D_8006F978.options.option13;
            if (D_8006F978.options.complete) {
                for (i = 0; i < 8; i++) {
                    D_8006F978.flags[i] = 0;
                }
            }
        } else {
            D_80099D98.option4 = 0;
            D_80099D98.option5 = 0;
            D_80099D98.option6 = 2;
            D_80099D98.level = 0;
            func_80088A40();
        }
    }
}

/* Refresh the progress flags, then check for completion. */
void func_80088BD4(void) {
    func_800888B0();
    func_800889C8();
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088BFC);

/* Debug counter: pad bits 0x4/0x1 step it up/down (not below zero), then
 * print it. */
void func_80088C28(void) {
    u16 pad = D_80059570;

    if (pad & 4) {
        D_800927F4++;
    }
    if (pad & 1) {
        D_800927F4--;
    }
    if (D_800927F4 < 0) {
        D_800927F4 = 0;
    }
    func_8003278C(1, D_800927F4, 10, 0x80AD);
}

/* Set up the given window record's defaults. */
void func_80088CBC(s32 index) {
    Window *window = &D_8009A0D8[index];

    window->sprite.len = 3;
    window->sprite.code = 0x7D;
    *(u16 *)&window->sprite.u0 = 0x3000;
    window->sprite.clut = func_80043A58(0x3F0, 0xC0);
}

/* Menu mode start-up: frame callback, display and windows, the start
 * state from the boot word, then the mode's first screen. */
void func_80088D1C(void) {
    s32 unused[2]; /* never used; the original frame has these 8 bytes */

    /* The callback starts one word into splat's func_80088BFC, which begins
     * with a data word. */
    func_800444D8((u8 *)func_80088BFC + 4);
    func_80048BC4();
    func_80032498(6, D_80091BB0);
    func_80028470(0x30, 0);
    func_800374E8(4, 2, 0x138, 0xDA, 0x14, 1, 0x3C0, 0x1F0, 0x3C0, 0x1EF, 0);
    func_80088CBC(0);
    func_80088CBC(1);
    switch (D_80010000) {
    case -1:
        D_800928CC = 2;
        break;
    case 0:
        D_800928CC = 1;
        break;
    default:
        D_800928CC = 0;
        break;
    }
    D_80092868 = &D_8009A0D8[0];
    D_80092870 = &D_8009A0D8[1];
    func_8008A110(-1, -1);
    func_8008A128(-1, -1);
    D_80092898 = 2;
    D_8009289C = 1;
    D_80092930 = NULL;
    D_800928E8 = 0;
    D_800928A0 = 0;
    D_80092920 = 0;
    D_80092930 = NULL;
    D_800928D0 = 7;
    func_8008E620();
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_80070284);

#ifdef NON_MATCHING
/* Menu mode entry: start up, start the mode's task, then run the frame
 * loop forever (resume the task, build one buffer while the other is
 * shown, debug meters).
 * Does not match: the buffer flip stores are scheduled differently, and the
 * original rate string is followed by two non-zero padding bytes (0x0894)
 * that a C literal cannot reproduce. */
void func_80088E90(void) {
    DispEnv disp;
    Task *task;
    s32 last;
    s32 fps;
    s32 load;

    func_80088D1C();
    task = func_8008BA2C(((void (**)(s32))func_80088BFC)[D_80050618], 0, (u32 *)0x801FE000, 0x400);
    D_80059488;
frame:
    D_800595C0 = 0;
    D_80059578 = 0;
    D_800928A0 = (D_800928E8 + 1) & 1;
    D_80092870 = &D_8009A0D8[D_800928E8 & 1];
    D_800928E8++;
    D_80092868 = &D_8009A0D8[D_800928A0];
    D_80092938 = &D_80092868->ot;
    disp = D_80092868->disp;
    func_80019CA0();
    func_80043BE4(D_80092938);
    if ((D_800928D0 & 0x10) && D_80092930 != NULL) {
        D_80092930(D_80092938);
    }
    func_80037324(D_80092938);
    func_8008BB3C(task);
    func_8008EADC();
    func_80032CB8();
    if (D_80092920 & 1) {
        func_80043B48(D_80092938, &D_80092868->background);
    }
    load = func_8004B54C(1);
    fps = 60 / (u32)(D_80059488 - last);
    last = D_80059488;
    if (D_800928D0 & 8) {
        func_80088C28();
    }
    if (D_800928D0 & 1) {
        func_8003700C("POLYGON:%4d/%4d\n", D_80059578, D_800595C0);
    }
    if (D_800928D0 & 2) {
        func_8003700C("CPU/GPU:%4d/%3d\n", load, D_800927F0);
    }
    if (D_800928D0 & 4) {
        func_8003700C("RATE   : %3dfps\n", fps);
    }
    func_80036DC8(0xFF, 0xFF, 0xFF);
    func_8008ACB8(D_80092898);
    func_8004B54C(D_80092898);
    func_8008AC8C();
    func_800445D0(0);
    func_80044E9C(&disp);
    func_80044D48(D_80092938, D_80092868);
    goto frame;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088E90);
#endif

/* Load a whole file into a new allocation and return it. */
void *func_800891C0(s32 file) {
    void *data = func_80031BDC(func_80028738(file), 1);

    func_800295D8(file, data, 0, 0);
    return data;
}

/* Set the screen scale matrices for a width x height display (320x240 is
 * unit scale). */
void func_80089210(s32 width, s32 height) {
    s32 sx = ((width << 12) / 320) * height / width;
    s32 sy = ((height << 12) / 240) * height / width;

    D_8009A2D8 = D_80091C0C;
    D_80096FE0 = D_80091C0C;
    D_80096FE0.m[0][0] = sx;
    D_80096FE0.m[1][1] = sy;
}

/* Set both buffers' display environments and background tiles for the
 * resolution; taller than 256 lines is interlaced. */
void func_80089330(s32 width, s32 height) {
    if (height > 256) {
        func_800439E0(&D_8009A0D8[0].disp, 0, 0, width, height);
        func_800439E0(&D_8009A0D8[1].disp, 0, 0, width, height);
        D_8009A0D8[1].disp.isinter = 1;
        D_8009A0D8[0].disp.isinter = 1;
        D_8009A0D8[0].disp.screen.x = 0;
        D_8009A0D8[0].disp.screen.y = 0x10;
        D_8009A0D8[0].disp.screen.w = 0x100;
        D_8009A0D8[0].disp.screen.h = 0xD4;
        D_8009A0D8[1].disp.screen.x = 0;
        D_8009A0D8[1].disp.screen.y = 0x10;
        D_8009A0D8[1].disp.screen.w = 0x100;
        D_8009A0D8[1].disp.screen.h = 0xD4;
    } else {
        func_800439E0(&D_8009A0D8[0].disp, 0, 0x100, width, height);
        func_800439E0(&D_8009A0D8[1].disp, 0, 0, width, height);
        D_8009A0D8[1].disp.isinter = 0;
        D_8009A0D8[0].disp.isinter = 0;
        D_8009A0D8[0].disp.screen.x = 0;
        D_8009A0D8[0].disp.screen.y = 0xA;
        D_8009A0D8[0].disp.screen.w = 0x100;
        D_8009A0D8[0].disp.screen.h = height;
        D_8009A0D8[1].disp.screen.x = 0;
        D_8009A0D8[1].disp.screen.y = 0xA;
        D_8009A0D8[1].disp.screen.w = 0x100;
        D_8009A0D8[1].disp.screen.h = height;
    }
    D_8009285C = width;
    D_8009286C = height;
    func_80089210(D_8009285C, D_8009286C);
    D_8009A0D8[0].background.len = 3;
    D_8009A0D8[0].background.colour = 0x60000000;
    D_8009A0D8[0].background.x0 = 0;
    D_8009A0D8[0].background.y0 = 0;
    D_8009A0D8[0].background.w = width;
    D_8009A0D8[0].background.h = height;
    D_8009A0D8[1].background = D_8009A0D8[0].background;
}

/* Set the geometry and both buffers' drawing environments for the
 * resolution. */
void func_80089534(s32 width, s32 height) {
    func_8004A12C(width / 2, height / 2);
    func_8004A14C(0x180);
    func_8002DFF0(width, height);
    if (height > 256) {
        func_80043928(&D_8009A0D8[0].draw, 0, 0, width, height);
        func_80043928(&D_8009A0D8[1].draw, 0, 0, width, height);
        D_8009A0D8[1].draw.dfe = 0;
        D_8009A0D8[0].draw.dfe = 0;
    } else {
        func_80043928(&D_8009A0D8[0].draw, 0, 0, width, height);
        func_80043928(&D_8009A0D8[1].draw, 0, 0x100, width, height);
    }
    D_8009A0D8[0].draw.dtd = D_8009A0D8[1].draw.dtd = 1;
    D_8009A0D8[0].draw.isbg = D_8009A0D8[1].draw.isbg = 0;
    D_8009A0D8[0].draw.tpage = D_8009A0D8[1].draw.tpage = func_80043A1C(0, 2, 0x280, 0);
    func_80045534(D_8009A0D8[0].draw.dr_env, &D_8009A0D8[0].draw);
    func_80045534(D_8009A0D8[1].draw.dr_env, &D_8009A0D8[1].draw);
    func_800453E8(D_8009A0D8[0].modeD0, &D_8009A0D8[0].draw.clip);
    func_800453E8(D_8009A0D8[1].modeD0, &D_8009A0D8[1].draw.clip);
    func_8004546C(D_8009A0D8[0].modeDC, D_8009A0D8[0].draw.ofs);
    func_8004546C(D_8009A0D8[1].modeDC, D_8009A0D8[1].draw.ofs);
}

/* Set up geometry, screen and scale for a width x height display. */
void func_800896C4(s32 width, s32 height) {
    func_8004A12C(width / 2, height / 2);
    func_8004A14C((width << 8) / width);
    func_8002DFF0(width, height);
    func_80089210(width, height);
}

/* Re-apply the drawing environments at the current resolution. */
void func_8008973C(void) {
    func_80089534(D_8009285C, D_8009286C);
}

/* Set the display and drawing environments for a resolution. */
void func_8008976C(s32 width, s32 height) {
    func_80089330(width, height);
    func_80089534(width, height);
}

/* Set a layer's drawing areas and offsets for both buffers (the second
 * buffer lies `second` lines lower) and its black background tiles. */
void func_800897AC(OtPair *layer, s32 x, s32 y, s32 w, s32 h, s32 second) {
    Rect area;
    s16 offset[2];

    area.x = x;
    area.y = y;
    area.w = w;
    area.h = h;
    func_800453E8(layer->area[0], &area);
    area.y = y + second;
    func_800453E8(layer->area[1], &area);
    offset[0] = x;
    offset[1] = y;
    func_8004546C(layer->offset[0], offset);
    offset[1] = y + second;
    func_8004546C(layer->offset[1], offset);
    layer->tile[0].len = 3;
    layer->tile[0].colour = 0x60000000;
    layer->tile[0].x0 = 0;
    layer->tile[0].y0 = 0;
    layer->tile[0].w = w;
    layer->tile[0].h = h;
    layer->tile[1] = layer->tile[0];
    layer->flags |= 0xC;
}

/* Build a view matrix looking from eye to at with the given up vector. */
void func_800898BC(Matrix *m, SVector *eye, SVector *at, SVector *up) {
    D_8009A0C8.vx = at->vx - eye->vx;
    D_8009A0C8.vy = at->vy - eye->vy;
    D_8009A0C8.vz = at->vz - eye->vz;
    D_8009A918.vx = up->vx;
    D_8009A918.vy = up->vy;
    D_8009A918.vz = up->vz;
    func_80048D7C(&D_8009A0C8, &D_80096F98);
    func_8004A480(&D_8009A918, &D_80096F98, &D_8009A0C8);
    func_80048D7C(&D_8009A0C8, &D_80097000);
    func_8004A480(&D_80096F98, &D_80097000, &D_8009A0C8);
    func_80048D7C(&D_8009A0C8, &D_8009A918);
    m->m[0][0] = D_80097000.vx;
    m->m[0][1] = D_80097000.vy;
    m->m[0][2] = D_80097000.vz;
    m->m[1][0] = D_8009A918.vx;
    m->m[1][1] = D_8009A918.vy;
    m->m[1][2] = D_8009A918.vz;
    m->m[2][0] = D_80096F98.vx;
    m->m[2][1] = D_80096F98.vy;
    m->m[2][2] = D_80096F98.vz;
    func_80049CEC(m, eye, &D_8009A0C8);
    func_80049BDC(&D_80096FE0, m);
    m->t[0] = -D_8009A0C8.vx;
    m->t[1] = -D_8009A0C8.vy;
    m->t[2] = -D_8009A0C8.vz;
}

/* Point the owner's view from eye toward target (eye kept as the last eye
 * position). */
void func_80089A98(NodeOwner *owner, Vector *target, Vector *eye) {
    SVector up;
    SVector from;
    SVector origin;

    up.vy = 0x1000;
    up.vz = 0;
    up.vx = 0;
    D_80096FA8 = *eye;
    from.vx = target->vx - eye->vx;
    from.vy = target->vy - eye->vy;
    from.vz = target->vz - eye->vz;
    origin.vz = 0;
    origin.vx = 0;
    origin.vy = 0;
    func_800898BC(&owner->node->view, &from, &origin, &up);
}

/* Reset a node: unlinked, no payload, zero position and angles, identity
 * matrices. */
Node *func_80089B44(Node *node) {
    node->type = 0;
    node->parent = NULL;
    node->child = NULL;
    node->next = NULL;
    node->callback = NULL;
    node->data = NULL;
    node->position.vz = 0;
    node->position.vy = 0;
    node->position.vx = 0;
    node->unk44.vz = 0;
    node->unk44.vy = 0;
    node->unk44.vx = 0;
    node->rotation.vz = 0;
    node->rotation.vy = 0;
    node->rotation.vx = 0;
    node->unk6C = D_80091C0C;
    node->unk4C = node->unk6C;
    node->view = node->unk4C;
    return node;
}

/* Allocate a reset scene node. */
Node *func_80089C54(void) {
    func_800324B8(8);
    return func_80089B44(func_80031BDC(sizeof(Node), 0));
}

/* Append child as the last child of parent. */
void func_80089C88(Node *parent, Node *child) {
    Node *last;

    child->parent = parent;
    if (parent->child == NULL) {
        parent->child = child;
    } else {
        last = parent->child;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = child;
    }
}

/* Unlink a node from its parent's child list. */
void func_80089CD8(Node *node) {
    Node *parent;
    Node *first;
    Node *prev;

    if (node == NULL) {
        return;
    }
    parent = node->parent;
    if (parent == NULL) {
        return;
    }
    node->parent = NULL;
    first = parent->child;
    if (first == NULL) {
        return;
    }
    if (first == node) {
        parent->child = node->next;
    } else {
        prev = first;
        while (prev->next != node) {
            prev = prev->next;
        }
        prev->next = node->next;
    }
    node->next = NULL;
}

/* Free a node, its children and following siblings, and its payload. */
void func_80089D5C(Node *node) {
    if (node == NULL) {
        return;
    }
    func_80089D5C(node->child);
    func_80089D5C(node->next);
    switch (node->type) {
    case 1:
        func_80089FF8(node->data);
        break;
    case 2:
        func_80089EB4(node->data);
        break;
    case 5:
        func_8008C120(node->data);
        break;
    }
    if (node->data != NULL) {
        func_800320E8(node->data);
    }
    func_800320E8(node);
}

/* Make a node a model node. */
void func_80089E2C(Node *node, Model *model) {
    node->data = model;
    node->type = 1;
}

/* Make a node a type 3 node. */
void func_80089E3C(Node *node) {
    node->type = 3;
}

/* Make a node a type 4 node. */
void func_80089E48(Node *node) {
    node->type = 4;
}

/* Make a node a model set node. */
void func_80089E54(Node *node, ModelSet *set) {
    node->data = set;
    node->type = 2;
}

/* Make a node a type 6 node. */
void func_80089E64(Node *node, void *data) {
    node->data = data;
    node->type = 6;
}

/* Allocate an empty model set payload at unit scale. */
ModelSet *func_80089E74(void) {
    ModelSet *set;

    func_800324B8(4);
    set = func_80031BDC(sizeof(ModelSet), 0);
    set->scale[2] = 0x1000;
    set->scale[1] = 0x1000;
    set->scale[0] = 0x1000;
    set->nodes = NULL;
    set->players = NULL;
    return set;
}

/* Free a model set's node table and players (and, when owned, the
 * players' keys). */
void func_80089EB4(ModelSet *set) {
    s32 i;

    if (set->nodes != NULL) {
        func_800320E8(set->nodes);
    }
    if (set->players != NULL) {
        if (!D_80091C2C) {
            i = set->count;
            while (--i != -1) {
                if (set->players[i].header != NULL) {
                    func_800320E8(set->players[i].keys);
                }
            }
        }
        func_800320E8(set->players);
    }
}

/* Reset a model payload: grey, nothing loaded. */
Model *func_80089F8C(Model *model) {
    model->unk4 = 1;
    model->flags = 0;
    model->packets[0] = NULL;
    model->packets[1] = NULL;
    model->unk10 = 0;
    model->file = NULL;
    model->unk1C = 0;
    model->b = 0x40;
    model->g = 0x40;
    model->r = 0x40;
    return model;
}

/* Allocate a reset model payload. */
Model *func_80089FC4(void) {
    func_800324B8(1);
    return func_80089F8C(func_80031BDC(sizeof(Model), 0));
}

/* Release a model payload's resources. */
void func_80089FF8(Model *model) {
    if (model->packets[0] != NULL) {
        func_80032C18(model->packets[0], 2);
    }
    func_8002CBBC(model->file);
}

/* Pass the model texture page and CLUT positions to target, or zeros when
 * no texture page is set. */
void func_8008A040(void *target) {
    if (D_80092800 > 0) {
        func_8002DDE4(target, 1, D_80092800, D_80092804, 1, D_80092808, D_8009280C);
    } else {
        func_8002DDE4(target, 0, 0, 0, 0, 0, 0);
    }
}

/* Set a model node's colour. */
void func_8008A0B4(Node *node, u8 r, u8 g, u8 b) {
    ((Model *)node->data)->r = r;
    ((Model *)node->data)->g = g;
    ((Model *)node->data)->b = b;
    ((Model *)node->data)->flags |= 0x10;
}

/* Clear a model node's colour override. */
void func_8008A0F4(Node *node) {
    ((Model *)node->data)->flags &= ~0x10;
}

/* Set the texture page position used for loaded models (-1 = none). */
void func_8008A110(s16 x, s16 y) {
    D_80092800 = x;
    D_80092804 = y;
}

/* Set the CLUT position used for loaded models (-1 = none). */
void func_8008A128(s16 x, s16 y) {
    D_80092808 = x;
    D_8009280C = y;
}

/* Set the texture page and CLUT positions used for loaded models. */
void func_8008A140(s16 tx, s16 ty, s16 cx, s16 cy) {
    D_80092800 = tx;
    D_80092804 = ty;
    D_80092808 = cx;
    D_8009280C = cy;
}

/* Use no texture page or CLUT override for loaded models. */
void func_8008A168(void) {
    D_80092800 = D_80092808 = -1;
}

/* Load a model file into a model payload, applying the texture page and
 * CLUT overrides. */
void func_8008A184(Model *model, ModelFile *file) {
    model->file = file;
    model->unk10 = func_800303C8(file, 1);
    func_8002CB54(model->file, &model->packets[0], &model->packets[1]);
    if (D_80092800 >= 0) {
        func_8002CC54(func_80043A1C(0, 1, D_80092800, D_80092804));
    }
    if (D_80092808 >= 0) {
        func_8002CC74(D_80092808, D_8009280C);
    }
    func_8002C8CC(model->file, model->packets[0], 2);
    func_800732AC(model->packets[1], model->packets[0], model->file->unk34);
    model->flags |= 2;
}

/* Allocate a dim light pointing along zero angles. */
Light *func_8008A254(void) {
    Light *light;

    func_800324B8(0xB);
    light = func_80031BDC(sizeof(Light), 0);
    light->colour[0] = light->colour[1] = light->colour[2] = 0x10;
    light->direction[0] = light->direction[1] = light->direction[2] = 0;
    return light;
}

/* Free a payload. */
void func_8008A298(void *p) {
    func_800320E8(p);
}

/* Allocate an ordering table pair of the given length and its depth
 * shift (the length should be a power of two up to 0x4000). */
OtPair *func_8008A2B8(u16 length) {
    OtPair *pair;
    u32 *ot;
    s32 bit;

    func_800324B8(0xC);
    pair = func_80031BDC(sizeof(OtPair), 0);
    func_800324B8(0xC);
    ot = func_80031BDC(length * 8, 0);
    pair->ot[0] = ot;
    pair->ot[1] = ot + length;
    pair->flags = 1;
    pair->shift = 14;
    pair->unk0 = 0;
    pair->length = length;
    pair->last[0] = &pair->ot[0][length - 1];
    pair->last[1] = &pair->ot[1][length - 1];
    for (bit = 1; bit != length;) {
        bit <<= 1;
        if (bit > 0x4000) {
            pair->shift = 14;
            break;
        }
        pair->shift--;
    }
    return pair;
}

void func_8008A3A0(void) {
}

/* Free a holder and its resource. */
void func_8008A3A8(Holder *holder) {
    func_80032C18(holder->resource, 3);
    func_800320E8(holder);
}

/* Allocate a light rig: a root and three light nodes (key light turned
 * round, fill lights level), grey ambient, owning holder. */
LightRig *func_8008A3E0(Holder *holder) {
    LightRig *rig;

    func_800324B8(9);
    rig = func_80031BDC(sizeof(LightRig), 0);
    rig->unk0 = 0;
    rig->nodes[0] = &rig->storage[0];
    rig->nodes[1] = &rig->storage[1];
    rig->nodes[2] = &rig->storage[2];
    rig->nodes[3] = &rig->storage[3];
    func_80089B44(&rig->storage[0]);
    func_80089B44(&rig->storage[1]);
    func_80089B44(&rig->storage[2]);
    func_80089B44(&rig->storage[3]);
    func_80089E64(rig->nodes[1], func_8008A254());
    func_80089E64(rig->nodes[2], func_8008A254());
    func_80089E64(rig->nodes[3], func_8008A254());
    rig->nodes[1]->position.vy = rig->nodes[2]->position.vy = rig->nodes[3]->position.vy = -2;
    rig->nodes[1]->position.vx = 0;
    rig->nodes[1]->position.vz = 1;
    rig->nodes[2]->position.vx = -1;
    rig->nodes[2]->position.vz = -1;
    rig->nodes[3]->position.vx = 1;
    rig->nodes[3]->position.vz = -1;
    ((Light *)rig->nodes[1]->data)->direction[0] = ((Light *)rig->nodes[1]->data)->direction[1] =
        ((Light *)rig->nodes[1]->data)->direction[2] = 0x800;
    ((Light *)rig->nodes[2]->data)->direction[0] = ((Light *)rig->nodes[2]->data)->direction[1] =
        ((Light *)rig->nodes[2]->data)->direction[2] = 0;
    *(Light *)rig->nodes[3]->data = *(Light *)rig->nodes[2]->data;
    rig->holder = holder;
    rig->r = rig->g = rig->b = 0;
    rig->r = rig->g = rig->b = 0x10;
    func_8008ABAC(&rig->nodes[1]);
    return rig;
}

/* Free a light rig, its lights and its holder. */
void func_8008A5BC(LightRig *rig) {
    func_800320E8(rig->storage[1].data);
    func_800320E8(rig->storage[2].data);
    func_800320E8(rig->storage[3].data);
    func_8008A3A8(rig->holder);
    func_800320E8(rig);
}

/* Enable model colour overrides. */
void func_8008A618(void) {
    D_80092810 = 1;
}

/* Disable model colour overrides. */
void func_8008A62C(void) {
    D_80092810 = 0;
}

/* Draw a model into the current ordering table, with its colour override
 * (or grey) as the GTE back colour when overrides are enabled. */
void func_8008A63C(Model *model) {
    D_80050104 = 0;
    if (D_80092810) {
        if (model->flags & 0x10) {
            gte_SetBackColor(model->r, model->g, model->b);
        } else {
            gte_SetBackColor(0x40, 0x40, 0x40);
        }
    }
    func_8002C700(model->file, model->packets[D_800928A0], D_800928E4, model->unk4);
}

/* Set the current buffer's colour, noting whether it changed. */
void func_8008A6F8(CVector *colour) {
    CVector *current = &D_80092818[D_800928A0];

    if (colour->r == current->r && colour->g == current->g && colour->b == current->b) {
        D_80092914 = 0;
    } else {
        D_80092914 = 1;
        *current = *colour;
        current->cd = 0x20;
    }
}

/* Write the current buffer's colour into every primitive of an instance. */
void func_8008A78C(Node *node) {
    ModelPrims *prims = ((Instance *)node->data)->prims;
    s32 i = prims->count;
    ModelPrim *prim = prims->prims[D_800928A0];
    u32 colour = *(u32 *)&D_80092818[D_800928A0];

    while (--i != -1) {
        prim->colour = colour;
        prim++;
    }
}

/* Update a node tree's matrices (model sets relative to the eye, other
 * nodes relative to their parent) and draw its shown models and
 * instances. */
void func_8008A7E0(Node *node) {
    if (node->callback != NULL) {
        node->callback(node);
    }
    switch (node->type) {
    case 2:
        func_8003F738(&node->unk44, &node->view);
        if (D_8009289C) {
            func_8004920C(&node->parent->view, &node->view, &node->unk6C);
        } else {
            node->unk6C = node->view;
        }
        func_800731F8(&node->view, ((ModelSet *)node->data)->scale);
        node->unk4C = node->view;
        node->unk4C.t[0] = node->unk4C.t[1] = node->unk4C.t[2] = 0;
        node->view.t[0] = node->position.vx - D_80096FA8.vx;
        node->view.t[1] = node->position.vy - D_80096FA8.vy;
        node->view.t[2] = node->position.vz - D_80096FA8.vz;
        func_8004931C(&node->parent->view, &node->view, &node->view);
        break;
    case 0:
    case 1:
        if (node->parent != NULL) {
            node->position.vx = node->rotation.vx;
            node->position.vy = node->rotation.vy;
            node->position.vz = node->rotation.vz;
            func_8003F738(&node->unk44, &node->view);
            func_80049D9C(&node->view, &node->position);
            func_8004920C(&node->parent->unk6C, &node->view, &node->unk6C);
            func_8004931C(&node->parent->unk4C, &node->view, &node->unk4C);
            func_8004931C(&node->parent->view, &node->view, &node->view);
        }
        if (node->type == 1 && !(((Model *)node->data)->flags & 1)) {
            func_80030B14(&node->unk6C);
            gte_SetRotMatrix(&node->view);
            gte_SetTransMatrix(&node->view);
            func_8008A63C(node->data);
        }
        break;
    case 5:
        if (((Instance *)node->data)->type == 1) {
            node->view = ((Instance *)node->data)->source->unk4C;
            gte_SetRotMatrix(&node->view);
            gte_SetTransMatrix(&node->view);
            if (D_80092914) {
                func_8008A78C(node);
            }
            func_8008BCC8(((Instance *)node->data)->prims->mesh, ((Instance *)node->data)->prims->work);
        }
        break;
    }
    if (node->child != NULL) {
        func_8008A7E0(node->child);
    }
    if (node->next != NULL) {
        func_8008A7E0(node->next);
    }
}

/* Load the three rig lights into the light slots. */
void func_8008ABAC(Node **lights) {
    func_80030A30(0, lights[0]->data);
    func_80030A30(1, lights[1]->data);
    func_80030A30(2, lights[2]->data);
}

/* Clear the current buffer's ordering table of a pair and make it the one
 * primitives are added to. */
void func_8008AC0C(OtPair *pair) {
    func_80044AD8(pair->ot[D_800928A0], pair->length);
    D_800928E4 = pair->ot[D_800928A0];
    D_80050100 = pair->shift;
}

/* Choose the ordering table pair to compact at the end of the frame. */
void func_8008AC7C(OtPair *pair) {
    D_80091C30 = pair;
}

/* Note the frame's start time. */
void func_8008AC8C(void) {
    D_80092820 = func_80040690(0xF2000001);
}

/* While time remains in the frame budget (frames x 240 ticks, default
 * 192), link the tags the chosen table's entries point at past runs of
 * empty primitives, from the deepest entry down to entry 4. */
void func_8008ACB8(s32 frames) {
    OtPair *pair = D_80091C30;
    s32 start;
    s32 limit;
    s32 elapsed;
    s32 i;
    u32 *entry;
    u32 *tag;

    if (pair == NULL) {
        return;
    }
    start = D_80092820;
    D_80091C30 = NULL;
    if (frames != 0) {
        limit = frames * 240;
    } else {
        limit = 0xC0;
    }
    for (i = pair->length - 1; i >= 4; i--) {
        elapsed = func_80040690(0xF2000001) - start;
        if (elapsed < 0) {
            elapsed += 0x10000;
        }
        if (elapsed > limit) {
            return;
        }
        entry = (u32 *)pair->ot[D_800928A0][i];
        tag = (u32 *)((*entry & 0xFFFFFF) - 0x80000000);
        if (TAG_LEN(tag) == 0) {
            while (i >= 5) {
                tag = (u32 *)((*tag & 0xFFFFFF) - 0x80000000);
                i--;
                if (TAG_LEN(tag) != 0) {
                    break;
                }
            }
            *entry = (*entry & 0xFF000000) | ((u32)tag & 0xFFFFFF);
        }
    }
}

/* Link a layer's table into the frame's ordering table with its area,
 * offset and background packets for the current buffer. */
void func_8008AE1C(OtPair *layer) {
    func_8008AC7C(layer);
    func_80043B84(D_80092938, layer->last[D_800928A0], layer->ot[D_800928A0]);
    if (!(layer->flags & 4)) {
        func_800453E8(layer->area[D_800928A0], &D_80092868->draw.clip);
    }
    if (!(layer->flags & 8)) {
        func_8004546C(layer->offset[D_800928A0], D_80092868->draw.ofs);
    }
    if (layer->flags & 0x10) {
        func_80043B48(D_80092938, &layer->tile[D_800928A0]);
    }
    func_80043B48(D_80092938, layer->offset[D_800928A0]);
    func_80043B48(D_80092938, layer->area[D_800928A0]);
}

/* Relocate a scene file's pointers to where it was loaded. */
SceneFile *func_8008AF6C(SceneFile *scene) {
    s32 delta = (u8 *)scene - scene->base;
    u32 i;

    scene->base = (u8 *)scene;
    scene->unk0 += delta;
    scene->unk4 += delta;
    scene->unk10 += delta;
    scene->unk14 += delta;
    scene->unk18 += delta;
    scene->unk20 += delta;
    scene->unk24 += delta;
    if (scene->target != NULL) {
        scene->target += delta;
        func_8008A040(scene->target);
    }
    if (scene->table != NULL) {
        scene->table = (u32 *)((u8 *)scene->table + delta);
        for (i = 1; i < scene->table[0] + 1; i++) {
            if (scene->table[i] != 0) {
                scene->table[i] += delta;
            }
        }
    }
    return scene;
}

/* Load and relocate a scene file. */
SceneFile *func_8008B070(s32 file) {
    SceneFile *scene;

    func_800324B8(0xA);
    scene = func_80031BDC(func_80028738(file), 0);
    func_800295D8(file, scene, 0, 0);
    func_80028A60(0);
    return func_8008AF6C(scene);
}

/* Rewind every channel of a player. */
void func_8008B0D8(Player *player) {
    s32 unused; /* never used; the original frame has these bytes */
    Channel *channel;
    s32 i;

    player->unk10 = 0;
    player->frame = 0;
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++) {
        channel->hold = 0;
        channel->value = 0;
        channel->current = channel->start;
        channel->delta = 0;
        channel++;
    }
}

#ifdef NON_MATCHING
/* Bind an animation to a model set node: its constant keys and streamed
 * channels drive node angle (short way round) or 0x2C components.
 * Does not match: the data pointer's register copies (t1/s0/s1) differ. */
void func_8008B13C(u8 *data, Player *player, Node *root) {
    Node **nodes = ((ModelSet *)root->data)->nodes;
    AnimHeader *anim = (AnimHeader *)data;
    AnimRecord *record;
    Key *key;
    Channel *channel;
    Node *node;
    u32 i;

    player->header = anim;
    func_800324B8(0x10);
    key = func_80031BDC(anim->keys * sizeof(Key) + anim->channels * sizeof(Channel), 0);
    player->keys = key;
    record = anim->records;
    channel = player->channels = (Channel *)(key + anim->keys);
    for (i = 0; i < anim->keys; i++) {
        node = nodes[record->node];
        key->value = record->value;
        switch (record->kind & 0x7F) {
        case 3:
            key->target = &node->unk44.vx;
            key->angular = 1;
            break;
        case 4:
            key->target = &node->unk44.vy;
            key->angular = 1;
            break;
        case 5:
            key->target = &node->unk44.vz;
            key->angular = 1;
            break;
        case 6:
            key->target = &node->rotation.vx;
            key->angular = 0;
            break;
        case 7:
            key->target = &node->rotation.vy;
            key->angular = 0;
            break;
        case 8:
            key->target = &node->rotation.vz;
            key->angular = 0;
            break;
        }
        key++;
        record++;
    }
    for (i = 0; i < anim->channels; i++) {
        node = nodes[record->node];
        channel->current = data + record->value;
        channel->start = data + record->value;
        switch (record->kind & 0x7F) {
        case 3:
            channel->target = &node->unk44.vx;
            channel->angular = 1;
            break;
        case 4:
            channel->target = &node->unk44.vy;
            channel->angular = 1;
            break;
        case 5:
            channel->target = &node->unk44.vz;
            channel->angular = 1;
            break;
        case 6:
            channel->target = &node->rotation.vx;
            channel->angular = 0;
            break;
        case 7:
            channel->target = &node->rotation.vy;
            channel->angular = 0;
            break;
        case 8:
            channel->target = &node->rotation.vz;
            channel->angular = 0;
            break;
        }
        channel++;
        record++;
    }
    func_8008B0D8(player);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B13C);
#endif

#ifdef NON_MATCHING
/* Build a model set node tree from a model set file: a node per hierarchy
 * record (with its model, parent, angle and offset) and a player per
 * animation. Returns the root node. Does not match: register allocation
 * spills the models pointer where the original spills the animations. */
Node *func_8008B38C(ModelSetFile *file) {
    u32 i;
    u8 *models = file->models;
    u32 *hierarchy = file->hierarchy;
    u32 *animations = file->animations;
    u32 count = hierarchy[0];
    HierarchyRecord *records = (HierarchyRecord *)(hierarchy + 1);
    Node **nodes;
    ModelSet *set;
    Node *root;
    Node *node;
    Model *model;
    Player *player;

    func_8002C3E8(models);
    func_800324B8(0x12);
    nodes = func_80031BDC(count * 4, 0);
    set = func_80089E74();
    root = func_80089C54();
    func_80089E54(root, set);
    set->nodes = nodes;
    set->nodeCount = count;
    set->records = records;
    for (i = 0; i < count; i++) {
        node = nodes[i] = func_80089C54();
        if (records[i].model != -1) {
            model = func_80089FC4();
            func_80089E2C(node, model);
            func_8008A184(model, (ModelFile *)(models + records[i].model * 0x38 + 0x10));
        }
        func_80089C88(records[i].parent == -1 ? root : nodes[records[i].parent], node);
        node->unk44.vx = records[i].angle.vx;
        node->unk44.vy = records[i].angle.vy;
        node->unk44.vz = records[i].angle.vz;
        node->rotation.vx = records[i].offset[0];
        node->rotation.vy = records[i].offset[1];
        node->rotation.vz = records[i].offset[2];
    }
    if (animations != NULL) {
        func_800324B8(0x11);
        player = set->players = func_80031BDC(animations[0] * sizeof(Player), 0);
        set->count = animations[0];
        for (i = 0; i < animations[0]; i++) {
            if (animations[i + 1] != 0) {
                func_8008B13C((u8 *)animations[i + 1], player, root);
            } else {
                player->header = NULL;
            }
            player++;
        }
    }
    return root;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B38C);
#endif

/* Advance a player by some frames, snapping to the keys. */
s32 func_8008B5DC(Player *player, s32 frames) {
    return func_8008B730(player, frames, 1);
}

/* Ease an angle toward target by a fraction (1/steps) of the shorter way
 * round (angles are 12-bit). */
s16 func_8008B5FC(s32 angle, s32 target, s32 steps) {
    s32 diff;
    s16 result;

    angle &= 0xFFF;
    diff = (angle - target) & 0xFFF;
    result = angle;
    if (diff != 0) {
        if (diff < 0x800) {
            result = angle - diff / steps;
        } else {
            result = angle + (0x1000 - diff) / steps;
        }
    }
    return result;
}

/* Turn an angle toward target by a fixed step the shorter way round
 * (a random way when opposite), stopping on the target. */
s32 func_8008B650(s32 from, s32 to, s32 step) {
    s16 angle = from;
    s16 target = to;
    s32 diff = (from - to) & 0xFFF;

    if (diff != 0) {
        if (diff == 0x800 ? (func_8003FA38() & 1) : diff < 0x800) {
            angle -= step;
            if (((angle - target) & 0xFFF) > 0x800) {
                angle = target;
            }
        } else {
            angle += step;
            if (((angle - target) & 0xFFF) < 0x800) {
                angle = target;
            }
        }
    }
    return angle;
}

#ifdef NON_MATCHING
/* Advance a player by some frames (clamped to the animation's end) and
 * move every target 1/steps of the way to its key or channel value.
 * Channel streams hold a byte per frame: 0xxxxxxx a 7-bit delta, 10xxxxxx
 * hold the delta for x frames, 11xxxxxx plus a byte a 14-bit delta.
 * Returns whether the end was reached in a final step (1 without an
 * animation). Does not match: the stream byte is tested in its load
 * register rather than the copy, which shifts registers in the decoder. */
s32 func_8008B730(Player *player, s32 frames, s32 steps) {
    Key *key;
    Channel *channel;
    u8 *code;
    s8 value;
    s16 current;
    s32 i;
    s32 j;

    if (player->header == NULL) {
        return 1;
    }
    if (frames == 0) {
        return;
    }
    if (player->frame + frames > player->header->frames) {
        frames = player->header->frames - player->frame;
    }
    player->frame += frames;
    steps -= frames;
    if (steps <= 0) {
        steps = 1;
    }
    key = player->keys;
    if (steps == 1) {
        for (i = 0; i < player->header->keys; i++, key++) {
            *key->target = key->value;
        }
    } else {
        for (i = 0; i < player->header->keys; i++, key++) {
            current = *key->target;
            if (key->angular) {
                *key->target = func_8008B5FC(current, key->value, steps);
            } else {
                *key->target = current + (key->value - current) / steps;
            }
        }
    }
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++, channel++) {
        for (j = 0; j < frames; j++) {
            if (channel->hold) {
                channel->hold--;
            } else {
                code = channel->current++;
                value = *(s8 *)code;
                if (value & 0x80) {
                    if (value & 0x40) {
                        channel->current = code + 2;
                        channel->delta = (value & 0x3F) | ((s8)code[1] << 6);
                    } else {
                        channel->hold = value & 0x3F;
                    }
                } else {
                    channel->delta = (value << 25) >> 25;
                }
            }
            channel->value += channel->delta;
        }
        if (steps == 1) {
            *channel->target = channel->value;
        } else {
            current = *channel->target;
            if (channel->angular) {
                *channel->target = func_8008B5FC(current, channel->value, steps);
            } else {
                *channel->target = current + (channel->value - current) / steps;
            }
        }
    }
    if (player->frame == player->header->frames) {
        return steps == 1;
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B730);
#endif

/* Create a task running entry(arg) on its own stack of `words` words and
 * run it until it first yields. */
Task *func_8008BA2C(void (*entry)(s32), s32 arg, u32 *stack, s32 words) {
    Task *task;
    s32 i;

    func_800324B8(3);
    task = func_80031BDC(sizeof(Task), 2);
    for (i = 0; i < 32; i++) {
        task->regs[i] = 0;
    }
    task->stack = stack;
    task->regs[28] = func_800405E4();
    task->regs[31] = (u32)entry;
    task->regs[4] = arg;
    task->regs[30] = task->regs[29] = (u32)(task->stack + words);
    func_8008BB3C(task);
    return task;
}

/* Free a task. */
void func_8008BAE0(Task *task) {
    func_800320E8(task);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BC04);

/* Set the mesh light direction (a fixed down-left vector) and light a
 * mesh's vertices. */
void func_8008BCC8(Mesh *mesh, u8 *work) {
    Vector direction;
    Vector unused; /* never used; the original frame has these bytes */

    direction.vx = -8;
    direction.vy = -8;
    direction.vz = -0x10;
    func_80048D7C(&direction, &D_8009A2C8);
    D_8009A2C8.vx <<= 4;
    D_8009A2C8.vy <<= 4;
    D_8009A2C8.vz <<= 4;
    func_8008C3A8(mesh->data, work, mesh->count);
}

#ifdef NON_MATCHING
/* Draw a mesh's primitive groups (flag 8: quads, else triangles) into the
 * given packets and ordering table using the vertex work area. Does not match: the next-group pointer and
 * the primitive count swap registers (t0/v1 vs v1/a0). */
void func_8008BD70(Mesh *mesh, ModelPrim *prims, u32 *ot, u8 *work) {
    u8 *group;
    s32 groups = mesh->groups;
    u8 *next = mesh->groupData;

    D_80059424 = (s32)prims;
    D_80059568 = (s32)ot;
    D_8005953C = (s32)work;
    D_800595C0 += mesh->prims;
    while (D_80059528 = next, --groups != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            func_8008C620(D_80059528, ((s16 *)group)[1]);
        } else {
            func_8008C4B0(D_80059528, ((s16 *)group)[1]);
        }
        next = D_80059528 + ((s16 *)group)[1] * 8;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BD70);
#endif

/* Build a mesh's packet buffers: a vertex work area and, per display
 * buffer, a flat grey quad (0x18 bytes) or triangle (0x14 bytes) packet
 * for every primitive. */
void func_8008BE4C(ModelPrims *mp, Mesh *mesh) {
    s32 n; /* vertex, then group counter, then packet bytes per buffer */
    s32 i;
    s32 j;
    s32 triangles;
    s32 quads;
    u8 *group;
    u8 *vertex;
    u8 *packet;

    mp->vertices = mesh->count;
    mp->count = mesh->prims;
    mp->vertexData = mesh->data;
    mp->mesh = mesh;
    D_80059528 = mesh->groupData;
    func_800324B8(0x13);
    vertex = mp->work = func_80031BDC(mp->vertices * 8, 2);
    n = mp->vertices;
    while (--n != -1) {
        ((s16 *)vertex)[1] = 0;
        vertex += 8;
    }
    func_800324B8(5);
    triangles = 0;
    quads = 0;
    n = mesh->groups;
    while (--n != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            quads += ((s16 *)group)[1];
        } else {
            triangles += ((s16 *)group)[1];
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
    n = triangles * 0x14 + quads * 0x18;
    packet = func_80031BDC(n * 2, 2);
    mp->prims[0] = (ModelPrim *)packet;
    mp->prims[1] = (ModelPrim *)(packet + n);
    i = mesh->groups;
    D_80059528 = mesh->groupData;
    while (--i != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            j = ((s16 *)group)[1];
            while (--j != -1) {
                TAG_LEN(packet) = 5;
                ((u32 *)packet)[1] = 0x28403030;
                packet += 0x18;
            }
        } else {
            j = ((s16 *)group)[1];
            while (--j != -1) {
                TAG_LEN(packet) = 4;
                ((u32 *)packet)[1] = 0x20403030;
                packet += 0x14;
            }
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
    func_800732AC(mp->prims[1], mp->prims[0], n);
}

/* Make a node an instance node. */
void func_8008C0BC(Node *node, Instance *instance) {
    node->type = 5;
    node->data = instance;
}

/* Allocate an instance payload drawing source. */
Instance *func_8008C0CC(Node *source) {
    Instance *instance;

    func_800324B8(7);
    instance = func_80031BDC(sizeof(Instance), 2);
    instance->type = source->type;
    instance->unk8 = (s32)D_80092828;
    instance->source = source;
    instance->prims = NULL;
    return instance;
}

/* Free an instance payload's packet buffers. */
void func_8008C120(Instance *instance) {
    ModelPrims *prims = instance->prims;

    if (prims != NULL) {
        if (prims->work != NULL) {
            func_800320E8(prims->work);
        }
        if (prims->prims[0] != NULL) {
            func_80032C18(prims->prims[0], 2);
        }
        func_800320E8(prims);
    }
}

/* Copy a node tree as instance nodes (model sources get their own packet
 * buffers); following siblings are appended to parent. */
Node *func_8008C188(Node *source, Node *parent) {
    Node *node;
    Instance *instance;
    Mesh *mesh;

    func_800324B8(6);
    node = func_80089C54();
    instance = func_8008C0CC(source);
    func_8008C0BC(node, instance);
    if (instance->type == 1) {
        mesh = (Mesh *)((Model *)source->data)->file;
        func_800324B8(5);
        instance->prims = func_80031BDC(sizeof(ModelPrims), 2);
        func_8008BE4C(instance->prims, mesh);
    }
    D_80092824++;
    if (source->child != NULL) {
        func_80089C88(node, func_8008C188(source->child, node));
    }
    if (source->next != NULL) {
        func_80089C88(parent, func_8008C188(source->next, parent));
    }
    return node;
}

/* Copy a node tree as instances. */
Node *func_8008C298(Node *source) {
    D_80092824 = 0;
    return func_8008C188(source, NULL);
}

/* Copy a node tree as instances of itself. */
Node *func_8008C2C0(Node *source) {
    D_80092828 = source;
    return func_8008C298(source);
}

/* Draw a tree of instance nodes whose model sources are shown. */
void func_8008C2E8(Node *node) {
    Instance *instance = node->data;
    ModelPrims *prims;

    if (instance->type == 1 && !(((Model *)instance->source->data)->flags & 1)) {
        prims = instance->prims;
        func_8008BD70(prims->mesh, prims->prims[D_800928A0], D_800928E4 + 1, prims->work);
    }
    if (node->child != NULL) {
        func_8008C2E8(node->child);
    }
    if (node->next != NULL) {
        func_8008C2E8(node->next);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C3A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C4B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C620);

/* Shift a vector history: entries 4, 3 and 2 all take entry 0. */
void func_8008C7C0(SVector *history) {
    history[4] = history[0];
    history[3] = history[4];
    history[2] = history[3];
}

/* Set up a four-point spark line: semi-transparent, in the source colour,
 * the same in both draw buffers. */
void func_8008C828(SparkLine4 *spark, Emitter *source) {
    LineF4 *line = &spark->line[0];

    setlen(line, 6), setcode(line, 0x4C), line->pad = 0x55555555;
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a four-point spark line through its trail, age the trail and add
 * the line to the ordering table. */
void func_8008C8B4(SparkLine4 *spark, u32 *ot) {
    LineF4 *line = &spark->line[D_800928A0];
    s32 depth;
    s32 otz;

    otz = func_8004A73C(&spark->pos, &spark->trail[0], &spark->trail[1], &spark->trail[2],
                        (s32 *)&line->x0, (s32 *)&line->x1, (s32 *)&line->x2, (s32 *)&line->x3,
                        &depth, &depth);
    spark->trail[2] = spark->trail[1];
    spark->trail[1] = spark->trail[0];
    spark->trail[0] = spark->pos;
    func_80031750(ot + (otz >> 2), line);
}

/* Collapse a three-point spark line's trail onto its position. */
void func_8008C9B8(SparkLine3 *spark) {
    spark->trail[1] = spark->pos;
    spark->trail[0] = spark->trail[1];
}

/* Set up a three-point spark line. */
void func_8008CA00(SparkLine3 *spark, Emitter *source) {
    LineF3 *line = &spark->line[0];

    setlen(line, 5), setcode(line, 0x48), line->pad = 0x55555555;
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a three-point spark line relative to the view origin with the
 * GTE, age its trail and add it. */
void func_8008CA84(SparkLine3 *spark, u32 *ot) {
    SVector *origin = D_80092830;
    SVector *work = D_8009282C;
    LineF3 *line;
    s32 otz;

    work[0].vx = spark->pos.vx - origin->vx;
    work[0].vy = spark->pos.vy - origin->vy;
    work[0].vz = spark->pos.vz - origin->vz;
    work[1].vx = spark->trail[0].vx - origin->vx;
    work[1].vy = spark->trail[0].vy - origin->vy;
    work[1].vz = spark->trail[0].vz - origin->vz;
    work[2].vx = spark->trail[1].vx - origin->vx;
    work[2].vy = spark->trail[1].vy - origin->vy;
    work[2].vz = spark->trail[1].vz - origin->vz;
    gte_ldv3c(D_8009282C);
    gte_rtpt();
    line = &spark->line[D_800928A0];
    spark->trail[1] = spark->trail[0];
    spark->trail[0] = spark->pos;
    gte_stsxy3(&line->x0, &line->x1, &line->x2);
    gte_stszotz(&otz);
    func_80031708(ot + (otz >> 2), line);
}

/* Collapse a two-point spark line's trail onto its position. */
void func_8008CC2C(SparkLine2 *spark) {
    spark->trail[0] = spark->pos;
}

/* Set up a two-point spark line. */
void func_8008CC54(SparkLine2 *spark, Emitter *source) {
    LineF2Tag *line = &spark->line[0];

    setlen(line, 3), setcode(line, 0x40);
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a two-point spark line, age its trail and add it. */
void func_8008CCB0(SparkLine2 *spark, u32 *ot) {
    LineF2Tag *line = &spark->line[D_800928A0];
    s32 depth;
    s32 otz;

    otz = func_8004A67C(&spark->pos, &spark->trail[0], &depth, (s32 *)&line->x0,
                        (s32 *)&line->x1, &depth, &depth, &depth);
    spark->trail[0] = spark->pos;
    func_800316C0(ot + (otz >> 2), line);
}

void func_8008CD54(void) {
}

/* Set up a spark drawn as a small semi-transparent tile of random size. */
void func_8008CD5C(SparkTile *spark, Emitter *source) {
    TileRgb *tile = &spark->tile[0];

    setlen(tile, 3), setcode(tile, 0x62);
    tile->h = func_8003FA38() % 2 + 2;
    tile->w = tile->h * 2;
    setRGB0(tile, source->r, source->g, source->b);
    spark->tile[1] = spark->tile[0];
}

/* Project a tile spark relative to the view origin and add it. */
void func_8008CE0C(SparkTile *spark, u32 *ot) {
    SVector *origin = D_80092830;
    SVector v;
    TileRgb *tile;
    s32 otz;

    v.vx = spark->pos.vx - origin->vx;
    v.vy = spark->pos.vy - origin->vy;
    v.vz = spark->pos.vz - origin->vz;
    gte_ldv0(&v);
    gte_rtps();
    tile = &spark->tile[D_800928A0];
    gte_stsxy(&tile->x0);
    gte_stszotz(&otz);
    func_80031804(ot + (otz >> 2), tile);
}

void func_8008CED4(void) {
}

/* Set up a spark drawn as a single semi-transparent dot. */
void func_8008CEDC(SparkDot *spark, Emitter *source) {
    Tile1Tag *dot = &spark->dot[0];

    setlen(dot, 2), setcode(dot, 0x6A);
    setRGB0(dot, source->r, source->g, source->b);
    spark->dot[1] = spark->dot[0];
}

/* Project a dot spark and add it. */
void func_8008CF30(SparkDot *spark, u32 *ot) {
    Tile1Tag *dot = &spark->dot[D_800928A0];
    s32 depth;

    func_80031870(ot + (func_8004A64C(&spark->pos, (s32 *)&dot->x0, &depth, &depth) >> 2), dot);
}

/* Place a spark at its source's origin. */
void func_8008CF9C(Emitter *source, SVector *pos) {
    *pos = source->origin;
}

/* Place a spark at a random point of its source's box, rotated with the
 * source. */
void func_8008CFC4(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;

    v.vx = func_8003FA38() % source->range.vx - source->offset.vx;
    v.vy = func_8003FA38() % source->range.vy - source->offset.vy;
    v.vz = func_8003FA38() % source->range.vz - source->offset.vz;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Place a spark at a random point of its source's box. */
void func_8008D0A4(Emitter *source, SVector *pos) {
    pos->vx = source->origin.vx + func_8003FA38() % source->range.vx - source->offset.vx;
    pos->vy = source->origin.vy + func_8003FA38() % source->range.vy - source->offset.vy;
    pos->vz = source->origin.vz + func_8003FA38() % source->range.vz - source->offset.vz;
}

/* Place a spark at a random point of its source's horizontal rectangle,
 * rotated with the source. */
void func_8008D14C(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;

    v.vx = func_8003FA38() % source->range.vx - source->offset.vx;
    v.vy = 0;
    v.vz = func_8003FA38() % source->range.vz - source->offset.vz;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Place a spark at a random point of its source's horizontal ellipse,
 * rotated with the source. */
void func_8008D208(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;
    s32 angle = func_8003FA38();
    s32 radius = func_8003FA38();

    v.vx = (func_8003F8B0(angle) * (radius % source->range.vx)) >> 13;
    v.vy = 0;
    v.vz = (func_8003F8CC(angle) * (radius % source->range.vz)) >> 13;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Place a spark on its source's ring at a random height, rotated with the
 * source. The ring angle is never initialised in the original. */
void func_8008D304(Emitter *source, SVector *pos) {
    SVector v;
    Vector r;
    s32 angle;

    v.vx = (func_8003F8B0(angle) * source->range.vx) >> 12;
    v.vy = func_8003FA38() % source->range.vy - source->range.vy / 2;
    v.vz = (func_8003F8CC(angle) * source->range.vz) >> 12;
    func_800495DC(&v, &r);
    pos->vx = source->origin.vx + r.vx;
    pos->vy = source->origin.vy + r.vy;
    pos->vz = source->origin.vz + r.vz;
}

/* Create an emitter of the given spark shape and placement rule: unit
 * spread centred on the origin, white, no sparks yet. */
Emitter *func_8008D3F4(s32 shape, s32 placement) {
    Emitter *emitter;
    SparkShape *kind;

    func_800324B8(0x15);
    emitter = func_80031BDC(0x7C, 0);
    emitter->range.vx = 0x1000;
    emitter->range.vy = 0x1000;
    emitter->range.vz = 0x1000;
    emitter->spread = 1;
    emitter->placement = placement;
    emitter->unk2 = 0;
    emitter->unk4 = 0;
    emitter->unk6 = 0;
    emitter->base.vx = 0;
    emitter->base.vy = 0;
    emitter->base.vz = 0;
    emitter->angles.vx = 0;
    emitter->angles.vy = 0;
    emitter->angles.vz = 0;
    emitter->turn.vx = 0;
    emitter->turn.vy = 0;
    emitter->turn.vz = 0;
    emitter->gravity = 0;
    emitter->unk46 = 0;
    emitter->speed = 0x100;
    emitter->speed_range = 0x100;
    emitter->offset.vx = emitter->range.vx / 2;
    emitter->offset.vy = emitter->range.vy / 2;
    emitter->offset.vz = emitter->range.vz / 2;
    emitter->place = D_80091CC4[(s16)placement];
    emitter->update = D_80091CDC[0];
    emitter->sparks = NULL;
    emitter->shape = shape;
    emitter->unk64 = 0;
    emitter->life = 100;
    emitter->r = 0xFF;
    emitter->g = 0xFF;
    emitter->b = 0xFF;
    emitter->unk68 = 0;
    kind = &D_80091C74[emitter->shape];
    emitter->size = kind->size;
    emitter->reset = kind->reset;
    emitter->draw = kind->draw;
    emitter->setup = kind->setup;
    return emitter;
}

/* Mark every spark of an emitter for restart. */
void func_8008D580(Emitter *emitter) {
    u8 *spark = emitter->sparks;
    s32 i;

    for (i = 0; i < emitter->count; i++) {
        ((SVector *)spark)->pad = 0;
        spark += emitter->size;
    }
}

#ifdef NON_MATCHING
/* (Re)allocate an emitter's pool for count sparks and set each one up.
 * Does not match: the emitter and loop counter swap $s1/$s2. */
void func_8008D5C0(Emitter *emitter, s32 count) {
    u8 *spark;
    void (*setup)(void *, Emitter *);
    s32 i;

    if (emitter->sparks != NULL) {
        func_80032C18(emitter->sparks, 3);
    }
    emitter->count = count;
    func_800324B8(0x14);
    spark = func_80031BDC(emitter->size * emitter->count, 0);
    emitter->sparks = spark;
    setup = emitter->setup;
    for (i = 0; i < emitter->count; i++) {
        setup(spark, emitter);
        ((SVector *)spark)->pad = 0;
        spark += emitter->size;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D5C0);
#endif

/* Launch up to count idle sparks: each gets a random direction inside the
 * emitter's spread cone and a random speed, both rotated into place, then a
 * position from the placement rule, the emitter's life and a fresh shape. */
void func_8008D680(Emitter *emitter, Matrix *rotation, s32 count) {
    SVector dir;
    SVector unit;
    Matrix local;
    Matrix world;
    Matrix turned;
    u8 *spark;
    void (*place)(Emitter *, SVector *);
    void (*reset)(void *);
    s32 left;
    s32 i;
    s32 angle;
    s32 heading;

    reset = emitter->reset;
    place = emitter->place;
    world = *rotation;
    emitter->origin.vx = emitter->base.vx + rotation->t[0];
    emitter->origin.vy = emitter->base.vy + rotation->t[1];
    emitter->origin.vz = emitter->base.vz + rotation->t[2];
    func_8003F738(&emitter->angles, &local);
    func_80049ACC(&world, &local);
    func_8003F738(&emitter->turn, &turned);
    func_80049ACC(&turned, &world);
    func_80049EFC(&turned);
    left = count;
    spark = emitter->sparks;
    for (i = 0; i < emitter->count; i++) {
        if (((Spark *)spark)->pos.pad == 0) {
            if (--left == -1) {
                break;
            }
            angle = func_8003FA38() % emitter->spread;
            heading = func_8003FA38();
            dir.vy = -func_8003F8CC(angle);
            angle = func_8003F8B0(angle);
            dir.vx = (func_8003F8B0(heading) * angle) >> 12;
            dir.vz = (func_8003F8CC(heading) * angle) >> 12;
            heading = emitter->speed + func_8003FA38() % emitter->speed_range; /* now the speed */
            gte_ldv0(&dir);
            gte_rtv0();
            gte_stsv(&unit);
            gte_lddp(heading);
            gte_ldsv(&unit);
            gte_gpf12();
            gte_stsv(&((Spark *)spark)->vel);
        }
        spark += emitter->size;
    }
    func_80049EFC(&world);
    left = count;
    spark = emitter->sparks;
    for (i = 0; i < emitter->count; i++) {
        if (((Spark *)spark)->pos.pad == 0) {
            if (--left == -1) {
                break;
            }
            place(emitter, (SVector *)spark);
            ((Spark *)spark)->pos.pad = emitter->life;
            reset(spark);
        }
        spark += emitter->size;
    }
}

/* Bounce a falling spark off the floor under it, losing half its speed.
 * The floor query reads the spark position as a 32-bit vector. */
void func_8008D980(Spark *spark) {
    if (spark->vel.vy > 0 && spark->pos.vy > func_80082488((Vector *)spark, 0)) {
        spark->vel.vy = -spark->vel.vy / 2;
    }
}

/* Bounce a spark off the ground plane (y = 0), losing half its speed;
 * a spark that has come to rest dies. */
void func_8008D9F0(Spark *spark) {
    if (spark->pos.vy > 0) {
        spark->vel.vy = -spark->vel.vy / 2;
        if (abs(spark->vel.vy) < 8) {
            spark->pos.pad = 0;
        }
    }
}

/* Move and draw every live spark of an emitter: gravity, a bounce on the
 * ground plane, projection relative to the camera through the scratchpad. */
void func_8008DA48(Emitter *emitter, u32 *ot, Matrix *view) {
    Matrix unused_matrix;
    SVector unused_vector;
    Spark *spark;
    void (*draw)(void *, u32 *);
    s32 i;

    D_8009282C = (SVector *)0x1F800000;
    D_80092830 = (SVector *)0x1F800030;
    ((SVector *)0x1F800030)->vx = D_80096FA8.vx;
    ((SVector *)0x1F800030)->vy = D_80096FA8.vy;
    ((SVector *)0x1F800030)->vz = D_80096FA8.vz;
    spark = (Spark *)emitter->sparks;
    draw = emitter->draw;
    for (i = 0; i < emitter->count; i++) {
        if (spark->pos.pad != 0) {
            spark->pos.pad--;
            spark->vel.vy += emitter->gravity;
            spark->pos.vx += spark->vel.vx;
            spark->pos.vy += spark->vel.vy;
            spark->pos.vz += spark->vel.vz;
            if (spark->pos.vy > 0) {
                spark->vel.vy = -spark->vel.vy * 2 / 3;
                if (abs(spark->vel.vy) < 4) {
                    spark->pos.pad = 0;
                }
            }
            draw(spark, ot);
        }
        spark = (Spark *)((u8 *)spark + emitter->size);
    }
}

/* Copy one model part's local transform. */
void func_8008DBC0(SparkModel *model, s16 part, Matrix *out) {
    Matrix unused;

    *out = model->list->parts[part]->matrix;
}

/* Create the menu's spark emitter: 256 orange three-point sparks. */
void func_8008DC28(void) {
    Emitter *emitter = func_8008D3F4(1, 0);

    emitter->r = 0xFF;
    emitter->g = 0xA0;
    emitter->b = 0x70;
    func_8008D5C0(emitter, 0x100);
    emitter->gravity = 4;
    emitter->spread = 0x60;
    emitter->speed_range = 0x60;
    emitter->speed = 4;
    emitter->unk68 = 0;
    emitter->life = 0x20;
    D_80092834 = emitter;
}

/* Start a spark burst of the given strength. */
void func_8008DCA8(s32 strength) {
    D_80092838 = strength;
}

/* Emit a burst from a model part while the burst lasts, then move and draw
 * the menu's sparks under the given view. */
void func_8008DCB8(u32 *ot, SparkModel *model, Matrix *view, Vector *pos) {
    Emitter *emitter = D_80092834;
    Matrix rotation;
    Matrix part;

    if (D_80092838 >= 0x10) {
        func_8008DBC0(model, 0x27, &part);
        func_80048E94(&part, &rotation);
        rotation.t[0] = rotation.t[1] = rotation.t[2] = 0;
        emitter->base.vx = pos->vx;
        emitter->base.vy = pos->vy;
        emitter->base.vz = pos->vz;
        emitter->turn.vx = 0;
        emitter->turn.vy = 0;
        emitter->turn.vz = 0;
        emitter->angles.vx = 0;
        emitter->angles.vy = 0;
        emitter->angles.vz = 0x800;
        func_8008D680(emitter, &rotation, D_80092838 >> 4);
        D_80092838 -= 4;
    }
    gte_SetTransMatrix(view);
    gte_SetRotMatrix(view);
    func_8008DA48(emitter, ot, view);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DDFC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DE54);

/* Forget the three glow buffers. */
void func_8008DF30(void) {
    D_80092844 = NULL;
    D_8009283C = NULL;
    D_80092840 = NULL;
}

/* Allocate the glow buffers once, clear them, and upload the glow palette
 * with every entry marked semi-transparent. */
void func_8008DF50(void) {
    Rect rect;
    s32 i;

    if (D_80092844 == NULL) {
        D_80092844 = func_80031BDC(0x1500, 1);
        D_8009283C = func_80031BDC(0x2BC0, 1);
        D_80092840 = func_80031BDC(0x2BC0, 1);
    }
    for (i = 0x1570; i != -1; i--) {
        D_80092840[i] = 0;
        D_8009283C[i] = 0;
    }
    for (i = 0; i < 0x1500; i++) {
        D_80092844[i] = 0;
    }
    for (i = 0; i < 0x100; i++) {
        D_80091CE0[i] |= 0x8000;
    }
    rect.y = 0x1FD;
    rect.w = 0xFF;
    rect.x = 0;
    rect.h = 1;
    func_80044894(&rect, (u32 *)D_80091CE0);
}

/* Release the glow buffers. */
void func_8008E064(void) {
    if (D_80092844 != NULL) {
        func_80032C18(D_80092844, 2);
        func_80032C18(D_8009283C, 2);
        func_80032C18(D_80092840, 2);
        D_80092844 = NULL;
        D_8009283C = NULL;
        D_80092840 = NULL;
    }
}

/* Copy the new glow field over the old one and pack every word's low bytes
 * of both halves into the byte field. */
void func_8008E0C8(void) {
    u8 *bytes = D_80092844;
    u32 *old = (u32 *)D_8009283C;
    u32 *new = (u32 *)D_80092840;
    u32 value;
    s32 i;

    for (i = 0xA7F; i != -1; i--) {
        value = *new++;
        *old++ = value;
        *bytes++ = value;
        *bytes++ = value >> 16;
    }
}

#ifdef NON_MATCHING
/* Advance the glow field one step: seed the two bottom rows with random
 * heat, let every cell take the cooled average of its neighbours below,
 * then keep the result for the next step.
 * Does not match: GCC does not strength-reduce the five neighbour loads
 * into separate pointers as the original does. */
void func_8008E120(void) {
    s16 *new;
    s16 *old;
    s16 *seed;
    s32 heat;
    s32 value;
    s32 row;
    s32 i;
    s32 x;
    s32 y;

    if (D_80092844 != NULL) {
        heat = 0;
        new = D_80092840;
        seed = &new[47 * 0x70];
        for (x = 0; x < 0x70; x++) {
            switch (func_8003FA38() & 3) {
            case 0:
                heat = 0x180;
                break;
            case 1:
                heat = 0;
                break;
            }
            seed[x] = seed[x + 0x70] = heat;
        }
        old = D_8009283C;
        for (y = 0x2F; y > 1; y--) {
            for (x = 1; x < 0x70; x++) {
                value = (old[(y - 1) * 0x70 + x] + old[y * 0x70 + x + 1] + old[y * 0x70 + x - 1] +
                         old[(y + 1) * 0x70 + x + 1] + old[(y + 1) * 0x70 + x - 1]) /
                        5;
                if (value > 3) {
                    value -= 3;
                }
                new[(y - 1) * 0x70 + x] = value;
            }
        }
        func_8008E0C8();
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E120);
#endif

/* Draw a full-screen grey tile of the given level, additive or subtractive,
 * with the draw mode that selects the blend. */
void func_8008E2B8(u32 *ot, s32 level, s32 subtract) {
    TileRgb *tile = &D_80096DE0[D_800928A0];

    *(u32 *)&tile->r0 = level | (level << 8) | (level << 16) | 0x60000000;
    setlen(tile, 3);
    *(u32 *)&tile->x0 = 0;
    *(u32 *)&tile->w = 0xDA0140;
    setSemiTrans(tile, 1);
    func_80043B48(ot, tile);
    if (subtract) {
        func_800454DC(&D_80096E00[D_800928A0], 0, 1, func_80043A1C(0, 2, 0, 0), NULL);
    } else {
        func_800454DC(&D_80096E00[D_800928A0], 0, 1, func_80043A1C(0, 1, 0, 0), NULL);
    }
    func_80043B48(ot, &D_80096E00[D_800928A0]);
}

/* Draw the glow field: upload its byte image and stretch it over the
 * screen as a semi-transparent textured quad at two thirds of the level
 * (plain texture at full level); optionally add a brightening tile. */
void func_8008E3CC(u32 *ot, s32 level, s32 brighten) {
    PolyFT4 *quad = &D_80096D90[D_800928A0];
    TileRgb *tile;
    Rect rect;
    s32 shade;

    setlen(quad, 9);
    shade = level * 2 / 3;
    *(u32 *)&quad->r0 = shade | (shade << 8) | (shade << 16) | 0x2C000000;
    setShadeTex(quad, shade == 0x80);
    *(u32 *)&quad->x0 = 0;
    *(u32 *)&quad->x1 = 0x140;
    *(u32 *)&quad->x2 = 0xDA0000;
    *(u32 *)&quad->x3 = 0xDA0140;
    *(u16 *)&quad->u0 = 0;
    *(u16 *)&quad->u1 = 0x6F;
    *(u16 *)&quad->u2 = 0x2A00;
    *(u16 *)&quad->u3 = 0x2A6F;
    setSemiTrans(quad, 1);
    quad->tpage = func_80043A1C(1, 1, 0x140, 0x100);
    quad->clut = func_80043A58(0, 0x1FD);
    func_80043B48(ot, quad);
    rect.x = 0x140;
    rect.y = 0x100;
    rect.w = 0x38;
    rect.h = 0x2B;
    func_80044894(&rect, (u32 *)D_80092844);
    if (brighten) {
        tile = &D_80096DE0[D_800928A0];
        if (level > 0x80) {
            shade = level * 2;
            *(u32 *)&tile->r0 = shade | (shade << 8) | (shade << 16) | 0x60000000;
            setlen(tile, 3);
            *(u32 *)&tile->x0 = 0;
            *(u32 *)&tile->w = 0xDA0140;
            setSemiTrans(tile, 1);
            func_80043B48(ot, tile);
        }
    }
    func_800454DC(&D_80096E00[D_800928A0], 0, 1, func_80043A1C(0, 2, 0, 0), NULL);
    func_80043B48(ot, &D_80096E00[D_800928A0]);
}

/* Reset the sound driver and the four positional voices. */
void func_8008E620(void) {
    s32 mask = 0x300;
    SoundVoice *voice;
    u32 i;

    func_80039FF8();
    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        voice->mask = mask;
        mask <<= 2;
        voice->active = 0;
        voice->age = 0;
        voice->voice = i * 2;
    }
}

/* Age every positional voice (saturating). */
void func_8008E67C(void) {
    if (D_80096EA0[0].age != 0xFFFF) {
        D_80096EA0[0].age++;
    }
    if (D_80096EA0[1].age != 0xFFFF) {
        D_80096EA0[1].age++;
    }
    if (D_80096EA0[2].age != 0xFFFF) {
        D_80096EA0[2].age++;
    }
    if (D_80096EA0[3].age != 0xFFFF) {
        D_80096EA0[3].age++;
    }
}

/* Choose a character's command sound table by its model kind. */
void func_8008E6F8(Actor *owner) {
    switch (owner->kind) {
    case 9:
        owner->sounds = D_80091FA0;
        break;
    case 0x1D:
        owner->sounds = D_80091F60;
        break;
    case 0x1B:
        owner->sounds = D_80091F80;
        break;
    case 0x24:
        owner->sounds = D_80091F70;
        break;
    default:
        owner->sounds = D_80091F90;
        break;
    }
}

#ifdef NON_MATCHING
/* Start a sound on a free positional voice (or a matching unpositioned
 * one, else the oldest); positioned sounds follow pos or its snapshot.
 * Does not match: the search pointer and the age temporary swap $v1/$a0. */
void func_8008E78C(s32 sound, s32 mode, Vector *pos, s32 arg3) {
    s32 oldest = 0;
    SoundVoice *chosen = &D_80096EA0[3];
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (!voice->active || (mode == 0 && voice->mode == 0)) {
            chosen = voice;
            break;
        }
        if (oldest < voice->age) {
            oldest = voice->age;
            chosen = voice;
        }
    }
    func_8008E67C();
    voice = chosen;
    voice->mode = mode;
    voice->sound = sound;
    voice->active = 1;
    voice->unk3 = arg3;
    voice->follow = pos;
    if (pos != NULL) {
        voice->pos = *pos;
    }
    voice->age = 0;
    if (mode == 0) {
        func_80039F9C(voice->sound, voice->voice, 0x7F, 0x40);
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E78C);
#endif

/* Pan and attenuate every positioned voice from its screen position and
 * depth; a voice just started is keyed on with those values. */
void func_8008E8B0(void) {
    SVector v;
    SVector screen;
    s32 sz;
    SoundVoice *voice;
    s32 volume;
    s32 x;
    s32 pan;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (voice->active && voice->mode != 0) {
            if (voice->mode == 1) {
                v.vx = voice->pos.vx;
                v.vy = voice->pos.vy;
                v.vz = voice->pos.vz;
            } else {
                v.vx = voice->follow->vx;
                v.vy = voice->follow->vy;
                v.vz = voice->follow->vz;
            }
            v.vx -= D_80096FA8.vx;
            v.vy -= D_80096FA8.vy;
            v.vz -= D_80096FA8.vz;
            gte_ldv0(&v);
            gte_rtps();
            gte_stsxy(&screen);
            gte_stsz(&sz);
            x = screen.vx;
            if (x < 0) {
                x = 0;
            }
            if (x > 0x140) {
                x = 0x140;
            }
            volume = (0x3000 - sz) * 0x7F / 0x3000;
            if (volume < 0x28) {
                volume = 0x28;
            }
            if (volume > 0x7F) {
                volume = 0x7F;
            }
            pan = x * 0x7F / 0x140;
            if (voice->age != 0) {
                func_8003A55C(voice->voice, pan);
                func_8003A344(voice->voice, volume);
            } else {
                func_80039F9C(voice->sound, voice->voice, volume, pan);
            }
        }
    }
    func_8008E67C();
}

/* Free the voices whose sound has stopped, then age them all. */
void func_8008EADC(void) {
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (!(func_8003A5D0(voice->sound) & voice->mask)) {
            voice->active = 0;
        }
    }
    func_8008E67C();
}

#ifdef NON_MATCHING
/* Play a menu sound effect (unpositioned).
 * Does not match: the tag load is scheduled after the sound id. */
void func_8008EB4C(s32 id) {
    if (id != 0) {
        func_8008E78C(0x60000 + id, 0, NULL, D_80059488);
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EB4C);
#endif

/* Play a character's sound effect, tagged with its id and side. */
void func_8008EB88(Actor *owner, s32 id, Vector *pos, s32 mode) {
    if (id != 0) {
        func_8008E78C(id + 0x60000, mode, pos, (id & 0x7F) | ((owner->flags >> 20) & 0x80));
    }
}

#ifdef NON_MATCHING
/* Play one of a character's command sounds (random 1-6 when index is 0):
 * up to two effects from the shared pair table.
 * Does not match: the original keeps the pair table address in
 * a saved register for the second id. */
void func_8008EBD0(Actor *owner, s32 index, Vector *pos, s32 mode) {
    s32 entry;

    if (index == 0) {
        index = func_8003FA38() % 6 + 1;
    }
    entry = owner->sounds[index];
    if (entry != 0xFF) {
        index = D_80091EE0[entry].first;
        if (index != 0) {
            func_8008E78C(index | 0x60000, mode, pos, (index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
        index = D_80091EE0[entry].second;
        if (index != 0) {
            func_8008E78C(index | 0x60000, mode, pos, (index & 0x7F) | ((owner->flags >> 20) & 0x80));
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EBD0);
#endif

/* Stop every voice started with the given tag. */
void func_8008ECEC(u8 tag) {
    SoundVoice *voice;
    s32 i;

    for (i = 0; i < 4; i++) {
        voice = &D_80096EA0[i];
        if (voice->active && voice->unk3 == tag) {
            func_8003A20C(voice->voice);
            voice->active = 0;
        }
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ED6C);

/* Accelerate an actor toward the speed limit (or brake to a stop, harder
 * when not guarding) for two ticks, and turn it toward a heading. */
void func_8008EE1C(Actor *actor, s16 heading, s16 limit) {
    Vector unused;
    s32 brake = actor->brake;
    s32 accel = actor->accel;
    s32 moving;
    s32 i;

    if (actor->flags & 0x100) {
        brake = brake * 2 / 3;
        moving = 0;
    } else {
        moving = 1;
    }
    for (i = 0; i < 2; i++) {
        if (limit != 0 && moving) {
            actor->state += accel;
            if (limit < actor->state) {
                actor->state = limit;
            }
        } else {
            actor->state -= brake;
            if (actor->state < 0) {
                actor->state = 0;
            }
        }
    }
    actor->target_angle = func_8008B650(actor->target_angle, heading, 0x40);
    actor->unkCE = 0;
}

/* Opponent command: act, then wait a second. */
void func_8008EF00(Actor *actor, Brain *brain) {
    func_800767C8(actor);
    brain->timer = 0x3C;
}

/* Opponent command: act, then wait longer when told to. */
void func_8008EF30(Actor *actor, Brain *brain, s32 long_wait) {
    func_8008FE80(actor);
    brain->timer = long_wait ? 0x1E : 0xA;
}

/* Opponent command: input 3, then wait a second. */
void func_8008EF74(Actor *actor, Brain *brain) {
    func_8007639C(actor, 3);
    brain->timer = 0x3C;
}

/* Opponent command: toggle guarding. */
void func_8008EFA8(Actor *actor, Brain *brain) {
    brain->defending ^= 1;
    if (brain->defending) {
        actor->flags |= 2;
        brain->timer = 0x3C;
    } else {
        actor->flags &= ~2;
        actor->flags &= ~0x38;
        brain->timer = 0x1E;
    }
}

/* Opponent command: inputs 4 and 3, then wait a second. */
void func_8008F014(Actor *actor, Brain *brain) {
    func_8007639C(actor, 4);
    func_8007639C(actor, 3);
    brain->timer = 0x3C;
}

/* Opponent command: input 4, then wait a second. */
void func_8008F060(Actor *actor, Brain *brain) {
    func_8007639C(actor, 4);
    brain->timer = 0x3C;
}

/* Opponent roaming: while far away keep deciding every frame; otherwise
 * pick a new random heading and duration when the timer runs out. */
void func_8008F094(Actor *actor, Brain *brain) {
    if (D_8009284C > 0x800) {
        brain->timer = 1;
        brain->unkC = 0;
    } else if (--brain->timer == -1) {
        brain->unkA = func_8003FA38() % 0x600 + 0x500;
        brain->timer = func_8003FA38() % 50 + 10;
        brain->unkC = 0xFF;
    }
}

/* Opponent circling: while very close keep deciding every frame; otherwise
 * pick a new random turn and duration when the timer runs out. */
void func_8008F17C(Actor *actor, Brain *brain) {
    if (D_8009284C < 0x100) {
        brain->timer = 1;
        brain->unkC = 0;
    } else if (--brain->timer == -1) {
        brain->unkA = func_8003FA38() % 0x600 - 0x300;
        brain->timer = func_8003FA38() % 120 + 10;
        brain->unkC = 0xFF;
    }
}

/* Opponent command: store its argument, then run the mode's step. */
void func_8008F260(Actor *actor, Brain *brain, u8 arg) {
    brain->unkF = arg;
    func_80090E10(actor);
}

#ifdef NON_MATCHING
/* Drive the computer opponent one frame: reset its state when the command
 * changes, count down to the next decision (some commands decide every
 * frame), run the command and then steer and accelerate.
 * Does not match: the command-change test and the stored command are
 * scheduled/reloaded differently around the stores. */
void func_8008F280(Actor *actor) {
    Brain *brain = actor->brain;

    D_80099D98.driven = 1;
    if (D_80092848 != D_80099D98.command) {
        brain->timer = 0;
        brain->unkC = 0;
        actor->flags &= ~2;
        actor->state = 0;
        actor->flags &= ~0x38;
        brain->defending = 0;
        D_80092848 = D_80099D98.command;
        actor->unkCE = actor->unkCC + 0x800;
    }
    if (brain->defending) {
        actor->flags |= 2;
    }
    switch (D_80099D98.command) {
    case 2:
    case 8:
    case 9:
    case 11:
    case 12:
    case 13:
        break;
    default:
        if (--brain->timer != -1) {
            return;
        }
        break;
    }
    switch (D_80099D98.command) {
    case 3:
        func_8008EF30(actor, brain, 0);
        break;
    case 4:
        func_8008EF30(actor, brain, 1);
        break;
    case 5:
        func_8008EF74(actor, brain);
        break;
    case 6:
        func_8008F014(actor, brain);
        break;
    case 7:
        func_8008F060(actor, brain);
        break;
    case 9:
        func_8008F094(actor, brain);
        break;
    case 8:
        func_8008F17C(actor, brain);
        break;
    case 10:
        func_8008EF00(actor, brain);
        break;
    case 11:
        func_8008F260(actor, brain, 0);
        return;
    case 12:
        func_8008F260(actor, brain, 1);
        return;
    case 13:
        func_8008F260(actor, brain, 2);
        return;
    case 2:
        D_80099D98.driven = 0;
        return;
    case 1:
        func_8008EFA8(actor, brain);
        break;
    case 0:
    default:
        brain->timer = 1;
        break;
    }
    func_8008EE1C(actor, brain->unkA, brain->unkC);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F280);
#endif

/* Whether an actor's hp is still above the given fraction (of 255) of
 * its maximum. */
s32 func_8008F4F4(Actor *actor, s32 fraction) {
    return actor->max_hp * fraction / 255 < actor->hp;
}

/* Whether an actor lacks the charge for its special move (or, with a
 * flag, whether spending it is allowed). */
s32 func_8008F530(Actor *actor, s32 check) {
    if (check) {
        return func_80073DE4(actor, actor->unkBE);
    }
    return actor->unkB6 < 0x1000 - actor->unkBE;
}

/* The charge left over after a special move. */
s32 func_8008F570(Actor *actor, Brain *brain) {
    return 0x1000 - actor->unkBE;
}

/* Compare an actor's charge with the level its brain waits for: 2 while
 * well below, else 1 up to the level and 0 above it. */
s32 func_8008F580(Actor *actor) {
    Brain *brain = actor->brain;

    if (brain->unk24 - 0x200 >= actor->unkB6) {
        return 2;
    }
    return !(brain->unk24 < actor->unkB6);
}

/* Decide whether the opponent attacks now, weighing its eagerness, its
 * charge and hp and the other actor's hp. */
s32 func_8008F5B4(Actor *actor, s32 unused) {
    Brain *brain = actor->brain;

    if ((func_8003FA38() & 0xFF) < (brain->unk10 * 320) >> 4) {
        if (func_8008F530(actor, 0)) {
            goto press;
        }
        if (func_8008F4F4(actor, 0xC0)) {
            goto press;
        }
        if ((func_8003FA38() & 0xFF) < (brain->unk1C * 192) >> 4) {
            goto press;
        }
        if (!func_8008F4F4(actor, 0x80) || actor->opponent->hp >= actor->hp) {
            return 0;
        }
    } else if ((func_8003FA38() & 0xFF) >= (brain->unk1C * 320) >> 4) {
        return 0;
    }
press:
    if ((func_8003FA38() & 0xFF) < brain->unk18) {
        if (actor->opponent->unkC4 == 4) {
            return 0;
        }
        if (func_8008F4F4(actor->opponent, 0x20)) {
            return 1;
        }
        if (actor->opponent->hp < actor->hp) {
            return 0;
        }
    }
    return 1;
}

/* Decide whether the opponent closes in: an eager opponent that is already
 * near holds back; otherwise it follows its charge or its eagerness. */
s32 func_8008F720(Actor *actor, s32 eager) {
    Brain *brain = actor->brain;

    if (eager && (func_8003FA38() & 0xFF) < brain->unk10 && D_8009284C < 0x600) {
        return 0;
    }
    if (func_8008F580(actor)) {
        return 1;
    }
    return (func_8003FA38() & 0xFF) < brain->unk1C;
}

/* Roll the opponent's choices for the next round from its tendencies. */
void func_8008F7B8(Brain *brain) {
    brain->unk2C_9 = (func_8003FA38() & 0xFF) < brain->unk10;
    brain->unk2C_10 = (func_8003FA38() & 0xFF) < brain->unk14;
    brain->unk2C_12 = func_8003FA38() & 1;
    brain->unk2C_11 = (func_8003FA38() & 0xFF) < brain->unk18;
    brain->unk2C_8 = (func_8003FA38() & 0xFF) < brain->unk10 && func_8003FA38() % 10 < 3;
    brain->roll = func_8003FA38();
    brain->unk30 = brain->owner->unk1668;
}

/* Opponent jump attack: unless the other actor is airborne (then only one
 * time in four), act or jump and attack. */
void func_8008F900(Actor *actor) {
    if ((actor->opponent->flags & 0x60000000) != 0x20000000 || (func_8003FA38() & 3) == 0) {
        if (D_80092884) {
            func_800767C8(actor);
            func_8007639C(actor, 4);
        } else if ((actor->flags & 0x60000000) == 0x20000000) {
            func_8007639C(actor, 4);
        }
        func_8007639C(actor, 3);
    }
}

/* Whether an actor stands in the far quadrant of the scene or on a floor
 * of kind 1. */
s32 func_8008F9B0(Actor *actor) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if (pos.vx > 0 && pos.vz > 0) {
        return 1;
    }
    return (func_800828C4(actor) & 0x3000000) == 0x1000000;
}

/* Steer the opponent toward one of two headings depending on which side
 * of the scene centre it stands, at full speed. */
s32 func_8008FA2C(Actor *actor, Brain *brain) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((func_8004B32C(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        brain->unkA = 0x800 - D_80092934;
    } else {
        brain->unkA = 0xC00 - D_80092934;
    }
    brain->unkC = 0xFF;
    return 0;
}

#ifdef NON_MATCHING
/* Opponent retreat rule (when enabled and on side 1): leave the far
 * quadrant toward the centre; when the other actor is there, dodge its
 * shots by turning to face away while they are close and stop once they
 * are far. Returns whether the rule took over.
 * Does not match: the far-shot branch keeps the return value in $v0
 * across its stores (temporaries in $v1/$a0). */
s32 func_8008FACC(Actor *actor, Brain *brain) {
    s32 dist;

    if (!D_800928C4 || !(actor->flags & 0x08000000)) {
        return 0;
    }
    if (func_8008F9B0(actor)) {
        func_8008FA2C(actor, brain);
        return 1;
    }
    if (func_8008F9B0(actor->opponent)) {
        dist = actor->opponent->nearest_dist;
        if (dist < 0x800) {
            actor->flags |= 0x8000;
            brain->unkE = 1;
            brain->unkC = 0xFF;
            brain->unkA = 0x800;
            actor->target_angle = 0x800;
            return 1;
        }
        if (dist > 0x1000) {
            actor->flags &= ~0x8000;
            brain->unkE = 0;
            brain->unkC = 0;
            actor->target_angle = 0x800;
            return 1;
        }
        return 1;
    }
    /* falls off the end: the original returns the failed check (0) */
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FACC);
#endif

/* Opponent guard reaction: always at level 2, else on a random roll
 * (every other round at level 1, one in six at level 0). */
void func_8008FBD8(Actor *actor, Brain *brain) {
    if (brain->unkF >= 2) {
        actor->flags |= 2;
        func_80076424(actor);
    } else if (brain->unkF != 0) {
        if (brain->roll & 1) {
            actor->flags |= 2;
            func_80076424(actor);
        }
    } else if (brain->roll % 6 == 0) {
        actor->flags |= 2;
        func_80076424(actor);
    }
}

/* Start a new opponent round: idle mode, a pause that is shorter at
 * higher levels, and fresh rolls. */
void func_8008FC7C(Actor *actor) {
    Brain *brain = actor->brain;

    brain->unk20 = 0;
    brain->mode = 0;
    brain->timer = (2 - brain->unkF) * 30 + 90;
    func_8008F7B8(brain);
}

/* The opponent's idle mode: after the retreat rule, react to closeness,
 * guard against a charging opponent, pick a fight when the round's clock
 * runs out, dodge close shots, and wait or attack. */
void func_8008FCC8(Actor *actor, Brain *brain) {
    brain->unkC = 0;
    brain->unkE = 0;
    brain->unkA = 0;
    if (func_8008FACC(actor, brain)) {
        return;
    }
    if (D_8009284C < 0x200) {
        if (brain->unk2C_12) {
            func_80090174(actor);
        } else if (brain->unkF) {
            func_80090894(actor, 1);
        }
        if (actor->opponent->unkC5 == 2) {
            func_8008FBD8(actor, brain);
        }
        if (actor->unk1668 + 2 < brain->unk30) {
            func_80090174(actor);
        }
    }
    if (actor->opponent->nearest_dist < 0x400) {
        func_80090894(actor, 3);
        brain->unkE = 1;
    }
    switch (brain->unk20) {
    case 0:
        if (--brain->timer < 0) {
            brain->unk20++;
        }
        if (actor->opponent->unkC4 != 0) {
            break;
        }
        if (!brain->unk2C_12) {
            break;
        }
        if (func_8008F5B4(actor, 0)) {
            func_8008F900(actor);
        }
        func_80090504(actor, 0);
        break;
    case 1:
        if (func_8008F530(actor, 0)) {
            func_80090504(actor, 0);
        }
        break;
    }
}

/* Opponent command: one to three random inputs (1 or 2). */
void func_8008FE80(Actor *actor) {
    s32 roll = func_8003FA38() % 10;
    s32 count = roll >= 2 ? 2 : 1;

    if (roll >= 5) {
        count++;
    }
    while (count != 0) {
        count--;
        func_8007639C(actor, (func_8003FA38() & 1) + 1);
    }
}

/* Opponent attack mode step: against a downed opponent maybe jump in;
 * otherwise press random inputs and wait a level-dependent time. */
void func_8008FF24(Actor *actor, Brain *brain) {
    if (actor->opponent->unkC4 == 4) {
        if (brain->unk2C_9 && !brain->unk2C_11) {
            func_8008F900(actor);
            brain->timer = 3;
        }
    } else {
        func_8008FE80(actor);
        brain->timer = (2 - brain->unkF) * 20 + 1 + func_8003FA38() % 20;
    }
    func_8008F7B8(brain);
}

/* Opponent special move: enter the inputs of a random usable learned
 * move, then wait a level-dependent time. Returns 1 when it knows none. */
s32 func_8008FFEC(Actor *actor, Brain *brain) {
    s32 pick;
    s32 i;

    /* pick first counts the moves, then selects one of them */
    pick = actor->move_count;
    if (pick == 0) {
        return 1;
    }
    pick = func_8003FA38() % pick;
    for (i = 0; i < 14; i++) {
        if (actor->moves->learned[i] && actor->move_slots[i].usable) {
            if (pick == 0) {
                if (D_800925A4[i][0]) {
                    func_8007639C(actor, D_800925A4[i][0]);
                }
                if (D_800925A4[i][1]) {
                    func_8007639C(actor, D_800925A4[i][1]);
                }
                if (D_800925A4[i][2]) {
                    func_8007639C(actor, D_800925A4[i][2]);
                }
                break;
            }
            pick--;
        }
    }
    brain->timer = (2 - brain->unkF) * 20 + 1 + func_8003FA38() % 20;
    func_8008F7B8(brain);
    return 0;
}

/* Enter the opponent's attack mode: one attack step now and a number of
 * further steps that grows with its level. */
void func_80090174(Actor *actor) {
    Brain *brain = actor->brain;

    brain->mode = 1;
    func_8008FF24(actor, brain);
    if (brain->unkF >= 2) {
        brain->unk9 = func_8003FA38() % 8 + 1;
    } else if (brain->unkF != 0) {
        brain->unk9 = func_8003FA38() % 6 + 1;
    } else {
        brain->unk9 = func_8003FA38() % 4 + 1;
    }
    brain->unkC = 0;
    brain->unkE = 0;
    func_8008F7B8(brain);
}

/* Opponent attack choice: a jump attack or a special move (when it knows
 * any and is close enough). Returns 1 when it did nothing. */
s32 func_80090258(Actor *actor, Brain *brain) {
    if (actor->move_count != 0) {
        if (!(func_8003FA38() & 1)) {
            return 1;
        }
        if (!(func_8003FA38() & 1) || !func_8008F5B4(actor, 0)) {
            if (D_8009284C > 0x1000) {
                return 1;
            }
            func_8008FFEC(actor, brain);
            return 0;
        }
    } else if (!func_8008F5B4(actor, 0)) {
        return 1;
    }
    func_8008F900(actor);
    return 0;
}

/* The opponent's attack mode: after the retreat rule and guard reactions,
 * when the step timer runs out pick the next action at random, then keep
 * attacking while steps remain or fall back to the approach mode. */
void func_8009031C(Actor *actor, Brain *brain) {
    s32 dist;

    if (func_8008FACC(actor, brain)) {
        return;
    }
    if (actor->unkC5 != 2 && actor->opponent->unkC5 == 2) {
        func_8008FBD8(actor, brain);
    }
    dist = actor->opponent->nearest_dist;
    if (dist > 0x200 && dist < 0x600 && brain->unkF) {
        func_8008FBD8(actor, brain);
    }
    if (--brain->timer > 0) {
        return;
    }
    if (D_8009284C > 0x300) {
        switch (func_8003FA38() % 10) {
        case 0:
            if ((func_8003FA38() & 0xFF) >= brain->unk14) {
                func_80090894(actor, 0);
            }
            break;
        case 2:
            if (actor->move_count != 0) {
                func_8008FFEC(actor, brain);
                break;
            }
            func_8008FC7C(actor);
            break;
        case 3:
        case 4:
        case 5:
            if (!func_80090258(actor, brain)) {
                break;
            }
            /* fallthrough */
        case 1:
            func_8008FC7C(actor);
            break;
        case 6:
        case 7:
        case 8:
        case 9:
            func_80090504(actor, 0);
            break;
        }
    }
    if (brain->unk9--) {
        func_8008FF24(actor, brain);
    } else {
        func_80090894(actor, func_8003FA38() & 1);
    }
}

/* Enter the opponent's approach mode (3): a few steps, fresh rolls, and
 * whether it closes in. */
void func_80090504(Actor *actor, s32 kind) {
    Brain *brain = actor->brain;

    brain->mode = 3;
    brain->unk9 = func_8003FA38() % 4 + 1;
    brain->timer = 0;
    func_8008F7B8(brain);
    brain->unkE = func_8008F720(actor, 1);
    brain->unk2E = 0;
}

/* The opponent's approach mode (3) step: give up when the other actor retreated,
 * sidestep homing shots (and maybe counter-attack), attack when close,
 * and pick a new heading and duration whenever the timer runs out. */
void func_80090580(Actor *actor, Brain *brain) {
    s32 roll;

    if (func_8008F9B0(actor->opponent) && D_800928C4 && (actor->flags & 0x08000000)) {
        func_8008FC7C(actor);
        return;
    }
    if (actor->opponent->nearest_shot->steer == 1 && actor->opponent->nearest_dist < 0x500 &&
        brain->unkF) {
        brain->unkE = 1;
        brain->unkA = brain->unk2C_12 ? 0x400 : -0x400;
        if (brain->unkF >= 2 && (func_8003FA38() & 0xFF) < brain->unk14 &&
            func_8008F5B4(actor, 0) && brain->unk2C_12) {
            if ((func_8003FA38() & 3) == 0) {
                func_8007639C(actor, 4);
            }
            func_8008F900(actor);
            brain->unk2E = 0;
        }
        brain->unk2E++;
    }
    if (D_8009284C < 0x180) {
        if ((func_8003FA38() & 3) == 0) {
            func_8007639C(actor, 4);
        }
        func_80090174(actor);
    }
    if (!func_8008F580(actor)) {
        brain->unkE = 0;
    }
    if (brain->timer < 0) {
        brain->unkA = func_8003FA38() % 0x600 - 0x300;
        roll = func_8003FA38();
        brain->timer = (brain->unk2C_10 ? roll % 120 : roll % 100) + 10;
        brain->unkC = 0xFF;
        if (brain->unk9 != 0) {
            brain->unk9--;
        } else {
            if ((func_8003FA38() & 3) == 0) {
                func_8007639C(actor, 4);
            }
            if (func_80090258(actor, brain)) {
                func_8008FC7C(actor);
            }
            brain->unk9 = func_8003FA38() % 4 + 1;
        }
    }
    brain->timer--;
}

/* Enter the opponent's distance mode (2): maybe act first, then a random
 * distance to keep and a few decisions. */
void func_80090894(Actor *actor, s32 kind) {
    Brain *brain = actor->brain;

    brain->mode = 2;
    if (func_8003FA38() % 3 == 0) {
        func_800767C8(actor);
    }
    brain->unk28 = func_8003FA38() % 0x600 + 0x100;
    brain->timer = 0;
    brain->unk9 = func_8003FA38() % 5 + 3;
    func_8008F7B8(brain);
    brain->unkE = func_8008F720(actor, 0);
    brain->unk2E = 0;
}

/* The opponent's distance mode (2) step: sidestep homing shots (maybe
 * countering), use a special move once far enough (or when forced), and
 * pick a new wide heading and duration whenever the timer runs out. */
void func_80090990(Actor *actor, Brain *brain) {
    s32 roll;

    if (func_8008FACC(actor, brain)) {
        return;
    }
    if (actor->opponent->nearest_shot->steer == 1 && actor->opponent->nearest_dist < 0x500 &&
        brain->unkF) {
        brain->unkE = 1;
        brain->unkA = brain->unk2C_12 ? 0x400 : -0x400;
        if (brain->unkF >= 2 && func_8008F5B4(actor, 0) && brain->unk2C_12) {
            if ((func_8003FA38() & 3) == 0) {
                func_8007639C(actor, 4);
            }
            func_8008F900(actor);
            brain->unk2E = 0;
        }
        brain->unk2E++;
    }
    if (!func_8008F580(actor)) {
        brain->unkE = 0;
    }
    if (D_8009284C > brain->unk28 || (D_80092884 && D_80092850 > 0x4B0)) {
        if (D_80092884) {
            func_8007639C(actor, 4);
        }
        func_8008FFEC(actor, brain);
        func_8008FC7C(actor);
    }
    if (brain->timer < 0 || brain->unkC == 0) {
        brain->unkA = func_8003FA38() % 0x600 + 0x500;
        roll = func_8003FA38();
        brain->timer = (brain->unk2C_10 ? roll % 40 : roll % 60) + 10;
        brain->unkC = 0xFF;
        if (brain->unk9 != 0) {
            brain->unk9--;
        } else {
            if (func_8008F5B4(actor, 0)) {
                func_8007639C(actor, 4);
                func_8008F900(actor);
            }
            brain->unk9 = func_8003FA38() % 5 + 3;
        }
    }
    brain->timer--;
}

/* Load the opponent's four tendencies from its move list. */
void func_80090C88(Actor *actor) {
    MoveList *moves = actor->moves;
    Brain *brain = actor->brain;

    brain->unk10 = moves->tendency[0];
    brain->unk14 = moves->tendency[1];
    brain->unk18 = moves->tendency[2];
    brain->unk1C = moves->tendency[3];
}

/* Attach and reset the opponent brain of the actor's side and start it in
 * a random mode. */
void func_80090CC0(Actor *actor) {
    Brain *brain = &D_80096F30;

    if (actor->flags & 0x08000000) {
        brain = &D_80096F64;
    }
    actor->brain = brain;
    brain->unk6 = 0x10;
    brain->owner = actor;
    brain->timer = 0;
    brain->unk7 = 0xA;
    brain->unkA = 0;
    brain->unkC = 0;
    brain->mode = 0;
    brain->unk9 = 0;
    actor->state = 0;
    brain->unk24 = func_8008F570(actor, brain);
    brain->unkF = D_80099D98.level;
    func_80090C88(actor);
    switch (func_8003FA38() % 3) {
    case 0:
        func_8008FC7C(actor);
        break;
    case 1:
        func_80090174(actor);
        break;
    case 2:
        func_80090894(actor, 0);
        break;
    case 3:
        func_80090504(actor, 0);
        break;
    }
    func_80076424(actor);
}

/* Run the computer opponent for one frame when enabled: its current mode's
 * step, then dodge and guard flags and steering. */
void func_80090E10(Actor *actor) {
    Brain *brain;

    if (actor->flags & 0x40) {
        brain = actor->brain;
        brain->unkF = D_80099D98.level;
        switch (brain->mode) {
        case 0:
            func_8008FCC8(actor, brain);
            break;
        case 1:
            func_8009031C(actor, brain);
            break;
        case 2:
            func_80090990(actor, brain);
            break;
        case 3:
            func_80090580(actor, brain);
            break;
        }
        if (brain->unkE) {
            actor->flags |= 0x8000;
        }
        if (brain->defending) {
            actor->flags |= 2;
        }
        func_8008EE1C(actor, brain->unkA, brain->unkC);
    }
}
