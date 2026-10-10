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

/* What the screen draws, and the party being edited (MenuState flags): a
 * flag per block or label list (the draw routines test them). */
typedef struct MenuFlags {
    u8 field_blocks_shown[3]; /* 0x00 */
    u8 cursor_shown;          /* 0x03: the highlight box */
    u8 sprite_shown;          /* 0x04: the highlight sprite */
    u8 field_menu_shown;      /* 0x05 */
    u8 field_menu2_shown;     /* 0x06 */
    u8 detail_shown;          /* 0x07 */
    u8 equipment_shown;       /* 0x08 */
    u8 images_shown;          /* 0x09 */
    u8 lists_shown;           /* 0x0a */
    u8 file_info_shown;       /* 0x0b */
    u8 list_labels_shown[8];  /* 0x0c */
    u8 row_labels_shown[6];   /* 0x14 */
    u8 extra_labels_shown[6]; /* 0x1a */
    u8 panels_shown[7];       /* 0x20 */
    u8 panels_growing[7];     /* 0x27 */
    u8 messages_shown;        /* 0x2e: the message labels */
    u8 markers_shown;         /* 0x2f */
    u8 party[3];              /* 0x30: the party being edited, 0xff empty */
    u8 card_message_shown;    /* 0x33 */
    u8 labels_shown[4];       /* 0x34 */
    u8 labels10e0_shown[8];   /* 0x38 */
    u8 labels14e0_shown[6];   /* 0x40 */
    u8 party_panels_shown;    /* 0x46 */
    u8 name_entry_shown;      /* 0x47 */
    u8 item_list_shown;       /* 0x48 */
    u8 scroll_shown;          /* 0x49 */
    u8 arts_list_shown;       /* 0x4a */
    u8 equip_labels_shown;    /* 0x4b */
    u8 equip_list_shown;      /* 0x4c */
    u8 status_list_shown;     /* 0x4d */
    u8 label17e0_shown;       /* 0x4e */
    u8 unknown4f;
    u8 cursors_shown[3];      /* 0x50: the list cursors, then the card access
                               * indicator (1 shown, 2 closing) */
    u8 marks_shown;           /* 0x53 */
    u8 labels18e0_shown[6];   /* 0x54 */
    u8 unknown5a[2];
    u8 sound_labels_shown[4]; /* 0x5c */
    u8 ready[3];              /* 0x60: per party slot */
    u8 model_shown;           /* 0x63: ovl2602 */
    u8 price_shown;           /* 0x64: ovl2602 */
    u8 gear_shown;            /* 0x65: ovl2602 */
    u8 gear_parts_shown;      /* 0x66: ovl2602 */
    u8 unknown67;
    u8 card_mode;             /* 0x68: the file screen is in card mode */
    u8 pad69[3];
} MenuFlags;

/* The highlight and fade primitives (MenuState prims). */
typedef struct MenuPrims {
    POLY_FT4 sprite[2];     /* 0x000: the highlight sprite */
    POLY_G4 shade[2];       /* 0x050: the highlight box */
    POLY_F4 fade[2];        /* 0x098: a full-screen fade */
    LINE_F3 upper[2];       /* 0x0c8: the box's outline: top and right */
    LINE_F3 lower[2];       /* 0x0f8: left and bottom */
    DR_MODE mode_label[2];  /* 0x128: the label texture page (0x140, 0x80) */
    DR_MODE mode_sprite[2]; /* 0x140: subtractive, the sprite page (0x180, 0) */
    u8 sprite_buffer;       /* 0x158 */
    u8 shade_buffer;        /* 0x159 */
    u8 pad15a;
    u8 width;               /* 0x15b: the box's width */
} MenuPrims;

/* The screen images (MenuState images): two packet groups, and the screen
 * area copied into the other buffer each frame. */
typedef struct MenuImages {
    POLY_FT4 packets[56];  /* 0x000 */
    POLY_FT4 packets2[56]; /* 0x8c0 */
    RECT screen;           /* 0x1180 */
    s32 count;             /* 0x1188 */
    s32 count2;            /* 0x118c */
    u8 buffer;             /* 0x1190 */
    u8 buffer2;            /* 0x1191 */
    u8 dim;                /* 0x1192: dimming requested (inside a command) */
    u8 dimmed;             /* 0x1193: dimming applied */
} MenuImages;

/* Two sprite lists (MenuState lists). */
typedef struct MenuSpriteLists {
    POLY_FT4 first[32];  /* 0x000 */
    POLY_FT4 second[96]; /* 0x500 */
    s32 first_count;     /* 0x1400 */
    s32 second_count;    /* 0x1404 */
    u8 first_buffer;     /* 0x1408 */
    u8 second_buffer;    /* 0x1409 */
    u8 pad140a[2];
} MenuSpriteLists;

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

/* The resident's vertical blank count (resident/pad.h says why it is declared
 * here): the screens keep it while they wait with the sound paused, and slot39
 * saves and restores it as the play time in frames. */
extern s32 pad_vblank_count;

LAYOUT_CHECK(MenuScreenSizes, sizeof(MenuFlags) == 0x6C && sizeof(MenuPrims) == 0x15C &&
                                  sizeof(MenuImages) == 0x1194 && sizeof(MenuSpriteLists) == 0x140C &&
                                  sizeof(MenuMarkers) == 0x14C && sizeof(MenuScrollBar) == 0x74 &&
                                  sizeof(MenuMarkerQuads) == 0x1C4 && sizeof(MenuCursor) == 0x78);

#endif
