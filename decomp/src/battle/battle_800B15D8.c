/* Battle unit from 800B15D8 to 800B3F04, built by the
 * Cygnus CDK GCC 2.7.2 with a later ASPSX (docs/matching.md): stores to
 * globals take a register for %hi, positive `li` becomes `addiu`, and some
 * epilogues (800B8090, 800B88BC, 800BEF84, 800BEFEC, 800BF718) carry the
 * stack adjustment in the `jr $ra` delay slot. The unit starts at 800B15D8,
 * the first function whose global stores take a register for %hi (800B14CC's
 * take $at); its rodata starts at 0x800707DC, after 800B12D0's jump table.
 * Its one table (800B1F6C's, 29 entries at 4 mod 8) is followed directly by
 * 800B3F04's at 0x80070850 (0 mod 8), so the unit ends before 800B3F04. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "battle/effect_script.h"
#include "battle/screen.h"
#include "battle/sprite.h"
#include "battle/stage.h"
#include "gte.h"
#include "own_declarations.h"
#include "resident_views.h"

/* A command's u16 at offset, and the vertex (or normal) its index at offset
 * names. */
#define SCRIPT_CMD_U16(cmd, offset) (*(u16 *)((cmd) + (offset)))
#define SCRIPT_CMD_VERTEX(list, cmd, offset) ((SVECTOR *)(SCRIPT_CMD_U16(cmd, offset) * 8 + (s32)(list)))

/* The unit's own uninitialized variables (its .bss, after
 * battle_8009E53C.c's; ASPSX 2.56 aligns each by its size up to a word:
 * decomp/Makefile). The users of the last, 800B3E04, end the unit before
 * 800B3F04. */
static ScriptEntry D_800C3BD0; /* the selected script */
static u8 *D_800C3BEC;         /* effect script cursor */
static s32 D_800C3BF0;         /* effect script step count */
static s32 D_800C3BF4;         /* unreferenced */
static DrawPrim8 D_800C3BF8;   /* the blend mode's texture page */
static ScreenFade D_800C3C00;  /* the second screen fade */
static s32 D_800C3C4C;         /* unreferenced */
static ScreenFade D_800C3C50;
static RECT D_800C3C9C;

Quake *D_800C3548 = NULL;
SVECTOR D_800C354C = {0, 0, 0};
ScreenFade *D_800C3554 = NULL;
ScreenFade *D_800C3558 = NULL;
u8 D_800C355C = 0;
LightFade *D_800C3560 = NULL;

/* Select script index of an effect script file: copy its entry into
 * D_800C3BD0 (relocating its offsets to addresses unless the file is already
 * relocated) and start its commands; its command count. */
s32 func_800B15D8(ScriptFile *file, s32 index) {
    ScriptEntry *entry = &file->entries[index];

    D_800C3BD0 = *entry;
    if (!(file->flags & 1)) {
        D_800C3BD0.vertices += (u32)entry;
        D_800C3BD0.normals += (u32)entry;
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

/* Build the GPU primitives of script entry's commands into prims: code,
 * colours (neutral 0x80 when unlit), texture coordinates, CLUT and texture
 * page; with blend (1-4) set, semi-transparent with that blend mode, which
 * is also written into the commands' texture pages. */
void func_800B1720(entry, prims, blend, shade)
    ScriptEntry *entry;
    u8 *prims;
    s32 blend;
    s32 shade;
{
    u8 *cmd;
    s32 count;
    s32 i;
    s32 abr;
    s32 kind;

    count = entry->count;
    cmd = entry->commands + (u32)entry;
    if (blend != 0) {
        abr = (blend - 1) & 3;
        abr <<= 5;
    }
    for (i = 0; i != count; i++) {
        prims[3] = cmd[0];
        prims[7] = cmd[3];
        SetShadeTex(prims, (u8)shade);
        if (blend != 0) {
            SetSemiTrans(prims, blend != 0);
        }
        kind = cmd[3] & 0x1C;
        kind |= ((cmd[2] ^ 1) & 1) << 8; /* unlit */
        switch (kind) {
        case 0x10:
        case 0x110:
            ((POLY_G3 *)prims)->r0 = cmd[4];
            ((POLY_G3 *)prims)->g0 = cmd[5];
            ((POLY_G3 *)prims)->b0 = cmd[6];
            ((POLY_G3 *)prims)->r1 = cmd[8];
            ((POLY_G3 *)prims)->g1 = cmd[9];
            ((POLY_G3 *)prims)->b1 = cmd[0xA];
            ((POLY_G3 *)prims)->r2 = cmd[0xC];
            ((POLY_G3 *)prims)->g2 = cmd[0xD];
            ((POLY_G3 *)prims)->b2 = cmd[0xE];
            break;
        case 0x18:
        case 0x118:
            ((POLY_G4 *)prims)->r0 = cmd[4];
            ((POLY_G4 *)prims)->g0 = cmd[5];
            ((POLY_G4 *)prims)->b0 = cmd[6];
            ((POLY_G4 *)prims)->r1 = cmd[8];
            ((POLY_G4 *)prims)->g1 = cmd[9];
            ((POLY_G4 *)prims)->b1 = cmd[0xA];
            ((POLY_G4 *)prims)->r2 = cmd[0xC];
            ((POLY_G4 *)prims)->g2 = cmd[0xD];
            ((POLY_G4 *)prims)->b2 = cmd[0xE];
            ((POLY_G4 *)prims)->r3 = cmd[0x10];
            ((POLY_G4 *)prims)->g3 = cmd[0x11];
            ((POLY_G4 *)prims)->b3 = cmd[0x12];
            break;
        case 0x0:
        case 0x8:
        case 0x100:
        case 0x108:
            ((POLY_F3 *)prims)->r0 = cmd[4];
            ((POLY_F3 *)prims)->g0 = cmd[5];
            ((POLY_F3 *)prims)->b0 = cmd[6];
            break;
        case 0x104:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_FT3 *)prims)->r0 = 0x80;
            ((POLY_FT3 *)prims)->g0 = 0x80;
            ((POLY_FT3 *)prims)->b0 = 0x80;
            goto ft3;
        case 0x4:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_FT3 *)prims)->r0 = cmd[0x10];
            ((POLY_FT3 *)prims)->g0 = cmd[0x11];
            ((POLY_FT3 *)prims)->b0 = cmd[0x12];
        ft3:
            ((POLY_FT3 *)prims)->tpage = SCRIPT_CMD_U16(cmd, 0xA);
            ((POLY_FT3 *)prims)->clut = SCRIPT_CMD_U16(cmd, 6);
            ((POLY_FT3 *)prims)->u0 = cmd[4];
            ((POLY_FT3 *)prims)->v0 = cmd[5];
            ((POLY_FT3 *)prims)->u1 = cmd[8];
            ((POLY_FT3 *)prims)->v1 = cmd[9];
            ((POLY_FT3 *)prims)->u2 = cmd[0xC];
            ((POLY_FT3 *)prims)->v2 = cmd[0xD];
            break;
        case 0x114:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_GT3 *)prims)->r0 = 0x80;
            ((POLY_GT3 *)prims)->g0 = 0x80;
            ((POLY_GT3 *)prims)->b0 = 0x80;
            ((POLY_GT3 *)prims)->r1 = 0x80;
            ((POLY_GT3 *)prims)->g1 = 0x80;
            ((POLY_GT3 *)prims)->b1 = 0x80;
            ((POLY_GT3 *)prims)->r2 = 0x80;
            ((POLY_GT3 *)prims)->g2 = 0x80;
            ((POLY_GT3 *)prims)->b2 = 0x80;
            goto gt3;
        case 0x14:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_GT3 *)prims)->r0 = cmd[0x10];
            ((POLY_GT3 *)prims)->g0 = cmd[0x11];
            ((POLY_GT3 *)prims)->b0 = cmd[0x12];
            ((POLY_GT3 *)prims)->r1 = cmd[0x14];
            ((POLY_GT3 *)prims)->g1 = cmd[0x15];
            ((POLY_GT3 *)prims)->b1 = cmd[0x16];
            ((POLY_GT3 *)prims)->r2 = cmd[0x18];
            ((POLY_GT3 *)prims)->g2 = cmd[0x19];
            ((POLY_GT3 *)prims)->b2 = cmd[0x1A];
        gt3:
            ((POLY_GT3 *)prims)->tpage = SCRIPT_CMD_U16(cmd, 0xA);
            ((POLY_GT3 *)prims)->clut = SCRIPT_CMD_U16(cmd, 6);
            ((POLY_GT3 *)prims)->u0 = cmd[4];
            ((POLY_GT3 *)prims)->v0 = cmd[5];
            ((POLY_GT3 *)prims)->u1 = cmd[8];
            ((POLY_GT3 *)prims)->v1 = cmd[9];
            ((POLY_GT3 *)prims)->u2 = cmd[0xC];
            ((POLY_GT3 *)prims)->v2 = cmd[0xD];
            break;
        case 0x10C:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_FT4 *)prims)->r0 = 0x80;
            ((POLY_FT4 *)prims)->g0 = 0x80;
            ((POLY_FT4 *)prims)->b0 = 0x80;
            goto ft4;
        case 0xC:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_FT4 *)prims)->r0 = cmd[0x14];
            ((POLY_FT4 *)prims)->g0 = cmd[0x15];
            ((POLY_FT4 *)prims)->b0 = cmd[0x16];
        ft4:
            ((POLY_FT4 *)prims)->tpage = SCRIPT_CMD_U16(cmd, 0xA);
            ((POLY_FT4 *)prims)->clut = SCRIPT_CMD_U16(cmd, 6);
            ((POLY_FT4 *)prims)->u0 = cmd[4];
            ((POLY_FT4 *)prims)->v0 = cmd[5];
            ((POLY_FT4 *)prims)->u1 = cmd[8];
            ((POLY_FT4 *)prims)->v1 = cmd[9];
            ((POLY_FT4 *)prims)->u2 = cmd[0xC];
            ((POLY_FT4 *)prims)->v2 = cmd[0xD];
            ((POLY_FT4 *)prims)->u3 = cmd[0x10];
            ((POLY_FT4 *)prims)->v3 = cmd[0x11];
            break;
        case 0x11C:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_GT4 *)prims)->r0 = 0x80;
            ((POLY_GT4 *)prims)->g0 = 0x80;
            ((POLY_GT4 *)prims)->b0 = 0x80;
            ((POLY_GT4 *)prims)->r1 = 0x80;
            ((POLY_GT4 *)prims)->g1 = 0x80;
            ((POLY_GT4 *)prims)->b1 = 0x80;
            ((POLY_GT4 *)prims)->r2 = 0x80;
            ((POLY_GT4 *)prims)->g2 = 0x80;
            ((POLY_GT4 *)prims)->b2 = 0x80;
            ((POLY_GT4 *)prims)->r3 = 0x80;
            ((POLY_GT4 *)prims)->g3 = 0x80;
            ((POLY_GT4 *)prims)->b3 = 0x80;
            goto gt4;
        case 0x1C:
            if (blend != 0) {
                SCRIPT_CMD_U16(cmd, 0xA) = (SCRIPT_CMD_U16(cmd, 0xA) & ~0x60) | abr;
            }
            ((POLY_GT4 *)prims)->r0 = cmd[0x14];
            ((POLY_GT4 *)prims)->g0 = cmd[0x15];
            ((POLY_GT4 *)prims)->b0 = cmd[0x16];
            ((POLY_GT4 *)prims)->r1 = cmd[0x18];
            ((POLY_GT4 *)prims)->g1 = cmd[0x19];
            ((POLY_GT4 *)prims)->b1 = cmd[0x1A];
            ((POLY_GT4 *)prims)->r2 = cmd[0x1C];
            ((POLY_GT4 *)prims)->g2 = cmd[0x1D];
            ((POLY_GT4 *)prims)->b2 = cmd[0x1E];
            ((POLY_GT4 *)prims)->r3 = cmd[0x20];
            ((POLY_GT4 *)prims)->g3 = cmd[0x21];
            ((POLY_GT4 *)prims)->b3 = cmd[0x22];
        gt4:
            ((POLY_GT4 *)prims)->tpage = SCRIPT_CMD_U16(cmd, 0xA);
            ((POLY_GT4 *)prims)->clut = SCRIPT_CMD_U16(cmd, 6);
            ((POLY_GT4 *)prims)->u0 = cmd[4];
            ((POLY_GT4 *)prims)->v0 = cmd[5];
            ((POLY_GT4 *)prims)->u1 = cmd[8];
            ((POLY_GT4 *)prims)->v1 = cmd[9];
            ((POLY_GT4 *)prims)->u2 = cmd[0xC];
            ((POLY_GT4 *)prims)->v2 = cmd[0xD];
            ((POLY_GT4 *)prims)->u3 = cmd[0x10];
            ((POLY_GT4 *)prims)->v3 = cmd[0x11];
            break;
        }
        prims += (cmd[0] + 1) * 4;
        cmd += (cmd[1] + 1) * 4;
    }
}

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

    if ((u8 *)D_80059580 + sizeof(DrawPrim8) < D_80059534) {
        D_80059580 = (SpriteQueueEntry *)((u8 *)D_80059580 + sizeof(DrawPrim8));
        *prim = D_800C3BF8;
        AddPrim(ot, prim);
    }
}

/* Point v0-v2 (v0-v3) at the vertices that a command's indices at offsets
 * o0-o3 name. */
#define SET_VERTICES3(o0, o1, o2)                                                                 \
    do {                                                                                           \
        v0 = SCRIPT_CMD_VERTEX(vertices, cmd, o0);                                                 \
        v1 = SCRIPT_CMD_VERTEX(vertices, cmd, o1);                                                 \
        v2 = SCRIPT_CMD_VERTEX(vertices, cmd, o2);                                                 \
    } while (0)

#define SET_VERTICES4(o0, o1, o2, o3)                                                             \
    do {                                                                                           \
        v0 = SCRIPT_CMD_VERTEX(vertices, cmd, o0);                                                 \
        v1 = SCRIPT_CMD_VERTEX(vertices, cmd, o1);                                                 \
        v2 = SCRIPT_CMD_VERTEX(vertices, cmd, o2);                                                 \
        v3 = SCRIPT_CMD_VERTEX(vertices, cmd, o3);                                                 \
    } while (0)

/* Draw script entry's commands with their built primitives packets (800B1720,
 * one set per display buffer) into ordering table ot: transform each
 * polygon's vertices, cull back faces and add the facing ones at their
 * average depth (shifted by D_80050100, plus bias, at least 5); untextured
 * ones in blend mode blend (1-4) also get the blend mode's texture page.
 * Commands with flag bit 0 clear (kind 0x100) carry normal indices: F3, F4
 * and FT3 primitives are coloured from one normal, GT3 from three, keeping
 * the GPU code byte that the colour's last byte overwrites; the other such
 * kinds keep the colours 800B1720 built. Only the plain kinds fetch their
 * vertices through SET_VERTICES3/4 (the macros' loop level is part of what
 * gives the cursor and v0 their original registers). The binary bounds
 * that choice: a loop note after a lighting call adds a load-delay nop
 * where the original reads `lhu v0; lhu v1; sll` (800B22F8), so the lit
 * fetches were written out by hand, and with fewer than two loop-wrapped
 * fetch groups the function grows to 757 words. Any two to eight of the
 * eleven groups that start their own block match; the eight plain kinds
 * are the reading that keeps each half uniform. */
void func_800B1F6C(entry, packets, ot, unused, bias, blend)
    ScriptEntry *entry;
    u8 *packets;
    u32 *ot;
    s32 unused;
    s32 bias;
    s32 blend;
{
    SVECTOR *vertices;
    SVECTOR *normals;
    SVECTOR *v0;
    SVECTOR *v1;
    SVECTOR *v2;
    SVECTOR *v3;
    s32 count;
    s32 i;
    long opz;
    long otz;
    long p;
    long flag;
    s32 kind;
    u8 *prims;
    u8 *cmd;

    if (blend != 0) {
        SetDrawTPage((DR_TPAGE *)&D_800C3BF8, 0, 0, ((blend - 1) & 3) << 5);
    }
    prims = packets;
    cmd = entry->commands + (u32)entry;
    vertices = (SVECTOR *)(entry->vertices + (u32)entry);
    count = entry->count;
    normals = (SVECTOR *)(entry->normals + (u32)entry);
    for (i = 0; i != count; i++) {
        kind = cmd[3] & 0x1C;
        kind |= ((cmd[2] ^ 1) & 1) << 8; /* lit from normals */
        switch (kind) {
        case 0x0:
            SET_VERTICES3(0x8, 0xA, 0xC);
            break;
        case 0x10:
            SET_VERTICES3(0x10, 0x12, 0x14);
            break;
        case 0x18:
            SET_VERTICES4(0x14, 0x16, 0x18, 0x1A);
            break;
        case 0x8:
            SET_VERTICES4(0x8, 0xA, 0xC, 0xE);
            break;
        case 0x4:
            SET_VERTICES3(0x14, 0x16, 0x18);
            break;
        case 0x14:
            SET_VERTICES3(0x1C, 0x1E, 0x20);
            break;
        case 0xC:
            SET_VERTICES4(0x18, 0x1A, 0x1C, 0x1E);
            break;
        case 0x1C:
            SET_VERTICES4(0x24, 0x26, 0x28, 0x2A);
            break;
        case 0x100:
            v0 = SCRIPT_CMD_VERTEX(normals, cmd, 0x8);
            {
                u8 code = prims[7];

                NormalColor(v0, (CVECTOR *)(prims + 4));
                prims[7] = code;
            }
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0xA);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0xC);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0xE);
            break;
        case 0x110:
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x10);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x16);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x1A);
            break;
        case 0x108:
            v0 = SCRIPT_CMD_VERTEX(normals, cmd, 0x8);
            {
                u8 code = prims[7];

                NormalColor(v0, (CVECTOR *)(prims + 4));
                prims[7] = code;
            }
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0xA);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0xC);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0xE);
            v3 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x10);
            break;
        case 0x104:
            v0 = SCRIPT_CMD_VERTEX(normals, cmd, 0x10);
            {
                u8 code = prims[7];

                NormalColor(v0, (CVECTOR *)(prims + 4));
                prims[7] = code;
            }
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x12);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x14);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x16);
            break;
        case 0x114:
            v0 = SCRIPT_CMD_VERTEX(normals, cmd, 0x10);
            v1 = SCRIPT_CMD_VERTEX(normals, cmd, 0x14);
            v2 = SCRIPT_CMD_VERTEX(normals, cmd, 0x18);
            {
                u8 code = prims[7];

                NormalColor3(v0, v1, v2, (CVECTOR *)(prims + 4), (CVECTOR *)(prims + 0x10),
                             (CVECTOR *)(prims + 0x1C));
                prims[7] = code;
            }
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x12);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x16);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x1A);
            break;
        case 0x10C:
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x16);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x18);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x1A);
            v3 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x1C);
            break;
        case 0x118:
        case 0x11C:
            v0 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x16);
            v1 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x1A);
            v2 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x1E);
            v3 = SCRIPT_CMD_VERTEX(vertices, cmd, 0x22);
            break;
        }
        kind = cmd[3] & 0x1C;
        switch (kind) {
        case 0x0:
            {
                long flag;

                gte_ldv3(v0, v1, v2);
                gte_rtpt();
                gte_stflg(&flag);
                gte_nclip();
                if (!(flag & 0x4000)) {
                    gte_stopz(&opz);
                    gte_avsz3();
                    if (opz > 0) {
                        gte_stotz(&otz);
                        otz = (otz >> D_80050100) + bias;
                        if (otz < 5) {
                            otz = 5;
                        }
                        if (otz < 0x1000) {
                            gte_stsxy3(&((POLY_F3 *)prims)->x0, &((POLY_F3 *)prims)->x1, &((POLY_F3 *)prims)->x2);
                            addPrim(ot + otz, prims);
                            if (blend != 0) {
                                func_800B1F0C(ot + otz);
                            }
                        }
                    }
                }
            }
            break;
        case 0x10:
            {
                long flag;

                gte_ldv3(v0, v1, v2);
                gte_rtpt();
                gte_stflg(&flag);
                gte_nclip();
                if (!(flag & 0x4000)) {
                    gte_stopz(&opz);
                    gte_avsz3();
                    if (opz > 0) {
                        gte_stotz(&otz);
                        otz = (otz >> D_80050100) + bias;
                        if (otz < 5) {
                            otz = 5;
                        }
                        if (otz < 0x1000) {
                            gte_stsxy3(&((POLY_G3 *)prims)->x0, &((POLY_G3 *)prims)->x1, &((POLY_G3 *)prims)->x2);
                            addPrim(ot + otz, prims);
                            if (blend != 0) {
                                func_800B1F0C(ot + otz);
                            }
                        }
                    }
                }
            }
            break;
        case 0x4:
            {
                long flag;

                gte_ldv3(v0, v1, v2);
                gte_rtpt();
                gte_stflg(&flag);
                gte_nclip();
                if (!(flag & 0x4000)) {
                    gte_stopz(&opz);
                    gte_avsz3();
                    if (opz > 0) {
                        gte_stotz(&otz);
                        otz = (otz >> D_80050100) + bias;
                        if (otz < 5) {
                            otz = 5;
                        }
                        if (otz < 0x1000) {
                            gte_stsxy3(&((POLY_FT3 *)prims)->x0, &((POLY_FT3 *)prims)->x1, &((POLY_FT3 *)prims)->x2);
                            addPrim(ot + otz, prims);
                        }
                    }
                }
            }
            break;
        case 0x14:
            {
                long flag;

                gte_ldv3(v0, v1, v2);
                gte_rtpt();
                gte_stflg(&flag);
                gte_nclip();
                if (!(flag & 0x4000)) {
                    gte_stopz(&opz);
                    gte_avsz3();
                    if (opz > 0) {
                        gte_stotz(&otz);
                        otz = (otz >> D_80050100) + bias;
                        if (otz < 5) {
                            otz = 5;
                        }
                        if (otz < 0x1000) {
                            gte_stsxy3(&((POLY_GT3 *)prims)->x0, &((POLY_GT3 *)prims)->x1, &((POLY_GT3 *)prims)->x2);
                            addPrim(ot + otz, prims);
                        }
                    }
                }
            }
            break;
        case 0x8:
            opz = func_8004A83C(v0, v1, v2, v3, (long *)&((POLY_F4 *)prims)->x0, (long *)&((POLY_F4 *)prims)->x1,
                                (long *)&((POLY_F4 *)prims)->x2, (long *)&((POLY_F4 *)prims)->x3, &p, &otz, &flag);
            if (opz > 0) {
                otz = (otz >> D_80050100) + bias;
                if (otz < 5) {
                    otz = 5;
                }
                if (!(flag & 0x8000) && otz < 0x1000) {
                    addPrim(ot + otz, prims);
                    if (blend != 0) {
                        func_800B1F0C(ot + otz);
                    }
                }
            }
            break;
        case 0x18:
            opz = func_8004A83C(v0, v1, v2, v3, (long *)&((POLY_G4 *)prims)->x0, (long *)&((POLY_G4 *)prims)->x1,
                                (long *)&((POLY_G4 *)prims)->x2, (long *)&((POLY_G4 *)prims)->x3, &p, &otz, &flag);
            if (opz > 0) {
                otz = (otz >> D_80050100) + bias;
                if (otz < 5) {
                    otz = 5;
                }
                if (!(flag & 0x8000) && otz < 0x1000) {
                    addPrim(ot + otz, prims);
                    if (blend != 0) {
                        func_800B1F0C(ot + otz);
                    }
                }
            }
            break;
        case 0xC:
            opz = func_8004A83C(v0, v1, v2, v3, (long *)&((POLY_FT4 *)prims)->x0, (long *)&((POLY_FT4 *)prims)->x1,
                                (long *)&((POLY_FT4 *)prims)->x2, (long *)&((POLY_FT4 *)prims)->x3, &p, &otz, &flag);
            if (opz > 0) {
                otz = (otz >> D_80050100) + bias;
                if (otz < 5) {
                    otz = 5;
                }
                if (!(flag & 0x8000) && otz < 0x1000) {
                    addPrim(ot + otz, prims);
                }
            }
            break;
        case 0x1C:
            opz = func_8004A83C(v0, v1, v2, v3, (long *)&((POLY_GT4 *)prims)->x0, (long *)&((POLY_GT4 *)prims)->x1,
                                (long *)&((POLY_GT4 *)prims)->x2, (long *)&((POLY_GT4 *)prims)->x3, &p, &otz, &flag);
            if (opz > 0) {
                otz = (otz >> D_80050100) + bias;
                if (otz < 5) {
                    otz = 5;
                }
                if (!(flag & 0x8000) && otz < 0x1000) {
                    addPrim(ot + otz, prims);
                }
            }
            break;
        }
        prims += (cmd[0] + 1) * 4;
        cmd += (cmd[1] + 1) * 4;
    }
}

#undef SET_VERTICES4
#undef SET_VERTICES3

/* Apply clamped RGB offsets to both packet buffers of an unrelocated script
 * entry. Lit primitives use their command colours; unlit textured primitives
 * start at neutral 0x80. The two packet streams advance by each command's
 * packet size, while the command stream advances by its separate length. */
void func_800B2AEC(entry, packets0, packets1, red, green, blue)
    ScriptEntry *entry;
    u8 *packets0;
    u8 *packets1;
    s32 red;
    s32 green;
    s32 blue;
{
    u8 *cmd;
    s32 i = 0;
    s32 count;
    s32 kind;
    s32 size;

    cmd = entry->commands + (u32)entry;
    count = entry->count;
    for (; i != count; i++) {
        kind = cmd[3] & 0x1C;
        kind |= ((cmd[2] ^ 1) & 1) << 8;
        switch (kind) {
        case 0x0:
            ((POLY_F3 *)packets0)->r0 = func_80021AD8(cmd[0x4], red);
            ((POLY_F3 *)packets0)->g0 = func_80021AD8(cmd[0x5], green);
            ((POLY_F3 *)packets0)->b0 = func_80021AD8(cmd[0x6], blue);
            ((POLY_F3 *)packets1)->r0 = ((POLY_F3 *)packets0)->r0;
            ((POLY_F3 *)packets1)->g0 = ((POLY_F3 *)packets0)->g0;
            ((POLY_F3 *)packets1)->b0 = ((POLY_F3 *)packets0)->b0;
            break;
        case 0x10:
            ((POLY_G3 *)packets0)->r0 = func_80021AD8(cmd[0x4], red);
            ((POLY_G3 *)packets0)->g0 = func_80021AD8(cmd[0x5], green);
            ((POLY_G3 *)packets0)->b0 = func_80021AD8(cmd[0x6], blue);
            ((POLY_G3 *)packets0)->r1 = func_80021AD8(cmd[0x8], red);
            ((POLY_G3 *)packets0)->g1 = func_80021AD8(cmd[0x9], green);
            ((POLY_G3 *)packets0)->b1 = func_80021AD8(cmd[0xA], blue);
            ((POLY_G3 *)packets0)->r2 = func_80021AD8(cmd[0xC], red);
            ((POLY_G3 *)packets0)->g2 = func_80021AD8(cmd[0xD], green);
            ((POLY_G3 *)packets0)->b2 = func_80021AD8(cmd[0xE], blue);
            ((POLY_G3 *)packets1)->r0 = ((POLY_G3 *)packets0)->r0;
            ((POLY_G3 *)packets1)->g0 = ((POLY_G3 *)packets0)->g0;
            ((POLY_G3 *)packets1)->b0 = ((POLY_G3 *)packets0)->b0;
            ((POLY_G3 *)packets1)->r1 = ((POLY_G3 *)packets0)->r1;
            ((POLY_G3 *)packets1)->g1 = ((POLY_G3 *)packets0)->g1;
            ((POLY_G3 *)packets1)->b1 = ((POLY_G3 *)packets0)->b1;
            ((POLY_G3 *)packets1)->r2 = ((POLY_G3 *)packets0)->r2;
            ((POLY_G3 *)packets1)->g2 = ((POLY_G3 *)packets0)->g2;
            ((POLY_G3 *)packets1)->b2 = ((POLY_G3 *)packets0)->b2;
            break;
        case 0x18:
            ((POLY_G4 *)packets0)->r0 = func_80021AD8(cmd[0x4], red);
            ((POLY_G4 *)packets0)->g0 = func_80021AD8(cmd[0x5], green);
            ((POLY_G4 *)packets0)->b0 = func_80021AD8(cmd[0x6], blue);
            ((POLY_G4 *)packets0)->r1 = func_80021AD8(cmd[0x8], red);
            ((POLY_G4 *)packets0)->g1 = func_80021AD8(cmd[0x9], green);
            ((POLY_G4 *)packets0)->b1 = func_80021AD8(cmd[0xA], blue);
            ((POLY_G4 *)packets0)->r2 = func_80021AD8(cmd[0xC], red);
            ((POLY_G4 *)packets0)->g2 = func_80021AD8(cmd[0xD], green);
            ((POLY_G4 *)packets0)->b2 = func_80021AD8(cmd[0xE], blue);
            ((POLY_G4 *)packets0)->r3 = func_80021AD8(cmd[0x10], red);
            ((POLY_G4 *)packets0)->g3 = func_80021AD8(cmd[0x11], green);
            ((POLY_G4 *)packets0)->b3 = func_80021AD8(cmd[0x12], blue);
            ((POLY_G4 *)packets1)->r0 = ((POLY_G4 *)packets0)->r0;
            ((POLY_G4 *)packets1)->g0 = ((POLY_G4 *)packets0)->g0;
            ((POLY_G4 *)packets1)->b0 = ((POLY_G4 *)packets0)->b0;
            ((POLY_G4 *)packets1)->r1 = ((POLY_G4 *)packets0)->r1;
            ((POLY_G4 *)packets1)->g1 = ((POLY_G4 *)packets0)->g1;
            ((POLY_G4 *)packets1)->b1 = ((POLY_G4 *)packets0)->b1;
            ((POLY_G4 *)packets1)->r2 = ((POLY_G4 *)packets0)->r2;
            ((POLY_G4 *)packets1)->g2 = ((POLY_G4 *)packets0)->g2;
            ((POLY_G4 *)packets1)->b2 = ((POLY_G4 *)packets0)->b2;
            ((POLY_G4 *)packets1)->r3 = ((POLY_G4 *)packets0)->r3;
            ((POLY_G4 *)packets1)->g3 = ((POLY_G4 *)packets0)->g3;
            ((POLY_G4 *)packets1)->b3 = ((POLY_G4 *)packets0)->b3;
            break;
        case 0x8:
            ((POLY_F4 *)packets0)->r0 = func_80021AD8(cmd[0x4], red);
            ((POLY_F4 *)packets0)->g0 = func_80021AD8(cmd[0x5], green);
            ((POLY_F4 *)packets0)->b0 = func_80021AD8(cmd[0x6], blue);
            ((POLY_F4 *)packets1)->r0 = ((POLY_F4 *)packets0)->r0;
            ((POLY_F4 *)packets1)->g0 = ((POLY_F4 *)packets0)->g0;
            ((POLY_F4 *)packets1)->b0 = ((POLY_F4 *)packets0)->b0;
            break;
        case 0x104:
            ((POLY_FT3 *)packets0)->r0 = func_80021AD8(0x80, red);
            ((POLY_FT3 *)packets0)->g0 = func_80021AD8(0x80, green);
            ((POLY_FT3 *)packets0)->b0 = func_80021AD8(0x80, blue);
            ((POLY_FT3 *)packets1)->r0 = ((POLY_FT3 *)packets0)->r0;
            ((POLY_FT3 *)packets1)->g0 = ((POLY_FT3 *)packets0)->g0;
            ((POLY_FT3 *)packets1)->b0 = ((POLY_FT3 *)packets0)->b0;
            break;
        case 0x4:
            ((POLY_FT3 *)packets0)->r0 = func_80021AD8(cmd[0x10], red);
            ((POLY_FT3 *)packets0)->g0 = func_80021AD8(cmd[0x11], green);
            ((POLY_FT3 *)packets0)->b0 = func_80021AD8(cmd[0x12], blue);
            ((POLY_FT3 *)packets1)->r0 = ((POLY_FT3 *)packets0)->r0;
            ((POLY_FT3 *)packets1)->g0 = ((POLY_FT3 *)packets0)->g0;
            ((POLY_FT3 *)packets1)->b0 = ((POLY_FT3 *)packets0)->b0;
            break;
        case 0x114:
            ((POLY_GT3 *)packets0)->r0 = func_80021AD8(0x80, red);
            ((POLY_GT3 *)packets0)->g0 = func_80021AD8(0x80, green);
            ((POLY_GT3 *)packets0)->b0 = func_80021AD8(0x80, blue);
            ((POLY_GT3 *)packets0)->r1 = func_80021AD8(0x80, red);
            ((POLY_GT3 *)packets0)->g1 = func_80021AD8(0x80, green);
            ((POLY_GT3 *)packets0)->b1 = func_80021AD8(0x80, blue);
            ((POLY_GT3 *)packets0)->r2 = func_80021AD8(0x80, red);
            ((POLY_GT3 *)packets0)->g2 = func_80021AD8(0x80, green);
            ((POLY_GT3 *)packets0)->b2 = func_80021AD8(0x80, blue);
            ((POLY_GT3 *)packets1)->r0 = ((POLY_GT3 *)packets0)->r0;
            ((POLY_GT3 *)packets1)->g0 = ((POLY_GT3 *)packets0)->g0;
            ((POLY_GT3 *)packets1)->b0 = ((POLY_GT3 *)packets0)->b0;
            ((POLY_GT3 *)packets1)->r1 = ((POLY_GT3 *)packets0)->r1;
            ((POLY_GT3 *)packets1)->g1 = ((POLY_GT3 *)packets0)->g1;
            ((POLY_GT3 *)packets1)->b1 = ((POLY_GT3 *)packets0)->b1;
            ((POLY_GT3 *)packets1)->r2 = ((POLY_GT3 *)packets0)->r2;
            ((POLY_GT3 *)packets1)->g2 = ((POLY_GT3 *)packets0)->g2;
            ((POLY_GT3 *)packets1)->b2 = ((POLY_GT3 *)packets0)->b2;
            break;
        case 0x14:
            ((POLY_GT3 *)packets0)->r0 = func_80021AD8(cmd[0x10], red);
            ((POLY_GT3 *)packets0)->g0 = func_80021AD8(cmd[0x11], green);
            ((POLY_GT3 *)packets0)->b0 = func_80021AD8(cmd[0x12], blue);
            ((POLY_GT3 *)packets0)->r1 = func_80021AD8(cmd[0x14], red);
            ((POLY_GT3 *)packets0)->g1 = func_80021AD8(cmd[0x15], green);
            ((POLY_GT3 *)packets0)->b1 = func_80021AD8(cmd[0x16], blue);
            ((POLY_GT3 *)packets0)->r2 = func_80021AD8(cmd[0x18], red);
            ((POLY_GT3 *)packets0)->g2 = func_80021AD8(cmd[0x19], green);
            ((POLY_GT3 *)packets0)->b2 = func_80021AD8(cmd[0x1A], blue);
            ((POLY_GT3 *)packets1)->r0 = ((POLY_GT3 *)packets0)->r0;
            ((POLY_GT3 *)packets1)->g0 = ((POLY_GT3 *)packets0)->g0;
            ((POLY_GT3 *)packets1)->b0 = ((POLY_GT3 *)packets0)->b0;
            ((POLY_GT3 *)packets1)->r1 = ((POLY_GT3 *)packets0)->r1;
            ((POLY_GT3 *)packets1)->g1 = ((POLY_GT3 *)packets0)->g1;
            ((POLY_GT3 *)packets1)->b1 = ((POLY_GT3 *)packets0)->b1;
            ((POLY_GT3 *)packets1)->r2 = ((POLY_GT3 *)packets0)->r2;
            ((POLY_GT3 *)packets1)->g2 = ((POLY_GT3 *)packets0)->g2;
            ((POLY_GT3 *)packets1)->b2 = ((POLY_GT3 *)packets0)->b2;
            break;
        case 0x10C:
            ((POLY_FT4 *)packets0)->r0 = func_80021AD8(0x80, red);
            ((POLY_FT4 *)packets0)->g0 = func_80021AD8(0x80, green);
            ((POLY_FT4 *)packets0)->b0 = func_80021AD8(0x80, blue);
            ((POLY_FT4 *)packets1)->r0 = ((POLY_FT4 *)packets0)->r0;
            ((POLY_FT4 *)packets1)->g0 = ((POLY_FT4 *)packets0)->g0;
            ((POLY_FT4 *)packets1)->b0 = ((POLY_FT4 *)packets0)->b0;
            break;
        case 0xC:
            ((POLY_FT4 *)packets0)->r0 = func_80021AD8(cmd[0x14], red);
            ((POLY_FT4 *)packets0)->g0 = func_80021AD8(cmd[0x15], green);
            ((POLY_FT4 *)packets0)->b0 = func_80021AD8(cmd[0x16], blue);
            ((POLY_FT4 *)packets1)->r0 = ((POLY_FT4 *)packets0)->r0;
            ((POLY_FT4 *)packets1)->g0 = ((POLY_FT4 *)packets0)->g0;
            ((POLY_FT4 *)packets1)->b0 = ((POLY_FT4 *)packets0)->b0;
            break;
        case 0x11C:
            ((POLY_GT4 *)packets0)->r0 = func_80021AD8(0x80, red);
            ((POLY_GT4 *)packets0)->g0 = func_80021AD8(0x80, green);
            ((POLY_GT4 *)packets0)->b0 = func_80021AD8(0x80, blue);
            ((POLY_GT4 *)packets0)->r1 = func_80021AD8(0x80, red);
            ((POLY_GT4 *)packets0)->g1 = func_80021AD8(0x80, green);
            ((POLY_GT4 *)packets0)->b1 = func_80021AD8(0x80, blue);
            ((POLY_GT4 *)packets0)->r2 = func_80021AD8(0x80, red);
            ((POLY_GT4 *)packets0)->g2 = func_80021AD8(0x80, green);
            ((POLY_GT4 *)packets0)->b2 = func_80021AD8(0x80, blue);
            ((POLY_GT4 *)packets0)->r3 = func_80021AD8(0x80, red);
            ((POLY_GT4 *)packets0)->g3 = func_80021AD8(0x80, green);
            ((POLY_GT4 *)packets0)->b3 = func_80021AD8(0x80, blue);
            ((POLY_GT4 *)packets1)->r0 = ((POLY_GT4 *)packets0)->r0;
            ((POLY_GT4 *)packets1)->g0 = ((POLY_GT4 *)packets0)->g0;
            ((POLY_GT4 *)packets1)->b0 = ((POLY_GT4 *)packets0)->b0;
            ((POLY_GT4 *)packets1)->r1 = ((POLY_GT4 *)packets0)->r1;
            ((POLY_GT4 *)packets1)->g1 = ((POLY_GT4 *)packets0)->g1;
            ((POLY_GT4 *)packets1)->b1 = ((POLY_GT4 *)packets0)->b1;
            ((POLY_GT4 *)packets1)->r2 = ((POLY_GT4 *)packets0)->r2;
            ((POLY_GT4 *)packets1)->g2 = ((POLY_GT4 *)packets0)->g2;
            ((POLY_GT4 *)packets1)->b2 = ((POLY_GT4 *)packets0)->b2;
            ((POLY_GT4 *)packets1)->r3 = ((POLY_GT4 *)packets0)->r3;
            ((POLY_GT4 *)packets1)->g3 = ((POLY_GT4 *)packets0)->g3;
            ((POLY_GT4 *)packets1)->b3 = ((POLY_GT4 *)packets0)->b3;
            break;
        case 0x1C:
            ((POLY_GT4 *)packets0)->r0 = func_80021AD8(cmd[0x14], red);
            ((POLY_GT4 *)packets0)->g0 = func_80021AD8(cmd[0x15], green);
            ((POLY_GT4 *)packets0)->b0 = func_80021AD8(cmd[0x16], blue);
            ((POLY_GT4 *)packets0)->r1 = func_80021AD8(cmd[0x18], red);
            ((POLY_GT4 *)packets0)->g1 = func_80021AD8(cmd[0x19], green);
            ((POLY_GT4 *)packets0)->b1 = func_80021AD8(cmd[0x1A], blue);
            ((POLY_GT4 *)packets0)->r2 = func_80021AD8(cmd[0x1C], red);
            ((POLY_GT4 *)packets0)->g2 = func_80021AD8(cmd[0x1D], green);
            ((POLY_GT4 *)packets0)->b2 = func_80021AD8(cmd[0x1E], blue);
            ((POLY_GT4 *)packets0)->r3 = func_80021AD8(cmd[0x20], red);
            ((POLY_GT4 *)packets0)->g3 = func_80021AD8(cmd[0x21], green);
            ((POLY_GT4 *)packets0)->b3 = func_80021AD8(cmd[0x22], blue);
            ((POLY_GT4 *)packets1)->r0 = ((POLY_GT4 *)packets0)->r0;
            ((POLY_GT4 *)packets1)->g0 = ((POLY_GT4 *)packets0)->g0;
            ((POLY_GT4 *)packets1)->b0 = ((POLY_GT4 *)packets0)->b0;
            ((POLY_GT4 *)packets1)->r1 = ((POLY_GT4 *)packets0)->r1;
            ((POLY_GT4 *)packets1)->g1 = ((POLY_GT4 *)packets0)->g1;
            ((POLY_GT4 *)packets1)->b1 = ((POLY_GT4 *)packets0)->b1;
            ((POLY_GT4 *)packets1)->r2 = ((POLY_GT4 *)packets0)->r2;
            ((POLY_GT4 *)packets1)->g2 = ((POLY_GT4 *)packets0)->g2;
            ((POLY_GT4 *)packets1)->b2 = ((POLY_GT4 *)packets0)->b2;
            ((POLY_GT4 *)packets1)->r3 = ((POLY_GT4 *)packets0)->r3;
            ((POLY_GT4 *)packets1)->g3 = ((POLY_GT4 *)packets0)->g3;
            ((POLY_GT4 *)packets1)->b3 = ((POLY_GT4 *)packets0)->b3;
            break;
        }
        size = (cmd[0] + 1) * 4;
        packets0 += size;
        packets1 += size;
        cmd += (cmd[1] + 1) * 4;
    }
}

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
    func_8001CD94(&quake->task);
    func_800320E8(quake);
    D_800C3548 = NULL;
}

/* The quake task, created at rest or restarted from its current amplitude. */
Quake *func_800B35C0(void) {
    Quake *quake;

    if (D_800C3548 == NULL) {
        quake = (Quake *)func_8001CD08(NULL, sizeof(Quake) - sizeof(Task));
        func_8001CD6C(&quake->task, (void (*)(Task *))func_800B3358);
        func_8001CD74(&quake->task, (void (*)(Task *))func_800B3588);
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

/* Screen fade update: ease the colour to the target over the frames left;
 * end once it is black. */
void func_800B36BC(Task *task) {
    ScreenFade *fade = (ScreenFade *)task;
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

/* End the screen fade tasks. */
void func_800B383C(ScreenFade *fade) {
    func_8001CB48(&fade->draw);
    func_8001CD94(&fade->task);
    D_800C3558 = NULL;
}

/* Draw the screen fade: a blended rectangle over the whole screen. */
void func_800B3878(Task *draw) {
    POLY_F4 *poly = (POLY_F4 *)D_80059580;
    ScreenFade *fade = draw->data;
    DR_MODE *mode;

    if ((u8 *)D_80059580 + 0x50 < D_80059534) {
        D_80059580 = (SpriteQueueEntry *)((u8 *)D_80059580 + sizeof(POLY_F4) + sizeof(DR_MODE));
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
        AddPrim((u32 *)D_8005956C + 2, poly);
        AddPrim((u32 *)D_8005956C + 2, mode);
    }
}

/* Fade the second screen fade (800B39C0). */
void func_800B397C(s32 frames, s32 blend, u8 r, u8 g, u8 b) {
    D_800C355C = 1;
    func_800B39C0(frames, blend, r, g, b);
    D_800C355C = 0;
}

/* Fade the screen (800B36BC) to colour r, g, b over frames * 2 frames with
 * the given blend mode; starts the fade tasks from black, or eases on from
 * the current colour when a fade runs (the second fade, D_800C3C00, when
 * D_800C355C is set). Defined without a prototype: the colours arrive as
 * bytes. */
void func_800B39C0(frames, blend, r, g, b)
    s32 frames;
    s32 blend;
    u8 r;
    u8 g;
    u8 b;
{
    ScreenFade *fade;

    if (D_800D3638 != 0) {
        return;
    }
    frames *= 2;
    if (D_800C355C != 0) {
        if (D_800C3558 == NULL) {
            fade = &D_800C3C00;
            D_800C3554 = fade;
        } else {
            fade = D_800C3554;
            goto resume;
        }
    } else {
        if (D_800C3558 == NULL) {
            fade = &D_800C3C50;
            D_800C3558 = fade;
        } else {
            fade = D_800C3558;
            goto resume;
        }
    }
    func_8001CC18(NULL, &fade->task);
    func_8001CA58(&fade->task, &fade->draw);
    fade->task.link.word &= 0x7FFFFFFF;
    if (D_800591AC != 0) {
        D_80059464--;
    }
    func_8001CD6C(&fade->task, func_800B36BC);
    func_8001CD64(&fade->draw, func_800B3878);
    func_8001CD74(&fade->task, (void (*)(Task *))func_800B383C);
    fade->task.data = fade;
    fade->draw.data = fade;
    fade->field40 = 0;
    fade->from[0] = 0;
    fade->from[1] = 0;
    fade->from[2] = 0;
    goto start;
resume:
    fade->from[0] = fade->colour[0];
    fade->from[1] = fade->colour[1];
    fade->from[2] = fade->colour[2];
start:
    fade->blend = blend;
    fade->to[0] = r;
    fade->to[1] = g;
    fade->to[2] = b;
    fade->total = frames;
    fade->left = frames;
    func_800B36BC(&fade->task);
}

/* The screen fade's blend mode (1 when none runs). */
u8 func_800B3B6C(void) {
    if (D_800C3558 != NULL) {
        return D_800C3558->blend;
    }
    return 1;
}

/* Light fade update: ease the level from its start to its target over the
 * frames left; end once it is zero. */
void func_800B3B94(Task *task) {
    LightFade *fade = (LightFade *)task;
    s32 left;
    s32 to;
    s32 fraction;
    s32 level;

    if (fade->left != 0) {
        left = fade->left - 1;
        fade->left = left;
        fraction = (left << 5) / fade->total;
        to = fade->to;
        level = to - fade->from;
        level *= fraction;
        level /= 32;
        level = to - level;
        fade->level = level;
    } else if (fade->level == 0) {
        fade->task.destroy(&fade->task);
    }
}

/* End the light fade tasks and restore the stage lights (800A6F98). */
void func_800B3C2C(LightFade *fade) {
    func_8001CB48(&fade->draw);
    func_8001CD94(&fade->task);
    func_800320E8(fade);
    D_800C3560 = NULL;
    func_800A6F98();
}

/* Apply the light fade's level to light slot 0 when it changed. */
void func_800B3C74(Task *draw) {
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
        D_800C3560 = fade = (LightFade *)func_8001D1D8(sizeof(LightFade), NULL, func_800B3B94, func_800B3C74,
                                                    (void (*)(Task *))func_800B3C2C);
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
    func_800B3B94(&fade->task);
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
