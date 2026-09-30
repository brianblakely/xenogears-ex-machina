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

/* Step party slot `slot` forward (`dir` 0) or back (1) to the next occupied
 * slot, or with `readyOnly` to the next ready one; wraps around the three. */
s32 func_801D9704(s32 slot, u8 dir, u8 readyOnly) {
    switch (dir) {
    case 0:
        for (;;) {
            slot++;
            if (slot >= 3) {
                slot = 0;
            }
            if (!readyOnly) {
                if (D_800625A0->party->ids[slot] != 0xff) {
                    break;
                }
            } else if (D_800625A0->party->ready[slot] != 0) {
                break;
            }
        }
        break;
    case 1:
        for (;;) {
            slot--;
            if (slot < 0) {
                slot = 2;
            }
            if (!readyOnly) {
                if (D_800625A0->party->ids[slot] != 0xff) {
                    break;
                }
            } else if (D_800625A0->party->ready[slot] != 0) {
                break;
            }
        }
        break;
    }
    return slot;
}

/* The sound mode screen: choose one of the sound driver's output
 * modes (choice 0 is mode 0, 1 mode 2, 2 mode 1); confirm applies it, cancel
 * leaves. Always continues the menu. */
u8 func_801D9808(void) {
    s32 mode;
    u8 first;
    u8 stay;
    u8 apply;

    stay = 1;
    first = 1;
    apply = 0;
    do {
        func_801C7BF4();
        if (first) {
            func_801E8018(4, D_800625A0->soundLabels, D_801EA574, D_800625A0->party->unk5C);
            func_801E86C8(0);
            D_800625A0->choiceShown = 0xff;
            switch (func_80038824()) {
            case 0:
                mode = 0;
                break;
            case 1:
                mode = 2;
                break;
            case 2:
                mode = 1;
                break;
            }
            first = 0;
            D_800625A0->choice = mode;
        }
        if (D_800625A0->choice != D_800625A0->choiceShown) {
            func_801E8070(6, D_800625A0->soundLabels, D_801EA578, D_801E9F88, D_800625A0->party->unk5C,
                          D_800625A0->choice, 7, 0);
            func_801E8B4C(0);
            D_800625A0->choiceShown = D_800625A0->choice;
        }
        switch (D_800625A0->input) {
        case 4:
            apply = 1;
        case 5:
            stay = 0;
            break;
        case 1:
            if (D_800625A0->choice != 0) {
                D_800625A0->choice--;
            } else {
                D_800625A0->choice = D_800625A0->choiceCount - 1;
            }
            break;
        case 3:
            if (++D_800625A0->choice >= D_800625A0->choiceCount) {
                D_800625A0->choice = 0;
            }
            break;
        }
    } while (stay);
    if (apply) {
        switch (D_800625A0->choice) {
        case 0:
            mode = 0;
            break;
        case 1:
            mode = 2;
            break;
        case 2:
            mode = 1;
            break;
        }
        func_800386C4(mode);
    }
    func_801E8044(4, D_800625A0->party->unk5C);
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
    return 1;
}

/* Restart memory-card access: close the card events, reinitialise the card
 * library and open and enable its four events (ioe, error, timeout, new card). */
void func_801D9B08(void) {
    func_801C8960();
    VSync(0);
    InitCARD(1);
    StartCARD();
    _bu_init();
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    D_800625A0->card->events[0] = OpenEvent(0xf4000001, 4, 0x2000, 0);
    D_800625A0->card->events[1] = OpenEvent(0xf4000001, 0x8000, 0x2000, 0);
    D_800625A0->card->events[2] = OpenEvent(0xf4000001, 0x100, 0x2000, 0);
    D_800625A0->card->events[3] = OpenEvent(0xf4000001, 0x2000, 0x2000, 0);
    EnableEvent(D_800625A0->card->events[0]);
    EnableEvent(D_800625A0->card->events[1]);
    EnableEvent(D_800625A0->card->events[2]);
    EnableEvent(D_800625A0->card->events[3]);
    ExitCriticalSection();
}

/* Enter the file screen's card mode: show message 20, wait for the view to
 * stop, restart card access with the CD callbacks saved and cleared, forget
 * the card presence and listings and check the cards now. Returns nonzero
 * when the check fails (the message stays up); otherwise closes the message. */
u8 func_801D9C84(void) {
    MenuCard *card;
    u8 failed;

    failed = 1;
    func_801D2F4C(0x20);
    D_800625A0->party->messageShown = 1;
    while (D_800625A0->viewMotion != 0) {
        func_801C7BF4();
    }
    D_800625A0->card->busy = 1;
    func_801C7BF4();
    func_801D9B08();
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    D_801EA718 = CdSyncCallback(0);
    D_801EA71C = CdReadyCallback(0);
    D_801EA720 = CdReadCallback(0);
    ExitCriticalSection();
    card = D_800625A0->card;
    card->presentShown[0] = card->presentShown[1] = 0xff;
    D_800625A0->card->scanned[0] = 0;
    D_800625A0->card->scanned[1] = 0;
    D_800625A0->card->mode = 2;
    D_800625A0->cardPollTimer = 0x3c;
    func_801C7BF4();
    func_801C7BF4();
    if (func_801C93A8() == 0) {
        if (D_800625A0->party->messageShown != 0) {
            func_801D32B4();
            D_800625A0->party->messageShown = 0;
        }
        failed = 0;
    }
    return failed;
}

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

/* The file screen (save or load by `save`): on the first pass build the card
 * panels, zoom in, show the header and enter card mode; then each frame
 * check the cards, show the choice labels and handle the input: confirm runs
 * the file command (and ends the screen outside the title), cancel ends it.
 * Returns 0 when a `loading` screen was left by the player, the cards failed
 * outside the field menu or there is no card outside the field menu. */
u8 func_801D9F98(u8 loading, u8 save) {
    u8 running;
    u8 first;
    u8 result;
    u8 header;

    running = 1;
    first = 1;
    result = 1;
    D_801E9784 = 0;
    D_800625A0->choice = 2;
    header = 0;
    if (D_80059460 == 0 && D_80059171 == 0) {
        D_800625A0->choice = 1;
    }
    D_800625A0->choiceShown = 0xff;
    func_801D9F34();
    func_801C7BF4();
    while (running) {
        D_800625A0->party->unkB = 0;
        D_800625A0->loadState = 1;
        if (first) {
            func_801E6450();
            func_801E5ACC();
            D_800625A0->card->present[0] = 1;
            D_800625A0->card->present[1] = 1;
            func_801D1E80();
            switch (D_80059460) {
            case 0:
                func_801D29A8(0, 0);
                if (D_80059171 == 0) {
                    header = 9;
                }
                break;
            case 2:
                header = 7;
                break;
            case 6:
                break;
            }
            func_801E86C8(header);
            D_800625A0->party->redraw6 = 0;
            first = 0;
            D_800625A0->party->unk20[1] = 0;
            if (func_801D9C84()) {
                if (D_80059460 != 0) {
                    result = 0;
                }
                break;
            }
            D_800625A0->party->cardMode = 1;
        }
        if (D_801E9778 != 0) {
            func_801D2F4C(0x20);
        }
        if (func_801C93A8()) {
            running = 0;
        }
        if (D_801E9778 != 0) {
            func_801D32B4();
            D_801E9778 = 0;
        }
        if (!running) {
            break;
        }
        if (D_800625A0->choice != D_800625A0->choiceShown) {
            func_801E8070(6, D_800625A0->fileLabels, D_801EA542, D_801E9EA0, D_800625A0->party->unk1A,
                          D_800625A0->choice, 7, 0);
            if (D_80059460 != 2) {
                func_801E8B4C(0);
            } else {
                func_801E8B4C(7);
            }
            D_800625A0->choiceShown = D_800625A0->choice;
        }
        switch (D_800625A0->input) {
        case 4:
            func_801D22C4();
            func_801E8044(6, D_800625A0->party->unk1A);
            running = func_801CD710(save);
            D_800625A0->choiceShown = 0xff;
            func_801D9F34();
            if (D_80059460 == 2) {
                break;
            }
        case 5:
            running = 0;
            if (loading) {
                result = 0;
            }
            break;
        case 1:
            if (D_800625A0->choice != 0) {
                D_800625A0->choice--;
            } else {
                D_800625A0->choice = D_800625A0->choiceCount - 1;
            }
            break;
        case 3:
            if (++D_800625A0->choice >= D_800625A0->choiceCount) {
                D_800625A0->choice = 0;
            }
            break;
        }
    }
    if (*(u16 *)D_800625A0->card->present == 0 && D_80059460 != 0) {
        result = 0;
    }
    D_800625A0->party->cardMode = 0;
    D_800625A0->party->unkB = 0;
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
    func_801E8044(6, D_800625A0->party->unk1A);
    func_801C8960();
    return result;
}

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

/* Build the 16 visible entries of the item list from scroll row `row`: an
 * entry without an id or count is emptied; otherwise the count is capped at
 * 99 and the item name and two-digit count are rendered into the image area
 * and laid out, greyed when the item cannot be used here. */
#ifdef NON_MATCHING
/* Differs in register allocation: this keeps 0xcccccccd in a saved register
 * and spills `row`; the original materialises the constant at each use. */
void func_801DA5BC(s32 row) {
    u8 codes[4];
    u8 text[8];
    RECT rect;
    s32 i;
    s32 tens;
    s32 x;
    s32 left;
    u16 y;
    u8 kind;
    u8 grey;
    u8 *image;
    u8 *ids;
    u8 *counts;

    image = func_80031BDC(0x3f6, 0);
    codes[1] = 0;
    codes[3] = 0;
    ids = D_8006F65A;
    counts = ids - 150;
    for (i = 0; i < 16; i++) {
        if (ids[row * 2 + i] != 0) {
            if (counts[row * 2 + i] != 0) {
                if (counts[row * 2 + i] >= 100) {
                    counts[row * 2 + i] = 99;
                }
                D_800625A0->block42C->names[i].width = func_80034EAC(func_80033818(ids[row * 2 + i]), image, 0x24, 0);
                tens = counts[row * 2 + i] / 10;
                codes[0] = tens + 0x10;
                if (tens == 0) {
                    codes[0] = 0xc3;
                }
                codes[2] = counts[row * 2 + i] % 10 + 0x10;
                func_80033B34(codes, text, 2);
                D_800625A0->block42C->values[i].width = func_80034EAC(text, image, 0x24, 1);
                rect.x = (i & 1) * 0x18 + 0x180;
                rect.y = (i / 2) * 0xd + 0x80;
                rect.w = 0x28;
                rect.h = 0xd;
                LoadImage(&rect, image);
                DrawSync(0);
                kind = D_800625A0->tables->items[ids[row * 2 + i]].use;
                if (kind & 0x20) {
                    grey = kind & 0x80;
                    if (D_80059171 == 0) {
                        grey = 0;
                    }
                } else {
                    grey = kind & 0x80;
                }
                func_801E7C50(&D_800625A0->block42C->names[i], i, 0x80, grey | 1);
                func_801E7C50(&D_800625A0->block42C->values[i], i, 0x80, grey | 2);
                x = (i % 2) * 0x88;
                y = (i / 2) * 0x10 | 0xe;
                left = (x + 0x28) & 0xfff8;
                func_801C851C(D_800625A0->block42C->names[i].verts, left, y, D_800625A0->block42C->names[i].width,
                              0xd);
                left = (x + 0x90) & 0xfff8;
                func_801C851C(D_800625A0->block42C->values[i].verts, left, y, D_800625A0->block42C->values[i].width,
                              0xd);
                D_800625A0->block42C->names[i].count = D_800625A0->bufferIndex;
                D_800625A0->block42C->values[i].count = D_800625A0->bufferIndex;
                D_800625A0->block42C->shown[i] = 1;
            } else {
                ids[row * 2 + i] = 0;
                D_800625A0->block42C->shown[i] = 0;
            }
        } else {
            counts[row * 2 + i] = 0;
            D_800625A0->block42C->shown[i] = 0;
        }
    }
    func_800320E8(image);
    D_800625A0->party->unk48 = 1;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA5BC);
#endif

/* Show the description of item list entry `entry` at scroll row `row`: the
 * item's message line, a copy of its name and count texts and, for items
 * used from the menu, its target labels. An empty entry hides it. */
void func_801DA9A8(s32 entry, s32 row) {
    RECT rect;
    u8 *ids;
    u8 *id;
    u8 *image;
    s32 pad;
    u8 all;
    u8 kind;
    u16 target;
    MenuItem *item;

    ids = D_8006F65A;
    id = &ids[row * 2 + entry];
    if (*id != 0) {
        image = func_80031BDC(0x618, 0);
        bzero(image, 0x618);
        D_800625A0->block42C->extra[2].width =
            func_80034EAC(func_80033728(D_800625A0->block42C->unk1180, *id), image, 0x39, 0);
        rect.x = 0x140;
        rect.y = 0x4e;
        rect.w = 0x3c;
        rect.h = 0xd;
        LoadImage(&rect, image);
        DrawSync(0);
        func_801E7C50(&D_800625A0->block42C->extra[2], 0, 0, 0);
        func_801E920C(&D_800625A0->block42C->extra[2].polys[D_800625A0->bufferIndex], 0x1c, 0xa1, 0, 0x4e,
                      D_800625A0->block42C->extra[2].width, 0xd);
        func_801C851C(D_800625A0->block42C->extra[2].verts, 0x1c, 0xa1, D_800625A0->block42C->extra[2].width, 0xd);
        func_800320E8(image);
        pad = D_800625A0->block42C->values[entry].width == 0x10 ? 4 : 0;
        memmove(&D_800625A0->block42C->extra[0], &D_800625A0->block42C->names[entry], sizeof(MenuLabelSlot));
        memmove(&D_800625A0->block42C->extra[1], &D_800625A0->block42C->values[entry], sizeof(MenuLabelSlot));
        func_801C851C(D_800625A0->block42C->extra[0].verts, 0x10, 0x93, D_800625A0->block42C->names[entry].width,
                      0xd);
        func_801C851C(D_800625A0->block42C->extra[1].verts, pad | 0x78, 0x93,
                      D_800625A0->block42C->values[entry].width, 0xd);
        (D_800625A0->block42C->extra[0].polys + D_800625A0->bufferIndex)->r0 = 0x80;
        (D_800625A0->block42C->extra[0].polys + D_800625A0->bufferIndex)->g0 = 0x80;
        (D_800625A0->block42C->extra[0].polys + D_800625A0->bufferIndex)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->block42C->extra[0].polys[D_800625A0->bufferIndex], 0);
        (D_800625A0->block42C->extra[1].polys + D_800625A0->bufferIndex)->r0 = 0x80;
        (D_800625A0->block42C->extra[1].polys + D_800625A0->bufferIndex)->g0 = 0x80;
        (D_800625A0->block42C->extra[1].polys + D_800625A0->bufferIndex)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->block42C->extra[1].polys[D_800625A0->bufferIndex], 0);
        func_801E8044(8, D_800625A0->party->unk38);
        item = &D_800625A0->tables->items[*id];
        if (item->use & 0xc0) {
            target = item->target;
            if (target & 0x4000) {
                all = 2;
            } else if (target & 0x1000) {
                all = 0;
            } else {
                all = 1;
            }
            func_801E8070(8, D_800625A0->labels10E0, D_801EA550, D_801E9EA0, D_800625A0->party->unk38, all, 0, 1);
            kind = (item->target & 3) + 3;
            func_801E8070(8, D_800625A0->labels10E0, D_801EA550, D_801E9EA0, D_800625A0->party->unk38, kind, 0, 1);
            (D_800625A0->labels10E0[all].polys + D_800625A0->bufferIndex)->r0 = 0x80;
            (D_800625A0->labels10E0[all].polys + D_800625A0->bufferIndex)->g0 = 0x80;
            (D_800625A0->labels10E0[all].polys + D_800625A0->bufferIndex)->b0 = 0x80;
            SetSemiTrans(&D_800625A0->labels10E0[all].polys[D_800625A0->bufferIndex], 0);
            (D_800625A0->labels10E0[kind].polys + D_800625A0->bufferIndex)->r0 = 0x80;
            (D_800625A0->labels10E0[kind].polys + D_800625A0->bufferIndex)->g0 = 0x80;
            (D_800625A0->labels10E0[kind].polys + D_800625A0->bufferIndex)->b0 = 0x80;
            SetSemiTrans(&D_800625A0->labels10E0[kind].polys[D_800625A0->bufferIndex], 0);
        }
        D_800625A0->block42C->extra[0].count = D_800625A0->bufferIndex;
        D_800625A0->block42C->extra[1].count = D_800625A0->bufferIndex;
        D_800625A0->block42C->extra[2].count = D_800625A0->bufferIndex;
        D_800625A0->block42C->extraShown = 1;
    } else {
        func_801E8044(8, D_800625A0->party->unk38);
        D_800625A0->block42C->extraShown = 0;
    }
}

/* Allocate image block `index` (+444). */
void func_801DB02C(u8 index) {
    void *block;

    block = func_80031BDC(0x78, 0);
    D_800625A0->blocks444[index] = block;
    bzero(block, 0x78);
    D_800625A0->blocks444[index]->frame = 4;
    D_800625A0->blocks444[index]->timer = 0;
}

/* Animate list cursor `index` (image block +444) and place it at entry
 * `entry` of the list kind `kind` (0 item list, 1 scrolled item list at
 * scroll row `row`, hidden when off the page, 2 two-column list, 3 one
 * column). */
void func_801DB0A8(s32 entry, s32 row, u8 kind, u8 index) {
    MenuImageBlock *cursor;
    POLY_FT4 *poly;
    s32 x;
    s32 y;
    s32 shown;

    cursor = D_800625A0->blocks444[index];
    shown = 1;
    if (++cursor->timer >= 6) {
        if (--cursor->frame < 0) {
            cursor->frame = 4;
        }
        cursor->timer = 0;
    }
    switch (kind) {
    case 0:
        x = (entry % 2) * 0x88 + 0x1c;
        y = (entry / 2) * 0x10 + 0x11;
        break;
    case 1:
        if (entry >= row * 2 && entry < row * 2 + 0x10) {
            x = (entry % 2) * 0x88 + 0x18;
            y = (entry - row * 2) / 2 * 0x10 + 0x11;
        } else {
            shown = 0;
        }
        break;
    case 2:
        x = (entry % 2) * 0x88 + 0x18;
        y = entry / 2 * 0x10 + 0x14;
        break;
    case 3:
        x = 0xa0;
        y = entry * 0xd + 0x14;
        shown = 2;
        break;
    }
    if (shown) {
        func_8002675C(D_800625A0->sheet, cursor->frame + 0x15b, cursor, D_800625A0->bufferIndex, 0, 0, 0x1000);
        poly = &cursor->polys[D_800625A0->bufferIndex];
        func_801C851C(cursor->verts, poly->x0 + x, poly->y0 + y, poly->x1 - poly->x0, poly->y3 - poly->y0);
        cursor->count = D_800625A0->bufferIndex;
        D_800625A0->party->unk50[index] = 1;
    } else {
        D_800625A0->party->unk50[index] = 0;
    }
}

/* Free image block `index` (+444) and clear its party flag (+50). */
void func_801DB340(u8 index) {
    func_800320E8(D_800625A0->blocks444[index]);
    D_800625A0->party->unk50[index] = 0;
}

/* Shade the item screen's texts by `mode` (801e8eac): windows 3 and 4, the
 * window quad, each built entry name and count, the two cursors, the
 * selected entry, the description and the eight help labels. */
void func_801DB39C(u8 mode) {
    s32 i;

    func_801E8F60(3, mode);
    func_801E8F60(4, mode);
    func_801E8EAC(&D_800625A0->block43C->polys[D_800625A0->block43C->buffer], mode);
    i = 0;
    do {
        if (D_800625A0->block42C->names[i].polys[D_800625A0->block42C->names[i].count].r0 != 0x20) {
            func_801E8EAC(&D_800625A0->block42C->names[i].polys[D_800625A0->block42C->names[i].count], mode);
            func_801E8EAC(&D_800625A0->block42C->values[i].polys[D_800625A0->block42C->values[i].count], mode);
        }
        i++;
    } while (i < 16);
    for (i = 0; i < 2; i++) {
        func_801E8EAC(&D_800625A0->blocks444[i]->polys[D_800625A0->blocks444[i]->count], mode);
    }
    func_801E8EAC(&D_800625A0->block42C->extra[0].polys[D_800625A0->block42C->extra[0].count], mode);
    func_801E8EAC(&D_800625A0->block42C->extra[1].polys[D_800625A0->block42C->extra[1].count], mode);
    func_801E8EAC(&D_800625A0->block42C->extra[2].polys[D_800625A0->block42C->extra[2].count], mode);
    for (i = 0; i < 8; i++) {
        func_801E8EAC(&D_800625A0->labels10E0[i].polys[D_800625A0->labels10E0[i].count], mode);
    }
}

/* Build the target selection's party panels: allocate the three panel
 * blocks once, clear them and build each occupied slot's panel (layout 1
 * for `mode` 2, where only characters with a gear qualify; `mode` then
 * becomes that layout flag); then place the three target cursor quads. */
void func_801DB5E4(u8 mode) {
    MenuAnchor *xs;
    MenuAnchor *ys;
    MenuPanel *panel;
    void *block;
    s32 i;
    u8 ok;
    u8 id;

    if (D_801E9785 == 0) {
        for (i = 0; i < 3; i++) {
            block = func_80031BDC(0xbec, 0);
            D_800625A0->panels[i] = block;
            bzero(block, 0xbec);
        }
        D_801E9785 = 1;
    }
    for (i = 0; i < 3; i++) {
        bzero(D_800625A0->panels[i], 0xbec);
    }
    if (mode != 2) {
        xs = D_801EA054;
        ys = D_801EA0DC;
        mode = 0;
    } else {
        xs = D_801EA098;
        ys = D_801EA120;
        mode = 1;
    }
    for (i = 0; i < 3; i++) {
        panel = D_800625A0->panels[i];
        id = D_800625A0->party->ids[i];
        ok = 1;
        if (id != 0xff) {
            if (mode) {
                ok = D_8006D8A0[id].gear != 0xff;
            }
            if (ok) {
                func_801CE0CC(panel, id, i, xs, ys, mode);
            }
        } else {
            panel->shown = 0;
        }
    }
    D_800625A0->party->unk46 = 1;
    for (i = 0; i < 3; i++) {
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x0 = 0x90;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y0 = i * 0x38 + 0x30;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x1 = 0xa0;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y1 = i * 0x38 + 0x30;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x2 = 0x90;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y2 = i * 0x38 + 0x40;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x3 = 0xa0;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y3 = i * 0x38 + 0x40;
    }
}

/* Use the item at visible entry `entry` of scroll row `row`: for an item
 * usable here, select targets (the whole party for all-target items, else
 * the cursor's slot; left/right move it) and use the item on them on
 * confirm until it runs out or the player cancels. Returns the targets
 * marked last (0 when cancelled or unusable). */
#ifdef NON_MATCHING
/* Register allocation differs: the original spills `row` and keeps the
 * inventory index in a saved register. */
u8 func_801DB920(s32 row, s32 entry) {
    u16 marks;
    u8 running;
    u8 redraw;
    s32 slot;
    u8 used;
    s32 all;
    s32 i;
    MenuItem *item;

    marks = 0;
    running = 1;
    slot = D_800625A0->firstMember;
    D_801E9785 = 0;
    item = &D_800625A0->tables->items[D_8006F65A[row * 2 + entry]];
    redraw = 1;
    if (item->use & 0x80) {
        marks = 1;
        if (item->use & 0x20) {
            marks = D_80059171 != 0;
        }
    }
    all = item->target & 1;
    if (marks) {
        func_801D397C(2, 0x10, 0xe, 0x90, 0xb0, 0, 0, 4, 0);
        while (running) {
            func_801C7BF4();
            marks = 0;
            if (redraw) {
                redraw = 0;
                func_801DA5BC(row);
                func_801DA9A8(entry, row);
                func_801DB39C(1);
                func_801DB5E4(0);
            }
            D_800625A0->markers->visible[0] = D_800625A0->markers->visible[1] = D_800625A0->markers->visible[2] = 0;
            if (all) {
                for (i = 0; i < 3; i++) {
                    if (D_800625A0->party->ids[i] != 0xff) {
                        marks |= 1 << i;
                        D_800625A0->markers->visible[i] = 1;
                    }
                }
            } else {
                marks = 1 << slot;
                D_800625A0->markers->visible[slot] = 1;
            }
            D_800625A0->party->unk2F = 1;
            if (INVENTORY->counts[row * 2 + entry] == 0) {
                running = 0;
            }
            if (!running) {
                break;
            }
            switch (D_800625A0->input) {
            case 4:
                used = 0;
                for (i = 0; i < 3; i++) {
                    if (func_801C865C(marks, i) &&
                        func_801E31C0(D_800625A0->tables, D_800625A0->party->ids[i], D_8006F65A[row * 2 + entry]) == 0) {
                        used |= 1;
                    }
                }
                if (used) {
                    func_801C8574(0x37);
                    redraw = 1;
                    if (--INVENTORY->counts[row * 2 + entry] == 0) {
                        INVENTORY->ids[row * 2 + entry] = 0;
                    }
                } else {
                    func_801C8574(4);
                    redraw = 1;
                }
                break;
            case 5:
                running = 0;
                marks = 0;
                break;
            case 1:
                slot = func_801D9704(slot, 0, 0);
                break;
            case 3:
                slot = func_801D9704(slot, 1, 0);
                break;
            }
        }
        D_800625A0->markers->visible[0] = D_800625A0->markers->visible[1] = D_800625A0->markers->visible[2] = 0;
        func_801DB39C(0);
        D_800625A0->party->unk46 = 0;
        func_801C7BF4();
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->panels[i]);
        }
        D_801E9785 = 0;
        func_801D4EA0(2);
    }
    return marks;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB920);
#endif

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

/* The item screen: a two-column list of eight rows scrolled over the
 * inventory with a cursor, the selected entry's description and its
 * windows. Confirm selects an entry, uses it when selected again or swaps
 * it with the selected one; cancel clears the selection or leaves. */
#ifdef NON_MATCHING
/* Cannot match as C in this unit yet: GCC 8-aligns the input jump table
 * (the original's is at 801c50fc, only 4-aligned); the case-4 tails also
 * cross-jump differently. */
u8 func_801DBE54(void) {
    u8 running;
    u8 windows;
    s32 scroll;
    s32 scrollShown;
    s32 cursor;
    s32 cursorShown;
    s32 selected;
    s32 next;

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
    do {
        func_801C7BF4();
        if (scroll != scrollShown) {
            func_801DA5BC(scroll);
            scrollShown = scroll;
            func_801D3344(0xc, (u16)D_801EA72C * scroll / 100 + 0x12, (u16)D_801EA724);
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
            } else if (scroll * 2 + cursor == selected) {
                if (func_801DB920(scroll, cursor)) {
                    scrollShown = 0xff;
                    cursorShown = 0xff;
                }
                selected = 0xff;
            } else {
                func_801DBD4C(scroll * 2 + cursor, selected);
                scrollShown = 0xff;
                cursorShown = 0xff;
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
            next = cursor + 2;
            if (next >= 16) {
                if (D_801EA728 < ++scroll) {
                    scroll--;
                }
            } else {
                cursor = next;
            }
            cursorShown = 0xff;
            break;
        case 3:
            next = cursor - 2;
            if (next < 0) {
                if (--scroll < 0) {
                    scroll++;
                }
            } else {
                cursor = next;
            }
            cursorShown = 0xff;
            break;
        case 0:
            next = cursor + 1;
            if (next >= 16) {
                if (D_801EA728 < ++scroll) {
                    scroll--;
                } else {
                    cursor = 14;
                }
            } else {
                cursor = next;
            }
            cursorShown = 0xff;
            break;
        case 2:
            next = cursor - 1;
            if (next < 0) {
                if (--scroll < 0) {
                    scroll++;
                } else {
                    cursor = 1;
                }
            } else {
                cursor = next;
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
    } while (running);
    func_801D2484();
    func_801DB340(0);
    func_801DB340(1);
    func_801E8044(8, D_800625A0->party->unk38);
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DBE54);
#endif

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

/* Show the description of file list row `row` for party slot `slot` (`kind`
 * 0 the character's, 1 the gear's, 2 the gear's paired rows): the entry's
 * two text lines, a copy of its name and its target labels; an unused row
 * hides them. */
#ifdef NON_MATCHING
/* Differs in the first switch's tail merging and register allocation. */
void func_801DCE60(u8 slot, u8 row, u8 kind) {
    RECT rect;
    MenuEffect *effect;
    u8 *image;
    u16 text;
    u16 target;
    u8 all;
    s32 i;

    switch (kind) {
    case 0:
        text = (D_800625A0->party->ids[slot] << 5) + row * 2;
        break;
    case 1:
        text = (D_8006D8A0[D_800625A0->party->ids[slot]].gear << 5) + row * 2;
        break;
    case 2:
        text = D_8006D8A0[D_800625A0->party->ids[slot]].gear * 8 + (row & 0xfe);
        break;
    }
    if (D_800625A0->block430->shown[row] != 0) {
        image = func_80031BDC(0x618, 0);
        bzero(image, 0x618);
        D_800625A0->block430->extra[0].width =
            func_80034EAC(func_80033728(D_800625A0->block430->texts, text), image, 0x39, 0);
        D_800625A0->block430->extra[1].width =
            func_80034EAC(func_80033728(D_800625A0->block430->texts, text + 1), image, 0x39, 1);
        rect.x = 0x140;
        rect.y = 0x4e;
        rect.w = 0x3c;
        rect.h = 0xd;
        LoadImage(&rect, image);
        DrawSync(0);
        func_801E7C50(&D_800625A0->block430->extra[0], 0, 0, 0);
        func_801E920C(&D_800625A0->block430->extra[0].polys[D_800625A0->bufferIndex], 0x1c, 0x9e, 0, 0x4e,
                      D_800625A0->block430->extra[0].width, 0xd);
        func_801C851C(D_800625A0->block430->extra[0].verts, 0x1c, 0x9e, D_800625A0->block430->extra[0].width, 0xd);
        func_801E7C50(&D_800625A0->block430->extra[1], 1, 0, 0);
        func_801E920C(&D_800625A0->block430->extra[1].polys[D_800625A0->bufferIndex], 0x1c, 0xae, 0, 0x4e,
                      D_800625A0->block430->extra[1].width, 0xd);
        func_801C851C(D_800625A0->block430->extra[1].verts, 0x1c, 0xae, D_800625A0->block430->extra[1].width, 0xd);
        func_800320E8(image);
        memmove(&D_800625A0->block430->headA, &D_800625A0->block430->names[row], sizeof(MenuLabelSlot));
        func_801C851C(D_800625A0->block430->headA.verts, 0x12, 0x8e, D_800625A0->block430->names[row].width, 0xd);
        (D_800625A0->block430->headA.polys + D_800625A0->bufferIndex)->r0 = 0x80;
        (D_800625A0->block430->headA.polys + D_800625A0->bufferIndex)->g0 = 0x80;
        (D_800625A0->block430->headA.polys + D_800625A0->bufferIndex)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->block430->headA.polys[D_800625A0->bufferIndex], 0);
        func_801E8044(8, D_800625A0->party->unk38);
        switch (kind) {
        case 0:
            effect = D_800625A0->tables->effects[D_800625A0->party->ids[slot]] + row;
            effect += 22;
            break;
        case 1:
            effect = D_800625A0->tables->effects[11 + D_8006D8A0[D_800625A0->party->ids[slot]].gear] + row;
            effect += 21;
            break;
        case 2:
            effect = D_800625A0->tables->effects[11 + D_8006D8A0[D_800625A0->party->ids[slot]].gear] + (row >> 1);
            effect += 37;
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
        func_801E8070(8, D_800625A0->labels10E0, D_801EA550, D_801E9EA0, D_800625A0->party->unk38, all, 0, 2);
        func_801E8070(8, D_800625A0->labels10E0, D_801EA550, D_801E9EA0, D_800625A0->party->unk38,
                      (effect->target & 3) + 3, 0, 2);
        D_800625A0->block430->headA.count = D_800625A0->bufferIndex;
        D_800625A0->block430->headB.count = D_800625A0->bufferIndex;
        D_800625A0->block430->extra[0].count = D_800625A0->bufferIndex;
        D_800625A0->block430->extra[1].count = D_800625A0->bufferIndex;
        D_800625A0->block430->extraShown = 1;
        D_800625A0->party->unk38[6] = 1;
        D_800625A0->party->unk38[7] = 1;
    } else {
        D_800625A0->block430->extraShown = 0;
        for (i = 0; i < 6; i++) {
            D_800625A0->party->unk38[i] = 0;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DCE60);
#endif

/* Shade the file list screen's texts by `mode` (801e8eac): windows 5 and 6,
 * each built name and value of the first twelve rows, the cursor, the
 * heading and the two extra labels. */
void func_801DD5E8(u8 mode) {
    s32 i;

    func_801E8F60(5, mode);
    func_801E8F60(6, mode);
    for (i = 0; i < 12; i++) {
        if (D_800625A0->block430->names[i].polys[D_800625A0->block430->names[i].count].r0 != 0x20) {
            func_801E8EAC(&D_800625A0->block430->names[i].polys[D_800625A0->block430->names[i].count], mode);
            func_801E8EAC(&D_800625A0->block430->values[i].polys[D_800625A0->block430->values[i].count], mode);
        }
    }
    func_801E8EAC(&D_800625A0->blocks444[0]->polys[D_800625A0->blocks444[0]->count], mode);
    func_801E8EAC(&D_800625A0->block430->headA.polys[D_800625A0->block430->headA.count], mode);
    for (i = 0; i < 2; i++) {
        func_801E8EAC(&D_800625A0->block430->extra[i].polys[D_800625A0->block430->extra[i].count], mode);
    }
}

/* Use art `row` of party slot `slot` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list) from the menu: select the targets (the
 * whole party for all-target arts, else the cursor's slot) and use it on
 * confirm while its cost can be paid, until cancelled. */
#ifdef NON_MATCHING
/* Same shape as the original; register allocation and a few schedules differ. */
void func_801DD790(u8 slot, s32 row, u8 kind) {
    MenuEffect *effect;
    s32 x;
    s32 all;
    s32 i;
    s32 sound;
    u16 now;
    u16 max;
    u8 redraw;
    u8 cursor;
    u8 targets;
    u8 used;

    redraw = 1;
    x = 0;
    cursor = D_800625A0->firstMember;
    switch (kind) {
    case 0:
        effect = D_800625A0->tables->effects[D_800625A0->party->ids[slot]] + row;
        effect += 22;
        break;
    case 1:
        effect = D_800625A0->tables->effects[11 + D_8006D8A0[D_800625A0->party->ids[slot]].gear] + row;
        effect += 21;
        break;
    case 2:
        x = 0x18;
        cursor = slot;
        effect = D_800625A0->tables->effects[11 + D_8006D8A0[D_800625A0->party->ids[slot]].gear] + row;
        effect += 37;
        break;
    }
    func_801D397C(2, 0x10, 0xe, x + 0x90, 0xb0, 0, 0, 4, 0);
    all = effect->target & 1;
    targets = 1;
    while (targets) {
        func_801C7BF4();
        targets = 0;
        if (redraw) {
            func_801DC3D8(slot, kind);
            func_801DCE60(slot, row, kind);
            func_801DD5E8(1);
            func_801DB5E4(kind);
            redraw = 0;
        }
        D_800625A0->markers->visible[0] = D_800625A0->markers->visible[1] = D_800625A0->markers->visible[2] = 0;
        if (all) {
            for (i = 0; i < 3; i++) {
                if (D_800625A0->party->ids[i] != 0xff) {
                    targets |= 1 << i;
                    D_800625A0->markers->visible[i] = 1;
                }
            }
        } else {
            targets = 1 << cursor;
            D_800625A0->markers->visible[cursor] = 1;
        }
        D_800625A0->party->unk2F = 1;
        if (kind != 2) {
            i = D_8006D8A0[D_800625A0->party->ids[slot]].ether - effect->cost;
        } else {
            i = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk38 - effect->gearCost;
        }
        if (i < 0) {
            targets = 0;
        }
        if (!targets) {
            break;
        }
        switch (D_800625A0->input) {
        case 4:
            used = 0;
            for (i = 0; i < 3; i++) {
                if (func_801C865C(targets, i)) {
                    if (kind != 2) {
                        now = D_8006D8A0[D_800625A0->party->ids[i]].hp;
                        max = D_8006D8A0[D_800625A0->party->ids[i]].hpMax;
                    } else {
                        /* the maximum is read from party slot `row` */
                        now = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[i]].gear].unk60;
                        max = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[row]].gear].unk64;
                    }
                    if (now != max) {
                        used = 1;
                        func_801E35BC(D_800625A0->tables, D_800625A0->party->ids[slot], D_800625A0->party->ids[i],
                                      row, kind);
                    }
                }
            }
            if (used) {
                if (kind != 2) {
                    D_8006D8A0[D_800625A0->party->ids[slot]].ether -= effect->cost;
                } else {
                    D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk38 -= effect->gearCost;
                }
                sound = 0x37;
            } else {
                sound = 4;
            }
            redraw = 1;
            func_801C8574(sound);
            break;
        case 5:
            targets = 0;
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
    D_800625A0->markers->visible[0] = D_800625A0->markers->visible[1] = D_800625A0->markers->visible[2] = 0;
    func_801DD5E8(0);
    D_800625A0->party->unk46 = 0;
    func_801C7BF4();
    if (D_801E9785 != 0) {
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->panels[i]);
        }
        D_801E9785 = 0;
    }
    func_801D4EA0(2);
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DD790);
#endif

#ifdef NON_MATCHING
/* Cannot match as C in this unit yet: GCC 8-aligns the input jump table (the
 * original's is at 801c512c, only 4-aligned). */
/* The arts screen of party slot `slot` (`kind` 0 the character's, 1 its
 * gear's, 2 the gear's other list): a cursor over twelve rows (two columns,
 * one for kind 2), the selected art's description; confirm uses a usable
 * art, 9/10 switch party members, cancel leaves. `zoom` first zooms in. */
void func_801DDF24(u8 slot, u8 zoom, u8 kind) {
    s32 cursor;
    s32 cursorShown;
    u8 slotShown;
    u8 windows;
    u8 running;

    windows = 1;
    cursor = 0;
    cursorShown = 0xff;
    slotShown = 0xff;
    running = 1;
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
            windows = 0;
            if (zoom) {
                func_801D1E80();
                func_801D29A8(0, 0);
                func_801C7BF4();
            }
            D_800625A0->party->redraw6 = 0;
            D_800625A0->party->unk20[1] = 0;
        }
        switch (D_800625A0->input) {
        case 4:
            if (D_800625A0->block430->shown[cursor] & 0x80) {
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
    func_801E8044(8, D_800625A0->party->unk38);
    func_801DB340(0);
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DDF24);
#endif

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

/* Commit the equipment change of part `part` of party slot `slot` (with
 * `special` a special part, with `gear` the gear's): the newly equipped part
 * leaves its inventory list and the replaced one kept by 801df5d0 joins it
 * (worn special parts are dropped). Without a new part the kept one goes
 * back. Returns 1 when character 4 changed weapon. */
#ifdef NON_MATCHING
/* Differs: the original's frame is 0x28 (this builds 0x18) and it converts
 * `gear` at each use; the inventory update code otherwise has the same shape. */
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
    ids = D_8006F754;
    result = 0;
    if (!gear) {
        ids = D_8006F3D0;
    }
    counts = ids - 100;
    length = 100;
    if (special) {
        if (!gear) {
            at = &D_8006D8A0[D_800625A0->party->ids[slot]].specials[part];
            selected = *at;
            kept = D_800625A0->labels360->parts[1][part];
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
            at = &D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk4[part];
            selected = *at;
            kept = D_800625A0->labels360->parts[1][part];
            if (selected == 0) {
                *at = kept;
                swap = 0;
            } else {
                if (D_8006F8EA[*at] < 100) {
                    kept = 0;
                }
                D_8006F8EA[*at] = 100;
            }
        }
    } else if (!gear) {
        if (part == 0) {
            id = D_800625A0->party->ids[slot];
            selected = D_8006D8A0[id].weapons[0];
            kept = D_800625A0->labels360->parts[0][0];
            if (selected == 0) {
                D_8006D8A0[id].weapons[0] = kept;
                swap = 0;
            } else if (id == 4) {
                result = 1;
            }
        } else {
            ids = D_8006F4FC;
            counts = D_8006F434;
            kept = D_800625A0->labels360->parts[2][part - 1];
            selected = D_8006D8A0[D_800625A0->party->ids[slot]].accessories[part - 1];
            length = 200;
        }
    } else {
        if (part == 0) {
            id = D_800625A0->party->ids[slot];
            selected = D_8006DFAC[D_8006D8A0[id].gear].unkC[0];
            kept = D_800625A0->labels360->parts[0][0];
            if (selected == 0) {
                D_8006DFAC[D_8006D8A0[id].gear].unkC[0] = kept;
                swap = 0;
            } else if (id == 4) {
                result = 1;
            }
        } else {
            ids = D_8006F84E;
            counts = D_8006F7B8;
            length = 150;
            kept = D_800625A0->labels360->parts[2][part - 1];
            selected = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk9[part - 1];
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
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF0D4);
#endif

/* Keep the shown stats and the equipment of party slot `slot` (its gear's
 * parts when `gear`) in the equipment screen's block. */
void func_801DF5D0(u8 slot, u8 gear) {
    s32 i;

    for (i = 0; i < 9; i++) {
        D_800625A0->labels360->stats[i] = D_800625A0->tables->shown[i];
    }
    if (!gear) {
        for (i = 0; i < 5; i++) {
            D_800625A0->labels360->parts[0][i] = D_8006D8A0[D_800625A0->party->ids[slot]].weapons[i];
            D_800625A0->labels360->parts[1][i] = D_8006D8A0[D_800625A0->party->ids[slot]].specials[i];
            D_800625A0->labels360->parts[2][i] = D_8006D8A0[D_800625A0->party->ids[slot]].accessories[i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            D_800625A0->labels360->parts[0][i] = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unkC[i];
            D_800625A0->labels360->parts[1][i] = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk4[i];
            D_800625A0->labels360->parts[2][i] = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk9[i];
        }
    }
}

/* Put back the equipment of party slot `slot` (its gear's parts when `gear`)
 * kept by 801df5d0; the equipment screen was cancelled. */
void func_801DF890(u8 slot, u8 gear) {
    s32 i;

    if (!gear) {
        for (i = 0; i < 5; i++) {
            D_8006D8A0[D_800625A0->party->ids[slot]].weapons[i] = D_800625A0->labels360->parts[0][i];
            D_8006D8A0[D_800625A0->party->ids[slot]].specials[i] = D_800625A0->labels360->parts[1][i];
        }
        for (i = 0; i < 3; i++) {
            D_8006D8A0[D_800625A0->party->ids[slot]].accessories[i] = D_800625A0->labels360->parts[2][i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unkC[i] = D_800625A0->labels360->parts[0][i];
            D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk4[i] = D_800625A0->labels360->parts[1][i];
        }
        for (i = 0; i < 3; i++) {
            D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk9[i] = D_800625A0->labels360->parts[2][i];
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
                D_8006D8A0[D_800625A0->party->ids[slot]].weapons[0] = D_801EA730[top + row];
                break;
            case 1:
            case 2:
            case 3:
                D_8006D8A0[D_800625A0->party->ids[slot]].accessories[part - 1] = D_801EA730[top + row];
                break;
            }
        } else {
            D_8006D8A0[D_800625A0->party->ids[slot]].specials[part] = D_801EA730[top + row];
        }
    } else {
        if (!special) {
            switch (part) {
            case 0:
                D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unkC[0] = D_801EA730[top + row];
                break;
            case 1:
            case 2:
            case 3:
                D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk9[part - 1] = D_801EA730[top + row];
                break;
            }
        } else {
            D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk4[part] = D_801EA730[top + row];
        }
    }
}

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

/* Show the three-line description of equipment list entry `top` + `row` (or,
 * with `current`, of the part equipped) for part `part` of party slot
 * `slot` (`special` a special part, `gear` the gear's parts). */
#ifdef NON_MATCHING
/* Differs: GCC strength-reduces the line's y position into a saved register
 * (the original recomputes it) and combines the kind with an `or`. */
void func_801DFF5C(s32 part, s32 row, s32 top, u8 special, u8 gear, u8 current, u8 slot) {
    RECT rect;
    u8 *table;
    u8 *image;
    s32 line;
    s32 kind;
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
        switch ((u8)(kind + gear * 2)) {
        case 0:
            table = D_800625A0->block434->texts[0];
            if (current) {
                if (!special) {
                    id = D_8006D8A0[D_800625A0->party->ids[slot]].weapons[0];
                } else {
                    id = D_8006D8A0[D_800625A0->party->ids[slot]].specials[part];
                }
            }
            break;
        case 1:
            table = D_800625A0->block434->texts[1];
            if (current) {
                id = D_8006D8A0[D_800625A0->party->ids[slot]].accessories[part - 1];
            }
            break;
        case 2:
            table = D_800625A0->block434->texts[2];
            if (current) {
                if (!special) {
                    id = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unkC[0];
                } else {
                    id = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk4[part];
                }
            }
            break;
        case 3:
            table = D_800625A0->block434->texts[3];
            if (current) {
                id = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk9[part - 1];
            }
            break;
        }
        if (id != 0) {
            image = func_80031BDC(0x3f6, 0);
            bzero(image, 0x3f6);
            for (line = 0; line < 3; line++) {
                D_800625A0->block434->extra[line].width =
                    func_80034EAC(func_80033728(table, id * 3 + line), image, 0x24, 0);
                rect.x = ((line + 8) & 1) * 0x18 + 0x180;
                rect.y = (line + 8) / 2 * 0xd + 0x80;
                rect.w = 0x28;
                rect.h = 0xd;
                LoadImage(&rect, image);
                DrawSync(0);
                func_801E7C50(&D_800625A0->block434->extra[line], line + 8, 0x80, 0x81);
                func_801C851C(D_800625A0->block434->extra[line].verts, 0x10, (line * 0x10 + 0x96) & ~1,
                              D_800625A0->block434->extra[line].width, 0xd);
                D_800625A0->block434->extra[line].count = D_800625A0->bufferIndex;
            }
            D_800625A0->block434->extraShown = 1;
            func_800320E8(image);
            return;
        }
    }
    D_800625A0->block434->extraShown = 0;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DFF5C);
#endif

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E0434);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1014);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1418);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1544);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1AC8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E20C8);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E3A80);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4754);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E56E8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5924);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5B88);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5E4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E61B0);

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

/* Print the character at `s`: ASCII is converted to its two-byte code
 * (control characters to a space), two-byte codes pass through. */
void func_801E65E4(u8 *s) {
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
    func_800405C4(lo | (hi << 8));
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6668);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E68AC);

/* Set up view `index` of the block at +34c from its sheet image (14e + set). */
void func_801E6AE8(u8 index, MenuViewSet *set) {
    func_8002675C(D_800625A0->sheet, set->images[index] + 0x14e, &D_800625A0->block34C->views[index],
                  D_800625A0->bufferIndex, D_801EA004[index], D_801EA010[index], 0x1000);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6B70);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6CFC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6F5C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E71B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E733C);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E7C50);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E7E68);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8474);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E86C8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8978);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8B4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8DA8);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8F60);

/* Make `poly` semi-transparent, textured without shading, at neutral colour. */
void func_801E91C4(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Place `poly` at (x, y) with size (w, h) and texture origin (u, v). */
#ifdef NON_MATCHING
void func_801E920C(POLY_FT4 *poly, s32 x, s32 y, s32 u, s32 v, s32 w, s32 h) {
    poly->x2 = poly->x0 = x;
    poly->y1 = poly->y0 = y;
    poly->x3 = poly->x1 = x + w;
    poly->y3 = poly->y2 = y + h;
    poly->u2 = poly->u0 = u;
    poly->v1 = poly->v0 = v;
    poly->u3 = poly->u1 = u + w;
    poly->v3 = poly->v2 = v + h;
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E920C);
#endif

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

/* Run 8004c398 on a handle opened by 8004c318(`arg0`, 0, 0), then close it. */
void func_801E9340(s32 arg0, s32 arg1, s32 arg2) {
    s32 handle;

    handle = PCopen(arg0, 0, 0);
    func_8004C398(handle, arg1, arg2);
    PCclose(handle);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E93A0);
