#include "common.h"
#include "mode.h"
#include "menu.h"
#include "stream.h"
#include "sprite.h"

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

/* Once the party files are read, unpack each member's file into its field sprite block (8005a414) and release it. */
void func_8001B3A8(void) {
    s32 i;

    if (D_8004F374 != 0) {
        func_8001AD1C();
        for (i = 0; i < 3; i++) {
            func_800320B8(D_8005A414[i]);
            if (D_80062590[i] != 0xFF) {
                func_800320B8(D_80065AFC[i]);
                func_80032EB4(D_80065AFC[i], D_8005A414[i]);
                func_800320E8(D_80065AFC[i]);
            }
        }
        D_8004F374 = 0;
    }
}

/* Read a map's data ahead into its own block unless that map is already loaded; returns 0 when loaded, -1 while the disc is busy or after starting the read. */
s32 func_8001B484(s32 map, s32 slot) {
    if (D_8004F334 != slot || D_8004F330 != map) {
        if (func_800286CC() == 0) {
            func_80028A60(0);
            if (D_8004F334 != -1) {
                func_800320B8(D_8005A4E0);
                func_800320E8(D_8005A4E0);
            }
            func_8001B53C(map);
            D_8004F334 = slot;
            D_8004F330 = map;
        }
        return -1;
    }
    return 0;
}

/* Start reading map file 0xb8 + map into a kept block from the heap top. */
void func_8001B53C(s32 map) {
    s32 file = map + 0xB8;

    D_8005A4C0 = func_800288EC(file);
    D_8005A4E0 = func_80031BDC(D_8005A4C0, 1);
    func_800320A4(D_8005A4E0);
    func_800295D8(file, D_8005A4E0, 0, 0x80);
}

/* Release the transferred wave bank if it was loaded for this music. */
void func_8001B5A8(void) {
    if (D_8004F360 == 1) {
        func_80038310(D_8006258C);
        D_8004F360 = 0;
    }
}

/* Stop the active sequence; release it unless it is kept for reuse, in which case it becomes the cached sequence. */
void func_8001B5E8(void) {
    if (D_8004F35C == 1) {
        func_80039C4C(D_80062528);
        if (D_8004F348 == 0) {
            func_800399D4(D_80062528);
        } else {
            D_8004F2FC = D_80062528;
        }
        D_8004F35C = 0;
        D_8004F348 = 0;
    }
}

/* Stop the music and forget the loaded sequence and wave bank. */
void func_8001B66C(void) {
    if (D_8004F36C != 0) {
        func_8001B5E8();
        func_8001B5A8();
    }
    D_8004F33C = -1;
    D_8004F338 = -1;
    D_8004F36C = 0;
}

void func_8001B6BC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B6C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B844);

/* Battle draw environment: clear the background to (0x3c, 0x78, 0x78) with dithering. */
void func_8001B94C(DRAWENV *env) {
    env->isbg = 1;
    env->dtd = 1;
    env->r0 = 0x3C;
    env->g0 = 0x78;
    env->b0 = 0x78;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001B970);

/* Pass the scene selector 8006f9de and three resident tables to 800379d8. */
void func_8001BB0C(void) {
    func_800379D8(D_8006F9DE, 0, D_80059470, D_80059520, D_8005949C);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BB50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BBAC);

/* A random byte in [low, high]; 0xff for an unset low, 0 for an unset high, any byte when the range is 0xff or wider. */
/* Nonmatching under GCC 2.7.2 and 2.6.3: the low == high return merges with a shared exit. */
#ifdef NON_MATCHING
u8 func_8001BD40(u8 low, u8 high) {
    s32 span;

    if (low == 0xFF) {
        return 0xFF;
    }
    if (high == 0) {
        return 0;
    }
    span = high - low;
    if (low == high) {
        return low;
    }
    if (span >= 0xFF) {
        return func_8003FA38();
    }
    return low + (u8)func_8003FA38() % (span + 1);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BD40);
#endif

/* Menu buffer: no background clear, dithering, and a 256x216 display area 10 lines down. */
void func_8001BDDC(MenuBuffer *buffer) {
    buffer->draw.dtd = 1;
    buffer->draw.isbg = 0;
    buffer->draw.r0 = 0;
    buffer->draw.g0 = 0;
    buffer->draw.b0 = 0;
    buffer->disp.screen.x = 0;
    buffer->disp.screen.y = 0xA;
    buffer->disp.screen.w = 0x100;
    buffer->disp.screen.h = 0xD8;
}

/* Menu display set-up: GTE projection and the two 320x224 buffers. */
void func_8001BE14(void) {
    func_8004A12C(0xA0, 0x70);
    func_8004A14C(0x200);
    func_800439E0(&D_800625A0->buffers[0].disp, 0, 0xE0, 0x140, 0xE0);
    func_80043928(&D_800625A0->buffers[0].draw, 0, 0, 0x140, 0xE0);
    func_800439E0(&D_800625A0->buffers[1].disp, 0, 0, 0x140, 0xE0);
    func_80043928(&D_800625A0->buffers[1].draw, 0, 0xE0, 0x140, 0xE0);
    func_8001BDDC(&D_800625A0->buffers[0]);
    func_8001BDDC(&D_800625A0->buffers[1]);
}

/* Reset the menu's two views to the origin at distance 0x800. */
void func_8001BEEC(void) {
    MenuWork *work = D_800625A0;

    work->offset.vz = 0x800;
    work->offset2.vz = 0x800;
    work->angles.vx = work->angles.vy = work->angles.vz = 0;
    work->offset.vx = work->offset.vy = 0;
    work->angles2.vx = work->angles2.vy = work->angles2.vz = 0;
    work->offset2.vx = work->offset2.vy = 0;
    work->word2e8 = 1;
    work->view_motion = 0;
}

/* Decode the menu input of this frame: directions 0-3, confirm 4, debug toggles; 8 when nothing applies. */
/* GCC 2.6.3 code: the image matches exactly with this C under 2.6.3 (which reloads debug_value), not under 2.7.2. */
#ifdef NON_MATCHING
void func_8001BF38(void) {
    s32 input = 8;
    u16 buttons;

    if (func_80036410() != 0) {
        func_80035DB0();
    } else {
        while (func_80035CDC() != 0) {
            buttons = D_800594A4;
            if (buttons & 0x2000) {
                input = 0;
                break;
            }
            if (buttons & 0x4000) {
                input = 1;
                break;
            }
            if (buttons & 0x8000) {
                input = 2;
                break;
            }
            if (buttons & 0x1000) {
                input = 3;
                break;
            }
            if (buttons & 0x20) {
                input = 4;
                break;
            }
            if (buttons & 0x100) {
                D_800625A0->debug_show = D_800625A0->debug_show == 0;
                input = 0xC;
                break;
            }
            if (buttons & 0x4) {
                if (D_800625A0->debug_value != 0) {
                    D_800625A0->debug_value--;
                }
                break;
            }
            if (buttons & 0x1) {
                D_800625A0->debug_value++;
                break;
            }
        }
    }
    D_800625A0->input = input;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001BF38);
#endif

/* Menu frame: decode input, flip buffers, clear the ordering table, draw the debug overlays, then present. */
void func_8001C074(void) {
    MenuWork *work;

    func_8001BF38();
    work = D_800625A0;
    work->current = work->current == &work->buffers[0] ? &work->buffers[1] : &work->buffers[0];
    work->buffer_index = work->buffer_index == 0;
    func_80044AD8(work->current->ot, 16);
    if (*D_8005917C != -1) {
        if (D_800625A0->debug_show) {
            func_8003278C(3, D_800625A0->debug_value, 0xF, 0x80AC);
        }
        if (*D_8005917C != -1) {
            func_80037324(D_800625A0->current->ot);
        }
    }
    func_800445D0(0);
    func_8004B54C(0);
    func_80044C44(&D_800625A0->current->draw);
    func_80044E9C(&D_800625A0->current->disp);
    func_80044BD0(&D_800625A0->current->ot[15]);
}

/* Menu mode body: with the debug start, pick the menu screen and its parameter on screen and load the menu overlay (plus the extra blocks screen 5 needs); then run the chosen screen and, after a debug start, release everything and dispatch. */
/* Matches when its two jump tables are aligned to 8 relative to a rodata
 * section starting at 0x8001833c (4 mod 8): the original unit's rodata starts
 * there. Built inside main.c, GCC's .align 3 places them 4 bytes off. */
#ifdef NON_MATCHING
void func_8001C1A8(void) {
    s32 screen;
    s32 number;
    u8 running;
    void *low;
    void *overlay;

    screen = 0;
    number = 0;
    running = 1;
    if (D_80059178 != 0) {
        do {
            func_8003700C("\n\n     Menu=(%d)%s\n", screen, D_8004FA9C[screen]);
            if (screen < 4) {
                if (number < 11) {
                    func_8003700C("\n\n     Chr =(%d)\n", number);
                } else {
                    func_8003700C("\n\n     Robo =(%d)\n", number - 11);
                }
            } else if (screen != 6) {
                func_8003700C("\n\n     ShopNo =(%d)\n", number);
            } else {
                func_8003700C("\n\n     CdNo =(%d)\n", number);
            }
            switch (D_800625A0->input) {
            case 4:
                running = 0;
                break;
            case 0:
                screen++;
                number = 0;
                if (screen >= 7) {
                    screen = 0;
                }
                break;
            case 2:
                screen--;
                number = 0;
                if (screen < 0) {
                    screen = 6;
                }
                break;
            case 3:
                if (screen < 4) {
                    if (++number >= 31) {
                        number = 0;
                    }
                } else if (screen != 6) {
                    number++;
                } else {
                    number = number == 0;
                }
                break;
            case 1:
                if (--number < 0) {
                    if (screen < 4) {
                        number = 30;
                    } else if (screen != 6) {
                        number = 0xFF;
                    } else {
                        number = number == 0;
                    }
                }
                break;
            }
            func_8001C074();
        } while (running);
        D_80059460 = screen;
        D_80059171 = number;
        D_800625A0->buffers[0].draw.isbg = 0;
        D_800625A0->buffers[1].draw.isbg = 0;
        func_80044534(0);
        D_800625A0->current = &D_800625A0->buffers[1];
        func_80044534(1);
    }
    func_80028470(0x10, 0);
    if (D_80059178 != 0) {
        D_8006EF58 = 999999999;
        func_80032498(2, 0);
        D_8005945C = func_80031BDC(func_800288EC(1), 0);
        func_800295D8(1, D_8005945C, 0, 0x80);
        func_80028A60(0);
        if (D_80059460 == 5) {
            func_80028470(4, 0);
            D_800658CC = func_80031BDC(4, 1);
            D_8006BE24 = func_80031BDC((u8 *)D_800658CC - (u8 *)0x801DC000, 1);
            func_800295D8(0x6B9, (void *)0x801DC000, 0, 0x80);
            func_80028A60(0);
            func_80028470(0x10, 0);
            D_8005A4AC = func_80031BDC(0x4000, 0);
            D_8005A4B0 = func_80031BDC(0x4000, 0);
        }
        low = func_80031BDC(4, 1);
        overlay = func_80031BDC((u8 *)low - (u8 *)0x801C5000, 1);
        func_800295D8(D_80059460 + 5, (void *)0x801C5000, 0, 0x80);
        func_80028A60(0);
    }
    func_80028470(0x10, 0);
    switch (D_80059460) {
    case 0:
        func_801C62A8();
        break;
    case 1:
        func_801CB0A8();
        break;
    case 3:
        func_801CBDBC();
        break;
    case 4:
        func_801CCD28();
        break;
    case 2:
    case 6:
        func_801C62A8();
        func_8001996C(1);
        break;
    case 5:
        func_801CE024();
        break;
    }
    if (D_80059178 != 0) {
        func_800320E8(low);
        func_800320E8(overlay);
        if (D_80059460 == 5) {
            func_800320E8(D_800658CC);
            func_800320E8(D_8006BE24);
            func_800320E8(D_8005A4AC);
            func_800320E8(D_8005A4B0);
        }
        D_80059178 = 1;
        func_80019ACC(0);
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C1A8);
#endif

/* Mode 5, the menu: allocate and clear its work block, set up the display, then run the menu body. */
void func_8001C634(void) {
    D_800625A0 = func_80031BDC(0x1E98, 0);
    func_8003F8E8(D_800625A0, 0x1E98);
    D_800625A0->input = 8;
    func_80032498(2, 0);
    D_800625A0->current = &D_800625A0->buffers[1];
    D_800625A0->debug_show = 0;
    D_800625A0->debug_value = 1;
    D_800625A0->frame_counter = 0;
    D_800625A0->drawing = 0;
    func_8001BE14();
    if (D_80059178 != 0) {
        D_800625A0->buffers[0].draw.isbg = 1;
        D_800625A0->buffers[1].draw.isbg = 1;
    }
    func_8001BEEC();
    func_8004B54C(0);
    func_80044C44(&D_800625A0->buffers[0].draw);
    func_80044C44(&D_800625A0->buffers[1].draw);
    func_80044E9C(&D_800625A0->buffers[0].disp);
    func_80044E9C(&D_800625A0->buffers[1].disp);
    func_80044534(1);
    func_8001C1A8();
    D_80059178 = 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C76C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C944);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CA58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CB48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001CC18);

/* Set a task's update callback. */
void func_8001CD64(Task *task, void (*update)(Task *)) {
    task->update = update;
}

/* Set a task's update callback (second entry). */
void func_8001CD6C(Task *task, void (*update)(Task *)) {
    task->update = update;
}

/* Set a task's destroy callback. */
void func_8001CD74(Task *task, void (*destroy)(Task *)) {
    task->destroy = destroy;
}

/* A task's update callback. */
void *func_8001CD7C(Task *task) {
    return task->update;
}

/* A task's destroy callback. */
void *func_8001CD88(Task *task) {
    return task->destroy;
}

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

/* Whether a frame table entry takes its image from the sequencer (second byte bit 7). */
u8 func_8001EE68(u8 *frame) {
    return frame[1] >> 7;
}

/* The part count of a frame header (bits 9-14). */
u16 func_8001EE74(u16 *header) {
    return (*header >> 9) & 0x3F;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001EE88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F1D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F6B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001F8E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FAB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FB30);

/* The operand a script byte names: a frame table entry (bit 7 set) or a byte on the sprite's stack. */
/* Nonmatching under GCC 2.7.2 and 2.6.3: register choice for the stack index and operand byte. */
#ifdef NON_MATCHING
u8 *func_8001FBA4(Sprite *sprite, u8 *code) {
    u8 *operand;

    if (*code & 0x80) {
        operand = sprite->frames + (*code & 0x7F);
    } else {
        operand = &sprite->stack[(s8)*code + sprite->stack_top];
    }
    return operand;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FBA4);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001FBE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021AD8);

void func_80021B04(SVECTOR *vector, s16 x, s16 y, s16 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

void func_80021B14(VECTOR *vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

void func_80021B24(SVECTOR *to, SVECTOR *from) {
    to->vx = from->vx;
    to->vy = from->vy;
    to->vz = from->vz;
}

void func_80021B48(VECTOR *to, VECTOR *from) {
    to->vx = from->vx;
    to->vy = from->vy;
    to->vz = from->vz;
}

/* Drop a sprite's part colour. */
void func_80021B6C(Sprite *sprite) {
    sprite->colour_flags |= 1;
    func_8001F6B0(sprite);
}

/* Colour a sprite's one-sided parts. */
void func_80021B98(Sprite *sprite, u8 red, u8 green, u8 blue) {
    sprite->red = red;
    sprite->green = green;
    sprite->blue = blue;
    sprite->colour_flags &= ~1;
    func_8001F6B0(sprite);
}

/* Set a sprite's gravity divisor. */
void func_80021BCC(Sprite *sprite, s32 divisor) {
    sprite->motion.bits.divisor = divisor;
}

void func_80021BF0(Sprite *sprite, s32 resource) {
    sprite->resource = resource;
}

/* Set a sprite's completion callback. */
void func_80021BF8(Sprite *sprite, void *callback) {
    sprite->callback = callback;
}

/* Set a sprite's facing group (flags bits 8-12). */
void func_80021C00(Sprite *sprite, s32 group) {
    sprite->flags = (sprite->flags & ~0x1F00) | ((group & 0x1F) << 8);
}

/* Pop a byte from a sprite's stack. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
u8 func_80021C20(Sprite *sprite) {
    u8 value = sprite->stack[sprite->stack_top];

    sprite->stack_top += 1;
    return value;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C20);
#endif

/* Pop a halfword from a sprite's stack. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
s16 func_80021C3C(Sprite *sprite) {
    s16 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8);

    sprite->stack_top += 2;
    return value;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C3C);
#endif

/* Pop three bytes from a sprite's stack. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
s32 func_80021C6C(Sprite *sprite) {
    s32 value = sprite->stack[sprite->stack_top] + (sprite->stack[sprite->stack_top + 1] << 8) +
                (sprite->stack[sprite->stack_top + 2] << 16);

    sprite->stack_top += 3;
    return value;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021C6C);
#endif

/* Push a byte onto a sprite's stack. */
void func_80021CA0(Sprite *sprite, u8 value) {
    sprite->stack[--sprite->stack_top] = value;
}

/* Push a halfword onto a sprite's stack. */
void func_80021CC4(Sprite *sprite, u16 value) {
    sprite->stack_top -= 2;
    sprite->stack[sprite->stack_top] = value;
    sprite->stack[sprite->stack_top + 1] = value >> 8;
}

/* Push three bytes onto a sprite's stack. */
void func_80021CF8(Sprite *sprite, s32 value) {
    sprite->stack_top -= 3;
    sprite->stack[sprite->stack_top] = value;
    sprite->stack[sprite->stack_top + 1] = value >> 8;
    sprite->stack[sprite->stack_top + 2] = value >> 16;
}

/* Set a position's x and z from whole units (16.16). */
void func_80021D3C(VECTOR *position, s32 x, s32 z) {
    position->vz = z << 16;
    position->vx = x << 16;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021D50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80021EBC);

void func_80021FB8(Sprite *sprite, u8 value) {
    sprite->byteb0 = value;
}

/* Set a sprite's walking speed (its velocity follows). */
void func_80021FC0(Sprite *sprite, s32 speed) {
    sprite->speed = speed;
    func_80022974(sprite);
}

/* Turn a sprite (its speed follows the direction). */
void func_80021FE0(Sprite *sprite, s16 direction) {
    sprite->direction = direction;
    func_80022974(sprite);
}

/* Set a sprite's uniform scale (and its renderer's), marking the orientation dirty. */
void func_80022000(Sprite *sprite, s16 scale) {
    SpriteRenderer *renderer = sprite->renderer;

    if (renderer != NULL) {
        renderer->scale_x = renderer->scale_y = renderer->scale_z = sprite->scale = scale;
        sprite->render.bits.dirty = 1;
    }
}

/* Rebuild a sprite's orientation if it is marked dirty. */
void func_80022038(Sprite *sprite) {
    if ((sprite->render.word >> 28) & 1) {
        func_80022090(sprite);
        sprite->render.bits.dirty = 0;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022090);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022224);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800222BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800223B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022660);

/* Derive a sprite's horizontal velocity from its walking speed, gravity divisor and direction. */
void func_80022974(Sprite *sprite) {
    s32 speed = ((sprite->speed >> 4) << 8) / sprite->motion.bits.divisor;

    sprite->speed_x = ((func_8003F8CC(sprite->direction) >> 2) * speed) >> 6;
    sprite->speed_z = -((func_8003F8B0(sprite->direction) >> 2) * speed) >> 6;
}

s32 func_80022A00(s32 *word) {
    return *word;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022A0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022A70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022B2C);

/* Scale a value by the sprite's speed factor (1024 = 1) when it has one. */
s32 func_80022CAC(Sprite *sprite, s32 value) {
    s32 result = value;
    u16 rate = sprite->rate;

    if (rate != 0) {
        result *= rate;
        result /= 1024;
    }
    return result;
}

/* Move a sprite horizontally by its speed (scaled by its speed factor), then 80022b2c. */
void func_80022CDC(Sprite *sprite) {
    sprite->x += func_80022CAC(sprite, sprite->speed_x >> 4) << 4;
    sprite->z += func_80022CAC(sprite, sprite->speed_z >> 4) << 4;
    func_80022B2C(sprite);
}

/* Show the frame the frame index selects, mirrored when its flip bit and the sprite's mirror flag differ. */
void func_80022D44(Sprite *sprite) {
    s32 index = sprite->frame_bits.frame;
    u16 entry;
    s32 frame;

    if (index < 0) {
        index = 0;
    }
    entry = sprite->frame_table[index];
    frame = entry & 0x1FF;
    if (entry & 0x200) {
        sprite->motion.bits.frame_flip = 1;
    } else {
        sprite->motion.bits.frame_flip = 0;
    }
    sprite->render.bits.flip = sprite->motion.bits.frame_flip ^ sprite->motion.bits.mirror;
    func_8001D2B0(sprite, frame);
}

/* Sprite task update: advance the animation and move (twice with double_step); destroy the task when the frames run out. */
void func_80022DF4(Task *task) {
    Sprite *sprite = task->data;

    func_80023210(sprite);
    func_80022CDC(sprite);
    if (sprite->frames_left != 0) {
        if (!((sprite->motion.word >> 6) & 1)) {
            return;
        }
        func_80023210(sprite);
        func_80022CDC(sprite);
        if (sprite->frames_left != 0) {
            return;
        }
    }
    task->destroy(task);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022E8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80022EB8);

/* Replace a sprite renderer's part list with room for `count` parts. */
void func_80022FC4(Sprite *sprite, s32 count, s32 from_top) {
    if (sprite->renderer->parts != NULL) {
        func_800320E8(sprite->renderer->parts);
    }
    sprite->renderer->part_cursor = sprite->renderer->parts = func_80031BDC(count * 24, from_top);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002303C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800230A8);

/* The direction (0-0xfff) from one ground point to another. */
s32 func_80023124(DVECTOR from, DVECTOR to) {
    VECTOR delta;

    delta.vx = from.vx - to.vx;
    delta.vz = from.vy - to.vy;
    return -func_8004B32C(delta.vz, delta.vx) & 0xFFF;
}

/* Show a frame with the given mirroring, restarting the frame countdown and step. */
/* Nonmatching: same operations, different register allocation and scheduling of the two bitfield updates. */
#ifdef NON_MATCHING
void func_80023170(Sprite *sprite, s32 frame, s32 flip, s32 flip_y) {
    sprite->countdown = 0;
    sprite->render.bits.flip_y = flip_y;
    sprite->render.bits.flip = flip;
    sprite->frame_bits.phase = 0;
    sprite->frame_bits.step = 0;
    func_8001D2B0(sprite, frame);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023170);
#endif

/* The first word of the section a block's fourth word locates. */
s32 func_800231E0(s32 *block) {
    return *(s32 *)(block[3] + (s32)block);
}

/* One more than the second word of that section. */
s32 func_800231F8(s32 *block) {
    return ((s32 *)(block[3] + (s32)block))[1] + 1;
}

/* Count down the frame timer once per displayed frame, running the next command when it expires. */
/* Nonmatching: the original reads the countdown twice (lh to test, lhu to decrement), which GCC 2.7.2/2.6.3 do not reproduce here. */
#ifdef NON_MATCHING
void func_80023210(Sprite *sprite) {
    s32 i;

    for (i = 0; i != D_80059198.skip + 1; i++) {
        if (sprite->countdown != 0) {
            if (--sprite->countdown == 0) {
                func_800248D4(sprite);
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023210);
#endif

/* Set a sprite's blend rate; type 8 and 9 sprites keep it one lower, the others recolour their parts. */
void func_80023290(Sprite *sprite, s32 rate) {
    s32 type;
    s32 blend;

    rate &= 7;
    sprite->render.bits.blend = rate;
    if (rate != 0) {
        sprite->colour_flags |= 2;
    } else {
        sprite->colour_flags &= ~2;
    }
    type = (sprite->flags >> 13) & 0xF;
    if (type == 8 || type == 9) {
        blend = (sprite->render.word >> 5) & 7;
        if (blend != 0) {
            sprite->render.bits.blend = blend - 1;
        }
    } else {
        func_8001F6B0(sprite);
    }
}

/* Replace a sprite renderer's part list with room for `count` parts (from the heap bottom). */
void func_80023340(Sprite *sprite, s32 count) {
    func_800320E8(sprite->renderer->parts);
    sprite->renderer->parts = sprite->renderer->part_cursor = func_80031BDC(count * 24, 0);
}

/* Create a sprite task (with `extra` bytes after the sprite) under `owner`: its auxiliary node, default sprite and update/destroy callbacks. */
/* Nonmatching: the original keeps the auxiliary node pointer in its own register (s2); GCC folds its offset. */
#ifdef NON_MATCHING
SpriteTask *func_800233A4(Task *owner, s32 extra) {
    SpriteTask *node = func_80031BDC(extra + sizeof(SpriteTask), D_800591AF);
    Task *auxiliary;
    Sprite *sprite;

    func_8001CC18(owner, &node->task);
    auxiliary = &node->auxiliary;
    func_8001CA58(&node->task, auxiliary);
    sprite = &node->sprite;
    func_80023804(sprite);
    node->task.data = sprite;
    auxiliary->data = sprite;
    func_8001CD6C(&node->task, func_80022DF4);
    func_8001CD74(&node->task, func_80022EB8);
    return node;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800233A4);
#endif

/* A frame entry's image index: bits 8-10, plus 8 when bit 14 is set. */
s32 func_80023440(u16 *entry) {
    s32 index = (*entry >> 8) & 7;

    if ((*entry >> 14) & 1) {
        index += 8;
    }
    return index;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800234AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023538);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80023804);

/* Clear a renderer's angles and part list. */
void func_8002393C(SpriteRenderer *renderer) {
    renderer->angle_x = 0;
    renderer->angle_y = 0;
    renderer->angle_z = 0;
    renderer->parts = NULL;
}

void func_80023950(Sprite *sprite) {
    sprite->renderer = NULL;
}

/* Give a sprite the renderer stored right after it. */
void func_80023958(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->word40 = 0;
}

/* Give a sprite its inline renderer, sequencer and image storage (after the sprite). */
void func_800239A0(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->sequencer = (u8 *)sprite + 0xF4;
    sprite->renderer->pointer34 = (u8 *)sprite + 0x124;
    sprite->image = (u8 *)sprite + 0x110;
    sprite->renderer->pointer38 = NULL;
}

/* Give a sprite its inline renderer with an inline part list. */
void func_800239F4(Sprite *sprite) {
    sprite->renderer = (SpriteRenderer *)(sprite + 1);
    func_8002393C(sprite->renderer);
    sprite->renderer->part_cursor = (u8 *)sprite + 0xF4;
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->pointer38 = NULL;
}

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

void func_80024FE4(s32 value) {
    D_8005956C = value;
}

/* Copy the current light settings. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
void func_80024FF4(SpriteLight *light) {
    D_8004FBB8 = *light;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80024FF4);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025044);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800250E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800251C8);

/* Set a task's update callback from the table at 8004fd40. */
/* Sprite-unit code (GCC 2.7.2-cdk, -G8, ASPSX 2.5+): this C matches under that configuration (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
void func_80025224(Task *task, s32 kind) {
    func_8001CD64(task, D_8004FD40[kind]);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80025224);
#endif

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

/* Release a block if there is one. */
void func_80027D40(void *block) {
    if (block != NULL) {
        func_800320E8(block);
    }
}

/* Set up a texture scroll of `count` bands over an area; the band phases are allocated (heap tag 4) and cleared. Returns the scroll, or NULL. */
/* GCC 2.6.3 code with the assembler's div checks (maspsx --expand-div): the image matches exactly with this C under that configuration, not under this build's. */
#ifdef NON_MATCHING
TextureScroll *func_80027D64(TextureScroll *scroll, s16 x, s16 y, s16 w, s16 h, s16 count, u16 source_x,
                             u16 source_y, s8 *speeds) {
    s32 i;

    func_80032498(4, 0);
    scroll->x = x;
    scroll->y = y;
    scroll->w = w;
    scroll->h = h;
    scroll->step = h / count;
    scroll->count = count;
    scroll->source_x = source_x;
    scroll->source_y = source_y;
    scroll->speeds = speeds;
    scroll->phases = func_80031BDC(count * 2, 0);
    if (scroll->phases == NULL) {
        scroll = NULL;
    } else {
        for (i = 0; i < count; i++) {
            scroll->phases[i] = 0;
        }
    }
    return scroll;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80027D64);
#endif

/* Advance each band's phase by its speed and redraw the area rotated by it (two MoveImage copies per band). */
/* GCC 2.6.3 code with the assembler's div checks (maspsx --expand-div): the image matches exactly with this C under that configuration, not under this build's. */
#ifdef NON_MATCHING
void func_80027EAC(TextureScroll *scroll) {
    RECT rect;
    s32 i;
    u16 line;
    u16 offset;
    u16 rest;

    if (scroll->phases == NULL) {
        return;
    }
    rect.y = scroll->y;
    rect.h = scroll->step;
    line = scroll->source_y;
    for (i = 0; i < scroll->count; i++) {
        scroll->phases[i] += scroll->speeds[i];
        offset = (u16)((s16)scroll->phases[i] >> 4) % scroll->w;
        rest = scroll->w - offset;
        rect.x = scroll->x;
        rect.w = offset;
        func_8004495C(&rect, scroll->source_x + rest, line);
        rect.x = offset + scroll->x;
        rect.w = rest;
        func_8004495C(&rect, scroll->source_x, line);
        line += scroll->step;
        rect.y += scroll->step;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80027EAC);
#endif

/* Release a texture scroll's phases. */
void func_8002800C(TextureScroll *scroll) {
    if (scroll->phases != NULL) {
        func_800320E8(scroll->phases);
        scroll->phases = NULL;
    }
}

/* Disc error indicator: draw a coloured bar for retry `level`; from the fourth, switch to a text screen showing the failing file forever. */
void func_8002804C(s32 level, s32 r, s32 g, s32 b) {
    RECT rect;
    struct {
        DRAWENV draw;
        DISPENV disp;
        u32 ot[16];
    } screen;

    rect.x = level * 10 + 10;
    rect.w = 8;
    rect.y = 0;
    rect.h = 0x1C0;
    if (level + 1 >= 4) {
        r = 0xFF;
        g = 0xFF;
        b = 0xFF;
    }
    func_80044764(&rect, r, g, b);
    func_800445D0(0);
    if (level + 1 >= 4) {
        func_80044110(0);
        func_80048BC4();
        func_80043928(&screen.draw, 0, 0, 0x140, 0x100);
        func_800439E0(&screen.disp, 0, 0, 0x140, 0xF0);
        screen.draw.dtd = 0;
        screen.draw.isbg = 0;
        screen.draw.dfe = 1;
        screen.disp.isinter = 0;
        func_80044C44(&screen.draw);
        func_80044E9C(&screen.disp);
        func_800374E8(0x10, 0x10, 0x280, 0xF0, 0x400, 0, 0x280, 0, 0x280, 0x100, 0);
        func_80044534(1);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x30;
        func_80044764(&rect, 0, 0, 0);
        for (;;) {
            func_80044AD8(screen.ot, 8);
            func_8003700C("\n%d", D_8004FE14 + D_80059F0C - 1);
            func_8003700C("\n%s", func_80028998(D_80059F0C));
            func_80037324(screen.ot);
            func_80044BD0(&screen.ot[7]);
            func_800445D0(0);
        }
    }
}

/* Initialise disc access: the CD library (or the PC file server for other modes), the file index and directory table, reading both from sectors 24 and 40 when booting from CD. */
void func_80028230(u8 *files, u16 *directories, u32 mode) {
    D_8005A488 = 0;
    D_8005A48C = 0;
    D_8005A490 = 0;
    D_8005A494 = 0;
    D_8005A498 = 0;
    D_8005A49C = 0;
    D_8005A4A4 = 0;
    D_8005A4A8 = 0;
    D_8005A4B4 = 0;
    if (mode == 0 || mode == -1) {
        while (func_80040D08() == 0) {
        }
        func_80040EF4(0);
        func_800413EC(0);
        func_80040FB4(0);
        func_80040FCC(0);
        func_80040FE4(7, 0, D_80059F1C);
        func_8002A428(0xA0);
        func_80028A60(0);
        func_8004B54C(3);
    } else {
        func_8004C38C();
    }
    if (mode != -1) {
        D_8004FE48 = (char *)mode;
    } else {
        D_8004FE48 = NULL;
    }
    D_8004FDF0 = files;
    D_8004FDF4 = directories;
    D_8004FE14 = 0;
    D_8004FDFC = 0;
    D_8004FDF8 = 0;
    D_8004FE1C = 0;
    D_8004FE4C = -1;
    if (mode == 0) {
        func_8002954C(0x18, files, 0x8000, 0, 0);
        func_80028A60(0);
        func_8002954C(0x28, D_8004FDF4, 0x7A, 0, 0);
        func_80028A60(0);
    }
}

/* End disc access: stop the read, pause the drive (on CD) and clear the CD callbacks. */
void func_800283D4(void) {
    func_8002A498(0);
    func_80028A60(0);
    if (D_8004FE48 == NULL) {
        while (func_80041248(9, 0, D_80059F1C) == 0) {
        }
        func_8002A428(0xA0);
        func_80028A60(0);
        func_8004B54C(3);
    }
    func_800413EC(0);
    func_80040FB4(0);
    func_80040FCC(0);
    D_8004FDFC = 0;
    D_8004FDF8 = 0;
    D_8004FE1C = 0;
}

/* Select a directory by group and index in the directory table; returns it, or -1 (selecting 0) when that entry is empty. */
s32 func_80028470(s32 group, s32 index) {
    D_8004FE14 = D_8004FDF4[group + index] - 1;
    if (D_8004FE14 < 0) {
        D_8004FE14 = 0;
        return -1;
    }
    return D_8004FE14;
}

/* The directory group (a multiple of four) and index of the selected directory, both zero when none matches; returns the selection. */
s32 func_800284B4(s32 *group, s32 *index) {
    u16 *entry = D_8004FDF4;
    s32 i;

    for (i = 0; i < 0x40; i++, entry++) {
        if (*entry == D_8004FE14 + 1) {
            *group = i / 4 * 4;
            *index = i - i / 4 * 4;
            break;
        }
    }
    if (i == 0x40) {
        *group = 0;
        *index = 0;
    }
    return D_8004FE14;
}

/* The disc number (directory table word 0x3c). */
s32 func_80028530(void) {
    return D_8004FDF4[0x3C];
}

/* The directory at group + index relative to the selected one. */
s32 func_80028548(s32 group, s32 index) {
    return D_8004FDF4[group + index] - D_8004FE14;
}

/* Load a whole PC file into a new heap block (four tries per file-server call); returns the block, or NULL. */
/* A GCC 2.6.3 function (its first loop reproduces only there); the tail of the close loop still differs. */
#ifdef NON_MATCHING
void *func_80028570(char *name, s32 *size) {
    s32 fd;
    s32 length;
    s32 read;
    s32 i;
    void *block;

    for (i = 0; i < 4; i++) {
        fd = func_8004C318(name, 0, 0);
        if (fd != -1) {
            break;
        }
    }
    if (fd != -1) {
        length = func_8004C348(fd, 0, 2);
        if (size != NULL) {
            *size = length;
        }
        func_8004C348(fd, 0, 0);
        block = func_80031BDC(length, 0);
        read = 0;
        if (block != NULL) {
            for (i = 0; i < 4; i++) {
                read = func_8004C398(fd, block, length);
                if (read != 0) {
                    break;
                }
            }
        }
        if (read == 0) {
            if (block != NULL) {
                func_800320E8(block);
            }
            block = NULL;
        }
        for (i = 0; i < 4; i++) {
            if (func_8004C338(fd) == 0) {
                return block;
            }
        }
        if (block != NULL) {
            func_800320E8(block);
        }
    }
    block = NULL;
    return block;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028570);
#endif

s32 func_800286BC(void) {
    return D_8004FDF8;
}

/* Nonzero while a disc read is pending, the drive is busy or a read is in progress. */
s32 func_800286CC(void) {
    s32 state = D_8004FDFC;

    if (state == 0) {
        if (D_8004FE48 == NULL && func_80041410(1) != 0) {
            return 1;
        }
        if (D_8004FE1C != 0) {
            return 1;
        }
    }
    return state;
}

/* A file's byte size: from the PC file server when it knows the file, else from the file index. */
/* Nonmatching: the original keeps the index sum as addu and holds file, fd and size in s2/s0/s1 (closest under GCC 2.6.3). */
#ifdef NON_MATCHING
s32 func_80028738(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (D_8004FE48 != NULL) {
        fd = func_8004C318(func_80028998(file), 0, 0);
        size = func_8004C348(fd, 0, 2);
        func_8004C338(fd);
        if (size > 0) {
            return size;
        }
    }
    entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
    return size;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028738);
#endif

/* As 80028738 in the second directory selection (8004fe18), rounded up to words. */
/* GCC 2.6.3 code: the image matches exactly with this C under 2.6.3, not under 2.7.2. */
#ifdef NON_MATCHING
s32 func_80028808(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (D_8004FE48 != NULL) {
        fd = func_8004C318(func_80028998(file), 0, 0);
        size = func_8004C348(fd, 0, 2);
        func_8004C338(fd);
        if (size > 0) {
            return (size + 3) / 4 * 4;
        }
    }
    entry = &D_8004FDF0[(file + D_8004FE18 - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
    return (size + 3) / 4 * 4;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028808);
#endif

/* A file's byte size rounded up to words. */
/* GCC 2.6.3 code: the image matches exactly with this C under 2.6.3, not under 2.7.2. */
#ifdef NON_MATCHING
s32 func_800288EC(s32 file) {
    s32 size = func_80028738(file);

    return (size + 3) / 4 * 4;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800288EC);
#endif

/* For an index entry with a negative size (a directory), its file count; 0 for a file. */
/* Nonmatching: GCC turns the final addu of the index sum into or. */
#ifdef NON_MATCHING
s16 func_80028928(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];
    s32 size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];

    if (size >= 0) {
        return 0;
    }
    return -size;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028928);
#endif

/* The PC file server name of a file (64 bytes per file), or NULL without the server. */
/* GCC 2.6.3 code: the image matches exactly with this C under 2.6.3, not under 2.7.2. */
#ifdef NON_MATCHING
char *func_80028998(s32 file) {
    char *name = NULL;

    if (D_8004FE48 != NULL) {
        name = D_8004FE48 + (file + D_8004FE14 - 1) * 64;
    }
    return name;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028998);
#endif

/* A file's first sector in the selected directory. */
/* GCC 2.6.3 code: the image matches exactly with this C under 2.6.3, not under 2.7.2. */
#ifdef NON_MATCHING
s32 func_800289D0(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_800289D0);
#endif

/* A file's first sector in the second directory selection (8004fe18). */
/* GCC 2.6.3 code: the image matches exactly with this C under 2.6.3, not under 2.7.2. */
#ifdef NON_MATCHING
s32 func_80028A18(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE18 - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028A18);
#endif

/* Wait for the disc (mode 0: until idle); returns the disc status. */
s32 func_80028A60(s32 mode) {
    if (mode == 0) {
        while (func_800286CC() > 0) {
        }
    }
    return func_800286CC();
}

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
        slots[i].length = 0;
        slots[i].w6 = 0;
    }
    slots->length = count;
    return count;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028B14);

/* Whether any of `count` ring slots from `index` differs from `state` or runs past the last slot. */
s32 func_80028E60(s32 index, s32 count, s32 state) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (D_8004FE2C[index].state != state) {
            return 1;
        }
        if (++index > D_8004FE40) {
            return 1;
        }
    }
    return 0;
}

/* Merge the free run that follows slot `index` into its length. */
void func_80028ECC(s32 index) {
    s16 length = D_8004FE2C[index].length;
    s32 next = index + length;

    if (next < D_8004FE40 && D_8004FE2C[next].state == 0) {
        D_8004FE2C[index].length = length + D_8004FE2C[next].length;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80028F30);

/* Release a ring chunk: clear its slot's state and return the old state (0xffff without a ring, 0 for no chunk). */
/* Nonmatching under GCC 2.7.2 and 2.6.3: register choice for the ring (a1) and the payload arithmetic do not reproduce together. */
#ifdef NON_MATCHING
u16 func_8002945C(u8 *chunk) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 index;
    u16 state;
    u8 *payload;

    if (ring == NULL) {
        return 0xFFFF;
    }
    slots = ring->slots;
    if (chunk == NULL) {
        return 0;
    }
    payload = (u8 *)ring + ring->count * 8 + 0x24;
    index = (u32)(chunk - payload) >> 11;
    state = slots[index].state;
    slots[index].state = 0;
    return state;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002945C);
#endif

/* Release a run of ring chunks (the chunk header's halfword 3 counts them), merge the freed run and return the first slot's old state. */
u16 func_800294B4(u8 *chunk) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 index;
    s32 i;
    u16 state;
    u8 *payload;

    if (ring == NULL) {
        return 0xFFFF;
    }
    slots = ring->slots;
    if (chunk == NULL) {
        return 0;
    }
    i = ((u16 *)chunk)[3];
    payload = (u8 *)ring + ring->count * 8 + 0x24;
    index = (u32)(chunk - payload) >> 11;
    state = slots[index].state;
    for (; i > 0; i--) {
        slots[index + i - 1].state = 0;
    }
    func_80028ECC(index);
    return state;
}

/* Read `size` bytes from a raw disc sector (CD only); -1 with the PC file server. */
s32 func_8002954C(s32 sector, void *destination, s32 size, s32 a3, s32 a4) {
    if (D_8004FE48 != NULL) {
        return -1;
    }
    func_80028A60(0);
    D_8004FE04 = sector;
    D_8004FDF8 = size;
    return func_80029690(0, destination, a3, a4);
}

/* Read a file of the selected directory; -3 for an invalid file, an empty file or no destination. */
s32 func_800295D8(s32 file, void *destination, s32 a2, s32 a3) {
    if (file <= 0 || func_80028738(file) <= 0 || destination == NULL) {
        return -3;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_800288EC(file);
    return func_80029690(file, destination, a2, a3);
}

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
