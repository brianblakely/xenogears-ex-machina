#ifndef MENU_PANEL_H
#define MENU_PANEL_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The menu screens' framed panels (mode 5; see menu/screen.h). */

/* A framed 3D panel (MenuState panels[]): corner and edge sprites around a
 * translucent fill and an optional scroll bar, a quad per piece and draw
 * buffer, each piece with the corners it is projected from. */
typedef struct MenuPanel {
    POLY_FT4 corner[8];       /* 0x000: two per corner part */
    POLY_FT4 edge[4][4];      /* 0x140: top, bottom, left and right, two pieces each */
    POLY_FT4 bar_side[2];     /* 0x3c0: the scroll bar, sprite 0x106 */
    POLY_FT4 bar_ends[4];     /* 0x410: its ends, sprite 0x105 at the top and flipped below */
    POLY_G4 fill[2];          /* 0x4b0 */
    DR_MODE fill_mode[2];     /* 0x4f8 */
    SVECTOR corner_at[16];    /* 0x510: four corners per corner part */
    SVECTOR edge_at[4][2][4]; /* 0x590: per edge, two pieces */
    SVECTOR fill_at[4];       /* 0x690 */
    SVECTOR side_at[4];       /* 0x6b0 */
    SVECTOR ends_at[8];       /* 0x6d0: the top and bottom pieces */
    s32 corner_parts;         /* 0x710: corner parts built */
    s32 flat;                 /* 0x714: nonzero: drawn with the current matrices;
                               * 0: under its own identity rotation */
    s32 ot_entry;             /* 0x718: the ordering table entry it is drawn into */
    u8 buffer;                /* 0x71c: the buffer it was laid out for */
    u8 has_bar;               /* 0x71d: with the scroll bar */
    u8 pad71e[2];
} MenuPanel;

/* A panel opening from its centre (MenuState growth[]). */
typedef struct MenuGrowth {
    u16 x, y, w, h;   /* the final rectangle */
    u16 cur_w, cur_h; /* 0x08: the current size */
    s32 ot_entry;     /* 0x0c: the ordering table entry it is drawn into */
    u8 index;         /* 0x10: its panel */
    u8 done;          /* 0x11: fully open */
    u8 flat;          /* 0x12: drawn with the current matrices, not its own */
    u8 has_bar;       /* 0x13: with the scroll bar */
    u8 pad14[4];
} MenuGrowth;

/* A character's status panel (MenuState member_panels[], party_panels[]):
 * its layout, portrait and name sprites and the digits of its levels, HP
 * and EP, two quads per sprite. */
typedef struct MenuStatusPanel {
    POLY_FT4 layout[18];  /* 0x000 */
    POLY_FT4 extra[10];   /* 0x2d0 */
    POLY_FT4 face[2];     /* 0x460: the portrait */
    POLY_FT4 label[2];    /* 0x4b0: the name */
    POLY_FT4 level[6];    /* 0x500: digits */
    POLY_FT4 level2[6];   /* 0x5f0 */
    POLY_FT4 hp[10];      /* 0x6e0 */
    POLY_FT4 hp_max[10];  /* 0x870 */
    POLY_FT4 ep[6];       /* 0xa00 */
    POLY_FT4 ep_max[6];   /* 0xaf0 */
    u8 level_count;       /* 0xbe0 */
    u8 level2_count;      /* 0xbe1 */
    u8 hp_count;          /* 0xbe2 */
    u8 hp_max_count;      /* 0xbe3 */
    u8 ep_count;          /* 0xbe4 */
    u8 ep_max_count;      /* 0xbe5 */
    u8 buffer;            /* 0xbe6: the buffer it was built for */
    u8 shown;             /* 0xbe7 */
    u8 layout_count;      /* 0xbe8 */
    u8 extra_count;       /* 0xbe9 */
    u8 padbea[2];
} MenuStatusPanel;

LAYOUT_CHECK(MenuPanelSizes, sizeof(MenuPanel) == 0x720 && sizeof(MenuGrowth) == 0x18 &&
                                 sizeof(MenuStatusPanel) == 0xBEC);

#endif
