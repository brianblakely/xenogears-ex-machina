#ifndef MENU_SELECT_H
#define MENU_SELECT_H

#include "common.h"
#include "psyq/libgpu.h"
#include "packets.h"

/* The selection screen (arena_menu_screens 8007EEE8-8007F834, 800802A4-800808F4): the
 * list of the 49 entries, their portraits in VRAM, and the two sides'
 * selection wheels. */

/* An entry of the list: its id, model file and name. */
typedef struct ListEntry {
    s32 id;
    char *model; /* 0x04: model file name */
    u8 *name;    /* 0x08 */
} ListEntry;

/* VRAM areas of one of the 49 portrait slots (20 bytes; arena_select_portrait_slots):
 * its palette row and its 30x64 image. */
typedef struct {
    RECT clut;
    RECT image;
    u8 unk10[4];
} GridCell;

extern s16 arena_select_wheel_neighbors[2][4];       /* neighbour offsets and slide of the wheel portraits, per row */
extern ListEntry arena_select_gears[49];
extern s32 arena_select_entry_count;                 /* entries in the list */
extern u8 *arena_select_portraits;                   /* the 49 portraits, 0x1000 bytes each */
extern ListEntry **arena_select_entries;             /* the list */
extern s32 arena_select_portraits_in_vram;           /* the portraits are in VRAM */
/* Two-player selection wheels: each side's portraits per buffer, and the
 * neighbour offsets and slide of the portraits beside the pick (row 1
 * while sliding right or still). */
extern PolyFT4Words arena_select_wheel_quads[2][10];

void arena_select_build_list(s32 filter);
void arena_select_alloc_portrait_slots(void);
void arena_select_draw_portrait(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade);
void arena_select_draw_wheels(void *ot, s32 arg);
void arena_select_load_picked_models(void);
void arena_select_load_pick_portraits(s32 first, s32 second);

#endif
