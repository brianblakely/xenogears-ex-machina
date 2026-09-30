#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

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

/* Place the glow emitter, launch this frame's sparks and draw them in view. */
void func_8007334C(u32 *ot, Matrix *view) {
    Emitter *emitter = D_80092644;

    emitter->base.vx = D_80092A24.vx;
    emitter->base.vy = D_80092A24.vy;
    emitter->base.vz = D_80092A24.vz;
    emitter->angles.vx = 0;
    emitter->angles.vy = 0;
    emitter->angles.vz = 0;
    func_8008D680(emitter, &D_80091C0C, D_80092648);
    gte_SetTransMatrix(view);
    gte_SetRotMatrix(view);
    func_8008DA48(emitter, ot, view);
}

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
        func_8008EBD0(actor, info->sound, &shot->pos, 2);
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
        VectorNormalS(&toward, &dir);
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
            colour.r = colour.g = rand() % 191 + 0x40;
            colour.b = 0xFF;
            shot->pos.vx += shot->velocity.vx;
            shot->pos.vy += shot->velocity.vy;
            shot->pos.vz += shot->velocity.vz;
            func_8007E31C(&shot->prev, &shot->pos, &colour);
            break;
        case 5:
            colour.r = colour.g = rand() % 191 + 0x40;
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

/* World position of a model part's vertex (1-based; 0 or a non-model part
 * gives the part's origin), relative to the actor's position. */
void func_80073B7C(Actor *actor, s32 part, s32 vertex, Vector *out) {
    Node *node = ((ModelSet *)actor->node->data)->nodes[part];

    if (vertex != 0 && node->type == 1) {
        gte_SetRotMatrix(&node->unk4C);
        gte_SetTransMatrix(&node->unk4C);
        gte_ldv0(&((SVector *)((Mesh *)((Model *)node->data)->file)->data)[vertex - 1]);
        gte_rt();
        gte_stlvnl(out);
        out->vx += actor->pos.vx;
        out->vy += actor->pos.vy;
        out->vz += actor->pos.vz;
    } else {
        out->vx = node->unk4C.t[0] + actor->pos.vx;
        out->vy = node->unk4C.t[1] + actor->pos.vy;
        out->vz = node->unk4C.t[2] + actor->pos.vz;
    }
}

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
    amount += actor->charge;
    if (amount > 0x1000) {
        return (amount - 0xFF1) / 20 < actor->hp;
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80073DE4);
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
    actor->charge += amount;
    if (actor->charge > 0x1000) {
        excess = (actor->charge - 0x1000) / 20;
        if (excess >= actor->hp) {
            actor->charge -= amount;
            actor->unkBA = 0;
            if (func_80083CD8() != 4) {
                func_8008EB4C(0x2E);
            }
            return 0;
        }
        actor->charge = 0x1000;
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

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80074678);

/* Show the model objects of the current move: unhide every kind-1 object,
 * then hide the listed ones (and object 13 in mode 0xD). */
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
    if (actor->model_id == 0xD) {
        ((Model *)nodes[13]->data)->flags |= 1;
    }
}

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80074BA4);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_800751C8);

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
    len = SquareRoot0(sq.vx + sq.vy);
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
    len = SquareRoot0(sq.vx + sq.vy);
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
        along -= SquareRoot0(diff);
        D_80092654 = along * ux / 4096 + x0;
        D_80092658 = along * uz / 4096 + z0;
    }
    return hit;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80075888);
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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80075B50);

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

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC3C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC48);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC54);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC58);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC64);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC6C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC74);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC78);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC8C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FC94);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FCA8);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FCB4);

/* Debug: print an actor's queued inputs, oldest first. The name argument of
 * the leading "%s:" is missing in the original. */
void func_80076438(Actor *actor) {
    s32 i;
    s32 index = actor->input_tail;

    func_800379C8("%s:");
    for (i = 0; i < actor->input_count; i++) {
        func_800379C8("%d", actor->inputs[index++ & 0x1F]);
    }
    func_800379C8("\n");
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FCCC);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007661C);

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
        && ((actor->flags & 0x60000000) != 0x20000000 || (actor->kind & 2))) {
        func_80076424(actor);
        func_8007639C(actor, 5);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80076884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80077038);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80077770);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80077A9C);

/* Put an actor back at its home position, idle. */
void func_80078154(Actor *actor) {
    actor->pos = actor->home;
    actor->anim = 0;
    actor->unkC4 = 0;
    actor->unkC5 = 0;
    actor->flags |= 0x2000000;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80078194);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80078704);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80078920);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80078D20);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80078F00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007920C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_800796B8);

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
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80079D6C);
#endif

/* Clear D_80092640. */
void func_80079DE0(void) {
    D_80092640 = 0;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_80079DF0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007A21C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007A344);

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

#ifdef NON_MATCHING
/* Point the camera at an actor for its victory view: height and distance
 * from its move header, a random direction around it. Does not match: GCC
 * folds the look-at height into y - (D_80092670 + 0x400). */
void func_8007A768(Actor *actor) {
    SceneHeader *header = actor->header;
    s32 angle;
    s32 y;

    D_80092668 = header->unk24;
    D_8009266C = header->unk28;
    D_80092670 = header->unk26;
    D_80092674 = header->unk2A;
    SetGeomScreen(0x200);
    angle = rand();
    y = actor->pos.vy;
    D_8009867C.vy = y;
    D_8009867C.vx = actor->pos.vx;
    D_8009871C.vy = y - 0x400 - D_80092670;
    D_8009867C.vz = actor->pos.vz;
    D_8009871C.vx = D_8009867C.vx + (((func_8003F8B0(angle) << 2) * D_80092674) >> 12);
    D_8009871C.vz = D_8009867C.vz + (((func_8003F8CC(angle) << 2) * D_80092674) >> 12);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007A768);
#endif

/* End the bout's effects and pick the next stage from the winner's move
 * header; both actors are lifted to the start height. */
void func_8007A884(void) {
    SceneHeader *header;

    func_8007E24C();
    func_8008D580(D_80092644);
    func_8007BB7C();
    func_8007F834();
    func_8008E620();
    if (D_80092890 != 0) {
        header = D_80097010.header;
    } else {
        header = D_8009872C.header;
    }
    if (header->unk2C != 0) {
        D_80091150 = 1;
        D_80091151 = 0x10;
    } else {
        D_80091150 = 2;
        D_80091151 = -1;
    }
    D_8009872C.pos.vy = 0x100;
    D_80097010.pos.vy = 0x100;
    D_8009872C.unk70 = 0x100;
    D_80097010.unk70 = 0x100;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007A958);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007AC3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007AE10);

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
    D_800926A0 = LoadClut2(D_800926A8, D_80092698, D_8009269C);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007B388);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007BBA0);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007C280);

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
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007C880);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007CAA4);

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
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007CD44);
#endif

/* Draw the scene effects: the passes that need the view and its derived
 * matrix, then the screen-space passes under the view matrix. */
void func_8007CF78(Matrix *view, u32 *ot) {
    Matrix local;

    SCENE_SCRATCH->camera = D_80096FA8;
    func_8004A8EC(view, &local);
    func_8007BBA0(view, &local, ot);
    func_8007C280(view, &local, ot);
    func_8007CAA4(view, NULL, ot);
    gte_SetRotMatrix(view);
    gte_SetTransMatrix(view);
    func_8007D918(ot);
    func_8007E020(ot);
    func_8007E3CC(ot);
}

/* Copy the camera position to the scratchpad and run the scene pass. */
void func_8007D068(void *arg) {
    SCENE_SCRATCH->camera = D_80096FA8;
    func_8007E3CC(arg);
}

/* Jitter a short position by -24..23 on each axis. */
void func_8007D0B4(SVector *pos) {
    pos->vx += rand() % 48 - 24;
    pos->vy += rand() % 48 - 24;
    pos->vz += rand() % 48 - 24;
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
    to->vx = from->vx + rand() % 64 - 32;
    to->vy = from->vy + rand() % 64 - 32;
    to->vz = from->vz + rand() % 64 - 32;
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
                color.r = rand() & 0x3F;
                color.g = rand() % 191 + 0x40;
                color.b = 0xFF;
                break;
            case 1:
                value = rand() % 256 + 0x40;
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
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007D334);
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
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007D6B8);
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
            x = pos->vx + (rand() % 64 - 32);
            cell->unk0 = x;
            z = pos->vz + (rand() % 64 - 32);
            cell->unk4 = z;
            cell->unk2 = cell->unk8 = D_800928DC[((s16)z >> 8) * 128 + ((s16)x >> 8)].height;
            cell->unk7 = -(rand() % 10 + 10);
            count--;
            cell->unk6 = 20;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007D7A8);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007D918);

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
        tile->w = rand() % 3 + 2;
        tile->h = rand() % 3 + 2;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk8 = cell->unkA = cell->unk9 = cell->unk6 = 0;
    }
    func_800732AC(D_800926D0, D_800926CC, 0x21C0);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007DC74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007E020);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu3", func_8007E3CC);

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
            AddPrim(ot, &D_800954D8[D_800928A0].sprite);
            AddPrim(ot, &D_800954D8[D_800928A0].tpage);
        }
        D_80092708--;
    }
}

s32 func_8007E624(void) {
    return D_800926DC;
}
