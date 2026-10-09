#ifndef OVL2598_PARTY_MENU_H
#define OVL2598_PARTY_MENU_H

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
#include "menu/panel.h"
#include "menu/screen.h"


/* The 0x5034-byte menu work block. */
typedef struct MenuCard {
    u8 pad_0[0xB80];
    TIM_IMAGE tim;    /* 0xB80: the card icon */
    u8 pad_B94[0x4B94 - 0xB94];
    u8 magic[2];      /* 0x4B94: save header "SC" */
    u8 icon_type;     /* 0x4B96 */
    u8 blocks;        /* 0x4B97 */
    u8 title[0x5C];   /* 0x4B98 */
    u8 clut[0x20];    /* 0x4BF4 */
    u8 icon[0x80];    /* 0x4C14 */
    u8 pad_4C94[0x4F7C - 0x4C94];
    s32 cursor;       /* 0x4F7C: file screen cursor */
    u8 pad_4F80[0x4FCE - 0x4F80];
    char file_name[13]; /* 0x4FCE: card file name prefix */
    u8 pad_4FDB[0x5034 - 0x4FDB];
} MenuWork;

/* Sprite part packets built by func_8002675C: one quad per draw buffer. */
typedef struct {
    POLY_FT4 poly[2];
} SpriteParts;

/* A 3D menu panel: four edge strips (two pieces per draw buffer), the frame
 * sprites, the translucent fill and the corner vectors it is projected from. */
typedef struct MenuPanel {
    POLY_FT4 corner[8];       /* 0x0: corner sprite parts, two per part */
    POLY_FT4 edge[4][4];      /* 0x140 */
    POLY_FT4 frame_side[2];   /* 0x3C0: sprite 0x106 */
    POLY_FT4 frame_ends[4];   /* 0x410: sprite 0x105 at the top, flipped at the bottom */
    POLY_G4 fill[2];          /* 0x4B0 */
    DR_MODE fill_mode[2];     /* 0x4F8 */
    SVECTOR corner_at[16];    /* 0x510: four corner quads */
    SVECTOR edge_at[4][2][4]; /* 0x590: two quads per edge */
    SVECTOR fill_at[4];       /* 0x690 */
    SVECTOR side_at[4];       /* 0x6B0 */
    SVECTOR ends_at[8];       /* 0x6D0: top and bottom quads */
    s32 corner_parts;         /* 0x710: corner parts built */
    s32 style;                /* 0x714 */
    s32 param;                /* 0x718 */
    u8 buffer;                /* 0x71C: buffer it was laid out for */
    u8 framed;                /* 0x71D: frame sprites built */
    u8 pad_71E[2];
} Panel;

/* A character status panel (0xBEC bytes): sprite quads, two per sprite
 * (one per draw buffer). */
typedef struct MenuStatusPanel {
    POLY_FT4 layout[18];  /* 0x0 */
    POLY_FT4 extra[10];   /* 0x2D0 */
    POLY_FT4 face[2];     /* 0x460 */
    POLY_FT4 label[2];    /* 0x4B0 */
    POLY_FT4 level[6];    /* 0x500 */
    POLY_FT4 next[6];     /* 0x5F0 */
    POLY_FT4 hp[10];      /* 0x6E0 */
    POLY_FT4 hp_max[10];  /* 0x870 */
    POLY_FT4 ep[6];       /* 0xA00 */
    POLY_FT4 ep_max[6];   /* 0xAF0 */
    u8 level_count;       /* 0xBE0 */
    u8 next_count;        /* 0xBE1 */
    u8 hp_count;          /* 0xBE2 */
    u8 hp_max_count;      /* 0xBE3 */
    u8 ep_count;          /* 0xBE4 */
    u8 ep_max_count;      /* 0xBE5 */
    u8 buffer;            /* 0xBE6: buffer it was built for */
    u8 shown;             /* 0xBE7 */
    u8 layout_count;      /* 0xBE8 */
    u8 extra_count;       /* 0xBE9 */
    u8 pad_BEA[2];
} StatusPanel;



extern void func_80039DB8(s32 sound);          /* play a sound */
extern s32 D_80059488;                         /* vsync count */
extern s32 func_8002675C(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y,
                         s32 scale);
extern s32 func_800263E4(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y,
                          s32 scale, s32 a, s32 b);
extern void *func_80033728(void *table, s32 index);      /* message address */
extern u8 func_80034EAC(void *text, u8 *image, s32 a, s32 b); /* render text */
extern void func_80033698(s32 a, s32 b);

/* Resource loading and panel drawing. */
void func_801C5390(void);
void func_801C5724(void);
void func_801C5BEC(void);
void func_801C9098(void);

#endif
