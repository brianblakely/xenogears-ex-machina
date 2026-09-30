/* Battle unit from 800B15D8 to 800B8098, built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (docs/matching.md): stores to
 * globals take a register for %hi, positive `li` becomes `addiu`, and some
 * epilogues (800B8090, 800B88BC, 800BEF84, 800BEFEC, 800BF718) carry the
 * stack adjustment in the `jr $ra` delay slot. The unit starts at 800B15D8,
 * the first function whose global stores take a register for %hi (800B14CC's
 * take $at); its rodata starts at 0x800707DC, after 800B12D0's jump table.
 * It ends before 800B8098 (see battle_800B8098.c). */
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
#include "popup.h"
#include "frame.h"
#include "stage.h"

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
    SVECTOR *vertex;
    s32 count;
    s32 i;

    if (!(list->flags & 0x8000)) {
        list->flags |= 0x8000;
        vertex = (SVECTOR *)(list->offset + (s32)list);
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
    VECTOR delta;

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
void func_800B3658(SVECTOR *amplitude, s32 frames) {
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
    VECTOR delta;

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
void func_800B4F88(BattleSprite *sprite, s32 index, MATRIX *m) {
    SpriteAnchor *anchor;
    s32 x;
    s32 y;
    SVECTOR angles;

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
void func_800B50D4(BattleSprite *sprite, SVECTOR *out) {
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
    VECTOR delta;
    VECTOR squares;

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
    MATRIX m;
    SVECTOR offset;
    SVECTOR angles;
    VECTOR position;

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
    GroundPoint position;
    GroundPoint target;
    s16 direction;

    position.x = sprite->x.fixed >> 16;
    position.z = sprite->z.fixed >> 16;
    target.x = sprite->target[0];
    target.z = sprite->target[2];
    direction = func_80023124(target, position);
    func_80021FE0(sprite, direction);
    func_800223B0(sprite, direction);
}

#ifdef NON_MATCHING
/* Script command: turn the sprite's speed towards its target by at most
 * args[0] * 4 (of 4096) in each angle, keeping its length. */
void func_800B65B0(BattleSprite *sprite, u8 *args) {
    SVECTOR want;
    SVECTOR angles;
    VECTOR delta;
    VECTOR squares;
    VECTOR velocity;
    SVECTOR length;
    MATRIX m;
    VECTOR speed;
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
    SVECTOR angles;
    VECTOR delta;
    VECTOR squares;
    SVECTOR length;
    MATRIX m;
    VECTOR speed;
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
    VECTOR speed;
    VECTOR squares;
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
    SVECTOR angles;
    MATRIX m;
    VECTOR velocity;

    func_80021B04(&angles, 0, 0, args[0] * 16);
    func_8003F738(&angles, &m);
    ApplyMatrixLV(&m, (VECTOR *)sprite->velocity, &velocity);
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
    SVECTOR step;

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
    MATRIX m;
    s32 p;
    s32 flag;
    s32 screen;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVECTOR *triangle;

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

/* Clear D_800D2FDC. */
void func_800B7C28(void) {
    D_800D2FDC = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7E94);

/* Set the acting sprite of a single action. */
void func_800B8048(BattleSprite *sprite) {
    D_800C3E1C = sprite;
}

/* Request sound (run by the frame loop, 800B8068). */
void func_800B8054(s32 sound) {
    D_800591B4 = sound;
    D_800591B1 = 0;
}

/* Run a requested sound command (800B7C34, 800B7E94) and mark it done. */
void func_800B8068(s32 sound) {
    func_800B7C34(sound);
    func_800B7E94();
    D_800591B1 = 1;
}
