/* Battle setup module (overlay slot 2615, directory 12 file 4, loaded at
 * 0x801E4000 by the resident battle entry 8001bbac together with the effect
 * header and archive files 2 and 3). It places the formation, builds the
 * combatant records and turn tables, sets up the stage, gauges and intro,
 * and runs as the battle's setup task (801e5840 phases, 801e7098 task).
 *
 * The image holds five units: this one (text 801E4048-801E62E0, GCC 2.6.3),
 * battle_loader.c, stage.c, load_modes.c and burst_modes.c. The change to
 * the Cygnus CDK GCC 2.7.2 and its later ASPSX (positive li as addiu) at
 * 801e62e0 ends this unit (ovl2615.mk). */
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
        D_80059468[i] = D_800CCCE8_setup.partyIds[i];
        D_800C3EB0.slots[i].field2 = D_800CCCE8_setup.partyIds[i];
    }
    for (i = 0; i < SLOT_COUNT; i++) {
        D_800C3EB0.slots[i].hidden = 0;
        if (D_800D3294 == 0) {
            D_800C3EB0.slots[i].gear = D_8006F8E5[i];
        } else {
            D_800C3EB0.slots[i].gear = 1;
        }
        D_800C3EB0.slots[i].field5 = 0;
    }
    D_800C3EB0.outcome = 0;
    D_800D2DC0 = 0;
}

/* Place the formation: party and enemy presence, ids and groups from the
 * formation record, group membership and each member's standing position
 * from the battle scene data. The eight enemies take slots 3-10. */
void func_801E4160(void) {
    s32 i;
    u8 id;

    D_800D3280 = 0;
    D_800D3364 = (BattleScene *)D_8005949C;
    D_800C3EB0.formation = (struct Formation *)D_8005949C;
    for (i = 0; i < 3; i++) {
        if ((D_800C3EB0.slots[i].field2 & 0x7F) != NO_COMBATANT) {
            D_800D2DCC.present[i] = 1;
            D_800D3280++;
        } else {
            D_800D2DCC.present[i] = 0;
        }
        if (D_800C3EB0.slots[i].gear == 0) {
            D_800C3EB0.slots[i].group = FORMATION_PARTY_GROUP(i) & 0x7F;
        } else {
            D_800C3EB0.slots[i].group = i;
        }
    }
    D_800D3280 += 0xFF; /* one less */
    for (i = 0; i < 8; i++) {
        id = FORMATION_ENEMY_ID(i) & 0x7F;
        if (id != NO_COMBATANT) {
            D_800C3EB0.slots[i + 3].field2 = id;
            D_800C3EB0.slots[i + 3].hidden = FORMATION_ENEMY_FLAGS(i) & 0x80;
            D_800C3EB0.slots[i + 3].gear = FORMATION_ENEMY_ID(i) & 0x80;
            D_800C3EB0.slots[i + 3].field5 = FORMATION_ENEMY_FLAGS(i) & 1;
            D_800D2DCC.present[i + 3] = 1;
            D_800C3EB0.slots[i + 3].group = FORMATION_ENEMY_GROUP(i) & 0x7F;
        } else {
            D_800CCCE8_setup.work.records[i + 3].pilot.maxHp = 0;
            D_800CCCE8_setup.work.records[i + 3].pilot.hp = 0;
            D_800C3EB0.slots[i + 3].field2 = NO_COMBATANT;
            D_800C3EB0.slots[i + 3].hidden = 0;
            D_800C3EB0.slots[i + 3].gear = 0;
            D_800D2DCC.present[i + 3] = 0;
        }
        D_800C3E3D[i + 3] = D_800C3EB0.slots[i + 3].field2 + 1;
    }
    for (i = 0; i < 32; i++) {
        D_800D301C[i].count = 0;
        D_800D301C[i].mask = 0;
    }
    for (i = 0; i < 3; i++) {
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            if (D_800C3EB0.slots[i].gear == 0) {
                D_800C3EB0.slots[i].member = D_800D301C[D_800C3EB0.slots[i].group].count;
                D_800D301C[D_800C3EB0.slots[i].group].mask |= func_80089C08(D_800C3EB0.slots[i].member);
                D_800D301C[D_800C3EB0.slots[i].group].count++;
            } else {
                D_800C3EB0.slots[i].member = 0;
                D_800D301C[D_800C3EB0.slots[i].group + 16].mask = 1;
                D_800D301C[D_800C3EB0.slots[i].group + 16].count = 1;
            }
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            if (D_800C3EB0.slots[i].gear == 0) {
                D_800C3EB0.slots[i].member = D_800D301C[D_800C3EB0.slots[i].group + 8].count;
                D_800D301C[D_800C3EB0.slots[i].group + 8].mask |= func_80089C08(D_800C3EB0.slots[i].member);
                D_800D301C[D_800C3EB0.slots[i].group + 8].count++;
            } else {
                D_800C3EB0.slots[i].member = 0;
                D_800D301C[D_800C3EB0.slots[i].group + 24].mask = 1;
                D_800D301C[D_800C3EB0.slots[i].group + 24].count = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            if (D_800C3EB0.slots[i].gear == 0) {
                D_800C3EB0.slots[i].x =
                    D_800D3364->group[D_800C3EB0.slots[i].group].party[D_800C3EB0.slots[i].member].x;
                D_800C3EB0.slots[i].z =
                    D_800D3364->group[D_800C3EB0.slots[i].group].party[D_800C3EB0.slots[i].member].z;
            } else {
                D_800C3EB0.slots[i].x = D_800D3364->gear[D_800C3EB0.slots[i].group].party.x;
                D_800C3EB0.slots[i].z = D_800D3364->gear[D_800C3EB0.slots[i].group].party.z;
            }
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            if (D_800C3EB0.slots[i].gear == 0) {
                D_800C3EB0.slots[i].x =
                    D_800D3364->group[D_800C3EB0.slots[i].group].enemy[D_800C3EB0.slots[i].member].x;
                D_800C3EB0.slots[i].z =
                    D_800D3364->group[D_800C3EB0.slots[i].group].enemy[D_800C3EB0.slots[i].member].z;
                D_800C3EB0.slots[i].targetCode = FORMATION_FLAG6(i) & 0x80;
            } else {
                D_800C3EB0.slots[i].x = D_800D3364->gear[D_800C3EB0.slots[i].group].enemy.x;
                D_800C3EB0.slots[i].z = D_800D3364->gear[D_800C3EB0.slots[i].group].enemy.z;
                D_800C3EB0.slots[i].targetCode = FORMATION_FLAG6(i) & 0x80;
            }
        }
    }
}

/* Copy each present enemy's combatant record from the enemy data file and
 * point its AI state at its scripts (absent enemies are cleared). */
void func_801E4870(void) {
    u8 *records;
    u16 *scripts;
    s32 i;
    s32 j;

    D_800C3EB0.knockedOut = 0;
    D_800D39E0 = 0;
    records = D_800C3DD0 + 0x32;
    D_800C3DDC = D_800C3DD0 + ((u16 *)D_800C3DD0)[0x18];
    for (i = 3; i < SLOT_COUNT; i++) {
        D_800C3D18[i - 3].unk3 = 0;
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            memmove(&D_800CCCE8_setup.work.records[i], records + D_800C3EB0.slots[i].field2 * sizeof(Combatant),
                          sizeof(Combatant));
            scripts = (u16 *)(D_800C3DD0 + ((u16 *)D_800C3DD0)[D_800C3EB0.slots[i].field2]);
            D_800D3400[i - 3].script = (u8 *)scripts + scripts[0];
            D_800D3400[i - 3].unk4 = (u8 *)scripts + scripts[1];
            if (scripts[2] != 0xFFFF) {
                D_800D3400[i - 3].reaction = (u8 *)scripts + scripts[2];
                D_800C3D18[i - 3].armed = 1;
            } else {
                D_800C3D18[i - 3].armed = 0;
            }
            if (scripts[3] != 0xFFFF) {
                D_800D3400[i - 3].turnScript = (u8 *)scripts + scripts[3];
                D_800C3D18[i - 3].unk1[0] = 1;
            } else {
                D_800C3D18[i - 3].unk1[0] = 0;
            }
            for (j = 3; j >= 0; j--) {
                D_800D3400[i - 3].longs[j] = 0;
            }
            for (j = 7; j >= 0; j--) {
                D_800D3400[i - 3].vars[j] = 0;
            }
            for (j = 15; j >= 0; j--) {
                D_800D3400[i - 3].bytes[j] = 0;
            }
        } else {
            bzero((u8 *)&D_800CCCE8_setup.work.records[i], sizeof(Combatant));
            D_800C3D18[i - 3].armed = 0;
            D_800C3D18[i - 3].unk1[0] = 0;
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
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            D_800C3EA4->panels[i].state = 1;
            if (D_800C3EB0.slots[i].gear != 0) {
                D_800D32A1.party[i].in_gear = 1;
                D_800CCCE8_setup.work.records[i].flags15A |= 0x80;
                if (D_800C3EB0.slots[i].field2 != 7) {
                    D_800C3EA4->panels[i].state = 2;
                }
            } else {
                D_800D32A1.party[i].in_gear = 0;
                D_800CCCE8_setup.work.records[i].flags15A &= 0x7F;
            }
        } else {
            D_800D32A1.party[i].in_gear = 0;
            D_800CCCE8_setup.work.records[i].flags15A &= 0x7F;
            D_800C3EA4->panels[i].state = 0;
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            if (D_800C3EB0.slots[i].gear != 0) {
                D_800D32A1.enemy[i - 3].in_gear = 1;
            } else {
                D_800D32A1.enemy[i - 3].in_gear = 0;
            }
        } else {
            D_800D32A1.enemy[i - 3].in_gear = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        D_800D32A1.party[i].stat62 = D_800CCCE8_setup.work.records[i].pilot.level;
        D_800D32A1.party[i].stat63 = D_800CCCE8_setup.work.records[i].pilot.level2;
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
    for (i = 0, count = D_8006D634.itemCounts; i < INVENTORY_SLOTS; i++) {
        if (*count >= 100) {
            *count = 99;
        }
        if (*count++ == 0) {
            D_8006D634.itemIds[i] = 0;
        }
    }
    listed = 0;
    for (i = 0; i < INVENTORY_SLOTS && listed < BATTLE_ITEMS; i++) {
        if (D_8006D634.itemIds[i] != 0 && D_8006D634.itemIds[i] < 49) {
            LIST_ITEM(D_800D2CE0, listed, D_8006D634.itemIds[i]);
            D_800D2CB0[listed] = D_8006D634.itemCounts[i];
            D_800D2FE4[listed] = D_8006D634.itemIds[i];
            listed++;
        }
    }
    D_800C3EAC->lastItem = 47;
    for (i = 0, listed = 0; i < 100; i++) {
        if (D_8006D634.weaponIds[i] >= 50 && D_8006D634.weaponIds[i] < 73) {
            LIST_ITEM(D_800C3D70, listed, D_8006D634.weaponIds[i]);
            D_800D3688[listed] = D_8006D634.weaponCounts[i];
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
    D_800D2DCC.cursor = 0;
    for (i = 3; i < SLOT_COUNT; i++) {
        if (D_800D2DCC.present[i] != 0 && (D_800CCCE8_setup.work.records[i].pilot.flags34 & 0x200)) {
            D_800D2DCC.timers[0][i] = D_800D2DCC.timers[1][i] = 1;
        }
    }
    least = 0xFFFF;
    for (i = 0; i < SLOT_COUNT; i++) {
        if (D_800D2DCC.present[i] != 0) {
            if (D_800D2DCC.timers[1][i] < least) {
                least = D_800D2DCC.timers[1][i];
            }
        }
    }
    i = 0;
    delta = least - 1;
    for (; i < SLOT_COUNT; i++) {
        if (D_800D2DCC.present[i] != 0) {
            D_800D2DCC.timers[1][i] -= delta;
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
        D_800D32A1.party[i].character_b = D_8006D634.skills[D_800CCCE8_setup.partyIds[i]].tier;
        for (j = 0; j < 8; j++) {
            D_800C3EAC->slots[i].layout[j] = D_800C20F0[D_800CCCE8_setup.work.records[i].pilot.characterId][j];
        }
        for (j = 0; j < 4; j++) {
            if (D_800CCCE8_setup.partyIds[i] != 7) {
                D_800C3EAC->slots[i].digits[0][j] = D_800C2130[j];
            } else {
                D_800C3EAC->slots[i].digits[0][j] = D_800C2138[j];
            }
            D_800C3EAC->slots[i].digits[1][j] = D_800C2134[j];
        }
        D_800C3EAC->slots[i].defaultTarget = func_800841E0(i);
        D_800C3EB0.slots[i].targetCode = func_80085310(i, D_800C3EAC->slots[i].defaultTarget);
        for (k = 0; k < 16; k++) {
            D_800C3EAC->slots[i].items[k] = D_800CCCE8_setup.work.records[i].pilot.status7A & D_800C3234[k];
        }
        if (D_800CCCE8_setup.work.records[i].pilot.gearId == 0xFF || (FORMATION_FLAGS & 0x40)) {
            D_800C3EAC->slots[i].items[7] = D_800C3234[7];
            D_800CCCE8_setup.work.records[i].pilot.status7A |= D_800C3234[7];
        }
        if (FORMATION_FLAGS & 0x80) {
            D_800C3EAC->slots[i].items[8] = D_800C3234[8];
            D_800CCCE8_setup.work.records[i].pilot.status7A |= D_800C3234[8];
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        D_800C3EAC->slots[i].defaultTarget = func_800841E0(i);
        D_800C3EB0.slots[i].targetCode = func_80085310(i, D_800C3EAC->slots[i].defaultTarget);
    }
}

/* Setup phase 0: choose the party (or the demo party), copy each member's
 * character and gear records and battle data from the setup archive, load
 * its images and tables, then start reading the formation's enemy files. */
void func_801E5384(void) {
    s32 count;
    s32 i;
    void **archive;
    void *block;
    u16 mask;
    u8 gear;
    u16 *list;

    if (FORMATION_FLAGS & 0x10) {
        D_800D3294 = 1;
    } else {
        D_800D3294 = 0;
    }
    mask = (D_8006D634.joined & D_8006D634.available) & 0x7FF;
    if (D_800D3294 == 0) {
        count = 0;
        for (i = 0; i < 3; i++) {
            if (func_80089C9C(mask, D_8006D634.party[i])) {
                D_800CCCE8_setup.partyIds[count] = D_8006D634.party[i] & 0x7F;
                count++;
            }
        }
        for (; count < 3; count++) {
            D_800CCCE8_setup.partyIds[count] = NO_COMBATANT;
        }
    } else {
        D_800CCCE8_setup.partyIds[1] = 10;
        D_800CCCE8_setup.partyIds[2] = 10;
        D_800CCCE8_setup.partyIds[0] = D_8006D634.party[0] & 0x7F;
    }
    func_8003342C(D_800595A8);
    archive = D_800595A8;
    for (i = 0; i < 3; i++) {
        if (D_800CCCE8_setup.partyIds[i] != NO_COMBATANT) {
            memmove(&D_800CCCE8_setup.work.records[i].pilot, &D_8006D634.characters[D_800CCCE8_setup.partyIds[i]], 0xA4);
            if (D_800D3294 != 0 && (u32)(i - 1) < 2) {
                D_800CCCE8_setup.work.records[i].pilot.gearId = 0x11;
            }
            gear = D_800CCCE8_setup.work.records[i].pilot.gearId;
            if (gear == 0xFF) {
                gear = 0;
            }
            memmove(&D_800CCCE8_setup.work.records[i].gear, &D_8006D634.gears[gear], 0xA4);
            block = func_80032E88(archive[5 + D_800CCCE8_setup.partyIds[i]], 1);
            memmove(D_800CCCE8_setup.work.partyCommands[i], block, 0x5F0);
            func_800320E8(block);
            block = func_80032E88(archive[0x11 + gear], 1);
            memmove(D_800CCCE8_setup.work.gearCommands[i], block, 0x690);
            func_800320E8(block);
        }
    }
    /* The setup archive fills the work area from the enemy command table
     * (0x35D8, 0x1F40 bytes of item 4) to 0x5B18 (items 3 and 0x25 at 0x5518
     * and 0x5818). */
    block = func_80032E88(archive[4], 1);
    memmove(D_800CCCE8_setup.work.enemyCommands, block, 0x1F40);
    func_800320E8(block);
    block = func_80032E88(archive[3], 1);
    memmove((u8 *)&D_800CCCE8_setup + 0x5518, block, 0x300);
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
    memmove((u8 *)&D_800CCCE8_setup + 0x5818, (u8 *)block + 0x320, 0x300);
    func_800320E8(block);
    D_800D39F0 = func_80032E88(archive[0x26], 0);
    func_800320E8(D_800595A8);
    func_80028470(0xC, 1);
    D_800C3DD0 = func_8008ABB8(func_800288EC(D_8006F9DC.battle * 2 + 2), 0);
    D_800D33EC = D_800C3DD0;
    list = &D_800D33E8;
    *list = D_8006F9DC.battle * 2 + 2;
    D_800C3DEC = (s32)func_8008ABB8(func_800288EC(D_8006F9DC.battle * 2 + 3), 1);
    D_800D33F4 = (void *)D_800C3DEC;
    D_800D33F8 = 0;
    D_800D33FC = NULL;
    D_800D33F0 = D_8006F9DC.battle * 2 + 3;
    func_80029AFC((FileRequest *)list, 0, 0x80);
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
        bzero((u8 *)D_800C3E24, 0xEC);
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

    D_800D2D28->reaction[0] = 1;
    D_800D2D28->reaction[1] = 1;
    D_800D2D28->reaction[2] = 1;
    func_80026338(D_800D2F5C, 0x5C, &D_800C3EA4->sprites[0].unk0, &D_800C3EA4->sprites[0].tpageMode,
                  &D_800C3EA4->sprites[0].clutX, &D_800C3EA4->sprites[0].clutY, &D_800C3EA4->sprites[0].pageX,
                  &D_800C3EA4->sprites[0].pageY);
    D_800C3EA4->barCluts[1] = GetClut(D_800C3EA4->sprites[0].clutX, D_800C3EA4->sprites[0].clutY);
    D_800C3EA4->barCluts[0] = GetClut(D_800C3EA4->sprites[0].clutX, D_800C3EA4->sprites[0].clutY - 1);
    D_800C3EA4->barCluts[3] = GetClut(D_800C3EA4->sprites[0].clutX, D_800C3EA4->sprites[0].clutY - 2);
    D_800C3EA4->barCluts[2] = GetClut(D_800C3EA4->sprites[0].clutX, D_800C3EA4->sprites[0].clutY - 3);
    for (i = 0; i < 8; i++) {
        SetPolyGT4(&D_800C3EA4->gaugeBars[i]);
        SetShadeTex(&D_800C3EA4->gaugeBars[i], 0);
        (D_800C3EA4->gaugeBars + i)->r0 = 0x80;
        (D_800C3EA4->gaugeBars + i)->g0 = 0x80;
        (D_800C3EA4->gaugeBars + i)->b0 = 0x80;
        (D_800C3EA4->gaugeBars + i)->r1 = 0x80;
        (D_800C3EA4->gaugeBars + i)->g1 = 0x80;
        (D_800C3EA4->gaugeBars + i)->b1 = 0x80;
        (D_800C3EA4->gaugeBars + i)->r2 = 0;
        (D_800C3EA4->gaugeBars + i)->g2 = 0;
        (D_800C3EA4->gaugeBars + i)->b2 = 0;
        (D_800C3EA4->gaugeBars + i)->r3 = 0;
        (D_800C3EA4->gaugeBars + i)->g3 = 0;
        (D_800C3EA4->gaugeBars + i)->b3 = 0;
        D_800C3EA4->gaugeBars[i].tpage =
            GetTPage(D_800C3EA4->sprites[0].tpageMode, 0, D_800C3EA4->sprites[0].pageX, D_800C3EA4->sprites[0].pageY);
        SetPolyG4(&D_800C3EA4->shade[i]);
        (D_800C3EA4->shade + i)->r2 = 0x4F;
        (D_800C3EA4->shade + i)->g2 = 0x4F;
        (D_800C3EA4->shade + i)->b2 = 0x4F;
        (D_800C3EA4->shade + i)->r3 = 0x4F;
        (D_800C3EA4->shade + i)->g3 = 0x4F;
        (D_800C3EA4->shade + i)->b3 = 0x4F;
    }
    for (i = 0; i < 12; i++) {
        SetLineF2(&D_800C3EA4->unk908[i]);
        (D_800C3EA4->unk908 + i)->r0 = 0xFF;
        (D_800C3EA4->unk908 + i)->g0 = 0xFF;
        (D_800C3EA4->unk908 + i)->b0 = 0xFF;
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
        SetDrawMode(&D_800C3EA4->panelMode[i], 0, 0,
                      GetTPage(0, 2, D_800C3EA4->sprites[0].pageX, D_800C3EA4->sprites[0].pageY), &window);
    }
    D_800C3EA4->unk6415 = 0;
    D_800C3EA4->panelAlpha = 0xFF;
    D_800C3EA4->unk6416 = 0;
}

/* Render the ten battle messages 0-9 into text images. The original frame
 * reserves eight bytes that no instruction touches. */
void func_801E5E78(void) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    s32 i;

    for (i = 0; i < 10; i++) {
        D_800C3E5C[i].pixels = func_8008AC00(4);
        func_80034EAC(func_800338D8(i), D_800C3E5C[i].pixels, 2, 0);
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
        if (D_800C3EB0.slots[i].field2 != NO_COMBATANT) {
            D_800D2D28->gaugeParts[i] += func_80076A6C(
                0x52, &D_800C3EA4->gauge[i][0][D_800D2D28->gaugeParts[i] * 2],
                i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x44), 0x24);
            part = D_800D2D28->gaugeParts[i];
            first = part * 2;
            D_800D2D28->gaugeParts[i] += func_80076A6C(
                0x53, &D_800C3EA4->gauge[i][0][part * 2],
                i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x44), 0x24);
            for (k = first; k < D_800D2D28->gaugeParts[i] * 2; k += 2) {
                func_80076C34(&D_800C3EA4->gauge[i][0][k + D_800C3EB0.buffer]);
            }
            func_80076A10(0x61 + i, D_800C3EA4->portrait[i],
                          i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x1C), 0x14);
            D_800C3EA4->panels[i].parts[0] =
                func_80076A10(0x90, D_800C3EA4->panels[i].value[0],
                              i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x38), 0x27);
            D_800C3EA4->panels[i].parts[1] =
                func_80076A10(0x91, D_800C3EA4->panels[i].gear[0],
                              i * 0x60 + (D_800C3254[D_800D3280 * 3 + i] + 0x3C), 0x27);
            D_800C3EA4->panels[i].buffer = D_800C3EB0.buffer;
        }
    }
    D_800D2D28->gaugeBuffer = D_800C3EB0.buffer;
    D_800D2D28->portraitBuffer = D_800C3EB0.buffer;
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
