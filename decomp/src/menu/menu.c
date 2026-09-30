#include "menu.h"
#include "spark.h"
#include "sound.h"

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
    MenuFrame *frame = D_80092868;

    frame->offset_prim[2] = D_800925E0 | (D_800925E4 << 16);
    func_80043B48(ot, frame->offset_prim);
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
    D_80099D9E = 0;
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
        D_80099D9E = 0;
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

/* Set up a loaded scene: graphics state, its resources, the scene origin
 * and the values from its header. */
void func_800725B0(SceneData *scene) {
    SceneHeader *header;

    func_80030988(5, 4, 0x40, 0x40);
    D_800910F0 = func_8008A3E0(func_8008A2B8(0x10));
    D_80092610 = func_8008C2C0(scene->unk5C);
    func_8008976C(0x280, 0xDA);
    func_8004A14C(0x400);
    D_800928D0 = 0;
    D_80092614 = scene;
    func_80078F00(scene);
    D_80096FA8.vz = 0;
    D_80096FA8.vy = 0;
    D_80096FA8.vx = 0;
    header = scene->header;
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
void func_8007273C(SceneModel *model, Matrix *matrix, Matrix *out) {
    Matrix local;

    *out = *matrix;
    local = D_80091C0C;
    local.t[0] = model->x - D_80096FA8.vx;
    local.t[1] = -D_80096FA8.vy;
    local.t[2] = model->z - D_80096FA8.vz;
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
    u8 colour[3];
    Actor *opponent;
    Shot *shot;
    s32 dist;
    s32 i;

    opponent = actor->opponent;
    actor->nearest_dist = 0x10000;
    func_8007C100(actor->unk15D4);
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
            colour[0] = 0xFF;
            colour[2] = 0x40;
            colour[1] = ((D_800928E8 + i) << 6) - 1;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, colour);
            break;
        case 3:
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007C880((actor->flags >> 27) & 1, &shot->pos, shot->unk38, 2);
            break;
        case 4:
            colour[0] = colour[1] = func_8003FA38() % 191 + 0x40;
            colour[2] = 0xFF;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, colour);
            break;
        case 5:
            colour[0] = colour[1] = func_8003FA38() % 191 + 0x40;
            colour[2] = 0xFF;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, colour);
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

/* Age the trail segments: new ones start fading, fading ones are freed. */
void func_80073CA4(TrailPool *pool) {
    s32 i;
    Trail *trail;

    for (i = 0; i < 16; i++) {
        trail = &pool->trails[i];
        if (trail->state == 2) {
            trail->state = 1;
        } else {
            trail->state = 0;
        }
    }
}

/* Record a new trail segment between two points. */
void func_80073CEC(Vector *a, Vector *b, s32 flip, u8 *style, Trail *trail, s32 arg5, Actor *owner) {
    trail->a_prev = trail->a;
    trail->b_prev = trail->b;
    trail->a = *a;
    trail->b = *b;
    trail->unk43 = arg5;
    trail->flip = flip & 1;
    trail->state = 2;
    trail->unk47 = owner->unk644;
    trail->unk50 = owner->unk84;
    trail->unk46 = style[1];
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
    func_8007C100(actor->unk15D4);
    if (hit->type >= 0x20) {
        func_8007C880((actor->flags >> 27) & 1, &a, style, hit->type - 0x20);
    } else {
        func_8007D190(&a, hit->type);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800740E4);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074678);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074998);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074AB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074BA4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075060);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800751C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007570C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075738);

void func_80075748(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075750);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075888);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075A4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075B50);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007639C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800763E4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076424);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800764CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007661C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800767C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077038);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077584);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007762C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800776A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077770);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A38);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A9C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078154);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078194);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078704);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078920);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078D20);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078E94);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078ED4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078F00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007920C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800796B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079A8C);

void func_80079B04(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079B0C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079B44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079D08);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079D6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079DE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079DF0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A21C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A344);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A6D0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A730);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A768);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A958);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007AC3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007AE10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B210);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B270);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B388);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BACC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BB7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BBA0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C100);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C124);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C280);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C880);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CAA4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CD14);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CD44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CF78);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D068);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D0B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D190);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D25C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D274);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D334);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D65C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D6B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D7A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D918);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007DB28);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007DC74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E020);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E24C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E2D8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E31C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E3CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E528);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E574);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E624);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E634);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E894);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E8AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E954);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E964);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EB6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EBE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EC54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007ECF0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007ED84);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE08);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE68);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EEE8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EFB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F05C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F258);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F834);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F854);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F8B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F8E4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F948);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F97C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF60);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F9A0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FB0C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FBEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FE48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FF70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080054);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080090);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800800CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080108);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080144);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080180);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800801BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800801F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080234);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080268);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800802A4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008040C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080570);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080644);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080780);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800808F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080920);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080964);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800809BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800809D8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080A58);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080AA0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080AE8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080B58);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080C48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080D10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080D20);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080F04);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800889C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088A40);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088AF8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088BD4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088BFC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088C28);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088CBC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088D1C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_80070284);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088E90);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800891C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089210);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089330);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089534);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800896C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008973C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008976C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800897AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800898BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089A98);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089B44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089C54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089C88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089CD8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089D5C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E64);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089EB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089F8C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089FC4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089FF8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A040);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A0B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A0F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A110);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A128);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A140);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A168);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A184);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A254);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A298);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A2B8);

void func_8008A3A0(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A3A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A3E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A5BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A618);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A62C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A63C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A6F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A78C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A7E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ABAC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AC0C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AC7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AC8C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ACB8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AE1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AF6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B070);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B0D8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B13C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B38C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B5DC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B5FC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B650);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B730);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BA2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BAE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BC04);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BCC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BD70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BE4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C0BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C0CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C120);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C188);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C298);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C2C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C2E8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C3A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C4B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C620);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C7C0);

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
    LineF2 *line = &spark->line[0];

    setlen(line, 3), setcode(line, 0x40);
    setSemiTrans(line, 1);
    setRGB0(line, source->r, source->g, source->b);
    spark->line[1] = spark->line[0];
}

/* Project a two-point spark line, age its trail and add it. */
void func_8008CCB0(SparkLine2 *spark, u32 *ot) {
    LineF2 *line = &spark->line[D_800928A0];
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
    Tile *tile = &spark->tile[0];

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
    Tile *tile;
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
    Tile1 *dot = &spark->dot[0];

    setlen(dot, 2), setcode(dot, 0x6A);
    setRGB0(dot, source->r, source->g, source->b);
    spark->dot[1] = spark->dot[0];
}

/* Project a dot spark and add it. */
void func_8008CF30(SparkDot *spark, u32 *ot) {
    Tile1 *dot = &spark->dot[D_800928A0];
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

#ifdef NON_MATCHING
/* Create an emitter of the given spark shape and placement rule: unit
 * spread centred on the origin, white, no sparks yet.
 * Does not match: the placement-rule store is scheduled after the
 * update-rule load. */
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
    emitter->update = D_80091CDC;
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
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D3F4);
#endif

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
void func_8008DBC0(Model *model, s16 part, Matrix *out) {
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
void func_8008DCB8(u32 *ot, Model *model, Matrix *view, Vector *pos) {
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
    Tile *tile = &D_80096DE0[D_800928A0];

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
    Tile *tile;
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
void func_8008E6F8(SoundOwner *owner) {
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
void func_8008EB88(SoundOwner *owner, s32 id, Vector *pos, s32 mode) {
    if (id != 0) {
        func_8008E78C(id + 0x60000, mode, pos, (id & 0x7F) | ((owner->flags >> 20) & 0x80));
    }
}

#ifdef NON_MATCHING
/* Play one of a character's command sounds (random 1-6 when index is 0):
 * up to two effects from the shared pair table.
 * Does not match: the original keeps the pair table address in
 * a saved register for the second id. */
void func_8008EBD0(SoundOwner *owner, s32 index, Vector *pos, s32 mode) {
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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EE1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EF00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EF30);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EF74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EFA8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F014);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F060);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F094);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F17C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F260);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F280);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F4F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F530);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F570);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F580);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F5B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F720);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F7B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F900);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F9B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FA2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FACC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FBD8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FC7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FCC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FE80);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FF24);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FFEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090174);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090258);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8009031C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090504);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090580);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090894);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090990);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090C88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090CC0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090E10);
