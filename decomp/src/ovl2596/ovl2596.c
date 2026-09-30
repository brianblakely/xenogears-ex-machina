/* Post-battle module (Disc 1 slot 2596, directory 10 file 4), loaded at
 * 801de000 by the battle overlay's 80070f40 (the read at 800715d8). It
 * builds and queues the victory result screens (member cards, summary and
 * spoils windows), distributes experience and levels, grants skills, gold and
 * drops, and writes the party back to the game data. The battle overlay calls
 * 801de594 (queue the screens' primitives) and 801e252c.
 *
 * Compiled with GCC 2.6.3: only it reproduces the (index + base) operand
 * order of this unit's address arithmetic (e.g. 801de5c4, 801de1c4). */
#include "battle_results.h"

/* Queue every member card's glyph runs while the cards are shown. */
void func_801DE048(void) {
    s32 i;

    if (D_800D2D28->showCards != 0) {
        for (i = 0; i < 3; i++) {
            func_800728B8(D_800D32F8[i]->portrait, D_800D32F8[i]->runs[0].count, D_800D32F8[i]->runs[0].buffer);
            func_800728B8(D_800D32F8[i]->labels, D_800D32F8[i]->runs[1].count, D_800D32F8[i]->runs[1].buffer);
            func_800728B8(D_800D32F8[i]->field960, D_800D32F8[i]->runs[4].count, D_800D32F8[i]->runs[4].buffer);
            func_800728B8(D_800D32F8[i]->fieldB40, D_800D32F8[i]->runs[6].count, D_800D32F8[i]->runs[6].buffer);
            func_800728B8(D_800D32F8[i]->fieldA50, D_800D32F8[i]->runs[5].count, D_800D32F8[i]->runs[5].buffer);
            func_800728B8(D_800D32F8[i]->fieldBE0, D_800D32F8[i]->runs[7].count, D_800D32F8[i]->runs[7].buffer);
            func_800728B8(D_800D32F8[i]->field780, D_800D32F8[i]->runs[2].count, D_800D32F8[i]->runs[2].buffer);
            func_800728B8(D_800D32F8[i]->field870, D_800D32F8[i]->runs[3].count, D_800D32F8[i]->runs[3].buffer);
            func_800728B8(D_800D32F8[i]->fieldC80, D_800D32F8[i]->runs[8].count, D_800D32F8[i]->runs[8].buffer);
            func_800728B8(D_800D32F8[i]->fieldF00, D_800D32F8[i]->runs[9].count, D_800D32F8[i]->runs[9].buffer);
            func_800728B8(D_800D32F8[i]->field1180, D_800D32F8[i]->runs[10].count, D_800D32F8[i]->runs[10].buffer);
            func_800728B8(D_800D32F8[i]->field13B0, D_800D32F8[i]->runs[11].count, D_800D32F8[i]->runs[11].buffer);
        }
    }
}

/* Queue the summary window's glyphs and bars, and the 8F panel. */
void func_801DE1C4(void) {
    s32 i;

    if (D_800D2D28->showSummary != 0) {
        func_800728B8(D_800D334C->title[0], D_800D334C->runs[0].count, D_800D334C->runs[0].buffer);
        func_800728B8(D_800D334C->text, D_800D334C->runs[1].count, D_800D334C->runs[1].buffer);
        func_800728B8(D_800D334C->glyphs1630, D_800D334C->runs[2].count, D_800D334C->runs[2].buffer);
        func_800728B8(D_800D334C->glyphs1720, D_800D334C->runs[4].count, D_800D334C->runs[4].buffer);
        func_800728B8(D_800D334C->glyphs17C0[0], D_800D334C->runs[3].count, D_800D334C->runs[3].buffer);
        func_800728B8(D_800D334C->glyphs1900[0], D_800D334C->runs[5].count, D_800D334C->runs[5].buffer);
        for (i = 0; i < 7; i++) {
            func_80043B48(D_800CCB00.ot + 1, &D_800D334C->barB[i][D_800D334C->barBuffer[i]]);
            func_80043B48(D_800CCB00.ot + 1, &D_800D334C->barA[i][D_800D334C->barBuffer[i]]);
            func_800728B8(D_800D334C->rowA[i][0], D_800D334C->rowACount[i], D_800D334C->rowABuffer[i]);
            func_800728B8(D_800D334C->rowB[i][0], D_800D334C->rowBCount[i], D_800D334C->rowBBuffer[i]);
        }
    }
    i = 0;
    if (D_800D2D28->show8F != 0) {
        func_800728B8(D_800D334C->title[0], D_800D334C->runs[0].count, D_800D334C->runs[0].buffer);
        for (; i < 2; i++) {
            func_80043B48(D_800CCB00.ot + 1, &D_800D334C->glyphs34B0[i][D_800D334C->buffer34B0[i]]);
        }
    }
}

/* Queue the spoils window's glyphs and item list. */
void func_801DE408(void) {
    s32 i;

    if (D_800D2D28->showSpoils != 0) {
        func_800728B8(D_800D334C->glyphs2D30[0], 7, D_800D334C->buffer2D30);
        func_800728B8(D_800D334C->glyphs2F60[0], D_800D334C->run2F60.count, D_800D334C->run2F60.buffer);
        func_800728B8(D_800D334C->glyphs3140[0], D_800D334C->run3140.count, D_800D334C->run3140.buffer);
        for (i = 0; i < D_800D334C->listCount; i++) {
            func_80043B48(D_800CCB00.ot + 1, &D_800D334C->listA[i][D_800D334C->listBuffer]);
            func_80043B48(D_800CCB00.ot + 1, &D_800D334C->listB[i][D_800D334C->listBuffer]);
        }
        for (i = 0; i < 2; i++) {
            func_80043B48(D_800CCB00.ot + 1, &D_800D334C->glyphs3410[i][D_800D334C->buffer3410]);
        }
    }
}

/* Queue the result screens' primitives. */
void func_801DE594(void) {
    func_801DE048();
    func_801DE1C4();
    func_801DE408();
}

/* Build each present member's portrait glyphs. */
void func_801DE5C4(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[0].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            D_800D32F8[i]->runs[0].count += func_80076A10(i + 0xFC, &D_800D32F8[i]->portrait[D_800D32F8[i]->runs[0].count * 2], 0x20, i * 0x20 + 0x24);
        }
        D_800D32F8[i]->runs[0].buffer = D_800CCB00.buffer;
    }
}

/* Build each present member's card labels; with the first card's flag, add
 * the two marker glyphs, shaded red and green. */
void func_801DE69C(void) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[1].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            for (j = 0; j < 18; j++) {
                D_800D32F8[i]->runs[1].count += func_80076A10(D_800C3268[j], &D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2], D_800C327C[j], D_800C32A0[j] + i * 0x20);
            }
            if (D_800D32F8[0]->flag15F8 != 0) {
                D_800D32F8[i]->runs[1].count += func_80076A10(0xE8, &D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2], 0x88, i * 0x20 + 0x20);
                D_800D32F8[i]->runs[1].count += func_80076A10(0xE9, &D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2], 0x88, i * 0x20 + 0x28);
                func_80043C24(&D_800D32F8[i]->labels[(D_800D32F8[i]->runs[1].count - 2) * 2 + D_800CCB00.buffer], 0);
                setRGB0(&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 4, 0x80, 0x40, 0x40);
                func_80043C24(&D_800D32F8[i]->labels[(D_800D32F8[i]->runs[1].count - 1) * 2 + D_800CCB00.buffer], 0);
                setRGB0(&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 2, 0x40, 0x80, 0x40);
            }
        }
        D_800D32F8[i]->runs[1].buffer = D_800CCB00.buffer;
    }
}

/* Build each present member's four numbers as digit glyphs: two stats (three
 * and two digits) and two further values beside them. */
void func_801DEA18(void) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[4].count = 0;
        D_800D32F8[i]->runs[6].count = 0;
        D_800D32F8[i]->runs[5].count = 0;
        D_800D32F8[i]->runs[7].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            func_8008AAA0(D_800CCD34[i].value4C);
            for (j = 0; j < 3; j++) {
                n = j + 9;
                digit = D_800C3CF1[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[4].count += func_80076A10(digit, &D_800D32F8[i]->field960[D_800D32F8[i]->runs[4].count * 2], j * 8 + 0x48, i * 0x20 + 0x20);
                }
            }
            D_800D32F8[i]->runs[4].buffer = D_800CCB00.buffer;
            func_8008AAA0(D_800CCD34[i].value50);
            for (j = 0; j < 2; j++) {
                n = j + 10;
                digit = D_800C3CF1[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[6].count += func_80076A10(digit, &D_800D32F8[i]->fieldB40[D_800D32F8[i]->runs[6].count * 2], j * 8 + 0x50, i * 0x20 + 0x28);
                }
            }
            D_800D32F8[i]->runs[6].buffer = D_800CCB00.buffer;
            func_8008AAA0(D_800CDCE8[i].valueA);
            for (j = 0; j < 3; j++) {
                n = j + 13;
                digit = D_800C3CED[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[5].count += func_80076A10(digit, &D_800D32F8[i]->fieldA50[D_800D32F8[i]->runs[5].count * 2], j * 8 + 0x68, i * 0x20 + 0x20);
                }
            }
            D_800D32F8[i]->runs[5].buffer = D_800CCB00.buffer;
            func_8008AAA0(D_800CDCE8[i].valueB);
            for (j = 0; j < 2; j++) {
                n = j + 14;
                digit = D_800C3CED[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[7].count += func_80076A10(digit, &D_800D32F8[i]->fieldBE0[D_800D32F8[i]->runs[7].count * 2], j * 8 + 0x70, i * 0x20 + 0x28);
                }
            }
            D_800D32F8[i]->runs[7].buffer = D_800CCB00.buffer;
        }
    }
}

/* Build each present member's level glyphs, from the battle slots or (with
 * fromGameData) the game data; with the first card's flag also the second
 * level, and shade the two numbers red and green. */
void func_801DEDC0(u8 fromGameData) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[2].count = 0;
        D_800D32F8[i]->runs[3].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            if (fromGameData == 0) {
                func_8008AAA0(D_800D32A5[i].level);
            } else {
                func_8008AAA0(D_8006D902[D_800D2D24[i]].level);
            }
            for (j = 0; j < 3; j++) {
                n = j + 18;
                digit = D_800C3CE8[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[2].count += func_80076A10(digit, &D_800D32F8[i]->field780[D_800D32F8[i]->runs[2].count * 2], j * 8 + 0x90, i * 0x20 + 0x20);
                }
            }
            D_800D32F8[i]->runs[2].buffer = D_800CCB34;
            if (D_800D32F8[0]->flag15F8 != 0) {
                if (fromGameData == 0) {
                    func_8008AAA0(D_800D32A5[i].level2);
                } else {
                    func_8008AAA0(D_8006D902[D_800D2D24[i]].level2);
                }
                for (j = 0; j < 3; j++) {
                    n = j + 18;
                    digit = D_800C3CE8[n];
                    if (digit != 0xFF) {
                        D_800D32F8[i]->runs[3].count += func_80076A10(digit, &D_800D32F8[i]->field870[D_800D32F8[i]->runs[3].count * 2], j * 8 + 0x90, i * 0x20 + 0x28);
                    }
                }
                D_800D32F8[i]->runs[3].buffer = D_800CCB34;
                for (j = 0; j < D_800D32F8[i]->runs[2].count; j++) {
                    func_80043C24(&D_800D32F8[i]->field780[j * 2 + D_800CCB34], 0);
                    setRGB0(&D_800D32F8[i]->field780[j * 2 + D_800CCB34], 0x80, 0x40, 0x40);
                }
                for (j = 0; j < D_800D32F8[i]->runs[3].count; j++) {
                    func_80043C24(&D_800D32F8[i]->field870[j * 2 + D_800CCB34], 0);
                    setRGB0(&D_800D32F8[i]->field870[j * 2 + D_800CCB34], 0x40, 0x80, 0x40);
                }
            }
        }
    }
}

/* Build each present member's first eight-digit number (and with the first
 * card's flag the second) as glyphs. */
void func_801DF270(void) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[8].count = 0;
        D_800D32F8[i]->runs[9].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            func_8008AAA0(D_800CDCB8[i].value);
            for (j = 0; j < 8; j++) {
                n = j + 22;
                digit = D_800C3CDF[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[8].count += func_80076A10(digit, &D_800D32F8[i]->fieldC80[D_800D32F8[i]->runs[8].count * 2], j * 8 + 0xB0, i * 0x20 + 0x20);
                }
            }
            D_800D32F8[i]->runs[8].buffer = D_800CCB34;
            if (D_800D32F8[0]->flag15F8 != 0) {
                func_8008AAA0(D_800CDCB8[i].value2);
                for (j = 0; j < 8; j++) {
                    n = j + 22;
                    digit = D_800C3CDF[n];
                    if (digit != 0xFF) {
                        D_800D32F8[i]->runs[9].count += func_80076A10(digit, &D_800D32F8[i]->fieldF00[D_800D32F8[i]->runs[9].count * 2], j * 8 + 0xB0, i * 0x20 + 0x28);
                    }
                }
                D_800D32F8[i]->runs[9].buffer = D_800CCB34;
            }
        }
    }
}

/* Build each present member's seven-digit numbers as glyphs, like 801df270. */
void func_801DF4C0(void) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[10].count = 0;
        D_800D32F8[i]->runs[11].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            func_8008AAA0(D_800CDCD0[i].value);
            for (j = 0; j < 7; j++) {
                n = j + 31;
                digit = D_800C3CD7[n];
                if (digit != 0xFF) {
                    D_800D32F8[i]->runs[10].count += func_80076A10(digit, &D_800D32F8[i]->field1180[D_800D32F8[i]->runs[10].count * 2], j * 8 + 0xF8, i * 0x20 + 0x20);
                }
            }
            D_800D32F8[i]->runs[10].buffer = D_800CCB34;
            if (D_800D32F8[0]->flag15F8 != 0) {
                func_8008AAA0(D_800CDCD0[i].value2);
                for (j = 0; j < 7; j++) {
                    n = j + 31;
                    digit = D_800C3CD7[n];
                    if (digit != 0xFF) {
                        D_800D32F8[i]->runs[11].count += func_80076A10(digit, &D_800D32F8[i]->field13B0[D_800D32F8[i]->runs[11].count * 2], j * 8 + 0xF8, i * 0x20 + 0x28);
                    }
                }
                D_800D32F8[i]->runs[11].buffer = D_800CCB34;
            }
        }
    }
}

/* Set up a gauge bar's two primitives: a gradient from colour (0 pink,
 * 1 light green, 2 red, 3 blue) at the top to black. */
void func_801DF710(POLY_G4 *bar, u8 colour) {
    u8 rgb[3];
    s32 i;

    switch (colour) {
    case 0:
        rgb[0] = 0xFF;
        rgb[1] = 0x80;
        rgb[2] = 0x80;
        break;
    case 1:
        rgb[0] = 0x80;
        rgb[1] = 0xFF;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0xFF;
        rgb[1] = 0;
        rgb[2] = 0;
        break;
    case 3:
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0xFF;
        break;
    }
    for (i = 0; i < 2; i++) {
        func_80043CC4(&bar[i]);
        bar[i].r0 = rgb[0];
        bar[i].g0 = rgb[1];
        bar[i].b0 = rgb[2];
        bar[i].r1 = rgb[0];
        bar[i].g1 = rgb[1];
        bar[i].b1 = rgb[2];
        bar[i].r2 = 0;
        bar[i].g2 = 0;
        bar[i].b2 = 0;
        bar[i].r3 = 0;
        bar[i].g3 = 0;
        bar[i].b3 = 0;
    }
}

/* Shade count glyph parts from the given draw buffer red (or blue). */
void func_801DF840(POLY_FT4 *prims, u8 blue, u8 count, u8 buffer) {
    u8 rgb[3];
    s32 i;

    rgb[1] = 0x40;
    if (blue == 0) {
        rgb[0] = 0x80;
        rgb[2] = 0x40;
    } else {
        rgb[0] = 0x40;
        rgb[2] = 0x80;
    }
    for (i = 0; i < count; i++) {
        func_80043C24(&prims[i * 2 + buffer], 0);
        setRGB0(&prims[i * 2 + buffer], rgb[0], rgb[1], rgb[2]);
    }
}

/* Start the level gauge animation from one value to another out of max:
 * lengths on a 64-pixel scale, the colour and arrow for up or down. */
void func_801DF910(u8 from, u8 to, s32 max) {
    D_801E44CC = from;
    D_801E44D0 = to;
    D_801E44D4 = to - from;
    D_801E44D8 = from * 100 / max * 0x1900 / 10000;
    if (D_801E44D4 >= 0) {
        D_801E44E0 = 2;
        D_801E44E4 = 0xE3;
    } else {
        D_801E44E0 = 3;
        D_801E44E4 = 0xE5;
        D_801E44D4 = from - to;
    }
    D_801E44DC = D_801E44D4 * 100 / max * 0x1900 / 10000;
}

/* The highest of a slot's seven entries in both byte tables. */
u8 func_801DFA38(u8 slot) {
    u8 best = 0;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800CDD10[slot][i] >= best) {
            best = D_800CDD10[slot][i];
        }
        if (D_800CDD10[slot + 3][i] >= best) {
            best = D_800CDD10[slot + 3][i];
        }
    }
    return best;
}

/* Build the summary window: the member's portrait title and the 27 text
 * glyphs, shading the marked ones with their colour. */
#ifdef NON_MATCHING
void func_801DFAA8(u8 member) {
    s16 colours[2][3];
    s32 i;
    s32 start;
    s32 k;

    colours[0][0] = 0x80;
    colours[0][1] = 0x40;
    colours[0][2] = 0x40;
    colours[1][0] = 0x40;
    colours[1][1] = 0x40;
    colours[1][2] = 0x40;
    D_800D334C->runs[0].count = func_80076A10(member + 0xFC, D_800D334C->title[0], 0x3E, 0xA4);
    D_800D334C->runs[0].buffer = D_800CCB34;
    D_800D334C->runs[1].count = 0;
    for (i = 0; i < 27; i++) {
        start = D_800D334C->runs[1].count;
        if (D_800C32C4[i].glyph != 0xFF) {
            D_800D334C->runs[1].count += func_80076A10(D_800C32C4[i].glyph, &D_800D334C->text[start * 2], D_800C3318[i], D_800C3350[i]);
            if (D_800C32C4[i].shaded != 0) {
                for (k = start; k < D_800D334C->runs[1].count; k++) {
                    func_80043C24(&D_800D334C->text[k * 2 + D_800CCB34], 0);
                    setRGB0(&D_800D334C->text[k * 2 + D_800CCB34], colours[D_800C32C4[i].colour][0], colours[D_800C32C4[i].colour][1], colours[D_800C32C4[i].colour][2]);
                }
            }
        }
    }
    D_800D334C->runs[1].buffer = D_800CCB34;
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFAA8);
#endif

/* Build the summary's first member value (three digits); clear the other
 * summary number runs. */
void func_801DFD58(u8 member) {
    s32 j;
    s32 n;
    s32 digit;

    D_800D334C->runs[2].count = 0;
    D_800D334C->runs[4].count = 0;
    D_800D334C->runs[3].count = 0;
    D_800D334C->runs[5].count = 0;
    func_8008AAA0(D_800CDCE8[member].valueA);
    for (j = 0; j < 3; j++) {
        n = j + 23;
        digit = D_800C3CE3[n];
        if (digit != 0xFF) {
            D_800D334C->runs[2].count += func_80076A10(digit, &D_800D334C->glyphs1630[D_800D334C->runs[2].count * 2], j * 8 + 0xB8, 0x80);
        }
    }
    D_800D334C->runs[2].buffer = D_800CCB34;
}

/* Build the summary's second member value (two digits). */
void func_801DFE6C(u8 member) {
    s32 j;
    s32 n;
    s32 digit;

    func_8008AAA0(D_800CDCE8[member].valueB);
    for (j = 0; j < 2; j++) {
        n = j + 24;
        digit = D_800C3CE3[n];
        if (digit != 0xFF) {
            D_800D334C->runs[4].count += func_80076A10(digit, &D_800D334C->glyphs1720[D_800D334C->runs[4].count * 2], j * 8 + 0xC0, 0x88);
        }
    }
    D_800D334C->runs[4].buffer = D_800CCB34;
}

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFF50);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E0184);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E03B8);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E03FC);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E09C0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E09F4);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E0A4C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E0ACC);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1044);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E10F8);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E126C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1370);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1444);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1590);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1690);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E196C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1AA4);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1C10);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1E10);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1FB8);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E211C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E2280);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E24B0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E252C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E2794);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E2888);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E2ACC);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E2EB0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E308C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E335C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3500);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3610);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3700);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E38CC);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3A18);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3BE0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3D54);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3E14);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3EA4);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3F28);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3FB0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E403C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E41B4);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E42C4);
