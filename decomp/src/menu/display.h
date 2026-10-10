#ifndef MENU_DISPLAY_H
#define MENU_DISPLAY_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "packets.h"

/* The display (arena_scene_graph_and_opponent 80089210-800898BC, 8008A2B8-8008A3E0, 8008AC0C-
 * 8008AF6C; arena_stage_views_and_hud 80083BB4): the two display buffers, the screen setup,
 * the drawing layers with their ordering tables, and the frame's time
 * budget. */

/* One of the two display buffers (table at 0x8009a0d8, 0xF8 bytes each). */
typedef struct DisplayBuffer {
    DRAWENV draw;      /* 0x00 */
    DISPENV disp;      /* 0x5C */
    u32 ot;            /* 0x70: one-entry ordering table */
    SPRT_16 sprite;    /* 0x74 */
    u8 unk84[0x4C];
    DR_AREA area;      /* 0xD0 */
    DR_OFFSET offset;  /* 0xDC */
    TILE background;   /* 0xE8: also the screen fade */
} DisplayBuffer;

/* Drawing layer (0x68 bytes): an ordering table per display buffer with
 * its drawing area, offset and background packets. */
typedef struct {
    s32 unk0;
    u32 *ot[2];        /* 0x04: per display buffer */
    u32 *last[2];      /* 0x0C: last entry of each */
    s16 length;        /* 0x14 */
    u8 flags;          /* 0x16: 4 own area, 8 own offset, 0x10 background */
    u8 shift;          /* 0x17: 14 - log2(length) */
    DR_AREA area[2];   /* 0x18: per buffer */
    DR_OFFSET offset[2]; /* 0x30: per buffer */
    TileWords tile[2]; /* 0x48 */
} OtPair;

/* Word count of a primitive, from its tag (libgpu P_TAG len). */
#define TAG_LEN(tag) (((u8 *)(tag))[3])

extern OtPair *arena_display_layer_to_compact;   /* table to compact at the end of the frame */
extern s16 arena_display_width;                  /* display width */
extern DisplayBuffer *arena_current_draw_buffer; /* the buffer being drawn */
extern s16 arena_display_height;                 /* display height */
extern DisplayBuffer *arena_unread_shown_buffer; /* the buffer being displayed */
extern u8 arena_draw_buffer_index;               /* index of the buffer being built */
extern u32 *arena_current_layer_ot;              /* ordering table primitives are added to */
extern s32 arena_node_color_changed;             /* colour changed this frame */
extern u32 *arena_current_ot;                    /* ordering table of the buffer being built */
extern MATRIX arena_display_screen_scale;        /* screen scale */
extern DisplayBuffer arena_display_buffers[2];

void arena_display_clear_buffers(s32 both); /* clear one or both display areas */
void arena_display_set_screen_scale(s32 width, s32 height);
void arena_display_set_disp_envs(s32 width, s32 height);
void arena_display_set_draw_envs(s32 width, s32 height);
void arena_display_set_resolution(s32 width, s32 height);
OtPair *arena_display_alloc_layer(u16 length);
void arena_display_free_layer(OtPair *layer);
void arena_display_start_layer(OtPair *pair);
void arena_display_choose_layer_to_compact(OtPair *pair);
void arena_display_note_frame_start(void);
void arena_display_compact_layer(s32 frames);
void arena_display_link_layer(OtPair *layer);

#endif
