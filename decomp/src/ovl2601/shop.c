/*
 * The item shop's own screens (801cce1c onwards): member stat views, the buy
 * and sell lists, prices and the sale. A separate translation unit from the
 * shared screen code before it: it calls the party-bit helpers 801c50b0 and
 * 801c50cc without a prototype in scope (arguments and results unmasked),
 * which the shared unit's own callers do not.
 */
#include "menu_card.h"

/* Fill a view's nine stat words from character `id`'s base and bonus bytes, capped at 999 or 99. */
void func_801CCE1C(ResourceSet *view, u8 id) {
    Character *c;

    c = &D_8006D8A0[id];
    if (c->unk56 == 4) {
        view->stats[0] = c->unk4 + c->unk1C;
    } else {
        view->stats[0] = c->unk4 + (c->bonus[0] + c->base[0]);
    }
    view->stats[0] = c->unk4 + (c->bonus[0] + c->base[0]);
    view->stats[1] = c->bonus[6] + c->base[6];
    view->stats[2] = c->base[5] + (c->bonus[1] + c->base[1]);
    view->stats[3] = c->bonus[7] + c->base[7];
    view->stats[4] = c->bonus[3] + c->base[3];
    view->stats[5] = 99;
    view->stats[6] = c->bonus[4] + c->base[4];
    view->stats[7] = 10;
    view->stats[8] = c->bonus[2] + c->base[2];
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

/* Draw the shop's detail packets: member bars, portraits, headings, list rows, labels and numbers. */
void func_801CCFF4(void) {
    s32 i;

    if (D_800625A0->flags->unk5A != 0) {
        for (i = 0; i < 9; i++) {
            if (D_800625A0->details->bar_shown[i] != 0) {
                AddPrim(&D_800625A0->draw_env->ot[4],
                        &D_800625A0->details->bar_upper[i * 2 + D_800625A0->buffer]);
                AddPrim(&D_800625A0->draw_env->ot[4],
                        &D_800625A0->details->bar_lower[i * 2 + D_800625A0->buffer]);
            }
        }
        func_801C8D58(D_800625A0->details->heading_count, D_800625A0->details->heading,
                      D_800625A0->details->heading_buffer);
        func_801C8D58(D_800625A0->details->group2D0_count, D_800625A0->details->group2D0,
                      D_800625A0->details->group2D0_buffer);
        func_801C8D58(D_800625A0->details->members_count, D_800625A0->details->members,
                      D_800625A0->details->members_buffer);
        for (i = 0; i < 8; i++) {
            if (D_800625A0->details->name_shown[i] != 0) {
                func_801C8C3C(1, D_800625A0->details->names_a[i].quad, D_800625A0->details->names_a[i].poly,
                              D_800625A0->details->names_a[i].buffer);
                func_801C8C3C(1, D_800625A0->details->names_b[i].quad, D_800625A0->details->names_b[i].poly,
                              D_800625A0->details->names_b[i].buffer);
            }
        }
        if (D_800625A0->details->label4430_shown != 0) {
            func_801C8C3C(1, D_800625A0->details->label4430.quad, D_800625A0->details->label4430.poly,
                          D_800625A0->details->label4430.buffer);
        }
        if (D_800625A0->details->label44B0_shown != 0) {
            func_801C8C3C(1, D_800625A0->details->label44B0.quad, D_800625A0->details->label44B0.poly,
                          D_800625A0->details->label44B0.buffer);
        }
        if (D_800625A0->details->label45B0_shown != 0) {
            func_801C8C3C(1, D_800625A0->details->label45B0.quad, D_800625A0->details->label45B0.poly,
                          D_800625A0->details->label45B0.buffer);
        }
        if (D_800625A0->details->digits_shown != 0) {
            AddPrim(&D_800625A0->draw_env->ot[4], &D_800625A0->details->frame[D_800625A0->buffer]);
            func_801C8D58(D_800625A0->details->digits1_count, D_800625A0->details->digits1,
                          D_800625A0->details->digits1_buffer);
            func_801C8D58(D_800625A0->details->digits2_count, D_800625A0->details->digits2,
                          D_800625A0->details->digits2_buffer);
            func_801C8D58(D_800625A0->details->digits3_count, D_800625A0->details->digits3,
                          D_800625A0->details->digits3_buffer);
        }
        for (i = 0; i < 8; i++) {
            func_801C8D58(D_800625A0->details->row_count[i], D_800625A0->details->rows[i],
                          D_800625A0->details->row_buffer[i]);
        }
        for (i = 0; i < 9; i++) {
            func_801C8D58(D_800625A0->details->cells_a_count[i], D_800625A0->details->cells_a[i],
                          D_800625A0->details->cells_a_buffer[i]);
            func_801C8D58(D_800625A0->details->cells_b_count[i], D_800625A0->details->cells_b[i],
                          D_800625A0->details->cells_b_buffer[i]);
        }
    }
    if (D_800625A0->flags->unk5B == 1) {
        func_801C8D58(D_800625A0->details->group1220_count, D_800625A0->details->group1220,
                      D_800625A0->details->group1220_buffer);
    }
}

/* Tint `count` packet pairs of this buffer red (0) or blue (1). */
void func_801CD404(s32 count, POLY_FT4 *packets, u8 color) {
    s32 i;

    for (i = 0; i < count; i++) {
        SetShadeTex(&packets[i * 2 + D_800625A0->buffer], 0);
        switch (color) {
        case 0:
            (packets + (i * 2 + D_800625A0->buffer))->r0 = 0x80;
            (packets + (i * 2 + D_800625A0->buffer))->g0 = 0x40;
            (packets + (i * 2 + D_800625A0->buffer))->b0 = 0x40;
            break;
        case 1:
            (packets + (i * 2 + D_800625A0->buffer))->r0 = 0x40;
            (packets + (i * 2 + D_800625A0->buffer))->g0 = 0x40;
            (packets + (i * 2 + D_800625A0->buffer))->b0 = 0x80;
            break;
        }
    }
}

/* Reveal the available party members' portraits one member per frame. */
void func_801CD5D0(void) {
    s32 step;
    s32 shown;
    s32 i;

    D_800625A0->flags->unk5A = 1;
    for (step = 1; step < 12; step++) {
        shown = 0;
        D_800625A0->details->members_count = 0;
        for (i = 0; i < step; i++) {
            if (D_800625A0->member_present[i] != 0) {
                D_800625A0->details->members_count +=
                    func_8002675C(D_800625A0->sprite_sheet, i + 0x14E, &D_800625A0->details->members[shown * 2],
                                  D_800625A0->buffer, D_801D21CC[shown], 0xA6, 0x1000);
                shown++;
            }
        }
        D_800625A0->details->members_buffer = D_800625A0->buffer;
        func_801CB014();
    }
}

/* Draw the four heading sprites. */
void func_801CD6F8(void) {
    s32 i;

    D_800625A0->details->heading_count = 0;
    for (i = 0; i < 4; i++) {
        D_800625A0->details->heading_count +=
            func_8002675C(D_800625A0->sprite_sheet, D_801D2210[i],
                          D_800625A0->details->heading + D_800625A0->details->heading_count * 2,
                          D_800625A0->buffer, D_801D2218[i], D_801D2230[i], 0x1000);
    }
    D_800625A0->details->heading_buffer = D_800625A0->buffer;
}

/* Draw the two alternative heading sprites. */
void func_801CD7E4(void) {
    s32 i;

    D_800625A0->details->heading_count = 0;
    for (i = 0; i < 2; i++) {
        D_800625A0->details->heading_count +=
            func_8002675C(D_800625A0->sprite_sheet, D_801D2214[i],
                          D_800625A0->details->heading + D_800625A0->details->heading_count * 2,
                          D_800625A0->buffer, D_801D2228[i], D_801D2240[i], 0x1000);
    }
    D_800625A0->details->heading_buffer = D_800625A0->buffer;
}

/* Draw three nine-digit numbers (leading zeros blank) at their three positions. */
void func_801CD8D0(u32 first, u32 second, u32 third) {
    s32 i;

    func_801C50E8(first);
    D_800625A0->details->digits1_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->digits1_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              D_800625A0->details->digits1 + D_800625A0->details->digits1_count * 2,
                              D_800625A0->buffer, i * 8 + D_801D2248, D_801D224C, 0x1000);
        }
    }
    D_800625A0->details->digits1_buffer = D_800625A0->buffer;
    func_801C50E8(second);
    D_800625A0->details->digits2_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->digits2_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              D_800625A0->details->digits2 + D_800625A0->details->digits2_count * 2,
                              D_800625A0->buffer, i * 8 + D_801D2250, D_801D2254, 0x1000);
        }
    }
    D_800625A0->details->digits2_buffer = D_800625A0->buffer;
    func_801C50E8(third);
    D_800625A0->details->digits3_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->digits3_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              D_800625A0->details->digits3 + D_800625A0->details->digits3_count * 2,
                              D_800625A0->buffer, i * 8 + D_801D2258, D_801D225C, 0x1000);
        }
    }
    D_800625A0->details->digits3_buffer = D_800625A0->buffer;
    D_800625A0->details->digits_shown = 1;
}

/* Party bits of the available members holding item `item` in their gear (kind 0) or accessories (kind 1). */
u16 func_801CDBA0(u8 item, u8 kind) {
    u16 members;
    u8 found;
    s32 i;
    s32 k;

    members = 0;
    if (item != 0 && kind != 2) {
        for (i = 0; i < 16; i++) {
            found = 0;
            if (D_800625A0->member_present[i] != 0) {
                switch (kind) {
                case 0:
                    for (k = 0; k < 5; k++) {
                        if (item < 0x32) {
                            if (D_8006D8A0[i].weapons[k] == item) {
                                found = 1;
                                break;
                            }
                        } else if (D_8006D8A0[i].armour[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                case 1:
                    for (k = 0; k < 3; k++) {
                        if (D_8006D8A0[i].accessories[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                }
            }
            if (found) {
                members |= func_801C50CC(i);
            }
        }
    }
    return members;
}

/*
 * Draw the eight visible rows of the shop's stock from entry `top`: each
 * item's name and price (dimmed, `dims[row]` 0, when it costs more than
 * `gold`) and, when some are chosen, "x" and the amount.
 */
#ifdef NON_MATCHING
void func_801CDD14(s32 top, s32 gold, u8 *dims) {
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
    pixels = func_80031BDC(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        dims[row] = 0;
        bzero(codes, 14);
        D_800625A0->details->row_count[row] = 0;
        if (D_800625A0->shop_items[top + row] != 0) {
            switch (D_800625A0->shop_kinds[top + row]) {
            case 0:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033848(D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                value = D_800625A0->resources->equipment[D_800625A0->shop_items[top + row]].price;
                price = value;
                break;
            case 1:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_800337E8(D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                value = D_800625A0->resources->accessories[D_800625A0->shop_items[top + row]].price;
                price = value;
                break;
            case 2:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033818(D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                value = D_800625A0->resources->items[D_800625A0->shop_items[top + row]].price;
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
            codes[8] = value % 10 + 0x10;
            func_80033B34(codes, text, 5);
            D_800625A0->details->names_b[row].width = func_80034EAC(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            func_801C5A7C(&D_800625A0->details->names_a[row], row, 0x80, dims[row] + 1);
            func_801C6E90(D_800625A0->details->names_a[row].quad, 0x24, row * 13 + 0x32,
                          D_800625A0->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            DrawSync(0);
            func_801C5A7C(&D_800625A0->details->names_b[row], row, 0x80, dims[row] + 2);
            func_801C6E90(D_800625A0->details->names_b[row].quad, 0x8C, row * 13 + 0x32,
                          D_800625A0->details->names_b[row].width, 13);
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->name_shown[row] = 1;
            if (D_800625A0->details->amounts[top + row] != 0) {
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xF1, D_800625A0->details->rows[row],
                                  D_800625A0->buffer, 0xB4, row * 13 + 0x36, 0x1000);
                tens = D_800625A0->details->amounts[top + row] / 10;
                if (tens != 0) {
                    D_800625A0->details->row_count[row] +=
                        func_8002675C(D_800625A0->sprite_sheet, tens,
                                      &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                      D_800625A0->buffer, 0xBC, row * 13 + 0x36, 0x1000);
                }
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, (u8)(D_800625A0->details->amounts[top + row] % 10),
                                  &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                  D_800625A0->buffer, 0xC4, row * 13 + 0x36, 0x1000);
                D_800625A0->details->row_buffer[row] = D_800625A0->buffer;
            }
        } else {
            D_800625A0->details->name_shown[row] = 0;
        }
    }
    func_800320E8(pixels);
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/shop", func_801CDD14);
#endif

/*
 * Compare member `member`'s attack (kind 0) or defence (kind 1) with item
 * `id` equipped: `diffs` gets the two differences and `worse` whether each
 * would drop. Member 4 equips four armour slots from the equipment table;
 * accessories of one group replace each other, otherwise the weakest is
 * replaced.
 */
void func_801CE480(s32 *diffs, u8 *worse, u8 id, u8 kind, u8 member) {
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
            current = &D_800625A0->resources->equipment[D_8006D8A0[member].weapons[0]];
            before[0] = current->power + D_8006D8A0[member].bonus[0];
            current = &D_800625A0->resources->equipment[id];
            after[0] = current->power + D_8006D8A0[member].bonus[0];
        } else {
            for (k = 0; k < 4; k++) {
                current = &D_800625A0->resources->equipment[D_8006D8A0[member].armour[k]];
                before[0] += current->power;
            }
            fitted = &D_800625A0->resources->equipment[id];
            for (k = 0; k < 4; k++) {
                current = &D_800625A0->resources->equipment[D_8006D8A0[member].armour[k]];
                if (current->type == fitted->type && current->type != 5) {
                    after[0] += fitted->power;
                } else {
                    after[0] += current->power;
                }
            }
        }
        break;
    case 1:
        replace = 1;
        before[1] = after[1] = D_8006D8A0[member].bonus[1];
        for (k = 0; k < 3; k++) {
            accessory = &D_800625A0->resources->accessories[D_8006D8A0[member].accessories[k]];
            before[1] += accessory->power;
        }
        fitted_accessory = &D_800625A0->resources->accessories[id];
        for (k = 0; k < 3; k++) {
            accessory = &D_800625A0->resources->accessories[D_8006D8A0[member].accessories[k]];
            if (accessory->group != 0 && accessory->group == fitted_accessory->group) {
                after[1] += fitted_accessory->power;
                replace = 0;
            } else {
                after[1] += accessory->power;
            }
        }
        if (replace) {
            lowest = 0xFF;
            for (k = 0; k < 3; k++) {
                accessory = &D_800625A0->resources->accessories[D_8006D8A0[member].accessories[k]];
                if (lowest >= accessory->power) {
                    lowest = accessory->power;
                    weakest = k;
                }
            }
            after[1] = D_8006D8A0[member].bonus[1] + fitted_accessory->power;
            for (k = 0; k < 3; k++) {
                if (k != weakest) {
                    accessory = &D_800625A0->resources->accessories[D_8006D8A0[member].accessories[k]];
                    after[1] += accessory->power;
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

/* The count held of item `id` in an inventory of `n` ids and counts (0 if absent). */
u16 func_801CE8D8(u8 *ids, u8 *counts, s32 n, u8 id) {
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

/* Show how many of item `id` the party holds in inventory `kind` (label beside the list). */
void func_801CE91C(u8 kind, u8 id) {
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
        ids = D_8006F3D0;
        counts = ids - 100;
        n = 100;
        break;
    case 1:
        ids = D_8006F4FC;
        counts = ids - 200;
        n = 200;
        break;
    case 2:
        ids = D_8006F65A;
        counts = ids - 150;
        n = 150;
        break;
    }
    count = func_801CE8D8(ids, counts, n, id);
    D_801D2260 = count;
    pixels = func_80031BDC(0x3F6, 0);
    codes[1] = 0;
    codes[3] = 0;
    if (count / 10) {
        codes[0] = count / 10 + 0x10;
    } else {
        codes[0] = 0xC3;
    }
    codes[2] = count % 10 + 0x10;
    func_80033B34(codes, text, 2);
    D_800625A0->details->label45B0.width = func_80034EAC(text, pixels, 0x24, 1);
    rect.x = 0x198;
    rect.y = 0xB4;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_801C5A7C(&D_800625A0->details->label45B0, 9, 0x80, 0x82);
    func_801C6E90(D_800625A0->details->label45B0.quad, 0xF8, 0x8E, D_800625A0->details->label45B0.width,
                  13);
    D_800625A0->details->label45B0.buffer = D_800625A0->buffer;
    D_800625A0->details->label45B0_shown = 1;
    func_800320E8(pixels);
}

/*
 * Show stock entry `top + row` (`dims` unused): its name label, the bars of the members who
 * can equip it, the marks of those holding it and, for each member who can,
 * the attack and defence change (tinted by whether it drops), then how many
 * the party holds. Returns its price.
 */
u32 func_801CEB3C(s32 row, s32 top, u8 *dims) {
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
    id = D_800625A0->shop_items[top + row];
    kind = D_800625A0->shop_kinds[top + row];
    pixels = func_80031BDC(0x618, 0);
    bzero(pixels, 0x618);
    switch (kind) {
    case 0:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[1], id), pixels, 0x39, 0);
        price = D_800625A0->resources->equipment[id].price;
        users = D_800625A0->resources->equipment[id].users;
        break;
    case 1:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[2], id), pixels, 0x39, 0);
        price = D_800625A0->resources->accessories[id].price;
        users = D_800625A0->resources->accessories[id].users;
        break;
    case 2:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[0], id), pixels, 0x39, 0);
        price = D_800625A0->resources->items[id].price;
        break;
    }
    holders = func_801CDBA0(id, kind);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_801C5A7C(&D_800625A0->details->label4430, 0, 0, 0);
    func_801C5040(&D_800625A0->details->label4430.poly[D_800625A0->buffer], 0x2C, 0x12, 0, 0x4E,
                  D_800625A0->details->label4430.width, 13);
    func_801C6E90(D_800625A0->details->label4430.quad, 0x2C, 0x12, D_800625A0->details->label4430.width, 13);
    D_800625A0->details->label4430.buffer = D_800625A0->buffer;
    func_800320E8(pixels);
    if (id) {
        D_800625A0->details->label4430_shown = 1;
    } else {
        D_800625A0->details->label4430_shown = 0;
    }
    i = 0;
    shown = 0;
    D_800625A0->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (D_800625A0->member_present[i] != 0) {
            if (func_801C50B0(users, i)) {
                D_800625A0->details->bar_shown[shown] = 1;
            } else {
                D_800625A0->details->bar_shown[shown] = 0;
            }
            if (func_801C50B0(holders, i)) {
                D_800625A0->details->group2D0_count +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xE,
                                  &D_800625A0->details->group2D0[D_800625A0->details->group2D0_count * 2],
                                  D_800625A0->buffer, D_801D21CC[shown] + 0xE, 0xB4, 0x1000);
            }
            D_800625A0->details->cells_a_count[shown] = 0;
            D_800625A0->details->cells_b_count[shown] = 0;
            if (D_800625A0->details->bar_shown[shown] != 0) {
                diffs[1] = 0;
                diffs[0] = 0;
                func_801CE480(diffs, worse, id, kind, i);
                if (diffs[0] != 0) {
                    func_801C50E8(diffs[0]);
                    for (k = 0, xa = shown * 26 + 0x49; k < 3; k++) {
                        if (D_800625A0->digits[k + 6] != 0xFF) {
                            D_800625A0->details->cells_a_count[shown] += func_8002675C(
                                D_800625A0->sprite_sheet, D_800625A0->digits[k + 6],
                                &D_800625A0->details->cells_a[shown][D_800625A0->details->cells_a_count[shown] * 2],
                                D_800625A0->buffer, xa + k * 8, 0xBE, 0x1000);
                        }
                    }
                    func_801CD404(D_800625A0->details->cells_a_count[shown], D_800625A0->details->cells_a[shown],
                                  worse[0]);
                    D_800625A0->details->cells_a_buffer[shown] = D_800625A0->buffer;
                }
                if (diffs[1] != 0) {
                    func_801C50E8(diffs[1]);
                    for (k = 0, xb = shown * 26 + 0x49; k < 3; k++) {
                        if (D_800625A0->digits[k + 6] != 0xFF) {
                            D_800625A0->details->cells_b_count[shown] += func_8002675C(
                                D_800625A0->sprite_sheet, D_800625A0->digits[k + 6],
                                &D_800625A0->details->cells_b[shown][D_800625A0->details->cells_b_count[shown] * 2],
                                D_800625A0->buffer, xb + k * 8, 0xC6, 0x1000);
                        }
                    }
                    func_801CD404(D_800625A0->details->cells_b_count[shown], D_800625A0->details->cells_b[shown],
                                  worse[1]);
                    D_800625A0->details->cells_b_buffer[shown] = D_800625A0->buffer;
                }
            }
            shown++;
        }
    }
    D_800625A0->details->group2D0_buffer = D_800625A0->buffer;
    func_801CE91C(kind, id);
    return price;
}

/*
 * Set the party's gold (capped at 9999999) and put the bought amounts into
 * the inventories: added to an item already held (at most 99), otherwise
 * into the first free slot.
 */
void func_801CF2A0(u32 gold) {
    u32 *party_gold;
    s32 i;
    s32 j;
    u8 new_item;

    func_801CAC7C(0xD1);
    party_gold = &D_8006D634.gold;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    for (i = 0; i < 0x30; i++) {
        if (D_800625A0->shop_items[i] != 0 && D_800625A0->details->amounts[i] != 0) {
            switch (D_800625A0->shop_kinds[i]) {
            case 0:
                new_item = 1;
                for (j = 0; j < 100; j++) {
                    if (D_8006F3D0[j] == D_800625A0->shop_items[i]) {
                        new_item = 0;
                        if ((D_8006F36C[j] += D_800625A0->details->amounts[i]) >= 100) {
                            D_8006F36C[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 100; j++) {
                        if (D_8006F3D0[j] == 0) {
                            D_8006F3D0[j] = D_800625A0->shop_items[i];
                            D_8006F36C[j] = D_800625A0->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            case 1:
                new_item = 1;
                for (j = 0; j < 200; j++) {
                    if (D_8006F4FC[j] == D_800625A0->shop_items[i]) {
                        new_item = 0;
                        if ((D_8006F434[j] += D_800625A0->details->amounts[i]) >= 100) {
                            D_8006F434[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 200; j++) {
                        if (D_8006F4FC[j] == 0) {
                            D_8006F4FC[j] = D_800625A0->shop_items[i];
                            D_8006F434[j] = D_800625A0->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            case 2:
                new_item = 1;
                for (j = 0; j < 150; j++) {
                    if (D_8006F65A[j] == D_800625A0->shop_items[i]) {
                        new_item = 0;
                        if ((D_8006F5C4[j] += D_800625A0->details->amounts[i]) >= 100) {
                            D_8006F5C4[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 150; j++) {
                        if (D_8006F65A[j] == 0) {
                            D_8006F65A[j] = D_800625A0->shop_items[i];
                            D_8006F5C4[j] = D_800625A0->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

/* Draw a nine-digit number (the party's gold) at (6bh, 54h). */
void func_801CF678(u32 value) {
    s32 i;
    s32 x;

    func_801C50E8(value);
    i = 0;
    x = 0x6B;
    D_800625A0->details->group1220_count = 0;
    for (; i < 9; i++, x += 8) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->group1220_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              D_800625A0->details->group1220 + D_800625A0->details->group1220_count * 2,
                              D_800625A0->buffer, x, 0x54, 0x1000);
        }
    }
    D_800625A0->details->group1220_buffer = D_800625A0->buffer;
    D_800625A0->flags->unk5B = 2;
}

/*
 * Command 2 (buy): choose amounts of the shop's stock, eight rows at a time,
 * with the running total and the gold left; confirming settles the
 * purchase. Returns 1 (the command screen is redrawn).
 */
#ifdef NON_MATCHING
u8 func_801CF780(void) {
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

    gold = D_8006D634.gold;
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
        func_801CCE1C(D_800625A0->resources, i);
        D_800625A0->details->attack[i] = D_800625A0->resources->stats[0];
        D_800625A0->details->defence[i] = D_800625A0->resources->stats[2];
    }
    bzero(D_800625A0->details->amounts, 0x30);
    D_800625A0->images->dim = 1;
    func_801C70FC(0);
    while (running) {
        func_801CB014();
        if (top != last_top || redraw) {
            func_801CDD14(top, new_gold, dims);
            func_801C6F30(0xC, 0x32, 0x3C, D_801D1F50, top);
        }
        if (row != last_row || top != last_top) {
            price = func_801CEB3C(row, top, dims);
            last_row = row;
            last_top = top;
            D_800625A0->flags->unk5A = 1;
        }
        func_801C7178(row, top, 0, 0);
        if (first) {
            func_801CBC88(1, 2, D_800625A0->list_labels, D_801D1FD4, D_800625A0->flags->list_label_shown);
            func_801CBCF0(2, D_800625A0->list_labels, D_801D1FD4, D_801D1FE8, D_800625A0->flags->list_label_shown, 0,
                          0, 1);
            D_800625A0->flags->list_label_shown[0] = 0;
            func_801C896C(2, 0xC, 0x2A, 0xC4, 0x74, 0, 1, 4, 1);
            func_801C896C(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            func_801C896C(5, 0xE0, 0x7A, 0x40, 0x24, 0, 1, 4, 0);
            func_801CB340();
            func_801CD5D0();
            func_801CD6F8();
            first = 0;
            while (D_800625A0->view_motion != 0) {
                func_801CB014();
            }
            D_800625A0->flags->list_label_shown[0] = 1;
        }
        if (redraw) {
            func_801CD8D0(gold, total, new_gold);
            redraw = 0;
        }
        switch (D_800625A0->input) {
        case 4:
            if (total != 0) {
                func_801CAC7C(2);
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->panel_shown[5] = 0;
                D_800625A0->flags->scroll_shown = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                running = 0;
                D_800625A0->flags->list_label_shown[0] = 0;
                func_801CB13C(0);
                func_801CF678(total);
                if (func_801CBA50(0x8F, 0xFF, 1)) {
                    func_801CF2A0(new_gold);
                } else {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->panel_shown[5] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    D_800625A0->flags->marker_shown[0] = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->list_label_shown[0] = 1;
                }
                func_801CB2FC();
            } else {
                func_801CAC7C(4);
            }
            break;
        case 5:
            running = 0;
            D_800625A0->flags->list_label_shown[0] = 0;
            if (total != 0) {
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->panel_shown[5] = 0;
                D_800625A0->flags->scroll_shown = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                func_801CB13C(0);
                if (!func_801CBA50(0x8C, 0xFF, 1)) {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->panel_shown[5] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    D_800625A0->flags->marker_shown[0] = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->list_label_shown[0] = 1;
                }
                func_801CB2FC();
            }
            func_801CB014();
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                top++;
                if (D_801D1F50 - 8 < top) {
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
            if (dims[row] != 0 && D_800625A0->details->amounts[top + row] + (D_801D2260 + 1) < 100) {
                total += price;
                new_gold -= price;
                D_800625A0->details->amounts[top + row] += 1;
                redraw = 1;
            }
            break;
        case 2:
            if (D_800625A0->details->amounts[top + row] != 0) {
                total -= price;
                new_gold += price;
                D_800625A0->details->amounts[top + row] -= 1;
                redraw = 1;
            }
            break;
        }
    }
    D_800625A0->details->label45B0_shown = 0;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/shop", func_801CF780);
#endif

/*
 * Show item `id` of kind `kind` (0 equipment, 1 accessory, 2 item): its name and
 * sell price (half the table price) labels, the bars of the members who can
 * equip it and the marks of those holding it. Returns the sell price.
 */
/* Frame layout unresolved: the original reserves 16 additional bytes
 * that the recovered operations do not explain. */
#ifdef NON_MATCHING
u32 func_801CFF58(u8 id, u8 kind) {
    RECT rect;
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
    pixels = func_80031BDC(0x618, 0);
    bzero(pixels, 0x618);
    bzero(codes, 14);
    switch (kind) {
    case 0:
        D_800625A0->details->label4430.width = func_80034EAC(func_80033848(id), pixels, 0x39, 0);
        users = D_800625A0->resources->equipment[id].users;
        price = D_800625A0->resources->equipment[id].price >> 1;
        value = price;
        break;
    case 1:
        D_800625A0->details->label4430.width = func_80034EAC(func_800337E8(id), pixels, 0x39, 0);
        users = D_800625A0->resources->accessories[id].users;
        price = D_800625A0->resources->accessories[id].price >> 1;
        value = price;
        break;
    case 2:
        D_800625A0->details->label4430.width = func_80034EAC(func_80033818(id), pixels, 0x39, 0);
        price = D_800625A0->resources->items[id].price >> 1;
        value = price;
        break;
    }
    holders = func_801CDBA0(id, kind);
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
    func_80033B34(codes, text, 5);
    D_800625A0->details->label44B0.width = func_80034EAC(text, pixels, 0x39, 1);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_801C5A7C(&D_800625A0->details->label4430, 0, 0, 0);
    func_801C5A7C(&D_800625A0->details->label44B0, 0, 0, 0);
    D_800625A0->details->label44B0.poly[D_800625A0->buffer].clut = D_80059414;
    func_801C5040(&D_800625A0->details->label4430.poly[D_800625A0->buffer], 0x2C, 0x12, 0, 0x4E,
                  D_800625A0->details->label4430.width, 13);
    func_801C5040(&D_800625A0->details->label44B0.poly[D_800625A0->buffer], 0x98, 0x12, 0, 0x4E,
                  D_800625A0->details->label44B0.width, 13);
    func_801C6E90(D_800625A0->details->label4430.quad, 0x2C, 0x12, D_800625A0->details->label4430.width, 13);
    func_801C6E90(D_800625A0->details->label44B0.quad, 0x98, 0x12, D_800625A0->details->label44B0.width, 13);
    D_800625A0->details->label4430.buffer = D_800625A0->buffer;
    D_800625A0->details->label44B0.buffer = D_800625A0->buffer;
    func_800320E8(pixels);
    if (id) {
        D_800625A0->details->label4430_shown = 1;
        D_800625A0->details->label44B0_shown = 1;
    } else {
        D_800625A0->details->label4430_shown = 0;
        D_800625A0->details->label44B0_shown = 0;
    }
    i = 0;
    j = 0;
    D_800625A0->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (D_800625A0->member_present[i] != 0) {
            if (func_801C50B0(users, i)) {
                D_800625A0->details->bar_shown[j] = 1;
            } else {
                D_800625A0->details->bar_shown[j] = 0;
            }
            if (func_801C50B0(holders, i)) {
                D_800625A0->details->group2D0_count +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xE,
                                  &D_800625A0->details->group2D0[D_800625A0->details->group2D0_count * 2],
                                  D_800625A0->buffer, D_801D21CC[j] + 0xE, 0xB4, 0x1000);
            }
            j++;
        }
    }
    D_800625A0->details->group2D0_buffer = D_800625A0->buffer;
    return price;
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/shop", func_801CFF58);
#endif

/*
 * Draw the eight visible rows of a list from entry `top`: each item's name,
 * the count held and, when some are chosen, "x" and the chosen count.
 */
#ifdef NON_MATCHING
void func_801D05BC(s32 top, u8 *ids, u8 *kinds, u8 *chosen, u8 *held) {
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
    pixels = func_80031BDC(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        bzero(codes, 14);
        D_800625A0->details->row_count[row] = 0;
        if (ids[top + row] != 0) {
            value = held[top + row];
            switch (kinds[top + row]) {
            case 0:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033848(ids[top + row]), pixels, 0x24, 0);
                break;
            case 1:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_800337E8(ids[top + row]), pixels, 0x24, 0);
                break;
            case 2:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033818(ids[top + row]), pixels, 0x24, 0);
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
            codes[8] = value % 10 + 0x10;
            func_80033B34(codes, text, 5);
            D_800625A0->details->names_b[row].width = func_80034EAC(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            func_801C5A7C(&D_800625A0->details->names_a[row], row, 0x80, 0x81);
            func_801C6E90(D_800625A0->details->names_a[row].quad, 0x24, row * 13 + 0x32,
                          D_800625A0->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            DrawSync(0);
            func_801C5A7C(&D_800625A0->details->names_b[row], row, 0x80, 0x82);
            func_801C6E90(D_800625A0->details->names_b[row].quad, 0x8C, row * 13 + 0x32,
                          D_800625A0->details->names_b[row].width, 13);
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->name_shown[row] = 1;
            if (chosen[top + row] != 0) {
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xE5, D_800625A0->details->rows[row],
                                  D_800625A0->buffer, 0xB4, row * 13 + 0x36, 0x1000);
                tens = chosen[top + row] / 10;
                if (tens != 0) {
                    D_800625A0->details->row_count[row] +=
                        func_8002675C(D_800625A0->sprite_sheet, tens,
                                      &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                      D_800625A0->buffer, 0xBC, row * 13 + 0x36, 0x1000);
                }
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, (u8)(chosen[top + row] % 10),
                                  &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                  D_800625A0->buffer, 0xC4, row * 13 + 0x36, 0x1000);
                D_800625A0->details->row_buffer[row] = D_800625A0->buffer;
            }
        } else {
            D_800625A0->details->name_shown[row] = 0;
        }
    }
    func_800320E8(pixels);
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/shop", func_801D05BC);
#endif

/*
 * Set the party's gold (capped at 9999999) and take the sold items away: with
 * `inventory`, the chosen amounts out of an inventory's counts; otherwise
 * each sold item (kind 0 gear, 1 accessory) out of member `member`'s equipment.
 */
void func_801D0C18(u32 gold, u8 *ids, u8 *amounts, s32 n, u8 *inv_ids, u8 *inv_counts, u8 *kinds,
                   u8 inventory, u8 member) {
    u32 *party_gold;
    s32 i;
    s32 j;

    func_801CAC7C(0xD1);
    party_gold = &D_8006D634.gold;
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
                        D_8006D634.characters[member].weapons[0] = 0;
                    } else {
                        for (j = 0; j < 4; j++) {
                            if (ids[i] == D_8006D634.characters[member].armour[j]) {
                                D_8006D634.characters[member].armour[j] = 0;
                                break;
                            }
                        }
                    }
                    break;
                case 1:
                    for (j = 0; j < 3; j++) {
                        if (ids[i] == D_8006D634.characters[member].accessories[j]) {
                            D_8006D634.characters[member].accessories[j] = 0;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
}

/*
 * The sell list: choose how many of each of `n` held items (ids and counts)
 * to sell, eight rows at a time, with the running total and the gold after
 * the sale; confirming settles the sale. Items of kind 2 flagged unsellable
 * are left out. With `same_kind` every item is of kind `kind`, otherwise
 * `kinds` gives each one's kind; `member` is the member selling equipment.
 */
#ifdef NON_MATCHING
void func_801D0E68(s32 n, u8 *ids, u8 *counts, u8 kind, u8 same_kind, u8 *kinds, u8 member) {
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
    u8 held[n];
    u32 total;
    u32 new_gold;
    u32 price;
    s32 count;
    u8 ok;
    s32 i;
    s32 index;
    u32 gold;

    gold = D_8006D634.gold;
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
    count = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] != 0 && counts[i] != 0) {
            ok = 1;
            if (sell_kinds[i] == 2) {
                ok = (D_800625A0->resources->items[ids[i]].flags & 0x10) == 0;
            }
            if (ok) {
                sell_ids[count] = ids[i];
                held[count] = counts[i];
                selectable[count] = 1;
                count++;
            }
        }
    }
    func_801C70FC(0);
    while (running) {
        func_801CB014();
        if (top != last_top || redraw) {
            func_801D05BC(top, sell_ids, sell_kinds, chosen, held);
            func_801C6F30(0xC, 0x32, 0x3C, count, top);
        }
        if (row != last_row || top != last_top) {
            last_row = row;
            price = func_801CFF58(sell_ids[top + row], sell_kinds[top + row]);
            last_top = top;
            D_800625A0->flags->unk5A = 1;
        }
        func_801C7178(row, top, 0, 0);
        if (first) {
            func_801C896C(2, 0xC, 0x2A, 0xC4, 0x74, 0, 1, 4, 1);
            func_801C896C(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            func_801CB340();
            if (same_kind) {
                func_801CD5D0();
            }
            while (D_800625A0->view_motion != 0) {
                func_801CB014();
            }
            func_801CD7E4();
            first = 0;
        }
        if (redraw) {
            func_801CD8D0(gold, total, new_gold);
            redraw = 0;
        }
        switch (D_800625A0->input) {
        case 4:
            if (total != 0) {
                func_801CAC7C(2);
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->scroll_shown = 0;
                running = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                func_801CB13C(0);
                func_801CF678(total);
                if (func_801CBA50(0x95, 0xFF, 1)) {
                    func_801D0C18(new_gold, sell_ids, chosen, n, ids, counts, sell_kinds, same_kind, member);
                } else {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->marker_shown[0] = running;
                }
                func_801CB2FC();
            } else {
                func_801CAC7C(4);
            }
            break;
        case 5:
            running = 0;
            if (total != 0) {
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->scroll_shown = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                func_801CB13C(0);
                if (!func_801CBA50(0x92, 0xFF, 1)) {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->marker_shown[0] = running;
                }
                func_801CB2FC();
            }
            break;
        case 1:
            row++;
            if (row >= 8) {
                top++;
                row = 7;
                if (count - 8 < top) {
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
                redraw = 1;
                total -= price;
                held[index]++;
                new_gold -= price;
                chosen[index]--;
            }
            break;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/shop", func_801D0E68);
#endif

/*
 * Sell list 0: choose a party member by portrait, then sell from that
 * member's accessories. Returns 0 when cancelled, 1 when chosen.
 */
u8 func_801D1658(void) {
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
    func_801CD5D0();
    func_801CB384(0x98);
    do {
        func_801CB014();
        switch (D_800625A0->input) {
        case 5:
            result = 0;
            break;
        case 4:
            func_801CAC7C(2);
            result = 1;
            break;
        case 2:
            cursor--;
            if (cursor < 0) {
                cursor = D_800625A0->details->members_count - 1;
            }
            break;
        case 0:
            cursor++;
            if (cursor >= D_800625A0->details->members_count) {
                cursor = 0;
            }
            break;
        }
        if (cursor != shown) {
            D_800625A0->details->bar_shown[cursor] = 1;
            D_800625A0->details->bar_shown[shown] = 0;
            shown = cursor;
        }
    } while (result == 2);
    func_801CB7F4();
    if (result != 0) {
        member = 0;
        while (cursor != 0) {
            member++;
            if (D_800625A0->member_present[member] != 0) {
                cursor--;
            }
        }
        for (i = 0; i < 8; i++) {
            ids[i] = 0;
            amounts[i] = 1;
        }
        n = 0;
        for (i = 0; i < 3; i++) {
            if (D_8006D8A0[member].accessories[i] != 0) {
                ids[n] = D_8006D8A0[member].accessories[i];
                kinds[n] = 1;
                n++;
            }
        }
        func_801D0E68(8, ids, amounts, 1, 0, kinds, member);
    }
    return result;
}

/* Run the sell list for inventory 1 (200 entries). */
void func_801D18A8(void) {
    func_801D0E68(200, D_8006F4FC, D_8006F4FC - 200, 1, 1, D_8006F4FC - 200, 0);
}

/* Run the sell list for inventory 0 (100 entries). */
void func_801D18E8(void) {
    func_801D0E68(100, D_8006F3D0, D_8006F3D0 - 100, 0, 1, D_8006F3D0 - 100, 0);
}

/* Run the sell list for inventory 2 (150 entries). */
void func_801D1928(void) {
    func_801D0E68(150, D_8006F65A, D_8006F65A - 150, 2, 1, D_8006F65A - 150, 0);
}

/* Hide the shop list's packets; with `close` also close its panels (5 too with `all`), scroll bar and marker. */
void func_801D1968(u8 close, u8 all) {
    s32 i;

    D_800625A0->flags->unk5A = 0;
    D_800625A0->details->label4430_shown = 0;
    D_800625A0->details->label44B0_shown = 0;
    D_800625A0->details->digits_shown = 0;
    D_800625A0->details->heading_count = 0;
    D_800625A0->details->group2D0_count = 0;
    D_800625A0->details->members_count = 0;
    for (i = 0; i < 9; i++) {
        D_800625A0->details->bar_shown[i] = 0;
        D_800625A0->details->cells_a_count[i] = 0;
        D_800625A0->details->cells_b_count[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        D_800625A0->details->name_shown[i] = 0;
        D_800625A0->details->row_count[i] = 0;
    }
    if (close) {
        func_801C88E0(2);
        func_801C88E0(3);
        if (all) {
            func_801C88E0(5);
        }
        func_801C70B8();
        func_801C7314(0);
    }
}

/* Run the chosen sell list (0 a member's equipment, 1-3 the three inventories), then restore the list labels. */
void func_801D1B18(void) {
    u8 close;

    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
    D_800625A0->flags->lists_shown = 0;
    func_801CBC88(0, 4, D_800625A0->list_labels, D_801D1FD0, D_800625A0->flags->list_label_shown);
    close = 1;
    switch (D_800625A0->list_cursor) {
    case 0:
        close = func_801D1658();
        break;
    case 1:
        func_801D18A8();
        break;
    case 2:
        func_801D18E8();
        break;
    case 3:
        func_801D1928();
        break;
    }
    func_801D1968(close, 0);
    D_800625A0->flags->lists_shown = 1;
    D_800625A0->flags->unk4 = 1;
    D_800625A0->flags->cursor_shown = 1;
    func_801CBC88(1, 4, D_800625A0->list_labels, D_801D1FD0, D_800625A0->flags->list_label_shown);
}

/* Command 1 (sell): choose one of the sell lists until cancelled. */
u8 func_801D1CA4(void) {
    u8 running;
    u8 first;

    running = 1;
    first = 1;
    D_800625A0->list_cursor = 3;
    D_800625A0->unk339 = 0xFF;
    do {
        func_801CB014();
        if (first) {
            func_801CBC88(1, 4, D_800625A0->list_labels, D_801D1FD0, D_800625A0->flags->list_label_shown);
            first = 0;
            func_801CB340();
            func_801CC278(0);
        }
        if (D_800625A0->list_cursor != D_800625A0->unk339) {
            func_801CBCF0(4, D_800625A0->list_labels, D_801D1FD0, D_801D1FE8,
                          D_800625A0->flags->list_label_shown, D_800625A0->list_cursor, 3, 0);
            func_801CC720(0);
            D_800625A0->unk339 = D_800625A0->list_cursor;
        }
        switch (D_800625A0->input) {
        case 4:
            func_801CAC7C(2);
            func_801D1B18();
            D_800625A0->unk339 = 0xFF;
            break;
        case 5:
            running = 0;
            break;
        case 1:
            if (D_800625A0->list_cursor != 0) {
                D_800625A0->list_cursor--;
            } else {
                D_800625A0->list_cursor = D_800625A0->list_count - 1;
            }
            break;
        case 3:
            if (++D_800625A0->list_cursor >= D_800625A0->list_count) {
                D_800625A0->list_cursor = 0;
            }
            break;
        }
    } while (running);
    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
    func_801CBC88(0, 4, D_800625A0->list_labels, D_801D1FD0, D_800625A0->flags->list_label_shown);
    return 1;
}

/* Follow-up after a command returns: command 2 redraws its screen. */
void func_801D1F10(void) {
    switch (D_800625A0->top_cursor) {
    case 1:
        break;
    case 2:
        func_801D1968(1, 1);
        break;
    }
}
