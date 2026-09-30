#include "common.h"
#include "mode.h"

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80019524);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80019548);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80019560);

/* Boot: initialise the system libraries, the disc index and the heap, load and install the resident data files, then enter the first mode. */
void func_80019578(void) {
    RECT screen;
    void *file2;
    void *file3;
    void *file4;
    void *file5;
    void *data;
    s32 tag;

    func_8004B740();
    func_800443A8(0);
    func_8004C2F0(0);
    func_80044110(0);
    screen.x = 0;
    screen.y = 0;
    screen.w = 0x180;
    screen.h = 0x1E0;
    func_80044764(&screen, 0, 0, 0);
    func_800445D0(0);
    func_80044534(1);
    func_80048BC4();
    func_80036288();
    func_8004E794(1);
    func_8004E7E8();
    func_80040464();
    func_8004B7D0(func_8003634C);
    func_80031A68(func_8002DFE0(), (void *)0x801FC000);
    func_8004C548();
    func_80028230(D_80010004, D_80018004, D_80010000);
    func_80037B88(0);
    func_80028470(0, 1);

    file2 = func_80031BDC(func_80028738(2), 0);
    file3 = func_80031BDC(func_80028738(3), 0);
    file4 = func_80031BDC(func_80028738(4), 0);
    file5 = func_80031BDC(func_80028738(5), 0);
    func_800295D8(2, file2, 0, 0);
    func_800295D8(3, file3, 0, 0);
    func_800295D8(4, file4, 0, 0);
    func_800295D8(5, file5, 0, 0);
    func_80028A60(0);
    func_80037FD8(file2, 0);
    D_80059560 = func_80037FD8(file3, 0);
    func_80037FD8(file4, 0);
    D_800595AC = func_80037FD8(file5, 0);

    tag = func_80031B9C();
    func_80031BA8(6);
    data = func_80031BDC(func_80028738(6), 0);
    func_800295D8(6, data, 0, 0);
    func_80028A60(0);
    func_800324B8(0x30);
    func_80033558(func_80032E88(data, 1));
    func_800320E8(data);
    data = func_80031BDC(func_80028738(7), 0);
    func_800295D8(7, data, 0, 0);
    func_80028A60(0);
    func_800324B8(0x31);
    func_800335F4(func_80032E88(data, 1));
    func_800320E8(data);
    func_80031BA8(tag);

    func_8003BDFC(0x10);
    func_800320E8(file2);
    func_800320E8(file3);
    func_800320E8(file4);
    func_800320E8(file5);
    func_8001AADC();
    func_8001BB50();
    func_80024F20();
    func_800379B4(0);
    D_800592C0 = -1;
    D_800592BC = NULL;
    D_8004FE44 = 1;
    D_8004FE46 = 1;
    D_8004FE47 = 0;
    if (func_80028530() == 1) {
        D_8004FE45 = 0x10;
    } else {
        D_8004FE45 = 7;
    }
    if (func_80035734(0) != 0) {
        while (D_80059570 == 0x90C) {
            func_80035CDC();
        }
    }
    func_80019D48();
    func_8001B6BC();
    func_8001996C(6);
    func_80019ACC(0);
}

void func_80019964(void) {
}

/* Select the next mode; a different mode releases the cached mode block. */
void func_8001996C(s32 mode) {
    D_80018088 = mode;
    if (mode != D_800592C0) {
        if (D_800592BC != NULL) {
            func_800320E8(D_800592BC);
            D_800592BC = NULL;
        }
        D_800592C0 = -1;
    }
}

/* Load the mode's overlay file into a heap block (tag 6, from the top, quietly) unless it is already cached; returns the block. */
void *func_800199CC(s32 mode) {
    s32 tag;
    s32 quiet;
    s32 base;
    s32 index;

    if (D_800592C0 != mode) {
        D_800592C0 = mode;
        tag = func_80031B9C();
        func_800284B4(&base, &index);
        func_80031BA8(6);
        func_80028470(0, 1);
        quiet = func_80031BB4(1);
        D_800592BC = func_80031BDC(func_80028738(D_8004EAA0[mode]), 1);
        if (D_800592BC != NULL) {
            func_800295D8(D_8004EAA0[mode], D_800592BC, 0, 0);
        } else {
            D_800592C0 = -1;
        }
        func_80031BB4(quiet);
        func_80028470(base, index);
        func_80031BA8(tag);
    }
    return D_800592BC;
}

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018080);

/* Where a mode's overlay block is decoded. */
u8 *const D_80018084 = D_8006FAF0;

/* Mode dispatcher: report a fatal error (with the caller) if given, reset graphics and the heap, clear the next mode's BSS, load its overlay, then run it and dispatch again. */
void func_80019ACC(s32 error) {
    ModeEntry *mode;
    void *block;
    u32 unused[2]; /* an unused local the original frame reserves */
    u32 caller;

    if (error != 0) {
        __asm__ volatile("move $15, %0\n\tsw $31, 0($15)" : : "r"(&caller) : "$15");
        func_80019EF8(error, caller);
    }
    mode = &D_8001808C[D_80018088];
    func_80044110(1);
    func_800444D8(0);
    func_800363F0(0);
    func_800445D0(0);
    func_8004B54C(2);
    func_80031B10(mode->bss_end + 0x800);
    func_80019C7C();
    if (mode->loaded) {
        func_80019560(mode->bss_start, mode->bss_end);
        block = func_800199CC(D_80018088);
        func_80028A60(0);
        func_80032EB4(block, D_80018084);
        func_800445D0(0);
        func_8004B54C(0);
        func_800404D4();
        func_800445D0(0);
        func_8004B54C(0);
        func_80040454();
        func_800404E4();
    }
    func_80019548();
    func_80031B10(mode->bss_end + 4);
    func_80031A30();
    func_80035DB0();
    func_8001996C(0);
    mode->entry();
    func_80019ACC(0);
}

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018088);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_8001808C);

/* Write main RAM (2 MiB) to the development PC as c:\core. */
void func_80019C2C(void) {
    s32 fd;

    func_8004C38C();
    fd = func_8004C36C("c:\\core", 0);
    func_8004C470(fd, (void *)0x80000000, 0x200000);
    func_8004C338(fd);
}

/* Heap diagnostics, listed by the message table 8004f2c0. */
const char D_80018104[] = "LsFreeMem:This ptr isn't MCB";
const char D_80018124[] = "LsGetMem:MCB Broken";
const char D_80018138[] = "LsFreeMem:Can't Release NULL Pointer";
const char D_80018160[] = "LsGetMem:Memory Not Enough";
const char D_8001817C[] = "LsKernel:PC File Not Found";
const char D_80018198[] = "LsKernel:Program Not Defined";

/* Select heap owner tag 10 (clearing its word and the quiet flag). */
void func_80019C7C(void) {
    func_80032498(10, 0);
}

/* Soft reset while the reset button combination is held. */
void func_80019CA0(void) {
    if (D_80059570 == 0x90C) {
        func_80019CD0();
    }
}

/* Shut down the libraries and restart from the entry point. */
void func_80019CD0(void) {
    func_80040514();
    func_80044110(0);
    func_800283D4();
    func_80037DC0();
    func_8004D294();
    func_800363F0(0);
    func_800444D8(0);
    func_8004B7D0(NULL);
    func_80040ED4();
    func_800408F4();
    func_800404F4();
    func_80019524();
}

/* Boot logo: upload the logo image and its palette, then fade the logo sprite in, hold it and fade it out. */
void func_80019D48(void) {
    DRAWENV draw;
    DISPENV disp;
    SPRT logo;
    RECT rect;
    u8 *image;
    s32 level;
    s32 frame;

    func_80031BA8(6);
    image = func_80032E88(D_8004EABC, 1);
    rect.x = 0;
    rect.y = 0xF0;
    rect.w = 0x10;
    rect.h = 1;
    func_80044894(&rect, image + 0x14);
    rect.x = 0x280;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    func_80044894(&rect, image + 0x40);
    setSprt(&logo);
    logo.x0 = 0x20;
    logo.y0 = 0x58;
    logo.v0 = 0;
    logo.u0 = 0;
    logo.w = 0x100;
    logo.h = 0x30;
    logo.clut = func_80043A58(0, 0xF0);
    func_80043928(&draw, 0, 0, 0x140, 0xE0);
    func_800439E0(&disp, 0, 0, 0x140, 0xE0);
    func_80044C44(&draw);
    func_80044E9C(&disp);
    func_800445D0(0);
    for (level = 0; level < 0x80; level += 8) {
        logo.r0 = level;
        logo.g0 = level;
        logo.b0 = level;
        func_80044B70(&logo);
        func_8004B54C(0);
    }
    for (frame = 0x6D; frame != -1; frame--) {
        func_8004B54C(0);
    }
    for (level = 0x80; level >= 0; level -= 8) {
        logo.r0 = level;
        logo.g0 = level;
        logo.b0 = level;
        func_80044B70(&logo);
        func_8004B54C(0);
    }
    func_800320E8(image);
}

/* Fatal error screen: dump the heap log to the PC (or, without one, clear the screen red and hang), then print the error, its caller and heap details every frame forever. */
void func_80019EF8(s32 error, u32 caller) {
    DRAWENV draw[2];
    DISPENV disp[2];
    RECT rect;
    u32 unused[2]; /* an unused local the original frame reserves */
    s32 frame;
    s32 first;
    s32 second;

    frame = 0;
    if (D_80010000 != 0 && D_80010000 != -1) {
        func_80032E04("c:\\lserrmem.txt");
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x1E0;
        func_80044764(&rect, 0xFF, 0, 0);
        for (;;) {
        }
    }
    func_800322B4();
    func_80043928(&draw[0], 0, 0, 0x180, 0xF0);
    func_800439E0(&disp[0], 0, 0xF0, 0x180, 0xF0);
    func_80043928(&draw[1], 0, 0xF0, 0x180, 0xF0);
    func_800439E0(&disp[1], 0, 0, 0x180, 0xF0);
    func_800444D8(0);
    func_800363F0(0);
    func_800374E8(0x10, 0x10, 0x120, 0xF0, 0x1F4, 0, 0x3C0, 0x100, 0x3C0, 0x1FF, 0);
    D_8004F2BC++;
    draw[1].isbg = 1;
    draw[0].isbg = 1;
    draw[0].r0 = 0;
    draw[0].g0 = 0;
    draw[0].b0 = 0;
    draw[1].r0 = 0;
    draw[1].g0 = 0;
    draw[1].b0 = 0;
    func_80044534(1);
loop:
    {
        func_80044C44((frame & 1) ? &draw[0] : &draw[1]);
        func_80044E9C((frame & 1) ? &disp[0] : &disp[1]);
        func_80037324(0);
        func_8003700C("System Error No %d\n", error);
        func_8003700C("From  %08x\n", caller);
        func_8003700C("Count %d\n", D_8004F2BC);
        func_8003700C("Frame %d\n", frame);
        func_8003700C("MCBlog -> c:\\lserrmem.txt\n");
        func_8003700C("\n");
        if (error & 0x80) {
            func_8003700C("%s\n", D_8004F0C0[error]);
            if (error == 0x82) {
                func_80031BC4(&first, &second);
                func_8003700C("Program From %08x\n", first);
                func_8003700C("Failure Size %d (%xh) byte \n", second, second);
            }
            if (error == 0x85) {
                func_80031BC4(&first, &second);
                func_8003700C("Program From %08x\n", first);
                func_8003700C("Failure Pointer %p\n", second);
            }
        }
        func_8004B54C(0);
        frame++;
    }
    goto loop;
}

/* Names listed by the table at 8004f2dc. */
const char D_8001826C[] = "MASAKI";
const char D_80018274[] = "HIGUCHI";
const char D_8001827C[] = "SUGIMOTO";
const char D_80018288[] = "KAZUMI";
const char D_80018290[] = "HIGUCHI,MIYAGAWA,MASAKI";
const char D_800182A8[] = "YOSHII";

/* Kernel menu buffer: clear to dark blue and set up the white cursor triangle. */
void func_8001A1E4(s32 index) {
    KernelBuffer *buffer = &D_800595E8[index];

    buffer->draw.isbg = 1;
    buffer->draw.dtd = 1;
    buffer->draw.r0 = 0;
    buffer->draw.g0 = 0;
    buffer->draw.b0 = 0x20;
    func_80043C4C(&buffer->cursor);
    buffer->cursor.r0 = 0xFF;
    buffer->cursor.g0 = 0xFF;
    buffer->cursor.b0 = 0xFF;
}

/* Kernel menu set-up: debug text window and the two display buffers. */
void func_8001A250(void) {
    u32 unused[2]; /* an unused local the original frame reserves */

    func_80032498(6, 0);
    func_800374E8(8, 0x10, 0x170, 0x1E0, 0x3E8, 1, 0x3C0, 0x100, 0x3C0, 0x1FF, 0);
    func_80043928(&D_800595E8[0].draw, 0, 0, 0x140, 0xE0);
    func_80043928(&D_800595E8[1].draw, 0, 0xF0, 0x140, 0xE0);
    func_800439E0(&D_800595E8[0].disp, 0, 0xF0, 0x140, 0xE0);
    func_800439E0(&D_800595E8[1].disp, 0, 0, 0x140, 0xE0);
    func_8001A1E4(0);
    func_8001A1E4(1);
}

/* Kernel menu frame: move the cursor over the six modes, start the chosen one, print the menu with the play time and place the cursor. */
void func_8001A344(void) {
    char clock[24];
    s32 y;
    KernelBuffer *buffer;

    if (D_800594A4 & 0x1000) {
        if (--D_8004F2D8 < 0) {
            D_8004F2D8 = 5;
        }
    }
    if (D_800594A4 & 0x4000) {
        if (++D_8004F2D8 >= 6) {
            D_8004F2D8 = 0;
        }
    }
    if (D_8005948C & 0x20) {
        func_8001996C(D_8004F2D8 + 1);
        D_800592D0 = 0;
    }
    func_8003FBF8(clock, "%02d:%02d:%02d", D_80059484, D_80059420, D_80059418);
    func_8003700C(" XENOGEARS Kernel MENU\n  %s %s MODE\n\n", clock, D_80010000 ? "PC HDD" : "CD EMU");
    func_8003700C("    Field\n    Battle\n    Worldmap\n    Battling\n    Menu\n    Movie\n\n");
    y = D_8004F2D8 * 8;
    buffer = D_800592CC;
    *(u32 *)&buffer->cursor.x0 = ((y + 0x28) << 16) | 0x20;
    *(u32 *)&buffer->cursor.x1 = ((y + 0x2C) << 16) | 0x27;
    *(u32 *)&buffer->cursor.x2 = ((y + 0x30) << 16) | 0x20;
}

/* Mode 0, the kernel menu: run its frame loop until a mode is chosen, then dispatch. */
void func_8001A4B4(void) {
    u32 *ot;

    func_8001A250();
    D_800592D0 = 1;
    D_800592C8 = 0;
    do {
        D_800592C4++;
        D_800592C8 = D_800592C4 & 1;
        D_800592CC = &D_800595E8[D_800592C8];
        ot = D_800592CC->ot;
        func_80043BE4(ot);
        func_80037324(ot);
        func_8001A344();
        func_80043B48(ot, &D_800592CC->cursor);
        func_800445D0(0);
        func_8004B54C(0);
        func_80044C44(&D_800592CC->draw);
        func_80044E9C(&D_800592CC->disp);
        func_80044BD0(ot);
    } while (D_800592D0 != 0 || D_800592C8 == 0);
    func_800445D0(0);
    func_80019ACC(0);
}

/* Allocate the debug screen buffers and clear the two 40x28 cell grids. */
void func_8001A5CC(void) {
    s32 row;
    s32 column;

    D_800592DC[0] = func_80031BDC(0x3480, 1);
    D_800592DC[1] = func_80031BDC(0x3480, 1);
    D_800592D4 = func_80031BDC(0x460, 1);
    D_800592D8 = func_80031BDC(0x460, 1);
    for (row = 0; row < 28; row++) {
        for (column = 0; column < 40; column++) {
            D_800592D4[row * 40 + column] = 0;
            D_800592D8[row * 40 + column] = 0;
        }
    }
}

/* Count a hit in a cell of the second grid; out-of-range coordinates wrap to the other edge. */
void func_8001A684(s32 row, s32 column) {
    if (row < 0) {
        row = 28;
    }
    if (row > 28) {
        row = 0;
    }
    if (column < 0) {
        column = 40;
    }
    if (column > 40) {
        column = 0;
    }
    D_800592D8[row * 40 + column]++;
}

/* Debug screen, one Game of Life generation on the 40x28 grid: draw each live cell as an 8x8 tile, count its neighbours, apply the rules and reseed a random walk of 20 cells when fewer than 20 live, plus one random neighbourhood. */
void func_8001A6E8(u32 *ot) {
    LifeTile *tile;
    s32 live;
    s32 row; /* also the cell index of the update pass */
    s32 column;

    live = 0;
    tile = D_800592DC[D_800592C8];
    for (row = 0; row < 28; row++) {
        for (column = 0; column < 40; column++) {
            if (D_800592D4[row * 40 + column] != 0) {
                setlen(tile, 2);
                tile->rgbc = 0x70280000;
                tile->xy = (column << 3) | (row << 19);
                func_80043B48(ot, tile);
                tile++;
                live++;
                func_8001A684(row - 1, column - 1);
                func_8001A684(row - 1, column);
                func_8001A684(row - 1, column + 1);
                func_8001A684(row, column - 1);
                func_8001A684(row, column + 1);
                func_8001A684(row + 1, column - 1);
                func_8001A684(row + 1, column);
                func_8001A684(row + 1, column + 1);
            }
        }
    }
    for (row = 0; row < 28 * 40; row++) {
        if (D_800592D8[row] != 2) {
            D_800592D4[row] = D_800592D8[row] == 3;
        }
        D_800592D8[row] = 0;
    }
    if (live < 20) {
        live = 0;
        row = func_8003FA38() % 28;
        column = func_8003FA38() % 40;
        do {
            row += func_8003FA38() % 3 - 1;
            column += func_8003FA38() % 3 - 1;
            if (row < 0) {
                row = 28;
            }
            if (row > 28) {
                row = 0;
            }
            if (column < 0) {
                column = 40;
            }
            if (column > 40) {
                column = 0;
            }
            live++;
            D_800592D4[row * 40 + column] = 1;
        } while (live < 20);
    }
    row = func_8003FA38() % 28;
    column = func_8003FA38() % 40;
    func_8001A684(row - 1, column - 1);
    func_8001A684(row - 1, column);
    func_8001A684(row - 1, column + 1);
    func_8001A684(row, column - 1);
    func_8001A684(row, column + 1);
    func_8001A684(row + 1, column - 1);
    func_8001A684(row + 1, column);
    func_8001A684(row + 1, column + 1);
}

/* Reset the game-wide state words and flags. */
void func_8001AADC(void) {
    s32 i;
    s32 *last;

    D_8004F364 = 1;
    D_8004F328 = 0xFF;
    D_8004F324 = 0xFF;
    D_8004F2FC = 0;
    D_8004F36C = 0;
    D_8004F2F8 = 0;
    D_8004F31C = 0;
    D_8004F320 = 0;
    D_8004F314 = 0;
    D_8004F310 = 0;
    D_8004F30C = 0;
    D_8004F370 = 0;
    D_8004F35C = 0;
    D_8004F360 = 0;
    D_8004F374 = 0;
    D_8004F358 = 0;
    D_8004F354 = 0;
    D_8004F350 = 0;
    D_8004F2F4 = 0;
    D_8004F344 = 0;
    D_8004F348 = 0;
    D_8004F304 = 0;
    D_8004F368 = 0;
    D_8004F300 = 0;
    D_8004F380 = 0;
    D_8004F37C = 0;
    D_8004F378 = 0;
    D_8005942C = 0;
    D_800594D0 = 0;
    D_8004F384 = 0;
    D_8004F318 = 0;
    D_8004F334 = -1;
    D_8004F34C = -1;
    D_8004F33C = -1;
    D_8004F338 = -1;
    D_8004F330 = -1;
    D_8004F32C = -1;
    D_8004F340 = -1;
    D_8004F308 = -1;
    for (i = 0; i < 3; i++) {
        D_8006FABC[i] = 0;
        D_8006F990[i] = 0;
        D_8005A444[i] = 0;
        D_80062590[i] = 0;
    }
    for (i = 3, last = &D_80062524; i >= 0; i--) {
        *last-- = 0;
    }
}

/* Clear the state word 8004f30c (also cleared by a battle defeat). */
void func_8001AC94(void) {
    D_8004F30C = 0;
}

/* Select heap tag 8 and directory 4, then load file 1 of it. */
void func_8001ACA4(void) {
    D_8004F334 = -1;
    D_8004F330 = -1;
    func_80032498(8, 0);
    func_80028470(4, 0);
    func_8001B158(1);
}

/* The first byte of character record `index` in the game data. */
s32 func_8001ACF0(s32 index) {
    return D_8005A39C->characters[index].first;
}

/* Wait until the disc is idle, then for the pending read (80028a60). */
void func_8001AD1C(void) {
    while (func_800286CC() != 0) {
    }
    func_80028A60(0);
}

/* Take the party from the game data and load each member's field character file (member + 5) into a kept block. */
void func_8001AD4C(void) {
    s32 i;
    s32 count;
    u8 member;

    D_8005A39C = &D_8006D634;
    for (i = 0, count = 0; i < 3; i++) {
        D_80062590[i] = 0xFF;
        member = D_8005A39C->party[i];
        if (member != 0xFF) {
            D_80062590[count++] = member;
        }
    }
    for (i = 0, count = 0; i < 3; i++) {
        if (D_80062590[i] != 0xFF) {
            D_800625A4[count].file = D_80062590[i] + 5;
            D_8006FABC[i] = D_80062590[i];
            D_80065AFC[count] = func_80031BDC(func_800288EC(D_80062590[i] + 5), 0);
            D_800625A4[count].destination = D_80065AFC[count];
            func_800320A4(D_80065AFC[count]);
            count++;
        }
    }
    D_800625A4[count].destination = NULL;
    D_800625A4[count].file = 0;
    func_80029AFC(D_800625A4, 0, 0);
    D_8004F31C = 1;
}

/* As 8001ad4c, but load each member's gear file instead (16 + the gear of the character record, 0xff meaning none). */
void func_8001AEB8(void) {
    s32 i;
    s32 count;
    s32 file;
    u8 member;

    D_8005A39C = &D_8006D634;
    for (i = 0, count = 0; i < 3; i++) {
        D_80062590[i] = 0xFF;
        member = D_8005A39C->party[i];
        if (member != 0xFF) {
            D_80062590[count++] = member;
        }
    }
    for (i = 0, count = 0; i < 3; i++) {
        if (D_80062590[i] != 0xFF) {
            file = func_8001ACF0(D_80062590[i]);
            if (file == 0xFF) {
                file = 0;
            }
            file += 0x10;
            D_800625A4[count].file = file + 5;
            D_8006FABC[i] = file;
            D_80065AFC[count] = func_80031BDC(func_800288EC(file + 5), 0);
            D_800625A4[count].destination = D_80065AFC[count];
            func_800320A4(D_80065AFC[count]);
            count++;
        }
    }
    D_800625A4[count].destination = NULL;
    D_800625A4[count].file = 0;
    func_80029AFC(D_800625A4, 0, 0);
    D_8004F31C = 2;
}

/* Make sure the party files match the current state: characters on foot, gears when 8004f34c has 0xc000 set. */
void func_8001B044(void) {
    func_8001AD1C();
    if (D_8004F374 != 1) {
        if (D_8004F30C != 0) {
            func_8001B158(0);
            return;
        }
    } else {
        func_8001B3A8();
        if (D_8004F30C != 0) {
            return;
        }
    }
    if ((D_8004F34C & 0xC000) == 0) {
        D_8004F320 = 0;
    } else {
        D_8004F320 = 1;
    }
    D_8004F374 = 0;
    if (D_8004F320 == 0) {
        if (D_8004F31C != 1) {
            func_8001AD4C();
            D_8004F374 = 1;
        }
    } else {
        if (D_8004F31C != 2) {
            func_8001AEB8();
            D_8004F374 = 1;
        }
    }
}

/* Reload the party files listed in 8006fabc (quietly, from the heap top), plus files 0xa7/0xa8 when asked; on a failed allocation release what was loaded. */
void func_8001B158(s32 extra) {
    s32 i;
    s32 count;

    func_80031BB4(1);
    if (D_8004F374 == 1) {
        func_8001B3A8();
    }
    func_8001AD1C();
    for (i = 0, count = 0; i < 3; i++) {
        if (D_8006FABC[i] != 0xFF) {
            D_800625A4[count].file = D_8006FABC[i] + 5;
            D_80065AFC[count] = func_80031BDC(func_800288EC(D_8006FABC[i] + 5), 1);
            D_800625A4[count].destination = D_80065AFC[count];
            if (D_800625A4[count].destination == NULL) {
                for (i = 0; i < count; i++) {
                    func_800320B8(D_80065AFC[i]);
                    func_800320E8(D_80065AFC[i]);
                }
                func_80031BB4(0);
                return;
            }
            func_800320A4(D_80065AFC[count]);
            count++;
        }
    }
    if (extra) {
        D_8005A4A0 = func_80031BDC(func_800288EC(0xA7), 1);
        D_800625A4[count].destination = D_8005A4A0;
        if (D_8005A4A0 != NULL) {
            func_800320A4(D_8005A4A0);
            D_800625A4[count++].file = 0xA7;
            D_8004F344 = 1;
        }
        D_8005A4BC = func_80031BDC(func_800288EC(0xA8), 1);
        D_800625A4[count].destination = D_8005A4BC;
        if (D_8005A4BC != NULL) {
            func_800320A4(D_8005A4BC);
            D_800625A4[count++].file = 0xA8;
            D_8004F32C = 0;
        }
    }
    D_800625A4[count].destination = NULL;
    D_800625A4[count].file = 0;
    func_80029AFC(D_800625A4, 0, 0);
    D_8004F374 = 1;
    func_80031BB4(0);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B3A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B5A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B5E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B66C);

void func_8001B6BC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B6C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B844);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B94C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B970);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BB0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BB50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BBAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BD40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BDDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BE14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BEEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C074);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C1A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C634);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C76C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C944);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CA58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CB48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CC18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CD64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CD6C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CD74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CD94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CE74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D034);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D2A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D2B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D3F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D4E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001D53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001DAE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001E148);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001E298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001E3D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001E9BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001EE68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001EE74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001EE88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F1D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F6B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F8E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FAB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FB30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FBA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FBE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021AD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021B04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021B24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021B48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021B6C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021B98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021BCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021BF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021BF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C6C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021CA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021CC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021CF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021D3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021D50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021EBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021FB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021FC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021FE0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022000);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022038);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022090);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022224);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800222BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800223B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022660);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022974);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022A00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022A0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022A70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022B2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022CAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022CDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022D44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022DF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022E8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022EB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022FC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002303C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800230A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023124);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023170);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800231E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800231F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023210);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023290);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023340);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800233A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023440);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800234AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023538);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023804);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002393C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023950);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023958);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800239A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800239F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023A48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023B84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023FD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024294);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800242F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002435C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024524);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800245D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800248D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024F20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024F64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024FB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024FE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024FF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025044);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800250E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800251C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025224);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025258);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002541C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025544);

void func_80025710(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025718);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800257F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025A88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025C04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025D4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025FA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80026338);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800263E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002675C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80026A0C);

void func_80026B9C(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80026BA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80026DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80026F44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80026FE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002709C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800273C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800278F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80027D40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80027D64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80027EAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002800C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002804C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028230);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800283D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028470);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800284B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028548);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028570);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800286BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800286CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028738);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028808);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800288EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028928);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028998);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800289D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028A18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028A60);

/* Disc stream ring (EVID: analysis/formats/disc-stream-source.md). The ring
 * header holds the slot count, then eight bytes per slot. */
typedef struct {
    u16 state;
    u16 sequence;
    u16 w4;
    u16 w6;
} StreamSlot;

typedef struct {
    s32 count;
    StreamSlot slots[1];
} StreamRing;

extern StreamRing *D_8004FE30;

/* Replace the shared ring and return the previous one. */
StreamRing *func_80028A94(StreamRing *ring) {
    StreamRing *previous = D_8004FE30;
    D_8004FE30 = ring;
    return previous;
}

/* Clear every slot; the first slot's third halfword (ring offset 8) keeps
 * the low count. Returns the count, or -1 without a ring. */
s32 func_80028AAC(void) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 i;
    s32 count;

    if (ring == NULL) {
        return -1;
    }
    count = ring->count;
    slots = ring->slots;
    for (i = 0; i < count; i++) {
        slots[i].state = 0;
        slots[i].sequence = 0;
        slots[i].w4 = 0;
        slots[i].w6 = 0;
    }
    slots->w4 = count;
    return count;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028E60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028ECC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028F30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002945C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800294B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002954C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800295D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80029690);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80029AFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80029EB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A260);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A2D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A394);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A428);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A498);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A524);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A57C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A68C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002AC24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B084);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B2F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B5D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B8B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BA40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BA58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BB50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C310);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C3D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C3E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C4BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C59C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C644);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C68C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C6E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C8CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CB54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CBBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CC10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CC54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CC74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CCAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CCC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CD24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CD64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CDCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CF34);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CF58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D0C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D0E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D244);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D354);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D420);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D6AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D77C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D814);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002D984);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DA14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DAFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DB84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DC9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DD20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DDE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DFE0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002DFF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002E010);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002E448);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002E64C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002E8B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002EAB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002ED20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002EEF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002F0E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002F2E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002F4B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002F6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002F8D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002FAE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002FCFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002FF0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003014C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800301C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030228);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800302D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800303C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800305D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800306D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030988);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030A30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030C40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030C78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030C98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80030EE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003101C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800315A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800315C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800315E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003160C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031630);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031654);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003169C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800316C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800316E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031708);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003172C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031774);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031798);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800317BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800317E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031804);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031828);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003184C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031870);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031894);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800318A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800318BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800318DC);

void func_800318F0(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800318F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031A30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031A68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031B10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031B9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031BA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031BB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031BC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031BDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031F70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80031FF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800320A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800320B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800320D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800320E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003218C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003223C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800322B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032340);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800323B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032404);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032498);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800324B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800324C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032584);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018998);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003278C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032B0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032B64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032BAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032BDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032C18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032CB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032D60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032E04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032E7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032E88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80032EB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003342C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033474);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800334B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800334C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800334D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033518);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033558);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800335F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033668);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033698);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033728);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003373C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033760);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033784);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800337B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800337E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033818);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033848);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033878);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800338A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800338D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033908);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033938);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033968);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033998);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800339C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800339FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033A2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033A5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033A8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033ABC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033B34);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033BAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033C20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033CD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033CF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033DD4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80033DF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800345E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034614);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003463C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800346A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800346D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034714);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800347AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800347C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034800);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034874);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003487C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034888);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034EAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034F98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80034FFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003569C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035734);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800357C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003582C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035884);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800358A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800358BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035C0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035CDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035DA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035DB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035E44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035F1C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80035FF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003611C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036188);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036220);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036258);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036270);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036288);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003633C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003634C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800363E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800363F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036400);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036410);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036420);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036528);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800365FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800366E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800366F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036718);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036CD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036CF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036D18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036D30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036D50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036D70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036D88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036D98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036DA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036DB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036DC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036E4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036F44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036F5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036F74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036F8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036FA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036FBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80036FE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003700C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037058);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003708C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800370DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800372CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037324);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003747C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003748C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800374E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037878);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800379B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800379C8);

void func_800379D0(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800379D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037B88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037DC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037E8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037EE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037F44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037F88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80037FD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800380D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800381F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038264);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003827C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038310);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800383EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038428);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003852C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038624);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003864C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003869C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800386C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038824);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003885C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800388D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003890C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038934);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038AD4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038B4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038C68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038D18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038DB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038DF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038E6C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038EC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80038F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039024);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039144);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800391CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039248);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800392EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039360);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800393B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800394B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800395B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800396E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039748);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003977C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039784);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800397C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800397FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039850);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039910);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800399D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039A80);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039B68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039C4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039C8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039CC4);

void func_80039D24(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039D2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039D78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039DB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039E18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039E60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039EC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039F9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80039FF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A094);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A14C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A20C);

void func_8003A2D4(void) {
}

void func_8003A2DC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A2E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A344);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A3B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A450);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A4FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A55C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A5D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A65C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A82C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A838);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A89C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A948);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003A9BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AA30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AAC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003ABE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003ABF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AC58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003ACC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AD20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AD98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003ADCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AE84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AF24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003AFFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B060);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B0AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B148);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B1FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B22C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B32C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B370);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B424);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B644);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B930);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B97C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003B9E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BA38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BB08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BB40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BB64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BC10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BC34);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BC58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BC7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BCA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BDBC);

void func_8003BDF4(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BDFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BE68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003BFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003C010);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003C020);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003C484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003C4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003C6E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CC84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CD8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CE04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CE18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CE38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CE50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CE68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CE9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CEC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CED4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CEF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CFA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003CFF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D034);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D070);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D0E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D110);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D13C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D17C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D1BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D208);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D21C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D2D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D300);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D328);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D340);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D358);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D370);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D3A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D3D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D438);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D4A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D4E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D59C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D5C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D5CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D5D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D60C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D640);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D65C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D694);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D6D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D6F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D714);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D74C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D770);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D79C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D7C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D7FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D854);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D86C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D884);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D8B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003D9A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DAB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DAEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DB0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DB2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DB58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DB98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DBE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DC50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DD24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DE18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DE54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DE74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DE94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DEB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DEE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DF3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003DF78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E04C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E140);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E160);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E1F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E290);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E358);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E360);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E3E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E40C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E44C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E4BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E4F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E680);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E6C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E724);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E7E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E83C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E8A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003E900);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003EB5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003EBF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003EEA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003EF04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003EFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003EFE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F190);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F1A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F1EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F240);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F2A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F354);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F3C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F42C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F43C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F4A0);

void func_8003F4BC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F4E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F4FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F518);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F560);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F588);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F5EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F614);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F67C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F684);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F6B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F738);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F8B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F8CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F8E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F918);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F968);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003F99C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FA08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FA38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FA68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FA78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FB20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FB84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FBC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8003FBF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040454);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040464);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040474);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800404A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800404B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800404D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800404E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800404F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040514);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040534);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040554);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800405D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800405F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040690);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800406C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800406FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040734);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004076C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004077C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004078C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040828);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800408C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800408F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004092C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800409AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800409E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040A4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040A8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040A9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040AAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040ABC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040ACC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040ADC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040AEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040B00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040B7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040BA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040C20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040C3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040C5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040CBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040CD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040CE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040D08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040DA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040DC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040DF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040E18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040E28);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040E38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040E48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040E58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040E68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040ED4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040EF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040F0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040F40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040F74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040F94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040FB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040FCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80040FE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004111C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80041248);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004138C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800413AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800413CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800413EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80041410);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80041430);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80041534);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018CE4);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018E28);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018E38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800415B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80041B3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80041DBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042088);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800424A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004252C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004260C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004293C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042AA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042BA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042C98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042CA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042D8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042DDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042E90);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042EC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80042EF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800431C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800432BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800434D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004356C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043670);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004373C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043754);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004376C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043858);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800438C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043928);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800439E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043A1C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043A58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043A70);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80018F88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043AD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043B10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043B2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043B48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043B84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043BC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043BE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043BFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043C24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043C4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043C60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043C74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043C88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043C9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043CB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043CC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043CD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043CEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D28);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043D8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043DA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043DC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043DE0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043E00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043E20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043E4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043EAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80043F50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044064);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044110);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044294);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800443A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004440C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800444B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800444C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800444D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044534);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800445D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004463C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044764);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800447F8);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_80019180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044894);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800448F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004495C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044A20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044AD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044B70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044BD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044C44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044D48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044E64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80044E9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045344);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004537C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800453AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800453E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004546C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800454B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800454DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045534);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004574C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800459DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045A34);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045B00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045BCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045C10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045C94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045D44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045D5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80045E44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800460A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800462DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046560);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046588);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004659C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800465EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046638);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046668);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004668C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004696C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046C58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046DB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046EFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80046F30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004709C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80047178);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800471A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800471B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800471C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004722C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004726C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80047518);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80047638);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800477D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048AB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048BBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048BC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048C4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048D68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048DA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80048DD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004920C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004931C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004947C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004960C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800496AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004974C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004987C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049CEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049D3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049D9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049EFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049F2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049F5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80049F8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A0A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A0B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A0BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A0DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A0EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A12C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A14C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A19C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A1B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A260);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A280);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A4D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A64C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A67C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A73C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004A7BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B18C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B32C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B4AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B694);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B740);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B770);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B7A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B7D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B894);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B8BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004B9B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BE24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BE50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BED8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BEF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BF00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BF10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004BF20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C01C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C048);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C2C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C2F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C318);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C338);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C348);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C36C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C38C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C398);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C458);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C470);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C548);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C568);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C660);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main", D_8001946C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C6DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004C970);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004CBFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004CCA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004CF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004CFC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D028);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D070);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D1B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D1DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D208);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D270);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D294);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D310);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D364);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D3B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D504);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D590);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D600);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D740);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D784);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D7A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D818);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D878);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D8D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D930);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D964);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004D988);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004DD1C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004DEF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E3C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E564);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E574);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E5A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E6B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E774);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E794);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E7E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E850);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E860);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E870);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E8D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004E990);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8004EA20);
