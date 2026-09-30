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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6400);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C65F4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6AA0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6D4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6D5C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6D90);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6E0C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6E68);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C6F70);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C72BC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C7B0C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C7BF4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C7D78);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C7F34);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C80B8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8164);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C81E0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8324);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C851C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8574);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C85C0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C85DC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C85F8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C861C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8640);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C865C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8678);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8694);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C87C4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C881C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C891C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8960);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8A10);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8BEC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8CA4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8D1C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8D78);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C8EE8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C9038);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C90B0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801C9270);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0E20);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0E38);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D0EBC);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1464);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D14B0);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1E80);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1EB0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D1EE0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D22C4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D22F4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2484);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D249C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D25E4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D261C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D28A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D28FC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2968);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D29A8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2D38);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2EC0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D2F4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D32B4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3344);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801D3444);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801DE29C);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E2324);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4170);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E41C0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E4258);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5B3C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5B88);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E5E4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E61B0);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E6450);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E649C);

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

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8018);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8044);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8070);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8474);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E86C8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8978);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8B4C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8DA8);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8EAC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E8F60);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E91C4);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E920C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E927C);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E92CC);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E9340);

INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39", func_801E93A0);
