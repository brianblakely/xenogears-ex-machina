#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/area.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"

/* Sound programs requested for each battle mode; 0xff absent. */
u8 D_8004F388[6][3] = {
    {1, 2, 0xFF}, {3, 4, 0xFF}, {5, 6, 7}, {8, 9, 10}, {1, 2, 0xFF}, {1, 2, 0xFF},
};

/* Menu screen names, in 256-byte buffers. */
char D_8004F39C[7][0x100] = {
    "Normal Menu", "Member Change", "Load Game", "Enter Name", "Shop", "Robo", "CD Change",
};
char *D_8004FA9C[7] = {
    D_8004F39C[0], D_8004F39C[1], D_8004F39C[2], D_8004F39C[3],
    D_8004F39C[4], D_8004F39C[5], D_8004F39C[6],
};

/* Stripped debug hooks share this no-op entry. Their old argument lists
 * and forwarded return register remain part of the calling sequence, so the
 * calls here are K&R ones of an s32 function (its definition in
 * main2_800366E0.c is void (void)). */
s32 func_800379D0();
void func_8001B94C(DRAWENV *env);

/* Initialize the battle's 320x224 double buffer (the battle area's two
 * frame buffers) and GTE projection. Each draw environment is reached from
 * the display environment after it. */
void func_8001B844(void) {
    DISPENV *disp;
    DRAWENV *draw;
    DRAWENV *otherDraw;

    ResetGraph(1);
    func_800379D0(0x300, 0);
    func_800379D0(func_800379D0(8, 0x10, 0x140, 0xF0, 0, 0x1000));
    InitGeom();
    SetGeomOffset(0xA0, 0xB4);
    SetGeomScreen(0x200);
    disp = &D_800C3EB0.buffers[0].dispEnv;
    SetDefDispEnv(disp, 0, 0xE0, 0x140, 0xE0);
    draw = (DRAWENV *)((u8 *)disp - sizeof(DRAWENV));
    SetDefDrawEnv(draw, 0, 0, 0x140, 0xE0);
    SetDefDispEnv((DISPENV *)((u8 *)disp + sizeof(FrameBuffer)), 0, 0, 0x140, 0xE0);
    otherDraw = (DRAWENV *)((u8 *)disp + sizeof(FrameBuffer) - sizeof(DRAWENV));
    SetDefDrawEnv(otherDraw, 0, 0xE0, 0x140, 0xE0);
    func_8001B94C(draw);
    func_8001B94C(otherDraw);
}

/* Battle draw environment: clear the background to (0x3c, 0x78, 0x78) with dithering. */
void func_8001B94C(DRAWENV *env) {
    env->isbg = 1;
    env->dtd = 1;
    env->r0 = 0x3C;
    env->g0 = 0x78;
    env->b0 = 0x78;
}

/* The second byte of the saved name slots: 8001b970 reads them from a base
 * of their own, which D_8006D634.names does not compile to (link.ld). */
extern u8 D_8006D635[];
/* The battle script variables. 8001b970 clears twenty halfwords back from
 * [19]: the 16 variables and the first 8 bytes of the sound driver's SPU
 * attributes D_8005A3C0 that follow them. The battle reads them signed, so
 * the shared headers leave them out. */
extern u16 D_8005A3A0[];
u8 D_800594CC;
u8 D_8005947C; /* pending scene + 1 */
/* Callers in other targets declare these two differently. The resident's
 * own prototypes (own_declarations.h) are not included here: its window.h
 * declares the window colour as the array this unit cannot. */
void func_80033B34(u16 *codes, u8 *out, u32 count);
void func_80039DB8(s32 program);

/* Load directory 16 file 3 into the saved game data, decode the first
 * 31 twenty-byte name slots, and clear the battle script variables. */
void func_8001B970(void) {
    u16 codes[12]; /* 24-byte workspace; at most ten codes per name */
    u8 decoded[20];
    void *file;
    u16 *counter;
    s32 slot;
    s32 i;
    u8 *low;
    u8 *high;

    func_80028470(16, 0);
    func_80032498(2, 0);
    file = func_80031BDC(func_800288EC(3), 1);
    func_800295D8(3, file, 0, 0x80);
    func_80028A60(0);
    memmove(&D_8006D634, file, 0x2358);
    func_800320E8(file);
    for (slot = 0; slot < 31; slot++) {
        for (i = 0; i < 20; i += 2) {
            low = (u8 *)codes;
            high = low + 1;
            low[i] = D_8006D634.names[slot][i];
            high[i] = D_8006D635[slot * 20 + i];
            if (D_8006D634.names[slot][i] == 0xF && D_8006D635[slot * 20 + i] == 0) {
                break;
            }
        }
        func_80033B34(codes, decoded, i / 2);
        for (i = 0; i < 20; i++) {
            D_8006D634.names[slot][i] = decoded[i];
        }
    }
    for (i = 19, counter = &D_8005A3A0[19]; i >= 0; i--) {
        *counter-- = 0;
    }
    D_800594CC = 6;
    D_8005947C = 0;
}

/* Pass the scene selector 8006f9de and three resident tables to 800379d8. */
void func_8001BB0C(void) {
    func_800379D8(D_8006F9DC[2], 0, &D_80059470, &D_80059520, &D_8005949C);
}

u8 D_800594F8;
u8 D_8005946C;
/* The window colour is one object, the u8[3] the overlays declare: under
 * the slot rule no other variable shares its word. ASPSX 2.34 addressed a
 * common at an offset absolutely, so only [0] went through $gp (maspsx
 * models that), but GNU as moves a small common's offset accesses to $gp
 * as well, so the array cannot be declared here and its last two bytes are
 * names of their own (link.ld). */
u8 D_800594D4;
extern u8 D_800594D5;
extern u8 D_800594D6;
s32 D_800595A0;
void func_8001B970(void);

/* Set the battle setup flags, initialize battle setup data, wait for disc I/O,
 * then install the three initial bytes and the phase selector. */
void func_8001BB50(void) {
    D_800594F8 = 1;
    D_8005946C = 0;
    func_8001B970();
    func_80028A60(0);
    D_800594D4 = 0x88;
    D_800594D5 = 0x76;
    D_800594D6 = 0x54;
    D_800595A0 = 2;
}

/* This unit's small commons, addressed through $gp; they merge with
 * commons/common_80059404.c's definitions. */
void *D_80059480; /* heap marker for the high-memory reservation */
void *D_800594AC; /* reservation below the heap marker */
SoundBank *D_800595D0;
void *D_800595A8;
u8 D_8005954C;

/* Reserve high memory, load a sound bank and files 3 and 4, then request
 * the mode's three optional sound programs. */
void func_8001BBAC(void) {
    s32 i;
    u8 program;
    void *overlay;
    void *file;

    func_80032498(2, 0);
    func_80028470(12, 0);
    D_80059480 = func_80031BDC(4, 1);
    D_800594AC = func_80031BDC((u32)D_80059480 - 0x801E4000U, 1);
    overlay = (void *)0x801E4000;
    D_800595D0 = func_80031BDC(func_800288EC(2), 1);
    file = func_80031BDC(func_800288EC(3), 1);
    D_8006F9BC[0].file = 2;
    D_8006F9BC[1].file = 3;
    D_800595A8 = file;
    D_8006F9BC[1].destination = file;
    D_8006F9BC[2].file = 4;
    D_8006F9BC[2].destination = overlay;
    D_8006F9BC[3].file = 0;
    D_8006F9BC[3].destination = NULL;
    D_8006F9BC[0].destination = D_800595D0;
    func_80029AFC(D_8006F9BC, 0, 0x80);
    while (func_800286CC() == 3) {
    }
    func_80038428(D_800595D0);
    if (D_8005954C != 4) {
        for (i = 0; i < 3; i++) {
            program = ((u8 *)D_8004F388)[D_8005954C * 3 + i];
            if (program != 0xFF) {
                func_80039DB8(((u32)D_800595D0->id << 16) | program);
            }
        }
    }
}

/* A random byte from the requested range. Low 0xff is returned unchanged;
 * otherwise high 0 returns 0 and equal bounds return low. A span of 0xff
 * uses the whole random byte; smaller signed spans use modulo span + 1. */
u8 func_8001BD40(u8 low, u8 high) {
    s32 span;

    if (low != 0xFF) {
        if (high == 0) {
            return 0;
        }
        if (low == high) {
            return low;
        }
        span = high - low;
        if (span < 0xFF) {
            return low + (u8)rand() % (span + 1);
        }
        return rand();
    }
    return low;
}

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
    MenuState *work = D_800625A0;

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
    MenuState *work;

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
        D_8006D634.gold = 999999999;
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
            D_8005A4AC[0] = func_80031BDC(0x4000, 0);
            D_8005A4AC[1] = func_80031BDC(0x4000, 0);
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
            func_800320E8(D_8005A4AC[0]);
            func_800320E8(D_8005A4AC[1]);
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
