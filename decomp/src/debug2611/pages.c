/* debug2611 state pages (text 8028022C-80280844, rodata 80280000-80280088):
 * text pages of battle state chosen by the resident debug page number. A
 * unit of its own: its positive li are ori (ASPSX 2.34) where the tools
 * unit's are addiu, and GCC 2.6.3 and 2.7.2 build it where the tools unit's
 * 2.7.2-cdk does not (debug2611.mk). */
#include "battle_debug.h"

/* "\nChar#%d:", linked as original rodata below its user (INCLUDE_RODATA). */
extern char D_8028007C[];

/* Print the battle state page chosen by the resident debug page number:
 * 1 the enemies' HP (their gear's when they fight in one) and the action
 * list, 2 the presentation events (type, parameter, target mask), 3 the
 * acting enemy's AI flags, 4 the party's and the characters' progress
 * counters. The action list's row y is computed from the index, (i + 2) * 8. */
void func_8028022C(void) {
    s32 i, j;
    s32 x;
    s32 hp;

    switch (D_8005959C) {
    case 1:
        func_80037058(0, 0);
        for (i = 3; i < 11; i++) {
            if (D_800C3EB0.slots[i].gear == 0) {
                hp = D_800CCCE8.records[i].pilot.hp;
            } else {
                hp = D_800CCCE8.records[i].gear.hp;
            }
            func_8003700C("%d,", hp);
        }
        func_8003700C("\n");
        func_8003700C("No  Cd  Cl  An  P1  P2  P3  Tg\n");
        for (i = 0; i < 23; i++) {
            func_80037058(0, (i + 2) * 8);
            func_8003700C("%X", i);
            func_80037058(0x24, (i + 2) * 8);
            func_8003700C("%X", D_800D2E5C[i].code);
            func_80037058(0x48, (i + 2) * 8);
            func_8003700C("%X", D_800D2E5C[i].cls);
            func_80037058(0x6C, (i + 2) * 8);
            func_8003700C("%X", D_800D2E5C[i].anim);
            func_80037058(0x90, (i + 2) * 8);
            func_8003700C("%X", D_800D2E5C[i].param[0]);
            func_80037058(0xB4, (i + 2) * 8);
            func_8003700C("%X", D_800D2E5C[i].param[1]);
            func_80037058(0xD8, (i + 2) * 8);
            func_8003700C("%X", D_800D2E5C[i].param[2]);
            func_80037058(0xFC, (i + 2) * 8);
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
            func_8003700C("%X", D_800C3EB0.events[i].type);
            func_80037058(x + 0x48, (i / 2 + 5) * 8);
            func_8003700C("%X", D_800C3EB0.events[i].parameter);
            func_80037058(x + 0x6C, (i / 2 + 5) * 8);
            func_8003700C("%X", D_800C3EB0.events[i].targetMask);
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
                func_8003700C(" %d", D_800CCCE8.records[i].pilot.useCounts[j]);
            }
        }
        func_8003700C("\n");
        for (i = 0; i < 11; i++) {
            func_8003700C(D_8028007C, i);
            for (j = 0; j < 7; j++) {
                func_8003700C(" %d", D_8006D634.characters[i].useCounts[j]);
            }
        }
        break;
    }
}

/* "\nChar#%d:". The original assembler left a stray byte (0x2c) in the
 * string's alignment padding, so the literal is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/debug2611/asm/nonmatchings/pages", D_8028007C);
