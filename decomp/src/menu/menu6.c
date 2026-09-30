#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

/* The mode's tasks, indexed by the resident mode word D_80050618; the only
 * entry is the menu task. The table is stored in .text, ahead of the code. */
void (*D_80088BFC[])(s32) __attribute__((section(".text"))) = { func_800852C4 };

/* Draw-sync callback: note the vertical blank count at the end of drawing. */
void func_80088C00(void) {
    D_800927F0 = VSync(1);
}

/* Debug counter: pad bits 0x4/0x1 step it up/down (not below zero), then
 * print it. */
void func_80088C28(void) {
    u16 pad = D_80059570;

    if (pad & 4) {
        D_800927F4++;
    }
    if (pad & 1) {
        D_800927F4--;
    }
    if (D_800927F4 < 0) {
        D_800927F4 = 0;
    }
    func_8003278C(1, D_800927F4, 10, 0x80AD);
}

/* Set up the given window record's defaults. */
void func_80088CBC(s32 index) {
    Window *window = &D_8009A0D8[index];

    window->sprite.len = 3;
    window->sprite.code = 0x7D;
    *(u16 *)&window->sprite.u0 = 0x3000;
    window->sprite.clut = GetClut(0x3F0, 0xC0);
}

/* Menu mode start-up: frame callback, display and windows, the start
 * state from the boot word, then the mode's first screen. */
void func_80088D1C(void) {
    s32 unused[2]; /* never used; the original frame has these 8 bytes */

    DrawSyncCallback(func_80088C00);
    InitGeom();
    func_80032498(6, D_80091BB0);
    func_80028470(0x30, 0);
    func_800374E8(4, 2, 0x138, 0xDA, 0x14, 1, 0x3C0, 0x1F0, 0x3C0, 0x1EF, 0);
    func_80088CBC(0);
    func_80088CBC(1);
    switch (D_80010000) {
    case -1:
        D_800928CC = 2;
        break;
    case 0:
        D_800928CC = 1;
        break;
    default:
        D_800928CC = 0;
        break;
    }
    D_80092868 = &D_8009A0D8[0];
    D_80092870 = &D_8009A0D8[1];
    func_8008A110(-1, -1);
    func_8008A128(-1, -1);
    D_80092898 = 2;
    D_8009289C = 1;
    D_80092930 = NULL;
    D_800928E8 = 0;
    D_800928A0 = 0;
    D_80092920 = 0;
    D_80092930 = NULL;
    D_800928D0 = 7;
    func_8008E620();
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu6", D_80070284);

#ifdef NON_MATCHING
/* Menu mode entry: start up, start the mode's task, then run the frame
 * loop forever (resume the task, build one buffer while the other is
 * shown, debug meters).
 * Does not match: the buffer flip stores are scheduled differently, and the
 * original rate string is followed by two non-zero padding bytes (0x0894)
 * that a C literal cannot reproduce. */
void func_80088E90(void) {
    DispEnv disp;
    Task *task;
    s32 last;
    s32 fps;
    s32 load;

    func_80088D1C();
    task = func_8008BA2C(D_80088BFC[D_80050618], 0, (u32 *)0x801FE000, 0x400);
    D_80059488;
frame:
    D_800595C0 = 0;
    D_80059578 = 0;
    D_800928A0 = (D_800928E8 + 1) & 1;
    D_80092870 = &D_8009A0D8[D_800928E8 & 1];
    D_800928E8++;
    D_80092868 = &D_8009A0D8[D_800928A0];
    D_80092938 = &D_80092868->ot;
    disp = D_80092868->disp;
    func_80019CA0();
    TermPrim(D_80092938);
    if ((D_800928D0 & 0x10) && D_80092930 != NULL) {
        D_80092930(D_80092938);
    }
    func_80037324(D_80092938);
    func_8008BB3C(task);
    func_8008EADC();
    func_80032CB8();
    if (D_80092920 & 1) {
        AddPrim(D_80092938, &D_80092868->background);
    }
    load = VSync(1);
    fps = 60 / (u32)(D_80059488 - last);
    last = D_80059488;
    if (D_800928D0 & 8) {
        func_80088C28();
    }
    if (D_800928D0 & 1) {
        func_8003700C("POLYGON:%4d/%4d\n", D_80059578, D_800595C0);
    }
    if (D_800928D0 & 2) {
        func_8003700C("CPU/GPU:%4d/%3d\n", load, D_800927F0);
    }
    if (D_800928D0 & 4) {
        func_8003700C("RATE   : %3dfps\n", fps);
    }
    func_80036DC8(0xFF, 0xFF, 0xFF);
    func_8008ACB8(D_80092898);
    VSync(D_80092898);
    func_8008AC8C();
    DrawSync(0);
    PutDispEnv(&disp);
    DrawOTagEnv(D_80092938, D_80092868);
    goto frame;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu6", func_80088E90);
#endif
