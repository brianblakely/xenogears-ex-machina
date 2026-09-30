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
    return -ratan2(delta.vz, delta.vx) & 0xFFF;
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
        MoveImage(&rect, scroll->source_x + rest, line);
        rect.x = offset + scroll->x;
        rect.w = rest;
        MoveImage(&rect, scroll->source_x, line);
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
        u_long ot[16];
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
    ClearImage(&rect, r, g, b);
    DrawSync(0);
    if (level + 1 >= 4) {
        ResetGraph(0);
        InitGeom();
        SetDefDrawEnv(&screen.draw, 0, 0, 0x140, 0x100);
        SetDefDispEnv(&screen.disp, 0, 0, 0x140, 0xF0);
        screen.draw.dtd = 0;
        screen.draw.isbg = 0;
        screen.draw.dfe = 1;
        screen.disp.isinter = 0;
        PutDrawEnv(&screen.draw);
        PutDispEnv(&screen.disp);
        func_800374E8(0x10, 0x10, 0x280, 0xF0, 0x400, 0, 0x280, 0, 0x280, 0x100, 0);
        SetDispMask(1);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x30;
        ClearImage(&rect, 0, 0, 0);
        for (;;) {
            ClearOTagR(screen.ot, 8);
            func_8003700C("\n%d", D_8004FE14 + D_80059F0C - 1);
            func_8003700C("\n%s", func_80028998(D_80059F0C));
            func_80037324(screen.ot);
            DrawOTag(&screen.ot[7]);
            DrawSync(0);
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
        while (CdInit() == 0) {
        }
        CdSetDebug(0);
        CdDataCallback(0);
        CdSyncCallback(0);
        CdReadyCallback(0);
        CdControl(7, 0, D_80059F1C);
        func_8002A428(0xA0);
        func_80028A60(0);
        VSync(3);
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
        while (CdControlB(9, 0, D_80059F1C) == 0) {
        }
        func_8002A428(0xA0);
        func_80028A60(0);
        VSync(3);
    }
    CdDataCallback(0);
    CdSyncCallback(0);
    CdReadyCallback(0);
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
        fd = PCopen(name, 0, 0);
        if (fd != -1) {
            break;
        }
    }
    if (fd != -1) {
        length = PClseek(fd, 0, 2);
        if (size != NULL) {
            *size = length;
        }
        PClseek(fd, 0, 0);
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
            if (PCclose(fd) == 0) {
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
        if (D_8004FE48 == NULL && CdDataSync(1) != 0) {
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
        fd = PCopen(func_80028998(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
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
        fd = PCopen(func_80028998(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
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

/* Start reading `D_8004FDF8` bytes from sector D_8004FE04: into a stream ring (flags 0x100, or 0x200 with CD mode byte flags | 0xa0) or into memory; with the PC file server the file is opened (ring) or read now. */
/* GCC 2.6.3 disc-unit code; under 2.6.3 + --expand-div this C differs only in the file/destination register assignment (s3/s2 swapped). */
#ifdef NON_MATCHING
s32 func_80029690(s32 file, void *destination, s32 mode, s32 flags) {
    StreamRing *ring;
    char *name;
    s32 fd;
    s32 i;
    CdlLOC *position = &D_80059F10;
    u8 *mode_byte;

    D_80059F0C = file;
    for (i = 2; i >= 0; i--) {
        D_80059EF8[i] = 0;
    }
    D_8004FDFC = 1;
    D_8004FE08 = destination;
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE10 = 0;
    D_8004FE0C = NULL;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    CdIntToPos(D_8004FE04, position);
    if (flags & 0x100) {
        func_80028A94(destination);
        ring = D_8004FE30;
        if (ring->count == 0) {
            return -4;
        }
        D_8004FE08 = (u8 *)ring + ring->count * 8 + 0x24;
        D_8004FE2C = ring->slots;
        D_8004FE40 = ring->count;
        D_8004FE26 = 0;
        D_8004FE28 = 0;
        D_8004FE24 = 0;
        func_80028AAC();
        if (D_8004FE48 != NULL) {
            name = func_80028998(file);
            for (i = 0; i < 4; i++) {
                D_8004FE4C = PCopen(name, 0, 0);
                if (D_8004FE4C != -1) {
                    break;
                }
                func_8002804C(i, 0xFF, 0, 0);
            }
            return D_8004FE4C == -1 ? -3 : 0;
        }
        D_8004FE1C = 1;
        CdDataCallback(func_8002BA58);
        CdSyncCallback(func_8002A68C);
        CdReadyCallback(func_8002B2F0);
    } else if (flags & 0x200) {
        func_80028A94(destination);
        ring = D_8004FE30;
        if (ring->count == 0) {
            return -4;
        }
        D_8004FE08 = (u8 *)ring + ring->count * 8 + 0x24;
        D_8004FE2C = ring->slots;
        D_8004FE40 = ring->count;
        D_80059F60 = 0;
        D_8004FE26 = 0;
        D_8004FE28 = 0;
        D_8004FE24 = 0;
        func_80028AAC();
        for (i = 3, mode_byte = &D_80059F18[3]; i >= 0; i--) {
            *mode_byte-- = 0;
        }
        D_80059F18[0] = flags | 0xA0;
        if (D_8004FE48 == NULL) {
            return 0;
        }
        name = func_80028998(file);
        for (i = 0; i < 4; i++) {
            D_8004FE4C = PCopen(name, 0, 0);
            if (D_8004FE4C != -1) {
                break;
            }
            func_8002804C(i, 0xFF, 0, 0);
        }
        return D_8004FE4C == -1 ? -3 : 0;
    } else {
        if (D_8004FE48 != NULL) {
            name = func_80028998(file);
            for (i = 0; i < 4; i++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                func_8002804C(i, 0xFF, 0, 0);
            }
            if (fd == -1) {
                return -4;
            }
        opened:
            if (destination != NULL) {
                for (i = 0; i < 4; i++) {
                    if (func_8004C398(fd, destination, D_8004FDF8) != 0) {
                        break;
                    }
                    func_8002804C(i, 0, 0xFF, 0);
                }
            }
            for (i = 0; i < 4; i++) {
                if (PCclose(fd) == 0) {
                    D_8004FDF8 = 0;
                    D_8004FDFC = 0;
                    return 0;
                }
                func_8002804C(i, 0, 0, 0xFF);
            }
            return -6;
        }
        D_8004FE1C = 1;
        CdDataCallback(NULL);
        CdSyncCallback(func_8002A68C);
        CdReadyCallback(func_8002B084);
    }
    D_8005A488++;
    CdControlF(2, (u8 *)position);
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80029690);
#endif

/* Read a zero-terminated file list: sort it by file, then start the CD reads (the callbacks continue them), or with the PC file server read every file now. Returns 0, or -3 for an empty list. */
/* GCC 2.6.3 code with div checks (the disc unit): this C matches under 2.6.3 + maspsx --expand-div (object compare with relocations masked), not in this build. */
#ifdef NON_MATCHING
s32 func_80029AFC(FileRequest *list, s32 mode, s32 unused) {
    s32 count;
    s32 i;
    s32 j;
    s32 best;
    u16 file;
    void *destination;
    char *name;
    s32 fd;

    if (list == NULL) {
        return -3;
    }
    for (count = 0; list[count].file != 0; count++) {
    }
    if (count == 0) {
        return -3;
    }
    for (i = 0; i < count - 1; i++) {
        file = list[i].file;
        best = i;
        for (j = i + 1; j < count; j++) {
            if (list[j].file < file) {
                best = j;
                file = list[j].file;
            }
        }
        file = list[i].file;
        destination = list[i].destination;
        list[i].file = list[best].file;
        list[i].destination = list[best].destination;
        list[best].file = file;
        list[best].destination = destination;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    for (i = 2; i >= 0; i--) {
        D_80059EF8[i] = 0;
    }
    D_8004FE10 = 0;
    D_8004FE0C = list;
    D_8004FDFC = count;
    D_8004FE00 = count;
    D_8004FE08 = list->destination;
    file = list->file;
    if (file == 0 || D_8004FE08 == NULL) {
        func_8002A394(mode);
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        return 0;
    }
    D_80059F0C = file;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_80028808(file);
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE3C = 0;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8004FE48 != NULL) {
        for (i = 0; i < count; i++) {
            file = list[i].file;
            D_80059F0C = file;
            name = func_80028998(file);
            for (j = 0; j < 4; j++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                func_8002804C(j, 0xFF, 0, 0);
            }
            goto close;
        opened:
            if (list[i].destination != NULL) {
                for (j = 0; j < 4; j++) {
                    if (func_8004C398(fd, list[i].destination, func_80028808(file)) != 0) {
                        break;
                    }
                    func_8002804C(j, 0, 0xFF, 0);
                }
            }
        close:
            for (j = 0; j < 4; j++) {
                if (PCclose(fd) == 0) {
                    break;
                }
                func_8002804C(j, 0, 0, 0xFF);
            }
        }
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        return 0;
    }
    D_8004FE1C = 1;
    CdDataCallback(func_8002BA40);
    CdSyncCallback(func_8002A68C);
    CdReadyCallback(func_8002AC24);
    D_8005A488++;
    CdControlF(2, (u8 *)&D_80059F10);
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_80029AFC);
#endif

/* Start streaming a file through a ring of at least two slots with six stream parameters; with the PC file server the whole stream is pumped now. Returns 0, -3 for a bad file, -4 for a bad ring, -6 when the file does not close. */
s32 func_80029EB0(s32 file, StreamRing *ring, s32 mode, s32 unused, u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    s32 count;
    char *name;
    s16 i;

    if (ring == NULL || (u32)(count = ring->count) < 2) {
        return -4;
    }
    if (file <= 0) {
        return -3;
    }
    if (func_80028738(file) <= 0) {
        return -3;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    for (i = 0; i < 3; i++) {
        D_80059EF8[i] = 0;
    }
    func_80028A94(ring);
    D_80059F0C = file;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_800288EC(file);
    D_8004FDFC = 1;
    D_8004FE08 = (u8 *)ring + count * 8 + 0x24;
    D_8004FE2C = ring->slots;
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE10 = 0;
    D_8004FE40 = count;
    D_8004FE26 = 0;
    D_8004FE28 = 0;
    D_8004FE0C = NULL;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    D_80059F24 = a;
    D_80059F28 = b;
    D_80059F2C = c;
    D_80059F30 = d;
    D_80059F34 = e;
    D_80059F38 = f;
    D_80059F3C = 0;
    D_80059F40 = 0;
    D_80059F44 = 0;
    D_80059F48 = 0;
    D_80059F4C = 0;
    D_80059F50 = 0;
    func_80028AAC();
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8004FE48 != NULL) {
        name = func_80028998(file);
        for (i = 0; i < 4; i++) {
            D_80059F04 = PCopen(name, 0, 0);
            if (D_80059F04 != -1) {
                break;
            }
            func_8002804C(i, 0xFF, 0, 0);
        }
        do {
            func_8002B8B0(0, 0);
            func_8002BF38(0, 0);
        } while (D_8004FDFC > 0);
        for (i = 0; i < 4; i++) {
            i = PCclose(D_80059F04);
            if (i == 0) {
                break;
            }
            func_8002804C(i, 0, 0, 0xFF);
        }
        if (i != 0) {
            return -6;
        }
        D_8004FDFC = 0;
        D_8004FDF8 = 0;
        return 0;
    }
    D_8004FE1C = 1;
    CdDataCallback(func_8002BB50);
    CdSyncCallback(func_8002A68C);
    CdReadyCallback(func_8002B5D0);
    D_8005A488++;
    CdControlF(2, (u8 *)&D_80059F10);
    return 0;
}

/* Allocate a stream ring of `count` 2,048-byte sectors (plus the slot
 * header), then select and reset it. Returns the ring or NULL. */
StreamRing *func_8002A260(s32 count, s32 mode) {
    StreamRing *ring;

    if (count > 0) {
        ring = func_80031BDC(count * 0x808 + 0x24, mode);
        if (ring == NULL) {
            return NULL;
        }
        ring->count = count;
        func_80028A94(ring);
        func_80028AAC();
        return ring;
    }
    return NULL;
}

/* Unless a read is already running, seek to `file` (or pause for a
 * nonpositive file) with the resident CD ready callback installed. */
void func_8002A2D0(s32 file) {
    if (D_8004FE48 == 0 && func_800286CC() == 0) {
        D_8004FE18 = D_8004FE14;
        if (file > 0) {
            CdIntToPos(func_800289D0(file), &D_80059F10);
            D_8004FE1C = 3;
            CdSyncCallback(func_8002A68C);
            CdControlF(2, (u8 *)&D_80059F10);
        } else {
            D_8004FE1C = 5;
            CdSyncCallback(func_8002A68C);
            CdControlF(9, NULL);
        }
    }
}

/* Seek to `file` (or pause) unconditionally. */
void func_8002A394(s32 file) {
    if (file > 0) {
        CdIntToPos(func_800289D0(file), &D_80059F10);
        D_8004FE1C = 3;
        CdSyncCallback(func_8002A68C);
        CdControlF(2, (u8 *)&D_80059F10);
    } else {
        D_8004FE1C = 5;
        CdSyncCallback(func_8002A68C);
        CdControlF(9, NULL);
    }
}

/* Issue CdlSetmode with `mode`. */
void func_8002A428(u8 mode) {
    u8 *param;
    s32 i;

    D_8004FE1C = 9;
    CdSyncCallback(func_8002A68C);
    for (i = 3, param = &D_80059F18[3]; i >= 0; i--) {
        *param-- = 0;
    }
    D_80059F18[0] = mode;
    CdControlF(0xE, D_80059F18);
}

/* Request a stop with `reason`; when a read is active, drop it and close the
 * open host file handle (retrying a few transient results). */
void func_8002A498(s32 reason) {
    s32 result;

    D_8004FE34 = 1;
    D_8004FE38 = reason;
    if (D_8004FE48 != 0) {
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        if (D_8004FE4C != -1) {
            do {
                result = PCclose(D_8004FE4C);
            } while (result != 0 && result + 1 < 4);
            D_8004FE4C = -1;
        }
    }
}

/* Free every loaded file's data in a zero-terminated file table. */
void func_8002A524(FileEntry *table) {
    FileEntry *entry;
    void *data;

    if (table->id != 0) {
        entry = table;
        do {
            data = entry->data;
            entry++;
            if (data != NULL) {
                func_800320E8(data);
            }
        } while (entry->id != 0);
    }
}

/* Load the files following `first` into `table` (allocated when NULL). On an
 * allocation failure everything loaded is freed and NULL returned.
 * Nonmatching: GCC strength-reduces &table[i]; the original recomputes it. */
#ifdef NON_MATCHING
FileEntry *func_8002A57C(s32 first, FileEntry *table) {
    s32 count;
    s32 owned = 0;
    s32 i;

    count = func_80028928(first);
    if (count > 0) {
        if (table == NULL) {
            table = func_80031BDC((count + 1) * 8, 0);
            owned = 1;
            if (table == NULL) {
                return NULL;
            }
        }
        for (i = 0; i < count; i++) {
            table[i].id = first + i + 1;
            table[i].data = func_80031BDC(func_800288EC(first + i + 1), 0);
            if (table[i].data == NULL) {
                func_8002A524(table);
                if (owned > 0) {
                    func_800320E8(table);
                }
                return NULL;
            }
        }
        table[count].id = 0;
        table[count].data = NULL;
    } else {
        table = NULL;
    }
    return table;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A57C);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002A68C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002AC24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B084);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B2F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B5D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002B8B0);


void func_8002BA40(void) {
    D_8004FDFC = D_8004FE00;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BA58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BB50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002BF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C310);

/* Nonzero when files come from the PC file server (its name table). */
s32 func_8002C3D8(void) {
    return (s32)D_8004FE48;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C3E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C4BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C59C);

/* Trim a model group's heap block to its data (once). Returns 1 when it
 * was already trimmed. */
s32 func_8002C644(ModelGroup *group) {
    if (group->flags & 2) {
        return 1;
    }
    group->flags |= 2;
    func_80031F70((u8 *)group, group->primitives - (u8 *)group);
    return 0;
}

/* Trim a model buffer's heap block at its end (once). Returns 1 when it
 * was already trimmed. */
s32 func_8002C68C(ModelBuffer *buffer) {
    if (buffer->flags & 0x40) {
        return 1;
    }
    buffer->flags |= 0x40;
    func_80031F70((u8 *)buffer, buffer->end - (u8 *)buffer);
    buffer->end = NULL;
    return 0;
}

extern u8 D_80059598;
extern u8 D_80059599;
extern u8 D_8005959A;

void func_8002C6E0(u8 r, u8 g, u8 b) {
    D_80059598 = r;
    D_80059599 = g;
    D_8005959A = b;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002C8CC);

/* Allocate a model buffer's two halves of `size` bytes each. */
void func_8002CB54(ModelBuffer *buffer, u8 **first, u8 **second) {
    u8 *block;

    func_800324B8(0x25);
    block = func_80031BDC(buffer->size * 2, 0);
    *first = block;
    *second = block + buffer->size;
}

/* Release a model buffer's owned block. */
void func_8002CBBC(ModelBuffer *buffer) {
    if (buffer->flags & 1) {
        func_800320E8(buffer->buffer);
        buffer->flags &= ~1;
    }
}

extern s32 D_80050108; /* texture page override: 0 none, 1 page, 2 raw */
extern s32 D_8005010C; /* CLUT override: 0 on */
extern s32 D_80059310;
extern s32 D_80059314;

/* Override model texture pages with the page at (x, y). */
void func_8002CC10(u16 x, u16 y) {
    D_80059310 = GetTPage(0, 0, x, y) & 0x1F;
    D_80050108 = 1;
}

void func_8002CC54(u16 tpage) {
    D_80059310 = tpage;
    D_80050108 = 2;
}

/* Override model CLUTs with the CLUT at (x, y). */
void func_8002CC74(u16 x, u16 y) {
    D_80059314 = GetClut(x, y) & 0xFFF0;
    D_8005010C = 0;
}

void func_8002CCAC(void) {
    D_80050108 = 0;
    D_8005010C = 1;
}

extern u16 D_80059308;
extern u16 D_8005930C;

/* Apply the texture page override to a primitive's page. */
void func_8002CCC8(u16 *tpage) {
    u16 value = *tpage;

    D_80059308 = value;
    if (D_80050108 == 1) {
        D_80059308 = value & 0xFFE0;
        D_80059308 = (value & 0xFFE0) | D_80059310;
    } else if (D_80050108 == 2) {
        D_80059308 = D_80059310;
    }
}

/* Apply the CLUT override to a primitive's CLUT. */
void func_8002CD24(u16 *clut) {
    u16 value = *clut;

    D_8005930C = value;
    if (D_8005010C == 0) {
        D_8005930C = value & 0xF;
        D_8005930C = (value & 0xF) | D_80059314;
    }
}

/* Handle a texture page (0xC4) or CLUT (0xC8) command. Returns 1 for any
 * other command. */
s32 func_8002CD64(u8 *command) {
    if ((command[3] & 0xF0) != 0xC0) {
        return 1;
    }
    switch (command[3]) {
    case 0xC4:
        func_8002CCC8((u16 *)command);
        return 0;
    case 0xC8:
        func_8002CD24((u16 *)command);
        return 0;
    }
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CDCC);

s32 func_8002CF34(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 4;
    packet->value = *value;
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main", func_8002CF58);

s32 func_8002D0C0(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 5;
    packet->value = *value;
    return 1;
}

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

/* The shared unpack buffer. */
u8 *func_8002DFE0(void) {
    return D_8006FAF0;
}

extern s32 D_800500F8;
extern s32 D_800500FC;

void func_8002DFF0(s32 a, s32 b) {
    D_800500FC = (b - 1) << 16;
    D_800500F8 = a;
}

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

/* Copy the vertices listed in `indices` (last first) from `in` to `out`. */
void func_800301C8(SVECTOR *out, SVECTOR *in, s32 count, s16 *indices) {
    s32 i;
    s32 k;

    for (i = count - 1; i != -1; i--) {
        k = indices[i];
        out[k].vx = in[k].vx;
        out[k].vy = in[k].vy;
        out[k].vz = in[k].vz;
    }
}

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
