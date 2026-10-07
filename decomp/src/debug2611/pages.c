/* debug2611 state pages: text pages of battle state chosen by the resident
 * debug page number. A unit of its own: its positive li are ori and globals
 * are addressed through assembler macros, like the qualified toolchain. The C
 * below differs from the original only in register allocation (the original
 * puts the text row in s0 and the record offset/column in s1). The unit's
 * .rodata ends with a stray 0x2c byte in the padding after "
Char#%d:". */
#include "battle_debug.h"

/* Print the battle state page chosen by the resident debug page number:
 * 1 enemy HP and the effect records, 2 the command records, 3 the acting
 * enemy's AI flags, 4 the party's and the characters' work values.
 * NON_MATCHING: only the order of the effect table loop's setup differs:
 * the original clears the record offset (s1) in the "No Cd..." call's delay
 * slot and sets y = 0x10 (s0) after the format pointer; here y is set first. */
#ifdef NON_MATCHING
void func_8028022C(void) {
    s32 i, j;
    s32 x, y;
    s32 hp;

    switch (D_8005959C) {
    case 1:
        func_80037058(0, 0);
        for (i = 3; i < 11; i++) {
            if (D_800C3EB0.placements[i].enemy == 0) {
                hp = D_800CCCE8[i].hp;
            } else {
                hp = D_800CCCE8[i].enemy_hp;
            }
            func_8003700C("%d,", hp);
        }
        func_8003700C("\n");
        func_8003700C("No  Cd  Cl  An  P1  P2  P3  Tg\n");
        for (i = 0, y = 0x10; i < 23; i++, y += 8) {
            func_80037058(0, y);
            func_8003700C("%X", i);
            func_80037058(0x24, y);
            func_8003700C("%X", D_800D2E5C[i].code);
            func_80037058(0x48, y);
            func_8003700C("%X", D_800D2E5C[i].cls);
            func_80037058(0x6C, y);
            func_8003700C("%X", D_800D2E5C[i].anim);
            func_80037058(0x90, y);
            func_8003700C("%X", D_800D2E5C[i].param[0]);
            func_80037058(0xB4, y);
            func_8003700C("%X", D_800D2E5C[i].param[1]);
            func_80037058(0xD8, y);
            func_8003700C("%X", D_800D2E5C[i].param[2]);
            func_80037058(0xFC, y);
            func_8003700C("%X", D_800D2E5C[i].target);
        }
        break;
    case 2:
        func_80037058(0, 0x20);
        func_8003700C("No  An  Sb  Tg  No  An  Sb  Tg  \n");
        for (i = 0; i < 32; i++) {
            x = (i % 2) * 0x90;
            func_80037058(x, (i / 2 + 5) * 8);
            func_8003700C("%X", i);
            func_80037058(x + 0x24, (i / 2 + 5) * 8);
            func_8003700C("%X", D_800C3FFE[i].anim);
            func_80037058(x + 0x48, (i / 2 + 5) * 8);
            func_8003700C("%X", D_800C3FFE[i].sub);
            func_80037058(x + 0x6C, (i / 2 + 5) * 8);
            func_8003700C("%X", D_800C3FFE[i].target);
        }
        break;
    case 3:
        if (D_800C3EAC->actor < 3) {
            break;
        }
        func_80037058(0, 0x50);
        func_8003700C("bFlag\n");
        for (i = 0; i < 8; i++) {
            func_8003700C("%X ", D_800D3400[D_800C3EAC->actor - 3].bflag[i]);
        }
        func_8003700C("\n");
        for (i = 0; i < 8; i++) {
            func_8003700C("%X ", D_800D3400[D_800C3EAC->actor - 3].bflag[i + 8]);
        }
        func_8003700C("\nhFlag\n");
        for (i = 0; i < 4; i++) {
            func_8003700C("%X ", D_800D3400[D_800C3EAC->actor - 3].hflag[i]);
        }
        func_8003700C("\n");
        for (i = 0; i < 4; i++) {
            func_8003700C("%X ", D_800D3400[D_800C3EAC->actor - 3].hflag[i + 4]);
        }
        func_8003700C("\nlFlag\n");
        for (i = 0; i < 2; i++) {
            func_8003700C("%X ", D_800D3400[D_800C3EAC->actor - 3].lflag[i]);
        }
        func_8003700C("\n");
        for (i = 0; i < 2; i++) {
            func_8003700C("%X ", D_800D3400[D_800C3EAC->actor - 3].lflag[i + 2]);
        }
        break;
    case 4:
        func_80037058(0, 0x20);
        for (i = 0; i < 3; i++) {
            func_8003700C("\nWork#%d:", i);
            for (j = 0; j < 7; j++) {
                func_8003700C(" %d", D_800CCCE8[i].work[j]);
            }
        }
        func_8003700C("\n");
        for (i = 0; i < 11; i++) {
            func_8003700C("\nChar#%d:", i);
            for (j = 0; j < 7; j++) {
                func_8003700C(" %d", D_8006D8A0[i].work[j]);
            }
        }
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/debug2611/asm/nonmatchings/pages", func_8028022C);
#endif
