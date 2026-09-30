/* Battle unit from 800B15D8 to the end of the overlay text, built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (docs/matching.md): stores to
 * globals take a register for %hi, positive `li` becomes `addiu`, and some
 * epilogues (800B8090, 800B88BC, 800BEF84, 800BEFEC, 800BF718) carry the
 * stack adjustment in the `jr $ra` delay slot. The unit starts at 800B15D8,
 * the first function whose global stores take a register for %hi (800B14CC's
 * take $at); its rodata starts at 0x800707DC, after 800B12D0's jump table. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "effect.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
#include "actor.h"

/* Select script index of an effect script file: copy its entry into
 * D_800C3BD0 (relocating its offsets to addresses unless the file is already
 * relocated) and start its commands; its command count. */
s32 func_800B15D8(ScriptFile *file, s32 index) {
    ScriptEntry *entry = &file->entries[index];

    D_800C3BD0 = *entry;
    if (!(file->flags & 1)) {
        D_800C3BD0.data0 += (u32)entry;
        D_800C3BD0.data8 += (u32)entry;
        D_800C3BD0.commands += (u32)entry;
    }
    D_800C3BF0 = 0;
    D_800C3BEC = D_800C3BD0.commands;
    return entry->count;
}

/* Address of entry index (0x1C bytes each) of a table with a 0xC-byte
 * header. */
u8 *func_800B168C(u8 *table, s32 index) {
    return table + (index * 0x1C + 0xC);
}

/* The total size of an unrelocated script entry's commands (each command's
 * first byte + 1 words). */
s32 func_800B16A4(ScriptEntry *entry) {
    s32 i = 0;
    s32 size = 0;
    u8 *command = entry->commands + (u32)entry;
    s32 count = entry->count;

    while (i != count) {
        i++;
        size += (command[0] + 1) * 4;
        command += (command[1] + 1) * 4;
    }
    return size;
}

/* Step the effect script cursor to the next command (its second byte + 1
 * words further). */
void func_800B16F0(void) {
    D_800C3BEC += (D_800C3BEC[1] + 1) * 4;
    D_800C3BF0++;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1720);

/* Scale a vertex list's points by 1 << shift, once. */
void func_800B1EA0(VertexList *list, s32 shift) {
    SVector *vertex;
    s32 count;
    s32 i;

    if (!(list->flags & 0x8000)) {
        list->flags |= 0x8000;
        vertex = (SVector *)(list->offset + (s32)list);
        count = list->count;
        for (i = 0; i != count; i++) {
            vertex[i].vx <<= shift;
            vertex[i].vy <<= shift;
            vertex[i].vz <<= shift;
        }
    }
}

/* Add a copy of the draw mode primitive D_800C3BF8 to the ordering table
 * entry ot. */
void func_800B1F0C(u32 *ot) {
    DrawPrim8 *prim = (DrawPrim8 *)D_80059580;

    if (D_80059580 + sizeof(DrawPrim8) < D_80059534) {
        D_80059580 += sizeof(DrawPrim8);
        *prim = D_800C3BF8;
        AddPrim(ot, prim);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1F6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B2AEC);

void func_800B3348(void) {
}

void func_800B3350(void) {
}

/* Quake update: ease the amplitude from its start to its target over the
 * frames left, and shake the view offset by it with the sign flipping every
 * two frames; end once it is zero and done. */
void func_800B3358(Quake *quake) {
    Vector delta;

    if (quake->left == 0) {
        quake->amplitude.vx = quake->to.vx;
        quake->amplitude.vy = quake->to.vy;
        quake->amplitude.vz = quake->to.vz;
    } else {
        quake->left--;
        delta.vx = quake->to.vx - quake->from.vx;
        delta.vy = quake->to.vy - quake->from.vy;
        delta.vz = quake->to.vz - quake->from.vz;
        gte_lddp(quake->left >> 1);
        gte_ldlvl(&delta);
        gte_gpf0();
        gte_stlvl(&delta);
        delta.vx /= quake->total >> 1;
        delta.vy /= quake->total >> 1;
        delta.vz /= quake->total >> 1;
        quake->amplitude.vx = quake->to.vx - delta.vx;
        quake->amplitude.vy = quake->to.vy - delta.vy;
        quake->amplitude.vz = quake->to.vz - delta.vz;
    }
    quake->tick++;
    if (quake->tick & 2) {
        D_800C354C.vx = -quake->amplitude.vx;
    } else {
        D_800C354C.vx = quake->amplitude.vx;
    }
    if (quake->tick & 2) {
        D_800C354C.vy = -quake->amplitude.vy;
    } else {
        D_800C354C.vy = quake->amplitude.vy;
    }
    if (quake->tick & 2) {
        D_800C354C.vz = -quake->amplitude.vz;
    } else {
        D_800C354C.vz = quake->amplitude.vz;
    }
    /* x and y tested as one word */
    if (*(s32 *)&quake->amplitude == 0 && quake->amplitude.vz == 0 && quake->left == 0) {
        quake->task.destroy(&quake->task);
    }
}

/* End the quake task. */
void func_800B3588(Quake *quake) {
    func_8001CD94(quake);
    func_800320E8(quake);
    D_800C3548 = NULL;
}

/* The quake task, created at rest or restarted from its current amplitude. */
Quake *func_800B35C0(void) {
    Quake *quake;

    if (D_800C3548 == NULL) {
        quake = func_8001CD08(0, sizeof(Quake) - sizeof(BattleTask));
        func_8001CD6C(quake, func_800B3358);
        func_8001CD74(quake, func_800B3588);
        quake->from.vx = 0;
        quake->from.vy = 0;
        quake->from.vz = 0;
        D_800C3548 = quake;
    } else {
        quake = D_800C3548;
        quake->from.vx = quake->amplitude.vx;
        quake->from.vy = quake->amplitude.vy;
        quake->from.vz = quake->amplitude.vz;
    }
    return quake;
}

/* Quake the view towards amplitude over frames * 2 frames. */
void func_800B3658(SVector *amplitude, s32 frames) {
    Quake *quake = func_800B35C0();

    quake->to.vx = amplitude->vx;
    quake->to.vy = amplitude->vy;
    quake->to.vz = amplitude->vz;
    quake->left = frames * 2;
    quake->total = frames * 2;
    func_800B3358(quake);
}

#ifdef NON_MATCHING
/* Screen fade update: ease the colour to the target over the frames left;
 * end once it is black. */
void func_800B36BC(ScreenFade *fade) {
    Vector delta;

    if (fade->left == 0) {
        fade->colour[0] = fade->to[0];
        fade->colour[1] = fade->to[1];
        fade->colour[2] = fade->to[2];
        if ((fade->colour[0] | fade->colour[1] | fade->colour[2]) == 0) {
            D_800C3558->task.destroy(&D_800C3558->task);
        }
    } else {
        fade->left--;
        delta.vx = fade->to[0] - fade->from[0];
        delta.vy = fade->to[1] - fade->from[1];
        delta.vz = fade->to[2] - fade->from[2];
        gte_lddp(fade->left >> 1);
        gte_ldlvl(&delta);
        gte_gpf0();
        gte_stlvl(&delta);
        delta.vx /= fade->total >> 1;
        delta.vy /= fade->total >> 1;
        delta.vz /= fade->total >> 1;
        fade->colour[0] = fade->to[0] - delta.vx;
        fade->colour[1] = fade->to[1] - delta.vy;
        fade->colour[2] = fade->to[2] - delta.vz;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B36BC);
#endif

/* End the screen fade tasks. */
void func_800B383C(ScreenFade *fade) {
    func_8001CB48(&fade->draw);
    func_8001CD94(fade);
    D_800C3558 = NULL;
}

#ifdef NON_MATCHING
/* Draw the screen fade: a blended rectangle over the whole screen. */
void func_800B3878(BattleTask *draw) {
    POLY_F4 *poly = (POLY_F4 *)D_80059580;
    ScreenFade *fade = draw->data;
    DR_MODE *mode;

    if (D_80059580 + 0x50 < D_80059534) {
        D_80059580 += sizeof(POLY_F4) + sizeof(DR_MODE);
        SetPolyF4(poly);
        SetSemiTrans(poly, 1);
        poly->r0 = fade->colour[0];
        poly->g0 = fade->colour[1];
        poly->b0 = fade->colour[2];
        poly->x0 = -32;
        poly->y0 = -32;
        poly->x1 = 320;
        poly->y1 = -32;
        poly->x2 = -32;
        poly->y2 = 240;
        poly->x3 = 320;
        poly->y3 = 240;
        mode = (DR_MODE *)(poly + 1);
        SetDrawMode(mode, 0, 0, GetTPage(0, fade->blend, 0, 0), NULL);
        AddPrim(D_8005956C + 2, poly);
        AddPrim(D_8005956C + 2, mode);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3878);
#endif

/* Fade the second screen fade (800B39C0). */
void func_800B397C(s32 frames, s32 blend, u8 r, u8 g, u8 b) {
    D_800C355C = 1;
    func_800B39C0(frames, blend, r, g, b);
    D_800C355C = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B39C0);

/* The screen fade's blend mode (1 when none runs). */
u8 func_800B3B6C(void) {
    if (D_800C3558 != NULL) {
        return D_800C3558->blend;
    }
    return 1;
}

#ifdef NON_MATCHING
/* Light fade update: ease the level from its start to its target over the
 * frames left; end once it is zero. */
void func_800B3B94(LightFade *fade) {
    s32 left;

    if (fade->left != 0) {
        left = fade->left - 1;
        fade->left = left;
        fade->level = fade->to - (fade->to - fade->from) * ((left << 5) / fade->total) / 32;
    } else if (fade->level == 0) {
        fade->task.destroy(&fade->task);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3B94);
#endif

/* End the light fade tasks and restore the stage lights (800A6F98). */
void func_800B3C2C(LightFade *fade) {
    func_8001CB48(&fade->draw);
    func_8001CD94(fade);
    func_800320E8(fade);
    D_800C3560 = NULL;
    func_800A6F98();
}

/* Apply the light fade's level to light slot 0 when it changed. */
void func_800B3C74(BattleTask *draw) {
    LightFade *fade = draw->data;

    if (fade->applied != fade->level) {
        fade->applied = fade->level;
        func_800A6444(0, fade->red, 32 - fade->level, fade->blue, fade->field4C, fade->field4E);
    }
}

/* Fade light slot 0 (800B3B94) to level to over frames * 2 frames with the
 * given red, blue and parameters, first saving the stage lights (800A5EB4,
 * on a stack in a heap block). Defined without a prototype: callers pass
 * the parameters unconverted (800B639C). */
void func_800B3CD4(to, frames, red, blue, field4C, field4E)
    s32 to;
    s32 frames;
    s32 red;
    s16 blue;
    u16 field4C;
    u16 field4E;
{
    LightFade *fade;
    u8 *stack;

    if (D_800C3560 == NULL) {
        D_800C3560 = fade = func_8001D1D8(sizeof(LightFade), 0, func_800B3B94, func_800B3C74, func_800B3C2C);
        stack = func_80031BDC(0x1000, 1);
        STACK_ENTER(stack + 0xC00);
        func_800A5EB4();
        STACK_LEAVE();
        func_800320E8(stack);
        fade->from = 0;
        fade->applied = 0;
        fade->level = 0;
    } else {
        fade = D_800C3560;
        fade->from = fade->level;
    }
    fade->to = to;
    fade->total = frames * 2;
    fade->left = frames * 2;
    fade->blue = blue;
    fade->field4C = field4C;
    fade->field4E = field4E;
    fade->red = red;
    func_800B3B94(fade);
}

/* Copy the three 64 x 256 VRAM columns at x 0x280, 0x240 and 0x200 to the
 * places in D_800C3668 (on a stack in a heap block). */
void func_800B3E04(void) {
    u8 *stack = func_80031BDC(0x1000, 0);

    STACK_ENTER(stack + 0xF00);
    D_800C3C9C.x = 0x280;
    D_800C3C9C.y = 0x100;
    D_800C3C9C.w = 0x40;
    D_800C3C9C.h = 0x100;
    MoveImage(&D_800C3C9C, D_800C3668[0].x, D_800C3668[0].y);
    D_800C3C9C.x = 0x240;
    D_800C3C9C.y = 0x100;
    D_800C3C9C.w = 0x40;
    D_800C3C9C.h = 0x100;
    MoveImage(&D_800C3C9C, D_800C3668[1].x, D_800C3668[1].y);
    D_800C3C9C.x = 0x200;
    D_800C3C9C.y = 0x100;
    D_800C3C9C.w = 0x40;
    D_800C3C9C.h = 0x100;
    MoveImage(&D_800C3C9C, D_800C3668[2].x, D_800C3668[2].y);
    STACK_LEAVE();
    func_800320E8(stack);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3F04);

/* Copy the sprite's current part's 8 x 8 texture block and its 16-colour
 * CLUT row to VRAM (0x3F0, 0x1F0) and (0x3F0, 0x1EE). */
void func_800B4EDC(BattleSprite *sprite) {
    SpriteImagePart *part = sprite->view->part;
    RECT rect;
    u16 tpage;
    u16 clut;

    tpage = part->tpage;
    rect.x = (part->u >> 2) + ((tpage & 0xF) << 6);
    rect.y = part->v + ((tpage << 4) & 0x100);
    rect.w = 8;
    rect.h = 8;
    MoveImage(&rect, 0x3F0, 0x1F0);
    clut = part->clut;
    rect.w = 16;
    rect.h = 1;
    rect.x = clut & 0x3F;
    rect.y = (clut >> 6) & 0x1FF;
    MoveImage(&rect, 0x3F0, 0x1EE);
}

/* The matrix of the sprite's anchor index: its angles, at the anchor's
 * offset (mirrored with the sprite, scaled) from the sprite's position, in
 * the sprite's screen matrix. */
void func_800B4F88(BattleSprite *sprite, s32 index, Matrix *m) {
    SpriteAnchor *anchor;
    s32 x;
    s32 y;
    SVector angles;

    if (sprite->view != NULL) {
        anchor = (SpriteAnchor *)(index * sizeof(SpriteAnchor) + (s32)sprite->view->anchors);
        y = anchor->y;
        x = anchor->x;
        if ((sprite->motion.word >> 2) & 1) {
            x = -x;
        }
        y = y * sprite->scale / 4096;
        x = x * sprite->scale / 4096;
        angles.vx = anchor->angle[0];
        angles.vy = sprite->view->anchors[index].angle[1];
        angles.vz = sprite->view->anchors[index].angle[2];
        func_8003F738(&angles, m);
        m->t[0] = sprite->view->matrix.t[0] + x;
        m->t[1] = sprite->view->matrix.t[1] + y;
        m->t[2] = sprite->view->matrix.t[2];
        SetMulMatrix(m, &sprite->view->matrix);
    }
}

#ifdef NON_MATCHING
/* The offsets of the sprite's five trail anchors (D_800C356C), mirrored with
 * the sprite and scaled, as points (x, y, 0) of out, when it is drawn one
 * sided. */
void func_800B50D4(BattleSprite *sprite, SVector *out) {
    SpriteAnchor *anchor;
    s32 i;
    s32 x;
    s32 y;

    if ((sprite->render.word & 3) == 1 && sprite->view != NULL && sprite->view->anchors != NULL) {
        for (i = 0; i != 5; i++) {
            anchor = &sprite->view->anchors[D_800C356C[i]];
            x = anchor->x;
            y = anchor->y;
            if ((sprite->motion.word >> 2) & 1) {
                x = -x;
            }
            x = x * sprite->scale / 8192;
            y = y * sprite->scale / 8192;
            out[i].vx = x;
            out[i].vy = y;
            out[i].vz = 0;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B50D4);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B51B0);

#ifdef NON_MATCHING
/* Trail update: publish its colours and blend for drawing, refresh the
 * anchors when the sprite's frame changed and ease the trail towards them;
 * end once the sprite's motion changes or its phase is 0 or 1. */
void func_800B5588(BattleTask *task) {
    SpriteTrail *trail = task->data;
    BattleSprite *sprite;
    s32 phase;
    s32 i;

    D_800D2FD8 = trail->colours;
    sprite = trail->sprite;
    D_800C3E9C = trail->count;
    D_800C3D4C = trail->blend;
    D_800D3334 = sprite->depth;
    if (sprite->frame != trail->frame) {
        trail->frame = sprite->frame;
        func_800B50D4(sprite, trail->anchors);
    }
    for (i = 1; i != 5; i++) {
        trail->trail[i].vx += (trail->anchors[i].vx - trail->trail[i].vx) / 2;
        trail->trail[i].vy += (trail->anchors[i].vy - trail->trail[i].vy) / 2;
        trail->trail[i].vz += (trail->anchors[i].vz - trail->trail[i].vz) / 2;
    }
    trail->trail[0].vx = trail->anchors[0].vx;
    trail->trail[0].vy = trail->anchors[0].vy;
    trail->trail[0].vz = trail->anchors[0].vz;
    sprite = trail->sprite;
    if (trail->motion != sprite->motion.bytes[3] || (phase = (sprite->frameBits.word >> 28) & 3) == 0 || phase == 1) {
        trail->task.destroy(&trail->task);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5588);
#endif

/* Trail draw: the sprite, then the trail (800C08CC). */
void func_800B56E4(BattleTask *draw) {
    SpriteTrail *trail = draw->data;

    func_8001E148(trail->sprite);
    func_800C08CC(5, trail->trail, func_800B51B0);
}

#ifdef NON_MATCHING
/* Give sprite a trail in colours (the first byte the colour count, 0 for
 * 4). */
void func_800B572C(BattleSprite *sprite, u8 *colours) {
    SpriteTrail *trail = func_8001D1D8(0xB8, sprite->task, func_800B5588, func_800B56E4, NULL);

    trail->sprite = sprite;
    trail->frame = sprite->frame;
    trail->motion = sprite->motion.bytes[3];
    trail->colours = colours;
    trail->blend = sprite->render.bytes[0] >> 5;
    trail->count = colours[0] == 0 ? 4 : colours[0];
    func_800B50D4(sprite, trail->trail);
    func_800B50D4(sprite, trail->anchors);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B572C);
#endif

/* The distance from the sprite to its target. */
s32 func_800B57E4(BattleSprite *sprite) {
    Vector delta;
    Vector squares;

    delta.vx = sprite->target[0] - sprite->x.part.whole;
    delta.vy = sprite->target[1] - sprite->y.part.whole;
    delta.vz = sprite->target[2] - sprite->z.part.whole;
    func_8004A414(&delta, &squares);
    return SquareRoot0(squares.vx + squares.vz + squares.vy);
}

#ifdef NON_MATCHING
/* Approach watch update: stop the sprite (after frames) once it passes its
 * target or comes near it; end when it stopped or its motion changed. */
void func_800B5854(SpriteApproach *approach) {
    u8 done = 0;
    BattleSprite *sprite = approach->sprite;
    s32 last = approach->distance;
    s32 distance = func_800B57E4(sprite);

    approach->distance = distance;
    if (last < distance || distance < approach->near) {
        done = 1;
        sprite->countdown = 1;
        sprite->framesLeft = approach->frames;
    }
    if (sprite->countdown == 0) {
        done = 1;
    }
    if (sprite->motion.bytes[3] != approach->motion) {
        done = 1;
    }
    if (done) {
        approach->task.destroy(&approach->task);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5854);
#endif

/* Watch sprite approach its target (800B5854). */
SpriteApproach *func_800B5924(BattleSprite *sprite, s32 near, s32 frames) {
    SpriteApproach *approach = func_8001CD08(sprite->task, sizeof(SpriteApproach) - sizeof(BattleTask));

    func_8001CD6C(approach, func_800B5854);
    approach->sprite = sprite;
    approach->distance = func_800B57E4(sprite);
    approach->near = near;
    approach->frames = frames;
    approach->motion = sprite->motion.bytes[3];
    sprite->motion.word |= 0x20;
    return approach;
}

/* The offset of the sprite's anchor index (mirrored with the sprite,
 * scaled), when it is drawn one sided. */
Point2 func_800B59BC(BattleSprite *sprite, s32 index) {
    Point2 offset;

    if (sprite->view != NULL && (sprite->render.word & 3) == 1 && sprite->view->anchors != NULL) {
        offset.y = sprite->view->anchors[index].y;
        offset.x = sprite->view->anchors[index].x;
        if ((sprite->motion.word >> 2) & 1) {
            offset.x = -offset.x;
        }
        offset.y = offset.y * sprite->scale / 4096;
        offset.x = offset.x * sprite->scale / 4096;
        return offset;
    }
}

/* The screen position of the sprite's anchor index. */
Point2 func_800B5AC4(BattleSprite *sprite, s32 index) {
    Point2 point = func_800B59BC(sprite, index);

    point.x += sprite->x.fixed >> 16;
    point.y += sprite->y.fixed >> 16;
    return point;
}

/* Link update: move the partner so that its anchor meets the sprite's;
 * end once the sprite's motion changes or its phase is 0 or 1. */
void func_800B5B3C(SpriteLink *link) {
    BattleSprite *partner = link->sprite->partner;
    Point2 a = func_800B5AC4(link->sprite, link->anchor);
    Point2 b = func_800B5AC4(partner, link->partnerAnchor);
    Point2 delta;
    BattleSprite *sprite;
    s32 phase;

    delta.x = a.x - b.x;
    delta.y = a.y - b.y;
    partner->x.fixed += delta.x << 16;
    partner->y.fixed += delta.y << 16;
    sprite = link->sprite;
    if (link->motion != sprite->motion.bytes[3] || (phase = (sprite->frameBits.word >> 28) & 3) == 0 || phase == 1) {
        link->task.destroy(&link->task);
    }
}

/* Link sprite's partner to it at anchors (low nibble the sprite's, high
 * nibble the partner's). */
SpriteLink *func_800B5C18(BattleSprite *sprite, u8 *anchors) {
    SpriteLink *link = func_8001CD08(sprite->task, sizeof(SpriteLink) - sizeof(BattleTask));

    func_8001CD6C(link, func_800B5B3C);
    link->sprite = sprite;
    link->partner = sprite->partner;
    link->frame = sprite->frame;
    link->motion = sprite->motion.bytes[3];
    link->anchor = *anchors & 0xF;
    link->partnerAnchor = *anchors >> 4;
    func_800B5B3C(link);
    return link;
}

/* Sprite orbit update: place the sprite around its target by its speeds
 * (radius and angles); end with its frames. */
void func_800B5CC0(BattleTask *task) {
    BattleSprite *sprite = task->data;
    Matrix m;
    SVector offset;
    SVector angles;
    Vector position;

    func_80023210(sprite);
    angles.vy = sprite->velocity[1] >> 13;
    angles.vz = sprite->velocity[2] >> 13;
    angles.vx = 0;
    offset.vx = func_80022CAC(sprite, sprite->velocity[0] >> 13);
    offset.vy = 0;
    offset.vz = 0;
    func_8003F738(&angles, &m);
    ApplyMatrix(&m, &offset, &position);
    position.vx += sprite->target[0];
    position.vy += sprite->target[1];
    position.vz += sprite->target[2];
    sprite->x.fixed = position.vx << 16;
    sprite->y.fixed = position.vy << 16;
    sprite->z.fixed = position.vz << 16;
    if (sprite->framesLeft == 0) {
        task->destroy(task);
    }
}

/* Start sprite's orbit (800B5CC0). */
void func_800B5DC4(BattleSprite *sprite) {
    sprite->frame = 1;
    func_8001CD6C(sprite->task, func_800B5CC0);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5DF4);

/* Draw sprite with 800B5DF4, uncoloured. */
void func_800B5FBC(BattleSprite *sprite) {
    sprite->frame = 1;
    func_8001CD64(&sprite->task->draw, func_800B5DF4);
    sprite->colourFlags = 0x40;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6004);

/* Draw sprite with 800B6004, uncoloured. */
void func_800B61B0(BattleSprite *sprite) {
    sprite->frame = 1;
    func_8001CD64(&sprite->task->draw, func_800B6004);
    sprite->colourFlags = 0x40;
}

/* Script command: reset D_800D36BC and load stage object set args[0]
 * (800A96B4, on a stack in a heap block). */
void func_800B61F8(BattleSprite *sprite, u8 *args) {
    u8 *stack = func_80031BDC(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    D_800D36BC = 0;
    func_800A96B4(args[0]);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* Script command: free stage object 11 (800A9FF0, on a stack in a heap
 * block). */
void func_800B626C(void) {
    u8 *stack = func_80031BDC(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    func_800A9FF0(0xB);
    STACK_LEAVE();
    func_800320E8(stack);
}

#ifdef NON_MATCHING
/* Script command: show stage object 11 in mode args[0] (800BEE2C); mode 2
 * first places it and shows it to the side of D_800C3E1C only. */
void func_800B62C8(BattleSprite *sprite, u8 *args) {
    u8 *stack = func_80031BDC(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    if (args[0] == 2) {
        func_800A979C(0xB, 0x300, 0x100, 0, 0x1DB);
        func_800BEE2C(0xB, 1 << (((D_800C3E1C->motion.word & 3) << 2) | (D_800C3E1C->frameBits.word >> 30)), args[0]);
    } else {
        func_800BEE2C(0xB, D_800D3634, args[0]);
    }
    STACK_LEAVE();
    func_800320E8(stack);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B62C8);
#endif

/* Script command: fade the lights (800B3CD4) with the parameters at the
 * relative offset in args. */
void func_800B639C(BattleSprite *sprite, u8 *args) {
    s8 *fade = (s8 *)SCRIPT_DATA(args);

    func_800B3CD4((u8)fade[5], (u8)fade[3], (u8)fade[4], fade[0], fade[1], fade[2]);
}

/* Script command: upload the images at the relative offset in args. */
void func_800B63F0(BattleSprite *sprite, u8 *args) {
    func_8002DDE4(SCRIPT_DATA(args), 0, 0, 0, 0, 0, 0);
}

/* Script command: draw sprite with the resident sprite drawer (80025A88). */
void func_800B6438(BattleSprite *sprite) {
    func_8001CD64(&sprite->task->draw, func_80025A88);
}

#ifdef NON_MATCHING
/* Script command: move the CLUTs of the sprite's parts by (args[0],
 * args[1]). */
void func_800B6464(BattleSprite *sprite, u8 *args) {
    SpriteImagePart *part = sprite->view->part;
    u32 count = sprite->flags.bytes[0] >> 2;
    s16 i = 0;
    s32 x;
    s32 y;

    if (count != 0) {
        do {
            i++;
            x = (part->clut & 0x3F) + args[0];
            y = ((part->clut >> 6) & 0x1FF) + args[1];
            part->clut = x | (y << 6);
            part++;
        } while (i != count);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6464);
#endif

/* Script command: set battle sprite args[1]'s value (800245D8) to args[0]. */
void func_800B64D4(BattleSprite *sprite, u8 *args) {
    func_800245D8(BATTLE_AREA.sprites[args[1]], args[0]);
}

/* Script command: turn the sprite towards its target (on the x-z plane). */
void func_800B6518(BattleSprite *sprite) {
    Point2 position;
    Point2 target;
    s16 direction;

    position.x = sprite->x.fixed >> 16;
    position.y = sprite->z.fixed >> 16;
    target.x = sprite->target[0];
    target.y = sprite->target[2];
    direction = func_80023124(target, position);
    func_80021FE0(sprite, direction);
    func_800223B0(sprite, direction);
}

#ifdef NON_MATCHING
/* Script command: turn the sprite's speed towards its target by at most
 * args[0] * 4 (of 4096) in each angle, keeping its length. */
void func_800B65B0(BattleSprite *sprite, u8 *args) {
    SVector want;
    SVector angles;
    Vector delta;
    Vector squares;
    Vector velocity;
    SVector length;
    Matrix m;
    Vector speed;
    s16 distance;
    s16 speedLength;
    s32 step;
    s32 turn;
    s32 difference;

    delta.vx = sprite->target[0] - sprite->x.part.whole;
    delta.vy = sprite->target[1] - sprite->y.part.whole;
    delta.vz = sprite->target[2] - sprite->z.part.whole;
    func_8004A414(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    want.vy = -ratan2(delta.vz, delta.vx);
    want.vz = ratan2(delta.vy, distance);
    want.vx = 0;
    velocity.vx = sprite->velocity[0] >> 7;
    velocity.vy = sprite->velocity[1] >> 7;
    velocity.vz = sprite->velocity[2] >> 7;
    func_8004A414(&velocity, &squares);
    speedLength = SquareRoot0(squares.vy + squares.vx + squares.vz);
    distance = SquareRoot0(squares.vx + squares.vz);
    angles.vy = -ratan2(velocity.vz, velocity.vx);
    angles.vz = ratan2(velocity.vy, distance);
    angles.vx = 0;
    step = args[0] * 4;
    difference = ((s32)((u16)want.vy - (u16)angles.vy) << 20) >> 20;
    turn = difference;
    if (step < abs(difference)) {
        turn = step;
        if (difference < 0) {
            turn = -step;
        }
    }
    angles.vy += turn;
    difference = ((s32)((u16)want.vz - (u16)angles.vz) << 20) >> 20;
    turn = difference;
    if (step < abs(difference)) {
        turn = step;
        if (difference < 0) {
            turn = -step;
        }
    }
    angles.vz += turn;
    func_80021B04(&length, speedLength, 0, 0);
    func_8003F738(&angles, &m);
    ApplyMatrix(&m, &length, &speed);
    sprite->velocity[0] = speed.vx << 7;
    sprite->velocity[1] = speed.vy << 7;
    sprite->velocity[2] = speed.vz << 7;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B65B0);
#endif

/* Script command: aim the sprite's speed (its speed setting, field18) at
 * its target and face it that way. */
void func_800B6808(BattleSprite *sprite) {
    SVector angles;
    Vector delta;
    Vector squares;
    SVector length;
    Matrix m;
    Vector speed;
    s32 distance;

    delta.vx = sprite->target[0] - sprite->x.part.whole;
    delta.vy = sprite->target[1] - sprite->y.part.whole;
    delta.vz = sprite->target[2] - sprite->z.part.whole;
    func_8004A414(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    angles.vy = -ratan2(delta.vz, delta.vx);
    angles.vz = ratan2(delta.vy, distance);
    angles.vx = 0;
    sprite->direction = angles.vy;
    func_80021B04(&length, (sprite->speed << 9) >> 16, 0, 0);
    func_8003F738(&angles, &m);
    ApplyMatrix(&m, &length, &speed);
    sprite->velocity[0] = speed.vx << 7;
    sprite->velocity[1] = speed.vy << 7;
    sprite->velocity[2] = speed.vz << 7;
}

/* Script command: set a 16.16 point to the script's three s16s. */
void func_800B6930(Fixed16 *point, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    point[0].fixed = SCRIPT_S16(data, 0) << 16;
    point[1].fixed = SCRIPT_S16(data, 2) << 16;
    point[2].fixed = SCRIPT_S16(data, 4) << 16;
}

/* Script command: set a vector to the script's three s16s. */
void func_800B6990(s16 *vector, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    s32 value;

    value = SCRIPT_S16(data, 0);
    vector[0] = value;
    value = SCRIPT_S16(data, 2);
    vector[1] = value;
    value = SCRIPT_S16(data, 4);
    vector[2] = value;
}

/* Script command: add the script's three s16s to a vector. */
void func_800B69E4(s16 *vector, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    s32 value;

    value = SCRIPT_S16(data, 0);
    vector[0] += value;
    value = SCRIPT_S16(data, 2);
    vector[1] += value;
    value = SCRIPT_S16(data, 4);
    vector[2] += value;
}

/* Script command: take the speed of the sprite's parent. */
void func_800B6A50(BattleSprite *sprite) {
    BattleSprite *parent = sprite->parent;

    sprite->velocity[0] = parent->velocity[0];
    sprite->velocity[1] = parent->velocity[1];
    sprite->velocity[2] = parent->velocity[2];
}

/* Script command: break the sprite's image into pieces (801FC4C4) with the
 * script's parameters (scaled with the sprite when field3A is set), leaving
 * it without an image. */
void func_800B6A7C(BattleSprite *sprite, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 a;
    s32 b;
    s32 c;
    SpriteView *view;

    if (sprite->field3A != 0) {
        a = func_80022CAC(sprite, (((s8 *)data)[1] << 3) | data[0]) << 8;
        b = func_80022CAC(sprite, data[2]) << 16;
        c = func_80022CAC(sprite, data[3]) << 16;
    } else {
        a = ((((s8 *)data)[1] << 3) | data[0]) << 5;
        b = data[2] << 13;
        c = data[3] << 13;
    }
    view = sprite->view;
    func_801FC4C4(view->anchors, view->parts, &view->matrix, a, b, c, data[4] * 16, data[5] * 4);
    sprite->view->anchors = NULL;
    sprite->view->parts = NULL;
    sprite->view->part = NULL;
}

/* Script command: start a burst from the sprite (801FC53C) with the
 * script's parameters. */
void func_800B6B98(BattleSprite *sprite, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    func_801FC53C(sprite, data[0] * 16, data[1], ((s8 *)data)[2] * 8, data[3] * 8, ((s8 *)data)[4] * 8, data[5]);
}

/* Script command: copy the screen to VRAM (0x2C0, 0x100) and shatter it
 * (800B73A0). */
void func_800B6BFC(void) {
    RECT rect;

    rect.w = 320;
    rect.x = 0;
    rect.y = 0;
    rect.h = 224;
    MoveImage(&rect, 0x2C0, 0x100);
    func_800B73A0();
}

/* Script command: turn the sprite about z to its speed's direction in x-y. */
void func_800B6C44(BattleSprite *sprite) {
    sprite->view->angle[2] = ratan2(sprite->velocity[1] >> 8, sprite->velocity[0] >> 8);
    sprite->render.word |= 0x10000000;
}

/* Script command: turn the sprite about x to its speed's direction in x-z. */
void func_800B6C98(BattleSprite *sprite) {
    sprite->view->angle[0] = ratan2(sprite->velocity[2] >> 8, sprite->velocity[0] >> 8);
    sprite->render.word |= 0x10000000;
}

/* Script command: turn the sprite to its speed's direction. */
void func_800B6CEC(BattleSprite *sprite) {
    Vector speed;
    Vector squares;
    s32 distance;

    speed.vx = sprite->velocity[0] >> 8;
    speed.vy = sprite->velocity[1] >> 8;
    speed.vz = sprite->velocity[2] >> 8;
    if (speed.vz == 0) {
        speed.vz = 4;
    }
    func_8004A414(&speed, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    sprite->view->angle[1] = -ratan2(speed.vz, speed.vx);
    sprite->view->angle[2] = ratan2(speed.vy, distance);
    sprite->view->angle[0] = 0;
    sprite->render.word |= 0x10000000;
}

/* Script command: clear the sprite's eight anchors. */
void func_800B6DC0(BattleSprite *sprite) {
    s32 i;

    if (sprite->view != NULL && sprite->view->anchors != NULL) {
        for (i = 0; i != 8; i++) {
            sprite->view->anchors[i].x = 0;
            sprite->view->anchors[i].y = 0;
            sprite->view->anchors[i].angle[0] = 0;
            sprite->view->anchors[i].angle[1] = 0;
            sprite->view->anchors[i].angle[2] = 0;
        }
        sprite->view->field3C = 0;
        sprite->view->field3D = 0;
    }
}

/* Script command: turn the sprite's speed about z by args[0] * 16. */
void func_800B6E84(BattleSprite *sprite, s8 *args) {
    SVector angles;
    Matrix m;
    Vector velocity;

    func_80021B04(&angles, 0, 0, args[0] * 16);
    func_8003F738(&angles, &m);
    ApplyMatrixLV(&m, (Vector *)sprite->velocity, &velocity);
    sprite->velocity[0] = velocity.vx;
    sprite->velocity[1] = velocity.vy;
    sprite->velocity[2] = velocity.vz;
}

/* Shatter update: after its delay each shard fades, moves, turns and
 * falls, its velocity easing out. */
void func_800B6F0C(BattleTask *task) {
    ScreenShatter *shatter = task->data;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVector step;

    shatter->frame++;
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                if (shard->delay != 0) {
                    shard->delay--;
                } else {
                    poly = &shard->poly[BATTLE_AREA.buffer];
                    poly->r0 = func_80021AD8(poly->r0, -6);
                    poly->g0 = func_80021AD8(poly->g0, -6);
                    poly->b0 = func_80021AD8(poly->b0, -6);
                    step.vx = shard->velocity.vx >> 16;
                    step.vy = shard->velocity.vy >> 16;
                    step.vz = shard->velocity.vz >> 16;
                    shard->position.vx += step.vx;
                    shard->position.vy += step.vy;
                    shard->position.vz += step.vz;
                    shard->angles.vx += shard->spin.vx;
                    shard->angles.vy += shard->spin.vy;
                    shard->angles.vz += shard->spin.vz;
                    shard->velocity.vy -= shard->velocity.vy / 16;
                    shard->velocity.vx -= shard->velocity.vx / 16;
                    shard->velocity.vz -= shard->velocity.vz / 16;
                    shard->velocity.vy += shard->fall;
                }
            }
        }
    }
}

/* Shatter draw: into the ordering table (800B7160). */
void func_800B7134(BattleTask *draw) {
    D_800C3CB4 = D_8005956C;
    func_800B7160(draw);
}

#ifdef NON_MATCHING
/* Shatter draw: each shard that has fallen in front of the screen (z at
 * least 64), its layer's triangle turned and placed, projected at the
 * screen centre and distance 512. */
void func_800B7160(BattleTask *draw) {
    ScreenShatter *shatter = draw->data;
    s32 offsetX;
    s32 offsetY;
    Matrix m;
    s32 p;
    s32 flag;
    s32 screen;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVector *triangle;

    ReadGeomOffset(&offsetX, &offsetY);
    screen = ReadGeomScreen();
    SetGeomOffset(160, 112);
    SetGeomScreen(512);
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                poly = &shard->poly[BATTLE_AREA.buffer];
                if (shard->position.vz >= 64) {
                    func_8003F738(&shard->angles, &m);
                    TransMatrix(&m, &shard->position);
                    SetRotMatrix(&m);
                    SetTransMatrix(&m);
                    if (layer == 0) {
                        triangle = D_800C3594;
                    } else {
                        triangle = D_800C35AC;
                    }
                    AddPrim(D_800C3CB4 + (RotTransPers3(&triangle[0], &triangle[1], &triangle[2], (u32 *)&poly->x0,
                                                        (u32 *)&poly->x1, (u32 *)&poly->x2, &p, &flag) >> 6),
                            poly);
                }
            }
        }
    }
    SetGeomOffset(offsetX, offsetY);
    SetGeomScreen(screen);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7160);
#endif

/* Free a heap block once drawing is done. */
void func_800B7330(void *block) {
    DrawSync(0);
    func_800320E8(block);
}

/* Shatter destroy: end the draw task, the task and its sprites. */
void func_800B7364(ScreenShatter *shatter) {
    func_8001CB48(&shatter->draw);
    func_8001CD94(shatter);
    func_80025180(shatter);
}

/* Shatter the screen copied to VRAM (0x2C0, 0x100). */
void func_800B73A0(void) {
    func_800B7424(func_8001D1D8(sizeof(ScreenShatter), 0, func_800B6F0C, func_800B7134, func_800B7364));
}

/* Set up a shattered screen in a heap block (not run as a task). */
void func_800B73EC(void) {
    ScreenShatter *shatter = func_80031BDC(sizeof(ScreenShatter), 1);

    shatter->task.data = shatter;
    shatter->draw.data = shatter;
    func_800B7424(shatter);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7424);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7C28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7E94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8048);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8054);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8068);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B81BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B838C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B853C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8774);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B88C4);

void func_800B89F4(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B89FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8D04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8D7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8DA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B8EBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9020);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B905C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9B30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9B54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9C00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B9F78);

/* End slot's turn presentation: wait for the stage objects (800B136C), then
 * restore the view (800B8D7C) and, for a gear, 800BFBA0; for a party member
 * on foot its sprite's state (800BF2B8). */
void func_800BA4E0(s32 slot) {
    D_80059464 = 0;
    D_800591AC = 0;
    func_800B136C();
    if (BATTLE_AREA.slots[slot].gear) {
        func_800B8D7C();
        func_800BFBA0();
    } else if (slot < 3) {
        if (BATTLE_AREA.sprites[slot] != NULL) {
            func_800BF2B8(BATTLE_AREA.sprites[slot]);
        }
    } else {
        func_800B8D7C();
    }
    func_800BC454(0xC0);
}

/* Turn sprite to direction, its horizontal speed a quarter of its speed
 * along it. */
void func_800BA59C(BattleSprite *sprite, s16 direction) {
    s32 speed;

    sprite->direction = direction;
    speed = sprite->speed >> 3;
    sprite->velocity[0] = (func_8003F8CC(direction) >> 1) * speed >> 8;
    sprite->velocity[2] = -((func_8003F8B0(sprite->direction) >> 1) * speed) >> 8;
}

/* Aim sprite's jump at its target: turn it towards the target and set the
 * rising speed that lands it on the ground there (or the target's height
 * when that is higher). */
void func_800BA614(BattleSprite *sprite) {
    Vector delta;
    SVector point;
    Vector out;
    s32 triangle;
    s32 height;
    s16 angle;
    s32 distance;

    func_80021B04(&point, sprite->target[0], sprite->target[1], sprite->target[2]);
    triangle = func_800A5914(&point, sprite->triangle, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    if (point.vy > sprite->target[1]) {
        point.vy = sprite->target[1];
    }
    height = ((point.vy << 16) - sprite->y.fixed) >> 16;
    delta.vx = sprite->target[0] - (sprite->x.fixed >> 16);
    delta.vz = sprite->target[2] - (sprite->z.fixed >> 16);
    angle = -ratan2(delta.vz, delta.vx);
    func_8004A414(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz);
    sprite->velocity[1] = -sprite->gravity * distance * 16 / (sprite->speed >> 11) + sprite->speed * height / distance;
    func_800BA59C(sprite, angle);
}

/* Aim sprite's jump at its target keeping its rising speed: snap it to
 * whole units, turn it towards the target and set the speed that covers the
 * distance (and the height difference) in the jump's frames. */
void func_800BA768(BattleSprite *sprite) {
    Vector delta;
    SVector point;
    Vector out;
    s32 frames;
    s32 triangle;
    s32 angle;
    s32 distance;
    s32 height;

    frames = -(sprite->velocity[1] * 2 / sprite->gravity);
    sprite->x.fixed &= 0xFFFF0000;
    sprite->y.fixed &= 0xFFFF0000;
    sprite->z.fixed &= 0xFFFF0000;
    delta.vx = sprite->target[0] - (sprite->x.fixed >> 16);
    delta.vz = sprite->target[2] - (sprite->z.fixed >> 16);
    delta.vy = 0;
    angle = -ratan2(delta.vz, delta.vx);
    func_8004A414(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz) << 16;
    if (frames != 0) {
        sprite->speed = distance / frames;
    } else {
        sprite->speed = 0;
    }
    func_80021B04(&point, sprite->target[0], sprite->target[1], sprite->target[2]);
    triangle = func_800A5914(&point, sprite->triangle, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    if (point.vy > sprite->target[1]) {
        point.vy = sprite->target[1];
    }
    height = (point.vy << 16) - sprite->y.fixed;
    if (frames != 0) {
        sprite->velocity[1] += height / frames;
    }
    func_800BA59C(sprite, angle);
    func_80022B2C(sprite);
}

/* Put sprite on the scene's ground: its triangle and ground height. */
void func_800BA8F4(BattleSprite *sprite) {
    SVector point;
    Vector out;
    s32 triangle;

    point.vx = sprite->x.fixed >> 16;
    point.vy = sprite->y.fixed >> 16;
    point.vz = sprite->z.fixed >> 16;
    triangle = func_800A5914(&point, sprite->triangle, 4);
    if (triangle < 0) {
        triangle = func_800A579C(&point);
    }
    func_800A5870(&point, triangle, &out);
    sprite->ground = point.vy;
    sprite->triangle = triangle;
}

/* Create a sprite task (updated by 800BAC50, drawn by 800BAB0C) at x, y, z
 * facing direction, running animation. */
ActorTask *func_800BA984(s32 resource, s16 a, s16 b, s16 c, s16 d, s16 e, s16 x, s16 y, s16 z, s16 animation,
                         s16 direction, s32 unused11, s32 unused12, s32 g) {
    ActorTask *task;
    BattleSprite *sprite;

    task = func_8001D1D8(0x19C, NULL, func_800BAC50, func_800BAB0C, func_800BABDC);
    sprite = (BattleSprite *)(task + 1);
    task->data = sprite;
    task->draw.data = sprite;
    task->draw.owner = NULL;
    func_800242F4(sprite, resource, a, b, c, d, e, g);
    sprite->task = task;
    sprite->x.fixed = x << 16;
    sprite->y.fixed = y << 16;
    sprite->z.fixed = z << 16;
    sprite->fieldB0 = animation;
    sprite->render.word |= 4;
    sprite->direction = direction;
    func_80022000(&sprite->x.fixed, 0x2000);
    sprite->field82 = 0x2000;
    sprite->triangle = 0;
    func_800245D8(sprite, animation);
    return task;
}

/* Draw a sprite task: its depth in the view, and its parts when visible. */
void func_800BAB0C(ActorTask *task) {
    SVector point;
    s32 result[2]; /* screen position, then the GTE flags */
    Vector unused;
    BattleSprite *sprite;
    s32 depth;

    if (D_800C3664 == 0) {
        sprite = task->data;
        point.vx = sprite->x.fixed >> 16;
        point.vy = sprite->y.fixed >> 16;
        point.vz = sprite->z.fixed >> 16;
        SetRotMatrix(&D_800D30BC);
        SetTransMatrix(&D_800D30BC);
        depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> D_80050100) + sprite->depthBias;
        if (result[1] & 0x8000) {
            depth = 0;
        }
        sprite->depth = depth;
        if ((u32)(depth - 1) < 0xFFF) {
            func_8001E298(sprite, D_8005956C + depth);
        }
    }
}

#ifdef NON_MATCHING
/* Destroy a sprite task: its part block, children, sprite and node.
 * Nonmatching: the original computes the sprite from $a0 before copying the
 * task to $s0. */
void func_800BABDC(ActorTask *task) {
    BattleSprite *sprite = (BattleSprite *)(task + 1);
    void *parts = sprite->view->parts;

    if (parts != NULL) {
        func_800320E8(parts);
    }
    func_8001CE74(task);
    func_8001D3F4(sprite);
    func_8001CB48(&task->draw);
    func_8001CD94(task);
    func_800320E8(task);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BABDC);
#endif

/* Update a sprite task (twice with double steps) unless paused. */
void func_800BAC50(ActorTask *task) {
    BattleSprite *sprite = task->data;

    if (D_800C3664 == 0) {
        func_80023210(sprite);
        func_80022CDC(sprite);
        if (sprite->motion.bits.doubleStep) {
            func_80023210(sprite);
            func_80022CDC(sprite);
        }
    }
}

/* Party slot's sprite on screen: its position, depth and a box around it. */
void func_800BACBC(s32 slot, s16 *x, s16 *y, s16 *depth, s16 *left, s16 *width, s16 *centre) {
    SVector point;
    s16 sxy[2];
    s32 p;
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];

    point.vx = sprite->x.fixed >> 16;
    point.vy = sprite->y.fixed >> 16;
    point.vz = sprite->z.fixed >> 16;
    PushMatrix();
    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    *depth = RotTransPers(&point, (s32 *)sxy, &p, &p) >> 4;
    *x = sxy[0];
    *y = sxy[1];
    *left = sxy[0] - 0x30;
    *width = 0x30;
    *centre = sxy[0] - 0x18;
    PopMatrix();
}

/* Remove party slot's sprite task: stop its effects (800BFC80), free its
 * sprite source, destroy the task and clear the slot's sprite. */
void func_800BADD4(s32 slot) {
    ActorTask *task = BATTLE_AREA.tasks[slot];

    if (task != NULL) {
        func_800BFC80(task, 0, 2);
        if (slot < 3) {
            if (BATTLE_AREA.sources[slot].data != NULL) {
                func_800320E8(BATTLE_AREA.sources[slot].data);
            }
            BATTLE_AREA.sources[slot].data = NULL;
        }
        task->destroy(task);
        func_8001CE74(task);
        BATTLE_AREA.sprites[slot] = NULL;
        BATTLE_AREA.tasks[slot] = NULL;
    }
}

/* Face slot's sprite along its side (turned for a nonzero target code),
 * unless it runs animation 0x15. */
void func_800BAEB8(s32 slot) {
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];
    s32 direction;

    if (sprite->motion.bytes[3] != 0x15) {
        direction = (BATTLE_AREA.slots[slot].targetCode != 0) << 11;
        func_800223B0(&sprite->x.fixed, direction);
        func_80021FE0(&sprite->x.fixed, direction);
    }
}

void func_800BAF40(void) {
}

#ifdef NON_MATCHING
/* Send party slot's sprite off: select it (800BC404), run its exit
 * animation 0x16 and wait for it and its tasks, then remove the sprite and
 * load the slot's gear object in its place (800BB760), waiting for it.
 * Nonmatching: battle_core.h declares slot u8 and 800BC404's mask u16; the
 * original takes and passes words, unextended. */
void func_800BAF48(u8 slot) {
    BattleSprite *sprite;
    s32 tasks;

    func_800BC404(1 << slot);
    func_800BC404(0);
    tasks = D_80059188;
    sprite = BATTLE_AREA.sprites[slot];
    func_800B8D7C();
    func_800245D8(sprite, 0x16);
    while (sprite->countdown != 0 && sprite->motion.bytes[3] == 0x16) {
        func_800BE790();
    }
    while (D_80059188 != tasks) {
        func_800BE790();
    }
    func_800BADD4(slot);
    func_800BE790();
    func_800BE790();
    func_800BB760(slot);
    while (D_800C35D8 != 0) {
        func_800BE790();
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BAF48);
#endif

/* Destroy the party members' sprites other than keep's that are not in
 * use, then end their stage objects (800B14CC). */
void func_800BB080(s32 keep) {
    s32 i;
    BattleSprite *sprite;

    for (i = 0; i != 3; i++) {
        if (i != keep) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL && sprite->field48 == 0) {
                sprite->task->destroy(sprite->task);
                BATTLE_AREA.sprites[i] = NULL;
                BATTLE_AREA.tasks[i] = NULL;
            }
        }
    }
    func_800B14CC(keep);
}

/* Update of a sprite following its slot's stage object: step its animation
 * while it runs, then put it at the object's position. */
void func_800BB13C(ActorTask *task) {
    SVector unused; /* the original's frame has this unused local */
    BattleSprite *sprite = task->data;
    u32 low = sprite->frameBits.bits.slotLow;
    BattleObject *object = D_800D3368[sprite->motion.bits.slotHigh << 2 | low];

    if (object != NULL) {
        if (sprite->countdown == 0) {
            sprite->framesLeft = 0;
        }
        if (sprite->framesLeft != 0) {
            func_80023210(sprite);
            func_80022CDC(sprite);
            if (sprite->motion.bits.doubleStep) {
                func_80023210(sprite);
                func_80022CDC(sprite);
            }
        }
        sprite->x.fixed = object->hierarchy->translation[0] << 16;
        sprite->y.fixed = object->hierarchy->translation[1] << 16;
        sprite->z.fixed = object->hierarchy->translation[2] << 16;
    }
}

/* Draw of a slot-following sprite: its size from the slot's object and its
 * depth in the view. */
void func_800BB248(ActorTask *task) {
    SVector point;
    s32 result[2]; /* screen position, then the GTE flags */
    BattleSprite *sprite = task->data;
    s32 depth;
    u32 low;

    low = sprite->frameBits.bits.slotLow;
    sprite->size = func_800AA600(sprite->motion.bits.slotHigh << 2 | low);
    sprite->halfSize = sprite->size / 2;
    point.vx = sprite->x.fixed >> 16;
    point.vy = sprite->y.fixed >> 16;
    point.vz = sprite->z.fixed >> 16;
    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> D_80050100) + sprite->depthBias;
    if (result[1] & 0x8000) {
        depth = 0;
    }
    sprite->depth = depth;
}

/* Destroy a task node. */
void func_800BB314(ActorTask *task) {
    SVector unused; /* the original's frame has this unused local */

    func_8001CB48(&task->draw);
    func_8001CD94(task);
    func_800320E8(task);
}

/* Create slot's sprite following its stage object (800BB13C, 800BB248),
 * unless it has one. */
void func_800BB350(u32 slot) {
    ActorTask *task;
    BattleSprite *sprite;
    u8 saved;

    if (BATTLE_AREA.sprites[slot] == NULL) {
        saved = D_800591AC;
        D_800591AC = 0;
        task = func_8001D1D8(0x19C, NULL, func_800BB13C, func_800BB248, func_800BB314);
        sprite = (BattleSprite *)(task + 1);
        sprite->task = task;
        task->data = sprite;
        task->draw.data = sprite;
        func_80023804(sprite);
        func_800239A0(sprite);
        sprite->flags.bits.group = 4;
        sprite->task = task;
        sprite->render.word &= ~3;
        sprite->frameBits.bits.sequencerOwned = 0;
        sprite->sequencer->field8 = 0;
        sprite->sequencer->fieldC = 0;
        sprite->field82 = D_800591A8;
        sprite->x.fixed = (u16)BATTLE_AREA.slots[slot].x << 16;
        sprite->z.fixed = (u16)BATTLE_AREA.slots[slot].z << 16;
        sprite->y.fixed = 0;
        sprite->size = func_800AA600(slot);
        sprite->field82 = 0x2000;
        sprite->halfSize = sprite->size >> 1;
        func_80022000(&sprite->x.fixed, 0x2000);
        sprite->resource = D_8006BE10;
        BATTLE_AREA.sprites[slot] = sprite;
        BATTLE_AREA.tasks[slot] = task;
        sprite->field4C = 0;
        sprite->field48 = 0;
        D_800591AC = saved;
        sprite->frameBits.bits.slotLow = slot;
        sprite->motion.bits.slotHigh = slot >> 2;
    }
}

/* Task step: take a free stage place (of three), create the gear object of
 * the task's slot there, its sprite (800BB350), and end the task; the last
 * one sets D_800C37CC. */
void func_800BB540(SlotTask *task) {
    s32 i;
    s32 bit;

    for (i = 0, bit = 1; i != 3; i++, bit <<= 1) {
        if (!(D_800C3666 & bit)) {
            D_800C3666 |= bit;
            break;
        }
    }
    func_800A979C(task->slot, D_800C3668[i].x, D_800C3668[i].y, 0, task->slot + 0x1C0);
    D_800C3CB8--;
    func_800BB350(task->slot);
    task->task.destroy(&task->task);
    if (--D_800C35D8 == 0) {
        D_800C37CC = 1;
    }
}

/* Task step: once the disc is idle, run 800BB540 on a separate stack. */
void func_800BB620(SlotTask *task) {
    u8 *stack;

    if (func_800286CC() == 0) {
        stack = func_80031BDC(0x1000, 1);
        STACK_ENTER(stack + 0xF00);
        func_800BB540(task);
        STACK_LEAVE();
        func_800320E8(stack);
    }
}

/* Task step: read the gear files of the task's slot (800A9540), then
 * continue with 800BB620. */
void func_800BB690(SlotTask *task) {
    func_800A9540(task->slot);
    D_800C3CB8++;
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BB620);
}

/* Task step: once the disc and the file reads are idle, run 800BB690 on a
 * separate stack. */
void func_800BB6E0(SlotTask *task) {
    u8 *stack;

    if (func_800286CC() == 0 && D_800C3CB8 == 0) {
        stack = func_80031BDC(0x1000, 1);
        STACK_ENTER(stack + 0xF00);
        func_800BB690(task);
        STACK_LEAVE();
        func_800320E8(stack);
    }
}

/* Start a task loading slot's gear object (800BB6E0). */
void func_800BB760(s32 slot) {
    u8 saved = D_800591AC;
    SlotTask *task;

    D_800591AC = 0;
    D_800591AF = 1;
    task = func_8001CD08(NULL, 4);
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BB6E0);
    task->slot = slot;
    D_800591AF = 0;
    D_800C35D8++;
    D_800591AC = saved;
}

/* Reset the camera modes. */
void func_800BB7F8(void) {
    D_800C3674 = 0x200;
    D_800C3678 = -1;
    D_800C3CC4 = 0;
    D_800C3CBC = 1;
    func_800BC2F0(0);
}

/* Build view matrix m looking from eye at target with up vector up. */
void func_800BB844(Matrix *m, SVector *eye, SVector *target, SVector *up) {
    Vector v;
    Vector forward;
    Vector right;
    Vector upward;

    func_80021B14(&v, target->vx - eye->vx, target->vy - eye->vy, target->vz - eye->vz);
    upward.vx = up->vx;
    upward.vy = up->vy;
    upward.vz = up->vz;
    func_80048D7C(&v, &forward);
    func_8004A480(&upward, &forward, &v);
    func_80048D7C(&v, &right);
    func_8004A480(&forward, &right, &v);
    func_80048D7C(&v, &upward);
    m->m[0][0] = right.vx;
    m->m[0][1] = right.vy;
    m->m[0][2] = right.vz;
    m->m[1][0] = upward.vx;
    m->m[1][1] = upward.vy;
    m->m[1][2] = upward.vz;
    m->m[2][0] = forward.vx;
    m->m[2][1] = forward.vy;
    m->m[2][2] = forward.vz;
    PushMatrix();
    ApplyMatrix(m, eye, &v);
    m->t[0] = -v.vx;
    m->t[1] = -v.vy;
    m->t[2] = -v.vz;
    PopMatrix();
}

/* Set the battle view from the camera points, shaken by 800c354c, and draw
 * the stage unless that is off. */
void func_800BB9D4(void) {
    func_800BB844(&D_800D309C.matrix, &D_800D3354, &D_800D335C, &D_800C3730);
    D_800D309C.matrix.t[0] += D_800C354C.vx;
    D_800D309C.matrix.t[1] += D_800C354C.vy;
    D_800D309C.matrix.t[2] += D_800C354C.vz;
    if (D_800C372C == 0) {
        func_800A4654(&D_800D309C.matrix, NULL, 0, BATTLE_AREA.ot, BATTLE_AREA.buffer, &D_800D3354, &D_800D335C,
                      0x1000);
    }
}

/* Step the battle camera: take its wanted points from the camera mode, move
 * the eye and look-at points a fraction (800c3674) of the way there, and
 * derive its angles and range. */
void func_800BBAB8(void) {
    SVector *point;
    Vector step;
    Vector unused[2]; /* the original's frame has these unused locals */
    Vector delta;
    Vector unused2;
    Vector square;
    s32 horizontal;

    switch (D_800C3CC0) {
    case 0:
        break;
    case 1:
        func_800BC460(D_800C3678);
        break;
    case 2:
        D_800D30A0[0].vx = D_8006F99C.vx >> 16;
        D_800D30A0[0].vy = D_8006F99C.vy >> 16;
        D_800D30A0[0].vz = D_8006F99C.vz >> 16;
        point = &D_800D30A0[1];
        point->vx = D_8006F9AC.vx >> 16;
        point->vy = D_8006F9AC.vy >> 16;
        point->vz = D_8006F9AC.vz >> 16;
        break;
    case 3:
        /* step holds the wanted look-at, then eye point */
        ((SVector *)&step)[1].vx = ((SVector *)&step)[0].vx = D_800D39EC->x.fixed >> 16;
        ((SVector *)&step)[0].vy = D_800D39EC->y.fixed >> 16;
        ((SVector *)&step)[0].vz = D_800D39EC->z.fixed >> 16;
        ((SVector *)&step)[1].vz = ((SVector *)&step)[0].vz - func_8003F8CC(D_800C373C) * D_800C3738 / 4096;
        ((SVector *)&step)[1].vy = ((SVector *)&step)[0].vy - func_8003F8B0(D_800C373C) * D_800C3738 / 4096;
        D_800D309C.eye = ((SVector *)&step)[1];
        D_800D309C.target = ((SVector *)&step)[0];
        break;
    }
    if (D_800C3CBC == 1) {
        gte_lddp(D_800C3674);
        step.vx = D_800D309C.eye.vx - D_800D3354.vx;
        step.vy = D_800D309C.eye.vy - D_800D3354.vy;
        step.vz = D_800D309C.eye.vz - D_800D3354.vz;
        gte_ldlvl(&step);
        gte_gpf12();
        gte_stlvl(&step);
        if (step.vx | step.vz | step.vy) {
            D_800D3354.vx += step.vx;
            D_800D3354.vy += step.vy;
            D_800D3354.vz += step.vz;
        } else {
            SVector *wanted = &D_800D309C.eye;

            D_800D3354.vx = wanted->vx;
            D_800D3354.vy = wanted->vy;
            D_800D3354.vz = wanted->vz;
        }
        step.vx = D_800D309C.target.vx - D_800D335C.vx;
        step.vy = D_800D309C.target.vy - D_800D335C.vy;
        step.vz = D_800D309C.target.vz - D_800D335C.vz;
        gte_ldlvl(&step);
        gte_gpf12();
        gte_stlvl(&step);
        if (step.vx | step.vz | step.vy) {
            D_800D335C.vx += step.vx;
            D_800D335C.vy += step.vy;
            D_800D335C.vz += step.vz;
        } else {
            SVector *wanted = &D_800D309C.target;

            D_800D335C.vx = wanted->vx;
            D_800D335C.vy = wanted->vy;
            D_800D335C.vz = wanted->vz;
        }
    }
    delta.vx = D_800D335C.vx - D_800D3354.vx;
    delta.vy = D_800D335C.vy - D_800D3354.vy;
    delta.vz = D_800D335C.vz - D_800D3354.vz;
    func_8004A414(&delta, &square);
    horizontal = SquareRoot0(square.vx + square.vz);
    D_800D309C.range = SquareRoot0(square.vx + square.vy + square.vz);
    D_800D309C.rot.vy = -ratan2(delta.vz, delta.vx);
    D_800D309C.rot.vx = -ratan2(delta.vy, horizontal);
    D_800D309C.rot.vz = 0;
}

/* Destroy of a camera sprite task: release its camera role (restoring the
 * saved point unless effects are off), free it, and when the last one ends
 * return to camera mode 800c367c. */
void func_800BBEE0(ActorTask *task) {
    SVector *point;
    BattleSprite *sprite = task->data;

    if (sprite->flags.bits.group == 0xA) {
        if (D_800C3680 == task) {
            D_800C3680 = NULL;
            if (D_800C37C8 == 0) {
                D_800D30A0[0].vx = D_800C3CCC.vx;
                D_800D30A0[0].vy = D_800C3CCC.vy;
                D_800D30A0[0].vz = D_800C3CCC.vz;
            }
        }
    } else if (D_800C3684 == task) {
        D_800C3684 = NULL;
        if (D_800C37C8 == 0) {
            point = &D_800D30A0[1];
            point->vx = D_800C3CD4.vx;
            point->vy = D_800C3CD4.vy;
            point->vz = D_800C3CD4.vz;
        }
    }
    if (sprite->motion.bits.owned) {
        func_8001CE74(task);
    }
    func_8001CD94(task);
    func_8001CB48(&task->draw);
    func_800320E8(task);
    if (--D_800C3CC4 == 0) {
        func_800BC2F0(D_800C367C);
    }
}

/* Update of a camera sprite task: step its animation (twice when double
 * stepping), make its position the camera eye (group 0xA) or look-at point,
 * and destroy it when its animation ends. */
void func_800BC018(ActorTask *task) {
    BattleSprite *sprite = task->data;

    func_80023210(sprite);
    func_80022CDC(sprite);
    if (sprite->flags.bits.group == 0xA) {
        D_8006F99C.vx = sprite->x.fixed;
        D_8006F99C.vy = sprite->y.fixed;
        D_8006F99C.vz = sprite->z.fixed;
    } else {
        D_8006F9AC.vx = sprite->x.fixed;
        D_8006F9AC.vy = sprite->y.fixed;
        D_8006F9AC.vz = sprite->z.fixed;
    }
    if (sprite->framesLeft != 0) {
        if (sprite->motion.bits.doubleStep) {
            func_80023210(sprite);
            func_80022CDC(sprite);
            if (sprite->flags.bits.group == 0xA) {
                D_8006F99C.vx = sprite->x.fixed;
                D_8006F99C.vy = sprite->y.fixed;
                D_8006F99C.vz = sprite->z.fixed;
            } else {
                D_8006F9AC.vx = sprite->x.fixed;
                D_8006F9AC.vy = sprite->y.fixed;
                D_8006F9AC.vz = sprite->z.fixed;
            }
            if (sprite->framesLeft == 0) {
                task->destroy(task);
            }
        }
    } else {
        task->destroy(task);
    }
}

/* Make sprite task a camera sprite: the eye (group 0xA) or look-at sprite,
 * saving the camera point or taking over (field34 1) from a running one,
 * else stopping the new one; then camera mode 2 follows the sprites. */
void func_800BC158(ActorTask *task) {
    SVector *point;
    BattleSprite *sprite = (BattleSprite *)(task + 1);

    if (sprite->flags.bits.group == 0xA) {
        if (D_800C3680 != NULL) {
            if (((BattleSprite *)(D_800C3680 + 1))->frame != 1 && sprite->frame == 1) {
                D_800C3680->destroy(D_800C3680);
                D_800C3680 = task;
            } else {
                sprite->countdown = 0;
                sprite->framesLeft = 0;
            }
        } else {
            D_800C3680 = task;
            D_800C3CCC.vx = D_800D30A0[0].vx;
            D_800C3CCC.vy = D_800D30A0[0].vy;
            D_800C3CCC.vz = D_800D30A0[0].vz;
        }
    } else if (D_800C3684 != NULL) {
        if (((BattleSprite *)(D_800C3684 + 1))->frame != 1 && sprite->frame == 1) {
            D_800C3684->destroy(D_800C3684);
            D_800C3684 = task;
        } else {
            sprite->countdown = 0;
            sprite->framesLeft = 0;
        }
    } else {
        D_800C3684 = task;
        point = &D_800D30A0[1];
        D_800C3CD4.vx = point->vx;
        D_800C3CD4.vy = point->vy;
        D_800C3CD4.vz = point->vz;
    }
    D_800C3CC4++;
    if (sprite->motion.bits.flip) {
        sprite->direction = 0x800;
    } else {
        sprite->direction = 0;
    }
    func_8001CD74(task, func_800BBEE0);
    func_8001CD6C((EffectSprite *)task, (void (*)(EffectSprite *))func_800BC018);
    func_800BC2F0(2);
}

/* Set the camera mode: 2 puts the eye and look-at sprites at the saved
 * points, 4 sets D_800C3CBC to 5, others release them. */
void func_800BC2F0(s32 mode) {
    SVector *point;

    D_800C3CC0 = mode;
    D_800C3CBC = 1;
    switch (mode) {
    case 4:
        D_800C3CBC = 5;
        break;
    case 2:
        D_8006F99C.vx = D_800D30A0[0].vx << 16;
        D_8006F99C.vy = D_800D30A0[0].vy << 16;
        D_8006F99C.vz = D_800D30A0[0].vz << 16;
        point = &D_800D30A0[1];
        D_8006F9AC.vx = point->vx << 16;
        D_8006F9AC.vy = point->vy << 16;
        D_8006F9AC.vz = point->vz << 16;
        break;
    default:
        if (D_800C3680 != NULL) {
            D_800C3680->destroy(D_800C3680);
            D_800C3680 = NULL;
        }
        if (D_800C3684 != NULL) {
            D_800C3684->destroy(D_800C3684);
            D_800C3684 = NULL;
        }
        break;
    }
}

/* Set D_800C367C. */
void func_800BC3F8(s32 value) {
    D_800C367C = value;
}

#ifdef NON_MATCHING
/* Start camera move (800BC460) unless effects are off; restore D_80059454.
 * Nonmatching: battle_core.h declares mask u16, which the original passes on
 * unextended (its own parameter is a word). */
void func_800BC404(u16 mask) {
    if (D_800C37C8 == 0) {
        func_800BC2F0(1);
        func_800BC460(mask);
    }
    D_80059454 = D_800C3CDC;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC404);
#endif

/* Set the camera framing pitch. */
void func_800BC454(s16 value) {
    D_800C3740.vx = value;
}

/* Frame the camera on the party slots in mask: look at the middle of their
 * sprites from the framing angles, at a range that keeps the farthest sprite
 * (and its gear top) on screen; the points go to the camera's wanted eye and
 * look-at points. */
void func_800BC460(u32 mask) {
    Vector center;
    SVector eye;
    SVector target;
    Matrix m;
    Vector offset;
    SVector point;
    s32 screen[2];
    SVector v;
    s32 result[2];
    Matrix m2;
    Vector out;
    SVector v2;
    Matrix m3;
    Vector unused;
    SVector v3;
    BattleSprite *sprite;
    s32 i;
    s32 count;
    s32 farthest;
    s32 minX, maxX, minY, maxY, minZ, maxZ;
    s32 distance;
    s32 range;
    u32 bits;

    memset(&center, 0, sizeof(center));
    farthest = 0;
    D_800C3678 = mask;
    i = 0;
    count = 0;
    for (bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
        if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
            count++;
            center.vx += sprite->x.fixed >> 1;
            center.vy += sprite->y.fixed >> 1;
            center.vz += sprite->z.fixed >> 1;
        }
    }
    if (count != 0) {
        center.vx = center.vx / count * 2;
        center.vy = center.vy / count * 2;
        center.vz = center.vz / count * 2;
        maxX = minX = center.vx;
        maxZ = minZ = center.vz;
        maxY = minY = center.vy;
        for (i = 0, bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
            if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
                if (maxX < sprite->x.fixed) {
                    maxX = sprite->x.fixed;
                }
                if (sprite->x.fixed < minX) {
                    minX = sprite->x.fixed;
                }
                if (maxZ < sprite->z.fixed) {
                    maxZ = sprite->z.fixed;
                }
                if (sprite->z.fixed < minZ) {
                    minZ = sprite->z.fixed;
                }
                if (maxY < sprite->y.fixed) {
                    maxY = sprite->y.fixed;
                }
                if (sprite->y.fixed < minY) {
                    minY = sprite->y.fixed;
                }
            }
        }
        center.vx = (minX + maxX) / 2;
        center.vy = (maxY + minY) / 2;
        center.vz = (minZ + maxZ) / 2;
        center.vx >>= 16;
        center.vy >>= 16;
        center.vz >>= 16;
        func_8004ABBC(&D_800C3740, &m);
        v.vx = 0;
        v.vy = 0;
        v.vz = ReadGeomScreen() * 8;
        ApplyMatrix(&m, &v, &offset);
        eye.vx = center.vx;
        eye.vy = center.vy;
        eye.vz = center.vz;
        target.vx = center.vx;
        target.vy = center.vy;
        target.vz = center.vz;
        eye.vx -= offset.vx;
        eye.vy += offset.vy;
        eye.vz -= offset.vz;
        func_800BB844(&m, &eye, &target, &D_800C3730);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (i = 0, bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
            if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
                point.vx = sprite->x.fixed >> 16;
                point.vy = sprite->y.fixed >> 16;
                point.vz = sprite->z.fixed >> 16;
                RotTransPers(&point, screen, &result[0], &result[1]);
                ((s16 *)screen)[0] -= 160;
                ((s16 *)screen)[1] -= 164;
                ((s16 *)screen)[0] <<= 2;
                ((s16 *)screen)[1] <<= 2;
                distance = ((s16 *)screen)[0] * ((s16 *)screen)[0];
                distance += ((s16 *)screen)[1] * ((s16 *)screen)[1];
                if (farthest < distance) {
                    farthest = distance;
                }
                if (BATTLE_AREA.slots[i].gear && D_800C3688 == 0) {
                    point.vy -= sprite->size;
                    RotTransPers(&point, screen, &result[0], &result[1]);
                    ((s16 *)screen)[0] -= 160;
                    ((s16 *)screen)[1] -= 164;
                    ((s16 *)screen)[0] <<= 2;
                    ((s16 *)screen)[1] <<= 2;
                    distance = ((s16 *)screen)[0] * ((s16 *)screen)[0];
                    distance += ((s16 *)screen)[1] * ((s16 *)screen)[1];
                    if (farthest < distance) {
                        farthest = distance;
                    }
                }
            }
        }
        farthest = SquareRoot0(farthest);
        if (farthest < 120) {
            func_8004ABBC(&D_800C3740, &m2);
            v2.vx = 0;
            v2.vy = 0;
            v2.vz = ReadGeomScreen() * 2;
            D_800C3CDC = ReadGeomScreen() * 2;
            ApplyMatrix(&m2, &v2, (Vector *)&point);
            eye.vx = center.vx;
            eye.vy = center.vy;
            eye.vz = center.vz;
            target.vx = center.vx;
            target.vy = center.vy;
            target.vz = center.vz;
            eye.vx -= (*(Vector *)&point).vx;
            eye.vy += (*(Vector *)&point).vy;
            eye.vz -= (*(Vector *)&point).vz;
            D_800D30A0[0].vx = eye.vx;
            D_800D30A0[0].vy = eye.vy;
            D_800D30A0[0].vz = eye.vz;
            {
                SVector *p = &D_800D30A0[1];

                p->vx = target.vx;
                p->vy = target.vy;
                p->vz = target.vz;
            }
        } else {
            range = (farthest << 14) / 120;
            range = (range << 1) * ReadGeomScreen();
            range >>= 14;
            D_800C3CDC = range;
            func_8004ABBC(&D_800C3740, &m3);
            v3.vx = 0;
            v3.vy = 0;
            v3.vz = range;
            ApplyMatrix(&m3, &v3, &out);
            eye.vx = center.vx;
            eye.vy = center.vy;
            eye.vz = center.vz;
            target.vx = center.vx;
            target.vy = center.vy;
            target.vz = center.vz;
            eye.vx -= out.vx;
            eye.vy += out.vy;
            eye.vz -= out.vz;
            D_800D30A0[0].vx = eye.vx;
            D_800D30A0[0].vy = eye.vy;
            D_800D30A0[0].vz = eye.vz;
            {
                SVector *p = &D_800D30A0[1];

                p->vx = target.vx;
                p->vy = target.vy;
                p->vz = target.vz;
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCAA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCAD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCAFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCBB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCC60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCD8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCEAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BCFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD024);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD1FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD2E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD3AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD7A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD810);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BD974);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDB74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDC14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDC78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDCF8);

void func_800BDD34(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDD3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDE58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BDF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE11C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE1C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE538);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE6A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE6E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE790);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEB04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEC18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BED30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BED4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEDE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEE2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEEB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEF24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEF8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BEFF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF0B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF0C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF1EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF2B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF3A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF3E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF4F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF5E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF6CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF6F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF730);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF73C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF7C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF85C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF8CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF954);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF9EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFA9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFBA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFC80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFD88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFDA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C06E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0758);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C07CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0828);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C08CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0D18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0F70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C0FAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C1140);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800C11CC);
