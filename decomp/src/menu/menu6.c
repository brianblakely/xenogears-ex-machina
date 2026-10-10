/* menu6: text 80088BFC-800891C0, rodata 80070284-800706E8 (strings only),
 * data 80091964-80091C0C, variables 800927F0-80092800. The menu mode's
 * entry and frame loop: the mode-task table, the start-up, the debug
 * meters, and the list of the 49 gears. Its .text opens with the task
 * table D_80088BFC, a data word between menu5's last return and
 * func_80088C00 that no other unit keeps in .text (menu.classification.txt),
 * and the gear list's strings open its rodata. */
#include "common.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "display.h"
#include "effects.h"
#include "mode.h"
#include "node.h"
#include "resident_views.h"
#include "select.h"
#include "sound.h"
#include "task.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static s32 D_800927F0;
static s32 D_800927F4;
static s32 D_800927F8[2]; /* unreferenced */

/* The mode's tasks, indexed by the resident mode word mode_arena_task; the only
 * entry is the menu task. The table is stored in .text, ahead of the code. */
void (*D_80088BFC[])(s32) __attribute__((section(".text"))) = { func_800852C4 };

/* The 49 gears of the selection list: id, model file and name. GCC emits an
 * initializer's string literals last to first, so they open the unit's
 * rodata in reverse order; the last, "ply_01", has a stray byte the
 * original assembler left in its alignment padding and stays original
 * (D_800705F0, linked right after them). */
extern char D_800705F0[];
ListEntry D_80091964[49] = {
    { 0, D_800705F0, "WELTALL" }, /* "ply_01" */
    { 1, "ply_03", "VIERGE" },
    { 2, "ply_04", "HEIMDAL" },
    { 3, "ply_05", "BRIGANDIER" },
    { 4, "ply_06", "RENMAZUO" },
    { 5, "ply_07", "STIER" },
    { 6, "ply_08", "BLADEGASH" },
    { 7, "ply_09", "SIEBZEHN" },
    { 8, "ply_10", "CRESCENS" },
    { 9, "ply_11", "CHU-CHU" },
    { 0xA, "ply_02", "WELTALL-2" },
    { 0xB, "ply_12", "XENOGEARS" },
    { 0xC, "ply_13", "EL-REGRS" },
    { 0xD, "ply_14", "EL-FENRIR" },
    { 0xE, "ply_15", "EL-ANDVARI" },
    { 0xF, "ply_16", "EL-RENMAZUO" },
    { 0x10, "ply_17", "EL-STIER" },
    { 0x11, "batt_01", "GANADOR" },
    { 0x12, "batt_02", "TITAN" },
    { 0x13, "batt_03", "WSHAVER" },
    { 0x14, "sol_11", "FIREWHEEL" },
    { 0x15, "batt_06", "SILVERSTAR" },
    { 0x16, "batt_04", "ARGENTO" },
    { 0x17, "kis_02", "MUSHA" },
    { 0x18, "kis_01", "HATAMOTO" },
    { 0x19, "kis_05", "BACKFIRER" },
    { 0x1A, "kis_03", "SHINOBI" },
    { 0x1B, "cre_01", "WYRM" },
    { 0x1C, "yas_01", "TIN ROBO" },
    { 0x1D, "bos_02", "RANKAR" },
    { 0x1E, "kyo_01", "ETONE1" },
    { 0x1F, "kyo_02", "ETONE2" },
    { 0x20, "cre_03", "GOLEM" },
    { 0x21, "yas_03", "FIXBOT" },
    { 0x22, "tuti_01", "WORKER" },
    { 0x23, "tuti_02", "DOZER" },
    { 0x24, "cre_14", "DEATH" },
    { 0x25, "miz_04", "MERMAN" },
    { 0x26, "yas_02", "SALVAGER" },
    { 0x27, "ave_01", "TROOPER" },
    { 0x28, "ave_02", "TWINBURNER" },
    { 0x29, "ave_04", "S-TROOPER" },
    { 0x2A, "ave_05", "S-TRIPPER" },
    { 0x2B, "cre_16", "SUFAL" },
    { 0x2C, "sol_01", "EG-GUNNER" },
    { 0x2D, "sol_02", "EG-ARMOR" },
    { 0x2E, "sol_03", "PEDESTAL" },
    { 0x2F, "sol_10", "EDIN" },
    { 0x30, "sol_13", "EG-BLADE" },
};

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu6", D_800705F0);

/* Names of the menu's heap block kinds (owner tag 6, heap_select_owner_tag); their
 * literals follow "ply_01", also last to first. */
char *D_80091BB0[] = {
    "", "OBJECT", "CAMEPOS", "TASK", "ROOT", "SHPRIM", "SHROOT", "SHADOW", "ELEM",
    "SCENE", "HRC", "LIGHT", "OTAG", "ANM_DAT_HED", "ANM_DAT", "ELHED", "ANM",
    "MOTION", "ELIST", "SHVTEX", "PARTICLE", "PT_SRC", "PT_PART",
};

/* Draw-sync callback: note the vertical blank count at the end of drawing. */
void func_80088C00(void) {
    D_800927F0 = VSync(1);
}

/* Debug counter: pad bits 0x4/0x1 step it up/down (not below zero), then
 * print it. */
void func_80088C28(void) {
    u16 pad = pad_port0_held;

    if (pad & 4) {
        D_800927F4++;
    }
    if (pad & 1) {
        D_800927F4--;
    }
    if (D_800927F4 < 0) {
        D_800927F4 = 0;
    }
    heap_print_report(1, D_800927F4, 10, 0x80AD);
}

/* Set up the given window record's defaults. */
void func_80088CBC(s32 index) {
    DisplayBuffer *window = &D_8009A0D8[index];

    setlen(&window->sprite, 3);
    setcode(&window->sprite, 0x7D);
    *(u16 *)&window->sprite.u0 = 0x3000;
    window->sprite.clut = GetClut(0x3F0, 0xC0);
}

/* Menu mode start-up: frame callback, display and windows, the start
 * state from the boot word, then the mode's first screen. */
void func_80088D1C(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    DrawSyncCallback(func_80088C00);
    InitGeom();
    heap_select_owner_tag(6, D_80091BB0);
    cd_select_directory(0x30, 0);
    console_open(4, 2, 0x138, 0xDA, 0x14, 1, 0x3C0, 0x1F0, 0x3C0, 0x1EF, 0);
    func_80088CBC(0);
    func_80088CBC(1);
    switch (mode_disc_mode) {
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

extern char D_800706D4[]; /* "RATE   : %3dfps\n" */

/* Menu mode entry: start up, start the mode's task, then run the frame
 * loop forever (resume the task, build one buffer while the other is
 * shown, debug meters). The original reads the tick counter at entry but
 * leaves `last` uninitialized until the first frame's rate calculation. */
void func_80088E90(void) {
    DISPENV disp;
    TaskContext *task;
    s32 last;
    s32 fps;
    s32 load;
    u8 index;

    func_80088D1C();
    task = func_8008BA2C(D_80088BFC[mode_arena_task], 0, (u32 *)0x801FE000, 0x400);
    pad_vblank_count;
frame:
    model_submitted_primitive_count = 0;
    model_drawn_primitive_count = 0;
    /* Keep the byte written to the draw-buffer selector for this frame. */
    index = D_800928A0 = (D_800928E8 + 1) & 1;
    D_80092870 = &D_8009A0D8[D_800928E8 & 1];
    D_800928E8++;
    D_80092868 = &D_8009A0D8[index];
    D_80092938 = &D_80092868->ot;
    disp = D_80092868->disp;
    boot_check_soft_reset();
    TermPrim(D_80092938);
    if ((D_800928D0 & 0x10) && D_80092930 != NULL) {
        D_80092930(D_80092938);
    }
    console_flush((u_long *)D_80092938);
    func_8008BB3C(task);
    func_8008EADC();
    heap_update_delayed_frees();
    if (D_80092920 & 1) {
        AddPrim(D_80092938, &D_80092868->background);
    }
    load = VSync(1);
    fps = 60 / (u32)(pad_vblank_count - last);
    last = pad_vblank_count;
    if (D_800928D0 & 8) {
        func_80088C28();
    }
    if (D_800928D0 & 1) {
        console_printf("POLYGON:%4d/%4d\n", model_drawn_primitive_count, model_submitted_primitive_count);
    }
    if (D_800928D0 & 2) {
        console_printf("CPU/GPU:%4d/%3d\n", load, D_800927F0);
    }
    if (D_800928D0 & 4) {
        console_printf(D_800706D4, fps);
    }
    console_set_color(0xFF, 0xFF, 0xFF);
    func_8008ACB8(D_80092898);
    VSync(D_80092898);
    func_8008AC8C();
    DrawSync(0);
    PutDispEnv(&disp);
    DrawOTagEnv((u_long *)D_80092938, &D_80092868->draw);
    goto frame;
}

/* "RATE   : %3dfps\n". Stray bytes (0x94, 0x08) follow it at the end of the
 * unit's rodata; it is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu6", D_800706D4);
