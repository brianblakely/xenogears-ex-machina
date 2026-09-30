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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001B6C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001B844);

/* Battle draw environment: clear the background to (0x3c, 0x78, 0x78) with dithering. */
void func_8001B94C(DRAWENV *env) {
    env->isbg = 1;
    env->dtd = 1;
    env->r0 = 0x3C;
    env->g0 = 0x78;
    env->b0 = 0x78;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001B970);

/* Pass the scene selector 8006f9de and three resident tables to 800379d8. */
void func_8001BB0C(void) {
    func_800379D8(D_8006F9DE, 0, D_80059470, D_80059520, D_8005949C);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001BB50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001BBAC);

/* A random byte in [low, high]: any byte for an unset low (0xff) or a range of 0xff or wider, 0 for an unset high. */
/* Nonmatching: GCC 2.6.3 cross-jumps the low == high return into the tail of the modulo result; the original keeps its own. */
#ifdef NON_MATCHING
u8 func_8001BD40(u8 low, u8 high) {
    s32 span;

    if (low != 0xFF) {
        if (high == 0) {
            return 0;
        }
        span = high - low;
        if (low == high) {
            return low;
        }
        if (span < 0xFF) {
            return low + (u8)rand() % (span + 1);
        }
    }
    return rand();
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001BD40);
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
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8001B6C4", func_8001C76C);
