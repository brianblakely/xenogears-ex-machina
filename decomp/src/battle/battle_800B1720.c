/* Battle unit from 800B1720 to the end of the overlay text.
 *
 * This code comes from a different toolchain than the rest of the overlay:
 * stores to globals use a register for %hi instead of the assembler's $at
 * expansion, positive `li` becomes `addiu`, and five epilogues (800B8090,
 * 800B88BC, 800BEF84, 800BEFEC, 800BF718) carry the stack adjustment in the
 * `jr $ra` delay slot. Neither GCC 2.6.3 nor 2.7.2 with maspsx (2.34 or 2.79)
 * reproduces that, so the unit stays assembly. Its rodata starts at
 * 0x800707DC, after 800B12D0's jump table. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B1720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B1EA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B1F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B1F6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B2AEC);

void func_800B3348(void) {
}

void func_800B3350(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3358);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B35C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3658);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B36BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B383C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3878);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B397C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B39C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3B6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3B94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3C2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3C74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3CD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3E04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B3F04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B4EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B4F88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B50D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B51B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B56E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B572C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B57E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5924);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B59BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5C18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5CC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5DC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5DF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B5FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6004);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B61B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B61F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B626C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B62C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B639C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B63F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6464);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B64D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B65B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6808);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6930);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B69E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6B98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6BFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6C98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6CEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6DC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6E84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B6F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B73A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B73EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7424);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7C28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B7E94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8048);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8054);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8068);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B81BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B838C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B853C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8774);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B88C4);

void func_800B89F4(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B89FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8D04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8D7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8DA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B8EBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9020);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B905C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9B30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9B54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9C00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800B9F78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BA4E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BA59C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BA614);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BA768);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BA8F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BA984);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BAB0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BABDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BAC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BACBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BADD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BAEB8);

void func_800BAF40(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BAF48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB080);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB13C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB248);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB350);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB540);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB620);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB690);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB6E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB760);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB7F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BB9D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BBAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BBEE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC158);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC2F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC3F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC404);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BC460);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCAA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCAD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCAFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCBB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCC60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCD8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCEAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BCFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD024);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD1FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD2E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD3AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD7A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD810);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BD974);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDB74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDC14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDC78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDCF8);

void func_800BDD34(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDD3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDE58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BDF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE11C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE1C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE538);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE6A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE6E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BE790);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEB04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEC18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BED30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BED4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEDE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEE2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEEB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEF24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEF8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BEFF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF0B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF0C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF1EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF2B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF3A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF3E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF4F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF5E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF6CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF6F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF730);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF73C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF7C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF85C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF8CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF954);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BF9EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BFA9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BFBA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BFC80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BFD88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BFDA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C06E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0758);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C07CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0828);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C08CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0D18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0F70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C0FAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C1140);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B1720", func_800C11CC);
