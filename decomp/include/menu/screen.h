#ifndef MENU_SCREEN_H
#define MENU_SCREEN_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* Blocks of the menu mode's screens (mode 5: the overlays the resident loads
 * at 801c5000, slot39 and ovl2598-ovl2602; not the mode 4 overlay in
 * decomp/src/menu), which each screen allocates behind the menu state's
 * pointers (resident/menu.h) from the same screen code. Primitives come in
 * pairs, one per draw buffer. */

/* The cursor and confirmation markers (MenuState markers): two quads per
 * marker. */
typedef struct MenuMarkers {
    POLY_FT4 polys[8];
    u8 shown[4];     /* 0x140 */
    u8 at_cursor[4]; /* 0x144: placed at the file cursor */
    u8 buffer[4];    /* 0x148: the buffer each was built for */
} MenuMarkers;

/* The scroll bar (MenuState scroll): one quad and its corners. */
typedef struct MenuScrollBar {
    POLY_FT4 polys[2];
    SVECTOR verts[4]; /* 0x50 */
    u8 buffer;        /* 0x70 */
    u8 pad71[3];
} MenuScrollBar;

/* Four quads drawn through the GTE (MenuState marks): ovl2602's second
 * markers, slot39's four-part sprite. */
typedef struct MenuMarkerQuads {
    POLY_FT4 polys[8];
    SVECTOR verts[16]; /* 0x140: four corners per quad */
    u8 buffer;         /* 0x1c0 */
    u8 pad1c1[3];
} MenuMarkerQuads;

/* An animated list cursor (MenuState cursors[]). */
typedef struct MenuCursor {
    POLY_FT4 polys[2];
    SVECTOR verts[4]; /* 0x50 */
    s32 frame;        /* 0x70: animation frame, counting down from 4 */
    u8 timer;         /* 0x74: frames the current one has been shown */
    u8 buffer;        /* 0x75 */
    u8 pad76[2];
} MenuCursor;

LAYOUT_CHECK(MenuScreenSizes, sizeof(MenuMarkers) == 0x14C && sizeof(MenuScrollBar) == 0x74 &&
                                  sizeof(MenuMarkerQuads) == 0x1C4 && sizeof(MenuCursor) == 0x78);

#endif
