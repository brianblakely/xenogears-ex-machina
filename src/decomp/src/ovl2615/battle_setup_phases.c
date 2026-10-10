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

/* 801E4048: Clear the battle outcome flags, publish the party ids and reset every
 * slot's flags (demo battles place everyone alone). */
void battle_setup_reset_outcome_and_slots(void) {
    s32 i;

    battle_continue_to_movie_mode = 0;
    battle_resume_event_script_at_end = 0;
    battle_skip_result_screens = 0;
    battle_exit_requested = 0;
    battle_defeat_allowed_by_event_script = 0;
    battle_unread_setup_flag = 0;
    battle_turn_hud_hidden = 0;
    mode_battle_turn_count = 0;
    mode_battle_return_fade = 0;
    for (i = 0; i < 3; i++) {
        mode_battle_party_ids[i] = battle_setup_work_area.partyIds[i];
        battle_area.slots[i].field2 = battle_setup_work_area.partyIds[i];
    }
    for (i = 0; i < SLOT_COUNT; i++) {
        battle_area.slots[i].hidden = 0;
        if (battle_uses_fixed_party == 0) {
            battle_area.slots[i].gear = game_data_slot_in_gear[i];
        } else {
            battle_area.slots[i].gear = 1;
        }
        battle_area.slots[i].field5 = 0;
    }
    battle_area.outcome = 0;
    battle_forced_next_turn = 0;
}

/* 801E4160: Place the formation: party and enemy presence, ids and groups from the
 * formation record, group membership and each member's standing position
 * from the battle scene data. The eight enemies take slots 3-10. */
void battle_setup_place_formation(void) {
    s32 i;
    u8 id;

    battle_party_panel_layout = 0;
    battle_formation = (BattleScene *)mode_battle_scene_file;
    battle_area.formation = (struct Formation *)mode_battle_scene_file;
    for (i = 0; i < 3; i++) {
        if ((battle_area.slots[i].field2 & 0x7F) != NO_COMBATANT) {
            battle_turn_queue.present[i] = 1;
            battle_party_panel_layout++;
        } else {
            battle_turn_queue.present[i] = 0;
        }
        if (battle_area.slots[i].gear == 0) {
            battle_area.slots[i].group = FORMATION_PARTY_GROUP(i) & 0x7F;
        } else {
            battle_area.slots[i].group = i;
        }
    }
    battle_party_panel_layout += 0xFF; /* one less */
    for (i = 0; i < 8; i++) {
        id = FORMATION_ENEMY_ID(i) & 0x7F;
        if (id != NO_COMBATANT) {
            battle_area.slots[i + 3].field2 = id;
            battle_area.slots[i + 3].hidden = FORMATION_ENEMY_FLAGS(i) & 0x80;
            battle_area.slots[i + 3].gear = FORMATION_ENEMY_ID(i) & 0x80;
            battle_area.slots[i + 3].field5 = FORMATION_ENEMY_FLAGS(i) & 1;
            battle_turn_queue.present[i + 3] = 1;
            battle_area.slots[i + 3].group = FORMATION_ENEMY_GROUP(i) & 0x7F;
        } else {
            battle_setup_work_area.work.records[i + 3].pilot.maxHp = 0;
            battle_setup_work_area.work.records[i + 3].pilot.hp = 0;
            battle_area.slots[i + 3].field2 = NO_COMBATANT;
            battle_area.slots[i + 3].hidden = 0;
            battle_area.slots[i + 3].gear = 0;
            battle_turn_queue.present[i + 3] = 0;
        }
        battle_enemy_name_indices_by_slot[i + 3] = battle_area.slots[i + 3].field2 + 1;
    }
    for (i = 0; i < 32; i++) {
        battle_formation_groups[i].count = 0;
        battle_formation_groups[i].members = 0;
    }
    for (i = 0; i < 3; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            if (battle_area.slots[i].gear == 0) {
                battle_area.slots[i].member = battle_formation_groups[battle_area.slots[i].group].count;
                battle_formation_groups[battle_area.slots[i].group].members |= battle_get_slot_bit(battle_area.slots[i].member);
                battle_formation_groups[battle_area.slots[i].group].count++;
            } else {
                battle_area.slots[i].member = 0;
                battle_formation_groups[battle_area.slots[i].group + 16].members = 1;
                battle_formation_groups[battle_area.slots[i].group + 16].count = 1;
            }
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            if (battle_area.slots[i].gear == 0) {
                battle_area.slots[i].member = battle_formation_groups[battle_area.slots[i].group + 8].count;
                battle_formation_groups[battle_area.slots[i].group + 8].members |= battle_get_slot_bit(battle_area.slots[i].member);
                battle_formation_groups[battle_area.slots[i].group + 8].count++;
            } else {
                battle_area.slots[i].member = 0;
                battle_formation_groups[battle_area.slots[i].group + 24].members = 1;
                battle_formation_groups[battle_area.slots[i].group + 24].count = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            if (battle_area.slots[i].gear == 0) {
                battle_area.slots[i].x =
                    battle_formation->group[battle_area.slots[i].group].party[battle_area.slots[i].member].x;
                battle_area.slots[i].z =
                    battle_formation->group[battle_area.slots[i].group].party[battle_area.slots[i].member].z;
            } else {
                battle_area.slots[i].x = battle_formation->gear[battle_area.slots[i].group].party.x;
                battle_area.slots[i].z = battle_formation->gear[battle_area.slots[i].group].party.z;
            }
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            if (battle_area.slots[i].gear == 0) {
                battle_area.slots[i].x =
                    battle_formation->group[battle_area.slots[i].group].enemy[battle_area.slots[i].member].x;
                battle_area.slots[i].z =
                    battle_formation->group[battle_area.slots[i].group].enemy[battle_area.slots[i].member].z;
                battle_area.slots[i].targetCode = FORMATION_FLAG6(i) & 0x80;
            } else {
                battle_area.slots[i].x = battle_formation->gear[battle_area.slots[i].group].enemy.x;
                battle_area.slots[i].z = battle_formation->gear[battle_area.slots[i].group].enemy.z;
                battle_area.slots[i].targetCode = FORMATION_FLAG6(i) & 0x80;
            }
        }
    }
}

/* 801E4870: Copy each present enemy's combatant record from the enemy data file and
 * point its AI state at its scripts (absent enemies are cleared). */
void battle_setup_copy_enemy_records_and_ai(void) {
    u8 *records;
    u16 *scripts;
    s32 i;
    s32 j;

    battle_area.knockedOut = 0;
    battle_joint_action_slots = 0;
    records = battle_enemy_data_file + 0x32;
    battle_enemy_name_table = battle_enemy_data_file + ((u16 *)battle_enemy_data_file)[0x18];
    for (i = 3; i < SLOT_COUNT; i++) {
        battle_enemy_reactions[i - 3].unk3 = 0;
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            memmove(&battle_setup_work_area.work.records[i], records + battle_area.slots[i].field2 * sizeof(Combatant),
                          sizeof(Combatant));
            scripts = (u16 *)(battle_enemy_data_file + ((u16 *)battle_enemy_data_file)[battle_area.slots[i].field2]);
            battle_enemy_ai_blocks[i - 3].script = (u8 *)scripts + scripts[0];
            battle_enemy_ai_blocks[i - 3].unk4 = (u8 *)scripts + scripts[1];
            if (scripts[2] != 0xFFFF) {
                battle_enemy_ai_blocks[i - 3].reaction = (u8 *)scripts + scripts[2];
                battle_enemy_reactions[i - 3].armed = 1;
            } else {
                battle_enemy_reactions[i - 3].armed = 0;
            }
            if (scripts[3] != 0xFFFF) {
                battle_enemy_ai_blocks[i - 3].turnScript = (u8 *)scripts + scripts[3];
                battle_enemy_reactions[i - 3].unk1[0] = 1;
            } else {
                battle_enemy_reactions[i - 3].unk1[0] = 0;
            }
            for (j = 3; j >= 0; j--) {
                battle_enemy_ai_blocks[i - 3].longs[j] = 0;
            }
            for (j = 7; j >= 0; j--) {
                battle_enemy_ai_blocks[i - 3].vars[j] = 0;
            }
            for (j = 15; j >= 0; j--) {
                battle_enemy_ai_blocks[i - 3].bytes[j] = 0;
            }
        } else {
            bzero((u8 *)&battle_setup_work_area.work.records[i], sizeof(Combatant));
            battle_enemy_reactions[i - 3].armed = 0;
            battle_enemy_reactions[i - 3].unk1[0] = 0;
        }
    }
}

/* 801E4AC0: Derive the party's stats, then each slot's placed-alone flags and the
 * party members' panel states. */
void battle_setup_derive_stats_and_slot_states(void) {
    s32 i;

    battle_derive_party_stats();
    if (battle_uses_fixed_party != 0) {
        battle_set_debug_party_stats();
    }
    for (i = 0; i < 3; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            battle_graphics->panels[i].state = 1;
            if (battle_area.slots[i].gear != 0) {
                battle_slot_states.party[i].in_gear = 1;
                battle_setup_work_area.work.records[i].flags15A |= 0x80;
                if (battle_area.slots[i].field2 != 7) {
                    battle_graphics->panels[i].state = 2;
                }
            } else {
                battle_slot_states.party[i].in_gear = 0;
                battle_setup_work_area.work.records[i].flags15A &= 0x7F;
            }
        } else {
            battle_slot_states.party[i].in_gear = 0;
            battle_setup_work_area.work.records[i].flags15A &= 0x7F;
            battle_graphics->panels[i].state = 0;
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            if (battle_area.slots[i].gear != 0) {
                battle_slot_states.enemy[i - 3].in_gear = 1;
            } else {
                battle_slot_states.enemy[i - 3].in_gear = 0;
            }
        } else {
            battle_slot_states.enemy[i - 3].in_gear = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        battle_slot_states.party[i].stat62 = battle_setup_work_area.work.records[i].pilot.level;
        battle_slot_states.party[i].stat63 = battle_setup_work_area.work.records[i].pilot.level2;
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

/* 801E4CD0: Build the battle item lists from the inventory (counts capped at 99, empty
 * slots cleared) and the special item list from ids 50..72. */
void battle_setup_build_item_lists(void) {
    s32 i;
    u8 *count;
    u8 listed;

    for (i = 0; i < BATTLE_ITEMS; i++) {
        battle_item_ids[i] = 0;
        battle_item_counts[i] = 0;
        battle_item_inventory_ids[i] = 0;
    }
    for (i = 0, count = game_data.itemCounts; i < INVENTORY_SLOTS; i++) {
        if (*count >= 100) {
            *count = 99;
        }
        if (*count++ == 0) {
            game_data.itemIds[i] = 0;
        }
    }
    listed = 0;
    for (i = 0; i < INVENTORY_SLOTS && listed < BATTLE_ITEMS; i++) {
        if (game_data.itemIds[i] != 0 && game_data.itemIds[i] < 49) {
            LIST_ITEM(battle_item_ids, listed, game_data.itemIds[i]);
            battle_item_counts[listed] = game_data.itemCounts[i];
            battle_item_inventory_ids[listed] = game_data.itemIds[i];
            listed++;
        }
    }
    battle_turn_state->lastItem = 47;
    for (i = 0, listed = 0; i < 100; i++) {
        if (game_data.weaponIds[i] >= 50 && game_data.weaponIds[i] < 73) {
            LIST_ITEM(battle_gear_part_ids, listed, game_data.weaponIds[i]);
            battle_gear_part_counts[listed] = game_data.weaponCounts[i];
            listed++;
        }
    }
    for (; listed < BATTLE_ITEMS; listed++) {
        battle_gear_part_ids[listed] = 0;
        battle_gear_part_counts[listed] = 0;
    }
}

/* 801E4E7C: Start the turn timers, draw a random turn order of the eleven slots, let
 * enemies flagged 0x200 act first, then rebase every present timer so the
 * smallest becomes 1. */
void battle_setup_init_turn_order_and_timers(void) {
    u8 drawn[SLOT_COUNT];
    s32 i;
    s32 least;
    s32 delta;
    u8 slot;

    battle_work_command_index = 0;
    battle_reset_turn_timers(drawn);
    i = 0;
    do {
        slot = mode_get_random_byte_in_range(0, 10);
        if (drawn[slot] == 0) {
            drawn[slot] = 1;
            battle_turn_queue.order[i] = slot;
            i++;
        }
    } while (i < SLOT_COUNT);
    battle_turn_queue.cursor = 0;
    for (i = 3; i < SLOT_COUNT; i++) {
        if (battle_turn_queue.present[i] != 0 && (battle_setup_work_area.work.records[i].pilot.flags34 & 0x200)) {
            battle_turn_queue.timers[0][i] = battle_turn_queue.timers[1][i] = 1;
        }
    }
    least = 0xFFFF;
    for (i = 0; i < SLOT_COUNT; i++) {
        if (battle_turn_queue.present[i] != 0) {
            if (battle_turn_queue.timers[1][i] < least) {
                least = battle_turn_queue.timers[1][i];
            }
        }
    }
    i = 0;
    delta = least - 1;
    for (; i < SLOT_COUNT; i++) {
        if (battle_turn_queue.present[i] != 0) {
            battle_turn_queue.timers[1][i] -= delta;
        }
    }
}

/* 801E5014: Set up each slot's command menu: the party's layouts, menu sources and
 * command masks (commands 7 and 8 forced by the formation), and every slot's
 * default target and facing. */
void battle_setup_init_command_menus(void) {
    s32 i;
    s32 j;
    u8 k;

    if (FORMATION_FLAGS & 0x20) {
        battle_uses_event_script = 1;
    }
    for (i = 0; i < 3; i++) {
        battle_slot_states.party[i].character_b = game_data.skills[battle_setup_work_area.partyIds[i]].tier;
        for (j = 0; j < 8; j++) {
            battle_turn_state->slots[i].layout[j] = battle_command_layouts_by_character[battle_setup_work_area.work.records[i].pilot.characterId][j];
        }
        for (j = 0; j < 4; j++) {
            if (battle_setup_work_area.partyIds[i] != 7) {
                battle_turn_state->slots[i].digits[0][j] = battle_command_layout_gear_ptr[j];
            } else {
                battle_turn_state->slots[i].digits[0][j] = battle_command_layout_gear_character7_ptr[j];
            }
            battle_turn_state->slots[i].digits[1][j] = battle_command_layout_gear_second_ptr[j];
        }
        battle_turn_state->slots[i].defaultTarget = battle_order_attack_candidates(i);
        battle_area.slots[i].targetCode = battle_is_target_at_lower_x(i, battle_turn_state->slots[i].defaultTarget);
        for (k = 0; k < 16; k++) {
            battle_turn_state->slots[i].items[k] = battle_setup_work_area.work.records[i].pilot.status7A & battle_command_seal_bits[k];
        }
        if (battle_setup_work_area.work.records[i].pilot.gearId == 0xFF || (FORMATION_FLAGS & 0x40)) {
            battle_turn_state->slots[i].items[7] = battle_command_seal_bits[7];
            battle_setup_work_area.work.records[i].pilot.status7A |= battle_command_seal_bits[7];
        }
        if (FORMATION_FLAGS & 0x80) {
            battle_turn_state->slots[i].items[8] = battle_command_seal_bits[8];
            battle_setup_work_area.work.records[i].pilot.status7A |= battle_command_seal_bits[8];
        }
    }
    for (i = 3; i < SLOT_COUNT; i++) {
        battle_turn_state->slots[i].defaultTarget = battle_order_attack_candidates(i);
        battle_area.slots[i].targetCode = battle_is_target_at_lower_x(i, battle_turn_state->slots[i].defaultTarget);
    }
}

/* 801E5384: Setup phase 0: choose the party (or the demo party), copy each member's
 * character and gear records and battle data from the setup archive, load
 * its images and tables, then start reading the formation's enemy files. */
void battle_setup_load_party_and_enemy_files(void) {
    s32 count;
    s32 i;
    void **archive;
    void *block;
    u16 mask;
    u8 gear;
    u16 *list;

    if (FORMATION_FLAGS & 0x10) {
        battle_uses_fixed_party = 1;
    } else {
        battle_uses_fixed_party = 0;
    }
    mask = (game_data.joined & game_data.available) & 0x7FF;
    if (battle_uses_fixed_party == 0) {
        count = 0;
        for (i = 0; i < 3; i++) {
            if (battle_is_slot_in_mask(mask, game_data.party[i])) {
                battle_setup_work_area.partyIds[count] = game_data.party[i] & 0x7F;
                count++;
            }
        }
        for (; count < 3; count++) {
            battle_setup_work_area.partyIds[count] = NO_COMBATANT;
        }
    } else {
        battle_setup_work_area.partyIds[1] = 10;
        battle_setup_work_area.partyIds[2] = 10;
        battle_setup_work_area.partyIds[0] = game_data.party[0] & 0x7F;
    }
    text_relocate_offset_table(mode_battle_setup_archive);
    archive = mode_battle_setup_archive;
    for (i = 0; i < 3; i++) {
        if (battle_setup_work_area.partyIds[i] != NO_COMBATANT) {
            memmove(&battle_setup_work_area.work.records[i].pilot, &game_data.characters[battle_setup_work_area.partyIds[i]], 0xA4);
            if (battle_uses_fixed_party != 0 && (u32)(i - 1) < 2) {
                battle_setup_work_area.work.records[i].pilot.gearId = 0x11;
            }
            gear = battle_setup_work_area.work.records[i].pilot.gearId;
            if (gear == 0xFF) {
                gear = 0;
            }
            memmove(&battle_setup_work_area.work.records[i].gear, &game_data.gears[gear], 0xA4);
            block = text_unpack_lzss_alloc(archive[5 + battle_setup_work_area.partyIds[i]], 1);
            memmove(battle_setup_work_area.work.partyCommands[i], block, 0x5F0);
            heap_free(block);
            block = text_unpack_lzss_alloc(archive[0x11 + gear], 1);
            memmove(battle_setup_work_area.work.gearCommands[i], block, 0x690);
            heap_free(block);
        }
    }
    /* The setup archive fills the work area from the enemy command table
     * (0x35D8, 0x1F40 bytes of item 4) to 0x5B18 (items 3 and 0x25 at 0x5518
     * and 0x5818). */
    block = text_unpack_lzss_alloc(archive[4], 1);
    memmove(battle_setup_work_area.work.enemyCommands, block, 0x1F40);
    heap_free(block);
    block = text_unpack_lzss_alloc(archive[3], 1);
    memmove((u8 *)&battle_setup_work_area + 0x5518, block, 0x300);
    heap_free(block);
    block = text_unpack_lzss_alloc(archive[2], 1);
    model_load_image_list(block, 0, 0, 0, 0, 0, 0);
    heap_free(block);
    battle_glyph_table = text_unpack_lzss_alloc(archive[1], 0);
    text_load_palette(0, 0x1F0);
    battle_item_name_table = text_unpack_lzss_alloc(archive[0x10], 0);
    block = text_unpack_lzss_alloc(archive[0x24], 1);
    battle_upload_party_portraits(block, 0x61);
    heap_free(block);
    block = text_unpack_lzss_alloc(archive[0x25], 1);
    memmove((u8 *)&battle_setup_work_area + 0x5818, (u8 *)block + 0x320, 0x300);
    heap_free(block);
    battle_message_table = text_unpack_lzss_alloc(archive[0x26], 0);
    heap_free(mode_battle_setup_archive);
    cd_select_directory(0xC, 1);
    battle_enemy_data_file = battle_heap_alloc(cd_get_aligned_file_size(formation_active.battle * 2 + 2), 0);
    battle_enemy_read_list_destination0 = battle_enemy_data_file;
    list = &battle_enemy_read_list;
    *list = formation_active.battle * 2 + 2;
    battle_enemy_set_file = (s32)battle_heap_alloc(cd_get_aligned_file_size(formation_active.battle * 2 + 3), 1);
    battle_enemy_read_list_destination1 = (void *)battle_enemy_set_file;
    battle_enemy_read_list_end = 0;
    battle_enemy_read_list_end_destination = NULL;
    battle_enemy_read_list_file1 = formation_active.battle * 2 + 3;
    cd_read_file_list((FileRequest *)list, 0, 0x80);
}

/* 801E5840: Run one battle setup phase: 0 the party, 1 the formation and combatant
 * records, 2 the items, turn timers and turn tables, 3 the gauges and texts. */
void battle_setup_run_phase(u8 phase) {
    switch (phase) {
    case 0:
        battle_setup_load_party_and_enemy_files();
        break;
    case 1:
        battle_setup_reset_outcome_and_slots();
        battle_setup_place_formation();
        battle_setup_copy_enemy_records_and_ai();
        battle_setup_derive_stats_and_slot_states();
        break;
    case 2:
        battle_setup_build_item_lists();
        battle_setup_init_turn_order_and_timers();
        battle_setup_init_command_menus();
        battle_direction_arrows = battle_heap_alloc(0xEC, 0);
        bzero((u8 *)battle_direction_arrows, 0xEC);
        break;
    case 3:
        battle_setup_init_gauges_and_panel_glyphs();
        battle_setup_init_panel_quads_and_digits();
        break;
    }
}

/* 801E5924: Prepare the ATB gauge primitives: textured bars lit at the top, grey
 * shades and white lines, on the gauge glyph's texture page and palettes. */
void battle_setup_init_atb_gauges(void) {
    s32 i;

    battle_ui->reaction[0] = 1;
    battle_ui->reaction[1] = 1;
    battle_ui->reaction[2] = 1;
    sprite_sheet_get_texture(battle_glyph_table, 0x5C, &battle_graphics->sprites[0].unk0, &battle_graphics->sprites[0].tpageMode,
                  &battle_graphics->sprites[0].clutX, &battle_graphics->sprites[0].clutY, &battle_graphics->sprites[0].pageX,
                  &battle_graphics->sprites[0].pageY);
    battle_graphics->barCluts[1] = GetClut(battle_graphics->sprites[0].clutX, battle_graphics->sprites[0].clutY);
    battle_graphics->barCluts[0] = GetClut(battle_graphics->sprites[0].clutX, battle_graphics->sprites[0].clutY - 1);
    battle_graphics->barCluts[3] = GetClut(battle_graphics->sprites[0].clutX, battle_graphics->sprites[0].clutY - 2);
    battle_graphics->barCluts[2] = GetClut(battle_graphics->sprites[0].clutX, battle_graphics->sprites[0].clutY - 3);
    for (i = 0; i < 8; i++) {
        SetPolyGT4(&battle_graphics->gaugeBars[i]);
        SetShadeTex(&battle_graphics->gaugeBars[i], 0);
        (battle_graphics->gaugeBars + i)->r0 = 0x80;
        (battle_graphics->gaugeBars + i)->g0 = 0x80;
        (battle_graphics->gaugeBars + i)->b0 = 0x80;
        (battle_graphics->gaugeBars + i)->r1 = 0x80;
        (battle_graphics->gaugeBars + i)->g1 = 0x80;
        (battle_graphics->gaugeBars + i)->b1 = 0x80;
        (battle_graphics->gaugeBars + i)->r2 = 0;
        (battle_graphics->gaugeBars + i)->g2 = 0;
        (battle_graphics->gaugeBars + i)->b2 = 0;
        (battle_graphics->gaugeBars + i)->r3 = 0;
        (battle_graphics->gaugeBars + i)->g3 = 0;
        (battle_graphics->gaugeBars + i)->b3 = 0;
        battle_graphics->gaugeBars[i].tpage =
            GetTPage(battle_graphics->sprites[0].tpageMode, 0, battle_graphics->sprites[0].pageX, battle_graphics->sprites[0].pageY);
        SetPolyG4(&battle_graphics->shade[i]);
        (battle_graphics->shade + i)->r2 = 0x4F;
        (battle_graphics->shade + i)->g2 = 0x4F;
        (battle_graphics->shade + i)->b2 = 0x4F;
        (battle_graphics->shade + i)->r3 = 0x4F;
        (battle_graphics->shade + i)->g3 = 0x4F;
        (battle_graphics->shade + i)->b3 = 0x4F;
    }
    for (i = 0; i < 12; i++) {
        SetLineF2(&battle_graphics->unk908[i]);
        (battle_graphics->unk908 + i)->r0 = 0xFF;
        (battle_graphics->unk908 + i)->g0 = 0xFF;
        (battle_graphics->unk908 + i)->b0 = 0xFF;
    }
}

/* 801E5D2C: Prepare the two white semi-transparent panel quads and their draw modes
 * (blend mode 2 on the effect texture page). */
void battle_setup_init_panel_quads(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    for (i = 0; i < 2; i++) {
        SetPolyF4(&battle_graphics->panel[i]);
        (battle_graphics->panel + i)->r0 = 0xFF;
        (battle_graphics->panel + i)->g0 = 0xFF;
        (battle_graphics->panel + i)->b0 = 0xFF;
        SetSemiTrans(&battle_graphics->panel[i], 1);
        SetDrawMode(&battle_graphics->panelMode[i], 0, 0,
                      GetTPage(0, 2, battle_graphics->sprites[0].pageX, battle_graphics->sprites[0].pageY), &window);
    }
    battle_graphics->unk6415 = 0;
    battle_graphics->panelAlpha = 0xFF;
    battle_graphics->unk6416 = 0;
}

/* 801E5E78: Render the ten battle messages 0-9 into text images. The original frame
 * reserves eight bytes that no instruction touches. */
void battle_setup_render_digit_text_images(void) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    s32 i;

    for (i = 0; i < 10; i++) {
        battle_digit_text_images[i].pixels = battle_heap_alloc_text_image(4);
        window_render_text_line(text_get_battle_message(i), battle_digit_text_images[i].pixels, 2, 0);
    }
}

/* 801E5EE8: Draw each present member's gauge glyphs (the second set dimmed), its
 * portrait and its two digit glyphs, placed by the party layout's columns. */
void battle_setup_build_party_panel_glyphs(void) {
    s32 i;
    s32 part;
    s32 first; /* first dimmed glyph: two prims per part */
    s32 k;

    for (i = 0; i < 3; i++) {
        if (battle_area.slots[i].field2 != NO_COMBATANT) {
            battle_ui->gaugeParts[i] += battle_build_glyph_half_scale(
                0x52, &battle_graphics->gauge[i][0][battle_ui->gaugeParts[i] * 2],
                i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x44), 0x24);
            part = battle_ui->gaugeParts[i];
            first = part * 2;
            battle_ui->gaugeParts[i] += battle_build_glyph_half_scale(
                0x53, &battle_graphics->gauge[i][0][part * 2],
                i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x44), 0x24);
            for (k = first; k < battle_ui->gaugeParts[i] * 2; k += 2) {
                battle_quad_init_half_subtractive(&battle_graphics->gauge[i][0][k + battle_area.buffer]);
            }
            battle_build_glyph(0x61 + i, battle_graphics->portrait[i],
                          i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x1C), 0x14);
            battle_graphics->panels[i].parts[0] =
                battle_build_glyph(0x90, battle_graphics->panels[i].value[0],
                              i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x38), 0x27);
            battle_graphics->panels[i].parts[1] =
                battle_build_glyph(0x91, battle_graphics->panels[i].gear[0],
                              i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x3C), 0x27);
            battle_graphics->panels[i].buffer = battle_area.buffer;
        }
    }
    battle_ui->gaugeBuffer = battle_area.buffer;
    battle_ui->portraitBuffer = battle_area.buffer;
}

/* 801E6290: Setup phase 3, first frame: the gauge panels and each member's glyphs. */
void battle_setup_init_gauges_and_panel_glyphs(void) {
    battle_setup_init_atb_gauges();
    battle_setup_build_party_panel_glyphs();
}

/* 801E62B8: Setup phase 3, second frame: the panel backdrops and the message images. */
void battle_setup_init_panel_quads_and_digits(void) {
    battle_setup_init_panel_quads();
    battle_setup_render_digit_text_images();
}
