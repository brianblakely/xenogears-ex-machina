/* Menu overlay unit from 801DBDB4: the field menu's arts, equipment and
 * status screens, the gear stat computations, the file screen's views and
 * the label builders. Rodata 801C50FC-801C5278, text 801DBDB4-801E8070 and
 * its variables 801EA724-801EA8F4. Named after 801DBE54, where it started
 * before its variables moved the boundary. Its rodata starts at 801C50FC,
 * 4 mod 8 (docs/matching.md, jump tables), where 801DBE54's table sits;
 * menu_delete_command_run is the last function using the previous unit's rodata. Its
 * uninitialized variables open with the item list's scroll bar, which
 * 801DBDB4 sizes and 801DBE54 reads, so the text boundary lies after
 * 801CD2AC and at or before 801DBDB4, where it is kept. An earlier one would
 * move the .bss boundary with it and must not split 801D84B4/801D8644 or
 * 801D9C84/801D9E3C, which share variables; the item screen's helpers from
 * 801DA4A8, which only this unit's functions call, may belong here too. */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/text.h"
#include "menu/card.h"
#include "menu/panel.h"
#include "menu/screen.h"
#include "menu/tables.h"
#include "menu.h"
#include "file.h"
#include "field.h"

/* The unit's uninitialized variables, zero in the file after slot39's, each
 * in a slot of whole words (decomp/Makefile). */
static s16 menu_item_screen_bar_height;            /* 801EA724: item list scroll bar */
static s32 menu_item_screen_scroll_limit;          /* 801EA728 */
static s16 menu_item_screen_bar_step;              /* 801EA72C */
static u8 menu_equip_screen_candidate_ids[200];    /* 801EA730: equipment list entry ids */
static u8 menu_equip_screen_candidate_counts[200]; /* 801EA7F8: equipment list entry counts */
static u8 menu_save_title_char_is_two_byte;        /* 801EA8C0: the last printed character was two-byte */
static u8 menu_save_icon_palette[0x20];            /* 801EA8C4: icon palette buffer */
static RECT menu_save_icon_image_rect;             /* 801EA8E4: icon image area */
static RECT menu_save_icon_palette_rect;           /* 801EA8EC: icon palette area */

/* 801DBDB4: Size the item list's scroll bar from the last occupied inventory entry. */
void menu_item_screen_size_scroll_bar(void) {
    s32 i;
    s32 last;
    s32 pages;

    for (i = 0; i < 150; i++) {
        if (game_data.itemIds[i] != 0) {
            last = i;
        }
    }
    if (last < 16) {
        menu_item_screen_bar_height = 0x74;
        menu_item_screen_scroll_limit = 0;
        menu_item_screen_bar_step = 0;
    } else {
        pages = (last - 16) / 2 + 1;
        menu_item_screen_bar_height = 0x4a;
        menu_item_screen_scroll_limit = pages;
        menu_item_screen_bar_step = 0x1068 / pages;
    }
}

/* 801DBE54: The item screen: a two-column list of eight rows scrolled over the
 * inventory with a cursor, the selected entry's description and its
 * windows. Confirm selects an entry, uses it when selected again or swaps
 * it with the selected one; cancel clears the selection or leaves. */
u8 menu_item_screen_run(void) {
    u8 running;
    u8 windows;
    s32 scroll;
    s32 scrollShown;
    s32 cursor;
    s32 cursorShown;
    s32 selected;

    running = 1;
    windows = 1;
    scroll = 0;
    scrollShown = 0xff;
    cursor = 0;
    cursorShown = 0xff;
    selected = 0xff;
    menu_item_screen_open();
    menu_item_screen_size_scroll_bar();
    menu_list_cursor_alloc(0);
    menu_list_cursor_alloc(1);
    while (running) {
        menu_run_frame();
        if (scroll != scrollShown) {
            menu_item_screen_build_list(scroll);
            menu_scroll_bar_show(0xc, (u16)menu_item_screen_bar_step * scroll / 100 + 0x12, (u16)menu_item_screen_bar_height);
            scrollShown = scroll;
        }
        menu_list_cursor_place(cursor, scroll, 0, 0);
        if (cursor != cursorShown) {
            menu_item_screen_show_description(cursor, scroll);
            cursorShown = cursor;
        }
        if (windows) {
            menu_panel_open(3, 0xc, 0xa, 0x124, 0x84, 0, 1, 4, 1);
            menu_panel_open(4, 8, 0x8f, 0x130, 0x22, 0, 1, 4, 0);
            windows = 0;
            menu_view_start_zoom_in();
            menu_field_blocks_slide(0, 0);
        }
        menu_list_cursor_place(selected, scroll, 1, 1);
        switch (menu_state_current->input) {
        case 4:
            if (selected == 0xff) {
                selected = scroll * 2 + cursor;
            } else {
                if (scroll * 2 + cursor == selected) {
                    if (menu_item_screen_use_item(scroll, cursor)) {
                        scrollShown = 0xff;
                        cursorShown = 0xff;
                    }
                } else {
                    menu_item_screen_swap_entries(scroll * 2 + cursor, selected);
                    scrollShown = 0xff;
                    cursorShown = 0xff;
                }
                selected = 0xff;
            }
            break;
        case 5:
            if (selected == 0xff) {
                running = 0;
            } else {
                selected = 0xff;
            }
            break;
        case 1:
            if (cursor + 2 >= 16) {
                if (menu_item_screen_scroll_limit < ++scroll) {
                    scroll--;
                }
            } else {
                cursor += 2;
            }
            cursorShown = 0xff;
            break;
        case 3:
            if (cursor - 2 < 0) {
                if (--scroll < 0) {
                    scroll++;
                }
            } else {
                cursor -= 2;
            }
            cursorShown = 0xff;
            break;
        case 0:
            if (cursor + 1 >= 16) {
                if (menu_item_screen_scroll_limit < ++scroll) {
                    scroll--;
                } else {
                    cursor = 14;
                }
            } else {
                cursor++;
            }
            cursorShown = 0xff;
            break;
        case 2:
            if (cursor - 1 < 0) {
                if (--scroll < 0) {
                    scroll++;
                } else {
                    cursor = 1;
                }
            } else {
                cursor--;
            }
            cursorShown = 0xff;
            break;
        case 9:
            scroll += 8;
            if (menu_item_screen_scroll_limit < scroll) {
                scroll = menu_item_screen_scroll_limit;
            }
            cursorShown = 0xff;
            break;
        case 10:
            scroll -= 8;
            if (scroll < 0) {
                scroll = 0;
            }
            cursorShown = 0xff;
            break;
        }
    }
    menu_markers_hide();
    menu_list_cursor_free(0);
    menu_list_cursor_free(1);
    menu_label_clear_shown(8, menu_state_current->flags->labels10e0_shown);
    return 1;
}

/* 801DC1D4: Open the arts screen of `kind` (0 the character's arts, data view 2; 1 its
 * gear's, view 5; 2 the gear's other list, view 6, with the second label
 * page): its labels, its 1094-byte list block and the view's data. */
void menu_arts_screen_open(u8 kind) {
    void *block;
    u8 view;
    s32 page;

    page = 0;
    block = heap_alloc(0x1094, 0);
    menu_state_current->arts_list = block;
    bzero(block, 0x1094);
    switch (kind) {
    case 0:
        view = 2;
        break;
    case 1:
        view = 5;
        break;
    case 2:
        view = 6;
        page = 1;
        break;
    }
    menu_label_render_table(8, menu_state_current->labels10e0, menu_item_arts_label_ids + page * 8, menu_state_current->flags->labels10e0_shown);
    menu_load_or_release_data_set(view);
    menu_markers_layout(2);
    menu_member_marks_show(2, kind);
}

/* 801DC2CC: Close the arts screen of `kind`: panels 3-6, its list block and the
 * view's data (the view | 10 releases it). */
void menu_arts_screen_close(u8 kind) {
    u8 view;

    menu_panel_close(3);
    menu_panel_close(4);
    menu_panel_close(5);
    menu_panel_close(6);
    menu_markers_hide();
    menu_state_current->flags->arts_list_shown = 0;
    menu_run_frame();
    switch (kind) {
    case 0:
        view = 2;
        break;
    case 1:
        view = 5;
        break;
    case 2:
        view = 6;
        break;
    }
    menu_load_or_release_data_set(view | 0x10);
    heap_free(menu_state_current->arts_list);
    menu_state_current->flags->field_menu2_shown = 1;
    menu_state_current->flags->panels_shown[1] = 1;
}

/* 801DC3D8: Build the arts list of party slot `slot` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list): each known art's name and cost, then the
 * current and maximum ether (fuel for kind 2) in rows 12 and 13; arts that
 * cannot be used now are greyed. */
void menu_arts_screen_build_list(u8 slot, u8 kind) {
    u8 codes[10];
    u8 text[16];
    RECT rect;
    s32 powers[5];
    s32 ep;
    s32 row;
    s32 digits;
    s32 n;
    s32 pos;
    s32 cost;
    s32 value;
    s32 digit;
    u8 *image;
    u8 known;
    u8 started;
    u8 grey;
    u8 fuel;

    fuel = 0;
    ep = game_data.characters[menu_state_current->flags->party[slot]].ep;
    image = heap_alloc(0x3f6, 0);
    powers[0] = 1;
    powers[1] = 10;
    powers[2] = 100;
    powers[3] = 1000;
    powers[4] = 10000;
    for (row = 4; row >= 0; row--) {
        codes[row * 2 + 1] = 0;
    }
    for (row = 0; row < 14; row++) {
        bzero(codes, 8);
        known = 0;
        switch (kind) {
        case 0:
            known = menu_test_bit_msb_first(game_data.skills[menu_state_current->flags->party[slot]].levelSkills, row) || row >= 12;
            break;
        case 1:
            known = menu_test_bit_msb_first(game_data.skills[menu_state_current->flags->party[slot]].unlocksB, row) || row >= 12;
            break;
        case 2:
            if (!(row & 1)) {
                known = menu_test_bit_msb_first(game_data.skills[menu_state_current->flags->party[slot]].flags1A, row / 2) || row >= 12;
            }
            break;
        }
        if (known) {
            digits = 2;
            if (row < 12) {
                switch (kind) {
                case 0:
                    menu_state_current->arts_list->names[row].width = window_render_text_line(
                        text_get_character_art_name(menu_state_current->flags->party[slot] * 16 + row), image, 0x24, 0);
                    cost = menu_state_current->tables->arts[menu_state_current->flags->party[slot]][22 + row].cost;
                    break;
                case 1:
                    menu_state_current->arts_list->names[row].width = window_render_text_line(
                        text_get_gear_art_name(game_data.characters[menu_state_current->flags->party[slot]].gearId * 16 + row), image, 0x24, 0);
                    cost = (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[slot]].gearId][21 + row].cost;
                    break;
                case 2:
                    menu_state_current->arts_list->names[row].width = window_render_text_line(
                        text_get_gear_fuel_art_name(game_data.characters[menu_state_current->flags->party[slot]].gearId * 4 + row / 2), image, 0x24, 0);
                    ep = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].fuel;
                    cost = (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[slot]].gearId][37 + row / 2].gearCost;
                    digits = 4;
                    break;
                }
            } else if (row == 12) {
                if (kind != 2) {
                    cost = game_data.characters[menu_state_current->flags->party[slot]].ep;
                } else {
                    fuel = 3;
                    cost = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].fuel;
                    digits = 5;
                }
            } else {
                cost = game_data.characters[menu_state_current->flags->party[slot]].maxEp;
            }
            value = cost;
            started = 0;
            pos = 0;
            for (n = digits - 1; n > 0; n--) {
                digit = value / powers[n];
                if (digit != 0 || started) {
                    codes[pos * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * powers[n];
                } else {
                    codes[pos * 2] = 0xc3;
                }
                pos++;
            }
            codes[pos * 2] = value % 10 + 0x10;
            text_decode_codes(codes, text, digits);
            grey = 0x80;
            menu_state_current->arts_list->values[row].width = window_render_text_line(text, image, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = row / 2 * 0xd + 0x80;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)image);
            DrawSync(0);
            if (row < 12) {
                switch (kind) {
                case 0:
                    if (menu_test_bit(menu_arts_screen_usable_art_masks[menu_state_current->flags->party[slot]], row)) {
                        if (ep < cost) {
                            grey = 0;
                        }
                    } else {
                        grey = 0;
                    }
                    break;
                case 1:
                    grey = 0;
                    break;
                case 2:
                    if (row != 0 || ep < cost) {
                        grey = 0;
                    }
                    break;
                }
            }
            if (row < 12) {
                menu_label_init_quads(&menu_state_current->arts_list->names[row], row, 0x80, grey | 1);
                menu_set_rect_verts(menu_state_current->arts_list->names[row].verts, (row % 2 * 0x88 + 0x24) & 0xfffc,
                              (row / 2 * 0x10 + 0x12) & 0xfffe, menu_state_current->arts_list->names[row].width, 0xd);
            }
            menu_label_init_quads(&menu_state_current->arts_list->values[row], row, 0x80, grey | 2);
            menu_set_rect_verts(menu_state_current->arts_list->values[row].verts, menu_arts_screen_cost_x_table[row], menu_arts_screen_cost_y_table[row],
                          menu_state_current->arts_list->values[row].width, 0xd);
            menu_state_current->arts_list->names[row].buffer = menu_state_current->buffer_index;
            menu_state_current->arts_list->values[row].buffer = menu_state_current->buffer_index;
            menu_state_current->arts_list->shown[row] = grey | 1;
        } else {
            menu_state_current->arts_list->shown[row] = 0;
        }
    }
    heap_free(image);
    menu_label_place(8, menu_state_current->labels10e0, menu_item_target_label_ids, menu_file_command_label_x_offsets, menu_state_current->flags->labels10e0_shown, 6, 1, fuel + 2);
    menu_label_place(8, menu_state_current->labels10e0, menu_item_target_label_ids, menu_file_command_label_x_offsets, menu_state_current->flags->labels10e0_shown, 7, 1, fuel + 2);
    menu_name_label_layout(&menu_state_current->arts_list->footer, slot, kind, 1);
    menu_state_current->flags->arts_list_shown = 1;
}

/* 801DCE60: Show the description of arts list row `row` for party slot `slot` (`kind`
 * 0 the character's, 1 the gear's, 2 the gear's paired rows): the entry's
 * two text lines, a copy of its name and its target labels; an unused row
 * hides them. */
void menu_arts_screen_show_description(u8 slot, u8 row, u8 kind) {
    RECT rect;
    ArtInfo *effect;
    u8 *image;
    u16 text;
    u16 target;
    u8 all;
    u8 targetKind;
    s32 i;

    switch (kind) {
    case 0:
        text = (menu_state_current->flags->party[slot] << 5) + row * 2;
        break;
    case 1:
        text = (game_data.characters[menu_state_current->flags->party[slot]].gearId << 5) + row * 2;
        break;
    case 2:
        text = game_data.characters[menu_state_current->flags->party[slot]].gearId * 8 + (row & ~1);
        break;
    }
    if (menu_state_current->arts_list->shown[row] != 0) {
        image = heap_alloc(0x618, 0);
        bzero(image, 0x618);
        menu_state_current->arts_list->extra[0].width =
            window_render_text_line(text_get_resource_entry(menu_state_current->arts_list->texts, text), image, 0x39, 0);
        menu_state_current->arts_list->extra[1].width =
            window_render_text_line(text_get_resource_entry(menu_state_current->arts_list->texts, text + 1), image, 0x39, 1);
        rect.x = 0x140;
        rect.y = 0x4e;
        rect.w = 0x3c;
        rect.h = 0xd;
        LoadImage(&rect, (u_long *)image);
        DrawSync(0);
        menu_label_init_quads(&menu_state_current->arts_list->extra[0], 0, 0, 0);
        menu_quad_place(&menu_state_current->arts_list->extra[0].polys[menu_state_current->buffer_index], 0x1c, 0x9e, 0, 0x4e,
                      menu_state_current->arts_list->extra[0].width, 0xd);
        menu_set_rect_verts(menu_state_current->arts_list->extra[0].verts, 0x1c, 0x9e, menu_state_current->arts_list->extra[0].width, 0xd);
        menu_label_init_quads(&menu_state_current->arts_list->extra[1], 1, 0, 0);
        menu_quad_place(&menu_state_current->arts_list->extra[1].polys[menu_state_current->buffer_index], 0x1c, 0xae, 0, 0x4e,
                      menu_state_current->arts_list->extra[1].width, 0xd);
        menu_set_rect_verts(menu_state_current->arts_list->extra[1].verts, 0x1c, 0xae, menu_state_current->arts_list->extra[1].width, 0xd);
        heap_free(image);
        memmove(&menu_state_current->arts_list->headA, &menu_state_current->arts_list->names[row], sizeof(MenuLabel));
        menu_set_rect_verts(menu_state_current->arts_list->headA.verts, 0x12, 0x8e, menu_state_current->arts_list->names[row].width, 0xd);
        (menu_state_current->arts_list->headA.polys + menu_state_current->buffer_index)->r0 = 0x80;
        (menu_state_current->arts_list->headA.polys + menu_state_current->buffer_index)->g0 = 0x80;
        (menu_state_current->arts_list->headA.polys + menu_state_current->buffer_index)->b0 = 0x80;
        SetSemiTrans(&menu_state_current->arts_list->headA.polys[menu_state_current->buffer_index], 0);
        menu_label_clear_shown(8, menu_state_current->flags->labels10e0_shown);
        switch (kind) {
        case 0:
            effect = menu_state_current->tables->arts[menu_state_current->flags->party[slot]] + row + 22;
            break;
        case 1:
            effect = (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[slot]].gearId] + row + 21;
            break;
        case 2:
            effect = (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[slot]].gearId] + (row >> 1) + 37;
            break;
        }
        target = effect->target;
        if (target & 0x4000) {
            all = 2;
        } else if (target & 0x1000) {
            all = 0;
        } else {
            all = 1;
        }
        menu_label_place(8, menu_state_current->labels10e0, menu_item_target_label_ids, menu_file_command_label_x_offsets, menu_state_current->flags->labels10e0_shown, all, 0, 2);
        targetKind = (effect->target & 3) + 3;
        menu_label_place(8, menu_state_current->labels10e0, menu_item_target_label_ids, menu_file_command_label_x_offsets, menu_state_current->flags->labels10e0_shown, targetKind, 0, 2);
        menu_state_current->arts_list->headA.buffer = menu_state_current->buffer_index;
        menu_state_current->arts_list->headB.buffer = menu_state_current->buffer_index;
        menu_state_current->arts_list->extra[0].buffer = menu_state_current->buffer_index;
        menu_state_current->arts_list->extra[1].buffer = menu_state_current->buffer_index;
        menu_state_current->arts_list->extraShown = 1;
        menu_state_current->flags->labels10e0_shown[6] = 1;
        menu_state_current->flags->labels10e0_shown[7] = 1;
    } else {
        menu_state_current->arts_list->extraShown = 0;
        for (i = 0; i < 6; i++) {
            menu_state_current->flags->labels10e0_shown[i] = 0;
        }
    }
}

/* 801DD5E8: Shade the file list screen's texts by `mode` (801e8eac): windows 5 and 6,
 * each built name and value of the first twelve rows, the cursor, the
 * heading and the two extra labels. */
void menu_arts_screen_set_dimmed(u8 mode) {
    s32 i;

    menu_panel_set_dimmed(5, mode);
    menu_panel_set_dimmed(6, mode);
    for (i = 0; i < 12; i++) {
        if (menu_state_current->arts_list->names[i].polys[menu_state_current->arts_list->names[i].buffer].r0 != 0x20) {
            menu_quad_set_blending(&menu_state_current->arts_list->names[i].polys[menu_state_current->arts_list->names[i].buffer], mode);
            menu_quad_set_blending(&menu_state_current->arts_list->values[i].polys[menu_state_current->arts_list->values[i].buffer], mode);
        }
    }
    menu_quad_set_blending(&menu_state_current->cursors[0]->polys[menu_state_current->cursors[0]->buffer], mode);
    menu_quad_set_blending(&menu_state_current->arts_list->headA.polys[menu_state_current->arts_list->headA.buffer], mode);
    for (i = 0; i < 2; i++) {
        menu_quad_set_blending(&menu_state_current->arts_list->extra[i].polys[menu_state_current->arts_list->extra[i].buffer], mode);
    }
}

/* 801DD790: Use art `row` of party slot `slot` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list) from the menu: select the targets (the
 * whole party for all-target arts, else the cursor's slot) and use it on
 * confirm while its cost can be paid, until cancelled. */
void menu_arts_screen_use_art(u8 slot, s32 row, u8 kind) {
    ArtInfo *effect;
    s32 x;
    s32 cursor;
    s32 all;
    s32 i;
    s32 sound;
    u8 redraw;
    u8 hit;
    u8 targets;
    u8 used;

    targets = 1;
    redraw = 1;
    x = 0;
    cursor = menu_state_current->first_member;
    switch (kind) {
    case 0:
        effect = menu_state_current->tables->arts[menu_state_current->flags->party[slot]] + row + 22;
        break;
    case 1:
        effect = (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[slot]].gearId] + row + 21;
        break;
    case 2:
        x = 0x18;
        effect = (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[slot]].gearId] + row + 37;
        cursor = slot;
        break;
    }
    menu_panel_open(2, 0x10, 0xe, x + 0x90, 0xb0, 0, 0, 4, 0);
    all = effect->target & 1;
    while (targets) {
        menu_run_frame();
        if (redraw) {
            menu_arts_screen_build_list(slot, kind);
            menu_arts_screen_show_description(slot, row, kind);
            menu_arts_screen_set_dimmed(1);
            menu_target_panels_build(kind);
            redraw = 0;
        }
        targets = 0;
        menu_state_current->markers->shown[0] = menu_state_current->markers->shown[1] = menu_state_current->markers->shown[2] = 0;
        if (all) {
            for (i = 0; i < 3; i++) {
                if (menu_state_current->flags->party[i] != 0xff) {
                    targets |= 1 << i;
                    menu_state_current->markers->shown[i] = 1;
                }
            }
        } else {
            targets = 1 << cursor;
            menu_state_current->markers->shown[cursor] = 1;
        }
        menu_state_current->flags->markers_shown = 1;
        if (kind != 2) {
            if (game_data.characters[menu_state_current->flags->party[slot]].ep - effect->cost < 0) {
                targets = 0;
            }
        } else {
            if (game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].fuel - effect->gearCost < 0) {
                targets = 0;
            }
        }
        if (!targets) {
            break;
        }
        switch (menu_state_current->input) {
        case 5:
            targets = 0;
            break;
        case 4:
            used = 0;
            for (i = 0; i < 3; i++) {
                hit = 0;
                if (menu_test_bit(targets, i)) {
                    if (kind != 2) {
                        if (game_data.characters[menu_state_current->flags->party[i]].hp != game_data.characters[menu_state_current->flags->party[i]].maxHp) {
                            used = 1;
                            hit = 1;
                        }
                    } else {
                        if (game_data.gears[game_data.characters[menu_state_current->flags->party[i]].gearId].hp !=
                            game_data.gears[game_data.characters[menu_state_current->flags->party[row]].gearId].maxHp) {
                            used = 1;
                            hit = 1;
                        }
                    }
                    if (hit) {
                        menu_apply_restoring_art(menu_state_current->tables, menu_state_current->flags->party[slot], menu_state_current->flags->party[i],
                                      row, kind);
                    }
                }
            }
            if (used) {
                if (kind != 2) {
                    game_data.characters[menu_state_current->flags->party[slot]].ep -= effect->cost;
                } else {
                    game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].fuel -= effect->gearCost;
                }
                sound = 0x37;
            } else {
                sound = 4;
            }
            redraw = 1;
            menu_play_sound(sound);
            break;
        case 1:
            if (kind == 0) {
                cursor = menu_step_party_slot(cursor, 0, 0);
            }
            break;
        case 3:
            if (kind == 0) {
                cursor = menu_step_party_slot(cursor, 1, 0);
            }
            break;
        }
    }
    menu_state_current->markers->shown[0] = menu_state_current->markers->shown[1] = menu_state_current->markers->shown[2] = 0;
    menu_arts_screen_set_dimmed(0);
    menu_state_current->flags->party_panels_shown = 0;
    menu_run_frame();
    if (menu_target_panels_allocated != 0) {
        for (i = 0; i < 3; i++) {
            heap_free(menu_state_current->party_panels[i]);
        }
        menu_target_panels_allocated = 0;
    }
    menu_panel_close(2);
}

/* The arts screen's windows are up: clear `windows`, zoom in when `zoom`
 * asks for it and drop the panel redraw flags. A statement macro. */
#define FINISH_OPENING(windows, zoom)                  \
    do {                                               \
        (windows) = 0;                                 \
        if (zoom) {                                    \
            menu_view_start_zoom_in();                           \
            menu_field_blocks_slide(0, 0);                       \
            menu_run_frame();                           \
        }                                              \
        menu_state_current->flags->field_menu2_shown = 0;      \
        menu_state_current->flags->panels_shown[1] = 0;        \
    } while (0)

/* 801DDF24: The arts screen of party slot `member` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list): a cursor over twelve rows (two columns,
 * one for kind 2), the selected art's description; confirm uses a usable
 * art, 9/10 switch party members, cancel leaves. `zoom` first zooms in. `slot` is
 * the member currently shown. */
void menu_arts_screen_run(u8 member, u8 zoom, u8 kind) {
    s32 cursor;
    s32 cursorShown;
    u8 slot;
    u8 slotShown;
    u8 windows;
    u8 running;

    running = 1;
    windows = 1;
    cursor = 0;
    cursorShown = 0xff;
    slot = member;
    slotShown = 0xff;
    menu_arts_screen_open(kind);
    menu_list_cursor_alloc(0);
    do {
        menu_run_frame();
        if (slot != slotShown) {
            menu_arts_screen_build_list(slot, kind);
            slotShown = slot;
            cursorShown = 0xff;
        }
        menu_list_cursor_place(cursor, 0, 2, 0);
        if (cursor != cursorShown) {
            menu_arts_screen_show_description(slot, cursor, kind);
            cursorShown = cursor;
        }
        if (windows) {
            menu_panel_open(6, 0x10, 0xa, menu_arts_screen_panel6_widths[kind], 0x70, 0, 1, 4, 0);
            menu_panel_open(5, 0xc, 0x86, 0xac, 0x38, 0, 1, 4, 0);
            menu_panel_open(4, menu_arts_screen_panel4_x_table[kind], 0xa6, menu_arts_screen_panel4_widths[kind], 0x18, 0, 1, 4, 0);
            menu_panel_open(3, 0xc8, 0x86, 0x50, 0x18, 0, 1, 4, 0);
            FINISH_OPENING(windows, zoom);
        }
        switch (menu_state_current->input) {
        case 4:
            if (menu_state_current->arts_list->shown[cursor] & 0x80) {
                menu_arts_screen_use_art(slot, cursor, kind);
                slotShown = 0xff;
                cursorShown = 0xff;
            }
            break;
        case 5:
            running = 0;
            break;
        case 0:
            if (kind != 2) {
                if (++cursor >= 12) {
                    cursor = 11;
                }
            }
            break;
        case 2:
            if (kind != 2) {
                if (--cursor < 0) {
                    cursor = 0;
                }
            }
            break;
        case 1:
            if (cursor + 2 < 12) {
                cursor += 2;
            }
            break;
        case 3:
            if (cursor - 2 >= 0) {
                cursor -= 2;
            }
            break;
        case 9:
            slot = menu_step_party_slot(slot, 0, kind);
            break;
        case 10:
            slot = menu_step_party_slot(slot, 1, kind);
            break;
        }
    } while (running);
    menu_label_clear_shown(8, menu_state_current->flags->labels10e0_shown);
    menu_list_cursor_free(0);
}

/* 801DE29C: Run the 801ddf24 screen for party slot `slot`; always continues the menu. */
u8 menu_arts_command_run(u8 slot, u8 zoom) {
    menu_arts_screen_run(slot, zoom, 0);
    return 1;
}

/* 801DE2C8: Open the 801ddf24 screen on page `page`: its block, labels and view 7. */
void menu_equip_screen_open(u8 page) {
    void *block;

    block = heap_alloc(0xa1c, 0);
    menu_state_current->equip_list = block;
    bzero(block, 0xa1c);
    menu_label_render_table(6, menu_state_current->labels14e0, menu_equip_screen_label_ids + page * 6, menu_state_current->flags->labels14e0_shown);
    menu_load_or_release_data_set(7);
    menu_markers_layout(3);
    menu_list_cursor_alloc(0);
    menu_member_marks_show(1, page);
}

/* 801DE36C: Close the 801ddf24 screen: panels 2-5, its labels and block (+434); view 17. */
void menu_equip_screen_close(void) {
    menu_state_current->flags->equip_list_shown = 0;
    menu_panel_close(2);
    menu_panel_close(3);
    menu_panel_close(4);
    menu_panel_close(5);
    menu_run_frame();
    menu_label_clear_shown(6, menu_state_current->flags->labels14e0_shown);
    menu_load_or_release_data_set(0x17);
    heap_free(menu_state_current->equip_list);
    menu_scroll_bar_hide();
}

/* 801DE400: Close the 801ddf24 screen: hide its sprites and free its blocks. */
void menu_equip_command_close(void) {
    menu_state_current->flags->equipment_shown = 0;
    menu_state_current->flags->equip_labels_shown = 0;
    menu_run_frame();
    heap_free(menu_state_current->equip_panel);
    heap_free(menu_state_current->equip_labels);
}

/* 801DE474: Lay out the page `page` labels of the 801ddf24 screen (rows 2-5 when
 * `wide`, else 0-1) and its window. */
void menu_equip_screen_layout_labels(u8 wide, u8 page) {
    s32 i;
    s32 first;
    s32 end;
    s32 h;

    for (i = 0; i < 6; i++) {
        menu_state_current->flags->labels14e0_shown[i] = 0;
    }
    if (wide) {
        first = 2;
        end = 6;
        h = 0x72;
    } else {
        first = 0;
        end = 2;
        h = 0x5a;
    }
    for (i = first; i < end; i++) {
        menu_label_place(6, menu_state_current->labels14e0, menu_equip_screen_label_ids + page * 6, menu_file_command_label_x_offsets, menu_state_current->flags->labels14e0_shown, i,
                      i, 3);
    }
    if (menu_state_current->flags->panels_shown[4] != 0) {
        menu_panel_close(4);
    }
    menu_panel_open(4, 0x10, 0xc, 0x80, h, 0, 1, 4, 0);
}

/* 801DE5CC: Build the equipment candidate list for part `part` of party slot `slot`
 * (`special` special parts, `gear` the gear's lists) and draw its rows from
 * `top`: usable weapons of a class below 5, special parts of the kept part's
 * class, or accessories whose groups are free or held by the replaced one
 * (entry 0 of the accessory list stays empty for removing). Returns the
 * scroll limit. */
s32 menu_equip_screen_build_candidates(u8 slot, s32 top, s32 part, u8 special, u8 gear) {
    RECT rect;
    u8 codes[4];
    u8 text[8];
    s32 length;
    u8 kind;
    s32 count;
    EquipInfo *weapon;
    AccessoryInfo *accessory;
    EquipInfo *keptWeapon;
    GearWeaponInfo *gearWeapon;
    GearWeaponInfo *keptGearWeapon;
    GearAccessoryInfo *gearAccessory;
    u16 own;
    u16 used;
    s32 i;
    u8 tens;
    u8 ok;
    u8 *image;

    codes[1] = 0;
    codes[3] = 0;
    if (special) {
        if (!gear) {
            kind = part + 1;
            count = 100;
            keptWeapon = &menu_state_current->tables->equipment[menu_state_current->equip_labels->parts[0][part]];
        } else {
            kind = part + 1;
            count = 100;
            keptGearWeapon = &menu_state_current->tables->gear_weapons[menu_state_current->equip_labels->parts[0][part]];
        }
    } else if (!gear) {
        if (part != 0) {
            kind = 5;
            count = 200;
            accessory = &menu_state_current->tables->accessories[menu_state_current->equip_labels->parts[2][part - 1]];
            own = accessory->groups;
            for (i = 0, used = 0; i < 3; i++) {
                accessory = &menu_state_current->tables->accessories[menu_state_current->equip_labels->parts[2][i]];
                used |= accessory->groups;
            }
        } else {
            kind = 0;
            count = 100;
        }
    } else if (part != 0) {
        kind = 5;
        count = 150;
        gearAccessory = &menu_state_current->tables->gear_accessories[menu_state_current->equip_labels->parts[2][part - 1]];
        own = gearAccessory->groups;
        for (i = 0, used = 0; i < 3; i++) {
            gearAccessory = &menu_state_current->tables->gear_accessories[menu_state_current->equip_labels->parts[2][i]];
            used |= gearAccessory->groups;
        }
    } else {
        kind = 0;
        count = 100;
    }
    for (i = 0; i < 200; i++) {
        menu_equip_screen_candidate_ids[i] = 0;
        menu_equip_screen_candidate_counts[i] = 0;
    }
    length = kind == 5;
    for (i = 0; i < count; i++) {
        if (!gear) {
            accessory = &menu_state_current->tables->accessories[game_data.accessoryIds[i]];
            weapon = &menu_state_current->tables->equipment[game_data.weaponIds[i]];
        } else {
            gearWeapon = &menu_state_current->tables->gear_weapons[game_data.gearPartIds[i]];
            gearAccessory = &menu_state_current->tables->gear_accessories[game_data.gearAccessoryIds[i]];
        }
        ok = 0;
        if (!gear) {
            switch (kind) {
            case 0:
                if (menu_test_bit(weapon->users, menu_state_current->flags->party[slot]) && weapon->kind < 5 &&
                    game_data.weaponIds[i] < 50) {
                    ok = 1;
                }
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                if (menu_test_bit(weapon->users, menu_state_current->flags->party[slot]) &&
                    weapon->kind == keptWeapon->kind && game_data.weaponIds[i] >= 50) {
                    ok = 1;
                }
                break;
            case 5:
                if (menu_test_bit(accessory->users, menu_state_current->flags->party[slot])) {
                    if (accessory->groups == 0 || (own & accessory->groups)) {
                        ok = 1;
                    } else if (!(used & accessory->groups)) {
                        ok = 1;
                    }
                }
                break;
            }
        } else {
            switch (kind) {
            case 0:
                if (menu_test_bit32(gearWeapon->users, game_data.characters[menu_state_current->flags->party[slot]].gearId) &&
                    gearWeapon->kind < 5 && game_data.gearPartIds[i] < 50) {
                    ok = 1;
                }
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                if (menu_test_bit32(gearWeapon->users, game_data.characters[menu_state_current->flags->party[slot]].gearId) &&
                    gearWeapon->kind == keptGearWeapon->kind && game_data.gearPartIds[i] >= 50) {
                    ok = 1;
                }
                break;
            case 5:
                if (menu_test_bit32(gearAccessory->users, game_data.characters[menu_state_current->flags->party[slot]].gearId)) {
                    if (gearAccessory->groups == 0 || (own & gearAccessory->groups)) {
                        ok = 1;
                    } else if (!(used & gearAccessory->groups)) {
                        ok = 1;
                    }
                }
                break;
            }
        }
        if (ok) {
            if (!gear) {
                if (kind != 5) {
                    menu_equip_screen_candidate_ids[length] = game_data.weaponIds[i];
                    menu_equip_screen_candidate_counts[length] = game_data.weaponCounts[i];
                } else {
                    menu_equip_screen_candidate_ids[length] = game_data.accessoryIds[i];
                    menu_equip_screen_candidate_counts[length] = game_data.accessoryCounts[i];
                }
            } else if (kind != 5) {
                menu_equip_screen_candidate_ids[length] = game_data.gearPartIds[i];
                menu_equip_screen_candidate_counts[length] = game_data.gearPartCounts[i];
            } else {
                menu_equip_screen_candidate_ids[length] = game_data.gearAccessoryIds[i];
                menu_equip_screen_candidate_counts[length] = game_data.gearAccessoryCounts[i];
            }
            length++;
        }
    }
    image = heap_alloc(0x3f6, 0);
    for (i = 0; i < 8; i++) {
        if (menu_equip_screen_candidate_ids[top + i] != 0) {
            if (!gear) {
                if (kind != 5) {
                    menu_state_current->equip_list->names[i].width = window_render_text_line(text_get_weapon_name(menu_equip_screen_candidate_ids[top + i]), image, 0x24, 0);
                } else {
                    menu_state_current->equip_list->names[i].width = window_render_text_line(text_get_accessory_name(menu_equip_screen_candidate_ids[top + i]), image, 0x24, 0);
                }
            } else if (kind != 5) {
                menu_state_current->equip_list->names[i].width = window_render_text_line(text_get_gear_part_name(menu_equip_screen_candidate_ids[top + i]), image, 0x24, 0);
            } else {
                menu_state_current->equip_list->names[i].width = window_render_text_line(text_get_gear_accessory_name(menu_equip_screen_candidate_ids[top + i]), image, 0x24, 0);
            }
            tens = menu_equip_screen_candidate_counts[top + i] / 10;
            codes[0] = tens != 0 ? tens + 0x10 : 0xc3;
            codes[2] = menu_equip_screen_candidate_counts[top + i] % 10 + 0x10;
            text_decode_codes(codes, text, 2);
            menu_state_current->equip_list->values[i].width = window_render_text_line(text, image, 0x24, 1);
            rect.x = (i & 1) * 0x18 + 0x180;
            rect.y = i / 2 * 0xd + 0x80;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)image);
            DrawSync(0);
            menu_label_init_quads(&menu_state_current->equip_list->names[i], i, 0x80, 0x81);
            menu_label_init_quads(&menu_state_current->equip_list->values[i], i, 0x80, 0x82);
            menu_set_rect_verts(menu_state_current->equip_list->names[i].verts, 0xa8, i * 0xd + 0x12,
                          menu_state_current->equip_list->names[i].width, 0xd);
            menu_set_rect_verts(menu_state_current->equip_list->values[i].verts, 0x10c, i * 0xd + 0x12,
                          menu_state_current->equip_list->values[i].width, 0xd);
            menu_state_current->equip_list->names[i].buffer = menu_state_current->buffer_index;
            menu_state_current->equip_list->values[i].buffer = menu_state_current->buffer_index;
            menu_state_current->equip_list->shown[i] = 1;
        } else {
            menu_state_current->equip_list->shown[i] = 0;
        }
    }
    heap_free(image);
    menu_name_label_layout(&menu_state_current->equip_list->title, slot, gear, 0);
    length -= 8;
    menu_state_current->flags->equip_list_shown = 1;
    if (length < 0) {
        length = 0;
    }
    return length;
}

/* 801DF0D4: Commit the equipment change of part `part` of party slot `slot` (with
 * `special` a special part, with `gear` the gear's): the newly equipped part
 * leaves its inventory list and the replaced one kept by 801df5d0 joins it.
 * A newly equipped special part's id gets 100 rounds of ammo (ammo or
 * gearAmmo, resident/gamedata.h); when it had fewer, the replaced part is
 * dropped instead. Without a new part the kept one goes back. Returns 1 when
 * character 4 changed weapon. */
s32 menu_equip_screen_commit_part(u8 slot, u8 part, u8 special, u8 gear) {
    u8 *ids;
    u8 *counts;
    u8 *at;
    s32 length;
    s32 i;
    s32 result;
    u8 swap;
    u8 add;
    u8 selected;
    u8 kept;
    u8 id;

    add = 1;
    swap = 1;
    result = 0;
    if (!gear) {
        ids = game_data.weaponIds;
        counts = ids - 100;
    } else {
        ids = game_data.gearPartIds;
        counts = ids - 100;
    }
    length = 100;
    if (special) {
        if (!gear) {
            kept = menu_state_current->equip_labels->parts[1][part];
            at = &game_data.characters[menu_state_current->flags->party[slot]].entryItems[part];
            selected = *at;
            if (selected == 0) {
                *at = kept;
                swap = 0;
            } else {
                if (game_data.ammo[*at - 50] < 100) {
                    kept = 0;
                }
                game_data.ammo[*at - 50] = 100;
            }
        } else {
            kept = menu_state_current->equip_labels->parts[1][part];
            at = &game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[part];
            selected = *at;
            if (selected == 0) {
                *at = kept;
                swap = 0;
            } else {
                if (game_data.gearAmmo[*at - 50] < 100) {
                    kept = 0;
                }
                game_data.gearAmmo[*at - 50] = 100;
            }
        }
    } else if (!gear) {
        if (part == 0) {
            id = menu_state_current->flags->party[slot];
            selected = game_data.characters[id].weapons[0];
            kept = menu_state_current->equip_labels->parts[0][0];
            if (selected == 0) {
                game_data.characters[id].weapons[0] = kept;
                swap = 0;
            } else if (id == 4) {
                result = 1;
            }
        } else {
            selected = game_data.characters[menu_state_current->flags->party[slot]].accessories[part - 1];
            ids = game_data.accessoryIds;
            counts = game_data.accessoryCounts;
            kept = menu_state_current->equip_labels->parts[2][part - 1];
            length = 200;
        }
    } else {
        if (part == 0) {
            id = menu_state_current->flags->party[slot];
            selected = game_data.gears[game_data.characters[id].gearId].weapons[0];
            kept = menu_state_current->equip_labels->parts[0][0];
            if (selected == 0) {
                game_data.gears[game_data.characters[id].gearId].weapons[0] = kept;
                swap = 0;
            } else if (id == 4) {
                result = 1;
            }
        } else {
            selected = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].parts[part - 1];
            ids = game_data.gearAccessoryIds;
            counts = game_data.gearAccessoryCounts;
            length = 150;
            kept = menu_state_current->equip_labels->parts[2][part - 1];
        }
    }
    if (swap) {
        if (selected != 0) {
            for (i = 0; i < length; i++) {
                if (ids[i] == selected) {
                    counts[i]--;
                    break;
                }
            }
        }
        if (kept != 0) {
            for (i = 0; i < length; i++) {
                if (ids[i] == kept) {
                    add = 0;
                    counts[i]++;
                    break;
                }
            }
            if (add) {
                for (i = 0; i < length; i++) {
                    if (ids[i] == 0) {
                        ids[i] = kept;
                        counts[i] = 1;
                        break;
                    }
                }
            }
        }
        for (i = 0; i < length; i++) {
            if (counts[i] == 0) {
                ids[i] = 0;
            } else if (counts[i] >= 100) {
                counts[i] = 99;
            }
        }
    }
    return result;
}

/* 801DF5D0: Keep the shown stats and the equipment of party slot `slot` (its gear's
 * parts when `gear`) in the equipment screen's block. */
void menu_equip_screen_keep_parts(u8 slot, u8 gear) {
    s32 i;

    for (i = 0; i < 9; i++) {
        menu_state_current->equip_labels->stats[i] = menu_state_current->tables->stats[i];
    }
    if (!gear) {
        for (i = 0; i < 5; i++) {
            menu_state_current->equip_labels->parts[0][i] = game_data.characters[menu_state_current->flags->party[slot]].weapons[i];
            menu_state_current->equip_labels->parts[1][i] = game_data.characters[menu_state_current->flags->party[slot]].entryItems[i];
            menu_state_current->equip_labels->parts[2][i] = game_data.characters[menu_state_current->flags->party[slot]].accessories[i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            menu_state_current->equip_labels->parts[0][i] = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].weapons[i];
            menu_state_current->equip_labels->parts[1][i] = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[i];
            menu_state_current->equip_labels->parts[2][i] = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].parts[i];
        }
    }
}

/* 801DF890: Put back the equipment of party slot `slot` (its gear's parts when `gear`)
 * kept by 801df5d0; the equipment screen was cancelled. */
void menu_equip_screen_restore_parts(u8 slot, u8 gear) {
    s32 i;

    if (!gear) {
        for (i = 0; i < 5; i++) {
            game_data.characters[menu_state_current->flags->party[slot]].weapons[i] = menu_state_current->equip_labels->parts[0][i];
            game_data.characters[menu_state_current->flags->party[slot]].entryItems[i] = menu_state_current->equip_labels->parts[1][i];
        }
        for (i = 0; i < 3; i++) {
            game_data.characters[menu_state_current->flags->party[slot]].accessories[i] = menu_state_current->equip_labels->parts[2][i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].weapons[i] = menu_state_current->equip_labels->parts[0][i];
            game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[i] = menu_state_current->equip_labels->parts[1][i];
        }
        for (i = 0; i < 3; i++) {
            game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].parts[i] = menu_state_current->equip_labels->parts[2][i];
        }
    }
}

/* 801DFB68: Try list entry `top` + `row` on part `part` of party slot `slot`: the
 * weapon (0) or an accessory (1-3), with `special` a special part; with
 * `gear` the gear's parts. */
void menu_equip_screen_try_candidate(u8 slot, s32 part, s32 row, s32 top, u8 special, u8 gear) {
    if (!gear) {
        if (!special) {
            switch (part) {
            case 0:
                game_data.characters[menu_state_current->flags->party[slot]].weapons[0] = menu_equip_screen_candidate_ids[top + row];
                break;
            case 1:
            case 2:
            case 3:
                game_data.characters[menu_state_current->flags->party[slot]].accessories[part - 1] = menu_equip_screen_candidate_ids[top + row];
                break;
            }
        } else {
            game_data.characters[menu_state_current->flags->party[slot]].entryItems[part] = menu_equip_screen_candidate_ids[top + row];
        }
    } else {
        if (!special) {
            switch (part) {
            case 0:
                game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].weapons[0] = menu_equip_screen_candidate_ids[top + row];
                break;
            case 1:
            case 2:
            case 3:
                game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].parts[part - 1] = menu_equip_screen_candidate_ids[top + row];
                break;
            }
        } else {
            game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[part] = menu_equip_screen_candidate_ids[top + row];
        }
    }
}

/* 801DFE2C: Compute party slot `slot`'s gear stats and copy them to the shown values. */
void menu_compute_slot_gear_stats(u8 slot) {
    menu_compute_gear_equipment(menu_state_current->tables, game_data.characters[menu_state_current->flags->party[slot]].gearId);
    menu_compute_gear_summary(menu_state_current->tables, game_data.characters[menu_state_current->flags->party[slot]].gearId);
    menu_state_current->tables->stats[0] = menu_state_current->tables->gear.attack;
    menu_state_current->tables->stats[1] = menu_state_current->tables->gear.defense;
    menu_state_current->tables->stats[2] = menu_state_current->tables->gear.ether_defense;
    menu_state_current->tables->stats[3] = menu_state_current->tables->gear.hit;
    menu_state_current->tables->stats[4] = menu_state_current->tables->gear.speed;
    menu_state_current->tables->stats[5] = menu_state_current->tables->gear.frame_factor;
}

/* 801DFF5C: Show the three-line description of equipment list entry `top` + `row` (or,
 * with `current`, of the part equipped) for part `part` of party slot
 * `slot` (`special` a special part, `gear` the gear's parts). */
void menu_equip_screen_show_description(s32 part, s32 row, s32 top, u8 special, u8 gear, u8 current, u8 slot) {
    RECT rect;
    u8 *table;
    u8 *image;
    s32 line;
    u8 kind;
    u16 id;

    id = menu_equip_screen_candidate_ids[top + row];
    kind = 0;
    if (current) {
        id = 0xff;
    }
    if (id != 0) {
        if (!special && part != 0) {
            kind = 1;
        }
        kind += gear * 2;
        switch (kind) {
        case 0:
            table = menu_state_current->equip_list->texts[0];
            if (current) {
                if (!special) {
                    id = game_data.characters[menu_state_current->flags->party[slot]].weapons[0];
                } else {
                    id = game_data.characters[menu_state_current->flags->party[slot]].entryItems[part];
                }
            }
            break;
        case 1:
            table = menu_state_current->equip_list->texts[1];
            if (current) {
                id = game_data.characters[menu_state_current->flags->party[slot]].accessories[part - 1];
            }
            break;
        case 2:
            table = menu_state_current->equip_list->texts[2];
            if (current) {
                if (!special) {
                    id = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].weapons[0];
                } else {
                    id = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[part];
                }
            }
            break;
        case 3:
            table = menu_state_current->equip_list->texts[3];
            if (current) {
                id = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].parts[part - 1];
            }
            break;
        }
        if (id != 0) {
            image = heap_alloc(0x3f6, 0);
            bzero(image, 0x3f6);
            for (line = 0; line < 3; line++) {
                menu_state_current->equip_list->extra[line].width =
                    window_render_text_line(text_get_resource_entry(table, id * 3 + line), image, 0x24, 0);
                rect.x = ((line + 8) & 1) * 0x18 + 0x180;
                rect.y = (line + 8) / 2 * 0xd + 0x80;
                rect.w = 0x28;
                rect.h = 0xd;
                LoadImage(&rect, (u_long *)image);
                DrawSync(0);
                menu_label_init_quads(&menu_state_current->equip_list->extra[line], line + 8, 0x80, 0x81);
                menu_set_rect_verts(menu_state_current->equip_list->extra[line].verts, 0x10, (u16)(line * 0x10 + 0x96) / 2 * 2,
                              menu_state_current->equip_list->extra[line].width, 0xd);
                menu_state_current->equip_list->extra[line].buffer = menu_state_current->buffer_index;
            }
            menu_state_current->equip_list->extraShown = 1;
            heap_free(image);
            return;
        }
    }
    menu_state_current->equip_list->extraShown = 0;
}

/* 801E0434: Return party slot `slot`'s accessory (or with `gear` its gear's part) to
 * its inventory list: add one to the entry holding it (at most 99), or put
 * it into the first free entry. */
void menu_equip_screen_return_special_part(u8 slot, u8 gear) {
    u8 *ids;
    u8 *counts;
    u8 item;
    u8 size;
    u8 fresh;
    s32 i;

    fresh = 1;
    if (!gear) {
        ids = game_data.weaponIds;
        counts = ids - 100;
        item = game_data.characters[menu_state_current->flags->party[slot]].entryItems[0];
        game_data.characters[menu_state_current->flags->party[slot]].entryItems[0] = 0;
        size = 100;
    } else {
        ids = game_data.gearPartIds;
        counts = ids - 100;
        item = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[0];
        size = 100;
        game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems[0] = 0;
    }
    for (i = 0; i < size; i++) {
        if (ids[i] == item) {
            if (++counts[i] >= 100) {
                counts[i] = 99;
            }
            fresh = 0;
        }
    }
    if (fresh) {
        for (i = 0; i < size; i++) {
            if (ids[i] == 0) {
                ids[i] = item;
                counts[i] = 1;
                return;
            }
        }
    }
}

/* Keep the newly equipped parts (801df5d0) and leave the candidate list:
 * hide its cursor, show the part marker again and ask for the list reset.
 * A statement macro. */
#define KEEP_AND_LEAVE_LIST()                          \
    do {                                               \
        menu_equip_screen_keep_parts(slot, gear);                     \
        cursor = 0;                                    \
        menu_state_current->markers->shown[0] = 1;             \
        reset = 1;                                     \
        menu_list_cursor_place(0, top, 3, 0);                   \
    } while (0)

/* 801E05D0: The equipment screen of party slot `slot` (`gear`: the gear's parts)
 * until it is left. In part mode the cursor steps over the four parts (a
 * character 4 toggles its special parts, 9/10 switch the party slot) and
 * confirm opens the candidate list; in list mode the cursor scrolls the
 * candidates, confirm equips one (801df0d4) and cancel restores the kept
 * parts (801df890). Each frame redraws what changed; the first opens the
 * windows and, with `fade`, waits for the view to settle. */
void menu_equip_screen_run(u8 slot, u8 fade, u8 gear) {
    u8 running;
    u8 panel;
    u8 special;
    u8 drawnSlot;
    u8 first;
    u8 previewed;
    u8 cursor;
    u8 committed;
    u8 listing;
    u8 reset;
    s32 part;
    s32 row;
    s32 drawnPart;
    s32 drawnRow;
    s32 drawnTop;
    s32 top;
    s32 limit;
    s32 y;
    s32 h;
    u8 swapped;

    running = 1;
    first = 1;
    panel = 1;
    special = 0;
    drawnSlot = 0xff;
    part = 0;
    drawnPart = 0xff;
    row = 0;
    drawnRow = 0xff;
    previewed = 0;
    cursor = 0;
    committed = 0;
    listing = 0;
    drawnTop = 0xff;
    top = 0;
    menu_state_current->flags->detail_shown = 0;
    menu_equip_screen_open(gear);
    menu_equip_screen_keep_parts(slot, gear);
    while (running) {
        menu_run_frame();
        if (panel || slot != drawnSlot) {
            menu_equip_labels_layout_parts(slot, special + 1, listing, gear);
            menu_equip_screen_layout_labels(special, gear);
            panel = 0;
        }
        if (top != drawnTop || slot != drawnSlot) {
            limit = menu_equip_screen_build_candidates(slot, top, part, special, gear);
            if (limit != 0) {
                y = top * 100 / limit;
                y /= 2;
                h = 0x32;
            } else {
                y = 0;
                h = 0x64;
            }
            menu_scroll_bar_show(0x94, y + 0x12, h);
        }
        if (cursor) {
            menu_list_cursor_place(row, top, 3, 0);
        } else {
            menu_state_current->flags->cursors_shown[0] = 0;
        }
        if (row != drawnRow || slot != drawnSlot || top != drawnTop) {
            if (previewed) {
                menu_equip_screen_try_candidate(slot, part, row, top, special, gear);
            }
            previewed = 1;
            if (!gear) {
                menu_compute_character_equipment(menu_state_current->tables, menu_state_current->flags->party[slot]);
                menu_compute_character_stats(menu_state_current->tables, menu_state_current->flags->party[slot]);
            } else {
                menu_compute_slot_gear_stats(slot);
            }
            menu_equip_screen_show_description(part, row, top, special, gear, 0, slot);
            menu_equip_panel_build(slot, 1, listing, gear);
            drawnRow = row;
            drawnTop = top;
        }
        if (part != drawnPart || slot != drawnSlot) {
            menu_equip_screen_show_description(part, row, top, special, gear, 1, slot);
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->x0 = 0x8c;
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->y0 = menu_equip_screen_part_cursor_y_table[special * 4 + part];
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->x1 = 0x9c;
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->y1 = menu_equip_screen_part_cursor_y_table[special * 4 + part];
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->x2 = 0x8c;
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->y2 = menu_equip_screen_part_cursor_y_table[special * 4 + part] + 0x10;
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->x3 = 0x9c;
            drawnPart = part;
            drawnSlot = slot;
            (menu_state_current->markers->polys + menu_state_current->markers->buffer[0])->y3 = menu_equip_screen_part_cursor_y_table[special * 4 + part] + 0x10;
        }
        if (first) {
            menu_panel_open(2, 0x94, 0xa, 0x94, 0x74, 0, 1, 4, 1);
            menu_panel_open(3, 0x6c, 0x87, 0xc4, 0x48, 0, 1, 4, 0);
            menu_panel_open(5, 8, 0x8e, 0x60, 0x40, 0, 1, 4, 0);
            if (fade) {
                menu_view_start_zoom_in();
                menu_field_blocks_slide(0, 0);
                while (menu_state_current->view_motion != 0) {
                    menu_run_frame();
                }
            }
            menu_state_current->markers->shown[0] = 1;
            menu_state_current->flags->field_menu2_shown = 0;
            first = 0;
            menu_state_current->flags->panels_shown[1] = 0;
        }
        menu_state_current->flags->markers_shown = 1;
        if (committed) {
            committed = 0;
            menu_state_current->input = 4;
        }
        if (!listing) {
            switch (menu_state_current->input) {
            case 5:
                running = 0;
                break;
            case 4:
                menu_equip_screen_keep_parts(slot, gear);
                listing = 1;
                top = 0;
                drawnTop = 0xff;
                drawnRow = 0xff;
                menu_state_current->markers->shown[0] = 0;
                panel = 1;
                cursor = 1;
                row = 0;
                menu_state_current->flags->cursors_shown[0] = 1;
                break;
            case 0:
            case 2:
                if (menu_state_current->flags->party[slot] == 4) {
                    special ^= 1;
                    drawnPart = 0xff;
                    panel = 1;
                    part = 0;
                }
                break;
            case 1:
                if (++part >= 4) {
                    part = 0;
                }
                if (special && gear && (part == 1 || part == 2)) {
                    part = 3;
                }
                break;
            case 3:
                if (--part < 0) {
                    part = 3;
                }
                if (special && gear && (part == 1 || part == 2)) {
                    part = 0;
                }
                break;
            case 9:
                if (menu_step_party_slot(slot, 0, gear) != slot) {
                    slot = menu_step_party_slot(slot, 0, gear);
                    special = 0;
                    previewed = 0;
                }
                break;
            case 10:
                if (menu_step_party_slot(slot, 1, gear) != slot) {
                    slot = menu_step_party_slot(slot, 1, gear);
                    special = 0;
                    previewed = 0;
                }
                break;
            }
        } else {
            reset = 0;
            switch (menu_state_current->input) {
            case 5:
                menu_equip_screen_restore_parts(slot, gear);
                cursor = 0;
                menu_state_current->markers->shown[0] = 1;
                reset = 1;
                menu_list_cursor_place(0, top, 3, 0);
                row = 0;
                drawnSlot = 0xff;
                menu_state_current->flags->cursors_shown[0] = 0;
                break;
            case 4:
                swapped = menu_equip_screen_commit_part(slot, part, special, gear);
                drawnSlot = 0xff;
                if (swapped) {
                    special ^= 1;
                    panel = 1;
                    committed = 1;
                    menu_equip_screen_return_special_part(slot, gear);
                }
                KEEP_AND_LEAVE_LIST();
                menu_state_current->flags->cursors_shown[0] = 0;
                row = 0;
                break;
            case 1:
                if (++row >= 8) {
                    row = 7;
                    top++;
                    if (limit < top) {
                        top = limit;
                    }
                }
                break;
            case 3:
                if (--row < 0) {
                    top--;
                    row = 0;
                    if (top < 0) {
                        top = 0;
                    }
                }
                break;
            }
            if (reset) {
                row = 0;
                top = 0;
                listing = 0;
                drawnRow = 0xff;
                panel = 1;
                previewed = 0;
                menu_state_current->equip_panel->highlighted = 0;
            }
        }
    }
    menu_markers_hide();
    menu_list_cursor_free(0);
}

/* 801E0F78: Run the 801e05d0 screen for party slot `slot` with its blocks; views 3 and 13. */
u8 menu_equip_command_run(u8 slot, u8 zoom) {
    void *block;

    block = heap_alloc(0x32f4, 0);
    menu_state_current->equip_panel = block;
    bzero(block, 0x32f4);
    block = heap_alloc(0x2ac, 0);
    menu_state_current->equip_labels = block;
    bzero(block, 0x2ac);
    menu_load_or_release_data_set(3);
    menu_equip_screen_run(slot, zoom, 0);
    menu_load_or_release_data_set(0x13);
    return 1;
}

/* 801E1014: Open the 801e1544 screen: its block (+438), labels and window, the party
 * panel when two or more members can take part, and the green gauges. */
void menu_deathblow_screen_open(void) {
    MenuStatusList *block;
    s32 i;
    s32 j;

    block = heap_alloc(0x25c0, 0);
    menu_state_current->status_list = block;
    bzero((u_char *)block, 0x25c0);
    menu_load_or_release_data_set(4);
    menu_label_render_table(2, menu_state_current->labels17e0, menu_deathblow_screen_label_ids, &menu_state_current->flags->label17e0_shown);
    menu_label_place(2, menu_state_current->labels17e0, menu_deathblow_screen_label_ids, menu_file_command_label_x_offsets, &menu_state_current->flags->label17e0_shown, 0, 0, 4);
    menu_panel_open(2, 0x44, 0xa, 0xe4, 0xc4, 0, 1, 4, 0);
    menu_state_current->flags->field_menu2_shown = 0;
    i = 0;
    j = 0; /* members that can take part */
    menu_state_current->flags->panels_shown[1] = 0;
    for (; i < 3; i++) {
        if (menu_state_current->flags->party[i] != 0xff) {
            if (menu_state_current->flags->party[i] != 7 && menu_state_current->flags->party[i] != 8) {
                j++;
            }
        }
    }
    if (j >= 2) {
        menu_member_marks_show(3, 0);
    } else {
        menu_state_current->flags->marks_shown = 0;
    }
    for (i = 0; i < 13; i++) {
        j = 0;
        do {
            SetPolyG4(&menu_state_current->status_list->gauges[i][j]);
            (menu_state_current->status_list->gauges[i] + j)->r0 = 0;
            (menu_state_current->status_list->gauges[i] + j)->g0 = 0xff;
            (menu_state_current->status_list->gauges[i] + j)->b0 = 0;
            (menu_state_current->status_list->gauges[i] + j)->r1 = 0;
            (menu_state_current->status_list->gauges[i] + j)->g1 = 0xff;
            (menu_state_current->status_list->gauges[i] + j)->b1 = 0;
            (menu_state_current->status_list->gauges[i] + j)->r2 = 0;
            (menu_state_current->status_list->gauges[i] + j)->g2 = 0;
            (menu_state_current->status_list->gauges[i] + j)->b2 = 0;
            (menu_state_current->status_list->gauges[i] + j)->r3 = 0;
            (menu_state_current->status_list->gauges[i] + j)->g3 = 0;
            (menu_state_current->status_list->gauges[i] + j)->b3 = 0;
            j++;
        } while (j < 2);
    }
}

/* 801E1398: Close the 801e1544 screen: its sprites, labels and block (+438); view 14. */
void menu_deathblow_screen_close(void) {
    menu_member_marks_hide();
    menu_state_current->flags->status_list_shown = 0;
    menu_panel_close(2);
    menu_run_frame();
    menu_label_clear_shown(2, &menu_state_current->flags->label17e0_shown);
    menu_load_or_release_data_set(0x14);
    heap_free(menu_state_current->status_list);
}

/* Rate target `i` of the row: a nonzero target counts towards the average,
 * and a nonzero value adds its percentage of a target other than ffff (at
 * most 100); then step to the next value. A statement macro. */
#define RATE_TARGET()                                                   \
    do {                                                                \
        value = *values;                                                \
        off = i * 2;                                                    \
        line = (s32)table + id * 0x110;                                 \
        target = *(u16 *)(off + (r * 14 + line));                       \
        if (value != 0) {                                               \
            if (target != 0) {                                          \
                if (target != 0xffff) {                                 \
                    percent = value * 100 / target;                     \
                    if (percent >= 100) {                               \
                        sum += 100;                                     \
                    } else {                                            \
                        sum += percent;                                 \
                    }                                                   \
                }                                                       \
                count++;                                                \
            }                                                           \
        } else if (target != 0) {                                       \
            count++;                                                    \
        }                                                               \
        i++;                                                            \
        values++;                                                       \
    } while (0)

/* 801E1418: Return party slot `slot`'s average completion (percent, each capped at
 * 100) of the seven targets of row `row` of its table (+438 +2578), counting
 * nonzero target entries (ffff contributes zero); rows from 7 need
 * game flag 4000. */
/* The target loop is a goto loop (the original recomputes each target
 * address); the u16 copy of `row` zero-extends it once, in place. */
u32 menu_compute_deathblow_progress(u8 slot, u8 row) {
    u32 sum;
    s32 off;
    s32 line;
    u16 r;
    s32 i;
    u32 count;
    s32 id;
    u16 *values;
    u8 *table;
    u32 value;
    u32 target;
    u32 percent;

    sum = 0;
    if (row < 7 || (game_data.flags & 0x4000)) {
        i = 0;
        count = 0;
        r = row;
        id = menu_state_current->flags->party[slot];
        table = menu_state_current->status_list->unk2578;
        values = game_data.characters[id].useCounts;
    loop:
        RATE_TARGET();
        if (i < 7) {
            goto loop;
        }
        if (count != 0) {
            return sum / count;
        }
    }
    return 0;
}

/* Lay out row `row` of the 801e1544 screen: mode 1 its five sheet images
 * (menu_deathblow_row_images); mode 2 its completion `percent` as three digits into
 * `pixels` (uploaded to the row's label image) and its green gauge
 * (54 pixels at 100%). */
void menu_deathblow_screen_layout_row(u8 row, u8 mode, u8 *pixels, u32 percent) {
    u8 text[6];
    u8 glyphs[8];
    RECT rect;
    s32 i;
    u32 tens;

    text[5] = 0;
    text[3] = 0;
    text[1] = 0;
    switch (mode) {
    case 1:
        for (i = 0; i < 5; i++) {
            if (menu_deathblow_row_images[row * 5 + i] != 0xff) {
                menu_state_current->status_list->counts[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, menu_deathblow_row_images[row * 5 + i],
                                  &menu_state_current->status_list->lists[row][menu_state_current->status_list->counts[row] * 2],
                                  menu_state_current->buffer_index, 0xd4 + i * 16, row * 13 + 0x1f, 0x1000);
            }
        }
        menu_state_current->status_list->starts[row] = menu_state_current->buffer_index;
        break;
    case 2:
        if (percent / 100) {
            text[0] = 0x11;
            percent -= 100;
        } else {
            text[0] = 0xc3;
        }
        tens = percent / 10;
        if (tens != 0 || text[0] != 0) {
            text[2] = tens + 0x10;
            text[4] = percent - tens * 10 + 0x10;
        } else {
            text[2] = 0xc3;
            text[4] = 0xc3;
        }
        text_decode_codes(text, glyphs, 3);
        menu_state_current->status_list->values[row].width = window_render_text_line(glyphs, pixels, 0x24, 1);
        rect.x = (row & 1) * 24 + 0x180;
        rect.y = (row >> 1) * 13 + 0x80;
        rect.w = 0x28;
        rect.h = 0xd;
        LoadImage(&rect, (u_long *)pixels);
        DrawSync(0);
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->x0 = 0x10a;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->y0 = row * 13 + 0x1f;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->x1 =
            menu_state_current->status_list->values[row].width + 0x10a;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->y1 = row * 13 + 0x1f;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->x2 = 0x10a;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->y2 = row * 13 + 0x2c;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->x3 =
            menu_state_current->status_list->values[row].width + 0x10a;
        (menu_state_current->status_list->values[row].polys + menu_state_current->buffer_index)->y3 = row * 13 + 0x2c;
        percent = percent * 5400 / 10000;
        menu_label_init_quads(&menu_state_current->status_list->values[row], row, 0x80, 0x82);
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->x0 = 0xd4;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->y0 = row * 13 + 0x23;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->x1 = percent + 0xd4;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->y1 = row * 13 + 0x23;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->x2 = 0xd4;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->y2 = row * 13 + 0x2b;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->x3 = percent + 0xd4;
        (menu_state_current->status_list->gauges[row] + menu_state_current->buffer_index)->y3 = row * 13 + 0x2b;
        menu_state_current->status_list->gaugeBuffer[row] = menu_state_current->buffer_index;
        menu_state_current->status_list->gaugeShown[row] = 1;
        break;
    }
}

/* 801E1AC8: Lay out the 801e1544 screen's thirteen rows for party slot `slot`: rows
 * the character has (bit of its game_data.skills record) show their name and
 * level digit (rows 0-6); others show only at 50% progress or more, with
 * the progress gauge; then the title. Owned rows pass `percent` unset, as
 * the original does. */
void menu_deathblow_screen_build(u8 slot) {
    u8 *pixels;
    RECT rect;
    u8 text[2];
    u8 glyphs[8];
    s32 i;
    u8 kind;
    u8 learned;
    u8 show;
    u32 percent;

    text[1] = 0;
    pixels = heap_alloc(0x3f6, 0);
    for (i = 0; i < 13; i++) {
        menu_state_current->status_list->gaugeShown[i] = 0;
        if (menu_test_bit_msb_first(game_data.skills[menu_state_current->flags->party[slot]].counterSkills, i)) {
            show = 1;
            kind = 1;
            learned = 1;
        } else {
            percent = menu_compute_deathblow_progress(slot, i);
            if (percent >= 50) {
                show = 1;
                kind = 2;
            } else {
                show = 0;
                kind = 0;
            }
            learned = 0;
        }
        if (show) {
            menu_state_current->status_list->names[i].width =
                window_render_text_line(text_get_system_resource_entry(menu_state_current->flags->party[slot], i), pixels, 0x24, 0);
            if (i < 7 && learned) {
                text[0] = menu_state_current->tables->arts[menu_state_current->flags->party[slot]][i + 7].unk17 + 0x10;
            } else {
                text[0] = 0xf;
            }
            text_decode_codes(text, glyphs, 1);
            menu_state_current->status_list->values[i].width = window_render_text_line(glyphs, pixels, 0x24, 1);
            rect.x = (i & 1) * 24 + 0x180;
            rect.y = i / 2 * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)pixels);
            DrawSync(0);
            menu_label_init_quads(&menu_state_current->status_list->names[i], i, 0x80, 0x81);
            menu_label_init_quads(&menu_state_current->status_list->values[i], i, 0x80, 0x82);
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->x0 = 0x5c;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->y0 = i * 13 + 0x1f;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->x1 =
                menu_state_current->status_list->names[i].width + 0x5c;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->y1 = i * 13 + 0x1f;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->x2 = 0x5c;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->y2 = i * 13 + 0x2c;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->x3 =
                menu_state_current->status_list->names[i].width + 0x5c;
            (menu_state_current->status_list->names[i].polys + menu_state_current->buffer_index)->y3 = i * 13 + 0x2c;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->x0 = 0xc8;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->y0 = i * 13 + 0x1f;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->x1 =
                menu_state_current->status_list->values[i].width + 0xc8;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->y1 = i * 13 + 0x1f;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->x2 = 0xc8;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->y2 = i * 13 + 0x2c;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->x3 =
                menu_state_current->status_list->values[i].width + 0xc8;
            (menu_state_current->status_list->values[i].polys + menu_state_current->buffer_index)->y3 = i * 13 + 0x2c;
            menu_state_current->status_list->names[i].buffer = menu_state_current->buffer_index;
            menu_state_current->status_list->shown[i] = 1;
        } else {
            menu_state_current->status_list->shown[i] = 0;
        }
        menu_state_current->status_list->counts[i] = 0;
        menu_deathblow_screen_layout_row(i, kind, pixels, percent);
    }
    heap_free(pixels);
    menu_name_label_layout(&menu_state_current->status_list->title, slot, 0, 2);
    menu_state_current->flags->status_list_shown = 1;
}

/* 801E20C8: The 801e1544 screen for party slot `slot`: members 7 and 8 are refused
 * (sound 4); otherwise show the slot's page, switching members (9 previous,
 * 10 next, skipping 7 and 8) until cancelled. */
u8 menu_deathblow_screen_run(u8 slot) {
    u8 stay;
    u8 shown;

    stay = 1;
    shown = 0xff;
    if ((u32)(menu_state_current->flags->party[slot] - 7) < 2) {
        menu_play_sound(4);
        return 1;
    }
    menu_deathblow_screen_open();
    do {
        menu_run_frame();
        if (slot != shown) {
            shown = slot;
            menu_deathblow_screen_build(slot);
        }
        switch (menu_state_current->input) {
        case 5:
            stay = 0;
            break;
        case 9:
            do {
                slot = menu_step_party_slot(slot, 0, 0);
            } while ((u32)(menu_state_current->flags->party[slot] - 7) < 2);
            break;
        case 10:
            do {
                slot = menu_step_party_slot(slot, 1, 0);
            } while ((u32)(menu_state_current->flags->party[slot] - 7) < 2);
            break;
        }
    } while (stay);
    menu_deathblow_screen_close();
    return 1;
}

/* 801E2250: Open the 801d3488 screen on the first ready party slot, which it returns. */
u8 menu_gear_command_open(void) {
    void *block;
    s32 i;

    block = heap_alloc(0x2af0, 0);
    menu_state_current->detail = block;
    bzero(block, 0x2af0);
    block = heap_alloc(0x32f4, 0);
    menu_state_current->equip_panel = block;
    bzero(block, 0x32f4);
    block = heap_alloc(0x2ac, 0);
    menu_state_current->equip_labels = block;
    bzero(block, 0x2ac);
    menu_load_or_release_data_set(3);
    i = 0;
    while (1) {
        if (menu_state_current->flags->ready[i] != 0) {
            break;
        }
        i++;
    }
    menu_member_marks_show(0, 1);
    return i;
}

/* 801E2324: Lay out the six labels of page `page` (menu_gear_command_label_ids) at +18e0. */
void menu_gear_command_render_labels(u8 page) {
    menu_label_render_table(6, menu_state_current->labels18e0, menu_gear_command_label_ids + page, menu_state_current->flags->labels18e0_shown);
}

/* 801E2368: Free the three screen blocks (+358, +35c, +360) and restore the view (13). */
void menu_gear_command_close(void) {
    heap_free(menu_state_current->detail);
    heap_free(menu_state_current->equip_panel);
    heap_free(menu_state_current->equip_labels);
    menu_load_or_release_data_set(0x13);
}

/* 801E23CC: The status command: show party slot `slot`'s status page (6 later
 * labels when its +f8e5 flag is set) and choose among four choices (0 the
 * 801e05d0 screen, 1 and 2 the 801ddf24 screen modes, 3 toggles the flag
 * when the member has a gear; member 7 and the flag mode_gear_riding_lock refuse),
 * switching members with 9/10, until cancelled. The first 801d9704 call
 * passes the slot before it is set, as the original does; 801d7cfc is
 * called without a prototype in this unit (the slot goes unmasked). */
u8 menu_gear_command_run(void) {
    s32 slot;
    u8 shown;
    u8 stay;
    u8 first;
    u8 page;

    stay = 1;
    menu_step_party_slot(slot, 0, 1);
    shown = 0xf3;
    first = 1;
    menu_state_current->choice = 0;
    menu_state_current->choice_shown = 0xff;
    slot = menu_gear_command_open();
    while (stay) {
        menu_run_frame();
        if (slot != shown) {
            menu_compute_slot_gear_stats(slot);
            menu_member_page_build(slot, 1);
            shown = slot;
            page = game_data.inGear[slot] ? 6 : 0;
            menu_gear_command_render_labels(page);
            menu_label_place(6, menu_state_current->labels18e0, &menu_gear_command_label_ids[6], menu_gear_command_label_x_offsets, menu_state_current->flags->labels18e0_shown, 4, 7, 6);
            menu_label_place(6, menu_state_current->labels18e0, &menu_gear_command_label_ids[6], menu_gear_command_label_x_offsets, menu_state_current->flags->labels18e0_shown, 5, 7, 6);
            menu_state_current->labels18e0[4].projected = 1;
            menu_state_current->labels18e0[5].projected = 1;
            if (first) {
                first = 0;
                menu_view_start_zoom_in();
                menu_field_blocks_slide(0, 0);
                menu_choice_window_open(0);
            }
        }
        if (menu_state_current->choice != menu_state_current->choice_shown) {
            menu_label_place(4, menu_state_current->labels18e0, &menu_gear_command_label_ids[6], &menu_gear_command_label_x_offsets[page], menu_state_current->flags->labels18e0_shown,
                          menu_state_current->choice, 7, 0);
            menu_choice_window_set_cursor(0);
            menu_state_current->choice_shown = menu_state_current->choice;
        }
        switch (menu_state_current->input) {
        case 4:
            menu_highlight_hide();
            menu_label_clear_shown(4, menu_state_current->flags->labels18e0_shown);
            menu_state_current->flags->detail_shown = 0;
            menu_state_current->flags->equipment_shown = 0;
            menu_state_current->flags->equip_labels_shown = 0;
            menu_state_current->flags->labels18e0_shown[4] = 0;
            menu_state_current->flags->labels18e0_shown[5] = 0;
            switch (menu_state_current->choice) {
            case 0:
                if (menu_state_current->flags->party[slot] != 7) {
                    menu_state_current->flags->lists_shown = 0;
                    menu_equip_screen_run(slot, 0, 1);
                    menu_equip_screen_close();
                    menu_equip_panel_build(slot, 0, 0, 1);
                    menu_equip_labels_layout_parts(slot, 0, 0, 1);
                    menu_state_current->flags->lists_shown = 1;
                } else {
                    menu_play_sound(4);
                }
                shown = 0xff;
                break;
            case 1:
                if (menu_state_current->flags->party[slot] != 7) {
                    menu_arts_screen_run(slot, 0, 1);
                    menu_arts_screen_close(1);
                } else {
                    menu_play_sound(4);
                }
                shown = 0xff;
                break;
            case 2:
                if (menu_state_current->flags->party[slot] != 7) {
                    menu_arts_screen_run(slot, 0, 2);
                    menu_arts_screen_close(2);
                } else {
                    menu_play_sound(4);
                }
                shown = 0xff;
                break;
            case 3:
                if (mode_gear_riding_lock == 0) {
                    if (game_data.inGear[slot] != 0) {
                        game_data.inGear[slot] = 0;
                        page = 0;
                    } else if (game_data.characters[menu_state_current->flags->party[slot]].gearId != 0xff) {
                        page = 6;
                        game_data.inGear[slot] = 1;
                    }
                    menu_detail_layout_tabs(slot, 1, game_data.inGear[slot]);
                } else {
                    menu_play_sound(4);
                }
                shown = 0xff;
                break;
            }
            menu_state_current->flags->labels18e0_shown[4] = 1;
            menu_state_current->flags->labels18e0_shown[5] = 1;
            menu_label_render_table(6, menu_state_current->labels18e0, &menu_gear_command_label_ids[page], menu_state_current->flags->labels18e0_shown);
            menu_state_current->flags->detail_shown = 1;
            menu_state_current->flags->equipment_shown = 1;
            menu_state_current->flags->equip_labels_shown = 1;
            menu_highlight_place(menu_state_current->choice + 7, 1);
            menu_state_current->flags->sprite_shown = 1;
            menu_state_current->choice_shown = 0xff;
            menu_member_marks_show(0, 1);
            break;
        case 5:
            stay = 0;
            break;
        case 2:
        case 6:
        case 7:
        case 8:
            break;
        case 1:
            if (menu_state_current->choice != 0) {
                menu_state_current->choice--;
            } else {
                menu_state_current->choice = menu_state_current->choice_count - 1;
            }
            break;
        case 3:
            if (++menu_state_current->choice >= menu_state_current->choice_count) {
                menu_state_current->choice = 0;
            }
            break;
        case 9:
            slot = menu_step_party_slot(slot, 0, 1);
            menu_state_current->choice_shown = 0xff;
            break;
        case 10:
            slot = menu_step_party_slot(slot, 1, 1);
            menu_state_current->choice_shown = 0xff;
            break;
        }
    }
    menu_label_clear_shown(6, menu_state_current->flags->labels18e0_shown);
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    return 1;
}

/* 801E2AE0: Open the 801d3488 screen: its three blocks and view 3. */
void menu_character_command_open(void) {
    void *block;

    menu_party_labels_show(1);
    block = heap_alloc(0x2af0, 0);
    menu_state_current->detail = block;
    bzero(block, 0x2af0);
    block = heap_alloc(0x32f4, 0);
    menu_state_current->equip_panel = block;
    bzero(block, 0x32f4);
    block = heap_alloc(0x2ac, 0);
    menu_state_current->equip_labels = block;
    bzero(block, 0x2ac);
    menu_load_or_release_data_set(3);
    menu_member_marks_show(0, 0);
}

/* 801E2B80: Free the three screen blocks (+358, +35c, +360) and restore the view (13). */
void menu_character_command_close(void) {
    heap_free(menu_state_current->detail);
    heap_free(menu_state_current->equip_panel);
    heap_free(menu_state_current->equip_labels);
    menu_load_or_release_data_set(0x13);
}

/* 801E2BE4: The equipment command: on the first party member's page, choose among
 * three choices (0 the 801e05d0 screen, 1 the 801ddf24 screen, 2 the
 * 801e1544 screen), switching members with 9/10, until cancelled. */
u8 menu_character_command_run(void) {
    s32 slot;
    u8 shown;
    u8 stay;
    u8 first;
    s32 i;

    stay = 1;
    shown = 0xf3;
    first = 1;
    slot = menu_state_current->first_member;
    menu_state_current->choice = 2;
    menu_state_current->choice_shown = 0xff;
    menu_character_command_open();
    for (i = 0; i < 3; i++) {
        if (menu_state_current->flags->party[i] != 0xff) {
            slot = i;
            break;
        }
    }
    while (stay) {
        menu_run_frame();
        if (slot != shown) {
            menu_compute_character_equipment(menu_state_current->tables, menu_state_current->flags->party[slot]);
            menu_compute_character_stats(menu_state_current->tables, menu_state_current->flags->party[slot]);
            menu_member_page_build(slot, 0);
            shown = slot;
            if (first) {
                first = 0;
                menu_view_start_zoom_in();
                menu_field_blocks_slide(0, 0);
                menu_choice_window_open(0);
            }
        }
        if (menu_state_current->choice != menu_state_current->choice_shown) {
            menu_choice_label_place();
            menu_choice_window_set_cursor(0);
            menu_state_current->choice_shown = menu_state_current->choice;
        }
        switch (menu_state_current->input) {
        case 4:
            menu_highlight_hide();
            menu_row_labels_hide();
            menu_state_current->flags->detail_shown = 0;
            menu_state_current->flags->equipment_shown = 0;
            menu_state_current->flags->equip_labels_shown = 0;
            switch (menu_state_current->choice) {
            case 0:
                menu_state_current->flags->lists_shown = 0;
                menu_equip_screen_run(slot, 0, 0);
                menu_equip_screen_close();
                menu_equip_panel_build(slot, 0, 0, 0);
                menu_equip_labels_layout_parts(slot, 0, 0, 0);
                shown = 0xff;
                menu_state_current->flags->lists_shown = 1;
                break;
            case 1:
                menu_arts_screen_run(slot, 0, 0);
                menu_arts_screen_close(0);
                shown = 0xff;
                break;
            case 2:
                menu_deathblow_screen_run(slot);
                break;
            }
            menu_party_labels_show(1);
            menu_state_current->flags->detail_shown = 1;
            menu_state_current->flags->equipment_shown = 1;
            menu_state_current->flags->equip_labels_shown = 1;
            menu_highlight_place(menu_state_current->choice + 7, 1);
            menu_state_current->flags->sprite_shown = 1;
            menu_state_current->choice_shown = 0xff;
            menu_member_marks_show(0, 0);
            break;
        case 5:
            stay = 0;
            break;
        case 2:
        case 6:
        case 7:
        case 8:
            break;
        case 1:
            if (menu_state_current->choice != 0) {
                menu_state_current->choice--;
            } else {
                menu_state_current->choice = menu_state_current->choice_count - 1;
            }
            break;
        case 3:
            if (++menu_state_current->choice >= menu_state_current->choice_count) {
                menu_state_current->choice = 0;
            }
            break;
        case 9:
            slot = menu_step_party_slot(slot, 0, 0);
            break;
        case 10:
            slot = menu_step_party_slot(slot, 1, 0);
            break;
        }
    }
    menu_party_labels_show(0);
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    return 1;
}

/* 801E3088: Close the screen of the command at `offset` past the top cursor. */
void menu_top_command_close(u8 offset) {
    switch (menu_state_current->cursor + offset) {
    case 1:
    case 8:
        menu_file_screen_leave_card_mode();
        break;
    case 2:
        menu_state_current->flags->detail_shown = 0;
        menu_state_current->flags->equipment_shown = 0;
        menu_state_current->flags->equip_labels_shown = 0;
        menu_gear_command_close();
        break;
    case 3:
        menu_arts_screen_close(0);
        break;
    case 4:
        menu_item_screen_close();
        break;
    case 5:
        menu_equip_screen_close();
        menu_equip_command_close();
        break;
    case 6:
        menu_state_current->flags->detail_shown = 0;
        menu_state_current->flags->equipment_shown = 0;
        menu_state_current->flags->equip_labels_shown = 0;
        menu_row_labels_hide();
        menu_character_command_close();
        break;
    case 0:
    case 7:
    case 9:
        break;
    }
}

/* 801E31C0: Use item `item` on character `id`: restore HP (x50) and/or EP (x10),
 * raise stats (capped at 200, HP max 999, EP max 99), change +78, or run a
 * debug fill. Returns nonzero when the restoring item had no effect. */
u8 menu_use_item_on_character(MenuTables *tables, u8 id, u8 item) {
    CharacterRecord *chara;
    ItemInfo *record;
    u8 hpFull;
    u8 epFull;
    s32 hpRate;
    s32 epRate;

    hpFull = 0;
    epFull = 0;
    record = tables->items;
    record += item;
    chara = game_data.characters + id;
    epRate = 10;
    if (record->flags & 0x8000) {
        if (chara->hp == chara->maxHp) {
            hpFull = 1;
        } else {
            hpRate = 50;
            chara->hp += record->amount * hpRate;
        }
    }
    if (record->flags & 0x4000) {
        if (chara->ep == chara->maxEp) {
            epFull = 1;
        } else {
            chara->ep += epRate * record->amount;
        }
    }
    if (chara->hp > chara->maxHp) {
        chara->hp = chara->maxHp;
    }
    if (chara->ep > chara->maxEp) {
        chara->ep = chara->maxEp;
    }
    if (record->flags & 4) {
        if (record->stats & 0x8000) {
            chara->attack += record->amount;
        }
        if (record->stats & 0x4000) {
            chara->defense += record->amount;
        }
        if (record->stats & 0x2000) {
            chara->ether += record->amount;
        }
        if (record->stats & 0x1000) {
            chara->etherDefense += record->amount;
        }
        if (record->stats & 0x800) {
            chara->maxHp += record->amount;
        }
        if (record->stats & 0x400) {
            chara->maxEp += record->amount;
        }
        if (chara->attack > 200) {
            chara->attack = 200;
        }
        if (chara->defense > 200) {
            chara->defense = 200;
        }
        if (chara->ether > 200) {
            chara->ether = 200;
        }
        if (chara->etherDefense > 200) {
            chara->etherDefense = 200;
        }
        if (chara->maxHp >= 1000) {
            chara->maxHp = 999;
        }
        if (chara->maxEp >= 100) {
            chara->maxEp = 99;
        }
    }
    if (record->flags & 2) {
        if (record->stats & 0x8000) {
            chara->field78 += record->stats;
            if (chara->field78 > 200) {
                chara->field78 = 200;
            }
        } else if (chara->field78 < (record->stats & 0xff)) {
            chara->field78 = 0;
        } else {
            chara->field78 -= record->stats;
        }
    }
    if (record->flags & 1) {
        switch (record->amount) {
        case 1:
            menu_debug_fill_inventory();
            break;
        case 2:
            menu_debug_set_skill_masks();
            break;
        }
    }
    if (record->flags & 0x8000) {
        if (record->flags & 0x4000) {
            if (hpFull && epFull) {
                return epFull;
            }
            return 0;
        }
        return hpFull;
    }
    if (record->flags & 0x4000) {
        return epFull;
    }
    return 0;
}

/* 801E35BC: Apply `user`'s restoring effect: to `target`'s HP (the user's ether, +5b,
 * times the effect's +11, capped at the maximum), or with `gear` to the user's gear
 * (+60 up by a tenth of +64, capped at +64). */
void menu_apply_restoring_art(tables, user, target, effect, gear)
MenuTables *tables;
u8 user;
u8 target;
u8 effect;
u8 gear;
{
    CharacterRecord *source;
    CharacterRecord *dest;
    GearRecord *machine;
    ArtInfo *record;

    source = &game_data.characters[user];
    dest = &game_data.characters[target];
    machine = (GearRecord *)&game_data.characters[game_data.characters[user].gearId + 11];
    if (!gear) {
        record = tables->arts[user];
        record += effect;
        dest->hp += source->ether * record->unk11;
        if (dest->hp > dest->maxHp) {
            dest->hp = dest->maxHp;
        }
    } else {
        machine->hp += machine->maxHp / 10;
        if (machine->maxHp < machine->hp) {
            machine->hp = machine->maxHp;
        }
    }
}

/* 801E36D4: Recompute character `id`'s equipment values: sum its three accessories
 * (amount, kind bits and stat bonuses) and take its weapon's values (kind 4
 * characters: both weapons). */
void menu_compute_character_equipment(MenuTables *tables, u8 id) {
    CharacterRecord *chara;
    AccessoryInfo *accessory;
    EquipInfo *weapon;
    u8 i;
    u8 amount;

    chara = &game_data.characters[id];
    chara->bodyDefense = 0;
    chara->flags32 = 0;
    chara->equipAttack = 0;
    chara->equipDefense = 0;
    chara->equipSpeed = 0;
    chara->equipEther = 0;
    chara->equipEtherDefense = 0;
    chara->equip5E = 0;
    chara->equip5F = 0;
    chara->hpBonus = 0;
    chara->epBonus = 0;
    chara->status7E = 0;
    chara->status82 = 0;
    chara->status84.half.permanent = 0;
    chara->status88.half.permanent = 0;
    chara->status8C.half.permanent = 0;
    chara->fieldA1 = 0;
    for (i = 0; i < 3; i++) {
        accessory = tables->accessories;
        accessory += chara->accessories[i];
        chara->bodyDefense += accessory->amount;
        switch (accessory->kind) {
        case 1:
            chara->status7E |= accessory->value;
            break;
        case 2:
            chara->status82 |= accessory->value;
            break;
        case 3:
            chara->status84.half.permanent |= accessory->value;
            break;
        case 4:
            chara->status88.half.permanent |= accessory->value;
            break;
        case 7:
            chara->status8C.half.permanent |= accessory->value;
            break;
        case 5:
            chara->flags32 |= accessory->value;
            break;
        case 8:
        case 9:
            chara->hpBonus += accessory->value;
            break;
        case 10:
            chara->fieldA1 += accessory->value;
            break;
        }
        amount = accessory->stats;
        if (accessory->stats & 0x8000) {
            chara->equipAttack += amount;
        }
        if (accessory->stats & 0x4000) {
            chara->equipDefense += amount;
        }
        if (accessory->stats & 0x2000) {
            chara->equipSpeed += amount;
        }
        if (accessory->stats & 0x1000) {
            chara->equipEther += amount;
        }
        if (accessory->stats & 0x800) {
            chara->equipEtherDefense += amount;
        }
        if (accessory->stats & 0x400) {
            chara->equip5E += amount;
        }
        if (accessory->stats & 0x200) {
            chara->equip5F += amount;
        }
        if (accessory->stats & 0x100) {
            chara->bodyDefense += amount;
        }
    }
    weapon = tables->equipment;
    weapon += chara->weapons[0];
    chara->entries[0].value4 = weapon->power;
    chara->entries[0].field0 = weapon->value;
    chara->entries[0].value2 = weapon->a;
    chara->entries[0].value3 = weapon->b;
    if (chara->characterId == 4) {
        weapon = tables->equipment;
        weapon += chara->entryItems[0];
        chara->entries[0].value4 = weapon->power;
        chara->entries[0].field0 = weapon->value;
        chara->entries[0].value2 = weapon->a;
        chara->entries[0].value3 = weapon->b;
        weapon = tables->equipment;
        weapon += chara->entryItems[3];
        chara->entries[3].value4 = weapon->power;
        chara->entries[3].field0 = weapon->value;
        chara->entries[3].value2 = weapon->a;
        chara->entries[3].value3 = weapon->b;
    }
    if (chara->entries[0].value3 == 100) {
        chara->status8C.half.permanent |= chara->entries[0].field0;
    }
}

/* 801E3A80: Compute character `id`'s shown stats: base values plus equipment bonuses
 * (the first from the level, scaled 6/10 with +1c for kind 4), capped at
 * 250, 99 or 16. */
void menu_compute_character_stats(MenuTables *tables, u8 id) {
    CharacterRecord *chara;

    chara = &game_data.characters[id];
    if (chara->characterId == 4) {
        tables->stats[0] = (chara->entries[0].value4 + chara->entries[3].value4) * 6 / 10;
    } else {
        tables->stats[0] = chara->entries[0].value4 + (chara->attack + chara->equipAttack);
    }
    tables->stats[1] = chara->field5E + chara->equip5E;
    tables->stats[2] = chara->bodyDefense + (chara->defense + chara->equipDefense);
    tables->stats[3] = chara->field5F + chara->equip5F;
    tables->stats[4] = chara->ether + chara->equipEther;
    tables->stats[5] = chara->etherDefense + chara->equipEtherDefense;
    tables->stats[6] = chara->speed + chara->equipSpeed;
    if (tables->stats[0] >= 251) {
        tables->stats[0] = 250;
    }
    if (tables->stats[1] >= 100) {
        tables->stats[1] = 99;
    }
    if (tables->stats[2] >= 251) {
        tables->stats[2] = 250;
    }
    if (tables->stats[3] >= 100) {
        tables->stats[3] = 99;
    }
    if (tables->stats[4] >= 251) {
        tables->stats[4] = 250;
    }
    if (tables->stats[5] >= 251) {
        tables->stats[5] = 250;
    }
    if (tables->stats[6] >= 21) {
        tables->stats[6] = 16;
    }
}

/* 801E3C2C: Compute gear `gear`'s shown stats from its record and its pilot (flag 1000
 * makes gear 9's pilot character 10); gear 7 first takes its values from
 * character 7 (HP x50, stats plus bonuses). */
void menu_compute_gear_summary(MenuTables *tables, u8 gear) {
    GearRecord *record;
    CharacterRecord *pilot;
    s32 bonus;

    if (game_data.flags & 0x1000) {
        menu_gear_pilots[9] = 10;
    }
    if (gear == 7) {
        game_data.gears[7].hp = game_data.characters[7].hp * 50;
        game_data.gears[7].maxHp = game_data.characters[7].maxHp * 50;
        game_data.gears[7].attack = game_data.characters[7].attack + game_data.characters[7].equipAttack;
        game_data.gears[7].bodyDefense = (game_data.characters[7].defense + game_data.characters[7].equipDefense) * 12;
        game_data.gears[7].armor = (game_data.characters[7].etherDefense + game_data.characters[7].equipEtherDefense) * 6;
        game_data.gears[7].speed = game_data.characters[7].speed + game_data.characters[7].equipSpeed;
    }
    record = &game_data.gears[gear];
    pilot = &game_data.characters[menu_gear_pilots[gear]];
    tables->gear.hp = record->hp;
    tables->gear.max_hp = record->maxHp;
    tables->gear.defense = record->bodyDefense + record->equipBodyDefense;
    tables->gear.ether_defense = pilot->etherDefense + pilot->equipEtherDefense + record->equipArmor + record->armor;
    tables->gear.value68 = record->field68 + record->equip68a;
    tables->gear.value6a = record->field6A;
    tables->gear.fuel = record->fuel;
    tables->gear.max_fuel = record->maxFuel;
    bonus = record->attack * (record->field74 + record->equipAttackScale);
    if (gear == 5 || gear == 13) {
        tables->gear.attack = (record->entries[0].valueE + record->entries[2].valueE) * 6 / 10 + bonus;
    } else {
        tables->gear.attack = record->entries[0].valueE + bonus;
    }
    tables->gear.hit = record->hitBonus + record->equipHitBonus;
    tables->gear.speed = record->speed - record->speedPenalty;
    tables->gear.frame_factor = record->frameFactor + record->equipFrameFactor;
    tables->gear.value9d = record->field9D;
    tables->gear.guard = record->guard;
}

/* 801E3ECC: Compute gear `gear`'s part and weapon values, then mirror its +9 values
 * (and for gears 4, 5 its weapons) into its other form (gears 1, 15, 10-14)
 * and compute that too. */
void menu_compute_gear_equipment(MenuTables *tables, u8 gear) {
    u8 i;

    menu_sum_gear_accessories(tables, gear);
    menu_set_gear_weapon_values(tables, gear);
    switch (gear) {
    case 0:
        for (i = 0; i < 3; i++) {
            game_data.gears[1].parts[i] = game_data.gears[0].parts[i];
        }
        menu_sum_gear_accessories(tables, 1);
        break;
    case 1:
        for (i = 0; i < 3; i++) {
            game_data.gears[15].parts[i] = game_data.gears[1].parts[i];
        }
        menu_sum_gear_accessories(tables, 15);
        break;
    case 2:
        for (i = 0; i < 3; i++) {
            game_data.gears[10].parts[i] = game_data.gears[2].parts[i];
        }
        menu_sum_gear_accessories(tables, 10);
        break;
    case 3:
        for (i = 0; i < 3; i++) {
            game_data.gears[11].parts[i] = game_data.gears[3].parts[i];
        }
        menu_sum_gear_accessories(tables, 11);
        break;
    case 4:
        for (i = 0; i < 3; i++) {
            game_data.gears[12].parts[i] = game_data.gears[4].parts[i];
        }
        menu_sum_gear_accessories(tables, 12);
        game_data.gears[12].weapons[0] = game_data.gears[4].weapons[0];
        menu_set_gear_weapon_values(tables, 12);
        break;
    case 5:
        for (i = 0; i < 3; i++) {
            game_data.gears[13].parts[i] = game_data.gears[5].parts[i];
        }
        menu_sum_gear_accessories(tables, 13);
        game_data.gears[13].weapons[0] = game_data.gears[5].weapons[0];
        game_data.gears[13].weapons[3] = game_data.gears[5].weapons[3];
        game_data.gears[13].partItems[0] = game_data.gears[5].partItems[0];
        game_data.gears[13].partItems[3] = game_data.gears[5].partItems[3];
        menu_set_gear_weapon_values(tables, 13);
        break;
    case 6:
        for (i = 0; i < 3; i++) {
            game_data.gears[14].parts[i] = game_data.gears[6].parts[i];
        }
        menu_sum_gear_accessories(tables, 14);
        break;
    }
}

/* 801E4170: Recompute gear `gear`'s derived values from the data tables. */
void menu_set_gear_base_values(MenuTables *tables, u8 gear) {
    menu_set_gear_engine_values(tables, gear);
    menu_set_gear_part_values(tables, gear);
    menu_set_gear_frame_values(tables, gear);
}

/* 801E41C0: Take gear `gear`'s engine values from the tables, keeping +60 within +64. */
void menu_set_gear_engine_values(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearEngineInfo *engine;

    record = &game_data.gears[gear];
    engine = tables->engines;
    engine += record->engine;
    record->hp = engine->unk4;
    record->maxHp = engine->unk4;
    record->speed = engine->unk14;
    record->frameFactor = engine->unk15;
    record->field9D = engine->unk16;
    record->hitBonus = engine->unk17;
    if (record->maxHp < record->hp) {
        record->hp = record->maxHp;
    }
}

/* 801E4258: Copy gear `gear`'s two frame values (+8, +a of its frame record) into the
 * gear record (+70, +72). */
void menu_set_gear_frame_values(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearFrameInfo *frame;

    record = &game_data.gears[gear];
    frame = tables->frames;
    frame += record->frame;
    record->bodyDefense = frame->unk8;
    record->armor = frame->unkA;
}

/* 801E42AC: Take gear `gear`'s part values from the tables, keeping +38 within +3a. */
void menu_set_gear_part_values(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearPartInfo *part;

    record = &game_data.gears[gear];
    part = tables->parts;
    part += record->field3;
    record->maxFuel = part->unk6;
    record->attack = part->unkC;
    record->pad3D = part->unkD;
    record->field3E = part->unkE;
    record->attackScale = part->unkE;
    if (record->fuel > record->maxFuel) {
        record->fuel = record->maxFuel;
    }
}

/* 801E433C: Sum gear `gear`'s three parts (table +14) into its record: their stats,
 * and by each part's kind its bit words, amounts or the pilot's flag bits;
 * then its level (801e4928) and the pilot's flag 8000 (set while +4f, else
 * cleared when the pilot flies this gear). */
void menu_sum_gear_accessories(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearAccessoryInfo *part;
    u16 *bits;
    u16 *flags;
    u8 i;
    u8 j;

    record = &game_data.gears[gear];
    bits = &game_data.skills[menu_gear_pilots[gear]].unlocksA;
    flags = &game_data.skills[menu_gear_pilots[gear]].flags1A;
    record->equipBodyDefense = 0;
    record->equipArmor = 0;
    record->equip68a = 0;
    record->field48 = 0;
    record->equipGuard = 0;
    record->equipHitBonus = 0;
    record->equipSpeed = 0;
    record->field4F = 0;
    record->field6E = 0;
    record->equipFrameFactor = 0;
    for (i = 0; i < 16; i++) {
        record->resistances[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        record->speedBonus[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        ((GearRecordBytes55 *)record)->bytes55[i] = 0;
    }
    record->field7E = 0;
    record->status82 = 0;
    record->status84.half.permanent &= 0xf000;
    *bits &= 0xfb6f;
    for (j = 0; j < 3; j++) {
        part = tables->gear_accessories;
        part += record->parts[j];
        record->equipBodyDefense += part->unkD;
        record->equipArmor += part->unkE;
        record->equip68a += part->unk6;
        record->equipGuard += part->unk18;
        record->equipHitBonus += part->unk14;
        record->equipFrameFactor += part->unk1B;
        for (i = 0; i < 4; i++) {
            record->speedBonus[i] += part->unk10[i];
        }
        switch (part->kind) {
        case 1:
            record->field7E |= part->value;
            break;
        case 2:
            record->status82 |= part->value;
            break;
        case 3:
            record->status84.half.permanent |= part->value;
            break;
        case 4:
            record->field6E |= part->value;
            for (i = 0; i < 16; i++) {
                if (part->value & (0x8000 >> i)) {
                    record->resistances[i] += part->unk1A;
                }
            }
            break;
        case 5:
            record->field4F += part->value;
            break;
        case 6:
            if ((*bits & 0x1000) && (*bits & 0x800)) {
                *bits |= 0x400;
            }
            break;
        case 7:
            if ((*bits & 0x200) && (*bits & 0x100)) {
                *bits |= 0x80;
            }
            break;
        case 8:
            if ((*bits & 0x40) && (*bits & 0x20)) {
                *bits |= 0x10;
            }
            break;
        case 9:
            record->field48 |= part->value;
        case 10:
            record->equipAttackScale += part->value;
            break;
        case 11:
            record->chargeRate += part->value;
            break;
        }
    }
    record->speedPenalty = menu_compute_gear_speed_penalty(gear);
    if (record->field4F) {
        *flags |= 0x8000;
    } else if (gear == game_data.characters[menu_gear_pilots[gear]].gearId) {
        *flags &= 0x7fff;
    }
}

/* 801E4754: Take gear `gear`'s weapon values (table +18) into its first slot and
 * attributes; gears 5 and 13 instead take their three slot weapons. */
void menu_set_gear_weapon_values(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearWeaponInfo *weapon;

    record = &game_data.gears[gear];
    weapon = tables->gear_weapons;
    weapon += record->weapons[0];
    record->entries[0].valueE = weapon->unkE;
    record->entries[0].field0 = weapon->unk12;
    record->entries[0].value10 = weapon->unk10;
    record->entries[0].value11 = weapon->unk11;
    record->fileVariant = weapon->attrs[0];
    record->spriteVariants[0] = weapon->attrs[1];
    record->spriteVariants[1] = weapon->attrs[2];
    record->spriteVariants[2] = weapon->attrs[3];
    if (record->entries[0].value11 == 100) {
        record->status84.half.permanent &= 0xfff;
        record->status84.half.permanent |= record->entries[0].field0;
    }
    if (gear == 5 || gear == 13) {
        weapon = tables->gear_weapons;
        weapon += record->partItems[0];
        record->entries[0].valueE = weapon->unkE;
        record->entries[0].field0 = weapon->unk12;
        record->entries[0].value10 = weapon->unk10;
        record->entries[0].value11 = weapon->unk11;
        record->spriteVariants[0] = weapon->attrs[1];
        weapon = tables->gear_weapons;
        weapon += record->partItems[1];
        record->entries[1].valueE = weapon->unkE;
        record->entries[1].field0 = weapon->unk12;
        record->entries[1].value10 = weapon->unk10;
        record->entries[1].value11 = weapon->unk11;
        record->spriteVariants[1] = weapon->attrs[2];
        weapon = tables->gear_weapons;
        weapon += record->partItems[3];
        record->entries[2].valueE = weapon->unkE;
        record->entries[2].field0 = weapon->unk12;
        record->entries[2].value10 = weapon->unk10;
        record->entries[2].value11 = weapon->unk11;
        record->spriteVariants[2] = weapon->attrs[3];
    }
}

/* 801E4928: Gear `gear`'s value: (its +44 / 120 - its +75) / 2, at least 0. */
u8 menu_compute_gear_speed_penalty(u8 gear) {
    GearRecord *record;
    s16 value;

    record = &game_data.gears[gear];
    value = ((u16)(record->equip68a / 120) - record->field75) / 2;
    if (value < 0) {
        value = 0;
    }
    return value;
}

/* 801E4998: Set the fuel cost of gear `gear`'s art record 37, the first of its
 * fuel-cost list, to 2/90 of the gear's maximum HP, in steps of ten. */
void menu_set_gear_fuel_art_cost(MenuTables *tables, u8 gear) {
    ArtInfo *art;

    /* One pointer walks from the gear's art records to the record. */
    art = (tables->arts + 11)[gear];
    art = &art[37];
    art->gearCost = game_data.gears[gear].maxHp / 10 * 2 / 9;
    art->gearCost = art->gearCost / 10 * 10;
}

/* A prototype, which drops GCC's built-in memcpy (psyq/libc.h keeps it; cc1 warns
 * of the conflict): the built-in moves the two constant 0xa38-byte copies below
 * inline, where the original calls memcpy. A length passed in a variable keeps the
 * built-in and gives the same calls; nothing decides which the original had. */
void *memcpy(void *dest, void *src, int n);

/* 801E4A28: Copy the game data into save buffer `save`: characters, gears (their
 * kept fields), names and the other blocks. */
void menu_copy_game_data_to_save(SaveData *save) {
    SaveWordsA4 *chara;
    SaveGear *dst;
    GearRecord *src;
    u8 i;

    for (i = 0; i < 11; i++) {
        chara = &save->chars[i];
        *chara = *(SaveWordsA4 *)&game_data.characters[i];
    }
    for (i = 0; i < 20; i++) {
        dst = &save->gears[i];
        src = &game_data.gears[i];
        dst->head = *(SaveWords10 *)src;
        dst->entries = *(SaveWords18 *)src->entries;
        dst->variants = *(s32 *)&src->fileVariant;
        dst->fuel = src->fuel;
        dst->hp = src->hp;
        dst->defense = src->defense;
        dst->field74 = src->field74;
        dst->field75 = src->field75;
    }
    save->names = *(SaveWordsDC *)&game_data.names;
    save->unk100 = *(SaveWords190 *)game_data.names[11];
    save->unkE4C = *(SaveWords78 *)game_data.unk1648;
    save->records = *(SaveWords160 *)game_data.skills;
    save->unk1024 = *(SaveWords100 *)&game_data.worldmap;
    memcpy(save->unk1124, ((u8 *)&game_data + 0x1920), 0xa38);
}

/* 801E4D10: Load the game data from save buffer `save`: characters, records, gears
 * (recomputing their table values before restoring the kept fields), names
 * and the other blocks. */
void menu_restore_game_data_from_save(SaveData *save, MenuTables *tables) {
    SaveWordsA4 *chara;
    SaveWordsA4 *saved;
    GearRecord *dst;
    SaveGear *src;
    u8 i;

    for (i = 0; i < 11; i++) {
        chara = (SaveWordsA4 *)&game_data.characters[i];
        saved = &save->chars[i];
        *chara = *saved;
    }
    *(SaveWords160 *)game_data.skills = save->records;
    for (i = 0; i < 20; i++) {
        dst = &game_data.gears[i];
        src = &save->gears[i];
        *(SaveWords10 *)dst = src->head;
        *(SaveWords18 *)dst->entries = src->entries;
        *(s32 *)&dst->fileVariant = src->variants;
        menu_set_gear_engine_values(tables, i);
        menu_set_gear_frame_values(tables, i);
        menu_set_gear_part_values(tables, i);
        menu_sum_gear_accessories(tables, i);
        dst->fuel = src->fuel;
        dst->hp = src->hp;
        dst->defense = src->defense;
        dst->field74 = src->field74;
        dst->field75 = src->field75;
    }
    *(SaveWordsDC *)game_data.names = save->names;
    *(SaveWords190 *)game_data.names[11] = save->unk100;
    *(SaveWords78 *)game_data.unk1648 = save->unkE4C;
    *(SaveWords100 *)&game_data.worldmap = save->unk1024;
    memcpy(((u8 *)&game_data + 0x1920), save->unk1124, 0xa38);
}

/* 801E5058: Debug: put ten of every entry into the five inventory lists. */
void menu_debug_fill_inventory(void) {
    u8 i;

    for (i = 1; i < 0x48; i++) {
        game_data.weaponIds[i] = i;
        game_data.weaponCounts[i] = 10;
    }
    for (i = 1; i < 0x96; i++) {
        game_data.accessoryIds[i] = i;
        game_data.accessoryCounts[i] = 10;
    }
    for (i = 1; i < 0x4c; i++) {
        game_data.itemIds[i + 2] = i;
        game_data.itemCounts[i + 2] = 10;
    }
    for (i = 1; i < 0x48; i++) {
        game_data.gearPartIds[i] = i;
        game_data.gearPartCounts[i] = 10;
    }
    for (i = 1; i < 0x69; i++) {
        game_data.gearAccessoryIds[i] = i;
        game_data.gearAccessoryCounts[i] = 10;
    }
}

/* 801E5178: Debug: reset game_data.joined and the eleven game_data.skills records (four values,
 * flag 7 and the +1a value) to their defaults. */
void menu_debug_set_skill_masks(void) {
    game_data.joined = 0x7ff;
    game_data.skills[0].counterSkills = 0xfff8;
    game_data.skills[0].levelSkills = 0xff00;
    game_data.skills[0].unlocksA = 0xfff0;
    game_data.skills[0].unlocksB = 0xfe00;
    game_data.skills[0].flags1A = 0xe000;
    game_data.skills[0].tier = 7;
    game_data.skills[1].counterSkills = 0xffe0;
    game_data.skills[1].levelSkills = 0xfff0;
    game_data.skills[1].unlocksA = 0xfff0;
    game_data.skills[1].unlocksB = 0xfff0;
    game_data.skills[1].flags1A = 0xc000;
    game_data.skills[1].tier = 7;
    game_data.skills[2].counterSkills = 0xffe0;
    game_data.skills[2].levelSkills = 0xffe0;
    game_data.skills[2].unlocksA = 0xfff0;
    game_data.skills[2].unlocksB = 0xff00;
    game_data.skills[2].flags1A = 0x8000;
    game_data.skills[2].tier = 7;
    game_data.skills[3].counterSkills = 0xffe0;
    game_data.skills[3].levelSkills = 0xffc0;
    game_data.skills[3].unlocksA = 0xfff0;
    game_data.skills[3].unlocksB = 0xff00;
    game_data.skills[3].flags1A = 0xf000;
    game_data.skills[3].tier = 7;
    game_data.skills[4].counterSkills = 0xffc0;
    game_data.skills[4].levelSkills = 0xffc0;
    game_data.skills[4].unlocksA = 0xfff0;
    game_data.skills[4].unlocksB = 0xffc0;
    game_data.skills[4].flags1A = 0xe000;
    game_data.skills[4].tier = 7;
    game_data.skills[5].counterSkills = 0xffc0;
    game_data.skills[5].levelSkills = 0xf000;
    game_data.skills[5].unlocksA = 0xfff0;
    game_data.skills[5].unlocksB = 0xf000;
    game_data.skills[5].flags1A = 0x8000;
    game_data.skills[5].tier = 7;
    game_data.skills[6].counterSkills = 0xffc0;
    game_data.skills[6].levelSkills = 0xff00;
    game_data.skills[6].unlocksA = 0xfff0;
    game_data.skills[6].unlocksB = 0xff00;
    game_data.skills[6].flags1A = 0x8000;
    game_data.skills[6].tier = 7;
    game_data.skills[7].counterSkills = 0;
    game_data.skills[7].levelSkills = 0xff00;
    game_data.skills[7].unlocksA = 0;
    game_data.skills[7].unlocksB = 0xff00;
    game_data.skills[7].flags1A = 0;
    game_data.skills[7].tier = 7;
    game_data.skills[8].counterSkills = 0;
    game_data.skills[8].levelSkills = 0xf800;
    game_data.skills[8].unlocksA = 0xfff0;
    game_data.skills[8].unlocksB = 0;
    game_data.skills[8].flags1A = 0xe000;
    game_data.skills[8].tier = 7;
    game_data.skills[9].counterSkills = 0xffe0;
    game_data.skills[9].levelSkills = 0xffe0;
    game_data.skills[9].unlocksA = 0xfff0;
    game_data.skills[9].unlocksB = 0xffe0;
    game_data.skills[9].flags1A = 0x8000;
    game_data.skills[9].tier = 7;
    game_data.skills[10].counterSkills = 0xffc0;
    game_data.skills[10].levelSkills = 0xff00;
    game_data.skills[10].unlocksA = 0xfff0;
    game_data.skills[10].unlocksB = 0xff00;
    game_data.skills[10].flags1A = 0x8000;
    game_data.skills[10].tier = 7;
}

/* 801E53CC: Set up panel `index`: hide it and stop its opening, set the grey fills and
 * their draw modes, and the four edge lists' quads (sheet records 0-3). */
void menu_panel_init(u8 index) {
    MenuPanel *panel;
    RECT window;
    u8 i;

    panel = menu_state_current->panels[index];
    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    menu_state_current->flags->panels_shown[index] = 0;
    menu_state_current->flags->panels_growing[index] = 0;
    for (i = 0; i < 2; i++) {
        SetPolyG4(&panel->fill[i]);
        (panel->fill + i)->r0 = 0x68;
        (panel->fill + i)->g0 = 0x68;
        (panel->fill + i)->b0 = 0x68;
        (panel->fill + i)->r1 = 0x68;
        (panel->fill + i)->g1 = 0x68;
        (panel->fill + i)->b1 = 0x68;
        (panel->fill + i)->r2 = 0x68;
        (panel->fill + i)->g2 = 0x68;
        (panel->fill + i)->b2 = 0x68;
        (panel->fill + i)->r3 = 0x68;
        (panel->fill + i)->g3 = 0x68;
        (panel->fill + i)->b3 = 0x68;
        SetSemiTrans(&panel->fill[i], 1);
        SetDrawMode(&panel->fill_mode[i], 0, 0,
                    GetTPage(0, 0, menu_state_current->sheet_entries[0].page_x, menu_state_current->sheet_entries[0].page_y), &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->edge[0][i]);
        SetShadeTex(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xff;
        (panel->edge[0] + i)->g0 = 0xff;
        (panel->edge[0] + i)->b0 = 0xff;
        panel->edge[0][i].tpage = GetTPage(menu_state_current->sheet_entries[0].mode, 0, menu_state_current->sheet_entries[0].page_x,
                                            menu_state_current->sheet_entries[0].page_y);
        panel->edge[0][i].clut = GetClut(menu_state_current->sheet_entries[0].clut_x, menu_state_current->sheet_entries[0].clut_y);
        SetPolyFT4(&panel->edge[1][i]);
        SetShadeTex(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xff;
        (panel->edge[1] + i)->g0 = 0xff;
        (panel->edge[1] + i)->b0 = 0xff;
        panel->edge[1][i].tpage = GetTPage(menu_state_current->sheet_entries[1].mode, 0, menu_state_current->sheet_entries[1].page_x,
                                            menu_state_current->sheet_entries[1].page_y);
        panel->edge[1][i].clut = GetClut(menu_state_current->sheet_entries[1].clut_x, menu_state_current->sheet_entries[1].clut_y);
        SetPolyFT4(&panel->edge[2][i]);
        SetShadeTex(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xff;
        (panel->edge[2] + i)->g0 = 0xff;
        (panel->edge[2] + i)->b0 = 0xff;
        panel->edge[2][i].tpage = GetTPage(menu_state_current->sheet_entries[2].mode, 0, menu_state_current->sheet_entries[2].page_x,
                                            menu_state_current->sheet_entries[2].page_y);
        panel->edge[2][i].clut = GetClut(menu_state_current->sheet_entries[2].clut_x, menu_state_current->sheet_entries[2].clut_y);
        SetPolyFT4(&panel->edge[3][i]);
        SetShadeTex(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xff;
        (panel->edge[3] + i)->g0 = 0xff;
        (panel->edge[3] + i)->b0 = 0xff;
        panel->edge[3][i].tpage = GetTPage(menu_state_current->sheet_entries[3].mode, 0, menu_state_current->sheet_entries[3].page_x,
                                            menu_state_current->sheet_entries[3].page_y);
        panel->edge[3][i].clut = GetClut(menu_state_current->sheet_entries[3].clut_x, menu_state_current->sheet_entries[3].clut_y);
    }
}

/* 801E56E8: Set up image block `index`'s 16x16 sprite and semi-transparent cover at
 * its position for both buffers, and the two draw modes (blend mode 2). */
void menu_file_slot_init_icon(s32 index) {
    MenuSlotImage *image;
    s16 *x;
    s16 *y;
    s32 i;
    RECT window;

    i = 0;
    x = menu_file_slot_x_table[index];
    y = menu_file_slot_y_table[index];
    image = menu_state_current->slots[index];
    for (; i < 2; i++) {
        menu_quad_init(&image->icon[i]);
        image->icon[i].x0 = *x;
        image->icon[i].y0 = *y;
        image->icon[i].x1 = *x + 0x10;
        image->icon[i].y1 = *y;
        image->icon[i].x2 = *x;
        image->icon[i].y2 = *y + 0x10;
        image->icon[i].x3 = *x + 0x10;
        image->icon[i].y3 = *y + 0x10;
        image->icon[i].tpage = GetTPage(0, 0, 0x140, 0x80);
        SetPolyF4(&image->box[i]);
        image->box[i].r0 = 0x80;
        image->box[i].g0 = 0x80;
        image->box[i].b0 = 0x80;
        SetSemiTrans(&image->box[i], 1);
        image->box[i].x0 = *x;
        image->box[i].y0 = *y;
        image->box[i].x1 = *x + 0x10;
        image->box[i].y1 = *y;
        image->box[i].x2 = *x;
        image->box[i].y2 = *y + 0x10;
        image->box[i].x3 = *x + 0x10;
        image->box[i].y3 = *y + 0x10;
        menu_set_rect_verts(image->iconAt, *x, *y, 0x10, 0x10);
    }
    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    SetDrawMode(&image->boxMode[0], 0, 0, GetTPage(0, 2, 0x140, 0x80), &window);
    SetDrawMode(&image->boxMode[1], 0, 0, GetTPage(0, 2, 0x140, 0x80), &window);
}

/* 801E5924: Set up image block `index`'s green 16x16 frame at its position: the
 * top/right and left/bottom lines and their vertices, for both buffers. */
void menu_file_slot_init_frame(s32 index) {
    MenuSlotImage *image;
    s16 *x;
    s16 *y;
    s32 i;

    x = menu_file_slot_x_table[index];
    y = menu_file_slot_y_table[index];
    image = menu_state_current->slots[index];
    for (i = 0; i < 2; i++) {
        SetLineF3(&image->lineA[i]);
        image->lineA[i].r0 = 0;
        image->lineA[i].g0 = 0xff;
        image->lineA[i].b0 = 0;
        image->lineA[i].x0 = *x;
        image->lineA[i].y0 = *y;
        image->lineA[i].x1 = *x + 0x10;
        image->lineA[i].y1 = *y;
        image->lineA[i].x2 = *x + 0x10;
        image->lineA[i].y2 = *y + 0x10;
        menu_set_rect_verts(image->lineAAt, *x, *y, 0x10, 0x10);
        SetLineF3(&image->lineB[i]);
        image->lineB[i].r0 = 0;
        image->lineB[i].g0 = 0xff;
        image->lineB[i].b0 = 0;
        image->lineB[i].x0 = *x;
        image->lineB[i].y0 = *y;
        image->lineB[i].x1 = *x;
        image->lineB[i].y1 = *y + 0x10;
        image->lineB[i].x2 = *x + 0x10;
        image->lineB[i].y2 = *y + 0x10;
        menu_set_rect_verts(image->lineBAt, *x, *y, 0x10, 0x10);
    }
}

/* 801E5ACC: Allocate and set up the 32 image blocks (+3a8). */
void menu_file_slots_alloc(void) {
    s32 i;
    void *block;

    for (i = 0; i < 32; i++) {
        block = heap_alloc(0x158, 0);
        menu_state_current->slots[i] = block;
        bzero(block, 0x158);
        menu_file_slot_init_icon(i);
        menu_file_slot_init_frame(i);
    }
}

/* 801E5B3C: Free the 32 image blocks at +3a8. */
void menu_file_slots_free(void) {
    s32 i;

    for (i = 0; i < 32; i++) {
        heap_free(menu_state_current->slots[i]);
    }
}

/* 801E5B88: Set up the 32 text character quads (both buffers): 21 per line, 12x16
 * glyphs of the 140 page from v e0, 16 per glyph row. */
void menu_file_info_init_char_quads(void) {
    s32 i;
    s32 buffer;

    for (i = 0; i < 32; i++) {
        for (buffer = 0; buffer < 2; buffer++) {
            menu_quad_init(&menu_state_current->file_info->chars[i * 2 + buffer]);
            (menu_state_current->file_info->chars + (i * 2 + buffer))->x0 = menu_file_info_char_x_table[i % 21] + i / 21 * 8;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->y0 = menu_file_info_char_y_table[i / 21];
            (menu_state_current->file_info->chars + (i * 2 + buffer))->x1 = menu_file_info_char_x_table[i % 21] + i / 21 * 8 + 12;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->y1 = menu_file_info_char_y_table[i / 21];
            (menu_state_current->file_info->chars + (i * 2 + buffer))->x2 = menu_file_info_char_x_table[i % 21] + i / 21 * 8;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->y2 = menu_file_info_char_y_table[i / 21] + 16;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->x3 = menu_file_info_char_x_table[i % 21] + i / 21 * 8 + 12;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->y3 = menu_file_info_char_y_table[i / 21] + 16;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->u0 = i % 16 * 16;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->v0 = i / 16 * 16 - 0x20;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->u1 = i % 16 * 16 + 12;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->v1 = i / 16 * 16 - 0x20;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->u2 = i % 16 * 16;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->v2 = i / 16 * 16 - 0x11;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->u3 = i % 16 * 16 + 12;
            (menu_state_current->file_info->chars + (i * 2 + buffer))->v3 = i / 16 * 16 - 0x11;
            menu_state_current->file_info->chars[i * 2 + buffer].tpage = GetTPage(0, 0, 0x140, 0x80);
            menu_state_current->file_info->chars[i * 2 + buffer].clut = GetClut(0, 0x1c0);
        }
    }
}

/* 801E5E4C: Set up the 32x32 cursor sprite and the purple-to-black shaded band
 * (0,4a)-(140,8a) for both buffers. */
void menu_file_info_init_cursor_and_band(void) {
    s32 *x;
    s32 *y;
    s32 i;
    s32 right; /* the band's right edge */

    right = 0x140;
    i = 0;
    x = &menu_file_info_cursor_x;
    y = &menu_file_info_cursor_y;
    for (; i < 2; i++) {
        menu_quad_init(&menu_state_current->file_info->cursor[i]);
        (menu_state_current->file_info->cursor + i)->x0 = *x;
        (menu_state_current->file_info->cursor + i)->y0 = *y;
        (menu_state_current->file_info->cursor + i)->x1 = *x + 0x20;
        (menu_state_current->file_info->cursor + i)->y1 = *y;
        (menu_state_current->file_info->cursor + i)->x2 = *x;
        (menu_state_current->file_info->cursor + i)->y2 = *y + 0x20;
        (menu_state_current->file_info->cursor + i)->x3 = *x + 0x20;
        (menu_state_current->file_info->cursor + i)->y3 = *y + 0x20;
        menu_state_current->file_info->cursor[i].tpage = GetTPage(0, 0, 0x140, 0x80);
        SetPolyG4(&menu_state_current->file_info->band[i]);
        (menu_state_current->file_info->band + i)->r0 = 0x80;
        (menu_state_current->file_info->band + i)->g0 = 0;
        (menu_state_current->file_info->band + i)->b0 = 0x80;
        (menu_state_current->file_info->band + i)->r1 = 0;
        (menu_state_current->file_info->band + i)->g1 = 0;
        (menu_state_current->file_info->band + i)->b1 = 0x80;
        (menu_state_current->file_info->band + i)->r2 = 0x10;
        (menu_state_current->file_info->band + i)->g2 = 0;
        (menu_state_current->file_info->band + i)->b2 = 0x10;
        (menu_state_current->file_info->band + i)->r3 = 0;
        (menu_state_current->file_info->band + i)->g3 = 0;
        (menu_state_current->file_info->band + i)->b3 = 0x10;
        (menu_state_current->file_info->band + i)->x0 = 0;
        (menu_state_current->file_info->band + i)->y0 = 0x4a;
        (menu_state_current->file_info->band + i)->x1 = right;
        (menu_state_current->file_info->band + i)->y1 = 0x4a;
        (menu_state_current->file_info->band + i)->x2 = 0;
        (menu_state_current->file_info->band + i)->y2 = 0x8a;
        (menu_state_current->file_info->band + i)->x3 = right;
        (menu_state_current->file_info->band + i)->y3 = 0x8a;
    }
}

/* 801E61B0: Lay out the three save views' frames (nine images each, 50 apart) and
 * their 72x13 name quads (label rows 6 + view). */
void menu_file_info_layout_view_frames(void) {
    s32 view;
    s32 i;

    for (view = 0; view < 3; view++) {
        menu_state_current->file_info->views[view].frameCount = 0;
        for (i = 0; i < 9; i++) {
            if (menu_save_view_frame_images[i] != 0xffff) {
                menu_state_current->file_info->views[view].frameCount +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, menu_save_view_frame_images[i],
                                  menu_state_current->file_info->views[view].frame[menu_state_current->file_info->views[view].frameCount],
                                  menu_state_current->buffer_index, view * 0x50 + menu_save_view_frame_x_table[i], menu_save_view_frame_y_table[i], 0x1000);
            }
        }
        menu_state_current->file_info->views[view].frameBuffer = menu_state_current->buffer_index;
        menu_quad_init(&menu_state_current->file_info->views[view].name[menu_state_current->buffer_index]);
        menu_state_current->file_info->views[view].name[menu_state_current->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
        menu_state_current->file_info->views[view].name[menu_state_current->buffer_index].clut = text_plane0_clut;
        menu_quad_place(&menu_state_current->file_info->views[view].name[menu_state_current->buffer_index], menu_save_view_frame_x_table[0] + view * 0x50,
                      menu_save_view_frame_y_table[0] + 7, menu_name_image_vram_x_table[view + 6] * 4, menu_name_image_vram_y_table[view + 6], 0x48, 0xd);
        menu_state_current->file_info->views[view].nameBuffer = menu_state_current->buffer_index;
    }
}

/* 801E6450: Allocate and clear the 2dc0-byte block at +34c, then set it up. */
void menu_file_info_alloc(void) {
    void *block;
    u8 reserved[8]; /* the original frame reserves 8 unused bytes */

    block = heap_alloc(0x2dc0, 0);
    menu_state_current->file_info = block;
    bzero(block, 0x2dc0);
    menu_file_info_init_char_quads();
    menu_file_info_init_cursor_and_band();
}

/* 801E649C: Free the 2dc0-byte block at +34c and clear party flag +b. */
void menu_file_info_free(void) {
    heap_free(menu_state_current->file_info);
    menu_state_current->flags->file_info_shown = 0;
}

/* 801E64E0: Clear the 64x32 image area at (140, e0) to black and clear party flag +b. */
void menu_file_info_clear(void) {
    RECT rect;

    rect.x = 0x140;
    rect.y = 0xe0;
    rect.w = 0x40;
    rect.h = 0x20;
    ClearImage(&rect, 0, 0, 0);
    menu_state_current->flags->file_info_shown = 0;
}

/* 801E6544: Pack each of the 16 rows of `rows` in place, merging bytes 1-2, 5-6, 9-10
 * and 13-14. */
void menu_save_title_narrow_glyph(u8 rows[16][16]) {
    s32 i;

    for (i = 0; i < 16; i++) {
        rows[i][0] = rows[i][0];
        rows[i][1] = rows[i][1] | rows[i][2];
        rows[i][2] = rows[i][3];
        rows[i][3] = rows[i][4];
        rows[i][4] = rows[i][5] | rows[i][6];
        rows[i][5] = rows[i][7];
        rows[i][6] = rows[i][8];
        rows[i][7] = rows[i][9] | rows[i][10];
        rows[i][8] = rows[i][11];
        rows[i][9] = rows[i][12];
        rows[i][10] = rows[i][13] | rows[i][14];
        rows[i][11] = rows[i][15];
    }
}

/* 801E65E4: Return the 16x16 font glyph of the character at `s`: ASCII is converted
 * to its two-byte code (control characters to a space), two-byte codes pass
 * through. */
u16 *menu_save_title_find_glyph(u8 *s) {
    u8 hi;
    u8 lo;

    hi = s[0];
    lo = s[1];
    menu_save_title_char_is_two_byte = 1;
    if (hi < 0x80) {
        if (hi >= 0x20) {
            lo = menu_ascii_to_sjis_table[hi - 0x20];
            menu_save_title_char_is_two_byte = 0;
            hi = menu_ascii_to_sjis_table[hi - 0x20] >> 8;
        } else {
            hi = 0x81;
            lo = 0x40;
            menu_save_title_char_is_two_byte = 0;
        }
    }
    return (u16 *)Krom2RawAdd(lo | (hi << 8));
}

/* 801E6668: Render listed file `index`'s save title (up to 32 characters, 64 bytes)
 * as 12-pixel glyphs into the 4-bit 256x32 image at (140, e0). */
void menu_save_title_render(s32 index) {
    u8 *pixels;
    u16 *image;
    u8 *text;
    u16 *glyph;
    u8 *pixel;
    s32 bytes;
    s32 chars;
    s32 row;
    s32 col;
    RECT rect;

    pixels = heap_alloc(0x100, 1);
    image = heap_alloc(0x1000, 1);
    bzero((u_char *)image, 0x1000);
    bytes = 0;
    chars = 0;
    text = (u8 *)menu_state_current->card + (index << 9) + 0xb98;
    while (1) {
        if (*text == 0) {
            break;
        }
        pixel = pixels;
        glyph = menu_save_title_find_glyph(text);
        if (glyph != (u16 *)-1) {
            for (row = 0; row < 16; row++, glyph++) {
                for (col = 7; col >= 0; col--) {
                    *pixel++ = (*glyph >> col) & 1;
                }
                for (col = 15; col >= 8; col--) {
                    *pixel++ = (*glyph >> col) & 1;
                }
            }
            menu_save_title_narrow_glyph((u8(*)[16])pixels);
            for (row = 0; row < 16; row++) {
                for (col = 0; col < 12; col++) {
                    image[col / 4 + row * 64 + chars % 16 * 4 + chars / 16 * 1024] |= pixels[row * 16 + col]
                                                                                    << (col % 4 * 4);
                }
            }
        }
        text++;
        bytes++;
        if (menu_save_title_char_is_two_byte) {
            text++;
            bytes++;
        }
        if (bytes >= 0x40) {
            break;
        }
        if (++chars >= 0x20) {
            break;
        }
    }
    rect.x = 0x140;
    rect.y = 0xe0;
    rect.w = 0x40;
    rect.h = 0x20;
    LoadImage(&rect, (u_long *)image);
    DrawSync(0);
    heap_free(pixels);
    heap_free(image);
}

/* 801E68AC: Lay out the save's play time (two separators and seven digits at y 7a)
 * and its file digit plus one, as two digits, with its label at (8, 66). */
void menu_file_info_layout_play_time_and_number(SaveSummary *set) {
    s32 i;

    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xee, menu_state_current->file_info->colon0, menu_state_current->buffer_index, menu_file_info_play_time_x_table[0], 0x7a,
                  0x1000);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xee, menu_state_current->file_info->colon1, menu_state_current->buffer_index, menu_file_info_play_time_x_table[1], 0x7a,
                  0x1000);
    menu_split_play_time(set->time);
    for (i = 0; i < 7; i++) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->time[i], menu_state_current->file_info->timeDigits[i],
                      menu_state_current->buffer_index, menu_file_info_play_time_x_table[i + 2], 0x7a, 0x1000);
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x17, menu_state_current->file_info->discLabel, menu_state_current->buffer_index, 8, 0x66, 0x1000);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x32, menu_state_current->file_info->discMark, menu_state_current->buffer_index, 0x10, 0x66, 0x1000);
    sprite_sheet_draw_scaled(menu_state_current->sheet, (set->digit + 1) / 10, menu_state_current->file_info->discDigits[0],
                  menu_state_current->buffer_index, 0x10, 0x6e, 0x1000);
    sprite_sheet_draw_scaled(menu_state_current->sheet, (set->digit + 1) % 10, menu_state_current->file_info->discDigits[1],
                  menu_state_current->buffer_index, 0x18, 0x6e, 0x1000);
}

/* 801E6AE8: Set up view `index` of the block at +34c from its character's sheet image
 * (14e + id). */
void menu_save_view_layout_image(u8 index, SaveSummary *set) {
    sprite_sheet_draw_scaled(menu_state_current->sheet, set->ids[index] + 0x14e, &menu_state_current->file_info->views[index],
                  menu_state_current->buffer_index, menu_save_view_image_x_table[index], menu_save_view_image_y_table[index], 0x1000);
}

/* 801E6B70: Lay out view `index`'s level as up to three digit sprites, and reset its
 * second digit row for the +63 value. */
void menu_save_view_layout_level(u8 index, SaveSummary *set) {
    s32 i;
    u8 digit;

    menu_split_digits(set->level[index]);
    menu_state_current->file_info->views[index].levelCount = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[i + 6];
        if (digit != 0xff) {
            menu_state_current->file_info->views[index].levelCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->file_info->views[index].levelDigits[menu_state_current->file_info->views[index].levelCount],
                              menu_state_current->buffer_index, menu_save_view_level_x + index * 0x50 + i * 8, menu_save_view_level_y, 0x1000);
        }
    }
    menu_split_digits(set->level2[index]);
    menu_state_current->file_info->views[index].level2Count = 0;
}

/* 801E6CFC: Lay out view `index`'s HP and maximum HP as up to three digit sprites
 * each (the maximum packed without leading blanks). */
void menu_save_view_layout_hp(u8 index, SaveSummary *set) {
    s32 i;
    s32 drawn;
    u8 digit;

    menu_split_digits(set->hp[index]);
    menu_state_current->file_info->views[index].hpCount = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[i + 6];
        if (digit != 0xff) {
            menu_state_current->file_info->views[index].hpCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              menu_state_current->file_info->views[index].hpDigits[menu_state_current->file_info->views[index].hpCount],
                              menu_state_current->buffer_index, menu_save_view_hp_x + index * 0x50 + i * 8, menu_save_view_hp_y, 0x1000);
        }
    }
    drawn = 0;
    menu_split_digits(set->hpMax[index]);
    menu_state_current->file_info->views[index].hpMaxCount = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[i + 6];
        if (digit != 0xff) {
            menu_state_current->file_info->views[index].hpMaxCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              menu_state_current->file_info->views[index].hpMaxDigits[menu_state_current->file_info->views[index].hpMaxCount],
                              menu_state_current->buffer_index, menu_save_view_max_hp_x + index * 0x50 + drawn * 8, menu_save_view_max_hp_y, 0x1000);
            drawn++;
        }
    }
}

/* 801E6F5C: Lay out view `index`'s EP and maximum EP as up to two digit sprites each
 * (the maximum packed without leading blanks). */
void menu_save_view_layout_ep(u8 index, SaveSummary *set) {
    s32 i;
    s32 drawn;
    u8 digit;

    menu_split_digits(set->ep[index]);
    menu_state_current->file_info->views[index].epCount = 0;
    for (i = 0; i < 2; i++) {
        digit = menu_state_current->digits[i + 7];
        if (digit != 0xff) {
            menu_state_current->file_info->views[index].epCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              menu_state_current->file_info->views[index].epDigits[menu_state_current->file_info->views[index].epCount],
                              menu_state_current->buffer_index, menu_save_view_ep_x + index * 0x50 + i * 8, menu_save_view_ep_y, 0x1000);
        }
    }
    drawn = 0;
    menu_split_digits(set->epMax[index]);
    menu_state_current->file_info->views[index].epMaxCount = 0;
    for (i = 0; i < 2; i++) {
        digit = menu_state_current->digits[i + 7];
        if (digit != 0xff) {
            menu_state_current->file_info->views[index].epMaxCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              menu_state_current->file_info->views[index].epMaxDigits[menu_state_current->file_info->views[index].epMaxCount],
                              menu_state_current->buffer_index, menu_save_view_max_ep_x + index * 0x50 + drawn * 8, menu_save_view_max_ep_y, 0x1000);
            drawn++;
        }
    }
}

/* 801E71B4: Render the name of view `index`'s character from listed file `file`'s
 * name table (up to ten two-byte characters) and upload it to label row
 * `index` of the view rows. */
void menu_save_view_render_name(u8 index, SaveSummary *set, s32 file) {
    RECT rect;
    u8 name[24];
    u8 text[24];
    u8 *pixels;
    MenuSaveInfo *info;
    s32 i;

    info = (MenuSaveInfo *)(menu_state_current->card->heads[file] + 0x100);
    for (i = 0; i < 20; i += 2) {
        name[i] = info->names[set->ids[index]].text[i];
        name[i + 1] = info->names[set->ids[index]].text[i + 1];
        if (name[i] == 0 && name[i + 1] == 0) {
            break;
        }
    }
    text_decode_codes(name, text, i / 2);
    pixels = heap_alloc(0x3f6, 0);
    bzero(pixels, 0x3f6);
    window_render_text_line(text, pixels, 0x24, 0);
    rect.x = menu_name_image_vram_x_table[index + 6] + 0x180;
    rect.y = menu_name_image_vram_y_table[index + 6];
    rect.w = 0x28;
    rect.h = 0xd;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    heap_free(pixels);
}

/* 801E733C: Lay out the 16 character quads of the save title image (row f0 of the
 * 140 page, 12-pixel glyphs 16 apart) in the current buffer. */
void menu_save_title_layout_quads(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        menu_quad_init(&menu_state_current->file_info->title[i * 2 + menu_state_current->buffer_index]);
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->x0 = menu_save_title_x + i * 12;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->y0 = menu_save_title_y;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->x1 = menu_save_title_x + i * 12 + 12;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->y1 = menu_save_title_y;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->x2 = menu_save_title_x + i * 12;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->y2 = menu_save_title_y + 16;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->x3 = menu_save_title_x + i * 12 + 12;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->y3 = menu_save_title_y + 16;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->u0 = i * 16;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->v0 = 0xf0;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->u1 = i * 16 + 12;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->v1 = 0xf0;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->u2 = i * 16;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->v2 = 0xff;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->u3 = i * 16 + 12;
        (menu_state_current->file_info->title + (i * 2 + menu_state_current->buffer_index))->v3 = 0xff;
        menu_state_current->file_info->title[i * 2 + menu_state_current->buffer_index].tpage = GetTPage(0, 0, 0x140, 0x80);
        menu_state_current->file_info->title[i * 2 + menu_state_current->buffer_index].clut = GetClut(0, 0x1c0);
    }
}

/* 801E76EC: Build the three views of card file `index`'s save information. */
void menu_file_info_build_views(s32 index) {
    SaveSummary *set;
    s32 i;

    set = (SaveSummary *)(menu_state_current->card->heads[index] + 0x100);
    menu_file_info_layout_view_frames();
    for (i = 0; i < 3; i++) {
        if (set->ids[i] != 0xff) {
            menu_state_current->file_info->views[i].shown = 1;
            menu_save_view_layout_image(i, set);
            menu_save_view_layout_level(i, set);
            menu_save_view_layout_hp(i, set);
            menu_save_view_layout_ep(i, set);
            menu_save_view_render_name(i, set, index);
        } else {
            menu_state_current->file_info->views[i].shown = 0;
        }
        menu_state_current->file_info->views[i].buffer = menu_state_current->buffer_index;
    }
    menu_file_info_layout_play_time_and_number(set);
    menu_save_title_layout_quads();
}

/* 801E781C: Redraw view `index` (none for ff), rebuilding it first when `rebuild`. */
void menu_file_info_show(s32 index, u8 rebuild) {
    menu_file_info_clear();
    if (index != 0xff) {
        if (rebuild) {
            menu_file_info_build_views(index);
            menu_save_title_render(index);
            menu_state_current->file_info->rebuilt = 1;
        } else {
            menu_save_title_render(index);
            menu_state_current->file_info->rebuilt = 0;
        }
        menu_state_current->flags->file_info_shown = 1;
    }
}

/* 801E78C8: Upload listed file `file`'s save icon to its slot of the icon pages (the
 * palette to row 1c1 + port, the three 16x16 frames to rows 80, a0 and c0
 * plus port * 16, column 140 + file % 16 * 4), set its six animation steps
 * to those rows by the header's icon flag (11-13: one to three frames) and
 * add its block count to its port's total; a file with another flag gets
 * state 0. Every position is written as a `file / 16` expression, as in the
 * prologue. That shapes the switch: the first cse pass keeps each case's
 * first division (its block starts after the loop's end note), the second
 * merges it into the loop's but leaves the dead division to flow, and the
 * branch that remains until the jump pass after reload splits the case, so
 * each case's record offset is allocated globally (v1, a0, a2; the card
 * pointer a3). */
void menu_save_icon_upload(s32 file) {
    s32 i;
    u8 noIcon;

    noIcon = 1;
    menu_save_icon_image_rect.x = 0x140 + file * 4 - file / 16 * 64;
    menu_save_icon_image_rect.w = 4;
    menu_save_icon_image_rect.h = 0x10;
    menu_save_icon_palette_rect.x = file * 16;
    menu_save_icon_palette_rect.y = 0x1c1 + file / 16;
    menu_save_icon_palette_rect.w = 0x10;
    menu_save_icon_palette_rect.h = 1;
    memmove(menu_save_icon_palette, &menu_state_current->card->heads[file][0x60], 0x20);
    LoadImage(&menu_save_icon_palette_rect, (u_long *)menu_save_icon_palette);
    DrawSync(0);
    for (i = 0; i < 3; i++) {
        menu_save_icon_image_rect.y = 0x80 + i * 32 + file / 16 * 16;
        LoadImage(&menu_save_icon_image_rect, (u_long *)&menu_state_current->card->heads[file][0x80 + i * 0x80]);
        DrawSync(0);
    }
    switch (menu_state_current->card->heads[file][2]) {
    case 0x11:
        menu_state_current->card->files[file].frames[0] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[1] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[2] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[3] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[4] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[5] = 0x80 + file / 16 * 16;
        noIcon = 0;
        break;
    case 0x12:
        menu_state_current->card->files[file].frames[0] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[1] = 0xa0 + file / 16 * 16;
        menu_state_current->card->files[file].frames[2] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[3] = 0xa0 + file / 16 * 16;
        menu_state_current->card->files[file].frames[4] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[5] = 0xa0 + file / 16 * 16;
        noIcon = 0;
        break;
    case 0x13:
        menu_state_current->card->files[file].frames[0] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[1] = 0xa0 + file / 16 * 16;
        menu_state_current->card->files[file].frames[2] = 0xc0 + file / 16 * 16;
        menu_state_current->card->files[file].frames[3] = 0x80 + file / 16 * 16;
        menu_state_current->card->files[file].frames[4] = 0xa0 + file / 16 * 16;
        menu_state_current->card->files[file].frames[5] = 0xc0 + file / 16 * 16;
        noIcon = 0;
        break;
    }
    menu_card_blocks_used[file / 16] += menu_state_current->card->heads[file][3];
    if (noIcon) {
        menu_state_current->card->files[file].state = 0;
    }
}

/* Point the current quad at its CLUT (the highlighted text_plane1_clut or the plain
 * text_plane0_clut). A statement macro. */
#define SET_QUAD_CLUT()                                                \
    do {                                                               \
        POLY_FT4 *p = poly;                                            \
        p->clut = label->highlight != 0 ? text_plane1_clut : text_plane0_clut;      \
    } while (0)

/* Set up the current quad of `label`: mode 0 takes image `index` from the
 * 140 column pages (rows from `first`); otherwise from the 180 page (+80
 * keeps it opaque, else dimmed) with palette choice `mode & 7f` - 1. A
 * statement macro. */
#define SET_QUAD()                                                     \
    do {                                                               \
        semi = 0;                                                      \
        menu_quad_init(poly);                                           \
        if (mode == 0) {                                               \
            label->highlight = half;                                     \
            poly->tpage = GetTPage(0, 0, 0x140, 0);                    \
            setUVWH(poly, column, (index + first) / 4 * 0xd, label->width, 0xd); \
        } else {                                                       \
            if (!(mode & 0x80)) {                                      \
                semi = 0x20;                                           \
                SetSemiTrans(poly, 1);                                 \
                setRGB0(poly, semi, semi, semi);                       \
            }                                                          \
            label->highlight = (mode & 0x7f) - 1;                        \
            poly->tpage = semi | GetTPage(0, 0, 0x180, 0x80);          \
            setUVWH(poly, half * 0x60, row * 0xd + first, label->width, 0xd); \
        }                                                              \
        SET_QUAD_CLUT();                                               \
    } while (0)

/* Step to the next quad, back to `loop` for the second one. A statement
 * macro. */
#define NEXT_QUAD()                                                    \
    do {                                                               \
        i++;                                                           \
        poly++;                                                        \
        if (i < 2) {                                                   \
            goto loop;                                                 \
        }                                                              \
    } while (0)

/* 801E7C50: Set up `label`'s two quads for label image `index` (the quad loop is
 * not loop-optimized: each quad's palette and window are recomputed) and
 * hide the label. */
void menu_label_init_quads(MenuLabel *label, s32 index, s32 first, u8 mode) {
    POLY_FT4 *poly;
    s32 i;
    u16 semi;
    s32 half;
    s32 row;
    s32 column;

    i = 0;
    half = index & 1;
    row = index / 2;
    column = (row & 1) << 7;
    poly = label->polys;
loop:
    SET_QUAD();
    NEXT_QUAD();
    label->projected = 0;
}

/* 801E7E68: Render `count` labels (text ids in `layout`) in pairs into one 28x13
 * image each (two columns of 32 from x 140, rows of 13 from label `first`),
 * lay them out and upload the images. */
void menu_label_render_pairs(MenuLabel *labels, u8 *layout, s32 first, s32 count) {
    s32 i;
    RECT *image;

    for (i = 0; i < count; i += 2) {
        labels[i].width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, layout[i]), menu_state_current->labels[0].pixels,
                                        0x18, 0);
        image = &labels[i].rect;
        labels[i + 1].width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, layout[i + 1]), menu_state_current->labels[0].pixels,
                                        0x18, 1);
        labels[i].rect.x = (i / 2 & 1) * 0x20 + 0x140;
        labels[i].rect.y = (i + first) / 4 * 0xd;
        labels[i].rect.w = 0x1c;
        labels[i].rect.h = 0xd;
        labels[i + 1].rect = labels[i].rect;
        menu_label_init_quads(&labels[i], i, first, 0);
        menu_label_init_quads(&labels[i + 1], i + 1, first, 0);
        LoadImage(image, (u_long *)menu_state_current->labels[0].pixels);
        DrawSync(0);
    }
}

/* 801E8018: Lay out `count` labels from `table` into `labels` (the placement is unused). */
void menu_label_render_table(u8 count, MenuLabel *labels, u8 *table, u8 *flags) {
    menu_label_render_pairs(labels, table, 4, count);
}

/* 801E8044: Clear `count` label flags. */
void menu_label_clear_shown(u8 count, u8 *flags) {
    s32 i;

    for (i = 0; i < count; i++) {
        flags[i] = 0;
    }
}
