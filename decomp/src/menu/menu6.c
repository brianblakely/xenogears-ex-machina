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

/* The 49 gears of the selection list: id, model file and name (strings of
 * D_80070284, which holds them in reverse order). */
ListEntry D_80091964[49] = {
    { 0, &D_80070284[0x36C], &D_80070284[0x364] },     /* ply_01 WELTALL */
    { 1, &D_80070284[0x35C], &D_80070284[0x354] },     /* ply_03 VIERGE */
    { 2, &D_80070284[0x34C], &D_80070284[0x344] },     /* ply_04 HEIMDAL */
    { 3, &D_80070284[0x33C], &D_80070284[0x330] },     /* ply_05 BRIGANDIER */
    { 4, &D_80070284[0x328], &D_80070284[0x31C] },     /* ply_06 RENMAZUO */
    { 5, &D_80070284[0x314], &D_80070284[0x30C] },     /* ply_07 STIER */
    { 6, &D_80070284[0x304], &D_80070284[0x2F8] },     /* ply_08 BLADEGASH */
    { 7, &D_80070284[0x2F0], &D_80070284[0x2E4] },     /* ply_09 SIEBZEHN */
    { 8, &D_80070284[0x2DC], &D_80070284[0x2D0] },     /* ply_10 CRESCENS */
    { 9, &D_80070284[0x2C8], &D_80070284[0x2C0] },     /* ply_11 CHU-CHU */
    { 0xA, &D_80070284[0x2B8], &D_80070284[0x2AC] },   /* ply_02 WELTALL-2 */
    { 0xB, &D_80070284[0x2A4], &D_80070284[0x298] },   /* ply_12 XENOGEARS */
    { 0xC, &D_80070284[0x290], &D_80070284[0x284] },   /* ply_13 EL-REGRS */
    { 0xD, &D_80070284[0x27C], &D_80070284[0x270] },   /* ply_14 EL-FENRIR */
    { 0xE, &D_80070284[0x268], &D_80070284[0x25C] },   /* ply_15 EL-ANDVARI */
    { 0xF, &D_80070284[0x254], &D_80070284[0x248] },   /* ply_16 EL-RENMAZUO */
    { 0x10, &D_80070284[0x240], &D_80070284[0x234] },  /* ply_17 EL-STIER */
    { 0x11, &D_80070284[0x22C], &D_80070284[0x224] },  /* batt_01 GANADOR */
    { 0x12, &D_80070284[0x21C], &D_80070284[0x214] },  /* batt_02 TITAN */
    { 0x13, &D_80070284[0x20C], &D_80070284[0x204] },  /* batt_03 WSHAVER */
    { 0x14, &D_80070284[0x1FC], &D_80070284[0x1F0] },  /* sol_11 FIREWHEEL */
    { 0x15, &D_80070284[0x1E8], &D_80070284[0x1DC] },  /* batt_06 SILVERSTAR */
    { 0x16, &D_80070284[0x1D4], &D_80070284[0x1CC] },  /* batt_04 ARGENTO */
    { 0x17, &D_80070284[0x1C4], &D_80070284[0x1BC] },  /* kis_02 MUSHA */
    { 0x18, &D_80070284[0x1B4], &D_80070284[0x1A8] },  /* kis_01 HATAMOTO */
    { 0x19, &D_80070284[0x1A0], &D_80070284[0x194] },  /* kis_05 BACKFIRER */
    { 0x1A, &D_80070284[0x18C], &D_80070284[0x184] },  /* kis_03 SHINOBI */
    { 0x1B, &D_80070284[0x17C], &D_80070284[0x174] },  /* cre_01 WYRM */
    { 0x1C, &D_80070284[0x16C], &D_80070284[0x160] },  /* yas_01 TIN ROBO */
    { 0x1D, &D_80070284[0x158], &D_80070284[0x150] },  /* bos_02 RANKAR */
    { 0x1E, &D_80070284[0x148], &D_80070284[0x140] },  /* kyo_01 ETONE1 */
    { 0x1F, &D_80070284[0x138], &D_80070284[0x130] },  /* kyo_02 ETONE2 */
    { 0x20, &D_80070284[0x128], &D_80070284[0x120] },  /* cre_03 GOLEM */
    { 0x21, &D_80070284[0x118], &D_80070284[0x110] },  /* yas_03 FIXBOT */
    { 0x22, &D_80070284[0x108], &D_80070284[0x100] },  /* tuti_01 WORKER */
    { 0x23, &D_80070284[0xF8], &D_80070284[0xF0] },    /* tuti_02 DOZER */
    { 0x24, &D_80070284[0xE8], &D_80070284[0xE0] },    /* cre_14 DEATH */
    { 0x25, &D_80070284[0xD8], &D_80070284[0xD0] },    /* miz_04 MERMAN */
    { 0x26, &D_80070284[0xC8], &D_80070284[0xBC] },    /* yas_02 SALVAGER */
    { 0x27, &D_80070284[0xB4], &D_80070284[0xAC] },    /* ave_01 TROOPER */
    { 0x28, &D_80070284[0xA4], &D_80070284[0x98] },    /* ave_02 TWINBURNER */
    { 0x29, &D_80070284[0x90], &D_80070284[0x84] },    /* ave_04 S-TROOPER */
    { 0x2A, &D_80070284[0x7C], &D_80070284[0x70] },    /* ave_05 S-TRIPPER */
    { 0x2B, &D_80070284[0x68], &D_80070284[0x60] },    /* cre_16 SUFAL */
    { 0x2C, &D_80070284[0x58], &D_80070284[0x4C] },    /* sol_01 EG-GUNNER */
    { 0x2D, &D_80070284[0x44], &D_80070284[0x38] },    /* sol_02 EG-ARMOR */
    { 0x2E, &D_80070284[0x30], &D_80070284[0x24] },    /* sol_03 PEDESTAL */
    { 0x2F, &D_80070284[0x1C], &D_80070284[0x14] },    /* sol_10 EDIN */
    { 0x30, &D_80070284[0xC], D_80070284 },            /* sol_13 EG-BLADE */
};

/* Heap block tag names for the debug display (func_80032498 kind 6). */
char *D_80091BB0[] = {
    &D_80070284[0x424], /* "" */
    &D_80070284[0x41C], /* OBJECT */
    &D_80070284[0x414], /* CAMEPOS */
    &D_80070284[0x40C], /* TASK */
    &D_80070284[0x404], /* ROOT */
    &D_80070284[0x3FC], /* SHPRIM */
    &D_80070284[0x3F4], /* SHROOT */
    &D_80070284[0x3EC], /* SHADOW */
    &D_80070284[0x3E4], /* ELEM */
    &D_80070284[0x3DC], /* SCENE */
    &D_80070284[0x3D8], /* HRC */
    &D_80070284[0x3D0], /* LIGHT */
    &D_80070284[0x3C8], /* OTAG */
    &D_80070284[0x3BC], /* ANM_DAT_HED */
    &D_80070284[0x3B4], /* ANM_DAT */
    &D_80070284[0x3AC], /* ELHED */
    &D_80070284[0x3A8], /* ANM */
    &D_80070284[0x3A0], /* MOTION */
    &D_80070284[0x398], /* ELIST */
    &D_80070284[0x390], /* SHVTEX */
    &D_80070284[0x384], /* PARTICLE */
    &D_80070284[0x37C], /* PT_SRC */
    &D_80070284[0x374], /* PT_PART */
};

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
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

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

extern const char D_800706D4[20];

/* Menu mode entry: start up, start the mode's task, then run the frame
 * loop forever (resume the task, build one buffer while the other is
 * shown, debug meters). The original reads the tick counter at entry but
 * leaves `last` uninitialized until the first frame's rate calculation. */
void func_80088E90(void) {
    DispEnv disp;
    Task *task;
    s32 last;
    s32 fps;
    s32 load;
    u8 index;

    func_80088D1C();
    task = func_8008BA2C(D_80088BFC[D_80050618], 0, (u32 *)0x801FE000, 0x400);
    D_80059488;
frame:
    D_800595C0 = 0;
    D_80059578 = 0;
    /* Keep the byte written to the draw-buffer selector for this frame. */
    index = D_800928A0 = (D_800928E8 + 1) & 1;
    D_80092870 = &D_8009A0D8[D_800928E8 & 1];
    D_800928E8++;
    D_80092868 = &D_8009A0D8[index];
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
        func_8003700C((char *)D_800706D4, fps);
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

/* The original rate-format object retains data after its terminator. */
const char D_800706D4[20] = "RATE   : %3dfps\n\0\0\x94\x08";
