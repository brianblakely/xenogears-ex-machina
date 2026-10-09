#ifndef MENU_PANEL_H
#define MENU_PANEL_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The menu screens' framed panels (mode 5; see menu/screen.h). */

/* A panel opening from its centre (MenuState growth[]). */
typedef struct MenuGrowth {
    u16 x, y, w, h;   /* the final rectangle */
    u16 cur_w, cur_h; /* 0x08: the current size */
    s32 ot_entry;     /* 0x0c: the ordering table entry it is drawn into */
    u8 index;         /* 0x10: its panel */
    u8 done;          /* 0x11: fully open */
    u8 flat;          /* 0x12: drawn with the current matrices, not its own */
    u8 framed;        /* 0x13: with the frame (scroll bar) sprites */
    u8 pad14[4];
} MenuGrowth;

LAYOUT_CHECK(MenuPanelSizes, sizeof(MenuGrowth) == 0x18);

#endif
