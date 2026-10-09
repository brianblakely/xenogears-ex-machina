#ifndef MENU_SELECT_H
#define MENU_SELECT_H

#include "common.h"
#include "psyq/libgpu.h"
#include "packets.h"

/* The selection screen (menu4 8007EEE8-8007F834, 800802A4-800808F4): the
 * list of the 49 entries, their portraits in VRAM, and the two sides'
 * selection wheels. */

/* An entry of the list: its id, model file and name. */
typedef struct ListEntry {
    s32 id;
    char *model; /* 0x04: model file name */
    u8 *name;    /* 0x08 */
} ListEntry;

/* VRAM areas of one of the 49 portrait slots (20 bytes; D_8009270C):
 * its palette row and its 30x64 image. */
typedef struct {
    RECT clut;
    RECT image;
    u8 unk10[4];
} GridCell;

extern s16 D_800912E0[2][4];       /* neighbour offsets and slide of the wheel portraits, per row */
extern ListEntry D_80091964[49];
extern s32 D_80092888;             /* entries in the list */
extern u8 *D_800928D8;             /* the 49 portraits, 0x1000 bytes each */
extern ListEntry **D_800928EC;     /* the list */
extern s32 D_80092940;             /* the portraits are in VRAM */
/* Two-player selection wheels: each side's portraits per buffer, and the
 * neighbour offsets and slide of the portraits beside the pick (row 1
 * while sliding right or still). */
extern PolyFT4Words D_80099DA8[2][10];

void func_8007EEE8(s32 filter);
void func_8007EFB4(void);
void func_8007F05C(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade);
void func_8007F258(void *ot, s32 flag);
void func_80080570(void);
void func_80080644(s32 first, s32 second);

#endif
