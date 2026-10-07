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
        D_80059468[i] = D_800CCCE8.party_ids[i];
        D_800C3EB4.slot[i].id = D_800CCCE8.party_ids[i];
    }
    for (i = 0; i < SLOT_COUNT; i++) {
        D_800C3EB4.slot[i].flag3 = 0;
        if (D_800D3294 == 0) {
            D_800C3EB4.slot[i].alone = D_8006F8E5[i];
        } else {
            D_800C3EB4.slot[i].alone = 1;
        }
        D_800C3EB4.slot[i].flag5 = 0;
    }
    D_800C48EA = 0;
    D_800D2DC0 = 0;
}

/* Place the formation: party and enemy presence, ids and groups from the
 * formation record, group membership and each member's standing position
 * from the battle scene data.
 * NON_MATCHING: the original indexes the formation record (D_8006F9DC) with
 * i itself; here loop optimization combines its repeated reads into reduced
 * pointers (and eliminates i), and the later loops' registers differ.
 * In the loop dump the two FORMATION_ID (and FLAGS3) reads are combined
 * givs (benefit 4) and reduced; the original keeps every byte-array access
 * on a register incremented with i (a0, like a mult-1 DEST_REG giv the
 * address givs were expressed from). */
#ifdef NON_MATCHING
void func_801E4160(void) {
    s32 i;
    u8 id;
    BattleScene *scene;

    D_800D3280 = 0;
    D_800D3364 = D_8005949C;
    D_800C3EB0.scene = D_8005949C;
    for (i = 0; i < 3; i++) {
        if ((D_800C3EB4.slot[i].id & 0x7F) != NO_COMBATANT) {
            D_800D2DCC.present[i] = 1;
            D_800D3280++;
        } else {
            D_800D2DCC.present[i] = 0;
        }
        if (D_800C3EB4.slot[i].alone == 0) {
            D_800C3EB4.slot[i].group = FORMATION_PARTY_GROUP(i) & 0x7F;
        } else {
            D_800C3EB4.slot[i].group = i;
        }
    }
    D_800D3280 += 0xFF; /* one less */
    for (i = 3; i < SLOT_COUNT; i++) {
        id = FORMATION_ID(i) & 0x7F;
        if (id != NO_COMBATANT) {
            D_800C3EB4.slot[i].id = id;
            D_800C3EB4.slot[i].flag3 = FORMATION_FLAGS3(i) & 0x80;
            D_800C3EB4.slot[i].alone = FORMATION_ID(i) & 0x80;
            D_800C3EB4.slot[i].flag5 = FORMATION_FLAGS3(i) & 1;
            D_800D2DCC.present[i] = 1;
            D_800C3EB4.slot[i].group = FORMATION_GROUP(i) & 0x7F;
        } else {
            D_800CCCE8.record[i].pos4E = 0;
            D_800CCCE8.record[i].pos4C = 0;
            D_800C3EB4.slot[i].id = NO_COMBATANT;
            D_800C3EB4.slot[i].flag3 = 0;
            D_800C3EB4.slot[i].alone = 0;
            D_800D2DCC.present[i] = 0;
        }
        D_800C3E3D[i] = D_800C3EB4.slot[i].id + 1;
    }
    for (i = 0; i < 32; i++) {
        D_800D301C[i].count = 0;
        D_800D301C[i].mask = 0;
    }
    for (i = 0; i < 3; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            if (D_800C3EB4.slot[i].alone == 0) {
                D_800C3EB4.slot[i].index = D_800D301C[D_800C3EB4.slot[i].group].count;
                D_800D301C[D_800C3EB4.slot[i].group].mask |= func_80089C08(D_800C3EB4.slot[i].index);
                D_800D301C[D_800C3EB4.slot[i].group].count++;
            } else {
                D_800C3EB4.slot[i].index = 0;
                D_800D301C[D_800C3EB4.slot[i].group + 16].mask = 1;
                D_800D301C[D_800C3EB4.slot[i].group + 16].count = 1;
            }
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            if (D_800C3EB4.slot[i].alone == 0) {
                D_800C3EB4.slot[i].index = D_800D301C[D_800C3EB4.slot[i].group + 8].count;
                D_800D301C[D_800C3EB4.slot[i].group + 8].mask |= func_80089C08(D_800C3EB4.slot[i].index);
                D_800D301C[D_800C3EB4.slot[i].group + 8].count++;
            } else {
                D_800C3EB4.slot[i].index = 0;
                D_800D301C[D_800C3EB4.slot[i].group + 24].mask = 1;
                D_800D301C[D_800C3EB4.slot[i].group + 24].count = 1;
            }
        }
    }
    scene = D_800D3364;
    for (i = 0; i < 3; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            if (D_800C3EB4.slot[i].alone == 0) {
                D_800C3EB4.slot[i].x =
                    scene->group[D_800C3EB4.slot[i].group].party[D_800C3EB4.slot[i].index].x;
                D_800C3EB4.slot[i].z =
                    scene->group[D_800C3EB4.slot[i].group].party[D_800C3EB4.slot[i].index].z;
            } else {
                D_800C3EB4.slot[i].x = scene->alone[D_800C3EB4.slot[i].group].party.x;
                D_800C3EB4.slot[i].z = scene->alone[D_800C3EB4.slot[i].group].party.z;
            }
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            if (D_800C3EB4.slot[i].alone == 0) {
                D_800C3EB4.slot[i].x =
                    D_800D3364->group[D_800C3EB4.slot[i].group].enemy[D_800C3EB4.slot[i].index].x;
                D_800C3EB4.slot[i].z =
                    D_800D3364->group[D_800C3EB4.slot[i].group].enemy[D_800C3EB4.slot[i].index].z;
            } else {
                D_800C3EB4.slot[i].x = D_800D3364->alone[D_800C3EB4.slot[i].group].enemy.x;
                D_800C3EB4.slot[i].z = D_800D3364->alone[D_800C3EB4.slot[i].group].enemy.z;
            }
            D_800C3EB4.slot[i].flag6 = FORMATION_FLAG6(i) & 0x80;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/ovl2615", func_801E4160);
#endif

/* Copy each present enemy's combatant record from the enemy data file and
 * point its AI state at its scripts (absent enemies are cleared). */
void func_801E4870(void) {
    u8 *records;
    u16 *scripts;
    s32 i;
    s32 j;

    D_800C3EB4.w48E8 = 0;
    D_800D39E0 = 0;
    records = D_800C3DD0 + 0x32;
    D_800C3DDC = D_800C3DD0 + ((u16 *)D_800C3DD0)[0x18];
    for (i = 3; i < SLOT_COUNT; i++) {
        D_800C3D0C.enemy[i - 3].b3 = 0;
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            memmove(&D_800CCCE8.record[i], records + D_800C3EB4.slot[i].id * sizeof(CombatantRecord),
                          sizeof(CombatantRecord));
            scripts = (u16 *)(D_800C3DD0 + ((u16 *)D_800C3DD0)[D_800C3EB4.slot[i].id]);
            D_800D3400[i - 3].main = (u8 *)scripts + scripts[0];
            D_800D3400[i - 3].sub = (u8 *)scripts + scripts[1];
            if (scripts[2] != 0xFFFF) {
                D_800D3400[i - 3].script = (u8 *)scripts + scripts[2];
                D_800C3D0C.enemy[i - 3].script_armed = 1;
            } else {
                D_800C3D0C.enemy[i - 3].script_armed = 0;
            }
            if (scripts[3] != 0xFFFF) {
                D_800D3400[i - 3].reaction = (u8 *)scripts + scripts[3];
                D_800C3D0C.enemy[i - 3].reaction_armed = 1;
            } else {
                D_800C3D0C.enemy[i - 3].reaction_armed = 0;
            }
            for (j = 3; j >= 0; j--) {
                D_800D3400[i - 3].vars[j] = 0;
            }
            for (j = 7; j >= 0; j--) {
                D_800D3400[i - 3].hvars[j] = 0;
            }
            for (j = 15; j >= 0; j--) {
                D_800D3400[i - 3].bvars[j] = 0;
            }
        } else {
            bzero(&D_800CCCE8.record[i], sizeof(CombatantRecord));
            D_800C3D0C.enemy[i - 3].script_armed = 0;
            D_800C3D0C.enemy[i - 3].reaction_armed = 0;
        }
    }
}

/* Derive the party's stats, then each slot's placed-alone flags and the
 * party members' panel states. */
void func_801E4AC0(void) {
    s32 i;

    func_80097D5C();
    if (D_800D3294 != 0) {
        func_8009B098();
    }
    for (i = 0; i < 3; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            D_800C3EA4->member_panel[i].state = 1;
            if (D_800C3EB4.slot[i].alone != 0) {
                D_800D32A1.party[i].alone = 1;
                D_800CCCE8.record[i].state |= 0x80;
                if (D_800C3EB4.slot[i].id != 7) {
                    D_800C3EA4->member_panel[i].state = 2;
                }
            } else {
                D_800D32A1.party[i].alone = 0;
                D_800CCCE8.record[i].state &= 0x7F;
            }
        } else {
            D_800D32A1.party[i].alone = 0;
            D_800CCCE8.record[i].state &= 0x7F;
            D_800C3EA4->member_panel[i].state = 0;
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            if (D_800C3EB4.slot[i].alone != 0) {
                D_800D32A1.enemy[i - 3].alone = 1;
            } else {
                D_800D32A1.enemy[i - 3].alone = 0;
            }
        } else {
            D_800D32A1.enemy[i - 3].alone = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        D_800D32A1.party[i].stat62 = D_800CCCE8.record[i].stat62;
        D_800D32A1.party[i].stat63 = D_800CCCE8.record[i].stat63;
    }
}

/* Compiled-out debug trace of an item entering a battle list. */
#define LIST_ITEM_TRACE(id) do { } while (0)

/* Enter item `id` into slot `n` of the battle item list `list`. */
#define LIST_ITEM(list, n, id)       \
    do {                             \
        LIST_ITEM_TRACE(id);         \
        (list)[n] = (id);            \
    } while (0)

/* Build the battle item lists from the inventory (counts capped at 99, empty
 * slots cleared) and the special item list from ids 50..72. */
void func_801E4CD0(void) {
    s32 i;
    u8 *count;
    u8 listed;

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
            LIST_ITEM(D_800D2CE0, listed, D_8006F65A[i]);
            D_800D2CB0[listed] = D_8006F5C4[i];
            D_800D2FE4[listed] = D_8006F65A[i];
            listed++;
        }
    }
    D_800C3EAC->last_item = 47;
    for (i = 0, listed = 0; i < 100; i++) {
        if (D_8006F3D0[i] >= 50 && D_8006F3D0[i] < 73) {
            LIST_ITEM(D_800C3D70, listed, D_8006F3D0[i]);
            D_800D3688[listed] = D_8006F36C[i];
            listed++;
        }
    }
    for (; listed < BATTLE_ITEMS; listed++) {
        D_800C3D70[listed] = 0;
        D_800D3688[listed] = 0;
    }
}

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
        if (D_800D2DCC.present[i] != 0 && (D_800CCCE8.record[i].flags & 0x200)) {
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

/* Set up each slot's command menu: the party's layouts, menu sources and
 * command masks (commands 7 and 8 forced by the formation), and every slot's
 * default target and facing. */
void func_801E5014(void) {
    s32 i;
    s32 j;
    u8 k;

    if (FORMATION_FLAGS & 0x20) {
        D_800C3D48 = 1;
    }
    for (i = 0; i < 3; i++) {
        D_800D32A1.party[i].character_b = D_8006ED0B[D_800CCCE8.party_ids[i]][0];
        for (j = 0; j < 8; j++) {
            D_800C3EAC->slot[i].layout[j] = D_800C20F0[D_800CCCE8.record[i].menu_layout][j];
        }
        for (j = 0; j < 4; j++) {
            if (D_800CCCE8.party_ids[i] != 7) {
                D_800C3EAC->slot[i].menu8[j] = D_800C2130[j];
            } else {
                D_800C3EAC->slot[i].menu8[j] = D_800C2138[j];
            }
            D_800C3EAC->slot[i].menuC[j] = D_800C2134[j];
        }
        D_800C3EAC->slot[i].target = func_800841E0(i);
        D_800C3EB4.slot[i].flag6 = func_80085310(i, D_800C3EAC->slot[i].target);
        for (k = 0; k < 16; k++) {
            D_800C3EAC->slot[i].commands[k] = D_800CCCE8.record[i].commands & D_800C3234[k];
        }
        if (D_800CCCE8.record[i].bA0 == 0xFF || (FORMATION_FLAGS & 0x40)) {
            D_800C3EAC->slot[i].commands[7] = D_800C3234[7];
            D_800CCCE8.record[i].commands |= D_800C3234[7];
        }
        if (FORMATION_FLAGS & 0x80) {
            D_800C3EAC->slot[i].commands[8] = D_800C3234[8];
            D_800CCCE8.record[i].commands |= D_800C3234[8];
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        D_800C3EAC->slot[i].target = func_800841E0(i);
        D_800C3EB4.slot[i].flag6 = func_80085310(i, D_800C3EAC->slot[i].target);
    }
}

/* Setup phase 0: choose the party (or the demo party), copy each member's
 * character and gear records and battle data from the setup archive, load
 * its images and tables, then start reading the formation's enemy files. */
void func_801E5384(void) {
    s32 count;
    s32 i;
    u32 *archive;
    void *block;
    u16 mask;
    u8 gear;
    u16 *list;

    if (FORMATION_FLAGS & 0x10) {
        D_800D3294 = 1;
    } else {
        D_800D3294 = 0;
    }
    mask = (D_8006F364.available & D_8006F364.available2) & 0x7FF;
    if (D_800D3294 == 0) {
        count = 0;
        for (i = 0; i < 3; i++) {
            if (func_80089C9C(mask, D_8006F364.party[i])) {
                D_800CCCE8.party_ids[count] = D_8006F364.party[i] & 0x7F;
                count++;
            }
        }
        for (; count < 3; count++) {
            D_800CCCE8.party_ids[count] = NO_COMBATANT;
        }
    } else {
        D_800CCCE8.party_ids[1] = 10;
        D_800CCCE8.party_ids[2] = 10;
        D_800CCCE8.party_ids[0] = D_8006F364.party[0] & 0x7F;
    }
    func_8003342C(D_800595A8);
    archive = D_800595A8;
    for (i = 0; i < 3; i++) {
        if (D_800CCCE8.party_ids[i] != NO_COMBATANT) {
            memmove(&D_800CCCE8.record[i], D_8006D8A0[D_800CCCE8.party_ids[i]], 0xA4);
            if (D_800D3294 != 0 && (u32)(i - 1) < 2) {
                D_800CCCE8.record[i].bA0 = 0x11;
            }
            gear = D_800CCCE8.record[i].bA0;
            if (gear == 0xFF) {
                gear = 0;
            }
            memmove(D_800CCCE8.record[i].gear, D_8006DFAC[gear], 0xA4);
            block = func_80032E88(archive[5 + D_800CCCE8.party_ids[i]], 1);
            memmove(D_800CCCE8.member_data[i], block, 0x5F0);
            func_800320E8(block);
            block = func_80032E88(archive[0x11 + gear], 1);
            memmove(D_800CCCE8.member_gear[i], block, 0x690);
            func_800320E8(block);
        }
    }
    block = func_80032E88(archive[4], 1);
    memmove(D_800CCCE8.data35D8, block, 0x1F40);
    func_800320E8(block);
    block = func_80032E88(archive[3], 1);
    memmove(D_800CCCE8.data5518, block, 0x300);
    func_800320E8(block);
    block = func_80032E88(archive[2], 1);
    func_8002DDE4(block, 0, 0, 0, 0, 0, 0);
    func_800320E8(block);
    D_800D2F5C = func_80032E88(archive[1], 0);
    func_80033698(0, 0x1F0);
    D_800D329C = func_80032E88(archive[0x10], 0);
    block = func_80032E88(archive[0x24], 1);
    func_80078310(block, 0x61);
    func_800320E8(block);
    block = func_80032E88(archive[0x25], 1);
    memmove(D_800CCCE8.data5818, (u8 *)block + 0x320, 0x300);
    func_800320E8(block);
    D_800D39F0 = func_80032E88(archive[0x26], 0);
    func_800320E8(D_800595A8);
    func_80028470(0xC, 1);
    D_800C3DD0 = func_8008ABB8(func_800288EC(D_8006F9DC[0] * 2 + 2), 0);
    D_800D33EC = D_800C3DD0;
    list = &D_800D33E8;
    *list = D_8006F9DC[0] * 2 + 2;
    D_800C3DEC = func_8008ABB8(func_800288EC(D_8006F9DC[0] * 2 + 3), 1);
    D_800D33F4 = D_800C3DEC;
    D_800D33F8 = 0;
    D_800D33FC = NULL;
    D_800D33F0 = D_8006F9DC[0] * 2 + 3;
    func_80029AFC(list, 0, 0x80);
}

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
        bzero(D_800C3E24, 0xEC);
        break;
    case 3:
        func_801E6290();
        func_801E62B8();
        break;
    }
}

/* Prepare the ATB gauge primitives: textured bars lit at the top, grey
 * shades and white lines, on the gauge glyph's texture page and palettes. */
void func_801E5924(void) {
    s32 i;

    D_800D2D28->gauge_shown[0] = 1;
    D_800D2D28->gauge_shown[1] = 1;
    D_800D2D28->gauge_shown[2] = 1;
    func_80026338(D_800D2F5C, 0x5C, &D_800C3EA4->glyph_a234, &D_800C3EA4->tpage_tp,
                  &D_800C3EA4->clut_x, &D_800C3EA4->clut_y, &D_800C3EA4->tpage_x,
                  &D_800C3EA4->tpage_y);
    D_800C3EA4->gauge_clut[1] = GetClut(D_800C3EA4->clut_x, D_800C3EA4->clut_y);
    D_800C3EA4->gauge_clut[0] = GetClut(D_800C3EA4->clut_x, D_800C3EA4->clut_y - 1);
    D_800C3EA4->gauge_clut[3] = GetClut(D_800C3EA4->clut_x, D_800C3EA4->clut_y - 2);
    D_800C3EA4->gauge_clut[2] = GetClut(D_800C3EA4->clut_x, D_800C3EA4->clut_y - 3);
    for (i = 0; i < 8; i++) {
        SetPolyGT4(&D_800C3EA4->gauge[i]);
        SetShadeTex(&D_800C3EA4->gauge[i], 0);
        (D_800C3EA4->gauge + i)->r0 = 0x80;
        (D_800C3EA4->gauge + i)->g0 = 0x80;
        (D_800C3EA4->gauge + i)->b0 = 0x80;
        (D_800C3EA4->gauge + i)->r1 = 0x80;
        (D_800C3EA4->gauge + i)->g1 = 0x80;
        (D_800C3EA4->gauge + i)->b1 = 0x80;
        (D_800C3EA4->gauge + i)->r2 = 0;
        (D_800C3EA4->gauge + i)->g2 = 0;
        (D_800C3EA4->gauge + i)->b2 = 0;
        (D_800C3EA4->gauge + i)->r3 = 0;
        (D_800C3EA4->gauge + i)->g3 = 0;
        (D_800C3EA4->gauge + i)->b3 = 0;
        D_800C3EA4->gauge[i].tpage =
            GetTPage(D_800C3EA4->tpage_tp, 0, D_800C3EA4->tpage_x, D_800C3EA4->tpage_y);
        SetPolyG4(&D_800C3EA4->gauge_shade[i]);
        (D_800C3EA4->gauge_shade + i)->r2 = 0x4F;
        (D_800C3EA4->gauge_shade + i)->g2 = 0x4F;
        (D_800C3EA4->gauge_shade + i)->b2 = 0x4F;
        (D_800C3EA4->gauge_shade + i)->r3 = 0x4F;
        (D_800C3EA4->gauge_shade + i)->g3 = 0x4F;
        (D_800C3EA4->gauge_shade + i)->b3 = 0x4F;
    }
    for (i = 0; i < 12; i++) {
        SetLineF2(&D_800C3EA4->gauge_line[i]);
        (D_800C3EA4->gauge_line + i)->r0 = 0xFF;
        (D_800C3EA4->gauge_line + i)->g0 = 0xFF;
        (D_800C3EA4->gauge_line + i)->b0 = 0xFF;
    }
}

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
        SetPolyF4(&D_800C3EA4->panel[i]);
        (D_800C3EA4->panel + i)->r0 = 0xFF;
        (D_800C3EA4->panel + i)->g0 = 0xFF;
        (D_800C3EA4->panel + i)->b0 = 0xFF;
        SetSemiTrans(&D_800C3EA4->panel[i], 1);
        SetDrawMode(&D_800C3EA4->panel_mode[i], 0, 0,
                      GetTPage(0, 2, D_800C3EA4->tpage_x, D_800C3EA4->tpage_y), &window);
    }
    D_800C3EA4->panel6415 = 0;
    D_800C3EA4->panel_alpha = 0xFF;
    D_800C3EA4->panel6416 = 0;
}

/* Render the ten battle messages 0-9 into text images. The original frame
 * reserves eight bytes that no instruction touches. */
void func_801E5E78(void) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    s32 i;

    for (i = 0; i < 10; i++) {
        D_800C3E5C[i] = func_8008AC00(4);
        func_80034EAC(func_800338D8(i), D_800C3E5C[i], 2, 0);
    }
}

/* Draw each present member's gauge glyphs (the second set dimmed), its
 * portrait and its two digit glyphs, placed by the party layout's columns. */
void func_801E5EE8(void) {
    s32 i;
    s32 part;
    s32 first; /* first dimmed glyph: two prims per part */
    s32 k;

    for (i = 0; i < 3; i++) {
        if (D_800C3EB4.slot[i].id != NO_COMBATANT) {
            D_800D2D28->gauge_parts[i] += func_80076A6C(
                0x52, &D_800C3EA4->member_gauge[i].prim[D_800D2D28->gauge_parts[i] * 2],
                i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x44), 0x24);
            part = D_800D2D28->gauge_parts[i];
            first = part * 2;
            D_800D2D28->gauge_parts[i] += func_80076A6C(
                0x53, &D_800C3EA4->member_gauge[i].prim[part * 2],
                i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x44), 0x24);
            for (k = first; k < D_800D2D28->gauge_parts[i] * 2; k += 2) {
                func_80076C34(&D_800C3EA4->member_gauge[i].prim[k + D_800CCB34]);
            }
            func_80076A10(0x61 + i, D_800C3EA4->portrait[i],
                          i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x1C), 0x14);
            D_800C3EA4->member_panel[i].digit_parts[0] =
                func_80076A10(0x90, D_800C3EA4->member_panel[i].digit[0],
                              i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x38), 0x27);
            D_800C3EA4->member_panel[i].digit_parts[1] =
                func_80076A10(0x91, D_800C3EA4->member_panel[i].digit[1],
                              i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x3C), 0x27);
            D_800C3EA4->member_panel[i].buffer = D_800CCB34;
        }
    }
    D_800D2D28->bA2 = D_800CCB34;
    D_800D2D28->b83 = D_800CCB34;
}

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
