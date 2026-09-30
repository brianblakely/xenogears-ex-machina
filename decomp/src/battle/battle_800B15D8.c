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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1EA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B1F6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B2AEC);

void func_800B3348(void) {
}

void func_800B3350(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3358);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B35C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3658);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B36BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B383C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3878);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B397C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B39C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3B6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3B94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3C2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800B3C74);

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

/* Clear D_800D2D68 and D_800C374C. */
void func_800BE108(void) {
    D_800D2D68 = 0;
    D_800C374C = 0;
}

/* Popup update: spin and shrink it, fade its colour, end it with its life. */
void func_800BE11C(NumberPopup *popup) {
    popup->angle.vz += popup->spin;
    popup->scale.vx += 0x330;
    popup->scale.vy += 0x330;
    popup->scale.vz += 0x330;
    popup->colour.rgbc[0] = func_80021AD8(popup->colour.rgbc[0], -4);
    popup->colour.rgbc[1] = func_80021AD8(popup->colour.rgbc[1], -4);
    popup->colour.rgbc[2] = func_80021AD8(popup->colour.rgbc[2], -4);
    if (--popup->life == 0) {
        popup->destroy(popup);
    }
}

/* Popup drawing: its glyphs six times, each copy turned back 5 more and
 * shrunk by 0x330, centred on the screen from the geometry offset. */
void func_800BE1C4(PopupTask *task) {
    Matrix m;
    SVector unused; /* allocated in the original frame */
    SVector angle;
    Vector offset;
    Vector scale;
    s32 x;
    s32 y;
    NumberPopup *popup = task->popup;
    s32 i;
    s32 j;
    PopupGlyph *glyph;

    ReadGeomOffset(&x, &y);
    offset.vx = (0xA0 - x) * 2;
    offset.vy = (0x46 - y) * 2;
    offset.vz = ReadGeomScreen();
    D_800C377C = ReadGeomScreen();
    func_80021B24(&angle, &popup->angle);
    scale.vx = popup->scale.vx;
    scale.vy = popup->scale.vy;
    scale.vz = popup->scale.vz;
    for (i = 0; i != 6; i++) {
        func_8003F738(&angle, &m);
        TransMatrix(&m, &offset);
        CompMatrix(&D_800C3760, &m, &m);
        ScaleMatrix(&m, &scale);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (j = 0, glyph = popup->glyphs; j != popup->glyphCount; j++, glyph++) {
            func_800BD810(glyph, popup->colour.word);
        }
        angle.vz -= 5;
        scale.vx -= 0x330;
        scale.vy -= 0x330;
        scale.vz -= 0x330;
    }
}

/* Show value as a number popup, coloured by the popup kind D_800D3630 (2
 * green, 3 magenta, 11 blue, else white), spinning one way at random. */
void func_800BE330(s32 value) {
    NumberPopup *popup;
    u8 text[8];
    s32 i;
    s32 x;

    popup = func_8001D1D8(sizeof(NumberPopup), 0, func_800BE11C, func_800BE1C4, 0);
    popup->spin = -(((rand() & 3) - 2) * 8);
    if (popup->spin == 0) {
        popup->spin = 6;
    }
    popup->life = 32;
    popup->colour.rgbc[3] = 0x2E;
    popup->scale.vx = 0x2000;
    popup->scale.vy = 0x2000;
    popup->scale.vz = 0x2000;
    popup->glyphCount = 0;
    popup->angle.vx = 0;
    popup->angle.vy = 0;
    popup->angle.vz = 0;
    switch (D_800D3630) {
    case 11:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 3:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 2:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0;
        break;
    default:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0x80;
        break;
    }
    func_800BE6E8(value, text, 5, 0, 0);
    x = D_800C3752[text[0]];
    popup->glyphCount = 0;
    for (i = 0; i != text[0]; x += 10) {
        popup->glyphCount += func_80026DCC(D_800D2F5C, text[i + 1] + 0x72, &popup->glyphs[popup->glyphCount], x, -8);
        i++;
    }
    for (i = 0; i != popup->glyphCount; i++) {
        popup->glyphs[i].w--;
        popup->glyphs[i].h--;
    }
}

/* Run up to three commands (kinds 0, 1 and 10) on slot's sprite outside the
 * battle menu and wait frames until they are done. */
void func_800BE538(s32 slot, s32 first, s32 second, s32 third) {
    SlotSprite *sprite;
    s32 mode;
    BattleMenu *menu;

    D_80059464 = 0;
    D_800591AC = 1;
    sprite = BATTLE_FRAME.slotSprites[slot];
    D_800C3780 = 1;
    if (sprite != NULL) {
        mode = sprite->motion.b.mode;
        func_800245D8(sprite, 10);
        menu = D_800C3610;
        D_800C3610 = (BattleMenu *)1;
        if (first) {
            func_800BD3AC(sprite, first, 0);
        }
        if (second) {
            func_800BD3AC(sprite, second, 1);
        }
        if (third) {
            func_800BD3AC(sprite, third, 10);
        }
        D_800C3610 = menu;
        while (func_800BF6F8()) {
            func_800BE790();
        }
        while (sprite->motion.b.mode == 10) {
            func_800BE790();
        }
        func_800245D8(sprite, mode);
    }
    D_800C3780 = 0;
    func_800BE0DC();
    D_80059464 = 0;
    D_800591AC = 0;
}

/* Write value as digits hexadecimal glyphs (D_800C3784, plus base) after
 * the count in text. */
void func_800BE6A0(s32 value, u8 *text, s32 digits, s32 base) {
    s32 i;
    s32 last;

    i = 0;
    if (digits != 0) {
        last = digits - 1;
        do {
            text[i + 1] = D_800C3784[(value >> ((last - i) * 4)) & 0xF] + base;
        } while (++i != digits);
    }
    text[0] = digits;
}

/* Write value in decimal after the count in text: a '-' for a negative
 * value, then its last digits + 1 digits (plus base), leading zeros only
 * when leading is set. */
void func_800BE6E8(s32 value, u8 *text, s32 digits, u8 leading, s32 base) {
    s32 count = 0;
    u8 *out = text + 1;
    u8 digit;

    if (value < 0) {
        count = 1;
        text[1] = '-';
        out = text + 2;
        value = -value;
        digits--;
    }
    while (value %= D_800C37A4[digits], digits != 0) {
        digits--;
        digit = value / D_800C37A4[digits];
        if (digit) {
            leading = 1;
        }
        if (leading) {
            *out++ = digit + base;
            count++;
        }
    }
    *out = value + base;
    text[0] = count + 1;
}

#ifdef NON_MATCHING
/* Run one battle frame: swap the display buffers, read the controllers,
 * update the sprites, the stage and the effects (the skipped frames once
 * more each) with the stack in the scratchpad, draw, time the frame and
 * present it; the outermost frame also runs the battle menu, a pending sound
 * request and the deferred free of the objects' extra files. Nonmatching:
 * the original rematerialises the frame's address at each use instead of
 * keeping it in a saved register. */
void func_800BE790(void) {
    BattleFrame *frame = &BATTLE_FRAME;
    FrameBuffer *buffer;
    s32 skipped;

    D_800C37D0++;
    func_80019CA0();
    D_800D309C.start = VSync(-1);
    buffer = &frame->buffers[0];
    if (frame->current == buffer) {
        buffer = &frame->buffers[1];
    }
    frame->current = buffer;
    frame->ot = buffer->ot;
    ClearOTagR(buffer->ot, 0x1000);
    frame->buffer = 1 - frame->buffer;
    if (D_80010000 != -1) {
        func_800BEBC4();
        __asm__ volatile(".word 0x0001000D"); /* break 1: the debugger breakpoint */
        func_80280A9C();
    }
    func_800250E0(frame->buffer);
    func_800BBAB8();
    func_800BB9D4();
    func_80024FF4(&D_800D309C.view);
    func_80024FE4(frame->ot);
    if (D_80010000 != -1) {
        func_80037324(frame->ot);
    }
    func_800A9A50(&D_800D309C.view, (s32)D_800CCB94, D_8005956C, frame->buffer);
    SPAD_STACK_ENTER();
    func_8001D468();
    func_8001C9F8();
    func_8001C964();
    for (skipped = D_80059494 - 1; skipped != -1; skipped--) {
        func_800BBAB8();
        func_8001C964();
    }
    SPAD_STACK_LEAVE();
    func_80076544();
    func_8008A9C0(0);
    while (--D_80059494 != -1) {
        func_8008A9C0(1);
    }
    D_800D309C.drawn = VSync(1);
    DrawSync(0);
    D_800D309C.synced = VSync(1);
    D_80059494 = VSync(-1) - D_800D309C.start - D_80059198;
    if (D_80059494 < 0) {
        D_80059494 = 0;
    }
    if (D_80059494 >= 5) {
        D_80059494 = 4;
    }
    frame->frameTicks = D_80059494 + D_80059198;
    VSync(D_80059198 != 0 ? D_80059198 + 1 : 0);
    PutDispEnv(frame->current->dispEnv);
    PutDrawEnv(frame->current->drawEnv);
    func_80025044();
    DrawOTag(&frame->current->ot[0xFFF]);
    func_800BEB04();
    if (D_800C37D0 == 1) {
        if (D_800C3610 != NULL) {
            D_800C3610->update(D_800C3610);
        }
        if (D_800591B4 != 0) {
            u16 sound = D_800591B4;

            D_800591B4 = 0;
            func_800B8068(sound);
        }
        if (D_800C37CC != 0 && func_800286CC() == 0) {
            D_800C37CC = 0;
            func_800B136C();
        }
    }
    D_800C37D0--;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE790);
#endif

/* Load the requested battle module (D_800591B3) into 0x801FC000 when it
 * changed, around the module switch 800B8354, and mark it loaded. */
void func_800BEB04(void) {
    s32 saved0;
    s32 saved1;
    u8 module;

    if (D_800591B2 != (module = D_800591B3)) {
        D_800591B2 = D_800591B3;
        func_800B8354();
        func_800284B4(&saved0, &saved1);
        func_80028470(0xC, 2);
        func_800295D8(module + 2, 0x801FC000, 0, 0x80);
        func_800B8354();
        func_80028470(saved0, saved1);
        DrawSync(0);
        VSync(0);
        EnterCriticalSection();
        FlushCache();
        ExitCriticalSection();
    }
    D_800591B0 = 1;
}

/* Read the controllers; holding select (0x100) slows the frame down. */
void func_800BEBC4(void) {
    func_800BEC18();
    if (BATTLE_FRAME.held & 0x100) {
        VSync(8);
        D_80059494 = 0;
    }
}

/* Read both controllers: held, newly pressed and released buttons, and a
 * history of the first controller's last changes. */
void func_800BEC18(void) {
    s32 held;
    u16 old;

    held = func_8003569C(0) & 0xFFFF;
    old = BATTLE_FRAME.held;
    BATTLE_FRAME.held = held;
    BATTLE_FRAME.pressed = ~old & held;
    BATTLE_FRAME.released = old & ~held;
    held = func_8003569C(1);
    BATTLE_FRAME.pressed2 = ~BATTLE_FRAME.held2 & held;
    BATTLE_FRAME.held2 = held;
    BATTLE_FRAME.heldOnly = BATTLE_FRAME.held & ~held;
    if (BATTLE_FRAME.held != BATTLE_FRAME.history[0].held) {
        BATTLE_FRAME.history[3] = BATTLE_FRAME.history[2];
        BATTLE_FRAME.history[2] = BATTLE_FRAME.history[1];
        BATTLE_FRAME.history[1] = BATTLE_FRAME.history[0];
        BATTLE_FRAME.history[0].held = BATTLE_FRAME.held;
        BATTLE_FRAME.history[0].pressed = BATTLE_FRAME.pressed;
        BATTLE_FRAME.history[0].released = BATTLE_FRAME.released;
        BATTLE_FRAME.history[0].time = D_800D30E4;
    }
}

/* Clear the battle menu state. */
void func_800BED30(void) {
    D_800C3E20 = 0;
    D_800C3610 = NULL;
    D_800D2E54 = 0;
}

/* Open the battle menu (800B9F78 its update). */
BattleMenu *func_800BED4C(void) {
    BattleMenu *menu = func_80031BDC(sizeof(BattleMenu), 0);

    D_800C3610 = menu;
    menu->update = func_800B9F78;
    D_800C360C = 0;
    menu->field4A = 0;
    D_800C3610->field30 = 0;
    D_800C3610->field48 = 1;
    D_800C3610->field2C = 1;
    D_800C3610->sprite = NULL;
    D_800C3610->field49 = 0;
    D_800C3610->field34 = 0;
    func_800BF0B4(0);
    D_80059464 = 0;
    D_800591AC = 1;
    return D_800C3610;
}

/* Close the battle menu. */
void func_800BEDE8(void) {
    func_800320E8(D_800C3610);
    D_80059464 = 0;
    D_800C3610 = NULL;
    D_800591AC = 0;
}

/* Run 800AA320 on a 4 KB stack of its own. */
void func_800BEE2C(s32 index, s32 mask, s32 arg2) {
    u8 *stack = func_80031BDC(0x1000, 1);

    STACK_ENTER(stack + 0xF9C);
    func_800AA320(index, mask, arg2);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* List the slot sprites of the slots in mask (up to 11, NULL-terminated),
 * setting their target; their count. */
s32 func_800BEEB4(u32 mask, SlotSprite **list, SlotSprite *target) {
    s32 i;
    s32 count;
    SlotSprite *sprite;

    i = 0;
    count = i;

    for (; i != 11; i++, mask = (mask & 0xFFFF) >> 1) {
        if (mask & 1) {
            sprite = BATTLE_FRAME.slotSprites[i];
            if (sprite != NULL) {
                sprite->target = target;
                list[count] = sprite;
                count++;
            }
        }
    }
    list[count] = NULL;
    return count;
}


/* The direction from sprite from to sprite to on the ground. */
s16 func_800BEF24(SlotSprite *from, SlotSprite *to) {
    GroundPoint a;
    GroundPoint b;

    a.x = from->x >> 16;
    a.z = from->z >> 16;
    b.x = to->x >> 16;
    b.z = to->z >> 16;
    return func_80023124(b, a);
}

/* The direction from sprite to its target point on the ground. */
s16 func_800BEF8C(SlotSprite *sprite) {
    GroundPoint a;
    GroundPoint b;

    a.x = sprite->x >> 16;
    a.z = sprite->z >> 16;
    b.x = sprite->targetX;
    b.z = sprite->targetZ;
    return func_80023124(b, a);
}

/* Make slot the acting slot, returning the previous acting sprite to idle. */
void func_800BEFF4(s32 slot) {
    SlotSprite *sprite = D_800C3610->sprite;

    if (sprite != NULL && D_800C3610->slot != slot && !BATTLE_FRAME.slots[SPRITE_SLOT(sprite)].hidden) {
        func_800245D8(sprite, sprite->idleMode);
    }
    D_800C3610->slot = slot;
    D_800C3610->sprite = BATTLE_FRAME.slotSprites[slot];
}

/* Set the battle menu's state. */
void func_800BF0B4(s32 state) {
    D_800C3610->state = state;
}

/* Walk sprite to the next point of the path, or at its end, to its target. */
void func_800BF0C4(SlotSprite *sprite) {
    if (BATTLE_FRAME.path[D_800C3610->field2C].x == 0xFFFF && BATTLE_FRAME.path[D_800C3610->field2C].z == 0xFFFF) {
        sprite->targetY = 0;
        sprite->targetX = sprite->x >> 16;
        sprite->targetZ = sprite->z >> 16;
        func_800BF4F0(sprite, sprite->target);
        return;
    }
    sprite->targetX = BATTLE_FRAME.path[D_800C3610->field2C].x;
    sprite->targetZ = BATTLE_FRAME.path[D_800C3610->field2C].z;
    sprite->targetY = 0;
    func_800BF1EC(sprite, BATTLE_FRAME.path[D_800C3610->field2C].run ? 3 : 2);
    D_800C3610->field2C++;
}

/* Start sprite moving to its target point with motion mode. */
void func_800BF1EC(SlotSprite *sprite, s32 mode) {
    GroundPoint from;
    GroundPoint to;

    from.x = sprite->x >> 16;
    from.z = sprite->z >> 16;
    to.x = sprite->targetX;
    to.z = sprite->targetZ;
    D_800C3610->field44 = func_800C07CC(from, to);
    func_80021FE0((s32 *)sprite, func_800BEF8C(sprite));
    func_800223B0((s32 *)sprite, func_800BEF8C(sprite));
    func_800245D8(sprite, mode);
    func_800BF0B4(6);
}

/* Load the file of sprite's resource for its slot's command. */
void func_800BF2B8(SlotSprite *sprite) {
    s32 file;
    void *block;

    func_800B8D7C();
    func_80028470(0x2C, 1);
    file = *sprite->resource;
    block = func_80031BDC(func_800288EC(file), 1);
    func_800295D8(file, (s32)block, 0, 0x80);
    D_800C3618 = block;
    D_800C361C = SPRITE_SLOT(sprite);
}

/* Start the loaded command file once. */
s32 func_800BF354(void) {
    s32 result;

    if (D_800D3350 == 0) {
        result = func_800C0FAC(D_800C3618);
        D_800D3350 = 1;
    }
    return result;
}

/* Stop the started command file. */
void func_800BF3A4(void) {
    if (D_800D3350 != 0) {
        func_800C1140(D_800C3618);
        D_800D3350 = 0;
    }
}

/* Face sprite and the first target of the current event at each other. */
void func_800BF3E8(SlotSprite *sprite) {
    SlotSprite *first;
    s32 slot;
    SlotSprite *target;

    D_800D3634 = BATTLE_FRAME.events[D_800C360C].targetMask;
    if ((D_800D3678 = func_800BEEB4(BATTLE_FRAME.events[D_800C360C].targetMask, D_800D363C, sprite)) == 0) {
        D_800D363C[0] = sprite;
    }
    target = D_800D363C[0];
    sprite->target = target;
    target->target = sprite;
    first = D_800D363C[0];
    slot = SPRITE_SLOT(target);
    D_800C3610->target = first;
    D_800C3610->targetSlot = slot;
    func_800223B0((s32 *)sprite, func_800BEF24(sprite, sprite->target));
    if (target->motion.b.mode != 0x15) {
        func_800223B0((s32 *)target, func_800BEF24(sprite->target, sprite));
    }
}

/* At the path's end, step sprite beside target; else walk the path on. */
void func_800BF4F0(SlotSprite *sprite, SlotSprite *target) {
    s16 x;

    if (BATTLE_FRAME.path[D_800C3610->field2C].x == 0xFFFF && BATTLE_FRAME.path[D_800C3610->field2C].z == 0xFFFF) {
        sprite->x = sprite->targetX << 16;
        sprite->z = sprite->targetZ << 16;
        x = target->x >> 16;
        sprite->targetX = (s16)(sprite->x >> 16) >= x ? x + 0x50 : x - 0x50;
        sprite->targetZ = target->z >> 16;
        sprite->targetY = 0;
        if (sprite->targetX == (s16)(sprite->x >> 16) && sprite->targetZ == (s16)(sprite->z >> 16)) {
            func_800B9C00(sprite);
            return;
        }
        func_800BF1EC(sprite, 3);
        func_800BF0B4(2);
        return;
    }
    func_800BF0C4(sprite);
}

/* Count a finished sprite motion. */
void func_800BF5E8(void) {
    D_800C3CE8++;
}

/* Run command with sprite playing its motion, then wait for the motion's end. */
void func_800BF600(s32 command, SlotSprite *sprite) {
    if (sprite->field48 == 0) {
        func_800B7C34(command);
        return;
    }
    D_800C3CE8 = 0;
    if (sprite->motion.b.mode != 0) {
        func_80021BF8(sprite, func_800BF5E8);
        func_800245D8(sprite, sprite->motion.b.mode);
    }
    func_800B7C34(command);
    if (sprite->motion.b.mode != 0) {
        while (D_800C3CE8 == 0) {
            func_800BE790();
        }
        func_80021BF8(sprite, NULL);
    }
}

/* Update the battle menu when it is open. */
void func_800BF6CC(void) {
    if (D_800C3610 != NULL) {
        func_800BD2E4();
    }
}

/* D_80059464 less one while a number popup shows. */
s32 func_800BF6F8(void) {
    return D_80059464 - func_800BF720();
}

/* Whether a number popup shows. */
s32 func_800BF720(void) {
    return D_800D2D68 != 0;
}

/* Set D_800C3628. */
void func_800BF730(s32 value) {
    D_800C3628 = value;
}

/* Watch a sprite's value; on a rise or a fall under the threshold call back
 * and end. */
void func_800BF73C(EffectSprite *task) {
    SlotWatch *watch = (SlotWatch *)task;
    s32 last = watch->value;

    watch->value = func_800B57E4(watch->sprite);
    if (last < watch->value || watch->value < watch->threshold) {
        watch->callback(watch->sprite);
        watch->destroy(watch);
    }
}

/* Start watching sprite's value against threshold with callback. */
void func_800BF7C8(SlotSprite *sprite, s32 threshold, void (*callback)(SlotSprite *sprite)) {
    SlotWatch *watch = func_8001CD08(sprite->task, sizeof(SlotWatch) - 0x1C);

    func_8001CD6C((EffectSprite *)watch, func_800BF73C);
    watch->callback = callback;
    watch->sprite = sprite;
    watch->mode = sprite->motion.b.mode;
    watch->value = func_800B57E4(sprite);
    watch->threshold = threshold;
    sprite->motion.word |= 0x20;
}

#ifdef NON_MATCHING
/* Make slot's sprite act on the sprite of slot target alone. Nonmatching:
 * the original keeps the store of D_800D3634 before the sprite's, and
 * allocates the registers otherwise. */
void func_800BF85C(s32 slot, s32 target) {
    SlotSprite *sprite = BATTLE_FRAME.slotSprites[slot];

    if (sprite != NULL) {
        D_800C3E1C = sprite;
        D_800D3634 = 1 << target;
        sprite->target = BATTLE_FRAME.slotSprites[target];
        D_800D363C[1] = NULL;
        D_800D363C[0] = BATTLE_FRAME.slotSprites[target];
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BF85C);
#endif

/* Move sprite's target to the next of the event's targets. */
void func_800BF8CC(SlotSprite *sprite) {
    s32 i;

    for (i = 0; i != D_800D3678; i++) {
        if (D_800D363C[i] == sprite->target) {
            break;
        }
    }
    if (i >= D_800D3678) {
        sprite->target = D_800D363C[0];
    } else {
        sprite->target = D_800D363C[i + 1];
    }
}

/* The index of sprite among the event's targets. */
s32 func_800BF954(SlotSprite *sprite) {
    s32 i;

    for (i = 0; i != D_800D3678; i++) {
        if (D_800D363C[i] == sprite) {
            break;
        }
    }
    return i;
}

/* Count an effect hit; at the second, signal event 11. */
void func_800BF998(void) {
    D_800D2D4C++;
    D_800D36BC++;
    if (D_800D2D4C == 2) {
        func_800A9FF0(0xB);
    }
}

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
