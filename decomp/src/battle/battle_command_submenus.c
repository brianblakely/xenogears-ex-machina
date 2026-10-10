/* Battle unit from 8008B478 to 8008CCCC: the technique, item and combo menus
 * and the execution of the chosen technique, item or combo steps. Its rodata
 * is the three jump tables at 80070314-80070370 (8008B478, 8008BED8,
 * 8008C81C), at 4 mod 8 between runs at 0 mod 8 (docs/matching.md). The text
 * boundaries are not fixed by the tables: this unit starts after 800861D0 and
 * ends before 80094EE4; these files take the tables' owners. */
#include "common.h"
#include "resident/gamedata.h"
#include "resident/sprite.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/command.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/menu_pages.h"
#include "battle/scene.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/work.h"
#include "own_declarations.h"

/* Callers convert arguments differently from the definition (800BAF40, in
 * 800B8098's unit, takes none): they pass a slot and a mode it ignores. */
void battle_empty_commit_step(u8 slot, s32 mode);

/* This unit's functions, declared before their first use. */
u8 battle_combo_menu_run(u8 member);          /* run the combo menu */

/* 8008CCCC's unit's art page quads as this unit calls them: unprototyped (the row
 * offset is a u8 there; the art menu passes its scroll word unnarrowed). */
void battle_art_menu_point_list();

/* 8008B478: Run the member's technique menu: four windows, a two-column list of
 * twelve visible cells scrolled by rows (800d3288 in pixels, 800d39d4 the
 * scroll request), until a technique is committed (1, its index in the
 * turn state +0x2e6) or the menu is cancelled (0). */
u8 battle_art_menu_run(u8 member) {
    s32 frame;
    u8 ticks;
    u8 cell;
    u8 top;
    u8 shownCell;
    u8 shownTop;
    s32 shownScroll;
    u8 result;
    s32 target;

    cell = 0;
    top = 0;
    shownCell = 0xFF;
    shownTop = 0xFF;
    shownScroll = 0xFFFF;
    result = 2;
    frame = 4;
    ticks = 0;
    battle_list_page_scroll = 0;
    battle_list_page_scroll_request = 0;
    battle_ui->unkCB = 0;
    battle_window_open(0, 0x1C, 0xA0, 0xAC, 0x38, 0, 1);
    battle_window_open(1, 0x20, 0x2C, 0x118, 0x70, 0, 1);
    battle_window_open(2, 0xD0, 0xA4, 0x50, 0x18, 0, 1);
    battle_window_open(3, 0xE6, 0xC0, 0x4C, 0x18, 0, 1);
    battle_art_menu_build_page(member);
    battle_show_direction_arrows();
    do {
        if (cell != shownCell || top != shownTop) {
            battle_art_menu_show_entry(member, cell, top);
            shownCell = cell;
            shownTop = top;
        }
        if (battle_list_page_scroll != shownScroll) {
            battle_art_menu_build_row_glyphs(battle_list_page_scroll);
            battle_art_menu_point_list(battle_list_page_scroll);
            shownScroll = battle_list_page_scroll;
        }
        battle_animate_cursor_glyph((cell % 2) * 0x80 + (cell % 2) * 4 + 0x2A, (cell / 2) * 16 + 0x38, &frame, &ticks);
        battle_wait_frame();
        switch (battle_pressed_key) {
        case 5:
            result = 0;
            break;
        case 4:
            if (battle_confirm_art(member, cell, top)) {
                result = 1;
                battle_turn_state->unk2E6 = cell + top * 2;
            }
            break;
        case 0:
            if (top * 2 + cell != 15) {
                if (cell + 1 == 12) {
                    battle_list_page_scroll_request = 1;
                } else {
                    cell++;
                }
            }
            break;
        case 2:
            if (top * 2 + cell > 0) {
                if (cell == 0 && top != 0) {
                    battle_list_page_scroll_request = 2;
                } else {
                    cell--;
                }
            }
            break;
        case 1:
            if (top * 2 + cell < 14) {
                if (cell + 2 >= 12) {
                    battle_list_page_scroll_request = 3;
                } else {
                    cell += 2;
                }
            }
            break;
        case 3:
            if (top * 2 + cell >= 2) {
                if (cell < 2 && top != 0) {
                    battle_list_page_scroll_request = 4;
                } else {
                    cell -= 2;
                }
            }
            break;
        }
        switch (battle_list_page_scroll_request) {
        case 1:
            target = (top + 1) * 16;
            if (battle_list_page_scroll >= target) {
                top++;
                cell--;
                battle_list_page_scroll = target;
                battle_list_page_scroll_request = 0;
            }
            break;
        case 2:
            target = (top - 1) * 16;
            if (battle_list_page_scroll < target) {
                top--;
                cell++;
                battle_list_page_scroll = target;
                battle_list_page_scroll_request = 0;
            }
            break;
        case 3:
            target = (top + 1) * 16;
            if (battle_list_page_scroll >= target) {
                top++;
                battle_list_page_scroll = target;
                battle_list_page_scroll_request = 0;
            }
            break;
        case 4:
            target = (top - 1) * 16;
            if (battle_list_page_scroll < target) {
                top--;
                battle_list_page_scroll = target;
                battle_list_page_scroll_request = 0;
            }
            break;
        }
    } while (result == 2);
    battle_hide_command_windows(result);
    battle_release_list_page_block();
    battle_hide_direction_arrows();
    battle_window_close(0);
    battle_window_close(1);
    battle_window_close(2);
    battle_window_close(3);
    return result;
}

/* 8008B908: Execute the chosen item (turn state +0x2e6) for the member: reset the
 * events, show the member's use model, commit the item against its targets
 * (the target candidates, or the chosen target), use one up unless the
 * item keeps (0x8000), apply its results as event 0xf5, react the targeted
 * enemies, and wait for the presentation; targeted party members react. */
void battle_execute_chosen_item(member)
u8 member;
{
    u16 targets;
    s32 i;

    battle_command_menu_sounds_enabled = 0;
    battle_highlight_slots(0);
    for (i = 0; i < 32; i++) {
        battle_area_events[i].type = 0xFF;
    }
    battle_menu_open_turn(1, member, battle_turn_state->slots[member].defaultTarget, battle_find_next_turn_slot(member));
    if (battle_item_effects[battle_turn_state->unk2E6].target & 7) {
        targets = battle_target_candidate_mask;
    } else {
        targets = battle_get_slot_bit(battle_target_cursor_slot);
    }
    battle_queue_reacting_enemies_event(targets, member);
    battle_clear_event_results();
    battle_commit_item_targets(member, targets, battle_turn_state->unk2E6);
    if (!(battle_committed_action.held & 0x8000)) {
        if (--battle_committed_action.itemCounts[battle_item_menu_chosen_row * 2 + battle_item_menu_chosen_column] == 0) {
            battle_committed_action.itemIds[battle_item_menu_chosen_row * 2 + battle_item_menu_chosen_column] = 0;
        }
    }
    battle_applying_item_results = 1;
    battle_accumulate_and_apply_results(battle_turn_state->eventCount);
    battle_applying_item_results = 0;
    battle_area_events[battle_turn_state->eventCount].type = 0xF5;
    battle_area_events[battle_turn_state->eventCount].parameter = battle_committed_action.animation;
    battle_area_events[battle_turn_state->eventCount].actor = member;
    battle_area_events[battle_turn_state->eventCount].targetMask = battle_committed_action.targets;
    for (i = 3; i < 11; i++) {
        if (battle_is_slot_in_mask(battle_committed_action.targets, i)) {
            battle_ai_tell_target_about_actor(member, i);
        }
    }
    battle_turn_state->eventCount++;
    battle_close_actor_event_queue(member);
    while (battle_turn_state->eventsDone == 0) {
        battle_wait_frame();
    }
    for (i = 0; i < 3; i++) {
        if (battle_is_slot_in_mask(targets, i)) {
            battle_ui->reaction[i] = 1;
        }
    }
}

/* 8008BC40: Hide the command windows (two panels); without `keep` show the +0x641c
 * lists. */
void battle_item_menu_hide_windows(u8 keep) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 0;
    battle_ui->windows[0] = battle_ui->windows[1] = 0;
    battle_ui->unkB7 = 0;
    if (keep == 0) {
        battle_ui->unkCB = 1;
    }
}

/* 8008BC98: Show the command windows (two panels, page 2) and frame the camera on the
 * member and its default target. */
void battle_item_menu_show_windows(u8 member) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 1;
    battle_ui->windows[0] = battle_ui->windows[1] = 1;
    battle_ui->unkB7 = 2;
    battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
    battle_highlight_slots(battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
}

/* 8008BD50: Use the list item at (column, row) for `member`: an item (from the
 * character list) starts its effect with target selection, 1 when started;
 * a gear part (from the gear list, inGear) is applied and its count taken,
 * 2 when applied. An empty entry buzzes (0x4f). */
u8 battle_item_menu_use_cell(member, column, row, inGear)
u8 member;
u8 column;
u8 row;
u8 inGear;
{
    u16 effect;
    u8 item;
    u8 result;

    effect = battle_item_effects[battle_item_ids[row * 2 + column]].target;
    result = 0;
    if (!inGear) {
        item = battle_item_ids[row * 2 + column];
    } else {
        item = battle_gear_part_ids[row * 2 + column];
    }
    if (item != 0) {
        if (!inGear) {
            battle_item_menu_hide_windows(1);
            battle_ui->unkC6 = 1;
            if (battle_choose_target(effect, member, 0)) {
                result = 1;
            } else {
                battle_ui->unkC6 = 0;
                battle_item_menu_show_windows(member);
            }
        } else if (((s32 (*)())battle_is_character4_entry_item)(item - 50)) {
            /* Both calls are unprototyped in the original: the part index
             * is passed unnarrowed and 8009a854's entry argument is left
             * undefined (it then uses whatever the register holds). */
            ((void (*)())battle_put_item_in_character4_entry)(item);
            if (--battle_gear_part_counts[row * 2 + column] == 0) {
                battle_gear_part_ids[row * 2 + column] = 0;
            }
            result = 2;
        }
    } else {
        battle_play_menu_sound(0x4F);
    }
    return result;
}

/* 8008BED8: Run the member's item menu: two windows, a two-column list of sixteen
 * visible cells (48 entries) scrolled a 13-pixel row at a time (800d3288,
 * requests in 800d39d4), the character list or (after 8008bd50 returns 2)
 * the gear parts list. Returns 1 when an item was started (its id in the
 * turn state +0x2e6, its cell in 800d3670/800c3d00), 0 when cancelled. */
u8 battle_item_menu_run(u8 member) {
    s32 frame;
    u8 ticks;
    u8 shownCell;
    u8 shownTop;
    u8 open;
    u8 cell;
    u8 top;
    u8 result;
    u8 pending;
    u8 list;
    s32 shownScroll;

    cell = 0;
    top = 0;
    shownCell = 0xFF;
    shownTop = 0xFF;
    shownScroll = 0xFFFF;
    result = 2;
    pending = 1;
    list = 0;
    frame = 4;
    open = 1;
    ticks = 0;
    battle_list_page_scroll = 0;
    battle_list_page_scroll_request = 0;
    battle_ui->unkCB = 0;
    battle_window_open(0, 0x20, 0x58, 0x118, 0x78, 0, 1);
    battle_window_open(1, 0x10, 0x2C, 0x128, 0x28, 0, 1);
    battle_show_direction_arrows();
    do {
        if (pending != 0) {
            battle_item_menu_build_page(member, pending - 1, open);
            list = pending - 1;
            pending = 0;
            open = 0;
        }
        if (cell != shownCell || top != shownTop) {
            battle_item_menu_show_cell(cell, top, list);
            shownCell = cell;
            shownTop = top;
        }
        if (battle_list_page_scroll != shownScroll) {
            battle_item_menu_build_row_glyphs(battle_list_page_scroll / 13 * 2);
            battle_item_menu_point_list(battle_list_page_scroll);
            shownScroll = battle_list_page_scroll;
        }
        battle_animate_cursor_glyph((cell % 2) * 0x80 + 0x2A + (cell % 2) * 2, (cell / 2) * 13 + 0x63, &frame, &ticks);
        battle_wait_frame();
        switch (battle_pressed_key) {
        case 5:
            result = 0;
            break;
        case 4:
            switch (battle_item_menu_use_cell(member, cell, top, list)) {
            case 1:
                battle_turn_state->unk2E6 = battle_item_ids[top * 2 + cell];
                battle_item_menu_chosen_column = cell;
                battle_item_menu_chosen_row = top;
                result = 1;
                break;
            case 2:
                pending = 2;
                break;
            }
            break;
        case 0:
            if (top * 2 + cell != 47) {
                if (cell + 1 == 16) {
                    battle_list_page_scroll_request = 1;
                } else {
                    cell++;
                }
            }
            break;
        case 2:
            if (top * 2 + cell > 0) {
                if (cell == 0 && top != 0) {
                    battle_list_page_scroll_request = 2;
                } else {
                    cell--;
                }
            }
            break;
        case 1:
            if (top * 2 + cell < 46) {
                if (cell + 2 >= 16) {
                    battle_list_page_scroll_request = 3;
                } else {
                    cell += 2;
                }
            }
            break;
        case 3:
            if (top * 2 + cell >= 2) {
                if (cell < 2 && top != 0) {
                    battle_list_page_scroll_request = 4;
                } else {
                    cell -= 2;
                }
            }
            break;
        }
        switch (battle_list_page_scroll_request) {
        case 1:
            battle_list_page_scroll = (top + 1) * 13;
            top++;
            cell--;
            battle_list_page_scroll_request = 0;
            break;
        case 2:
            battle_list_page_scroll = (top - 1) * 13;
            top--;
            cell++;
            battle_list_page_scroll_request = 0;
            break;
        case 3:
            battle_list_page_scroll = (top + 1) * 13;
            top++;
            battle_list_page_scroll_request = 0;
            break;
        case 4:
            battle_list_page_scroll = (top - 1) * 13;
            top--;
            battle_list_page_scroll_request = 0;
            break;
        }
    } while (result == 2);
    battle_item_menu_hide_windows(result);
    battle_release_list_page_block();
    battle_hide_direction_arrows();
    battle_window_close(0);
    battle_window_close(1);
    return result;
}

/* 8008C360: Hide the command windows; with `close` also close windows 0 and 1 and
 * release the graphics block. */
void battle_combo_menu_hide_windows(u8 close) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 0;
    battle_ui->unkB7 = 0;
    if (close != 0) {
        battle_ui->unkC6 = 0;
        battle_window_close(0);
        battle_window_close(1);
        battle_wait_frame();
        battle_release_list_page_block();
        battle_hide_direction_arrows();
    } else {
        battle_ui->windows[0] = battle_ui->windows[1] = 0;
    }
}

/* 8008C3F0: Show the command windows (two panels, page 3) and frame the camera on the
 * member and its default target. */
void battle_combo_menu_show_windows(u8 member) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 1;
    battle_ui->unkB7 = 3;
    battle_ui->windows[0] = battle_ui->windows[1] = 1;
    battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
    battle_highlight_slots(battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
}

/* 8008C4A8: Run the member's combo: choose the steps (8008c81c); when cancelled
 * reopen the command windows and return 1. Otherwise enter the attack page
 * against the chosen target (an event 0xf3 first when it reacts while
 * down), commit each chosen step (action step + 8) as an event, move into
 * the target's group and wait for the presentation. Returns 0. */
u8 battle_combo_command_run(member)
u8 member;
{
    u8 cancelled = 1;
    s32 i;

    if (battle_combo_menu_run(member)) {
        battle_empty_commit_step(member, 0x100);
        battle_combo_menu_hide_windows(1);
        battle_ui->unkCB = 1;
        cancelled = 0;
    } else {
        battle_empty_commit_step(member, 0x80);
        battle_ui->unkAF = 0;
        battle_combo_menu_hide_windows(1);
        battle_command_menu_sounds_enabled = 0;
        battle_highlight_slots(0);
        battle_turn_state->slots[member].defaultTarget = battle_target_cursor_slot;
        battle_plan_approach_route(member, battle_turn_state->slots[member].defaultTarget);
        battle_acting_with_partner = 1;
        battle_attack_page_enter(member);
        if (battle_work_area.records[battle_turn_state->slots[member].defaultTarget].pilot.flags34 & 0x800) {
            battle_area_events[battle_turn_state->eventCount].actor = member;
            battle_area_events[battle_turn_state->eventCount].type = 0xF3;
            battle_area_events[battle_turn_state->eventCount].parameter = battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget);
            battle_turn_state->eventCount++;
        }
        for (i = 0; i < 7; i++) {
            if (battle_turn_state->combo[i] != 0xFF) {
                battle_turn_state->unk2DC = battle_turn_state->combo[i] + 8;
                battle_commit_action(member, battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget), battle_turn_state->unk2DC - 1);
                battle_accumulate_and_apply_results(battle_turn_state->eventCount);
                battle_area_events[battle_turn_state->eventCount].type = battle_turn_state->unk2DC - 1;
                battle_area_events[battle_turn_state->eventCount].actor = member;
                battle_area_events[battle_turn_state->eventCount].targetMask = battle_committed_action.targets;
                battle_turn_state->eventCount++;
            }
        }
        battle_close_actor_event_queue(member);
        battle_join_target_group(member, battle_turn_state->slots[member].defaultTarget);
        while (battle_turn_state->eventsDone == 0) {
            battle_wait_frame();
        }
    }
    return cancelled;
}

/* 8008C81C: Run the member's combo menu: list the known combo steps (0-6) with their
 * AP costs, then move the cursor over the eight cells (cell 7 confirms),
 * add the step under it while AP last (up to seven steps) or take the last
 * one back, and on confirm pick the target. Returns 0 when confirmed with a
 * target (the remaining AP stored), 1 when cancelled. The cell marks are
 * cleared with the cell count held in `n` (a variable bound, so the loop
 * runs ascending). */
u8 battle_combo_menu_run(u8 member) {
    u8 steps[7];
    u8 marks[8];
    u8 costs[7];
    u8 spare[8];
    u8 paid[7];
    s32 frame;
    u8 ticks;
    u8 redraw;
    u8 ap;
    u8 cursor;
    u8 count;
    u8 done;
    s32 i;
    s32 n;

    done = 0;
    cursor = 0;
    redraw = 1;
    count = 0;
    ap = battle_slot_flags[member].unk0;
    frame = 4;
    ticks = 0;
    battle_ui->unkCB = 0;
    for (i = 0; i < 7; i++) {
        steps[i] = 0xFF;
        costs[i] = 0;
        battle_turn_state->combo[i] = 0xFF;
        battle_combo_step_buttons[i] = 0xFF;
    }
    n = 8;
    for (i = 0; i < n; i++) {
        marks[i] = 0;
    }
    n = 0;
    for (i = 0; i < 7; i++) {
        if (battle_is_flag_in_mask(game_data.skills[mode_battle_party_ids[member]].counterSkills, i)) {
            steps[n] = i;
            costs[n] = battle_work_area.partyCommands[member][i + 7].apCost;
            n++;
        }
    }
    battle_window_open(0, 0x16, 0x5C, 0x122, 0x4C, 1, 1);
    battle_window_open(1, 0x10, 0x2C, 0xE8, 0x2C, 1, 1);
    while (battle_ui->windowOpen[0] == 0 || battle_ui->windowOpen[1] == 0) {
        battle_wait_frame();
    }
    battle_combo_menu_build_page(member, steps, costs);
    battle_show_direction_arrows();
    while (done == 0) {
        if (redraw) {
            battle_combo_menu_build_entry_display(member, ap);
            redraw = 0;
        }
        battle_animate_cursor_glyph((cursor % 2) * 0x88 + 0x1E + (cursor % 2) * 4, (cursor / 2) * 16 + 0x64, &frame, &ticks);
        battle_wait_frame();
        switch (battle_pressed_key) {
        case 5:
            if (count != 0) {
                count--;
                redraw = 1;
                ap += paid[count];
                battle_turn_state->combo[count] = 0xFF;
                battle_combo_step_buttons[count] = 0xFF;
            } else {
                done = 2;
            }
            break;
        case 4:
            if (cursor == 7) {
                if (battle_turn_state->combo[0] == 0xFF) {
                    done = 2;
                } else {
                    battle_combo_menu_hide_windows(0);
                    battle_ui->unkC6 = 1;
                    if (battle_choose_target(0x1000, member, 1)) {
                        battle_slot_flags[member].unk0 = ap;
                        done = 1;
                    } else {
                        battle_ui->unkC6 = 0;
                        battle_combo_menu_show_windows(member);
                    }
                }
            } else if (count != 7 && steps[cursor] != 0xFF && ap >= costs[cursor]) {
                battle_turn_state->combo[count] = steps[cursor];
                battle_combo_step_buttons[count] = cursor;
                ap -= costs[cursor];
                paid[count] = costs[cursor];
                count++;
                redraw = 1;
            }
            break;
        case 0:
            cursor++;
            if (cursor >= 8) {
                cursor--;
            }
            break;
        case 1:
            cursor += 2;
            if (cursor >= 8) {
                cursor -= 2;
            }
            break;
        case 2:
            if (cursor != 0) {
                cursor--;
            }
            break;
        case 3:
            if (cursor >= 2) {
                cursor -= 2;
            }
            break;
        case 6:
        case 7:
        case 9:
        case 10:
            cursor = 7;
            break;
        }
    }
    return done - 1;
}
