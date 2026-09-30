/*
 * Menu overlay (mode 5; Disc 1 slot 39 / unpacked 2597, Disc 2 slot 34 /
 * unpacked 2592; loaded at 801c5000). The resident mode table enters it
 * through 8001c634 with no overlay preloaded. By the menu kind in
 * D_80059460 it runs the field main menu (items, equipment and the other
 * commands; kind 0), the title screen's file (memory-card load) screen
 * (kind 2) or kind 6. It keeps its state behind D_800625A0 and reads and
 * writes "bu00:"/"bu10:" memory-card files (BASLUS-00664...).
 */
#include "menu.h"

/* Run the command at `offset` past the top cursor (0 back, 1 load/save file,
 * 2..6 the field-menu screens, 7/8 the title file screen's load and new game,
 * 9 reset), then restore the command window. Returns 0 when the menu ends. */
u8 func_801C531C(u8 offset) {
    u8 redraw;
    u8 stay;

    D_801E977A = 1;
    stay = 1;
    redraw = 1;
    switch (D_800625A0->cursor + offset) {
    case 7:
        redraw = func_801D9808();
        break;
    case 8:
        if (func_801D9F98(1, 0)) {
            D_800594D0 = 2;
            stay = 0;
        }
        break;
    case 1:
        redraw = func_801D9F98(0, D_801E96A4);
        break;
    case 2:
        redraw = func_801E23CC();
        break;
    case 3:
        redraw = func_801DE29C(D_800625A0->firstMember, 1);
        break;
    case 4:
        redraw = func_801DBE54();
        break;
    case 5:
        redraw = func_801E0F78(D_800625A0->firstMember, 1);
        break;
    case 6:
        redraw = func_801E2BE4();
        break;
    case 9:
        func_8001B970();
    case 0:
        redraw = 0;
        stay = 0;
        break;
    }
    D_800625A0->card->mode = 0;
    if (redraw) {
        func_801D1EB0();
        if (D_80059460 == 0) {
            func_801D29A8(1, 0);
        } else if (D_80059460 == 2) {
            func_801E8018(8, D_800625A0->commandLabels, D_801EA530, D_800625A0->party->labels);
            D_800625A0->primitives->shade = 0x4c;
        }
    }
    func_801E3088(offset);
    func_801D3674();
    D_800625A0->screenImages->captured = 0;
    D_800625A0->screenImages->refresh = 1;
    D_800625A0->party->redraw4 = 1;
    D_800625A0->party->redraw3 = 1;
    D_800625A0->cursorShown = 0xff;
    D_800625A0->party->redrawA = 0;
    D_801E9784 = 1;
    return stay;
}

/* The field menu's command loop: move the cursor over the seven commands and
 * run the chosen one until the menu is left. */
void func_801C55A0(void) {
    u8 ok;
    u8 stay;

    stay = 1;
    do {
        func_801C7BF4();
        switch (D_800625A0->input) {
        case 4:
            ok = 1;
            if (D_800625A0->cursor == 2 && D_800625A0->fighters == 0) {
                ok = 0;
                func_801C8574(4);
            }
            if (ok) {
                D_800625A0->screenImages->captured = 1;
                func_801D22C4();
                func_801E8044(8, D_800625A0->party->labels);
                stay = func_801C531C(0);
            }
            break;
        case 5:
            stay = 0;
            break;
        case 1:
            if (D_800625A0->cursor != 0) {
                D_800625A0->cursor--;
            } else {
                D_800625A0->cursor = 6;
            }
            break;
        case 3:
            if (++D_800625A0->cursor >= 7) {
                D_800625A0->cursor = 0;
            }
            break;
        }
        if (D_800625A0->cursor != D_800625A0->cursorShown) {
            func_801E8978(7, D_800625A0->cursor, D_801EA19C);
            func_801E8070(8, D_800625A0->commandLabels, D_801EA528, D_801E9E64, D_800625A0->party->labels,
                          D_800625A0->cursor, 0, 0);
            D_800625A0->cursorShown = D_800625A0->cursor;
        }
    } while (stay);
}

/* Menu kind 6: wait for the pad to be released, check the cards and, when a
 * file can be saved, run the save file screen; then the loaded-disc check. */
void func_801C57A4(void) {
    u8 save;
    u8 found;

    D_80059171 = 1;
    D_801E977A = 0;
    save = 1;
    func_801D1E80();
    while (D_800625A0->viewMotion != 0) {
        func_801C7BF4();
    }
    func_801D22F4(0);
    found = func_801CACF8(0x7d, 0xff, 1);
    if ((found || D_801EA8FC != 0) && func_801CACF8(0x80, 0xff, 1) != 0) {
        save = 0;
    }
    func_801D2484();
    if (save) {
        D_801E96A5 = 1;
        D_801E96A4 = 1;
        func_801C531C(0);
        D_801E96A4 = 0;
        D_801E96A5 = 0;
    }
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
    func_801C8694(D_80059171);
}

/* The title screen's file screen loop: choose among its three commands; after
 * 600 idle frames on the first screen it returns with D_800594D0 = 1. */
void func_801C58EC(void) {
    u8 stay;

    stay = 1;
    func_801E8474(4, D_801EA1D4);
    func_801E8018(8, D_800625A0->commandLabels, D_801EA530, D_800625A0->party->labels);
    D_800625A0->frameCounter = 0;
    do {
        func_801C7BF4();
        switch (D_800625A0->input) {
        case 4:
            D_800625A0->screenImages->captured = 1;
            func_801D22C4();
            func_801E8044(8, D_800625A0->party->labels);
            D_800625A0->primitives->shade = 0x40;
            stay = func_801C531C(7);
            D_801E9784 = 0;
            D_800625A0->frameCounter = 0;
            break;
        case 1:
            if (D_800625A0->cursor != 0) {
                D_800625A0->cursor--;
            } else {
                D_800625A0->cursor = 2;
            }
            D_800625A0->frameCounter = 0;
            break;
        case 3:
            if (++D_800625A0->cursor >= 3) {
                D_800625A0->cursor = 0;
            }
            D_800625A0->frameCounter = 0;
            break;
        }
        if (D_800625A0->cursor != D_800625A0->cursorShown) {
            func_801E8978(3, D_800625A0->cursor, D_801EA1D4);
            func_801E8070(8, D_800625A0->commandLabels, D_801EA530, D_801E9E84, D_800625A0->party->labels,
                          D_800625A0->cursor, 0, 0);
            D_800625A0->cursorShown = D_800625A0->cursor;
        }
        if (func_80028530() == 1 && (u32)D_800625A0->frameCounter > 600) {
            stay = 0;
            D_800594D0 = 1;
        }
    } while (stay);
    func_801E8044(8, D_800625A0->party->labels);
}

/* Allocate and clear (nonzero) or free (zero) the memory-card state block. */
void func_801C5B54(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->card = block;
        bzero(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate and clear (nonzero) or free (zero) the party block. */
void func_801C5BB8(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6c, 0);
        D_800625A0->party = block;
        bzero(block, 0x6c);
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate and clear (nonzero) or free (zero) the screen image block. */
void func_801C5C1C(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->screenImages = block;
        bzero(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->screenImages);
    }
}

/* Allocate and clear (nonzero) or free (zero) the sprite lists (+354). */
void func_801C5C80(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140c, 0);
        D_800625A0->spriteLists = block;
        bzero(block, 0x140c);
    } else {
        func_800320E8(D_800625A0->spriteLists);
    }
}

/* Allocate and clear (nonzero) or free (zero) the data table directory. */
void func_801C5CE4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xcc, 0);
        D_800625A0->tables = block;
        bzero(block, 0xcc);
    } else {
        func_800320E8(D_800625A0->tables);
    }
}

/* Allocate and clear (nonzero) or free (zero) the first field-menu block. */
void func_801C5D48(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x328, 0);
        D_800625A0->fieldMenu = block;
        bzero(block, 0x328);
    } else {
        func_800320E8(D_800625A0->fieldMenu);
    }
}

/* Allocate and clear (nonzero) or free (zero) the second field-menu block. */
void func_801C5DAC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x374, 0);
        D_800625A0->fieldMenu2 = block;
        bzero(block, 0x374);
    } else {
        func_800320E8(D_800625A0->fieldMenu2);
    }
}

/* Allocate and clear (nonzero) or free (zero) the shared primitive block. */
void func_801C5E10(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15c, 0);
        D_800625A0->primitives = block;
        bzero(block, 0x15c);
    } else {
        func_800320E8(D_800625A0->primitives);
    }
}

/* Allocate and clear (nonzero) or free (zero) the three field blocks. */
void func_801C5E74(u8 allocate) {
    s32 i;

    if (allocate) {
        for (i = 0; i < 3; i++) {
            void *block = func_80031BDC(0x127c, 0);
            D_800625A0->fieldBlocks[i] = block;
            bzero(block, 0x127c);
        }
    } else {
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->fieldBlocks[i]);
        }
    }
}

/* Allocate the blocks of the menu kind in D_80059460 (the card state for the
 * title file screen and kind 6; the card state and field blocks for the field
 * menu). */
void func_801C5F10(void) {
    void *block;

    func_801C5BB8(1);
    func_801C5C1C(1);
    func_801C5C80(1);
    func_801C5CE4(1);
    func_801C5E10(1);
    block = func_80031BDC(0x14c, 0);
    D_800625A0->markers = block;
    bzero(block, 0x14c);
    switch (D_80059460) {
    case 0:
        func_801C5B54(1);
        func_801C5D48(1);
        func_801C5DAC(1);
        func_801C5E74(1);
        break;
    case 2:
    case 6:
        func_801C5B54(1);
        break;
    }
}

/* Leave the menu: keep the field menu's cursor, stop drawing, free the sound
 * bank and every block, then the state itself. */
void func_801C5FE4(void) {
    switch (D_80059460) {
    case 0:
        func_801D22C4();
        func_801D29A8(0, 0);
        D_800625A0->party->redraw6 = 0;
        D_800625A0->party->redraw5 = 0;
        if (D_80059171 == 0) {
            D_800594CC = D_800625A0->cursor;
        }
        break;
    case 2:
    case 6:
        break;
    }
    func_801C7BF4();
    func_801C7BF4();
    D_800625A0->drawing = 0;
    func_801C7BF4();
    do {
        func_801C7BF4();
    } while (D_800625A0->bufferIndex != 0);
    func_801C5BB8(0);
    func_801C5C1C(0);
    func_801C5C80(0);
    func_801C5CE4(0);
    func_801C5E10(0);
    func_800320E8(D_800625A0->markers);
    func_800320E8(D_800625A0->sheet);
    func_800320E8(D_800625A0->labels);
    func_800320E8(D_800625A0->topLabels[0].pixels);
    if (D_80059178 != 0) {
        func_8003A094(D_800625A0->effectBank);
        func_801C7BF4();
        func_8003852C(D_800625A0->effectBank);
        func_801C7BF4();
        func_800320E8(D_800625A0->effectBank);
    }
    switch (D_80059460) {
    case 0:
        func_801C5B54(0);
        func_801C5D48(0);
        func_801C5DAC(0);
        func_801C5E74(0);
        func_800320E8(D_800625A0->portraits[0]);
        func_800320E8(D_800625A0->portraitMarks[0]);
        func_800320E8(D_800625A0->portraits[1]);
        func_800320E8(D_800625A0->portraitMarks[1]);
        break;
    case 2:
    case 6:
        func_801C5B54(0);
        break;
    }
    func_800320E8(D_800625A0);
}

/* The menu mode: allocate, set up the screen, run the menu of kind
 * D_80059460 (then, after the title file screen, the disc check of the loaded
 * file) and tear down. */
void func_801C62A8(void) {
    u8 kind;

    func_801C5F10();
    func_801C7B0C();
    D_800625A0->drawing = 1;
    D_800625A0->sounds = 1;
    kind = D_80059460;
    switch (kind) {
    case 0:
        func_801D2D38();
        func_801C55A0();
        break;
    case 2:
        D_800594D0 = 0;
        func_801C58EC();
        D_800625A0->party->redraw9 = 0;
        D_800625A0->party->redraw4 = 0;
        D_800625A0->party->redraw3 = 0;
        switch (D_800594D0) {
        case 0:
            func_801C8694(0);
            break;
        case 2:
            func_801C8694(D_8006F008);
            break;
        }
        break;
    case 6:
        func_801C57A4();
        break;
    }
    func_801C5FE4();
}

/* Reset the card state and copy save title line D_8006EF64 of text file 1
 * into it. */
#ifdef NON_MATCHING
void func_801C6400(void) {
    s32 i;
    s32 j;
    u16 line;
    u8 *text;
    u8 *src;
    u8 c;

    for (i = 0; i < 2; i++) {
        D_800625A0->card->scanned[i] = 0;
        D_800625A0->card->unk4F8A[i] = 0;
        D_800625A0->card->unk4F8C[i] = 0xff;
    }
    D_800625A0->card->mode = 0;
    for (i = 0; i < 32; i++) {
        D_800625A0->card->files[i].state = 0;
        D_800625A0->card->fileSlots[i] = 0xff;
    }
    line = D_8006EF64;
    i = 0;
    func_80028470(0x10, 1);
    text = func_80031BDC(func_800288EC(1), 1);
    func_800295D8(1, text, 0, 0x80);
    func_80028A60(0);
    if (line != 0) {
        do {
        next:
            c = text[i];
            if (c >= 0x80) {
                i += 2;
                goto next;
            }
            if (c != '\n') {
                i += 1;
                goto next;
            }
            line--;
            i += 1;
        } while (line != 0);
    }
    src = text + i;
    for (j = 0; j < 30; j++) {
        D_800625A0->card->title[j] = *src++;
    }
    D_800625A0->card->unk501B = 0;
    D_800625A0->card->unk501A = 0;
    func_80028470(0x10, 0);
    func_800320E8(text);
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6400);
#endif
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C65F4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6AA0);

/* Mark the buffer being built as sent (the draw callback). */
void func_801C6D4C(void) {
    D_800625A0->bufferIndex = 0;
}

/* Clear the resource load state. */
void func_801C6D5C(void) {
    D_800625A0->unk4CC = 0;
    D_800625A0->unk4D0 = 0;
    D_800625A0->loadState = 0;
    D_800625A0->unk4D9 = 0;
    D_800625A0->unk4D4 = 0;
}

/* Upload a 16-entry palette at (0, 1c0) whose entry 1 is white. */
void func_801C6D90(void) {
    RECT rect;
    u8 reserved[8]; /* the original frame reserves 8 unused bytes */
    u16 *clut;

    clut = func_80031BDC(0x20, 0);
    bzero(clut, 0x20);
    clut[1] = 0x7fff;
    rect.x = 0;
    rect.y = 0x1c0;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, clut);
    DrawSync(0);
    func_800320E8(clut);
}

/* Set up the label text: the font position, the label pixel block and the
 * four label image records, and the label palette. */
void func_801C6E0C(void) {
    func_80033698(0, 0x1d1);
    D_800625A0->topLabels[0].pixels = func_80031BDC(0x38e, 0);
    func_801E7E68(D_800625A0->topLabels, D_801EA524, 0, 4);
    func_801C6D90();
}

/* Read the four sprite sheet records used by the menu. */
void func_801C6E68(void) {
    u8 reserved[40]; /* the original frame reserves 40 unused bytes */

    func_80026338(D_800625A0->sheet, 0xfe, &D_800625A0->sheetEntries[0][0], &D_800625A0->sheetEntries[0][1],
                  &D_800625A0->sheetEntries[0][2], &D_800625A0->sheetEntries[0][3],
                  &D_800625A0->sheetEntries[0][4], &D_800625A0->sheetEntries[0][5]);
    func_80026338(D_800625A0->sheet, 0x103, &D_800625A0->sheetEntries[1][0], &D_800625A0->sheetEntries[1][1],
                  &D_800625A0->sheetEntries[1][2], &D_800625A0->sheetEntries[1][3],
                  &D_800625A0->sheetEntries[1][4], &D_800625A0->sheetEntries[1][5]);
    func_80026338(D_800625A0->sheet, 0x100, &D_800625A0->sheetEntries[2][0], &D_800625A0->sheetEntries[2][1],
                  &D_800625A0->sheetEntries[2][2], &D_800625A0->sheetEntries[2][3],
                  &D_800625A0->sheetEntries[2][4], &D_800625A0->sheetEntries[2][5]);
    func_80026338(D_800625A0->sheet, 0x101, &D_800625A0->sheetEntries[3][0], &D_800625A0->sheetEntries[3][1],
                  &D_800625A0->sheetEntries[3][2], &D_800625A0->sheetEntries[3][3],
                  &D_800625A0->sheetEntries[3][4], &D_800625A0->sheetEntries[3][5]);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6F70);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C72BC);

/* Set up the screen: the copied screen area, the environments, labels,
 * palettes and sheet records, then the card state and load state. */
void func_801C7B0C(void) {
    MenuState *state = D_800625A0;

    state->screenImages->copy.x = 0x2c0;
    state->screenImages->copy.y = 0x100;
    state->screenImages->copy.w = 0x140;
    state->screenImages->copy.h = 0xe0;
    state->primitives->shade = 0x40;
    func_801C6AA0(state);
    func_801C6D4C();
    func_801C6E0C();
    func_801C6F70();
    func_801C6E68();
    switch (D_80059460) {
    case 2:
        D_800625A0->primitives->shade = 0x4c;
    case 0:
    case 6:
        func_801C6400();
        func_801C6D5C();
        break;
    }
}

/* One menu frame: poll the pads, swap and clear the buffers, build the frame
 * (updates, input, sounds), wait for the previous frame, show this one and copy
 * the screen area into the other buffer. */
void func_801C7BF4(void) {
    MenuBuffer *buffer;
    s32 other;

    if (*D_8005917C != -1) {
        /* ASPSX "break 1": code 1 in the upper code field (0x400 for maspsx) */
        __asm__ volatile("break 0x400");
    }
    func_801C7D78();
    if (D_801E9784 != 0) {
        func_80019CA0();
    }
    buffer = &D_800625A0->buffers[0];
    if (D_800625A0->current == buffer) {
        buffer = &D_800625A0->buffers[1];
    }
    D_800625A0->current = buffer;
    D_800625A0->bufferIndex = D_800625A0->bufferIndex == 0;
    ClearOTagR(D_800625A0->current->ot, 16);
    func_8001BD40(0, 0xff);
    func_801D1D40();
    D_800625A0->frameCounter++;
    func_801C7F34(D_80059488);
    func_801D2968();
    func_801D1CA0();
    other = D_800625A0->bufferIndex == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(D_800625A0->current->draw);
    PutDispEnv(D_800625A0->current->disp);
    MoveImage(&D_800625A0->screenImages->copy, 0, other * 0xe0);
    DrawOTag(&D_800625A0->current->ot[15]);
    func_801C8BEC();
    func_801C8EE8();
}

/* Decode this frame's input into D_800625A0->input (0-3 directions, 4-7 the
 * face buttons, 9/10 the shoulder buttons, 12 select, 8 none), playing the
 * cursor sounds; while the pad is disconnected, pause the sound and the play
 * time. */
void func_801C7D78(void) {
    u8 waiting;
    u8 paused;
    s32 frames;
    u8 input;

    waiting = 1;
    paused = 0;
    do {
        if (func_80035734(0) == 0) {
            if (!paused) {
                paused++;
                func_80037EE4();
                frames = D_80059488;
            }
        } else {
            waiting--;
            if (paused) {
                func_80037E8C();
                D_80059488 = frames;
            }
        }
    } while (waiting);
    input = 8;
    if (func_80036410() != 0) {
        func_80035DB0();
    } else {
        while (func_80035CDC() != 0) {
            if (D_800594A4 & 0x2000) {
                input = 0;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 0x4000) {
                input = 1;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 0x8000) {
                input = 2;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 0x1000) {
                input = 3;
                func_801C8574(1);
                break;
            }
            if (D_8005948C & 0x20) {
                input = 4;
                func_801C8574(2);
                break;
            }
            if (D_8005948C & 0x40) {
                input = 5;
                func_801C8574(3);
                break;
            }
            if (D_8005948C & 0x80) {
                input = 6;
                break;
            }
            if (D_8005948C & 0x10) {
                input = 7;
                break;
            }
            if (D_800594A4 & 4) {
                input = 10;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 8) {
                input = 9;
                func_801C8574(1);
                break;
            }
            if (D_8005948C & 0x100) {
                input = 12;
                break;
            }
        }
    }
    D_800625A0->input = input;
}

/* Split the play time `frames` into the digits of hhh:mm:ss (the hundreds,
 * tens and units of hours, then tens and units of minutes and seconds). */
void func_801C7F34(u32 frames) {
    D_800625A0->time[0] = frames / 21600000;
    frames %= 21600000;
    D_800625A0->time[1] = frames / 2160000;
    frames %= 2160000;
    D_800625A0->time[2] = frames / 216000;
    frames %= 216000;
    D_800625A0->time[3] = frames / 36000;
    frames %= 36000;
    D_800625A0->time[4] = frames / 3600;
    frames %= 3600;
    D_800625A0->time[5] = frames / 600;
    frames %= 600;
    D_800625A0->time[6] = frames / 60;
}

/* Split `value` into nine decimal digits, blanking (ff) the leading zeros. */
void func_801C80B8(u32 value) {
    u32 unit;
    s32 i;

    unit = 100000000;
    for (i = 0; i < 9; i++) {
        D_800625A0->digits[i] = value / unit;
        value %= unit;
        unit /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (D_800625A0->digits[i] != 0) {
            if (D_800625A0->digits[i - 1] == 0) {
                D_800625A0->digits[i - 1] = 0xff;
            }
            return;
        }
        D_800625A0->digits[i - 1] = 0xff;
    }
}

/* Initialise a gradient quad: the top edge colour (r, g, b), the bottom black. */
void func_801C8164(POLY_G4 *poly, u8 r, u8 g, u8 b) {
    SetPolyG4(poly);
    poly->r0 = r;
    poly->g0 = g;
    poly->b0 = b;
    poly->r1 = r;
    poly->g1 = g;
    poly->b1 = b;
    poly->r2 = 0;
    poly->g2 = 0;
    poly->b2 = 0;
    poly->r3 = 0;
    poly->g3 = 0;
    poly->b3 = 0;
}

/* Start mover `slot` along the line (x0, y0)-(x1, y1) at `speed` steps per
 * frame: the major axis steps one unit (8.8 fixed point), the minor axis its
 * slope. */
void func_801C81E0(s32 x0, s32 y0, s32 x1, s32 y1, u8 speed, u8 slot) {
    s32 dx;
    s32 dy;

    D_800625A0->movers[slot].x0 = x0;
    D_800625A0->movers[slot].y0 = y0;
    D_800625A0->movers[slot].x1 = x1;
    D_800625A0->movers[slot].y1 = y1;
    if (x1 < x0) {
        dx = x0 - x1;
        D_800625A0->movers[slot].negX = 1;
    } else {
        dx = x1 - x0;
        D_800625A0->movers[slot].negX = 0;
    }
    if (y1 < y0) {
        dy = y0 - y1;
        D_800625A0->movers[slot].negY = 1;
    } else {
        dy = y1 - y0;
        D_800625A0->movers[slot].negY = 0;
    }
    if (dx >= dy) {
        D_800625A0->movers[slot].stepX = 0x100;
        D_800625A0->movers[slot].stepY = (dy << 8) / dx;
    } else {
        D_800625A0->movers[slot].stepY = 0x100;
        D_800625A0->movers[slot].stepX = (dx << 8) / dy;
    }
    D_800625A0->movers[slot].speed = speed;
    D_800625A0->movers[slot].accX = 0;
    D_800625A0->movers[slot].accY = 0;
    D_800625A0->movers[slot].done = 0;
}

/* Advance mover `slot` by its speed and mark it done once the major axis
 * passes the end point. */
void func_801C8324(u8 slot) {
    s32 i;

    for (i = 0; i < D_800625A0->movers[slot].speed; i++) {
        if (D_800625A0->movers[slot].negX) {
            D_800625A0->movers[slot].accX -= D_800625A0->movers[slot].stepX;
        } else {
            D_800625A0->movers[slot].accX += D_800625A0->movers[slot].stepX;
        }
        if (D_800625A0->movers[slot].negY) {
            D_800625A0->movers[slot].accY -= D_800625A0->movers[slot].stepY;
        } else {
            D_800625A0->movers[slot].accY += D_800625A0->movers[slot].stepY;
        }
    }
    if (D_800625A0->movers[slot].stepX == 0x100) {
        if (D_800625A0->movers[slot].negX) {
            if (D_800625A0->movers[slot].accX / 256 + D_800625A0->movers[slot].x0 < D_800625A0->movers[slot].x1) {
                D_800625A0->movers[slot].done = 1;
            }
        } else if (D_800625A0->movers[slot].x1 < D_800625A0->movers[slot].accX / 256 + D_800625A0->movers[slot].x0) {
            D_800625A0->movers[slot].done = 1;
        }
    } else if (D_800625A0->movers[slot].negY) {
        if (D_800625A0->movers[slot].accY / 256 + D_800625A0->movers[slot].y0 < D_800625A0->movers[slot].y1) {
            D_800625A0->movers[slot].done = 1;
        }
    } else if (D_800625A0->movers[slot].y1 < D_800625A0->movers[slot].accY / 256 + D_800625A0->movers[slot].y0) {
        D_800625A0->movers[slot].done = 1;
    }
}

/* Set the four corners of a screen rectangle as vertices centred on (a0, 70). */
void func_801C851C(SVECTOR *v, u16 x, u16 y, u16 w, u16 h) {
    v[0].vx = x - 0xa0;
    v[0].vy = y - 0x70;
    v[0].vz = 0;
    v[1].vx = x + w - 0xa0;
    v[1].vy = y - 0x70;
    v[1].vz = 0;
    v[2].vx = x - 0xa0;
    v[2].vz = 0;
    v[3].vx = x + w - 0xa0;
    v[3].vz = 0;
    v[2].vy = y + h - 0x70;
    v[3].vy = y + h - 0x70;
}

/* Play menu sound effect `sound` from the menu's bank when sounds are on. */
void func_801C8574(s32 sound) {
    if (D_800625A0->sounds != 0) {
        func_80039DB8((D_800625A0->effectBank->id << 16) | (u8)sound, sound);
    }
}

/* Bit `bit` of the D_801E96C8 masks. */
u16 func_801C85C0(u8 bit) {
    return D_801E96C8[bit];
}

/* Bit `bit` of the D_801E96A8 masks. */
u16 func_801C85DC(u8 bit) {
    return D_801E96A8[bit];
}

/* All bits but `bit` (D_801E96C8). */
u16 func_801C85F8(u8 bit) {
    return ~D_801E96C8[bit];
}

/* All bits but `bit` (D_801E96A8). */
u16 func_801C861C(u8 bit) {
    return ~D_801E96A8[bit];
}

/* Test bit `bit` (D_801E96C8) of `flags`. */
u16 func_801C8640(u16 flags, u8 bit) {
    return D_801E96C8[bit] & flags;
}

/* Test bit `bit` (D_801E96A8) of `flags`. */
u16 func_801C865C(u16 flags, u8 bit) {
    return D_801E96A8[bit] & flags;
}

/* Test bit `bit` (D_801E96E8) of `flags`. */
u32 func_801C8678(u32 flags, u8 bit) {
    return flags & D_801E96E8[bit];
}

/* Ask for disc `disc` + 1 until it is in the drive, showing the change-disc
 * notice (and, for a wrong disc, the wrong-disc message for 29 frames). */
void func_801C8694(u8 disc) {
    u8 checking;
    u8 frames;

    func_801D1E80();
    checking = 1;
    while (D_800625A0->viewMotion != 0) {
        func_801C7BF4();
    }
    func_801D22F4(0);
    while (checking) {
        if (func_80028530() == disc + 1) {
            checking = 0;
        } else {
            func_801E92CC();
            func_801D2F4C(disc * 3 - 0x7d);
            if (func_801E93A0(disc + 1) != 0) {
                frames = 0x1d;
                func_801D32B4();
                func_801D2F4C(0x89);
                do {
                    frames--;
                    func_801C7BF4();
                } while (frames);
                func_801D32B4();
                func_801C7BF4();
            } else {
                func_801D32B4();
                checking = 0;
            }
        }
    }
    func_801D2484();
}

/* Discard pending memory-card events. */
void func_801C87C4(void) {
    func_800404C4(0xf4000001, 4);
    func_800404C4(0xf4000001, 0x8000);
    func_800404C4(0xf4000001, 0x100);
    func_800404C4(0xf4000001, 0x2000);
}

/* Wait for a memory-card event; returns 0 done, 1 error, 2 timeout, 3 new card. */
u8 func_801C881C(void) {
    for (;;) {
        if (func_80040494(D_800625A0->card->events[3]) == 1) {
            func_801C87C4();
            return 3;
        }
        if (func_80040494(D_800625A0->card->events[1]) == 1) {
            func_801C87C4();
            return 1;
        }
        if (func_80040494(D_800625A0->card->events[0]) == 1) {
            func_801C87C4();
            return 0;
        }
        if (func_80040494(D_800625A0->card->events[2]) == 1) {
            func_801C87C4();
            return 2;
        }
    }
}

/* Start a card check on `channel` and wait for its event: the D_801E9768
 * result for it, or -1 when the check cannot start. */
s32 func_801C891C(s32 channel) {
    if (func_8004E784(channel) == 0) {
        return -1;
    }
    return D_801E9768[func_801C881C()];
}

/* Finish a frame, then enable the four card events in a critical section. */
void func_801C8960(void) {
    func_801C7BF4();
    EnterCriticalSection();
    CloseEvent(D_800625A0->card->events[0]);
    CloseEvent(D_800625A0->card->events[1]);
    CloseEvent(D_800625A0->card->events[2]);
    CloseEvent(D_800625A0->card->events[3]);
    ExitCriticalSection();
}

/* Check the card in `port`; a removed card clears its file listing. Returns 0
 * when the check could not run (-2). */
u8 func_801C8A10(u8 port) {
    u8 ok;
    s32 result;
    s32 i;

    ok = 1;
    D_800625A0->card->present[port] = 1;
    result = func_801C891C(port ? 0x10 : 0);
    if (result == 0 && D_800625A0->card->result[port] == -1) {
        result = 1;
        D_800625A0->card->result[port] = 0;
    } else {
        D_800625A0->card->result[port] = result;
    }
    if (result == -1) {
        D_800625A0->card->unk4F8A[port] = 0;
        D_800625A0->card->present[port] = 0;
        D_801EA900[port] = 0;
        for (i = 0; i < 16; i++) {
            D_800625A0->card->fileSlots[port * 16 + i] = 0xff;
            D_800625A0->card->ours[port * 16 + i] = 0;
            D_800625A0->card->files[port * 16 + i].state = 0;
        }
    }
    if (result == -2) {
        ok = 0;
    }
    if (D_800625A0->card->presentShown[port] != D_800625A0->card->present[port]) {
        D_800625A0->card->scanned[port] = 0;
        D_800625A0->card->presentShown[port] = D_800625A0->card->present[port];
    }
    return ok;
}

/* While the card screen is up, recheck both ports every D_801E9779 frames. */
void func_801C8BEC(void) {
    if (D_800625A0->card->mode != 0) {
        if (++D_800625A0->cardPollTimer > D_801E9779) {
            func_801C8A10(0);
            func_801C8A10(1);
            if (D_800625A0->card->result[0] == -1 && D_800625A0->card->result[1] == -1) {
                D_800625A0->cardsPresent = 0;
            }
            D_800625A0->cardPollTimer = 0;
        }
    }
}

/* Unless port `port` could not be checked, show the card notice for 59 frames. */
void func_801C8CA4(u8 port) {
    MenuCard *card;
    s32 frames;

    card = D_800625A0->card;
    if (card->result[port] != -2) {
        card->mode = 2;
        frames = 59;
        do {
            frames--;
            func_801C7BF4();
        } while (frames != 0);
        D_800625A0->card->mode = 0;
    }
}

/* Unless port `port` could not be checked, wait 59 vertical blanks. */
void func_801C8D1C(u8 port) {
    s32 frames;

    if (D_800625A0->card->result[port] != -2) {
        frames = 59;
        do {
            VSync(0);
            frames--;
        } while (frames != 0);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8D78);

/* List the files of each port not yet scanned; in mode 1 a port with files
 * marks the cards present. */
void func_801C8EE8(void) {
    switch (D_800625A0->card->mode) {
    case 1:
        if (D_800625A0->card->scanned[0] == 0) {
            if (func_801C8D78(0)) {
                D_800625A0->cardsPresent = 1;
            }
            D_800625A0->card->scanned[0] = 1;
        }
        if (D_800625A0->card->scanned[1] == 0) {
            if (func_801C8D78(1)) {
                D_800625A0->cardsPresent = 1;
            }
            D_800625A0->card->scanned[1] = 1;
        }
        break;
    case 2:
        if (D_800625A0->card->scanned[0] == 0) {
            func_801C8D78(0);
            D_800625A0->card->scanned[0] = 1;
        }
        if (D_800625A0->card->scanned[1] == 0) {
            func_801C8D78(1);
            D_800625A0->card->scanned[1] = 1;
        }
        break;
    }
}

/* Read the 512-byte first block of card file `name` into `dst`; 0 on success,
 * -1 on failure. */
s32 func_801C9038(char *name, void *dst) {
    s32 fd;

    fd = open(name, 3);
    if (fd == -1) {
        return -1;
    }
    if (func_80040544(fd, dst, 0x200) != 0x200) {
        func_80040564(fd);
        return -1;
    }
    func_80040564(fd);
    return 0;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C90B0);

/* Mark the files of `port` whose names carry this game's prefix and note the
 * save slot each holds. */
void func_801C9270(s32 port) {
    s32 i;
    s32 j;
    s32 match;
    u8 *header;
    u8 *saves;

    for (i = 0; i < 16; i++) {
        D_800625A0->card->ours[port * 16 + i] = 0;
    }
    for (i = 0; i < 15; i++) {
        j = 0;
        match = 1;
        for (; j < 12; j++) {
            if (D_800625A0->card->files[D_800625A0->card->fileSlots[port * 16 + i]].name[j] !=
                D_800625A0->card->prefix[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            D_800625A0->card->ours[port * 16 + i] = 1;
            header = D_800625A0->card->headers[D_800625A0->card->fileSlots[port * 16 + i]];
            D_801EA6F4 = header + 0x100;
            saves = &D_801EA6D0[port * 16];
            saves[header[0x123]] = 1;
        }
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C93A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C9BCC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C9D34);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C9EF4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CA1D4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CA480);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CA5F0);

INCLUDE_RODATA(".local/decomp/slot39/asm/nonmatchings/slot39", D_801C50A8);

INCLUDE_RODATA(".local/decomp/slot39/asm/nonmatchings/slot39", D_801C50AC);

INCLUDE_RODATA(".local/decomp/slot39/asm/nonmatchings/slot39", D_801C50B0);

INCLUDE_RODATA(".local/decomp/slot39/asm/nonmatchings/slot39", D_801C50B4);

INCLUDE_RODATA(".local/decomp/slot39/asm/nonmatchings/slot39", D_801C50B8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CA750);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CA8C0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CAA38);

/* Show card message `message` and ask (801caa38 with `arg`); when answered
 * yes and a follow-up `confirm` is given (not ff), ask that too. Returns the
 * last answer. */
s32 func_801CACF8(u8 message, u8 confirm, u8 arg) {
    u8 answer;

    func_801D2F4C(message);
    D_800625A0->markers->visible[3] = 1;
    answer = func_801CAA38(arg);
    func_801D32B4();
    if (confirm != 0xff && answer) {
        func_801D2F4C(confirm);
        D_800625A0->markers->visible[3] = 1;
        answer = func_801CAA38(arg);
        func_801D32B4();
    }
    return answer;
}

/* Reset the file cursor: the first port with a card (port 2's slots start at 15). */
void func_801CADB0(void) {
    D_800625A0->card->unk4F80 = 0xff;
    D_800625A0->card->cursor = 0;
    if (D_800625A0->card->present[0] == 0 && D_800625A0->card->present[1] != 0) {
        D_800625A0->card->cursor = 15;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CAE08);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB184);

/* Apply a loaded save: its derived tables, play time and the 16 resident
 * words copied from the game data, then finish (801cb184). */
void func_801CB28C(s32 *save) {
    s32 i;

    func_801E4D10(save, D_800625A0->tables);
    D_80059488 = *save;
    for (i = 0; i < 16; i++) {
        D_8005A3A0[i] = D_8006F958[i];
    }
    func_801CB184();
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB304);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB8AC);

/* The save slot to use on `port`: `slot`, or for ff the first free one. */
#ifdef NON_MATCHING
u8 func_801CB9E8(u8 port, u8 slot) {
    s32 i;
    s32 found;
    u8 *used;

    found = 0;
    if (slot == 0xff) {
        i = 0;
        used = &D_801EA6D0[port * 16];
        for (; i < 15; i++) {
            if (*used == 0) {
                found = i;
                break;
            }
            used++;
        }
        return found;
    }
    return slot;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB9E8);
#endif

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CBA4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CBD90);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CC6D8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CD2AC);

/* Run the card command chosen on the file screen (0 copy?, 1 delete?, 2 the
 * save or load of the menu kind); returns 0 when the menu should close. */
u8 func_801CD710(u8 arg) {
    u8 stay;

    func_801D22F4(0);
    D_800625A0->party->unk2F = 0;
    stay = 1;
    switch (D_800625A0->choice) {
    case 0:
        D_800625A0->cardsPresent = 7;
        if (func_801CD2AC()) {
            stay = 0;
        }
        break;
    case 1:
        D_800625A0->cardsPresent = 6;
        if (func_801CC6D8()) {
            stay = 0;
        }
        break;
    case 2:
        if (D_80059460 != 2) {
            D_800625A0->cardsPresent = 3;
            if (func_801CBD90(arg)) {
                stay = 0;
            }
        } else {
            D_800625A0->cardsPresent = 2;
            if (func_801CB304()) {
                stay = 0;
            }
        }
        break;
    }
    func_801D2484();
    return stay;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CD81C);

/* Lay out character `ch`'s level digits (the last three of +62) at row `row`
 * of `panel` and prepare the +63 digits. */
void func_801CDB1C(MenuPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y) {
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[ch].unk62);
    panel->counts[0] = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            panel->counts[0] += func_8002675C(D_800625A0->sheet, digit, &panel->list1[panel->counts[0] * 2],
                                              D_800625A0->bufferIndex, i * 8 + x->base, row * 56 + y->base,
                                              0x1000);
        }
    }
    func_801C80B8(D_8006D8A0[ch].unk63);
    panel->counts[1] = 0;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CDC6C);

/* Build the parts of `panel` (801cd81c, 801cdb1c, 801cdc6c) and show it. */
void func_801CE0CC(MenuPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e) {
    func_801CD81C(panel, a, b, c, d, e);
    func_801CDB1C(panel, a, b, c, d);
    func_801CDC6C(panel, a, b, c, d, e);
    panel->shown = 1;
    panel->buffer = D_800625A0->bufferIndex;
}

/* Project `count` quads: each takes the next four of `verts` into every
 * other quad of `polys` from `index` and is added to the frame. */
void func_801CE198(s32 count, SVECTOR *verts, POLY_FT4 *polys, s32 first) {
    s32 p;
    s32 flag;
    s32 i;

    for (i = 0; i < count; i++) {
        RotTransPers4(&verts[i * 4], &verts[i * 4 + 1], &verts[i * 4 + 2], &verts[i * 4 + 3],
                      (s32 *)&polys[first + i * 2].x0, (s32 *)&polys[first + i * 2].x1,
                      (s32 *)&polys[first + i * 2].x2, (s32 *)&polys[first + i * 2].x3, &p, &flag);
        AddPrim(&D_800625A0->current->ot[4], &polys[first + i * 2]);
    }
}

/* Add `count` quads of `polys` to the frame, every other one from `first`. */
void func_801CE2B4(s32 count, POLY_FT4 *polys, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(&D_800625A0->current->ot[4], &polys[first + i * 2]);
    }
}

/* Add the shared mode primitive and, while panel 4 shows, its quad. */
void func_801CE338(void) {
    AddPrim(&D_800625A0->current->ot[4], D_800625A0->primitives->modes0[D_800625A0->primitives->mode]);
    if (D_800625A0->party->redraw4 != 0) {
        AddPrim(&D_800625A0->current->ot[4], &D_800625A0->primitives->polys[D_800625A0->primitives->frame]);
    }
}

/* While party flag +2f is set, add the current quad of each visible marker. */
void func_801CE3C8(void) {
    s32 i;

    if (D_800625A0->party->unk2F != 0) {
        for (i = 0; i < 4; i++) {
            if (D_800625A0->markers->visible[i] != 0) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->markers->polys[i * 2 + D_800625A0->markers->current[i]]);
            }
        }
    }
}

/* Draw the field menu blocks: the command list and its cursor (+340) and the
 * two lists of the second block (+344). */
void func_801CE464(void) {
    if (D_800625A0->party->redraw5 != 0) {
        func_801CE2B4(D_800625A0->fieldMenu->count, D_800625A0->fieldMenu->polys, D_800625A0->fieldMenu->start);
        AddPrim(&D_800625A0->current->ot[4], &D_800625A0->fieldMenu->cursor[D_800625A0->fieldMenu->start]);
    }
    if (D_800625A0->party->redraw6 != 0) {
        func_801CE2B4(7, D_800625A0->fieldMenu2->polys, D_800625A0->fieldMenu2->start);
        func_801CE2B4(4, D_800625A0->fieldMenu2->polys2, D_800625A0->fieldMenu2->start);
    }
}

/* Draw the shown field blocks: their two frame quads and part lists. */
void func_801CE540(void) {
    s32 i;
    MenuFieldBlock *block;

    for (i = 0; i < 3; i++) {
        block = D_800625A0->fieldBlocks[i];
        if (D_800625A0->party->fieldShown[i] != 0) {
            AddPrim(&D_800625A0->current->ot[4], &block->frameA[block->buffer]);
            AddPrim(&D_800625A0->current->ot[4], &block->frameB[block->buffer]);
            func_801CE2B4(block->count0, block->list0, block->buffer);
            func_801CE2B4(block->count1, block->list1, block->buffer);
            func_801CE2B4(block->count2, block->list2, block->buffer);
            func_801CE2B4(block->count3, block->list3, block->buffer);
            func_801CE2B4(block->count4, block->list4, block->buffer);
            func_801CE2B4(block->count5, block->list5, block->buffer);
            func_801CE2B4(block->count6, block->list6, block->buffer);
        }
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE660);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE860);

/* While party flag +8 is set, draw the sprites of the block at +35c. */
void func_801CEB5C(void) {
    MenuBlock35C *block;

    if (D_800625A0->party->unk8 != 0) {
        block = D_800625A0->block35C;
        func_801CE198(block->kind, block->verts, block->polys, block->buffer);
        func_801CE860();
    }
}

/* While party flag +4b is set, draw the visible labels of the block at +360. */
void func_801CEBB4(void) {
    s32 i;

    if (D_800625A0->party->unk4B != 0) {
        for (i = 0; i < 5; i++) {
            if (D_800625A0->labels360->visible[i] != 0) {
                func_801CE198(1, D_800625A0->labels360->labels[i].verts,
                              D_800625A0->labels360->labels[i].polys, D_800625A0->labels360->count);
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CEC40);

/* While party flag +a is set, draw the two sprite lists of the block at +354. */
void func_801CF308(void) {
    if (D_800625A0->party->redrawA != 0) {
        func_801CE2B4(D_800625A0->spriteLists->secondCount, D_800625A0->spriteLists->second,
                      D_800625A0->spriteLists->secondStart);
        func_801CE2B4(D_800625A0->spriteLists->firstCount, D_800625A0->spriteLists->first,
                      D_800625A0->spriteLists->firstStart);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF37C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF5E4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF8D8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CFB48);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CFF64);

/* Animate the file screen: its layers, the 15-frame x 6 blink and the
 * 4-step pulse between 4 and 128. */
void func_801D01D0(void) {
    func_801CF37C();
    func_801CF5E4(0, 0, 0x10);
    func_801CF5E4(0x10, 0x10, 0x20);
    func_801CFB48();
    func_801CF8D8();
    func_801CFF64();
    if (++D_800625A0->unk4D0 == 15) {
        D_800625A0->unk4D0 = 0;
        if (++D_800625A0->unk4CC == 6) {
            D_800625A0->unk4CC = 0;
        }
    }
    if (D_800625A0->unk4D9 == 0) {
        D_800625A0->unk4D4 += 4;
        if (D_800625A0->unk4D4 > 0x80) {
            D_800625A0->unk4D9 = 1;
            D_800625A0->unk4D4 = 0x7c;
        }
    } else {
        D_800625A0->unk4D4 -= 4;
        if (D_800625A0->unk4D4 < 0) {
            D_800625A0->unk4D9 = 0;
            D_800625A0->unk4D4 = 4;
        }
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D02D8);

/* Project the four vertices `v` into quad `index` of `polys` and add it at
 * depth `otz`. */
void func_801D0954(SVECTOR *v, POLY_FT4 *polys, s32 index, s32 otz) {
    s32 p;
    s32 flag;
    POLY_FT4 *poly;

    poly = &polys[index];
    RotTransPers4(&v[0], &v[1], &v[2], &v[3], (s32 *)&poly->x0, (s32 *)&poly->x1, (s32 *)&poly->x2,
                  (s32 *)&poly->x3, &p, &flag);
    AddPrim(&D_800625A0->current->ot[otz], poly);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D09F0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0C78);

/* Add the current quad of each top label whose party flag (+34) is set. */
void func_801D0D90(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->party->unk34[i] != 0) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->topLabels[i].polys[D_800625A0->topLabels[i].count]);
        }
    }
}

/* A short busy delay (eight iterations). */
void func_801D0E20(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
    }
}

/* Draw the sprites of labels 8-13 whose party flags (+14) and own flags are set. */
void func_801D0E38(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->party->unk14[i] != 0 && D_800625A0->partyLabels[i].visible != 0) {
            func_801CE198(1, D_800625A0->partyLabels[i].verts, D_800625A0->partyLabels[i].polys,
                          D_800625A0->partyLabels[i].count);
        }
    }
}

/* A short busy delay (six iterations). */
void func_801D0EBC(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
    }
}

/* Draw the sprites of labels 20-27 whose party flags (+38) are set. */
void func_801D0ED4(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->party->unk38[i] != 0) {
            func_801CE198(1, D_800625A0->labels10E0[i].verts, D_800625A0->labels10E0[i].polys,
                          D_800625A0->labels10E0[i].count);
        }
    }
}

/* Draw the sprites of labels 28-33 whose party flags (+40) are set. */
void func_801D0F54(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->party->unk40[i] != 0) {
            func_801CE198(1, D_800625A0->labels14E0[i].verts, D_800625A0->labels14E0[i].polys,
                          D_800625A0->labels14E0[i].count);
        }
    }
}

/* While party flag +4e is set, add label 17e0's current quad to the frame. */
void func_801D0FD4(void) {
    if (D_800625A0->party->unk4E != 0) {
        AddPrim(&D_800625A0->current->ot[4],
                      &D_800625A0->labels17E0[0].polys[D_800625A0->labels17E0[0].count]);
    }
}

/* While party flag +2e is set, draw the three notice labels (+1de0): their
 * sprites when visible, else their current quad. */
void func_801D1030(void) {
    s32 i;
    MenuLabelSlot *label;

    if (D_800625A0->party->unk2E != 0) {
        for (i = 0; i < 3; i++) {
            label = D_800625A0->blocks1DE0[i];
            if (label->visible != 0) {
                func_801CE198(1, label->verts, label->polys, label->count);
            } else {
                AddPrim(&D_800625A0->current->ot[4], &label->polys[label->count]);
            }
        }
    }
}

/* Draw the visible labels at +18e0 whose party flags (+54) are set. */
void func_801D10DC(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->party->unk54[i] != 0 && D_800625A0->labels18E0[i].visible != 0) {
            func_801CE198(1, D_800625A0->labels18E0[i].verts, D_800625A0->labels18E0[i].polys,
                          D_800625A0->labels18E0[i].count);
        }
    }
}

/* Add the current quad of each sound label whose party flag (+5c) is set. */
void func_801D1160(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->party->unk5C[i] != 0) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->soundLabels[i].polys[D_800625A0->soundLabels[i].count]);
        }
    }
}

/* Draw the label layers of the field menu screen. */
void func_801D11F0(void) {
    func_801D0D90();
    func_801D0E20();
    func_801D0E38();
    func_801D0EBC();
    func_801D10DC();
    func_801D1160();
    func_801D0ED4();
    func_801D0F54();
    func_801D0FD4();
    func_801D1030();
}

/* Add this buffer's fill and mode primitives to the frame. */
void func_801D1258(void) {
    AddPrim(&D_800625A0->current->ot[8], D_800625A0->primitives->fills[D_800625A0->bufferIndex]);
    AddPrim(&D_800625A0->current->ot[8], D_800625A0->primitives->modes[D_800625A0->bufferIndex]);
}

/* Draw `panel` when shown: its frame quads and part lists (and the extra list
 * when `extra`). */
void func_801D12D4(MenuPanel *panel, u8 extra) {
    if (panel->shown != 0) {
        AddPrim(&D_800625A0->current->ot[4], &panel->frameA[panel->buffer]);
        AddPrim(&D_800625A0->current->ot[4], &panel->frameB[panel->buffer]);
        func_801CE2B4(panel->count0, panel->list0, panel->buffer);
        func_801CE2B4(panel->counts[0], panel->list1, panel->buffer);
        func_801CE2B4(panel->counts[1], panel->list2, panel->buffer);
        func_801CE2B4(panel->counts[2], panel->list3, panel->buffer);
        func_801CE2B4(panel->counts[3], panel->list4, panel->buffer);
        func_801CE2B4(panel->counts[4], panel->list5, panel->buffer);
        func_801CE2B4(panel->counts[5], panel->list6, panel->buffer);
        if (extra) {
            func_801CE2B4(5, panel->extra, panel->buffer);
        }
    }
}

/* While party flag +46 is set, draw the three portrait panels (+1e08). */
void func_801D13F8(void) {
    s32 i;

    if (D_800625A0->party->unk46 != 0) {
        for (i = 0; i < 3; i++) {
            func_801D12D4(D_800625A0->panels[i], 1);
        }
    }
}

/* While party flag +49 is set, draw the sprites of the block at +43c. */
void func_801D1464(void) {
    MenuBlock43C *block;

    if (D_800625A0->party->unk49 != 0) {
        block = D_800625A0->block43C;
        func_801CE198(1, block->verts, block->polys, block->buffer);
    }
}

/* While party flag +53 is set, draw the sprites of the block at +440. */
void func_801D14B0(void) {
    MenuBlock440 *block;

    if (D_800625A0->party->unk53 != 0) {
        block = D_800625A0->block440;
        func_801CE198(4, block->verts, block->polys, block->buffer);
    }
}

/* While party flag +48 is set, draw the save/load screen list (+42c): each
 * shown row's name and value and, when shown, the three extra labels. */
void func_801D14FC(void) {
    s32 i;

    if (D_800625A0->party->unk48 != 0) {
        for (i = 0; i < 16; i++) {
            if (D_800625A0->block42C->shown[i] != 0) {
                func_801CE198(1, D_800625A0->block42C->names[i].verts, D_800625A0->block42C->names[i].polys,
                              D_800625A0->block42C->names[i].count);
                func_801CE198(1, D_800625A0->block42C->values[i].verts, D_800625A0->block42C->values[i].polys,
                              D_800625A0->block42C->values[i].count);
            }
        }
        if (D_800625A0->block42C->extraShown != 0) {
            func_801CE198(1, D_800625A0->block42C->extra[0].verts, D_800625A0->block42C->extra[0].polys,
                          D_800625A0->block42C->extra[0].count);
            func_801CE198(1, D_800625A0->block42C->extra[1].verts, D_800625A0->block42C->extra[1].polys,
                          D_800625A0->block42C->extra[1].count);
            func_801CE198(1, D_800625A0->block42C->extra[2].verts, D_800625A0->block42C->extra[2].polys,
                          D_800625A0->block42C->extra[2].count);
        }
    }
}

/* While party flag +4a is set, draw the file list (+430): each shown row's
 * name and value, when shown the header labels, and the footer. */
void func_801D1640(void) {
    s32 i;

    if (D_800625A0->party->unk4A != 0) {
        for (i = 0; i < 14; i++) {
            if (D_800625A0->block430->shown[i] != 0) {
                func_801CE198(1, D_800625A0->block430->names[i].verts, D_800625A0->block430->names[i].polys,
                              D_800625A0->block430->names[i].count);
                func_801CE198(1, D_800625A0->block430->values[i].verts, D_800625A0->block430->values[i].polys,
                              D_800625A0->block430->values[i].count);
            }
        }
        if (D_800625A0->block430->extraShown != 0) {
            func_801CE198(1, D_800625A0->block430->headA.verts, D_800625A0->block430->headA.polys,
                          D_800625A0->block430->headA.count);
            func_801CE198(1, D_800625A0->block430->headB.verts, D_800625A0->block430->headB.polys,
                          D_800625A0->block430->headB.count);
            for (i = 0; i < 2; i++) {
                func_801CE198(1, D_800625A0->block430->extra[i].verts, D_800625A0->block430->extra[i].polys,
                              D_800625A0->block430->extra[i].count);
            }
        }
        func_801CE198(1, D_800625A0->block430->footer.verts, D_800625A0->block430->footer.polys,
                      D_800625A0->block430->footer.count);
    }
}

/* While party flag +4c is set, draw the list of the block at +434: each shown
 * row's name and value, the title and, when shown, the three extra labels. */
void func_801D17C4(void) {
    s32 i;

    if (D_800625A0->party->unk4C != 0) {
        for (i = 0; i < 8; i++) {
            if (D_800625A0->block434->shown[i] != 0) {
                func_801CE198(1, D_800625A0->block434->names[i].verts, D_800625A0->block434->names[i].polys,
                              D_800625A0->block434->names[i].count);
                func_801CE198(1, D_800625A0->block434->values[i].verts, D_800625A0->block434->values[i].polys,
                              D_800625A0->block434->values[i].count);
            }
        }
        func_801CE198(1, D_800625A0->block434->title.verts, D_800625A0->block434->title.polys,
                      D_800625A0->block434->title.count);
        if (D_800625A0->block434->extraShown != 0) {
            for (i = 0; i < 3; i++) {
                func_801CE198(1, D_800625A0->block434->extra[i].verts, D_800625A0->block434->extra[i].polys,
                              D_800625A0->block434->extra[i].count);
            }
        }
    }
}

/* While party flag +4d is set, draw the status list (+438): each shown row's
 * name and value quads, the title, each row's parts and its gauge. */
void func_801D1914(void) {
    s32 i;

    if (D_800625A0->party->unk4D != 0) {
        for (i = 0; i < 13; i++) {
            if (D_800625A0->block438->shown[i] != 0) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->block438->names[i].polys[D_800625A0->block438->names[i].count]);
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->block438->values[i].polys[D_800625A0->block438->names[i].count]);
            }
        }
        func_801CE198(1, D_800625A0->block438->title.verts, D_800625A0->block438->title.polys,
                      D_800625A0->block438->title.count);
        for (i = 0; i < 13; i++) {
            func_801CE2B4(D_800625A0->block438->counts[i], D_800625A0->block438->lists[i],
                          D_800625A0->block438->starts[i]);
            if (D_800625A0->block438->gaugeShown[i] != 0) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->block438->gauges[i][D_800625A0->block438->gaugeBuffer[i]]);
            }
        }
    }
}

/* Draw the sprites of the two image blocks (+444) whose party flags are set. */
void func_801D1AAC(void) {
    s32 i;
    MenuImageBlock *block;

    for (i = 0; i < 2; i++) {
        if (D_800625A0->party->unk50[i] != 0) {
            block = D_800625A0->blocks444[i];
            func_801CE198(1, block->verts, block->polys, block->count);
        }
    }
}

/* Build the field menu screen, layer by layer. */
void func_801D1B20(void) {
    func_801D3B00();
    func_801D11F0();
    func_801CE3C8();
    func_801CE338();
    func_801D02D8();
    func_801D01D0();
    func_801CE540();
    func_801CE660();
    func_801CEB5C();
    func_801CEBB4();
    func_801CE464();
    func_801D13F8();
    func_801D1AAC();
    func_801D14FC();
    func_801D1640();
    func_801D17C4();
    func_801D1914();
    func_801D1464();
    func_801D14B0();
    func_801D0C78();
    func_801CEC40();
    func_801CF308();
}

/* Draw one layer set of the field menu screen. */
void func_801D1BE8(void) {
    func_801D3B00();
    func_801D11F0();
    func_801CE3C8();
    func_801CE338();
    func_801D02D8();
    func_801D01D0();
    func_801CEC40();
    func_801CF308();
    func_801D0C78();
}

/* Draw the other layer set of the field menu screen. */
void func_801D1C48(void) {
    func_801D3B00();
    func_801CE3C8();
    func_801D11F0();
    func_801CE338();
    func_801D02D8();
    func_801D01D0();
    func_801D0C78();
    func_801CF308();
}

/* Build the screen of the menu kind (when drawing), then the shared prims. */
void func_801D1CA0(void) {
    if (D_800625A0->drawing != 0) {
        switch (D_80059460) {
        case 0:
            func_801D1B20();
            break;
        case 2:
            func_801D1BE8();
            break;
        case 6:
            func_801D1C48();
            break;
        }
    }
    func_801D1258();
}

/* Step the view motion (3/4 start moving in/out, 1/2 move) and load the view
 * rotation and translation into the GTE. */
#ifdef NON_MATCHING
void func_801D1D40(void) {
    MenuState *state;
    u8 motion;
    s32 z;

    state = D_800625A0;
    switch (state->viewMotion) {
    case 4:
        state->viewOffset.vz = 0x200;
        motion = 2;
        goto start;
    case 3:
        state->viewOffset.vz = 0x800;
        motion = 1;
    start:
        state->viewAngles.vz = 0;
        state->viewAngles.vy = 0;
        state->viewAngles.vx = 0;
        state->viewOffset.vy = 0;
        state->viewOffset.vx = 0;
        state->viewMotion = motion;
        break;
    case 2:
        state->viewAngles.vy -= 0x60;
        state->viewOffset.vz = z = state->viewOffset.vz + 0x40;
        if (z >= 0xe00) {
            state->viewMotion = 0;
        }
        break;
    case 1:
        state->viewAngles.vx += 0x7c;
        state->viewOffset.vz = z = state->viewOffset.vz - 0x30;
        if (z < 0x200) {
            state->viewOffset.vz = 0x200;
            state->viewAngles.vz = 0;
            state->viewAngles.vx = 0;
            state->viewAngles.vy = 0;
            state->viewMotion = 0;
        }
        break;
    }
    func_8003F738(&D_800625A0->viewAngles, &D_800625A0->viewMatrix);
    TransMatrix(&D_800625A0->viewMatrix, &D_800625A0->viewOffset);
    SetRotMatrix(&D_800625A0->viewMatrix);
    SetTransMatrix(&D_800625A0->viewMatrix);
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1D40);
#endif

/* Start the view moving in (3) with its sound. */
void func_801D1E80(void) {
    D_800625A0->viewMotion = 3;
    func_801C8574(0x5b);
}

/* Start the view moving out (4) with its sound. */
void func_801D1EB0(void) {
    D_800625A0->viewMotion = 4;
    func_801C8574(0x5c);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1EE0);

/* Hide the party panels (redraw flags 3 and 4). */
void func_801D22C4(void) {
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
}

/* Lay out the screen markers for `mode`: 0 all four and the two extra flags,
 * 2 all four, 3 the first at the origin, 1 none. */
void func_801D22F4(u8 mode) {
    s32 i;

    D_800625A0->party->unk2F = 0;
    switch (mode) {
    case 0:
        D_800625A0->party->unk2F = 1;
        D_800625A0->markers->unk144[0] = 1;
        D_800625A0->markers->unk144[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sheet, 0x108, &D_800625A0->markers->polys[i * 2], D_800625A0->bufferIndex,
                          D_801E9A58[i], D_801E9A68[i], 0x800);
            D_800625A0->markers->current[i] = D_800625A0->bufferIndex;
        }
        break;
    case 3:
        func_8002675C(D_800625A0->sheet, 0x108, D_800625A0->markers->polys, D_800625A0->bufferIndex, 0, 0, 0x800);
        D_800625A0->markers->current[0] = D_800625A0->bufferIndex;
    case 1:
        break;
    }
}

/* Clear the party block flag at +2f. */
void func_801D2484(void) {
    D_800625A0->party->unk2F = 0;
}

/* Show (`show`) the six party labels, placing labels 3-5 as quads, or hide
 * the party name labels. */
#ifdef NON_MATCHING
void func_801D249C(u8 show) {
    s32 i;

    if (show) {
        func_801E7E68(D_800625A0->partyLabels, D_801EA534, 4, 6);
        for (i = 0; i < 3; i++) {
            func_801C851C(D_800625A0->partyLabels[3 + i].verts, D_801E9E4C[i][0], D_801E9E58[i][0],
                          D_800625A0->partyLabels[3 + i].width, 0xd);
            D_800625A0->partyLabels[3 + i].count = D_800625A0->bufferIndex;
            D_800625A0->partyLabels[3 + i].visible = 1;
            D_800625A0->party->unk14[3 + i] = 1;
        }
    } else {
        for (i = 0; i < 3; i++) {
            D_800625A0->party->unk14[i] = 0;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D249C);
#endif

/* Clear the party block's six bytes at +14. */
void func_801D25E4(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        D_800625A0->party->unk14[i] = 0;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D261C);

/* Draw the small window at (d4, b2) and its element at (d8, b6). */
void func_801D28A8(void) {
    func_801D397C(0, 0xd4, 0xb2, 0x60, 0x10, 0, 0, 4, 0);
    func_801D5BA4(0xd8, 0xb6);
}

/* Open the small window at (cc, c6) with its element and show it. */
void func_801D28FC(void) {
    func_801D397C(1, 0xcc, 0xc6, 0x50, 0x10, 0, 0, 4, 0);
    func_801D5CF8(0xd0, 0xca);
    D_800625A0->party->redraw6 = 1;
}

/* Draw the element at (d0, ca) while party flag 6 is set. */
void func_801D2968(void) {
    if (D_800625A0->party->redraw6 != 0) {
        func_801D5CF8(0xd0, 0xca);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D29A8);

/* Open the field menu: the two top portraits, then each party member's and
 * gear's name image, the command cursor, panels and money window. */
#ifdef NON_MATCHING
void func_801D2D38(void) {
    s32 i;
    s32 row;
    void *block;

    if (D_80059460 == 0) {
        for (row = 0; row < 2; row++) {
            block = func_80031BDC(0x720, 0);
            D_800625A0->portraits[row] = block;
            bzero(block, 0x720);
            block = func_80031BDC(0x18, 0);
            D_800625A0->portraitMarks[row] = block;
            bzero(block, 0x18);
            func_801E53CC(row);
        }
        func_801C8574(0x5e);
    }
    for (i = 0, row = 6; i < 3; i++, row += 2) {
        if (D_800625A0->party->ids[i] != 0xff) {
            func_801E8DA8(D_800625A0->party->ids[i], i * 2);
            func_801E8DA8(D_8006D8A0[D_800625A0->party->ids[i]].gear != 0xff
                              ? D_8006D8A0[D_800625A0->party->ids[i]].gear + 11
                              : 0xff,
                          row);
        }
    }
    func_801E8474(8, D_801EA19C);
    func_801D29A8(1, 0);
    func_801D28FC();
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2D38);
#endif

/* Refresh the item/status panels of `slot` for `mode` over two frames. */
void func_801D2EC0(u8 slot, u8 mode) {
    func_801D7C3C(slot, mode);
    func_801D7CFC(slot, mode, D_8006F8E5[slot]);
    func_801C7BF4();
    func_801D8DE4(slot, 0, 0, mode);
    func_801C7BF4();
    func_801D8EA4(slot, 0, 0, mode);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2F4C);

/* Close the notice when open (party +22): free portrait 2 and its four
 * blocks (+1de0), then finish a frame. */
void func_801D32B4(void) {
    s32 i;

    if (D_800625A0->party->unk20[2] != 0) {
        func_801D4EA0(2);
        D_800625A0->party->unk2E = 0;
        for (i = 0; i < 4; i++) {
            func_800320E8(D_800625A0->blocks1DE0[i]);
        }
    }
    func_801C7BF4();
}

/* Show the one-quad sprite (+43c; sheet image 107) at (x, y), `h` high. */
void func_801D3344(s32 x, s32 y, s32 h) {
    void *block;

    if (D_800625A0->party->unk49 == 0) {
        block = func_80031BDC(0x74, 0);
        D_800625A0->block43C = block;
        bzero(block, 0x74);
    }
    func_8002675C(D_800625A0->sheet, 0x107, D_800625A0->block43C, D_800625A0->bufferIndex, x, y, 0x1000);
    func_801C851C(D_800625A0->block43C->verts, x, y, 8, h);
    D_800625A0->block43C->buffer = D_800625A0->bufferIndex;
    D_800625A0->party->unk49 = 1;
}

/* Clear party flag +49 and free the block at state +43c. */
void func_801D3444(void) {
    D_800625A0->party->unk49 = 0;
    func_800320E8(D_800625A0->block43C);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3488);

/* Close the block at +440 when it is open (party +67), hiding its sprites. */
void func_801D3674(void) {
    MenuParty *party;

    party = D_800625A0->party;
    if (party->unk67 != 0) {
        party->unk53 = 0;
        D_800625A0->party->unk67 = 0;
        func_800320E8(D_800625A0->block440);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D36E0);

/* Open portrait window `index` at (x, y) of w x h: grow it in (`grow`) or
 * lay it out at once. Windows from 2 get their own blocks. */
void func_801D397C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 arg6, s32 arg7, u8 arg8) {
    MenuMark *mark;
    void *block;

    if (index >= 2) {
        block = func_80031BDC(0x720, 0);
        D_800625A0->portraits[index] = block;
        bzero(block, 0x720);
        block = func_80031BDC(0x18, 0);
        D_800625A0->portraitMarks[index] = block;
        bzero(block, 0x18);
        func_801E53CC(index);
    }
    mark = D_800625A0->portraitMarks[index];
    if (grow) {
        mark->image = index;
        mark->done = 0;
        mark->x = x;
        mark->y = y;
        mark->w = w;
        mark->h = h;
        mark->curW = 0;
        mark->curH = 0;
        D_800625A0->party->unk27[index] = 1;
        mark->unk12 = arg6;
        mark->unkC = arg7;
        return;
    }
    func_801D4D1C(index, x, y, w, h, arg6, arg7, arg8);
}

/* Grow each shown portrait mark by 32 per frame towards its full size
 * (centred), marking it done when both sides are full, and draw it. */
void func_801D3B00(void) {
    s32 i;
    MenuMark *mark;
    u8 full;

    for (i = 0; i < 7; i++) {
        mark = D_800625A0->portraitMarks[i];
        if (D_800625A0->party->unk27[i] != 0 && mark->done == 0) {
            full = 0;
            if (mark->curW + 0x20 >= mark->w) {
                mark->curW = mark->w;
                full = 1;
            } else {
                mark->curW = mark->curW + 0x20;
            }
            if (mark->curH + 0x20 >= mark->h) {
                mark->curH = mark->h;
                full++;
            } else {
                mark->curH = mark->curH + 0x20;
            }
            if (full == 2) {
                mark->done = 1;
            }
            func_801D4D1C(mark->image, mark->x + (mark->w >> 1) - (mark->curW >> 1),
                          mark->y + (mark->h >> 1) - (mark->curH >> 1), mark->curW, mark->curH, mark->unk12,
                          mark->unkC, mark->unk13);
        }
    }
}

/* Lay out portrait `slot`'s frame at (x, y), `h` high: the top, the flipped
 * bottom and the side pieces. */
void func_801D3C4C(u8 slot, u16 x, u16 y, s32 unused, u16 h) {
    MenuPortrait *portrait;

    portrait = D_800625A0->portraits[slot];
    func_8002675C(D_800625A0->sheet, 0x105, portrait->top, D_800625A0->bufferIndex, x, y, 0x1000);
    func_800263E4(D_800625A0->sheet, 0x105, portrait->bottom, D_800625A0->bufferIndex, x, y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sheet, 0x106, portrait->side, D_800625A0->bufferIndex, x, y + 8, 0x1000);
    func_801C851C(portrait->topVerts, x, y, 8, 8);
    func_801C851C(portrait->bottomVerts, x, y + h, 8, -8);
    func_801C851C(portrait->sideVerts, x, y + 8, 8, h - 8);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3DB0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3FF8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D433C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D4688);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D49D0);

/* Lay out portrait window `index` at (x, y) of w x h and show it. */
void func_801D4D1C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 arg5, s32 arg6, u8 frame) {
    MenuPortrait *portrait;

    portrait = D_800625A0->portraits[index];
    D_800625A0->party->unk20[index] = 0;
    func_801C851C(portrait->frameVerts, x, y, w, h);
    func_801D3DB0(index, x, y, w, h);
    func_801D3FF8(index, x, y, w);
    func_801D433C(index, x, y, w, h);
    func_801D4688(index, x, y, h);
    func_801D49D0(index, x, y, w, h);
    if (frame) {
        func_801D3C4C(index, x, y, w, h);
    }
    portrait->unk71D = frame;
    portrait->unk714 = arg5;
    portrait->unk718 = arg6;
    portrait->buffer = D_800625A0->bufferIndex;
    D_800625A0->party->unk20[index] = 1;
}

/* Hide and free portrait `slot`. */
void func_801D4EA0(u8 slot) {
    D_800625A0->party->unk20[slot] = 0;
    D_800625A0->party->unk27[slot] = 0;
    func_800320E8(D_800625A0->portraits[slot]);
    func_800320E8(D_800625A0->portraitMarks[slot]);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D4F2C);

/* Lay out the parts of field block `index` at (x, y) from the D_801EA34C
 * sheet images (ffff none). */
void func_801D50EC(u8 index, s32 x, s32 y) {
    MenuFieldBlock *block;
    s32 i;

    block = D_800625A0->fieldBlocks[index];
    block->count0 = 0;
    for (i = 0; i < 20; i++) {
        if (D_801EA34C[i] != 0xffff) {
            block->count0 += func_8002675C(D_800625A0->sheet, D_801EA34C[i], &block->list0[block->count0 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9A78[i], y + D_801E9AC8[i],
                                           0x1000);
        }
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D51EC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D53D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D55B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5794);

/* Lay out field block `index` (none for ff) at its mover's position in
 * `mode` and show it. */
void func_801D5A50(u8 index, u8 mode) {
    MenuFieldBlock *block;
    s32 x;
    s32 y;

    if (index != 0xff) {
        block = D_800625A0->fieldBlocks[index];
        x = D_800625A0->movers[index].accX / 256 + D_800625A0->movers[index].x0;
        y = D_800625A0->movers[index].accY / 256 + D_800625A0->movers[index].y0;
        func_801D4F2C(index, mode, x, y);
        func_801D50EC(index, x, y);
        func_801D51EC(index, mode, x, y);
        func_801D53D0(index, mode, x, y);
        func_801D55B4(index, mode, x, y);
        func_801D5794(index, mode, x, y);
        D_800625A0->party->fieldShown[index] = 1;
        block->buffer = D_800625A0->bufferIndex;
    }
}

/* Lay out the money (D_8006EF58) digits at (x, y) and its unit mark. */
void func_801D5BA4(s32 x, s32 y) {
    s32 i;
    s32 dx;
    u8 digit;

    func_801C80B8(D_8006EF58);
    i = 0;
    dx = x;
    D_800625A0->fieldMenu->count = 0;
    for (; i < 9; i++) {
        digit = D_800625A0->digits[i];
        if (digit != 0xff) {
            D_800625A0->fieldMenu->count +=
                func_8002675C(D_800625A0->sheet, digit, &D_800625A0->fieldMenu->polys[D_800625A0->fieldMenu->count * 2],
                              D_800625A0->bufferIndex, dx, y, 0x1000);
        }
        dx += 8;
    }
    func_8002675C(D_800625A0->sheet, 0x10, D_800625A0->fieldMenu->cursor, D_800625A0->bufferIndex, x + 0x50, y,
                  0x1000);
    D_800625A0->party->redraw5 = 1;
    D_800625A0->fieldMenu->start = D_800625A0->bufferIndex;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5CF8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5ED4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D6194);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D6338);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D680C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D6CF4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7154);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D74EC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7884);

/* Build the status panels of party slot `slot` for `mode` and show them. */
void func_801D7C3C(u8 slot, u8 mode) {
    func_801D5ED4(slot, mode);
    func_801D6194(mode);
    func_801D6338(slot, mode);
    func_801D680C(slot, mode);
    func_801D6CF4(slot, mode);
    func_801D74EC(slot, mode);
    func_801D7884(slot, mode);
    func_801D7154(slot, mode);
    D_800625A0->party->redraw7 = 1;
    D_800625A0->block358->buffer = D_800625A0->bufferIndex;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7CFC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7F50);

/* Initialise the two gradient quads of a gauge in colour `colour` (0 pink,
 * 1 green, 2 red, 3 blue), fading to black. */
void func_801D827C(POLY_G4 *polys, u8 colour) {
    u8 rgb[3];
    s32 i;

    switch (colour) {
    case 0:
        rgb[0] = 0xff;
        rgb[1] = 0x80;
        rgb[2] = 0x80;
        break;
    case 1:
        rgb[0] = 0x80;
        rgb[1] = 0xff;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0xff;
        rgb[1] = 0;
        rgb[2] = 0;
        break;
    case 3:
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0xff;
        break;
    }
    for (i = 0; i < 2; i++) {
        SetPolyG4(&polys[i]);
        polys[i].r0 = rgb[0];
        polys[i].g0 = rgb[1];
        polys[i].b0 = rgb[2];
        polys[i].r1 = rgb[0];
        polys[i].g1 = rgb[1];
        polys[i].b1 = rgb[2];
        polys[i].r2 = 0;
        polys[i].g2 = 0;
        polys[i].b2 = 0;
        polys[i].r3 = 0;
        polys[i].g3 = 0;
        polys[i].b3 = 0;
    }
}

/* Tint `count` quads of `polys` (every other one from `first`): 0 red,
 * 1 blue, 2 grey. */
void func_801D83AC(POLY_FT4 *polys, u8 colour, u8 count, u8 first) {
    u8 rgb[3];
    s32 i;
    POLY_FT4 *poly;

    rgb[1] = 0x40;
    switch (colour) {
    case 0:
        rgb[0] = 0x80;
        rgb[2] = 0x40;
        break;
    case 1:
        rgb[0] = 0x40;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0x40;
        rgb[2] = 0x40;
        break;
    }
    for (i = 0; i < count; i++) {
        poly = &polys[first + i * 2];
        SetShadeTex(poly, 0);
        poly->r0 = rgb[0];
        poly->g0 = rgb[1];
        poly->b0 = rgb[2];
    }
}

/* Set up a gauge moving from `from` to `to` of `max`: its two lengths (of
 * 64) and the rising or falling look. */
void func_801D84B4(u16 from, u16 to, s32 max) {
    s32 diff;

    D_801EA6FC = from;
    D_801EA700 = to;
    diff = to - from;
    D_801EA704 = diff;
    D_801EA708 = from * 100 / max * 0x1900 / 10000;
    if (diff >= 0) {
        D_801EA710 = 2;
        D_801EA714 = 0xe3;
    } else {
        D_801EA710 = 3;
        D_801EA714 = 0xe5;
        D_801EA704 = from - to;
    }
    D_801EA70C = D_801EA704 * 100 / max * 0x1900 / 10000;
}

/* The largest of the seven values in each of `a` and `b`. */
u16 func_801D85DC(s32 unused, u16 *a, u16 *b) {
    u16 max;
    s32 i;
    u8 reserved[24]; /* the original frame reserves 24 unused bytes */

    max = 0;
    for (i = 0; i < 7; i++) {
        if (max < *a) {
            max = *a;
        }
        a++;
    }
    for (i = 0; i < 7; i++) {
        if (max < *b) {
            max = *b;
        }
        b++;
    }
    return max;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D8644);

/* Build the equipment panels of `slot` at the upper or (`lower`) lower place. */
void func_801D8DE4(u8 slot, u8 lower, u8 arg2, u8 mode) {
    s32 x;
    s32 y;

    x = 0x98;
    y = 0x26;
    if (lower) {
        x = 0x80;
        y = 0x90;
    }
    func_801D7F50(x, y, mode);
    func_801D8644(slot, x, y, arg2, mode);
    D_800625A0->party->unk8 = 1;
    D_800625A0->block35C->buffer = D_800625A0->bufferIndex;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D8EA4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9704);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9808);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9B08);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9C84);

/* Leave the save/load screen: clear its images and listing, and close the
 * card events and handlers. */
void func_801D9E3C(void) {
    s32 i;

    D_800625A0->loadState = 0;
    func_801E64E0();
    func_801E5B3C();
    func_801E649C();
    for (i = 0; i < 32; i++) {
        D_800625A0->card->files[i].state = 0;
    }
    D_800625A0->card->unk4F8C[0] = 0xff;
    D_800625A0->card->unk4F8C[1] = 0xff;
    D_801EA900[1] = 0;
    D_801EA900[0] = 0;
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    CdSyncCallback(D_801EA718);
    CdReadyCallback(D_801EA71C);
    CdReadCallback(D_801EA720);
    ExitCriticalSection();
}

/* Lay out the six file screen command labels (save or title variant). */
void func_801D9F34(void) {
    if (D_80059460 != 2) {
        func_801E8018(6, D_800625A0->fileLabels, D_801EA53C, D_800625A0->party->unk1A);
    } else {
        func_801E8018(6, D_800625A0->fileLabels, D_801EA542, D_800625A0->party->unk1A);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9F98);

/* Open the save/load screen: its labels, its 1198-byte block and view 0. */
void func_801DA4A8(void) {
    void *block;

    func_801D22F4(2);
    func_801E8018(8, D_800625A0->labels10E0, D_801EA548, D_800625A0->party->unk38);
    block = func_80031BDC(0x1198, 0);
    D_800625A0->block42C = block;
    bzero(block, 0x1198);
    func_801C72BC(0);
}

/* Close the save/load screen: its labels, portraits 3-4, blocks and the
 * item table; view 10. */
void func_801DA518(void) {
    func_801D3444();
    func_801D4EA0(3);
    func_801D4EA0(4);
    D_800625A0->party->unk48 = 0;
    func_801C72BC(0x10);
    func_800320E8(D_800625A0->block42C->unk1180);
    func_800320E8(D_800625A0->block42C);
    func_800320E8(D_800625A0->tables->items);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA5BC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA9A8);

/* Allocate image block `index` (+444). */
void func_801DB02C(u8 index) {
    void *block;

    block = func_80031BDC(0x78, 0);
    D_800625A0->blocks444[index] = block;
    bzero(block, 0x78);
    D_800625A0->blocks444[index]->unk70 = 4;
    D_800625A0->blocks444[index]->unk74 = 0;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB0A8);

/* Free image block `index` (+444) and clear its party flag (+50). */
void func_801DB340(u8 index) {
    func_800320E8(D_800625A0->blocks444[index]);
    D_800625A0->party->unk50[index] = 0;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB39C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB5E4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB920);

/* Swap inventory entries `a` and `b` (item id and count). */
void func_801DBD4C(s32 a, s32 b) {
    u8 tmp;

    tmp = D_8006F65A[a];
    D_8006F65A[a] = D_8006F65A[b];
    D_8006F65A[b] = tmp;
    tmp = D_8006F5C4[a];
    D_8006F5C4[a] = D_8006F5C4[b];
    D_8006F5C4[b] = tmp;
}

/* Size the item list's scroll bar from the last occupied inventory entry. */
void func_801DBDB4(void) {
    s32 i;
    s32 last;
    s32 pages;

    for (i = 0; i < 150; i++) {
        if (D_8006F65A[i] != 0) {
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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DBE54);

/* Open the file list screen of `kind` (0 load, 1 save, 2 the other labels). */
void func_801DC1D4(u8 kind) {
    void *block;
    u8 view;
    s32 page;

    page = 0;
    block = func_80031BDC(0x1094, 0);
    D_800625A0->block430 = block;
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
    func_801E8018(8, D_800625A0->labels10E0, D_801EA548 + page * 8, D_800625A0->party->unk38);
    func_801C72BC(view);
    func_801D22F4(2);
    func_801D3488(2, kind);
}

/* Close the file list screen of `kind`: portraits 3-6, its block; back to the
 * view of the kind (| 10). */
void func_801DC2CC(u8 kind) {
    u8 view;

    func_801D4EA0(3);
    func_801D4EA0(4);
    func_801D4EA0(5);
    func_801D4EA0(6);
    func_801D2484();
    D_800625A0->party->unk4A = 0;
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
    func_800320E8(D_800625A0->block430);
    D_800625A0->party->redraw6 = 1;
    D_800625A0->party->unk20[1] = 1;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DC3D8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DCE60);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DD5E8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DD790);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DDF24);

/* Run the 801ddf24 screen for party slot `slot`; always continues the menu. */
u8 func_801DE29C(u8 slot, u8 arg1) {
    func_801DDF24(slot, arg1, 0);
    return 1;
}

/* Open the 801ddf24 screen on page `page`: its block, labels and view 7. */
void func_801DE2C8(u8 page) {
    void *block;

    block = func_80031BDC(0xa1c, 0);
    D_800625A0->block434 = block;
    bzero(block, 0xa1c);
    func_801E8018(6, D_800625A0->labels14E0, D_801EA558 + page * 6, D_800625A0->party->unk40);
    func_801C72BC(7);
    func_801D22F4(3);
    func_801DB02C(0);
    func_801D3488(1, page);
}

/* Close the 801ddf24 screen: portraits 2-5, its labels and block (+434); view 17. */
void func_801DE36C(void) {
    D_800625A0->party->unk4C = 0;
    func_801D4EA0(2);
    func_801D4EA0(3);
    func_801D4EA0(4);
    func_801D4EA0(5);
    func_801C7BF4();
    func_801E8044(6, D_800625A0->party->unk40);
    func_801C72BC(0x17);
    func_800320E8(D_800625A0->block434);
    func_801D3444();
}

/* Close the 801ddf24 screen: hide its sprites and free its blocks. */
void func_801DE400(void) {
    D_800625A0->party->unk8 = 0;
    D_800625A0->party->unk4B = 0;
    func_801C7BF4();
    func_800320E8(D_800625A0->block35C);
    func_800320E8(D_800625A0->labels360);
}

/* Lay out the page `page` labels of the 801ddf24 screen (rows 2-5 when
 * `wide`, else 0-1) and its window. */
void func_801DE474(u8 wide, u8 page) {
    s32 i;
    s32 first;
    s32 end;
    s32 h;

    for (i = 0; i < 6; i++) {
        D_800625A0->party->unk40[i] = 0;
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
        func_801E8070(6, D_800625A0->labels14E0, D_801EA558 + page * 6, D_801E9EA0, D_800625A0->party->unk40, i,
                      i, 3);
    }
    if (D_800625A0->party->unk20[4] != 0) {
        func_801D4EA0(4);
    }
    func_801D397C(4, 0x10, 0xc, 0x80, h, 0, 1, 4, 0);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE5CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF0D4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF5D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF890);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DFB68);

/* Compute party slot `slot`'s gear stats and copy them to the shown values. */
void func_801DFE2C(u8 slot) {
    func_801E3ECC(D_800625A0->tables, D_8006D8A0[D_800625A0->party->ids[slot]].gear);
    func_801E3C2C(D_800625A0->tables, D_8006D8A0[D_800625A0->party->ids[slot]].gear);
    D_800625A0->tables->shown[0] = D_800625A0->tables->unkB0;
    D_800625A0->tables->shown[1] = D_800625A0->tables->unkA4;
    D_800625A0->tables->shown[2] = D_800625A0->tables->unkA6;
    D_800625A0->tables->shown[3] = D_800625A0->tables->unkB2;
    D_800625A0->tables->shown[4] = D_800625A0->tables->unkB3;
    D_800625A0->tables->shown[5] = D_800625A0->tables->unkB4;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DFF5C);

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
        ids = D_8006F3D0;
        counts = ids - 100;
        item = D_8006D8A0[D_800625A0->party->ids[slot]].accessory;
        D_8006D8A0[D_800625A0->party->ids[slot]].accessory = 0;
        size = 100;
    } else {
        ids = D_8006F754;
        counts = ids - 100;
        item = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].part;
        size = 100;
        D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].part = 0;
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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E05D0);

/* Run the 801e05d0 screen for party slot `slot` with its blocks; views 3 and 13. */
u8 func_801E0F78(u8 slot, u8 arg1) {
    void *block;

    block = func_80031BDC(0x32f4, 0);
    D_800625A0->block35C = block;
    bzero(block, 0x32f4);
    block = func_80031BDC(0x2ac, 0);
    D_800625A0->labels360 = block;
    bzero(block, 0x2ac);
    func_801C72BC(3);
    func_801E05D0(slot, arg1, 0);
    func_801C72BC(0x13);
    return 1;
}

/* Open the 801e1544 screen: its block (+438), labels and window, the party
 * panel when two or more members can take part, and the green gauges. */
#ifdef NON_MATCHING
void func_801E1014(void) {
    MenuBlock438 *block;
    s32 i;
    s32 j;

    block = func_80031BDC(0x25c0, 0);
    D_800625A0->block438 = block;
    bzero(block, 0x25c0);
    func_801C72BC(4);
    func_801E8018(2, D_800625A0->labels17E0, D_801EA564, &D_800625A0->party->unk4E);
    func_801E8070(2, D_800625A0->labels17E0, D_801EA564, D_801E9EA0, &D_800625A0->party->unk4E, 0, 0, 4);
    func_801D397C(2, 0x44, 0xa, 0xe4, 0xc4, 0, 1, 4, 0);
    D_800625A0->party->redraw6 = 0;
    j = 0; /* members that can take part */
    D_800625A0->party->unk20[1] = 0;
    for (i = 0; i < 3; i++) {
        if (D_800625A0->party->ids[i] != 0xff) {
            if (D_800625A0->party->ids[i] != 7 && D_800625A0->party->ids[i] != 8) {
                j++;
            }
        }
    }
    if (j >= 2) {
        func_801D3488(3, 0);
    } else {
        D_800625A0->party->unk53 = 0;
    }
    for (i = 0; i < 13; i++) {
        j = 0;
        do {
            SetPolyG4(&D_800625A0->block438->gauges[i][j]);
            D_800625A0->block438->gauges[i][j].r0 = 0;
            D_800625A0->block438->gauges[i][j].g0 = 0xff;
            D_800625A0->block438->gauges[i][j].b0 = 0;
            D_800625A0->block438->gauges[i][j].r1 = 0;
            D_800625A0->block438->gauges[i][j].g1 = 0xff;
            D_800625A0->block438->gauges[i][j].b1 = 0;
            D_800625A0->block438->gauges[i][j].r2 = 0;
            D_800625A0->block438->gauges[i][j].g2 = 0;
            D_800625A0->block438->gauges[i][j].b2 = 0;
            D_800625A0->block438->gauges[i][j].r3 = 0;
            D_800625A0->block438->gauges[i][j].g3 = 0;
            D_800625A0->block438->gauges[i][j].b3 = 0;
            j++;
        } while (j < 2);
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1014);
#endif

/* Close the 801e1544 screen: its sprites, labels and block (+438); view 14. */
void func_801E1398(void) {
    func_801D3674();
    D_800625A0->party->unk4D = 0;
    func_801D4EA0(2);
    func_801C7BF4();
    func_801E8044(2, &D_800625A0->party->unk4E);
    func_801C72BC(0x14);
    func_800320E8(D_800625A0->block438);
}

/* Return party slot `slot`'s average completion (percent, each capped at
 * 100) of the seven targets of row `row` of its table (+438 +2578), counting
 * entries where either side is set; rows from 7 need game flag 4000. */
#ifdef NON_MATCHING
u32 func_801E1418(u8 slot, u8 row) {
    u32 sum;
    s32 i;
    u32 count;
    s32 id;
    u16 *values;
    u8 *table;
    u16 value;
    u16 target;
    u32 percent;

    sum = 0;
    if (row < 7 || (D_8006F8EA & 0x4000)) {
        count = 0;
        id = D_800625A0->party->ids[slot];
        table = D_800625A0->block438->unk2578;
        values = D_8006D8A0[id].unk90;
        for (i = 0; i < 7; i++, values++) {
            value = *values;
            target = *(u16 *)(table + id * 0x110 + row * 14 + i * 2);
            if (value != 0) {
                if (target != 0) {
                    if (target != 0xffff) {
                        percent = value * 100 / target;
                        if (percent >= 100) {
                            sum += 100;
                        } else {
                            sum += percent;
                        }
                    }
                    count++;
                }
            } else if (target != 0) {
                count++;
            }
        }
        if (count != 0) {
            return sum / count;
        }
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1418);
#endif

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1544);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1AC8);

/* The 801e1544 screen for party slot `slot`: members 7 and 8 are refused
 * (sound 4); otherwise show the slot's page, switching members (9 previous,
 * 10 next, skipping 7 and 8) until cancelled. */
u8 func_801E20C8(u8 slot) {
    u8 stay;
    u8 shown;

    stay = 1;
    shown = 0xff;
    if ((u32)(D_800625A0->party->ids[slot] - 7) < 2) {
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
            } while ((u32)(D_800625A0->party->ids[slot] - 7) < 2);
            break;
        case 10:
            do {
                slot = func_801D9704(slot, 1, 0);
            } while ((u32)(D_800625A0->party->ids[slot] - 7) < 2);
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
    D_800625A0->block358 = block;
    bzero(block, 0x2af0);
    block = func_80031BDC(0x32f4, 0);
    D_800625A0->block35C = block;
    bzero(block, 0x32f4);
    block = func_80031BDC(0x2ac, 0);
    D_800625A0->labels360 = block;
    bzero(block, 0x2ac);
    func_801C72BC(3);
    i = 0;
    while (1) {
        if (D_800625A0->party->ready[i] != 0) {
            break;
        }
        i++;
    }
    func_801D3488(0, 1);
    return i;
}

/* Lay out the six labels of page `page` (D_801EA568) at +18e0. */
void func_801E2324(u8 page) {
    func_801E8018(6, D_800625A0->labels18E0, D_801EA568 + page, D_800625A0->party->unk54);
}

/* Free the three screen blocks (+358, +35c, +360) and restore the view (13). */
void func_801E2368(void) {
    func_800320E8(D_800625A0->block358);
    func_800320E8(D_800625A0->block35C);
    func_800320E8(D_800625A0->labels360);
    func_801C72BC(0x13);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E23CC);

/* Open the 801d3488 screen: its three blocks and view 3. */
void func_801E2AE0(void) {
    void *block;

    func_801D249C(1);
    block = func_80031BDC(0x2af0, 0);
    D_800625A0->block358 = block;
    bzero(block, 0x2af0);
    block = func_80031BDC(0x32f4, 0);
    D_800625A0->block35C = block;
    bzero(block, 0x32f4);
    block = func_80031BDC(0x2ac, 0);
    D_800625A0->labels360 = block;
    bzero(block, 0x2ac);
    func_801C72BC(3);
    func_801D3488(0, 0);
}

/* Free the three screen blocks (+358, +35c, +360) and restore the view (13). */
void func_801E2B80(void) {
    func_800320E8(D_800625A0->block358);
    func_800320E8(D_800625A0->block35C);
    func_800320E8(D_800625A0->labels360);
    func_801C72BC(0x13);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2BE4);

/* Close the screen of the command at `offset` past the top cursor. */
#ifdef NON_MATCHING
void func_801E3088(u8 offset) {
    switch (D_800625A0->cursor + offset) {
    case 1:
    case 8:
        func_801D9E3C();
        break;
    case 2:
        D_800625A0->party->redraw7 = 0;
        D_800625A0->party->unk8 = 0;
        D_800625A0->party->unk4B = 0;
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
        D_800625A0->party->redraw7 = 0;
        D_800625A0->party->unk8 = 0;
        D_800625A0->party->unk4B = 0;
        func_801D25E4();
        func_801E2B80();
        break;
    case 0:
    case 7:
    case 9:
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E3088);
#endif

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E31C0);

/* Apply `user`'s restoring effect: to `target`'s HP (its +5b times the
 * effect's +11, capped at the maximum), or with `gear` to the user's gear
 * (+60 up by a tenth of +64, capped at +64). */
void func_801E35BC(MenuTables *tables, u8 user, u8 target, u8 effect, u8 gear) {
    CharRecord *source;
    CharRecord *dest;
    GearRecord *machine;
    MenuEffect *record;

    source = &D_8006D8A0[user];
    dest = &D_8006D8A0[target];
    machine = (GearRecord *)&D_8006D8A0[D_8006D8A0[user].gear + 11];
    if (!gear) {
        record = tables->effects[user];
        record += effect;
        dest->hp += source->unk5B * record->unk11;
        if (dest->hp > dest->hpMax) {
            dest->hp = dest->hpMax;
        }
    } else {
        machine->unk60 += machine->unk64 / 10;
        if (machine->unk64 < machine->unk60) {
            machine->unk60 = machine->unk64;
        }
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E36D4);

/* Compute character `id`'s shown stats: base values plus equipment bonuses
 * (the first from the level, scaled 6/10 with +1c for kind 4), capped at
 * 250, 99 or 16. */
void func_801E3A80(MenuTables *tables, u8 id) {
    CharRecord *chara;

    chara = &D_8006D8A0[id];
    if (chara->unk56 == 4) {
        tables->shown[0] = (chara->level + chara->unk1C) * 6 / 10;
    } else {
        tables->shown[0] = chara->level + (chara->unk58 + chara->bonus[0]);
    }
    tables->shown[1] = chara->unk5E + chara->bonus[6];
    tables->shown[2] = chara->bonus[5] + (chara->unk59 + chara->bonus[1]);
    tables->shown[3] = chara->unk5F + chara->bonus[7];
    tables->shown[4] = chara->unk5B + chara->bonus[3];
    tables->shown[5] = chara->unk5C + chara->bonus[4];
    tables->unkC4 = chara->unk5A + chara->bonus[2];
    if (tables->shown[0] >= 251) {
        tables->shown[0] = 250;
    }
    if (tables->shown[1] >= 100) {
        tables->shown[1] = 99;
    }
    if (tables->shown[2] >= 251) {
        tables->shown[2] = 250;
    }
    if (tables->shown[3] >= 100) {
        tables->shown[3] = 99;
    }
    if (tables->shown[4] >= 251) {
        tables->shown[4] = 250;
    }
    if (tables->shown[5] >= 251) {
        tables->shown[5] = 250;
    }
    if (tables->unkC4 >= 21) {
        tables->unkC4 = 16;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E3C2C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E3ECC);

/* Recompute gear `gear`'s derived values from the data tables. */
void func_801E4170(MenuTables *tables, u8 gear) {
    func_801E41C0(tables, gear);
    func_801E42AC(tables, gear);
    func_801E4258(tables, gear);
}

/* Take gear `gear`'s engine values from the tables, keeping +60 within +64. */
void func_801E41C0(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearEngine *engine;

    record = &D_8006DFAC[gear];
    engine = tables->engines;
    engine += record->engine;
    record->unk60 = engine->unk4;
    record->unk64 = engine->unk4;
    record->unk98 = engine->unk14;
    record->unk9E = engine->unk15;
    record->unk9D = engine->unk16;
    record->unk9F = engine->unk17;
    if (record->unk64 < record->unk60) {
        record->unk60 = record->unk64;
    }
}

/* Copy gear `gear`'s two frame values (+8, +a of its frame record) into the
 * gear record (+70, +72). */
void func_801E4258(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearFrame *frame;

    record = &D_8006DFAC[gear];
    frame = tables->frames;
    frame += record->frame;
    record->unk70 = frame->unk8;
    record->unk72 = frame->unkA;
}

/* Take gear `gear`'s part values from the tables, keeping +38 within +3a. */
void func_801E42AC(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearPart *part;

    record = &D_8006DFAC[gear];
    part = tables->parts;
    part += record->unk3;
    record->unk3A = part->unk6;
    record->unk3C = part->unkC;
    record->unk3D = part->unkD;
    record->unk3E = part->unkE;
    record->unk3F = part->unkE;
    if (record->unk38 > record->unk3A) {
        record->unk38 = record->unk3A;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E433C);

/* Take gear `gear`'s weapon values (table +18) into its first slot and
 * attributes; gears 5 and 13 instead take their three slot weapons. */
void func_801E4754(MenuTables *tables, u8 gear) {
    GearRecord *record;
    GearWeapon *weapon;

    record = &D_8006DFAC[gear];
    weapon = tables->weapons18;
    weapon += record->weapon;
    record->slots[0].unk2 = weapon->unkE;
    record->slots[0].value = weapon->unk12;
    record->slots[0].unk3 = weapon->unk10;
    record->slots[0].unk4 = weapon->unk11;
    record->attrs[0] = weapon->attrs[0];
    record->attrs[1] = weapon->attrs[1];
    record->attrs[2] = weapon->attrs[2];
    record->attrs[3] = weapon->attrs[3];
    if (record->slots[0].unk4 == 100) {
        record->unk86 &= 0xfff;
        record->unk86 |= record->slots[0].value;
    }
    if (gear == 5 || gear == 13) {
        weapon = tables->weapons18;
        weapon += record->part;
        record->slots[0].unk2 = weapon->unkE;
        record->slots[0].value = weapon->unk12;
        record->slots[0].unk3 = weapon->unk10;
        record->slots[0].unk4 = weapon->unk11;
        record->attrs[1] = weapon->attrs[1];
        weapon = tables->weapons18;
        weapon += record->part1;
        record->slots[1].unk2 = weapon->unkE;
        record->slots[1].value = weapon->unk12;
        record->slots[1].unk3 = weapon->unk10;
        record->slots[1].unk4 = weapon->unk11;
        record->attrs[2] = weapon->attrs[2];
        weapon = tables->weapons18;
        weapon += record->part2;
        record->slots[2].unk2 = weapon->unkE;
        record->slots[2].value = weapon->unk12;
        record->slots[2].unk3 = weapon->unk10;
        record->slots[2].unk4 = weapon->unk11;
        record->attrs[3] = weapon->attrs[3];
    }
}

/* Gear `gear`'s value: (its +44 / 120 - its +75) / 2, at least 0. */
u8 func_801E4928(u8 gear) {
    GearRecord *record;
    s16 value;

    record = &D_8006DFAC[gear];
    value = ((u16)(record->unk44 / 120) - record->unk75) / 2;
    if (value < 0) {
        value = 0;
    }
    return value;
}

/* Set view `gear`'s value to 2/90 of the gear's +64, in steps of ten. */
#ifdef NON_MATCHING
void func_801E4998(MenuGearViews *views, u8 gear) {
    MenuGearValue *value;

    value = &views->views[gear]->value;
    value->unk24 = D_8006DFAC[gear].unk64 / 10 * 2 / 9;
    value->unk24 = value->unk24 / 10 * 10;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4998);
#endif

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4A28);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4D10);

/* Debug: put ten of every entry into the five inventory lists. */
void func_801E5058(void) {
    u8 i;

    for (i = 1; i < 0x48; i++) {
        D_8006F3D0[i] = i;
        D_8006F36C[i] = 10;
    }
    for (i = 1; i < 0x96; i++) {
        D_8006F4FC[i] = i;
        D_8006F434[i] = 10;
    }
    for (i = 1; i < 0x4c; i++) {
        D_8006F65C[i] = i;
        D_8006F5C6[i] = 10;
    }
    for (i = 1; i < 0x48; i++) {
        D_8006F754[i] = i;
        D_8006F6F0[i] = 10;
    }
    for (i = 1; i < 0x69; i++) {
        D_8006F84E[i] = i;
        D_8006F7B8[i] = 10;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5178);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E53CC);

/* Set up image block `index`'s 16x16 sprite and semi-transparent cover at
 * its position for both buffers, and the two draw modes (blend mode 2). */
void func_801E56E8(s32 index) {
    MenuImage *image;
    u16 *x;
    u16 *y;
    s32 i;
    RECT window;

    i = 0;
    x = D_801E9894[index];
    y = D_801E9914[index];
    image = D_800625A0->images[index];
    for (; i < 2; i++) {
        func_801E927C(&image->polys[i]);
        image->polys[i].x0 = *x;
        image->polys[i].y0 = *y;
        image->polys[i].x1 = *x + 0x10;
        image->polys[i].y1 = *y;
        image->polys[i].x2 = *x;
        image->polys[i].y2 = *y + 0x10;
        image->polys[i].x3 = *x + 0x10;
        image->polys[i].y3 = *y + 0x10;
        image->polys[i].tpage = GetTPage(0, 0, 0x140, 0x80);
        SetPolyF4(&image->shade[i]);
        image->shade[i].r0 = 0x80;
        image->shade[i].g0 = 0x80;
        image->shade[i].b0 = 0x80;
        SetSemiTrans(&image->shade[i], 1);
        image->shade[i].x0 = *x;
        image->shade[i].y0 = *y;
        image->shade[i].x1 = *x + 0x10;
        image->shade[i].y1 = *y;
        image->shade[i].x2 = *x;
        image->shade[i].y2 = *y + 0x10;
        image->shade[i].x3 = *x + 0x10;
        image->shade[i].y3 = *y + 0x10;
        func_801C851C(image->shadeVerts, *x, *y, 0x10, 0x10);
    }
    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    SetDrawMode(&image->modes[0], 0, 0, GetTPage(0, 2, 0x140, 0x80), &window);
    SetDrawMode(&image->modes[1], 0, 0, GetTPage(0, 2, 0x140, 0x80), &window);
}

/* Set up image block `index`'s green 16x16 frame at its position: the
 * top/right and left/bottom lines and their vertices, for both buffers. */
void func_801E5924(s32 index) {
    MenuImage *image;
    u16 *x;
    u16 *y;
    s32 i;

    x = D_801E9894[index];
    y = D_801E9914[index];
    image = D_800625A0->images[index];
    for (i = 0; i < 2; i++) {
        SetLineF3(&image->top[i]);
        image->top[i].r0 = 0;
        image->top[i].g0 = 0xff;
        image->top[i].b0 = 0;
        image->top[i].x0 = *x;
        image->top[i].y0 = *y;
        image->top[i].x1 = *x + 0x10;
        image->top[i].y1 = *y;
        image->top[i].x2 = *x + 0x10;
        image->top[i].y2 = *y + 0x10;
        func_801C851C(image->topVerts, *x, *y, 0x10, 0x10);
        SetLineF3(&image->bottom[i]);
        image->bottom[i].r0 = 0;
        image->bottom[i].g0 = 0xff;
        image->bottom[i].b0 = 0;
        image->bottom[i].x0 = *x;
        image->bottom[i].y0 = *y;
        image->bottom[i].x1 = *x;
        image->bottom[i].y1 = *y + 0x10;
        image->bottom[i].x2 = *x + 0x10;
        image->bottom[i].y2 = *y + 0x10;
        func_801C851C(image->bottomVerts, *x, *y, 0x10, 0x10);
    }
}

/* Allocate and set up the 32 image blocks (+3a8). */
void func_801E5ACC(void) {
    s32 i;
    void *block;

    for (i = 0; i < 32; i++) {
        block = func_80031BDC(0x158, 0);
        D_800625A0->images[i] = block;
        bzero(block, 0x158);
        func_801E56E8(i);
        func_801E5924(i);
    }
}

/* Free the 32 image blocks at +3a8. */
void func_801E5B3C(void) {
    s32 i;

    for (i = 0; i < 32; i++) {
        func_800320E8(D_800625A0->images[i]);
    }
}

/* Set up the 32 text character quads (both buffers): 21 per line, 12x16
 * glyphs of the 140 page from v e0, 16 per glyph row. */
void func_801E5B88(void) {
    s32 i;
    s32 buffer;

    for (i = 0; i < 32; i++) {
        for (buffer = 0; buffer < 2; buffer++) {
            func_801E927C(&D_800625A0->block34C->chars[i * 2 + buffer]);
            (D_800625A0->block34C->chars + (i * 2 + buffer))->x0 = D_801E9994[i % 21] + i / 21 * 8;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->y0 = D_801E99E8[i / 21];
            (D_800625A0->block34C->chars + (i * 2 + buffer))->x1 = D_801E9994[i % 21] + i / 21 * 8 + 12;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->y1 = D_801E99E8[i / 21];
            (D_800625A0->block34C->chars + (i * 2 + buffer))->x2 = D_801E9994[i % 21] + i / 21 * 8;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->y2 = D_801E99E8[i / 21] + 16;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->x3 = D_801E9994[i % 21] + i / 21 * 8 + 12;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->y3 = D_801E99E8[i / 21] + 16;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->u0 = i % 16 * 16;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->v0 = i / 16 * 16 - 0x20;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->u1 = i % 16 * 16 + 12;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->v1 = i / 16 * 16 - 0x20;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->u2 = i % 16 * 16;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->v2 = i / 16 * 16 - 0x11;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->u3 = i % 16 * 16 + 12;
            (D_800625A0->block34C->chars + (i * 2 + buffer))->v3 = i / 16 * 16 - 0x11;
            D_800625A0->block34C->chars[i * 2 + buffer].tpage = GetTPage(0, 0, 0x140, 0x80);
            D_800625A0->block34C->chars[i * 2 + buffer].clut = GetClut(0, 0x1c0);
        }
    }
}

/* Set up the 32x32 cursor sprite and the purple-to-black shaded band
 * (0,4a)-(140,8a) for both buffers. */
#ifdef NON_MATCHING
void func_801E5E4C(void) {
    s32 *x;
    s32 *y;
    s32 i;

    i = 0;
    x = &D_801E99F0;
    y = &D_801E99F8;
    for (; i < 2; i++) {
        func_801E927C(&D_800625A0->block34C->cursor[i]);
        (D_800625A0->block34C->cursor + i)->x0 = *x;
        (D_800625A0->block34C->cursor + i)->y0 = *y;
        (D_800625A0->block34C->cursor + i)->x1 = *x + 0x20;
        (D_800625A0->block34C->cursor + i)->y1 = *y;
        (D_800625A0->block34C->cursor + i)->x2 = *x;
        (D_800625A0->block34C->cursor + i)->y2 = *y + 0x20;
        (D_800625A0->block34C->cursor + i)->x3 = *x + 0x20;
        (D_800625A0->block34C->cursor + i)->y3 = *y + 0x20;
        D_800625A0->block34C->cursor[i].tpage = GetTPage(0, 0, 0x140, 0x80);
        SetPolyG4(&D_800625A0->block34C->band[i]);
        (D_800625A0->block34C->band + i)->r0 = 0x80;
        (D_800625A0->block34C->band + i)->g0 = 0;
        (D_800625A0->block34C->band + i)->b0 = 0x80;
        (D_800625A0->block34C->band + i)->r1 = 0;
        (D_800625A0->block34C->band + i)->g1 = 0;
        (D_800625A0->block34C->band + i)->b1 = 0x80;
        (D_800625A0->block34C->band + i)->r2 = 0x10;
        (D_800625A0->block34C->band + i)->g2 = 0;
        (D_800625A0->block34C->band + i)->b2 = 0x10;
        (D_800625A0->block34C->band + i)->r3 = 0;
        (D_800625A0->block34C->band + i)->g3 = 0;
        (D_800625A0->block34C->band + i)->b3 = 0x10;
        (D_800625A0->block34C->band + i)->x0 = 0;
        (D_800625A0->block34C->band + i)->y0 = 0x4a;
        (D_800625A0->block34C->band + i)->x1 = 0x140;
        (D_800625A0->block34C->band + i)->y1 = 0x4a;
        (D_800625A0->block34C->band + i)->x2 = 0;
        (D_800625A0->block34C->band + i)->y2 = 0x8a;
        (D_800625A0->block34C->band + i)->x3 = 0x140;
        (D_800625A0->block34C->band + i)->y3 = 0x8a;
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5E4C);
#endif

/* Lay out the three save views' frames (nine images each, 50 apart) and
 * their 72x13 name quads (label rows 6 + view). */
void func_801E61B0(void) {
    s32 view;
    s32 i;

    for (view = 0; view < 3; view++) {
        D_800625A0->block34C->views[view].frameCount = 0;
        for (i = 0; i < 9; i++) {
            if (D_801EA494[i] != 0xffff) {
                D_800625A0->block34C->views[view].frameCount +=
                    func_8002675C(D_800625A0->sheet, D_801EA494[i],
                                  D_800625A0->block34C->views[view].frame[D_800625A0->block34C->views[view].frameCount],
                                  D_800625A0->bufferIndex, view * 0x50 + D_801E9F98[i], D_801E9FBC[i], 0x1000);
            }
        }
        D_800625A0->block34C->views[view].frameBuffer = D_800625A0->bufferIndex;
        func_801E927C(&D_800625A0->block34C->views[view].name[D_800625A0->bufferIndex]);
        D_800625A0->block34C->views[view].name[D_800625A0->bufferIndex].tpage = GetTPage(0, 0, 0x180, 0);
        D_800625A0->block34C->views[view].name[D_800625A0->bufferIndex].clut = D_800595D4;
        func_801E920C(&D_800625A0->block34C->views[view].name[D_800625A0->bufferIndex], D_801E9F98[0] + view * 0x50,
                      D_801E9FBC[0] + 7, D_801EA590[view] * 4, D_801EA5DC[view], 0x48, 0xd);
        D_800625A0->block34C->views[view].nameBuffer = D_800625A0->bufferIndex;
    }
}

/* Allocate and clear the 2dc0-byte block at +34c, then set it up. */
void func_801E6450(void) {
    void *block;
    u8 reserved[8]; /* the original frame reserves 8 unused bytes */

    block = func_80031BDC(0x2dc0, 0);
    D_800625A0->block34C = block;
    bzero(block, 0x2dc0);
    func_801E5B88();
    func_801E5E4C();
}

/* Free the 2dc0-byte block at +34c and clear party flag +b. */
void func_801E649C(void) {
    func_800320E8(D_800625A0->block34C);
    D_800625A0->party->unkB = 0;
}

/* Clear the 64x32 image area at (140, e0) to black and clear party flag +b. */
void func_801E64E0(void) {
    RECT rect;

    rect.x = 0x140;
    rect.y = 0xe0;
    rect.w = 0x40;
    rect.h = 0x20;
    ClearImage(&rect, 0, 0, 0);
    D_800625A0->party->unkB = 0;
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
            lo = D_801EA5D0[hi];
            D_801EA8C0 = 0;
            hi = D_801EA5D0[hi] >> 8;
        } else {
            hi = 0x81;
            lo = 0x40;
            D_801EA8C0 = 0;
        }
    }
    return func_800405C4(lo | (hi << 8));
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
    bzero(image, 0x1000);
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
    LoadImage(&rect, image);
    DrawSync(0);
    func_800320E8(pixels);
    func_800320E8(image);
}

/* Lay out the save's play time (two separators and seven digits at y 7a)
 * and its two-digit number (+23, plus one) with its label at (8, 66). */
void func_801E68AC(MenuViewSet *set) {
    s32 i;

    func_8002675C(D_800625A0->sheet, 0xee, D_800625A0->block34C->colon0, D_800625A0->bufferIndex, D_801E9FE0[0], 0x7a,
                  0x1000);
    func_8002675C(D_800625A0->sheet, 0xee, D_800625A0->block34C->colon1, D_800625A0->bufferIndex, D_801E9FE0[1], 0x7a,
                  0x1000);
    func_801C7F34(set->time);
    for (i = 0; i < 7; i++) {
        func_8002675C(D_800625A0->sheet, D_800625A0->time[i], D_800625A0->block34C->timeDigits[i],
                      D_800625A0->bufferIndex, D_801E9FE0[i + 2], 0x7a, 0x1000);
    }
    func_8002675C(D_800625A0->sheet, 0x17, D_800625A0->block34C->discLabel, D_800625A0->bufferIndex, 8, 0x66, 0x1000);
    func_8002675C(D_800625A0->sheet, 0x32, D_800625A0->block34C->discMark, D_800625A0->bufferIndex, 0x10, 0x66, 0x1000);
    func_8002675C(D_800625A0->sheet, (set->unk23 + 1) / 10, D_800625A0->block34C->discDigits[0],
                  D_800625A0->bufferIndex, 0x10, 0x6e, 0x1000);
    func_8002675C(D_800625A0->sheet, (set->unk23 + 1) % 10, D_800625A0->block34C->discDigits[1],
                  D_800625A0->bufferIndex, 0x18, 0x6e, 0x1000);
}

/* Set up view `index` of the block at +34c from its sheet image (14e + set). */
void func_801E6AE8(u8 index, MenuViewSet *set) {
    func_8002675C(D_800625A0->sheet, set->images[index] + 0x14e, &D_800625A0->block34C->views[index],
                  D_800625A0->bufferIndex, D_801EA004[index], D_801EA010[index], 0x1000);
}

/* Lay out view `index`'s number (the set's +16 value) as up to three digit
 * sprites, and reset its second digit row for the +19 value. */
void func_801E6B70(u8 index, MenuViewSet *set) {
    s32 i;
    u8 digit;

    func_801C80B8(set->levels[index]);
    D_800625A0->block34C->views[index].levelCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[i + 6];
        if (digit != 0xff) {
            D_800625A0->block34C->views[index].levelCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block34C->views[index].levelDigits[D_800625A0->block34C->views[index].levelCount],
                              D_800625A0->bufferIndex, D_801EA01C + index * 0x50 + i * 8, D_801EA020, 0x1000);
        }
    }
    func_801C80B8(set->unk19[index]);
    D_800625A0->block34C->views[index].unk871 = 0;
}

/* Lay out view `index`'s values A (+4) and B (+a) as up to three digit
 * sprites each (B packed without leading blanks). */
void func_801E6CFC(u8 index, MenuViewSet *set) {
    s32 i;
    s32 drawn;
    u8 digit;

    func_801C80B8(set->valueA[index]);
    D_800625A0->block34C->views[index].aCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[i + 6];
        if (digit != 0xff) {
            D_800625A0->block34C->views[index].aCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->block34C->views[index].aDigits[D_800625A0->block34C->views[index].aCount],
                              D_800625A0->bufferIndex, D_801EA02C + index * 0x50 + i * 8, D_801EA030, 0x1000);
        }
    }
    drawn = 0;
    func_801C80B8(set->valueB[index]);
    D_800625A0->block34C->views[index].bCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[i + 6];
        if (digit != 0xff) {
            D_800625A0->block34C->views[index].bCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->block34C->views[index].bDigits[D_800625A0->block34C->views[index].bCount],
                              D_800625A0->bufferIndex, D_801EA034 + index * 0x50 + drawn * 8, D_801EA038, 0x1000);
            drawn++;
        }
    }
}

/* Lay out view `index`'s values C (+10) and D (+13) as up to two digit
 * sprites each (D packed without leading blanks). */
void func_801E6F5C(u8 index, MenuViewSet *set) {
    s32 i;
    s32 drawn;
    u8 digit;

    func_801C80B8(set->valueC[index]);
    D_800625A0->block34C->views[index].cCount = 0;
    for (i = 0; i < 2; i++) {
        digit = D_800625A0->digits[i + 7];
        if (digit != 0xff) {
            D_800625A0->block34C->views[index].cCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->block34C->views[index].cDigits[D_800625A0->block34C->views[index].cCount],
                              D_800625A0->bufferIndex, D_801EA03C + index * 0x50 + i * 8, D_801EA040, 0x1000);
        }
    }
    drawn = 0;
    func_801C80B8(set->valueD[index]);
    D_800625A0->block34C->views[index].dCount = 0;
    for (i = 0; i < 2; i++) {
        digit = D_800625A0->digits[i + 7];
        if (digit != 0xff) {
            D_800625A0->block34C->views[index].dCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              D_800625A0->block34C->views[index].dDigits[D_800625A0->block34C->views[index].dCount],
                              D_800625A0->bufferIndex, D_801EA044 + index * 0x50 + drawn * 8, D_801EA048, 0x1000);
            drawn++;
        }
    }
}

/* Render the name of view `index`'s sheet entry from listed file `file`'s
 * header (up to ten two-byte characters) and upload it to label row
 * `index` of the view rows. */
#ifdef NON_MATCHING
void func_801E71B4(u8 index, MenuViewSet *set, s32 file) {
    RECT rect;
    u8 name[24];
    u8 text[24];
    u8 *pixels;
    MenuSaveInfo *info;
    s32 i;

    info = (MenuSaveInfo *)(D_800625A0->card->headers[file] + 0x100);
    for (i = 0; i < 20; i += 2) {
        name[i] = info->names[set->images[index]][i];
        name[i + 1] = info->names[set->images[index]][i + 1];
        if (name[i] == 0 && name[i + 1] == 0) {
            break;
        }
    }
    func_80033B34(name, text, i / 2);
    pixels = func_80031BDC(0x3f6, 0);
    bzero(pixels, 0x3f6);
    func_80034EAC(text, pixels, 0x24, 0);
    rect.x = D_801EA590[index] + 0x180;
    rect.y = D_801EA5DC[index];
    rect.w = 0x28;
    rect.h = 0xd;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_800320E8(pixels);
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E71B4);
#endif

/* Lay out the 16 character quads of the save title image (row f0 of the
 * 140 page, 12-pixel glyphs 16 apart) in the current buffer. */
void func_801E733C(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        func_801E927C(&D_800625A0->block34C->title[i * 2 + D_800625A0->bufferIndex]);
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->x0 = D_801EA04C + i * 12;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->y0 = D_801EA050;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->x1 = D_801EA04C + i * 12 + 12;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->y1 = D_801EA050;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->x2 = D_801EA04C + i * 12;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->y2 = D_801EA050 + 16;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->x3 = D_801EA04C + i * 12 + 12;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->y3 = D_801EA050 + 16;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->u0 = i * 16;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->v0 = 0xf0;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->u1 = i * 16 + 12;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->v1 = 0xf0;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->u2 = i * 16;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->v2 = 0xff;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->u3 = i * 16 + 12;
        (D_800625A0->block34C->title + (i * 2 + D_800625A0->bufferIndex))->v3 = 0xff;
        D_800625A0->block34C->title[i * 2 + D_800625A0->bufferIndex].tpage = GetTPage(0, 0, 0x140, 0x80);
        D_800625A0->block34C->title[i * 2 + D_800625A0->bufferIndex].clut = GetClut(0, 0x1c0);
    }
}

/* Build the three views of card file `index`'s save information. */
void func_801E76EC(s32 index) {
    MenuViewSet *set;
    s32 i;

    set = (MenuViewSet *)(D_800625A0->card->headers[index] + 0x100);
    func_801E61B0();
    for (i = 0; i < 3; i++) {
        if (set->images[i] != 0xff) {
            D_800625A0->block34C->views[i].shown = 1;
            func_801E6AE8(i, set);
            func_801E6B70(i, set);
            func_801E6CFC(i, set);
            func_801E6F5C(i, set);
            func_801E71B4(i, set, index);
        } else {
            D_800625A0->block34C->views[i].shown = 0;
        }
        D_800625A0->block34C->views[i].buffer = D_800625A0->bufferIndex;
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
            D_800625A0->block34C->rebuilt = 1;
        } else {
            func_801E6668(index);
            D_800625A0->block34C->rebuilt = 0;
        }
        D_800625A0->party->unkB = 1;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E78C8);

/* Set up `label`'s two quads for label image `index`: mode 0 takes the
 * image from the 140 column pages (rows from `first`); otherwise from the
 * 180 page (+80 keeps it opaque, else dimmed), with palette choice
 * `mode & 7f` - 1. Hides the label. */
#ifdef NON_MATCHING
void func_801E7C50(MenuLabelSlot *label, s32 index, s32 first, u8 mode) {
    POLY_FT4 *poly;
    s32 i;
    s32 semi;
    s32 row;
    u8 half;
    u8 column;
    u8 u;
    u8 v;

    half = index & 1;
    row = index / 2;
    column = (row & 1) << 7;
    for (i = 0; i < 2; i++) {
        poly = &label->polys[i];
        semi = 0;
        func_801E927C(poly);
        if (mode == 0) {
            label->palette = half;
            poly->tpage = GetTPage(0, 0, 0x140, 0);
            v = (index + first) / 4 * 0xd;
            poly->u0 = column;
            poly->v0 = v;
            poly->u1 = column + label->width;
            poly->v1 = v;
            poly->u2 = column;
            poly->v2 = v + 0xd;
            poly->u3 = column + label->width;
            poly->v3 = v + 0xd;
        } else {
            if (!(mode & 0x80)) {
                semi = 0x20;
                SetSemiTrans(poly, 1);
                poly->r0 = 0x20;
                poly->g0 = 0x20;
                poly->b0 = 0x20;
            }
            label->palette = (mode & 0x7f) - 1;
            u = half * 0x60;
            poly->tpage = semi | GetTPage(0, 0, 0x180, 0x80);
            v = row * 0xd + first;
            poly->u0 = u;
            poly->v0 = v;
            poly->u1 = u + label->width;
            poly->v1 = v;
            poly->u2 = u;
            poly->v2 = v + 0xd;
            poly->u3 = u + label->width;
            poly->v3 = v + 0xd;
        }
        if (label->palette != 0) {
            poly->clut = D_80059414;
        } else {
            poly->clut = D_800595D4;
        }
    }
    label->visible = 0;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E7C50);
#endif

/* Render `count` labels (text ids in `layout`) in pairs into one 28x13
 * image each (two columns of 32 from x 140, rows of 13 from label `first`),
 * lay them out and upload the images. */
void func_801E7E68(MenuLabelSlot *labels, u8 *layout, s32 first, s32 count) {
    s32 i;
    RECT *image;

    for (i = 0; i < count; i += 2) {
        labels[i].width = func_80034EAC(func_80033728(D_800625A0->labels, layout[i]), D_800625A0->topLabels[0].pixels,
                                        0x18, 0);
        image = &labels[i].image;
        labels[i + 1].width = func_80034EAC(func_80033728(D_800625A0->labels, layout[i + 1]), D_800625A0->topLabels[0].pixels,
                                        0x18, 1);
        labels[i].image.x = (i / 2 & 1) * 0x20 + 0x140;
        labels[i].image.y = (i + first) / 4 * 0xd;
        labels[i].image.w = 0x1c;
        labels[i].image.h = 0xd;
        labels[i + 1].image = labels[i].image;
        func_801E7C50(&labels[i], i, first, 0);
        func_801E7C50(&labels[i + 1], i + 1, first, 0);
        LoadImage(image, D_800625A0->topLabels[0].pixels);
        DrawSync(0);
    }
}

/* Lay out `count` labels from `table` into `labels` (the placement is unused). */
void func_801E8018(u8 count, MenuLabelSlot *labels, u8 *table, u8 *flags) {
    func_801E7E68(labels, table, 4, count);
}

/* Clear `count` label flags. */
void func_801E8044(u8 count, u8 *flags) {
    s32 i;

    for (i = 0; i < count; i++) {
        flags[i] = 0;
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8070);

/* Open the command window: grow the cursor column one command per two
 * frames (the label column one behind), up to `count` commands. */
void func_801E8474(s32 count, MenuCommandImages *images) {
    s32 n;
    s32 i;

    D_800625A0->screenImages->captured = 0;
    D_800625A0->screenImages->refresh = 0;
    D_800625A0->party->redraw9 = 1;
    for (n = 1; n <= count; n++) {
        if (n != count) {
            D_800625A0->screenImages->cursorCount = 0;
            for (i = 0; i < n; i++) {
                D_800625A0->screenImages->cursorCount +=
                    func_8002675C(D_800625A0->sheet, images[i].cursor,
                                  D_800625A0->screenImages->cursor[D_800625A0->screenImages->cursorCount],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
            }
            D_800625A0->screenImages->cursorBuffer = D_800625A0->bufferIndex;
        }
        D_800625A0->screenImages->cursor2Count = 0;
        if (n != 1) {
            for (i = 0; i < n - 1; i++) {
                D_800625A0->screenImages->cursor2Count +=
                    func_8002675C(D_800625A0->sheet, images[i].label,
                                  D_800625A0->screenImages->cursor2[D_800625A0->screenImages->cursor2Count],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
            }
            D_800625A0->screenImages->cursor2Buffer = D_800625A0->bufferIndex;
        }
        for (i = 0; i < 2; i++) {
            func_801C7BF4();
        }
    }
}

/* Open the choice window of the command at `offset` past the top cursor:
 * grow its cursor and label columns one choice per two frames (until an
 * empty choice, ffff), counting the choices. */
#ifdef NON_MATCHING
void func_801E86C8(u8 offset) {
    s32 n;
    s32 i;
    u8 growing;

    D_800625A0->spriteLists->firstCount = 0;
    D_800625A0->spriteLists->secondCount = 0;
    D_800625A0->party->redrawA = 1;
    growing = 1;
    for (n = 1; n < 5; n++) {
        D_800625A0->spriteLists->firstCount = 0;
        D_800625A0->choiceCount = 0;
        for (i = 0; i < n; i++) {
            if (D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2] != 0xffff) {
                D_800625A0->spriteLists->firstCount +=
                    func_8002675C(D_800625A0->sheet, D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2],
                                  &D_800625A0->spriteLists->first[D_800625A0->spriteLists->firstCount * 2],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
                D_800625A0->choiceCount++;
            } else {
                growing = 0;
            }
        }
        D_800625A0->spriteLists->firstStart = D_800625A0->bufferIndex;
        if (growing) {
            for (i = 0; i < 2; i++) {
                func_801C7BF4();
            }
        }
        D_800625A0->spriteLists->secondCount = 0;
        for (i = 0; i < n; i++) {
            if (D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2] != 0xffff) {
                D_800625A0->spriteLists->secondCount +=
                    func_8002675C(D_800625A0->sheet, D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2 + 1],
                                  &D_800625A0->spriteLists->second[D_800625A0->spriteLists->secondCount * 2],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
            }
        }
        D_800625A0->spriteLists->secondStart = D_800625A0->bufferIndex;
        if (growing) {
            for (i = 0; i < 2; i++) {
                func_801C7BF4();
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E86C8);
#endif

/* Lay out the command cursor sprites for `count` commands (images of the
 * `images`, the chosen one `cursor` taking its lit image,
 * +d), then mark the command window for redraw. */
void func_801E8978(u8 count, u8 cursor, MenuCommandImages *images) {
    s32 i;
    s32 image;

    D_800625A0->screenImages->cursorCount = 0;
    D_800625A0->screenImages->cursor2Count = 0;
    for (i = 0; i < count; i++) {
        if (i == cursor) {
            image = images[i].cursor + 0xd;
        } else {
            image = images[i].cursor;
        }
        D_800625A0->screenImages->cursorCount +=
            func_8002675C(D_800625A0->sheet, image,
                          D_800625A0->screenImages->cursor[D_800625A0->screenImages->cursorCount],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
        D_800625A0->screenImages->cursor2Count +=
            func_8002675C(D_800625A0->sheet, images[i].label,
                          D_800625A0->screenImages->cursor2[D_800625A0->screenImages->cursor2Count],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
    }
    D_800625A0->screenImages->cursorBuffer = D_800625A0->bufferIndex;
    D_800625A0->screenImages->cursor2Buffer = D_800625A0->bufferIndex;
    func_801D1EE0(cursor, 1);
    D_800625A0->party->redraw4 = 1;
}

/* Lay out the choice cursor sprites of the command at `offset` past the top
 * cursor (the chosen one lit), then mark the window for redraw. */
void func_801E8B4C(u8 offset) {
    s32 i;
    s32 image;

    D_800625A0->spriteLists->firstCount = 0;
    D_800625A0->spriteLists->secondCount = 0;
    for (i = 0; i < D_800625A0->choiceCount; i++) {
        if (i == D_800625A0->choice) {
            image = D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2] + 0xd;
        } else {
            image = D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2];
        }
        D_800625A0->spriteLists->firstCount +=
            func_8002675C(D_800625A0->sheet, image,
                          &D_800625A0->spriteLists->first[D_800625A0->spriteLists->firstCount * 2],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
        D_800625A0->spriteLists->secondCount +=
            func_8002675C(D_800625A0->sheet, D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2 + 1],
                          &D_800625A0->spriteLists->second[D_800625A0->spriteLists->secondCount * 2],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
    }
    D_800625A0->spriteLists->firstStart = D_800625A0->bufferIndex;
    D_800625A0->spriteLists->secondStart = D_800625A0->bufferIndex;
    func_801D1EE0(D_800625A0->choice + 7, 1);
    D_800625A0->party->redraw4 = 1;
}

/* Render the two name lines of name pair `image` (ff: blank) and upload them
 * to the label area of row `row` (rows pair up on one 40x13 image). */
void func_801E8DA8(u8 image, u8 row) {
    RECT rect;
    u8 *pixels;

    pixels = func_80031BDC(0x3f6, 0);
    bzero(pixels, 0x3f6);
    if (image != 0xff) {
        func_80034EAC(D_8006D634[image >> 1][0], pixels, 0x24, 0);
        func_80034EAC(D_8006D634[image >> 1][1], pixels, 0x24, 1);
    }
    rect.x = D_801EA578[row >> 1] + 0x180;
    rect.y = D_801EA5C4[row >> 1];
    rect.w = 0x28;
    rect.h = 0xd;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_800320E8(pixels);
}

/* Set `poly`'s blending for `mode`: 0 opaque, 1 additive-dim, 2 plain,
 * 3 dim. */
#ifdef NON_MATCHING
void func_801E8EAC(POLY_FT4 *poly, u8 mode) {
    u8 shade;

    SetShadeTex(poly, 0);
    switch (mode) {
    case 1:
        poly->tpage |= 0x20;
        SetSemiTrans(poly, 1);
        shade = 0x21;
        break;
    case 0:
        SetSemiTrans(poly, 0);
    case 2:
        shade = 0x80;
        break;
    case 3:
        shade = 0x21;
        break;
    default:
        return;
    }
    poly->r0 = shade;
    poly->g0 = shade;
    poly->b0 = shade;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8EAC);
#endif

/* Set the blending of portrait `index`'s quads of the current buffer: plain
 * (2), or dim (3) when `dim`. Every edge list is walked four pairs deep, so
 * each pass also covers the list after it, as the original does. */
#ifdef NON_MATCHING
void func_801E8F60(u8 index, u8 dim) {
    u8 mode;
    s32 i;

    mode = 2;
    if (dim) {
        mode = 3;
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->frame[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edgeA[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edgeB[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edgeC[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edgeD[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 2; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->top[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    func_801E8EAC(&D_800625A0->portraits[index]->side[D_800625A0->portraits[index]->buffer], mode);
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8F60);
#endif

/* Make `poly` semi-transparent, textured without shading, at neutral colour. */
void func_801E91C4(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Place `poly` at (x, y) with size (w, h) and texture origin (u, v). */
void func_801E920C(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h) {
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + w;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + h;
    poly->u3 = u + w;
    poly->v3 = v + h;
}

/* Initialise `poly` as an opaque textured quad at neutral colour. */
void func_801E927C(POLY_FT4 *poly) {
    SetPolyFT4(poly);
    SetSemiTrans(poly, 0);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Unless 8002c3d8 reports ready, reset the drive and retry command 8 until it
 * succeeds. */
void func_801E92CC(void) {
    if (func_8002C3D8() == 0) {
        func_8002A498(0);
        func_80028A60(0);
        func_8002A428(0);
        func_80028A60(0);
        VSync(3);
        do {
            VSync(3);
        } while (CdControlB(8, 0, D_801EA8F4) == 0);
    }
}

/* Read `size` bytes of host file `name` into `buffer` (development PC link). */
void func_801E9340(char *name, void *buffer, s32 size) {
    s32 handle;

    handle = PCopen(name, 0, 0);
    func_8004C398(handle, buffer, size);
    PCclose(handle);
}

/* Check that disc `disc` is in the drive and load its directory: from the
 * host files on the development link, else after waiting for the lid to
 * close and the drive to settle, from the disc label and files 18 and 28.
 * Returns 0 when loaded, 2 when no disc label was read, 3 for the other
 * disc. */
#ifdef NON_MATCHING
s32 func_801E93A0(s32 disc) {
    DiscLabel label = { { 0 } };
    u8 pos[4];
    s32 result;
    s32 ok;

    func_80028A60(0);
    if (func_8002C3D8() != 0) {
        result = 0;
        if (disc == 1) {
            func_801E9340("c:\\work\\cdrom.mdg", D_8004FDF0, 0x8000);
            func_801E9340("c:\\work\\cdrom.fid", D_8004FDF4, 0x7a);
            func_801E9340("c:\\work\\cdrom.fnd", D_8004FE48, 0x40000);
        } else {
            func_801E9340("c:\\work\\cdrom2.mdg", D_8004FDF0, 0x8000);
            func_801E9340("c:\\work\\cdrom2.fid", D_8004FDF4, 0x7a);
            func_801E9340("c:\\work\\cdrom2.fnd", D_8004FE48, 0x40000);
        }
        return result;
    }
    CdIntToPos(0, pos);
    do {
        VSync(3);
        CdControlB(1, 0, D_801EA8F4);
    } while (!(D_801EA8F4[0] & 0x10));
    do {
        VSync(3);
        CdControlB(1, 0, D_801EA8F4);
    } while (D_801EA8F4[0] & 0x10);
    do {
        VSync(3);
        ok = CdControlB(1, 0, D_801EA8F4);
    } while (!(D_801EA8F4[0] & 2) || ok == 0);
retry:
    do {
        VSync(3);
    } while (CdControlB(0x13, 0, D_801EA8F4) == 0);
    do {
        VSync(3);
    } while (CdControlB(2, pos, D_801EA8F4) == 0);
    ok = CdControlB(0x15, 0, D_801EA8F4);
    if ((D_801EA8F4[0] & 1) && (D_801EA8F4[1] & 0x40)) {
        if (ok == 0) {
            return 2;
        }
    } else if (ok == 0) {
        goto retry;
    }
    func_8002A428(0xa0);
    func_80028A60(0);
    VSync(3);
    VSync(3);
    func_8002954C(0x17, &label, 0x10, 0, 0);
    func_80028A60(0);
    result = 2;
    if (label.tag == 0x4e45585f) {
        result = 3;
        if (label.disc == disc + '0') {
            func_8002954C(0x18, D_8004FDF0, 0x8000, 0, 0);
            result = 0;
            func_80028A60(0);
            func_8002954C(0x28, D_8004FDF4, 0x7a, 0, 0);
            func_80028A60(0);
        }
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E93A0);
#endif
