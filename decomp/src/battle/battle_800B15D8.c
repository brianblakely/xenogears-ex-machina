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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1EA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1F0C);

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
        D_800C354C = -quake->amplitude.vx;
    } else {
        D_800C354C = quake->amplitude.vx;
    }
    if (quake->tick & 2) {
        D_800C354E = -quake->amplitude.vy;
    } else {
        D_800C354E = quake->amplitude.vy;
    }
    if (quake->tick & 2) {
        D_800C3550 = -quake->amplitude.vz;
    } else {
        D_800C3550 = quake->amplitude.vz;
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3CD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3E04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3F04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B4EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B4F88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B50D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B51B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B56E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B572C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B57E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5924);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B59BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5C18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5CC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5DC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5DF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B5FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6004);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B61B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B61F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B626C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B62C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B639C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B63F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6464);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B64D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B65B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6808);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6930);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B69E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6B98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6BFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6C98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6CEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6DC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6E84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B6F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B7364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B73A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B73EC);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BA4E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BA59C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BA614);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BA768);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BA8F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BA984);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BAB0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BABDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BAC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BACBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BADD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BAEB8);

void func_800BAF40(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BAF48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB080);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB13C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB248);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB350);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB540);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB620);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB690);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB6E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB760);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB7F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BB9D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BBAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BBEE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC158);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC2F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC3F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC404);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BC460);

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
