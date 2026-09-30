/* Battle setup module (overlay slot 2615, directory 12 file 4, loaded at
 * 0x801E4000 by the resident battle entry 8001bbac together with the effect
 * header and archive files 2 and 3). It places the formation, builds the
 * combatant records and turn tables, sets up the stage, gauges and intro,
 * and runs as the battle's setup task (801e5840 phases, 801e7098 task). */
#include "battle_setup.h"


/* Clear the battle outcome flags, publish the party ids and reset every
 * slot's flags (demo battles place everyone alone). */
void func_801E4048(void) {
    s32 i;

    D_800D3338 = 0;
    D_800C3D44 = 0;
    D_800D2D50 = 0;
    D_800D2FC4 = 0;
    D_800C3D5C = 0;
    D_800D2D44 = 0;
    D_800C492A = 0;
    D_8005941C = 0;
    D_8005942C = 0;
    for (i = 0; i < 3; i++) {
        D_80059468[i] = D_800D2D24[i];
        D_800C3EB4[i].id = D_800D2D24[i];
    }
    for (i = 0; i < SLOT_COUNT; i++) {
        D_800C3EB4[i].flag3 = 0;
        if (D_800D3294 == 0) {
            D_800C3EB4[i].alone = D_8006F8E5[i];
        } else {
            D_800C3EB4[i].alone = 1;
        }
        D_800C3EB4[i].flag5 = 0;
    }
    D_800C48EA = 0;
    D_800D2DC0 = 0;
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E4160);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E4870);

/* Derive the party's stats, then each slot's placed-alone flags and the
 * party members' panel states. */
#ifdef NON_MATCHING
void func_801E4AC0(void) {
    s32 i;

    func_80097D5C();
    if (D_800D3294 != 0) {
        func_8009B098();
    }
    for (i = 0; i < 3; i++) {
        if (D_800C3EB4[i].id != NO_COMBATANT) {
            D_800C3EA4->member_panel[i].state = 1;
            if (D_800C3EB4[i].alone != 0) {
                D_800D32A1[i].alone = 1;
                D_800CCCE8[i].state |= 0x80;
                if (D_800C3EB4[i].id != 7) {
                    D_800C3EA4->member_panel[i].state = 2;
                }
            } else {
                D_800D32A1[i].alone = 0;
                D_800CCCE8[i].state &= 0x7F;
            }
        } else {
            D_800D32A1[i].alone = 0;
            D_800CCCE8[i].state &= 0x7F;
            D_800C3EA4->member_panel[i].state = 0;
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB4[i].id != NO_COMBATANT && D_800C3EB4[i].alone != 0) {
            D_800D32A1[i].alone = 1;
        } else {
            D_800D32A1[i].alone = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        D_800D32A1[i].stat62 = D_800CCCE8[i].stat62;
        D_800D32A1[i].stat63 = D_800CCCE8[i].stat63;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E4AC0);
#endif

/* Build the battle item lists from the inventory (counts capped at 99, empty
 * slots cleared) and the special item list from ids 50..72. */
#ifdef NON_MATCHING
void func_801E4CD0(void) {
    s32 i;
    u8 *count;
    u8 listed;
    u32 id;

    for (i = 0; i < BATTLE_ITEMS; i++) {
        D_800D2CE0[i] = 0;
        D_800D2CB0[i] = 0;
        D_800D2FE4[i] = 0;
    }
    for (i = 0, count = D_8006F5C4; i < INVENTORY_SLOTS; i++) {
        if (*count >= 100) {
            *count = 99;
        }
        if (*count++ == 0) {
            D_8006F65A[i] = 0;
        }
    }
    listed = 0;
    for (i = 0; i < INVENTORY_SLOTS && listed < BATTLE_ITEMS; i++) {
        if (D_8006F65A[i] != 0 && D_8006F65A[i] < 49) {
            D_800D2CE0[listed] = D_8006F65A[i];
            D_800D2CB0[listed] = D_8006F5C4[i];
            D_800D2FE4[listed] = D_8006F65A[i];
            listed++;
        }
    }
    D_800C3EAC->last_item = 47;
    listed = 0;
    for (i = 0; i < 100; i++) {
        id = D_8006F3D0[i];
        if ((u32)(id - 50) < 23) {
            D_800C3D70[listed] = id;
            D_800D3688[listed] = D_8006F36C[i];
            listed++;
        }
    }
    for (; listed < BATTLE_ITEMS; listed++) {
        D_800C3D70[listed] = 0;
        D_800D3688[listed] = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E4CD0);
#endif

/* Start the turn timers, draw a random turn order of the eleven slots, let
 * enemies flagged 0x200 act first, then rebase every present timer so the
 * smallest becomes 1. */
void func_801E4E7C(void) {
    u8 drawn[SLOT_COUNT];
    s32 i;
    s32 least;
    s32 delta;
    u8 slot;

    D_800D2CAA = 0;
    func_80078508(drawn);
    i = 0;
    do {
        slot = func_8001BD40(0, 10);
        if (drawn[slot] == 0) {
            drawn[slot] = 1;
            D_800D2DCC.order[i] = slot;
            i++;
        }
    } while (i < SLOT_COUNT);
    D_800D2DCC.order_pos = 0;
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800D2DCC.present[i] != 0 && (D_800CCCE8[i].flags & 0x200)) {
            D_800D2DCC.timer_reset[i] = D_800D2DCC.timer[i] = 1;
        }
    }
    least = 0xFFFF;
    for (i = 0; i < SLOT_COUNT; i++) {
        if (D_800D2DCC.present[i] != 0) {
            if (D_800D2DCC.timer[i] < least) {
                least = D_800D2DCC.timer[i];
            }
        }
    }
    i = 0;
    delta = least - 1;
    for (; i < SLOT_COUNT; i++) {
        if (D_800D2DCC.present[i] != 0) {
            D_800D2DCC.timer[i] -= delta;
        }
    }
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E5014);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E5384);

/* Run one battle setup phase: 0 the party, 1 the formation and combatant
 * records, 2 the items, turn timers and turn tables, 3 the gauges and texts. */
void func_801E5840(u8 phase) {
    switch (phase) {
    case 0:
        func_801E5384();
        break;
    case 1:
        func_801E4048();
        func_801E4160();
        func_801E4870();
        func_801E4AC0();
        break;
    case 2:
        func_801E4CD0();
        func_801E4E7C();
        func_801E5014();
        D_800C3E24 = func_8008ABB8(0xEC, 0);
        func_8003F8E8(D_800C3E24, 0xEC);
        break;
    case 3:
        func_801E6290();
        func_801E62B8();
        break;
    }
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E5924);

/* Prepare the two white semi-transparent panel quads and their draw modes
 * (blend mode 2 on the effect texture page). */
void func_801E5D2C(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    for (i = 0; i < 2; i++) {
        func_80043C9C(&D_800C3EA4->panel[i]);
        (D_800C3EA4->panel + i)->r0 = 0xFF;
        (D_800C3EA4->panel + i)->g0 = 0xFF;
        (D_800C3EA4->panel + i)->b0 = 0xFF;
        func_80043BFC(&D_800C3EA4->panel[i], 1);
        func_800454DC(&D_800C3EA4->panel_mode[i], 0, 0,
                      func_80043A1C(0, 2, D_800C3EA4->tpage_x, D_800C3EA4->tpage_y), &window);
    }
    D_800C3EA4->panel6415 = 0;
    D_800C3EA4->panel_alpha = 0xFF;
    D_800C3EA4->panel6416 = 0;
}

/* Render the ten battle messages 0-9 into text images. */
void func_801E5E78(void) {
    s32 unused[2]; /* an 8-byte local the original frame reserves */
    s32 i;

    for (i = 0; i < 10; i++) {
        D_800C3E5C[i] = func_8008AC00(4);
        func_80034EAC(func_800338D8(i), D_800C3E5C[i], 2, 0);
    }
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E5EE8);

/* Setup phase 3, first frame: the gauge panels and each member's glyphs. */
void func_801E6290(void) {
    func_801E5924();
    func_801E5EE8();
}

/* Setup phase 3, second frame: the panel backdrops and the message images. */
void func_801E62B8(void) {
    func_801E5D2C();
    func_801E5E78();
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E62E0);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6314);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6710);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E67A4);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E693C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6A4C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6AC4);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6C80);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6D34);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6D6C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6DC8);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6E48);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6F00);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E6FEC);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E7098);

/* Relocate the stage image list and take the bounds of its pixel sections
 * (kind 0x1101: position, offset, size); returns the bounds' area. */
s32 func_801E70E8(s32 *images) {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
    s32 count;
    s32 i;
    u16 *p;
    s32 x;
    s32 y;
    s16 width;
    s16 height;

    func_8003342C(images);
    left = 0x800;
    top = 0x800;
    right = -0x800;
    bottom = -0x800;
    count = images[0];
    for (i = 0; i < count; i++) {
        p = (u16 *)images[i + 1];
        if (*p == 0x1101) {
            p += 2;
            x = *p++;
            y = *p++;
            x += *p++;
            y += *p++;
            if (x < left) {
                left = x;
            }
            if (y < top) {
                top = y;
            }
            x += p[0];
            y += p[1];
            if (right < x) {
                right = x;
            }
            if (bottom < y) {
                bottom = y;
            }
        }
    }
    width = right - left;
    height = bottom - top;
    D_800D2D30 = left;
    D_800D2D34 = top;
    D_800D2D2C = width;
    D_800C3EA8 = height;
    return width * height;
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E7210);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E7914);

/* Register the stage actors and light entries; clear each light's active
 * flag. Without lights both pointers are cleared. */
void func_801E7EC4(void *actors, StageLight *lights, s32 count) {
    s32 i;

    D_800D3344 = actors;
    D_800D39CC = lights;
    D_800D3348 = count;
    D_800D2F64 = 1;
    if (lights != NULL) {
        for (i = 0; i < D_800D3348; i++) {
            D_800D39CC[i].active = 0;
        }
    }
    if (count == 0) {
        D_800D3344 = NULL;
        D_800D39CC = NULL;
    }
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E7F4C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8088);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E80B4);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E827C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E82B0);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E82EC);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8320);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8588);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E893C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8964);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8A64);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8D48);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8D7C);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8DB8);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E8DF0);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E91E8);

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E9594);
