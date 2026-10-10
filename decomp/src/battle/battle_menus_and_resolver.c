/* Battle code from 8008CCCC to 8009E53C: the gear command menu and the
 * command panel's pages, the windows (opened, closed, grown), the item list
 * page, the cursor glyph, character 4's command window, the action resolver
 * with its rolls, formulas, elements and statuses, the turn timers and
 * regeneration, the gear HUD, the escape and defense commands and the party's
 * adjustments at the battle's start and end. Its rodata starts at 80070370
 * (80094EE4's jump table), back at 0 mod 8 after the previous unit's tables
 * at 4 mod 8 (docs/matching.md); the text boundary lies after 8008C81C. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/text.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/combatant.h"
#include "battle/command.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/resolver.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "action_resolve.h"
#include "gear_menu.h"
#include "own_declarations.h"
#include "resident_views.h"

/* This unit's functions, declared before their first use (the formula
 * functions its tables name are declared with the tables below). */
void battle_gear_menu_build_page(u8 member, u8 *present, u16 *values); /* build the gear command menu's list */
void battle_gear_menu_show_command(u8 member, u8 kind);
void battle_resolve_follow_up(void);
void battle_add_to_party_damage_totals(void);
void battle_resolve_cure_after_hit(void);
void battle_resolve_weapon_status_after_hit(void);
void battle_resolve_adjust_for_elements(u16 *attack, u16 *defense, s8 *hit);
void battle_resolve_ether_check(void);
void battle_resolve_counterattack(void);
s32 battle_resolve_hit_outcome(void);
s16 battle_resolve_attack_value(void);
s16 battle_resolve_defense_value(void);
s8 battle_resolve_status_roll(u8 chance, u8 kind, u16 flags);
void battle_clear_damage_and_results(void);
void battle_apply_item_effect_to_slot(u8 slot, u8 param);
s32 battle_can_put_member_out_of_action(void);
void battle_status_set_duration(u8 slot, u8 kind, u16 flag, u8 amount);
void battle_status_count_down_gear(GearRecord *gear, Combatant *record, volatile u8 *timers);
void battle_publish_current_command(void);
u8 battle_get_command_accuracy(u8 member, u8 command);
void battle_enable_deathblow_commands(u8 member); /* enable the member's deathblow commands */
void battle_seal_deathblow_commands(u8 member, u8 checked);
void battle_wear_weapon_items(void);
void battle_size_character7_gear_hp(u8 slot, Combatant *chuchu);
void battle_apply_character7_damage_bonus(u16 *damage);
void battle_status_show_message(u8 kind, u16 flag);
void battle_write_party_to_game_data(void);
void battle_resolve_gear_action(void);
void battle_resolve_gear_follow_up(void);
void battle_publish_current_gear_command(void);
void battle_use_on_foot_command_variant(void);
void battle_mirror_gear_status_to_pilot(u8 slot);
s8 battle_resolve_gear_hit_outcome(void);
u16 battle_resolve_gear_attack_value(void);
u16 battle_resolve_gear_defense_value(void);
s32 battle_resolve_gear_resistance(s32 damage);
s8 battle_resolve_gear_status_roll(u8 fromGear);

/* The unit's own uninitialized variable (its .bss, after battle.c's). */
static u16 battle_saved_command_seals[3]; /* 800C3AA4: each member's status7A before the battle's adjustments */

/* This unit's data, 800c348c-800c3508: the formula tables of its formula
 * functions, the formation mode and battle.c's combo steps. */
void battle_formula0_deal_damage(void);
void battle_formula1_heal_by_ether(void);
void battle_formula2_inflict_status(void);
void battle_formula3_drain_hp_or_ep(void);
void battle_formula4_deal_damage_by_kind(void);
void battle_formula5_cure_statuses(void);
void battle_formula6_revive(void);
void battle_formula7_heal_by_gear_max_hp(void);
void battle_gear_formula0_deal_damage(void);
void battle_gear_formula1_inflict_status(void);
void battle_gear_formula2_clear_defense(void);
void battle_gear_formula3_heal_by_part_amount(void);
void battle_gear_formula4_heal_by_power(void);
void battle_gear_formula5_set_attack_level4(void);
void battle_gear_formula6_drain_fuel(void);
void battle_gear_formula7_restore_fuel(void);
void battle_gear_formula8_cure_statuses(void);
void battle_gear_formula10_heal_by_ether(void);
/* The formula table, by CommandDescriptor.formula: battle_resolve_action calls
 * battle_formula_table[formula]() once per target without a range check. Gear
 * attackers and descriptors with flagsA 0x10 use battle_gear_formula_table instead
 * (battle_resolve_gear_action). tools/analysis/dispatch_tables.py counts the ids
 * the descriptors use. */
void (*battle_formula_table[])(void) = { /* 800C348C */
    /* 0 */ battle_formula0_deal_damage, /* 1 */ battle_formula1_heal_by_ether, /* 2 */ battle_formula2_inflict_status, /* 3 */ battle_formula3_drain_hp_or_ep,
    /* 4 */ battle_formula4_deal_damage_by_kind, /* 5 */ battle_formula5_cure_statuses, /* 6 */ battle_formula6_revive, /* 7 */ battle_formula7_heal_by_gear_max_hp,
};
u8 battle_unused_resolver_byte = 0; /* 800C34AC: never read */
u8 battle_party_member_count = 0; /* 800C34AD */
u8 battle_attack_item_broken = 0; /* 800C34AE */
BattleWork *battle_work_ptr = &battle_work_area; /* 800C34B0 */
/* The next combo step by step and AP paid (1-3); battle.c reads it from one
 * byte before, as battle_combo_next_step_table_by_paid (battle.data.ld). */
u8 battle_combo_next_step_table[8][3] = { /* 800C34B4 */
    {1, 5, 7}, {2, 6, 7}, {3, 5, 7}, {4, 6, 7}, {1, 5, 7}, {2, 6, 7}, {3, 5, 7}, {1, 5, 7},
};
/* The combo flag of each combo step index, 0-14 (each flag is its index):
 * battle_can_use_combo_step, battle_combo_chain_add_gear_step and
 * battle_combo_record_gear_step read entries 0-14. Its alignment padding holds a
 * stray byte (35, "5") that nothing reads, so it stays original data
 * (battle.classification.txt). */
INCLUDE_ORIGINAL(".data", battle_combo_step_flags, 0x800C34CC, 16);
/* The gear formula table, by CommandDescriptor.formula: battle_resolve_gear_action calls
 * battle_gear_formula_table[formula]() once per target without a range check. */
void (*battle_gear_formula_table[])(void) = { /* 800C34DC */
    /* 0 */ battle_gear_formula0_deal_damage, /* 1 */ battle_gear_formula1_inflict_status, /* 2 */ battle_gear_formula2_clear_defense, /* 3 */ battle_gear_formula3_heal_by_part_amount,
    /* 4 */ battle_gear_formula4_heal_by_power, /* 5 */ battle_gear_formula5_set_attack_level4, /* 6 */ battle_gear_formula6_drain_fuel, /* 7 */ battle_gear_formula7_restore_fuel,
    /* 8 */ battle_gear_formula8_cure_statuses, /* 9 */ battle_formula4_deal_damage_by_kind, /* 10 */ battle_gear_formula10_heal_by_ether,
};

/* 8008CCCC: Hide the command windows (three panels); without `keep` show the +0x641c
 * lists. */
void battle_gear_menu_hide_windows(u8 keep) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 0;
    battle_ui->windows[0] = battle_ui->windows[1] = battle_ui->windows[2] = 0;
    battle_ui->unkB7 = 0;
    if (keep == 0) {
        battle_ui->unkCB = 1;
    }
}

/* 8008CD28: Show the command windows (three panels, page 4) and frame the camera on
 * the member and its default target. */
void battle_gear_menu_show_windows(u8 member) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 1;
    battle_ui->windows[0] = battle_ui->windows[1] = battle_ui->windows[2] = 1;
    battle_ui->unkB7 = 4;
    battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
    battle_highlight_slots(battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
}

/* 8008CDE4: Confirm the member's gear command `index` (0-3, the gear's commands from
 * 37): it needs the fuel it costs, the character's permission bit and no
 * seal status for it. Hides the command windows and commits the command,
 * paying the fuel; if the commit fails the windows come back. A refused
 * command plays the error sound. Returns 1 when committed. */
u8 battle_gear_menu_confirm_command(u8 member, u8 index) {
    u16 sealed[4];
    u8 committed;
    u16 cost;
    u16 command;
    u8 refused;

    sealed[0] = battle_command_seal_bits[13];
    sealed[1] = battle_command_seal_bits[14];
    sealed[2] = battle_command_seal_bits[15];
    sealed[3] = battle_command_seal_bits[3];
    committed = 0;
    cost = battle_work_area.gearCommands[member][index + 37].hudState;
    command = battle_work_area.gearCommands[member][index + 37].state;
    refused = 1;

    if (battle_work_area.records[member].gear.fuel >= cost &&
        battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].flags1A, index) != 0 &&
        !(battle_work_area.records[member].pilot.status7A & sealed[index])) {
        battle_gear_menu_hide_windows(1);
        battle_ui->unkC6 = 1;
        if (battle_choose_target(command, member, 0)) {
            battle_work_area.records[member].gear.fuel -= cost;
            committed = 1;
        } else {
            battle_ui->unkC6 = 0;
            battle_gear_menu_show_windows(member);
        }
        refused = 0;
    }
    if (refused) {
        battle_play_menu_sound(0x4F);
    }
    return committed;
}

/* 8008CFB8: Run the member's gear command menu: list the gear's commands 37-40 the
 * character may use and no status seals (id and fuel cost), open its three
 * windows and move the cursor until a command is committed (1, remembered
 * + 0x10 in the turn state) or the menu is cancelled (0). */
u8 battle_gear_menu_run(member)
u8 member;
{
    u8 ids[4];
    u16 costs[4];
    u16 seals[4];
    s32 frame;
    u8 ticks;
    s32 shown;
    s32 cursor;
    u8 result;
    s32 i;

    cursor = 0;
    shown = 0xFF;
    result = 2;
    frame = 4;
    ticks = 0;
    seals[0] = battle_command_seal_bits[13];
    seals[1] = battle_command_seal_bits[14];
    seals[2] = battle_command_seal_bits[15];
    seals[3] = battle_command_seal_bits[3];
    for (i = 0; i < 4; i++) {
        ids[i] = 0xFF;
        costs[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].flags1A, i) != 0 &&
            !(battle_work_area.records[member].pilot.status7A & seals[i])) {
            ids[i] = battle_work_area.records[member].pilot.gearId * 4 + i;
            costs[i] = battle_work_area.gearCommands[member][i + 37].hudState;
        }
    }
    battle_ui->unkCB = 0;
    battle_window_open(0, 0x74, 0xA0, 0xAC, 0x38, 0, 1);
    battle_window_open(1, 0x84, 0x4C, 0x9C, 0x50, 0, 1);
    battle_window_open(2, 0x1C, 0xA0, 0x50, 0x18, 0, 1);
    battle_gear_menu_build_page(member, ids, costs);
    battle_show_direction_arrows();
    while (result == 2) {
        if (cursor != shown) {
            battle_gear_menu_show_command(member, cursor);
            shown = cursor;
        }
        battle_animate_cursor_glyph(0x8C, cursor * 16 + 0x58, &frame, &ticks);
        battle_wait_frame();
        switch (battle_pressed_key) {
        case 5:
            result = 0;
            break;
        case 4:
            if (battle_gear_menu_confirm_command(member, cursor)) {
                result = 1;
                battle_turn_state->unk2E6 = cursor + 0x10;
            }
            break;
        case 1:
            if (++cursor >= 4) {
                cursor = 3;
            }
            break;
        case 3:
            if (--cursor < 0) {
                cursor = 0;
            }
            break;
        }
    }
    battle_gear_menu_hide_windows(result);
    battle_release_list_page_block();
    battle_hide_direction_arrows();
    battle_window_close(0);
    battle_window_close(1);
    battle_window_close(2);
    return result;
}

/* 8008D328: Fade the list quads of both draw buffers (every other one from 800d2d28
 * +0xa3): semi-transparent, raw texture, darker by the turn state's step
 * each frame. When all have faded out, clear the fading flag (+0xcb). */
void battle_command_panel_fade(void) {
    s32 buffer;
    s32 i;
    u8 fading = 0;
    u8 shade;

    for (buffer = 0; buffer < 2; buffer++) {
        if (battle_ui->unkD0[buffer] != 0) {
            for (i = 0; i < battle_ui->unkD0[buffer] * 2; i += 2) {
                SetSemiTrans(&battle_graphics->unk641C[buffer][i + battle_ui->unkA3], 1);
                SetShadeTex(&battle_graphics->unk641C[buffer][i + battle_ui->unkA3], 0);
                battle_graphics->unk641C[buffer][i + battle_ui->unkA3].tpage |= 0x20;
                shade = battle_graphics->unk641C[buffer][i + battle_ui->unkA3].r0;
                if (shade != 0) {
                    fading = 1;
                    (battle_graphics->unk641C[buffer] + (i + battle_ui->unkA3))->r0 = shade - battle_turn_state->unk2E0 * 16;
                    (battle_graphics->unk641C[buffer] + (i + battle_ui->unkA3))->g0 = shade - battle_turn_state->unk2E0 * 16;
                    (battle_graphics->unk641C[buffer] + (i + battle_ui->unkA3))->b0 = shade - battle_turn_state->unk2E0 * 16;
                }
            }
        }
    }
    if (!fading) {
        battle_ui->unkCB = 0;
    }
}

/* 8008D598: Build the command panel page `page` for the member into the two +0x641c
 * glyph lists (800d2d28 +0xd0 counts): each of the page's glyph sets
 * places its glyphs (ids from 0x4000 read the turn slot's bytes, 0xff
 * skipped) and shades them (from 0x2000 by a slot item's availability:
 * shaded semi-transparent, else full brightness). With `fade` fade the lists
 * instead (8008d328) and return their buffer; otherwise return the draw
 * buffer. The list's first new glyph (`first`, set once) starts each
 * shading loop. */
s32 battle_command_panel_build_page(u8 member, u8 page, u8 fade) {
    u16 lists[2];
    u8 sets[2];
    s32 i;
    s32 j;
    s32 n;
    s32 first;
    s32 id;
    u16 shade;
    u32 value;

    if (fade != 0) {
        battle_command_panel_fade();
        return battle_ui->unkA3;
    }
    for (i = 0; i < 2; i++) {
        lists[i] = battle_command_panel_pages[page]->lists[i];
        sets[i] = battle_command_panel_pages[page]->sets[i];
        battle_ui->unkD0[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        if (lists[i] == 0xFF) {
            break;
        }
        /* Four halfwords per glyph record. */
        j = 0;
        while ((id = ((u16 *)battle_command_panel_glyph_sets[sets[i]])[j]) != 0xFFFF) {
            if (id >= 0x4000) {
                battle_turn_state->unk2C0[id & 3] = battle_turn_state->slots[member].layout[id & 0xFF];
                id = battle_turn_state->slots[member].layout[id & 0xFF];
                if (id == 0xFF) {
                    j += 4;
                    continue;
                }
            }
            first = battle_ui->unkD0[lists[i]] * 2;
            battle_ui->unkD0[lists[i]] +=
                battle_build_glyph(id, &battle_graphics->unk641C[lists[i]][battle_ui->unkD0[lists[i]] * 2],
                             ((s16 *)battle_command_panel_glyph_sets[sets[i]])[j + 1], ((s16 *)battle_command_panel_glyph_sets[sets[i]])[j + 2]);
            shade = ((u16 *)battle_command_panel_glyph_sets[sets[i]])[j + 3];
            value = (shade & 0xFF) >> 1;
            if (((u16 *)battle_command_panel_glyph_sets[sets[i]])[j + 3] >= 0x2000) {
                value = 0x10;
                if (battle_turn_state->slots[member].items[shade & 0xF] == 0) {
                    value = (shade & 0xF0) >> 1;
                }
            }
            if (value != 0) {
                for (n = first; n < battle_ui->unkD0[lists[i]] * 2; n += 2) {
                    SetSemiTrans(&battle_graphics->unk641C[lists[i]][n + battle_drawing_state.buffer], 1);
                    SetShadeTex(&battle_graphics->unk641C[lists[i]][n + battle_drawing_state.buffer], 0);
                    (battle_graphics->unk641C[lists[i]] + (n + battle_drawing_state.buffer))->r0 = value;
                    (battle_graphics->unk641C[lists[i]] + (n + battle_drawing_state.buffer))->g0 = value;
                    (battle_graphics->unk641C[lists[i]] + (n + battle_drawing_state.buffer))->b0 = value;
                    battle_graphics->unk641C[lists[i]][n + battle_drawing_state.buffer].tpage |= 0x20;
                }
            } else {
                for (n = first; n < battle_ui->unkD0[lists[i]] * 2; n += 2) {
                    (battle_graphics->unk641C[lists[i]] + (n + battle_drawing_state.buffer))->r0 = 0x80;
                    (battle_graphics->unk641C[lists[i]] + (n + battle_drawing_state.buffer))->g0 = 0x80;
                    (battle_graphics->unk641C[lists[i]] + (n + battle_drawing_state.buffer))->b0 = 0x80;
                }
            }
            j += 4;
        }
    }
    return battle_drawing_state.buffer;
}

/* 8008DC34: Place a window's four corner glyphs (the alternate set while the battle
 * is ending) at its corners in the current draw buffer. */
void battle_window_place_corners(u8 window, u16 x, u16 y, u16 w, u16 h) {
    u8 glyphs[4];
    WindowBlock *block = battle_window_blocks[window];
    s32 i;

    if (battle_frame_mode != 0) {
        glyphs[0] = 0x4A;
        glyphs[1] = 0x4C;
        glyphs[2] = 0x4F;
        glyphs[3] = 0x51;
    } else {
        glyphs[0] = 0xF0;
        glyphs[1] = 0xF2;
        glyphs[2] = 0xF5;
        glyphs[3] = 0xF7;
    }
    block->cornerCount = 0;
    block->cornerCount += battle_build_glyph(glyphs[0], &block->corners[block->cornerCount * 2], x, y);
    block->cornerCount += battle_build_glyph(glyphs[1], &block->corners[block->cornerCount * 2], x + w - 8, y);
    block->cornerCount += battle_build_glyph(glyphs[2], &block->corners[block->cornerCount * 2], x, y + h - 8);
    block->cornerCount += battle_build_glyph(glyphs[3], &block->corners[block->cornerCount * 2], x + w - 8, y + h - 8);
    for (i = 0; i < 4; i++) {
        battle_quad_init_full_window_blend(&block->corners[i * 2 + battle_drawing_state.buffer]);
    }
}

/* 8008DE04: Place a window's top edge: two pieces of texture 1 across the top,
 * each half the inner width, in the current draw buffer. */
void battle_window_place_top_edge(u8 window, u16 x, u16 y, u16 w) {
    WindowBlock *block = battle_window_blocks[window];
    s32 i;

    setXY4(block->frame[0] + battle_drawing_state.buffer, x + 8, y - 8, x + 8 + (w - 16) / 2, y - 8, x + 8, y + 8,
           x + 8 + (w - 16) / 2, y + 8);
    setXY4(block->frame[0] + battle_drawing_state.buffer + 2, x + 8 + (w - 16) / 2, y - 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y - 8, x + 8 + (w - 16) / 2, y + 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + 8);
    setUV4(block->frame[0] + battle_drawing_state.buffer, WINDOW_TEX_U(1), WINDOW_TEX_V(1), WINDOW_TEX_U(1) + 7,
           WINDOW_TEX_V(1), WINDOW_TEX_U(1), WINDOW_TEX_V(1) + 16, WINDOW_TEX_U(1) + 7, WINDOW_TEX_V(1) + 16);
    setUV4(block->frame[0] + battle_drawing_state.buffer + 2, WINDOW_TEX_U(1), WINDOW_TEX_V(1), WINDOW_TEX_U(1) + 7,
           WINDOW_TEX_V(1), WINDOW_TEX_U(1), WINDOW_TEX_V(1) + 16, WINDOW_TEX_U(1) + 7, WINDOW_TEX_V(1) + 16);
    for (i = 0; i < 2; i++) {
        battle_quad_init_full_window_blend(&block->frame[0][i * 2 + battle_drawing_state.buffer]);
    }
}

/* 8008E430: Place a window's bottom edge: two pieces of texture 2 across the bottom,
 * each half the inner width, in the current draw buffer. */
void battle_window_place_bottom_edge(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = battle_window_blocks[window];
    s32 i;

    setXY4(block->frame[1] + battle_drawing_state.buffer, x + 8, y + h - 8, x + 8 + (w - 16) / 2, y + h - 8, x + 8,
           y + h + 8, x + 8 + (w - 16) / 2, y + h + 8);
    setXY4(block->frame[1] + battle_drawing_state.buffer + 2, x + 8 + (w - 16) / 2, y + h - 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + h - 8, x + 8 + (w - 16) / 2, y + h + 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + h + 8);
    setUV4(block->frame[1] + battle_drawing_state.buffer, WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 1,
           WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2) + 16, WINDOW_TEX_U(2) - 1, WINDOW_TEX_V(2) + 16);
    setUV4(block->frame[1] + battle_drawing_state.buffer + 2, WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 1,
           WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2) + 16, WINDOW_TEX_U(2) - 1, WINDOW_TEX_V(2) + 16);
    for (i = 0; i < 2; i++) {
        battle_quad_init_full_window_blend(&block->frame[1][i * 2 + battle_drawing_state.buffer]);
    }
}

/* 8008EA70: Place a window's left edge: two pieces of texture 3 down the left side,
 * each half the inner height, in the current draw buffer. */
void battle_window_place_left_edge(u8 window, u16 x, u16 y, u16 h) {
    WindowBlock *block = battle_window_blocks[window];
    s32 i;

    setXY4(block->frame[2] + battle_drawing_state.buffer, x - 8, y + 8, x + 8, y + 8, x - 8, y + 8 + (h - 16) / 2, x + 8,
           y + 8 + (h - 16) / 2);
    setXY4(block->frame[2] + battle_drawing_state.buffer + 2, x - 8, y + 8 + (h - 16) / 2, x + 8, y + 8 + (h - 16) / 2,
           x - 8, y + 8 + (h - 16) / 2 + (h - 16) / 2, x + 8, y + 8 + (h - 16) / 2 + (h - 16) / 2);
    setUV4(block->frame[2] + battle_drawing_state.buffer, WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 30,
           WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3) + 7, WINDOW_TEX_U(3) + 30, WINDOW_TEX_V(3) + 7);
    setUV4(block->frame[2] + battle_drawing_state.buffer + 2, WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 30,
           WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3) + 7, WINDOW_TEX_U(3) + 30, WINDOW_TEX_V(3) + 7);
    for (i = 0; i < 2; i++) {
        battle_quad_init_full_window_blend(&block->frame[2][i * 2 + battle_drawing_state.buffer]);
    }
}

/* 8008F0A8: Place a window's right edge: two pieces of texture 4 down the right side,
 * each half the inner height, in the current draw buffer. */
void battle_window_place_right_edge(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = battle_window_blocks[window];
    s32 i;

    setXY4(block->frame[3] + battle_drawing_state.buffer, x + w - 8, y + 8, x + w + 8, y + 8, x + w - 8,
           y + 8 + (h - 16) / 2, x + w + 8, y + 8 + (h - 16) / 2);
    setXY4(block->frame[3] + battle_drawing_state.buffer + 2, x + w - 8, y + 8 + (h - 16) / 2, x + w + 8,
           y + 8 + (h - 16) / 2, x + w - 8, y + 8 + (h - 16) / 2 + (h - 16) / 2, x + w + 8,
           y + 8 + (h - 16) / 2 + (h - 16) / 2);
    setUV4(block->frame[3] + battle_drawing_state.buffer, WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 30,
           WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4) + 7, WINDOW_TEX_U(4) + 30, WINDOW_TEX_V(4) + 7);
    setUV4(block->frame[3] + battle_drawing_state.buffer + 2, WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 30,
           WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4) + 7, WINDOW_TEX_U(4) + 30, WINDOW_TEX_V(4) + 7);
    for (i = 0; i < 2; i++) {
        battle_quad_init_full_window_blend(&block->frame[3][i * 2 + battle_drawing_state.buffer]);
    }
}

/* 8008F6E4: Place a window at (x, y, w, h) in the current draw buffer: its
 * background, corners and edges. The window is hidden while it changes. */
void battle_window_place(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = battle_window_blocks[window];

    battle_ui->windows[window] = 0;
    setXY4(block->shade + battle_drawing_state.buffer, x, y, x + w, y, x, y + h, x + w, y + h);
    battle_window_place_corners(window, x, y, w, h);
    battle_window_place_top_edge(window, x, y, w);
    battle_window_place_bottom_edge(window, x, y, w, h);
    battle_window_place_left_edge(window, x, y, h);
    battle_window_place_right_edge(window, x, y, w, h);
    block->buffer = battle_drawing_state.buffer;
    battle_ui->windows[window] = 1;
}

/* 8008F8F4: Open window `window` at (x, y) of w x h: allocate its blocks when it is not
 * shown; `animate` grows it open, otherwise it is drawn at once (and a frame
 * waited with `wait`). */
void battle_window_open(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait) {
    WindowRect *rect;

    if (battle_ui->windows[window] == 0) {
        battle_window_blocks[window] = (void *)battle_heap_alloc(0x5A8, 0);
        bzero((u_char *)battle_window_blocks[window], 0x5A8);
        battle_window_rects[window] = (WindowRect *)battle_heap_alloc(0xE, 0);
        bzero((u_char *)battle_window_rects[window], 0xE);
        battle_window_init_prims(window);
    }
    if (animate != 0) {
        rect = battle_window_rects[window];
        rect->style = window;
        rect->x = x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        rect->curW = 0;
        rect->curH = 0;
        battle_ui->windowOpen[window] = 0;
        battle_ui->windowOpening[window] = 1;
    } else {
        battle_window_place(window, x, y, w, h);
        if (wait != 0) {
            battle_wait_frame();
        }
    }
}

/* 8008FA60: Close window `window` and release its two blocks after a frame. */
void battle_window_close(u8 window) {
    battle_ui->windows[window] = 0;
    battle_ui->windowOpening[window] = 0;
    battle_wait_frame();
    heap_free(battle_window_blocks[window]);
    heap_free(battle_window_rects[window]);
}

/* 8008FAD8: Grow every opening window by 32 pixels per frame up to its size, centred,
 * and mark it open when both sides are complete. */
void battle_window_grow_opening(void) {
    s32 i;
    WindowRect *rect;
    u8 done;

    for (i = 0; i < 7; i++) {
        rect = battle_window_rects[i];
        if (battle_ui->windowOpening[i] != 0 && battle_ui->windowOpen[i] == 0) {
            done = 0;
            if (rect->curW + 32 >= rect->w) {
                rect->curW = rect->w;
                done = 1;
            } else {
                rect->curW += 32;
            }
            if (rect->curH + 32 >= rect->h) {
                rect->curH = rect->h;
                done++;
            } else {
                rect->curH += 32;
            }
            if (done == 2) {
                battle_ui->windowOpen[i] = 1;
            }
            battle_window_place(rect->style, rect->x + (rect->w >> 1) - (rect->curW >> 1),
                          rect->y + (rect->h >> 1) - (rect->curH >> 1), rect->curW, rect->curH);
        }
    }
}

/* 8008FC1C: Build a message frame from glyphs into the +0x1e68 list: `rows` side
 * glyphs down from `top`, the corner at (x, y) and a stretched edge of
 * width `w`. */
void battle_build_message_frame(s16 x, s16 y, s16 w, s16 top, u8 rows) {
    s32 i;

    battle_ui->unkFC = 0;
    for (i = 0; i < rows; i++) {
        battle_ui->unkFC += battle_build_glyph(0x65, &battle_graphics->unk1E68[battle_ui->unkFC * 2], x, top + i * 8);
    }
    battle_ui->unkFC = battle_build_glyph(0x64, &battle_graphics->unk1E68[battle_ui->unkFC * 2], x, y) + battle_ui->unkFC;
    battle_ui->unkFC += sprite_sheet_draw_scaled_flip(battle_glyph_table, 0x64, &battle_graphics->unk1E68[battle_ui->unkFC * 2],
                                       battle_drawing_state.buffer, x, w, 0x1000, 0, 1);
    battle_ui->unkA6 = battle_drawing_state.buffer;
    battle_ui->unk9D = 1;
}

/* 8008FDE4: Open the standard message window (0x20, 0x5c, 0xcc x 0x60, style 0xe). */
void battle_build_standard_message_frame(void) {
    battle_build_message_frame(0x20, 0x5C, 0xCC, 0x60, 0xE);
}

/* 8008FE18: Build the item list page: (with `open`) set up the graphics block and the
 * message frame, then render every list entry's two item names and their
 * two-digit counts into VRAM text images (names at 0x380, digits at 0x3c0,
 * one 13-line row per entry pair). */
void battle_item_menu_build_page(u8 column, u8 row, u8 open) {
    TextImage images[32];
    RECT nameRect;
    RECT tensRect;
    RECT onesRect;
    RECT tens2Rect;
    RECT ones2Rect;
    RECT rect;
    s32 unused[2]; /* an unreferenced 8-byte local in the frame */
    u8 counts[48];
    u8 ids[48];
    s32 i;
    u8 tens;
    u32 *digit;

    if (open != 0) {
        battle_alloc_list_page_block();
        battle_upload_command_name_images();
        battle_build_message_frame(0x20, 0x5C, 0xCC, 0x60, 0xE);
    }
    battle_blank_text_image = (u32 *)battle_heap_alloc_text_image(0x39);
    bzero((u_char *)battle_blank_text_image, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    battle_upload_image_and_wait(&rect, battle_blank_text_image);
    for (i = 0; i < 48; i++) {
        ids[i] = battle_item_ids[i];
        counts[i] = battle_item_counts[i];
    }
    for (i = 0; i < 32; i++) {
        images[i].pixels = (u32 *)battle_heap_alloc_text_image(0x1B);
        bzero((u_char *)images[i].pixels, 0x30C);
        nameRect.x = (i % 2) * 30 + 0x380;
        nameRect.y = (i / 2) * 13 + 0x100;
        nameRect.w = 30;
        nameRect.h = 13;
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 13 + 0x100;
        tensRect.w = 6;
        tensRect.h = 13;
        onesRect.x = (i % 2) * 16 + 0x3C2;
        onesRect.y = (i / 2) * 13 + 0x100;
        onesRect.w = 6;
        onesRect.h = 13;
        tens2Rect.x = (i % 2) * 16 + 0x3C4;
        tens2Rect.y = (i / 2) * 13 + 0x100;
        tens2Rect.w = 6;
        tens2Rect.h = 13;
        ones2Rect.x = (i % 2) * 16 + 0x3C6;
        ones2Rect.y = (i / 2) * 13 + 0x100;
        ones2Rect.w = 6;
        ones2Rect.h = 13;
        if (ids[i] != 0) {
            window_render_text_line(text_get_item_name(ids[i]), images[i].pixels, 0x1B, 0);
        }
        if (ids[i + 16] != 0) {
            window_render_text_line(text_get_item_name(ids[i + 16]), images[i].pixels, 0x1B, 1);
        }
        battle_upload_image_and_wait(&nameRect, images[i].pixels);
        tens = counts[i] / 10;
        if (tens != 0) {
            digit = battle_digit_text_images[tens].pixels;
        } else {
            digit = battle_blank_text_image;
        }
        battle_upload_image_and_wait(&tensRect, digit);
        if (ids[i] != 0) {
            digit = battle_digit_text_images[(u8)(counts[i] % 10)].pixels;
        } else {
            digit = battle_blank_text_image;
        }
        battle_upload_image_and_wait(&onesRect, digit);
        if ((u8)(battle_item_counts[i + 16] / 10) != 0) {
            digit = battle_digit_text_images[counts[i + 16] / 10].pixels;
        } else {
            digit = battle_blank_text_image;
        }
        battle_upload_image_and_wait(&tens2Rect, digit);
        if (ids[i + 16] != 0) {
            digit = battle_digit_text_images[(u8)(counts[i + 16] % 10)].pixels;
        } else {
            digit = battle_blank_text_image;
        }
        battle_upload_image_and_wait(&ones2Rect, digit);
    }
    for (i = 0; i < 32; i++) {
        heap_free(images[i].pixels);
    }
    heap_free(battle_blank_text_image);
    battle_wait_frame();
    battle_graphics->unkA230->unk669 = 1;
    battle_ui->unkB7 = 2;
}

/* 8009023C: Build nine glyph rows (0x66) from y + 0x64 into the +0xba8 primitives. */
void battle_item_menu_build_row_glyphs(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x68;
    battle_ui->unkF8 = 0;
    for (; i < 9; i++) {
        battle_ui->unkF8 += battle_build_glyph(0x66, &battle_graphics->unkBA8[battle_ui->unkF8 * 2], 0x20, (offset - 4) + y);
        offset += 8;
    }
    battle_ui->unkA5 = battle_drawing_state.buffer;
    battle_ui->unk9C = 1;
}

/* 80090310: Point the page title quads at list entry (column, row): the entry's image
 * cell (two per image row, 13 lines each; entries past 16 on the second
 * image page with the alternate CLUT). */
void battle_item_menu_point_title(u8 column, u8 row) {
    s32 index;
    s32 page;

    index = row * 2 + column;
    page = 0;
    if (index > 16) {
        index -= 16;
        battle_graphics->unkA230->unk140[battle_drawing_state.buffer].clut = text_plane1_clut;
        page = 0x10;
    } else {
        battle_graphics->unkA230->unk140[battle_drawing_state.buffer].clut = text_plane0_clut;
    }
    battle_quad_place_text_row(&battle_graphics->unkA230->unk140[battle_drawing_state.buffer], 0x18, 0x33, (index % 2) * 0x78, (index / 2) * 13,
                  0x60);
    battle_quad_place_text_row(&battle_graphics->unkA230->unk190[battle_drawing_state.buffer], 0x84, 0x33, (index % 2) << 6 | page,
                  (index / 2) * 13, 0x10);
}

/* 800904A0: Point the item page icon quads at the images for the item in list cell
 * (column, row): its state icon (9 bit 0x4000, 7 bit 0x1000, else 8) and its
 * level frame (12, 13 or 21 for levels 0-2), each with its CLUT. */
void battle_item_menu_point_icons(u8 column, u8 row) {
    u16 flags;
    s32 icon;
    u8 frame;

    flags = battle_item_effects[battle_item_ids[row * 2 + column]].target;
    if (flags & 0x4000) {
        icon = 9;
    } else {
        icon = 8;
        if (flags & 0x1000) {
            icon = 7;
        }
    }
    switch (flags & 0xF) {
    case 0:
        frame = 12;
        break;
    case 1:
        frame = 13;
        break;
    case 2:
        frame = 21;
        break;
    }
    battle_quad_place_text_row(&battle_graphics->unkA230->unk320[battle_drawing_state.buffer], 0xA8, 0x33, battle_icon_cells[icon].u, battle_icon_cells[icon].v,
                  battle_icon_cells[icon].w);
    battle_graphics->unkA230->unk320[battle_drawing_state.buffer].clut = battle_icon_cells[icon].alternate ? text_plane1_clut : text_plane0_clut;
    battle_quad_place_text_row(&battle_graphics->unkA230->unk370[battle_drawing_state.buffer], 0xD0, 0x33, battle_icon_cells[frame].u,
                  battle_icon_cells[frame].v, battle_icon_cells[frame].w);
    battle_graphics->unkA230->unk370[battle_drawing_state.buffer].clut = battle_icon_cells[frame].alternate ? text_plane1_clut : text_plane0_clut;
}

/* 8009070C: Render the name of the item in the list cell (column, row) into a text
 * image and place it on the graphics block's +0x280 quad. */
void battle_item_menu_build_name(u8 column, u8 row) {
    RECT rect;
    u8 item = battle_item_ids[row * 2 + column];
    u32 *pixels = (u32 *)battle_heap_alloc_text_image(0x39);
    s32 width;

    bzero((u_char *)pixels, 0x618);
    width = window_render_text_line(text_get_resource_entry(battle_item_name_table, item), pixels, 0x39, 0);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    battle_upload_image_and_wait(&rect, pixels);
    battle_quad_place_text_row(&battle_graphics->unkA230->unk280[battle_drawing_state.buffer], 0x30, 0x42, 0, 0, width);
    heap_free(pixels);
}

/* 8009080C: Show the item list cell (column, row) of the item list or (from the
 * 800c3d70 list) the other list: frame, name and count. */
void battle_item_menu_show_cell(u8 column, u8 row, u8 other) {
    u8 item;

    if (other == 0) {
        item = battle_item_ids[row * 2 + column];
    } else {
        item = battle_gear_part_ids[row * 2 + column];
    }
    battle_graphics->unkA230->unk66D = 0;
    battle_item_menu_point_title(column, row);
    if (item != 0 && other == 0) {
        battle_item_menu_build_name(column, row);
        battle_item_menu_point_icons(column, row);
        battle_graphics->unkA230->unk66D = 1;
    }
    battle_graphics->unkA230->buffer = battle_drawing_state.buffer;
    battle_graphics->unkA230->unk66B = 1;
}

/* 8009093C: Point the four list page quads at the list image from row offset `v`
 * (offsets from 0x68 on the second image page with the alternate CLUT). */
void battle_item_menu_point_list(s32 v) {
    s32 page = 0;

    if (v >= 0x68) {
        v -= 0x68;
        battle_graphics->unkA230->unk0[battle_drawing_state.buffer].clut = text_plane1_clut;
        battle_graphics->unkA230->unk50[battle_drawing_state.buffer].clut = text_plane1_clut;
        page = 0x10;
    } else {
        battle_graphics->unkA230->unk0[battle_drawing_state.buffer].clut = text_plane0_clut;
        battle_graphics->unkA230->unk50[battle_drawing_state.buffer].clut = text_plane0_clut;
    }
    battle_quad_place(&battle_graphics->unkA230->unk0[battle_drawing_state.buffer], 0x30, 0x60, 0, v, 0x60, 0x68);
    battle_quad_place(&battle_graphics->unkA230->unk50[battle_drawing_state.buffer], 0xB4, 0x60, 0x78, v, 0x60, 0x68);
    battle_quad_place(&battle_graphics->unkA230->unkA0[battle_drawing_state.buffer], 0x98, 0x60, page, v, 0x10, 0x68);
    battle_quad_place(&battle_graphics->unkA230->unkF0[battle_drawing_state.buffer], 0x11C, 0x60, page | 0x40, v, 0x10, 0x68);
    battle_graphics->unkA230->unk668 = battle_drawing_state.buffer;
}

/* 80090B90: Animate the five-frame cursor glyph at (x, y): advance `frame` every third
 * tick. */
void battle_animate_cursor_glyph(s32 x, s32 y, s32 *frame, u8 *ticks) {
    if (++*ticks >= 3) {
        *frame -= 1;
        if (*frame < 0) {
            *frame = 4;
        }
        *ticks = 0;
    }
    battle_ui->cursorParts = battle_build_glyph(*frame + 0xE0, battle_graphics->cursor, x, y);
    battle_ui->cursorBuffer = battle_drawing_state.buffer;
    battle_ui->cursorShown = 1;
}

/* 80090C44: Show the member's EP and maximum EP as two digit glyphs each (no leading
 * zero). */
void battle_art_menu_show_ep_digits(u8 member) {
    u16 digit;

    digit = battle_work_area.records[member].pilot.ep / 10;
    if (digit != 0) {
        battle_quad_place_text_row(&battle_graphics->unkA230->unk460[battle_drawing_state.buffer], 0x104, 0xC6, digit * 8 + 0x78, 0, 8);
    }
    battle_quad_place_text_row(&battle_graphics->unkA230->unk4B0[battle_drawing_state.buffer], 0x10C, 0xC6,
                  (u16)(battle_work_area.records[member].pilot.ep % 10) * 8 + 0x78, 0, 8);
    digit = battle_work_area.records[member].pilot.maxEp / 10;
    if (digit != 0) {
        battle_quad_place_text_row(&battle_graphics->unkA230->unk500[battle_drawing_state.buffer], 0x11C, 0xC6, digit * 8 + 0x78, 0, 8);
    }
    battle_quad_place_text_row(&battle_graphics->unkA230->unk550[battle_drawing_state.buffer], 0x124, 0xC6,
                  (u16)(battle_work_area.records[member].pilot.maxEp % 10) * 8 + 0x78, 0, 8);
}

/* 80090E7C: Show the member's EP panel: the two EP icons (cells 6 and 5), the EP
 * digits and the EP label glyphs. */
void battle_art_menu_show_ep_panel(u8 member) {
    battle_quad_place_text_row(&battle_graphics->unkA230->unk3C0[battle_drawing_state.buffer], 0xA8, 0xA6, battle_icon_cells[6].u, battle_icon_cells[6].v,
                  battle_icon_cells[6].w);
    battle_graphics->unkA230->unk3C0[battle_drawing_state.buffer].clut = battle_icon_cells[6].alternate ? text_plane1_clut : text_plane0_clut;
    battle_quad_place_text_row(&battle_graphics->unkA230->unk410[battle_drawing_state.buffer], 0xEC, 0xC6, battle_icon_cells[5].u, battle_icon_cells[5].v,
                  battle_icon_cells[5].w);
    battle_graphics->unkA230->unk410[battle_drawing_state.buffer].clut = battle_icon_cells[5].alternate ? text_plane1_clut : text_plane0_clut;
    battle_art_menu_show_ep_digits(member);
    battle_build_glyph(0x71, battle_graphics->unkA230->unk5A0, 0x118, 0xD1);
    battle_graphics->unkA230->unk66C = battle_drawing_state.buffer;
}

/* 80091064: Build the member's art list page: set up the graphics block and the
 * message frame, then render the name and two-digit EP cost of every art its
 * character (or its gear) knows into VRAM text images, then the EP panel. */
void battle_art_menu_build_page(u8 member) {
    RECT nameRect;
    RECT tensRect;
    RECT onesRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect;
    TextImage images[16];
    u8 known[16];
    u8 costs[16];
    s32 i;
    u8 tens;
    u32 *digit;

    battle_alloc_list_page_block();
    battle_upload_command_name_images();
    battle_build_message_frame(0x20, 0x30, 0x98, 0x38, 0xC);
    battle_blank_text_image = (u32 *)battle_heap_alloc_text_image(0x39);
    bzero((u_char *)battle_blank_text_image, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    battle_upload_image_and_wait(&rect, battle_blank_text_image);
    if (battle_slot_flags[member].unk1 == 0) {
        for (i = 0; i < 16; i++) {
            if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].levelSkills, i)) {
                known[i] = 1;
                costs[i] = battle_work_area.partyCommands[member][i + 22].cost;
            } else {
                known[i] = 0;
                costs[i] = 0;
            }
        }
    } else {
        for (i = 0; i < 16; i++) {
            if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksB, i)) {
                known[i] = 1;
                costs[i] = battle_work_area.gearCommands[member][i + 21].cost;
            } else {
                known[i] = 0;
                costs[i] = 0;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        images[i].pixels = (u32 *)battle_heap_alloc_text_image(0x1B);
        bzero((u_char *)images[i].pixels, 0x30C);
        rowRect.x = (i % 2) * 30 + 0x380;
        rowRect.y = (i / 2) * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        battle_upload_image_and_wait(&rowRect, battle_blank_text_image);
        if (known[i] != 0) {
            if (battle_slot_flags[member].unk1 == 0) {
                window_render_text_line(text_get_character_art_name(battle_work_area.records[member].pilot.characterId * 16 + i), images[i].pixels, 0x1B,
                              0);
            } else {
                window_render_text_line(text_get_gear_art_name(battle_work_area.records[member].pilot.gearId * 16 + i), images[i].pixels, 0x1B, 0);
            }
            nameRect.x = (i % 2) * 30 + 0x380;
            nameRect.y = (i / 2) * 16 + 0x102;
            nameRect.w = 30;
            nameRect.h = 13;
            battle_upload_image_and_wait(&nameRect, images[i].pixels);
        }
        if (!(i & 1)) {
            rightRect.x = 0x3C0;
            rightRect.y = (i / 2) * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            battle_upload_image_and_wait(&rightRect, battle_blank_text_image);
        }
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 16 + 0x102;
        tensRect.w = 6;
        tensRect.h = 13;
        tens = costs[i] / 10;
        if (tens != 0) {
            digit = battle_digit_text_images[tens].pixels;
        } else {
            digit = battle_blank_text_image;
        }
        battle_upload_image_and_wait(&tensRect, digit);
        onesRect.x = (i % 2) * 16 + 0x3C2;
        onesRect.y = (i / 2) * 16 + 0x102;
        onesRect.w = 6;
        onesRect.h = 13;
        if (known[i] != 0) {
            digit = battle_digit_text_images[(u8)(costs[i] % 10)].pixels;
        } else {
            digit = battle_blank_text_image;
        }
        battle_upload_image_and_wait(&onesRect, digit);
    }
    battle_art_menu_show_ep_panel(member);
    for (i = 0; i < 16; i++) {
        heap_free(images[i].pixels);
    }
    heap_free(battle_blank_text_image);
    battle_wait_frame();
    battle_graphics->unkA230->unk140[0].clut = text_plane0_clut;
    battle_graphics->unkA230->unk140[1].clut = text_plane0_clut;
    battle_graphics->unkA230->unk669 = 1;
    battle_ui->unkB7 = 1;
}

/* 80091604: Build eight glyph rows (0x66) from y + 0x38 into the +0xba8 primitives. */
void battle_art_menu_build_row_glyphs(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x38;
    battle_ui->unkF8 = 0;
    for (; i < 8; i++) {
        battle_ui->unkF8 += battle_build_glyph(0x66, &battle_graphics->unkBA8[battle_ui->unkF8 * 2], 0x20, offset + y);
        offset += 8;
    }
    battle_ui->unkA5 = battle_drawing_state.buffer;
    battle_ui->unk9C = 1;
}

/* 800916D4: Point the page title quad at entry (column, row) of the technique image
 * and, for character 1 (Fei), show the entry's cost (8009a258) as up to three
 * digit glyphs. */
void battle_art_menu_point_title(u8 column, u8 row, u8 member) {
    u32 index;
    u32 cellX;
    u32 cellY;
    s32 i;
    s32 x;
    u8 digit;

    index = row * 2 + column;
    cellY = index / 2;
    cellX = index - cellY * 2;
    battle_quad_place(&battle_graphics->unkA230->unk140[battle_drawing_state.buffer], 0x20, 0xA4, cellX * 0x78, cellY * 16, 0x60, 0x10);
    battle_graphics->unkA230->unk66E = 0;
    if (battle_party_character_ids[member] == 1) {
        /* 8009a258 is called unprototyped: the command is passed and its
         * result returned unnarrowed. */
        battle_split_decimal_digits(((s32 (*)())battle_get_command_accuracy)(member, index + 0x16));
        for (i = 0, x = 0x8C; i < 3; i++) {
            digit = battle_decimal_digits[i + 6];
            if (digit != 0xFF) {
                battle_quad_place_text_row(&battle_graphics->unkA230->unk190[battle_graphics->unkA230->unk66E * 2 + battle_drawing_state.buffer], x,
                              0xA6, digit * 8 + 0x78, 0, 8);
                battle_graphics->unkA230->unk66E++;
            }
            x += 8;
        }
    }
}

/* 8009187C: Point the art page icon quads at the images for art (column, row) of the
 * member (its gear's arts in a gear): its state icon (9 sealed, 7 flag
 * 0x1000, else 8) and its level frame (13 level 1, 21 level 2, else 12). */
void battle_art_menu_point_icons(u8 member, u8 column, u8 row) {
    u16 state;
    s32 icon;
    s32 frame;

    if (battle_slot_flags[member].unk1 == 0) {
        state = battle_work_area.partyCommands[member][row * 2 + column + 22].state;
    } else {
        state = battle_work_area.gearCommands[member][row * 2 + column + 21].state;
    }
    if (state & 0x4000) {
        icon = 9;
    } else {
        icon = 8;
        if (state & 0x1000) {
            icon = 7;
        }
    }
    switch (state & 0xF) {
    case 0:
        frame = 12;
        break;
    case 1:
        frame = 13;
        break;
    case 2:
        frame = 21;
        break;
    default:
        frame = 12;
        break;
    }
    battle_quad_place_text_row(&battle_graphics->unkA230->unk320[battle_drawing_state.buffer], 0xDA, 0xAA, battle_icon_cells[icon].u, battle_icon_cells[icon].v,
                  battle_icon_cells[icon].w);
    battle_graphics->unkA230->unk320[battle_drawing_state.buffer].clut = battle_icon_cells[icon].alternate ? text_plane1_clut : text_plane0_clut;
    battle_quad_place_text_row(&battle_graphics->unkA230->unk370[battle_drawing_state.buffer], 0xFE, 0xAA, battle_icon_cells[frame].u,
                  battle_icon_cells[frame].v, battle_icon_cells[frame].w);
    battle_graphics->unkA230->unk370[battle_drawing_state.buffer].clut = battle_icon_cells[frame].alternate ? text_plane1_clut : text_plane0_clut;
}

/* 80091B38: Build the description of art (column, row) of the member (its gear's in
 * a gear): two text lines from the menu module block into VRAM, placed on
 * the page's two description quads. */
void battle_art_menu_build_description(u8 member, u8 column, u8 row) {
    RECT rect;
    u32 *pixels;
    s32 text;
    s32 width0;
    s32 width1;

    if (battle_slot_flags[member].unk1 == 0) {
        text = battle_party_character_ids[member] * 32 + (row * 2 + column) * 2;
    } else {
        text = game_data.characters[battle_party_character_ids[member]].gearId * 32 + (row * 2 + column) * 2;
    }
    pixels = (u32 *)battle_heap_alloc_text_image(0x39);
    bzero((u_char *)pixels, 0x618);
    width0 = window_render_text_line(text_get_resource_entry(battle_command_menu_module_block, text & 0xFFFF), pixels, 0x39, 0);
    width1 = window_render_text_line(text_get_resource_entry(battle_command_menu_module_block, (text & 0xFFFF) | 1), pixels, 0x39, 1);
    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x3C;
    rect.h = 13;
    battle_upload_image_and_wait(&rect, pixels);
    battle_quad_place_text_row(&battle_graphics->unkA230->unk280[battle_drawing_state.buffer], 0x30, 0xB6, 0, 0, width0);
    battle_quad_place_text_row(&battle_graphics->unkA230->unk2D0[battle_drawing_state.buffer], 0x30, 0xC6, 0, 0, width1);
    heap_free(pixels);
}

/* 80091D38: Open the combo/technique entry (column, row) of the member's page when its
 * character knows it (mask +2 on foot, +6 in a gear): build its graphics for
 * the current draw buffer; otherwise mark the page closed. */
void battle_art_menu_show_entry(u8 member, u8 column, u8 row) {
    u8 known = 0;

    if (battle_slot_flags[member].unk1 == 0) {
        known = battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].levelSkills, column + row * 2) != 0;
    } else if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksB, column + row * 2)) {
        known = 1;
    }
    if (known) {
        battle_art_menu_point_title(column, row, member);
        battle_art_menu_point_icons(member, column, row);
        battle_art_menu_build_description(member, column, row);
        battle_graphics->unkA230->buffer = battle_drawing_state.buffer;
        battle_graphics->unkA230->unk66B = 1;
    } else {
        battle_graphics->unkA230->unk66B = 0;
    }
}

/* 80091EC4: Point the four art page quads at the art list image from row `v`. */
void battle_art_menu_point_list(u8 v) {
    battle_graphics->unkA230->unk0[battle_drawing_state.buffer].clut = text_plane0_clut;
    battle_graphics->unkA230->unk50[battle_drawing_state.buffer].clut = text_plane0_clut;
    battle_quad_place(&battle_graphics->unkA230->unk0[battle_drawing_state.buffer], 0x34, 0x34, 0, v, 0x60, 0x60);
    battle_quad_place(&battle_graphics->unkA230->unk50[battle_drawing_state.buffer], 0xB8, 0x34, 0x78, v, 0x60, 0x60);
    battle_quad_place(&battle_graphics->unkA230->unkA0[battle_drawing_state.buffer], 0x9C, 0x34, 0, v, 0x10, 0x60);
    battle_quad_place(&battle_graphics->unkA230->unkF0[battle_drawing_state.buffer], 0x120, 0x34, 0x40, v, 0x10, 0x60);
    battle_graphics->unkA230->unk668 = battle_drawing_state.buffer;
}

/* 8009209C: Show the gear page list image on the four page quads and mark the page
 * (command window page 3) open. */
void battle_combo_menu_show_list_image(void) {
    battle_graphics->unkA230->unk0[battle_drawing_state.buffer].clut = text_plane0_clut;
    battle_graphics->unkA230->unk50[battle_drawing_state.buffer].clut = text_plane0_clut;
    battle_quad_place(&battle_graphics->unkA230->unk0[battle_drawing_state.buffer], 0x36, 0x62, 0, 0, 0x60, 0x40);
    battle_quad_place(&battle_graphics->unkA230->unk50[battle_drawing_state.buffer], 0xC2, 0x62, 0x78, 0, 0x60, 0x40);
    battle_quad_place(&battle_graphics->unkA230->unkA0[battle_drawing_state.buffer], 0x96, 0x62, 0, 0, 0x10, 0x40);
    battle_quad_place(&battle_graphics->unkA230->unkF0[battle_drawing_state.buffer], 0x122, 0x62, 0x40, 0, 0x10, 0x40);
    battle_graphics->unkA230->unk668 = battle_drawing_state.buffer;
    battle_graphics->unkA230->unk669 = 1;
    battle_ui->unkB7 = 3;
}

/* 80092298: Set up the combo page's shaded bar and black box primitives, then build
 * its glyphs (from the 800c33b4 table) for every entry of `shown` that is
 * not 0xff into the +0x1e68 list. */
void battle_combo_menu_build_glyphs(u8 member, u8 *shown) {
    s32 i;
    s32 glyph;

    for (i = 0; i < 2; i++) {
        SetPolyG4(&battle_graphics->unkA230->unk5F0[i]);
        (battle_graphics->unkA230->unk5F0 + i)->r0 = 0xFF;
        (battle_graphics->unkA230->unk5F0 + i)->g0 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->b0 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->r1 = 0xFF;
        (battle_graphics->unkA230->unk5F0 + i)->g1 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->b1 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->r2 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->g2 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->b2 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->r3 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->g3 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->b3 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->x0 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->y0 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->x1 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->y1 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->x2 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->y2 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->x3 = 0;
        (battle_graphics->unkA230->unk5F0 + i)->y3 = 0;
    }
    for (i = 0; i < 2; i++) {
        SetPolyF4(&battle_graphics->unkA230->unk638[i]);
        (battle_graphics->unkA230->unk638 + i)->r0 = 0;
        (battle_graphics->unkA230->unk638 + i)->g0 = 0;
        (battle_graphics->unkA230->unk638 + i)->b0 = 0;
        (battle_graphics->unkA230->unk638 + i)->x0 = 0;
        (battle_graphics->unkA230->unk638 + i)->y0 = 0;
        (battle_graphics->unkA230->unk638 + i)->x1 = 0;
        (battle_graphics->unkA230->unk638 + i)->y1 = 0;
        (battle_graphics->unkA230->unk638 + i)->x2 = 0;
        (battle_graphics->unkA230->unk638 + i)->y2 = 0;
        (battle_graphics->unkA230->unk638 + i)->x3 = 0;
        (battle_graphics->unkA230->unk638 + i)->y3 = 0;
    }
    battle_ui->unkFC = 0;
    for (glyph = 0; glyph < 16; glyph++) {
        if (shown[glyph] != 0xFF) {
            battle_ui->unkFC += battle_build_glyph(battle_combo_menu_glyph_ids[glyph], &battle_graphics->unk1E68[battle_ui->unkFC * 2],
                                               battle_combo_menu_glyph_x[glyph], battle_combo_menu_glyph_y[glyph]);
        }
    }
    battle_ui->unkA6 = battle_drawing_state.buffer;
    battle_ui->unk9D = 1;
}

/* 80092784: Build the member's combo page: set up the graphics block, render the name
 * and two-digit AP cost (`costs`) of each of the seven combo steps in `steps`
 * (0xff none) and the fixed eighth entry (battle message 10) into VRAM text
 * images, then the page glyphs and quads. */
void battle_combo_menu_build_page(u8 member, u8 *steps, u8 *costs) {
    RECT nameRect;
    RECT tensRect;
    RECT onesRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect;
    TextImage images[8];
    s32 i;
    u8 tens;
    u32 *digit;
    s16 nameWidth;

    battle_alloc_list_page_block();
    battle_upload_command_name_images();
    battle_blank_text_image = (u32 *)battle_heap_alloc_text_image(0x39);
    bzero((u_char *)battle_blank_text_image, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    battle_upload_image_and_wait(&rect, battle_blank_text_image);
    nameWidth = 30;
    for (i = 0; i < 8; i++) {
        images[i].pixels = (u32 *)battle_heap_alloc_text_image(0x1B);
        bzero((u_char *)images[i].pixels, 0x30C);
        rowRect.x = (i % 2) * 30 + 0x380;
        rowRect.y = (i / 2) * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        battle_upload_image_and_wait(&rowRect, battle_blank_text_image);
        if (i != 7) {
            if (steps[i] != 0xFF) {
                window_render_text_line(text_get_system_resource_entry(battle_party_character_ids[member], steps[i]), images[i].pixels, 0x1B, 0);
                nameRect.x = (i % 2) * 30 + 0x380;
                nameRect.y = (i / 2) * 16 + 0x102;
                nameRect.w = nameWidth;
                nameRect.h = 13;
                battle_upload_image_and_wait(&nameRect, images[i].pixels);
            }
        } else {
            window_render_text_line(text_get_battle_message(10), images[7].pixels, 0x1B, 0);
            nameRect.x = 0x39E;
            nameRect.y = 0x132;
            nameRect.w = nameWidth;
            nameRect.h = 13;
            battle_upload_image_and_wait(&nameRect, images[7].pixels);
        }
        if (!(i & 1)) {
            rightRect.x = 0x3C0;
            rightRect.y = (i / 2) * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            battle_upload_image_and_wait(&rightRect, battle_blank_text_image);
        }
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 16 + 0x102;
        tensRect.w = 6;
        tensRect.h = 13;
        if (i != 7) {
            tens = costs[i] / 10;
            if (tens != 0) {
                digit = battle_digit_text_images[tens].pixels;
            } else {
                digit = battle_blank_text_image;
            }
            battle_upload_image_and_wait(&tensRect, digit);
            onesRect.x = (i % 2) * 16 + 0x3C2;
            onesRect.y = (i / 2) * 16 + 0x102;
            onesRect.w = 6;
            onesRect.h = 13;
            if (steps[i] != 0xFF) {
                battle_upload_image_and_wait(&onesRect, battle_digit_text_images[(u8)(costs[i] % 10)].pixels);
            } else {
                battle_upload_image_and_wait(&onesRect, battle_blank_text_image);
            }
        } else {
            battle_upload_image_and_wait(&tensRect, battle_blank_text_image);
            onesRect.x = 0x3D2;
            onesRect.y = 0x132;
            onesRect.w = 6;
            onesRect.h = 13;
            battle_upload_image_and_wait(&onesRect, battle_blank_text_image);
        }
    }
    battle_combo_menu_build_glyphs(member, steps);
    for (i = 0; i < 8; i++) {
        heap_free(images[i].pixels);
    }
    heap_free(battle_blank_text_image);
    battle_combo_menu_show_list_image();
}

/* 80092B74: Build the combo entry display: the AP count as one or two digit glyphs,
 * the button glyph of each entered step with separators, and the remaining
 * AP bar (4 pixels per point) on the shaded bar and black box. */
void battle_combo_menu_build_entry_display(u8 member, u8 ap) {
    u8 digits[2];
    u8 tens;
    s32 i;

    digits[0] = tens = ap / 10;
    digits[1] = ap - tens * 10;
    battle_ui->unkF8 = 0;
    if (digits[0] != 0) {
        battle_ui->unkF8 += battle_build_glyph(digits[0] + 0x67, &battle_graphics->unkBA8[battle_ui->unkF8 * 2], 0x56, 0x38);
    }
    battle_ui->unkF8 += battle_build_glyph(digits[1] + 0x67, &battle_graphics->unkBA8[battle_ui->unkF8 * 2], 0x5E, 0x38);
    for (i = 0; i < 7; i++) {
        if (battle_turn_state->combo[i] == 0xFF) {
            break;
        }
        battle_ui->unkF8 += battle_build_glyph(battle_combo_step_buttons[i] + 0x39, &battle_graphics->unkBA8[battle_ui->unkF8 * 2], i * 32 + 0x2A, 0x4A);
        if (i != 0) {
            battle_ui->unkF8 += battle_build_glyph(0xA, &battle_graphics->unkBA8[battle_ui->unkF8 * 2],
                                               (i - 1) * 32 + 0x3A, 0x4A);
        }
    }
    battle_ui->unkA5 = battle_drawing_state.buffer;
    battle_ui->unk9C = 1;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->x0 = 0x80;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->y0 = 0x34;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->x1 = ap * 4 + 0x80;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->y1 = 0x34;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->x2 = 0x80;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->y2 = 0x3C;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->x3 = ap * 4 + 0x80;
    (battle_graphics->unkA230->unk5F0 + battle_drawing_state.buffer)->y3 = 0x3C;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->x0 = ap * 4 + 0x80;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->y0 = 0x34;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->x1 = 0xF0;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->y1 = 0x34;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->x2 = ap * 4 + 0x80;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->y2 = 0x3C;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->x3 = 0xF0;
    (battle_graphics->unkA230->unk638 + battle_drawing_state.buffer)->y3 = 0x3C;
    battle_graphics->unkA230->unk66C = battle_drawing_state.buffer;
    battle_graphics->unkA230->unk66F = 1;
}

/* 800930AC: Build the member's gear value page: the four entries' names (gear text
 * gearId * 4 + entry) and up to four-digit values (no leading zeros) into
 * VRAM text images; each shown digit is taken off the value. */
void battle_gear_menu_build_page(u8 member, u8 *present, u16 *values) {
    RECT nameRect;
    RECT onesRect;
    RECT digitRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect;
    TextImage images[4];
    u16 divisors[3];
    u8 shown;
    s32 i;
    s32 j;
    u8 digit;

    divisors[0] = 1000;
    divisors[1] = 100;
    divisors[2] = 10;
    battle_alloc_list_page_block();
    battle_upload_command_name_images();
    battle_blank_text_image = (u32 *)battle_heap_alloc_text_image(0x39);
    bzero((u_char *)battle_blank_text_image, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    battle_upload_image_and_wait(&rect, battle_blank_text_image);
    for (i = 0; i < 4; i++) {
        images[i].pixels = (u32 *)battle_heap_alloc_text_image(0x1B);
        bzero((u_char *)images[i].pixels, 0x30C);
        rowRect.x = 0x380;
        rowRect.y = i * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        battle_upload_image_and_wait(&rowRect, battle_blank_text_image);
        if (present[i] != 0xFF) {
            window_render_text_line(text_get_gear_fuel_art_name(battle_work_area.records[member].pilot.gearId * 4 + i), images[i].pixels, 0x1B, 0);
            nameRect.x = 0x380;
            nameRect.y = i * 16 + 0x102;
            nameRect.w = 30;
            nameRect.h = 13;
            battle_upload_image_and_wait(&nameRect, images[i].pixels);
        }
        rightRect.x = 0x3C0;
        rightRect.y = i * 16 + 0x100;
        rightRect.w = 0x1B;
        rightRect.h = 16;
        battle_upload_image_and_wait(&rightRect, battle_blank_text_image);
        shown = 0;
        for (j = 0; j < 3; j++) {
            digitRect.x = j * 2 + 0x3C0;
            digitRect.y = i * 16 + 0x102;
            digitRect.w = 6;
            digitRect.h = 13;
            digit = values[i] / divisors[j];
            if (digit != 0 || shown) {
                battle_upload_image_and_wait(&digitRect, battle_digit_text_images[digit].pixels);
                shown = 1;
                values[i] -= digit * divisors[j];
            } else {
                battle_upload_image_and_wait(&digitRect, battle_blank_text_image);
            }
        }
        onesRect.x = 0x3C6;
        onesRect.y = i * 16 + 0x102;
        onesRect.w = 6;
        onesRect.h = 13;
        if (present[i] != 0xFF) {
            battle_upload_image_and_wait(&onesRect, battle_digit_text_images[(u16)(values[i] % 10)].pixels);
        } else {
            battle_upload_image_and_wait(&onesRect, battle_blank_text_image);
        }
    }
    for (i = 0; i < 4; i++) {
        heap_free(images[i].pixels);
    }
    heap_free(battle_blank_text_image);
    battle_graphics->unkA230->unk0[battle_drawing_state.buffer].clut = text_plane0_clut;
    battle_quad_place(&battle_graphics->unkA230->unk0[battle_drawing_state.buffer], 0x94, 0x54, 0, 0, 0x60, 0x40);
    battle_quad_place(&battle_graphics->unkA230->unkA0[battle_drawing_state.buffer], 0xFC, 0x54, 0, 0, 0x20, 0x40);
    battle_graphics->unkA230->unk668 = battle_drawing_state.buffer;
    battle_graphics->unkA230->unk669 = 1;
    battle_ui->unkB7 = 4;
}

/* 80093578: Point the gear page `kind` quads at their images: the page title cell, the
 * command's state icon (9 sealed, 7 flag 0x1000, else 8) and its level frame
 * (13 for level 1, 21 for level 2, else 12), each with its CLUT. */
void battle_gear_menu_point_icons(u8 member, u8 kind) {
    u16 state;
    s32 icon;
    s32 frame;

    battle_quad_place(&battle_graphics->unkA230->unk140[battle_drawing_state.buffer], 0x7C, 0xA4, 0, kind * 16, 0x60, 0x10);
    state = battle_work_area.gearCommands[member][kind + 37].state;
    if (state & 0x4000) {
        icon = 9;
    } else {
        icon = 8;
        if (state & 0x1000) {
            icon = 7;
        }
    }
    switch (state & 0xF) {
    case 0:
        frame = 12;
        break;
    case 1:
        frame = 13;
        break;
    case 2:
        frame = 21;
        break;
    default:
        frame = 12;
        break;
    }
    battle_quad_place_text_row(&battle_graphics->unkA230->unk320[battle_drawing_state.buffer], 0x24, 0xA6, battle_icon_cells[icon].u, battle_icon_cells[icon].v,
                  battle_icon_cells[icon].w);
    battle_graphics->unkA230->unk320[battle_drawing_state.buffer].clut = battle_icon_cells[icon].alternate ? text_plane1_clut : text_plane0_clut;
    battle_quad_place_text_row(&battle_graphics->unkA230->unk370[battle_drawing_state.buffer], 0x48, 0xA6, battle_icon_cells[frame].u,
                  battle_icon_cells[frame].v, battle_icon_cells[frame].w);
    battle_graphics->unkA230->unk370[battle_drawing_state.buffer].clut = battle_icon_cells[frame].alternate ? text_plane1_clut : text_plane0_clut;
}

/* 8009382C: Build the name of the member's gear page `kind` (two text lines from the
 * file 3 block) into VRAM and point the page's two title quads at them. */
void battle_gear_menu_build_command_text(u8 member, u8 kind) {
    RECT rect;
    u32 *pixels;
    s32 text;
    s32 width0;
    s32 width1;

    text = (battle_work_area.records[member].pilot.gearId * 4 + kind) * 2;
    pixels = (u32 *)battle_heap_alloc_text_image(0x39);
    bzero((u_char *)pixels, 0x618);
    width0 = window_render_text_line(text_get_resource_entry(battle_command_menu_file3_block, text & 0xFFFF), pixels, 0x39, 0);
    width1 = window_render_text_line(text_get_resource_entry(battle_command_menu_file3_block, (text & 0xFFFF) | 1), pixels, 0x39, 1);
    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x3C;
    rect.h = 13;
    battle_upload_image_and_wait(&rect, pixels);
    battle_quad_place_text_row(&battle_graphics->unkA230->unk280[battle_drawing_state.buffer], 0x88, 0xB6, 0, 0, width0);
    battle_quad_place_text_row(&battle_graphics->unkA230->unk2D0[battle_drawing_state.buffer], 0x88, 0xC6, 0, 0, width1);
    heap_free(pixels);
}

/* 800939CC: Open the member's `kind` page (0-2 the special pages, 3 the item page)
 * when its character has it and no status seals it; the page's graphics
 * are built for the current draw buffer. Otherwise mark the page closed. */
void battle_gear_menu_show_command(u8 member, u8 kind) {
    u16 seals[4];

    seals[0] = battle_command_seal_bits[13];
    seals[1] = battle_command_seal_bits[14];
    seals[2] = battle_command_seal_bits[15];
    seals[3] = battle_command_seal_bits[3];
    if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].flags1A, kind) &&
        !(battle_work_area.records[member].pilot.status7A & seals[kind])) {
        battle_gear_menu_point_icons(member, kind);
        battle_gear_menu_build_command_text(member, kind);
        battle_graphics->unkA230->buffer = battle_drawing_state.buffer;
        battle_graphics->unkA230->unk66B = 1;
    } else {
        battle_graphics->unkA230->unk66B = 0;
    }
}

/* 80093B08: For party character 4 with no command page open: show the ammo of its
 * first and fourth special slots (in a gear: its gear's), each name with its
 * rounds left (hundreds and tens without leading zeros, then the ones digit),
 * in window 0. */
void battle_ammo_window_open(u8 member) {
    RECT nameRect;
    RECT onesRect;
    RECT digitRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect; /* unused */
    TextImage images[2];
    u16 divisors[2];
    u8 slots[2];
    u16 values[2];
    u8 shown;
    s32 i;
    s32 j;
    u8 digit;

    if (battle_party_character_ids[member] == 4 && battle_ui->unkB7 == 0) {
        divisors[0] = 100;
        divisors[1] = 10;
        if (battle_slot_flags[member].unk1 == 0) {
            slots[0] = game_data.characters[battle_party_character_ids[member]].entryItems[0];
            slots[1] = game_data.characters[battle_party_character_ids[member]].entryItems[3];
            values[0] = game_data.ammo[slots[0] - 50];
            values[1] = game_data.ammo[slots[1] - 50];
        } else {
            slots[0] = game_data.gears[game_data.characters[battle_party_character_ids[member]].gearId].partItems[0];
            slots[1] = game_data.gears[game_data.characters[battle_party_character_ids[member]].gearId].partItems[3];
            values[0] = game_data.gearAmmo[slots[0] - 50];
            values[1] = game_data.gearAmmo[slots[1] - 50];
        }
        battle_alloc_list_page_block();
        battle_upload_command_name_images();
        battle_blank_text_image = (u32 *)battle_heap_alloc_text_image(0x39);
        bzero((u_char *)battle_blank_text_image, 0x618);
        for (i = 0; i < 2; i++) {
            images[i].pixels = (u32 *)battle_heap_alloc_text_image(0x1B);
            bzero((u_char *)images[i].pixels, 0x30C);
            rowRect.x = 0x380;
            rowRect.y = i * 16 + 0x100;
            rowRect.w = 0x1B;
            rowRect.h = 16;
            battle_upload_image_and_wait(&rowRect, battle_blank_text_image);
            if (slots[i] != 0) {
                window_render_text_line(text_get_weapon_name(slots[i]), images[i].pixels, 0x1B, 0);
                nameRect.x = 0x380;
                nameRect.y = i * 16 + 0x102;
                nameRect.w = 30;
                nameRect.h = 13;
                battle_upload_image_and_wait(&nameRect, images[i].pixels);
            }
            rightRect.x = 0x3C0;
            rightRect.y = i * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            battle_upload_image_and_wait(&rightRect, battle_blank_text_image);
            shown = 0;
            for (j = 0; j < 2; j++) {
                digitRect.x = j * 2 + 0x3C0;
                digitRect.y = i * 16 + 0x102;
                digitRect.w = 6;
                digitRect.h = 13;
                digit = values[i] / divisors[j];
                if (digit != 0 || shown) {
                    battle_upload_image_and_wait(&digitRect, battle_digit_text_images[digit].pixels);
                    shown = 1;
                    values[i] -= digit * divisors[j];
                } else {
                    battle_upload_image_and_wait(&digitRect, battle_blank_text_image);
                }
            }
            onesRect.x = 0x3C4;
            onesRect.y = i * 16 + 0x102;
            onesRect.w = 6;
            onesRect.h = 13;
            if (slots[i] != 0) {
                battle_upload_image_and_wait(&onesRect, battle_digit_text_images[(u16)(values[i] % 10)].pixels);
            } else {
                battle_upload_image_and_wait(&onesRect, battle_blank_text_image);
            }
        }
        for (i = 0; i < 2; i++) {
            heap_free(images[i].pixels);
        }
        heap_free(battle_blank_text_image);
        battle_graphics->unkA230->unk0[battle_drawing_state.buffer].clut = text_plane0_clut;
        battle_quad_place(&battle_graphics->unkA230->unk0[battle_drawing_state.buffer], 0xA0, 0xAC, 0, 0, 0x60, 0x20);
        battle_quad_place(&battle_graphics->unkA230->unkA0[battle_drawing_state.buffer], 0x104, 0xAC, 0, 0, 0x18, 0x20);
        battle_graphics->unkA230->unk668 = battle_drawing_state.buffer;
        battle_graphics->unkA230->unk669 = 1;
        battle_window_open(0, 0x98, 0xA8, 0x8C, 0x28, 0, 1);
        battle_ui->unkB7 = 5;
        battle_ui->unk8E = 1;
    }
}

/* 8009413C: For party character 4: close its command window and, with `release`,
 * its extra resources. */
void battle_ammo_window_hide(u8 member, u8 release) {
    if (battle_party_character_ids[member] == 4) {
        battle_ui->unkB7 = 0;
        battle_ui->windows[0] = 0;
        if (release != 0) {
            battle_ammo_window_release(member);
        }
    }
}

/* 800941A4: Resolve the committed action: select the attacker and its command
 * descriptor (a gear's goes to 8009c198), then for every target in the
 * target mask run the descriptor's formula, the post-adjustment and the
 * status step its flags ask for; finally publish the command, hold the
 * attacker's commands and raise the used command's use count. Returns the
 * resolve status, except on the two gear paths, which return without a
 * value. */
u8 battle_resolve_action(void) {
    u16 bit;
    u8 member;

    battle_clear_damage_and_results();
    battle_resolve_status = 0;
    battle_attack_item_broken = 0;
    battle_attacker_slot = battle_work_ptr->attackerIndex;
    battle_attacker_record = &battle_work_ptr->records[battle_attacker_slot];
    battle_attacker_gear = &battle_work_ptr->records[battle_attacker_slot].gear;
    if (battle_work_ptr->records[battle_work_ptr->attackerIndex].flags15A & 0x80) {
        battle_resolve_gear_action();
        battle_enable_deathblow_commands(battle_attacker_slot);
        return;
    }
    if (battle_attacker_slot < 3) {
        battle_current_command = &battle_work_ptr->partyCommands[battle_attacker_slot][battle_work_ptr->commandIndex];
    } else {
        battle_current_command = &battle_work_ptr->enemyCommands[battle_work_ptr->commandIndex];
    }
    battle_seal_deathblow_commands(battle_attacker_slot, 1);
    if (battle_current_command->flagsA & 0x10) {
        battle_resolve_gear_action();
        battle_enable_deathblow_commands(battle_attacker_slot);
        battle_add_to_party_damage_totals();
        return;
    }
    battle_ether_check_failed = 0;
    if ((battle_current_command->flagsA & 0x100) && battle_attacker_record->pilot.characterId == 1) {
        battle_resolve_ether_check();
    }
    if (battle_attacker_record->pilot.characterId == 4 && (battle_work_ptr->commandIndex == 4 || battle_work_ptr->commandIndex == 5)) {
        battle_current_command->attributes[2] = battle_attacker_record->pilot.entries[battle_work_ptr->commandIndex - 3].value4;
    }
    if ((battle_attacker_record->pilot.status80 & 0x20) && (battle_committed_action.targets & 7) && battle_committed_action.targets < 7) {
        for (member = 0; member < 3; member++) {
            if (battle_work_area.records[member].pilot.characterId == 3) {
                battle_committed_action.targets = 1 << member;
            }
        }
    }
    bit = 1;
    for (battle_target_slot = 0; battle_target_slot < 11; battle_target_slot++, bit <<= 1) {
        if (bit & battle_work_ptr->targetMask) {
            battle_target_record = &battle_work_ptr->records[battle_target_slot];
            battle_target_gear = &battle_work_ptr->records[battle_target_slot].gear;
            battle_target_attack_level = &battle_work_ptr->records[battle_target_slot].field148;
            if (battle_target_slot < 3) {
                battle_resolve_counterattack();
            }
            battle_formula_table[battle_current_command->formula]();
            battle_resolve_follow_up();
            if (battle_work_ptr->resultCode[battle_target_slot] == 0) {
                if (battle_current_command->flagsA & 0x800) {
                    battle_resolve_weapon_status_after_hit();
                } else if (battle_current_command->flagsA & 0x4000) {
                    battle_formula2_inflict_status();
                } else if (battle_current_command->flagsA & 4) {
                    battle_resolve_cure_after_hit();
                }
            }
            if (battle_current_command->flagsA & 1) {
                battle_work_ptr->shownCommand = battle_current_command->name;
            } else {
                battle_work_ptr->shownCommand = battle_work_ptr->commandIndex;
            }
        }
    }
    battle_publish_current_command();
    battle_enable_deathblow_commands(battle_attacker_slot);
    if (battle_attacker_record->pilot.characterId == 4 && battle_attack_item_broken == 0) {
        battle_wear_weapon_items();
    }
    if (battle_attacker_slot < 3 && battle_work_ptr->commandIndex < 7) {
        if (battle_attacker_record->pilot.useCounts[battle_work_ptr->commandIndex] <= 0xFDE7) {
            battle_attacker_record->pilot.useCounts[battle_work_ptr->commandIndex] +=
                battle_attacker_record->pilot.field55 + battle_attacker_record->pilot.fieldA1;
        }
    }
    battle_add_to_party_damage_totals();
    return battle_resolve_status;
}

/* 800946F4: Per-target follow-up of a party or enemy action. On a hit (result 0): a
 * party attacker's counter +0x3a rises when the damage equals the target's
 * HP; the target loses status80 bit 0x1000 and, on 70%, 0x2000; an ether
 * command (flags 0x100) gets result 2 against flags36 0x4000 (or 0x2000
 * without attribute 2 bits); flags32 0x80 halves (60%, 80% for character
 * 0) or raises damage by half; flags32 0x20 turns it on the attacker; a
 * party attack clears the target's status7c bit 2 (restoring status7a); a
 * blow at least character 3's HP clears every enemy's status80 bit 0x20.
 * Then flags36 0x8000 swaps results 0/5 and 2, status84 0x80 doubles
 * result-2 damage and status88 0x200 nullifies result-1 damage. */
void battle_resolve_follow_up(void) {
    s32 chance;
    u8 slot;

    if (battle_work_ptr->resultCode[battle_target_slot] == 0) {
        if (battle_work_ptr->damage[battle_target_slot] == battle_target_record->pilot.hp && battle_attacker_slot < 3) {
            if (++battle_attacker_record->pilot.field3A > 0xFDE8) {
                battle_attacker_record->pilot.field3A--;
            }
        }
        battle_target_record->pilot.status80 &= ~0x1000;
        if ((battle_target_record->pilot.status80 & 0x2000) && rand() % 100 < 70) {
            battle_target_record->pilot.status80 &= ~0x2000;
        }
        if (battle_current_command->flagsA & 0x100) {
            if (battle_target_record->pilot.flags36 & 0x4000) {
                battle_work_ptr->resultCode[battle_target_slot] = 2;
            }
            if ((battle_target_record->pilot.flags36 & 0x2000) && !(battle_current_command->attributes[2] & 0xF)) {
                battle_work_ptr->resultCode[battle_target_slot] = 2;
            }
        }
        if (battle_target_record->pilot.flags32 & 0x80) {
            chance = 60;
            if (battle_target_record->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                battle_work_ptr->damage[battle_target_slot] >>= 1;
            } else {
                battle_work_ptr->damage[battle_target_slot] += battle_work_ptr->damage[battle_target_slot] >> 1;
            }
        }
        if (battle_target_record->pilot.flags32 & 0x20) {
            battle_work_ptr->resultCode[battle_attacker_slot] = 0;
            battle_work_ptr->damage[battle_attacker_slot] = battle_work_ptr->damage[battle_target_slot];
        }
        if (battle_attacker_slot < 3 && battle_attacker_slot != battle_target_slot) {
            if (battle_work_ptr->records[battle_target_slot].pilot.status7C & 2) {
                battle_work_ptr->records[battle_target_slot].pilot.status7C &= ~2;
                battle_work_ptr->records[battle_target_slot].pilot.status7A = battle_saved_command_seals[battle_target_slot];
            }
        }
        if (battle_target_record->pilot.characterId == 3 && (u32)battle_slot_damages[battle_target_slot] >= battle_target_record->pilot.hp) {
            for (slot = 3; slot < 11; slot++) {
                battle_work_area.records[slot].pilot.status80 &= ~0x20;
            }
        }
    }
    if (battle_target_record->pilot.flags36 & 0x8000) {
        switch (battle_work_ptr->resultCode[battle_target_slot]) {
        case 0:
        case 5:
            battle_work_ptr->resultCode[battle_target_slot] = 2;
            break;
        case 2:
            battle_work_ptr->resultCode[battle_target_slot] = 0;
            break;
        }
    }
    if ((battle_target_record->pilot.status84.half.permanent & 0x80) && battle_work_ptr->resultCode[battle_target_slot] == 2) {
        battle_work_ptr->damage[battle_target_slot] *= 2;
    }
    if ((battle_target_record->pilot.status88.half.permanent & 0x200) && battle_work_ptr->resultCode[battle_target_slot] == 1) {
        battle_work_ptr->damage[battle_target_slot] = 0;
    }
}

/* 80094C78: Add the damage dealt to the target to each hit party member's running
 * total (+0x5f60 with record flag 0x80 at +0x15a, else +0x5f54). */
void battle_add_to_party_damage_totals(void) {
    u8 member;

    for (member = 0; member < 3; member++) {
        if (battle_work_ptr->resultCode[member] == 0) {
            if (battle_work_ptr->records[member].flags15A & 0x80) {
                battle_work_ptr->field5F60[member] += battle_work_ptr->damage[battle_target_slot];
            } else {
                battle_work_ptr->field5F54[member] += battle_work_ptr->damage[battle_target_slot];
            }
        }
    }
}

/* 80094D24: With one party member out of action (status7c 0xc000) and two carrying
 * status7c bit 2, or two and one, clear bit 2 on the party and restore their
 * status7a. */
void battle_clear_status7c_bit2_if_all_out(void) {
    u8 member;
    u8 down;
    u8 marked;

    down = 0;
    for (member = 0; member < 3; member++) {
        if (battle_work_ptr->records[member].pilot.status7C & 0xC000) {
            down++;
        }
    }
    marked = 0;
    for (member = 0; member < 3; member++) {
        if (battle_work_ptr->records[member].pilot.status7C & 2) {
            marked++;
        }
    }
    if (down == 1 && marked == 2) {
        for (member = 0; member < 3; member++) {
            if (battle_work_ptr->records[member].pilot.status7C & 2) {
                battle_work_ptr->records[member].pilot.status7C &= ~2;
                battle_work_ptr->records[member].pilot.status7A = battle_saved_command_seals[member];
            }
        }
    }
    if (down == 2 && marked == 1) {
        for (member = 0; member < 3; member++) {
            if (battle_work_ptr->records[member].pilot.status7C & 2) {
                battle_work_ptr->records[member].pilot.status7C &= ~2;
                battle_work_ptr->records[member].pilot.status7A = battle_saved_command_seals[member];
            }
        }
    }
}

/* 80094EE4: Formula 0 (battle_formula_table[0], physical and ether damage): none against an
 * immune target (flags34 0x8000, 0x4000 for ether); otherwise attack and
 * defense adjusted by both sides' statuses and the command's attributes,
 * scaled 4:3 (5:4 for ether), by the power / 20 for kinds 0-1, randomised,
 * then shaped by the hit outcome and clamped to 0-9999. */
void battle_formula0_deal_damage(void) {
    u16 attack;
    u16 defense;
    s8 hit;
    u8 power;
    s32 attackScale;
    s32 defenseScale;
    s32 amount;
    s32 immune;
    s32 kind;

    power = battle_current_command->power;
    if (battle_current_command->flagsA & 0x100) {
        immune = battle_target_record->pilot.flags34 & 0x4000;
    } else {
        immune = battle_target_record->pilot.flags34 & 0x8000;
    }
    if (immune) {
        battle_work_ptr->resultCode[battle_target_slot] = 0;
        return;
    }
    hit = battle_resolve_hit_outcome();
    attack = battle_resolve_attack_value();
    defense = battle_resolve_defense_value();
    if ((battle_attacker_record->pilot.status88.half.active | battle_attacker_record->pilot.status88.half.permanent) & 8) {
        attack += attack / 5;
    }
    if ((battle_attacker_record->pilot.status88.half.active | battle_attacker_record->pilot.status88.half.permanent) & 2) {
        attack += attack / 10;
    }
    if ((battle_attacker_record->pilot.status88.half.active | battle_attacker_record->pilot.status88.half.permanent) & 4) {
        attack -= attack / 5;
    }
    if ((battle_attacker_record->pilot.status88.half.active | battle_attacker_record->pilot.status88.half.permanent) & 1) {
        attack -= attack / 10;
    }
    if ((battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent) & 4) {
        defense += defense / 5;
    }
    if ((battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent) & 1) {
        defense += defense / 10;
    }
    if ((battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent) & 8) {
        defense -= defense / 5;
    }
    if ((battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent) & 2) {
        defense -= defense / 10;
    }
    if (battle_current_command->attributes[2] & 0x10) {
        if (!(battle_target_record->pilot.status82 & 0x40)) {
            battle_target_record->pilot.status80 |= 0x40;
        }
        if ((battle_attacker_record->pilot.status8C.half.active | battle_attacker_record->pilot.status8C.half.permanent) & 0x4000) {
            battle_slot_result_codes[battle_attacker_slot] = 3;
            battle_slot_damages[battle_attacker_slot] = (u16)(battle_attacker_record->pilot.maxEp / 10) * 2;
        }
        if ((battle_attacker_record->pilot.status8C.half.active | battle_attacker_record->pilot.status8C.half.permanent) & 0x1000) {
            battle_slot_result_codes[battle_attacker_slot] = 2;
            battle_slot_damages[battle_attacker_slot] = (u16)(battle_attacker_record->pilot.maxHp / 10) * 2;
        }
    }
    if ((battle_current_command->attributes[2] & 0x20) && !(battle_target_record->pilot.status82 & 0x80)) {
        battle_target_record->pilot.status80 |= 0x80;
    }
    if (battle_target_record->pilot.status80 & 0x40) {
        defense -= defense >> 2;
        battle_target_record->pilot.status80 &= ~0x40;
    }
    if (battle_attacker_record->pilot.status80 & 0x80) {
        attack -= attack >> 2;
        battle_attacker_record->pilot.status80 &= ~0x80;
    }
    if (battle_current_command->flagsA & 0x400) {
        power = 20;
    }
    battle_resolve_adjust_for_elements(&attack, &defense, &hit);
    attackScale = 5;
    if (battle_current_command->flagsA & 0x100) {
        defenseScale = 4;
    } else {
        attackScale = 4;
        defenseScale = 3;
    }
    if (defense != 0) {
        amount = attackScale * attack - defenseScale * defense;
    } else {
        amount = attackScale * attack;
    }
    kind = battle_current_command->amountKind;
    if (kind >= 0) {
        if (kind < 2) {
            amount = power * amount / 20;
        }
    }
    if (amount <= 0) {
        amount = 0;
    } else if (amount < 10) {
        amount += rand() % 2;
    } else {
        amount += rand() % (amount / 10 + 2);
    }
    switch (hit) {
    case 1:
        if (amount > 0) {
            battle_work_ptr->resultCode[battle_target_slot] = 0;
        } else {
            amount = 1;
            battle_work_ptr->resultCode[battle_target_slot] = 0;
        }
        break;
    case 2:
        amount /= 2;
        battle_work_ptr->resultCode[battle_target_slot] = 5;
        break;
    case 3:
        amount = 0;
        battle_work_ptr->resultCode[battle_target_slot] = 4;
        break;
    case 4:
        battle_work_ptr->resultCode[battle_target_slot] = 2;
        break;
    case 5:
        battle_work_ptr->resultCode[battle_target_slot] = 7;
        break;
    }
    if (battle_ether_check_failed != 0 && (battle_current_command->flagsA & 0x100) && amount != 0) {
        amount /= 3;
    }
    if (amount >= 10000) {
        amount = 9999;
    }
    if (amount < 0) {
        amount = 0;
    }
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 80095690: Formula 1 (battle_formula_table[1]): a heal (result code 2) of the attacker's
 * ether (+0x5b) times the descriptor's +0x11 (doubled with attacker +0x8a bit
 * 0x2000), scaled 0.7 / 1.3 by the target's +0x8c|+0x8e bits 0x100 / 0x200,
 * none for a +0x15a 0x80 target. */
void battle_formula1_heal_by_ether(void) {
    s16 amount = battle_attacker_record->pilot.ether * battle_current_command->power;
    u16 status;

    if (battle_attacker_record->pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    status = battle_target_record->pilot.status8C.half.active | battle_target_record->pilot.status8C.half.permanent;
    if (status & 0x100) {
        amount = amount * 7 / 10;
    }
    if (status & 0x200) {
        amount = amount * 13 / 10;
    }
    if (battle_work_area.records[battle_target_slot].flags15A & 0x80) {
        amount = 0;
    }
    battle_work_ptr->resultCode[battle_target_slot] = 2;
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 800957D8: Formula 6 (battle_formula_table[6]): the target defends: a +0x56 state 2 target
 * first leaves it (8009ac48), its status words clear, result code 2 and a
 * tenth of its +0x4e times the descriptor's +0x11 as the amount; its timer is
 * held (the held mask 800d2c9e is addressed as its own variable here). */
void battle_formula6_revive(void) {
    s32 *amount;
    u8 slot;

    if (battle_work_area.records[battle_target_slot].pilot.characterId == 2) {
        battle_seal_deathblow_commands(battle_target_slot, 1);
    }
    battle_target_record->pilot.status7C = 0;
    battle_target_record->pilot.status80 = 0;
    battle_target_record->pilot.status84.half.active = 0;
    battle_target_record->pilot.status88.half.active = 0;
    battle_target_record->pilot.status8C.half.active = 0;
    battle_slot_result_codes[battle_target_slot] = 2;
    slot = battle_target_slot;
    amount = &battle_slot_damages[slot];
    *amount = (battle_target_record->pilot.maxHp * battle_current_command->power) / 10;
    battle_held_slot_mask |= 1 << slot;
}

/* 800958D8: With the command's chance (+0x1c in percent) and kind 0x6e, clear the
 * target's status words named by the command's bits 0x8000-0x400 (message
 * 0x3a). */
void battle_resolve_cure_after_hit(void) {
    if (rand() % 100 <= battle_current_command->field1C && battle_current_command->field1D == 0x6E) {
        if (battle_current_command->field1E & 0x8000) {
            battle_target_record->pilot.status84.half.active = 0;
        }
        if (battle_current_command->field1E & 0x4000) {
            battle_target_record->pilot.status84.half.permanent = 0;
        }
        if (battle_current_command->field1E & 0x2000) {
            battle_target_record->pilot.status88.half.active = 0;
        }
        if (battle_current_command->field1E & 0x1000) {
            battle_target_record->pilot.status88.half.permanent = 0;
        }
        if (battle_current_command->field1E & 0x800) {
            battle_target_record->pilot.status8C.half.active = 0;
        }
        if (battle_current_command->field1E & 0x400) {
            battle_target_record->pilot.status8C.half.permanent = 0;
        }
        battle_work_ptr->message = 0x3A;
    }
}

/* 80095A78: Formula 2 (battle_formula_table[2]; 800941a4 also runs it after a hit with flagsA
 * 0x4000): status effect of the descriptor on the target (mode +0x11, or 5
 * with +0xa bit 0x4000); a refused status marks the target's result 6. */
void battle_formula2_inflict_status(void) {
    s8 accepted = battle_resolve_status_roll(battle_current_command->field1C, battle_current_command->field1D, battle_current_command->field1E);

    if (battle_current_command->flagsA & 0x4000) {
        battle_status_set_duration(battle_target_slot, battle_current_command->field1D, battle_current_command->field1E, 5);
    } else {
        battle_status_set_duration(battle_target_slot, battle_current_command->field1D, battle_current_command->field1E, battle_current_command->power);
        if (accepted != 1) {
            battle_work_ptr->resultCode[battle_target_slot] = 6;
        }
    }
}

/* 80095B44: When 80097964 accepts the attacker's +2/+3/+0 values, run 800995a0 on the
 * target with the descriptor's +0x1d/+0x1e and mode 5. */
void battle_resolve_weapon_status_after_hit(void) {
    if (battle_resolve_status_roll(battle_attacker_record->pilot.entries[0].value2, battle_attacker_record->pilot.entries[0].value3, battle_attacker_record->pilot.entries[0].field0) == 1) {
        battle_status_set_duration(battle_target_slot, battle_current_command->field1D, battle_current_command->field1E, 5);
    }
}

/* 80095BAC: Formula 5 (battle_formula_table[5]): with the command's chance (+0x1c in percent)
 * clear the target's statuses named by the command's bits (+0x1d: 0x80 the
 * state word except KO/down, 0x40 the timer holds and 0x20 of 7a, 0x20-0x08
 * the active status words); otherwise it misses (result 6). */
void battle_formula5_cure_statuses(void) {
    if (battle_current_command->field1C < rand() % 100) {
        battle_work_ptr->resultCode[battle_target_slot] = 6;
        return;
    }
    if (battle_current_command->field1D & 0x80) {
        battle_target_record->pilot.status7C &= 0xC000;
    }
    if (battle_current_command->field1D & 0x40) {
        battle_target_record->pilot.status80 = 0;
        battle_target_record->pilot.status7A &= ~0x20;
    }
    if (battle_current_command->field1D & 0x20) {
        battle_target_record->pilot.status84.half.active = 0;
    }
    if (battle_current_command->field1D & 0x10) {
        battle_target_record->pilot.status88.half.active = 0;
    }
    if (battle_current_command->field1D & 8) {
        battle_target_record->pilot.status8C.half.active = 0;
    }
}

/* 80095D4C: Formula 3 (battle_formula_table[3]): on a chance roll (the attacker's +0x60 or the
 * command's +0x1c), transfer power / 20 of a maximum (attacker's or target's
 * HP for kinds 0-1, EP for 2-3; kind 5 the target's HP less one) between
 * attacker and target: both slots get the amount, HP kinds with results 2 /
 * 0, EP kinds 3 / 1 unless the target nullifies (status88 0x200). A failed
 * roll or nullified transfer is result 6. */
void battle_formula3_drain_hp_or_ep(void) {
    u8 chance;
    u16 base;
    u16 amount;

    switch (battle_current_command->chanceSource) {
    case 0:
        chance = battle_attacker_record->pilot.field60;
        break;
    case 1:
        chance = battle_current_command->field1C;
        break;
    }
    if (chance < rand() % 100) {
        goto missed;
    }
    switch (battle_current_command->amountKind) {
    case 0:
        base = battle_attacker_record->pilot.maxHp;
        break;
    case 1:
        base = battle_target_record->pilot.maxHp;
        break;
    case 2:
        base = battle_attacker_record->pilot.maxEp;
        break;
    case 3:
        base = battle_target_record->pilot.maxEp;
        break;
    }
    amount = base * battle_current_command->power / 20;
    if (battle_current_command->amountKind == 5) {
        amount = battle_target_record->pilot.hp - 1;
    }
    switch (battle_current_command->amountKind) {
    case 0:
    case 1:
    case 5:
        battle_work_ptr->resultCode[battle_attacker_slot] = 2;
        battle_work_ptr->resultCode[battle_target_slot] = 0;
        battle_work_ptr->damage[battle_attacker_slot] = amount;
        battle_work_ptr->damage[battle_target_slot] = amount;
        break;
    case 2:
    case 3:
        if ((battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent) & 0x200) {
        missed:
            battle_work_ptr->resultCode[battle_target_slot] = 6;
        } else {
            battle_work_ptr->resultCode[battle_attacker_slot] = 3;
            battle_work_ptr->resultCode[battle_target_slot] = 1;
            battle_work_ptr->damage[battle_attacker_slot] = amount;
            battle_work_ptr->damage[battle_target_slot] = amount;
        }
        break;
    }
}

/* 80096018: Formula 4 (battle_formula_table[4]) and gear formula 9
 * (battle_gear_formula_table[9]): chance roll (attacker +0x60 or the descriptor's
 * +0x1c, by +0x18), then the amount by the descriptor's kind +0x1a: target HP /
 * power, HP - 1, the attacker's missing HP, EP * 10, 1, HP, the maximum HP (capped
 * at 9999) or the target's down state (gears refuse it). */
void battle_formula4_deal_damage_by_kind(void) {
    u8 chance;

    switch (battle_current_command->chanceSource) {
    case 0:
        chance = battle_attacker_record->pilot.field60;
        break;
    case 1:
        chance = battle_current_command->field1C;
        break;
    }
    if (chance < rand() % 100) {
        battle_work_ptr->resultCode[battle_target_slot] = 6;
        return;
    }
    battle_work_ptr->resultCode[battle_target_slot] = 0;
    switch (battle_current_command->amountKind) {
    case 0:
        if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
            battle_work_ptr->damage[battle_target_slot] = battle_target_gear->hp / battle_current_command->power;
        } else {
            battle_work_ptr->damage[battle_target_slot] = battle_target_record->pilot.hp / battle_current_command->power;
        }
        break;
    case 1:
        if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
            battle_work_ptr->damage[battle_target_slot] = battle_target_gear->hp - 1;
        } else {
            battle_work_ptr->damage[battle_target_slot] = battle_target_record->pilot.hp - 1;
        }
        break;
    case 2:
        if (battle_work_ptr->records[battle_attacker_slot].flags15A & 0x80) {
            battle_work_ptr->damage[battle_target_slot] = battle_attacker_gear->maxHp - battle_attacker_gear->hp;
        } else {
            battle_work_ptr->damage[battle_target_slot] = battle_attacker_record->pilot.maxHp - battle_attacker_record->pilot.hp;
        }
        break;
    case 3:
        battle_work_ptr->damage[battle_target_slot] = battle_target_record->pilot.ep * 10;
        break;
    case 4:
        battle_work_ptr->damage[battle_target_slot] = 1;
        break;
    case 5:
        battle_work_ptr->damage[battle_target_slot] = battle_target_record->pilot.hp;
        break;
    case 6:
        battle_work_ptr->resultCode[battle_target_slot] = 0;
        if (battle_work_area.records[battle_target_slot].flags15A & 0x80) {
            battle_work_ptr->damage[battle_target_slot] = battle_target_gear->maxHp;
            if (battle_target_gear->maxHp >= 10000) {
                battle_target_gear->maxHp = 9999;
            }
        } else {
            battle_work_ptr->damage[battle_target_slot] = battle_target_record->pilot.maxHp;
            if (battle_target_record->pilot.maxHp >= 10000) {
                battle_target_record->pilot.maxHp = 9999;
            }
        }
        break;
    case 7:
        if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
            battle_work_ptr->resultCode[battle_target_slot] = 6;
            return;
        }
        battle_target_record->pilot.status7C |= 1;
        battle_target_record->pilot.status80 |= 1;
        break;
    }
}

/* Mark the target resistant when its status word has bit 0x2. */
#define STATUS_RESIST(word, resist) \
    do {                            \
        if ((word) & 2) {           \
            (resist) = 1;           \
        }                           \
    } while (0)

/* 80096494: Element adjustment of an attack/defense pair: the command's element (or
 * the attacker's own element status) against the target's weakness, its
 * resistance statuses (which may also force hit result 4) and its single
 * element guards, then a 20% boost for either side's +0x32 bit 0x10.
 * Both target branches test the resist bit with the same statement macro;
 * jump.c turns the first into a bit extract (resist is still 0 there) and
 * keeps the branch in the second. */
void battle_resolve_adjust_for_elements(u16 *attack, u16 *defense, s8 *hit) {
    s8 ether = 0;
    u8 flag = 0;
    s8 resist;
    u8 element;
    u8 bits;
    u8 targetGear;
    u32 status;
    s8 scale;
    s8 defenseScale;
    u32 guards;
    u16 word;
    u8 own;

    targetGear = battle_work_ptr->records[battle_target_slot].flags15A >> 7;
    element = battle_current_command->attributes[2] & 0x3F;
    bits = battle_target_record->pilot.weakness & 0x3F;
    if (!(battle_work_ptr->records[battle_attacker_slot].flags15A >> 7)) {
        status = battle_attacker_record->pilot.status8C.half.active | battle_attacker_record->pilot.status8C.half.permanent;
    } else {
        status = battle_attacker_gear->status84.half.active | battle_attacker_gear->status84.half.permanent;
    }
    own = status >> 12;
    if (battle_current_command->flagsA & 0x100) {
        ether = 1;
    }
    if (element == 0 && own != 0) {
        element = own;
    }
    if (element & own) {
        flag = 1;
    }
    scale = 10;
    defenseScale = 10;
    if (element & bits) {
        scale = 15;
        if (battle_target_record->pilot.weakness & 0x40) {
            scale = 18;
        }
        if (flag) {
            scale += 2;
        }
    }
    resist = 0;
    if (!targetGear) {
        word = battle_target_record->pilot.status8C.half.active | battle_target_record->pilot.status8C.half.permanent;
        bits = (word & 0xF00) >> 8;
        status = word;
        STATUS_RESIST(word, resist);
    } else {
        word = battle_target_gear->status84.half.active | battle_target_gear->status84.half.permanent;
        bits = (word & 0xF00) >> 8;
        status = word;
        STATUS_RESIST(word, resist);
    }
    if ((element & bits) && ether) {
        scale -= 3;
        if (status & 4) {
            scale -= 3;
        }
        if (status & 8) {
            *hit = 4;
        }
    }
    if ((element & bits) && resist) {
        scale -= 3;
        if (status & 4) {
            scale -= 3;
        }
        if (status & 8) {
            *hit = 4;
        }
    }
    guards = bits << 8;
    switch (element) {
    case 8:
        if (guards & 0x400) {
            scale += 3;
        }
        break;
    case 4:
        if (guards & 0x800) {
            scale += 3;
        }
        break;
    case 1:
        if (guards & 0x200) {
            scale += 3;
        }
        break;
    case 2:
        if (guards & 0x100) {
            scale += 3;
        }
        break;
    }
    if (scale <= 0) {
        scale = 1;
    }
    if (defenseScale == 0) {
        defenseScale = 1;
    }
    *attack = *attack * scale / 10;
    *defense = defenseScale * *defense / 10;
    if (battle_attacker_record->pilot.flags32 & 0x10) {
        *attack += *attack / 5U;
    }
    if (battle_target_record->pilot.flags32 & 0x10) {
        *defense += *defense / 5U;
    }
}

/* 80096824: Ether check: unless rand % 100 falls below the attacker's +0x5b plus the
 * descriptor's +0x14, the action fails (code 0x38 at +0x5fc7). */
void battle_resolve_ether_check(void) {
    s32 chance = battle_attacker_record->pilot.ether + battle_current_command->accuracy;

    if (rand() % 100 >= chance) {
        battle_work_ptr->message = 0x38;
        battle_ether_check_failed = 1;
    }
}

/* 800968C0: Counterattack: a target able to act (not down, turn timer free, an enemy
 * or one holding status 0x2000, with the counter status 0x1000 of +0x88)
 * answers a non-ether, non-gear attack with a 60% (character 0) or 50%
 * chance: attacker and target swap and the target's command 20 runs with
 * result 7 (message 0x34). */
void battle_resolve_counterattack(void) {
    s32 chance;
    u8 slot;
    Combatant *record;

    if (battle_target_record->pilot.status7C & 0xA000) {
        return;
    }
    if (battle_target_record->pilot.status80 & 0x1000) {
        return;
    }
    if (battle_attacker_slot < 3 && !(battle_target_record->pilot.status80 & 0x2000)) {
        return;
    }
    if (!(battle_target_record->pilot.status88.half.active & 0x1000)) {
        return;
    }
    if (battle_current_command->flagsA & 0x2000) {
        return;
    }
    if (battle_current_command->flagsA & 0x100) {
        return;
    }
    if (battle_work_ptr->records[battle_attacker_slot].flags15A & 0x80) {
        return;
    }
    chance = 50;
    if (battle_target_record->pilot.characterId == 0) {
        chance = 60;
    }
    if (rand() % 100 <= chance && battle_current_command->formula != 2) {
        slot = battle_attacker_slot;
        battle_attacker_slot = battle_target_slot;
        record = battle_attacker_record;
        battle_attacker_record = battle_target_record;
        battle_target_slot = slot;
        battle_target_record = record;
        battle_current_command = &battle_work_ptr->partyCommands[battle_attacker_slot][20];
        battle_work_ptr->resultCode[battle_attacker_slot] = 7;
        battle_work_ptr->damage[battle_attacker_slot] = 0;
        battle_work_ptr->message = 0x34;
    }
}

/* 80096AB8: Hit outcome of the current command on the target: 1 hit, 2 half, 3 miss,
 * 5 forced. Gear-only and ammo checks (character 4 misses with a command that
 * needs an ammo slot out of rounds, and battle_attack_item_broken keeps 8009afd8 from taking
 * one), sure-hit and never-hit statuses come first; then the attacker's +0x5e
 * plus the command's +0x15 against the target's +0x5f sets a margin for the
 * percent rolls. */
s32 battle_resolve_hit_outcome(void) {
    s16 bonus = 0;
    s16 guard = 0;
    u8 accuracy = battle_attacker_record->pilot.field5E;
    u8 evasion = battle_target_record->pilot.field5F;
    u16 flags;
    u16 status;
    s16 margin;
    s16 roll;

    if ((battle_current_command->flagsA & 0x40) && (battle_work_area.records[battle_target_slot].flags15A & 0x80)) {
        return 3;
    }
    if (battle_attacker_record->pilot.characterId == 4) {
        if ((battle_current_command->itemKinds & 0x80) && game_data.ammo[battle_attacker_record->pilot.entryItems[0] - 50] == 0) {
            battle_attack_item_broken = 1;
            return 3;
        }
        if ((battle_current_command->itemKinds & 0x10) && game_data.ammo[battle_attacker_record->pilot.entryItems[3] - 50] == 0) {
            battle_attack_item_broken = 1;
            return 3;
        }
    }
    if (battle_current_command->flagsA & 0x200) {
        if (battle_target_record->pilot.flags34 & 8) {
            return 3;
        }
        if (battle_work_area.records[battle_target_slot].flags15A & 0x80) {
            return 3;
        }
    }
    flags = battle_current_command->flagsA;
    if (flags & 0x1000) {
        return 3;
    }
    status = battle_target_record->pilot.status84.half.active | battle_target_record->pilot.status84.half.permanent;
    if (status & 0x100) {
        return 3;
    }
    if (battle_target_record->pilot.status7C & 0x2000) {
        return 1;
    }
    if (battle_target_record->pilot.status80 & 0x1000) {
        return 1;
    }
    if (flags & 0x8000) {
        return 1;
    }
    if (flags & 2) {
        return 5;
    }
    if (battle_attacker_record->pilot.status7C & 0x400) {
        bonus -= 50;
    }
    if ((battle_attacker_record->pilot.status84.half.active | battle_attacker_record->pilot.status84.half.permanent) & 0x1000) {
        bonus += 30;
    }
    if (status & 0x800) {
        guard += 50;
    }
    margin = battle_current_command->hitBonus + accuracy - evasion;
    if (battle_work_ptr->records[battle_target_slot].flags15A & 1) {
        if (rand() % 100 < 95) {
            return 2;
        }
        return 1;
    }
    if (status & 0x20) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 3;
        }
        return 1;
    }
    if ((battle_target_record->pilot.status84.half.active | battle_target_record->pilot.status84.half.permanent) & 0x40) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 2;
        }
        return 1;
    }
    roll = rand() % 100 - margin;
    if (roll >= (s16)(bonus - (s16)(guard - 90))) {
        return 3;
    }
    roll = rand() % 100 - margin;
    if (roll >= (s16)(bonus - (s16)(guard - 85))) {
        return 2;
    }
    return 1;
}

/* 80096FBC: Attack value of the current command: the item/part values its +0x10 bits
 * select plus the attack base (gear attack times scale, or the character's
 * +0x58, both scaled by statuses), or the ether value; kind 2 scales either
 * by the power over 20. Party attackers then get their character bonuses
 * and the target's weakness bonuses. */
s16 battle_resolve_attack_value(void) {
    u8 parts[5];
    u16 value;
    u8 i;
    u16 base;
    u16 sum;
    u16 ether;
    u8 kinds;
    u8 scale;
    u16 status;
    u16 flags;

    if (battle_work_ptr->records[battle_attacker_slot].flags15A & 0x80) {
        for (i = 0; i < 3; i++) {
            parts[i] = battle_attacker_gear->entries[i].valueE;
        }
        base = battle_attacker_gear->attack * battle_attacker_gear->attackScale;
        if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 0x1000) {
            base += battle_attacker_gear->attack * 2;
        }
    } else {
        for (i = 0; i < 4; i++) {
            parts[i] = battle_attacker_record->pilot.entries[i].value4;
        }
        base = battle_attacker_record->pilot.attack;
    }
    kinds = battle_current_command->itemKinds;
    ether = battle_attacker_record->pilot.ether;
    sum = 0;
    if (kinds & 0x80) {
        sum = parts[0];
    }
    if (kinds & 0x40) {
        sum += parts[1];
    }
    if (kinds & 0x20) {
        sum += parts[2];
    }
    if (kinds & 0x10) {
        sum += parts[3];
    }
    if (kinds & 0x08) {
        sum += parts[4];
    }
    if ((battle_attacker_record->pilot.status8C.half.active | battle_attacker_record->pilot.status8C.half.permanent) & 1) {
        sum += sum >> 1;
    }
    if (battle_current_command->flagsA & 0x100) {
        scale = 4;
        if ((battle_attacker_record->pilot.status88.half.active | battle_attacker_record->pilot.status88.half.permanent) & 0x8000) {
            scale = 5;
        }
        if (battle_attacker_record->pilot.status80 & 0x400) {
            scale--;
        }
        ether = ether * scale / 4;
        if ((battle_attacker_record->pilot.status88.half.active | battle_attacker_record->pilot.status88.half.permanent) & 0x2000) {
            ether *= 2;
        }
    } else if (!(battle_work_ptr->records[battle_attacker_slot].flags15A & 0x80)) {
        status = battle_attacker_record->pilot.status84.half.active | battle_attacker_record->pilot.status84.half.permanent;
        scale = 4;
        if (status & 0x2000) {
            scale = 5;
        }
        if (battle_attacker_record->pilot.status7C & 0x200) {
            scale--;
        }
        if (status & 0x400) {
            scale += 10 - battle_attacker_record->pilot.hp / (u16)(battle_attacker_record->pilot.maxHp / 10);
        }
        base = base * scale / 4;
    }
    flags = battle_current_command->flagsA;
    if (flags & 0x20) {
        base = 0;
    }
    switch (battle_current_command->amountKind) {
    case 0:
        value = sum + base;
        break;
    case 1:
        value = ether;
        break;
    case 2:
        if (battle_attacker_record->pilot.characterId == 4) {
            sum = sum * 6 / 10;
        }
        if (flags & 0x100) {
            value = ether * battle_current_command->power / 20;
        } else {
            value = (sum + base) * battle_current_command->power / 20;
        }
        break;
    }
    if (battle_attacker_slot < 3) {
        if (battle_attacker_record->pilot.characterId == 7) {
            battle_apply_character7_damage_bonus(&value);
        }
        if (battle_attacker_record->pilot.characterId == 4 || (battle_current_command->attributes[2] & 0x20)) {
            if (battle_target_record->pilot.weakness & 0x20) {
                value += value >> 2;
            }
            if ((*(u32 *)&battle_target_record->pilot.weakness & 0x60) == 0x60) {
                value += value >> 2;
            }
        }
        if (battle_attacker_record->pilot.characterId == 8 && !(battle_work_area.records[battle_attacker_slot].flags15A & 0x80) &&
            (battle_current_command->flagsA & 0x100)) {
            value = value * battle_work_area.records[battle_attacker_slot].gear.frameFactor / 4;
        }
        if (battle_attacker_record->pilot.characterId == 10) {
            value += value / 5;
        }
        if ((battle_current_command->attributes[2] & 0x10) && (battle_target_record->pilot.weakness & 0x10)) {
            value += value >> 2;
            if (battle_target_record->pilot.weakness & 0x10) {
                value += value >> 2;
            }
        }
    }
    return value;
}

/* 80097610: Defense value of the target against the current command: body defense
 * plus armor (1.5x with status 0x100), the ether defense (1.5x with status
 * 0x4000 of +0x88) or the body alone, by the command's +0x1b. A character
 * target's +0x32 bits then scale it by the party members down. */
s16 battle_resolve_defense_value(void) {
    u16 armor;
    u16 body;
    u16 ether;
    u16 value;
    u8 downed;
    u8 i;

    if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
        body = battle_target_gear->bodyDefense;
    } else {
        armor = battle_target_record->pilot.defense;
        body = battle_target_record->pilot.bodyDefense;
    }
    ether = battle_target_record->pilot.etherDefense;
    if (battle_current_command->flagsA & 0x100) {
        if ((battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent) & 0x4000) {
            ether = ether * 3 / 2;
        }
    } else if (!(battle_work_ptr->records[battle_target_slot].flags15A & 0x80)) {
        if ((battle_target_record->pilot.status84.half.active | battle_target_record->pilot.status84.half.permanent) & 0x100) {
            armor = armor * 3 / 2;
        }
    }
    if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
        armor = 0;
    }
    switch (battle_current_command->defenseKind) {
    case 0:
        value = body + armor;
        break;
    case 1:
        value = ether;
        break;
    case 2:
        value = body;
        break;
    }
    if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
        return value;
    }
    downed = 0;
    for (i = 0; i < 3; i++) {
        if (battle_work_ptr->records[i].pilot.status7C & 0x8000) {
            downed++;
        }
    }
    if ((battle_target_record->pilot.flags32 & 4) && downed != 0 && !(battle_current_command->flagsA & 0x100)) {
        value = value * (4 - downed) / 4;
    }
    if ((battle_target_record->pilot.flags32 & 2) && downed != 0 && !(battle_current_command->flagsA & 0x100)) {
        value = value * (downed + 2) / 2;
    }
    if ((battle_target_record->pilot.flags32 & 1) && downed != 0) {
        value = value * (downed + 2) / 2;
    }
    return value;
}

/* 8009795C: Empty; nothing in the overlay calls it. */
void battle_resolve_empty_unreferenced(void) {
}

/* 80097964: Roll a status onto the target (never a gear): with the chance in percent,
 * check the kind's immunities and clear the statuses it overrides, then set
 * the flag bits in the kind's status word and show its message. Returns 1
 * when the status took (or cancelled its opposite). */
s8 battle_resolve_status_roll(u8 chance, u8 kind, u16 flags) {
    Combatant *statusRecord;

    if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
        return 0;
    }
    if (chance < rand() % 100) {
        return 0;
    }
    switch (kind) {
    case 0: {
        u16 immuneFlags = flags & 0xFFFD;
        if (immuneFlags & battle_target_record->pilot.status7E) {
            return 0;
        }
        if (flags & 0x1000) {
            battle_target_record->pilot.status84.half.active &= 0x7FFF;
        }
        break;
    }
    case 2:
        if (flags & battle_target_record->pilot.status82) {
            return 0;
        }
        break;
    case 5:
        if ((battle_target_record->pilot.status7C | battle_target_record->pilot.status7E) & 1) {
            return 0;
        }
        if (flags & 0x8000) {
            battle_target_record->pilot.status7C &= 0xEFFF;
        }
        break;
    case 7:
        if ((battle_target_record->pilot.status80 | battle_target_record->pilot.status82) & 1) {
            return 0;
        }
        if (flags & 0xA) {
            if (!(battle_target_record->pilot.status88.half.active & 5)) {
                break;
            }
            battle_target_record->pilot.status88.half.active &= 0xFFFA;
            return 1;
        }
        if (flags & 5) {
            if (!(battle_target_record->pilot.status88.half.active & 0xA)) {
                break;
            }
            battle_target_record->pilot.status88.half.active &= 0xFFF5;
            return 1;
        }
    case 9:
        if (flags & 0xF000) {
            if (battle_target_record->pilot.status8C.half.permanent & 0xF000) {
                battle_work_ptr->message = 0x39;
                return 0;
            }
            battle_target_record->pilot.status8C.half.active &= 0xFFF;
        }
        if (flags & 0xF00) {
            if (battle_target_record->pilot.status8C.half.permanent & 0xF00) {
                battle_work_ptr->message = 0x39;
                return 0;
            }
            battle_target_record->pilot.status8C.half.active &= 0xF0FF;
        }
        break;
    }
    switch (kind) {
    case 0:
        battle_target_record->pilot.status7C = (flags | battle_target_record->pilot.status7C) & 0xFFFD;
        if (flags & 2) {
            if ((u8)battle_can_put_member_out_of_action()) {
                battle_target_record->pilot.status7C |= flags;
                battle_work_ptr->message = 0x31;
                statusRecord = battle_target_record;
                statusRecord->pilot.status7A = 0xFFEF;
            } else {
                battle_work_ptr->message = 0x32;
            }
        }
        break;
    case 2:
        battle_target_record->pilot.status80 |= flags;
        if (flags & 0x800) {
            statusRecord = battle_target_record;
            statusRecord->pilot.status7A |= 0x20;
        }
        break;
    case 5:
    case 7:
    case 9:
        /* Status halfwords from +0x7A: kinds 5/7/9 select +0x84/88/8C. */
        statusRecord = battle_target_record;
        statusRecord = (Combatant *)((u8 *)statusRecord + kind * 2);
        statusRecord->pilot.status7A |= flags;
        break;
    }
    battle_status_show_message(kind, flags);
    return 1;
}

/* 80097D08: Clear the per-slot damage and result codes (12 entries). */
void battle_clear_damage_and_results(void) {
    s16 slot = 11;

    do {
        battle_work_ptr->resultCode[slot] = 0xFF;
        battle_work_ptr->damage[slot] = 0;
    } while (--slot != -1);
}

/* 80097D5C: Derive each present party member's battle stats: keep the base values,
 * add the equipment bonuses and percentage HP/EP bonuses (capped at 999 and
 * 99), total the gear's equipment, set its fuel cost (command 37's shown
 * value), the attack level limit and scale, the status-driven command rate
 * changes and the character-specific adjustments; then count the members. */
void battle_derive_party_stats(void) {
    u8 member;
    u8 i;
    u8 *level;
    u8 rate;
    u16 immunities;
    CharacterRecord *pilot;
    Combatant *record;
    GearRecord *gear;

    for (member = 0; member < 3; member++) {
        if (battle_party_character_ids[member] == 0x7F) {
            continue;
        }
        battle_attacker_record = &battle_work_ptr->records[member];
        battle_attacker_gear = &battle_work_ptr->records[member].gear;
        level = &battle_work_ptr->records[member].field148;
        if (battle_party_character_ids[member] == 9) {
            game_data.characters[9].characterId = 9;
            battle_attacker_record->pilot.characterId = 9;
        }
        if (battle_party_character_ids[member] == 10) {
            game_data.characters[10].characterId = 10;
            battle_attacker_record->pilot.characterId = 10;
        }
        if (battle_attacker_record->pilot.characterId == 4) {
            battle_work_ptr->savedStats[member][0] = battle_attacker_record->pilot.entries[0].value4 + battle_attacker_record->pilot.entries[3].value4;
        } else {
            battle_work_ptr->savedStats[member][0] = battle_attacker_record->pilot.attack + battle_attacker_record->pilot.entries[0].value4;
        }
        battle_work_ptr->savedStats[member][1] = battle_attacker_record->pilot.field5E;
        battle_work_ptr->savedStats[member][2] = battle_attacker_record->pilot.defense + battle_attacker_record->pilot.bodyDefense;
        battle_work_ptr->savedStats[member][3] = battle_attacker_record->pilot.field5F;
        battle_work_ptr->savedStats[member][4] = battle_attacker_record->pilot.ether;
        battle_work_ptr->savedStats[member][5] = battle_attacker_record->pilot.etherDefense;
        battle_work_ptr->savedStats[member][6] = battle_attacker_record->pilot.speed;
        pilot = &battle_attacker_record->pilot;
        battle_work_ptr->expTotals[member][0] = pilot->expTotalA;
        battle_work_ptr->expTotals[member][1] = pilot->expTotalB;
        battle_work_ptr->savedMax[member][0] = pilot->maxHp;
        battle_work_ptr->savedMax[member][1] = pilot->maxEp;
        pilot->attack += pilot->equipAttack;
        pilot->flags34 = 0;
        battle_attacker_record->pilot.defense += battle_attacker_record->pilot.equipDefense;
        battle_attacker_record->pilot.speed += battle_attacker_record->pilot.equipSpeed;
        battle_attacker_record->pilot.ether += battle_attacker_record->pilot.equipEther;
        battle_attacker_record->pilot.etherDefense += battle_attacker_record->pilot.equipEtherDefense;
        battle_attacker_record->pilot.field5E += battle_attacker_record->pilot.equip5E;
        battle_attacker_record->pilot.field5F += battle_attacker_record->pilot.equip5F;
        if (battle_attacker_record->pilot.speed > 16) {
            battle_attacker_record->pilot.speed = 16;
        }
        if (battle_attacker_record->pilot.flags32 & 0x100) {
            battle_attacker_record->pilot.field5E += battle_attacker_record->pilot.field5E >> 2;
            battle_attacker_record->pilot.field5F += battle_attacker_record->pilot.field5F >> 2;
        }
        battle_attacker_record->pilot.hp += battle_attacker_record->pilot.maxHp * battle_attacker_record->pilot.hpBonus / 20;
        battle_attacker_record->pilot.ep += battle_attacker_record->pilot.maxEp * battle_attacker_record->pilot.epBonus / 20;
        battle_attacker_record->pilot.maxHp += battle_attacker_record->pilot.maxHp * battle_attacker_record->pilot.hpBonus / 20;
        battle_attacker_record->pilot.maxEp += battle_attacker_record->pilot.maxEp * battle_attacker_record->pilot.epBonus / 20;
        if (battle_attacker_record->pilot.hp >= 1000) {
            battle_attacker_record->pilot.hp = 999;
        }
        if (battle_attacker_record->pilot.ep >= 100) {
            battle_attacker_record->pilot.ep = 99;
        }
        if (battle_attacker_record->pilot.maxHp >= 1000) {
            battle_attacker_record->pilot.maxHp = 999;
        }
        if (battle_attacker_record->pilot.maxEp >= 100) {
            battle_attacker_record->pilot.maxEp = 99;
        }
        record = &battle_work_ptr->records[member];
        record->expWeightA = 5;
        record->expWeightB = 5;
        level[0] = 0;
        gear = battle_attacker_gear;
        gear->bodyDefense += gear->equipBodyDefense;
        gear->armor += gear->equipArmor;
        gear->field68 += gear->equip68a + gear->equip68b;
        gear->guard += gear->equipGuard;
        battle_attacker_gear->hitBonus += battle_attacker_gear->equipHitBonus;
        battle_attacker_gear->speed += battle_attacker_gear->equipSpeed - battle_attacker_gear->speedPenalty;
        battle_attacker_gear->frameFactor += battle_attacker_gear->equipFrameFactor;
        rate = battle_attacker_gear->field4F;
        if (rate != 0 && battle_attacker_record->pilot.gearId != 0x12) {
            battle_work_ptr->gearCommands[member][37].hudState = battle_attacker_gear->maxHp / 10 * rate * 2 / 9;
            battle_work_ptr->gearCommands[member][37].hudState /= 10;
            battle_work_ptr->gearCommands[member][37].hudState *= 10;
        }
        level[1] = 0;
        if (game_data.skills[battle_attacker_record->pilot.characterId].unlocksA & 0x1C00) {
            level[1] = 1;
        }
        if (game_data.skills[battle_attacker_record->pilot.characterId].unlocksA & 0x380) {
            level[1] = 2;
        }
        if (game_data.skills[battle_attacker_record->pilot.characterId].unlocksA & 0x70) {
            level[1] = 3;
        }
        battle_attacker_gear->attackScale = battle_attacker_gear->field74;
        battle_attacker_gear->field3E = battle_attacker_gear->field74;
        battle_attacker_gear->attackScale += battle_attacker_gear->equipAttackScale;
        battle_attacker_gear->field3E += battle_attacker_gear->equipAttackScale;
        if (battle_attacker_record->pilot.status88.half.permanent & 0x2000) {
            for (i = 0; i < 38; i++) {
                battle_work_ptr->partyCommands[member][i].cost *= 2;
            }
            for (i = 0; i < 42; i++) {
                battle_work_ptr->gearCommands[member][i].cost *= 2;
            }
        }
        if (battle_attacker_record->pilot.flags32 & 0x4000) {
            for (i = 0; i < 38; i++) {
                battle_work_ptr->partyCommands[member][i].cost = (battle_work_ptr->partyCommands[member][i].cost + 1) >> 1;
            }
            for (i = 0; i < 42; i++) {
                battle_work_ptr->gearCommands[member][i].cost = (battle_work_ptr->gearCommands[member][i].cost + 1) >> 1;
            }
        }
        if (battle_attacker_record->pilot.level >= 50) {
            game_data.skills[battle_attacker_record->pilot.characterId].unlocksA |= 8;
        }
        if (battle_attacker_record->pilot.characterId == 7) {
            battle_attacker_gear->hp = battle_attacker_record->pilot.hp * 50;
            battle_attacker_gear->maxHp = battle_attacker_record->pilot.maxHp * 50;
            battle_attacker_gear->attack = battle_attacker_record->pilot.attack;
            battle_attacker_gear->bodyDefense = battle_attacker_record->pilot.defense * 12;
            battle_attacker_gear->armor = battle_attacker_record->pilot.etherDefense * 6;
            battle_attacker_gear->speed = battle_attacker_record->pilot.speed;
            immunities = battle_attacker_gear->field7E;
            battle_attacker_gear->field7E = immunities | 0x3C4;
            if (battle_attacker_record->pilot.status82 & 0x2000) {
                battle_attacker_gear->field7E = immunities | 0x13C4;
            }
        }
        if (battle_attacker_record->pilot.gearId == 0xF) {
            game_data.skills[battle_attacker_record->pilot.characterId].flags1A &= 0x8FFF;
        }
        if (battle_attacker_record->pilot.gearId == 0x12) {
            battle_attacker_record->pilot.status7A = 0x238;
            battle_attacker_gear->fuel = 0x26AC;
            battle_attacker_gear->maxFuel = 0x26AC;
            level[1] = 0;
            game_data.skills[3].flags1A = 0x8000;
        }
        if (game_data.vars[0] >= 231 && !(game_data.flags2355 & 0x80)) {
            game_data.characters[9].weapons[0] = 0x27;
            game_data.characters[9].entries[0].value4 = 0x1E;
            game_data.flags2355 |= 0x80;
        }
    }
    battle_party_member_count = 3;
    for (member = 0; member < 3; member++) {
        if (battle_party_character_ids[member] == 0x7F) {
            battle_party_member_count--;
        }
    }
    for (member = 0; member < 3; member++) {
        battle_work_ptr->field5F54[member] = 0;
        battle_work_ptr->field5F60[member] = 0;
    }
}

/* 8009892C: Party adjustments at battle start: keep each present member's status
 * word 7A, replace the listed part speeds of its gear by the parts' own
 * (at most 16), set character 8's speed and battle flag, and before game
 * data word 0x1930 reaches 0xbb set the early gears' values. */
void battle_adjust_party_at_start(void) {
    u8 member;
    u8 i;
    Combatant *record;

    for (member = 0; member < 3; member++) {
        if (battle_party_character_ids[member] == 0x7F) {
            continue;
        }
        record = &battle_work_ptr->records[member];
        battle_attacker_record = record;
        battle_attacker_gear = &record->gear;
        battle_saved_command_seals[member] = record->pilot.status7A;
        for (i = 0; i < 4; i++) {
            if (battle_scene_part_speeds[i] != 0) {
                battle_attacker_gear->speed -= battle_scene_part_speeds[i];
                battle_attacker_gear->speed += battle_attacker_gear->speedBonus[i];
            }
        }
        if (battle_attacker_gear->speed > 16) {
            battle_attacker_gear->speed = 16;
        }
    }
    game_data.characters[8].speed = 7;
    if (game_data.skills[8].flags1A & 0x2000) {
        game_data.skills[8].levelSkills |= 0x800;
    }
    if (game_data.vars[0] < 0xBB) {
        game_data.gears[0].field74 = 10;
        game_data.gears[1].field74 = 10;
        game_data.gears[11].field74 = 9;
        game_data.gears[12].field74 = 9;
        game_data.gears[13].field74 = 8;
        game_data.gears[14].field74 = 12;
        game_data.gears[15].field74 = 12;
        game_data.gears[13].engine = 0x58;
        game_data.gears[7].field3 = 0;
        game_data.gears[15].frame = 0x28;
    }
}

/* 80098AF8: A slot's turn timer from its speed (party: less the current command's
 * weight, gear speed for a slot in gear), capped, with a random -3..4
 * spread. Sets the attacker globals and, for the party, the command
 * descriptor. Defined old-style: callers pass the slot unnarrowed. */
s32 battle_compute_turn_timer(slot, mode)
u8 slot;
s32 mode;
{
    Combatant *record = &battle_work_ptr->records[slot];
    u16 speed;

    battle_attacker_gear = &record->gear;
    battle_attacker_record = record;
    if (slot < 3) {
        if (!(record->flags15A & 0x80)) {
            battle_current_command = &battle_work_ptr->partyCommands[slot][battle_work_ptr->commandIndex];
            if (record->pilot.speed > battle_current_command->weight) {
                speed = (record->pilot.speed - battle_current_command->weight) * 9;
            } else {
                speed = 9;
            }
        } else {
            battle_current_command = &battle_work_ptr->gearCommands[slot][battle_work_ptr->commandIndex];
            if (record->gear.speed > battle_current_command->weight) {
                speed = (record->gear.speed - battle_current_command->weight) * 9;
            } else {
                speed = 9;
            }
        }
    } else {
        speed = record->pilot.speed * 9;
    }
    if (speed >= 0xA6) {
        speed = 0xA0;
    }
    speed = 0xA5 - speed;
    speed -= rand() % 8 - 4;
    battle_clear_status7c_bit2_if_all_out();
    return (u8)speed;
}

/* 80098C6C: Resolve an item/effect `param` on every slot in the +0x5fac mask, then
 * set its animation from the effect table. */
void battle_resolve_item_effect(u16 param) {
    s32 slot;
    s32 bit;

    battle_clear_damage_and_results();
    bit = 1;
    for (slot = 0; (u8)slot < 11; slot++) {
        if (bit & battle_work_ptr->targetMask) {
            battle_apply_item_effect_to_slot(slot, param);
        }
        bit <<= 1;
    }
    battle_work_ptr->shownCommand = battle_item_effects[param & 0xFF].animation;
    if (battle_committed_action.held & 0x8000) {
        battle_committed_action.animation = 0xC2;
    }
}

/* 80098D2C: Apply item effect `param` to `slot`: HP (amount x 50, doubled with status
 * 0x80 of +0x86) and EP (amount x 10) restoration, and unless either ran,
 * the status cures, revival (amount tenths of the maximum HP), status
 * grants, immunities and the special effects of flags value 1. The item
 * effect table lies inside the battle work area (+0x5518, battle_item_effects). */
void battle_apply_item_effect_to_slot(u8 slot, u8 param) {
    u8 restored;
    Combatant *record;
    ItemEffect *effect;
    GearRecord *gear;
    u8 i;
    s32 hp;
    s32 hpUnit;
    s32 epUnit;

    restored = 0;
    record = &battle_work_area.records[slot];
    effect = &((ItemEffect *)&battle_work_area.lists)[param + 2];
    gear = &battle_work_area.records[slot].gear;
    epUnit = 10;

    if (effect->flags & 0x8000) {
        hpUnit = 50;
        hp = effect->amount * hpUnit;
        battle_slot_result_codes[slot] = 2;
        battle_work_area.damage[slot] = hp;
        if (record->pilot.status84.half.permanent & 0x80) {
            battle_work_ptr->damage[slot] *= 2;
        }
        if (record->pilot.flags36 & 0x8000) {
            battle_slot_result_codes[slot] = 0;
        }
        restored = 1;
    }
    if (effect->flags & 0x4000) {
        battle_slot_damages[slot] = epUnit * effect->amount;
        battle_slot_result_codes[slot] = 3;
        restored++;
    }
    if (restored) {
        return;
    }
    if ((battle_work_area.records[slot].flags15A & 0x80) && effect->flags != 1) {
        battle_committed_action.message = 0x30;
        battle_committed_action.held |= 0x8000;
        return;
    }
    if (effect->flags & 0x2000) {
        record->pilot.status7C &= 0x8000;
    }
    if (effect->flags & 0x1000) {
        record->pilot.status80 = 0;
        record->pilot.status7A &= 0xFFDF;
    }
    if (effect->flags & 0x100) {
        if (battle_work_area.records[slot].pilot.characterId == 2) {
            battle_seal_deathblow_commands(slot, 0);
        }
        record->pilot.status7C = 0;
        record->pilot.status80 = 0;
        record->pilot.status84.half.active = 0;
        record->pilot.status88.half.active = 0;
        record->pilot.status8C.half.active = 0;
        record->pilot.hp = record->pilot.maxHp * effect->amount / 10;
        battle_work_ptr->revived |= 1 << slot;
    }
    if (effect->flags & 0x800) {
        record->pilot.status84.half.active |= effect->status;
        battle_status_set_duration(slot, 5, effect->status, 5);
        battle_status_show_message(5, effect->status);
    }
    if (effect->flags & 0x400) {
        record->pilot.status88.half.active |= effect->status;
        battle_status_set_duration(slot, 7, effect->status, 5);
        battle_status_show_message(7, effect->status);
    }
    if (effect->flags & 0x200) {
        record->pilot.status8C.half.active |= effect->status;
        if ((effect->status & 0xF000) && !(record->pilot.status8C.half.permanent & 0xF000)) {
            record->pilot.status8C.half.active &= 0xFFF;
            record->pilot.status8C.half.active |= effect->status;
            battle_status_set_duration(slot, 9, effect->status, 5);
            battle_status_show_message(9, effect->status);
        }
        if ((effect->status & 0xF00) && !(record->pilot.status8C.half.permanent & 0xF00)) {
            record->pilot.status8C.half.active &= 0xF0FF;
            record->pilot.status8C.half.active |= effect->status;
            battle_status_set_duration(slot, 9, effect->status, 5);
            battle_status_show_message(9, effect->status);
        }
    }
    if (effect->flags & 0x20) {
        record->pilot.status7E |= 0x3F7C;
    }
    if (effect->flags & 0x10) {
        record->pilot.status7E |= 0xFFFE;
    }
    if (effect->flags & 8) {
        (&record->pilot.status84.half.active)[effect->amount] = 0;
        switch (effect->amount) {
        case 0:
            battle_work_ptr->message = 0x35;
            break;
        case 2:
            battle_work_ptr->message = 0x36;
            break;
        case 4:
            battle_work_ptr->message = 0x37;
            break;
        }
    }
    if ((effect->flags & 0x80) && (effect->status & 2)) {
        if (slot >= 3) {
            battle_work_ptr->message = 0x33;
            return;
        }
        if (effect->duration == 0) {
            if ((u8)battle_can_put_member_out_of_action()) {
                record->pilot.status7C |= effect->status;
                battle_work_ptr->message = 0x31;
                record->pilot.status7A = 0xFFEF;
            } else {
                battle_work_ptr->message = 0x32;
            }
        } else {
            record->pilot.status7C &= ~effect->status;
            record->pilot.status7A = battle_saved_command_seals[slot];
        }
    }
    if (effect->flags == 1) {
        switch (effect->amount) {
        case 10:
            record->pilot.weakness = 0;
            record->pilot.weakness = effect->status;
            break;
        case 11:
            if (!(record->pilot.status7E & effect->status)) {
                record->pilot.status7C |= effect->status;
                battle_status_set_duration(slot, 0, effect->status, effect->duration);
                battle_status_show_message(0, effect->status);
            }
            break;
        case 12:
            if (!(record->pilot.status82 & effect->status)) {
                record->pilot.status80 |= effect->status;
                battle_status_set_duration(slot, 2, effect->status, effect->duration);
                battle_status_show_message(2, effect->status);
            }
            break;
        case 13:
            gear->defense += effect->status;
            battle_work_ptr->message = 0x28;
            break;
        case 14:
            game_data.characters[record->pilot.characterId].expNextA = 1;
            game_data.characters[record->pilot.characterId].expNextB = 1;
            break;
        case 15:
            for (i = 0; i < 7; i++) {
                record->pilot.useCounts[i] += 10;
            }
            break;
        }
    }
}

/* 80099498: Party gate on the formation mode: 1 when mode 2 has no member with status
 * bits 0xC002, or mode 3 does not have exactly two; otherwise 0. */
s32 battle_can_put_member_out_of_action(void) {
    s32 result = 0;
    u8 i;
    u8 count;

    switch (battle_party_member_count) {
    case 2:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (battle_work_ptr->records[i].pilot.status7C & 0xC002) {
                count++;
            }
        }
        if (count == 0) {
            result = 1;
        }
        break;
    case 3:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (battle_work_ptr->records[i].pilot.status7C & 0xC002) {
                count++;
            }
        }
        if (count != 2) {
            result = 1;
        }
        break;
    case 1:
        break;
    }
    return result;
}

/* 800995A0: Store a timed status's duration in slot's timer table. The status is named by
 * its kind and flag bit; the attacker's flag 0x2000 doubles the amount, and for
 * kinds 5, 7 and 9 the target's status bits 0x5 double and 0xA halve it, while
 * the slot's flag 0x40 doubles it again. Unknown statuses are ignored. */
void battle_status_set_duration(u8 slot, u8 kind, u16 flag, u8 amount) {
    Combatant *record;
    u8 index = 0xF;
    u16 state;

    if (battle_work_area.records[battle_attacker_slot].pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    record = &battle_work_area.records[slot];
    if (kind == 0) {
        index = (flag == 0x1000) ? 1 : (flag == 0x2000) ? 0 : 0xF;
    }
    if (kind == 2) {
        switch (flag) {
        case 0x1000:
            index = 2;
            break;
        case 0x800:
            index = 3;
            break;
        }
    }
    if (kind == 5) {
        switch (flag) {
        case 0x8000:
            index = 4;
            break;
        case 0x4000:
            index = 5;
            break;
        }
    }
    if (kind == 7) {
        switch (flag) {
        case 0x8000:
            index = 9;
            break;
        case 0x4000:
            index = 0xA;
            break;
        case 0x1000:
            index = 0xB;
            break;
        }
    }
    if (kind == 9) {
        switch (flag) {
        case 0x1000:
        case 0x2000:
        case 0x4000:
        case 0x8000:
            index = 0xC;
            break;
        case 0x100:
        case 0x200:
        case 0x400:
        case 0x800:
            index = 0xD;
            break;
        }
    }
    if (index == 0xF) {
        return;
    }
    switch (kind) {
    case 5:
    case 7:
    case 9:
        state = battle_target_record->pilot.status88.half.active | battle_target_record->pilot.status88.half.permanent;
        if (state & 5) {
            amount *= 2;
        }
        if (state & 0xA) {
            amount /= 2;
        }
        break;
    }
    if (record->pilot.flags32 & 0x40) {
        switch (kind) {
        case 5:
        case 7:
        case 9:
            amount *= 2;
            break;
        }
    }
    battle_work_area.records[slot].statusTimers[index] = amount;
}

/* 80099890: Count down slot's timed statuses at the start of its turn, clearing each one
 * whose timer runs out; a slot in a gear counts down its gear's statuses
 * instead (80099CF0). Returns a mask of the statuses that ended. */
u16 battle_status_count_down(u8 slot) {
    Combatant *record = &battle_work_area.records[slot];
    GearRecord *gear = &battle_work_area.records[slot].gear;
    volatile u8 *timers = battle_work_area.records[slot].statusTimers;
    u16 ended;

    if (battle_work_area.records[slot].flags15A & 0x80) {
        battle_status_count_down_gear(gear, record, timers);
        return 0;
    }
    ended = 0;
    if (record->pilot.status7C & 0x1000) {
        timers[1] += -1;
        if (timers[1] == 0) {
            ended = 0x4000;
            record->pilot.status7C &= ~0x1000;
        }
    }
    if (record->pilot.status80 & 0x1000) {
        timers[2] += -1;
        if (timers[2] == 0) {
            ended |= 0x2000;
            record->pilot.status80 &= ~0x1000;
        }
    }
    if (record->pilot.status80 & 0x800) {
        timers[3] += -1;
        if (timers[3] == 0) {
            ended |= 0x1000;
            record->pilot.status80 &= ~0x800;
            record->pilot.status7A &= ~0x20;
        }
    }
    if ((record->pilot.status84.word & 0x80008000) == 0x8000) {
        timers[4] += -1;
        if (timers[4] == 0) {
            ended |= 0x800;
            record->pilot.status84.half.active &= ~0x8000;
        }
    }
    if ((record->pilot.status84.word & 0x40004000) == 0x4000) {
        timers[5] += -1;
        if (timers[5] == 0) {
            ended |= 0x400;
            record->pilot.status84.half.active &= ~0x4000;
        }
    }
    if ((record->pilot.status84.word & 0x20002000) == 0x2000) {
        timers[6] += -1;
        if (timers[6] == 0) {
            ended |= 0x200;
            record->pilot.status84.half.active &= ~0x2000;
        }
    }
    if ((record->pilot.status84.word & 0x10001000) == 0x1000) {
        timers[7] += -1;
        if (timers[7] == 0) {
            ended |= 0x100;
            record->pilot.status84.half.active &= ~0x1000;
        }
    }
    if ((record->pilot.status84.word & 0x08000800) == 0x800) {
        timers[8] += -1;
        if (timers[8] == 0) {
            ended |= 0x80;
            record->pilot.status84.half.active &= ~0x800;
        }
    }
    if ((record->pilot.status88.word & 0x80008000) == 0x8000) {
        timers[9] += -1;
        if (timers[9] == 0) {
            ended |= 0x40;
            record->pilot.status88.half.active &= ~0x8000;
        }
    }
    if ((record->pilot.status88.word & 0x40004000) == 0x4000) {
        timers[10] += -1;
        if (timers[10] == 0) {
            ended |= 0x20;
            record->pilot.status88.half.active &= ~0x4000;
        }
    }
    if ((record->pilot.status88.word & 0x10001000) == 0x1000) {
        timers[11] += -1;
        if (timers[11] == 0) {
            ended |= 0x10;
            record->pilot.status88.half.active &= ~0x1000;
        }
    }
    if ((record->pilot.status8C.half.active & 0xF000) && !(record->pilot.status8C.half.permanent & 0xF000)) {
        timers[12] += -1;
        if (timers[12] == 0) {
            ended |= 8;
            record->pilot.status8C.half.active &= ~0xF000;
        }
    }
    if ((record->pilot.status8C.half.active & 0xF00) && !(record->pilot.status8C.half.permanent & 0xF00)) {
        timers[13] += -1;
        if (timers[13] == 0) {
            ended |= 4;
            record->pilot.status8C.half.active &= ~0xF00;
        }
    }
    return ended;
}

/* 80099CF0: Count down the timed statuses of a gear, clearing each one whose timer runs
 * out; the end of gear status 0x20 also ends the pilot's status 0x1000. */
void battle_status_count_down_gear(GearRecord *gear, Combatant *record, volatile u8 *timers) {
    if (gear->status7C & 0x200) {
        timers[1] += -1;
        if (timers[1] == 0) {
            gear->status7C &= ~0x200;
        }
    }
    if (gear->status7C & 0x100) {
        timers[2] += -1;
        if (timers[2] == 0) {
            gear->status7C &= ~0x100;
        }
    }
    if (gear->status7C & 0x80) {
        timers[3] += -1;
        if (timers[3] == 0) {
            gear->status7C &= ~0x80;
        }
    }
    if (gear->status7C & 0x20) {
        timers[4] += -1;
        if (timers[4] == 0) {
            gear->status7C &= ~0x20;
            record->pilot.status7C &= ~0x1000;
        }
    }
    if (gear->status7C & 0x10) {
        timers[5] += -1;
        if (timers[5] == 0) {
            gear->status7C &= ~0x10;
        }
    }
    if (gear->status7C & 0xF000) {
        timers[7] += -1;
        if (timers[7] == 0) {
            gear->status7C &= ~0xF000;
        }
    }
    if (gear->status7C & 0xF00) {
        timers[8] += -1;
        if (timers[8] == 0) {
            gear->status7C &= ~0xF00;
        }
    }
    if (gear->status80 & 0x1000) {
        timers[10] += -1;
        if (timers[10] == 0) {
            gear->status80 &= ~0x1000;
        }
    }
    if (gear->status80 & 0x40) {
        timers[11] += -1;
        if (timers[11] == 0) {
            gear->status80 &= ~0x40;
        }
    }
    if (gear->status80 & 0x20) {
        timers[12] += -1;
        if (timers[12] == 0) {
            gear->status80 &= ~0x20;
        }
    }
}

/* 80099FB0: Make the current command descriptor the battle's current command: copy its
 * attribute bytes and index, and give a command without an element the
 * attacker's element statuses. */
void battle_publish_current_command(void) {
    u16 elements = (battle_attacker_record->pilot.status8C.half.active | battle_attacker_record->pilot.status8C.half.permanent) >> 12;

    battle_work_ptr->commandAttributes[0] = battle_current_command->attributes[0];
    battle_work_ptr->commandAttributes[1] = battle_current_command->attributes[1];
    battle_work_ptr->commandAttributes[2] = battle_current_command->attributes[2];
    battle_work_ptr->commandAttributes[3] = battle_current_command->attributes[3];
    battle_work_ptr->commandIndexCopy = battle_work_ptr->commandIndex;
    if ((battle_work_ptr->commandAttributes[2] & 0x3F) == 0) {
        battle_work_ptr->commandAttributes[2] |= elements;
    }
}

/* 8009A074: Restrict the target mask to the allowed targets of its side: the low three
 * bits (party) and bits 3-10 (enemies). */
void battle_restrict_target_mask_to_side(void) {
    if (battle_work_ptr->targetMask & 7) {
        battle_work_ptr->targetMask = battle_work_ptr->targetMask2 & 7;
    }
    if (battle_work_ptr->targetMask & 0x7F8) {
        battle_work_ptr->targetMask = battle_work_ptr->targetMask2 & 0x7F8;
    }
}

/* 8009A0DC: The condition shown for slot, by priority: 8, 1, 2 for status bits 0x4000,
 * 0x8000, 0x2000; 3 and 4 for the second word's 0x1000 and 0x2000; 5, 6, 3 for
 * 0x800, 0x1000, 2; 7 for status pair 0x84 bit 0x8000; 5 below an eighth of
 * the maximum HP; otherwise 0. */
s32 battle_status_get_shown_condition(u8 slot) {
    Combatant *record = &battle_work_ptr->records[slot];

    if (record->pilot.status7C & 0x4000) {
        return 8;
    }
    if (record->pilot.status7C & 0x8000) {
        return 1;
    }
    if (record->pilot.status7C & 0x2000) {
        return 2;
    }
    if (record->pilot.status80 & 0x1000) {
        return 3;
    }
    if (record->pilot.status80 & 0x2000) {
        return 4;
    }
    if (record->pilot.status7C & 0x800) {
        return 5;
    }
    if (record->pilot.status7C & 0x1000) {
        return 6;
    }
    if (record->pilot.status7C & 2) {
        return 3;
    }
    if ((record->pilot.status84.half.active | record->pilot.status84.half.permanent) & 0x8000) {
        return 7;
    }
    if (record->pilot.hp < record->pilot.maxHp >> 3) {
        return 5;
    }
    return 0;
}

/* 8009A1AC: Mask of slot's timed conditions for display (0 once status bit 0x8000 is
 * set): 0x8000, 0x4000 and 0x2000 for three statuses, plus the element
 * statuses (pair 0x8C bits 8-15) shifted down by three. */
u16 battle_status_get_shown_condition_mask(u8 slot) {
    Combatant *record = &battle_work_ptr->records[slot];
    u16 mask = 0;
    u16 elements;

    if (record->pilot.status7C & 0x8000) {
        return 0;
    }
    if (record->pilot.status80 & 0x1000) {
        mask = 0x8000;
    }
    if (record->pilot.status80 & 0x2000) {
        mask |= 0x4000;
    }
    if (record->pilot.status7C & 0x800) {
        mask |= 0x2000;
    }
    elements = record->pilot.status8C.half.active | record->pilot.status8C.half.permanent;
    if (elements & 0xF00) {
        mask |= (elements & 0xF00) >> 3;
    }
    if (elements & 0xF000) {
        mask |= (elements & 0xF000) >> 3;
    }
    return mask;
}

/* 8009A258: Accuracy of member's command: the descriptor's accuracy plus the member's
 * bonus, capped at 100. */
u8 battle_get_command_accuracy(u8 member, u8 command) {
    CommandDescriptor *descriptor = &battle_work_ptr->partyCommands[member][command];
    u8 accuracy = descriptor->accuracy + (battle_work_ptr->records + member)->pilot.ether;

    if (accuracy > 100) {
        accuracy = 100;
    }
    return accuracy;
}

/* 8009A2D4: Fill the gear HUD for member: its first commands' states, charge rate,
 * attack, defense, the chance of a boost (from the gear's damage, when the
 * game data's boost flag 0x4000 is on), warning bits and overheat; count down an
 * active boost (level 4) or, at level 3, try to start one. */
void battle_gear_hud_fill(u8 member) {
    Combatant *record = &battle_work_area.records[member];
    GearRecord *gear = &battle_work_area.records[member].gear;
    u8 *level = &battle_work_area.records[member].field148;
    CommandDescriptor *commands = battle_work_area.gearCommands[member];
    GearHud *hud = &battle_work_area.gearHud;
    u8 i;
    u8 chance;

    for (i = 0; i < 15; i++) {
        hud->commands[i] = commands->hudState;
        commands++;
    }
    if (gear->chargeRate != 0) {
        hud->charge = gear->chargeRate * hud->commands[0];
    } else {
        hud->charge = 30;
    }
    switch (*level) {
    case 1:
    case 2:
    case 3:
        hud->charge += *level * 20;
        break;
    case 4:
        hud->charge *= 10;
        break;
    }
    hud->attack = gear->attack * gear->attackScale;
    hud->attack = gear->entries[0].valueE + hud->attack;
    if (record->pilot.characterId == 4) {
        hud->attack = gear->entries[2].valueE + hud->attack;
    }
    hud->field29 = gear->speed;
    hud->defense = gear->defense;
    if (gear->maxHp != gear->hp) {
        chance = (gear->maxHp - gear->hp) / (gear->maxHp / 10);
        if (chance == 0) {
            chance++;
        }
    } else {
        chance++; /* uninitialised in the original */
    }
    chance *= record->pilot.field54 + 5;
    if (!(game_data.flags & 0x4000)) {
        chance = 0;
    }
    if (record->pilot.level < 50) {
        chance = 0;
    }
    if (record->pilot.gearId == 3) {
        chance = 0;
    }
    if (record->pilot.gearId == 15) {
        chance = 99;
    }
    if (chance >= 100) {
        chance = 99;
    }
    hud->boostChance = chance;
    hud->status = 0;
    if (gear->status7C & 0x100) {
        hud->status = 0x8000;
    }
    if (gear->status7C & 0x200) {
        hud->status |= 0x4000;
    }
    if (gear->status7C & 0x80) {
        hud->status |= 0x2000;
    }
    if (gear->status7C & 0x10) {
        hud->status |= 0x1000;
    }
    if (gear->fuel < gear->maxFuel >> 3) {
        hud->status |= 0x800;
    } else {
        hud->status &= ~0x800;
    }
    if (gear->fuel == 0) {
        gear->status80 &= ~0x8000;
        record->pilot.status84.half.active &= ~0x8000;
    }
    if (gear->status80 & 0x8000) {
        hud->overheat = 1;
    } else {
        hud->overheat = 0;
    }
    if (*level == 4) {
        hud->level = 4;
        if (--battle_work_area.records[member].statusTimers[6] == 0) {
            gear->status80 &= ~0x4000;
            *level = 0;
            record->pilot.field54 = 0;
        }
    } else {
        hud->level = *level;
        if (*level == 3 && (game_data.flags & 0x4000) && rand() % 100 < chance) {
            gear->status80 |= 0x4000;
            battle_work_area.records[member].statusTimers[6] = 3;
            if (battle_work_area.records[member].pilot.flags32 & 0x40) {
                battle_work_area.records[member].statusTimers[6] = 6;
            }
            (*level)++;
        }
    }
}

/* 8009A7B8: Byte 5 of game-data unit record id. */
u8 battle_get_first_weapon_byte5(u8 id) {
    return game_data.characters[id].entries[0].pad5;
}

/* 8009A7E4: Whether gear item index is one of character 4's four entries. */
s32 battle_is_character4_entry_item(u8 index) {
    BattleItem *item = &battle_work_ptr->lists.items.members[index];

    if (game_data.characters[4].entries[0].id == item->id
        || game_data.characters[4].entries[1].id == item->id
        || game_data.characters[4].entries[2].id == item->id) {
        return 1;
    }
    return game_data.characters[4].entries[3].id == item->id;
}

/* 8009A854: Put battle item index into character 4's entry holding its id (entry k when
 * none does): copy its values, record the item slot, set the id's rounds from
 * the item's +3, and update the battle copies of character 4. */
void battle_put_item_in_character4_entry(u8 index, u8 k) {
    BattleItem *item = &battle_work_ptr->lists.items.list[index];
    u8 i;

    if (game_data.characters[4].entries[0].id == item->id) {
        k = 0;
    }
    if (game_data.characters[4].entries[1].id == item->id) {
        k = 1;
    }
    if (game_data.characters[4].entries[2].id == item->id) {
        k = 2;
    }
    if (game_data.characters[4].entries[3].id == item->id) {
        k = 3;
    }
    game_data.characters[4].entries[k].value4 = item->valueC;
    game_data.characters[4].entries[k].value3 = item->valueB;
    game_data.characters[4].entries[k].value2 = item->valueA;
    game_data.characters[4].entries[k].value3 = item->valueB;
    game_data.characters[4].entryItems[k] = index;
    game_data.ammo[index - 50] = item->rounds;
    game_data.characters[4].entryItems[k] = index;
    for (i = 0; i < 3; i++) {
        Combatant *record = &battle_work_ptr->records[i];

        if (record->pilot.characterId == 4) {
            record->pilot.entries[k].value4 = item->valueC;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entries[k].value2 = item->valueA;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entryItems[k] = index;
        }
    }
}

/* 8009A9D0: The Escape command: succeeds on half of the rolls, writing the party back
 * to the game data (8009BE0C). */
s32 battle_try_escape(void) {
    battle_work_ptr->commandIndex = 0;
    if (rand() % 100 < 50) {
        battle_write_party_to_game_data();
        return 1;
    }
    return 0;
}

/* 8009AA44: The Defense command for member: mark it defending; a gear with status 0x10
 * in its second word drops its gear statuses 0x1B0 and the pilot's 0x1000. */
void battle_start_defending(u8 member) {
    battle_work_ptr->commandIndex = 0;
    battle_work_ptr->records[member].flags15A |= 1;
    if ((battle_work_ptr->records[member].flags15A & 0x80) && (battle_work_ptr->records[member].gear.status82 & 0x10)) {
        battle_work_ptr->records[member].gear.status7C &= 0xFE4F;
        battle_work_ptr->records[member].pilot.status7C &= ~0x1000;
    }
    if (battle_gear_hud_attack_level == 4) {
        battle_work_ptr->message = 0x3D;
    }
}

/* 8009AB00: End member's defending. */
void battle_end_defending(u8 member) {
    battle_work_ptr->records[member].flags15A &= ~1;
}

/* 8009AB38: Enable member's Deathblow commands (descriptors 22 and 24-32; in a gear, gear
 * descriptors 21 and 23-28) when its command flag is set. */
void battle_enable_deathblow_commands(u8 member) {
    s32 flag;

    if (battle_work_ptr->records[member].flags15A & 0x80) {
        flag = battle_work_ptr->records[member].gear.status80 & 0x2000;
    } else {
        flag = battle_work_ptr->records[member].pilot.status88.half.active & 0x400;
    }
    if (flag) {
        if (battle_work_ptr->records[member].flags15A & 0x80) {
            CommandDescriptor *commands = battle_work_ptr->gearCommands[member];

            commands[21].state = 1;
            commands[23].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
        } else {
            CommandDescriptor *commands = battle_work_ptr->partyCommands[member];

            commands[22].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
            commands[29].state = 1;
            commands[30].state = 1;
            commands[31].state = 1;
            commands[32].state = 1;
        }
    }
}

/* 8009AC48: Seal the same commands again and clear member's command flag; unless
 * checked is 0, only for a current command with flag 0x100. */
void battle_seal_deathblow_commands(u8 member, u8 checked) {
    s32 flag;

    if (checked == 0 || (battle_current_command->flagsA & 0x100)) {
        if (battle_work_ptr->records[member].flags15A & 0x80) {
            flag = battle_work_ptr->records[member].gear.status80 & 0x2000;
        } else {
            flag = battle_work_ptr->records[member].pilot.status88.half.active & 0x400;
        }
        if (flag) {
            if (battle_work_ptr->records[member].flags15A & 0x80) {
                CommandDescriptor *commands = battle_work_ptr->gearCommands[member];

                commands[21].state = 0x2000;
                commands[23].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                battle_work_ptr->records[member].gear.status80 &= ~0x2000;
            } else {
                CommandDescriptor *commands = battle_work_ptr->partyCommands[member];

                commands[22].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                commands[29].state = 0x2000;
                commands[30].state = 0x2000;
                commands[31].state = 0x2000;
                commands[32].state = 0x2000;
                battle_work_ptr->records[member].pilot.status88.half.active &= ~0x400;
            }
        }
    }
}

/* 8009ADA0: Regeneration amounts of slot for its turn: HP (maxHp / 20), EP (maxEp / 20,
 * from the pilot's or the gear's status) and fuel (maxFuel / 50 for each of
 * two gear statuses). Returns whether any applies; nothing once KO'd. */
s32 battle_get_status_drain_amounts(u8 slot, s32 *amounts) {
    Combatant *record = &battle_work_ptr->records[slot];
    s32 any = 0;

    if (record->pilot.status7C & 0x8000) {
        return 0;
    }
    if (record->pilot.status7C & 0x800) {
        any = 1;
        *amounts = (u16)(record->pilot.maxHp / 20);
    }
    amounts++;
    if (record->pilot.status80 & 0x200) {
        any = 1;
        *amounts = (u16)(record->pilot.maxEp / 20);
    }
    if (record->gear.status7C & 0x200) {
        any = 1;
        *amounts = (u16)(record->pilot.maxEp / 20);
    }
    amounts++;
    *amounts = 0;
    if (record->gear.status7C & 0x80) {
        any = 1;
        *amounts = (u16)(record->gear.maxFuel / 50);
    }
    if (record->gear.status80 & 0x8000) {
        any = 1;
        *amounts += (u16)(record->gear.maxFuel / 50);
    }
    return any;
}

/* 8009AEFC: Revive slot's record: clear its statuses (keeping status 0x2000 of the
 * second word), size Chu-Chu's gear HP (8009B104), and when character 3 is
 * revived clear status 0x20 of every enemy. */
void battle_revive_slot_record(u8 slot) {
    Combatant *record;
    u8 i;

    battle_work_ptr->commandIndex = 0;
    record = &battle_work_ptr->records[slot];
    record->pilot.status7C = 0;
    record->pilot.status84.half.active = 0;
    record->pilot.status88.half.active = 0;
    record->pilot.status8C.half.active = 0;
    record->pilot.status80 &= 0x2000;
    if (record->pilot.characterId == 7) {
        battle_size_character7_gear_hp(slot, record);
    }
    if (record->pilot.characterId == 3) {
        for (i = 3; i < 11; i++) {
            battle_work_area.records[i].pilot.status80 &= ~0x20;
        }
    }
}

/* 8009AFD8: Take a round of the attacker's ammo for the current command (none below 0):
 * commands 0-3 the first special slot's, 6 the fourth's, 7-19 both. */
void battle_wear_weapon_items(void) {
    switch (battle_work_ptr->commandIndex) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (game_data.ammo[battle_attacker_record->pilot.entryItems[0] - 50] != 0) {
            game_data.ammo[battle_attacker_record->pilot.entryItems[0] - 50] += -1;
        }
        break;
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
        if (game_data.ammo[battle_attacker_record->pilot.entryItems[0] - 50] != 0) {
            game_data.ammo[battle_attacker_record->pilot.entryItems[0] - 50] += -1;
        }
        /* fallthrough */
    case 6:
        if (game_data.ammo[battle_attacker_record->pilot.entryItems[3] - 50] != 0) {
            game_data.ammo[battle_attacker_record->pilot.entryItems[3] - 50] += -1;
        }
        break;
    }
}

/* 8009B098: Debug party: members 1 and 2 get 100/100 HP and fixed stats. */
void battle_set_debug_party_stats(void) {
    u8 i;

    for (i = 1; i < 3; i++) {
        Combatant *record = &battle_work_ptr->records[i];

        record->pilot.hp = 100;
        record->pilot.maxHp = 100;
        record->pilot.field5E = 20;
        record->pilot.field5F = 15;
        record->pilot.status7A = 0x1FBF;
        record->field149 = 0;
    }
}

/* 8009B104: Chu-Chu's gear HP: fifty times her HP and maximum HP, capped at 99999. */
void battle_size_character7_gear_hp(u8 slot, Combatant *chuchu) {
    Combatant *record = &battle_work_ptr->records[slot];
    GearRecord *gear = &record->gear;

    if (chuchu->pilot.maxHp * 50 > 99999) {
        if (chuchu->pilot.hp * 50 > 99999) {
            record->gear.hp = 99999;
        } else {
            record->gear.hp = chuchu->pilot.hp * 50;
        }
        gear->maxHp = 99999;
    } else {
        record->gear.hp = chuchu->pilot.hp * 50;
        record->gear.maxHp = chuchu->pilot.maxHp * 50;
    }
}

/* 8009B1E4: Debug setup: 99 of every item 1-47, and fixed skill masks for all eleven
 * characters. */
void battle_grant_debug_items_and_skills(void) {
    u8 i;

    for (i = 1; i < 48; i++) {
        game_data.itemIds[i] = i;
        game_data.itemCounts[i] = 99;
    }
    game_data.skills[0].counterSkills = 0xFFF8;
    game_data.skills[0].levelSkills = 0xFF00;
    game_data.skills[0].unlocksA = 0xFFFF;
    game_data.skills[0].unlocksB = 0xFE00;
    game_data.skills[0].flags1A = 0xF000;
    game_data.skills[0].tier = 7;
    game_data.skills[1].counterSkills = 0xFFE0;
    game_data.skills[1].levelSkills = 0xFFF0;
    game_data.skills[1].unlocksA = 0xFFFF;
    game_data.skills[1].unlocksB = 0xFFF0;
    game_data.skills[1].flags1A = 0xC000;
    game_data.skills[1].tier = 7;
    game_data.skills[2].counterSkills = 0xFFE0;
    game_data.skills[2].levelSkills = 0xFFE0;
    game_data.skills[2].unlocksA = 0xFFFF;
    game_data.skills[2].unlocksB = 0xFF00;
    game_data.skills[2].flags1A = 0x8000;
    game_data.skills[2].tier = 7;
    game_data.skills[3].counterSkills = 0xFFE0;
    game_data.skills[3].levelSkills = 0xFFC0;
    game_data.skills[3].unlocksA = 0xFFFF;
    game_data.skills[3].unlocksB = 0xFF00;
    game_data.skills[3].flags1A = 0xF000;
    game_data.skills[3].tier = 7;
    game_data.skills[4].counterSkills = 0xFFC0;
    game_data.skills[4].levelSkills = 0xFFC0;
    game_data.skills[4].unlocksA = 0xFFFF;
    game_data.skills[4].unlocksB = 0xFC00;
    game_data.skills[4].flags1A = 0xE000;
    game_data.skills[4].tier = 7;
    game_data.skills[5].counterSkills = 0xFFC0;
    game_data.skills[5].levelSkills = 0xF000;
    game_data.skills[5].unlocksA = 0xFFFF;
    game_data.skills[5].unlocksB = 0xF000;
    game_data.skills[5].flags1A = 0x8000;
    game_data.skills[5].tier = 7;
    game_data.skills[6].counterSkills = 0xFFC0;
    game_data.skills[6].levelSkills = 0xFF00;
    game_data.skills[6].unlocksA = 0xFFFF;
    game_data.skills[6].unlocksB = 0xFF00;
    game_data.skills[6].flags1A = 0x8000;
    game_data.skills[6].tier = 7;
    game_data.skills[7].counterSkills = 0;
    game_data.skills[7].levelSkills = 0xFF00;
    game_data.skills[7].unlocksA = 0;
    game_data.skills[7].unlocksB = 0xFF00;
    game_data.skills[7].flags1A = 0;
    game_data.skills[7].tier = 7;
    game_data.skills[8].counterSkills = 0;
    game_data.skills[8].levelSkills = 0xF800;
    game_data.skills[8].unlocksA = 0xFFFF;
    game_data.skills[8].unlocksB = 0;
    game_data.skills[8].flags1A = 0xE000;
    game_data.skills[8].tier = 7;
    game_data.skills[9].counterSkills = 0xFFE0;
    game_data.skills[9].levelSkills = 0xFFE0;
    game_data.skills[9].unlocksA = 0xFFFF;
    game_data.skills[9].unlocksB = 0xFFE0;
    game_data.skills[9].flags1A = 0x8000;
    game_data.skills[9].tier = 7;
    game_data.skills[10].counterSkills = 0xFFC0;
    game_data.skills[10].levelSkills = 0xFF00;
    game_data.skills[10].unlocksA = 0xFFFF;
    game_data.skills[10].unlocksB = 0xFF00;
    game_data.skills[10].flags1A = 0xC000;
    game_data.skills[10].tier = 7;
}

/* 8009B46C: Raise an attack's damage: by half for each of characters 0 and 3 in the
 * party below half HP (gear HP in a gear) and again below a quarter; then a
 * critical chance (10%, 60% with attacker flag 0x200) multiplies it by 1.5
 * (2 with attacker flag 0x400). Each HP test is written per record kind
 * (cross-jumping merges the two tails), and the chance and scale defaults
 * follow the bonus. */
void battle_apply_character7_damage_bonus(u16 *damage) {
    u8 count = 0;
    u8 i;
    s32 chance;
    s16 scale;

    for (i = 0; i < 3; i++) {
        Combatant *record = &battle_work_ptr->records[i];
        GearRecord *gear = &record->gear;

        if (record->pilot.characterId == 0) {
            if (battle_work_area.records[i].flags15A & 0x80) {
                if (record->gear.hp < record->gear.maxHp >> 1) {
                    count++;
                }
                if (record->gear.hp < record->gear.maxHp >> 2) {
                    count++;
                }
            } else {
                if (record->pilot.hp < record->pilot.maxHp >> 1) {
                    count++;
                }
                if (record->pilot.hp < record->pilot.maxHp >> 2) {
                    count++;
                }
            }
        }
        if (record->pilot.characterId == 3) {
            if (battle_work_area.records[i].flags15A & 0x80) {
                if (gear->hp < gear->maxHp >> 1) {
                    count++;
                }
                if (gear->hp < gear->maxHp >> 2) {
                    count++;
                }
            } else {
                if (record->pilot.hp < record->pilot.maxHp >> 1) {
                    count++;
                }
                if (record->pilot.hp < record->pilot.maxHp >> 2) {
                    count++;
                }
            }
        }
    }
    if (count) {
        *damage += count * (*damage >> 1);
    }
    chance = 10;
    scale = 3;
    if (battle_attacker_record->pilot.flags32 & 0x400) {
        scale = 4;
    }
    if (battle_attacker_record->pilot.flags32 & 0x200) {
        chance = 60;
    }
    if (rand() % 100 < chance) {
        *damage = scale * *damage >> 1;
    }
}

/* 8009B684: Show the message for an applied status, named by its kind and flag bit. */
void battle_status_show_message(u8 kind, u16 flag) {
    switch (kind) {
    case 0:
        switch (flag) {
        case 0x2000:
            battle_work_ptr->message = 0x1;
            break;
        case 0x1000:
            battle_work_ptr->message = 0x2;
            break;
        case 0x800:
            battle_work_ptr->message = 0x3;
            break;
        case 0x400:
            battle_work_ptr->message = 0x4;
            break;
        case 0x200:
            battle_work_ptr->message = 0x5;
            break;
        case 0x1:
            battle_work_ptr->message = 0x8;
            break;
        }
        break;
    case 2:
        switch (flag) {
        case 0x2000:
            battle_work_ptr->message = 0x9;
            break;
        case 0x1000:
            battle_work_ptr->message = 0xA;
            break;
        case 0x800:
            battle_work_ptr->message = 0xB;
            break;
        case 0x400:
            battle_work_ptr->message = 0xC;
            break;
        case 0x1:
            battle_work_ptr->message = 0xD;
            break;
        case 0x20:
            battle_work_ptr->message = 0x7;
            break;
        }
        break;
    case 5:
        switch (flag) {
        case 0x8000:
            battle_work_ptr->message = 0xE;
            break;
        case 0x4000:
            battle_work_ptr->message = 0xF;
            break;
        case 0x2000:
            battle_work_ptr->message = 0x10;
            break;
        case 0x1000:
            battle_work_ptr->message = 0x11;
            break;
        case 0x800:
            battle_work_ptr->message = 0x12;
            break;
        case 0x1800:
            battle_work_ptr->message = 0x13;
            break;
        }
        break;
    case 7:
        switch (flag) {
        case 0x8000:
            battle_work_ptr->message = 0x15;
            break;
        case 0x4000:
            battle_work_ptr->message = 0x16;
            break;
        case 0x1000:
            battle_work_ptr->message = 0x18;
            break;
        case 0x2:
        case 0x8:
            battle_work_ptr->message = 0x19;
            break;
        case 0x1:
        case 0x4:
            battle_work_ptr->message = 0x1A;
            break;
        }
        break;
    case 9:
        switch (flag) {
        case 0x8000:
            battle_work_ptr->message = 0x1B;
            break;
        case 0x4000:
            battle_work_ptr->message = 0x1C;
            break;
        case 0x2000:
            battle_work_ptr->message = 0x1D;
            break;
        case 0x1000:
            battle_work_ptr->message = 0x1E;
            break;
        case 0x400:
            battle_work_ptr->message = 0x1F;
            break;
        case 0x800:
            battle_work_ptr->message = 0x20;
            break;
        case 0x100:
            battle_work_ptr->message = 0x21;
            break;
        case 0x200:
            battle_work_ptr->message = 0x22;
            break;
        }
        break;
    }
}

/* 8009BAC4: Choose an automatic action for slot (confusion or auto-battle): choice[0]
 * is the kind (4 defend, 2 a skill with choice[1] its index among the
 * character's usable skills, 0/1 an attack with choice[1] its strength). Out of
 * a gear: defend on 10% unless flagged, a known skill on 25%, then Chu-Chu's
 * basic attack; otherwise attack weak 48%, medium 32%, strong 20%. */
void battle_choose_automatic_action(u8 slot, u8 *choice, s16 *busy) {
    Combatant *record = &battle_work_area.records[slot];
    u16 skills;
    u8 count;
    u8 skill;

    if (!(battle_work_area.records[slot].flags15A & 0x80)) {
        if (rand() % 100 < 10 && !(record->pilot.status7A & 0x100) && *busy == 0) {
            choice[0] = 4;
            return;
        }
        switch (record->pilot.characterId) {
        case 3:
            skills = 0xC3C0;
            count = 10;
            break;
        case 4:
            skills = 0xDF80;
            count = 10;
            break;
        case 5:
            skills = 0x1000;
            count = 4;
            break;
        case 7:
            skills = 0xE000;
            count = 3;
            break;
        case 0:
        case 8:
            skills = 0xC000;
            count = 2;
            break;
        case 2:
        case 9:
            skills = 0xBFE0;
            count = 11;
            break;
        case 1:
        case 6:
        case 10:
            skills = 0xF000;
            count = 4;
            break;
        }
        if (rand() % 100 < 25 && !(record->pilot.status7A & 0x20)) {
            choice[0] = 2;
            skill = rand() % count;
            if (game_data.skills[record->pilot.characterId].levelSkills & ((0x8000 >> skill) & skills)) {
                choice[1] = skill;
                return;
            }
        }
        if (record->pilot.characterId == 8) {
            choice[0] = 0;
            choice[1] = 0;
            return;
        }
    }
    choice[0] = 1;
    if (rand() % 100 < 80) {
        if (rand() % 100 >= 60) {
            choice[1] = 1;
        } else {
            choice[1] = 0;
        }
    } else {
        choice[1] = 2;
    }
}

/* 8009BD94: Formula 7 (battle_formula_table[7]): heal the target (result code 2) by the
 * command's power in twentieths of its gear's maximum HP. */
void battle_formula7_heal_by_gear_max_hp(void) {
    battle_work_ptr->resultCode[battle_target_slot] = 2;
    battle_work_ptr->damage[battle_target_slot] = battle_current_command->power * battle_target_gear->maxHp / 20;
}

/* 8009BE0C: Write the party back to the game data: each present member's HP (Chu-Chu in
 * a gear takes hers from the gear's HP), EP, use counts and field 0x3A, with
 * HP 1 when KO'd, and the HP and fuel of its gear (ids 0-6 and 8-16), a tenth
 * of the maximum when the gear is wrecked. */
void battle_write_party_to_game_data(void) {
    u8 i;
    u8 j;
    s32 unused[2]; /* never used; it gives the original its 8-byte frame */

    for (i = 0; i < 3; i++) {
        Combatant *record;
        CharacterRecord *character;
        GearRecord *gearRecord;
        GearRecord *gear;

        if (battle_party_character_ids[i] == 0x7F) {
            continue;
        }
        record = &battle_work_area.records[i];
        character = &game_data.characters[record->pilot.characterId];
        gear = &battle_work_area.records[i].gear;
        gearRecord = &game_data.gears[record->pilot.gearId];
        if (record->pilot.characterId == 7 && (battle_work_area.records[i].flags15A & 0x80)) {
            record->pilot.hp = (gear->hp + 1) / 50;
            if (record->pilot.hp == 0) {
                record->pilot.hp = 1;
            }
        }
        character->hp = record->pilot.hp;
        character->ep = record->pilot.ep;
        if (character->hp > character->maxHp) {
            character->hp = character->maxHp;
        }
        if (character->ep > character->maxEp) {
            character->ep = character->maxEp;
        }
        for (j = 0; j < 7; j++) {
            character->useCounts[j] = record->pilot.useCounts[j];
        }
        character->field3A = record->pilot.field3A;
        if (record->pilot.status7C & 0xC000) {
            character->hp = 1;
        }
        switch (record->pilot.gearId) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            gearRecord->hp = gear->hp;
            gearRecord->fuel = gear->fuel;
            if (gearRecord->hp > gearRecord->maxHp) {
                gearRecord->hp = gearRecord->maxHp;
            }
            if (gearRecord->fuel > gearRecord->maxFuel) {
                gearRecord->fuel = gearRecord->maxFuel;
            }
            if (gear->status7C & 0x8000) {
                gearRecord->hp = gearRecord->maxHp / 10;
            }
            break;
        }
    }
}

/* 8009C050: Gear warning flags of slot: 1 gear status 0x400, 2 gear HP below an eighth
 * (unless the pilot's flag 1 at +0x36), 4 when field 0x148 is 4. */
s32 battle_get_gear_warning_flags(u8 slot) {
    Combatant *record = &battle_work_area.records[slot];
    GearRecord *gear = &battle_work_area.records[slot].gear;
    u8 *state = &battle_work_area.records[slot].field148;
    u8 flags = 0;

    if (gear->status7C & 0x400) {
        flags = 1;
    }

    if (gear->hp < gear->maxHp >> 3 && !(record->pilot.flags36 & 1)) {
        flags |= 2;
    }
    if (*state == 4) {
        flags |= 4;
    }
    return flags;
}

/* 8009C0E0: Put slot into state 4 with timer 6 at three turns, and set flag 0x4000 of
 * character 0. */
void battle_set_slot_attack_level4(u8 slot) {
    battle_work_area.records[slot].field148 = 4;
    battle_work_area.records[slot].statusTimers[6] = 3;
    game_data.skills[0].flags1A |= 0x4000;
}

/* 8009C134: Target the party member that is character 3. */
void battle_target_character3_member(void) {
    u8 i;

    for (i = 0; i < 3; i++) {
        if (battle_work_area.records[i].pilot.characterId == 3) {
            battle_work_area.targetMask = 1 << i;
        }
    }
}

/* 8009C198: Resolve a gear's action against every target in the target mask: select the
 * current command descriptor, run its gear formula (table battle_gear_formula_table) per
 * target with the status step it asks for (8009DBFC), then the per-target
 * follow-ups, the rounds character 4's gear uses (8009e788) and the command's
 * element. */
void battle_resolve_gear_action(void) {
    s32 bit;

    if (battle_attacker_slot < 3) {
        battle_current_command = &battle_work_ptr->gearCommands[battle_work_ptr->attackerIndex][battle_work_ptr->commandIndex];
    } else {
        battle_current_command = &battle_work_ptr->enemyCommands[battle_work_ptr->commandIndex];
    }
    /* Called without its second argument (an unprototyped call). */
    ((void (*)())battle_seal_deathblow_commands)(battle_attacker_slot);
    battle_attacker_attack_level = &battle_work_ptr->records[battle_work_ptr->attackerIndex].field148;
    battle_ether_check_failed = 0;
    if ((battle_current_command->flagsA & 0x100) && battle_attacker_record->pilot.characterId == 1) {
        battle_resolve_ether_check();
    }
    if ((battle_attacker_record->pilot.gearId == 5 || battle_attacker_record->pilot.gearId == 13) && battle_work_ptr->commandIndex == 1) {
        battle_current_command->attributes[2] = battle_attacker_gear->entries[1].valueE;
    }
    bit = 1;
    for (battle_target_slot = 0; battle_target_slot < 11; battle_target_slot++, bit <<= 1) {
        if (bit & battle_work_ptr->targetMask) {
            battle_target_record = &battle_work_ptr->records[battle_target_slot];
            battle_target_gear = &battle_work_ptr->records[battle_target_slot].gear;
            battle_target_attack_level = &battle_work_ptr->records[battle_target_slot].field148;
            battle_use_on_foot_command_variant();
            battle_gear_formula_table[battle_current_command->formula]();
            if (battle_work_ptr->resultCode[battle_target_slot] == 0) {
                if (battle_current_command->flagsA & 0x800) {
                    battle_resolve_gear_status_roll(1);
                } else if (battle_current_command->flagsA & 0x4000) {
                    battle_resolve_gear_status_roll(0);
                }
            }
            battle_work_ptr->shownCommand = battle_current_command->name;
            battle_resolve_gear_follow_up();
            battle_mirror_gear_status_to_pilot(battle_target_slot);
        }
    }
    if (battle_attacker_record->pilot.characterId == 4) {
        battle_wear_down_attacker_gear_parts();
    }
    battle_publish_current_gear_command();
}

/* 8009C4B4: Per-target follow-up of a gear action: a party gear's attack level (up by
 * one with commands 0-2, set below battle_gear_hud_attack_level by the level-3/6/9 command
 * groups, which also raise the pilot's field 0x54), then the target's
 * reactions: breaking status 0x2000 on 80%, fuel-drain immunity, halving or
 * raising damage by its flag 0x80, reflecting it with flag 0x20, and nullifying
 * with status 0x200. */
void battle_resolve_gear_follow_up(void) {
    s32 chance;

    if (battle_attacker_slot < 3) {
        if (battle_work_ptr->commandIndex < 3) {
            if (++battle_attacker_attack_level[0] > battle_attacker_attack_level[1]) {
                battle_attacker_attack_level[0]--;
            }
        }
        if ((u32)(battle_work_ptr->commandIndex - 3) < 3) {
            battle_attacker_attack_level[0] = battle_gear_hud_attack_level - 1;
        }
        if ((u32)(battle_work_ptr->commandIndex - 6) < 3) {
            battle_attacker_attack_level[0] = battle_gear_hud_attack_level - 2;
        }
        if ((u32)(battle_work_ptr->commandIndex - 9) < 3) {
            battle_attacker_attack_level[0] = battle_gear_hud_attack_level - 3;
        }
        if (battle_work_ptr->commandIndex >= 3 && battle_work_ptr->commandIndex < 12) {
            battle_attacker_record->pilot.field54 += battle_work_ptr->commandIndex / 3;
        }
        if (battle_work_ptr->commandIndex == 5) {
            battle_attacker_record->pilot.field54 += 1;
        }
        if (battle_work_ptr->commandIndex == 8) {
            battle_attacker_record->pilot.field54 += 2;
        }
        if (battle_work_ptr->commandIndex == 11) {
            battle_attacker_record->pilot.field54 += 3;
        }
    }
    if (battle_work_ptr->resultCode[battle_target_slot] == 0 && (battle_target_record->pilot.status80 & 0x2000)
        && rand() % 100 < 80) {
        battle_target_record->pilot.status80 &= ~0x2000;
        battle_target_gear->status7C &= ~0x1000;
    }
    if (battle_work_ptr->resultCode[battle_target_slot] == 10 && (battle_target_gear->field7E & 0x80)) {
        battle_work_ptr->damage[battle_target_slot] = 0;
    }
    if (battle_work_ptr->resultCode[battle_target_slot] == 0) {
        if (battle_target_record->pilot.flags32 & 0x80) {
            chance = 60;
            if (battle_target_record->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                battle_work_ptr->damage[battle_target_slot] >>= 1;
            } else {
                battle_work_ptr->damage[battle_target_slot] += battle_work_ptr->damage[battle_target_slot] >> 1;
            }
        }
        if (battle_target_record->pilot.flags32 & 0x20) {
            battle_work_ptr->resultCode[battle_attacker_slot] = 0;
            battle_work_ptr->damage[battle_attacker_slot] = battle_work_ptr->damage[battle_target_slot];
        }
    }
    if ((battle_target_record->pilot.status88.half.permanent & 0x200) && battle_work_ptr->resultCode[battle_target_slot] == 1) {
        battle_work_ptr->damage[battle_target_slot] = 0;
    }
}

/* 8009C9C4: The gear version of 80099FB0: make the current command descriptor the
 * battle's current command, a command without an element taking the gear's
 * element statuses. */
void battle_publish_current_gear_command(void) {
    u16 elements = (battle_attacker_gear->status84.half.active | battle_attacker_record->pilot.status88.half.permanent) >> 12;

    battle_work_ptr->commandAttributes[0] = battle_current_command->attributes[0];
    battle_work_ptr->commandAttributes[1] = battle_current_command->attributes[1];
    battle_work_ptr->commandAttributes[2] = battle_current_command->attributes[2];
    battle_work_ptr->commandAttributes[3] = battle_current_command->attributes[3];
    battle_work_ptr->commandIndexCopy = battle_work_ptr->commandIndex;
    if ((battle_work_ptr->commandAttributes[2] & 0x3F) == 0) {
        battle_work_ptr->commandAttributes[2] |= elements;
    }
}

/* 8009CA90: Against a target on foot, switch the current descriptor to its on-foot
 * variant: commands 12-14 three descriptors on, commands 0-2 fifteen. */
void battle_use_on_foot_command_variant(void) {
    if ((u32)(battle_work_ptr->commandIndex - 12) < 3 && !(battle_work_ptr->records[battle_target_slot].flags15A & 0x80)) {
        battle_current_command += 3;
    } else if (battle_work_ptr->commandIndex < 3 && !(battle_work_ptr->records[battle_target_slot].flags15A & 0x80)) {
        battle_current_command += 15;
    }
}

/* 8009CB68: Mirror the gear's status 0x200 (second word) as the pilot's status 0x20. */
void battle_mirror_gear_status_to_pilot(u8 slot) {
    GearRecord *gear = &battle_work_area.records[slot].gear;
    Combatant *record = &battle_work_area.records[slot];

    if (gear->status80 & 0x200) {
        record->pilot.status84.half.active |= 0x20;
    } else {
        record->pilot.status84.half.active &= ~0x20;
    }
}

/* 8009CBC4: Gear formula 0 (battle_gear_formula_table[0]), gear attack damage: the
 * gear hit outcome, attack and defense values with the element adjustment and both
 * gears' boost/break statuses, the command's drain effects, then (5a - 4d for
 * ether, else 4a - 3d) times the power over 20, a random spread, the element
 * resistance and the hit outcome's result code; at most 9999. */
void battle_gear_formula0_deal_damage(void) {
    u16 attack;
    u16 defense;
    s8 hit;
    u8 power;
    s32 damage;
    s32 attackScale;
    s32 defenseScale;
    u16 flags;
    u8 guard;

    power = battle_current_command->power;
    hit = battle_resolve_gear_hit_outcome();
    attack = battle_resolve_gear_attack_value();
    defense = battle_resolve_gear_defense_value();
    battle_resolve_adjust_for_elements(&attack, &defense, &hit);
    if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 8) {
        attack += attack / 5;
    }
    if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 2) {
        attack += attack / 10;
    }
    if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 4) {
        attack -= attack / 5;
    }
    if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 1) {
        attack -= attack / 10;
    }
    if ((battle_target_gear->status80 | battle_target_gear->status82) & 4) {
        defense += defense / 5;
    }
    if ((battle_target_gear->status80 | battle_target_gear->status82) & 1) {
        defense += defense / 10;
    }
    if ((battle_target_gear->status80 | battle_target_gear->status82) & 8) {
        defense -= defense / 5;
    }
    if ((battle_target_gear->status80 | battle_target_gear->status82) & 2) {
        defense -= defense / 10;
    }
    if (battle_current_command->attributes[2] & 0x10) {
        if (!(battle_target_record->pilot.status82 & 0x40)) {
            battle_target_record->pilot.status80 |= 0x40;
        }
        if ((battle_attacker_gear->status84.half.active | battle_attacker_gear->status84.half.permanent) & 0x4000) {
            battle_slot_result_codes[battle_attacker_slot] = 3;
            battle_slot_damages[battle_attacker_slot] = (u16)(battle_attacker_record->pilot.maxEp / 10) * 2;
        }
        if ((battle_attacker_gear->status84.half.active | battle_attacker_gear->status84.half.permanent) & 0x1000) {
            battle_slot_result_codes[battle_attacker_slot] = 2;
            battle_slot_damages[battle_attacker_slot] = battle_attacker_gear->maxHp / 10 * 2;
        }
    }
    if ((battle_current_command->attributes[2] & 0x20) && !(battle_target_record->pilot.status82 & 0x80)) {
        battle_target_record->pilot.status80 |= 0x80;
    }
    if (battle_target_record->pilot.status80 & 0x40) {
        defense -= defense >> 2;
        battle_target_record->pilot.status80 &= 0xFFBF;
    }
    if (battle_attacker_record->pilot.status80 & 0x80) {
        attack -= attack >> 2;
        battle_attacker_record->pilot.status80 &= 0xFF7F;
    }
    flags = battle_current_command->flagsA;
    if (flags & 0x400) {
        power = 20;
    }
    if (flags & 0x100) {
        attackScale = 5;
        defenseScale = 4;
    } else {
        attackScale = 4;
        defenseScale = 3;
    }
    if (defense != 0) {
        damage = attackScale * attack - defenseScale * defense;
    } else {
        damage = attackScale * attack;
    }
    switch (battle_current_command->amountKind) {
    case 0:
    case 1:
        damage = power * damage / 20;
        break;
    case 2:
        break;
    }
    if (damage <= 0) {
        damage = 0;
    } else if (damage < 15) {
        damage += rand() % 3;
    } else {
        damage += rand() % (damage / 15 + 2);
    }
    if (battle_current_command->elements != 0) {
        damage = battle_resolve_gear_resistance(damage);
    }
    switch (hit) {
    case 1:
        battle_work_ptr->resultCode[battle_target_slot] = 0;
        break;
    case 2:
        battle_work_ptr->resultCode[battle_target_slot] = 5;
        guard = battle_target_gear->guard;
        if (guard >= 10) {
            guard = 9;
        }
        if (damage != 0) {
            damage = damage * (10 - guard) / 20;
        }
        break;
    case 3:
        damage = 0;
        battle_work_ptr->resultCode[battle_target_slot] = 4;
        break;
    case 4:
        battle_work_ptr->resultCode[battle_target_slot] = 2;
        break;
    }
    if (battle_ether_check_failed && (battle_current_command->flagsA & 0x100) && damage != 0) {
        damage /= 3;
    }
    if (damage >= 10000) {
        damage = 9999;
    }
    if (damage < 0) {
        damage = 0;
    }
    battle_work_ptr->damage[battle_target_slot] = damage;
}

/* 8009D354: Gear formula 1 (battle_gear_formula_table[1]): mark the target missed (result 6) when
 * 8009DBFC finds no hit. */
void battle_gear_formula1_inflict_status(void) {
    if (battle_resolve_gear_status_roll(0) == 0) {
        battle_work_ptr->resultCode[battle_target_slot] = 6;
    }
}

/* 8009D3A0: Gear hit outcome of the current command on the target: 1 hit, 2 half,
 * 3 miss. Like 80096ab8 (character 4's gear misses with a command that needs
 * an ammo slot out of rounds) with gear accuracy (+0x9f while the first ammo
 * slot reads 0 rounds; an empty slot reads the low byte of the game data's
 * flags, which no code sets, so every gear without ammo there gains it; 1.5x
 * with status 0x800), the target's evasion (half its gear's +0x9f, 1.5x with
 * status 0x400) and the gears' blind/evade status 0x10. */
s8 battle_resolve_gear_hit_outcome(void) {
    s16 penalty = 0;
    s16 bonus = 0;
    s16 evasion;
    s16 accuracy;
    u8 rounds;
    s16 margin;
    s16 roll;
    u16 status;

    if ((battle_current_command->flagsA & 0x200) && (battle_target_record->pilot.flags34 & 8)) {
        return 3;
    }
    if (battle_current_command->flagsA & 0x1000) {
        return 3;
    }
    if ((battle_target_record->pilot.status84.half.active | battle_target_record->pilot.status84.half.permanent) & 0x100) {
        return 3;
    }
    if (battle_target_record->pilot.status7C & 0x2000) {
        return 1;
    }
    if (battle_target_record->pilot.status80 & 0x1000) {
        return 1;
    }
    if (battle_current_command->flagsA & 0x8000) {
        return 1;
    }
    evasion = battle_target_record->pilot.field5F;
    rounds = game_data.gearAmmo[battle_attacker_gear->partItems[0] - 50];
    accuracy = battle_attacker_record->pilot.field5E;
    if (rounds == 0) {
        accuracy += battle_attacker_gear->hitBonus;
    }
    if (battle_target_gear->hitBonus != 0) {
        evasion += battle_target_gear->hitBonus / 2;
    }
    if (battle_attacker_record->pilot.characterId == 4) {
        if ((battle_current_command->itemKinds & 0x80) && rounds == 0) {
            return 3;
        }
        if ((battle_current_command->itemKinds & 0x20) && game_data.gearAmmo[battle_attacker_gear->partItems[3] - 50] == 0) {
            return 3;
        }
    }
    if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 0x800) {
        accuracy += accuracy / 2;
    }
    if (battle_attacker_gear->status7C & 0x10) {
        penalty = 60;
    }
    if (battle_current_command->flagsA & 0x1000) {
        return 3;
    }
    if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
        if ((battle_target_gear->status80 | battle_target_gear->status82) & 0x400) {
            evasion += evasion / 2;
        }
        if (battle_target_gear->status7C & 0x10) {
            bonus = 30;
        }
        if (battle_target_gear->status7C & 0xC00) {
            return 1;
        }
    } else {
        accuracy /= 2;
        if (battle_target_record->pilot.status7C & 0x2000) {
            return 1;
        }
        if (battle_target_record->pilot.status80 & 0x1000) {
            return 1;
        }
        if ((battle_target_record->pilot.status84.half.active | battle_target_record->pilot.status84.half.permanent) & 0x800) {
            evasion += evasion / 2;
        }
    }
    if (battle_work_ptr->records[battle_target_slot].flags15A & 1) {
        if (rand() % 100 < 95) {
            return 2;
        }
        return 1;
    }
    margin = accuracy + battle_current_command->hitBonus - evasion;
    status = battle_target_record->pilot.status84.half.active | battle_target_record->pilot.status84.half.permanent;
    if (status & 0x20) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 3;
        }
        return 1;
    }
    if (status & 0x40) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 2;
        }
        return 1;
    }
    roll = rand() % 100 - margin;
    bonus += 85;
    bonus -= penalty;
    if (roll >= bonus) {
        return 3;
    }
    roll = rand() % 100 - margin;
    if (roll >= bonus) {
        return 2;
    }
    return 1;
}

/* 8009D948: Damage of a gear attack (80096FBC); a command with flag 0x100 scales it by
 * the gear's frame factor in quarters (4 when unset or with status 0x100), and
 * status 0x40 adds half again. */
u16 battle_resolve_gear_attack_value(void) {
    u16 damage = battle_resolve_attack_value();
    u8 factor;

    if (battle_current_command->flagsA & 0x100) {
        factor = battle_attacker_gear->frameFactor;
        if ((battle_attacker_gear->status7C & 0x100) || factor == 0) {
            factor = 4;
        }
        damage = factor * damage / 4;
        if ((battle_attacker_gear->status80 | battle_attacker_gear->status82) & 0x40) {
            damage += damage >> 1;
        }
    }
    return damage;
}

/* 8009DA04: Damage of an attack on a gear (80097610): a command with flag 0x100 adds
 * the target gear's armor (reduced by its defense percentage) when the target
 * is in a gear, and half again with status 0x20; otherwise the damage itself
 * is reduced by the defense percentage. */
u16 battle_resolve_gear_defense_value(void) {
    u16 damage = battle_resolve_defense_value();
    u16 armor;
    u8 defense;

    if (battle_current_command->flagsA & 0x100) {
        defense = battle_target_gear->defense;
        armor = battle_target_gear->armor;
        if (defense != 0 && armor != 0) {
            armor = armor * (100 - defense) / 100;
        }
        if (battle_work_ptr->records[battle_target_slot].flags15A & 0x80) {
            damage += armor;
        }
        if ((battle_target_gear->status80 | battle_target_gear->status82) & 0x20) {
            damage += damage >> 1;
        }
    } else if (battle_target_gear->defense != 0) {
        damage = damage * (100 - battle_target_gear->defense) / 100;
    }
    return damage;
}

/* 8009DB54: Scale damage by the target gear's resistance (in twentieths, 20 or more
 * nullifies) to the command's first element. */
s32 battle_resolve_gear_resistance(s32 damage) {
    u8 i;
    u8 element;
    u8 resistance;

    for (i = 0; i < 16; i++) {
        if (battle_current_command->elements & (0x8000 >> i)) {
            element = i;
            break;
        }
    }
    resistance = battle_target_gear->resistances[element];
    if (resistance != 0) {
        if (resistance < 20) {
            damage = damage * (20 - resistance) / 20;
        } else {
            damage = 0;
        }
    }
    return damage;
}

/* 8009DBFC: Roll a status onto the target's gear, from the attacking gear's first
 * part (`fromGear`: chance +0x13, kind +0x14, flag +0x10, 5 turns) or from
 * the command (+0x1c/+0x1d/+0x1e, turns +0x11): cancel opposite statuses,
 * check immunities, set the kind's status word and turn timer, or apply the
 * special kinds 10-16. Returns 1 when it took. */
s8 battle_resolve_gear_status_roll(u8 fromGear) {
    Combatant *record = &battle_work_ptr->records[battle_target_slot];
    CharacterRecord *pilot = &record->pilot;
    s8 chance;
    u8 kind;
    u16 flags;
    u8 turns;

    if (fromGear) {
        chance = battle_attacker_gear->entries[0].value10;
        kind = battle_attacker_gear->entries[0].value11;
        flags = battle_attacker_gear->entries[0].field0;
        turns = 5;
    } else {
        chance = battle_current_command->field1C;
        kind = battle_current_command->field1D;
        flags = battle_current_command->field1E;
        turns = battle_current_command->power;
    }
    if (!(battle_work_ptr->records[battle_target_slot].flags15A & 0x80)) {
        return 0;
    }
    if (chance < rand() % 100) {
        return 0;
    }
    switch (kind) {
    case 0:
        if ((flags & 0x20) && (battle_target_gear->status80 & 0x8000)) {
            battle_target_gear->status80 &= 0x7FFF;
            battle_target_record->pilot.status84.half.active &= 0x7FFF;
            return 1;
        }
        break;
    case 1:
        if (flags & 0xA) {
            if (battle_target_gear->status80 & 5) {
                battle_target_gear->status80 &= 0xFFFA;
                return 1;
            }
        } else if (flags & 5) {
            if (battle_target_gear->status80 & 0xA) {
                battle_target_gear->status80 &= 0xFFF5;
                return 1;
            }
        }
        break;
    }
    if (kind == 0) {
        if (flags & battle_target_gear->field7E) {
            return 0;
        }
        switch (flags) {
        case 0x400:
            record->statusTimers[0] = turns;
            battle_target_gear->status7C &= 0xFBFF;
            pilot->status7C |= 0x2000;
            break;
        case 0x1000:
            battle_target_gear->status7C &= 0xEFFF;
            pilot->status80 |= 0x2000;
            break;
        case 0x200:
            record->statusTimers[1] = turns;
            break;
        case 0x100:
            record->statusTimers[2] = turns;
            break;
        case 0x80:
            record->statusTimers[3] = turns;
            break;
        case 0x20:
            record->statusTimers[4] = turns;
            pilot->status7C |= 0x1000;
            break;
        case 0x10:
            record->statusTimers[5] = turns;
            break;
        }
        battle_target_gear->status7C |= flags;
    }
    if (kind == 1) {
        battle_target_gear->status80 |= flags;
        switch (flags) {
        case 0x1000:
            record->statusTimers[10] = turns;
            break;
        case 0x40:
            record->statusTimers[11] = turns;
            break;
        case 0x20:
            record->statusTimers[12] = turns;
            break;
        }
    }
    if (kind == 3) {
        if (flags & 0xF000) {
            if (battle_target_gear->status84.half.permanent & 0xF000) {
                return 0;
            }
            battle_target_gear->status84.half.active = flags | (battle_target_gear->status84.half.active & 0xFFF);
            record->statusTimers[7] = turns;
        }
        if (flags & 0xF00) {
            if (battle_target_gear->status84.half.permanent & 0xF00) {
                return 0;
            }
            battle_target_gear->status84.half.active = flags | (battle_target_gear->status84.half.active & 0xF0FF);
            record->statusTimers[8] = turns;
        }
    }
    if (kind == 10) {
        battle_target_gear->status84.half.active &= ~flags;
    }
    if (kind == 11 && !(battle_target_gear->field7E & 0x40)) {
        battle_target_gear->defense += flags;
        kind = 0;
        if (battle_target_gear->defense >= 100) {
            battle_target_gear->defense = 99;
        }
        flags = 0x40;
    }
    if (kind == 12) {
        battle_work_ptr->resultCode[battle_target_slot] = 0;
        battle_work_ptr->damage[battle_target_slot] = battle_target_gear->hp / flags;
    }
    if (kind == 13) {
        battle_work_ptr->resultCode[battle_target_slot] = 0;
        battle_work_ptr->damage[battle_target_slot] = battle_target_gear->hp - 1;
    }
    if (kind == 14) {
        battle_target_gear->status80 = 0;
        battle_target_gear->status84.half.active = 0;
        if (flags == 1) {
            battle_target_gear->status82 = 0;
            battle_target_gear->status84.half.permanent = 0;
        }
    }
    if (kind == 16) {
        battle_target_gear->status7C |= 1;
        battle_target_record->pilot.status7C |= 0x80;
    }
    if (kind == 15) {
        battle_target_gear->status7C &= 0xFFFE;
        battle_target_record->pilot.status7C &= 0xFF7F;
    }
    battle_status_show_gear_message(kind, flags);
    return 1;
}

/* 8009E268: Gear formula 2 (battle_gear_formula_table[2]): clear the target gear's defense
 * percentage. */
void battle_gear_formula2_clear_defense(void) {
    battle_target_gear->defense = 0;
}

/* 8009E278: Gear formula 3 (battle_gear_formula_table[3]): heal the target gear (result
 * code 2) by field 0x4F tenths of its maximum HP. */
void battle_gear_formula3_heal_by_part_amount(void) {
    u32 amount = battle_target_gear->field4F * battle_target_gear->maxHp / 10;

    battle_work_ptr->resultCode[battle_target_slot] = 2;
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 8009E2EC: Gear formula 4 (battle_gear_formula_table[4]): heal the target gear (result
 * code 2) by the command's power in twentieths of its maximum HP. */
void battle_gear_formula4_heal_by_power(void) {
    u32 amount = battle_current_command->power * battle_target_gear->maxHp / 20;

    battle_work_ptr->resultCode[battle_target_slot] = 2;
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 8009E364: Gear formula 10 (battle_gear_formula_table[10]): heal the target (result code
 * 2) by the attacker's ether times the command's power. */
void battle_gear_formula10_heal_by_ether(void) {
    u32 amount = battle_attacker_record->pilot.ether * battle_current_command->power;

    battle_work_ptr->resultCode[battle_target_slot] = 2;
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 8009E3C8: Gear formula 5 (battle_gear_formula_table[5]): put the target into state 4 with timer 6
 * at three turns. */
void battle_gear_formula5_set_attack_level4(void) {
    *battle_target_attack_level = 4;
    battle_work_area.records[battle_target_slot].statusTimers[6] = 3;
}

/* 8009E410: Gear formula 6 (battle_gear_formula_table[6]): drain the target gear's fuel (result 10)
 * by the command's power in twentieths of its maximum fuel. */
void battle_gear_formula6_drain_fuel(void) {
    s32 amount = battle_target_gear->maxFuel * battle_current_command->power / 20;

    battle_work_ptr->resultCode[battle_target_slot] = 10;
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 8009E48C: Gear formula 7 (battle_gear_formula_table[7]): restore the target gear's fuel (result
 * 11) by the command's power in twentieths of its maximum fuel. */
void battle_gear_formula7_restore_fuel(void) {
    s32 amount = battle_target_gear->maxFuel * battle_current_command->power / 20;

    battle_work_ptr->resultCode[battle_target_slot] = 11;
    battle_work_ptr->damage[battle_target_slot] = amount;
}

/* 8009E508: Gear formula 8 (battle_gear_formula_table[8]): clear the target gear's
 * statuses 0x7F4 and the target's status 0x20. */
void battle_gear_formula8_cure_statuses(void) {
    battle_target_gear->status7C &= 0xF80B;
    battle_target_record->pilot.status7A &= ~0x20;
}
