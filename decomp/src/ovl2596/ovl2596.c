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
        func_800728B8(D_800D334C->text[0], D_800D334C->runs[1].count, D_800D334C->runs[1].buffer);
        func_800728B8(D_800D334C->glyphs1630[0], D_800D334C->runs[2].count, D_800D334C->runs[2].buffer);
        func_800728B8(D_800D334C->glyphs1720[0], D_800D334C->runs[4].count, D_800D334C->runs[4].buffer);
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
                (&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 4)->r0 = 0x80;
                (&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 4)->g0 = 0x40;
                (&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 4)->b0 = 0x40;
                func_80043C24(&D_800D32F8[i]->labels[(D_800D32F8[i]->runs[1].count - 1) * 2 + D_800CCB00.buffer], 0);
                (&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 2)->r0 = 0x40;
                (&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 2)->g0 = 0x80;
                (&D_800D32F8[i]->labels[D_800D32F8[i]->runs[1].count * 2 + D_800CCB34] - 2)->b0 = 0x40;
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

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DEDC0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DF270);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DF4C0);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DF710);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DF840);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DF910);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFA38);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFAA8);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFD58);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DFE6C);

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
