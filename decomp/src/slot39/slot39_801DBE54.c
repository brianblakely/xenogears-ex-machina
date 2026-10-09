/* Menu overlay unit from 801DBDB4: the field menu's arts, equipment and
 * status screens, the gear stat computations, the file screen's views and
 * the label builders. Rodata 801C50FC-801C5278, text 801DBDB4-801E8070 and
 * its variables 801EA724-801EA8F4. Named after 801DBE54, where it started
 * before its variables moved the boundary. Its rodata starts at 801C50FC,
 * 4 mod 8 (docs/matching.md, jump tables), where 801DBE54's table sits;
 * func_801CD2AC is the last function using the previous unit's rodata. Its
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
static s16 D_801EA724; /* item list scroll bar */
static s32 D_801EA728;
static s16 D_801EA72C;
static u8 D_801EA730[200]; /* equipment list entry ids */
static u8 D_801EA7F8[200]; /* equipment list entry counts */
static u8 D_801EA8C0;      /* the last printed character was two-byte */
static u8 D_801EA8C4[0x20]; /* icon palette buffer */
static RECT D_801EA8E4;     /* icon image area */
static RECT D_801EA8EC;     /* icon palette area */

/* Size the item list's scroll bar from the last occupied inventory entry. */
void func_801DBDB4(void) {
    s32 i;
    s32 last;
    s32 pages;

    for (i = 0; i < 150; i++) {
        if (D_8006D634.itemIds[i] != 0) {
            last = i;
        }
    }
    if (last < 16) {
        D_801EA724 = 0x74;
        D_801EA728 = 0;
        D_801EA72C = 0;
    } else {
        pages = (last - 16) / 2 + 1;
        D_801EA724 = 0x4a;
        D_801EA728 = pages;
        D_801EA72C = 0x1068 / pages;
    }
}

/* The item screen: a two-column list of eight rows scrolled over the
 * inventory with a cursor, the selected entry's description and its
 * windows. Confirm selects an entry, uses it when selected again or swaps
 * it with the selected one; cancel clears the selection or leaves. */
u8 func_801DBE54(void) {
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
    func_801DA4A8();
    func_801DBDB4();
    func_801DB02C(0);
    func_801DB02C(1);
    while (running) {
        func_801C7BF4();
        if (scroll != scrollShown) {
            func_801DA5BC(scroll);
            func_801D3344(0xc, (u16)D_801EA72C * scroll / 100 + 0x12, (u16)D_801EA724);
            scrollShown = scroll;
        }
        func_801DB0A8(cursor, scroll, 0, 0);
        if (cursor != cursorShown) {
            func_801DA9A8(cursor, scroll);
            cursorShown = cursor;
        }
        if (windows) {
            func_801D397C(3, 0xc, 0xa, 0x124, 0x84, 0, 1, 4, 1);
            func_801D397C(4, 8, 0x8f, 0x130, 0x22, 0, 1, 4, 0);
            windows = 0;
            func_801D1E80();
            func_801D29A8(0, 0);
        }
        func_801DB0A8(selected, scroll, 1, 1);
        switch (D_800625A0->input) {
        case 4:
            if (selected == 0xff) {
                selected = scroll * 2 + cursor;
            } else {
                if (scroll * 2 + cursor == selected) {
                    if (func_801DB920(scroll, cursor)) {
                        scrollShown = 0xff;
                        cursorShown = 0xff;
                    }
                } else {
                    func_801DBD4C(scroll * 2 + cursor, selected);
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
                if (D_801EA728 < ++scroll) {
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
                if (D_801EA728 < ++scroll) {
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
            if (D_801EA728 < scroll) {
                scroll = D_801EA728;
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
    func_801D2484();
    func_801DB340(0);
    func_801DB340(1);
    func_801E8044(8, D_800625A0->flags->labels10e0_shown);
    return 1;
}

/* Open the arts screen of `kind` (0 the character's arts, data view 2; 1 its
 * gear's, view 5; 2 the gear's other list, view 6, with the second label
 * page): its labels, its 1094-byte list block and the view's data. */
void func_801DC1D4(u8 kind) {
    void *block;
    u8 view;
    s32 page;

    page = 0;
    block = func_80031BDC(0x1094, 0);
    D_800625A0->arts_list = block;
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
    func_801E8018(8, D_800625A0->labels10e0, D_801EA548 + page * 8, D_800625A0->flags->labels10e0_shown);
    func_801C72BC(view);
    func_801D22F4(2);
    func_801D3488(2, kind);
}

/* Close the arts screen of `kind`: panels 3-6, its list block and the
 * view's data (the view | 10 releases it). */
void func_801DC2CC(u8 kind) {
    u8 view;

    func_801D4EA0(3);
    func_801D4EA0(4);
    func_801D4EA0(5);
    func_801D4EA0(6);
    func_801D2484();
    D_800625A0->flags->arts_list_shown = 0;
    func_801C7BF4();
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
    func_801C72BC(view | 0x10);
    func_800320E8(D_800625A0->arts_list);
    D_800625A0->flags->field_menu2_shown = 1;
    D_800625A0->flags->panels_shown[1] = 1;
}

/* Build the arts list of party slot `slot` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list): each known art's name and cost, then the
 * current and maximum ether (fuel for kind 2) in rows 12 and 13; arts that
 * cannot be used now are greyed. */
void func_801DC3D8(u8 slot, u8 kind) {
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
    ep = D_8006D634.characters[D_800625A0->flags->party[slot]].ep;
    image = func_80031BDC(0x3f6, 0);
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
            known = func_801C8640(D_8006D634.skills[D_800625A0->flags->party[slot]].levelSkills, row) || row >= 12;
            break;
        case 1:
            known = func_801C8640(D_8006D634.skills[D_800625A0->flags->party[slot]].unlocksB, row) || row >= 12;
            break;
        case 2:
            if (!(row & 1)) {
                known = func_801C8640(D_8006D634.skills[D_800625A0->flags->party[slot]].flags1A, row / 2) || row >= 12;
            }
            break;
        }
        if (known) {
            digits = 2;
            if (row < 12) {
                switch (kind) {
                case 0:
                    D_800625A0->arts_list->names[row].width = func_80034EAC(
                        func_80033908(D_800625A0->flags->party[slot] * 16 + row), image, 0x24, 0);
                    cost = D_800625A0->tables->arts[D_800625A0->flags->party[slot]][22 + row].cost;
                    break;
                case 1:
                    D_800625A0->arts_list->names[row].width = func_80034EAC(
                        func_800339FC(D_8006D634.characters[D_800625A0->flags->party[slot]].gearId * 16 + row), image, 0x24, 0);
                    cost = (D_800625A0->tables->arts + 11)[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId][21 + row].cost;
                    break;
                case 2:
                    D_800625A0->arts_list->names[row].width = func_80034EAC(
                        func_80033A8C(D_8006D634.characters[D_800625A0->flags->party[slot]].gearId * 4 + row / 2), image, 0x24, 0);
                    ep = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].fuel;
                    cost = (D_800625A0->tables->arts + 11)[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId][37 + row / 2].gearCost;
                    digits = 4;
                    break;
                }
            } else if (row == 12) {
                if (kind != 2) {
                    cost = D_8006D634.characters[D_800625A0->flags->party[slot]].ep;
                } else {
                    fuel = 3;
                    cost = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].fuel;
                    digits = 5;
                }
            } else {
                cost = D_8006D634.characters[D_800625A0->flags->party[slot]].maxEp;
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
            func_80033B34(codes, text, digits);
            grey = 0x80;
            D_800625A0->arts_list->values[row].width = func_80034EAC(text, image, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = row / 2 * 0xd + 0x80;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)image);
            DrawSync(0);
            if (row < 12) {
                switch (kind) {
                case 0:
                    if (func_801C865C(D_801E97F0[D_800625A0->flags->party[slot]], row)) {
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
                func_801E7C50(&D_800625A0->arts_list->names[row], row, 0x80, grey | 1);
                func_801C851C(D_800625A0->arts_list->names[row].verts, (row % 2 * 0x88 + 0x24) & 0xfffc,
                              (row / 2 * 0x10 + 0x12) & 0xfffe, D_800625A0->arts_list->names[row].width, 0xd);
            }
            func_801E7C50(&D_800625A0->arts_list->values[row], row, 0x80, grey | 2);
            func_801C851C(D_800625A0->arts_list->values[row].verts, D_801E9DDC[row], D_801E9E14[row],
                          D_800625A0->arts_list->values[row].width, 0xd);
            D_800625A0->arts_list->names[row].buffer = D_800625A0->buffer_index;
            D_800625A0->arts_list->values[row].buffer = D_800625A0->buffer_index;
            D_800625A0->arts_list->shown[row] = grey | 1;
        } else {
            D_800625A0->arts_list->shown[row] = 0;
        }
    }
    func_800320E8(image);
    func_801E8070(8, D_800625A0->labels10e0, D_801EA550, D_801E9EA0, D_800625A0->flags->labels10e0_shown, 6, 1, fuel + 2);
    func_801E8070(8, D_800625A0->labels10e0, D_801EA550, D_801E9EA0, D_800625A0->flags->labels10e0_shown, 7, 1, fuel + 2);
    func_801D36E0(&D_800625A0->arts_list->footer, slot, kind, 1);
    D_800625A0->flags->arts_list_shown = 1;
}

/* Show the description of arts list row `row` for party slot `slot` (`kind`
 * 0 the character's, 1 the gear's, 2 the gear's paired rows): the entry's
 * two text lines, a copy of its name and its target labels; an unused row
 * hides them. */
void func_801DCE60(u8 slot, u8 row, u8 kind) {
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
        text = (D_800625A0->flags->party[slot] << 5) + row * 2;
        break;
    case 1:
        text = (D_8006D634.characters[D_800625A0->flags->party[slot]].gearId << 5) + row * 2;
        break;
    case 2:
        text = D_8006D634.characters[D_800625A0->flags->party[slot]].gearId * 8 + (row & ~1);
        break;
    }
    if (D_800625A0->arts_list->shown[row] != 0) {
        image = func_80031BDC(0x618, 0);
        bzero(image, 0x618);
        D_800625A0->arts_list->extra[0].width =
            func_80034EAC(func_80033728(D_800625A0->arts_list->texts, text), image, 0x39, 0);
        D_800625A0->arts_list->extra[1].width =
            func_80034EAC(func_80033728(D_800625A0->arts_list->texts, text + 1), image, 0x39, 1);
        rect.x = 0x140;
        rect.y = 0x4e;
        rect.w = 0x3c;
        rect.h = 0xd;
        LoadImage(&rect, (u_long *)image);
        DrawSync(0);
        func_801E7C50(&D_800625A0->arts_list->extra[0], 0, 0, 0);
        func_801E920C(&D_800625A0->arts_list->extra[0].polys[D_800625A0->buffer_index], 0x1c, 0x9e, 0, 0x4e,
                      D_800625A0->arts_list->extra[0].width, 0xd);
        func_801C851C(D_800625A0->arts_list->extra[0].verts, 0x1c, 0x9e, D_800625A0->arts_list->extra[0].width, 0xd);
        func_801E7C50(&D_800625A0->arts_list->extra[1], 1, 0, 0);
        func_801E920C(&D_800625A0->arts_list->extra[1].polys[D_800625A0->buffer_index], 0x1c, 0xae, 0, 0x4e,
                      D_800625A0->arts_list->extra[1].width, 0xd);
        func_801C851C(D_800625A0->arts_list->extra[1].verts, 0x1c, 0xae, D_800625A0->arts_list->extra[1].width, 0xd);
        func_800320E8(image);
        memmove(&D_800625A0->arts_list->headA, &D_800625A0->arts_list->names[row], sizeof(MenuLabel));
        func_801C851C(D_800625A0->arts_list->headA.verts, 0x12, 0x8e, D_800625A0->arts_list->names[row].width, 0xd);
        (D_800625A0->arts_list->headA.polys + D_800625A0->buffer_index)->r0 = 0x80;
        (D_800625A0->arts_list->headA.polys + D_800625A0->buffer_index)->g0 = 0x80;
        (D_800625A0->arts_list->headA.polys + D_800625A0->buffer_index)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->arts_list->headA.polys[D_800625A0->buffer_index], 0);
        func_801E8044(8, D_800625A0->flags->labels10e0_shown);
        switch (kind) {
        case 0:
            effect = D_800625A0->tables->arts[D_800625A0->flags->party[slot]] + row + 22;
            break;
        case 1:
            effect = (D_800625A0->tables->arts + 11)[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId] + row + 21;
            break;
        case 2:
            effect = (D_800625A0->tables->arts + 11)[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId] + (row >> 1) + 37;
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
        func_801E8070(8, D_800625A0->labels10e0, D_801EA550, D_801E9EA0, D_800625A0->flags->labels10e0_shown, all, 0, 2);
        targetKind = (effect->target & 3) + 3;
        func_801E8070(8, D_800625A0->labels10e0, D_801EA550, D_801E9EA0, D_800625A0->flags->labels10e0_shown, targetKind, 0, 2);
        D_800625A0->arts_list->headA.buffer = D_800625A0->buffer_index;
        D_800625A0->arts_list->headB.buffer = D_800625A0->buffer_index;
        D_800625A0->arts_list->extra[0].buffer = D_800625A0->buffer_index;
        D_800625A0->arts_list->extra[1].buffer = D_800625A0->buffer_index;
        D_800625A0->arts_list->extraShown = 1;
        D_800625A0->flags->labels10e0_shown[6] = 1;
        D_800625A0->flags->labels10e0_shown[7] = 1;
    } else {
        D_800625A0->arts_list->extraShown = 0;
        for (i = 0; i < 6; i++) {
            D_800625A0->flags->labels10e0_shown[i] = 0;
        }
    }
}

/* Shade the file list screen's texts by `mode` (801e8eac): windows 5 and 6,
 * each built name and value of the first twelve rows, the cursor, the
 * heading and the two extra labels. */
void func_801DD5E8(u8 mode) {
    s32 i;

    func_801E8F60(5, mode);
    func_801E8F60(6, mode);
    for (i = 0; i < 12; i++) {
        if (D_800625A0->arts_list->names[i].polys[D_800625A0->arts_list->names[i].buffer].r0 != 0x20) {
            func_801E8EAC(&D_800625A0->arts_list->names[i].polys[D_800625A0->arts_list->names[i].buffer], mode);
            func_801E8EAC(&D_800625A0->arts_list->values[i].polys[D_800625A0->arts_list->values[i].buffer], mode);
        }
    }
    func_801E8EAC(&D_800625A0->cursors[0]->polys[D_800625A0->cursors[0]->buffer], mode);
    func_801E8EAC(&D_800625A0->arts_list->headA.polys[D_800625A0->arts_list->headA.buffer], mode);
    for (i = 0; i < 2; i++) {
        func_801E8EAC(&D_800625A0->arts_list->extra[i].polys[D_800625A0->arts_list->extra[i].buffer], mode);
    }
}

/* Use art `row` of party slot `slot` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list) from the menu: select the targets (the
 * whole party for all-target arts, else the cursor's slot) and use it on
 * confirm while its cost can be paid, until cancelled. */
void func_801DD790(u8 slot, s32 row, u8 kind) {
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
    cursor = D_800625A0->first_member;
    switch (kind) {
    case 0:
        effect = D_800625A0->tables->arts[D_800625A0->flags->party[slot]] + row + 22;
        break;
    case 1:
        effect = (D_800625A0->tables->arts + 11)[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId] + row + 21;
        break;
    case 2:
        x = 0x18;
        effect = (D_800625A0->tables->arts + 11)[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId] + row + 37;
        cursor = slot;
        break;
    }
    func_801D397C(2, 0x10, 0xe, x + 0x90, 0xb0, 0, 0, 4, 0);
    all = effect->target & 1;
    while (targets) {
        func_801C7BF4();
        if (redraw) {
            func_801DC3D8(slot, kind);
            func_801DCE60(slot, row, kind);
            func_801DD5E8(1);
            func_801DB5E4(kind);
            redraw = 0;
        }
        targets = 0;
        D_800625A0->markers->shown[0] = D_800625A0->markers->shown[1] = D_800625A0->markers->shown[2] = 0;
        if (all) {
            for (i = 0; i < 3; i++) {
                if (D_800625A0->flags->party[i] != 0xff) {
                    targets |= 1 << i;
                    D_800625A0->markers->shown[i] = 1;
                }
            }
        } else {
            targets = 1 << cursor;
            D_800625A0->markers->shown[cursor] = 1;
        }
        D_800625A0->flags->markers_shown = 1;
        if (kind != 2) {
            if (D_8006D634.characters[D_800625A0->flags->party[slot]].ep - effect->cost < 0) {
                targets = 0;
            }
        } else {
            if (D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].fuel - effect->gearCost < 0) {
                targets = 0;
            }
        }
        if (!targets) {
            break;
        }
        switch (D_800625A0->input) {
        case 5:
            targets = 0;
            break;
        case 4:
            used = 0;
            for (i = 0; i < 3; i++) {
                hit = 0;
                if (func_801C865C(targets, i)) {
                    if (kind != 2) {
                        if (D_8006D634.characters[D_800625A0->flags->party[i]].hp != D_8006D634.characters[D_800625A0->flags->party[i]].maxHp) {
                            used = 1;
                            hit = 1;
                        }
                    } else {
                        if (D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[i]].gearId].hp !=
                            D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[row]].gearId].maxHp) {
                            used = 1;
                            hit = 1;
                        }
                    }
                    if (hit) {
                        func_801E35BC(D_800625A0->tables, D_800625A0->flags->party[slot], D_800625A0->flags->party[i],
                                      row, kind);
                    }
                }
            }
            if (used) {
                if (kind != 2) {
                    D_8006D634.characters[D_800625A0->flags->party[slot]].ep -= effect->cost;
                } else {
                    D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].fuel -= effect->gearCost;
                }
                sound = 0x37;
            } else {
                sound = 4;
            }
            redraw = 1;
            func_801C8574(sound);
            break;
        case 1:
            if (kind == 0) {
                cursor = func_801D9704(cursor, 0, 0);
            }
            break;
        case 3:
            if (kind == 0) {
                cursor = func_801D9704(cursor, 1, 0);
            }
            break;
        }
    }
    D_800625A0->markers->shown[0] = D_800625A0->markers->shown[1] = D_800625A0->markers->shown[2] = 0;
    func_801DD5E8(0);
    D_800625A0->flags->party_panels_shown = 0;
    func_801C7BF4();
    if (D_801E9785 != 0) {
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->party_panels[i]);
        }
        D_801E9785 = 0;
    }
    func_801D4EA0(2);
}

/* The arts screen's windows are up: clear `windows`, zoom in when `zoom`
 * asks for it and drop the panel redraw flags. A statement macro. */
#define FINISH_OPENING(windows, zoom)                  \
    do {                                               \
        (windows) = 0;                                 \
        if (zoom) {                                    \
            func_801D1E80();                           \
            func_801D29A8(0, 0);                       \
            func_801C7BF4();                           \
        }                                              \
        D_800625A0->flags->field_menu2_shown = 0;      \
        D_800625A0->flags->panels_shown[1] = 0;        \
    } while (0)

/* The arts screen of party slot `member` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list): a cursor over twelve rows (two columns,
 * one for kind 2), the selected art's description; confirm uses a usable
 * art, 9/10 switch party members, cancel leaves. `zoom` first zooms in. `slot` is
 * the member currently shown. */
void func_801DDF24(u8 member, u8 zoom, u8 kind) {
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
    func_801DC1D4(kind);
    func_801DB02C(0);
    do {
        func_801C7BF4();
        if (slot != slotShown) {
            func_801DC3D8(slot, kind);
            slotShown = slot;
            cursorShown = 0xff;
        }
        func_801DB0A8(cursor, 0, 2, 0);
        if (cursor != cursorShown) {
            func_801DCE60(slot, cursor, kind);
            cursorShown = cursor;
        }
        if (windows) {
            func_801D397C(6, 0x10, 0xa, D_801E9788[kind], 0x70, 0, 1, 4, 0);
            func_801D397C(5, 0xc, 0x86, 0xac, 0x38, 0, 1, 4, 0);
            func_801D397C(4, D_801E9794[kind], 0xa6, D_801E97A0[kind], 0x18, 0, 1, 4, 0);
            func_801D397C(3, 0xc8, 0x86, 0x50, 0x18, 0, 1, 4, 0);
            FINISH_OPENING(windows, zoom);
        }
        switch (D_800625A0->input) {
        case 4:
            if (D_800625A0->arts_list->shown[cursor] & 0x80) {
                func_801DD790(slot, cursor, kind);
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
            slot = func_801D9704(slot, 0, kind);
            break;
        case 10:
            slot = func_801D9704(slot, 1, kind);
            break;
        }
    } while (running);
    func_801E8044(8, D_800625A0->flags->labels10e0_shown);
    func_801DB340(0);
}

/* Run the 801ddf24 screen for party slot `slot`; always continues the menu. */
u8 func_801DE29C(u8 slot, u8 arg1) {
    func_801DDF24(slot, arg1, 0);
    return 1;
}

/* Open the 801ddf24 screen on page `page`: its block, labels and view 7. */
void func_801DE2C8(u8 page) {
    void *block;

    block = func_80031BDC(0xa1c, 0);
    D_800625A0->equip_list = block;
    bzero(block, 0xa1c);
    func_801E8018(6, D_800625A0->labels14e0, D_801EA558 + page * 6, D_800625A0->flags->labels14e0_shown);
    func_801C72BC(7);
    func_801D22F4(3);
    func_801DB02C(0);
    func_801D3488(1, page);
}

/* Close the 801ddf24 screen: panels 2-5, its labels and block (+434); view 17. */
void func_801DE36C(void) {
    D_800625A0->flags->equip_list_shown = 0;
    func_801D4EA0(2);
    func_801D4EA0(3);
    func_801D4EA0(4);
    func_801D4EA0(5);
    func_801C7BF4();
    func_801E8044(6, D_800625A0->flags->labels14e0_shown);
    func_801C72BC(0x17);
    func_800320E8(D_800625A0->equip_list);
    func_801D3444();
}

/* Close the 801ddf24 screen: hide its sprites and free its blocks. */
void func_801DE400(void) {
    D_800625A0->flags->equipment_shown = 0;
    D_800625A0->flags->equip_labels_shown = 0;
    func_801C7BF4();
    func_800320E8(D_800625A0->equip_panel);
    func_800320E8(D_800625A0->equip_labels);
}

/* Lay out the page `page` labels of the 801ddf24 screen (rows 2-5 when
 * `wide`, else 0-1) and its window. */
void func_801DE474(u8 wide, u8 page) {
    s32 i;
    s32 first;
    s32 end;
    s32 h;

    for (i = 0; i < 6; i++) {
        D_800625A0->flags->labels14e0_shown[i] = 0;
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
        func_801E8070(6, D_800625A0->labels14e0, D_801EA558 + page * 6, D_801E9EA0, D_800625A0->flags->labels14e0_shown, i,
                      i, 3);
    }
    if (D_800625A0->flags->panels_shown[4] != 0) {
        func_801D4EA0(4);
    }
    func_801D397C(4, 0x10, 0xc, 0x80, h, 0, 1, 4, 0);
}

/* Build the equipment candidate list for part `part` of party slot `slot`
 * (`special` special parts, `gear` the gear's lists) and draw its rows from
 * `top`: usable weapons of a class below 5, special parts of the kept part's
 * class, or accessories whose groups are free or held by the replaced one
 * (entry 0 of the accessory list stays empty for removing). Returns the
 * scroll limit. */
s32 func_801DE5CC(u8 slot, s32 top, s32 part, u8 special, u8 gear) {
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
            keptWeapon = &D_800625A0->tables->equipment[D_800625A0->equip_labels->parts[0][part]];
        } else {
            kind = part + 1;
            count = 100;
            keptGearWeapon = &D_800625A0->tables->gear_weapons[D_800625A0->equip_labels->parts[0][part]];
        }
    } else if (!gear) {
        if (part != 0) {
            kind = 5;
            count = 200;
            accessory = &D_800625A0->tables->accessories[D_800625A0->equip_labels->parts[2][part - 1]];
            own = accessory->groups;
            for (i = 0, used = 0; i < 3; i++) {
                accessory = &D_800625A0->tables->accessories[D_800625A0->equip_labels->parts[2][i]];
                used |= accessory->groups;
            }
        } else {
            kind = 0;
            count = 100;
        }
    } else if (part != 0) {
        kind = 5;
        count = 150;
        gearAccessory = &D_800625A0->tables->gear_accessories[D_800625A0->equip_labels->parts[2][part - 1]];
        own = gearAccessory->groups;
        for (i = 0, used = 0; i < 3; i++) {
            gearAccessory = &D_800625A0->tables->gear_accessories[D_800625A0->equip_labels->parts[2][i]];
            used |= gearAccessory->groups;
        }
    } else {
        kind = 0;
        count = 100;
    }
    for (i = 0; i < 200; i++) {
        D_801EA730[i] = 0;
        D_801EA7F8[i] = 0;
    }
    length = kind == 5;
    for (i = 0; i < count; i++) {
        if (!gear) {
            accessory = &D_800625A0->tables->accessories[D_8006D634.accessoryIds[i]];
            weapon = &D_800625A0->tables->equipment[D_8006D634.weaponIds[i]];
        } else {
            gearWeapon = &D_800625A0->tables->gear_weapons[D_8006D634.gearPartIds[i]];
            gearAccessory = &D_800625A0->tables->gear_accessories[D_8006D634.gearAccessoryIds[i]];
        }
        ok = 0;
        if (!gear) {
            switch (kind) {
            case 0:
                if (func_801C865C(weapon->users, D_800625A0->flags->party[slot]) && weapon->kind < 5 &&
                    D_8006D634.weaponIds[i] < 50) {
                    ok = 1;
                }
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                if (func_801C865C(weapon->users, D_800625A0->flags->party[slot]) &&
                    weapon->kind == keptWeapon->kind && D_8006D634.weaponIds[i] >= 50) {
                    ok = 1;
                }
                break;
            case 5:
                if (func_801C865C(accessory->users, D_800625A0->flags->party[slot])) {
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
                if (func_801C8678(gearWeapon->users, D_8006D634.characters[D_800625A0->flags->party[slot]].gearId) &&
                    gearWeapon->kind < 5 && D_8006D634.gearPartIds[i] < 50) {
                    ok = 1;
                }
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                if (func_801C8678(gearWeapon->users, D_8006D634.characters[D_800625A0->flags->party[slot]].gearId) &&
                    gearWeapon->kind == keptGearWeapon->kind && D_8006D634.gearPartIds[i] >= 50) {
                    ok = 1;
                }
                break;
            case 5:
                if (func_801C8678(gearAccessory->users, D_8006D634.characters[D_800625A0->flags->party[slot]].gearId)) {
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
                    D_801EA730[length] = D_8006D634.weaponIds[i];
                    D_801EA7F8[length] = D_8006D634.weaponCounts[i];
                } else {
                    D_801EA730[length] = D_8006D634.accessoryIds[i];
                    D_801EA7F8[length] = D_8006D634.accessoryCounts[i];
                }
            } else if (kind != 5) {
                D_801EA730[length] = D_8006D634.gearPartIds[i];
                D_801EA7F8[length] = D_8006D634.gearPartCounts[i];
            } else {
                D_801EA730[length] = D_8006D634.gearAccessoryIds[i];
                D_801EA7F8[length] = D_8006D634.gearAccessoryCounts[i];
            }
            length++;
        }
    }
    image = func_80031BDC(0x3f6, 0);
    for (i = 0; i < 8; i++) {
        if (D_801EA730[top + i] != 0) {
            if (!gear) {
                if (kind != 5) {
                    D_800625A0->equip_list->names[i].width = func_80034EAC(func_80033848(D_801EA730[top + i]), image, 0x24, 0);
                } else {
                    D_800625A0->equip_list->names[i].width = func_80034EAC(func_800337E8(D_801EA730[top + i]), image, 0x24, 0);
                }
            } else if (kind != 5) {
                D_800625A0->equip_list->names[i].width = func_80034EAC(func_80033A5C(D_801EA730[top + i]), image, 0x24, 0);
            } else {
                D_800625A0->equip_list->names[i].width = func_80034EAC(func_80033A2C(D_801EA730[top + i]), image, 0x24, 0);
            }
            tens = D_801EA7F8[top + i] / 10;
            codes[0] = tens != 0 ? tens + 0x10 : 0xc3;
            codes[2] = D_801EA7F8[top + i] % 10 + 0x10;
            func_80033B34(codes, text, 2);
            D_800625A0->equip_list->values[i].width = func_80034EAC(text, image, 0x24, 1);
            rect.x = (i & 1) * 0x18 + 0x180;
            rect.y = i / 2 * 0xd + 0x80;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)image);
            DrawSync(0);
            func_801E7C50(&D_800625A0->equip_list->names[i], i, 0x80, 0x81);
            func_801E7C50(&D_800625A0->equip_list->values[i], i, 0x80, 0x82);
            func_801C851C(D_800625A0->equip_list->names[i].verts, 0xa8, i * 0xd + 0x12,
                          D_800625A0->equip_list->names[i].width, 0xd);
            func_801C851C(D_800625A0->equip_list->values[i].verts, 0x10c, i * 0xd + 0x12,
                          D_800625A0->equip_list->values[i].width, 0xd);
            D_800625A0->equip_list->names[i].buffer = D_800625A0->buffer_index;
            D_800625A0->equip_list->values[i].buffer = D_800625A0->buffer_index;
            D_800625A0->equip_list->shown[i] = 1;
        } else {
            D_800625A0->equip_list->shown[i] = 0;
        }
    }
    func_800320E8(image);
    func_801D36E0(&D_800625A0->equip_list->title, slot, gear, 0);
    length -= 8;
    D_800625A0->flags->equip_list_shown = 1;
    if (length < 0) {
        length = 0;
    }
    return length;
}

/* Commit the equipment change of part `part` of party slot `slot` (with
 * `special` a special part, with `gear` the gear's): the newly equipped part
 * leaves its inventory list and the replaced one kept by 801df5d0 joins it
 * (worn special parts are dropped). Without a new part the kept one goes
 * back. Returns 1 when character 4 changed weapon. */
s32 func_801DF0D4(u8 slot, u8 part, u8 special, u8 gear) {
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
        ids = D_8006D634.weaponIds;
        counts = ids - 100;
    } else {
        ids = D_8006D634.gearPartIds;
        counts = ids - 100;
    }
    length = 100;
    if (special) {
        if (!gear) {
            kept = D_800625A0->equip_labels->parts[1][part];
            at = &D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[part];
            selected = *at;
            if (selected == 0) {
                *at = kept;
                swap = 0;
            } else {
                if (D_8006F8BA[*at] < 100) {
                    kept = 0;
                }
                D_8006F8BA[*at] = 100;
            }
        } else {
            kept = D_800625A0->equip_labels->parts[1][part];
            at = &D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[part];
            selected = *at;
            if (selected == 0) {
                *at = kept;
                swap = 0;
            } else {
                if (GEAR_PART_DURABILITY[*at] < 100) {
                    kept = 0;
                }
                GEAR_PART_DURABILITY[*at] = 100;
            }
        }
    } else if (!gear) {
        if (part == 0) {
            id = D_800625A0->flags->party[slot];
            selected = D_8006D634.characters[id].weapons[0];
            kept = D_800625A0->equip_labels->parts[0][0];
            if (selected == 0) {
                D_8006D634.characters[id].weapons[0] = kept;
                swap = 0;
            } else if (id == 4) {
                result = 1;
            }
        } else {
            selected = D_8006D634.characters[D_800625A0->flags->party[slot]].accessories[part - 1];
            ids = D_8006D634.accessoryIds;
            counts = D_8006D634.accessoryCounts;
            kept = D_800625A0->equip_labels->parts[2][part - 1];
            length = 200;
        }
    } else {
        if (part == 0) {
            id = D_800625A0->flags->party[slot];
            selected = D_8006D634.gears[D_8006D634.characters[id].gearId].weapons[0];
            kept = D_800625A0->equip_labels->parts[0][0];
            if (selected == 0) {
                D_8006D634.gears[D_8006D634.characters[id].gearId].weapons[0] = kept;
                swap = 0;
            } else if (id == 4) {
                result = 1;
            }
        } else {
            selected = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].parts[part - 1];
            ids = D_8006D634.gearAccessoryIds;
            counts = D_8006D634.gearAccessoryCounts;
            length = 150;
            kept = D_800625A0->equip_labels->parts[2][part - 1];
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

/* Keep the shown stats and the equipment of party slot `slot` (its gear's
 * parts when `gear`) in the equipment screen's block. */
void func_801DF5D0(u8 slot, u8 gear) {
    s32 i;

    for (i = 0; i < 9; i++) {
        D_800625A0->equip_labels->stats[i] = D_800625A0->tables->stats[i];
    }
    if (!gear) {
        for (i = 0; i < 5; i++) {
            D_800625A0->equip_labels->parts[0][i] = D_8006D634.characters[D_800625A0->flags->party[slot]].weapons[i];
            D_800625A0->equip_labels->parts[1][i] = D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[i];
            D_800625A0->equip_labels->parts[2][i] = D_8006D634.characters[D_800625A0->flags->party[slot]].accessories[i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            D_800625A0->equip_labels->parts[0][i] = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].weapons[i];
            D_800625A0->equip_labels->parts[1][i] = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[i];
            D_800625A0->equip_labels->parts[2][i] = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].parts[i];
        }
    }
}

/* Put back the equipment of party slot `slot` (its gear's parts when `gear`)
 * kept by 801df5d0; the equipment screen was cancelled. */
void func_801DF890(u8 slot, u8 gear) {
    s32 i;

    if (!gear) {
        for (i = 0; i < 5; i++) {
            D_8006D634.characters[D_800625A0->flags->party[slot]].weapons[i] = D_800625A0->equip_labels->parts[0][i];
            D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[i] = D_800625A0->equip_labels->parts[1][i];
        }
        for (i = 0; i < 3; i++) {
            D_8006D634.characters[D_800625A0->flags->party[slot]].accessories[i] = D_800625A0->equip_labels->parts[2][i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].weapons[i] = D_800625A0->equip_labels->parts[0][i];
            D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[i] = D_800625A0->equip_labels->parts[1][i];
        }
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].parts[i] = D_800625A0->equip_labels->parts[2][i];
        }
    }
}

/* Try list entry `top` + `row` on part `part` of party slot `slot`: the
 * weapon (0) or an accessory (1-3), with `special` a special part; with
 * `gear` the gear's parts. */
void func_801DFB68(u8 slot, s32 part, s32 row, s32 top, u8 special, u8 gear) {
    if (!gear) {
        if (!special) {
            switch (part) {
            case 0:
                D_8006D634.characters[D_800625A0->flags->party[slot]].weapons[0] = D_801EA730[top + row];
                break;
            case 1:
            case 2:
            case 3:
                D_8006D634.characters[D_800625A0->flags->party[slot]].accessories[part - 1] = D_801EA730[top + row];
                break;
            }
        } else {
            D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[part] = D_801EA730[top + row];
        }
    } else {
        if (!special) {
            switch (part) {
            case 0:
                D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].weapons[0] = D_801EA730[top + row];
                break;
            case 1:
            case 2:
            case 3:
                D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].parts[part - 1] = D_801EA730[top + row];
                break;
            }
        } else {
            D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[part] = D_801EA730[top + row];
        }
    }
}

/* Compute party slot `slot`'s gear stats and copy them to the shown values. */
void func_801DFE2C(u8 slot) {
    func_801E3ECC(D_800625A0->tables, D_8006D634.characters[D_800625A0->flags->party[slot]].gearId);
    func_801E3C2C(D_800625A0->tables, D_8006D634.characters[D_800625A0->flags->party[slot]].gearId);
    D_800625A0->tables->stats[0] = D_800625A0->tables->gear.attack;
    D_800625A0->tables->stats[1] = D_800625A0->tables->gear.defense;
    D_800625A0->tables->stats[2] = D_800625A0->tables->gear.ether_defense;
    D_800625A0->tables->stats[3] = D_800625A0->tables->gear.hit;
    D_800625A0->tables->stats[4] = D_800625A0->tables->gear.speed;
    D_800625A0->tables->stats[5] = D_800625A0->tables->gear.frame_factor;
}

/* Show the three-line description of equipment list entry `top` + `row` (or,
 * with `current`, of the part equipped) for part `part` of party slot
 * `slot` (`special` a special part, `gear` the gear's parts). */
void func_801DFF5C(s32 part, s32 row, s32 top, u8 special, u8 gear, u8 current, u8 slot) {
    RECT rect;
    u8 *table;
    u8 *image;
    s32 line;
    u8 kind;
    u16 id;

    id = D_801EA730[top + row];
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
            table = D_800625A0->equip_list->texts[0];
            if (current) {
                if (!special) {
                    id = D_8006D634.characters[D_800625A0->flags->party[slot]].weapons[0];
                } else {
                    id = D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[part];
                }
            }
            break;
        case 1:
            table = D_800625A0->equip_list->texts[1];
            if (current) {
                id = D_8006D634.characters[D_800625A0->flags->party[slot]].accessories[part - 1];
            }
            break;
        case 2:
            table = D_800625A0->equip_list->texts[2];
            if (current) {
                if (!special) {
                    id = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].weapons[0];
                } else {
                    id = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[part];
                }
            }
            break;
        case 3:
            table = D_800625A0->equip_list->texts[3];
            if (current) {
                id = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].parts[part - 1];
            }
            break;
        }
        if (id != 0) {
            image = func_80031BDC(0x3f6, 0);
            bzero(image, 0x3f6);
            for (line = 0; line < 3; line++) {
                D_800625A0->equip_list->extra[line].width =
                    func_80034EAC(func_80033728(table, id * 3 + line), image, 0x24, 0);
                rect.x = ((line + 8) & 1) * 0x18 + 0x180;
                rect.y = (line + 8) / 2 * 0xd + 0x80;
                rect.w = 0x28;
                rect.h = 0xd;
                LoadImage(&rect, (u_long *)image);
                DrawSync(0);
                func_801E7C50(&D_800625A0->equip_list->extra[line], line + 8, 0x80, 0x81);
                func_801C851C(D_800625A0->equip_list->extra[line].verts, 0x10, (u16)(line * 0x10 + 0x96) / 2 * 2,
                              D_800625A0->equip_list->extra[line].width, 0xd);
                D_800625A0->equip_list->extra[line].buffer = D_800625A0->buffer_index;
            }
            D_800625A0->equip_list->extraShown = 1;
            func_800320E8(image);
            return;
        }
    }
    D_800625A0->equip_list->extraShown = 0;
}

/* Return party slot `slot`'s accessory (or with `gear` its gear's part) to
 * its inventory list: add one to the entry holding it (at most 99), or put
 * it into the first free entry. */
void func_801E0434(u8 slot, u8 gear) {
    u8 *ids;
    u8 *counts;
    u8 item;
    u8 size;
    u8 fresh;
    s32 i;

    fresh = 1;
    if (!gear) {
        ids = D_8006D634.weaponIds;
        counts = ids - 100;
        item = D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[0];
        D_8006D634.characters[D_800625A0->flags->party[slot]].entryItems[0] = 0;
        size = 100;
    } else {
        ids = D_8006D634.gearPartIds;
        counts = ids - 100;
        item = D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[0];
        size = 100;
        D_8006D634.gears[D_8006D634.characters[D_800625A0->flags->party[slot]].gearId].partItems[0] = 0;
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
        func_801DF5D0(slot, gear);                     \
        cursor = 0;                                    \
        D_800625A0->markers->shown[0] = 1;             \
        reset = 1;                                     \
        func_801DB0A8(0, top, 3, 0);                   \
    } while (0)

/* The equipment screen of party slot `slot` (`gear`: the gear's parts)
 * until it is left. In part mode the cursor steps over the four parts (a
 * character 4 toggles its special parts, 9/10 switch the party slot) and
 * confirm opens the candidate list; in list mode the cursor scrolls the
 * candidates, confirm equips one (801df0d4) and cancel restores the kept
 * parts (801df890). Each frame redraws what changed; the first opens the
 * windows and, with `fade`, waits for the view to settle. */
void func_801E05D0(u8 slot, u8 fade, u8 gear) {
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
    D_800625A0->flags->detail_shown = 0;
    func_801DE2C8(gear);
    func_801DF5D0(slot, gear);
    while (running) {
        func_801C7BF4();
        if (panel || slot != drawnSlot) {
            func_801D8EA4(slot, special + 1, listing, gear);
            func_801DE474(special, gear);
            panel = 0;
        }
        if (top != drawnTop || slot != drawnSlot) {
            limit = func_801DE5CC(slot, top, part, special, gear);
            if (limit != 0) {
                y = top * 100 / limit;
                y /= 2;
                h = 0x32;
            } else {
                y = 0;
                h = 0x64;
            }
            func_801D3344(0x94, y + 0x12, h);
        }
        if (cursor) {
            func_801DB0A8(row, top, 3, 0);
        } else {
            D_800625A0->flags->cursors_shown[0] = 0;
        }
        if (row != drawnRow || slot != drawnSlot || top != drawnTop) {
            if (previewed) {
                func_801DFB68(slot, part, row, top, special, gear);
            }
            previewed = 1;
            if (!gear) {
                func_801E36D4(D_800625A0->tables, D_800625A0->flags->party[slot]);
                func_801E3A80(D_800625A0->tables, D_800625A0->flags->party[slot]);
            } else {
                func_801DFE2C(slot);
            }
            func_801DFF5C(part, row, top, special, gear, 0, slot);
            func_801D8DE4(slot, 1, listing, gear);
            drawnRow = row;
            drawnTop = top;
        }
        if (part != drawnPart || slot != drawnSlot) {
            func_801DFF5C(part, row, top, special, gear, 1, slot);
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->x0 = 0x8c;
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->y0 = D_801E9DBC[special * 4 + part];
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->x1 = 0x9c;
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->y1 = D_801E9DBC[special * 4 + part];
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->x2 = 0x8c;
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->y2 = D_801E9DBC[special * 4 + part] + 0x10;
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->x3 = 0x9c;
            drawnPart = part;
            drawnSlot = slot;
            (D_800625A0->markers->polys + D_800625A0->markers->buffer[0])->y3 = D_801E9DBC[special * 4 + part] + 0x10;
        }
        if (first) {
            func_801D397C(2, 0x94, 0xa, 0x94, 0x74, 0, 1, 4, 1);
            func_801D397C(3, 0x6c, 0x87, 0xc4, 0x48, 0, 1, 4, 0);
            func_801D397C(5, 8, 0x8e, 0x60, 0x40, 0, 1, 4, 0);
            if (fade) {
                func_801D1E80();
                func_801D29A8(0, 0);
                while (D_800625A0->view_motion != 0) {
                    func_801C7BF4();
                }
            }
            D_800625A0->markers->shown[0] = 1;
            D_800625A0->flags->field_menu2_shown = 0;
            first = 0;
            D_800625A0->flags->panels_shown[1] = 0;
        }
        D_800625A0->flags->markers_shown = 1;
        if (committed) {
            committed = 0;
            D_800625A0->input = 4;
        }
        if (!listing) {
            switch (D_800625A0->input) {
            case 5:
                running = 0;
                break;
            case 4:
                func_801DF5D0(slot, gear);
                listing = 1;
                top = 0;
                drawnTop = 0xff;
                drawnRow = 0xff;
                D_800625A0->markers->shown[0] = 0;
                panel = 1;
                cursor = 1;
                row = 0;
                D_800625A0->flags->cursors_shown[0] = 1;
                break;
            case 0:
            case 2:
                if (D_800625A0->flags->party[slot] == 4) {
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
                if (func_801D9704(slot, 0, gear) != slot) {
                    slot = func_801D9704(slot, 0, gear);
                    special = 0;
                    previewed = 0;
                }
                break;
            case 10:
                if (func_801D9704(slot, 1, gear) != slot) {
                    slot = func_801D9704(slot, 1, gear);
                    special = 0;
                    previewed = 0;
                }
                break;
            }
        } else {
            reset = 0;
            switch (D_800625A0->input) {
            case 5:
                func_801DF890(slot, gear);
                cursor = 0;
                D_800625A0->markers->shown[0] = 1;
                reset = 1;
                func_801DB0A8(0, top, 3, 0);
                row = 0;
                drawnSlot = 0xff;
                D_800625A0->flags->cursors_shown[0] = 0;
                break;
            case 4:
                swapped = func_801DF0D4(slot, part, special, gear);
                drawnSlot = 0xff;
                if (swapped) {
                    special ^= 1;
                    panel = 1;
                    committed = 1;
                    func_801E0434(slot, gear);
                }
                KEEP_AND_LEAVE_LIST();
                D_800625A0->flags->cursors_shown[0] = 0;
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
                D_800625A0->equip_panel->highlighted = 0;
            }
        }
    }
    func_801D2484();
    func_801DB340(0);
}

/* Run the 801e05d0 screen for party slot `slot` with its blocks; views 3 and 13. */
u8 func_801E0F78(u8 slot, u8 arg1) {
    void *block;

    block = func_80031BDC(0x32f4, 0);
    D_800625A0->equip_panel = block;
    bzero(block, 0x32f4);
    block = func_80031BDC(0x2ac, 0);
    D_800625A0->equip_labels = block;
    bzero(block, 0x2ac);
    func_801C72BC(3);
    func_801E05D0(slot, arg1, 0);
    func_801C72BC(0x13);
    return 1;
}

/* Open the 801e1544 screen: its block (+438), labels and window, the party
 * panel when two or more members can take part, and the green gauges. */
void func_801E1014(void) {
    MenuStatusList *block;
    s32 i;
    s32 j;

    block = func_80031BDC(0x25c0, 0);
    D_800625A0->status_list = block;
    bzero((u_char *)block, 0x25c0);
    func_801C72BC(4);
    func_801E8018(2, D_800625A0->labels17e0, D_801EA564, &D_800625A0->flags->label17e0_shown);
    func_801E8070(2, D_800625A0->labels17e0, D_801EA564, D_801E9EA0, &D_800625A0->flags->label17e0_shown, 0, 0, 4);
    func_801D397C(2, 0x44, 0xa, 0xe4, 0xc4, 0, 1, 4, 0);
    D_800625A0->flags->field_menu2_shown = 0;
    i = 0;
    j = 0; /* members that can take part */
    D_800625A0->flags->panels_shown[1] = 0;
    for (; i < 3; i++) {
        if (D_800625A0->flags->party[i] != 0xff) {
            if (D_800625A0->flags->party[i] != 7 && D_800625A0->flags->party[i] != 8) {
                j++;
            }
        }
    }
    if (j >= 2) {
        func_801D3488(3, 0);
    } else {
        D_800625A0->flags->marks_shown = 0;
    }
    for (i = 0; i < 13; i++) {
        j = 0;
        do {
            SetPolyG4(&D_800625A0->status_list->gauges[i][j]);
            (D_800625A0->status_list->gauges[i] + j)->r0 = 0;
            (D_800625A0->status_list->gauges[i] + j)->g0 = 0xff;
            (D_800625A0->status_list->gauges[i] + j)->b0 = 0;
            (D_800625A0->status_list->gauges[i] + j)->r1 = 0;
            (D_800625A0->status_list->gauges[i] + j)->g1 = 0xff;
            (D_800625A0->status_list->gauges[i] + j)->b1 = 0;
            (D_800625A0->status_list->gauges[i] + j)->r2 = 0;
            (D_800625A0->status_list->gauges[i] + j)->g2 = 0;
            (D_800625A0->status_list->gauges[i] + j)->b2 = 0;
            (D_800625A0->status_list->gauges[i] + j)->r3 = 0;
            (D_800625A0->status_list->gauges[i] + j)->g3 = 0;
            (D_800625A0->status_list->gauges[i] + j)->b3 = 0;
            j++;
        } while (j < 2);
    }
}

/* Close the 801e1544 screen: its sprites, labels and block (+438); view 14. */
void func_801E1398(void) {
    func_801D3674();
    D_800625A0->flags->status_list_shown = 0;
    func_801D4EA0(2);
    func_801C7BF4();
    func_801E8044(2, &D_800625A0->flags->label17e0_shown);
    func_801C72BC(0x14);
    func_800320E8(D_800625A0->status_list);
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

/* Return party slot `slot`'s average completion (percent, each capped at
 * 100) of the seven targets of row `row` of its table (+438 +2578), counting
 * nonzero target entries (ffff contributes zero); rows from 7 need
 * game flag 4000. */
/* The target loop is a goto loop (the original recomputes each target
 * address); the u16 copy of `row` zero-extends it once, in place. */
u32 func_801E1418(u8 slot, u8 row) {
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
    if (row < 7 || (D_8006D634.flags & 0x4000)) {
        i = 0;
        count = 0;
        r = row;
        id = D_800625A0->flags->party[slot];
        table = D_800625A0->status_list->unk2578;
        values = D_8006D634.characters[id].useCounts;
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
 * (D_801E97AC); mode 2 its completion `percent` as three digits into
 * `pixels` (uploaded to the row's label image) and its green gauge
 * (54 pixels at 100%). */
void func_801E1544(u8 row, u8 mode, u8 *pixels, u32 percent) {
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
            if (D_801E97AC[row * 5 + i] != 0xff) {
                D_800625A0->status_list->counts[row] +=
                    func_8002675C(D_800625A0->sheet, D_801E97AC[row * 5 + i],
                                  &D_800625A0->status_list->lists[row][D_800625A0->status_list->counts[row] * 2],
                                  D_800625A0->buffer_index, 0xd4 + i * 16, row * 13 + 0x1f, 0x1000);
            }
        }
        D_800625A0->status_list->starts[row] = D_800625A0->buffer_index;
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
        func_80033B34(text, glyphs, 3);
        D_800625A0->status_list->values[row].width = func_80034EAC(glyphs, pixels, 0x24, 1);
        rect.x = (row & 1) * 24 + 0x180;
        rect.y = (row >> 1) * 13 + 0x80;
        rect.w = 0x28;
        rect.h = 0xd;
        LoadImage(&rect, (u_long *)pixels);
        DrawSync(0);
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->x0 = 0x10a;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->y0 = row * 13 + 0x1f;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->x1 =
            D_800625A0->status_list->values[row].width + 0x10a;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->y1 = row * 13 + 0x1f;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->x2 = 0x10a;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->y2 = row * 13 + 0x2c;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->x3 =
            D_800625A0->status_list->values[row].width + 0x10a;
        (D_800625A0->status_list->values[row].polys + D_800625A0->buffer_index)->y3 = row * 13 + 0x2c;
        percent = percent * 5400 / 10000;
        func_801E7C50(&D_800625A0->status_list->values[row], row, 0x80, 0x82);
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->x0 = 0xd4;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->y0 = row * 13 + 0x23;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->x1 = percent + 0xd4;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->y1 = row * 13 + 0x23;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->x2 = 0xd4;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->y2 = row * 13 + 0x2b;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->x3 = percent + 0xd4;
        (D_800625A0->status_list->gauges[row] + D_800625A0->buffer_index)->y3 = row * 13 + 0x2b;
        D_800625A0->status_list->gaugeBuffer[row] = D_800625A0->buffer_index;
        D_800625A0->status_list->gaugeShown[row] = 1;
        break;
    }
}

/* Lay out the 801e1544 screen's thirteen rows for party slot `slot`: rows
 * the character has (bit of its D_8006D634.skills record) show their name and
 * level digit (rows 0-6); others show only at 50% progress or more, with
 * the progress gauge; then the title. Owned rows pass `percent` unset, as
 * the original does. */
void func_801E1AC8(u8 slot) {
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
    pixels = func_80031BDC(0x3f6, 0);
    for (i = 0; i < 13; i++) {
        D_800625A0->status_list->gaugeShown[i] = 0;
        if (func_801C8640(D_8006D634.skills[D_800625A0->flags->party[slot]].counterSkills, i)) {
            show = 1;
            kind = 1;
            learned = 1;
        } else {
            percent = func_801E1418(slot, i);
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
            D_800625A0->status_list->names[i].width =
                func_80034EAC(func_80033784(D_800625A0->flags->party[slot], i), pixels, 0x24, 0);
            if (i < 7 && learned) {
                text[0] = D_800625A0->tables->arts[D_800625A0->flags->party[slot]][i + 7].unk17 + 0x10;
            } else {
                text[0] = 0xf;
            }
            func_80033B34(text, glyphs, 1);
            D_800625A0->status_list->values[i].width = func_80034EAC(glyphs, pixels, 0x24, 1);
            rect.x = (i & 1) * 24 + 0x180;
            rect.y = i / 2 * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)pixels);
            DrawSync(0);
            func_801E7C50(&D_800625A0->status_list->names[i], i, 0x80, 0x81);
            func_801E7C50(&D_800625A0->status_list->values[i], i, 0x80, 0x82);
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->x0 = 0x5c;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->y0 = i * 13 + 0x1f;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->x1 =
                D_800625A0->status_list->names[i].width + 0x5c;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->y1 = i * 13 + 0x1f;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->x2 = 0x5c;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->y2 = i * 13 + 0x2c;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->x3 =
                D_800625A0->status_list->names[i].width + 0x5c;
            (D_800625A0->status_list->names[i].polys + D_800625A0->buffer_index)->y3 = i * 13 + 0x2c;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->x0 = 0xc8;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->y0 = i * 13 + 0x1f;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->x1 =
                D_800625A0->status_list->values[i].width + 0xc8;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->y1 = i * 13 + 0x1f;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->x2 = 0xc8;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->y2 = i * 13 + 0x2c;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->x3 =
                D_800625A0->status_list->values[i].width + 0xc8;
            (D_800625A0->status_list->values[i].polys + D_800625A0->buffer_index)->y3 = i * 13 + 0x2c;
            D_800625A0->status_list->names[i].buffer = D_800625A0->buffer_index;
            D_800625A0->status_list->shown[i] = 1;
        } else {
            D_800625A0->status_list->shown[i] = 0;
        }
        D_800625A0->status_list->counts[i] = 0;
        func_801E1544(i, kind, pixels, percent);
    }
    func_800320E8(pixels);
    func_801D36E0(&D_800625A0->status_list->title, slot, 0, 2);
    D_800625A0->flags->status_list_shown = 1;
}

/* The 801e1544 screen for party slot `slot`: members 7 and 8 are refused
 * (sound 4); otherwise show the slot's page, switching members (9 previous,
 * 10 next, skipping 7 and 8) until cancelled. */
u8 func_801E20C8(u8 slot) {
    u8 stay;
    u8 shown;

    stay = 1;
    shown = 0xff;
    if ((u32)(D_800625A0->flags->party[slot] - 7) < 2) {
        func_801C8574(4);
        return 1;
    }
    func_801E1014();
    do {
        func_801C7BF4();
        if (slot != shown) {
            shown = slot;
            func_801E1AC8(slot);
        }
        switch (D_800625A0->input) {
        case 5:
            stay = 0;
            break;
        case 9:
            do {
                slot = func_801D9704(slot, 0, 0);
            } while ((u32)(D_800625A0->flags->party[slot] - 7) < 2);
            break;
        case 10:
            do {
                slot = func_801D9704(slot, 1, 0);
            } while ((u32)(D_800625A0->flags->party[slot] - 7) < 2);
            break;
        }
    } while (stay);
    func_801E1398();
    return 1;
}

/* Open the 801d3488 screen on the first ready party slot, which it returns. */
u8 func_801E2250(void) {
    void *block;
    s32 i;

    block = func_80031BDC(0x2af0, 0);
    D_800625A0->detail = block;
    bzero(block, 0x2af0);
    block = func_80031BDC(0x32f4, 0);
    D_800625A0->equip_panel = block;
    bzero(block, 0x32f4);
    block = func_80031BDC(0x2ac, 0);
    D_800625A0->equip_labels = block;
    bzero(block, 0x2ac);
    func_801C72BC(3);
    i = 0;
    while (1) {
        if (D_800625A0->flags->ready[i] != 0) {
            break;
        }
        i++;
    }
    func_801D3488(0, 1);
    return i;
}

/* Lay out the six labels of page `page` (D_801EA568) at +18e0. */
void func_801E2324(u8 page) {
    func_801E8018(6, D_800625A0->labels18e0, D_801EA568 + page, D_800625A0->flags->labels18e0_shown);
}

/* Free the three screen blocks (+358, +35c, +360) and restore the view (13). */
void func_801E2368(void) {
    func_800320E8(D_800625A0->detail);
    func_800320E8(D_800625A0->equip_panel);
    func_800320E8(D_800625A0->equip_labels);
    func_801C72BC(0x13);
}

/* The status command: show party slot `slot`'s status page (6 later
 * labels when its +f8e5 flag is set) and choose among four choices (0 the
 * 801e05d0 screen, 1 and 2 the 801ddf24 screen modes, 3 toggles the flag
 * when the member has a gear; member 7 and the flag D_80059179 refuse),
 * switching members with 9/10, until cancelled. The first 801d9704 call
 * passes the slot before it is set, as the original does; 801d7cfc is
 * called without a prototype in this unit (the slot goes unmasked). */
u8 func_801E23CC(void) {
    s32 slot;
    u8 shown;
    u8 stay;
    u8 first;
    u8 page;

    stay = 1;
    func_801D9704(slot, 0, 1);
    shown = 0xf3;
    first = 1;
    D_800625A0->choice = 0;
    D_800625A0->choice_shown = 0xff;
    slot = func_801E2250();
    while (stay) {
        func_801C7BF4();
        if (slot != shown) {
            func_801DFE2C(slot);
            func_801D2EC0(slot, 1);
            shown = slot;
            page = D_8006D634.inGear[slot] ? 6 : 0;
            func_801E2324(page);
            func_801E8070(6, D_800625A0->labels18e0, &D_801EA568[6], D_801E9F48, D_800625A0->flags->labels18e0_shown, 4, 7, 6);
            func_801E8070(6, D_800625A0->labels18e0, &D_801EA568[6], D_801E9F48, D_800625A0->flags->labels18e0_shown, 5, 7, 6);
            D_800625A0->labels18e0[4].projected = 1;
            D_800625A0->labels18e0[5].projected = 1;
            if (first) {
                first = 0;
                func_801D1E80();
                func_801D29A8(0, 0);
                func_801E86C8(0);
            }
        }
        if (D_800625A0->choice != D_800625A0->choice_shown) {
            func_801E8070(4, D_800625A0->labels18e0, &D_801EA568[6], &D_801E9F48[page], D_800625A0->flags->labels18e0_shown,
                          D_800625A0->choice, 7, 0);
            func_801E8B4C(0);
            D_800625A0->choice_shown = D_800625A0->choice;
        }
        switch (D_800625A0->input) {
        case 4:
            func_801D22C4();
            func_801E8044(4, D_800625A0->flags->labels18e0_shown);
            D_800625A0->flags->detail_shown = 0;
            D_800625A0->flags->equipment_shown = 0;
            D_800625A0->flags->equip_labels_shown = 0;
            D_800625A0->flags->labels18e0_shown[4] = 0;
            D_800625A0->flags->labels18e0_shown[5] = 0;
            switch (D_800625A0->choice) {
            case 0:
                if (D_800625A0->flags->party[slot] != 7) {
                    D_800625A0->flags->lists_shown = 0;
                    func_801E05D0(slot, 0, 1);
                    func_801DE36C();
                    func_801D8DE4(slot, 0, 0, 1);
                    func_801D8EA4(slot, 0, 0, 1);
                    D_800625A0->flags->lists_shown = 1;
                } else {
                    func_801C8574(4);
                }
                shown = 0xff;
                break;
            case 1:
                if (D_800625A0->flags->party[slot] != 7) {
                    func_801DDF24(slot, 0, 1);
                    func_801DC2CC(1);
                } else {
                    func_801C8574(4);
                }
                shown = 0xff;
                break;
            case 2:
                if (D_800625A0->flags->party[slot] != 7) {
                    func_801DDF24(slot, 0, 2);
                    func_801DC2CC(2);
                } else {
                    func_801C8574(4);
                }
                shown = 0xff;
                break;
            case 3:
                if (D_80059179 == 0) {
                    if (D_8006D634.inGear[slot] != 0) {
                        D_8006D634.inGear[slot] = 0;
                        page = 0;
                    } else if (D_8006D634.characters[D_800625A0->flags->party[slot]].gearId != 0xff) {
                        page = 6;
                        D_8006D634.inGear[slot] = 1;
                    }
                    func_801D7CFC(slot, 1, D_8006D634.inGear[slot]);
                } else {
                    func_801C8574(4);
                }
                shown = 0xff;
                break;
            }
            D_800625A0->flags->labels18e0_shown[4] = 1;
            D_800625A0->flags->labels18e0_shown[5] = 1;
            func_801E8018(6, D_800625A0->labels18e0, &D_801EA568[page], D_800625A0->flags->labels18e0_shown);
            D_800625A0->flags->detail_shown = 1;
            D_800625A0->flags->equipment_shown = 1;
            D_800625A0->flags->equip_labels_shown = 1;
            func_801D1EE0(D_800625A0->choice + 7, 1);
            D_800625A0->flags->sprite_shown = 1;
            D_800625A0->choice_shown = 0xff;
            func_801D3488(0, 1);
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
            if (D_800625A0->choice != 0) {
                D_800625A0->choice--;
            } else {
                D_800625A0->choice = D_800625A0->choice_count - 1;
            }
            break;
        case 3:
            if (++D_800625A0->choice >= D_800625A0->choice_count) {
                D_800625A0->choice = 0;
            }
            break;
        case 9:
            slot = func_801D9704(slot, 0, 1);
            D_800625A0->choice_shown = 0xff;
            break;
        case 10:
            slot = func_801D9704(slot, 1, 1);
            D_800625A0->choice_shown = 0xff;
            break;
        }
    }
    func_801E8044(6, D_800625A0->flags->labels18e0_shown);
    D_800625A0->flags->sprite_shown = 0;
    D_800625A0->flags->cursor_shown = 0;
    return 1;
}

/* Open the 801d3488 screen: its three blocks and view 3. */
void func_801E2AE0(void) {
    void *block;

    func_801D249C(1);
    block = func_80031BDC(0x2af0, 0);
    D_800625A0->detail = block;
    bzero(block, 0x2af0);
    block = func_80031BDC(0x32f4, 0);
    D_800625A0->equip_panel = block;
    bzero(block, 0x32f4);
    block = func_80031BDC(0x2ac, 0);
    D_800625A0->equip_labels = block;
    bzero(block, 0x2ac);
    func_801C72BC(3);
    func_801D3488(0, 0);
}

/* Free the three screen blocks (+358, +35c, +360) and restore the view (13). */
void func_801E2B80(void) {
    func_800320E8(D_800625A0->detail);
    func_800320E8(D_800625A0->equip_panel);
    func_800320E8(D_800625A0->equip_labels);
    func_801C72BC(0x13);
}

/* The equipment command: on the first party member's page, choose among
 * three choices (0 the 801e05d0 screen, 1 the 801ddf24 screen, 2 the
 * 801e1544 screen), switching members with 9/10, until cancelled. */
u8 func_801E2BE4(void) {
    s32 slot;
    u8 shown;
    u8 stay;
    u8 first;
    s32 i;

    stay = 1;
    shown = 0xf3;
    first = 1;
    slot = D_800625A0->first_member;
    D_800625A0->choice = 2;
    D_800625A0->choice_shown = 0xff;
    func_801E2AE0();
    for (i = 0; i < 3; i++) {
        if (D_800625A0->flags->party[i] != 0xff) {
            slot = i;
            break;
        }
    }
    while (stay) {
        func_801C7BF4();
        if (slot != shown) {
            func_801E36D4(D_800625A0->tables, D_800625A0->flags->party[slot]);
            func_801E3A80(D_800625A0->tables, D_800625A0->flags->party[slot]);
            func_801D2EC0(slot, 0);
            shown = slot;
            if (first) {
                first = 0;
                func_801D1E80();
                func_801D29A8(0, 0);
                func_801E86C8(0);
            }
        }
        if (D_800625A0->choice != D_800625A0->choice_shown) {
            func_801D261C();
            func_801E8B4C(0);
            D_800625A0->choice_shown = D_800625A0->choice;
        }
        switch (D_800625A0->input) {
        case 4:
            func_801D22C4();
            func_801D25E4();
            D_800625A0->flags->detail_shown = 0;
            D_800625A0->flags->equipment_shown = 0;
            D_800625A0->flags->equip_labels_shown = 0;
            switch (D_800625A0->choice) {
            case 0:
                D_800625A0->flags->lists_shown = 0;
                func_801E05D0(slot, 0, 0);
                func_801DE36C();
                func_801D8DE4(slot, 0, 0, 0);
                func_801D8EA4(slot, 0, 0, 0);
                shown = 0xff;
                D_800625A0->flags->lists_shown = 1;
                break;
            case 1:
                func_801DDF24(slot, 0, 0);
                func_801DC2CC(0);
                shown = 0xff;
                break;
            case 2:
                func_801E20C8(slot);
                break;
            }
            func_801D249C(1);
            D_800625A0->flags->detail_shown = 1;
            D_800625A0->flags->equipment_shown = 1;
            D_800625A0->flags->equip_labels_shown = 1;
            func_801D1EE0(D_800625A0->choice + 7, 1);
            D_800625A0->flags->sprite_shown = 1;
            D_800625A0->choice_shown = 0xff;
            func_801D3488(0, 0);
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
            if (D_800625A0->choice != 0) {
                D_800625A0->choice--;
            } else {
                D_800625A0->choice = D_800625A0->choice_count - 1;
            }
            break;
        case 3:
            if (++D_800625A0->choice >= D_800625A0->choice_count) {
                D_800625A0->choice = 0;
            }
            break;
        case 9:
            slot = func_801D9704(slot, 0, 0);
            break;
        case 10:
            slot = func_801D9704(slot, 1, 0);
            break;
        }
    }
    func_801D249C(0);
    D_800625A0->flags->sprite_shown = 0;
    D_800625A0->flags->cursor_shown = 0;
    return 1;
}

/* Close the screen of the command at `offset` past the top cursor. */
void func_801E3088(u8 offset) {
    switch (D_800625A0->cursor + offset) {
    case 1:
    case 8:
        func_801D9E3C();
        break;
    case 2:
        D_800625A0->flags->detail_shown = 0;
        D_800625A0->flags->equipment_shown = 0;
        D_800625A0->flags->equip_labels_shown = 0;
        func_801E2368();
        break;
    case 3:
        func_801DC2CC(0);
        break;
    case 4:
        func_801DA518();
        break;
    case 5:
        func_801DE36C();
        func_801DE400();
        break;
    case 6:
        D_800625A0->flags->detail_shown = 0;
        D_800625A0->flags->equipment_shown = 0;
        D_800625A0->flags->equip_labels_shown = 0;
        func_801D25E4();
        func_801E2B80();
        break;
    case 0:
    case 7:
    case 9:
        break;
    }
}

/* Use item `item` on character `id`: restore HP (x50) and/or EP (x10),
 * raise stats (capped at 200, HP max 999, EP max 99), change +78, or run a
 * debug fill. Returns nonzero when the restoring item had no effect. */
u8 func_801E31C0(MenuTables *tables, u8 id, u8 item) {
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
    chara = D_8006D634.characters + id;
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
            chara->accuracy += record->amount;
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
        if (chara->accuracy > 200) {
            chara->accuracy = 200;
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
            func_801E5058();
            break;
        case 2:
            func_801E5178();
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

/* Apply `user`'s restoring effect: to `target`'s HP (its +5b times the
 * effect's +11, capped at the maximum), or with `gear` to the user's gear
 * (+60 up by a tenth of +64, capped at +64). */
void func_801E35BC(tables, user, target, effect, gear)
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

    source = &D_8006D634.characters[user];
    dest = &D_8006D634.characters[target];
    machine = (GearRecord *)&D_8006D634.characters[D_8006D634.characters[user].gearId + 11];
    if (!gear) {
        record = tables->arts[user];
        record += effect;
        dest->hp += source->accuracy * record->unk11;
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

/* Recompute character `id`'s equipment values: sum its three accessories
 * (amount, kind bits and stat bonuses) and take its weapon's values (kind 4
 * characters: both weapons). */
void func_801E36D4(MenuTables *tables, u8 id) {
    CharacterRecord *chara;
    AccessoryInfo *accessory;
    EquipInfo *weapon;
    u8 i;
    u8 amount;

    chara = &D_8006D634.characters[id];
    chara->bodyDefense = 0;
    chara->flags32 = 0;
    chara->equipAttack = 0;
    chara->equipDefense = 0;
    chara->equipSpeed = 0;
    chara->equipAccuracy = 0;
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
            chara->equipAccuracy += amount;
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

/* Compute character `id`'s shown stats: base values plus equipment bonuses
 * (the first from the level, scaled 6/10 with +1c for kind 4), capped at
 * 250, 99 or 16. */
void func_801E3A80(MenuTables *tables, u8 id) {
    CharacterRecord *chara;

    chara = &D_8006D634.characters[id];
    if (chara->characterId == 4) {
        tables->stats[0] = (chara->entries[0].value4 + chara->entries[3].value4) * 6 / 10;
    } else {
        tables->stats[0] = chara->entries[0].value4 + (chara->attack + chara->equipAttack);
    }
    tables->stats[1] = chara->field5E + chara->equip5E;
    tables->stats[2] = chara->bodyDefense + (chara->defense + chara->equipDefense);
    tables->stats[3] = chara->field5F + chara->equip5F;
    tables->stats[4] = chara->accuracy + chara->equipAccuracy;
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

/* Compute gear `gear`'s shown stats from its record and its pilot (flag 1000
 * makes gear 9's pilot character 10); gear 7 first takes its values from
 * character 7 (HP x50, stats plus bonuses). */
void func_801E3C2C(MenuTables *tables, u8 gear) {
    GearRecord *record;
    CharacterRecord *pilot;
    s32 bonus;

    if (D_8006D634.flags & 0x1000) {
        D_801E9808[9] = 10;
    }
    if (gear == 7) {
        D_8006D634.gears[7].hp = D_8006D634.characters[7].hp * 50;
        D_8006D634.gears[7].maxHp = D_8006D634.characters[7].maxHp * 50;
        D_8006D634.gears[7].attack = D_8006D634.characters[7].attack + D_8006D634.characters[7].equipAttack;
        D_8006D634.gears[7].bodyDefense = (D_8006D634.characters[7].defense + D_8006D634.characters[7].equipDefense) * 12;
        D_8006D634.gears[7].armor = (D_8006D634.characters[7].etherDefense + D_8006D634.characters[7].equipEtherDefense) * 6;
        D_8006D634.gears[7].speed = D_8006D634.characters[7].speed + D_8006D634.characters[7].equipSpeed;
    }
    record = &D_8006D634.gears[gear];
    pilot = &D_8006D634.characters[D_801E9808[gear]];
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

/* Compute gear `gear`'s part and weapon values, then mirror its +9 values
 * (and for gears 4, 5 its weapons) into its other form (gears 1, 15, 10-14)
 * and compute that too. */
void func_801E3ECC(MenuTables *tables, u8 gear) {
    u8 i;

    func_801E433C(tables, gear);
    func_801E4754(tables, gear);
    switch (gear) {
    case 0:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[1].parts[i] = D_8006D634.gears[0].parts[i];
        }
        func_801E433C(tables, 1);
        break;
    case 1:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[15].parts[i] = D_8006D634.gears[1].parts[i];
        }
        func_801E433C(tables, 15);
        break;
    case 2:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[10].parts[i] = D_8006D634.gears[2].parts[i];
        }
        func_801E433C(tables, 10);
        break;
    case 3:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[11].parts[i] = D_8006D634.gears[3].parts[i];
        }
        func_801E433C(tables, 11);
        break;
    case 4:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[12].parts[i] = D_8006D634.gears[4].parts[i];
        }
        func_801E433C(tables, 12);
        D_8006D634.gears[12].weapons[0] = D_8006D634.gears[4].weapons[0];
        func_801E4754(tables, 12);
        break;
    case 5:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[13].parts[i] = D_8006D634.gears[5].parts[i];
        }
        func_801E433C(tables, 13);
        D_8006D634.gears[13].weapons[0] = D_8006D634.gears[5].weapons[0];
        D_8006D634.gears[13].weapons[3] = D_8006D634.gears[5].weapons[3];
        D_8006D634.gears[13].partItems[0] = D_8006D634.gears[5].partItems[0];
        D_8006D634.gears[13].partItems[3] = D_8006D634.gears[5].partItems[3];
        func_801E4754(tables, 13);
        break;
    case 6:
        for (i = 0; i < 3; i++) {
            D_8006D634.gears[14].parts[i] = D_8006D634.gears[6].parts[i];
        }
        func_801E433C(tables, 14);
        break;
    }
}

/* Recompute gear `gear`'s derived values from the data tables. */
void func_801E4170(MenuTables *tables, u8 gear) {
    func_801E41C0(tables, gear);
    func_801E42AC(tables, gear);
    func_801E4258(tables, gear);
}

/* Take gear `gear`'s engine values from the tables, keeping +60 within +64. */
void func_801E41C0(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearEngineInfo *engine;

    record = &D_8006D634.gears[gear];
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

/* Copy gear `gear`'s two frame values (+8, +a of its frame record) into the
 * gear record (+70, +72). */
void func_801E4258(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearFrameInfo *frame;

    record = &D_8006D634.gears[gear];
    frame = tables->frames;
    frame += record->frame;
    record->bodyDefense = frame->unk8;
    record->armor = frame->unkA;
}

/* Take gear `gear`'s part values from the tables, keeping +38 within +3a. */
void func_801E42AC(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearPartInfo *part;

    record = &D_8006D634.gears[gear];
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

/* Sum gear `gear`'s three parts (table +14) into its record: their stats,
 * and by each part's kind its bit words, amounts or the pilot's flag bits;
 * then its level (801e4928) and the pilot's flag 8000 (set while +4f, else
 * cleared when the pilot flies this gear). */
void func_801E433C(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearAccessoryInfo *part;
    u16 *bits;
    u16 *flags;
    u8 i;
    u8 j;

    record = &D_8006D634.gears[gear];
    bits = &D_8006D634.skills[D_801E9808[gear]].unlocksA;
    flags = &D_8006D634.skills[D_801E9808[gear]].flags1A;
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
    record->speedPenalty = func_801E4928(gear);
    if (record->field4F) {
        *flags |= 0x8000;
    } else if (gear == D_8006D634.characters[D_801E9808[gear]].gearId) {
        *flags &= 0x7fff;
    }
}

/* Take gear `gear`'s weapon values (table +18) into its first slot and
 * attributes; gears 5 and 13 instead take their three slot weapons. */
void func_801E4754(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearWeaponInfo *weapon;

    record = &D_8006D634.gears[gear];
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

/* Gear `gear`'s value: (its +44 / 120 - its +75) / 2, at least 0. */
u8 func_801E4928(u8 gear) {
    GearRecord *record;
    s16 value;

    record = &D_8006D634.gears[gear];
    value = ((u16)(record->equip68a / 120) - record->field75) / 2;
    if (value < 0) {
        value = 0;
    }
    return value;
}

/* Set the fuel cost of gear `gear`'s art record 37, the first of its
 * fuel-cost list, to 2/90 of the gear's maximum HP, in steps of ten. */
void func_801E4998(MenuTables *tables, u8 gear) {
    ArtInfo *art;

    /* One pointer walks from the gear's art records to the record. */
    art = (tables->arts + 11)[gear];
    art = &art[37];
    art->gearCost = D_8006D634.gears[gear].maxHp / 10 * 2 / 9;
    art->gearCost = art->gearCost / 10 * 10;
}

/* A prototype, which drops GCC's built-in memcpy (psyq/libc.h keeps it; cc1 warns
 * of the conflict): the built-in moves the two constant 0xa38-byte copies below
 * inline, where the original calls memcpy. A length passed in a variable keeps the
 * built-in and gives the same calls; nothing decides which the original had. */
void *memcpy(void *dest, void *src, int n);

/* Copy the game data into save buffer `save`: characters, gears (their
 * kept fields), names and the other blocks. */
void func_801E4A28(SaveData *save) {
    SaveWordsA4 *chara;
    SaveGear *dst;
    GearRecord *src;
    u8 i;

    for (i = 0; i < 11; i++) {
        chara = &save->chars[i];
        *chara = *(SaveWordsA4 *)&D_8006D634.characters[i];
    }
    for (i = 0; i < 20; i++) {
        dst = &save->gears[i];
        src = &D_8006D634.gears[i];
        dst->head = *(SaveWords10 *)src;
        dst->entries = *(SaveWords18 *)src->entries;
        dst->variants = *(s32 *)&src->fileVariant;
        dst->fuel = src->fuel;
        dst->hp = src->hp;
        dst->defense = src->defense;
        dst->field74 = src->field74;
        dst->field75 = src->field75;
    }
    save->names = *(SaveWordsDC *)&D_8006D634.names;
    save->unk100 = *(SaveWords190 *)D_8006D634.names[11];
    save->unkE4C = *(SaveWords78 *)D_8006D634.unk1648;
    save->records = *(SaveWords160 *)D_8006D634.skills;
    save->unk1024 = *(SaveWords100 *)&D_8006D634.worldmap;
    memcpy(save->unk1124, ((u8 *)&D_8006D634 + 0x1920), 0xa38);
}

/* Load the game data from save buffer `save`: characters, records, gears
 * (recomputing their table values before restoring the kept fields), names
 * and the other blocks. */
void func_801E4D10(SaveData *save, MenuTables *tables) {
    SaveWordsA4 *chara;
    SaveWordsA4 *saved;
    GearRecord *dst;
    SaveGear *src;
    u8 i;

    for (i = 0; i < 11; i++) {
        chara = (SaveWordsA4 *)&D_8006D634.characters[i];
        saved = &save->chars[i];
        *chara = *saved;
    }
    *(SaveWords160 *)D_8006D634.skills = save->records;
    for (i = 0; i < 20; i++) {
        dst = &D_8006D634.gears[i];
        src = &save->gears[i];
        *(SaveWords10 *)dst = src->head;
        *(SaveWords18 *)dst->entries = src->entries;
        *(s32 *)&dst->fileVariant = src->variants;
        func_801E41C0(tables, i);
        func_801E4258(tables, i);
        func_801E42AC(tables, i);
        func_801E433C(tables, i);
        dst->fuel = src->fuel;
        dst->hp = src->hp;
        dst->defense = src->defense;
        dst->field74 = src->field74;
        dst->field75 = src->field75;
    }
    *(SaveWordsDC *)D_8006D634.names = save->names;
    *(SaveWords190 *)D_8006D634.names[11] = save->unk100;
    *(SaveWords78 *)D_8006D634.unk1648 = save->unkE4C;
    *(SaveWords100 *)&D_8006D634.worldmap = save->unk1024;
    memcpy(((u8 *)&D_8006D634 + 0x1920), save->unk1124, 0xa38);
}

/* Debug: put ten of every entry into the five inventory lists. */
void func_801E5058(void) {
    u8 i;

    for (i = 1; i < 0x48; i++) {
        D_8006D634.weaponIds[i] = i;
        D_8006D634.weaponCounts[i] = 10;
    }
    for (i = 1; i < 0x96; i++) {
        D_8006D634.accessoryIds[i] = i;
        D_8006D634.accessoryCounts[i] = 10;
    }
    for (i = 1; i < 0x4c; i++) {
        D_8006D634.itemIds[i + 2] = i;
        D_8006D634.itemCounts[i + 2] = 10;
    }
    for (i = 1; i < 0x48; i++) {
        D_8006D634.gearPartIds[i] = i;
        D_8006D634.gearPartCounts[i] = 10;
    }
    for (i = 1; i < 0x69; i++) {
        D_8006D634.gearAccessoryIds[i] = i;
        D_8006D634.gearAccessoryCounts[i] = 10;
    }
}

/* Debug: reset D_8006D634.joined and the eleven D_8006D634.skills records (four values,
 * flag 7 and the +1a value) to their defaults. */
void func_801E5178(void) {
    D_8006D634.joined = 0x7ff;
    D_8006D634.skills[0].counterSkills = 0xfff8;
    D_8006D634.skills[0].levelSkills = 0xff00;
    D_8006D634.skills[0].unlocksA = 0xfff0;
    D_8006D634.skills[0].unlocksB = 0xfe00;
    D_8006D634.skills[0].flags1A = 0xe000;
    D_8006D634.skills[0].tier = 7;
    D_8006D634.skills[1].counterSkills = 0xffe0;
    D_8006D634.skills[1].levelSkills = 0xfff0;
    D_8006D634.skills[1].unlocksA = 0xfff0;
    D_8006D634.skills[1].unlocksB = 0xfff0;
    D_8006D634.skills[1].flags1A = 0xc000;
    D_8006D634.skills[1].tier = 7;
    D_8006D634.skills[2].counterSkills = 0xffe0;
    D_8006D634.skills[2].levelSkills = 0xffe0;
    D_8006D634.skills[2].unlocksA = 0xfff0;
    D_8006D634.skills[2].unlocksB = 0xff00;
    D_8006D634.skills[2].flags1A = 0x8000;
    D_8006D634.skills[2].tier = 7;
    D_8006D634.skills[3].counterSkills = 0xffe0;
    D_8006D634.skills[3].levelSkills = 0xffc0;
    D_8006D634.skills[3].unlocksA = 0xfff0;
    D_8006D634.skills[3].unlocksB = 0xff00;
    D_8006D634.skills[3].flags1A = 0xf000;
    D_8006D634.skills[3].tier = 7;
    D_8006D634.skills[4].counterSkills = 0xffc0;
    D_8006D634.skills[4].levelSkills = 0xffc0;
    D_8006D634.skills[4].unlocksA = 0xfff0;
    D_8006D634.skills[4].unlocksB = 0xffc0;
    D_8006D634.skills[4].flags1A = 0xe000;
    D_8006D634.skills[4].tier = 7;
    D_8006D634.skills[5].counterSkills = 0xffc0;
    D_8006D634.skills[5].levelSkills = 0xf000;
    D_8006D634.skills[5].unlocksA = 0xfff0;
    D_8006D634.skills[5].unlocksB = 0xf000;
    D_8006D634.skills[5].flags1A = 0x8000;
    D_8006D634.skills[5].tier = 7;
    D_8006D634.skills[6].counterSkills = 0xffc0;
    D_8006D634.skills[6].levelSkills = 0xff00;
    D_8006D634.skills[6].unlocksA = 0xfff0;
    D_8006D634.skills[6].unlocksB = 0xff00;
    D_8006D634.skills[6].flags1A = 0x8000;
    D_8006D634.skills[6].tier = 7;
    D_8006D634.skills[7].counterSkills = 0;
    D_8006D634.skills[7].levelSkills = 0xff00;
    D_8006D634.skills[7].unlocksA = 0;
    D_8006D634.skills[7].unlocksB = 0xff00;
    D_8006D634.skills[7].flags1A = 0;
    D_8006D634.skills[7].tier = 7;
    D_8006D634.skills[8].counterSkills = 0;
    D_8006D634.skills[8].levelSkills = 0xf800;
    D_8006D634.skills[8].unlocksA = 0xfff0;
    D_8006D634.skills[8].unlocksB = 0;
    D_8006D634.skills[8].flags1A = 0xe000;
    D_8006D634.skills[8].tier = 7;
    D_8006D634.skills[9].counterSkills = 0xffe0;
    D_8006D634.skills[9].levelSkills = 0xffe0;
    D_8006D634.skills[9].unlocksA = 0xfff0;
    D_8006D634.skills[9].unlocksB = 0xffe0;
    D_8006D634.skills[9].flags1A = 0x8000;
    D_8006D634.skills[9].tier = 7;
    D_8006D634.skills[10].counterSkills = 0xffc0;
    D_8006D634.skills[10].levelSkills = 0xff00;
    D_8006D634.skills[10].unlocksA = 0xfff0;
    D_8006D634.skills[10].unlocksB = 0xff00;
    D_8006D634.skills[10].flags1A = 0x8000;
    D_8006D634.skills[10].tier = 7;
}

/* Set up panel `index`: hide it and stop its opening, set the grey fills and
 * their draw modes, and the four edge lists' quads (sheet records 0-3). */
void func_801E53CC(u8 index) {
    MenuPanel *panel;
    RECT window;
    u8 i;

    panel = D_800625A0->panels[index];
    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    D_800625A0->flags->panels_shown[index] = 0;
    D_800625A0->flags->panels_growing[index] = 0;
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
                    GetTPage(0, 0, D_800625A0->sheet_entries[0].page_x, D_800625A0->sheet_entries[0].page_y), &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->edge[0][i]);
        SetShadeTex(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xff;
        (panel->edge[0] + i)->g0 = 0xff;
        (panel->edge[0] + i)->b0 = 0xff;
        panel->edge[0][i].tpage = GetTPage(D_800625A0->sheet_entries[0].mode, 0, D_800625A0->sheet_entries[0].page_x,
                                            D_800625A0->sheet_entries[0].page_y);
        panel->edge[0][i].clut = GetClut(D_800625A0->sheet_entries[0].clut_x, D_800625A0->sheet_entries[0].clut_y);
        SetPolyFT4(&panel->edge[1][i]);
        SetShadeTex(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xff;
        (panel->edge[1] + i)->g0 = 0xff;
        (panel->edge[1] + i)->b0 = 0xff;
        panel->edge[1][i].tpage = GetTPage(D_800625A0->sheet_entries[1].mode, 0, D_800625A0->sheet_entries[1].page_x,
                                            D_800625A0->sheet_entries[1].page_y);
        panel->edge[1][i].clut = GetClut(D_800625A0->sheet_entries[1].clut_x, D_800625A0->sheet_entries[1].clut_y);
        SetPolyFT4(&panel->edge[2][i]);
        SetShadeTex(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xff;
        (panel->edge[2] + i)->g0 = 0xff;
        (panel->edge[2] + i)->b0 = 0xff;
        panel->edge[2][i].tpage = GetTPage(D_800625A0->sheet_entries[2].mode, 0, D_800625A0->sheet_entries[2].page_x,
                                            D_800625A0->sheet_entries[2].page_y);
        panel->edge[2][i].clut = GetClut(D_800625A0->sheet_entries[2].clut_x, D_800625A0->sheet_entries[2].clut_y);
        SetPolyFT4(&panel->edge[3][i]);
        SetShadeTex(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xff;
        (panel->edge[3] + i)->g0 = 0xff;
        (panel->edge[3] + i)->b0 = 0xff;
        panel->edge[3][i].tpage = GetTPage(D_800625A0->sheet_entries[3].mode, 0, D_800625A0->sheet_entries[3].page_x,
                                            D_800625A0->sheet_entries[3].page_y);
        panel->edge[3][i].clut = GetClut(D_800625A0->sheet_entries[3].clut_x, D_800625A0->sheet_entries[3].clut_y);
    }
}

/* Set up image block `index`'s 16x16 sprite and semi-transparent cover at
 * its position for both buffers, and the two draw modes (blend mode 2). */
void func_801E56E8(s32 index) {
    MenuSlotImage *image;
    s16 *x;
    s16 *y;
    s32 i;
    RECT window;

    i = 0;
    x = D_801E9894[index];
    y = D_801E9914[index];
    image = D_800625A0->slots[index];
    for (; i < 2; i++) {
        func_801E927C(&image->icon[i]);
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
        func_801C851C(image->iconAt, *x, *y, 0x10, 0x10);
    }
    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    SetDrawMode(&image->boxMode[0], 0, 0, GetTPage(0, 2, 0x140, 0x80), &window);
    SetDrawMode(&image->boxMode[1], 0, 0, GetTPage(0, 2, 0x140, 0x80), &window);
}

/* Set up image block `index`'s green 16x16 frame at its position: the
 * top/right and left/bottom lines and their vertices, for both buffers. */
void func_801E5924(s32 index) {
    MenuSlotImage *image;
    s16 *x;
    s16 *y;
    s32 i;

    x = D_801E9894[index];
    y = D_801E9914[index];
    image = D_800625A0->slots[index];
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
        func_801C851C(image->lineAAt, *x, *y, 0x10, 0x10);
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
        func_801C851C(image->lineBAt, *x, *y, 0x10, 0x10);
    }
}

/* Allocate and set up the 32 image blocks (+3a8). */
void func_801E5ACC(void) {
    s32 i;
    void *block;

    for (i = 0; i < 32; i++) {
        block = func_80031BDC(0x158, 0);
        D_800625A0->slots[i] = block;
        bzero(block, 0x158);
        func_801E56E8(i);
        func_801E5924(i);
    }
}

/* Free the 32 image blocks at +3a8. */
void func_801E5B3C(void) {
    s32 i;

    for (i = 0; i < 32; i++) {
        func_800320E8(D_800625A0->slots[i]);
    }
}

/* Set up the 32 text character quads (both buffers): 21 per line, 12x16
 * glyphs of the 140 page from v e0, 16 per glyph row. */
void func_801E5B88(void) {
    s32 i;
    s32 buffer;

    for (i = 0; i < 32; i++) {
        for (buffer = 0; buffer < 2; buffer++) {
            func_801E927C(&D_800625A0->file_info->chars[i * 2 + buffer]);
            (D_800625A0->file_info->chars + (i * 2 + buffer))->x0 = D_801E9994[i % 21] + i / 21 * 8;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->y0 = D_801E99E8[i / 21];
            (D_800625A0->file_info->chars + (i * 2 + buffer))->x1 = D_801E9994[i % 21] + i / 21 * 8 + 12;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->y1 = D_801E99E8[i / 21];
            (D_800625A0->file_info->chars + (i * 2 + buffer))->x2 = D_801E9994[i % 21] + i / 21 * 8;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->y2 = D_801E99E8[i / 21] + 16;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->x3 = D_801E9994[i % 21] + i / 21 * 8 + 12;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->y3 = D_801E99E8[i / 21] + 16;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->u0 = i % 16 * 16;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->v0 = i / 16 * 16 - 0x20;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->u1 = i % 16 * 16 + 12;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->v1 = i / 16 * 16 - 0x20;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->u2 = i % 16 * 16;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->v2 = i / 16 * 16 - 0x11;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->u3 = i % 16 * 16 + 12;
            (D_800625A0->file_info->chars + (i * 2 + buffer))->v3 = i / 16 * 16 - 0x11;
            D_800625A0->file_info->chars[i * 2 + buffer].tpage = GetTPage(0, 0, 0x140, 0x80);
            D_800625A0->file_info->chars[i * 2 + buffer].clut = GetClut(0, 0x1c0);
        }
    }
}

/* Set up the 32x32 cursor sprite and the purple-to-black shaded band
 * (0,4a)-(140,8a) for both buffers. */
void func_801E5E4C(void) {
    s32 *x;
    s32 *y;
    s32 i;
    s32 right; /* the band's right edge */

    right = 0x140;
    i = 0;
    x = &D_801E99F0;
    y = &D_801E99F8;
    for (; i < 2; i++) {
        func_801E927C(&D_800625A0->file_info->cursor[i]);
        (D_800625A0->file_info->cursor + i)->x0 = *x;
        (D_800625A0->file_info->cursor + i)->y0 = *y;
        (D_800625A0->file_info->cursor + i)->x1 = *x + 0x20;
        (D_800625A0->file_info->cursor + i)->y1 = *y;
        (D_800625A0->file_info->cursor + i)->x2 = *x;
        (D_800625A0->file_info->cursor + i)->y2 = *y + 0x20;
        (D_800625A0->file_info->cursor + i)->x3 = *x + 0x20;
        (D_800625A0->file_info->cursor + i)->y3 = *y + 0x20;
        D_800625A0->file_info->cursor[i].tpage = GetTPage(0, 0, 0x140, 0x80);
        SetPolyG4(&D_800625A0->file_info->band[i]);
        (D_800625A0->file_info->band + i)->r0 = 0x80;
        (D_800625A0->file_info->band + i)->g0 = 0;
        (D_800625A0->file_info->band + i)->b0 = 0x80;
        (D_800625A0->file_info->band + i)->r1 = 0;
        (D_800625A0->file_info->band + i)->g1 = 0;
        (D_800625A0->file_info->band + i)->b1 = 0x80;
        (D_800625A0->file_info->band + i)->r2 = 0x10;
        (D_800625A0->file_info->band + i)->g2 = 0;
        (D_800625A0->file_info->band + i)->b2 = 0x10;
        (D_800625A0->file_info->band + i)->r3 = 0;
        (D_800625A0->file_info->band + i)->g3 = 0;
        (D_800625A0->file_info->band + i)->b3 = 0x10;
        (D_800625A0->file_info->band + i)->x0 = 0;
        (D_800625A0->file_info->band + i)->y0 = 0x4a;
        (D_800625A0->file_info->band + i)->x1 = right;
        (D_800625A0->file_info->band + i)->y1 = 0x4a;
        (D_800625A0->file_info->band + i)->x2 = 0;
        (D_800625A0->file_info->band + i)->y2 = 0x8a;
        (D_800625A0->file_info->band + i)->x3 = right;
        (D_800625A0->file_info->band + i)->y3 = 0x8a;
    }
}

/* Lay out the three save views' frames (nine images each, 50 apart) and
 * their 72x13 name quads (label rows 6 + view). */
void func_801E61B0(void) {
    s32 view;
    s32 i;

    for (view = 0; view < 3; view++) {
        D_800625A0->file_info->views[view].frameCount = 0;
        for (i = 0; i < 9; i++) {
            if (D_801EA494[i] != 0xffff) {
                D_800625A0->file_info->views[view].frameCount +=
                    func_8002675C(D_800625A0->sheet, D_801EA494[i],
                                  D_800625A0->file_info->views[view].frame[D_800625A0->file_info->views[view].frameCount],
                                  D_800625A0->buffer_index, view * 0x50 + D_801E9F98[i], D_801E9FBC[i], 0x1000);
            }
        }
        D_800625A0->file_info->views[view].frameBuffer = D_800625A0->buffer_index;
        func_801E927C(&D_800625A0->file_info->views[view].name[D_800625A0->buffer_index]);
        D_800625A0->file_info->views[view].name[D_800625A0->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
        D_800625A0->file_info->views[view].name[D_800625A0->buffer_index].clut = D_800595D4;
        func_801E920C(&D_800625A0->file_info->views[view].name[D_800625A0->buffer_index], D_801E9F98[0] + view * 0x50,
                      D_801E9FBC[0] + 7, D_801EA578[view + 6] * 4, D_801EA5C4[view + 6], 0x48, 0xd);
        D_800625A0->file_info->views[view].nameBuffer = D_800625A0->buffer_index;
    }
}

/* Allocate and clear the 2dc0-byte block at +34c, then set it up. */
void func_801E6450(void) {
    void *block;
    u8 reserved[8]; /* the original frame reserves 8 unused bytes */

    block = func_80031BDC(0x2dc0, 0);
    D_800625A0->file_info = block;
    bzero(block, 0x2dc0);
    func_801E5B88();
    func_801E5E4C();
}

/* Free the 2dc0-byte block at +34c and clear party flag +b. */
void func_801E649C(void) {
    func_800320E8(D_800625A0->file_info);
    D_800625A0->flags->file_info_shown = 0;
}

/* Clear the 64x32 image area at (140, e0) to black and clear party flag +b. */
void func_801E64E0(void) {
    RECT rect;

    rect.x = 0x140;
    rect.y = 0xe0;
    rect.w = 0x40;
    rect.h = 0x20;
    ClearImage(&rect, 0, 0, 0);
    D_800625A0->flags->file_info_shown = 0;
}

/* Pack each of the 16 rows of `rows` in place, merging bytes 1-2, 5-6, 9-10
 * and 13-14. */
void func_801E6544(u8 rows[16][16]) {
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

/* Return the 16x16 font glyph of the character at `s`: ASCII is converted
 * to its two-byte code (control characters to a space), two-byte codes pass
 * through. */
u16 *func_801E65E4(u8 *s) {
    u8 hi;
    u8 lo;

    hi = s[0];
    lo = s[1];
    D_801EA8C0 = 1;
    if (hi < 0x80) {
        if (hi >= 0x20) {
            lo = D_801EA610[hi - 0x20];
            D_801EA8C0 = 0;
            hi = D_801EA610[hi - 0x20] >> 8;
        } else {
            hi = 0x81;
            lo = 0x40;
            D_801EA8C0 = 0;
        }
    }
    return (u16 *)Krom2RawAdd(lo | (hi << 8));
}

/* Render listed file `index`'s save title (up to 32 characters, 64 bytes)
 * as 12-pixel glyphs into the 4-bit 256x32 image at (140, e0). */
void func_801E6668(s32 index) {
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

    pixels = func_80031BDC(0x100, 1);
    image = func_80031BDC(0x1000, 1);
    bzero((u_char *)image, 0x1000);
    bytes = 0;
    chars = 0;
    text = (u8 *)D_800625A0->card + (index << 9) + 0xb98;
    while (1) {
        if (*text == 0) {
            break;
        }
        pixel = pixels;
        glyph = func_801E65E4(text);
        if (glyph != (u16 *)-1) {
            for (row = 0; row < 16; row++, glyph++) {
                for (col = 7; col >= 0; col--) {
                    *pixel++ = (*glyph >> col) & 1;
                }
                for (col = 15; col >= 8; col--) {
                    *pixel++ = (*glyph >> col) & 1;
                }
            }
            func_801E6544((u8(*)[16])pixels);
            for (row = 0; row < 16; row++) {
                for (col = 0; col < 12; col++) {
                    image[col / 4 + row * 64 + chars % 16 * 4 + chars / 16 * 1024] |= pixels[row * 16 + col]
                                                                                    << (col % 4 * 4);
                }
            }
        }
        text++;
        bytes++;
        if (D_801EA8C0) {
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
    func_800320E8(pixels);
    func_800320E8(image);
}

/* Lay out the save's play time (two separators and seven digits at y 7a)
 * and its file digit plus one, as two digits, with its label at (8, 66). */
void func_801E68AC(SaveSummary *set) {
    s32 i;

    func_8002675C(D_800625A0->sheet, 0xee, D_800625A0->file_info->colon0, D_800625A0->buffer_index, D_801E9FE0[0], 0x7a,
                  0x1000);
    func_8002675C(D_800625A0->sheet, 0xee, D_800625A0->file_info->colon1, D_800625A0->buffer_index, D_801E9FE0[1], 0x7a,
                  0x1000);
    func_801C7F34(set->time);
    for (i = 0; i < 7; i++) {
        func_8002675C(D_800625A0->sheet, D_800625A0->time[i], D_800625A0->file_info->timeDigits[i],
                      D_800625A0->buffer_index, D_801E9FE0[i + 2], 0x7a, 0x1000);
    }
    func_8002675C(D_800625A0->sheet, 0x17, D_800625A0->file_info->discLabel, D_800625A0->buffer_index, 8, 0x66, 0x1000);
    func_8002675C(D_800625A0->sheet, 0x32, D_800625A0->file_info->discMark, D_800625A0->buffer_index, 0x10, 0x66, 0x1000);
    func_8002675C(D_800625A0->sheet, (set->digit + 1) / 10, D_800625A0->file_info->discDigits[0],
                  D_800625A0->buffer_index, 0x10, 0x6e, 0x1000);
    func_8002675C(D_800625A0->sheet, (set->digit + 1) % 10, D_800625A0->file_info->discDigits[1],
                  D_800625A0->buffer_index, 0x18, 0x6e, 0x1000);
}

/* Set up view `index` of the block at +34c from its character's sheet image
 * (14e + id). */
void func_801E6AE8(u8 index, SaveSummary *set) {
    func_8002675C(D_800625A0->sheet, set->ids[index] + 0x14e, &D_800625A0->file_info->views[index],
                  D_800625A0->buffer_index, D_801EA004[index], D_801EA010[index], 0x1000);
}

/* Lay out view `index`'s level as up to three digit sprites, and reset its
 * second digit row for the +63 value. */
void func_801E6B70(u8 index, SaveSummary *set) {
    s32 i;
    u8 digit;

    func_801C80B8(set->level[index]);
    D_800625A0->file_info->views[index].levelCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[i + 6];
        if (digit != 0xff) {
            D_800625A0->file_info->views[index].levelCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->file_info->views[index].levelDigits[D_800625A0->file_info->views[index].levelCount],
                              D_800625A0->buffer_index, D_801EA01C + index * 0x50 + i * 8, D_801EA020, 0x1000);
        }
    }
    func_801C80B8(set->level2[index]);
    D_800625A0->file_info->views[index].level2Count = 0;
}

/* Lay out view `index`'s HP and maximum HP as up to three digit sprites
 * each (the maximum packed without leading blanks). */
void func_801E6CFC(u8 index, SaveSummary *set) {
    s32 i;
    s32 drawn;
    u8 digit;

    func_801C80B8(set->hp[index]);
    D_800625A0->file_info->views[index].hpCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[i + 6];
        if (digit != 0xff) {
            D_800625A0->file_info->views[index].hpCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->file_info->views[index].hpDigits[D_800625A0->file_info->views[index].hpCount],
                              D_800625A0->buffer_index, D_801EA02C + index * 0x50 + i * 8, D_801EA030, 0x1000);
        }
    }
    drawn = 0;
    func_801C80B8(set->hpMax[index]);
    D_800625A0->file_info->views[index].hpMaxCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[i + 6];
        if (digit != 0xff) {
            D_800625A0->file_info->views[index].hpMaxCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->file_info->views[index].hpMaxDigits[D_800625A0->file_info->views[index].hpMaxCount],
                              D_800625A0->buffer_index, D_801EA034 + index * 0x50 + drawn * 8, D_801EA038, 0x1000);
            drawn++;
        }
    }
}

/* Lay out view `index`'s EP and maximum EP as up to two digit sprites each
 * (the maximum packed without leading blanks). */
void func_801E6F5C(u8 index, SaveSummary *set) {
    s32 i;
    s32 drawn;
    u8 digit;

    func_801C80B8(set->ep[index]);
    D_800625A0->file_info->views[index].epCount = 0;
    for (i = 0; i < 2; i++) {
        digit = D_800625A0->digits[i + 7];
        if (digit != 0xff) {
            D_800625A0->file_info->views[index].epCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->file_info->views[index].epDigits[D_800625A0->file_info->views[index].epCount],
                              D_800625A0->buffer_index, D_801EA03C + index * 0x50 + i * 8, D_801EA040, 0x1000);
        }
    }
    drawn = 0;
    func_801C80B8(set->epMax[index]);
    D_800625A0->file_info->views[index].epMaxCount = 0;
    for (i = 0; i < 2; i++) {
        digit = D_800625A0->digits[i + 7];
        if (digit != 0xff) {
            D_800625A0->file_info->views[index].epMaxCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->file_info->views[index].epMaxDigits[D_800625A0->file_info->views[index].epMaxCount],
                              D_800625A0->buffer_index, D_801EA044 + index * 0x50 + drawn * 8, D_801EA048, 0x1000);
            drawn++;
        }
    }
}

/* Render the name of view `index`'s character from listed file `file`'s
 * name table (up to ten two-byte characters) and upload it to label row
 * `index` of the view rows. */
void func_801E71B4(u8 index, SaveSummary *set, s32 file) {
    RECT rect;
    u8 name[24];
    u8 text[24];
    u8 *pixels;
    MenuSaveInfo *info;
    s32 i;

    info = (MenuSaveInfo *)(D_800625A0->card->heads[file] + 0x100);
    for (i = 0; i < 20; i += 2) {
        name[i] = info->names[set->ids[index]].text[i];
        name[i + 1] = info->names[set->ids[index]].text[i + 1];
        if (name[i] == 0 && name[i + 1] == 0) {
            break;
        }
    }
    func_80033B34(name, text, i / 2);
    pixels = func_80031BDC(0x3f6, 0);
    bzero(pixels, 0x3f6);
    func_80034EAC(text, pixels, 0x24, 0);
    rect.x = D_801EA578[index + 6] + 0x180;
    rect.y = D_801EA5C4[index + 6];
    rect.w = 0x28;
    rect.h = 0xd;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    func_800320E8(pixels);
}

/* Lay out the 16 character quads of the save title image (row f0 of the
 * 140 page, 12-pixel glyphs 16 apart) in the current buffer. */
void func_801E733C(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        func_801E927C(&D_800625A0->file_info->title[i * 2 + D_800625A0->buffer_index]);
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->x0 = D_801EA04C + i * 12;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->y0 = D_801EA050;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->x1 = D_801EA04C + i * 12 + 12;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->y1 = D_801EA050;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->x2 = D_801EA04C + i * 12;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->y2 = D_801EA050 + 16;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->x3 = D_801EA04C + i * 12 + 12;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->y3 = D_801EA050 + 16;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->u0 = i * 16;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->v0 = 0xf0;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->u1 = i * 16 + 12;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->v1 = 0xf0;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->u2 = i * 16;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->v2 = 0xff;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->u3 = i * 16 + 12;
        (D_800625A0->file_info->title + (i * 2 + D_800625A0->buffer_index))->v3 = 0xff;
        D_800625A0->file_info->title[i * 2 + D_800625A0->buffer_index].tpage = GetTPage(0, 0, 0x140, 0x80);
        D_800625A0->file_info->title[i * 2 + D_800625A0->buffer_index].clut = GetClut(0, 0x1c0);
    }
}

/* Build the three views of card file `index`'s save information. */
void func_801E76EC(s32 index) {
    SaveSummary *set;
    s32 i;

    set = (SaveSummary *)(D_800625A0->card->heads[index] + 0x100);
    func_801E61B0();
    for (i = 0; i < 3; i++) {
        if (set->ids[i] != 0xff) {
            D_800625A0->file_info->views[i].shown = 1;
            func_801E6AE8(i, set);
            func_801E6B70(i, set);
            func_801E6CFC(i, set);
            func_801E6F5C(i, set);
            func_801E71B4(i, set, index);
        } else {
            D_800625A0->file_info->views[i].shown = 0;
        }
        D_800625A0->file_info->views[i].buffer = D_800625A0->buffer_index;
    }
    func_801E68AC(set);
    func_801E733C();
}

/* Redraw view `index` (none for ff), rebuilding it first when `rebuild`. */
void func_801E781C(s32 index, u8 rebuild) {
    func_801E64E0();
    if (index != 0xff) {
        if (rebuild) {
            func_801E76EC(index);
            func_801E6668(index);
            D_800625A0->file_info->rebuilt = 1;
        } else {
            func_801E6668(index);
            D_800625A0->file_info->rebuilt = 0;
        }
        D_800625A0->flags->file_info_shown = 1;
    }
}

/* Upload listed file `file`'s save icon to its slot of the icon pages (the
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
void func_801E78C8(s32 file) {
    s32 i;
    u8 noIcon;

    noIcon = 1;
    D_801EA8E4.x = 0x140 + file * 4 - file / 16 * 64;
    D_801EA8E4.w = 4;
    D_801EA8E4.h = 0x10;
    D_801EA8EC.x = file * 16;
    D_801EA8EC.y = 0x1c1 + file / 16;
    D_801EA8EC.w = 0x10;
    D_801EA8EC.h = 1;
    memmove(D_801EA8C4, &D_800625A0->card->heads[file][0x60], 0x20);
    LoadImage(&D_801EA8EC, (u_long *)D_801EA8C4);
    DrawSync(0);
    for (i = 0; i < 3; i++) {
        D_801EA8E4.y = 0x80 + i * 32 + file / 16 * 16;
        LoadImage(&D_801EA8E4, (u_long *)&D_800625A0->card->heads[file][0x80 + i * 0x80]);
        DrawSync(0);
    }
    switch (D_800625A0->card->heads[file][2]) {
    case 0x11:
        D_800625A0->card->files[file].frames[0] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[1] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[2] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[3] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[4] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[5] = 0x80 + file / 16 * 16;
        noIcon = 0;
        break;
    case 0x12:
        D_800625A0->card->files[file].frames[0] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[1] = 0xa0 + file / 16 * 16;
        D_800625A0->card->files[file].frames[2] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[3] = 0xa0 + file / 16 * 16;
        D_800625A0->card->files[file].frames[4] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[5] = 0xa0 + file / 16 * 16;
        noIcon = 0;
        break;
    case 0x13:
        D_800625A0->card->files[file].frames[0] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[1] = 0xa0 + file / 16 * 16;
        D_800625A0->card->files[file].frames[2] = 0xc0 + file / 16 * 16;
        D_800625A0->card->files[file].frames[3] = 0x80 + file / 16 * 16;
        D_800625A0->card->files[file].frames[4] = 0xa0 + file / 16 * 16;
        D_800625A0->card->files[file].frames[5] = 0xc0 + file / 16 * 16;
        noIcon = 0;
        break;
    }
    D_801EA900[file / 16] += D_800625A0->card->heads[file][3];
    if (noIcon) {
        D_800625A0->card->files[file].state = 0;
    }
}

/* Point the current quad at its CLUT (the highlighted D_80059414 or the plain
 * D_800595D4). A statement macro. */
#define SET_QUAD_CLUT()                                                \
    do {                                                               \
        POLY_FT4 *p = poly;                                            \
        p->clut = label->highlight != 0 ? D_80059414 : D_800595D4;      \
    } while (0)

/* Set up the current quad of `label`: mode 0 takes image `index` from the
 * 140 column pages (rows from `first`); otherwise from the 180 page (+80
 * keeps it opaque, else dimmed) with palette choice `mode & 7f` - 1. A
 * statement macro. */
#define SET_QUAD()                                                     \
    do {                                                               \
        semi = 0;                                                      \
        func_801E927C(poly);                                           \
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

/* Set up `label`'s two quads for label image `index` (the quad loop is
 * not loop-optimized: each quad's palette and window are recomputed) and
 * hide the label. */
void func_801E7C50(MenuLabel *label, s32 index, s32 first, u8 mode) {
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

/* Render `count` labels (text ids in `layout`) in pairs into one 28x13
 * image each (two columns of 32 from x 140, rows of 13 from label `first`),
 * lay them out and upload the images. */
void func_801E7E68(MenuLabel *labels, u8 *layout, s32 first, s32 count) {
    s32 i;
    RECT *image;

    for (i = 0; i < count; i += 2) {
        labels[i].width = func_80034EAC(func_80033728(D_800625A0->label_text, layout[i]), D_800625A0->labels[0].pixels,
                                        0x18, 0);
        image = &labels[i].rect;
        labels[i + 1].width = func_80034EAC(func_80033728(D_800625A0->label_text, layout[i + 1]), D_800625A0->labels[0].pixels,
                                        0x18, 1);
        labels[i].rect.x = (i / 2 & 1) * 0x20 + 0x140;
        labels[i].rect.y = (i + first) / 4 * 0xd;
        labels[i].rect.w = 0x1c;
        labels[i].rect.h = 0xd;
        labels[i + 1].rect = labels[i].rect;
        func_801E7C50(&labels[i], i, first, 0);
        func_801E7C50(&labels[i + 1], i + 1, first, 0);
        LoadImage(image, (u_long *)D_800625A0->labels[0].pixels);
        DrawSync(0);
    }
}

/* Lay out `count` labels from `table` into `labels` (the placement is unused). */
void func_801E8018(u8 count, MenuLabel *labels, u8 *table, u8 *flags) {
    func_801E7E68(labels, table, 4, count);
}

/* Clear `count` label flags. */
void func_801E8044(u8 count, u8 *flags) {
    s32 i;

    for (i = 0; i < count; i++) {
        flags[i] = 0;
    }
}
