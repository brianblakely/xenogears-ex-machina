#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "mode.h"
#include "menu.h"
#include "sprite.h"
#include "cd.h"
#include "stream.h"
#include "model.h"
#include "heap.h"
#include "text.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

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

    ResetCallback();
    SetGraphDebug(0);
    SetVideoMode(0);
    ResetGraph(0);
    screen.x = 0;
    screen.y = 0;
    screen.w = 0x180;
    screen.h = 0x1E0;
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);
    InitGeom();
    func_80036288();
    InitCARD(1);
    StartCARD();
    _bu_init();
    VSyncCallback(func_8003634C);
    func_80031A68((HeapHeader *)func_8002DFE0(), (u8 *)0x801FC000);
    SpuInit();
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
    ResetGraph(1);
    DrawSyncCallback(0);
    func_800363F0(0);
    DrawSync(0);
    VSync(2);
    func_80031B10((HeapHeader *)(mode->bss_end + 0x800));
    func_80019C7C();
    if (mode->loaded) {
        func_80019560(mode->bss_start, mode->bss_end);
        block = func_800199CC(D_80018088);
        func_80028A60(0);
        func_80032EB4(block, D_80018084);
        DrawSync(0);
        VSync(0);
        EnterCriticalSection();
        DrawSync(0);
        VSync(0);
        FlushCache();
        ExitCriticalSection();
    }
    func_80019548();
    func_80031B10((HeapHeader *)(mode->bss_end + 4));
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
    fd = PCcreat("c:\\core", 0);
    func_8004C470(fd, (void *)0x80000000, 0x200000);
    PCclose(fd);
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
    SwExitCriticalSection();
    ResetGraph(0);
    func_800283D4();
    func_80037DC0();
    SpuQuit();
    func_800363F0(0);
    DrawSyncCallback(0);
    VSyncCallback(NULL);
    CdFlush();
    StopPAD();
    SwEnterCriticalSection();
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
    LoadImage(&rect, (u_long *)(image + 0x14));
    rect.x = 0x280;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    LoadImage(&rect, (u_long *)(image + 0x40));
    setSprt(&logo);
    logo.x0 = 0x20;
    logo.y0 = 0x58;
    logo.v0 = 0;
    logo.u0 = 0;
    logo.w = 0x100;
    logo.h = 0x30;
    logo.clut = GetClut(0, 0xF0);
    SetDefDrawEnv(&draw, 0, 0, 0x140, 0xE0);
    SetDefDispEnv(&disp, 0, 0, 0x140, 0xE0);
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    DrawSync(0);
    for (level = 0; level < 0x80; level += 8) {
        logo.r0 = level;
        logo.g0 = level;
        logo.b0 = level;
        DrawPrim(&logo);
        VSync(0);
    }
    for (frame = 0x6D; frame != -1; frame--) {
        VSync(0);
    }
    for (level = 0x80; level >= 0; level -= 8) {
        logo.r0 = level;
        logo.g0 = level;
        logo.b0 = level;
        DrawPrim(&logo);
        VSync(0);
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
        ClearImage(&rect, 0xFF, 0, 0);
        for (;;) {
        }
    }
    func_800322B4();
    SetDefDrawEnv(&draw[0], 0, 0, 0x180, 0xF0);
    SetDefDispEnv(&disp[0], 0, 0xF0, 0x180, 0xF0);
    SetDefDrawEnv(&draw[1], 0, 0xF0, 0x180, 0xF0);
    SetDefDispEnv(&disp[1], 0, 0, 0x180, 0xF0);
    DrawSyncCallback(0);
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
    SetDispMask(1);
loop:
    {
        PutDrawEnv((frame & 1) ? &draw[0] : &draw[1]);
        PutDispEnv((frame & 1) ? &disp[0] : &disp[1]);
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
        VSync(0);
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
    SetPolyF3(&buffer->cursor);
    buffer->cursor.r0 = 0xFF;
    buffer->cursor.g0 = 0xFF;
    buffer->cursor.b0 = 0xFF;
}

/* Kernel menu set-up: debug text window and the two display buffers. */
void func_8001A250(void) {
    u32 unused[2]; /* an unused local the original frame reserves */

    func_80032498(6, 0);
    func_800374E8(8, 0x10, 0x170, 0x1E0, 0x3E8, 1, 0x3C0, 0x100, 0x3C0, 0x1FF, 0);
    SetDefDrawEnv(&D_800595E8[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800595E8[1].draw, 0, 0xF0, 0x140, 0xE0);
    SetDefDispEnv(&D_800595E8[0].disp, 0, 0xF0, 0x140, 0xE0);
    SetDefDispEnv(&D_800595E8[1].disp, 0, 0, 0x140, 0xE0);
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
    sprintf(clock, "%02d:%02d:%02d", D_80059484, D_80059420, D_80059418);
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
    u_long *ot;

    func_8001A250();
    D_800592D0 = 1;
    D_800592C8 = 0;
    do {
        D_800592C4++;
        D_800592C8 = D_800592C4 & 1;
        D_800592CC = &D_800595E8[D_800592C8];
        ot = D_800592CC->ot;
        TermPrim(ot);
        func_80037324(ot);
        func_8001A344();
        AddPrim(ot, &D_800592CC->cursor);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_800592CC->draw);
        PutDispEnv(&D_800592CC->disp);
        DrawOTag(ot);
    } while (D_800592D0 != 0 || D_800592C8 == 0);
    DrawSync(0);
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
void func_8001A6E8(u_long *ot) {
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
                AddPrim(ot, tile);
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
        row = rand() % 28;
        column = rand() % 40;
        do {
            row += rand() % 3 - 1;
            column += rand() % 3 - 1;
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
    row = rand() % 28;
    column = rand() % 40;
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
        func_80039C4C((SoundTrack *)D_80062528);
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
        return rand();
    }
    return low + (u8)rand() % (span + 1);
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
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(0x200);
    SetDefDispEnv(&D_800625A0->buffers[0].disp, 0, 0xE0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800625A0->buffers[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDispEnv(&D_800625A0->buffers[1].disp, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800625A0->buffers[1].draw, 0, 0xE0, 0x140, 0xE0);
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
    ClearOTagR(work->current->ot, 16);
    if (*D_8005917C != -1) {
        if (D_800625A0->debug_show) {
            func_8003278C(3, D_800625A0->debug_value, 0xF, 0x80AC);
        }
        if (*D_8005917C != -1) {
            func_80037324(D_800625A0->current->ot);
        }
    }
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&D_800625A0->current->draw);
    PutDispEnv(&D_800625A0->current->disp);
    DrawOTag(&D_800625A0->current->ot[15]);
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
        SetDispMask(0);
        D_800625A0->current = &D_800625A0->buffers[1];
        SetDispMask(1);
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
    bzero((u8 *)D_800625A0, 0x1E98);
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
    VSync(0);
    PutDrawEnv(&D_800625A0->buffers[0].draw);
    PutDrawEnv(&D_800625A0->buffers[1].draw);
    PutDispEnv(&D_800625A0->buffers[0].disp);
    PutDispEnv(&D_800625A0->buffers[1].disp);
    SetDispMask(1);
    func_8001C1A8();
    D_80059178 = 1;
}

/* 0x170 bytes of constant data in text (8001C76C-8001C8DC), not code. */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8001C76C);
