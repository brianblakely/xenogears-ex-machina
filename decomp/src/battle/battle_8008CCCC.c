/* Battle code from 8008CCCC to 8009E53C: its rodata starts at 80070370
 * (80094EE4's jump table), back at 0 mod 8 after the previous unit's tables
 * at 4 mod 8 (docs/matching.md); the text boundary lies after 8008C81C. */
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
#include "area.h"

/* Word view of BattleDraw.buffer, alongside the low-byte view in battle_core.h. */
extern s32 D_800CCB34_word __asm__("D_800CCB34");

/* Hide the command windows (three panels); without `keep` show the +0x641c
 * lists. */
void func_8008CCCC(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (three panels, page 4) and frame the camera on
 * the member and its default target. */
void func_8008CD28(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = 1;
    D_800D2D28->unkB7 = 4;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Confirm the member's gear command `index` (0-3, the gear's commands from
 * 37): it needs the fuel it costs, the character's permission bit and no
 * seal status for it. Hides the command windows and commits the command,
 * paying the fuel; if the commit fails the windows come back. A refused
 * command plays the error sound. Returns 1 when committed. */
u8 func_8008CDE4(u8 member, u8 index) {
    u16 sealed[4];
    u8 committed;
    u16 cost;
    u16 command;
    u8 refused;

    sealed[0] = D_800C3234[13];
    sealed[1] = D_800C3234[14];
    sealed[2] = D_800C3234[15];
    sealed[3] = D_800C3234[3];
    committed = 0;
    cost = D_800CCCE8.gearCommands[member][index + 37].hudState;
    command = D_800CCCE8.gearCommands[member][index + 37].state;
    refused = 1;

    if (D_800CCCE8.records[member].gear.fuel >= cost &&
        func_80089C6C(D_8006ECF4[D_800D2D24[member]].flags1A, index) != 0 &&
        !(D_800CCCE8.records[member].pilot.status7A & sealed[index])) {
        func_8008CCCC(1);
        D_800D2D28->unkC6 = 1;
        if (func_80085084(command, member, 0)) {
            D_800CCCE8.records[member].gear.fuel -= cost;
            committed = 1;
        } else {
            D_800D2D28->unkC6 = 0;
            func_8008CD28(member);
        }
        refused = 0;
    }
    if (refused) {
        func_8008AA74(0x4F);
    }
    return committed;
}

/* Run the member's gear command menu: list the gear's commands 37-40 the
 * character may use and no status seals (id and fuel cost), open its three
 * windows and move the cursor until a command is committed (1, remembered
 * + 0x10 in the turn state) or the menu is cancelled (0). */
u8 func_8008CFB8(member)
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
    seals[0] = D_800C3234[13];
    seals[1] = D_800C3234[14];
    seals[2] = D_800C3234[15];
    seals[3] = D_800C3234[3];
    for (i = 0; i < 4; i++) {
        ids[i] = 0xFF;
        costs[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].flags1A, i) != 0 &&
            !(D_800CCCE8.records[member].pilot.status7A & seals[i])) {
            ids[i] = D_800CCCE8.records[member].pilot.gearId * 4 + i;
            costs[i] = D_800CCCE8.gearCommands[member][i + 37].hudState;
        }
    }
    D_800D2D28->unkCB = 0;
    func_8008F8F4(0, 0x74, 0xA0, 0xAC, 0x38, 0, 1);
    func_8008F8F4(1, 0x84, 0x4C, 0x9C, 0x50, 0, 1);
    func_8008F8F4(2, 0x1C, 0xA0, 0x50, 0x18, 0, 1);
    func_800930AC(member, ids, costs);
    func_80077698();
    while (result == 2) {
        if (cursor != shown) {
            func_800939CC(member, cursor);
            shown = cursor;
        }
        func_80090B90(0x8C, cursor * 16 + 0x58, &frame, &ticks);
        func_800716D8();
        switch (D_800D3014) {
        case 5:
            result = 0;
            break;
        case 4:
            if (func_8008CDE4(member, cursor)) {
                result = 1;
                D_800C3EAC->unk2E6 = cursor + 0x10;
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
    func_8008CCCC(result);
    func_8007765C();
    func_80077980();
    func_8008FA60(0);
    func_8008FA60(1);
    func_8008FA60(2);
    return result;
}

/* Fade the list quads of both draw buffers (every other one from 800d2d28
 * +0xa3): semi-transparent, raw texture, darker by the turn state's step
 * each frame. When all have faded out, clear the fading flag (+0xcb). */
void func_8008D328(void) {
    s32 buffer;
    s32 i;
    u8 fading = 0;
    u8 shade;

    for (buffer = 0; buffer < 2; buffer++) {
        if (D_800D2D28->unkD0[buffer] != 0) {
            for (i = 0; i < D_800D2D28->unkD0[buffer] * 2; i += 2) {
                SetSemiTrans(&D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3], 1);
                SetShadeTex(&D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3], 0);
                D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3].tpage |= 0x20;
                shade = D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3].r0;
                if (shade != 0) {
                    fading = 1;
                    (D_800C3EA4->unk641C[buffer] + (i + D_800D2D28->unkA3))->r0 = shade - D_800C3EAC->unk2E0 * 16;
                    (D_800C3EA4->unk641C[buffer] + (i + D_800D2D28->unkA3))->g0 = shade - D_800C3EAC->unk2E0 * 16;
                    (D_800C3EA4->unk641C[buffer] + (i + D_800D2D28->unkA3))->b0 = shade - D_800C3EAC->unk2E0 * 16;
                }
            }
        }
    }
    if (!fading) {
        D_800D2D28->unkCB = 0;
    }
}

/* Build the command panel page `page` for the member into the two +0x641c
 * glyph lists (800d2d28 +0xd0 counts): each of the page's glyph sets
 * places its glyphs (ids from 0x4000 read the turn slot's bytes, 0xff
 * skipped) and shades them (from 0x2000 by a slot item's availability:
 * shaded semi-transparent, else full brightness). With `fade` fade the lists
 * instead (8008d328) and return their buffer; otherwise return the draw
 * buffer. Nonmatching: 18 instruction differences in the glyph call's
 * destination/count setup (8008d740-8008d7ac); the rest matches. */
#ifdef NON_MATCHING
s32 func_8008D598(u8 member, u8 page, u8 fade) {
    u16 lists[2];
    u8 sets[2];
    s32 i;
    s32 j;
    s32 n;
    s32 id;
    u16 shade;
    u32 value;

    if (fade != 0) {
        func_8008D328();
        return D_800D2D28->unkA3;
    }
    for (i = 0; i < 2; i++) {
        lists[i] = D_800C3000[page]->lists[i];
        sets[i] = D_800C3000[page]->sets[i];
        D_800D2D28->unkD0[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        if (lists[i] == 0xFF) {
            break;
        }
        /* Four halfwords per glyph record. */
        j = 0;
        while ((id = ((u16 *)D_800C2F4C[sets[i]])[j]) != 0xFFFF) {
            if (id >= 0x4000) {
                D_800C3EAC->unk2C0[id & 3] = D_800C3EAC->slots[member].unk0[id & 0xFF];
                id = D_800C3EAC->slots[member].unk0[id & 0xFF];
                if (id == 0xFF) {
                    j += 4;
                    continue;
                }
            }
            n = D_800D2D28->unkD0[lists[i]] * 2;
            D_800D2D28->unkD0[lists[i]] +=
                func_80076A10(id, &D_800C3EA4->unk641C[lists[i]][D_800D2D28->unkD0[lists[i]] * 2],
                             ((s16 *)D_800C2F4C[sets[i]])[j + 1], ((s16 *)D_800C2F4C[sets[i]])[j + 2]);
            shade = ((u16 *)D_800C2F4C[sets[i]])[j + 3];
            value = (shade & 0xFF) >> 1;
            if (((u16 *)D_800C2F4C[sets[i]])[j + 3] >= 0x2000) {
                value = 0x10;
                if (D_800C3EAC->slots[member].items[shade & 0xF] == 0) {
                    value = (shade & 0xF0) >> 1;
                }
            }
            if (value != 0) {
                for (; n < D_800D2D28->unkD0[lists[i]] * 2; n += 2) {
                    SetSemiTrans(&D_800C3EA4->unk641C[lists[i]][n + D_800CCB04.buffer], 1);
                    SetShadeTex(&D_800C3EA4->unk641C[lists[i]][n + D_800CCB04.buffer], 0);
                    (D_800C3EA4->unk641C[lists[i]] + (n + D_800CCB34_word))->r0 = value;
                    (D_800C3EA4->unk641C[lists[i]] + (n + D_800CCB34_word))->g0 = value;
                    (D_800C3EA4->unk641C[lists[i]] + (n + D_800CCB34_word))->b0 = value;
                    D_800C3EA4->unk641C[lists[i]][n + D_800CCB04.buffer].tpage |= 0x20;
                }
            } else {
                for (; n < D_800D2D28->unkD0[lists[i]] * 2; n += 2) {
                    (D_800C3EA4->unk641C[lists[i]] + (n + D_800CCB34_word))->r0 = 0x80;
                    (D_800C3EA4->unk641C[lists[i]] + (n + D_800CCB34_word))->g0 = 0x80;
                    (D_800C3EA4->unk641C[lists[i]] + (n + D_800CCB34_word))->b0 = 0x80;
                }
            }
            j += 4;
        }
    }
    return D_800CCB34_word;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8008CCCC", func_8008D598);
#endif

/* Place a window's four corner glyphs (the alternate set while the battle
 * is ending) at its corners in the current draw buffer. */
void func_8008DC34(u8 window, u16 x, u16 y, u16 w, u16 h) {
    u8 glyphs[4];
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    if (D_800C3E4C != 0) {
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
    block->cornerCount += func_80076A10(glyphs[0], &block->corners[block->cornerCount * 2], x, y);
    block->cornerCount += func_80076A10(glyphs[1], &block->corners[block->cornerCount * 2], x + w - 8, y);
    block->cornerCount += func_80076A10(glyphs[2], &block->corners[block->cornerCount * 2], x, y + h - 8);
    block->cornerCount += func_80076A10(glyphs[3], &block->corners[block->cornerCount * 2], x + w - 8, y + h - 8);
    for (i = 0; i < 4; i++) {
        func_80076B00(&block->corners[i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's top edge: two pieces of texture 1 across the top,
 * each half the inner width, in the current draw buffer. */
void func_8008DE04(u8 window, u16 x, u16 y, u16 w) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[0] + D_800CCB04.buffer, x + 8, y - 8, x + 8 + (w - 16) / 2, y - 8, x + 8, y + 8,
           x + 8 + (w - 16) / 2, y + 8);
    setXY4(block->frame[0] + D_800CCB04.buffer + 2, x + 8 + (w - 16) / 2, y - 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y - 8, x + 8 + (w - 16) / 2, y + 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + 8);
    setUV4(block->frame[0] + D_800CCB04.buffer, WINDOW_TEX_U(1), WINDOW_TEX_V(1), WINDOW_TEX_U(1) + 7,
           WINDOW_TEX_V(1), WINDOW_TEX_U(1), WINDOW_TEX_V(1) + 16, WINDOW_TEX_U(1) + 7, WINDOW_TEX_V(1) + 16);
    setUV4(block->frame[0] + D_800CCB04.buffer + 2, WINDOW_TEX_U(1), WINDOW_TEX_V(1), WINDOW_TEX_U(1) + 7,
           WINDOW_TEX_V(1), WINDOW_TEX_U(1), WINDOW_TEX_V(1) + 16, WINDOW_TEX_U(1) + 7, WINDOW_TEX_V(1) + 16);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[0][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's bottom edge: two pieces of texture 2 across the bottom,
 * each half the inner width, in the current draw buffer. */
void func_8008E430(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[1] + D_800CCB04.buffer, x + 8, y + h - 8, x + 8 + (w - 16) / 2, y + h - 8, x + 8,
           y + h + 8, x + 8 + (w - 16) / 2, y + h + 8);
    setXY4(block->frame[1] + D_800CCB04.buffer + 2, x + 8 + (w - 16) / 2, y + h - 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + h - 8, x + 8 + (w - 16) / 2, y + h + 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + h + 8);
    setUV4(block->frame[1] + D_800CCB04.buffer, WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 1,
           WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2) + 16, WINDOW_TEX_U(2) - 1, WINDOW_TEX_V(2) + 16);
    setUV4(block->frame[1] + D_800CCB04.buffer + 2, WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 1,
           WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2) + 16, WINDOW_TEX_U(2) - 1, WINDOW_TEX_V(2) + 16);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[1][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's left edge: two pieces of texture 3 down the left side,
 * each half the inner height, in the current draw buffer. */
void func_8008EA70(u8 window, u16 x, u16 y, u16 h) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[2] + D_800CCB04.buffer, x - 8, y + 8, x + 8, y + 8, x - 8, y + 8 + (h - 16) / 2, x + 8,
           y + 8 + (h - 16) / 2);
    setXY4(block->frame[2] + D_800CCB04.buffer + 2, x - 8, y + 8 + (h - 16) / 2, x + 8, y + 8 + (h - 16) / 2,
           x - 8, y + 8 + (h - 16) / 2 + (h - 16) / 2, x + 8, y + 8 + (h - 16) / 2 + (h - 16) / 2);
    setUV4(block->frame[2] + D_800CCB04.buffer, WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 30,
           WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3) + 7, WINDOW_TEX_U(3) + 30, WINDOW_TEX_V(3) + 7);
    setUV4(block->frame[2] + D_800CCB04.buffer + 2, WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 30,
           WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3) + 7, WINDOW_TEX_U(3) + 30, WINDOW_TEX_V(3) + 7);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[2][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's right edge: two pieces of texture 4 down the right side,
 * each half the inner height, in the current draw buffer. */
void func_8008F0A8(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[3] + D_800CCB04.buffer, x + w - 8, y + 8, x + w + 8, y + 8, x + w - 8,
           y + 8 + (h - 16) / 2, x + w + 8, y + 8 + (h - 16) / 2);
    setXY4(block->frame[3] + D_800CCB04.buffer + 2, x + w - 8, y + 8 + (h - 16) / 2, x + w + 8,
           y + 8 + (h - 16) / 2, x + w - 8, y + 8 + (h - 16) / 2 + (h - 16) / 2, x + w + 8,
           y + 8 + (h - 16) / 2 + (h - 16) / 2);
    setUV4(block->frame[3] + D_800CCB04.buffer, WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 30,
           WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4) + 7, WINDOW_TEX_U(4) + 30, WINDOW_TEX_V(4) + 7);
    setUV4(block->frame[3] + D_800CCB04.buffer + 2, WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 30,
           WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4) + 7, WINDOW_TEX_U(4) + 30, WINDOW_TEX_V(4) + 7);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[3][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window at (x, y, w, h) in the current draw buffer: its
 * background, corners and edges. The window is hidden while it changes. */
void func_8008F6E4(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = D_800D2E38[window];

    D_800D2D28->windows[window] = 0;
    setXY4(block->shade + D_800CCB04.buffer, x, y, x + w, y, x, y + h, x + w, y + h);
    func_8008DC34(window, x, y, w, h);
    func_8008DE04(window, x, y, w);
    func_8008E430(window, x, y, w, h);
    func_8008EA70(window, x, y, h);
    func_8008F0A8(window, x, y, w, h);
    block->buffer = D_800CCB04.buffer;
    D_800D2D28->windows[window] = 1;
}

/* Open window `window` at (x, y) of w x h: allocate its blocks when it is not
 * shown; `animate` grows it open, otherwise it is drawn at once (and a frame
 * waited with `wait`). */
void func_8008F8F4(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait) {
    WindowRect *rect;

    if (D_800D2D28->windows[window] == 0) {
        D_800D2E38[window] = (void *)func_8008ABB8(0x5A8, 0);
        bzero(D_800D2E38[window], 0x5A8);
        D_800D2D90[window] = (WindowRect *)func_8008ABB8(0xE, 0);
        bzero(D_800D2D90[window], 0xE);
        func_80077454(window);
    }
    if (animate != 0) {
        rect = D_800D2D90[window];
        rect->style = window;
        rect->x = x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        rect->curW = 0;
        rect->curH = 0;
        D_800D2D28->unkBF[window] = 0;
        D_800D2D28->unkB8[window] = 1;
    } else {
        func_8008F6E4(window, x, y, w, h);
        if (wait != 0) {
            func_800716D8();
        }
    }
}

/* Close window `window` and release its two blocks after a frame. */
void func_8008FA60(u8 window) {
    D_800D2D28->windows[window] = 0;
    D_800D2D28->unkB8[window] = 0;
    func_800716D8();
    func_800320E8(D_800D2E38[window]);
    func_800320E8(D_800D2D90[window]);
}

/* Grow every opening window by 32 pixels per frame up to its size, centred,
 * and mark it open when both sides are complete. */
void func_8008FAD8(void) {
    s32 i;
    WindowRect *rect;
    u8 done;

    for (i = 0; i < 7; i++) {
        rect = D_800D2D90[i];
        if (D_800D2D28->unkB8[i] != 0 && D_800D2D28->unkBF[i] == 0) {
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
                D_800D2D28->unkBF[i] = 1;
            }
            func_8008F6E4(rect->style, rect->x + (rect->w >> 1) - (rect->curW >> 1),
                          rect->y + (rect->h >> 1) - (rect->curH >> 1), rect->curW, rect->curH);
        }
    }
}

/* Build a message frame from glyphs into the +0x1e68 list: `rows` side
 * glyphs down from `top`, the corner at (x, y) and a stretched edge of
 * width `w`. */
void func_8008FC1C(s16 x, s16 y, s16 w, s16 top, u8 rows) {
    s32 i;

    D_800D2D28->unkFC = 0;
    for (i = 0; i < rows; i++) {
        D_800D2D28->unkFC += func_80076A10(0x65, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2], x, top + i * 8);
    }
    D_800D2D28->unkFC = func_80076A10(0x64, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2], x, y) + D_800D2D28->unkFC;
    D_800D2D28->unkFC += func_800263E4(D_800D2F5C, 0x64, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2],
                                       D_800CCB04.buffer, x, w, 0x1000, 0, 1);
    D_800D2D28->unkA6 = D_800CCB04.buffer;
    D_800D2D28->unk9D = 1;
}

/* Open the standard message window (0x20, 0x5c, 0xcc x 0x60, style 0xe). */
void func_8008FDE4(void) {
    func_8008FC1C(0x20, 0x5C, 0xCC, 0x60, 0xE);
}

/* Build the item list page: (with `open`) set up the graphics block and the
 * message frame, then render every list entry's two item names and their
 * two-digit counts into VRAM text images (names at 0x380, digits at 0x3c0,
 * one 13-line row per entry pair). */
void func_8008FE18(u8 column, u8 row, u8 open) {
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
        func_80077610();
        func_80076EA4();
        func_8008FC1C(0x20, 0x5C, 0xCC, 0x60, 0xE);
    }
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    for (i = 0; i < 48; i++) {
        ids[i] = D_800D2CE0[i];
        counts[i] = D_800D2CB0[i];
    }
    for (i = 0; i < 32; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
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
            func_80034EAC(func_80033818(ids[i]), images[i].pixels, 0x1B, 0);
        }
        if (ids[i + 16] != 0) {
            func_80034EAC(func_80033818(ids[i + 16]), images[i].pixels, 0x1B, 1);
        }
        func_800769E8(&nameRect, images[i].pixels);
        tens = counts[i] / 10;
        if (tens != 0) {
            digit = D_800C3E5C[tens].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&tensRect, digit);
        if (ids[i] != 0) {
            digit = D_800C3E5C[(u8)(counts[i] % 10)].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&onesRect, digit);
        if ((u8)(D_800D2CC0[i] / 10) != 0) {
            digit = D_800C3E5C[counts[i + 16] / 10].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&tens2Rect, digit);
        if (ids[i + 16] != 0) {
            digit = D_800C3E5C[(u8)(counts[i + 16] % 10)].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&ones2Rect, digit);
    }
    for (i = 0; i < 32; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    func_800716D8();
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 2;
}

/* Build nine glyph rows (0x66) from y + 0x64 into the +0xba8 primitives. */
void func_8009023C(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x68;
    D_800D2D28->unkF8 = 0;
    for (; i < 9; i++) {
        D_800D2D28->unkF8 += func_80076A10(0x66, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x20, (offset - 4) + y);
        offset += 8;
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
}

/* Point the page title quads at list entry (column, row): the entry's image
 * cell (two per image row, 13 lines each; entries past 16 on the second
 * image page with the alternate CLUT). */
void func_80090310(u8 column, u8 row) {
    s32 index;
    s32 page;

    index = row * 2 + column;
    page = 0;
    if (index > 16) {
        index -= 16;
        D_800C3EA4->unkA230->unk140[D_800CCB04.buffer].clut = D_80059414;
        page = 0x10;
    } else {
        D_800C3EA4->unkA230->unk140[D_800CCB04.buffer].clut = D_800595D4;
    }
    func_80076C78(&D_800C3EA4->unkA230->unk140[D_800CCB04.buffer], 0x18, 0x33, (index % 2) * 0x78, (index / 2) * 13,
                  0x60);
    func_80076C78(&D_800C3EA4->unkA230->unk190[D_800CCB04.buffer], 0x84, 0x33, (index % 2) << 6 | page,
                  (index / 2) * 13, 0x10);
}

/* Point the item page icon quads at the images for the item in list cell
 * (column, row): its state icon (9 bit 0x4000, 7 bit 0x1000, else 8) and its
 * level frame (12, 13 or 21 for levels 0-2), each with its CLUT. */
void func_800904A0(u8 column, u8 row) {
    u16 flags;
    s32 icon;
    u8 frame;

    flags = D_800D2200[D_800D2CE0[row * 2 + column]].target;
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
    func_80076C78(&D_800C3EA4->unkA230->unk320[D_800CCB04.buffer], 0xA8, 0x33, D_800D2F68[icon].u, D_800D2F68[icon].v,
                  D_800D2F68[icon].w);
    D_800C3EA4->unkA230->unk320[D_800CCB04.buffer].clut = D_800D2F68[icon].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk370[D_800CCB04.buffer], 0xD0, 0x33, D_800D2F68[frame].u,
                  D_800D2F68[frame].v, D_800D2F68[frame].w);
    D_800C3EA4->unkA230->unk370[D_800CCB04.buffer].clut = D_800D2F68[frame].alternate ? D_80059414 : D_800595D4;
}

/* Render the name of the item in the list cell (column, row) into a text
 * image and place it on the graphics block's +0x280 quad. */
void func_8009070C(u8 column, u8 row) {
    RECT rect;
    u8 item = D_800D2CE0[row * 2 + column];
    u32 *pixels = (u32 *)func_8008AC00(0x39);
    s32 width;

    bzero(pixels, 0x618);
    width = func_80034EAC(func_80033728(D_800D329C, item), pixels, 0x39, 0);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x30, 0x42, 0, 0, width);
    func_800320E8(pixels);
}

/* Show the item list cell (column, row) of the item list or (from the
 * 800c3d70 list) the other list: frame, name and count. */
void func_8009080C(u8 column, u8 row, u8 other) {
    u8 item;

    if (other == 0) {
        item = D_800D2CE0[row * 2 + column];
    } else {
        item = D_800C3D70[row * 2 + column];
    }
    D_800C3EA4->unkA230->unk66D = 0;
    func_80090310(column, row);
    if (item != 0 && other == 0) {
        func_8009070C(column, row);
        func_800904A0(column, row);
        D_800C3EA4->unkA230->unk66D = 1;
    }
    D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk66B = 1;
}

/* Point the four list page quads at the list image from row offset `v`
 * (offsets from 0x68 on the second image page with the alternate CLUT). */
void func_8009093C(s32 v) {
    s32 page = 0;

    if (v >= 0x68) {
        v -= 0x68;
        D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_80059414;
        D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_80059414;
        page = 0x10;
    } else {
        D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
        D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_800595D4;
    }
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x30, 0x60, 0, v, 0x60, 0x68);
    func_80076CE8(&D_800C3EA4->unkA230->unk50[D_800CCB04.buffer], 0xB4, 0x60, 0x78, v, 0x60, 0x68);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x98, 0x60, page, v, 0x10, 0x68);
    func_80076CE8(&D_800C3EA4->unkA230->unkF0[D_800CCB04.buffer], 0x11C, 0x60, page | 0x40, v, 0x10, 0x68);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
}

/* Animate the five-frame cursor glyph at (x, y): advance `frame` every third
 * tick. */
void func_80090B90(s32 x, s32 y, s32 *frame, u8 *ticks) {
    if (++*ticks >= 3) {
        *frame -= 1;
        if (*frame < 0) {
            *frame = 4;
        }
        *ticks = 0;
    }
    D_800D2D28->unk100 = func_80076A10(*frame + 0xE0, D_800C3EA4->unk27C8, x, y);
    D_800D2D28->unkA7 = D_800CCB04.buffer;
    D_800D2D28->unk9E = 1;
}

/* Show the member's EP and maximum EP as two digit glyphs each (no leading
 * zero). */
void func_80090C44(u8 member) {
    u16 digit;

    digit = D_800CCCE8.records[member].pilot.ep / 10;
    if (digit != 0) {
        func_80076C78(&D_800C3EA4->unkA230->unk460[D_800CCB04.buffer], 0x104, 0xC6, digit * 8 + 0x78, 0, 8);
    }
    func_80076C78(&D_800C3EA4->unkA230->unk4B0[D_800CCB04.buffer], 0x10C, 0xC6,
                  (u16)(D_800CCCE8.records[member].pilot.ep % 10) * 8 + 0x78, 0, 8);
    digit = D_800CCCE8.records[member].pilot.maxEp / 10;
    if (digit != 0) {
        func_80076C78(&D_800C3EA4->unkA230->unk500[D_800CCB04.buffer], 0x11C, 0xC6, digit * 8 + 0x78, 0, 8);
    }
    func_80076C78(&D_800C3EA4->unkA230->unk550[D_800CCB04.buffer], 0x124, 0xC6,
                  (u16)(D_800CCCE8.records[member].pilot.maxEp % 10) * 8 + 0x78, 0, 8);
}

/* Show the member's EP panel: the two EP icons (cells 6 and 5), the EP
 * digits and the EP label glyphs. */
void func_80090E7C(u8 member) {
    func_80076C78(&D_800C3EA4->unkA230->unk3C0[D_800CCB04.buffer], 0xA8, 0xA6, D_800D2F68[6].u, D_800D2F68[6].v,
                  D_800D2F68[6].w);
    D_800C3EA4->unkA230->unk3C0[D_800CCB04.buffer].clut = D_800D2F68[6].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk410[D_800CCB04.buffer], 0xEC, 0xC6, D_800D2F68[5].u, D_800D2F68[5].v,
                  D_800D2F68[5].w);
    D_800C3EA4->unkA230->unk410[D_800CCB04.buffer].clut = D_800D2F68[5].alternate ? D_80059414 : D_800595D4;
    func_80090C44(member);
    func_80076A10(0x71, D_800C3EA4->unkA230->unk5A0, 0x118, 0xD1);
    D_800C3EA4->unkA230->unk66C = D_800CCB04.buffer;
}

/* Build the member's art list page: set up the graphics block and the
 * message frame, then render the name and two-digit EP cost of every art its
 * character (or its gear) knows into VRAM text images, then the EP panel. */
void func_80091064(u8 member) {
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

    func_80077610();
    func_80076EA4();
    func_8008FC1C(0x20, 0x30, 0x98, 0x38, 0xC);
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    if (D_800D32A0[member].unk1 == 0) {
        for (i = 0; i < 16; i++) {
            if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask2, i)) {
                known[i] = 1;
                costs[i] = D_800CCCE8.partyCommands[member][i + 22].cost;
            } else {
                known[i] = 0;
                costs[i] = 0;
            }
        }
    } else {
        for (i = 0; i < 16; i++) {
            if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask6, i)) {
                known[i] = 1;
                costs[i] = D_800CCCE8.gearCommands[member][i + 21].cost;
            } else {
                known[i] = 0;
                costs[i] = 0;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        rowRect.x = (i % 2) * 30 + 0x380;
        rowRect.y = (i / 2) * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        func_800769E8(&rowRect, D_800D2DB0);
        if (known[i] != 0) {
            if (D_800D32A0[member].unk1 == 0) {
                func_80034EAC(func_80033908(D_800CCCE8.records[member].pilot.characterId * 16 + i), images[i].pixels, 0x1B,
                              0);
            } else {
                func_80034EAC(func_800339FC(D_800CCCE8.records[member].pilot.gearId * 16 + i), images[i].pixels, 0x1B, 0);
            }
            nameRect.x = (i % 2) * 30 + 0x380;
            nameRect.y = (i / 2) * 16 + 0x102;
            nameRect.w = 30;
            nameRect.h = 13;
            func_800769E8(&nameRect, images[i].pixels);
        }
        if (!(i & 1)) {
            rightRect.x = 0x3C0;
            rightRect.y = (i / 2) * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            func_800769E8(&rightRect, D_800D2DB0);
        }
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 16 + 0x102;
        tensRect.w = 6;
        tensRect.h = 13;
        tens = costs[i] / 10;
        if (tens != 0) {
            digit = D_800C3E5C[tens].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&tensRect, digit);
        onesRect.x = (i % 2) * 16 + 0x3C2;
        onesRect.y = (i / 2) * 16 + 0x102;
        onesRect.w = 6;
        onesRect.h = 13;
        if (known[i] != 0) {
            digit = D_800C3E5C[(u8)(costs[i] % 10)].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&onesRect, digit);
    }
    func_80090E7C(member);
    for (i = 0; i < 16; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    func_800716D8();
    D_800C3EA4->unkA230->unk140[0].clut = D_800595D4;
    D_800C3EA4->unkA230->unk140[1].clut = D_800595D4;
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 1;
}

/* Build eight glyph rows (0x66) from y + 0x38 into the +0xba8 primitives. */
void func_80091604(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x38;
    D_800D2D28->unkF8 = 0;
    for (; i < 8; i++) {
        D_800D2D28->unkF8 += func_80076A10(0x66, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x20, offset + y);
        offset += 8;
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
}

/* Point the page title quad at entry (column, row) of the technique image
 * and, for character 1 (Fei), show the entry's cost (8009a258) as up to three
 * digit glyphs. */
void func_800916D4(u8 column, u8 row, u8 member) {
    u32 index;
    u32 cellX;
    u32 cellY;
    s32 i;
    s32 x;
    u8 digit;

    index = row * 2 + column;
    cellY = index / 2;
    cellX = index - cellY * 2;
    func_80076CE8(&D_800C3EA4->unkA230->unk140[D_800CCB04.buffer], 0x20, 0xA4, cellX * 0x78, cellY * 16, 0x60, 0x10);
    D_800C3EA4->unkA230->unk66E = 0;
    if (D_800D2D24[member] == 1) {
        /* 8009a258 is called unprototyped: the command is passed and its
         * result returned unnarrowed. */
        func_8008AAA0(((s32 (*)())func_8009A258)(member, index + 0x16));
        for (i = 0, x = 0x8C; i < 3; i++) {
            digit = D_800C3CF4[i + 6];
            if (digit != 0xFF) {
                func_80076C78(&D_800C3EA4->unkA230->unk190[D_800C3EA4->unkA230->unk66E * 2 + D_800CCB04.buffer], x,
                              0xA6, digit * 8 + 0x78, 0, 8);
                D_800C3EA4->unkA230->unk66E++;
            }
            x += 8;
        }
    }
}

/* Point the art page icon quads at the images for art (column, row) of the
 * member (its gear's arts in a gear): its state icon (9 sealed, 7 flag
 * 0x1000, else 8) and its level frame (13 level 1, 21 level 2, else 12). */
void func_8009187C(u8 member, u8 column, u8 row) {
    u16 state;
    s32 icon;
    s32 frame;

    if (D_800D32A0[member].unk1 == 0) {
        state = D_800CCCE8.partyCommands[member][row * 2 + column + 22].state;
    } else {
        state = D_800CCCE8.gearCommands[member][row * 2 + column + 21].state;
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
    func_80076C78(&D_800C3EA4->unkA230->unk320[D_800CCB04.buffer], 0xDA, 0xAA, D_800D2F68[icon].u, D_800D2F68[icon].v,
                  D_800D2F68[icon].w);
    D_800C3EA4->unkA230->unk320[D_800CCB04.buffer].clut = D_800D2F68[icon].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk370[D_800CCB04.buffer], 0xFE, 0xAA, D_800D2F68[frame].u,
                  D_800D2F68[frame].v, D_800D2F68[frame].w);
    D_800C3EA4->unkA230->unk370[D_800CCB04.buffer].clut = D_800D2F68[frame].alternate ? D_80059414 : D_800595D4;
}

/* Build the description of art (column, row) of the member (its gear's in
 * a gear): two text lines from the menu module block into VRAM, placed on
 * the page's two description quads. */
void func_80091B38(u8 member, u8 column, u8 row) {
    RECT rect;
    u32 *pixels;
    s32 text;
    s32 width0;
    s32 width1;

    if (D_800D32A0[member].unk1 == 0) {
        text = D_800D2D24[member] * 32 + (row * 2 + column) * 2;
    } else {
        text = D_8006D8A0.characters[D_800D2D24[member]].gearId * 32 + (row * 2 + column) * 2;
    }
    pixels = (u32 *)func_8008AC00(0x39);
    bzero(pixels, 0x618);
    width0 = func_80034EAC(func_80033728(D_800D367C, text & 0xFFFF), pixels, 0x39, 0);
    width1 = func_80034EAC(func_80033728(D_800D367C, (text & 0xFFFF) | 1), pixels, 0x39, 1);
    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x3C;
    rect.h = 13;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x30, 0xB6, 0, 0, width0);
    func_80076C78(&D_800C3EA4->unkA230->unk2D0[D_800CCB04.buffer], 0x30, 0xC6, 0, 0, width1);
    func_800320E8(pixels);
}

/* Open the combo/technique entry (column, row) of the member's page when its
 * character knows it (mask +2 on foot, +6 in a gear): build its graphics for
 * the current draw buffer; otherwise mark the page closed. */
void func_80091D38(member, column, row)
u8 member;
u8 column;
u8 row;
{
    u8 known = 0;

    if (D_800D32A0[member].unk1 == 0) {
        known = func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask2, column + row * 2) != 0;
    } else if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask6, column + row * 2)) {
        known = 1;
    }
    if (known) {
        func_800916D4(column, row, member);
        func_8009187C(member, column, row);
        func_80091B38(member, column, row);
        D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
        D_800C3EA4->unkA230->unk66B = 1;
    } else {
        D_800C3EA4->unkA230->unk66B = 0;
    }
}

/* Point the four art page quads at the art list image from row `v`. */
void func_80091EC4(u8 v) {
    D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
    D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_800595D4;
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x34, 0x34, 0, v, 0x60, 0x60);
    func_80076CE8(&D_800C3EA4->unkA230->unk50[D_800CCB04.buffer], 0xB8, 0x34, 0x78, v, 0x60, 0x60);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x9C, 0x34, 0, v, 0x10, 0x60);
    func_80076CE8(&D_800C3EA4->unkA230->unkF0[D_800CCB04.buffer], 0x120, 0x34, 0x40, v, 0x10, 0x60);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
}

/* Show the gear page list image on the four page quads and mark the page
 * (command window page 3) open. */
void func_8009209C(void) {
    D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
    D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_800595D4;
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x36, 0x62, 0, 0, 0x60, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unk50[D_800CCB04.buffer], 0xC2, 0x62, 0x78, 0, 0x60, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x96, 0x62, 0, 0, 0x10, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unkF0[D_800CCB04.buffer], 0x122, 0x62, 0x40, 0, 0x10, 0x40);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 3;
}

/* Set up the gear page's shaded bar and black box primitives, then build its
 * glyphs (from the 800c33b4 table) for every entry of `shown` that is not
 * 0xff into the +0x1e68 list. */
void func_80092298(u8 member, u8 *shown) {
    s32 i;
    s32 glyph;

    for (i = 0; i < 2; i++) {
        SetPolyG4(&D_800C3EA4->unkA230->unk5F0[i]);
        (D_800C3EA4->unkA230->unk5F0 + i)->r0 = 0xFF;
        (D_800C3EA4->unkA230->unk5F0 + i)->g0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->r1 = 0xFF;
        (D_800C3EA4->unkA230->unk5F0 + i)->g1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->r2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->g2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->r3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->g3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y3 = 0;
    }
    for (i = 0; i < 2; i++) {
        SetPolyF4(&D_800C3EA4->unkA230->unk638[i]);
        (D_800C3EA4->unkA230->unk638 + i)->r0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->g0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->b0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x1 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y1 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x2 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y2 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x3 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y3 = 0;
    }
    D_800D2D28->unkFC = 0;
    for (glyph = 0; glyph < 16; glyph++) {
        if (shown[glyph] != 0xFF) {
            D_800D2D28->unkFC += func_80076A10(D_800C33B4[glyph], &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2],
                                               D_800C33C4[glyph], D_800C3404[glyph]);
        }
    }
    D_800D2D28->unkA6 = D_800CCB04.buffer;
    D_800D2D28->unk9D = 1;
}

/* Build the member's gear page: set up the graphics block, render the name
 * and two-digit count of each of the seven gear parts in `ids` (0xff none)
 * and the fixed eighth entry (system text 10) into VRAM text images, then
 * the page glyphs and quads. */
void func_80092784(u8 member, u8 *ids, u8 *counts) {
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

    func_80077610();
    func_80076EA4();
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    nameWidth = 30;
    for (i = 0; i < 8; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        rowRect.x = (i % 2) * 30 + 0x380;
        rowRect.y = (i / 2) * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        func_800769E8(&rowRect, D_800D2DB0);
        if (i != 7) {
            if (ids[i] != 0xFF) {
                func_80034EAC(func_80033784(D_800D2D24[member], ids[i]), images[i].pixels, 0x1B, 0);
                nameRect.x = (i % 2) * 30 + 0x380;
                nameRect.y = (i / 2) * 16 + 0x102;
                nameRect.w = nameWidth;
                nameRect.h = 13;
                func_800769E8(&nameRect, images[i].pixels);
            }
        } else {
            func_80034EAC(func_800338D8(10), images[7].pixels, 0x1B, 0);
            nameRect.x = 0x39E;
            nameRect.y = 0x132;
            nameRect.w = nameWidth;
            nameRect.h = 13;
            func_800769E8(&nameRect, images[7].pixels);
        }
        if (!(i & 1)) {
            rightRect.x = 0x3C0;
            rightRect.y = (i / 2) * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            func_800769E8(&rightRect, D_800D2DB0);
        }
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 16 + 0x102;
        tensRect.w = 6;
        tensRect.h = 13;
        if (i != 7) {
            tens = counts[i] / 10;
            if (tens != 0) {
                digit = D_800C3E5C[tens].pixels;
            } else {
                digit = D_800D2DB0;
            }
            func_800769E8(&tensRect, digit);
            onesRect.x = (i % 2) * 16 + 0x3C2;
            onesRect.y = (i / 2) * 16 + 0x102;
            onesRect.w = 6;
            onesRect.h = 13;
            if (ids[i] != 0xFF) {
                func_800769E8(&onesRect, D_800C3E5C[(u8)(counts[i] % 10)].pixels);
            } else {
                func_800769E8(&onesRect, D_800D2DB0);
            }
        } else {
            func_800769E8(&tensRect, D_800D2DB0);
            onesRect.x = 0x3D2;
            onesRect.y = 0x132;
            onesRect.w = 6;
            onesRect.h = 13;
            func_800769E8(&onesRect, D_800D2DB0);
        }
    }
    func_80092298(member, ids);
    for (i = 0; i < 8; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    func_8009209C();
}

/* Build the combo entry display: the AP count as one or two digit glyphs,
 * the button glyph of each entered step with separators, and the remaining
 * AP bar (4 pixels per point) on the shaded bar and black box. */
void func_80092B74(u8 member, u8 ap) {
    u8 digits[2];
    u8 tens;
    s32 i;

    digits[0] = tens = ap / 10;
    digits[1] = ap - tens * 10;
    D_800D2D28->unkF8 = 0;
    if (digits[0] != 0) {
        D_800D2D28->unkF8 += func_80076A10(digits[0] + 0x67, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x56, 0x38);
    }
    D_800D2D28->unkF8 += func_80076A10(digits[1] + 0x67, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x5E, 0x38);
    for (i = 0; i < 7; i++) {
        if (D_800C3EAC->combo[i] == 0xFF) {
            break;
        }
        D_800D2D28->unkF8 += func_80076A10(D_800C3DE0[i] + 0x39, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], i * 32 + 0x2A, 0x4A);
        if (i != 0) {
            D_800D2D28->unkF8 += func_80076A10(0xA, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2],
                                               (i - 1) * 32 + 0x3A, 0x4A);
        }
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x0 = 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y0 = 0x34;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x1 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y1 = 0x34;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x2 = 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y2 = 0x3C;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x3 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y3 = 0x3C;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x0 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y0 = 0x34;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x1 = 0xF0;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y1 = 0x34;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x2 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y2 = 0x3C;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x3 = 0xF0;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y3 = 0x3C;
    D_800C3EA4->unkA230->unk66C = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk66F = 1;
}

/* Build the member's gear value page: the four entries' names (gear text
 * gearId * 4 + entry) and up to four-digit values (no leading zeros) into
 * VRAM text images; each shown digit is taken off the value. */
void func_800930AC(u8 member, u8 *present, u16 *values) {
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
    func_80077610();
    func_80076EA4();
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    for (i = 0; i < 4; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        rowRect.x = 0x380;
        rowRect.y = i * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        func_800769E8(&rowRect, D_800D2DB0);
        if (present[i] != 0xFF) {
            func_80034EAC(func_80033A8C(D_800CCCE8.records[member].pilot.gearId * 4 + i), images[i].pixels, 0x1B, 0);
            nameRect.x = 0x380;
            nameRect.y = i * 16 + 0x102;
            nameRect.w = 30;
            nameRect.h = 13;
            func_800769E8(&nameRect, images[i].pixels);
        }
        rightRect.x = 0x3C0;
        rightRect.y = i * 16 + 0x100;
        rightRect.w = 0x1B;
        rightRect.h = 16;
        func_800769E8(&rightRect, D_800D2DB0);
        shown = 0;
        for (j = 0; j < 3; j++) {
            digitRect.x = j * 2 + 0x3C0;
            digitRect.y = i * 16 + 0x102;
            digitRect.w = 6;
            digitRect.h = 13;
            digit = values[i] / divisors[j];
            if (digit != 0 || shown) {
                func_800769E8(&digitRect, D_800C3E5C[digit].pixels);
                shown = 1;
                values[i] -= digit * divisors[j];
            } else {
                func_800769E8(&digitRect, D_800D2DB0);
            }
        }
        onesRect.x = 0x3C6;
        onesRect.y = i * 16 + 0x102;
        onesRect.w = 6;
        onesRect.h = 13;
        if (present[i] != 0xFF) {
            func_800769E8(&onesRect, D_800C3E5C[(u16)(values[i] % 10)].pixels);
        } else {
            func_800769E8(&onesRect, D_800D2DB0);
        }
    }
    for (i = 0; i < 4; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x94, 0x54, 0, 0, 0x60, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0xFC, 0x54, 0, 0, 0x20, 0x40);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 4;
}

/* Point the gear page `kind` quads at their images: the page title cell, the
 * command's state icon (9 sealed, 7 flag 0x1000, else 8) and its level frame
 * (13 for level 1, 21 for level 2, else 12), each with its CLUT. */
void func_80093578(u8 member, u8 kind) {
    u16 state;
    s32 icon;
    s32 frame;

    func_80076CE8(&D_800C3EA4->unkA230->unk140[D_800CCB04.buffer], 0x7C, 0xA4, 0, kind * 16, 0x60, 0x10);
    state = D_800CCCE8.gearCommands[member][kind + 37].state;
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
    func_80076C78(&D_800C3EA4->unkA230->unk320[D_800CCB04.buffer], 0x24, 0xA6, D_800D2F68[icon].u, D_800D2F68[icon].v,
                  D_800D2F68[icon].w);
    D_800C3EA4->unkA230->unk320[D_800CCB04.buffer].clut = D_800D2F68[icon].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk370[D_800CCB04.buffer], 0x48, 0xA6, D_800D2F68[frame].u,
                  D_800D2F68[frame].v, D_800D2F68[frame].w);
    D_800C3EA4->unkA230->unk370[D_800CCB04.buffer].clut = D_800D2F68[frame].alternate ? D_80059414 : D_800595D4;
}

/* Build the name of the member's gear page `kind` (two text lines from the
 * file 3 block) into VRAM and point the page's two title quads at them. */
void func_8009382C(u8 member, u8 kind) {
    RECT rect;
    u32 *pixels;
    s32 text;
    s32 width0;
    s32 width1;

    text = (D_800CCCE8.records[member].pilot.gearId * 4 + kind) * 2;
    pixels = (u32 *)func_8008AC00(0x39);
    bzero(pixels, 0x618);
    width0 = func_80034EAC(func_80033728(D_800C3DE8, text & 0xFFFF), pixels, 0x39, 0);
    width1 = func_80034EAC(func_80033728(D_800C3DE8, (text & 0xFFFF) | 1), pixels, 0x39, 1);
    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x3C;
    rect.h = 13;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x88, 0xB6, 0, 0, width0);
    func_80076C78(&D_800C3EA4->unkA230->unk2D0[D_800CCB04.buffer], 0x88, 0xC6, 0, 0, width1);
    func_800320E8(pixels);
}

/* Open the member's `kind` page (0-2 the special pages, 3 the item page)
 * when its character has it and no status seals it; the page's graphics
 * are built for the current draw buffer. Otherwise mark the page closed. */
void func_800939CC(u8 member, u8 kind) {
    u16 seals[4];

    seals[0] = D_800C3234[13];
    seals[1] = D_800C3234[14];
    seals[2] = D_800C3234[15];
    seals[3] = D_800C3234[3];
    if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].flags1A, kind) &&
        !(D_800CCCE8.records[member].pilot.status7A & seals[kind])) {
        func_80093578(member, kind);
        func_8009382C(member, kind);
        D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
        D_800C3EA4->unkA230->unk66B = 1;
    } else {
        D_800C3EA4->unkA230->unk66B = 0;
    }
}

/* For party character 4 with no command page open: show the two equipped
 * items' (in a gear: parts') names and durability (two digits, no leading
 * zero) in window 0. */
void func_80093B08(u8 member) {
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

    if (D_800D2D24[member] == 4 && D_800D2D28->unkB7 == 0) {
        divisors[0] = 100;
        divisors[1] = 10;
        if (D_800D32A0[member].unk1 == 0) {
            slots[0] = D_8006D8A0.characters[D_800D2D24[member]].entryItems[0];
            slots[1] = D_8006D8A0.characters[D_800D2D24[member]].entryItems[3];
            values[0] = D_8006F8BA[slots[0]];
            values[1] = D_8006F8BA[slots[1]];
        } else {
            slots[0] = D_8006D8A0.gears[D_8006D8A0.characters[D_800D2D24[member]].gearId].partItems[0];
            slots[1] = D_8006D8A0.gears[D_8006D8A0.characters[D_800D2D24[member]].gearId].partItems[3];
            values[0] = D_8006F8EA[slots[0]];
            values[1] = D_8006F8EA[slots[1]];
        }
        func_80077610();
        func_80076EA4();
        D_800D2DB0 = (u32 *)func_8008AC00(0x39);
        bzero(D_800D2DB0, 0x618);
        for (i = 0; i < 2; i++) {
            images[i].pixels = (u32 *)func_8008AC00(0x1B);
            bzero(images[i].pixels, 0x30C);
            rowRect.x = 0x380;
            rowRect.y = i * 16 + 0x100;
            rowRect.w = 0x1B;
            rowRect.h = 16;
            func_800769E8(&rowRect, D_800D2DB0);
            if (slots[i] != 0) {
                func_80034EAC(func_80033848(slots[i]), images[i].pixels, 0x1B, 0);
                nameRect.x = 0x380;
                nameRect.y = i * 16 + 0x102;
                nameRect.w = 30;
                nameRect.h = 13;
                func_800769E8(&nameRect, images[i].pixels);
            }
            rightRect.x = 0x3C0;
            rightRect.y = i * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            func_800769E8(&rightRect, D_800D2DB0);
            shown = 0;
            for (j = 0; j < 2; j++) {
                digitRect.x = j * 2 + 0x3C0;
                digitRect.y = i * 16 + 0x102;
                digitRect.w = 6;
                digitRect.h = 13;
                digit = values[i] / divisors[j];
                if (digit != 0 || shown) {
                    func_800769E8(&digitRect, D_800C3E5C[digit].pixels);
                    shown = 1;
                    values[i] -= digit * divisors[j];
                } else {
                    func_800769E8(&digitRect, D_800D2DB0);
                }
            }
            onesRect.x = 0x3C4;
            onesRect.y = i * 16 + 0x102;
            onesRect.w = 6;
            onesRect.h = 13;
            if (slots[i] != 0) {
                func_800769E8(&onesRect, D_800C3E5C[(u16)(values[i] % 10)].pixels);
            } else {
                func_800769E8(&onesRect, D_800D2DB0);
            }
        }
        for (i = 0; i < 2; i++) {
            func_800320E8(images[i].pixels);
        }
        func_800320E8(D_800D2DB0);
        D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
        func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0xA0, 0xAC, 0, 0, 0x60, 0x20);
        func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x104, 0xAC, 0, 0, 0x18, 0x20);
        D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
        D_800C3EA4->unkA230->unk669 = 1;
        func_8008F8F4(0, 0x98, 0xA8, 0x8C, 0x28, 0, 1);
        D_800D2D28->unkB7 = 5;
        D_800D2D28->unk8E = 1;
    }
}

/* For party character 4: close its command window and, with `release`,
 * its extra resources. */
void func_8009413C(u8 member, u8 release) {
    if (D_800D2D24[member] == 4) {
        D_800D2D28->unkB7 = 0;
        D_800D2D28->windows[0] = 0;
        if (release != 0) {
            func_8007FB70(member);
        }
    }
}

/* Resolve the committed action: select the attacker and its command
 * descriptor (a gear's goes to 8009c198), then for every target in the
 * target mask run the descriptor's formula, the post-adjustment and the
 * status step its flags ask for; finally publish the command, hold the
 * attacker's commands and raise the used command's use count. Returns the
 * resolve status, except on the two gear paths, which return without a
 * value. */
u8 func_800941A4(void) {
    u16 bit;
    u8 member;

    func_80097D08();
    D_800D2DB8 = 0;
    D_800C34AE = 0;
    D_800C3E04 = D_800C34B0->attackerIndex;
    D_800C3E00 = &D_800C34B0->records[D_800C3E04];
    D_800D2D6C = &D_800C34B0->records[D_800C3E04].gear;
    if (D_800C34B0->records[D_800C34B0->attackerIndex].flags15A & 0x80) {
        func_8009C198();
        func_8009AB38(D_800C3E04);
        return;
    }
    if (D_800C3E04 < 3) {
        D_800C3DFC = &D_800C34B0->partyCommands[D_800C3E04][D_800C34B0->commandIndex];
    } else {
        D_800C3DFC = &D_800C34B0->enemyCommands[D_800C34B0->commandIndex];
    }
    func_8009AC48(D_800C3E04, 1);
    if (D_800C3DFC->flagsA & 0x10) {
        func_8009C198();
        func_8009AB38(D_800C3E04);
        func_80094C78();
        return;
    }
    D_800D2DC4 = 0;
    if ((D_800C3DFC->flagsA & 0x100) && D_800C3E00->pilot.characterId == 1) {
        func_80096824();
    }
    if (D_800C3E00->pilot.characterId == 4 && (D_800C34B0->commandIndex == 4 || D_800C34B0->commandIndex == 5)) {
        D_800C3DFC->attributes[2] = D_800C3E00->pilot.entries[D_800C34B0->commandIndex - 3].value4;
    }
    if ((D_800C3E00->pilot.status80 & 0x20) && (D_800D2C94.targets & 7) && D_800D2C94.targets < 7) {
        for (member = 0; member < 3; member++) {
            if (D_800CCCE8.records[member].pilot.characterId == 3) {
                D_800D2C94.targets = 1 << member;
            }
        }
    }
    bit = 1;
    for (D_800C3E50 = 0; D_800C3E50 < 11; D_800C3E50++, bit <<= 1) {
        if (bit & D_800C34B0->targetMask) {
            D_800C3E34 = &D_800C34B0->records[D_800C3E50];
            D_800D2DC8 = &D_800C34B0->records[D_800C3E50].gear;
            D_800C3D60 = &D_800C34B0->records[D_800C3E50].field148;
            if (D_800C3E50 < 3) {
                func_800968C0();
            }
            D_800C348C[D_800C3DFC->formula]();
            func_800946F4();
            if (D_800C34B0->resultCode[D_800C3E50] == 0) {
                if (D_800C3DFC->flagsA & 0x800) {
                    func_80095B44();
                } else if (D_800C3DFC->flagsA & 0x4000) {
                    func_80095A78();
                } else if (D_800C3DFC->flagsA & 4) {
                    func_800958D8();
                }
            }
            if (D_800C3DFC->flagsA & 1) {
                D_800C34B0->shownCommand = D_800C3DFC->name;
            } else {
                D_800C34B0->shownCommand = D_800C34B0->commandIndex;
            }
        }
    }
    func_80099FB0();
    func_8009AB38(D_800C3E04);
    if (D_800C3E00->pilot.characterId == 4 && D_800C34AE == 0) {
        func_8009AFD8();
    }
    if (D_800C3E04 < 3 && D_800C34B0->commandIndex < 7) {
        if (D_800C3E00->pilot.useCounts[D_800C34B0->commandIndex] <= 0xFDE7) {
            D_800C3E00->pilot.useCounts[D_800C34B0->commandIndex] +=
                D_800C3E00->pilot.pad55 + D_800C3E00->pilot.padA1[0];
        }
    }
    func_80094C78();
    return D_800D2DB8;
}

/* Per-target follow-up of a party or enemy action. On a hit (result 0): a
 * party attacker's counter +0x3a rises when the damage equals the target's
 * HP; the target loses status80 bit 0x1000 and, on 70%, 0x2000; an ether
 * command (flags 0x100) gets result 2 against flags36 0x4000 (or 0x2000
 * without attribute 2 bits); flags32 0x80 halves (60%, 80% for character
 * 0) or raises damage by half; flags32 0x20 turns it on the attacker; a
 * party attack clears the target's status7c bit 2 (restoring status7a); a
 * blow at least character 3's HP clears every enemy's status80 bit 0x20.
 * Then flags36 0x8000 swaps results 0/5 and 2, status84 0x80 doubles
 * result-2 damage and status88 0x200 nullifies result-1 damage. */
void func_800946F4(void) {
    s32 chance;
    u8 slot;

    if (D_800C34B0->resultCode[D_800C3E50] == 0) {
        if (D_800C34B0->damage[D_800C3E50] == D_800C3E34->pilot.hp && D_800C3E04 < 3) {
            if (++D_800C3E00->pilot.field3A > 0xFDE8) {
                D_800C3E00->pilot.field3A--;
            }
        }
        D_800C3E34->pilot.status80 &= ~0x1000;
        if ((D_800C3E34->pilot.status80 & 0x2000) && rand() % 100 < 70) {
            D_800C3E34->pilot.status80 &= ~0x2000;
        }
        if (D_800C3DFC->flagsA & 0x100) {
            if (D_800C3E34->pilot.flags36 & 0x4000) {
                D_800C34B0->resultCode[D_800C3E50] = 2;
            }
            if ((D_800C3E34->pilot.flags36 & 0x2000) && !(D_800C3DFC->attributes[2] & 0xF)) {
                D_800C34B0->resultCode[D_800C3E50] = 2;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x80) {
            chance = 60;
            if (D_800C3E34->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                D_800C34B0->damage[D_800C3E50] >>= 1;
            } else {
                D_800C34B0->damage[D_800C3E50] += D_800C34B0->damage[D_800C3E50] >> 1;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x20) {
            D_800C34B0->resultCode[D_800C3E04] = 0;
            D_800C34B0->damage[D_800C3E04] = D_800C34B0->damage[D_800C3E50];
        }
        if (D_800C3E04 < 3 && D_800C3E04 != D_800C3E50) {
            if (D_800C34B0->records[D_800C3E50].pilot.status7C & 2) {
                D_800C34B0->records[D_800C3E50].pilot.status7C &= ~2;
                D_800C34B0->records[D_800C3E50].pilot.status7A = D_800C3AA4[D_800C3E50];
            }
        }
        if (D_800C3E34->pilot.characterId == 3 && (u32)D_800D2C54[D_800C3E50] >= D_800C3E34->pilot.hp) {
            for (slot = 3; slot < 11; slot++) {
                D_800CCCE8.records[slot].pilot.status80 &= ~0x20;
            }
        }
    }
    if (D_800C3E34->pilot.flags36 & 0x8000) {
        switch (D_800C34B0->resultCode[D_800C3E50]) {
        case 0:
        case 5:
            D_800C34B0->resultCode[D_800C3E50] = 2;
            break;
        case 2:
            D_800C34B0->resultCode[D_800C3E50] = 0;
            break;
        }
    }
    if ((D_800C3E34->pilot.status84.half.permanent & 0x80) && D_800C34B0->resultCode[D_800C3E50] == 2) {
        D_800C34B0->damage[D_800C3E50] *= 2;
    }
    if ((D_800C3E34->pilot.status88.half.permanent & 0x200) && D_800C34B0->resultCode[D_800C3E50] == 1) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
}

/* Add the damage dealt to the target to each hit party member's running
 * total (+0x5f60 with record flag 0x80 at +0x15a, else +0x5f54). */
void func_80094C78(void) {
    u8 member;

    for (member = 0; member < 3; member++) {
        if (D_800C34B0->resultCode[member] == 0) {
            if (D_800C34B0->records[member].flags15A & 0x80) {
                D_800C34B0->field5F60[member] += D_800C34B0->damage[D_800C3E50];
            } else {
                D_800C34B0->field5F54[member] += D_800C34B0->damage[D_800C3E50];
            }
        }
    }
}

/* With one party member out of action (status7c 0xc000) and two carrying
 * status7c bit 2, or two and one, clear bit 2 on the party and restore their
 * status7a. */
void func_80094D24(void) {
    u8 member;
    u8 down;
    u8 marked;

    down = 0;
    for (member = 0; member < 3; member++) {
        if (D_800C34B0->records[member].pilot.status7C & 0xC000) {
            down++;
        }
    }
    marked = 0;
    for (member = 0; member < 3; member++) {
        if (D_800C34B0->records[member].pilot.status7C & 2) {
            marked++;
        }
    }
    if (down == 1 && marked == 2) {
        for (member = 0; member < 3; member++) {
            if (D_800C34B0->records[member].pilot.status7C & 2) {
                D_800C34B0->records[member].pilot.status7C &= ~2;
                D_800C34B0->records[member].pilot.status7A = D_800C3AA4[member];
            }
        }
    }
    if (down == 2 && marked == 1) {
        for (member = 0; member < 3; member++) {
            if (D_800C34B0->records[member].pilot.status7C & 2) {
                D_800C34B0->records[member].pilot.status7C &= ~2;
                D_800C34B0->records[member].pilot.status7A = D_800C3AA4[member];
            }
        }
    }
}

/* Formula type 0 (physical and ether damage): none against an immune target
 * (flags34 0x8000, 0x4000 for ether); otherwise attack and defense adjusted
 * by both sides' statuses and the command's attributes, scaled 4:3 (5:4 for
 * ether), by the power / 20 for kinds 0-1, randomised, then shaped by the hit
 * outcome and clamped to 0-9999. */
void func_80094EE4(void) {
    u16 attack;
    u16 defense;
    s8 hit;
    u8 power;
    s32 attackScale;
    s32 defenseScale;
    s32 amount;
    s32 immune;
    s32 kind;

    power = D_800C3DFC->power;
    if (D_800C3DFC->flagsA & 0x100) {
        immune = D_800C3E34->pilot.flags34 & 0x4000;
    } else {
        immune = D_800C3E34->pilot.flags34 & 0x8000;
    }
    if (immune) {
        D_800C34B0->resultCode[D_800C3E50] = 0;
        return;
    }
    hit = func_80096AB8();
    attack = func_80096FBC();
    defense = func_80097610();
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 8) {
        attack += attack / 5;
    }
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 2) {
        attack += attack / 10;
    }
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 4) {
        attack -= attack / 5;
    }
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 1) {
        attack -= attack / 10;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 4) {
        defense += defense / 5;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 1) {
        defense += defense / 10;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 8) {
        defense -= defense / 5;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 2) {
        defense -= defense / 10;
    }
    if (D_800C3DFC->attributes[2] & 0x10) {
        if (!(D_800C3E34->pilot.status82 & 0x40)) {
            D_800C3E34->pilot.status80 |= 0x40;
        }
        if ((D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) & 0x4000) {
            D_800D2C88[D_800C3E04] = 3;
            D_800D2C54[D_800C3E04] = (u16)(D_800C3E00->pilot.maxEp / 10) * 2;
        }
        if ((D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) & 0x1000) {
            D_800D2C88[D_800C3E04] = 2;
            D_800D2C54[D_800C3E04] = (u16)(D_800C3E00->pilot.maxHp / 10) * 2;
        }
    }
    if ((D_800C3DFC->attributes[2] & 0x20) && !(D_800C3E34->pilot.status82 & 0x80)) {
        D_800C3E34->pilot.status80 |= 0x80;
    }
    if (D_800C3E34->pilot.status80 & 0x40) {
        defense -= defense >> 2;
        D_800C3E34->pilot.status80 &= ~0x40;
    }
    if (D_800C3E00->pilot.status80 & 0x80) {
        attack -= attack >> 2;
        D_800C3E00->pilot.status80 &= ~0x80;
    }
    if (D_800C3DFC->flagsA & 0x400) {
        power = 20;
    }
    func_80096494(&attack, &defense, &hit);
    attackScale = 5;
    if (D_800C3DFC->flagsA & 0x100) {
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
    kind = D_800C3DFC->amountKind;
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
            D_800C34B0->resultCode[D_800C3E50] = 0;
        } else {
            amount = 1;
            D_800C34B0->resultCode[D_800C3E50] = 0;
        }
        break;
    case 2:
        amount /= 2;
        D_800C34B0->resultCode[D_800C3E50] = 5;
        break;
    case 3:
        amount = 0;
        D_800C34B0->resultCode[D_800C3E50] = 4;
        break;
    case 4:
        D_800C34B0->resultCode[D_800C3E50] = 2;
        break;
    case 5:
        D_800C34B0->resultCode[D_800C3E50] = 7;
        break;
    }
    if (D_800D2DC4 != 0 && (D_800C3DFC->flagsA & 0x100) && amount != 0) {
        amount /= 3;
    }
    if (amount >= 10000) {
        amount = 9999;
    }
    if (amount < 0) {
        amount = 0;
    }
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Formula: amount = attacker +0x5b times the descriptor's +0x11 (doubled
 * with attacker +0x8a bit 0x2000), scaled 0.7 / 1.3 by the target's
 * +0x8c|+0x8e bits 0x100 / 0x200, none for a +0x15a 0x80 target; code 2. */
void func_80095690(void) {
    s16 amount = D_800C3E00->pilot.accuracy * D_800C3DFC->power;
    u16 status;

    if (D_800C3E00->pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    status = D_800C3E34->pilot.status8C.half.active | D_800C3E34->pilot.status8C.half.permanent;
    if (status & 0x100) {
        amount = amount * 7 / 10;
    }
    if (status & 0x200) {
        amount = amount * 13 / 10;
    }
    if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
        amount = 0;
    }
    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* The target defends: a +0x56 state 2 target first leaves it (8009ac48),
 * its status words clear, result code 2 and a tenth of its +0x4e times the
 * descriptor's +0x11 as the amount; its timer is held (the held mask
 * 800d2c9e is addressed as its own variable here). */
void func_800957D8(void) {
    s32 *amount;
    u8 slot;

    if (D_800CCCE8.records[D_800C3E50].pilot.characterId == 2) {
        func_8009AC48(D_800C3E50, 1);
    }
    D_800C3E34->pilot.status7C = 0;
    D_800C3E34->pilot.status80 = 0;
    D_800C3E34->pilot.status84.half.active = 0;
    D_800C3E34->pilot.status88.half.active = 0;
    D_800C3E34->pilot.status8C.half.active = 0;
    D_800D2C88[D_800C3E50] = 2;
    slot = D_800C3E50;
    amount = &D_800D2C54[slot];
    *amount = (D_800C3E34->pilot.maxHp * D_800C3DFC->power) / 10;
    D_800D2C9E |= 1 << slot;
}

/* With the command's chance (+0x1c in percent) and kind 0x6e, clear the
 * target's status words named by the command's bits 0x8000-0x400 (message
 * 0x3a). */
void func_800958D8(void) {
    if (rand() % 100 <= D_800C3DFC->field1C && D_800C3DFC->field1D == 0x6E) {
        if (D_800C3DFC->field1E & 0x8000) {
            D_800C3E34->pilot.status84.half.active = 0;
        }
        if (D_800C3DFC->field1E & 0x4000) {
            D_800C3E34->pilot.status84.half.permanent = 0;
        }
        if (D_800C3DFC->field1E & 0x2000) {
            D_800C3E34->pilot.status88.half.active = 0;
        }
        if (D_800C3DFC->field1E & 0x1000) {
            D_800C3E34->pilot.status88.half.permanent = 0;
        }
        if (D_800C3DFC->field1E & 0x800) {
            D_800C3E34->pilot.status8C.half.active = 0;
        }
        if (D_800C3DFC->field1E & 0x400) {
            D_800C3E34->pilot.status8C.half.permanent = 0;
        }
        D_800C34B0->message = 0x3A;
    }
}

/* Status effect of the descriptor on the target (mode +0x11, or 5 with
 * +0xa bit 0x4000); a refused status marks the target's result 6. */
void func_80095A78(void) {
    s8 accepted = func_80097964(D_800C3DFC->field1C, D_800C3DFC->field1D, D_800C3DFC->field1E);

    if (D_800C3DFC->flagsA & 0x4000) {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, 5);
    } else {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, D_800C3DFC->power);
        if (accepted != 1) {
            D_800C34B0->resultCode[D_800C3E50] = 6;
        }
    }
}

/* When 80097964 accepts the attacker's +2/+3/+0 values, run 800995a0 on the
 * target with the descriptor's +0x1d/+0x1e and mode 5. */
void func_80095B44(void) {
    if (func_80097964(D_800C3E00->pilot.entries[0].value2, D_800C3E00->pilot.entries[0].value3, D_800C3E00->pilot.entries[0].field0) == 1) {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, 5);
    }
}

/* With the command's chance (+0x1c in percent) clear the target's statuses
 * named by the command's bits (+0x1d: 0x80 the state word except KO/down,
 * 0x40 the timer holds and 0x20 of 7a, 0x20-0x08 the active status words);
 * otherwise it misses (result 6). */
void func_80095BAC(void) {
    if (D_800C3DFC->field1C < rand() % 100) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
        return;
    }
    if (D_800C3DFC->field1D & 0x80) {
        D_800C3E34->pilot.status7C &= 0xC000;
    }
    if (D_800C3DFC->field1D & 0x40) {
        D_800C3E34->pilot.status80 = 0;
        D_800C3E34->pilot.status7A &= ~0x20;
    }
    if (D_800C3DFC->field1D & 0x20) {
        D_800C3E34->pilot.status84.half.active = 0;
    }
    if (D_800C3DFC->field1D & 0x10) {
        D_800C3E34->pilot.status88.half.active = 0;
    }
    if (D_800C3DFC->field1D & 8) {
        D_800C3E34->pilot.status8C.half.active = 0;
    }
}

/* Formula type 3: on a chance roll (the attacker's +0x60 or the command's
 * +0x1c), transfer power / 20 of a maximum (attacker's or target's HP for
 * kinds 0-1, EP for 2-3; kind 5 the target's HP less one) between attacker
 * and target: both slots get the amount, HP kinds with results 2 / 0, EP
 * kinds 3 / 1 unless the target nullifies (status88 0x200). A failed roll
 * or nullified transfer is result 6. */
void func_80095D4C(void) {
    u8 chance;
    u16 base;
    u16 amount;

    switch (D_800C3DFC->chanceSource) {
    case 0:
        chance = D_800C3E00->pilot.field60;
        break;
    case 1:
        chance = D_800C3DFC->field1C;
        break;
    }
    if (chance < rand() % 100) {
        goto missed;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
        base = D_800C3E00->pilot.maxHp;
        break;
    case 1:
        base = D_800C3E34->pilot.maxHp;
        break;
    case 2:
        base = D_800C3E00->pilot.maxEp;
        break;
    case 3:
        base = D_800C3E34->pilot.maxEp;
        break;
    }
    amount = base * D_800C3DFC->power / 20;
    if (D_800C3DFC->amountKind == 5) {
        amount = D_800C3E34->pilot.hp - 1;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
    case 1:
    case 5:
        D_800C34B0->resultCode[D_800C3E04] = 2;
        D_800C34B0->resultCode[D_800C3E50] = 0;
        D_800C34B0->damage[D_800C3E04] = amount;
        D_800C34B0->damage[D_800C3E50] = amount;
        break;
    case 2:
    case 3:
        if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 0x200) {
        missed:
            D_800C34B0->resultCode[D_800C3E50] = 6;
        } else {
            D_800C34B0->resultCode[D_800C3E04] = 3;
            D_800C34B0->resultCode[D_800C3E50] = 1;
            D_800C34B0->damage[D_800C3E04] = amount;
            D_800C34B0->damage[D_800C3E50] = amount;
        }
        break;
    }
}

/* Chance roll (attacker +0x60 or the descriptor's +0x1c, by +0x18), then the
 * amount by the descriptor's kind +0x1a: target HP / power, HP - 1, the
 * attacker's missing HP, EP * 10, 1, HP, the maximum HP (capped at 9999)
 * or the target's down state (gears refuse it). */
void func_80096018(void) {
    u8 chance;

    switch (D_800C3DFC->chanceSource) {
    case 0:
        chance = D_800C3E00->pilot.field60;
        break;
    case 1:
        chance = D_800C3DFC->field1C;
        break;
    }
    if (chance < rand() % 100) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
        return;
    }
    D_800C34B0->resultCode[D_800C3E50] = 0;
    switch (D_800C3DFC->amountKind) {
    case 0:
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp / D_800C3DFC->power;
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.hp / D_800C3DFC->power;
        }
        break;
    case 1:
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp - 1;
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.hp - 1;
        }
        break;
    case 2:
        if (D_800C34B0->records[D_800C3E04].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2D6C->maxHp - D_800D2D6C->hp;
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E00->pilot.maxHp - D_800C3E00->pilot.hp;
        }
        break;
    case 3:
        D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.ep * 10;
        break;
    case 4:
        D_800C34B0->damage[D_800C3E50] = 1;
        break;
    case 5:
        D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.hp;
        break;
    case 6:
        D_800C34B0->resultCode[D_800C3E50] = 0;
        if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2DC8->maxHp;
            if (D_800D2DC8->maxHp >= 10000) {
                D_800D2DC8->maxHp = 9999;
            }
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.maxHp;
            if (D_800C3E34->pilot.maxHp >= 10000) {
                D_800C3E34->pilot.maxHp = 9999;
            }
        }
        break;
    case 7:
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->resultCode[D_800C3E50] = 6;
            return;
        }
        D_800C3E34->pilot.status7C |= 1;
        D_800C3E34->pilot.status80 |= 1;
        break;
    }
}

#ifdef NON_MATCHING
/* Element adjustment of an attack/defense pair: the command's element (or
 * the attacker's own element status) against the target's weakness, its
 * resistance statuses (which may also force hit result 4) and its single
 * element guards, then a 20% boost for either side's +0x32 bit 0x10. The
 * guard switch tests the target's guard bits shifted back into place
 * (status = bits << 8, computed once). defenseScale is a signed char (the
 * defense division keeps its sign correction).
 * Nonmatching: element, bits and flag are allocated $t0/$t1/$t2 here and
 * $t1/$t2/$t0 in the original; the original tests element == 0 with an
 * andi but forms element & own without one. */
void func_80096494(u16 *attack, u16 *defense, s8 *hit) {
    u8 ether = 0;
    u8 flag = 0;
    u8 element;
    u8 bits;
    u8 targetGear;
    u16 own;
    u16 status;
    s8 scale;
    s8 defenseScale;

    targetGear = D_800C34B0->records[D_800C3E50].flags15A >> 7;
    element = D_800C3DFC->attributes[2] & 0x3F;
    bits = D_800C3E34->pilot.weakness & 0x3F;
    if (!(D_800C34B0->records[D_800C3E04].flags15A >> 7)) {
        own = D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent;
    } else {
        own = D_800D2D6C->status84.half.active | D_800D2D6C->status84.half.permanent;
    }
    own >>= 12;
    if (D_800C3DFC->flagsA & 0x100) {
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
        if (D_800C3E34->pilot.weakness & 0x40) {
            scale = 18;
        }
        if (flag) {
            scale += 2;
        }
    }
    flag = 0;
    if (!targetGear) {
        status = D_800C3E34->pilot.status8C.half.active | D_800C3E34->pilot.status8C.half.permanent;
        bits = (status & 0xF00) >> 8;
        flag = (status >> 1) & 1;
    } else {
        status = D_800D2DC8->status84.half.active | D_800D2DC8->status84.half.permanent;
        bits = (status & 0xF00) >> 8;
        if (status & 2) {
            flag = 1;
        }
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
    if ((element & bits) && flag) {
        scale -= 3;
        if (status & 4) {
            scale -= 3;
        }
        if (status & 8) {
            *hit = 4;
        }
    }
    status = bits << 8;
    switch (element) {
    case 1:
        if (status & 0x200) {
            scale += 3;
        }
        break;
    case 2:
        if (status & 0x100) {
            scale += 3;
        }
        break;
    case 4:
        if (status & 0x800) {
            scale += 3;
        }
        break;
    case 8:
        if (status & 0x400) {
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
    if (D_800C3E00->pilot.flags32 & 0x10) {
        *attack += *attack / 5U;
    }
    if (D_800C3E34->pilot.flags32 & 0x10) {
        *defense += *defense / 5U;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8008CCCC", func_80096494);
#endif

/* Ether check: unless rand % 100 falls below the attacker's +0x5b plus the
 * descriptor's +0x14, the action fails (code 0x38 at +0x5fc7). */
void func_80096824(void) {
    s32 chance = D_800C3E00->pilot.accuracy + D_800C3DFC->accuracy;

    if (rand() % 100 >= chance) {
        D_800C34B0->message = 0x38;
        D_800D2DC4 = 1;
    }
}

/* Counterattack: a target able to act (not down, turn timer free, an enemy
 * or one holding status 0x2000, with the counter status 0x1000 of +0x88)
 * answers a non-ether, non-gear attack with a 60% (character 0) or 50%
 * chance: attacker and target swap and the target's command 20 runs with
 * result 7 (message 0x34). */
void func_800968C0(void) {
    s32 chance;
    u8 slot;
    Combatant *record;

    if (D_800C3E34->pilot.status7C & 0xA000) {
        return;
    }
    if (D_800C3E34->pilot.status80 & 0x1000) {
        return;
    }
    if (D_800C3E04 < 3 && !(D_800C3E34->pilot.status80 & 0x2000)) {
        return;
    }
    if (!(D_800C3E34->pilot.status88.half.active & 0x1000)) {
        return;
    }
    if (D_800C3DFC->flagsA & 0x2000) {
        return;
    }
    if (D_800C3DFC->flagsA & 0x100) {
        return;
    }
    if (D_800C34B0->records[D_800C3E04].flags15A & 0x80) {
        return;
    }
    chance = 50;
    if (D_800C3E34->pilot.characterId == 0) {
        chance = 60;
    }
    if (rand() % 100 <= chance && D_800C3DFC->formula != 2) {
        slot = D_800C3E04;
        D_800C3E04 = D_800C3E50;
        record = D_800C3E00;
        D_800C3E00 = D_800C3E34;
        D_800C3E50 = slot;
        D_800C3E34 = record;
        D_800C3DFC = &D_800C34B0->partyCommands[D_800C3E04][20];
        D_800C34B0->resultCode[D_800C3E04] = 7;
        D_800C34B0->damage[D_800C3E04] = 0;
        D_800C34B0->message = 0x34;
    }
}

/* Hit outcome of the current command on the target: 1 hit, 2 half, 3 miss,
 * 5 forced. Gear-only and weapon checks, sure-hit and never-hit statuses
 * come first; then the attacker's +0x5e plus the command's +0x15 against
 * the target's +0x5f sets a margin for the percent rolls. */
s32 func_80096AB8(void) {
    s16 bonus = 0;
    s16 guard = 0;
    u8 accuracy = D_800C3E00->pilot.field5E;
    u8 evasion = D_800C3E34->pilot.field5F;
    u16 flags;
    u16 status;
    s16 margin;
    s16 roll;

    if ((D_800C3DFC->flagsA & 0x40) && (D_800CCCE8.records[D_800C3E50].flags15A & 0x80)) {
        return 3;
    }
    if (D_800C3E00->pilot.characterId == 4) {
        if ((D_800C3DFC->itemKinds & 0x80) && D_8006F8BA[D_800C3E00->pilot.entryItems[0]] == 0) {
            D_800C34AE = 1;
            return 3;
        }
        if ((D_800C3DFC->itemKinds & 0x10) && D_8006F8BA[D_800C3E00->pilot.entryItems[3]] == 0) {
            D_800C34AE = 1;
            return 3;
        }
    }
    if (D_800C3DFC->flagsA & 0x200) {
        if (D_800C3E34->pilot.flags34 & 8) {
            return 3;
        }
        if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
            return 3;
        }
    }
    flags = D_800C3DFC->flagsA;
    if (flags & 0x1000) {
        return 3;
    }
    status = D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent;
    if (status & 0x100) {
        return 3;
    }
    if (D_800C3E34->pilot.status7C & 0x2000) {
        return 1;
    }
    if (D_800C3E34->pilot.status80 & 0x1000) {
        return 1;
    }
    if (flags & 0x8000) {
        return 1;
    }
    if (flags & 2) {
        return 5;
    }
    if (D_800C3E00->pilot.status7C & 0x400) {
        bonus -= 50;
    }
    if ((D_800C3E00->pilot.status84.half.active | D_800C3E00->pilot.status84.half.permanent) & 0x1000) {
        bonus += 30;
    }
    if (status & 0x800) {
        guard += 50;
    }
    margin = D_800C3DFC->hitBonus + accuracy - evasion;
    if (D_800C34B0->records[D_800C3E50].flags15A & 1) {
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
    if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x40) {
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

/* Attack value of the current command: the item/part values its +0x10 bits
 * select plus the attack base (gear attack times scale, or the character's
 * +0x58, both scaled by statuses), or the ether value; kind 2 scales either
 * by the power over 20. Party attackers then get their character bonuses
 * and the target's weakness bonuses. */
s16 func_80096FBC(void) {
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

    if (D_800C34B0->records[D_800C3E04].flags15A & 0x80) {
        for (i = 0; i < 3; i++) {
            parts[i] = D_800D2D6C->entries[i].valueE;
        }
        base = D_800D2D6C->attack * D_800D2D6C->attackScale;
        if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x1000) {
            base += D_800D2D6C->attack * 2;
        }
    } else {
        for (i = 0; i < 4; i++) {
            parts[i] = D_800C3E00->pilot.entries[i].value4;
        }
        base = D_800C3E00->pilot.attack;
    }
    kinds = D_800C3DFC->itemKinds;
    ether = D_800C3E00->pilot.accuracy;
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
    if ((D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) & 1) {
        sum += sum >> 1;
    }
    if (D_800C3DFC->flagsA & 0x100) {
        scale = 4;
        if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 0x8000) {
            scale = 5;
        }
        if (D_800C3E00->pilot.status80 & 0x400) {
            scale--;
        }
        ether = ether * scale / 4;
        if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 0x2000) {
            ether *= 2;
        }
    } else if (!(D_800C34B0->records[D_800C3E04].flags15A & 0x80)) {
        status = D_800C3E00->pilot.status84.half.active | D_800C3E00->pilot.status84.half.permanent;
        scale = 4;
        if (status & 0x2000) {
            scale = 5;
        }
        if (D_800C3E00->pilot.status7C & 0x200) {
            scale--;
        }
        if (status & 0x400) {
            scale += 10 - D_800C3E00->pilot.hp / (u16)(D_800C3E00->pilot.maxHp / 10);
        }
        base = base * scale / 4;
    }
    flags = D_800C3DFC->flagsA;
    if (flags & 0x20) {
        base = 0;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
        value = sum + base;
        break;
    case 1:
        value = ether;
        break;
    case 2:
        if (D_800C3E00->pilot.characterId == 4) {
            sum = sum * 6 / 10;
        }
        if (flags & 0x100) {
            value = ether * D_800C3DFC->power / 20;
        } else {
            value = (sum + base) * D_800C3DFC->power / 20;
        }
        break;
    }
    if (D_800C3E04 < 3) {
        if (D_800C3E00->pilot.characterId == 7) {
            func_8009B46C(&value);
        }
        if (D_800C3E00->pilot.characterId == 4 || (D_800C3DFC->attributes[2] & 0x20)) {
            if (D_800C3E34->pilot.weakness & 0x20) {
                value += value >> 2;
            }
            if ((*(u32 *)&D_800C3E34->pilot.weakness & 0x60) == 0x60) {
                value += value >> 2;
            }
        }
        if (D_800C3E00->pilot.characterId == 8 && !(D_800CCCE8.records[D_800C3E04].flags15A & 0x80) &&
            (D_800C3DFC->flagsA & 0x100)) {
            value = value * D_800CCCE8.records[D_800C3E04].gear.frameFactor / 4;
        }
        if (D_800C3E00->pilot.characterId == 10) {
            value += value / 5;
        }
        if ((D_800C3DFC->attributes[2] & 0x10) && (D_800C3E34->pilot.weakness & 0x10)) {
            value += value >> 2;
            if (D_800C3E34->pilot.weakness & 0x10) {
                value += value >> 2;
            }
        }
    }
    return value;
}

/* Defense value of the target against the current command: body defense
 * plus armor (1.5x with status 0x100), the ether defense (1.5x with status
 * 0x4000 of +0x88) or the body alone, by the command's +0x1b. A character
 * target's +0x32 bits then scale it by the party members down. */
s16 func_80097610(void) {
    u16 armor;
    u16 body;
    u16 ether;
    u16 value;
    u8 downed;
    u8 i;

    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        body = D_800D2DC8->bodyDefense;
    } else {
        armor = D_800C3E34->pilot.defense;
        body = D_800C3E34->pilot.bodyDefense;
    }
    ether = D_800C3E34->pilot.etherDefense;
    if (D_800C3DFC->flagsA & 0x100) {
        if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 0x4000) {
            ether = ether * 3 / 2;
        }
    } else if (!(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x100) {
            armor = armor * 3 / 2;
        }
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        armor = 0;
    }
    switch (D_800C3DFC->defenseKind) {
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
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        return value;
    }
    downed = 0;
    for (i = 0; i < 3; i++) {
        if (D_800C34B0->records[i].pilot.status7C & 0x8000) {
            downed++;
        }
    }
    if ((D_800C3E34->pilot.flags32 & 4) && downed != 0 && !(D_800C3DFC->flagsA & 0x100)) {
        value = value * (4 - downed) / 4;
    }
    if ((D_800C3E34->pilot.flags32 & 2) && downed != 0 && !(D_800C3DFC->flagsA & 0x100)) {
        value = value * (downed + 2) / 2;
    }
    if ((D_800C3E34->pilot.flags32 & 1) && downed != 0) {
        value = value * (downed + 2) / 2;
    }
    return value;
}

void func_8009795C(void) {
}

/* Roll a status onto the target (never a gear): with the chance in percent,
 * check the kind's immunities and clear the statuses it overrides, then set
 * the flag bits in the kind's status word and show its message. Returns 1
 * when the status took (or cancelled its opposite). */
s8 func_80097964(u8 chance, u8 kind, u16 flags) {
    Combatant *statusRecord;

    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        return 0;
    }
    if (chance < rand() % 100) {
        return 0;
    }
    switch (kind) {
    case 0: {
        u16 immuneFlags = flags & 0xFFFD;
        if (immuneFlags & D_800C3E34->pilot.status7E) {
            return 0;
        }
        if (flags & 0x1000) {
            D_800C3E34->pilot.status84.half.active &= 0x7FFF;
        }
        break;
    }
    case 2:
        if (flags & D_800C3E34->pilot.status82) {
            return 0;
        }
        break;
    case 5:
        if ((D_800C3E34->pilot.status7C | D_800C3E34->pilot.status7E) & 1) {
            return 0;
        }
        if (flags & 0x8000) {
            D_800C3E34->pilot.status7C &= 0xEFFF;
        }
        break;
    case 7:
        if ((D_800C3E34->pilot.status80 | D_800C3E34->pilot.status82) & 1) {
            return 0;
        }
        if (flags & 0xA) {
            if (!(D_800C3E34->pilot.status88.half.active & 5)) {
                break;
            }
            D_800C3E34->pilot.status88.half.active &= 0xFFFA;
            return 1;
        }
        if (flags & 5) {
            if (!(D_800C3E34->pilot.status88.half.active & 0xA)) {
                break;
            }
            D_800C3E34->pilot.status88.half.active &= 0xFFF5;
            return 1;
        }
    case 9:
        if (flags & 0xF000) {
            if (D_800C3E34->pilot.status8C.half.permanent & 0xF000) {
                D_800C34B0->message = 0x39;
                return 0;
            }
            D_800C3E34->pilot.status8C.half.active &= 0xFFF;
        }
        if (flags & 0xF00) {
            if (D_800C3E34->pilot.status8C.half.permanent & 0xF00) {
                D_800C34B0->message = 0x39;
                return 0;
            }
            D_800C3E34->pilot.status8C.half.active &= 0xF0FF;
        }
        break;
    }
    switch (kind) {
    case 0:
        D_800C3E34->pilot.status7C = (flags | D_800C3E34->pilot.status7C) & 0xFFFD;
        if (flags & 2) {
            if ((u8)func_80099498()) {
                D_800C3E34->pilot.status7C |= flags;
                D_800C34B0->message = 0x31;
                statusRecord = D_800C3E34;
                statusRecord->pilot.status7A = 0xFFEF;
            } else {
                D_800C34B0->message = 0x32;
            }
        }
        break;
    case 2:
        D_800C3E34->pilot.status80 |= flags;
        if (flags & 0x800) {
            statusRecord = D_800C3E34;
            statusRecord->pilot.status7A |= 0x20;
        }
        break;
    case 5:
    case 7:
    case 9:
        /* Status halfwords from +0x7A: kinds 5/7/9 select +0x84/88/8C. */
        statusRecord = D_800C3E34;
        statusRecord = (Combatant *)((u8 *)statusRecord + kind * 2);
        statusRecord->pilot.status7A |= flags;
        break;
    }
    func_8009B684(kind, flags);
    return 1;
}

/* Clear the per-slot damage and result codes (12 entries). */
void func_80097D08(void) {
    s16 slot = 11;

    do {
        D_800C34B0->resultCode[slot] = 0xFF;
        D_800C34B0->damage[slot] = 0;
    } while (--slot != -1);
}

/* Derive each present party member's battle stats: keep the base values,
 * add the equipment bonuses and percentage HP/EP bonuses (capped at 999 and
 * 99), total the gear's equipment, set its fuel cost (command 37's shown
 * value), the attack level limit and scale, the status-driven command rate
 * changes and the character-specific adjustments; then count the members. */
void func_80097D5C(void) {
    u8 member;
    u8 i;
    u8 *level;
    u8 rate;
    u16 immunities;
    CharacterRecord *pilot;
    Combatant *record;
    GearRecord *gear;

    for (member = 0; member < 3; member++) {
        if (D_800D2D24[member] == 0x7F) {
            continue;
        }
        D_800C3E00 = &D_800C34B0->records[member];
        D_800D2D6C = &D_800C34B0->records[member].gear;
        level = &D_800C34B0->records[member].field148;
        if (D_800D2D24[member] == 9) {
            D_8006D8A0.characters[9].characterId = 9;
            D_800C3E00->pilot.characterId = 9;
        }
        if (D_800D2D24[member] == 10) {
            D_8006D8A0.characters[10].characterId = 10;
            D_800C3E00->pilot.characterId = 10;
        }
        if (D_800C3E00->pilot.characterId == 4) {
            D_800C34B0->savedStats[member][0] = D_800C3E00->pilot.entries[0].value4 + D_800C3E00->pilot.entries[3].value4;
        } else {
            D_800C34B0->savedStats[member][0] = D_800C3E00->pilot.attack + D_800C3E00->pilot.entries[0].value4;
        }
        D_800C34B0->savedStats[member][1] = D_800C3E00->pilot.field5E;
        D_800C34B0->savedStats[member][2] = D_800C3E00->pilot.defense + D_800C3E00->pilot.bodyDefense;
        D_800C34B0->savedStats[member][3] = D_800C3E00->pilot.field5F;
        D_800C34B0->savedStats[member][4] = D_800C3E00->pilot.accuracy;
        D_800C34B0->savedStats[member][5] = D_800C3E00->pilot.etherDefense;
        D_800C34B0->savedStats[member][6] = D_800C3E00->pilot.speed;
        pilot = &D_800C3E00->pilot;
        D_800C34B0->expTotals[member][0] = pilot->expTotalA;
        D_800C34B0->expTotals[member][1] = pilot->expTotalB;
        D_800C34B0->savedMax[member][0] = pilot->maxHp;
        D_800C34B0->savedMax[member][1] = pilot->maxEp;
        pilot->attack += pilot->equipAttack;
        pilot->flags34 = 0;
        D_800C3E00->pilot.defense += D_800C3E00->pilot.equipDefense;
        D_800C3E00->pilot.speed += D_800C3E00->pilot.equipSpeed;
        D_800C3E00->pilot.accuracy += D_800C3E00->pilot.equipAccuracy;
        D_800C3E00->pilot.etherDefense += D_800C3E00->pilot.equipEtherDefense;
        D_800C3E00->pilot.field5E += D_800C3E00->pilot.equip5E;
        D_800C3E00->pilot.field5F += D_800C3E00->pilot.equip5F;
        if (D_800C3E00->pilot.speed > 16) {
            D_800C3E00->pilot.speed = 16;
        }
        if (D_800C3E00->pilot.flags32 & 0x100) {
            D_800C3E00->pilot.field5E += D_800C3E00->pilot.field5E >> 2;
            D_800C3E00->pilot.field5F += D_800C3E00->pilot.field5F >> 2;
        }
        D_800C3E00->pilot.hp += D_800C3E00->pilot.maxHp * D_800C3E00->pilot.hpBonus / 20;
        D_800C3E00->pilot.ep += D_800C3E00->pilot.maxEp * D_800C3E00->pilot.epBonus / 20;
        D_800C3E00->pilot.maxHp += D_800C3E00->pilot.maxHp * D_800C3E00->pilot.hpBonus / 20;
        D_800C3E00->pilot.maxEp += D_800C3E00->pilot.maxEp * D_800C3E00->pilot.epBonus / 20;
        if (D_800C3E00->pilot.hp >= 1000) {
            D_800C3E00->pilot.hp = 999;
        }
        if (D_800C3E00->pilot.ep >= 100) {
            D_800C3E00->pilot.ep = 99;
        }
        if (D_800C3E00->pilot.maxHp >= 1000) {
            D_800C3E00->pilot.maxHp = 999;
        }
        if (D_800C3E00->pilot.maxEp >= 100) {
            D_800C3E00->pilot.maxEp = 99;
        }
        record = &D_800C34B0->records[member];
        record->expWeightA = 5;
        record->expWeightB = 5;
        level[0] = 0;
        gear = D_800D2D6C;
        gear->bodyDefense += gear->equipBodyDefense;
        gear->armor += gear->equipArmor;
        gear->field68 += gear->equip68a + gear->equip68b;
        gear->guard += gear->equipGuard;
        D_800D2D6C->hitBonus += D_800D2D6C->equipHitBonus;
        D_800D2D6C->speed += D_800D2D6C->equipSpeed - D_800D2D6C->speedPenalty;
        D_800D2D6C->frameFactor += D_800D2D6C->equipFrameFactor;
        rate = D_800D2D6C->field4F;
        if (rate != 0 && D_800C3E00->pilot.gearId != 0x12) {
            D_800C34B0->gearCommands[member][37].hudState = D_800D2D6C->maxHp / 10 * rate * 2 / 9;
            D_800C34B0->gearCommands[member][37].hudState /= 10;
            D_800C34B0->gearCommands[member][37].hudState *= 10;
        }
        level[1] = 0;
        if (D_8006ECF4[D_800C3E00->pilot.characterId].mask4 & 0x1C00) {
            level[1] = 1;
        }
        if (D_8006ECF4[D_800C3E00->pilot.characterId].mask4 & 0x380) {
            level[1] = 2;
        }
        if (D_8006ECF4[D_800C3E00->pilot.characterId].mask4 & 0x70) {
            level[1] = 3;
        }
        D_800D2D6C->attackScale = D_800D2D6C->field74;
        D_800D2D6C->field3E = D_800D2D6C->field74;
        D_800D2D6C->attackScale += D_800D2D6C->equipAttackScale;
        D_800D2D6C->field3E += D_800D2D6C->equipAttackScale;
        if (D_800C3E00->pilot.status88.half.permanent & 0x2000) {
            for (i = 0; i < 38; i++) {
                D_800C34B0->partyCommands[member][i].cost *= 2;
            }
            for (i = 0; i < 42; i++) {
                D_800C34B0->gearCommands[member][i].cost *= 2;
            }
        }
        if (D_800C3E00->pilot.flags32 & 0x4000) {
            for (i = 0; i < 38; i++) {
                D_800C34B0->partyCommands[member][i].cost = (D_800C34B0->partyCommands[member][i].cost + 1) >> 1;
            }
            for (i = 0; i < 42; i++) {
                D_800C34B0->gearCommands[member][i].cost = (D_800C34B0->gearCommands[member][i].cost + 1) >> 1;
            }
        }
        if (D_800C3E00->pilot.field62 >= 50) {
            D_8006ECF4[D_800C3E00->pilot.characterId].mask4 |= 8;
        }
        if (D_800C3E00->pilot.characterId == 7) {
            D_800D2D6C->hp = D_800C3E00->pilot.hp * 50;
            D_800D2D6C->maxHp = D_800C3E00->pilot.maxHp * 50;
            D_800D2D6C->attack = D_800C3E00->pilot.attack;
            D_800D2D6C->bodyDefense = D_800C3E00->pilot.defense * 12;
            D_800D2D6C->armor = D_800C3E00->pilot.etherDefense * 6;
            D_800D2D6C->speed = D_800C3E00->pilot.speed;
            immunities = D_800D2D6C->field7E;
            D_800D2D6C->field7E = immunities | 0x3C4;
            if (D_800C3E00->pilot.status82 & 0x2000) {
                D_800D2D6C->field7E = immunities | 0x13C4;
            }
        }
        if (D_800C3E00->pilot.gearId == 0xF) {
            D_8006ECF4[D_800C3E00->pilot.characterId].flags1A &= 0x8FFF;
        }
        if (D_800C3E00->pilot.gearId == 0x12) {
            D_800C3E00->pilot.status7A = 0x238;
            D_800D2D6C->fuel = 0x26AC;
            D_800D2D6C->maxFuel = 0x26AC;
            level[1] = 0;
            D_8006ECF4[3].flags1A = 0x8000;
        }
        if (D_8006D634.value1930 >= 231 && !(D_8006D634.flags2355 & 0x80)) {
            D_8006D634.characters[9].field6A = 0x27;
            D_8006D634.characters[9].entries[0].value4 = 0x1E;
            D_8006D634.flags2355 |= 0x80;
        }
    }
    D_800C34AD = 3;
    for (member = 0; member < 3; member++) {
        if (D_800D2D24[member] == 0x7F) {
            D_800C34AD--;
        }
    }
    for (member = 0; member < 3; member++) {
        D_800C34B0->field5F54[member] = 0;
        D_800C34B0->field5F60[member] = 0;
    }
}

/* Party adjustments at battle start: keep each present member's status
 * word 7A, replace the listed part speeds of its gear by the parts' own
 * (at most 16), set character 8's speed and battle flag, and before game
 * data word 0x1930 reaches 0xbb set the early gears' values. */
void func_8009892C(void) {
    u8 member;
    u8 i;
    Combatant *record;

    for (member = 0; member < 3; member++) {
        if (D_800D2D24[member] == 0x7F) {
            continue;
        }
        record = &D_800C34B0->records[member];
        D_800C3E00 = record;
        D_800D2D6C = &record->gear;
        D_800C3AA4[member] = record->pilot.status7A;
        for (i = 0; i < 4; i++) {
            if (D_800D2D10[i] != 0) {
                D_800D2D6C->speed -= D_800D2D10[i];
                D_800D2D6C->speed += D_800D2D6C->speedBonus[i];
            }
        }
        if (D_800D2D6C->speed > 16) {
            D_800D2D6C->speed = 16;
        }
    }
    D_8006D8A0.characters[8].speed = 7;
    if (D_8006ECF4[8].flags1A & 0x2000) {
        D_8006ECF4[8].mask2 |= 0x800;
    }
    if (D_8006D634.value1930 < 0xBB) {
        D_8006D8A0.gears[0].field74 = 10;
        D_8006D8A0.gears[1].field74 = 10;
        D_8006D8A0.gears[11].field74 = 9;
        D_8006D8A0.gears[12].field74 = 9;
        D_8006D8A0.gears[13].field74 = 8;
        D_8006D8A0.gears[14].field74 = 12;
        D_8006D8A0.gears[15].field74 = 12;
        D_8006D8A0.gears[13].field2 = 0x58;
        D_8006D8A0.gears[7].field3 = 0;
        D_8006D8A0.gears[15].field8 = 0x28;
    }
}

/* A slot's turn timer from its speed (party: less the current command's
 * weight, gear speed for a slot in gear), capped, with a random -3..4
 * spread. Sets the attacker globals and, for the party, the command
 * descriptor. Defined old-style: callers pass the slot unnarrowed. */
s32 func_80098AF8(slot, mode)
u8 slot;
s32 mode;
{
    Combatant *record = &D_800C34B0->records[slot];
    u16 speed;

    D_800D2D6C = &record->gear;
    D_800C3E00 = record;
    if (slot < 3) {
        if (!(record->flags15A & 0x80)) {
            D_800C3DFC = &D_800C34B0->partyCommands[slot][D_800C34B0->commandIndex];
            if (record->pilot.speed > D_800C3DFC->weight) {
                speed = (record->pilot.speed - D_800C3DFC->weight) * 9;
            } else {
                speed = 9;
            }
        } else {
            D_800C3DFC = &D_800C34B0->gearCommands[slot][D_800C34B0->commandIndex];
            if (record->gear.speed > D_800C3DFC->weight) {
                speed = (record->gear.speed - D_800C3DFC->weight) * 9;
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
    func_80094D24();
    return (u8)speed;
}

/* Resolve an item/effect `param` on every slot in the +0x5fac mask, then
 * set its animation from the effect table. */
void func_80098C6C(u16 param) {
    s32 slot;
    s32 bit;

    func_80097D08();
    bit = 1;
    for (slot = 0; (u8)slot < 11; slot++) {
        if (bit & D_800C34B0->targetMask) {
            func_80098D2C(slot, param);
        }
        bit <<= 1;
    }
    D_800C34B0->shownCommand = D_800D2200[param & 0xFF].animation;
    if (D_800D2C94.held & 0x8000) {
        D_800D2C94.animation = 0xC2;
    }
}

/* Apply item effect `param` to `slot`: HP (amount x 50, doubled with status
 * 0x80 of +0x86) and EP (amount x 10) restoration, and unless either ran,
 * the status cures, revival (amount tenths of the maximum HP), status
 * grants, immunities and the special effects of flags value 1. The item
 * effect table lies inside the battle work area (+0x5518, D_800D2200). */
void func_80098D2C(u8 slot, u8 param) {
    u8 restored;
    Combatant *record;
    ItemEffect *effect;
    GearRecord *gear;
    u8 i;
    s32 hp;
    s32 hpUnit;
    s32 epUnit;

    restored = 0;
    record = &D_800CCCE8.records[slot];
    effect = &((ItemEffect *)&D_800CCCE8.lists)[param + 2];
    gear = &D_800CCCE8.records[slot].gear;
    epUnit = 10;

    if (effect->flags & 0x8000) {
        hpUnit = 50;
        hp = effect->amount * hpUnit;
        D_800D2C88[slot] = 2;
        D_800CCCE8.damage[slot] = hp;
        if (record->pilot.status84.half.permanent & 0x80) {
            D_800C34B0->damage[slot] *= 2;
        }
        if (record->pilot.flags36 & 0x8000) {
            D_800D2C88[slot] = 0;
        }
        restored = 1;
    }
    if (effect->flags & 0x4000) {
        D_800D2C54[slot] = epUnit * effect->amount;
        D_800D2C88[slot] = 3;
        restored++;
    }
    if (restored) {
        return;
    }
    if ((D_800CCCE8.records[slot].flags15A & 0x80) && effect->flags != 1) {
        D_800D2C94.message = 0x30;
        D_800D2C94.held |= 0x8000;
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
        if (D_800CCCE8.records[slot].pilot.characterId == 2) {
            func_8009AC48(slot, 0);
        }
        record->pilot.status7C = 0;
        record->pilot.status80 = 0;
        record->pilot.status84.half.active = 0;
        record->pilot.status88.half.active = 0;
        record->pilot.status8C.half.active = 0;
        record->pilot.hp = record->pilot.maxHp * effect->amount / 10;
        D_800C34B0->revived |= 1 << slot;
    }
    if (effect->flags & 0x800) {
        record->pilot.status84.half.active |= effect->status;
        func_800995A0(slot, 5, effect->status, 5);
        func_8009B684(5, effect->status);
    }
    if (effect->flags & 0x400) {
        record->pilot.status88.half.active |= effect->status;
        func_800995A0(slot, 7, effect->status, 5);
        func_8009B684(7, effect->status);
    }
    if (effect->flags & 0x200) {
        record->pilot.status8C.half.active |= effect->status;
        if ((effect->status & 0xF000) && !(record->pilot.status8C.half.permanent & 0xF000)) {
            record->pilot.status8C.half.active &= 0xFFF;
            record->pilot.status8C.half.active |= effect->status;
            func_800995A0(slot, 9, effect->status, 5);
            func_8009B684(9, effect->status);
        }
        if ((effect->status & 0xF00) && !(record->pilot.status8C.half.permanent & 0xF00)) {
            record->pilot.status8C.half.active &= 0xF0FF;
            record->pilot.status8C.half.active |= effect->status;
            func_800995A0(slot, 9, effect->status, 5);
            func_8009B684(9, effect->status);
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
            D_800C34B0->message = 0x35;
            break;
        case 2:
            D_800C34B0->message = 0x36;
            break;
        case 4:
            D_800C34B0->message = 0x37;
            break;
        }
    }
    if ((effect->flags & 0x80) && (effect->status & 2)) {
        if (slot >= 3) {
            D_800C34B0->message = 0x33;
            return;
        }
        if (effect->duration == 0) {
            if ((u8)func_80099498()) {
                record->pilot.status7C |= effect->status;
                D_800C34B0->message = 0x31;
                record->pilot.status7A = 0xFFEF;
            } else {
                D_800C34B0->message = 0x32;
            }
        } else {
            record->pilot.status7C &= ~effect->status;
            record->pilot.status7A = D_800C3AA4[slot];
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
                func_800995A0(slot, 0, effect->status, effect->duration);
                func_8009B684(0, effect->status);
            }
            break;
        case 12:
            if (!(record->pilot.status82 & effect->status)) {
                record->pilot.status80 |= effect->status;
                func_800995A0(slot, 2, effect->status, effect->duration);
                func_8009B684(2, effect->status);
            }
            break;
        case 13:
            gear->defense += effect->status;
            D_800C34B0->message = 0x28;
            break;
        case 14:
            D_8006D8A0.characters[record->pilot.characterId].expNextA = 1;
            D_8006D8A0.characters[record->pilot.characterId].expNextB = 1;
            break;
        case 15:
            for (i = 0; i < 7; i++) {
                record->pilot.useCounts[i] += 10;
            }
            break;
        }
    }
}

/* Party gate on the formation mode: 1 when mode 2 has no member with status
 * bits 0xC002, or mode 3 does not have exactly two; otherwise 0. */
s32 func_80099498(void) {
    s32 result = 0;
    u8 i;
    u8 count;

    switch (D_800C34AD) {
    case 2:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (D_800C34B0->records[i].pilot.status7C & 0xC002) {
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
            if (D_800C34B0->records[i].pilot.status7C & 0xC002) {
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

/* Store a timed status's duration in slot's timer table. The status is named by
 * its kind and flag bit; the attacker's flag 0x2000 doubles the amount, and for
 * kinds 5, 7 and 9 the target's status bits 0x5 double and 0xA halve it, while
 * the slot's flag 0x40 doubles it again. Unknown statuses are ignored. */
void func_800995A0(u8 slot, u8 kind, u16 flag, u8 amount) {
    Combatant *record;
    u8 index = 0xF;
    u16 state;

    if (D_800CCCE8.records[D_800C3E04].pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    record = &D_800CCCE8.records[slot];
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
        state = D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent;
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
    D_800CCCE8.records[slot].statusTimers[index] = amount;
}

/* Count down slot's timed statuses at the start of its turn, clearing each one
 * whose timer runs out; a slot in a gear counts down its gear's statuses
 * instead (80099CF0). Returns a mask of the statuses that ended. */
u16 func_80099890(u8 slot) {
    Combatant *record = &D_800CCCE8.records[slot];
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    volatile u8 *timers = D_800CCCE8.records[slot].statusTimers;
    u16 ended;

    if (D_800CCCE8.records[slot].flags15A & 0x80) {
        func_80099CF0(gear, record, timers);
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

/* Count down the timed statuses of a gear, clearing each one whose timer runs
 * out; the end of gear status 0x20 also ends the pilot's status 0x1000. */
void func_80099CF0(GearRecord *gear, Combatant *record, volatile u8 *timers) {
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

/* Make the current command descriptor the battle's current command: copy its
 * attribute bytes and index, and give a command without an element the
 * attacker's element statuses. */
void func_80099FB0(void) {
    u16 elements = (D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) >> 12;

    D_800C34B0->commandAttributes[0] = D_800C3DFC->attributes[0];
    D_800C34B0->commandAttributes[1] = D_800C3DFC->attributes[1];
    D_800C34B0->commandAttributes[2] = D_800C3DFC->attributes[2];
    D_800C34B0->commandAttributes[3] = D_800C3DFC->attributes[3];
    D_800C34B0->commandIndexCopy = D_800C34B0->commandIndex;
    if ((D_800C34B0->commandAttributes[2] & 0x3F) == 0) {
        D_800C34B0->commandAttributes[2] |= elements;
    }
}

/* Restrict the target mask to the allowed targets of its side: the low three
 * bits (party) and bits 3-10 (enemies). */
void func_8009A074(void) {
    if (D_800C34B0->targetMask & 7) {
        D_800C34B0->targetMask = D_800C34B0->targetMask2 & 7;
    }
    if (D_800C34B0->targetMask & 0x7F8) {
        D_800C34B0->targetMask = D_800C34B0->targetMask2 & 0x7F8;
    }
}

/* The condition shown for slot, by priority: 8, 1, 2 for status bits 0x4000,
 * 0x8000, 0x2000; 3 and 4 for the second word's 0x1000 and 0x2000; 5, 6, 3 for
 * 0x800, 0x1000, 2; 7 for status pair 0x84 bit 0x8000; 5 below an eighth of
 * the maximum HP; otherwise 0. */
s32 func_8009A0DC(u8 slot) {
    Combatant *record = &D_800C34B0->records[slot];

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

/* Mask of slot's timed conditions for display (0 once status bit 0x8000 is
 * set): 0x8000, 0x4000 and 0x2000 for three statuses, plus the element
 * statuses (pair 0x8C bits 8-15) shifted down by three. */
u16 func_8009A1AC(u8 slot) {
    Combatant *record = &D_800C34B0->records[slot];
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

/* Accuracy of member's command: the descriptor's accuracy plus the member's
 * bonus, capped at 100. */
u8 func_8009A258(u8 member, u8 command) {
    CommandDescriptor *descriptor = &D_800C34B0->partyCommands[member][command];
    u8 accuracy = descriptor->accuracy + (D_800C34B0->records + member)->pilot.accuracy;

    if (accuracy > 100) {
        accuracy = 100;
    }
    return accuracy;
}

/* Fill the gear HUD for member: its first commands' states, charge rate,
 * attack, defense, the chance of a boost (from the gear's damage, when the
 * boost flag of D_8006F8EA is on), warning bits and overheat; count down an
 * active boost (level 4) or, at level 3, try to start one. */
void func_8009A2D4(u8 member) {
    Combatant *record = &D_800CCCE8.records[member];
    GearRecord *gear = &D_800CCCE8.records[member].gear;
    u8 *level = &D_800CCCE8.records[member].field148;
    CommandDescriptor *commands = D_800CCCE8.gearCommands[member];
    GearHud *hud = &D_800CCCE8.gearHud;
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
    if (!(*(u16 *)D_8006F8EA & 0x4000)) {
        chance = 0;
    }
    if (record->pilot.field62 < 50) {
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
        if (--D_800CCCE8.records[member].statusTimers[6] == 0) {
            gear->status80 &= ~0x4000;
            *level = 0;
            record->pilot.field54 = 0;
        }
    } else {
        hud->level = *level;
        if (*level == 3 && (*(u16 *)D_8006F8EA & 0x4000) && rand() % 100 < chance) {
            gear->status80 |= 0x4000;
            D_800CCCE8.records[member].statusTimers[6] = 3;
            if (D_800CCCE8.records[member].pilot.flags32 & 0x40) {
                D_800CCCE8.records[member].statusTimers[6] = 6;
            }
            (*level)++;
        }
    }
}

/* Byte 5 of game-data unit record id. */
u8 func_8009A7B8(u8 id) {
    return D_8006D8A0.characters[id].entries[0].pad5;
}

/* Whether gear item index is one of character 4's four entries. */
s32 func_8009A7E4(u8 index) {
    BattleItem *item = &D_800C34B0->lists.items.members[index];

    if (D_8006D8A0.characters[4].entries[0].id == item->id
        || D_8006D8A0.characters[4].entries[1].id == item->id
        || D_8006D8A0.characters[4].entries[2].id == item->id) {
        return 1;
    }
    return D_8006D8A0.characters[4].entries[3].id == item->id;
}

/* Put battle item index into character 4's entry holding its id (entry k when
 * none does): copy its values, record the item slot and durability, and update
 * the battle copies of character 4. */
void func_8009A854(u8 index, u8 k) {
    BattleItem *item = &D_800C34B0->lists.items.list[index];
    u8 i;

    if (D_8006D8A0.characters[4].entries[0].id == item->id) {
        k = 0;
    }
    if (D_8006D8A0.characters[4].entries[1].id == item->id) {
        k = 1;
    }
    if (D_8006D8A0.characters[4].entries[2].id == item->id) {
        k = 2;
    }
    if (D_8006D8A0.characters[4].entries[3].id == item->id) {
        k = 3;
    }
    D_8006D8A0.characters[4].entries[k].value4 = item->valueC;
    D_8006D8A0.characters[4].entries[k].value3 = item->valueB;
    D_8006D8A0.characters[4].entries[k].value2 = item->valueA;
    D_8006D8A0.characters[4].entries[k].value3 = item->valueB;
    D_8006D8A0.characters[4].entryItems[k] = index;
    D_8006F8BA[index] = item->durability;
    D_8006D8A0.characters[4].entryItems[k] = index;
    for (i = 0; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];

        if (record->pilot.characterId == 4) {
            record->pilot.entries[k].value4 = item->valueC;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entries[k].value2 = item->valueA;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entryItems[k] = index;
        }
    }
}

/* The Escape command: succeeds on half of the rolls, writing the party back
 * to the game data (8009BE0C). */
s32 func_8009A9D0(void) {
    D_800C34B0->commandIndex = 0;
    if (rand() % 100 < 50) {
        func_8009BE0C();
        return 1;
    }
    return 0;
}

/* The Defense command for member: mark it defending; a gear with status 0x10
 * in its second word drops its gear statuses 0x1B0 and the pilot's 0x1000. */
void func_8009AA44(u8 member) {
    D_800C34B0->commandIndex = 0;
    D_800C34B0->records[member].flags15A |= 1;
    if ((D_800C34B0->records[member].flags15A & 0x80) && (D_800C34B0->records[member].gear.status82 & 0x10)) {
        D_800C34B0->records[member].gear.status7C &= 0xFE4F;
        D_800C34B0->records[member].pilot.status7C &= ~0x1000;
    }
    if (D_800D2C34 == 4) {
        D_800C34B0->message = 0x3D;
    }
}

/* End member's defending. */
void func_8009AB00(u8 member) {
    D_800C34B0->records[member].flags15A &= ~1;
}

/* Enable member's Deathblow commands (descriptors 22 and 24-32; in a gear, gear
 * descriptors 21 and 23-28) when its command flag is set. */
void func_8009AB38(u8 member) {
    s32 flag;

    if (D_800C34B0->records[member].flags15A & 0x80) {
        flag = D_800C34B0->records[member].gear.status80 & 0x2000;
    } else {
        flag = D_800C34B0->records[member].pilot.status88.half.active & 0x400;
    }
    if (flag) {
        if (D_800C34B0->records[member].flags15A & 0x80) {
            CommandDescriptor *commands = D_800C34B0->gearCommands[member];

            commands[21].state = 1;
            commands[23].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
        } else {
            CommandDescriptor *commands = D_800C34B0->partyCommands[member];

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

/* Seal the same commands again and clear member's command flag; unless
 * checked is 0, only for a current command with flag 0x100. */
void func_8009AC48(u8 member, u8 checked) {
    s32 flag;

    if (checked == 0 || (D_800C3DFC->flagsA & 0x100)) {
        if (D_800C34B0->records[member].flags15A & 0x80) {
            flag = D_800C34B0->records[member].gear.status80 & 0x2000;
        } else {
            flag = D_800C34B0->records[member].pilot.status88.half.active & 0x400;
        }
        if (flag) {
            if (D_800C34B0->records[member].flags15A & 0x80) {
                CommandDescriptor *commands = D_800C34B0->gearCommands[member];

                commands[21].state = 0x2000;
                commands[23].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                D_800C34B0->records[member].gear.status80 &= ~0x2000;
            } else {
                CommandDescriptor *commands = D_800C34B0->partyCommands[member];

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
                D_800C34B0->records[member].pilot.status88.half.active &= ~0x400;
            }
        }
    }
}

/* Regeneration amounts of slot for its turn: HP (maxHp / 20), EP (maxEp / 20,
 * from the pilot's or the gear's status) and fuel (maxFuel / 50 for each of
 * two gear statuses). Returns whether any applies; nothing once KO'd. */
s32 func_8009ADA0(u8 slot, s32 *amounts) {
    Combatant *record = &D_800C34B0->records[slot];
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

/* Revive slot's record: clear its statuses (keeping status 0x2000 of the
 * second word), size Chu-Chu's gear HP (8009B104), and when character 3 is
 * revived clear status 0x20 of every enemy. */
void func_8009AEFC(u8 slot) {
    Combatant *record;
    u8 i;

    D_800C34B0->commandIndex = 0;
    record = &D_800C34B0->records[slot];
    record->pilot.status7C = 0;
    record->pilot.status84.half.active = 0;
    record->pilot.status88.half.active = 0;
    record->pilot.status8C.half.active = 0;
    record->pilot.status80 &= 0x2000;
    if (record->pilot.characterId == 7) {
        func_8009B104(slot, record);
    }
    if (record->pilot.characterId == 3) {
        for (i = 3; i < 11; i++) {
            D_800CCCE8.records[i].pilot.status80 &= ~0x20;
        }
    }
}

/* Wear the attacker's weapon items for the current command: commands 0-3 the
 * first entry's, 6 the fourth's, 7-19 both. */
void func_8009AFD8(void) {
    switch (D_800C34B0->commandIndex) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[0]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[0]] += -1;
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
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[0]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[0]] += -1;
        }
        /* fallthrough */
    case 6:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[3]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[3]] += -1;
        }
        break;
    }
}

/* Debug party: members 1 and 2 get 100/100 HP and fixed stats. */
void func_8009B098(void) {
    u8 i;

    for (i = 1; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];

        record->pilot.hp = 100;
        record->pilot.maxHp = 100;
        record->pilot.field5E = 20;
        record->pilot.field5F = 15;
        record->pilot.status7A = 0x1FBF;
        record->field149 = 0;
    }
}

/* Chu-Chu's gear HP: fifty times her HP and maximum HP, capped at 99999. */
void func_8009B104(u8 slot, Combatant *chuchu) {
    Combatant *record = &D_800C34B0->records[slot];
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

/* Debug setup: 99 of every item 1-47, and fixed skill masks for all eleven
 * characters. */
void func_8009B1E4(void) {
    u8 i;

    for (i = 1; i < 48; i++) {
        D_8006F65A[i] = i;
        D_8006F5C4[i] = 99;
    }
    D_8006ECF4[0].mask0 = 0xFFF8;
    D_8006ECF4[0].mask2 = 0xFF00;
    D_8006ECF4[0].mask4 = 0xFFFF;
    D_8006ECF4[0].mask6 = 0xFE00;
    D_8006ECF4[0].flags1A = 0xF000;
    D_8006ECF4[0].field17 = 7;
    D_8006ECF4[1].mask0 = 0xFFE0;
    D_8006ECF4[1].mask2 = 0xFFF0;
    D_8006ECF4[1].mask4 = 0xFFFF;
    D_8006ECF4[1].mask6 = 0xFFF0;
    D_8006ECF4[1].flags1A = 0xC000;
    D_8006ECF4[1].field17 = 7;
    D_8006ECF4[2].mask0 = 0xFFE0;
    D_8006ECF4[2].mask2 = 0xFFE0;
    D_8006ECF4[2].mask4 = 0xFFFF;
    D_8006ECF4[2].mask6 = 0xFF00;
    D_8006ECF4[2].flags1A = 0x8000;
    D_8006ECF4[2].field17 = 7;
    D_8006ECF4[3].mask0 = 0xFFE0;
    D_8006ECF4[3].mask2 = 0xFFC0;
    D_8006ECF4[3].mask4 = 0xFFFF;
    D_8006ECF4[3].mask6 = 0xFF00;
    D_8006ECF4[3].flags1A = 0xF000;
    D_8006ECF4[3].field17 = 7;
    D_8006ECF4[4].mask0 = 0xFFC0;
    D_8006ECF4[4].mask2 = 0xFFC0;
    D_8006ECF4[4].mask4 = 0xFFFF;
    D_8006ECF4[4].mask6 = 0xFC00;
    D_8006ECF4[4].flags1A = 0xE000;
    D_8006ECF4[4].field17 = 7;
    D_8006ECF4[5].mask0 = 0xFFC0;
    D_8006ECF4[5].mask2 = 0xF000;
    D_8006ECF4[5].mask4 = 0xFFFF;
    D_8006ECF4[5].mask6 = 0xF000;
    D_8006ECF4[5].flags1A = 0x8000;
    D_8006ECF4[5].field17 = 7;
    D_8006ECF4[6].mask0 = 0xFFC0;
    D_8006ECF4[6].mask2 = 0xFF00;
    D_8006ECF4[6].mask4 = 0xFFFF;
    D_8006ECF4[6].mask6 = 0xFF00;
    D_8006ECF4[6].flags1A = 0x8000;
    D_8006ECF4[6].field17 = 7;
    D_8006ECF4[7].mask0 = 0;
    D_8006ECF4[7].mask2 = 0xFF00;
    D_8006ECF4[7].mask4 = 0;
    D_8006ECF4[7].mask6 = 0xFF00;
    D_8006ECF4[7].flags1A = 0;
    D_8006ECF4[7].field17 = 7;
    D_8006ECF4[8].mask0 = 0;
    D_8006ECF4[8].mask2 = 0xF800;
    D_8006ECF4[8].mask4 = 0xFFFF;
    D_8006ECF4[8].mask6 = 0;
    D_8006ECF4[8].flags1A = 0xE000;
    D_8006ECF4[8].field17 = 7;
    D_8006ECF4[9].mask0 = 0xFFE0;
    D_8006ECF4[9].mask2 = 0xFFE0;
    D_8006ECF4[9].mask4 = 0xFFFF;
    D_8006ECF4[9].mask6 = 0xFFE0;
    D_8006ECF4[9].flags1A = 0x8000;
    D_8006ECF4[9].field17 = 7;
    D_8006ECF4[10].mask0 = 0xFFC0;
    D_8006ECF4[10].mask2 = 0xFF00;
    D_8006ECF4[10].mask4 = 0xFFFF;
    D_8006ECF4[10].mask6 = 0xFF00;
    D_8006ECF4[10].flags1A = 0xC000;
    D_8006ECF4[10].field17 = 7;
}

#ifdef NON_MATCHING
/* Raise an attack's damage: by half for each of characters 0 and 3 in the
 * party below half HP (gear HP in a gear) and again below a quarter; then a
 * critical chance (10%, 60% with attacker flag 0x200) multiplies it by 1.5
 * (2 with attacker flag 0x400). */
void func_8009B46C(u16 *damage) {
    u8 count = 0;
    u8 i;
    u32 hp;
    u32 maxHp;
    s32 chance;
    s16 scale;

    for (i = 0; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];
        GearRecord *gear = &record->gear;

        if (record->pilot.characterId == 0) {
            if (D_800CCCE8.records[i].flags15A & 0x80) {
                maxHp = record->gear.maxHp;
                hp = record->gear.hp;
            } else {
                maxHp = record->pilot.maxHp;
                hp = record->pilot.hp;
            }
            if (hp < maxHp >> 1) {
                count++;
            }
            if (hp < maxHp >> 2) {
                count++;
            }
        }
        if (record->pilot.characterId == 3) {
            if (D_800CCCE8.records[i].flags15A & 0x80) {
                maxHp = gear->maxHp;
                hp = gear->hp;
            } else {
                maxHp = record->pilot.maxHp;
                hp = record->pilot.hp;
            }
            if (hp < maxHp >> 1) {
                count++;
            }
            if (hp < maxHp >> 2) {
                count++;
            }
        }
    }
    chance = 10;
    if (count) {
        *damage += count * (*damage >> 1);
    }
    scale = 3;
    if (D_800C3E00->pilot.flags32 & 0x400) {
        scale = 4;
    }
    if (D_800C3E00->pilot.flags32 & 0x200) {
        chance = 60;
    }
    if (rand() % 100 < chance) {
        *damage = scale * *damage >> 1;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_8008CCCC", func_8009B46C);
#endif

/* Show the message for an applied status, named by its kind and flag bit. */
void func_8009B684(u8 kind, u16 flag) {
    switch (kind) {
    case 0:
        switch (flag) {
        case 0x2000:
            D_800C34B0->message = 0x1;
            break;
        case 0x1000:
            D_800C34B0->message = 0x2;
            break;
        case 0x800:
            D_800C34B0->message = 0x3;
            break;
        case 0x400:
            D_800C34B0->message = 0x4;
            break;
        case 0x200:
            D_800C34B0->message = 0x5;
            break;
        case 0x1:
            D_800C34B0->message = 0x8;
            break;
        }
        break;
    case 2:
        switch (flag) {
        case 0x2000:
            D_800C34B0->message = 0x9;
            break;
        case 0x1000:
            D_800C34B0->message = 0xA;
            break;
        case 0x800:
            D_800C34B0->message = 0xB;
            break;
        case 0x400:
            D_800C34B0->message = 0xC;
            break;
        case 0x1:
            D_800C34B0->message = 0xD;
            break;
        case 0x20:
            D_800C34B0->message = 0x7;
            break;
        }
        break;
    case 5:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0xE;
            break;
        case 0x4000:
            D_800C34B0->message = 0xF;
            break;
        case 0x2000:
            D_800C34B0->message = 0x10;
            break;
        case 0x1000:
            D_800C34B0->message = 0x11;
            break;
        case 0x800:
            D_800C34B0->message = 0x12;
            break;
        case 0x1800:
            D_800C34B0->message = 0x13;
            break;
        }
        break;
    case 7:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x15;
            break;
        case 0x4000:
            D_800C34B0->message = 0x16;
            break;
        case 0x1000:
            D_800C34B0->message = 0x18;
            break;
        case 0x2:
        case 0x8:
            D_800C34B0->message = 0x19;
            break;
        case 0x1:
        case 0x4:
            D_800C34B0->message = 0x1A;
            break;
        }
        break;
    case 9:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x1B;
            break;
        case 0x4000:
            D_800C34B0->message = 0x1C;
            break;
        case 0x2000:
            D_800C34B0->message = 0x1D;
            break;
        case 0x1000:
            D_800C34B0->message = 0x1E;
            break;
        case 0x400:
            D_800C34B0->message = 0x1F;
            break;
        case 0x800:
            D_800C34B0->message = 0x20;
            break;
        case 0x100:
            D_800C34B0->message = 0x21;
            break;
        case 0x200:
            D_800C34B0->message = 0x22;
            break;
        }
        break;
    }
}

/* Choose an automatic action for slot (confusion or auto-battle): choice[0]
 * is the kind (4 defend, 2 a skill with choice[1] its index among the
 * character's usable skills, 0/1 an attack with choice[1] its strength). Out of
 * a gear: defend on 10% unless flagged, a known skill on 25%, then Chu-Chu's
 * basic attack; otherwise attack weak 48%, medium 32%, strong 20%. */
void func_8009BAC4(u8 slot, u8 *choice, s16 *busy) {
    Combatant *record = &D_800CCCE8.records[slot];
    u16 skills;
    u8 count;
    u8 skill;

    if (!(D_800CCCE8.records[slot].flags15A & 0x80)) {
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
            if (D_8006ECF4[record->pilot.characterId].mask2 & ((0x8000 >> skill) & skills)) {
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

/* Damage the target by the command's power in twentieths of its gear's
 * maximum HP. */
void func_8009BD94(void) {
    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = D_800C3DFC->power * D_800D2DC8->maxHp / 20;
}

/* Write the party back to the game data: each present member's HP (Chu-Chu in
 * a gear takes hers from the gear's HP), EP, use counts and field 0x3A, with
 * HP 1 when KO'd, and the HP and fuel of its gear (ids 0-6 and 8-16), a tenth
 * of the maximum when the gear is wrecked. */
void func_8009BE0C(void) {
    u8 i;
    u8 j;
    s32 unused[2]; /* never used; it gives the original its 8-byte frame */

    for (i = 0; i < 3; i++) {
        Combatant *record;
        CharacterRecord *character;
        GearRecord *gearRecord;
        GearRecord *gear;

        if (D_800D2D24[i] == 0x7F) {
            continue;
        }
        record = &D_800CCCE8.records[i];
        character = &D_8006D8A0.characters[record->pilot.characterId];
        gear = &D_800CCCE8.records[i].gear;
        gearRecord = &D_8006D8A0.gears[record->pilot.gearId];
        if (record->pilot.characterId == 7 && (D_800CCCE8.records[i].flags15A & 0x80)) {
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

/* Gear warning flags of slot: 1 gear status 0x400, 2 gear HP below an eighth
 * (unless the pilot's flag 1 at +0x36), 4 when field 0x148 is 4. */
s32 func_8009C050(u8 slot) {
    Combatant *record = &D_800CCCE8.records[slot];
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    u8 *state = &D_800CCCE8.records[slot].field148;
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

/* Put slot into state 4 with timer 6 at three turns, and set flag 0x4000 of
 * character 0. */
void func_8009C0E0(u8 slot) {
    D_800CCCE8.records[slot].field148 = 4;
    D_800CCCE8.records[slot].statusTimers[6] = 3;
    D_8006ECF4[0].flags1A |= 0x4000;
}

/* Target the party member that is character 3. */
void func_8009C134(void) {
    u8 i;

    for (i = 0; i < 3; i++) {
        if (D_800CCCE8.records[i].pilot.characterId == 3) {
            D_800CCCE8.targetMask = 1 << i;
        }
    }
}

/* Resolve a gear's action against every target in the target mask: select the
 * current command descriptor, run its gear formula (table D_800C34DC) per
 * target with the status step it asks for (8009DBFC), then the per-target
 * follow-ups and the command's wear and element. */
void func_8009C198(void) {
    s32 bit;

    if (D_800C3E04 < 3) {
        D_800C3DFC = &D_800C34B0->gearCommands[D_800C34B0->attackerIndex][D_800C34B0->commandIndex];
    } else {
        D_800C3DFC = &D_800C34B0->enemyCommands[D_800C34B0->commandIndex];
    }
    /* Called without its second argument (an unprototyped call). */
    ((void (*)())func_8009AC48)(D_800C3E04);
    D_800C3D3C = &D_800C34B0->records[D_800C34B0->attackerIndex].field148;
    D_800D2DC4 = 0;
    if ((D_800C3DFC->flagsA & 0x100) && D_800C3E00->pilot.characterId == 1) {
        func_80096824();
    }
    if ((D_800C3E00->pilot.gearId == 5 || D_800C3E00->pilot.gearId == 13) && D_800C34B0->commandIndex == 1) {
        D_800C3DFC->attributes[2] = D_800D2D6C->entries[1].valueE;
    }
    bit = 1;
    for (D_800C3E50 = 0; D_800C3E50 < 11; D_800C3E50++, bit <<= 1) {
        if (bit & D_800C34B0->targetMask) {
            D_800C3E34 = &D_800C34B0->records[D_800C3E50];
            D_800D2DC8 = &D_800C34B0->records[D_800C3E50].gear;
            D_800C3D60 = &D_800C34B0->records[D_800C3E50].field148;
            func_8009CA90();
            D_800C34DC[D_800C3DFC->formula]();
            if (D_800C34B0->resultCode[D_800C3E50] == 0) {
                if (D_800C3DFC->flagsA & 0x800) {
                    func_8009DBFC(1);
                } else if (D_800C3DFC->flagsA & 0x4000) {
                    func_8009DBFC(0);
                }
            }
            D_800C34B0->shownCommand = D_800C3DFC->name;
            func_8009C4B4();
            func_8009CB68(D_800C3E50);
        }
    }
    if (D_800C3E00->pilot.characterId == 4) {
        func_8009E788();
    }
    func_8009C9C4();
}

/* Per-target follow-up of a gear action: a party gear's attack level (up by
 * one with commands 0-2, set below D_800D2C34 by the level-3/6/9 command
 * groups, which also raise the pilot's field 0x54), then the target's
 * reactions: breaking status 0x2000 on 80%, fuel-drain immunity, halving or
 * raising damage by its flag 0x80, reflecting it with flag 0x20, and nullifying
 * with status 0x200. */
void func_8009C4B4(void) {
    s32 chance;

    if (D_800C3E04 < 3) {
        if (D_800C34B0->commandIndex < 3) {
            if (++D_800C3D3C[0] > D_800C3D3C[1]) {
                D_800C3D3C[0]--;
            }
        }
        if ((u32)(D_800C34B0->commandIndex - 3) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 1;
        }
        if ((u32)(D_800C34B0->commandIndex - 6) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 2;
        }
        if ((u32)(D_800C34B0->commandIndex - 9) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 3;
        }
        if (D_800C34B0->commandIndex >= 3 && D_800C34B0->commandIndex < 12) {
            D_800C3E00->pilot.field54 += D_800C34B0->commandIndex / 3;
        }
        if (D_800C34B0->commandIndex == 5) {
            D_800C3E00->pilot.field54 += 1;
        }
        if (D_800C34B0->commandIndex == 8) {
            D_800C3E00->pilot.field54 += 2;
        }
        if (D_800C34B0->commandIndex == 11) {
            D_800C3E00->pilot.field54 += 3;
        }
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 0 && (D_800C3E34->pilot.status80 & 0x2000)
        && rand() % 100 < 80) {
        D_800C3E34->pilot.status80 &= ~0x2000;
        D_800D2DC8->status7C &= ~0x1000;
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 10 && (D_800D2DC8->field7E & 0x80)) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 0) {
        if (D_800C3E34->pilot.flags32 & 0x80) {
            chance = 60;
            if (D_800C3E34->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                D_800C34B0->damage[D_800C3E50] >>= 1;
            } else {
                D_800C34B0->damage[D_800C3E50] += D_800C34B0->damage[D_800C3E50] >> 1;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x20) {
            D_800C34B0->resultCode[D_800C3E04] = 0;
            D_800C34B0->damage[D_800C3E04] = D_800C34B0->damage[D_800C3E50];
        }
    }
    if ((D_800C3E34->pilot.status88.half.permanent & 0x200) && D_800C34B0->resultCode[D_800C3E50] == 1) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
}

/* The gear version of 80099FB0: make the current command descriptor the
 * battle's current command, a command without an element taking the gear's
 * element statuses. */
void func_8009C9C4(void) {
    u16 elements = (D_800D2D6C->status84.half.active | D_800C3E00->pilot.status88.half.permanent) >> 12;

    D_800C34B0->commandAttributes[0] = D_800C3DFC->attributes[0];
    D_800C34B0->commandAttributes[1] = D_800C3DFC->attributes[1];
    D_800C34B0->commandAttributes[2] = D_800C3DFC->attributes[2];
    D_800C34B0->commandAttributes[3] = D_800C3DFC->attributes[3];
    D_800C34B0->commandIndexCopy = D_800C34B0->commandIndex;
    if ((D_800C34B0->commandAttributes[2] & 0x3F) == 0) {
        D_800C34B0->commandAttributes[2] |= elements;
    }
}

/* Against a target on foot, switch the current descriptor to its on-foot
 * variant: commands 12-14 three descriptors on, commands 0-2 fifteen. */
void func_8009CA90(void) {
    if ((u32)(D_800C34B0->commandIndex - 12) < 3 && !(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        D_800C3DFC += 3;
    } else if (D_800C34B0->commandIndex < 3 && !(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        D_800C3DFC += 15;
    }
}

/* Mirror the gear's status 0x200 (second word) as the pilot's status 0x20. */
void func_8009CB68(u8 slot) {
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    Combatant *record = &D_800CCCE8.records[slot];

    if (gear->status80 & 0x200) {
        record->pilot.status84.half.active |= 0x20;
    } else {
        record->pilot.status84.half.active &= ~0x20;
    }
}

/* Gear attack damage: the gear hit outcome, attack and defense values with
 * the element adjustment and both gears' boost/break statuses, the command's
 * drain effects, then (5a - 4d for ether, else 4a - 3d) times the power over
 * 20, a random spread, the element resistance and the hit outcome's result
 * code; at most 9999. */
void func_8009CBC4(void) {
    u16 attack;
    u16 defense;
    s8 hit;
    u8 power;
    s32 damage;
    s32 attackScale;
    s32 defenseScale;
    u16 flags;
    u8 guard;

    power = D_800C3DFC->power;
    hit = func_8009D3A0();
    attack = func_8009D948();
    defense = func_8009DA04();
    func_80096494(&attack, &defense, &hit);
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 8) {
        attack += attack / 5;
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 2) {
        attack += attack / 10;
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 4) {
        attack -= attack / 5;
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 1) {
        attack -= attack / 10;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 4) {
        defense += defense / 5;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 1) {
        defense += defense / 10;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 8) {
        defense -= defense / 5;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 2) {
        defense -= defense / 10;
    }
    if (D_800C3DFC->attributes[2] & 0x10) {
        if (!(D_800C3E34->pilot.status82 & 0x40)) {
            D_800C3E34->pilot.status80 |= 0x40;
        }
        if ((D_800D2D6C->status84.half.active | D_800D2D6C->status84.half.permanent) & 0x4000) {
            D_800D2C88[D_800C3E04] = 3;
            D_800D2C54[D_800C3E04] = (u16)(D_800C3E00->pilot.maxEp / 10) * 2;
        }
        if ((D_800D2D6C->status84.half.active | D_800D2D6C->status84.half.permanent) & 0x1000) {
            D_800D2C88[D_800C3E04] = 2;
            D_800D2C54[D_800C3E04] = D_800D2D6C->maxHp / 10 * 2;
        }
    }
    if ((D_800C3DFC->attributes[2] & 0x20) && !(D_800C3E34->pilot.status82 & 0x80)) {
        D_800C3E34->pilot.status80 |= 0x80;
    }
    if (D_800C3E34->pilot.status80 & 0x40) {
        defense -= defense >> 2;
        D_800C3E34->pilot.status80 &= 0xFFBF;
    }
    if (D_800C3E00->pilot.status80 & 0x80) {
        attack -= attack >> 2;
        D_800C3E00->pilot.status80 &= 0xFF7F;
    }
    flags = D_800C3DFC->flagsA;
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
    switch (D_800C3DFC->amountKind) {
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
    if (D_800C3DFC->elements != 0) {
        damage = func_8009DB54(damage);
    }
    switch (hit) {
    case 1:
        D_800C34B0->resultCode[D_800C3E50] = 0;
        break;
    case 2:
        D_800C34B0->resultCode[D_800C3E50] = 5;
        guard = D_800D2DC8->guard;
        if (guard >= 10) {
            guard = 9;
        }
        if (damage != 0) {
            damage = damage * (10 - guard) / 20;
        }
        break;
    case 3:
        damage = 0;
        D_800C34B0->resultCode[D_800C3E50] = 4;
        break;
    case 4:
        D_800C34B0->resultCode[D_800C3E50] = 2;
        break;
    }
    if (D_800D2DC4 && (D_800C3DFC->flagsA & 0x100) && damage != 0) {
        damage /= 3;
    }
    if (damage >= 10000) {
        damage = 9999;
    }
    if (damage < 0) {
        damage = 0;
    }
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Mark the target missed (result 6) when 8009DBFC finds no hit. */
void func_8009D354(void) {
    if (func_8009DBFC(0) == 0) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
    }
}

/* Gear hit outcome of the current command on the target: 1 hit, 2 half,
 * 3 miss. Like 80096ab8 with gear accuracy (+0x9f with a broken weapon,
 * 1.5x with status 0x800), the target's evasion (half its gear's +0x9f,
 * 1.5x with status 0x400) and the gears' blind/evade status 0x10. */
s8 func_8009D3A0(void) {
    s16 penalty = 0;
    s16 bonus = 0;
    s16 evasion;
    s16 accuracy;
    u8 durability;
    s16 margin;
    s16 roll;
    u16 status;

    if ((D_800C3DFC->flagsA & 0x200) && (D_800C3E34->pilot.flags34 & 8)) {
        return 3;
    }
    if (D_800C3DFC->flagsA & 0x1000) {
        return 3;
    }
    if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x100) {
        return 3;
    }
    if (D_800C3E34->pilot.status7C & 0x2000) {
        return 1;
    }
    if (D_800C3E34->pilot.status80 & 0x1000) {
        return 1;
    }
    if (D_800C3DFC->flagsA & 0x8000) {
        return 1;
    }
    evasion = D_800C3E34->pilot.field5F;
    durability = D_8006F8EA[D_800D2D6C->partItems[0]];
    accuracy = D_800C3E00->pilot.field5E;
    if (durability == 0) {
        accuracy += D_800D2D6C->hitBonus;
    }
    if (D_800D2DC8->hitBonus != 0) {
        evasion += D_800D2DC8->hitBonus / 2;
    }
    if (D_800C3E00->pilot.characterId == 4) {
        if ((D_800C3DFC->itemKinds & 0x80) && durability == 0) {
            return 3;
        }
        if ((D_800C3DFC->itemKinds & 0x20) && D_8006F8EA[D_800D2D6C->partItems[3]] == 0) {
            return 3;
        }
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x800) {
        accuracy += accuracy / 2;
    }
    if (D_800D2D6C->status7C & 0x10) {
        penalty = 60;
    }
    if (D_800C3DFC->flagsA & 0x1000) {
        return 3;
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 0x400) {
            evasion += evasion / 2;
        }
        if (D_800D2DC8->status7C & 0x10) {
            bonus = 30;
        }
        if (D_800D2DC8->status7C & 0xC00) {
            return 1;
        }
    } else {
        accuracy /= 2;
        if (D_800C3E34->pilot.status7C & 0x2000) {
            return 1;
        }
        if (D_800C3E34->pilot.status80 & 0x1000) {
            return 1;
        }
        if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x800) {
            evasion += evasion / 2;
        }
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 1) {
        if (rand() % 100 < 95) {
            return 2;
        }
        return 1;
    }
    margin = accuracy + D_800C3DFC->hitBonus - evasion;
    status = D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent;
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

/* Damage of a gear attack (80096FBC); a command with flag 0x100 scales it by
 * the gear's frame factor in quarters (4 when unset or with status 0x100), and
 * status 0x40 adds half again. */
u16 func_8009D948(void) {
    u16 damage = func_80096FBC();
    u8 factor;

    if (D_800C3DFC->flagsA & 0x100) {
        factor = D_800D2D6C->frameFactor;
        if ((D_800D2D6C->status7C & 0x100) || factor == 0) {
            factor = 4;
        }
        damage = factor * damage / 4;
        if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x40) {
            damage += damage >> 1;
        }
    }
    return damage;
}

/* Damage of an attack on a gear (80097610): a command with flag 0x100 adds
 * the target gear's armor (reduced by its defense percentage) when the target
 * is in a gear, and half again with status 0x20; otherwise the damage itself
 * is reduced by the defense percentage. */
u16 func_8009DA04(void) {
    u16 damage = func_80097610();
    u16 armor;
    u8 defense;

    if (D_800C3DFC->flagsA & 0x100) {
        defense = D_800D2DC8->defense;
        armor = D_800D2DC8->armor;
        if (defense != 0 && armor != 0) {
            armor = armor * (100 - defense) / 100;
        }
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            damage += armor;
        }
        if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 0x20) {
            damage += damage >> 1;
        }
    } else if (D_800D2DC8->defense != 0) {
        damage = damage * (100 - D_800D2DC8->defense) / 100;
    }
    return damage;
}

/* Scale damage by the target gear's resistance (in twentieths, 20 or more
 * nullifies) to the command's first element. */
s32 func_8009DB54(s32 damage) {
    u8 i;
    u8 element;
    u8 resistance;

    for (i = 0; i < 16; i++) {
        if (D_800C3DFC->elements & (0x8000 >> i)) {
            element = i;
            break;
        }
    }
    resistance = D_800D2DC8->resistances[element];
    if (resistance != 0) {
        if (resistance < 20) {
            damage = damage * (20 - resistance) / 20;
        } else {
            damage = 0;
        }
    }
    return damage;
}

/* Roll a status onto the target's gear, from the attacking gear's first
 * part (`fromGear`: chance +0x13, kind +0x14, flag +0x10, 5 turns) or from
 * the command (+0x1c/+0x1d/+0x1e, turns +0x11): cancel opposite statuses,
 * check immunities, set the kind's status word and turn timer, or apply the
 * special kinds 10-16. Returns 1 when it took. */
s8 func_8009DBFC(u8 fromGear) {
    Combatant *record = &D_800C34B0->records[D_800C3E50];
    CharacterRecord *pilot = &record->pilot;
    s8 chance;
    u8 kind;
    u16 flags;
    u8 turns;

    if (fromGear) {
        chance = D_800D2D6C->entries[0].value10;
        kind = D_800D2D6C->entries[0].value11;
        flags = D_800D2D6C->entries[0].field0;
        turns = 5;
    } else {
        chance = D_800C3DFC->field1C;
        kind = D_800C3DFC->field1D;
        flags = D_800C3DFC->field1E;
        turns = D_800C3DFC->power;
    }
    if (!(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        return 0;
    }
    if (chance < rand() % 100) {
        return 0;
    }
    switch (kind) {
    case 0:
        if ((flags & 0x20) && (D_800D2DC8->status80 & 0x8000)) {
            D_800D2DC8->status80 &= 0x7FFF;
            D_800C3E34->pilot.status84.half.active &= 0x7FFF;
            return 1;
        }
        break;
    case 1:
        if (flags & 0xA) {
            if (D_800D2DC8->status80 & 5) {
                D_800D2DC8->status80 &= 0xFFFA;
                return 1;
            }
        } else if (flags & 5) {
            if (D_800D2DC8->status80 & 0xA) {
                D_800D2DC8->status80 &= 0xFFF5;
                return 1;
            }
        }
        break;
    }
    if (kind == 0) {
        if (flags & D_800D2DC8->field7E) {
            return 0;
        }
        switch (flags) {
        case 0x400:
            record->statusTimers[0] = turns;
            D_800D2DC8->status7C &= 0xFBFF;
            pilot->status7C |= 0x2000;
            break;
        case 0x1000:
            D_800D2DC8->status7C &= 0xEFFF;
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
        D_800D2DC8->status7C |= flags;
    }
    if (kind == 1) {
        D_800D2DC8->status80 |= flags;
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
            if (D_800D2DC8->status84.half.permanent & 0xF000) {
                return 0;
            }
            D_800D2DC8->status84.half.active = flags | (D_800D2DC8->status84.half.active & 0xFFF);
            record->statusTimers[7] = turns;
        }
        if (flags & 0xF00) {
            if (D_800D2DC8->status84.half.permanent & 0xF00) {
                return 0;
            }
            D_800D2DC8->status84.half.active = flags | (D_800D2DC8->status84.half.active & 0xF0FF);
            record->statusTimers[8] = turns;
        }
    }
    if (kind == 10) {
        D_800D2DC8->status84.half.active &= ~flags;
    }
    if (kind == 11 && !(D_800D2DC8->field7E & 0x40)) {
        D_800D2DC8->defense += flags;
        kind = 0;
        if (D_800D2DC8->defense >= 100) {
            D_800D2DC8->defense = 99;
        }
        flags = 0x40;
    }
    if (kind == 12) {
        D_800C34B0->resultCode[D_800C3E50] = 0;
        D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp / flags;
    }
    if (kind == 13) {
        D_800C34B0->resultCode[D_800C3E50] = 0;
        D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp - 1;
    }
    if (kind == 14) {
        D_800D2DC8->status80 = 0;
        D_800D2DC8->status84.half.active = 0;
        if (flags == 1) {
            D_800D2DC8->status82 = 0;
            D_800D2DC8->status84.half.permanent = 0;
        }
    }
    if (kind == 16) {
        D_800D2DC8->status7C |= 1;
        D_800C3E34->pilot.status7C |= 0x80;
    }
    if (kind == 15) {
        D_800D2DC8->status7C &= 0xFFFE;
        D_800C3E34->pilot.status7C &= 0xFF7F;
    }
    func_8009E868(kind, flags);
    return 1;
}

/* Clear the target gear's defense percentage. */
void func_8009E268(void) {
    D_800D2DC8->defense = 0;
}

/* Damage the target gear by field 0x4F tenths of its maximum HP. */
void func_8009E278(void) {
    u32 damage = D_800D2DC8->field4F * D_800D2DC8->maxHp / 10;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Damage the target gear by the command's power in twentieths of its maximum
 * HP. */
void func_8009E2EC(void) {
    u32 damage = D_800C3DFC->power * D_800D2DC8->maxHp / 20;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Damage the target by the attacker's accuracy times the command's power. */
void func_8009E364(void) {
    u32 damage = D_800C3E00->pilot.accuracy * D_800C3DFC->power;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Put the target into state 4 with timer 6 at three turns. */
void func_8009E3C8(void) {
    *D_800C3D60 = 4;
    D_800CCCE8.records[D_800C3E50].statusTimers[6] = 3;
}

/* Drain the target gear's fuel (result 10) by the command's power in
 * twentieths of its maximum fuel. */
void func_8009E410(void) {
    s32 amount = D_800D2DC8->maxFuel * D_800C3DFC->power / 20;

    D_800C34B0->resultCode[D_800C3E50] = 10;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Restore the target gear's fuel (result 11) by the command's power in
 * twentieths of its maximum fuel. */
void func_8009E48C(void) {
    s32 amount = D_800D2DC8->maxFuel * D_800C3DFC->power / 20;

    D_800C34B0->resultCode[D_800C3E50] = 11;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Clear the target gear's statuses 0x7F4 and the target's status 0x20. */
void func_8009E508(void) {
    D_800D2DC8->status7C &= 0xF80B;
    D_800C3E34->pilot.status7A &= ~0x20;
}
