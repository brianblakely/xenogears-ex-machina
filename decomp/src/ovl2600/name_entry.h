#ifndef OVL2600_NAME_ENTRY_H
#define OVL2600_NAME_ENTRY_H

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "menu/card.h"
#include "menu/panel.h"
#include "menu/screen.h"

/* Sprite part packets built by func_8002675C: one quad per draw buffer. */
typedef struct {
    POLY_FT4 poly[2];
} SpriteParts;

/* The name entry block (0xDEC bytes). */
typedef struct NameEntry {
    POLY_FT4 confirm[2];     /* 0x0: sprite 0xF9 */
    POLY_FT4 back[2];        /* 0x50: sprite 0xFC */
    POLY_FT4 frame[4];       /* 0xA0: sprite 0xF0 */
    POLY_FT4 cursor[2];      /* 0x140: sprite 0x14B */
    POLY_FT4 chars[72];      /* 0x190: the 36 grid characters */
    POLY_FT4 name[2];        /* 0xCD0: the name being entered */
    LINE_F3 line_a[2];       /* 0xD20 */
    LINE_F3 line_b[2];       /* 0xD50 */
    LINE_F2 caret[2];        /* 0xD80 */
    SVECTOR cursor_at[4];    /* 0xDA0 */
    SVECTOR name_at[4];      /* 0xDC0 */
    u8 parts_buffer;         /* 0xDE0 */
    u8 cursor_buffer;        /* 0xDE1 */
    u8 chars_buffer;         /* 0xDE2 */
    u8 name_buffer;          /* 0xDE3 */
    u8 lines_buffer;         /* 0xDE4 */
    u8 shown;                /* 0xDE5 */
    u8 grid_shown;           /* 0xDE6 */
    u8 blink;                /* 0xDE7 */
    u8 length;     /* 0xDE8: codes entered */
    u8 max_length; /* 0xDE9 */
    u8 frame_count; /* 0xDEA */
    u8 pad_DEB;
} NameEntry;



extern void func_80039DB8(s32 sound);          /* play a sound */
extern s32 D_80059488;                         /* vsync count */
extern void func_80033B34(void *codes, u8 *text, s32 count); /* decode text codes */
extern s32 func_8002675C(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y,
                         s32 scale);
extern s32 func_800263E4(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y,
                          s32 scale, s32 a, s32 b);
extern void *func_80033728(void *table, s32 index);      /* message address */
extern u8 func_80034EAC(void *text, u8 *image, s32 a, s32 b); /* render text */
extern void func_80033698(s32 a, s32 b);

#endif
