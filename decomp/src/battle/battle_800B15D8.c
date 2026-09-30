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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B15D8", func_800BE538);

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
