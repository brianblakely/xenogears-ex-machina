/* Battle unit from 8008B478: its rodata is the three jump tables at
 * 80070314-80070370 (8008B478, 8008BED8, 8008C81C), at 4 mod 8 between runs
 * at 0 mod 8 (docs/matching.md). The text boundaries are not fixed by the
 * tables: this unit starts after 800861D0 and ends before 80094EE4; these
 * files take the tables' owners. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "menu_pages.h"
#include "resolver.h"
#include "action_resolve.h"
#include "hud_draw.h"
#include "battle_command.h"
#include "window_draw.h"
#include "formation_route.h"
#include "gear_menu.h"
#include "glyph_lists.h"
#include "item_command.h"
#include "result_input.h"

/* Run the member's technique menu: four windows, a two-column list of
 * twelve visible cells scrolled by rows (800d3288 in pixels, 800d39d4 the
 * scroll request), until a technique is committed (1, its index in the
 * turn state +0x2e6) or the menu is cancelled (0). */
u8 func_8008B478(u8 member) {
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
    D_800D3288 = 0;
    D_800D39D4 = 0;
    D_800D2D28->unkCB = 0;
    func_8008F8F4(0, 0x1C, 0xA0, 0xAC, 0x38, 0, 1);
    func_8008F8F4(1, 0x20, 0x2C, 0x118, 0x70, 0, 1);
    func_8008F8F4(2, 0xD0, 0xA4, 0x50, 0x18, 0, 1);
    func_8008F8F4(3, 0xE6, 0xC0, 0x4C, 0x18, 0, 1);
    func_80091064(member);
    func_80077698();
    do {
        if (cell != shownCell || top != shownTop) {
            func_80091D38(member, cell, top);
            shownCell = cell;
            shownTop = top;
        }
        if (D_800D3288 != shownScroll) {
            func_80091604(D_800D3288);
            func_80091EC4(D_800D3288);
            shownScroll = D_800D3288;
        }
        func_80090B90((cell % 2) * 0x80 + (cell % 2) * 4 + 0x2A, (cell / 2) * 16 + 0x38, &frame, &ticks);
        func_800716D8();
        switch (D_800D3014) {
        case 5:
            result = 0;
            break;
        case 4:
            if (func_8008B224(member, cell, top)) {
                result = 1;
                D_800C3EAC->unk2E6 = cell + top * 2;
            }
            break;
        case 0:
            if (top * 2 + cell != 15) {
                if (cell + 1 == 12) {
                    D_800D39D4 = 1;
                } else {
                    cell++;
                }
            }
            break;
        case 2:
            if (top * 2 + cell > 0) {
                if (cell == 0 && top != 0) {
                    D_800D39D4 = 2;
                } else {
                    cell--;
                }
            }
            break;
        case 1:
            if (top * 2 + cell < 14) {
                if (cell + 2 >= 12) {
                    D_800D39D4 = 3;
                } else {
                    cell += 2;
                }
            }
            break;
        case 3:
            if (top * 2 + cell >= 2) {
                if (cell < 2 && top != 0) {
                    D_800D39D4 = 4;
                } else {
                    cell -= 2;
                }
            }
            break;
        }
        switch (D_800D39D4) {
        case 1:
            target = (top + 1) * 16;
            if (D_800D3288 >= target) {
                top++;
                cell--;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        case 2:
            target = (top - 1) * 16;
            if (D_800D3288 < target) {
                top--;
                cell++;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        case 3:
            target = (top + 1) * 16;
            if (D_800D3288 >= target) {
                top++;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        case 4:
            target = (top - 1) * 16;
            if (D_800D3288 < target) {
                top--;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        }
    } while (result == 2);
    func_8008B108(result);
    func_8007765C();
    func_80077980();
    func_8008FA60(0);
    func_8008FA60(1);
    func_8008FA60(2);
    func_8008FA60(3);
    return result;
}

/* Execute the chosen item (turn state +0x2e6) for the member: reset the
 * events, show the member's use model, commit the item against its targets
 * (the target candidates, or the chosen target), use one up unless the
 * item keeps (0x8000), apply its results as event 0xf5, react the targeted
 * enemies, and wait for the presentation; targeted party members react. */
void func_8008B908(member)
u8 member;
{
    u16 targets;
    s32 i;

    D_800D366C = 0;
    func_800BCD98(0);
    for (i = 0; i < 32; i++) {
        D_800C3FE8[i].type = 0xFF;
    }
    func_800B89FC(1, member, D_800C3EAC->slots[member].defaultTarget, func_80080AE4(member));
    if (D_800D2200[D_800C3EAC->unk2E6].target & 7) {
        targets = D_800C3D64;
    } else {
        targets = func_80089C08(D_800C3E2C);
    }
    func_8008AC88(targets, member);
    func_80085388();
    func_80085C48(member, targets, D_800C3EAC->unk2E6);
    if (!(D_800D2C94.held & 0x8000)) {
        if (--D_800D2C94.itemCounts[D_800C3D00 * 2 + D_800D3670] == 0) {
            D_800D2C94.itemIds[D_800C3D00 * 2 + D_800D3670] = 0;
        }
    }
    D_800C2050 = 1;
    func_80085C88(D_800C3EAC->eventCount);
    D_800C2050 = 0;
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xF5;
    D_800C3FE8[D_800C3EAC->eventCount].parameter = D_800D2C94.animation;
    D_800C3FE8[D_800C3EAC->eventCount].actor = member;
    D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2C94.targets;
    for (i = 3; i < 11; i++) {
        if (func_80089C9C(D_800D2C94.targets, i)) {
            func_80079840(member, i);
        }
    }
    D_800C3EAC->eventCount++;
    func_80080B64(member);
    while (D_800C3EAC->eventsDone == 0) {
        func_800716D8();
    }
    for (i = 0; i < 3; i++) {
        if (func_80089C9C(targets, i)) {
            D_800D2D28->reaction[i] = 1;
        }
    }
}

/* Hide the command windows (two panels); without `keep` show the +0x641c
 * lists. */
void func_8008BC40(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (two panels, page 2) and frame the camera on the
 * member and its default target. */
void func_8008BC98(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 1;
    D_800D2D28->unkB7 = 2;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Use the list item at (column, row) for `member`: an item (from the
 * character list) starts its effect with target selection, 1 when started;
 * a gear part (from the gear list, inGear) is applied and its count taken,
 * 2 when applied. An empty entry buzzes (0x4f). */
u8 func_8008BD50(member, column, row, inGear)
u8 member;
u8 column;
u8 row;
u8 inGear;
{
    u16 effect;
    u8 item;
    u8 result;

    effect = D_800D2200[D_800D2CE0[row * 2 + column]].target;
    result = 0;
    if (!inGear) {
        item = D_800D2CE0[row * 2 + column];
    } else {
        item = D_800C3D70[row * 2 + column];
    }
    if (item != 0) {
        if (!inGear) {
            func_8008BC40(1);
            D_800D2D28->unkC6 = 1;
            if (func_80085084(effect, member, 0)) {
                result = 1;
            } else {
                D_800D2D28->unkC6 = 0;
                func_8008BC98(member);
            }
        } else if (((s32 (*)())func_8009A7E4)(item - 50)) {
            /* Both calls are unprototyped in the original: the part index
             * is passed unnarrowed and 8009a854's entry argument is left
             * undefined (it then uses whatever the register holds). */
            ((void (*)())func_8009A854)(item);
            if (--D_800D3688[row * 2 + column] == 0) {
                D_800C3D70[row * 2 + column] = 0;
            }
            result = 2;
        }
    } else {
        func_8008AA74(0x4F);
    }
    return result;
}

/* Run the member's item menu: two windows, a two-column list of sixteen
 * visible cells (48 entries) scrolled a 13-pixel row at a time (800d3288,
 * requests in 800d39d4), the character list or (after 8008bd50 returns 2)
 * the gear parts list. Returns 1 when an item was started (its id in the
 * turn state +0x2e6, its cell in 800d3670/800c3d00), 0 when cancelled. */
u8 func_8008BED8(u8 member) {
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
    D_800D3288 = 0;
    D_800D39D4 = 0;
    D_800D2D28->unkCB = 0;
    func_8008F8F4(0, 0x20, 0x58, 0x118, 0x78, 0, 1);
    func_8008F8F4(1, 0x10, 0x2C, 0x128, 0x28, 0, 1);
    func_80077698();
    do {
        if (pending != 0) {
            func_8008FE18(member, pending - 1, open);
            list = pending - 1;
            pending = 0;
            open = 0;
        }
        if (cell != shownCell || top != shownTop) {
            func_8009080C(cell, top, list);
            shownCell = cell;
            shownTop = top;
        }
        if (D_800D3288 != shownScroll) {
            func_8009023C(D_800D3288 / 13 * 2);
            func_8009093C(D_800D3288);
            shownScroll = D_800D3288;
        }
        func_80090B90((cell % 2) * 0x80 + 0x2A + (cell % 2) * 2, (cell / 2) * 13 + 0x63, &frame, &ticks);
        func_800716D8();
        switch (D_800D3014) {
        case 5:
            result = 0;
            break;
        case 4:
            switch (func_8008BD50(member, cell, top, list)) {
            case 1:
                D_800C3EAC->unk2E6 = D_800D2CE0[top * 2 + cell];
                D_800D3670 = cell;
                D_800C3D00 = top;
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
                    D_800D39D4 = 1;
                } else {
                    cell++;
                }
            }
            break;
        case 2:
            if (top * 2 + cell > 0) {
                if (cell == 0 && top != 0) {
                    D_800D39D4 = 2;
                } else {
                    cell--;
                }
            }
            break;
        case 1:
            if (top * 2 + cell < 46) {
                if (cell + 2 >= 16) {
                    D_800D39D4 = 3;
                } else {
                    cell += 2;
                }
            }
            break;
        case 3:
            if (top * 2 + cell >= 2) {
                if (cell < 2 && top != 0) {
                    D_800D39D4 = 4;
                } else {
                    cell -= 2;
                }
            }
            break;
        }
        switch (D_800D39D4) {
        case 1:
            D_800D3288 = (top + 1) * 13;
            top++;
            cell--;
            D_800D39D4 = 0;
            break;
        case 2:
            D_800D3288 = (top - 1) * 13;
            top--;
            cell++;
            D_800D39D4 = 0;
            break;
        case 3:
            D_800D3288 = (top + 1) * 13;
            top++;
            D_800D39D4 = 0;
            break;
        case 4:
            D_800D3288 = (top - 1) * 13;
            top--;
            D_800D39D4 = 0;
            break;
        }
    } while (result == 2);
    func_8008BC40(result);
    func_8007765C();
    func_80077980();
    func_8008FA60(0);
    func_8008FA60(1);
    return result;
}

/* Hide the command windows; with `close` also close windows 0 and 1 and
 * release the graphics block. */
void func_8008C360(u8 close) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->unkB7 = 0;
    if (close != 0) {
        D_800D2D28->unkC6 = 0;
        func_8008FA60(0);
        func_8008FA60(1);
        func_800716D8();
        func_8007765C();
        func_80077980();
    } else {
        D_800D2D28->windows[0] = D_800D2D28->windows[1] = 0;
    }
}

/* Show the command windows (two panels, page 3) and frame the camera on the
 * member and its default target. */
void func_8008C3F0(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->unkB7 = 3;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 1;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Run the member's combo: choose the steps (8008c81c); when cancelled
 * reopen the command windows and return 1. Otherwise enter the attack page
 * against the chosen target (an event 0xf3 first when it reacts while
 * down), commit each chosen step (action step + 8) as an event, move into
 * the target's group and wait for the presentation. Returns 0. */
u8 func_8008C4A8(member)
u8 member;
{
    u8 cancelled = 1;
    s32 i;

    if (func_8008C81C(member)) {
        func_800BAF40(member, 0x100);
        func_8008C360(1);
        D_800D2D28->unkCB = 1;
        cancelled = 0;
    } else {
        func_800BAF40(member, 0x80);
        D_800D2D28->unkAF = 0;
        func_8008C360(1);
        D_800D366C = 0;
        func_800BCD98(0);
        D_800C3EAC->slots[member].defaultTarget = D_800C3E2C;
        func_800877E0(member, D_800C3EAC->slots[member].defaultTarget);
        D_800C4928 = 1;
        func_80087A38(member);
        if (D_800CCCE8.records[D_800C3EAC->slots[member].defaultTarget].pilot.flags34 & 0x800) {
            D_800C3FE8[D_800C3EAC->eventCount].actor = member;
            D_800C3FE8[D_800C3EAC->eventCount].type = 0xF3;
            D_800C3FE8[D_800C3EAC->eventCount].parameter = func_80089C08(D_800C3EAC->slots[member].defaultTarget);
            D_800C3EAC->eventCount++;
        }
        for (i = 0; i < 7; i++) {
            if (D_800C3EAC->combo[i] != 0xFF) {
                D_800C3EAC->unk2DC = D_800C3EAC->combo[i] + 8;
                func_80085CCC(member, func_80089C08(D_800C3EAC->slots[member].defaultTarget), D_800C3EAC->unk2DC - 1);
                func_80085C88(D_800C3EAC->eventCount);
                D_800C3FE8[D_800C3EAC->eventCount].type = D_800C3EAC->unk2DC - 1;
                D_800C3FE8[D_800C3EAC->eventCount].actor = member;
                D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2C94.targets;
                D_800C3EAC->eventCount++;
            }
        }
        func_80080B64(member);
        func_80087EDC(member, D_800C3EAC->slots[member].defaultTarget);
        while (D_800C3EAC->eventsDone == 0) {
            func_800716D8();
        }
    }
    return cancelled;
}

/* Run the member's combo menu: list the known combo steps (0-6) with their
 * AP costs, then move the cursor over the eight cells (cell 7 confirms),
 * add the step under it while AP last (up to seven steps) or take the last
 * one back, and on confirm pick the target. Returns 0 when confirmed with a
 * target (the remaining AP stored), 1 when cancelled. The cell marks are
 * cleared with the cell count held in `n` (a variable bound, so the loop
 * runs ascending). */
u8 func_8008C81C(u8 member) {
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
    ap = D_800D32A0[member].unk0;
    frame = 4;
    ticks = 0;
    D_800D2D28->unkCB = 0;
    for (i = 0; i < 7; i++) {
        steps[i] = 0xFF;
        costs[i] = 0;
        D_800C3EAC->combo[i] = 0xFF;
        D_800C3DE0[i] = 0xFF;
    }
    n = 8;
    for (i = 0; i < n; i++) {
        marks[i] = 0;
    }
    n = 0;
    for (i = 0; i < 7; i++) {
        if (func_80089C6C(D_8006D634.skills[D_80059468[member]].counterSkills, i)) {
            steps[n] = i;
            costs[n] = D_800CCCE8.partyCommands[member][i + 7].apCost;
            n++;
        }
    }
    func_8008F8F4(0, 0x16, 0x5C, 0x122, 0x4C, 1, 1);
    func_8008F8F4(1, 0x10, 0x2C, 0xE8, 0x2C, 1, 1);
    while (D_800D2D28->unkBF[0] == 0 || D_800D2D28->unkBF[1] == 0) {
        func_800716D8();
    }
    func_80092784(member, steps, costs);
    func_80077698();
    while (done == 0) {
        if (redraw) {
            func_80092B74(member, ap);
            redraw = 0;
        }
        func_80090B90((cursor % 2) * 0x88 + 0x1E + (cursor % 2) * 4, (cursor / 2) * 16 + 0x64, &frame, &ticks);
        func_800716D8();
        switch (D_800D3014) {
        case 5:
            if (count != 0) {
                count--;
                redraw = 1;
                ap += paid[count];
                D_800C3EAC->combo[count] = 0xFF;
                D_800C3DE0[count] = 0xFF;
            } else {
                done = 2;
            }
            break;
        case 4:
            if (cursor == 7) {
                if (D_800C3EAC->combo[0] == 0xFF) {
                    done = 2;
                } else {
                    func_8008C360(0);
                    D_800D2D28->unkC6 = 1;
                    if (func_80085084(0x1000, member, 1)) {
                        D_800D32A0[member].unk0 = ap;
                        done = 1;
                    } else {
                        D_800D2D28->unkC6 = 0;
                        func_8008C3F0(member);
                    }
                }
            } else if (count != 7 && steps[cursor] != 0xFF && ap >= costs[cursor]) {
                D_800C3EAC->combo[count] = steps[cursor];
                D_800C3DE0[count] = cursor;
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
