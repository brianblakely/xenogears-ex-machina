/*
 * The item shop's own screens: member stat views, the buy and sell lists,
 * prices and the sale. Rodata 801c5010-801c5040, text 801cce1c-801d1f50,
 * data 801d2210-801d2260 and the variable item_shop_held_count that ends the file. A
 * separate translation unit from the shared screen code before it: it calls
 * the party-bit helpers 801c50b0 and 801c50cc without a prototype in scope
 * (arguments and results unmasked), which the shared unit's own callers do
 * not.
 */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/text.h"
#include "menu/screen.h"
#include "menu/shop.h"
#include "menu/tables.h"
#include "item_shop.h"

/* The screen code's yes/no notice, which this unit also calls without a
 * prototype in scope: its u8 result is tested unmasked. */
s32 item_shop_notice_ask_yes_no();

/* The four heading sprites and the two alternative ones: ids and positions. */
u8 item_shop_buy_heading_images[4] = {0xF2, 0xDE, 0xE0, 0xE5}; /* 801D2210 */
u8 item_shop_sell_heading_images[2] = {0xF2, 0xE3}; /* 801D2214 */
s32 item_shop_buy_heading_x_table[4] = {150, 48, 48, 224}; /* 801D2218: x */
s32 item_shop_sell_heading_x_table[2] = {150, 224}; /* 801D2228 */
s32 item_shop_buy_heading_y_table[4] = {158, 190, 198, 88}; /* 801D2230: y */
s32 item_shop_sell_heading_y_table[2] = {158, 88}; /* 801D2240 */
/* The three nine-digit numbers' positions. */
s32 item_shop_gold_x = 232; /* 801D2248 */
s32 item_shop_gold_y = 78; /* 801D224C */
s32 item_shop_total_x = 232; /* 801D2250 */
s32 item_shop_total_y = 88; /* 801D2254 */
s32 item_shop_new_gold_x = 232; /* 801D2258 */
s32 item_shop_new_gold_y = 100; /* 801D225C */
/* Uninitialized: the file holds it as zeros in a four-byte slot. */
u16 item_shop_held_count; /* 801D2260 */

/* This unit passes quad coordinates as words; the shared screen unit sees
 * the helper's narrow definition before its local calls. */
void item_shop_set_rect_verts();

/* 801CCE1C: Fill a view's nine stat words from character `id`'s base and bonus
 * bytes, capped at 999 or 99. */
void item_shop_compute_character_stats(MenuTables *view, u8 id) {
    CharacterRecord *c;

    c = &game_data.characters[id];
    if (c->characterId == 4) {
        view->stats[0] = c->entries[0].value4 + c->entries[3].value4;
    } else {
        view->stats[0] = c->entries[0].value4 + (c->attack + c->equipAttack);
    }
    view->stats[0] = c->entries[0].value4 + (c->attack + c->equipAttack);
    view->stats[1] = c->field5E + c->equip5E;
    view->stats[2] = c->bodyDefense + (c->defense + c->equipDefense);
    view->stats[3] = c->field5F + c->equip5F;
    view->stats[4] = c->ether + c->equipEther;
    view->stats[5] = 99;
    view->stats[6] = c->etherDefense + c->equipEtherDefense;
    view->stats[7] = 10;
    view->stats[8] = c->speed + c->equipSpeed;
    if (view->stats[0] >= 1000) {
        view->stats[0] = 999;
    }
    if (view->stats[1] >= 100) {
        view->stats[1] = 99;
    }
    if (view->stats[2] >= 1000) {
        view->stats[2] = 999;
    }
    if (view->stats[3] >= 100) {
        view->stats[3] = 99;
    }
    if (view->stats[4] >= 1000) {
        view->stats[4] = 999;
    }
    if (view->stats[5] >= 100) {
        view->stats[5] = 99;
    }
    if (view->stats[6] >= 1000) {
        view->stats[6] = 999;
    }
    if (view->stats[7] >= 100) {
        view->stats[7] = 99;
    }
    if (view->stats[8] >= 100) {
        view->stats[8] = 99;
    }
}

/* 801CCFF4: Draw the shop's detail packets: member bars, portraits, headings, list
 * rows, labels and numbers. */
void item_shop_draw_details(void) {
    s32 i;

    if (menu_state_current->flags->unknown5a[0] != 0) {
        for (i = 0; i < 9; i++) {
            if (menu_state_current->details->bar_shown[i] != 0) {
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->details->bar_upper[i * 2 + menu_state_current->buffer_index]);
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->details->bar_lower[i * 2 + menu_state_current->buffer_index]);
            }
        }
        item_shop_draw_quads(menu_state_current->details->heading_count, menu_state_current->details->heading,
                      menu_state_current->details->heading_buffer);
        item_shop_draw_quads(menu_state_current->details->group2D0_count, menu_state_current->details->group2D0,
                      menu_state_current->details->group2D0_buffer);
        item_shop_draw_quads(menu_state_current->details->members_count, menu_state_current->details->members,
                      menu_state_current->details->members_buffer);
        for (i = 0; i < 8; i++) {
            if (menu_state_current->details->name_shown[i] != 0) {
                item_shop_draw_projected_quads(1, menu_state_current->details->names_a[i].verts, menu_state_current->details->names_a[i].polys,
                              menu_state_current->details->names_a[i].buffer);
                item_shop_draw_projected_quads(1, menu_state_current->details->names_b[i].verts, menu_state_current->details->names_b[i].polys,
                              menu_state_current->details->names_b[i].buffer);
            }
        }
        if (menu_state_current->details->label4430_shown != 0) {
            item_shop_draw_projected_quads(1, menu_state_current->details->label4430.verts, menu_state_current->details->label4430.polys,
                          menu_state_current->details->label4430.buffer);
        }
        if (menu_state_current->details->label44B0_shown != 0) {
            item_shop_draw_projected_quads(1, menu_state_current->details->label44B0.verts, menu_state_current->details->label44B0.polys,
                          menu_state_current->details->label44B0.buffer);
        }
        if (menu_state_current->details->label45B0_shown != 0) {
            item_shop_draw_projected_quads(1, menu_state_current->details->label45B0.verts, menu_state_current->details->label45B0.polys,
                          menu_state_current->details->label45B0.buffer);
        }
        if (menu_state_current->details->digits_shown != 0) {
            AddPrim(&menu_state_current->current->ot[4], &menu_state_current->details->frame[menu_state_current->buffer_index]);
            item_shop_draw_quads(menu_state_current->details->digits1_count, menu_state_current->details->digits1,
                          menu_state_current->details->digits1_buffer);
            item_shop_draw_quads(menu_state_current->details->digits2_count, menu_state_current->details->digits2,
                          menu_state_current->details->digits2_buffer);
            item_shop_draw_quads(menu_state_current->details->digits3_count, menu_state_current->details->digits3,
                          menu_state_current->details->digits3_buffer);
        }
        for (i = 0; i < 8; i++) {
            item_shop_draw_quads(menu_state_current->details->row_count[i], menu_state_current->details->rows[i],
                          menu_state_current->details->row_buffer[i]);
        }
        for (i = 0; i < 9; i++) {
            item_shop_draw_quads(menu_state_current->details->cells_a_count[i], menu_state_current->details->cells_a[i],
                          menu_state_current->details->cells_a_buffer[i]);
            item_shop_draw_quads(menu_state_current->details->cells_b_count[i], menu_state_current->details->cells_b[i],
                          menu_state_current->details->cells_b_buffer[i]);
        }
    }
    if (menu_state_current->flags->unknown5a[1] == 1) {
        item_shop_draw_quads(menu_state_current->details->group1220_count, menu_state_current->details->group1220,
                      menu_state_current->details->group1220_buffer);
    }
}

/* 801CD404: Tint `count` packet pairs of this buffer red (0) or blue (1). */
void item_shop_tint_quads(s32 count, POLY_FT4 *packets, u8 color) {
    s32 i;

    for (i = 0; i < count; i++) {
        SetShadeTex(&packets[i * 2 + menu_state_current->buffer_index], 0);
        switch (color) {
        case 0:
            (packets + (i * 2 + menu_state_current->buffer_index))->r0 = 0x80;
            (packets + (i * 2 + menu_state_current->buffer_index))->g0 = 0x40;
            (packets + (i * 2 + menu_state_current->buffer_index))->b0 = 0x40;
            break;
        case 1:
            (packets + (i * 2 + menu_state_current->buffer_index))->r0 = 0x40;
            (packets + (i * 2 + menu_state_current->buffer_index))->g0 = 0x40;
            (packets + (i * 2 + menu_state_current->buffer_index))->b0 = 0x80;
            break;
        }
    }
}

/* 801CD5D0: Reveal the available party members' portraits one member per frame. */
void item_shop_reveal_portraits(void) {
    s32 step;
    s32 shown;
    s32 i;

    menu_state_current->flags->unknown5a[0] = 1;
    for (step = 1; step < 12; step++) {
        shown = 0;
        menu_state_current->details->members_count = 0;
        for (i = 0; i < step; i++) {
            if (menu_state_current->present[i] != 0) {
                menu_state_current->details->members_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, i + 0x14E, &menu_state_current->details->members[shown * 2],
                                  menu_state_current->buffer_index, item_shop_portrait_x_table[shown], 0xA6, 0x1000);
                shown++;
            }
        }
        menu_state_current->details->members_buffer = menu_state_current->buffer_index;
        item_shop_run_frame();
    }
}

/* 801CD6F8: Draw the four heading sprites. */
void item_shop_layout_buy_headings(void) {
    s32 i;

    menu_state_current->details->heading_count = 0;
    for (i = 0; i < 4; i++) {
        menu_state_current->details->heading_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, item_shop_buy_heading_images[i],
                          menu_state_current->details->heading + menu_state_current->details->heading_count * 2,
                          menu_state_current->buffer_index, item_shop_buy_heading_x_table[i], item_shop_buy_heading_y_table[i], 0x1000);
    }
    menu_state_current->details->heading_buffer = menu_state_current->buffer_index;
}

/* 801CD7E4: Draw the two alternative heading sprites. */
void item_shop_layout_sell_headings(void) {
    s32 i;

    menu_state_current->details->heading_count = 0;
    for (i = 0; i < 2; i++) {
        menu_state_current->details->heading_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, item_shop_sell_heading_images[i],
                          menu_state_current->details->heading + menu_state_current->details->heading_count * 2,
                          menu_state_current->buffer_index, item_shop_sell_heading_x_table[i], item_shop_sell_heading_y_table[i], 0x1000);
    }
    menu_state_current->details->heading_buffer = menu_state_current->buffer_index;
}

/* 801CD8D0: Draw three nine-digit numbers (leading zeros blank) at their three positions. */
void item_shop_layout_gold_numbers(u32 first, u32 second, u32 third) {
    s32 i;

    item_shop_split_digits(first);
    menu_state_current->details->digits1_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->digits1_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              menu_state_current->details->digits1 + menu_state_current->details->digits1_count * 2,
                              menu_state_current->buffer_index, i * 8 + item_shop_gold_x, item_shop_gold_y, 0x1000);
        }
    }
    menu_state_current->details->digits1_buffer = menu_state_current->buffer_index;
    item_shop_split_digits(second);
    menu_state_current->details->digits2_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->digits2_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              menu_state_current->details->digits2 + menu_state_current->details->digits2_count * 2,
                              menu_state_current->buffer_index, i * 8 + item_shop_total_x, item_shop_total_y, 0x1000);
        }
    }
    menu_state_current->details->digits2_buffer = menu_state_current->buffer_index;
    item_shop_split_digits(third);
    menu_state_current->details->digits3_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->digits3_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              menu_state_current->details->digits3 + menu_state_current->details->digits3_count * 2,
                              menu_state_current->buffer_index, i * 8 + item_shop_new_gold_x, item_shop_new_gold_y, 0x1000);
        }
    }
    menu_state_current->details->digits3_buffer = menu_state_current->buffer_index;
    menu_state_current->details->digits_shown = 1;
}

/* 801CDBA0: Party bits of the available members holding item `item` in their gear
 * (kind 0) or accessories (kind 1). */
u16 item_shop_find_item_holders(u8 item, u8 kind) {
    u16 members;
    u8 found;
    s32 i;
    s32 k;

    members = 0;
    if (item != 0 && kind != 2) {
        for (i = 0; i < 16; i++) {
            found = 0;
            if (menu_state_current->present[i] != 0) {
                switch (kind) {
                case 0:
                    for (k = 0; k < 5; k++) {
                        if (item < 0x32) {
                            if (game_data.characters[i].weapons[k] == item) {
                                found = 1;
                                break;
                            }
                        } else if (game_data.characters[i].entryItems[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                case 1:
                    for (k = 0; k < 3; k++) {
                        if (game_data.characters[i].accessories[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                }
            }
            if (found) {
                members |= item_shop_get_bit_mask(i);
            }
        }
    }
    return members;
}

/* 801CDD14
 * Draw the eight visible rows of the shop's stock from entry `top`: each
 * item's name and price (dimmed, `dims[row]` 0, when it costs more than
 * `gold`) and, when some are chosen, "x" and the amount.
 */
void item_shop_layout_stock_rows(s32 top, s32 gold, u8 *dims) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    s32 price;
    u8 *pixels;
    s32 value;
    s32 digit;
    u8 started;
    u8 tens;
    s32 row;
    s32 i;
    s32 j;

    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = heap_alloc(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        dims[row] = 0;
        bzero(codes, 14);
        menu_state_current->details->row_count[row] = 0;
        if (menu_state_current->shop_items[top + row] != 0) {
            switch (menu_state_current->shop_kinds[top + row]) {
            case 0:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_weapon_name(menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                value = menu_state_current->tables->equipment[menu_state_current->shop_items[top + row]].price;
                price = value;
                break;
            case 1:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_accessory_name(menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                value = menu_state_current->tables->accessories[menu_state_current->shop_items[top + row]].price;
                price = value;
                break;
            case 2:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_item_name(menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                value = menu_state_current->tables->items[menu_state_current->shop_items[top + row]].price;
                price = value;
                break;
            }
            if (gold >= price) {
                dims[row] = 0x80;
            } else {
                dims[row] = 0;
            }
            started = 0;
            for (i = 0, j = 4; j > 0; i++, j--) {
                digit = value / divisors[j];
                if (digit != 0 || started) {
                    codes[i * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * divisors[j];
                } else {
                    codes[i * 2] = 0xC3;
                }
            }
            codes[i * 2] = value % 10 + 0x10;
            text_decode_codes(codes, text, 5);
            menu_state_current->details->names_b[row].width = window_render_text_line(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            item_shop_label_init_quads(&menu_state_current->details->names_a[row], row, 0x80, dims[row] + 1);
            item_shop_set_rect_verts(menu_state_current->details->names_a[row].verts, 0x24, row * 13 + 0x32,
                          menu_state_current->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            DrawSync(0);
            item_shop_label_init_quads(&menu_state_current->details->names_b[row], row, 0x80, dims[row] + 2);
            item_shop_set_rect_verts(menu_state_current->details->names_b[row].verts, 0x8C, row * 13 + 0x32,
                          menu_state_current->details->names_b[row].width, 13);
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->name_shown[row] = 1;
            if (menu_state_current->details->amounts[top + row] != 0) {
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xF1, menu_state_current->details->rows[row],
                                  menu_state_current->buffer_index, 0xB4, row * 13 + 0x36, 0x1000);
                tens = menu_state_current->details->amounts[top + row] / 10;
                if (tens != 0) {
                    menu_state_current->details->row_count[row] +=
                        sprite_sheet_draw_scaled(menu_state_current->sheet, tens,
                                      &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                      menu_state_current->buffer_index, 0xBC, row * 13 + 0x36, 0x1000);
                }
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, (u8)(menu_state_current->details->amounts[top + row] % 10),
                                  &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                  menu_state_current->buffer_index, 0xC4, row * 13 + 0x36, 0x1000);
                menu_state_current->details->row_buffer[row] = menu_state_current->buffer_index;
            }
        } else {
            menu_state_current->details->name_shown[row] = 0;
        }
    }
    heap_free(pixels);
}

/* 801CE480
 * Compare member `member`'s attack (kind 0) or defence (kind 1) with item
 * `id` equipped: `diffs` gets the two differences and `worse` whether each
 * would drop. Member 4 equips four armour slots from the equipment table;
 * accessories of one group replace each other, otherwise the weakest is
 * replaced.
 */
void item_shop_compare_equipped_stats(s32 *diffs, u8 *worse, u8 id, u8 kind, u8 member) {
    s16 after[2];
    s16 before[2];
    EquipInfo *current;
    EquipInfo *fitted;
    AccessoryInfo *accessory;
    AccessoryInfo *fitted_accessory;
    u8 lowest;
    u8 replace;
    s32 weakest;
    s32 k;

    before[1] = 0;
    before[0] = 0;
    after[1] = 0;
    after[0] = 0;
    switch (kind) {
    case 0:
        if (member != 4) {
            current = &menu_state_current->tables->equipment[game_data.characters[member].weapons[0]];
            before[0] = current->power + game_data.characters[member].attack;
            current = &menu_state_current->tables->equipment[id];
            after[0] = current->power + game_data.characters[member].attack;
        } else {
            for (k = 0; k < 4; k++) {
                current = &menu_state_current->tables->equipment[game_data.characters[member].entryItems[k]];
                before[0] += current->power;
            }
            fitted = &menu_state_current->tables->equipment[id];
            for (k = 0; k < 4; k++) {
                current = &menu_state_current->tables->equipment[game_data.characters[member].entryItems[k]];
                if (current->kind == fitted->kind && current->kind != 5) {
                    after[0] += fitted->power;
                } else {
                    after[0] += current->power;
                }
            }
        }
        break;
    case 1:
        replace = 1;
        before[1] = after[1] = game_data.characters[member].defense;
        for (k = 0; k < 3; k++) {
            accessory = &menu_state_current->tables->accessories[game_data.characters[member].accessories[k]];
            before[1] += accessory->amount;
        }
        fitted_accessory = &menu_state_current->tables->accessories[id];
        for (k = 0; k < 3; k++) {
            accessory = &menu_state_current->tables->accessories[game_data.characters[member].accessories[k]];
            if (accessory->groups != 0 && accessory->groups == fitted_accessory->groups) {
                after[1] += fitted_accessory->amount;
                replace = 0;
            } else {
                after[1] += accessory->amount;
            }
        }
        if (replace) {
            lowest = 0xFF;
            for (k = 0; k < 3; k++) {
                accessory = &menu_state_current->tables->accessories[game_data.characters[member].accessories[k]];
                if (lowest >= accessory->amount) {
                    lowest = accessory->amount;
                    weakest = k;
                }
            }
            after[1] = game_data.characters[member].defense + fitted_accessory->amount;
            for (k = 0; k < 3; k++) {
                if (k != weakest) {
                    accessory = &menu_state_current->tables->accessories[game_data.characters[member].accessories[k]];
                    after[1] += accessory->amount;
                }
            }
        }
        break;
    }
    for (k = 0; k < 2; k++) {
        if (after[k] >= before[k]) {
            diffs[k] = after[k] - before[k];
            worse[k] = 0;
        } else {
            diffs[k] = before[k] - after[k];
            worse[k] = 1;
        }
    }
}

/* 801CE8D8: The count held of item `id` in an inventory of `n` ids and counts (0 if absent). */
u16 item_shop_find_inventory_count(u8 *ids, u8 *counts, s32 n, u8 id) {
    u8 count;
    s32 i;

    count = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] == id) {
            count = counts[i];
            break;
        }
    }
    return count;
}

/* 801CE91C: Show how many of item `id` the party holds in inventory `kind` (label
 * beside the list). */
void item_shop_show_held_count(u8 kind, u8 id) {
    RECT rect;
    u8 codes[4];
    u8 text[8];
    u8 *ids;
    u8 *counts;
    s32 n;
    u8 *pixels;
    u16 count;

    switch (kind) {
    case 0:
        ids = game_data.weaponIds;
        counts = ids - 100;
        n = 100;
        break;
    case 1:
        ids = game_data.accessoryIds;
        counts = ids - 200;
        n = 200;
        break;
    case 2:
        ids = game_data.itemIds;
        counts = ids - 150;
        n = 150;
        break;
    }
    count = item_shop_find_inventory_count(ids, counts, n, id);
    item_shop_held_count = count;
    pixels = heap_alloc(0x3F6, 0);
    codes[1] = 0;
    codes[3] = 0;
    if (count / 10) {
        codes[0] = count / 10 + 0x10;
    } else {
        codes[0] = 0xC3;
    }
    codes[2] = count % 10 + 0x10;
    text_decode_codes(codes, text, 2);
    menu_state_current->details->label45B0.width = window_render_text_line(text, pixels, 0x24, 1);
    rect.x = 0x198;
    rect.y = 0xB4;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    item_shop_label_init_quads(&menu_state_current->details->label45B0, 9, 0x80, 0x82);
    item_shop_set_rect_verts(menu_state_current->details->label45B0.verts, 0xF8, 0x8E, menu_state_current->details->label45B0.width,
                  13);
    menu_state_current->details->label45B0.buffer = menu_state_current->buffer_index;
    menu_state_current->details->label45B0_shown = 1;
    heap_free(pixels);
}

/* 801CEB3C
 * Show stock entry `top + row` (`dims` unused): its name label, the bars of the members who
 * can equip it, the marks of those holding it and, for each member who can,
 * the attack and defence change (tinted by whether it drops), then how many
 * the party holds. Returns its price.
 */
u32 item_shop_show_stock_entry(s32 row, s32 top, u8 *dims) {
    RECT rect;
    s32 diffs[2];
    u8 worse[2];
    u32 price;
    u16 holders;
    u16 users;
    u8 id;
    u8 kind;
    u8 *pixels;
    s32 i;
    s32 shown;
    s32 k;
    s32 xa;
    s32 xb;

    users = 0;
    id = menu_state_current->shop_items[top + row];
    kind = menu_state_current->shop_kinds[top + row];
    pixels = heap_alloc(0x618, 0);
    bzero(pixels, 0x618);
    switch (kind) {
    case 0:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[1], id), pixels, 0x39, 0);
        price = menu_state_current->tables->equipment[id].price;
        users = menu_state_current->tables->equipment[id].users;
        break;
    case 1:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[2], id), pixels, 0x39, 0);
        price = menu_state_current->tables->accessories[id].price;
        users = menu_state_current->tables->accessories[id].users;
        break;
    case 2:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[0], id), pixels, 0x39, 0);
        price = menu_state_current->tables->items[id].price;
        break;
    }
    holders = item_shop_find_item_holders(id, kind);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    item_shop_label_init_quads(&menu_state_current->details->label4430, 0, 0, 0);
    item_shop_quad_place(&menu_state_current->details->label4430.polys[menu_state_current->buffer_index], 0x2C, 0x12, 0, 0x4E,
                  menu_state_current->details->label4430.width, 13);
    item_shop_set_rect_verts(menu_state_current->details->label4430.verts, 0x2C, 0x12, menu_state_current->details->label4430.width, 13);
    menu_state_current->details->label4430.buffer = menu_state_current->buffer_index;
    heap_free(pixels);
    if (id) {
        menu_state_current->details->label4430_shown = 1;
    } else {
        menu_state_current->details->label4430_shown = 0;
    }
    i = 0;
    shown = 0;
    menu_state_current->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (menu_state_current->present[i] != 0) {
            if (item_shop_test_bit(users, i)) {
                menu_state_current->details->bar_shown[shown] = 1;
            } else {
                menu_state_current->details->bar_shown[shown] = 0;
            }
            if (item_shop_test_bit(holders, i)) {
                menu_state_current->details->group2D0_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE,
                                  &menu_state_current->details->group2D0[menu_state_current->details->group2D0_count * 2],
                                  menu_state_current->buffer_index, item_shop_portrait_x_table[shown] + 0xE, 0xB4, 0x1000);
            }
            menu_state_current->details->cells_a_count[shown] = 0;
            menu_state_current->details->cells_b_count[shown] = 0;
            if (menu_state_current->details->bar_shown[shown] != 0) {
                diffs[1] = 0;
                diffs[0] = 0;
                item_shop_compare_equipped_stats(diffs, worse, id, kind, i);
                if (diffs[0] != 0) {
                    item_shop_split_digits(diffs[0]);
                    for (k = 0, xa = shown * 26 + 0x49; k < 3; k++) {
                        if (menu_state_current->digits[k + 6] != 0xFF) {
                            menu_state_current->details->cells_a_count[shown] += sprite_sheet_draw_scaled(
                                menu_state_current->sheet, menu_state_current->digits[k + 6],
                                &menu_state_current->details->cells_a[shown][menu_state_current->details->cells_a_count[shown] * 2],
                                menu_state_current->buffer_index, xa + k * 8, 0xBE, 0x1000);
                        }
                    }
                    item_shop_tint_quads(menu_state_current->details->cells_a_count[shown], menu_state_current->details->cells_a[shown],
                                  worse[0]);
                    menu_state_current->details->cells_a_buffer[shown] = menu_state_current->buffer_index;
                }
                if (diffs[1] != 0) {
                    item_shop_split_digits(diffs[1]);
                    for (k = 0, xb = shown * 26 + 0x49; k < 3; k++) {
                        if (menu_state_current->digits[k + 6] != 0xFF) {
                            menu_state_current->details->cells_b_count[shown] += sprite_sheet_draw_scaled(
                                menu_state_current->sheet, menu_state_current->digits[k + 6],
                                &menu_state_current->details->cells_b[shown][menu_state_current->details->cells_b_count[shown] * 2],
                                menu_state_current->buffer_index, xb + k * 8, 0xC6, 0x1000);
                        }
                    }
                    item_shop_tint_quads(menu_state_current->details->cells_b_count[shown], menu_state_current->details->cells_b[shown],
                                  worse[1]);
                    menu_state_current->details->cells_b_buffer[shown] = menu_state_current->buffer_index;
                }
            }
            shown++;
        }
    }
    menu_state_current->details->group2D0_buffer = menu_state_current->buffer_index;
    item_shop_show_held_count(kind, id);
    return price;
}

/* 801CF2A0
 * Set the party's gold (capped at 9999999) and put the bought amounts into
 * the inventories: added to an item already held (at most 99), otherwise
 * into the first free slot.
 */
void item_shop_settle_purchase(u32 gold) {
    u32 *party_gold;
    s32 i;
    s32 j;
    u8 new_item;

    item_shop_play_sound(0xD1);
    party_gold = &game_data.gold;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    for (i = 0; i < 0x30; i++) {
        if (menu_state_current->shop_items[i] != 0 && menu_state_current->details->amounts[i] != 0) {
            switch (menu_state_current->shop_kinds[i]) {
            case 0:
                new_item = 1;
                for (j = 0; j < 100; j++) {
                    if (game_data.weaponIds[j] == menu_state_current->shop_items[i]) {
                        new_item = 0;
                        if ((game_data.weaponCounts[j] += menu_state_current->details->amounts[i]) >= 100) {
                            game_data.weaponCounts[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 100; j++) {
                        if (game_data.weaponIds[j] == 0) {
                            game_data.weaponIds[j] = menu_state_current->shop_items[i];
                            game_data.weaponCounts[j] = menu_state_current->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            case 1:
                new_item = 1;
                for (j = 0; j < 200; j++) {
                    if (game_data.accessoryIds[j] == menu_state_current->shop_items[i]) {
                        new_item = 0;
                        if ((game_data.accessoryCounts[j] += menu_state_current->details->amounts[i]) >= 100) {
                            game_data.accessoryCounts[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 200; j++) {
                        if (game_data.accessoryIds[j] == 0) {
                            game_data.accessoryIds[j] = menu_state_current->shop_items[i];
                            game_data.accessoryCounts[j] = menu_state_current->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            case 2:
                new_item = 1;
                for (j = 0; j < 150; j++) {
                    if (game_data.itemIds[j] == menu_state_current->shop_items[i]) {
                        new_item = 0;
                        if ((game_data.itemCounts[j] += menu_state_current->details->amounts[i]) >= 100) {
                            game_data.itemCounts[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 150; j++) {
                        if (game_data.itemIds[j] == 0) {
                            game_data.itemIds[j] = menu_state_current->shop_items[i];
                            game_data.itemCounts[j] = menu_state_current->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

/* 801CF678: Draw a nine-digit number (the party's gold) at (6bh, 54h). */
void item_shop_layout_notice_total(u32 value) {
    s32 i;
    s32 x;

    item_shop_split_digits(value);
    i = 0;
    x = 0x6B;
    menu_state_current->details->group1220_count = 0;
    for (; i < 9; i++, x += 8) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->group1220_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              menu_state_current->details->group1220 + menu_state_current->details->group1220_count * 2,
                              menu_state_current->buffer_index, x, 0x54, 0x1000);
        }
    }
    menu_state_current->details->group1220_buffer = menu_state_current->buffer_index;
    menu_state_current->flags->unknown5a[1] = 2;
}

/* 801CF780
 * Command 2 (buy): choose amounts of the shop's stock, eight rows at a time,
 * with the running total and the gold left; confirming settles the
 * purchase. Returns 1 (the command screen is redrawn).
 */
u8 item_shop_buy_command_run(void) {
    u8 dims[8];
    u8 first;
    u8 running;
    u8 redraw;
    s32 row;
    s32 last_row;
    s32 top;
    s32 last_top;
    u32 total;
    u32 gold;
    u32 new_gold;
    u32 price;
    s32 i;
    s32 held_next;

    gold = game_data.gold;
    first = 1;
    running = 1;
    redraw = 1;
    row = 0;
    last_row = 0xFF;
    top = 0;
    last_top = 0xFF;
    total = 0;
    new_gold = gold;
    for (i = 0; i < 11; i++) {
        item_shop_compute_character_stats(menu_state_current->tables, i);
        menu_state_current->details->attack[i] = menu_state_current->tables->stats[0];
        menu_state_current->details->defense[i] = menu_state_current->tables->stats[2];
    }
    bzero(menu_state_current->details->amounts, 0x30);
    menu_state_current->images->dim = 1;
    item_shop_list_cursor_alloc(0);
    while (running) {
        item_shop_run_frame();
        if (top != last_top || redraw) {
            item_shop_layout_stock_rows(top, new_gold, dims);
            item_shop_scroll_bar_show(0xC, 0x32, 0x3C, item_shop_stock_count, top);
        }
        if (row != last_row || top != last_top) {
            price = item_shop_show_stock_entry(row, top, dims);
            last_row = row;
            last_top = top;
            menu_state_current->flags->unknown5a[0] = 1;
        }
        item_shop_list_cursor_place(row, top, 0, 0);
        if (first) {
            item_shop_label_render_or_clear_shown(1, 2, menu_state_current->list_labels, item_shop_buy_label_ids, menu_state_current->flags->list_labels_shown);
            item_shop_label_place(2, menu_state_current->list_labels, item_shop_buy_label_ids, item_shop_sell_label_x_offsets, menu_state_current->flags->list_labels_shown, 0,
                          0, 1);
            menu_state_current->flags->list_labels_shown[0] = 0;
            item_shop_panel_open(2, 0xC, 0x2A, 0xC4, 0x74, 0, 1, 4, 1);
            item_shop_panel_open(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            item_shop_panel_open(5, 0xE0, 0x7A, 0x40, 0x24, 0, 1, 4, 0);
            item_shop_view_start_zoom_in();
            item_shop_reveal_portraits();
            item_shop_layout_buy_headings();
            first = 0;
            while (menu_state_current->view_motion != 0) {
                item_shop_run_frame();
            }
            menu_state_current->flags->list_labels_shown[0] = 1;
        }
        if (redraw) {
            item_shop_layout_gold_numbers(gold, total, new_gold);
            redraw = 0;
        }
        switch (menu_state_current->input) {
        case 4:
            if (total != 0) {
                item_shop_play_sound(2);
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->panels_shown[5] = 0;
                menu_state_current->flags->scroll_shown = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                running = 0;
                menu_state_current->flags->list_labels_shown[0] = 0;
                item_shop_markers_open(0);
                item_shop_layout_notice_total(total);
                if (item_shop_notice_ask_yes_no(0x8F, 0xFF, 1)) {
                    item_shop_settle_purchase(new_gold);
                } else {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->panels_shown[5] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    menu_state_current->flags->cursors_shown[0] = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->list_labels_shown[0] = 1;
                }
                item_shop_markers_close();
            } else {
                item_shop_play_sound(4);
            }
            break;
        case 5:
            running = 0;
            menu_state_current->flags->list_labels_shown[0] = 0;
            if (total != 0) {
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->panels_shown[5] = 0;
                menu_state_current->flags->scroll_shown = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                item_shop_markers_open(0);
                if (!item_shop_notice_ask_yes_no(0x8C, 0xFF, 1)) {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->panels_shown[5] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    menu_state_current->flags->cursors_shown[0] = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->list_labels_shown[0] = 1;
                }
                item_shop_markers_close();
            }
            item_shop_run_frame();
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                top++;
                if (item_shop_stock_count - 8 < top) {
                    top--;
                }
            }
            break;
        case 3:
            row--;
            if (row < 0) {
                top--;
                row = 0;
                if (top < 0) {
                    top = 0;
                }
            }
            break;
        case 0:
            if (dims[row] != 0 && menu_state_current->details->amounts[top + row] + (held_next = item_shop_held_count + 1) < 100) {
                total += price;
                new_gold -= price;
                menu_state_current->details->amounts[top + row] += 1;
                redraw = 1;
            }
            break;
        case 2:
            if (menu_state_current->details->amounts[top + row] != 0) {
                total -= price;
                new_gold += price;
                menu_state_current->details->amounts[top + row] -= 1;
                redraw = 1;
            }
            break;
        }
    }
    menu_state_current->details->label45B0_shown = 0;
    return 1;
}

/* 801CFF58
 * Show item `id` of kind `kind` (0 equipment, 1 accessory, 2 item): its name and
 * sell price (half the table price) labels, the bars of the members who can
 * equip it and the marks of those holding it. Returns the sell price.
 */
u32 item_shop_show_sell_entry(u8 id, u8 kind) {
    RECT rect;
    u8 unused[16]; /* unused in the original; reserves 16 bytes */
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u8 *pixels;
    u16 users;
    u32 price;
    s32 value;
    u16 holders;
    s32 digit;
    u8 started;
    s32 j;
    s32 i;

    users = 0;
    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = heap_alloc(0x618, 0);
    bzero(pixels, 0x618);
    bzero(codes, 14);
    switch (kind) {
    case 0:
        menu_state_current->details->label4430.width = window_render_text_line(text_get_weapon_name(id), pixels, 0x39, 0);
        users = menu_state_current->tables->equipment[id].users;
        price = menu_state_current->tables->equipment[id].price >> 1;
        value = price;
        break;
    case 1:
        menu_state_current->details->label4430.width = window_render_text_line(text_get_accessory_name(id), pixels, 0x39, 0);
        users = menu_state_current->tables->accessories[id].users;
        price = menu_state_current->tables->accessories[id].price >> 1;
        value = price;
        break;
    case 2:
        menu_state_current->details->label4430.width = window_render_text_line(text_get_item_name(id), pixels, 0x39, 0);
        price = menu_state_current->tables->items[id].price >> 1;
        value = price;
        break;
    }
    holders = item_shop_find_item_holders(id, kind);
    started = 0;
    for (i = 0, j = 4; j > 0; i++, j--) {
        digit = value / divisors[j];
        if (digit != 0 || started) {
            codes[i * 2] = digit + 0x10;
            started = 1;
            value -= digit * divisors[j];
        } else {
            codes[i * 2] = 0xC3;
        }
    }
    codes[8] = value % 10 + 0x10;
    text_decode_codes(codes, text, 5);
    menu_state_current->details->label44B0.width = window_render_text_line(text, pixels, 0x39, 1);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    item_shop_label_init_quads(&menu_state_current->details->label4430, 0, 0, 0);
    item_shop_label_init_quads(&menu_state_current->details->label44B0, 0, 0, 0);
    menu_state_current->details->label44B0.polys[menu_state_current->buffer_index].clut = text_plane1_clut;
    item_shop_quad_place(&menu_state_current->details->label4430.polys[menu_state_current->buffer_index], 0x2C, 0x12, 0, 0x4E,
                  menu_state_current->details->label4430.width, 13);
    item_shop_quad_place(&menu_state_current->details->label44B0.polys[menu_state_current->buffer_index], 0x98, 0x12, 0, 0x4E,
                  menu_state_current->details->label44B0.width, 13);
    item_shop_set_rect_verts(menu_state_current->details->label4430.verts, 0x2C, 0x12, menu_state_current->details->label4430.width, 13);
    item_shop_set_rect_verts(menu_state_current->details->label44B0.verts, 0x98, 0x12, menu_state_current->details->label44B0.width, 13);
    menu_state_current->details->label4430.buffer = menu_state_current->buffer_index;
    menu_state_current->details->label44B0.buffer = menu_state_current->buffer_index;
    heap_free(pixels);
    if (id) {
        menu_state_current->details->label4430_shown = 1;
        menu_state_current->details->label44B0_shown = 1;
    } else {
        menu_state_current->details->label4430_shown = 0;
        menu_state_current->details->label44B0_shown = 0;
    }
    i = 0;
    j = 0;
    menu_state_current->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (menu_state_current->present[i] != 0) {
            if (item_shop_test_bit(users, i)) {
                menu_state_current->details->bar_shown[j] = 1;
            } else {
                menu_state_current->details->bar_shown[j] = 0;
            }
            if (item_shop_test_bit(holders, i)) {
                menu_state_current->details->group2D0_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE,
                                  &menu_state_current->details->group2D0[menu_state_current->details->group2D0_count * 2],
                                  menu_state_current->buffer_index, item_shop_portrait_x_table[j] + 0xE, 0xB4, 0x1000);
            }
            j++;
        }
    }
    menu_state_current->details->group2D0_buffer = menu_state_current->buffer_index;
    return price;
}

/* 801D05BC
 * Draw the eight visible rows of a list from entry `top`: each item's name,
 * the count held and, when some are chosen, "x" and the chosen count.
 */
void item_shop_layout_sell_rows(s32 top, u8 *ids, u8 *kinds, u8 *chosen, u8 *held) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u8 *pixels;
    s32 value;
    s32 digit;
    u8 started;
    u8 tens;
    s32 row;
    s32 i;
    s32 j;

    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = heap_alloc(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        bzero(codes, 14);
        menu_state_current->details->row_count[row] = 0;
        if (ids[top + row] != 0) {
            value = held[top + row];
            switch (kinds[top + row]) {
            case 0:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_weapon_name(ids[top + row]), pixels, 0x24, 0);
                break;
            case 1:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_accessory_name(ids[top + row]), pixels, 0x24, 0);
                break;
            case 2:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_item_name(ids[top + row]), pixels, 0x24, 0);
                break;
            }
            started = 0;
            for (i = 0, j = 4; j > 0; i++, j--) {
                digit = value / divisors[j];
                if (digit != 0 || started) {
                    codes[i * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * divisors[j];
                } else {
                    codes[i * 2] = 0xC3;
                }
            }
            codes[i * 2] = value % 10 + 0x10;
            text_decode_codes(codes, text, 5);
            menu_state_current->details->names_b[row].width = window_render_text_line(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            item_shop_label_init_quads(&menu_state_current->details->names_a[row], row, 0x80, 0x81);
            item_shop_set_rect_verts(menu_state_current->details->names_a[row].verts, 0x24, row * 13 + 0x32,
                          menu_state_current->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            DrawSync(0);
            item_shop_label_init_quads(&menu_state_current->details->names_b[row], row, 0x80, 0x82);
            item_shop_set_rect_verts(menu_state_current->details->names_b[row].verts, 0x8C, row * 13 + 0x32,
                          menu_state_current->details->names_b[row].width, 13);
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->name_shown[row] = 1;
            if (chosen[top + row] != 0) {
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE5, menu_state_current->details->rows[row],
                                  menu_state_current->buffer_index, 0xB4, row * 13 + 0x36, 0x1000);
                tens = chosen[top + row] / 10;
                if (tens != 0) {
                    menu_state_current->details->row_count[row] +=
                        sprite_sheet_draw_scaled(menu_state_current->sheet, tens,
                                      &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                      menu_state_current->buffer_index, 0xBC, row * 13 + 0x36, 0x1000);
                }
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, (u8)(chosen[top + row] % 10),
                                  &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                  menu_state_current->buffer_index, 0xC4, row * 13 + 0x36, 0x1000);
                menu_state_current->details->row_buffer[row] = menu_state_current->buffer_index;
            }
        } else {
            menu_state_current->details->name_shown[row] = 0;
        }
    }
    heap_free(pixels);
}

/* 801D0C18
 * Set the party's gold (capped at 9999999) and take the sold items away: with
 * `inventory`, the chosen amounts out of an inventory's counts; otherwise
 * each sold item (kind 0 gear, 1 accessory) out of member `member`'s equipment.
 */
void item_shop_settle_sale(u32 gold, u8 *ids, u8 *amounts, s32 n, u8 *inv_ids, u8 *inv_counts, u8 *kinds,
                   u8 inventory, u8 member) {
    u32 *party_gold;
    s32 i;
    s32 j;

    item_shop_play_sound(0xD1);
    party_gold = &game_data.gold;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    if (inventory) {
        for (i = 0; i < n; i++) {
            if (ids[i] != 0) {
                for (j = 0; j < n; j++) {
                    if (ids[i] == inv_ids[j]) {
                        inv_counts[j] -= amounts[i];
                        if (inv_counts[j] == 0) {
                            inv_ids[j] = 0;
                        }
                    }
                }
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            if (ids[i] != 0 && amounts[i] != 0) {
                switch (kinds[i]) {
                case 0:
                    if (ids[i] < 0x32) {
                        game_data.characters[member].weapons[0] = 0;
                    } else {
                        for (j = 0; j < 4; j++) {
                            if (ids[i] == game_data.characters[member].entryItems[j]) {
                                game_data.characters[member].entryItems[j] = 0;
                                break;
                            }
                        }
                    }
                    break;
                case 1:
                    for (j = 0; j < 3; j++) {
                        if (ids[i] == game_data.characters[member].accessories[j]) {
                            game_data.characters[member].accessories[j] = 0;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
}

/* 801D0E68
 * The sell list: choose how many of each of `n` held items (ids and counts)
 * to sell, eight rows at a time, with the running total and the gold after
 * the sale; confirming settles the sale. Items of kind 2 flagged unsellable
 * are left out. With `same_kind` every item is of kind `kind`, otherwise
 * `kinds` gives each one's kind; `member` is the member selling equipment.
 */
void item_shop_sell_list_run(s32 n, u8 *ids, u8 *counts, u8 kind, u8 same_kind, u8 *kinds, u8 member) {
    u8 running = 1;
    u8 first = 1;
    u8 redraw = 1;
    s32 row = 0;
    s32 last_row = 0xFF;
    s32 top = 0;
    s32 last_top = 0xFF;
    u8 sell_ids[n];
    u8 sell_kinds[n];
    u8 chosen[n];
    u8 selectable[n];
    u8 *held;
    u32 gold = game_data.gold;
    u8 held_storage[n];
    u32 total;
    u32 new_gold;
    u32 price;
    s32 count;
    s32 collected;
    u8 ok;
    s32 i;
    s32 index;

    held = held_storage;
    new_gold = gold;
    total = 0;

    for (i = 0; i < n; i++) {
        held[i] = 0;
        selectable[i] = 0;
        chosen[i] = 0;
        sell_ids[i] = 0;
        if (same_kind) {
            sell_kinds[i] = kind;
        } else {
            sell_kinds[i] = kinds[i];
        }
    }
    /* The original assigns count only when an item is collected. */
    collected = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] != 0 && counts[i] != 0) {
            ok = 1;
            if (sell_kinds[i] == 2) {
                if (menu_state_current->tables->items[ids[i]].use & 0x10) {
                    ok = 0;
                }
            }
            if (ok) {
                sell_ids[collected] = ids[i];
                held[collected] = counts[i];
                selectable[collected] = 1;
                count = ++collected;
            }
        }
    }
    item_shop_list_cursor_alloc(0);
    while (running) {
        item_shop_run_frame();
        if (top != last_top || redraw) {
            item_shop_layout_sell_rows(top, sell_ids, sell_kinds, chosen, held);
            item_shop_scroll_bar_show(0xC, 0x32, 0x3C, count, top);
        }
        if (row != last_row || top != last_top) {
            last_row = row;
            price = item_shop_show_sell_entry(sell_ids[top + row], sell_kinds[top + row]);
            last_top = top;
            menu_state_current->flags->unknown5a[0] = 1;
        }
        item_shop_list_cursor_place(row, top, 0, 0);
        if (first) {
            item_shop_panel_open(2, 0xC, 0x2A, 0xC4, 0x74, 0, 1, 4, 1);
            item_shop_panel_open(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            item_shop_view_start_zoom_in();
            if (same_kind) {
                item_shop_reveal_portraits();
            }
            while (menu_state_current->view_motion != 0) {
                item_shop_run_frame();
            }
            item_shop_layout_sell_headings();
            first = 0;
        }
        if (redraw) {
            item_shop_layout_gold_numbers(gold, total, new_gold);
            redraw = 0;
        }
        switch (menu_state_current->input) {
        case 4:
            if (total != 0) {
                item_shop_play_sound(2);
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->scroll_shown = 0;
                running = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                item_shop_markers_open(0);
                item_shop_layout_notice_total(total);
                if (item_shop_notice_ask_yes_no(0x95, 0xFF, 1)) {
                    item_shop_settle_sale(new_gold, sell_ids, chosen, n, ids, counts, sell_kinds, same_kind, member);
                } else {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->cursors_shown[0] = running;
                }
                item_shop_markers_close();
            } else {
                item_shop_play_sound(4);
            }
            break;
        case 5:
            running = 0;
            if (total != 0) {
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->scroll_shown = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                item_shop_markers_open(0);
                if (!item_shop_notice_ask_yes_no(0x92, 0xFF, 1)) {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->cursors_shown[0] = running;
                }
                item_shop_markers_close();
            }
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                if (count - 8 < ++top) {
                    top--;
                }
            }
            break;
        case 3:
            row--;
            if (row < 0) {
                top--;
                row = 0;
                if (top < 0) {
                    top = 0;
                }
            }
            break;
        case 0:
            index = top + row;
            if (selectable[index] && held[index] - 1 >= 0) {
                redraw = 1;
                total += price;
                chosen[index]++;
                new_gold += price;
                held[index]--;
            }
            break;
        case 2:
            index = top + row;
            if (selectable[index] && chosen[index] - 1 >= 0) {
                total -= price;
                redraw = 1;
                held[index]++;
                new_gold -= price;
                chosen[index]--;
            }
            break;
        }
    }
}

/* 801D1658
 * Sell list 0: choose a party member by portrait, then sell from that
 * member's accessories. Returns 0 when cancelled, 1 when chosen.
 */
u8 item_shop_member_sell_list_run(void) {
    u8 ids[8];
    u8 amounts[8];
    u8 kinds[8];
    s32 cursor;
    s32 shown;
    u8 result;
    s32 member;
    s32 i;
    s32 n;

    cursor = 0;
    shown = 1;
    result = 2;
    item_shop_reveal_portraits();
    item_shop_notice_open(0x98);
    do {
        item_shop_run_frame();
        switch (menu_state_current->input) {
        case 5:
            result = 0;
            break;
        case 4:
            item_shop_play_sound(2);
            result = 1;
            break;
        case 2:
            cursor--;
            if (cursor < 0) {
                cursor = menu_state_current->details->members_count - 1;
            }
            break;
        case 0:
            cursor++;
            if (cursor >= menu_state_current->details->members_count) {
                cursor = 0;
            }
            break;
        }
        if (cursor != shown) {
            menu_state_current->details->bar_shown[cursor] = 1;
            menu_state_current->details->bar_shown[shown] = 0;
            shown = cursor;
        }
    } while (result == 2);
    item_shop_notice_close();
    if (result != 0) {
        member = 0;
        while (cursor != 0) {
            member++;
            if (menu_state_current->present[member] != 0) {
                cursor--;
            }
        }
        for (i = 0; i < 8; i++) {
            ids[i] = 0;
            amounts[i] = 1;
        }
        n = 0;
        for (i = 0; i < 3; i++) {
            if (game_data.characters[member].accessories[i] != 0) {
                ids[n] = game_data.characters[member].accessories[i];
                kinds[n] = 1;
                n++;
            }
        }
        item_shop_sell_list_run(8, ids, amounts, 1, 0, kinds, member);
    }
    return result;
}

/* 801D18A8: Run the sell list for inventory 1 (200 entries). */
void item_shop_accessory_sell_list_run(void) {
    item_shop_sell_list_run(200, game_data.accessoryIds, game_data.accessoryIds - 200, 1, 1, game_data.accessoryIds - 200, 0);
}

/* 801D18E8: Run the sell list for inventory 0 (100 entries). */
void item_shop_weapon_sell_list_run(void) {
    item_shop_sell_list_run(100, game_data.weaponIds, game_data.weaponIds - 100, 0, 1, game_data.weaponIds - 100, 0);
}

/* 801D1928: Run the sell list for inventory 2 (150 entries). */
void item_shop_item_sell_list_run(void) {
    item_shop_sell_list_run(150, game_data.itemIds, game_data.itemIds - 150, 2, 1, game_data.itemIds - 150, 0);
}

/* 801D1968: Hide the shop list's packets; with `close` also close its panels (5 too
 * with `all`), scroll bar and marker. */
void item_shop_hide_details(u8 close, u8 all) {
    s32 i;

    menu_state_current->flags->unknown5a[0] = 0;
    menu_state_current->details->label4430_shown = 0;
    menu_state_current->details->label44B0_shown = 0;
    menu_state_current->details->digits_shown = 0;
    menu_state_current->details->heading_count = 0;
    menu_state_current->details->group2D0_count = 0;
    menu_state_current->details->members_count = 0;
    for (i = 0; i < 9; i++) {
        menu_state_current->details->bar_shown[i] = 0;
        menu_state_current->details->cells_a_count[i] = 0;
        menu_state_current->details->cells_b_count[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        menu_state_current->details->name_shown[i] = 0;
        menu_state_current->details->row_count[i] = 0;
    }
    if (close) {
        item_shop_panel_close(2);
        item_shop_panel_close(3);
        if (all) {
            item_shop_panel_close(5);
        }
        item_shop_scroll_bar_hide();
        item_shop_list_cursor_free(0);
    }
}

/* 801D1B18: Run the chosen sell list (0 a member's equipment, 1-3 the three
 * inventories), then restore the list labels. */
void item_shop_chosen_sell_list_run(void) {
    u8 close;

    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    menu_state_current->flags->lists_shown = 0;
    item_shop_label_render_or_clear_shown(0, 4, menu_state_current->list_labels, item_shop_sell_label_ids, menu_state_current->flags->list_labels_shown);
    close = 1;
    switch (menu_state_current->choice) {
    case 0:
        close = item_shop_member_sell_list_run();
        break;
    case 1:
        item_shop_accessory_sell_list_run();
        break;
    case 2:
        item_shop_weapon_sell_list_run();
        break;
    case 3:
        item_shop_item_sell_list_run();
        break;
    }
    item_shop_hide_details(close, 0);
    menu_state_current->flags->lists_shown = 1;
    menu_state_current->flags->sprite_shown = 1;
    menu_state_current->flags->cursor_shown = 1;
    item_shop_label_render_or_clear_shown(1, 4, menu_state_current->list_labels, item_shop_sell_label_ids, menu_state_current->flags->list_labels_shown);
}

/* 801D1CA4: Command 1 (sell): choose one of the sell lists until cancelled. */
u8 item_shop_sell_command_run(void) {
    u8 running;
    u8 first;

    running = 1;
    first = 1;
    menu_state_current->choice = 3;
    menu_state_current->choice_shown = 0xFF;
    do {
        item_shop_run_frame();
        if (first) {
            item_shop_label_render_or_clear_shown(1, 4, menu_state_current->list_labels, item_shop_sell_label_ids, menu_state_current->flags->list_labels_shown);
            first = 0;
            item_shop_view_start_zoom_in();
            item_shop_choice_window_open(0);
        }
        if (menu_state_current->choice != menu_state_current->choice_shown) {
            item_shop_label_place(4, menu_state_current->list_labels, item_shop_sell_label_ids, item_shop_sell_label_x_offsets,
                          menu_state_current->flags->list_labels_shown, menu_state_current->choice, 3, 0);
            item_shop_choice_window_set_cursor(0);
            menu_state_current->choice_shown = menu_state_current->choice;
        }
        switch (menu_state_current->input) {
        case 4:
            item_shop_play_sound(2);
            item_shop_chosen_sell_list_run();
            menu_state_current->choice_shown = 0xFF;
            break;
        case 5:
            running = 0;
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
        }
    } while (running);
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    item_shop_label_render_or_clear_shown(0, 4, menu_state_current->list_labels, item_shop_sell_label_ids, menu_state_current->flags->list_labels_shown);
    return 1;
}

/* 801D1F10: Follow-up after a command returns: command 2 redraws its screen. */
void item_shop_finish_top_command(void) {
    switch (menu_state_current->cursor) {
    case 1:
        break;
    case 2:
        item_shop_hide_details(1, 1);
        break;
    }
}
