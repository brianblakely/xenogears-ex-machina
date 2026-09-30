#include "battle_results.h"

/* Queue every member card's glyph runs while the cards are shown. */
void func_801DE048(void) {
    s32 i;

    if (D_800D2D28->showCards != 0) {
        for (i = 0; i < 3; i++) {
            func_800728B8(D_800D32F8[i]->portrait[0], D_800D32F8[i]->runs[0].count, D_800D32F8[i]->runs[0].buffer);
            func_800728B8(D_800D32F8[i]->labels[0], D_800D32F8[i]->runs[1].count, D_800D32F8[i]->runs[1].buffer);
            func_800728B8(D_800D32F8[i]->field960[0], D_800D32F8[i]->runs[4].count, D_800D32F8[i]->runs[4].buffer);
            func_800728B8(D_800D32F8[i]->fieldB40[0], D_800D32F8[i]->runs[6].count, D_800D32F8[i]->runs[6].buffer);
            func_800728B8(D_800D32F8[i]->fieldA50[0], D_800D32F8[i]->runs[5].count, D_800D32F8[i]->runs[5].buffer);
            func_800728B8(D_800D32F8[i]->fieldBE0[0], D_800D32F8[i]->runs[7].count, D_800D32F8[i]->runs[7].buffer);
            func_800728B8(D_800D32F8[i]->field780[0], D_800D32F8[i]->runs[2].count, D_800D32F8[i]->runs[2].buffer);
            func_800728B8(D_800D32F8[i]->field870[0], D_800D32F8[i]->runs[3].count, D_800D32F8[i]->runs[3].buffer);
            func_800728B8(D_800D32F8[i]->fieldC80[0], D_800D32F8[i]->runs[8].count, D_800D32F8[i]->runs[8].buffer);
            func_800728B8(D_800D32F8[i]->fieldF00[0], D_800D32F8[i]->runs[9].count, D_800D32F8[i]->runs[9].buffer);
            func_800728B8(D_800D32F8[i]->field1180[0], D_800D32F8[i]->runs[10].count, D_800D32F8[i]->runs[10].buffer);
            func_800728B8(D_800D32F8[i]->field13B0[0], D_800D32F8[i]->runs[11].count, D_800D32F8[i]->runs[11].buffer);
        }
    }
}

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DE1C4);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DE408);

/* Queue the result screens' primitives. */
void func_801DE594(void) {
    func_801DE048();
    func_801DE1C4();
    func_801DE408();
}

/* Build each present member's portrait glyphs. */
#ifdef NON_MATCHING
void func_801DE5C4(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800D32F8[i]->runs[0].count = 0;
        if (D_800C3EB6[i].id != 0x7F) {
            D_800D32F8[i]->runs[0].count += func_80076A10(i + 0xFC, D_800D32F8[i]->portrait[D_800D32F8[i]->runs[0].count], 0x20, (i + 1) * 0x20 + 4);
        }
        D_800D32F8[i]->runs[0].buffer = D_800CCB34;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DE5C4);
#endif

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DE69C);

INCLUDE_ASM(".local/decomp/ovl2596/asm/nonmatchings/ovl2596", func_801DEA18);

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
