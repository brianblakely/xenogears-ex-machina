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
        func_800728B8(D_800D334C->glyphs17C0, D_800D334C->runs[3].count, D_800D334C->runs[3].buffer);
        func_800728B8(D_800D334C->glyphs1900, D_800D334C->runs[5].count, D_800D334C->runs[5].buffer);
        for (i = 0; i < 7; i++) {
            AddPrim(D_800CCB00.ot + 1, &D_800D334C->barB[i][D_800D334C->barBuffer[i]]);
            AddPrim(D_800CCB00.ot + 1, &D_800D334C->barA[i][D_800D334C->barBuffer[i]]);
            func_800728B8(D_800D334C->rowA[i], D_800D334C->rowACount[i], D_800D334C->rowABuffer[i]);
            func_800728B8(D_800D334C->rowB[i], D_800D334C->rowBCount[i], D_800D334C->rowBBuffer[i]);
        }
    }
    i = 0;
    if (D_800D2D28->show8F != 0) {
        func_800728B8(D_800D334C->title[0], D_800D334C->runs[0].count, D_800D334C->runs[0].buffer);
        for (; i < 2; i++) {
            AddPrim(D_800CCB00.ot + 1, &D_800D334C->glyphs34B0[i][D_800D334C->buffer34B0[i]]);
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
            AddPrim(D_800CCB00.ot + 1, &D_800D334C->listA[i][D_800D334C->listBuffer]);
            AddPrim(D_800CCB00.ot + 1, &D_800D334C->listB[i][D_800D334C->listBuffer]);
        }
        for (i = 0; i < 2; i++) {
            AddPrim(D_800CCB00.ot + 1, &D_800D334C->glyphs3410[i][D_800D334C->buffer3410]);
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
                SetShadeTex(&D_800D32F8[i]->labels[(D_800D32F8[i]->runs[1].count - 2) * 2 + D_800CCB00.buffer], 0);
                setRGB0(&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 4, 0x80, 0x40, 0x40);
                SetShadeTex(&D_800D32F8[i]->labels[(D_800D32F8[i]->runs[1].count - 1) * 2 + D_800CCB00.buffer], 0);
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
                func_8008AAA0(D_8006D8A0[D_800D2D24[i]].level);
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
                    func_8008AAA0(D_8006D8A0[D_800D2D24[i]].level2);
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
                    SetShadeTex(&D_800D32F8[i]->field780[j * 2 + D_800CCB34], 0);
                    setRGB0(&D_800D32F8[i]->field780[j * 2 + D_800CCB34], 0x80, 0x40, 0x40);
                }
                for (j = 0; j < D_800D32F8[i]->runs[3].count; j++) {
                    SetShadeTex(&D_800D32F8[i]->field870[j * 2 + D_800CCB34], 0);
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
        SetPolyG4(&bar[i]);
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
        SetShadeTex(&prims[i * 2 + buffer], 0);
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
                    SetShadeTex(&D_800D334C->text[k * 2 + D_800CCB34], 0);
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

/* Build the summary's change of the member's first stat since the battle
 * began: an up or down arrow and the difference, shaded red. */
#ifdef NON_MATCHING
void func_801DFF50(u8 member) {
    s32 before;
    s32 after;
    s32 difference;
    s32 arrow;
    s32 j;
    s16 x;
    s32 digit;
    s32 k;

    before = D_800CDCE8[member].valueA;
    after = D_8006D8A0[D_800D2D24[member]].maxHp;
    difference = after - before;
    arrow = 0xE3;
    if (difference < 0) {
        arrow = 0xE5;
        difference = before - after;
    }
    if (difference != 0) {
        D_800D334C->runs[3].count = func_80076A10(arrow, D_800D334C->glyphs17C0, 0xD8, 0x80);
        func_8008AAA0(difference);
        x = 0xE0;
        for (j = 0; j < 3; j++) {
            digit = D_800C3CFA[j];
            if (digit != 0xFF) {
                D_800D334C->runs[3].count += func_80076A10(digit, &D_800D334C->glyphs17C0[D_800D334C->runs[3].count * 2], x, 0x80);
                x += 8;
            }
        }
        for (k = 0; k < D_800D334C->runs[3].count; k++) {
            SetShadeTex(&D_800D334C->glyphs17C0[k * 2 + D_800CCB34], 0);
            setRGB0(&D_800D334C->glyphs17C0[k * 2 + D_800CCB34], 0x80, 0x40, 0x40);
        }
        D_800D334C->runs[3].buffer = D_800CCB34;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFF50);
#endif

/* Build the summary's change of the member's second stat, like 801dff50. */
#ifdef NON_MATCHING
void func_801E0184(u8 member) {
    s32 before;
    s32 after;
    s32 difference;
    s32 arrow;
    s32 j;
    s16 x;
    s32 digit;
    s32 k;

    before = D_800CDCE8[member].valueB;
    after = D_8006D8A0[D_800D2D24[member]].maxEp;
    difference = after - before;
    arrow = 0xE3;
    if (difference < 0) {
        arrow = 0xE5;
        difference = before - after;
    }
    if (difference != 0) {
        D_800D334C->runs[5].count = func_80076A10(arrow, D_800D334C->glyphs1900, 0xD8, 0x88);
        func_8008AAA0(difference);
        x = 0xE0;
        for (j = 0; j < 2; j++) {
            digit = D_800C3CFB[j];
            if (digit != 0xFF) {
                D_800D334C->runs[5].count += func_80076A10(digit, &D_800D334C->glyphs1900[D_800D334C->runs[5].count * 2], x, 0x88);
                x += 8;
            }
        }
        for (k = 0; k < D_800D334C->runs[5].count; k++) {
            SetShadeTex(&D_800D334C->glyphs1900[k * 2 + D_800CCB34], 0);
            setRGB0(&D_800D334C->glyphs1900[k * 2 + D_800CCB34], 0x80, 0x40, 0x40);
        }
        D_800D334C->runs[5].buffer = D_800CCB34;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E0184);
#endif

/* Build the summary's member numbers and their changes. */
void func_801E03B8(u8 member) {
    func_801DFD58(member);
    func_801DFE6C(member);
    func_801DFF50(member);
    func_801E0184(member);
}

/* Build the member's seven gauge rows: for each, the value before and after
 * the battle out of the highest (801dfa38) as a bar and its change bar, the
 * value, and when it changed an arrow and the change shaded by direction. */
#ifdef NON_MATCHING
void func_801E03FC(u8 member) {
    s32 max;
    s32 i;
    s32 j;
    s32 digit;
    s16 x;
    s16 top;
    s16 bottom;

    max = func_801DFA38(member);
    for (i = 0; i < 7; i++) {
        top = i * 8 + 0x92;
        bottom = i * 8 + 0x98;
        D_800D334C->rowACount[i] = 0;
        D_800D334C->rowBCount[i] = 0;
        func_801DF910(D_800CDD10[member][i], D_800CDD10[member + 3][i], max);
        func_801DF710(D_800D334C->barA[i], 0);
        func_801DF710(D_800D334C->barB[i], D_801E44E0);
        D_800D334C->barA[i][D_800CCB34].x0 = 0x78;
        D_800D334C->barA[i][D_800CCB34].y0 = top;
        D_800D334C->barA[i][D_800CCB34].x1 = D_801E44D8 + 0x78;
        D_800D334C->barA[i][D_800CCB34].y1 = top;
        D_800D334C->barA[i][D_800CCB34].x2 = 0x78;
        D_800D334C->barA[i][D_800CCB34].y2 = bottom;
        D_800D334C->barA[i][D_800CCB34].x3 = D_801E44D8 + 0x78;
        D_800D334C->barA[i][D_800CCB34].y3 = bottom;
        if (D_801E44E0 == 2) {
            x = D_801E44D8 + 0x78;
        } else {
            x = D_801E44D8 - (D_801E44DC - 0x78);
        }
        D_800D334C->barB[i][D_800CCB34].x0 = x;
        D_800D334C->barB[i][D_800CCB34].y0 = top;
        D_800D334C->barB[i][D_800CCB34].x1 = x + D_801E44DC;
        D_800D334C->barB[i][D_800CCB34].y1 = top;
        D_800D334C->barB[i][D_800CCB34].x2 = x;
        D_800D334C->barB[i][D_800CCB34].y2 = bottom;
        D_800D334C->barB[i][D_800CCB34].x3 = x + D_801E44DC;
        D_800D334C->barB[i][D_800CCB34].y3 = bottom;
        D_800D334C->barBuffer[i] = D_800CCB34;
        func_8008AAA0(D_801E44CC);
        for (j = 0; j < 3; j++) {
            digit = D_800C3CE3[j + 23];
            if (digit != 0xFF) {
                D_800D334C->rowACount[i] += func_80076A10(digit, &D_800D334C->rowA[i][D_800D334C->rowACount[i] * 2], j * 8 + 0xB8, i * 8 + 0x90);
            }
        }
        D_800D334C->rowABuffer[i] = D_800CCB34;
        if (D_801E44D4 != 0) {
            D_800D334C->rowBCount[i] = func_80076A10(D_801E44E4, D_800D334C->rowB[i], 0xD8, i * 8 + 0x90);
            func_8008AAA0(D_801E44D4);
            x = 0xE0;
            for (j = 0; j < 3; j++) {
                digit = D_800C3CFA[j];
                if (digit != 0xFF) {
                    D_800D334C->rowBCount[i] += func_80076A10(digit, &D_800D334C->rowB[i][D_800D334C->rowBCount[i] * 2], x, i * 8 + 0x90);
                    x += 8;
                }
            }
            func_801DF840(D_800D334C->rowB[i], D_801E44E0 - 2, D_800D334C->rowBCount[i], D_800CCB34);
            D_800D334C->rowBBuffer[i] = D_800CCB34;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E03FC);
#endif

/* Play effect id of the system effect bank. */
void func_801E09C0(u8 id) {
    func_80039E60((D_8005919C->id << 16) | id);
}

/* Start the result fanfare's three effects once. */
void func_801E09F4(void) {
    if (D_801E44C0 == 0) {
        D_80059180 = 1;
        func_801E09C0(0x5C);
        func_801E09C0(0x5D);
        func_801E09C0(0x5E);
        D_801E44C0 = 1;
    }
}

/* Start the fanfare and run battle frames until Cross is pressed. */
void func_801E0A4C(void) {
    func_801E09F4();
    func_800716D8();
    D_800D2D28->waitingCross = 1;
    while (D_800D3014 != 4) {
        func_800716D8();
    }
    D_800D2D28->waitingCross = 0;
}

/* Show the skills the member learnt in the battle, one at a time: the
 * summary title, then each new counter skill and each new level skill's
 * name with its mark, waiting for Cross after each. */
void func_801E0ACC(u8 member) {
    u16 newCounter;
    u16 newLevel;
    s32 i;
    void *image;
    u8 width;
    RECT rect;

    newCounter = D_8006D634.skills[D_800D2D24[member]].counterSkills & ~D_800C3E0C[member].counterSkills;
    newLevel = D_8006D634.skills[D_800D2D24[member]].levelSkills & ~D_800C3E0C[member].levelSkills;
    if (newCounter == 0 && newLevel == 0) {
        return;
    }
    if (D_800D2D28->unkB1 == 0) {
        func_8008F8F4(1, 0x28, 0x78, 0xE8, 0x58, 0, 1);
    }
    D_800D334C->runs[0].count = func_80076A10(member + 0xFC, D_800D334C->title[0], 0x40, 0xA4);
    D_800D334C->runs[0].buffer = D_800CCB34;
    D_800D2D28->waitingCross = 0;
    image = func_8008AC00(0x1B);
    func_80076D58(D_800D334C->glyphs34B0[0], 1, 2);
    func_80076D58(D_800D334C->glyphs34B0[1], 0, 3);
    for (i = 0; i < 16; i++) {
        if (func_80089C6C(newCounter, i) != 0) {
            s32 *buffer = &D_800CCB34;

            width = func_80034EAC(func_80033784(D_800D2D24[member], i), image, 0x1B, 0);
            rect.x = 0x3C0;
            rect.y = 0x1A;
            rect.w = 0x1E;
            rect.h = 0xD;
            func_800769E8(&rect, image);
            func_80076C78(&D_800D334C->glyphs34B0[1][*buffer], 0x52, 0x9C, 0, 0x1A, width);
            SetShadeTex(&D_800D334C->glyphs34B0[1][*buffer], 0);
            setRGB0(&D_800D334C->glyphs34B0[1][D_800CCB34], 0, 0x80, 0);
            D_800D334C->buffer34B0[1] = *buffer;
            func_80076C78(&D_800D334C->glyphs34B0[0][*buffer], width + 0x5A, 0x9C, D_800D2FA0[2], D_800D2FA0[3], D_800D2FA0[0]);
            D_800D334C->buffer34B0[0] = *buffer;
            D_800D2D28->show8F = 1;
            func_801E0A4C();
        }
    }
    for (i = 0; i < 16; i++) {
        if (func_80089C6C(newLevel, i) != 0) {
            s32 *buffer = &D_800CCB34;

            width = func_80034EAC(func_80033908(D_800D2D24[member] * 16 + i), image, 0x1B, 0);
            rect.x = 0x3C0;
            rect.y = 0x1A;
            rect.w = 0x1E;
            rect.h = 0xD;
            func_800769E8(&rect, image);
            func_80076C78(&D_800D334C->glyphs34B0[1][*buffer], 0x52, 0x9C, 0, 0x1A, width);
            SetShadeTex(&D_800D334C->glyphs34B0[1][*buffer], 0);
            setRGB0(&D_800D334C->glyphs34B0[1][D_800CCB34], 0x80, 0x80, 0);
            D_800D334C->buffer34B0[1] = *buffer;
            func_80076C78(&D_800D334C->glyphs34B0[0][*buffer], width + 0x5A, 0x9C, D_800D2FA0[2], D_800D2FA0[3], D_800D2FA0[0]);
            D_800D334C->buffer34B0[0] = *buffer;
            D_800D2D28->show8F = 1;
            func_801E0A4C();
        }
    }
    D_800D2D28->show8F = 0;
}


/* Lay out the summary's seven-glyph label (2d30). */
void func_801E1044(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        D_800D334C->count2D30 += func_80076A10(D_800C3388[i], D_800D334C->glyphs2D30[D_800D334C->count2D30], D_800C3390[i], D_800C33A0[i]);
    }
    D_800D334C->buffer2D30 = D_800CCB34;
}

/* Build the spoils window's numbers: the experience (six digits) and the
 * party gold (nine digits). */
void func_801E10F8(u32 experience) {
    s32 i;
    s32 n;
    s32 digit;

    func_8008AAA0(experience);
    for (i = 0; i < 6; i++) {
        n = i + 27;
        digit = D_800C3CDC[n];
        if (digit != 0xFF) {
            D_800D334C->run2F60.count += func_80076A10(digit, D_800D334C->glyphs2F60[D_800D334C->run2F60.count], i * 8 + 0xD8, 0x50);
        }
    }
    D_800D334C->run2F60.buffer = D_800CCB34;
    func_8008AAA0(D_8006EF58);
    for (i = 0; i < 9; i++) {
        n = i + 24;
        digit = D_800C3CDC[n];
        if (digit != 0xFF) {
            D_800D334C->run3140.count += func_80076A10(digit, D_800D334C->glyphs3140[D_800D334C->run3140.count], i * 8 + 0xC0, 0x60);
        }
    }
    D_800D334C->run3140.buffer = D_800CCB34;
}

/* Build the spoils window's two icons. */
void func_801E126C(void) {
    s32 *buffer;

    func_80076D58(D_800D334C->glyphs3410[0], 0, 2);
    func_80076D58(D_800D334C->glyphs3410[1], 1, 2);
    buffer = &D_800CCB34;
    func_80076C78(&D_800D334C->glyphs3410[0][*buffer], 0x20, 0x20, D_800D2F90[2], D_800D2F90[3], D_800D2F90[0]);
    func_80076C78(&D_800D334C->glyphs3410[1][*buffer], 0xB8, 0x40, D_800D2F90[6], D_800D2F90[7], D_800D2F90[4]);
    D_800D334C->buffer3410 = *buffer;
}

/* Add count of item id to an inventory list of size entries (ids and
 * counts): stack onto the item (at most 99) or take the first free entry;
 * a full list drops the item. */
void func_801E1370(u8 id, u8 count, u8 *ids, u8 *counts, u8 size) {
    s32 i;

    for (i = 0; i < size; i++) {
        if (ids[i] == id) {
            if (counts[i] + count >= 100) {
                counts[i] = 99;
            } else {
                counts[i] = count + counts[i];
            }
            break;
        }
    }
    if (i == size) {
        for (i = 0; i < size; i++) {
            if (ids[i] == 0) {
                ids[i] = id;
                counts[i] = count;
                break;
            }
        }
    }
}

/* Add eight drops (ids, counts and inventory list categories) to the
 * inventory. */
void func_801E1444(u8 *ids, u8 *counts, u8 *categories) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (ids[i] != 0) {
            switch (categories[i]) {
            case 0:
                func_801E1370(ids[i], counts[i], D_8006F36C.ids0, D_8006F36C.counts0, 100);
                break;
            case 1:
                func_801E1370(ids[i], counts[i], D_8006F36C.ids1, D_8006F36C.counts1, 200);
                break;
            case 2:
                func_801E1370(ids[i], counts[i], D_8006F36C.ids2, D_8006F36C.counts2, 150);
                break;
            case 3:
                func_801E1370(ids[i], counts[i], D_8006F36C.ids3, D_8006F36C.counts3, 100);
                break;
            case 4:
                func_801E1370(ids[i], counts[i], D_8006F36C.ids4, D_8006F36C.counts4, 150);
                break;
            }
        }
    }
}

/* Collect the rolled drops into eight distinct (category, id) entries
 * with their counts. */
void func_801E1590(u8 *ids, u8 *counts, u8 *categories) {
    s32 i;
    s32 j;
    s32 k;

    for (i = 0; i < 8; i++) {
        ids[i] = 0;
        counts[i] = 0;
    }
    k = 0;
    for (i = 0; i < 8; i++) {
        if (D_800CDCF4.ids[i] != 0) {
            for (j = 0; j < 8; j++) {
                if (D_800CDCF4.categories[i] == categories[j] && D_800CDCF4.ids[i] == ids[j]) {
                    counts[j]++;
                    break;
                }
            }
            if (j == 8) {
                categories[k] = D_800CDCF4.categories[i];
                ids[k] = D_800CDCF4.ids[i];
                counts[k]++;
                k++;
            }
        }
    }
}

/* Build the spoils window's item list: collect the drops, render each
 * item's name into VRAM with its count, then add the drops to the
 * inventory. */
#ifdef NON_MATCHING
void func_801E1690(void) {
    u8 ids[8];
    u8 categories[8];
    u8 counts[8];
    RECT rect;
    void *text[8];
    s32 i;
    s32 count;
    s32 width;

    func_801E1590(ids, counts, categories);
    for (i = 0, count = 0; i < 8; i++) {
        if (ids[i] != 0) {
            func_80076D58(D_800D334C->listA[count], 0, 1);
            func_80076D58(D_800D334C->listB[count], 0, 2);
            text[count] = func_8008AC00(0x1B);
            switch (categories[i]) {
            case 0:
                width = func_80034EAC(func_80033848(ids[i]), text[count], 0x1B, 0);
                break;
            case 1:
                width = func_80034EAC(func_800337E8(ids[i]), text[count], 0x1B, 0);
                break;
            case 2:
                width = func_80034EAC(func_80033818(ids[i]), text[count], 0x1B, 0);
                break;
            case 3:
                width = func_80034EAC(func_80033A5C(ids[i]), text[count], 0x1B, 0);
                break;
            case 4:
                width = func_80034EAC(func_80033A2C(ids[i]), text[count], 0x1B, 0);
                break;
            }
            rect.x = 0x380;
            rect.y = count * 13 + 0x100;
            rect.w = 0x1E;
            rect.h = 0xD;
            func_800769E8(&rect, text[count]);
            func_80076C78(&D_800D334C->listA[count][D_800CCB34], 0x2C, count * 16 + 0x30, 0, count * 13, width);
            func_80076C78(&D_800D334C->listB[count][D_800CCB34], 0x94, count * 16 + 0x30, counts[i] * 8 + 0x78, 0, 8);
            count++;
        }
    }
    func_801E1444(ids, counts, categories);
    for (i = 0; i < count; i++) {
        func_800320E8(text[i]);
    }
    D_800D334C->listCount = count;
    D_800D334C->listBuffer = D_800CCB34;
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E1690);
#endif

/* Show the member cards over six frames, then wait for Cross. */
void func_801E196C(void) {
    u32 step;
    u8 building;

    step = 0;
    building = 1;
    do {
        func_800716D8();
        switch (step) {
        case 0:
            func_801DE5C4();
            D_800D2D28->showCards = 1;
            D_800C3EAC->unk2DB = 0;
            break;
        case 1:
            func_801DE69C();
            break;
        case 2:
            func_801DEA18();
            break;
        case 3:
            func_801DEDC0(0);
            break;
        case 4:
            func_801DF270();
            break;
        case 5:
            func_801DF4C0();
            building = 0;
            break;
        }
        step++;
    } while (building);
    D_800D2D28->waitingCross = 1;
    while (D_800D3014 != 4) {
        func_800716D8();
    }
    D_800D2D28->waitingCross = 0;
}

/* Wait for Cross on the first member card (repeating the prompt sound),
 * then take each member's two values from the game data and rebuild the
 * summary rows. */
void func_801E1AA4(void) {
    s32 i;

    func_800716D8();
    D_800D2D28->waitingCross = 1;
    D_800D32F8[0]->flag15F9 = 1;
    while (D_800D32F8[0]->flag15F9 != 0) {
        if (D_800D3014 == 4) {
            break;
        }
        func_801E09C0(0x5B);
        func_800716D8();
    }
    D_800D2D28->waitingCross = 0;
    D_800D32F8[0]->flag15F9 = 0;
    for (i = 0; i < 3; i++) {
        D_800CDCB8[i].value = D_8006D8A0[D_800D2D24[i]].value3C;
        D_800CDCB8[i].value2 = D_8006D8A0[D_800D2D24[i]].value40;
        D_800CDCD0[i].value = 0;
        D_800CDCD0[i].value2 = 0;
    }
    func_801DF270();
    func_801DF4C0();
}

/* After Cross, show each present member's summary window when a stat
 * changed (waiting for Cross), then its skill results (801e0acc). */
void func_801E1C10(void) {
    s32 i;
    u8 shown;
    u8 member;

    func_801DEDC0(1);
    func_800716D8();
    D_800D2D28->waitingCross = 1;
    while (D_800D3014 != 4) {
        func_800716D8();
    }
    D_800D2D28->waitingCross = 0;
    func_800716D8();
    func_8008F8F4(1, 0x28, 0x78, 0xE8, 0x58, 0, 0);
    D_800D2D28->unkB1 = 0;
    for (i = 0; i < 3; i++) {
        shown = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            if (D_800CDD0A[i][0] != 0) {
                member = i;
                D_800D2D28->unkB1 = 1;
                func_801DFAA8(member);
                func_801E03B8(member);
                func_801E03FC(member);
                D_800D2D28->showSummary = 1;
                func_801E09F4();
                D_800D2D28->waitingCross = 0;
                shown = 1;
                func_800716D8();
            }
            D_800D2D28->waitingCross = 1;
            while (D_800D3014 != 4 && shown) {
                func_800716D8();
            }
            D_800D2D28->showSummary = 0;
            func_800716D8();
            func_801E0ACC(i);
            D_800D2D28->waitingCross = 0;
            D_800D2D28->unkB1 = 0;
        }
    }
}

/* Show the spoils window (experience, gold, items) until Cross. */
void func_801E1E10(u32 experience) {
    D_800D2D28->showCards = 0;
    D_800D2D28->showSummary = 0;
    D_800D2D28->show8F = 0;
    func_800716D8();
    func_8008F8F4(0, 0x18, 0x18, 0x90, 0xA0, 0, 1);
    func_8008F8F4(2, 0xB0, 0x38, 0x70, 0x38, 0, 1);
    func_800716D8();
    func_801E1044();
    func_801E10F8(experience);
    func_801E126C();
    func_801E1690();
    D_800D2D28->showSpoils = 1;
    func_80039DB8((D_8005919C->id << 16) | 0x5B);
    D_800D2D28->waitingCross = 1;
    while (D_800D3014 != 4) {
        func_800716D8();
    }
    D_800D2D28->waitingCross = 0;
    D_800D2D28->showSpoils = 0;
    D_800D2D28->unkB0 = 0;
    D_800D2D28->unkB1 = 0;
    D_800D2D28->unkB2 = 0;
    func_800716D8();
    func_8008FA60(0);
    func_8008FA60(1);
    func_8008FA60(2);
}

/* The battle results: allocate the member cards and the summary, show the
 * cards, the summaries and the spoils, then release them. */
void func_801E1FB8(u32 experience) {
    u8 saved;
    s32 i;

    saved = D_800C48EA;
    D_800D2D28->waitingCross = 0;
    for (i = 0; i < 3; i++) {
        D_800D32F8[i] = func_8008ABB8(sizeof(MemberCard), 0);
        bzero(D_800D32F8[i], sizeof(MemberCard));
    }
    D_800D334C = func_8008ABB8(sizeof(ResultSummary), 0);
    bzero(D_800D334C, sizeof(ResultSummary));
    D_800D32F8[0]->flag15F8 = (D_8006F8EA >> 15) ^ 1;
    func_800716D8();
    D_800C48EA = 0;
    func_801E196C();
    func_801E1AA4();
    func_801E1C10();
    func_801E1E10(experience);
    D_800D2D28->showCards = 0;
    D_800D2D28->showSummary = 0;
    D_800D2D28->show8F = 0;
    func_800716D8();
    for (i = 0; i < 3; i++) {
        func_800320E8(D_800D32F8[i]);
    }
    func_800320E8(D_800D334C);
    D_800C48EA = saved;
    func_80039FF8();
}

/* Hide the battle windows and reload the results resources: archive file
 * 2 of directory 0x10 (its items 1-4: text, a table, the glyph sprites and
 * the portraits). */
void func_801E211C(void) {
    u8 unused[0x60]; /* the original frame reserves 0x60 unused bytes */
    ResultArchive *archive;
    void *data;

    D_800C3E4C = 0;
    D_800D2D28->showCards = 0;
    D_800D2D28->showSummary = 0;
    D_800D2D28->show8F = 0;
    D_800D2D28->showSpoils = 0;
    D_800D2D28->unk7F = D_800D2D28->unk80 = D_800D2D28->unk81 = 0;
    func_800716D8();
    func_800716D8();
    func_800320E8(D_800D2F5C);
    func_80028470(0x10, 2);
    archive = func_8008ABB8(func_800288EC(2), 1);
    func_800295D8(2, archive, 0, 0x80);
    func_8008AC50();
    func_8003342C(archive);
    D_800D2C08[0] = func_80032E88(archive->items[0], 0);
    data = func_80032E88(archive->items[2], 0);
    func_8002DD20(data);
    func_800320E8(data);
    D_800D2F5C = func_80032E88(archive->items[1], 0);
    data = func_80032E88(archive->items[3], 0);
    func_80078310(data, 0xFC);
    func_800320E8(data);
    func_800320E8(archive);
    func_80076EA4();
}

/* Total the experience and gold of the defeated enemies, add the gold (up
 * to 9999999), clear empty party slots, grant the rewards and run the
 * result screens. */
void func_801E2280(void) {
    s32 i;
    u32 gold;
    u32 *partyGold;

    gold = 0;
    D_800D2C84.experience = 0;
    D_800D2C84.defeated = 0;
    if (D_800D2FC4 == 0) {
        for (i = 0; i < 8; i++) {
            if (D_800D2DCC[i + 3] != 0 && D_800C3EB6[i + 3].flag01 == 0
                && (D_800CCCE8[i + 3].flags7C & 0x8000) && D_800C3D1B[i][0] == 0) {
                D_800D2C84.experience += D_800CCCE8[i + 3].experience;
                gold += D_800CCCE8[i + 3].gold;
                D_800D2C84.defeated |= func_80089C08(i);
            }
        }
        partyGold = &D_8006EF58;
        *partyGold += gold;
        if (*partyGold > 9999999) {
            *partyGold = 9999999;
        }
        func_800BCD98(0);
    }
    for (i = 0; i < 3; i++) {
        if (D_800D2D24[i] == 0x7F) {
            D_800D2D24[i] = 0xFF;
        }
    }
    if (D_800D3294 != 0) {
        D_800D2D24[1] = D_800D2D24[2] = 0xFF;
        D_800C3EB6[1].id = D_800C3EB6[2].id = 0x7F;
    }
    func_801E2794();
    if (D_800D2D50 == 0 && !(D_8006F9DD & 8)) {
        func_801E1FB8(gold);
    }
}

/* Write the battle item counts back to inventory list 2. */
void func_801E24B0(void) {
    s32 i;
    s32 j;
    u8 *item;

    for (i = 0; i < 48; i++) {
        item = &D_800D2FE4[i];
        if (*item != 0) {
            for (j = 0; j < 150; j++) {
                if (*item == D_8006F65A[j]) {
                    D_8006F5C4[j] = D_800D2CB0[i];
                }
            }
        }
    }
}

/* Leave the battle: reload the resources, pick the next mode, write the
 * items back, grant the rewards (unless the battle was escaped or they are
 * skipped), release the battle's blocks and windows and reset the sound. */
void func_801E252C(void) {
    s32 i;
    u8 *outcome;

    func_801E211C();
    if (D_800C3D48 != 0) {
        func_800320E8(D_800D3284);
        func_800320E8(D_800D328C);
    }
    if (D_800594F8 == 0) {
        if (D_800D3338 != 0) {
            func_800199CC(6);
        } else if (D_8005947C != 0) {
            func_800199CC(2);
        } else if ((D_8006F94E & 0x7FF) >= 0x400) {
            func_800199CC(3);
        } else {
            func_8001ACA4();
            func_8008AC50();
            func_800199CC(1);
        }
    }
    func_8008AC50();
    func_801E24B0();
    outcome = &D_800C48EA;
    if (!(*outcome & 0xC0) && *outcome != 0x21 && D_800D2FC4 == 0 && D_800594D0 != 3) {
        func_800BFBA0();
        func_801E2280();
    }
    for (i = 0; i < 8; i += 2) {
        func_800320E8(D_800D3720[i].data);
    }
    for (i = 0; i < 10; i++) {
        func_800320E8(D_800C3E5C[i].data);
    }
    func_800320E8(D_800D329C);
    func_800320E8(D_800C3E24);
    func_8008FA60(5);
    func_8008FA60(4);
    func_800320E8(D_800D39F0);
    func_800320E8(D_800C3EA4);
    func_800320E8(D_800D2D28);
    func_800320E8(D_800C3EAC);
    func_800320E8(D_800D2C08[0]);
    func_800320E8(D_800D2F5C);
    func_8003218C(2);
    if (D_800594F8 != 0) {
        func_80039C4C(D_800C3E54);
        func_800399D4(D_800C3E54);
    }
    func_800B8774();
}

/* Grant the battle rewards unless the whole party is knocked out. */
void func_801E2794(void) {
    u8 slot;
    u8 knockedOut;
    u8 extra;

    knockedOut = 0;
    for (slot = 0; slot < 3; slot++) {
        if (D_800CCCE8[slot].flags7C & 0x8000) {
            knockedOut++;
        }
    }
    if (knockedOut != 3) {
        D_801E44E8 = D_801E44C8->growth;
        func_801E2ACC();
        func_801E3A18();
        func_801E403C();
        func_801E41B4();
        func_801E2888();
        func_801E42C4();
        if (D_8006DB2C == 0x12) {
            extra = D_8006E7AB;
            D_8006ED6E = 0x4000;
            if (extra != 0) {
                D_8006ED6E = 0xC000;
            }
        }
    }
}

/* Write each party member's HP, EP, counters and gear HP and fuel back to
 * the game data, clamped to their maximums (HP 1 when knocked out, gear HP a
 * tenth of the maximum when destroyed). */
void func_801E2888(void) {
    u8 unused[8]; /* the original frame reserves 8 unused bytes */
    u8 slot;
    u8 k;
    Combatant *record;
    Character *character;
    Gear *gear;
    Gear *block;

    for (slot = 0; slot < 3; slot++) {
        if (D_800D2D24[slot] == 0xFF) {
            continue;
        }
        record = &D_801E44C8->records[slot];
        character = &D_801E44C4->characters[record->id];
        gear = &D_801E44C4->u978.gears[record->gearId];
        block = &record->gear;
        if (record->id == 7 && (D_800CCCE8[slot].flags15A & 0x80)) {
            record->hp = (block->hp + 1) / 50;
            if (record->hp == 0) {
                record->hp = 1;
            }
        }
        character->hp = record->hp;
        character->ep = record->ep;
        if (character->hp > character->maxHp) {
            character->hp = character->maxHp;
        }
        if (character->ep > character->maxEp) {
            character->ep = character->maxEp;
        }
        for (k = 0; k < 7; k++) {
            character->counters[k] = record->counters[k];
        }
        character->value_3A = record->value_3A;
        if (record->flags7C & 0xC000) {
            character->hp = 1;
        }
        switch (record->gearId) {
        case 0 ... 6:
        case 8 ... 16:
            gear->hp = block->hp;
            gear->fuel = block->fuel;
            if (gear->hp > gear->maxHp) {
                gear->hp = gear->maxHp;
            }
            if (gear->fuel > gear->maxFuel) {
                gear->fuel = gear->maxFuel;
            }
            if (block->flags & 0x8000) {
                gear->hp = gear->maxHp / 10;
            }
            break;
        }
    }
}

/* Distribute the experience won: party members that stand share it (less
 * the penalty), every other character gets a reserve share of a third;
 * then record each slot's level gains and result stats. */
void func_801E2ACC(void) {
    s16 slots[11];
    u16 i;
    u16 absent;
    u32 experience;

    for (i = 0; i < 11; i++) {
        slots[i] = 100;
    }
    absent = 0;
    for (i = 0; i < 3; i++) {
        if (D_800D2D24[i] == 0xFF) {
            absent++;
            continue;
        }
        D_801E44EC = &D_801E44C8->records[i];
        if (D_801E44EC->flags7C & 0xC000) {
            absent++;
            slots[D_801E44EC->id] = 0xFF;
        } else {
            slots[D_801E44EC->id] = i;
        }
        D_801E44F8[i][0] = D_801E44EC->level;
        D_801E44F8[i][1] = D_801E44EC->level2;
    }
    experience = D_801E44C8->experience;
    if (D_801E44C8->penalty != 0) {
        experience -= (experience / 4) * D_801E44C8->penalty;
    }
    for (i = 0; i < 11; i++) {
        if (slots[i] == 0xFF) {
            continue;
        }
        D_801E44EC = (Combatant *)&D_801E44C4->characters[i];
        if (slots[i] < 3) {
            func_801E2EB0(experience / (3 - absent), slots[i], 0);
        } else {
            func_801E2EB0(experience / 3, 0xFF, 1);
        }
        func_801E308C();
    }
    for (i = 0; i < 3; i++) {
        if (D_800D2D24[i] == 0xFF) {
            continue;
        }
        D_801E44EC = (Combatant *)&D_801E44C4->characters[D_800D2D24[i]];
        D_801E44C8->levelGains[i][0] = D_801E44EC->level - D_801E44F8[i][0];
        D_801E44C8->levelGains[i][1] = D_801E44EC->level2 - D_801E44F8[i][1];
        if (D_801E44EC->id == 4) {
            D_801E44C8->stats[i][0] = D_801E44EC->value04 + D_801E44EC->value1C;
        } else {
            D_801E44C8->stats[i][0] = D_801E44EC->attack + D_801E44EC->value04;
        }
        D_801E44C8->stats[i][1] = D_801E44EC->stat5E;
        D_801E44C8->stats[i][2] = D_801E44EC->stat59 + D_801E44EC->value2D;
        D_801E44C8->stats[i][3] = D_801E44EC->stat5F;
        D_801E44C8->stats[i][4] = D_801E44EC->stat5B;
        D_801E44C8->stats[i][5] = D_801E44EC->stat5C;
        D_801E44C8->stats[i][6] = D_801E44EC->stat5A;
    }
}

/* Split a slot's experience into the level A and B pools by the record's
 * weights (three quarters each for a reserve member, all of it with option
 * 0x8000), at least 1, raised by half by flags 0x2000/0x1000; kept per slot. */
void func_801E2EB0(u32 experience, s16 slot, s16 reserve) {
    s16 weightA;
    s16 weightB;

    if (reserve == 1) {
        experience = experience * 3 / 4;
        D_801E44F0 = experience;
        D_801E44F4 = experience;
        return;
    }
    weightA = D_801E44C8->records[slot].weightA;
    weightB = D_801E44C8->records[slot].weightB;
    if (weightA < 2) {
        weightA = 1;
    }
    if (weightB < 2) {
        weightB = 1;
    }
    D_801E44F0 = experience * weightA / (weightA + weightB);
    D_801E44F4 = experience * weightB / (weightA + weightB);
    if (D_801E44C4->options & 0x8000) {
        D_801E44F0 = experience;
        D_801E44F4 = experience;
    }
    if (D_801E44F0 == 0) {
        D_801E44F0 = 1;
    }
    if (D_801E44F4 == 0) {
        D_801E44F4 = 1;
    }
    if (D_801E44EC->flags32 & 0x2000) {
        D_801E44F0 += D_801E44F0 >> 1;
        D_801E44F4 += D_801E44F4 >> 1;
    }
    if (D_801E44EC->flags32 & 0x1000) {
        D_801E44F4 += D_801E44F4 >> 1;
    }
    D_801E44C8->gained[slot].value = D_801E44F0;
    D_801E44C8->gained[slot].value2 = D_801E44F4;
}

/* Add the experience pools to the current record's totals and gain levels
 * A and B while the pools reach the next level (none past 99 with option
 * 0x8000, and nothing more at 99). */
void func_801E308C(void) {
    s32 rest;

    D_801E44EC->totalA += D_801E44F0;
    D_801E44EC->totalB += D_801E44F4;
    if (D_801E44EC->level == 99 && (D_801E44C4->options & 0x8000)) {
        D_801E44F0 = 0;
    }
    if (D_801E44EC->level2 == 99 && (D_801E44C4->options & 0x8000)) {
        D_801E44F4 = 0;
    }
    if (D_801E44EC->level == 99) {
        D_801E44F0 = 0;
    }
    if (D_801E44EC->level2 == 99) {
        D_801E44F4 = 0;
    }
    rest = D_801E44EC->nextA - D_801E44F0;
    if (rest > 0) {
        D_801E44EC->nextA = rest;
    } else {
        do {
            if (++D_801E44EC->level >= 100 && (D_801E44C4->options & 0x8000)) {
                D_801E44EC->level--;
            }
            D_801E44EC->nextA = D_801E44E8->experience[D_801E44EC->level - 1];
            func_801E335C();
            rest += D_801E44EC->nextA;
        } while (rest <= 0);
    }
    D_801E44EC->nextA = rest;
    rest = D_801E44EC->nextB - D_801E44F4;
    if (rest > 0) {
        D_801E44EC->nextB = rest;
    } else {
        do {
            if (++D_801E44EC->level2 >= 100 && (D_801E44C4->options & 0x8000)) {
                D_801E44EC->level2--;
            }
            D_801E44EC->nextB = D_801E44E8->experience[D_801E44EC->level2 - 1];
            func_801E3500();
            rest += D_801E44EC->nextB;
        } while (rest <= 0);
    }
    D_801E44EC->nextB = rest;
}

/* Level A growth of the current record: max HP, then stats 58, 59, 5e
 * and 5f toward the growth data's targets for its level range. */
void func_801E335C(void) {
    u8 high;
    u8 cap;
    u8 level;

    high = 0;
    cap = 100;
    if (D_801E44EC->level >= 100) {
        high = 1;
        cap = 200;
    }
    level = D_801E44EC->level;
    D_801E44EC->maxHp = func_801E3700(D_801E44EC->maxHp, D_801E44EC->level);
    D_801E44EC->attack = func_801E3610(D_801E44EC->attack,
        D_801E44E8->characters[D_801E44EC->id].statTargets[0][high], cap, level);
    D_801E44EC->stat59 = func_801E3610(D_801E44EC->stat59,
        D_801E44E8->characters[D_801E44EC->id].statTargets[1][high], cap, level);
    D_801E44EC->stat5E = func_801E3610(D_801E44EC->stat5E,
        D_801E44E8->characters[D_801E44EC->id].statTargets[2][high], cap, level);
    D_801E44EC->stat5F = func_801E3610(D_801E44EC->stat5F,
        D_801E44E8->characters[D_801E44EC->id].statTargets[3][high], cap, level);
}

/* Level B growth of the current record: max EP, then stats 5b and 5c
 * toward the growth data's targets for its level range. */
void func_801E3500(void) {
    u8 high;
    u8 cap;
    u8 level;

    high = 0;
    cap = 100;
    if (D_801E44EC->level2 >= 100) {
        high = 1;
        cap = 200;
    }
    level = D_801E44EC->level2;
    D_801E44EC->maxEp = func_801E38CC(D_801E44EC->maxEp, D_801E44EC->level2);
    D_801E44EC->stat5B = func_801E3610(D_801E44EC->stat5B,
        D_801E44E8->characters[D_801E44EC->id].statTargets[4][high], cap, level);
    D_801E44EC->stat5C = func_801E3610(D_801E44EC->stat5C,
        D_801E44E8->characters[D_801E44EC->id].statTargets[5][high], cap, level);
}

/* Grow a stat by 0 or 1: the chance is the share of the distance to the
 * target left over the levels to the cap. Capped at 200. */
/* Grow a stat by 0 or 1: the chance is the share of the distance to the
 * target left over the levels to the cap. Capped at 200. */
u8 func_801E3610(u8 stat, u8 target, u8 cap, u8 level) {
    s32 random;
    s32 share;
    u8 grow;

    random = rand();
    share = (target - stat) * 100 / (cap - level) - 50;
    grow = (s16)(share + random % 100) > 49;
    stat += grow;
    if (stat > 200) {
        stat = 200;
    }
    return stat;
}

/* Grow max HP by a random share of the distance to the growth data's
 * target for the level range, at least 2. Capped at 999. */
#ifdef NON_MATCHING
/* The original subtracts (level - 99) as its own addiu and schedules the
 * remainder registers differently; the result tail is also duplicated. */
u16 func_801E3700(u16 maxHp, u8 level) {
    s16 gain;
    s32 random;
    u16 hp;

    hp = maxHp;
    if (level < 100) {
        random = rand() % 100;
        gain = random * (D_801E44E8->characters[D_801E44EC->id].maxHpTargets[0] - (level - 99) - hp)
            / ((100 - level) * 100) + 2;
    } else {
        random = rand() % 100;
        gain = random * ((D_801E44E8->characters[D_801E44EC->id].maxHpTargets[1] - hp)
            / ((201 - level) * 100)) * 2 + 2;
    }
    if (gain >= 0) {
        hp = maxHp + gain;
    }
    if ((s16)hp >= 1000) {
        hp = 999;
    }
    return hp;
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3700);
#endif

/* Grow max EP by 0 or 1 toward the growth data's target for the level
 * range (as func_801E3610). Capped at 99. */
u8 func_801E38CC(u8 maxEp, u8 level) {
    u8 target;
    u8 cap;
    s32 random;
    s32 share;
    u8 grow;

    if (level < 100) {
        target = D_801E44E8->characters[D_801E44EC->id].maxEpTargets[0];
        cap = 99;
    } else {
        target = D_801E44E8->characters[D_801E44EC->id].maxEpTargets[1];
        cap = 200;
    }
    random = rand();
    share = (target - maxEp) * 100 / (cap - level) - 50;
    grow = (s16)(share + random % 100) > 49;
    maxEp += grow;
    if (maxEp >= 100) {
        maxEp = 99;
    }
    return maxEp;
}

/* Learn skills for each party slot that is not knocked out: a counter skill
 * (not characters 7 and 8), a level skill (not 10), the unlocks, and the
 * special cases of characters 8 and 7. */
void func_801E3A18(void) {
    u8 slot;
    u8 learnt;

    for (slot = 0; slot < 3; slot++) {
        D_801E44EC = &D_801E44C8->records[slot];
        if (D_801E44EC->flags7C & 0x8000) {
            continue;
        }
        switch (D_801E44EC->id) {
        case 7:
        case 8:
            break;
        default:
            learnt = func_801E3BE0(D_801E44EC->id);
            if (learnt != 0) {
                D_801E44C8->learntCounter[slot] = learnt;
            }
            break;
        }
        if (D_801E44EC->id != 10) {
            learnt = func_801E3D54(D_801E44EC->id);
            if (learnt != 0) {
                D_801E44C8->learntLevel[slot] = learnt;
            }
        }
        switch (D_801E44EC->id) {
        case 7:
        case 8:
        case 10:
            break;
        default:
            func_801E3E14(D_801E44EC->id);
            break;
        }
        switch (D_801E44EC->id) {
        case 8:
        case 9:
        case 10:
            break;
        default:
            func_801E3F28(D_801E44EC->id);
            break;
        }
        if (D_801E44EC->id == 8) {
            func_801E3EA4();
        }
        if (D_801E44EC->id == 7) {
            func_801E3FB0();
        }
    }
}

/* Learn the first unknown counter skill (of 7, or 13 with option 0x4000)
 * whose level is reached and whose seven counter requirements the current
 * record meets; stop at the first whose level is not reached. Returns its
 * index, or 0. */
#ifdef NON_MATCHING
/* The original keeps id in a copy and reloads the current record inside
 * the requirement loop; this C lets GCC hoist it. */
u8 func_801E3BE0(u8 id) {
    u8 count;
    u8 learnt;
    u8 j;
    u8 k;

    count = 7;
    if (D_8006F8EA & 0x4000) {
        count = 13;
    }
    learnt = 0xFF;
    for (j = 0; j < count; j++) {
        if (D_8006D634.skills[id].counterSkills & (0x8000 >> j)) {
            continue;
        }
        if (D_8006D8A0[id].level < D_801E44E8->characters[id].counterLevels[j]) {
            break;
        }
        for (k = 0; k < 7; k++) {
            if (D_801E44EC->counters[k] < D_801E44E8->characters[id].requirements[j][k]) {
                break;
            }
        }
        if (k == 7) {
            learnt = j;
            break;
        }
    }
    if (learnt == 0xFF) {
        return 0;
    }
    D_8006D634.skills[id].counterSkills |= 0x8000 >> learnt;
    return learnt;
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E3BE0);
#endif

/* Learn the first of the character's twelve level skills whose level is
 * reached and which is not yet known. Returns its number (1-12), or 0. */
u8 func_801E3D54(u8 id) {
    u32 bit;
    u8 k;
    u8 level;
    u32 known;

    bit = 0x8000;
    for (k = 0; k < 12; k++, bit >>= 1) {
        level = D_801E44E8->characters[id].levelSkills[k];
        if (level == 0xFF) {
            return 0;
        }
        if (D_8006D8A0[id].level >= level) {
            known = D_801E44C4->skills[id].levelSkills;
            if (!(bit & known)) {
                D_801E44C4->skills[id].levelSkills = bit | known;
                return k + 1;
            }
        }
    }
    return 0;
}

/* For each of the character's nine unlock entries whose counter skill is
 * known, set the matching unlock bit (from bit 3). */
void func_801E3E14(u8 id) {
    u8 k;
    u8 entry;

    for (k = 0; k < 9; k++) {
        entry = D_801E44E8->characters[id].unlocksA[k];
        if (entry == 0xFF) {
            return;
        }
        if (D_801E44C4->skills[id].counterSkills & (0x8000 >> (entry - 1))) {
            D_801E44C4->skills[id].unlocksA |= 0x8000 >> (k + 3);
        }
    }
}

/* Character 8's nine level entries: learn each one its level reaches. */
void func_801E3EA4(void) {
    u8 k;
    u8 level;
    u32 known;
    s32 bit;

    for (k = 0; k < 9; k++) {
        level = D_801E44E8->characters[8].unlocksA[k];
        if (level == 0xFF) {
            return;
        }
        if (D_8006D8A0[8].level >= level) {
            known = D_801E44C4->skills[8].unlocksA;
            bit = 0x1000 >> k;
            if (!(known & bit)) {
                D_801E44C4->skills[8].unlocksA = bit | known;
            }
        }
    }
}

/* For each of the character's thirteen second unlock entries whose level
 * skill is known, set the matching unlock bit. */
void func_801E3F28(u8 id) {
    u8 k;
    u8 entry;

    for (k = 0; k < 13; k++) {
        entry = D_801E44E8->characters[id].unlocksB[k];
        if (entry == 0) {
            return;
        }
        if (D_801E44C4->skills[id].levelSkills & (0x8000 >> (entry - 1))) {
            D_801E44C4->skills[id].unlocksB |= 0x8000 >> k;
        }
    }
}

/* Character 7's derived values from the current record's max HP and
 * attack. */
void func_801E3FB0(void) {
    GameData *game = D_801E44C4;

    game->u978.id7.value_E58 = D_801E44EC->maxHp * 200;
    game->u978.id7.value_E30 = D_801E44EC->attack / 5 + 1;
    game->u978.id7.value_E64 = D_801E44EC->maxHp * 10;
    game->u978.id7.value_E66 = D_801E44EC->maxHp * 10;
}

/* Advance each character's tier: 3, 4 and 5 at the growth data's tier
 * levels, 6 to 7 at level 50 with option 0x4000. */
void func_801E403C(void) {
    u8 id;
    Character *character;
    CharacterSkills *skills;

    for (id = 0; id < 11; id++) {
        character = &D_801E44C4->characters[id];
        skills = &D_801E44C4->skills[id];
        if (skills->tier == 7) {
            continue;
        }
        switch (skills->tier) {
        case 3:
            if (character->level >= D_801E44E8->characters[id].tierLevels[0]) {
                skills->tier = 4;
            }
            break;
        case 4:
            if (character->level >= D_801E44E8->characters[id].tierLevels[1]) {
                skills->tier = 5;
            }
            break;
        case 5:
            if (character->level >= D_801E44E8->characters[id].tierLevels[2]) {
                skills->tier = 6;
            }
            break;
        case 6:
            if (character->level >= 50 && (D_8006F8EA & 0x4000)) {
                skills->tier = 7;
            }
            break;
        }
    }
}

/* Levels 50, 60 and 70 set unlock bits 8, 4 and 2 of each party member. */
void func_801E41B4(void) {
    u8 slot;
    u8 id;

    for (slot = 0; slot < 3; slot++) {
        id = D_800CCCE8[slot].id;
        if (D_8006D8A0[id].level >= 50) {
            D_8006D634.skills[id].unlocksA |= 8;
        }
        if (D_8006D8A0[id].level >= 60) {
            D_8006D634.skills[id].unlocksA |= 4;
        }
        if (D_8006D8A0[id].level >= 70) {
            D_8006D634.skills[id].unlocksA |= 2;
        }
    }
}

/* Roll one drop per defeated enemy: the first at its chance (always when a
 * party member has flag 0x800), else the second at its chance. */
#ifdef NON_MATCHING
/* The original compares forced with its own constant 1 (masked); here GCC
 * reuses the hoisted 1 of the bit shift. */
void func_801E42C4(void) {
    u8 i;
    u8 forced;
    s32 bit;
    Combatant *enemy;

    forced = 0;
    for (i = 0; i < 3; i++) {
        D_801E44EC = &D_801E44C8->records[i];
        if (D_801E44EC->flags32 & 0x800) {
            forced = 1;
        }
    }
    for (i = 0; i < 8; i++) {
        D_801E44C8->drops.ids[i] = 0;
        bit = 1 << i;
        if (!(D_801E44C8->defeated & bit)) {
            continue;
        }
        D_801E44EC = enemy = &D_801E44C8->records[i + 3];
        if (rand() % 100 < enemy->dropChances[0] || forced == 1) {
            D_801E44C8->drops.categories[i] = enemy->dropCategories[0];
            D_801E44C8->drops.ids[i] = enemy->dropIds[0];
        } else if (rand() % 100 < enemy->dropChances[1]) {
            D_801E44C8->drops.categories[i] = enemy->dropCategories[1];
            D_801E44C8->drops.ids[i] = enemy->dropIds[1];
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801E42C4);
#endif
