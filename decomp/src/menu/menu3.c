#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static s32 D_80092638;
static s32 D_8009263C;
static s32 D_80092640;
static Emitter *D_80092644; /* the menu's glow emitter */
static s32 D_80092648;
static u8 D_8009264C[4]; /* default combo state */
static s32 D_80092650; /* trail segments added */
static s32 D_80092654; /* last crossing point x, z */
static s32 D_80092658;
static s32 D_8009265C[2]; /* unreferenced */
static u8 D_80092664;
static s32 D_80092668;
static s32 D_8009266C;
static s32 D_80092670;
static s32 D_80092674;
static u16 D_80092678;
static u16 D_8009267C;
static s16 D_80092680;
static s16 D_80092684;
static s16 D_80092688;
static s16 D_8009268C;
static s16 D_80092690;
static u16 D_80092694; /* texture page */
static s16 D_80092698;
static s16 D_8009269C;
static u16 D_800926A0; /* its CLUT id */
static s32 D_800926A4; /* frame counter */
static u16 D_800926A8[4];
static s32 D_800926B0; /* scene lines added this frame */
static s32 D_800926B4;
static CVECTOR D_800926B8; /* colour of kind-2 sparkles */
static SceneCell10 *D_800926BC;
static Tile1Words *D_800926C0[2]; /* ground particle tiles per draw buffer */
static SceneCell12 *D_800926C8;
static TileWords *D_800926CC[2]; /* scene cell tiles per draw buffer */

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk). */
static VECTOR D_80092A24; /* glow emitter position */
static VECTOR D_80092A34[4]; /* saved positions: both actors, then both homes */
static SparkleKind D_80092A74[5];
static Sparkle D_80092AD8[SPARKLE_COUNT];
static u8 D_800947E8[12]; /* sparkle kind 0: texture column of each frame */
static u8 D_800947F4[12]; /* its texture row of each frame */
static u16 D_80094800[12]; /* its CLUT of each frame */
static SceneLine D_80094818[100];

ShotKind D_800910F4[] = {
    { 0x0600, 0, 0x2D, 0x40, 1, 0x0B, 0 },
    { 0x0600, 2, 0x19, 0xC0, 3, 0x00, 0 },
    { 0x0300, 2, 0x19, 0xC0, 0, 0x00, 0 },
    { 0x0000, 3, 0x20, 0x80, 2, 0x00, 0 },
    { 0x0000, 2, 0x20, 0x80, 2, 0x00, 0 },
    { 0x0000, 4, 0x20, 0x80, 2, 0x00, 0 },
    { 0x0000, 5, 0x20, 0x80, 2, 0x00, 0 },
};

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
void func_8007334C(u32 *ot, MATRIX *view) {
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
void func_80073424(VECTOR *from, VECTOR *origin, Actor *actor, s32 kind, s32 arg4, s32 arg5) {
    VECTOR toward;
    VECTOR aim;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
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
    SVECTOR half;
    VECTOR toward;
    SVECTOR dir;
    CVECTOR colour;
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
void func_80073B7C(Actor *actor, s32 part, s32 vertex, VECTOR *out) {
    Node *node = ((ModelSet *)actor->node->data)->nodes[part];

    if (vertex != 0 && node->type == 1) {
        gte_SetRotMatrix(&node->unk4C);
        gte_SetTransMatrix(&node->unk4C);
        gte_ldv0(&((SVECTOR *)((Mesh *)((Model *)node->data)->file)->data)[vertex - 1]);
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
void func_80073CEC(VECTOR *a, VECTOR *b, s32 flip, HitSpec *hit, Trail *trail, s32 arg5, Actor *owner) {
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

/* Whether an actor can take amount more: always below 0x1000 total,
 * otherwise only while the excess / 20 is below its HP. */
s32 func_80073DE4(Actor *actor, s32 amount) {
    s32 total = actor->charge + amount;

    if (total > 0x1000) {
        if ((total - 0xFF1) / 20 >= actor->hp) {
            return 0;
        }
    }
    return 1;
}

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

/* Frame event kind 2: the effect a HitSpec's type selects. Types 0x10-0x1F
 * go between the two model points: 0x10 a line trail (func_8007CD44),
 * 0x11-0x13 a bolt of kind 0-2 (func_8007D65C), the rest nothing. Other
 * types go to point a, or the midpoint when b differs, after setting the
 * kind-2 sparkle colour to the actor's: 0x20 and up a sparkle trail of size
 * D_80091228[type - 0x20] (func_8007C880), 0-4 a sparkle of that kind and
 * 8-12 the same jittered (func_8007D190; other types nothing). */
void func_80073F34(Actor *actor, HitSpec *hit) {
    VECTOR a;
    VECTOR b;
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

/* Frame event kind 0, every frame of its range: unless the type has bit
 * 0x40 (or the actor's unk84[2] is 0), a sparkle trail at point a
 * (func_8007C880) or a line trail from a to b (func_8007CD44). While the hit
 * is live (lands): type 0x20 a charged shot (func_80073424 kind 0, from the
 * midpoint of two points) if func_80073E2C takes the charge, else sparkle 9;
 * type 4 a kind-1 shot at the opponent; 0x21-0x26 a kind 1-6 shot, away from
 * b when the points differ; any other type a trail segment from a to b
 * (func_80073CEC), which func_80075B50 tests against the opponent. */
void func_800740E4(Actor *actor, HitSpec *hit, s32 lands) {
    VECTOR a;
    VECTOR b;
    VECTOR unused; /* unused in the original; reserves 16 bytes */
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

/* Attack name shown for an ether attack (the combos' are D_80091198). Its
 * literal follows the code before it. */
s32 D_8009112C = (s32)"ETHER";

/* Per animation: kind and next animation (-1: none). Rules 10 and 16 are
 * set at run time. */
AnimRule D_80091130[] = {
    { 1, -1 }, { 1, -1 }, { 2, -1 }, { 0, -1 }, { 0, -1 }, { 3, -1 },
    { 3, -1 }, { 0, -1 }, { 0, 9 }, { 2, 0xA }, { 0, 0 }, { 0, -1 },
    { 0, 9 }, { 0, 0xE }, { 0, -1 }, { 2, -1 }, { 2, -1 }, { 0, -1 },
    { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 },
    { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 },
    { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 }, { 0, -1 },
};

/* Per combo number: the combo reached by button A, then by button B. */
u8 D_80091178[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* Run the frame events of an actor's current animation for count frames
 * from frame (once per frame): its list (header + unk900[anim], 0 for none)
 * holds FrameEvent records up to first 0xFF, and each record whose range
 * holds the frame runs the HitSpec at header + spec by its kind byte. An
 * unknown kind stalls the loop, as in the original. tools/analysis/
 * overlay_scripts.py decodes the lists of the arena model files. */
void func_80074678(Actor *actor, s16 frame, s16 count) {
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    HitSpec *trails[20];
    u8 sounded;
    FrameEvent *events;
    FrameEvent *event;
    HitSpec *spec;
    s32 trail_count;
    s32 offset; /* the event list offset, then the trail search index */

    if (count == 0) {
        count = 1;
    }
    if (actor->event_frame == frame) {
        return;
    }
    actor->event_frame = frame;
    offset = actor->anim;
    offset = ((s16 *)actor->unk900)[offset];
    if (offset != 0) {
        sounded = 0;
        D_80092650 = 0;
        events = (FrameEvent *)((u8 *)actor->header + offset);
        while (--count != -1) {
            event = events;
            while (event->first != 0xFF) {
                if (frame < event->first || event->last < frame) {
                    goto next;
                }
                spec = (HitSpec *)((u8 *)actor->header + event->spec);
                switch (spec->unk0) {
                /* 0 hit (type, part_a, vertex_a, part_b, vertex_b): live
                 * (0x4000000) from the first frame until the last, unless a
                 * trail connects first; func_800740E4 on every frame. */
                case 0:
                    if (frame == event->first) {
                        actor->flags |= 0x4000000;
                    }
                    func_800740E4(actor, spec, (actor->flags >> 26) & 1);
                    if (frame == event->last) {
                        actor->flags &= ~0x4000000;
                    }
                    break;
                /* 1 sounds (part_a, part_b: character sound ids, 0 none):
                 * play both at the actor; only the call's first kind-1 event. */
                case 1:
                    if (!sounded) {
                        func_8008EB88(actor, spec->part_a, &actor->pos, 2);
                        sounded = 1;
                        func_8008EB88(actor, spec->part_b, &actor->pos, 2);
                    }
                    break;
                /* 2 effect (type, part_a, vertex_a, part_b, vertex_b):
                 * func_80073F34 once per HitSpec in a call, up to 20 (the
                 * count is never initialised). */
                case 2:
                    for (offset = 0; offset < trail_count; offset++) {
                        if (trails[offset] == spec) {
                            goto next;
                        }
                    }
                    if (trail_count < 20) {
                        trails[trail_count++] = spec;
                        func_80073F34(actor, spec);
                    }
                    break;
                /* 3 return home: put the actor at its home position, idle. */
                case 3:
                    func_80078154(actor);
                    break;
                /* 4 hide part (type: model node): set its model's hidden flag. */
                case 4:
                    ((Model *)((ModelSet *)actor->node->data)->nodes[spec->type]->data)->flags |= 1;
                    break;
                /* 5 show part (type: model node): clear its hidden flag. */
                case 5:
                    ((Model *)((ModelSet *)actor->node->data)->nodes[spec->type]->data)->flags &= ~1;
                    break;
                /* Any other kind retests the same record forever. */
                default:
                    continue;
                }
            next:
                event++;
            }
            frame++;
        }
    }
    func_80073CA4(actor);
}

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

/* Step an actor's animation player by the elapsed animation steps and
 * record the frame; stepped/done go to the pose record. */
#define ANIM_ADVANCE(actor, player, steps)                                     \
    ACTOR_FLAG_BITS(actor)->flag11 = func_8008B730(player, steps, actor->anim_speed); \
    stepped = (steps);                                                         \
    speed = actor->anim_speed

/* Record an actor's pose for this frame and run its animation: a new move
 * (or a forced restart) resets the player, speed and parts; then advance
 * by the accumulated speed and apply the move's end rule (stop, chain to
 * the next move, hold, or loop). The end-of-animation result goes to flag
 * 11 through a bit-field store. */
void func_80074BA4(Actor *actor) {
    s32 steps;
    Pose *pose = actor->pose;
    AnimRule *rule;
    Player *player;
    s32 speed = 0;
    s32 stepped = 0;
    s32 i;

    pose->flags = (pose->flags & 0xF000) | (actor->angle & 0xFFF);
    pose->x = actor->pos.vx;
    pose->y = actor->pos.vy;
    pose->z = actor->pos.vz;
    ((Move *)pose)->anim = actor->anim;
    rule = &D_80091130[actor->anim];
    player = &((ModelSet *)actor->node->data)->players[actor->anim];
    pose->flags &= ~0x1000;
    if (actor->anim != actor->unk4E || (actor->flags & 0x10000000)) {
        rule = &D_80091130[actor->anim];
        actor->unk4E = actor->anim;
        actor->flags = (actor->flags | 0x1000) & ~0x800;
        if (!(actor->flags & 0x10000000)) {
            actor->anim_speed = ((u8 *)actor->unk7C)[actor->anim * 2];
        }
        if (actor->flags & 0x2000000) {
            actor->anim_speed = 1;
            actor->flags &= ~0x2000000;
        }
        actor->unk4F = ((u8 *)actor->unk7C)[actor->anim * 2 + 1] * actor->unk15F2 / 256;
        player = &((ModelSet *)actor->node->data)->players[actor->anim];
        func_80074998(actor);
        func_8008B0D8(player);
        actor->unk99E = 0;
        actor->event_frame = -1;
        actor->flags &= ~0x10000000;
        pose->flags |= 0x1000;
        if (rule->kind == 2) {
            actor->hold_anim = actor->anim;
        } else {
            actor->hold_anim = 0;
        }
    }
    {
        s32 before = actor->unk99E;

        actor->unk99E += actor->unk4F + actor->unk52;
        steps = (actor->unk99E >> 4) - (before >> 4);
    }
    actor->unk998 = player->frame;
    actor->unk99A = steps;
    for (i = 0; i < steps; i++) {
        if (actor->anim_speed != 1) {
            if (--actor->anim_speed <= 0) {
                actor->anim_speed = 1;
            }
        }
    }
    switch (rule->kind) {
    case 0:
        if (!(actor->flags & 0x800)) {
            ANIM_ADVANCE(actor, player, steps);
            if (actor->flags & 0x800) {
                if (rule->next == -1) {
                    actor->flags &= ~0x1000;
                } else {
                    actor->anim = rule->next;
                }
            }
        }
        break;
    case 1:
        actor->flags &= ~0x1000;
        ANIM_ADVANCE(actor, player, steps);
        if (actor->flags & 0x800) {
            actor->flags |= 0x10000000;
            if (rule->next != -1) {
                actor->anim = rule->next;
            }
        }
        break;
    case 2:
        actor->flags &= ~0x1000;
        if (!(actor->flags & 0x800)) {
            ANIM_ADVANCE(actor, player, steps);
        }
        if (actor->flags & 0x400) {
            if (rule->next == -1) {
                actor->flags &= ~0x1000;
            } else {
                actor->anim = rule->next;
            }
            actor->flags &= ~0x400;
        }
        break;
    case 3:
        if (!(actor->flags & 0x800)) {
            ANIM_ADVANCE(actor, player, steps);
        }
        break;
    }
    ((Move *)pose)->unk9 = stepped;
    ((Move *)pose)->unkA = speed;
}

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
    } else if (D_8009872C.unk1660 == 0 && D_8009872C.unk165C == 0) {
        D_80050622 = 0x88;
    } else if (D_8009872C.unk165C == 0) {
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

/* Referee of the bout: ring-out and knock-out checks, the end of the bout
 * (draw or winner, with the debug controller dump), the start captions
 * while the start countdown runs, and the distance and angle between the
 * two actors. */
void func_800751C8(Actor *first, Actor *second) {
    char text[16];

    D_8009294C++;
    func_8007E894(0xA0, 0x64);
    if (!func_8008F4F4(first, 0x60) || !func_8008F4F4(second, 0x60)) {
        if (D_800928C4 != 0 && !func_8008F9B0(first) && !func_8008F9B0(second)) {
            D_80091130[10].kind = 1;
            D_80091130[10].next = 0xA;
            func_800720D4();
        }
    }
    if (D_80092638 != 0) {
        if (D_80092640++ >= 0x3D) {
            func_80083C0C(D_80092890 == 2 ? 2 : 4);
            func_80019964("gm");
            func_80031BDC(1, 0);
            func_80019964(" fin\n");
        }
        if (D_80092890 == 2) {
            func_8007EC54("DRAW GAME");
        } else {
            func_8007EC54("KNOCK OUT!!");
        }
    } else if (first->flags & 0x800000) {
        if (second->flags & 0x800000) {
            D_80092638 = 1;
            D_800928D4 = 0;
            D_80092890 = 2;
            D_80092918++;
        } else {
            D_80097010.unkF2++;
            D_800928D4 = 0;
            D_80092638 = 1;
            D_80092890 = 1;
            if (second->flags & 0x40) {
                D_800928FC = 0;
            } else {
                D_800928FC = 1;
            }
            func_80075060(0);
        }
    } else if (second->flags & 0x800000) {
        D_8009872C.unkF2++;
        D_80092638 = 1;
        D_800928D4 = 0;
        D_80092890 = 0;
        D_800928FC = 0;
        func_80075060(1);
        if ((second->flags & 0x40) && D_800928C8 != 3) {
            func_80019964("00");
            func_80031BDC(1, 2);
            func_80019964(" fin\n");
            func_80088BD4(second->model_id);
            func_80019964("11");
            func_80031BDC(1, 2);
            func_80019964(" ctrl\n");
        }
    }
    if (D_8009263C != 0) {
        if (D_8009263C < 0x1E) {
            if (func_80083CD8() != 7 && D_800928C8 != 4) {
                func_8007EC54("FIGHT!!");
            }
            D_800928D4 = 1;
        } else if (D_8009263C < 0x3C) {
            if (func_80083CD8() != 7) {
                if (D_800928C8 == 4) {
                    func_8007EC54("START");
                    if (D_80092884 != 0) {
                        func_8007EC54("");
                        func_8007EC54("RUBBER BAND MODE");
                    }
                } else {
                    func_8007EC54("READY");
                    if (D_80092884 != 0) {
                        func_8007EC54("");
                        func_8007EC54("RUBBER BAND BATTLE");
                    }
                }
            }
        } else if (func_80083CD8() != 7) {
            if (D_800928C8 == 4) {
                func_8007EC54("PRACTICE");
                if (D_80092884 != 0) {
                    func_8007EC54("");
                    func_8007EC54("RUBBER BAND MODE");
                }
            } else {
                sprintf(text, "ROUND %d", D_80092950);
                func_8007EC54(text);
                if (D_80092884 != 0) {
                    func_8007EC54("");
                    func_8007EC54("RUBBER BAND BATTLE");
                }
            }
        }
        D_8009263C--;
    }
    D_80092850 = func_800887A4(&first->pos, &second->pos);
    D_8009284C = func_80088838(&first->pos, &second->pos);
    D_80092934 = ratan2(first->pos.vx - second->pos.vx, first->pos.vz - second->pos.vz);
}

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
    VECTOR d;
    VECTOR sq;
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

/* Like func_80075750, and on a hit store where the segment enters the
 * circle around the point in D_80092654/D_80092658. */
s32 func_80075888(s32 x0, s32 z0, s32 x1, s32 z1, s32 px, s32 pz, s32 radius) {
    VECTOR d;
    VECTOR sq;
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
        diff = along - SquareRoot0(diff); /* the entry distance along the segment */
        D_80092654 = diff * ux / 4096 + x0;
        D_80092658 = diff * uz / 4096 + z0;
    }
    return hit;
}

/* Whether a point comes within radius of any edge of a quad given as four
 * corner vectors; clears the crossing point to the point first. */
s32 func_80075A4C(VECTOR *quad, s32 px, s32 pz, s32 radius) {
    D_80092654 = px;
    D_80092658 = pz;
    if (func_80075750(quad[1].vx, quad[1].vz, quad[0].vx, quad[0].vz, px, pz, radius)
        || func_80075750(quad[3].vx, quad[3].vz, quad[2].vx, quad[2].vz, px, pz, radius)
        || func_80075750(quad[0].vx, quad[0].vz, quad[2].vx, quad[2].vz, px, pz, radius)) {
        return 1;
    }
    return func_80075750(quad[1].vx, quad[1].vz, quad[3].vx, quad[3].vz, px, pz, radius) != 0;
}

/* Point of the body axis (home to upper anchor) at a height between them;
 * frac is the scratch variable that holds the 4.12 fraction. */
#define AXIS_POINT(out, home, top, y, frac)                                     \
    frac = (((y) - (home).vy) << 12) / ((top).vy - (home).vy);                  \
    (out).vx = ((frac * ((top).vx - (home).vx)) >> 12) + (home).vx;             \
    (out).vy = ((frac * ((top).vy - (home).vy)) >> 12) + (home).vy;             \
    (out).vz = ((frac * ((top).vz - (home).vz)) >> 12) + (home).vz

/* Test the opponent's shots and trails against an actor's body axis; the
 * first shot or trail that hits sets the hit point, effect, glow, damage and
 * reaction. Returns 0. One variable serves as the damage and as the
 * scratch value before it (the axis fraction, the distance to the top). */
s32 func_80075B50(Actor *actor) {
    VECTOR home;
    VECTOR top;
    VECTOR point;
    VECTOR from;
    Shot *shot;
    Trail *trail;
    s32 hits = 0;
    s32 best;
    s32 kind;
    s32 damage;
    s32 hit;
    s32 dh;
    s32 dm;
    s32 i;

    home = actor->home;
    top = actor->unk92C;
    best = 0;
    kind = 0;
    for (i = 0; i < 8; i++) {
        shot = &actor->opponent->shots[i];
        if (!shot->active) {
            continue;
        }
        hit = 0;
        if (top.vy < shot->pos.vy && shot->pos.vy < home.vy) {
            AXIS_POINT(point, home, top, shot->pos.vy, damage);
            hit = func_80075888(shot->prev.vx, shot->prev.vz, shot->pos.vx, shot->pos.vz, point.vx, point.vz,
                                actor->header->unk13);
        } else if (shot->dist < actor->header->unk13) {
            damage = abs(top.vy - shot->prev.vy);
            dh = abs(home.vy - shot->prev.vy);
            dm = abs(shot->pos.vy - shot->prev.vy);
            if (damage < dh) {
                if (damage < dm) {
                    point = top;
                    hit = 1;
                }
            } else if (dh < dm) {
                hit = 1;
                point = home;
            }
        }
        if (!hit) {
            continue;
        }
        actor->glow = 0xFF;
        actor->unkD4 |= 0x20;
        damage = shot->unk3C;
        if ((u32)(((ratan2(shot->velocity.vx, shot->velocity.vz) - actor->angle) & 0xFFF) - 0x601) >= 0x3FF) {
            if (actor->flags & 4) {
                func_8008EBD0(actor, 0xF, &point, 1);
                damage /= 2;
                actor->glow = 0x80;
            } else {
                func_8008EBD0(actor, 0xC, &point, 1);
            }
        } else {
            damage *= 2;
            kind = 3;
            func_8008EBD0(actor, 0xC, &point, 1);
        }
        actor->unkE8 += damage * 2 / 3;
        actor->unk916 += damage * 2 / 3;
        actor->hit_point = point;
        if (damage != 0) {
            hits++;
        }
        D_80092A24 = point;
        D_80092648 = 0x10;
        actor->unkC8 = damage / 3 + 0xC;
        best = damage;
        from = point;
        shot->active = 0;
        break;
    }
    for (i = 0; i < 16; i++) {
        trail = &actor->opponent->trails[i];
        if (trail->state != 1) {
            continue;
        }
        if (trail->unk44_0) {
            continue;
        }
        hit = 0;
        if (trail->flip) {
            if (top.vy < trail->a.vy && trail->a.vy < home.vy) {
                AXIS_POINT(point, home, top, trail->a.vy, damage);
                hit = func_80075888(trail->a_prev.vx, trail->a_prev.vz, trail->a.vx, trail->a.vz, point.vx,
                                    point.vz, actor->header->unk13);
            }
        } else {
            damage = (trail->a.vy + trail->a_prev.vy + trail->b.vy + trail->b_prev.vy) / 4;
            if (top.vy < damage && damage < home.vy) {
                AXIS_POINT(point, home, top, damage, damage);
                hit = func_80075A4C((VECTOR *)trail, point.vx, point.vz, actor->header->unk13);
            }
        }
        if (!hit) {
            continue;
        }
        actor->opponent->flags &= ~0x4000000;
        point.vx = D_80092654;
        point.vz = D_80092658;
        if (actor->flags & 4) {
            func_8008EBD0(actor, 0xF, &point, 1);
            damage = trail->unk47 >> 1;
            D_80092A24 = point;
            D_80092648 = 4;
            actor->glow = 0xC0;
            actor->unkD4 |= 0x20;
        } else {
            func_8008EBD0(actor, trail->effect, &point, 1);
            func_8007D190(&point, 0);
            D_80092A24 = point;
            D_80092648 = 0x10;
            actor->glow = 0xFF;
            actor->unkD4 &= ~0x20;
            damage = trail->unk47;
        }
        actor->unkC8 = damage;
        actor->unkE8 += damage * 6 / 5;
        actor->unk916 += damage * 6 / 5;
        actor->hit_point = point;
        if (damage != 0) {
            hits++;
        }
        if (best < damage) {
            best = damage;
            from = trail->a;
            kind = trail->unk46;
        }
        func_80073CA4(actor->opponent);
        break;
    }
    if (hits != 0) {
        actor->unk100 = kind + 1;
        actor->unk970 = best;
        actor->hit_from = from;
        actor->unk1668++;
    } else {
        actor->unk100 = 0;
    }
    return 0;
}

/* Queue a pad input for an actor (dropped when 32 are pending). */
void func_8007639C(Actor *actor, u8 input) {
    if (actor->input_count < 32) {
        actor->inputs[actor->input_head++ & 0x1F] = input;
        actor->input_count++;
    }
}

/* Take the oldest queued input of an actor, 0 when none. */
s32 func_800763E4(Actor *actor) {
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

/* Name of each combo number. GCC emits an initializer's string literals last
 * to first, after those of the code before it ("" is func_800751C8's). */
s32 D_80091198[] = {
    (s32)"",
    (s32)"A", (s32)"B",
    (s32)"AA", (s32)"AB", (s32)"BA", (s32)"BB",
    (s32)"AAA", (s32)"AAB", (s32)"ABA", (s32)"ABB",
    (s32)"BAA", (s32)"BAB", (s32)"BBA", (s32)"BBB",
};

s32 D_800911D4 = 0;

/* Sparkle frame texel positions (u, v); the v tables get the image's y
 * added once. */
u8 D_800911D8[16] = {
    0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0,
    0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0,
};
u8 D_800911E8[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};
u8 D_800911F8[16] = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
    0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
};
u8 D_80091208[16] = { 0 };
u8 D_80091218[16] = { 0 };

s16 D_80091228[] = { 0x18, 0x30, 0x10 };

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

/* Debug: print an actor side's pending move name (dimmed unless it is the
 * current one), its strength and its frame range at the side's corner. */
void func_8007661C(Actor *actor) {
    char text[16];
    s32 x;
    s32 y = 0xA;

    if (actor->flags & 0x8000000) {
        x = 0xB4;
    } else {
        x = 0x14;
    }
    if (D_800928C8 != 4) {
        y = 0xB2;
    }
    func_8007E894(x, y);
    sprintf(text, "%s", D_80096FB8[ACTOR_SIDE(actor)].unk0);
    func_8007E954(D_80096FB8[ACTOR_SIDE(actor)].unk0 == D_8009112C ? 0xE7 : 0x100);
    func_8007EBE0(text);
    func_8007E954(0x100);
    func_8007E894(x + 0x34, y);
    sprintf(text, "STR:%d", D_80096FB8[ACTOR_SIDE(actor)].unk4);
    func_8007EBE0(text);
    y += 0x14;
    func_8007E894(x, y);
    sprintf(text, "FRAME:%d-%d", D_80096FB8[ACTOR_SIDE(actor)].unkC >> 4,
            D_80096FB8[ACTOR_SIDE(actor)].unk10 >> 4);
    func_8007EBE0(text);
}

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

/* Accelerate a directional speed toward a limit, or brake it to zero. */
#define STEER(speed, on, limit)                                                 \
    if (on) {                                                                  \
        (speed) += accel;                                                      \
        if ((speed) > (limit)) {                                               \
            (speed) = (limit);                                                 \
        }                                                                      \
    } else {                                                                   \
        (speed) -= brake;                                                      \
        if ((speed) < 0) {                                                     \
            (speed) = 0;                                                       \
        }                                                                      \
    }

/* Read a player's pad (analog sticks or the d-pad) and turn it into the
 * actor's commands, guard/dash flags and its four directional speeds,
 * giving the move speed and heading. Declared s32 with a bare return so
 * the early exit keeps the original's empty delay slot. */
s32 func_80076884(Actor *actor) {
    u8 stick_y;
    u8 stick_x;
    u16 held;
    u16 pressed;
    s32 analog;
    s32 accel;
    s32 brake;
    s32 move;
    s32 dx;
    s32 dy;
    s32 dir;
    s32 heading;
    s32 speed;

    if (actor->flags & 0x8000000) {
        analog = (u32)(func_80035734(1) - 3) < 2;
        held = D_80059574;
        pressed = D_80059490;
        if (analog) {
            stick_y = D_80059434;
            stick_x = D_8005943C;
        } else {
            stick_y = func_80035884(held);
            stick_x = func_800358A0(D_80059574);
        }
    } else {
        analog = (u32)(func_80035734(0) - 3) < 2;
        held = D_80059570;
        pressed = D_8005948C;
        if (analog) {
            stick_y = D_80059430;
            stick_x = D_80059438;
        } else {
            stick_y = func_80035884(held);
            stick_x = func_800358A0(D_80059570);
        }
    }
    if (actor->flags & 0x40) {
        return;
    }
    if (D_800928F4 != ((actor->flags >> 17) & 1)) {
        if (D_800928F4 != 0) {
            actor->unkCE = -actor->unkCC;
        } else {
            actor->unkCE = actor->unkCC;
        }
        actor->unk648 += 0x800;
        ACTOR_FLAG_BITS(actor)->flag17 = D_800928F4;
        actor->unkFC += 0x800;
    }
    if (pressed & 0x10) {
        func_8007639C(actor, 1);
        actor->unk1660++;
    }
    if (pressed & 0x20) {
        func_8007639C(actor, 2);
        actor->unk1660++;
    }
    if (pressed & 8) {
        func_8007639C(actor, 3);
        actor->unk165C++;
    }
    if (pressed & 0x80) {
        func_80076424(actor);
        func_8007639C(actor, 4);
    }
    if (held & 0x40) {
        actor->flags |= 0x8000;
    }
    if (pressed & 0x40) {
        func_80076424(actor);
    }
    brake = actor->brake;
    if (held & 4) {
        actor->flags |= 2;
        func_80076424(actor);
        brake <<= 2;
        actor->flags |= 0x100;
    } else {
        actor->flags &= ~0x38;
    }
    if ((actor->flags & 0x60000000) == 0x60000000 && !(actor->kind & 4)) {
        accel = actor->accel / 2;
    } else {
        accel = actor->accel;
    }
    if ((actor->flags & 0x60000000) == 0x20000000 && (actor->kind & 4)) {
        accel = actor->accel / 2;
    }
    if (actor->flags & 0x100) {
        move = 0;
        brake = brake * 2 / 3;
    } else {
        move = 1;
    }
    dx = (stick_x - 0x80) * 2;
    dy = (stick_y - 0x80) * 2;
    dir = ratan2(dx, dy) + actor->unkCE;
    if (held & 0xF000) {
        STEER(actor->unk8C, (held & 0x1000) && move, 0x100);
        STEER(actor->unk94, (held & 0x4000) && move, 0x100);
        STEER(actor->unk88, (held & 0x8000) && move, 0x100);
        STEER(actor->unk90, (held & 0x2000) && move, 0x100);
        if (!(held & 0xF000)) {
            dir = 0;
        }
    } else {
        if (SquareRoot0(dx * dx + dy * dy) < 0x30) {
            dir = 0;
        }
        STEER(actor->unk8C, dx < 0 && move, abs(dx));
        STEER(actor->unk94, dx > 0 && move, dx);
        STEER(actor->unk88, dy < 0 && move, abs(dy));
        STEER(actor->unk90, dy > 0 && move, dy);
    }
    dx = actor->unk8C - actor->unk94;
    dy = actor->unk90 - actor->unk88;
    heading = ratan2(dy, dx);
    speed = SquareRoot0(dx * dx + dy * dy);
    if (speed > 0x100) {
        speed = 0x100;
    }
    if (speed < 0x30) {
        speed = 0;
    }
    actor->state = speed;
    if (speed != 0) {
        actor->target_angle = heading + actor->unkCE;
    } else {
        actor->target_angle = 0;
    }
    dir &= 0xFFF;
    if ((u32)(dir - 0x201) < 0x3FF) {
        func_800767C8(actor);
        actor->flags |= 0x1000000;
    } else {
        actor->flags &= ~0x1000000;
    }
    if ((actor->flags & 0x80000) && actor->state != 0 &&
        abs((heading & 0xFFF) - ((ratan2(stick_x - 0x80, stick_y - 0x80) + 0x400) & 0xFFF)) > 0x200) {
        actor->flags |= 0x80000;
    }
}

/* Per-frame actor status: count down its timers, drain its charge, apply
 * this frame's damage to its hit points (with the hit sound), update its
 * gauge and knock it out when the hit points run out. Declared int without
 * a return value, as the original's unfilled delay slots in the knock-out
 * test show. */
s32 func_80077038(Actor *actor) {
    u8 state;
    u8 gauge;
    u32 stance;

    if (actor->level != 0) {
        actor->level -= 0x10;
    }
    if (actor->unkC4 == 5) {
        return;
    }
    if (actor->unkE8 > 0x14) {
        actor->unkE8 = 0x14;
    }
    if (actor->unkE8 != 0) {
        actor->unkE8--;
    } else {
        actor->flags &= ~0x1000;
    }
    if (actor->unk916 != 0) {
        actor->unk916--;
    }
    if (actor->unk914 != 0) {
        actor->unk914--;
    }
    if (actor->unkC8 != 0) {
        actor->unkC8--;
    } else {
        actor->unkF0 = 0;
        actor->flags = (actor->flags & ~0x700000) | 0x300000;
    }
    if (actor->unkC3 != 0) {
        actor->unkC5 = 4;
        actor->unkC3--;
    } else if (actor->unkC5 == 4) {
        actor->unkC5 = 0;
    }
    if (actor->unkCA != 0) {
        actor->unkCA--;
    }
    if (actor->charge != 0) {
        if (actor->unkC4 & 1) {
            stance = actor->flags & 0x60000000;
            if (stance == 0x60000000) {
                if (actor->kind & 4) {
                    actor->charge -= 8;
                } else {
                    actor->charge -= 2;
                }
            } else if (stance == 0x20000000) {
                actor->charge -= 6;
            } else {
                actor->charge -= 4;
            }
        } else {
            actor->charge -= 8;
        }
        if (actor->charge < 0) {
            actor->charge = 0;
        }
    }
    state = actor->unkC4;
    if (state == 4) {
        if (actor->unkCA == 0) {
            actor->unkC4 = 0;
            actor->unkC5 = 0;
        }
    } else {
        if (actor->floor_y - 0x30 < actor->pos.vy) {
            actor->unkC4 = state & ~2;
        }
        if (actor->state != 0) {
            actor->unkC4 |= 1;
        } else {
            actor->unkC4 &= ~1;
        }
    }
    if (actor->flags & 0x800) {
        switch (actor->hold_anim) {
        case 9:
            if (actor->unkCA == 0) {
                actor->flags |= 0x400;
            }
            break;
        case 15:
            if (actor->unkC5 != 4) {
                actor->flags |= 0x400;
            }
            break;
        case 2:
            if (actor->unkC4 & 2) {
                actor->flags |= 0x400;
            }
            break;
        }
    }
    actor->hp -= actor->unkBA;
    actor->hp -= actor->unk970;
    if (actor->unkBA != 0) {
        if (actor->unk910 == 0) {
            if (func_80083CD8() != 4) {
                func_8008EB4C(0x2D);
            }
            actor->unk910 = 0x14;
        }
        actor->unk910--;
    } else {
        actor->unk910 = 0;
    }
    if (D_800928C4 && !func_8008F4F4(actor, 0x50) && (actor->flags & 0x08000000)) {
        actor->hp = actor->max_hp * 0x50 / 256;
    }
    if (actor->hp < 0) {
        actor->hp = 0;
    }
    if (actor->unkB8 != actor->hp) {
        actor->unkB8 = actor->hp;
        gauge = ((actor->hp << 7) + actor->max_hp - 1) / actor->max_hp;
        actor->unkC1 = actor->unkC0;
        actor->unkC0 = gauge;
        if (gauge == 1) {
            actor->unkC0 = 2;
        }
        actor->level = 0xF0;
    }
    actor->unkBA = 0;
    actor->unk970 = 0;
    if (D_800928C8 != 4 && actor->hp == 0 && D_800928C8 != 6 && !(actor->flags & 0x800000)) {
        actor->anim = 0xD;
        actor->unk4E = 0xFF;
        actor->unkC4 = 5;
        actor->flags |= 0x1000;
        actor->unkC5 = 0;
        actor->flags |= 0x400;
        actor->velocity.vy -= 0x50;
        if (actor->flags & 0x08000000) {
            actor->push.vx -= func_8003F8B0(D_80092934) >> 9;
            actor->push.vz -= func_8003F8CC(D_80092934) >> 9;
        } else {
            actor->push.vx += func_8003F8B0(D_80092934) >> 9;
            actor->push.vz += func_8003F8CC(D_80092934) >> 9;
        }
        actor->flags |= 0x800000;
        func_800776A8(actor, 0x14);
        actor->unk100 = 0;
    }
}

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
        if ((D_800928C8 == 2 || D_800928C8 == 4) && (D_80099D98.option5 & 1) && !(actor->flags & 0x40)) {
            func_80036258(1, arg);
        }
    } else if ((D_80099D98.option4 & 1) && !(actor->flags & 0x40)) {
        func_80036258(0, arg);
    }
}

/* Start a reaction pose: animation, restart it, pose change pending. */
#define SET_POSE(actor, pose) ((actor)->anim = (pose), (actor)->unk4E = 0xFF, (actor)->flags |= 0x1000)

/* React to a hit: pick the flinch pose (alternating by the hit height
 * between the two anchor heights), then knock the actor away from where the
 * hit came from according to the hit's step, and vibrate the pad. */
void func_80077770(Actor *actor) {
    s32 strength = 4;
    s32 angle;
    s32 lift;
    u32 flags;

    actor->unkF0++;
    if (actor->unkCA != 0) {
        if (actor->anim == 9) {
            SET_POSE(actor, 0xC);
            goto done;
        }
    } else {
        flags = actor->flags;
        if (flags & 4) {
            strength = 2;
            goto done;
        }
        if (actor->hit_from.vy < (actor->unk92C.vy - actor->home.vy) * 2 / 3 + actor->home.vy) {
            if ((flags & 0x700000) == 0x100000) {
                SET_POSE(actor, 7);
                actor->flags &= ~0x700000;
            } else {
                SET_POSE(actor, 6);
                actor->flags = (actor->flags & ~0x700000) | 0x100000;
            }
        } else {
            if ((flags & 0x700000) == 0x200000) {
                SET_POSE(actor, 7);
                actor->flags &= ~0x700000;
            } else {
                SET_POSE(actor, 5);
                actor->flags = (actor->flags & ~0x700000) | 0x200000;
            }
        }
    }
    angle = ratan2(actor->pos.vx - actor->hit_from.vx, actor->pos.vz - actor->hit_from.vz);
    if (func_80088838(&actor->pos, &actor->opponent->pos) < func_80088838(&actor->hit_from, &actor->opponent->pos)) {
        angle += 0x800;
    }
    lift = 1;
    switch ((actor->unk100 - 1) & 7) {
    case 2:
        SET_POSE(actor, 6);
        func_80077584(actor, angle, 0xC, 0x64);
        break;
    case 3:
        func_8007762C(actor, angle, 0xB, 0x80);
        break;
    case 1:
        lift = 0;
    default:
        if (actor->unk916 >= 0x26 || actor->unkF0 >= 4) {
            func_8007762C(actor, angle, 0xA, 0xA0);
            strength = 0xF;
            actor->unkE8 = 0;
            actor->unk916 = 0;
        } else {
            func_80077584(actor, angle, 0xA, -lift & 0x1E);
        }
        break;
    }
done:
    func_800776A8(actor, strength);
}

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

/* Debug trace of a started special move, compiled out of the release. */
#define MOVE_TRACE() do { } while (0)

/* Per-frame actor action: unless busy or stunned, start the move for the
 * decoded command (combo attacks by button, the charged shot, the jump,
 * the dash), then set the animation, drift and turn for the current
 * stance, derive the walking speed and let a dash use up charge. */
void func_80077A9C(Actor *actor) {
    MoveSlot *slot;
    s32 bounce;

    if (actor->unkC4 == 5) {
        return;
    }
    if (actor->unk100 != 0) {
        func_80077770(actor);
        return;
    }
    bounce = 0;
    if (actor->unkE8 != 0) {
        func_80076424(actor);
        return;
    }
    actor->flags &= ~1;
    if (actor->unk914 != 0) {
        func_80076424(actor);
    }
    if ((actor->flags & 0x800) && actor->unkC5 != 4) {
        actor->unkC5 = 0;
    }
    if (actor->unkC5 == 0 && actor->unkC4 != 4) {
        switch (func_800763E4(actor)) {
        case 1:
            if (actor->unk914 != 0) {
                break;
            }
            slot = func_80077A38(actor, 1);
            goto start;
        case 2:
            if (actor->unk914 != 0) {
                break;
            }
            slot = func_80077A38(actor, 0);
        start:
            if (actor->unkC4 & 2) {
                break;
            }
            if (slot->unk0[0] != 0) {
                actor->anim = slot->unk0[0] + 0x12;
                actor->unk4E = 0xFF;
                actor->flags |= 0x1000;
                if (slot->unk0[1] != 0) {
                    bounce = 1;
                }
                actor->flags |= 1;
                actor->unkC5 = 2;
                actor->unk84 = (u8 *)slot;
                actor->unk644 = actor->moves->learned[actor->unk9C3 - 1] * actor->moves->base / 100;
                break;
            }
            actor->unk914 = 0xF;
            func_80076424(actor);
            break;
        case 0:
            func_80076424(actor);
            break;
        case 3:
            if (actor->unk914 == 0 && actor->moves->unk18 != 0 && actor->model_id != 0x29) {
                if (actor->unkC4 & 2) {
                    actor->anim = 4;
                } else {
                    actor->anim = 3;
                }
                actor->unk4E = 0xFF;
                actor->flags |= 0x1000;
                MOVE_TRACE();
                bounce = 1;
                actor->unkC5 = 2;
                actor->flags |= 1;
            }
            break;
        case 4:
            if (actor->unkCA != 0) {
                return;
            }
            if (!(actor->unkC4 & 2)) {
                func_8007D190(&actor->home, 9);
                actor->velocity.vy -= 0x8C;
                actor->unkC4 |= 2;
                func_8008ED6C(actor, 0xA);
                func_8008EBD0(actor, 0xA, &actor->home, 2);
            }
            break;
        case 5:
            actor->anim = 0xF;
            actor->unk4E = 0xFF;
            actor->flags |= 0x1000;
            func_8008EBD0(actor, 0xE, &actor->pos, 2);
            actor->unkC3 = 6;
            actor->unkC5 = 4;
            break;
        }
    }
    actor->unk52 = 0;
    actor->flags &= ~4;
    switch (actor->unkC4) {
    case 0:
        actor->target_angle = 0;
        switch (actor->unkC5) {
        case 0:
            if (actor->flags & 2) {
                actor->anim = 0x11;
                if ((actor->flags & 0x38) == 0x10) {
                    actor->flags |= 4;
                } else {
                    actor->flags = (actor->flags & ~0x38) | (((((actor->flags >> 3) & 7) + 1) & 7) << 3);
                }
            } else {
                actor->anim = 0;
            }
            break;
        case 2:
            if (bounce) {
                actor->unk44 += 0x40;
            }
            break;
        case 4:
            func_80077A88(actor);
            break;
        }
        actor->unkFC = func_8008B650(actor->unkFC, actor->target_angle, 0x100);
        break;
    case 1:
        actor->unk52 = actor->state * actor->unk15F2 / 4096;
        switch (actor->unkC5) {
        case 0:
            actor->anim = 1;
            break;
        case 2:
            if (bounce) {
                actor->unk44 += 0x40;
            }
            break;
        case 4:
            func_80077A88(actor);
            break;
        }
        actor->unkFC = func_8008B650(actor->unkFC, actor->target_angle, 0x100);
        if ((actor->flags & 0x60000000) == 0x20000000 && !(actor->kind & 2)) {
            actor->flags &= ~0x8000;
        }
        break;
    case 2:
        actor->state = 0;
        actor->unk44 = 0;
        actor->target_angle = 0;
        if (actor->unkC5 == 0) {
            actor->anim = 2;
        }
        break;
    case 3:
        if (actor->unkC5 == 0) {
            actor->anim = 2;
        }
        break;
    case 4:
        actor->state = 0;
        break;
    }
    actor->unk40 = actor->state * actor->unk15F0 * actor->unk15F2 >> 16;
    if (!(actor->flags & 0x8000)) {
        actor->unkD4 &= ~0x40;
        if (!(actor->flags & 0x8000)) {
            goto done;
        }
    }
    if (actor->unkC5 == 0 && !(actor->unkD4 & 0x40) && actor->unk40 > 0x30 &&
        !(actor->unkC4 == 2 || actor->unkC4 == 3)) {
        if (func_80073E2C(actor, 0x20, 2)) {
            actor->unk40 *= 2;
            actor->unk52 *= 2;
        } else {
            actor->flags &= ~0x8000;
            actor->unkD4 |= 0x40;
        }
        if (D_8009287C >= 2) {
            D_8009287C = 1;
        }
    }
done:
    if (actor->unkC4 == 0 || (actor->unkD4 & 0x40)) {
        actor->flags &= ~0x8000;
    }
}

/* Put an actor back at its home position, idle. */
void func_80078154(Actor *actor) {
    actor->pos = actor->home;
    actor->anim = 0;
    actor->unkC4 = 0;
    actor->unkC5 = 0;
    actor->flags |= 0x2000000;
}

/* Per-frame actor motion: note the round-start position, steer the heading
 * toward the camera-relative facing (the other way round on side 1), stop
 * dead while stunned, turn the forward speed, gravity, bounce and (in the
 * late round) a pull toward the opponent into this frame's velocity, and
 * add the push scaled by the stance. */
void func_80078194(Actor *actor) {
    VECTOR pull;
    s32 facing;
    s32 speed;
    s32 scale;

    actor->start = actor->pos;
    actor->start_home = actor->home;
    speed = actor->unk40;
    facing = D_80092934;
    if (actor->flags & 0x08000000) {
        facing += 0x800;
    }
    if (actor->unkC4 == 0) {
        if (actor->unkFC != 0) {
            actor->unk648 = facing;
        } else {
            actor->unk648 = func_8008B650(actor->unk648, facing, 0x100);
        }
        if (D_8009287C >= 5) {
            D_8009287C = 4;
        }
    } else if (actor->unkC4 == 1) {
        if (D_8009284C > 0x100) {
            if (actor->unkFC != 0) {
                actor->unk648 = facing;
            } else {
                actor->unk648 = func_8008B650(actor->unk648, facing, 0x100);
            }
        }
        if (D_8009287C >= 3) {
            D_8009287C = 2;
        }
    }
    if (actor->unkC5 == 4) {
        actor->angle = facing;
        actor->unk648 = facing;
        actor->unkFC = 0;
        actor->unk40 = 0;
        actor->unk94 = 0;
        actor->unk8C = 0;
        actor->unk88 = 0;
        actor->unk90 = 0;
        speed = -(actor->unkC3 * 0x50) / 3;
    } else {
        actor->angle = actor->unk648 + actor->unkFC;
    }
    if (actor->unkC5 == 2) {
        if ((actor->flags & 1) || (actor->unkC4 & 2)) {
            actor->unk90C = facing;
        }
        actor->angle = actor->unk90C;
        actor->flags = (actor->flags | 0x100) & ~0x80000;
    } else {
        actor->flags = actor->flags & ~0x100;
    }
    if (actor->flags & 0x40000) {
        actor->velocity.vx = -(func_8003F8B0(actor->unk648 + actor->unkFC) * speed) >> 12;
        actor->velocity.vz = -(func_8003F8CC(actor->unk648 + actor->unkFC) * speed) >> 12;
    } else {
        actor->flags &= ~0x80000;
    }
    actor->velocity.vy += 0xA;
    if (actor->unk44 != 0 && actor->pos.vy == actor->floor_y) {
        actor->unk44 = actor->unk44 * 0xA0 / 256;
        actor->unk30.vx = -(func_8003F8B0(facing) * actor->unk44) >> 12;
        actor->unk30.vz = -(func_8003F8CC(facing) * actor->unk44) >> 12;
        actor->velocity.vx += actor->unk30.vx;
        actor->velocity.vy += actor->unk30.vy;
        actor->velocity.vz += actor->unk30.vz;
    }
    if (D_80092850 > 0x280 && D_80092884 != 0) {
        pull.vx = actor->opponent->pos.vx;
        pull.vy = actor->opponent->pos.vy;
        pull.vz = actor->opponent->pos.vz;
        pull.vx -= actor->pos.vx;
        pull.vy -= actor->pos.vy;
        pull.vz -= actor->pos.vz;
        pull.vx /= 48;
        pull.vy /= 48;
        pull.vz /= 48;
        actor->velocity.vx += pull.vx;
        actor->velocity.vy += pull.vy;
        actor->velocity.vz += pull.vz;
    }
    scale = 0x80;
    switch ((s32)((actor->flags >> 29) & 3)) {
    case 0:
        scale = 0xFF;
        break;
    case 1:
    case 3:
        scale = 0;
        break;
    case 2:
        break;
    }
    if (actor->unkC4 == 4) {
        scale = 0xFF;
    }
    if (actor->flags & 0x40000) {
        actor->push.vx = actor->push.vx * scale / 256;
        actor->push.vz = actor->push.vz * scale / 256;
    }
    actor->velocity.vx += actor->push.vx;
    actor->velocity.vy += actor->push.vy;
    actor->velocity.vz += actor->push.vz;
}

/* Which of an actor's two anchor points (0x92c and home) lie on the other
 * side of the line from `a` to `b` than its round start position: 0 both
 * (choosing 0x92c when home is nearer to `a`), 1 or 2 only that one, 3
 * neither (choosing the nearer); the choice goes to *anchor. */
s32 func_80078704(VECTOR *a, VECTOR *b, Actor *actor, VECTOR **anchor) {
    s32 dz = a->vz - b->vz;
    s32 dx = b->vx - a->vx;
    s32 start = dz * actor->start.vx + dx * actor->start.vz + a->vx * b->vz - b->vx * a->vz;
    s32 first = dz * actor->unk92C.vx + dx * actor->unk92C.vz + a->vx * b->vz - b->vx * a->vz;
    s32 second = dz * actor->home.vx + dx * actor->home.vz + a->vx * b->vz - b->vx * a->vz;

    if (start < 0) {
        start = -1;
    } else if (start > 0) {
        start = 1;
    }
    if (first < 0) {
        first = -1;
    } else if (first > 0) {
        first = 1;
    }
    if (second < 0) {
        second = -1;
    } else if (second > 0) {
        second = 1;
    }
    first *= start;
    second *= start;
    if (first < 0) {
        if (second < 0) {
            VECTOR *p1 = &actor->unk92C;
            s32 d1 = func_80088838(p1, a);
            VECTOR *p2 = &actor->home;
            s32 d2 = func_80088838(p2, a);
            *anchor = d2 < d1 ? p1 : p2;
            return 0;
        }
        *anchor = &actor->unk92C;
        return 1;
    }
    if (second < 0) {
        *anchor = &actor->home;
        return 2;
    }
    {
        VECTOR *p1 = &actor->unk92C;
        s32 d1 = func_80088838(p1, a);
        VECTOR *p2 = &actor->home;
        s32 d2 = func_80088838(p2, a);
        *anchor = d1 < d2 ? p1 : p2;
    }
    return 3;
}

/* Keep two close actors apart: when their bodies overlap in height, take
 * each one's anchor across the line between their start positions, and if
 * those are closer than the actors' radii, push both apart along the line
 * between them, each by the other's share of their combined speed.
 * Declared int without a return value, as the original's unfilled delay
 * slot in the last height test shows. */
s32 func_80078920(Actor *first, Actor *second) {
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    VECTOR point_a;
    VECTOR point_b;
    VECTOR mid;
    VECTOR across;
    VECTOR *anchor_a;
    VECTOR *anchor_b;
    s32 radius_a;
    s32 radius_b;
    s32 dist;
    s32 sum;
    s32 angle; /* first the reach to the first anchor, then the push angle */
    s32 speed_a;
    s32 speed_b;
    s32 total;

    if (D_8009284C <= 0x300 && ((first->home.vy < second->unk92C.vy && second->home.vy < first->home.vy) ||
        (second->unk92C.vy < first->unk92C.vy && first->unk92C.vy < second->home.vy) ||
        (second->home.vy < first->unk92C.vy && first->home.vy < second->home.vy) ||
        (first->unk92C.vy < second->unk92C.vy && second->unk92C.vy < first->home.vy))) {
        mid.vx = (first->start.vx + second->start.vx) / 2;
        mid.vz = (first->start.vz + second->start.vz) / 2;
        across.vz = second->start.vx - first->start.vx + mid.vz;
        across.vx = first->start.vz - second->start.vz + mid.vx;
        radius_a = first->header->unk3;
        radius_b = second->header->unk3;
        func_80078704(&mid, &across, first, &anchor_a);
        func_80078704(&mid, &across, second, &anchor_b);
        point_a = *anchor_a;
        point_b = *anchor_b;
        point_a.vx -= first->start.vx;
        point_a.vy -= first->start.vy;
        point_a.vz -= first->start.vz;
        point_b.vx -= second->start.vx;
        point_b.vy -= second->start.vy;
        point_b.vz -= second->start.vz;
        point_a.vx += first->pos.vx;
        point_a.vy += first->pos.vy;
        point_a.vz += first->pos.vz;
        point_b.vx += second->pos.vx;
        point_b.vy += second->pos.vy;
        point_b.vz += second->pos.vz;
        dist = func_80088838(&point_a, &point_b);
        total = radius_a + radius_b;
        if (dist < total) {
            angle = func_80088838(&first->start, &point_a);
            if (func_80088838(&first->start, &point_b) < angle) {
                dist += total;
            } else {
                dist = total - dist;
            }
            angle = ratan2(first->start.vx - second->start.vx, first->start.vz - second->start.vz);
            speed_a = func_80088754(&first->velocity);
            speed_b = func_80088754(&second->velocity);
            sum = speed_a + speed_b;
            if (sum == 0) {
                speed_b = 1;
                speed_a = 1;
                sum = 2;
            }
            speed_a = (speed_a * dist << 8) / sum;
            speed_b = (speed_b * dist << 8) / sum;
            first->pos.vx += (func_8003F8B0(angle) * speed_b) >> 20;
            first->pos.vz += (func_8003F8CC(angle) * speed_b) >> 20;
            second->pos.vx -= (func_8003F8B0(angle) * speed_a) >> 20;
            second->pos.vz -= (func_8003F8CC(angle) * speed_a) >> 20;
        }
    }
}

/* Move an actor by its velocity: take the floor height and cell kind (bits
 * 29-30; both set also sets 0x90b), keep it in the arena, and land it on
 * the floor (a mode-4 landing bounces once, with a sound and effect). */
void func_80078D20(Actor *actor) {
    VECTOR unused; /* keeps the original's 16-byte frame slot */

    actor->floor_y = func_80082488(&actor->pos, 1);
    actor->flags = (actor->flags & ~0x60000000) |
                   ((((u32)func_800828C4(&actor->pos) >> 24) & 3) << 29);
    if ((actor->flags & 0x60000000) == 0x60000000) {
        actor->unk90B = 0xF;
    }
    func_800828F8(&actor->pos, &actor->velocity, 0x3E80);
    actor->flags &= ~0x40000;
    if (actor->floor_y < actor->pos.vy + actor->velocity.vy) {
        actor->flags |= 0x40000;
        if (actor->unkC4 == 4) {
            if (!(actor->unkD4 & 0x10)) {
                func_8008EBD0(actor, 0xD, &actor->pos, 2);
                func_800776A8(actor, 6);
            }
            actor->unkD4 |= 0x10;
            if (actor->velocity.vy >= 0x40) {
                actor->velocity.vy = -actor->velocity.vy / 3;
            } else {
                actor->velocity.vy = 0;
            }
        } else {
            actor->velocity.vy = 0;
        }
        actor->pos.vy = actor->floor_y;
    }
    actor->pos.vx += actor->velocity.vx;
    actor->pos.vy += actor->velocity.vy;
    actor->pos.vz += actor->velocity.vz;
}

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

/* Reset an actor for a new round: position and motion, model scale,
 * movement and health values, shots, trails, pose and flags, its side's hit
 * record and the combo, brain and effect state. The stance fields are
 * cleared as bit-fields (one read-modify-write of word 0xd4). */
void func_80078F00(Actor *actor) {
    SceneHeader *header = actor->header;
    s32 i;

    actor->pos.vx = actor->pos.vy = actor->pos.vz = 0;
    actor->velocity.vx = actor->velocity.vy = actor->velocity.vz = 0;
    actor->push.vx = actor->push.vy = actor->push.vz = 0;
    ((ModelSet *)actor->node->data)->scale[0] = ((ModelSet *)actor->node->data)->scale[1] =
        ((ModelSet *)actor->node->data)->scale[2] = header->unk20;
    actor->brake = actor->accel = 0x10;
    actor->unkA8 = 0x60;
    actor->max_hp = actor->unkB8 = actor->hp = 0x12C;
    actor->unkBE = 0x480;
    actor->level = 0xF0;
    actor->angle = 0;
    actor->unkC4 = 0;
    actor->unkC5 = 0;
    actor->charge = 0;
    actor->unkBA = 0;
    actor->unk910 = 0;
    actor->unkC0 = 0x7F;
    actor->unkC1 = 0x7F;
    actor->unk970 = 0;
    actor->unk9C3 = 0;
    actor->unkE8 = 0;
    actor->unk916 = 0;
    actor->unkC8 = 0;
    actor->unkCA = 0;
    for (i = 0; i < 8; i++) {
        actor->shots[i].active = 0;
        actor->shots[i].life = 0;
    }
    for (i = 0; i < 16; i++) {
        actor->trails[i].state = 0;
    }
    actor->unkF4 = 1;
    func_80076424(actor);
    actor->unk4E = 0xFF;
    actor->anim = 0;
    actor->unkC3 = 0;
    actor->flags &= ~0x1000;
    actor->flags |= 0x2000000;
    actor->flags |= 0x20000;
    actor->unkCE = actor->unkCC + 0x800;
    ACTOR_STANCE_BITS(actor)->prev_stance = 0;
    ACTOR_STANCE_BITS(actor)->stance = 0;
    actor->flags &= ~0x800000;
    actor->unk15F0 = header->unkF;
    actor->unk15F2 = actor->unk15F4 = header->unkC;
    actor->unk15F6 = 0x100;
    actor->unk15F8 = 0;
    actor->unk1654 = 0;
    actor->unk1658 = 0;
    actor->unk165C = 0;
    actor->unk1660 = 0;
    actor->unk90B = 0;
    actor->flags &= ~4;
    actor->flags &= ~2;
    actor->flags &= ~0x38;
    actor->unkD4 &= ~0x10;
    actor->unk84 = D_8009264C;
    D_8009264C[2] = 1;
    D_80096FB8[ACTOR_SIDE(actor)].unk0 = (s32)"";
    D_80096FB8[ACTOR_SIDE(actor)].unk8 = 0;
    D_80096FB8[ACTOR_SIDE(actor)].unk4 = 0;
    D_80096FB8[ACTOR_SIDE(actor)].unkC = 0;
    D_80096FB8[ACTOR_SIDE(actor)].unk10 = 0;
    func_80090CC0(actor);
    func_80078ED4((s16 *)actor->unk15D8);
    func_80087AB0(actor);
    actor->unk914 = 0;
    actor->unk1668 = 0;
    actor->unkD4 &= ~0x40;
}

/* Per-frame anchors of an actor: home between its feet, dust when landing
 * or skidding, cells thrown up while it stands in deep ground, the upper
 * anchor and core, the charged glow and its effect, and the stance effects
 * (7 and 9) when the stance changes. The stance state and last frame's
 * flag 15 are stored through bit-fields. */
void func_8007920C(Actor *actor) {
    VECTOR foot_a;
    VECTOR foot_b;
    VECTOR unused; /* keeps the original's 16-byte frame slot */
    SceneHeader *header = actor->header;
    s32 state;

    func_80073B7C(actor, header->foot_a_part, header->foot_a_vertex, &foot_a);
    func_80073B7C(actor, header->foot_b_part, header->foot_b_vertex, &foot_b);
    actor->home.vx = (foot_a.vx + foot_b.vx) / 2;
    actor->home.vy = (foot_a.vy + foot_b.vy) / 2;
    actor->home.vz = (foot_a.vz + foot_b.vz) / 2;
    if ((actor->flags & 0x80000) && (actor->flags & 0x60000000) != 0x20000000 && !(actor->kind & 1)) {
        func_8007D190(&foot_a, 9);
        func_8007D190(&foot_b, 9);
    }
    if (actor->unk90B != 0) {
        if (foot_a.vy + 0x40 > actor->floor_y - 0x20 && actor->foot_a_y + 6 < foot_a.vy) {
            func_8007D7A8(&foot_a, 0x20);
        }
        if (foot_b.vy + 0x40 > actor->floor_y - 0x20 && actor->foot_b_y + 6 < foot_b.vy) {
            func_8007D7A8(&foot_b, 0x20);
        }
        actor->unk90B--;
    }
    if ((actor->flags & 0x60000000) == 0x20000000) {
        func_8007DC74(&actor->home, &actor->start_home);
    }
    if (func_80083CD8() != 7 && (actor->flags & 0x60000000) == 0x20000000 && actor->home.vy > 0x80 &&
        actor->start_home.vy < 0x80) {
        func_8008EB88(actor, 0x3A, &actor->home, 2);
    }
    actor->foot_a_y = foot_a.vy;
    actor->foot_b_y = foot_b.vy;
    func_80073B7C(actor, header->core_part, header->core_vertex, &actor->unk92C);
    actor->unk92C.vy -= 0x38;
    actor->core.vx = (actor->home.vx + actor->unk92C.vx) / 2;
    actor->core.vz = (actor->home.vz + actor->unk92C.vz) / 2;
    actor->core.vy = (actor->home.vy + actor->unk92C.vy) / 2;
    if ((actor->flags & 0x8000) && actor->unkC5 == 0 && (actor->unkC4 & 1)) {
        func_8007C100(&actor->colour);
        func_8007CD44(ACTOR_SIDE(actor), &actor->unk92C, &actor->home, ACTOR_SIDE(actor));
        if (!(actor->flags & 0x10000)) {
            func_8008ED6C(actor, 8);
            func_8008EBD0(actor, 8, &actor->home, 2);
        }
    }
    if (actor->unkC5 != 4) {
        if (actor->unkC4 != 1) {
            ACTOR_STANCE_BITS(actor)->stance = 0;
        } else {
            ACTOR_STANCE_BITS(actor)->stance = 1;
            if (actor->flags & 0x8000) {
                ACTOR_STANCE_BITS(actor)->stance = 2;
            }
        }
        if (actor->unkC5 == 2) {
            ACTOR_STANCE_BITS(actor)->stance = 3;
        }
    }
    state = ACTOR_STANCE_BITS(actor)->stance;
    if (state != ACTOR_STANCE_BITS(actor)->prev_stance) {
        switch (state) {
        case 0:
        case 3:
            func_8008ED6C(actor, 7);
            func_8008ED6C(actor, 9);
            break;
        case 1:
            func_8008EBD0(actor, 7, &actor->pos, 2);
            func_8008ED6C(actor, 9);
            break;
        case 2:
            func_8008EBD0(actor, 9, &actor->home, 2);
            func_8008ED6C(actor, 7);
            break;
        }
    }
    ACTOR_FLAG_BITS(actor)->flag16 = ACTOR_FLAG_BITS(actor)->flag15;
    actor->unkC6 = actor->unkC4;
    ACTOR_STANCE_BITS(actor)->prev_stance = ACTOR_STANCE_BITS(actor)->stance;
}

/* Frame both actors with the camera: look at their midpoint, choose the
 * side (left or right of the line between them) whose eye point is nearer
 * the current one (the comparison reads the heights uninitialised, as the
 * original does), and ease the eye there, above the ground. */
void func_800796B8(Actor *first, Actor *second) {
    VECTOR eye;
    VECTOR step;
    s32 angle;
    s32 dist;
    s32 radius;
    s32 floor;

    if (D_8009293C != 0) {
        return;
    }
    angle = ratan2(first->pos.vx - second->pos.vx, first->pos.vz - second->pos.vz);
    dist = func_80088838(&first->pos, &second->pos);
    D_80092934 = angle;
    D_8009284C = dist;
    dist = func_800887A4(&first->pos, &second->pos);
    radius = dist * 2 / 3 + 0xC0;
    D_8009867C.vx = (first->pos.vx + second->pos.vx) / 2;
    D_8009867C.vy = (first->pos.vy + second->pos.vy) / 2 - 0xA0;
    D_8009867C.vz = (first->pos.vz + second->pos.vz) / 2;
    eye.vx = D_8009867C.vx + ((func_8003F8B0(angle - 0x400) * radius) >> 12);
    eye.vz = D_8009867C.vz + ((func_8003F8CC(angle - 0x400) * radius) >> 12);
    step.vx = D_8009867C.vx + ((func_8003F8B0(angle + 0x400) * radius) >> 12);
    step.vz = D_8009867C.vz + ((func_8003F8CC(angle + 0x400) * radius) >> 12);
    eye.vx -= D_8009871C.vx;
    eye.vy -= D_8009871C.vy;
    eye.vz -= D_8009871C.vz;
    step.vx -= D_8009871C.vx;
    step.vy -= D_8009871C.vy;
    step.vz -= D_8009871C.vz;
    floor = func_80088754(&eye);
    if (func_80088754(&step) < floor) {
        D_8009290C = 0x400;
        D_800928F4 = 0;
    } else {
        D_8009290C = -0x400;
        D_800928F4 = 1;
    }
    dist /= 4;
    if (dist > 0x300) {
        dist = 0x300;
    }
    eye.vy = D_8009867C.vy - 0x40 - dist;
    eye.vx = D_8009867C.vx + ((func_8003F8B0(angle + D_8009290C) * radius) >> 12);
    eye.vz = D_8009867C.vz + ((func_8003F8CC(angle + D_8009290C) * radius) >> 12);
    step.vx = (eye.vx - D_8009871C.vx) / D_8009287C;
    step.vz = (eye.vz - D_8009871C.vz) / D_8009287C;
    func_800828F8(&D_8009871C, &step, 0x3A00);
    D_8009871C.vx += step.vx;
    D_8009871C.vz += step.vz;
    floor = func_80082488(&D_8009871C, 0) - 0x100;
    if (floor < eye.vy) {
        eye.vy = floor;
    }
    D_8009871C.vy += (eye.vy - D_8009871C.vy) / D_8009287C;
    D_8009287C = 0x64;
}

/* Reset the bout: effects, glow and the round settings. */
void func_80079A8C(void) {
    func_800732CC();
    func_8008DC28();
    func_80088AF8();
    D_80099D98.com1 = 1;
    D_80099D98.speed = 3;
    D_8009292C = 0x100;
    D_80099D98.driven = 0;
    D_80099D98.rate = 0;
    D_80099D98.command = 0;
    D_80099D98.unkC = 0x100;
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
    D_80091130[10].kind = 0;
    D_80091130[10].next = 0;
    D_80092664 = 0;
    D_80092950++;
    func_800346A4(&D_8009868C);
    D_80099D98.rate = 0;
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

/* Restore actor flags 2, 15 and 19 from its pose. */
void func_80079D6C(Actor *actor) {
    actor->flags = (actor->flags & ~4) | ((actor->pose->unkA >> 6) & 4);
    actor->flags = (actor->flags & ~0x8000) | ((actor->pose->flags << 2) & 0x8000);
    actor->flags = (actor->flags & ~0x80000)
                 | ((((PoseFlagBits *)&actor->pose->flags)->flag19 & 1) << 19);
}

/* Clear D_80092640. */
void func_80079DE0(void) {
    D_80092640 = 0;
}

/* One frame of the bout (every frame rate setting + 1 frames): record the
 * pose slot, read the pads (letting a player on the free port take over
 * while the pause is open), then run both actors' frame: moves, AI,
 * physics, separation, model placement, anchors and frame events. The
 * settings are read through the Settings struct, which keeps the rate
 * load ahead of the pose stores. */
void func_80079DF0(Actor *first, Actor *second) {
    if (D_80092664 != 0) {
        D_80092664--;
        return;
    }
    D_80092664 = D_80099D98.rate;
    first->pose = (Pose *)first->unk9CC + D_800928C0;
    second->pose = (Pose *)second->unk9CC + D_800928C0;
    first->move = (Move *)first->pose;
    D_800928C0++;
    second->move = (Move *)second->pose;
    first->flags = (first->flags & ~0x40) | ((D_80099D98.com1 & 1) << 6);
    second->flags = (second->flags & ~0x40) | ((D_80099D98.driven & 1) << 6);
    func_80073644(first);
    func_80073644(second);
    func_800764CC(first);
    func_800764CC(second);
    D_80092648 = 0;
    func_80075B50(first);
    func_80075B50(second);
    func_8003708C(0xA, 0x60);
    func_8007E528(0);
    if (D_800928D4 != 0) {
        if (func_80036410()) {
            func_80035DB0();
        } else {
        poll:
            if (func_80035CDC()) {
                if ((((D_8005948C | D_80059490) & 0x800) && D_8009263C < 0x14) || !func_80035734(0) ||
                    (!func_80035734(1) && D_800928C8 == 2)) {
                    if ((D_8005948C & 0x800) || !func_80035734(0)) {
                        D_800928FC = 0;
                    } else {
                        if (second->flags & 0x40) {
                            goto next;
                        }
                        D_800928FC = 1;
                    }
                    if (D_800928C4 == 0) {
                        func_80080C48(D_800928C8 == 4 ? 2 : 1);
                    }
                }
            next:
                func_80076884(first);
                func_80076884(second);
                goto poll;
            }
        }
    } else {
        func_80036420();
    }
    if (D_800928D4 != 0) {
        if (D_80092944 != 0x2BF1F) {
            D_80092944++;
        }
        if (D_800928C8 == 4) {
            func_8008F280(second);
        } else {
            func_80090E10(first);
            func_80090E10(second);
        }
    }
    if (D_80092638 != 0) {
        first->state = 0;
        second->state = 0;
        func_80076424(first);
        func_80076424(second);
    }
    func_800751C8(first, second);
    func_80077038(first);
    func_80077038(second);
    func_8003708C(0x4A, 0);
    func_80077A9C(first);
    func_8003708C(0x6A, 0);
    func_80077A9C(second);
    func_8003708C(0xA, 0x80);
    func_80078194(first);
    func_80078194(second);
    func_80078D20(second);
    func_80078D20(first);
    func_80078920(first, second);
    func_80078E94(second);
    func_80078E94(first);
    func_80072170();
    func_80074BA4(first);
    func_80074BA4(second);
    func_80079D08(first);
    func_80079D08(second);
    func_8007920C(first);
    func_8007920C(second);
    func_80074678(first, first->unk998, first->unk99A);
    func_80074678(second, second->unk998, second->unk99A);
    func_8007BACC();
    if (D_80092884 != 0) {
        func_8007D65C(&first->core, &second->core, 0x13);
    }
}

/* Save both actors' positions and homes (at height 0x100) and set the
 * countdown from the given frame count. */
void func_8007A21C(s32 frames) {
    if (frames < 0xFF) {
        D_800928AC = frames - 2;
        D_800928C0 -= frames;
    } else {
        D_800928AC = 0xFF;
    }
    D_80092A34[0] = D_8009872C.pos;
    D_80092A34[1] = D_80097010.pos;
    D_80092A34[2] = D_8009872C.home;
    D_80092A34[3] = D_80097010.home;
    D_80092A34[0].vy = D_80092A34[1].vy = D_80092A34[2].vy = D_80092A34[3].vy = 0x100;
}

/* One frame of a replay: step both actors to the next recorded pose (the
 * first frame after a reset shows every part), show "REPLAY", then run
 * the actors' frame as in play and count down the replay. */
void func_8007A344(Actor *first, Actor *second) {
    u8 frame = D_800928C0++;

    first->pose = (Pose *)first->unk9CC + frame;
    second->pose = (Pose *)second->unk9CC + frame;
    first->move = (Move *)((Pose *)first->unk9CC + D_800928C0);
    second->move = (Move *)((Pose *)second->unk9CC + D_800928C0);
    if (D_800928AC == 0xFF) {
        first->move->flags |= 0x1000;
        second->move->flags |= 0x1000;
        first->move->unkA = 1;
        second->move->unkA = 1;
        first->move->unk9 = 1;
        second->move->unk9 = 1;
    }
    if (D_800928E8 & 8) {
        func_8007E894(0x10, 0x10);
        func_8007EBE0("REPLAY");
    }
    func_80073644(first);
    func_80073644(second);
    func_800764CC(first);
    func_800764CC(second);
    first->start_home = first->home;
    second->start_home = second->home;
    first->start = first->pos;
    second->start = second->pos;
    D_80092648 = 0;
    if (D_8009287C >= 2) {
        D_8009287C = 1;
    }
    func_80036420();
    if (D_800928FC == 1) {
        if (D_80059490 & 0x20) {
            func_80083C0C(8);
        }
        func_800832C0(D_80059574);
    } else {
        if (D_8005948C & 0x20) {
            func_80083C0C(8);
        }
        func_800832C0(D_80059570);
    }
    func_80074AB4(first);
    func_80074AB4(second);
    func_80078E94(first);
    func_80078E94(second);
    first->flags = (first->flags & ~0x60000000) | ((func_800828C4(&first->pos) & 0x3000000) << 5);
    second->flags = (second->flags & ~0x60000000) | ((func_800828C4(&second->pos) & 0x3000000) << 5);
    func_80079D6C(first);
    func_80079D6C(second);
    func_8007920C(first);
    func_8007920C(second);
    func_80075B50(first);
    func_80075B50(second);
    func_8007BACC();
    if (D_80092884 != 0) {
        func_8007D65C(&first->core, &second->core, 0x13);
    }
    if (D_800928AC == 0xFF) {
        func_8008369C();
    }
    if (D_800928AC == 0) {
        func_80083C0C(8);
    }
    D_800928AC--;
}

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

/* Point the camera at an actor for its victory view: height and distance
 * from its move header, a random direction around it. */
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
    D_8009867C.vy = actor->pos.vy;
    D_8009867C.vx = actor->pos.vx;
    D_8009867C.vz = actor->pos.vz;
    y = D_8009867C.vy - 0x400;
    D_8009871C.vy = y - D_80092670;
    D_8009871C.vx = D_8009867C.vx + (((func_8003F8B0(angle) << 2) * D_80092674) >> 12);
    D_8009871C.vz = D_8009867C.vz + (((func_8003F8CC(angle) << 2) * D_80092674) >> 12);
}

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
        D_80091130[16].kind = 1;
        D_80091130[16].next = 0x10;
    } else {
        D_80091130[16].kind = 2;
        D_80091130[16].next = -1;
    }
    D_8009872C.pos.vy = 0x100;
    D_80097010.pos.vy = 0x100;
    D_8009872C.start.vy = 0x100;
    D_80097010.start.vy = 0x100;
}

/* Follow an actor with the camera: look at its core at the reference
 * height, place the eye behind it by the camera angle, height and length
 * (tunable with the pad in debug), and back the look-at point off until it
 * is at least 0x200 away. */
void func_8007A958(Actor *actor) {
    VECTOR target;
    u16 held;

    if (D_800911D4 != 0) {
        held = D_80059570;
        if (held & 0x1000) {
            D_8009266C += 4;
        }
        if (held & 0x4000) {
            D_8009266C -= 4;
        }
        if (held & 0x10) {
            D_80092670 += 4;
        }
        if (held & 0x40) {
            D_80092670 -= 4;
        }
        if (held & 0x2000) {
            D_80092668 -= 0x20;
        }
        if (held & 0x8000) {
            D_80092668 += 0x20;
        }
        if (held & 8) {
            D_80092674 -= 0x10;
        }
        if (held & 2) {
            D_80092674 += 0x10;
        }
        func_800379C8("ANG %x\n", D_80092668 & 0xFFF);
        func_800379C8("REF %x\n", D_8009266C);
        func_800379C8("CAM %x\n", D_80092670);
        func_800379C8("LEN %x\n", D_80092674);
    }
    target = actor->core;
    target.vy = actor->pos.vy - D_8009266C;
    func_80070808(&target, 8);
    target.vy = actor->pos.vy - D_80092670;
    target.vx = actor->pos.vx + ((func_8003F8B0(actor->angle + D_80092668) * D_80092674) >> 12);
    target.vz = actor->pos.vz + ((func_8003F8CC(actor->angle + D_80092668) * D_80092674) >> 12);
    func_800708C4(&target, 0x10);
    while (func_800887A4(&D_8009871C, &actor->pos) < 0x200) {
        D_8009871C.vy -= 2;
        D_8009871C.vx -= 2;
    }
}

/* Restore the saved positions and homes, make them the round start and
 * set up the camera on the leading actor. */
void func_8007AC3C(void) {
    D_800928AC = 0x96;
    D_8009292C = 0x100;
    SetGeomScreen(0x200);
    D_8009872C.pos = D_80092A34[0];
    D_80097010.pos = D_80092A34[1];
    D_8009872C.home = D_80092A34[2];
    D_80097010.home = D_80092A34[3];
    D_8009872C.start_home = D_8009872C.home;
    D_80097010.start_home = D_80097010.home;
    D_8009872C.start = D_8009872C.pos;
    D_80097010.start = D_80097010.pos;
    if (D_80092890 != 0) {
        func_8007A768(&D_80097010);
    } else {
        func_8007A768(&D_8009872C);
    }
}

extern char D_8006FDD8[];

/* Update the bout-result view: finish its effects once, allow the selected
 * controller to leave, show the winner's caption for the current bout mode,
 * keep the camera above the ground, and pose both actors for victory/defeat.
 * Debug bouts repeat the result timer unless both actors are computer driven. */
void func_8007AE10(Actor *first, Actor *second) {
    char text[64];
    s32 held;
    s32 ground;

    if (D_800928AC == 0x95) {
        func_8007A884();
    }
    first->flags &= ~0x40;
    second->flags &= ~0x40;
    func_800764CC(first);
    func_800764CC(second);
    D_80092648 = 0;
    func_80036420();
    if (D_800928FC == 1) {
        held = D_80059490 & 0x20;
    } else {
        held = D_8005948C & 0x20;
    }
    if (held != 0) {
        func_80083C0C(2);
    }
    D_800928AC--;
    if (D_800928AC == -1) {
        if (D_800911D4 == 0 || D_800928C8 == 3) {
            func_80083C0C(2);
        } else {
            first->flags |= 0x2000000;
            second->flags |= 0x2000000;
            D_800928AC = 0x95;
        }
    }
    func_8007E894(0xA0, 0xA0);
    switch (D_80092890) {
    case 0:
        func_8007A958(first);
        switch (D_800928C8) {
        case 1:
            func_8007EC54("YOU WERE VICTORIOUS");
            break;
        case 2:
            func_8007EC54("1PLAYER VICTORY");
            break;
        case 3:
            func_8007EC54("COM1 VICTORY");
            break;
        }
        break;
    case 1:
        func_8007A958(second);
        switch (D_800928C8) {
        case 1:
            func_8007EC54("YOU WERE DEFEATED");
            sprintf(text, "     BY %s", D_80091964[second->model_id].name);
            func_8007EC54(text);
            break;
        case 2:
            func_8007EC54("2PLAYER VICTORY");
            break;
        case 3:
            func_8007EC54(D_8006FDD8);
            break;
        }
        break;
    case 2:
        func_8007EC54("DRAW GAME");
        break;
    }
    ground = func_80082488(&D_8009871C, 0) - D_80092670;
    if (ground < D_8009871C.vy) {
        D_8009871C.vy = ground;
    }
    first->state = 0;
    second->state = 0;
    func_80076424(first);
    func_80076424(second);
    func_800764CC(first);
    func_800764CC(second);
    D_80092648 = 0;
    func_80078194(first);
    func_80078194(second);
    func_80078D20(second);
    func_80078D20(first);
    func_80078920(first, second);
    func_80078E94(second);
    func_80078E94(first);
    if (first->flags & 0x800000) {
        func_8007A6D0(second);
        func_8007A730(first);
    } else {
        func_8007A6D0(first);
        func_8007A730(second);
    }
    func_80074BA4(first);
    func_80074BA4(second);
    func_8007920C(first);
    func_8007920C(second);
    func_80074678(first, first->unk998, first->unk99A);
    func_80074678(second, second->unk998, second->unk99A);
    func_8007BACC();
}

/* This original string also contains nonzero bytes after its terminator. */
INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu3", D_8006FDD8);

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
    D_800926A0 = LoadClut2((u_long *)D_800926A8, D_80092698, D_8009269C);
}

/* Load the effect textures from the scene file table: the twelve frames
 * of sparkle kind 0, the textures of kinds 1-4 (kind 3 is kind 1 drawn
 * additively), the sparkle packets, and the two other effect textures. */
void func_8007B388(u_long **files) {
    TIM_IMAGE tim;
    RECT unused; /* the original frame has 8 unused bytes */
    s16 *clut;
    Sparkle *sparkle;
    u8 first = D_800911E8[0] == 0;
    s32 i;

    for (i = 0; i < 12; i++) {
        OpenTIM(files[i + 1]);
        ReadTIM(&tim);
        D_800947E8[i] = (u8)tim.prect->x * 4;
        D_800947F4[i] = tim.prect->y;
        D_80094800[i] = GetClut(tim.crect->x, tim.crect->y);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
    D_80092A74[0].u = D_800947E8;
    D_80092A74[0].v = D_800947F4;
    D_80092A74[0].w = 0x26;
    D_80092A74[0].h = 0x26;
    D_80092A74[0].clut = 0;
    D_80092A74[0].tpage = GetTPage(0, 1, 0x3C0, 0x100);
    D_80092A74[0].frame_count = 0xC;
    D_80092A74[0].gravity = 0;
    D_80092A74[0].unkB = 0;

    OpenTIM(files[13]);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    if (first) {
        for (i = 0; i < 16; i++) {
            D_800911E8[i] += tim.prect->y;
        }
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_80092A74[1].u = D_800911D8;
    D_80092A74[1].v = D_800911E8;
    D_80092A74[1].w = 0x1F;
    D_80092A74[1].h = 0x1F;
    D_80092A74[1].clut = GetClut(tim.crect->x, tim.crect->y);
    D_80092A74[1].tpage = GetTPage(0, 1, tim.prect->x, tim.prect->y);
    D_80092A74[1].frame_count = 0x10;
    D_80092A74[1].gravity = -2;
    D_80092A74[1].unkB = 0;

    OpenTIM(files[16]);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    if (first) {
        for (i = 0; i < 12; i++) {
            D_80091208[i] += tim.prect->y;
        }
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_80092A74[2].u = D_800911F8;
    D_80092A74[2].v = D_80091208;
    D_80092A74[2].w = 0xF;
    D_80092A74[2].h = 0xF;
    D_80092A74[2].clut = GetClut(tim.crect->x, tim.crect->y);
    D_80092A74[2].tpage = GetTPage(0, 1, tim.prect->x, tim.prect->y);
    D_80092A74[2].frame_count = 0xC;
    D_80092A74[2].gravity = 0;
    D_80092A74[2].unkB = 1;

    D_80092A74[3] = D_80092A74[1];
    D_80092A74[3].tpage = (D_80092A74[3].tpage & ~0x60) | 0x40;

    OpenTIM(files[26]);
    ReadTIM(&tim);
    if (first) {
        for (i = 0; i < 16; i++) {
            D_80091218[i] += tim.prect->y;
        }
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_80092A74[4].u = D_800911F8;
    D_80092A74[4].v = D_80091218;
    D_80092A74[4].w = 0xF;
    D_80092A74[4].h = 0xF;
    D_80092A74[4].clut = GetClut(tim.crect->x, tim.crect->y);
    D_80092A74[4].tpage = GetTPage(0, 1, tim.prect->x, tim.prect->y);
    D_80092A74[4].frame_count = 0x10;
    D_80092A74[4].gravity = 0;
    D_80092A74[4].unkB = 2;

    sparkle = D_80092AD8;
    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        setlen(&sparkle->prim[0], 9);
        *(u32 *)&sparkle->prim[0].r0 = 0x2C808080;
        sparkle->prim[0].code |= 2;
        sparkle->prim[1] = sparkle->prim[0];
        sparkle->active = 0;
        sparkle->frame = 0;
    }

    OpenTIM(files[14]);
    ReadTIM(&tim);
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_80092678 = tim.prect->x;
    D_8009267C = tim.prect->y;
    D_80092680 = GetClut(tim.crect->x, tim.crect->y);
    D_80092684 = GetTPage(0, 2, tim.prect->x, tim.prect->y);

    OpenTIM(files[17]);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_80092688 = (u8)((u16)tim.prect->x * 4);
    D_8009268C = (u8)tim.prect->y;
    D_80092690 = GetClut(tim.crect->x, tim.crect->y);
    D_80092694 = GetTPage(0, 1, tim.prect->x, tim.prect->y);
    D_800926A4 = 0x8000;
    D_80092698 = tim.crect->x;
    D_8009269C = tim.crect->y + 1;
    func_8007D6B8();
    func_8007DB28();
    func_8007E2D8();
}

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

/* Link a projected primitive of the given length tag into the ordering
 * table at the depth left in the scratchpad. */
#define LINK_PRIM(ot, scratch, prim, len)                                      \
    prev = (ot)[(scratch)->depth >> 4];                                        \
    addr = (u32)(prim) & 0xFFFFFF;                                             \
    (ot)[(scratch)->depth >> 4] = addr;                                        \
    prev |= (len);                                                             \
    *(u32 *)addr = prev

/* Draw the falling sparkles as camera-facing quads: rotate the corner
 * offsets of the three sprite sizes by the local matrix once, then project
 * each type-0 sparkle's position plus the offsets of its size. */
void func_8007BBA0(MATRIX *view, MATRIX *local, u32 *ot) {
    SceneScratch *scratch = SCENE_SCRATCH;
    Sparkle *sparkle;
    SparkleKind *kind;
    POLY_FT4 *prim;
    s32 i;
    u32 prev;
    u32 addr;

    scratch->point.vz = 0;
    gte_SetRotMatrix(local);
    scratch->point.vx = scratch->point.vy = -0x60;
    gte_ldv0(&scratch->point);
    gte_rtv0();
    gte_stlvnl(&scratch->corner[0]);
    scratch->point.vx = scratch->point.vy = 0x60;
    gte_ldv0(&scratch->point);
    gte_rtv0();
    gte_stlvnl(&scratch->corner[1]);
    scratch->point.vx = scratch->point.vy = -0x48;
    gte_ldv0(&scratch->point);
    gte_rtv0();
    gte_stlvnl(&scratch->corner[2]);
    scratch->point.vx = scratch->point.vy = 0x48;
    gte_ldv0(&scratch->point);
    gte_rtv0();
    gte_stlvnl(&scratch->corner[3]);
    scratch->point.vx = scratch->point.vy = -0x20;
    gte_ldv0(&scratch->point);
    gte_rtv0();
    gte_stlvnl(&scratch->corner[4]);
    scratch->point.vx = scratch->point.vy = 0x20;
    gte_ldv0(&scratch->point);
    gte_rtv0();
    gte_stlvnl(&scratch->corner[5]);
    gte_SetRotMatrix(view);
    gte_SetTransMatrix(view);
    for (sparkle = D_80092AD8, i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active || sparkle->type != 0) {
            continue;
        }
        kind = sparkle->u.fall.kind;
        scratch->point.vx = sparkle->x - scratch->camera.vx;
        scratch->point.vy = sparkle->y - scratch->camera.vy;
        scratch->point.vz = sparkle->z - scratch->camera.vz;
        scratch->to = scratch->point;
        prim = &sparkle->prim[D_800928A0];
        switch (kind->unkB) {
        case 0:
            scratch->point.vx += scratch->corner[0].vx;
            scratch->point.vy += scratch->corner[0].vy;
            scratch->point.vz += scratch->corner[0].vz;
            scratch->to.vx += scratch->corner[1].vx;
            scratch->to.vy += scratch->corner[1].vy;
            scratch->to.vz += scratch->corner[1].vz;
            break;
        case 1:
            scratch->point.vx += scratch->corner[2].vx;
            scratch->point.vy += scratch->corner[2].vy;
            scratch->point.vz += scratch->corner[2].vz;
            scratch->to.vx += scratch->corner[3].vx;
            scratch->to.vy += scratch->corner[3].vy;
            scratch->to.vz += scratch->corner[3].vz;
            break;
        case 2:
            scratch->point.vx += scratch->corner[4].vx;
            scratch->point.vy += scratch->corner[4].vy;
            scratch->point.vz += scratch->corner[4].vz;
            scratch->to.vx += scratch->corner[5].vx;
            scratch->to.vy += scratch->corner[5].vy;
            scratch->to.vz += scratch->corner[5].vz;
            break;
        }
        gte_ldv01(&scratch->point, &scratch->to);
        gte_rtpt();
        gte_stsxy01(&prim->x0, &prim->x3);
        gte_stsz2(&scratch->depth);
        prim->x1 = prim->x3;
        prim->y1 = prim->y0;
        prim->x2 = prim->x0;
        prim->y2 = prim->y3;
        prim->u0 = kind->u[sparkle->frame - 1];
        prim->v0 = kind->v[sparkle->frame - 1];
        prim->u1 = prim->u0 + kind->w;
        prim->v1 = prim->v0;
        prim->u2 = prim->u0;
        prim->v2 = prim->v0 + kind->h;
        prim->u3 = prim->u0 + kind->w;
        prim->v3 = prim->v0 + kind->h;
        if ((s16)kind->clut == 0) {
            prim->clut = D_80094800[sparkle->frame - 1];
        }
        if (scratch->depth > 0x40) {
            scratch->depth -= 0x40;
        }
        LINK_PRIM(ot, scratch, prim, 0x09000000);
    }
}

/* Set the colour of kind-2 sparkles. */
void func_8007C100(CVECTOR *color) {
    D_800926B8 = *color;
}

/* Start a sparkle of the given kind at a position, in the first free slot. */
void func_8007C124(SVECTOR *pos, s32 kind) {
    Sparkle *sparkle = D_80092AD8;
    SparkleKind *info;
    POLY_FT4 *prim;
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

/* Draw the trail sparkles as quads from the previous segment's edge to an
 * edge across the direction of travel on screen (vertical for the first
 * segment), sized by the trail and shaded by age. */
void func_8007C280(MATRIX *view, MATRIX *local, u32 *ot) {
    SceneScratch *scratch = SCENE_SCRATCH;
    Sparkle *sparkle;
    SparkleTrail *prev;
    POLY_FT4 *prim;
    s32 i;
    s32 dx;
    s32 dy;
    s32 tmp; /* the segment length, then the shade, then the link address */
    s32 nx;
    s32 ny;
    s32 side;
    u32 prevlink;

    scratch->from.vx = 0;
    scratch->from.vz = 0;
    gte_SetRotMatrix(view);
    gte_SetTransMatrix(view);
    for (sparkle = D_80092AD8, i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active || sparkle->type != 1) {
            continue;
        }
        scratch->point.vx = sparkle->x - scratch->camera.vx;
        scratch->point.vy = sparkle->y - scratch->camera.vy;
        scratch->point.vz = sparkle->z - scratch->camera.vz;
        scratch->to = scratch->point;
        prim = &sparkle->prim[D_800928A0];
        gte_ldv0(&scratch->point);
        gte_rtps();
        gte_stsxy(&prim->x0);
        sparkle->u.trail.screen[4] = prim->x0;
        sparkle->u.trail.screen[5] = prim->y0;
        if (sparkle->u.trail.prev != NULL) {
            prev = &sparkle->u.trail.prev->u.trail;
            dx = sparkle->u.trail.screen[4] - prev->screen[4];
            dy = sparkle->u.trail.screen[5] - prev->screen[5];
            tmp = SquareRoot0(dx * dx + dy * dy);
            nx = dx * sparkle->u.trail.size / tmp;
            ny = dy * sparkle->u.trail.size / tmp;
            scratch->point.vz = 0;
            scratch->point.vy = -nx;
            scratch->point.vx = ny;
            gte_SetRotMatrix(local);
            gte_ldv0(&scratch->point);
            gte_rtv0();
            gte_stlvnl(&scratch->corner[2]);
            scratch->point.vy = nx;
            scratch->point.vx = -ny;
            gte_ldv0(&scratch->point);
            gte_rtv0();
            gte_stlvnl(&scratch->corner[3]);
            scratch->point.vx = sparkle->x - scratch->camera.vx;
            scratch->point.vy = sparkle->y - scratch->camera.vy;
            scratch->point.vz = sparkle->z - scratch->camera.vz;
            scratch->to = scratch->point;
        } else {
            gte_SetRotMatrix(local);
            scratch->from.vy = -sparkle->u.trail.size;
            gte_ldv0(&scratch->from);
            gte_rtv0();
            gte_stlvnl(&scratch->corner[2]);
            scratch->from.vy = sparkle->u.trail.size;
            gte_ldv0(&scratch->from);
            gte_rtv0();
            gte_stlvnl(&scratch->corner[3]);
        }
        scratch->point.vx += scratch->corner[2].vx;
        scratch->point.vy += scratch->corner[2].vy;
        scratch->point.vz += scratch->corner[2].vz;
        scratch->to.vx += scratch->corner[3].vx;
        scratch->to.vy += scratch->corner[3].vy;
        scratch->to.vz += scratch->corner[3].vz;
        gte_SetRotMatrix(view);
        gte_SetTransMatrix(view);
        gte_ldv01(&scratch->point, &scratch->to);
        gte_rtpt();
        gte_stsxy01(&prim->x0, &prim->x1);
        gte_stsz2(&scratch->depth);
        sparkle->u.trail.screen[0] = prim->x0;
        sparkle->u.trail.screen[1] = prim->y0;
        sparkle->u.trail.screen[2] = prim->x1;
        sparkle->u.trail.screen[3] = prim->y1;
        if (sparkle->u.trail.prev == NULL) {
            continue;
        }
        /* which side of the line through both centres each edge start is */
        side = ((prev->screen[5] - sparkle->u.trail.screen[5]) * sparkle->u.trail.screen[0] +
                (sparkle->u.trail.screen[4] - prev->screen[4]) * sparkle->u.trail.screen[1] +
                prev->screen[4] * sparkle->u.trail.screen[5] - sparkle->u.trail.screen[4] * prev->screen[5]) *
               ((prev->screen[5] - sparkle->u.trail.screen[5]) * prev->screen[0] +
                (sparkle->u.trail.screen[4] - prev->screen[4]) * prev->screen[1] +
                prev->screen[4] * sparkle->u.trail.screen[5] - sparkle->u.trail.screen[4] * prev->screen[5]);
        if (side > 0) {
            prim->x2 = prev->screen[0];
            prim->y2 = prev->screen[1];
            prim->x3 = prev->screen[2];
            prim->y3 = prev->screen[3];
        } else {
            prim->x3 = prev->screen[0];
            prim->y3 = prev->screen[1];
            prim->x2 = prev->screen[2];
            prim->y2 = prev->screen[3];
        }
        tmp = 0x40 - sparkle->frame * 8;
        prim->r0 = tmp;
        prim->g0 = tmp;
        prim->b0 = tmp;
        prevlink = ot[scratch->depth >> 4];
        tmp = (u32)prim & 0xFFFFFF;
        ot[scratch->depth >> 4] = tmp;
        prevlink |= 0x09000000;
        *(u32 *)tmp = prevlink;
    }
}

/* Start a trail segment of a key at a position for the current owner
 * (once per key and owner), with the given texture column and size, linked
 * to the segment started on the previous frame. */
void func_8007C880(s32 column, VECTOR *pos, s32 key, s32 size) {
    Sparkle *sparkle;
    Sparkle *other;
    POLY_FT4 *prim;
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
    prim->tpage = D_80092694;
    prim->clut = D_800926A0;
    prim->code &= ~1;
    sparkle->prim[1] = *prim;
    sparkle->frame_count = 7;
    sparkle->type = 1;
    sparkle->active = 1;
    sparkle->frame = 0;
    sparkle->x = pos->vx;
    sparkle->y = pos->vy;
    sparkle->z = pos->vz;
    sparkle->u.trail.owner = D_800928E8;
    sparkle->u.trail.key = key;
    sparkle->u.trail.prev = NULL;
    sparkle->u.trail.stamp = D_800926A4;
    sparkle->u.trail.size = D_80091228[size];
    for (other = D_80092AD8, i = 0; i < SPARKLE_COUNT; i++, other++) {
        if (other->active && other->u.trail.key == key && other != sparkle && other->type == 1 &&
            other->u.trail.stamp == (u16)(D_800926A4 - 1)) {
            sparkle->u.trail.prev = other;
        }
    }
}

/* Draw the line sparkles that continue last frame's segment as quads
 * joining both segments, fading with their age. */
void func_8007CAA4(MATRIX *view, MATRIX *unused, u32 *ot) {
    SceneScratch *scratch = SCENE_SCRATCH;
    Sparkle *sparkle;
    POLY_FT4 *prim;
    s32 i;
    u32 prev;
    u32 addr;

    gte_SetRotMatrix(view);
    gte_SetTransMatrix(view);
    for (sparkle = D_80092AD8, i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active || sparkle->type != 2 || sparkle->u.line.prev == NULL) {
            continue;
        }
        prim = &sparkle->prim[D_800928A0];
        scratch->point.vx = sparkle->x - scratch->camera.vx;
        scratch->point.vy = sparkle->y - scratch->camera.vy;
        scratch->point.vz = sparkle->z - scratch->camera.vz;
        scratch->from.vx = sparkle->u.line.x - scratch->camera.vx;
        scratch->from.vy = sparkle->u.line.y - scratch->camera.vy;
        scratch->from.vz = sparkle->u.line.z - scratch->camera.vz;
        scratch->to.vx = sparkle->u.line.prev->x - scratch->camera.vx;
        scratch->to.vy = sparkle->u.line.prev->y - scratch->camera.vy;
        scratch->to.vz = sparkle->u.line.prev->z - scratch->camera.vz;
        scratch->extra.vx = sparkle->u.line.prev->u.line.x - scratch->camera.vx;
        scratch->extra.vy = sparkle->u.line.prev->u.line.y - scratch->camera.vy;
        scratch->extra.vz = sparkle->u.line.prev->u.line.z - scratch->camera.vz;
        gte_ldv3(&scratch->point, &scratch->from, &scratch->to);
        gte_rtpt();
        gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
        gte_stsz(&scratch->depth);
        gte_ldv0(&scratch->extra);
        gte_rtps();
        gte_stsxy(&prim->x3);
        /* the shade shares its register with the link address */
        addr = 0x40 - sparkle->frame * 8;
        prim->r0 = addr;
        prim->g0 = addr;
        prim->b0 = addr;
        LINK_PRIM(ot, scratch, prim, 0x09000000);
    }
}

/* Pack four fields into one word: top byte, 16-bit middle, flag bit 7 and
 * a 7-bit low field. */
u32 func_8007CD14(s32 flag, s32 top, s32 middle, s32 low) {
    return (low & 0x7F) | ((flag << 7) & 0x80) | (top << 24) | ((middle << 8) & 0xFFFF00);
}

/* Start a line segment of a key between two positions for the current
 * owner (once per key and owner), with the given texture column, linked to
 * the segment started on the previous frame. */
void func_8007CD44(s32 column, VECTOR *from, VECTOR *to, s32 key) {
    Sparkle *sparkle;
    Sparkle *other;
    POLY_FT4 *prim;
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
    sparkle->z = from->vz;
    sparkle->u.line.owner = D_800928E8;
    sparkle->u.line.x = to->vx;
    sparkle->u.line.y = to->vy;
    sparkle->u.line.z = to->vz;
    sparkle->u.line.stamp = D_800926A4;
    prim = sparkle->prim;
    prim->u0 = prim->u1 = prim->u2 = prim->u3 = (u8)D_80092698 * 4 + 8 + column * 4;
    prim->v0 = prim->v1 = prim->v2 = prim->v3 = D_8009269C;
    sparkle->u.line.key = key;
    sparkle->u.line.prev = NULL;
    prim->tpage = D_80092694;
    prim->clut = D_800926A0;
    prim->code &= ~1;
    sparkle->prim[1] = *prim;
    for (other = D_80092AD8, i = 0; i < SPARKLE_COUNT; i++, other++) {
        if (other->active && other->u.line.key == key && other != sparkle && other->type == 2 &&
            other->u.line.stamp == (u16)(D_800926A4 - 1)) {
            sparkle->u.line.prev = other;
        }
    }
}

/* Draw the scene effects: the passes that need the view and its derived
 * matrix, then the screen-space passes under the view matrix. */
void func_8007CF78(MATRIX *view, u32 *ot) {
    MATRIX local;

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
void func_8007D0B4(SVECTOR *pos) {
    pos->vx += rand() % 48 - 24;
    pos->vy += rand() % 48 - 24;
    pos->vz += rand() % 48 - 24;
}

/* Start a sparkle of kind 0..4 at a position; kinds 8..12 are the same
 * sparkles with the position jittered first. Declared int without a return
 * value, as the original's unfilled branch delay slot shows. */
s32 func_8007D190(VECTOR *pos, u32 kind) {
    SVECTOR at;

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
void func_8007D274(VECTOR *from, VECTOR *to) {
    to->vx = from->vx + rand() % 64 - 32;
    to->vy = from->vy + rand() % 64 - 32;
    to->vz = from->vz + rand() % 64 - 32;
}

/* Queue a three-strand bolt of jittered 7-segment lines between two points,
 * coloured by kind: 0 green-blue flicker, 1 random grey-yellow, 2
 * alternating white and red segments. */
void func_8007D334(VECTOR *from, VECTOR *to, s32 kind) {
    VECTOR point;
    VECTOR step;
    VECTOR prev;
    VECTOR next;
    CVECTOR color;
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
            if (i != 6) {
                func_8007E31C(&prev, &next, &color);
            } else {
                func_8007E31C(&prev, to, &color);
            }
            prev = next;
        }
    }
}

/* Map codes 0x11..0x13 to kinds 0..2 and forward them. */
void func_8007D65C(VECTOR *from, VECTOR *to, s32 code) {
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

/* Allocate the scene cell table and both buffers' point primitives. */
void func_8007D6B8(void) {
    SceneCell10 *cell;
    s32 i;

    D_800926BC = func_80031BDC(0x9F6, 0);
    D_800926C0[0] = func_80031BDC(0xBF4, 0);
    D_800926C0[1] = func_80031BDC(0xBF4, 0);
    cell = D_800926BC;
    for (i = 0; i < 0xFF; i++) {
        D_800926C0[0][i].len = 2;
        D_800926C0[0][i].rgbc = 0x6880B0F0;
        D_800926C0[1][i].len = 2;
        D_800926C0[1][i].rgbc = 0x6880B0F0;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk0 = cell->unk4 = cell->unk6 = 0;
        cell++;
    }
}

/* Spawn up to count ground particles in free cells around a position: on
 * the ground below a random point within 32 units, rising for 20 frames. */
void func_8007D7A8(VECTOR *pos, s32 count) {
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
            x = (s16)x;
            z = pos->vz + (rand() % 64 - 32);
            cell->unk4 = z;
            z = (s16)z;
            cell->unk2 = cell->unk8 = D_800928DC[(x >> 8) + (z >> 8) * 128].height;
            cell->unk7 = -(rand() % 10 + 10);
            count--;
            cell->unk6 = 20;
        }
    }
}

/* Draw and advance this buffer's half of the ground particles: project
 * them three at a time into point tiles, then let each fall (accelerating)
 * until it reaches its ground height. */
void func_8007D918(u32 *ot) {
    SceneScratch *scratch = SCENE_SCRATCH;
    Tile1Words *tile = D_800926C0[D_800928A0];
    SceneCell10 *cell = &D_800926BC[D_800928A0];
    s32 cx = scratch->camera.vx;
    s32 cy = scratch->camera.vy;
    s32 cz = scratch->camera.vz;
    s32 loaded = 0;
    s32 i;
    u32 prev;
    u32 addr;

    for (i = D_800928A0; i < 0xFC; i += 2, cell += 2) {
        if (cell->unk6 == 0) {
            continue;
        }
        scratch->point.vx = cell->unk0 - cx;
        scratch->point.vy = cell->unk2 - cy;
        scratch->point.vz = cell->unk4 - cz;
        if (loaded == 0) {
            gte_ldv0(&scratch->point);
            loaded = 1;
        } else if (loaded == 1) {
            gte_ldv1(&scratch->point);
            loaded = 2;
        } else {
            gte_ldv2(&scratch->point);
            gte_rtpt();
            loaded = 0;
            gte_stsxy3(&tile[0].x0, &tile[1].x0, &tile[2].x0);
            gte_stsz1(&scratch->depth);
            LINK_PRIM(ot, scratch, &tile[0], 0x02000000);
            gte_stsz2(&scratch->depth);
            LINK_PRIM(ot, scratch, &tile[1], 0x02000000);
            gte_stsz(&scratch->depth);
            LINK_PRIM(ot, scratch, &tile[2], 0x02000000);
            tile += 3;
        }
        cell->unk2 += cell->unk7;
        if (cell->unk2 >= cell->unk8) {
            cell->unk6 = 0;
        }
        cell->unk7 += 2;
    }
}

/* Allocate the 540 scene cells and their small tiles (2..4 pixels square,
 * pale blue), with a copy of the tiles for the other draw buffer. */
void func_8007DB28(void) {
    SceneCell12 *cell;
    TileWords *tile;
    s32 i;

    D_800926C8 = func_80031BDC(0x1950, 0);
    tile = func_80031BDC(0x21C0, 0);
    D_800926CC[0] = tile;
    D_800926CC[1] = func_80031BDC(0x21C0, 0);
    cell = D_800926C8;
    for (i = 0; i < 540; i++, cell++, tile++) {
        tile->len = 3;
        tile->rgbc = 0x60FFD0A0;
        tile->w = rand() % 3 + 2;
        tile->h = rand() % 3 + 2;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk8 = cell->unkA = cell->unk9 = cell->unk6 = 0;
    }
    func_800732AC(D_800926CC[1], D_800926CC[0], 0x21C0);
}

/* Throw up scene cells along a segment that reaches above height 0x80: one
 * per six units of its length, starting around `from` below the floor and
 * drifting across the segment (randomly to either side) or at random, with
 * a rise and life that grow with its height difference. */
void func_8007DC74(VECTOR *from, VECTOR *to) {
    VECTOR across;
    s32 nx;
    s32 nz;
    s32 life;
    s32 speed;
    s32 count;
    s32 drift;
    s32 height;
    s32 i;
    s32 r;
    SceneCell12 *cell;

    if (from->vy <= 0x80 && to->vy <= 0x80) {
        return;
    }
    across.vz = to->vx - from->vx;
    across.vx = from->vz - to->vz;
    across.vy = from->vy - to->vy;
    cell = D_800926C8;
    count = func_800886FC(&across) / 6;
    if (count <= 0) {
        count = 1;
    }
    across.vy = 0x1000;
    drift = func_80088754(&across);
    if (drift < 0x10) {
        drift = 0;
    }
    if (across.vz != 0 || across.vx != 0) {
        VectorNormal(&across, &across);
        nx = across.vx;
        nz = across.vz;
    }
    height = abs(from->vy - to->vy);
    life = height / 3 + 1;
    if ((u32)life > 0x78) {
        life = 0x78;
    }
    for (i = 0; i < 0x218; i++, cell++) {
        if (count == 0) {
            return;
        }
        if (cell->unk6 != 0) {
            continue;
        }
        if (drift != 0) {
            r = rand() % 4096;
            speed = drift / 7;
            if (rand() & 1) {
                cell->unk8 = ((nx * r) >> 14) + speed;
                cell->unk9 = ((nz * r) >> 14) + speed;
            } else {
                cell->unk8 = -((nx * r) >> 14) - speed;
                cell->unk9 = -((nz * r) >> 14) - speed;
            }
        } else {
            cell->unk8 = (rand() & 0xF) - 8;
            cell->unk9 = (rand() & 0xF) - 8;
        }
        if (height >= 9) {
            cell->unkA = -(rand() % (height / 2)) + 1;
            cell->unk6 = life;
        } else {
            cell->unkA = -4;
            cell->unk6 = 0x14;
        }
        cell->unk0 = from->vx + (rand() % 32 - 0x10) + cell->unk8 * 2;
        cell->unk4 = from->vz + (rand() % 32 - 0x10) + cell->unk9 * 2;
        cell->unk2 = -0x10;
        count--;
    }
}

/* Draw and advance the scene cells: project the live ones three at a time
 * into their tiles, drift them sideways and let them rise or fall (capped
 * at height 0x10) until their life runs out. */
void func_8007E020(u32 *ot) {
    SceneScratch *scratch = SCENE_SCRATCH;
    s32 loaded = 0;
    s32 i;
    s32 cx = scratch->camera.vx;
    s32 cy = scratch->camera.vy;
    s32 cz = scratch->camera.vz;
    SceneCell12 *cell = D_800926C8;
    TileWords *tile = D_800926CC[D_800928A0];
    u32 prev;
    u32 addr;

    for (i = 0; i < 0x218; i++, cell++) {
        if (cell->unk6 == 0) {
            continue;
        }
        cell->unk0 += cell->unk8;
        cell->unk4 += cell->unk9;
        scratch->point.vx = cell->unk0 - cx;
        scratch->point.vy = cell->unk2 - cy;
        scratch->point.vz = cell->unk4 - cz;
        if (loaded == 0) {
            gte_ldv0(&scratch->point);
            loaded = 1;
        } else if (loaded == 1) {
            gte_ldv1(&scratch->point);
            loaded = 2;
        } else {
            gte_ldv2(&scratch->point);
            gte_rtpt();
            loaded = 0;
            gte_stsxy3(&tile[0].x0, &tile[1].x0, &tile[2].x0);
            gte_stsz1(&scratch->depth);
            LINK_PRIM(ot, scratch, &tile[0], 0x03000000);
            gte_stsz2(&scratch->depth);
            LINK_PRIM(ot, scratch, &tile[1], 0x03000000);
            gte_stsz(&scratch->depth);
            LINK_PRIM(ot, scratch, &tile[2], 0x03000000);
            tile += 3;
        }
        cell->unk2 += cell->unkA;
        cell->unkA += 2;
        if (cell->unk2 > 0x10) {
            cell->unk2 = 0x10;
        }
        cell->unk6--;
    }
}

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
        setlen(&line->line, 3);
        line->line.code = 0x40;
    }
    D_800926B0 = 0;
    D_800926B4 = 0;
}

/* Queue a coloured 3D line segment for this frame (at most 100). */
void func_8007E31C(VECTOR *from, VECTOR *to, CVECTOR *color) {
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

/* Project this frame's queued 3D line segments (relative to the camera in
 * the scratchpad) and link each into the ordering table by depth. */
void func_8007E3CC(u32 *ot) {
    SceneScratch *scratch = SCENE_SCRATCH;
    SceneLine *line = D_80094818;
    s32 cx = scratch->camera.vx;
    s32 cy = scratch->camera.vy;
    s32 cz = scratch->camera.vz;
    s32 i;
    s32 z;
    u32 prev;
    u32 addr;

    for (i = 0; i < D_800926B4; i++, line++) {
        scratch->from = line->from;
        scratch->to = line->to;
        scratch->from.vx -= cx;
        scratch->from.vy -= cy;
        scratch->from.vz -= cz;
        scratch->to.vx -= cx;
        scratch->to.vy -= cy;
        scratch->to.vz -= cz;
        gte_ldv01(&scratch->from, &scratch->to);
        gte_rtpt();
        gte_stsxy01(&line->line.x0, &line->line.x1);
        gte_stsz2(&z);
        prev = ot[z >> 4];
        addr = (u32)line & 0xFFFFFF;
        ot[z >> 4] = addr;
        *(u32 *)addr = prev | 0x03000000;
    }
}
