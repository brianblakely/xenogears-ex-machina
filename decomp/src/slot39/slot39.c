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
            func_801E8018(8, D_800625A0->labelSlots[0], D_801EA530, D_800625A0->party->labels);
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
            func_801E8070(8, D_800625A0->labelSlots[0], D_801EA528, D_801E9E64, D_800625A0->party->labels,
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
    func_801E8018(8, D_800625A0->labelSlots[0], D_801EA530, D_800625A0->party->labels);
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
            func_801E8070(8, D_800625A0->labelSlots[0], D_801EA530, D_801E9E84, D_800625A0->party->labels,
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
        func_8003F8E8(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate and clear (nonzero) or free (zero) the party block. */
void func_801C5BB8(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6c, 0);
        D_800625A0->party = block;
        func_8003F8E8(block, 0x6c);
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate and clear (nonzero) or free (zero) the screen image block. */
void func_801C5C1C(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->screenImages = block;
        func_8003F8E8(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->screenImages);
    }
}

/* Allocate and clear (nonzero) or free (zero) the 140c-byte block at +354. */
void func_801C5C80(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140c, 0);
        D_800625A0->block354 = block;
        func_8003F8E8(block, 0x140c);
    } else {
        func_800320E8(D_800625A0->block354);
    }
}

/* Allocate and clear (nonzero) or free (zero) the data table directory. */
void func_801C5CE4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xcc, 0);
        D_800625A0->tables = block;
        func_8003F8E8(block, 0xcc);
    } else {
        func_800320E8(D_800625A0->tables);
    }
}

/* Allocate and clear (nonzero) or free (zero) the first field-menu block. */
void func_801C5D48(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x328, 0);
        D_800625A0->fieldMenu = block;
        func_8003F8E8(block, 0x328);
    } else {
        func_800320E8(D_800625A0->fieldMenu);
    }
}

/* Allocate and clear (nonzero) or free (zero) the second field-menu block. */
void func_801C5DAC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x374, 0);
        D_800625A0->fieldMenu2 = block;
        func_8003F8E8(block, 0x374);
    } else {
        func_800320E8(D_800625A0->fieldMenu2);
    }
}

/* Allocate and clear (nonzero) or free (zero) the shared primitive block. */
void func_801C5E10(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15c, 0);
        D_800625A0->primitives = block;
        func_8003F8E8(block, 0x15c);
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
            func_8003F8E8(block, 0x127c);
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
    func_8003F8E8(block, 0x14c);
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
    func_800320E8(D_800625A0->labelPixels);
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
    RECT unused; /* the original frame reserves a second rectangle */
    u16 *clut;

    clut = func_80031BDC(0x20, 0);
    func_8003F8E8(clut, 0x20);
    clut[1] = 0x7fff;
    rect.x = 0;
    rect.y = 0x1c0;
    rect.w = 0x10;
    rect.h = 1;
    func_80044894(&rect, clut);
    func_800445D0(0);
    func_800320E8(clut);
}

/* Set up the label text: the font position, the label pixel block and the
 * four label image records, and the label palette. */
void func_801C6E0C(void) {
    func_80033698(0, 0x1d1);
    D_800625A0->labelPixels = func_80031BDC(0x38e, 0);
    func_801E7E68(D_800625A0->labelImages, D_801EA524, 0, 4);
    func_801C6D90();
}

/* Read the four sprite sheet records used by the menu. */
void func_801C6E68(void) {
    s32 unused[10]; /* the original frame reserves 40 unused bytes */

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
    func_80044AD8(D_800625A0->current->ot, 16);
    func_8001BD40(0, 0xff);
    func_801D1D40();
    D_800625A0->frameCounter++;
    func_801C7F34(D_80059488);
    func_801D2968();
    func_801D1CA0();
    other = D_800625A0->bufferIndex == 0;
    func_800445D0(0);
    func_8004B54C(0);
    func_80044C44(D_800625A0->current->draw);
    func_80044E9C(D_800625A0->current->disp);
    func_8004495C(&D_800625A0->screenImages->copy, 0, other * 0xe0);
    func_80044BD0(&D_800625A0->current->ot[15]);
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
    func_80043CC4(poly);
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
#ifdef NON_MATCHING
void func_801C8324(u8 slot) {
    MenuMover *mover;
    s32 i;

    mover = &D_800625A0->movers[slot];
    for (i = 0; i < D_800625A0->movers[slot].speed; i++) {
        if (D_800625A0->movers[slot].negX) {
            D_800625A0->movers[slot].accX -= D_800625A0->movers[slot].stepX;
        } else {
            D_800625A0->movers[slot].accX += D_800625A0->movers[slot].stepX;
        }
        if (mover->negY) {
            mover->accY -= mover->stepY;
        } else {
            mover->accY += mover->stepY;
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
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8324);
#endif

/* Set the four corners of a screen rectangle as vertices centred on (a0, 70). */
void func_801C851C(SVECTOR *v, u16 x, u16 y, s32 w, s32 h) {
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
    func_800404D4();
    func_80040484(D_800625A0->card->events[0]);
    func_80040484(D_800625A0->card->events[1]);
    func_80040484(D_800625A0->card->events[2]);
    func_80040484(D_800625A0->card->events[3]);
    func_800404E4();
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
            func_8004B54C(0);
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

    fd = func_80040534(name, 3);
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
#ifdef NON_MATCHING
void func_801C9270(s32 port) {
    s32 i;
    s32 j;
    u8 match;
    MenuCard *card;
    MenuCardFile *file;
    u8 *header;

    for (i = 0; i < 16; i++) {
        D_800625A0->card->ours[port * 16 + i] = 0;
    }
    for (i = port * 16; i < port * 16 + 15; i++) {
        card = D_800625A0->card;
        match = 1;
        file = &card->files[card->fileSlots[i]];
        for (j = 0; j < 12; j++) {
            if (file->name[j] != card->prefix[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            D_800625A0->card->ours[i] = 1;
            header = D_800625A0->card->headers[D_800625A0->card->fileSlots[i]];
            D_801EA6F4 = header + 0x100;
            D_801EA6D0[port * 16 + header[0x123]] = 1;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C9270);
#endif

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CACF8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CADB0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CAE08);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB184);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB28C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB304);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB8AC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CB9E8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CBA4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CBD90);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CC6D8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CD2AC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CD710);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CD81C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CDB1C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CDC6C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE0CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE198);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE2B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE338);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE3C8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE464);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE540);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE660);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CE860);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CEB5C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CEBB4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CEC40);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF308);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF37C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF5E4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CF8D8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CFB48);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801CFF64);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D01D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D02D8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0954);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D09F0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0C78);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0D90);

/* A short busy delay (eight iterations). */
void func_801D0E20(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0E38);

/* A short busy delay (six iterations). */
void func_801D0EBC(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0ED4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0F54);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0FD4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1030);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D10DC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1160);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D11F0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1258);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D12D4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D13F8);

/* While party flag +49 is set, draw the sprites of the block at +43c. */
void func_801D1464(void) {
    u8 *block;

    if (D_800625A0->party->unk49 != 0) {
        block = D_800625A0->block43C;
        func_801CE198(1, block + 0x50, block, block[0x70]);
    }
}

/* While party flag +53 is set, draw the sprites of the block at +440. */
void func_801D14B0(void) {
    u8 *block;

    if (D_800625A0->party->unk53 != 0) {
        block = D_800625A0->block440;
        func_801CE198(4, block + 0x140, block, block[0x1c0]);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D14FC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1640);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D17C4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1914);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1AAC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1B20);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1BE8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1C48);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1CA0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1D40);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D22F4);

/* Clear the party block flag at +2f. */
void func_801D2484(void) {
    D_800625A0->party->unk2F = 0;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D249C);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D28FC);

/* Draw the element at (d0, ca) while party flag 6 is set. */
void func_801D2968(void) {
    if (D_800625A0->party->redraw6 != 0) {
        func_801D5CF8(0xd0, 0xca);
    }
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D29A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2D38);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2EC0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2F4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D32B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3344);

/* Clear party flag +49 and free the block at state +43c. */
void func_801D3444(void) {
    D_800625A0->party->unk49 = 0;
    func_800320E8(D_800625A0->block43C);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3488);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3674);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D36E0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D397C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3B00);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3C4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3DB0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3FF8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D433C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D4688);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D49D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D4D1C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D4EA0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D4F2C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D50EC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D51EC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D53D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D55B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5794);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5A50);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5BA4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5CF8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D5ED4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D6194);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D6338);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D680C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D6CF4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7154);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D74EC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7884);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7C3C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7CFC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D7F50);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D827C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D83AC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D84B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D85DC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D8644);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D8DE4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D8EA4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9704);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9808);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9B08);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9C84);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9E3C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9F34);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D9F98);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA4A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA518);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA5BC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DA9A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB02C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB0A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB340);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB39C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB5E4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DB920);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DBD4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DBDB4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DBE54);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DC1D4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DC2CC);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE2C8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE36C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE400);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE474);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE5CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF0D4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF5D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DF890);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DFB68);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DFE2C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DFF5C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E0434);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E05D0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E0F78);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1014);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1398);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1418);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1544);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E1AC8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E20C8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2250);

/* Lay out the six labels of page `page` (D_801EA568) in label slot 18. */
void func_801E2324(u8 page) {
    func_801E8018(6, D_800625A0->labelSlots[0x12], D_801EA568 + page, &D_800625A0->party->unk54);
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2368);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E23CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2AE0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2B80);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2BE4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E3088);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E31C0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E35BC);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E41C0);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E42AC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E433C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4754);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4928);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4998);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4A28);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4D10);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5058);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5178);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E53CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E56E8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5924);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5ACC);

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
#ifdef NON_MATCHING
void func_801E6450(void) {
    void *block;

    block = func_80031BDC(0x2dc0, 0);
    D_800625A0->block34C = block;
    func_8003F8E8(block, 0x2dc0);
    func_801E5B88();
    func_801E5E4C();
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6450);
#endif

/* Free the 2dc0-byte block at +34c and clear party flag +b. */
void func_801E649C(void) {
    func_800320E8(D_800625A0->block34C);
    D_800625A0->party->unkB = 0;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E64E0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6544);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E65E4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6668);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E68AC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6AE8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6B70);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6CFC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6F5C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E71B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E733C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E76EC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E781C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E78C8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E7C50);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E7E68);

/* Lay out `count` labels from `table` into `labels` (the placement is unused). */
void func_801E8018(u8 count, u8 *labels, u8 *table, u8 *placement) {
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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8EAC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8F60);

/* Make `poly` semi-transparent, textured without shading, at neutral colour. */
void func_801E91C4(POLY_FT4 *poly) {
    func_80043BFC(poly, 1);
    func_80043C24(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E920C);

/* Initialise `poly` as an opaque textured quad at neutral colour. */
void func_801E927C(POLY_FT4 *poly) {
    func_80043CB0(poly);
    func_80043BFC(poly, 0);
    func_80043C24(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E92CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E9340);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E93A0);
