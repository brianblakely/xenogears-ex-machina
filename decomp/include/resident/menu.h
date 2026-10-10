#ifndef RESIDENT_MENU_H
#define RESIDENT_MENU_H

#include "gpu.h"

/* Resident support for the menu mode (mode 5): its work block, the menu
 * state, is reached through *800625a0. The resident allocates and clears it
 * (8001c634) and runs one of the menu screens it loads at 801c5000 (the
 * overlays slot39, ovl2598, ovl2600, ovl2601 and ovl2602), which share its
 * layout; their blocks behind the pointers below are in decomp/include/menu
 * (the types every screen uses) or beside the screen that alone uses one. */

/* One display buffer: environments and a 16-entry reverse ordering table. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u_long ot[16];
    u32 *ot_big; /* +0xb0: the Gear screen's 0x400-entry ordering table (ovl2602) */
} MenuBuffer;

/* A point moving along a line in steps (slot39 801c81e0, 801c8324). */
typedef struct {
    s32 x0;
    s32 x1;
    s32 y0;
    s32 y1;
    s32 step_x; /* 8.8 per step */
    s32 step_y;
    s32 acc_x;  /* 8.8 distance moved */
    s32 acc_y;
    u8 neg_x;   /* moving towards smaller x */
    u8 neg_y;
    u8 speed;   /* steps per frame */
    u8 done;
} MenuMover;

/* The model view's light, handed with the model matrix to the actor module's
 * draw (ovl2143's 801E7D14, by ovl2602), which reads the first 0x20 bytes as
 * the light matrix. */
typedef struct {
    SVECTOR direction;   /* 0x00 */
    s16 unknown8[5];     /* 0x08 */
    u8 unknown12[0xE];
    MATRIX colour;       /* 0x20: light colour matrix */
} MenuLight;

/* The texture placement of a sprite sheet entry (80026338's outputs). */
typedef struct {
    s32 first;
    s32 mode;   /* texture mode for GetTPage */
    s32 clut_x; /* GetClut */
    s32 clut_y;
    s32 page_x; /* GetTPage */
    s32 page_y;
} MenuSheetEntry;

/* A laid-out text label: a textured quad per draw buffer showing a line of
 * text rendered into VRAM. */
typedef struct {
    POLY_FT4 polys[2]; /* 0x00: per draw buffer */
    SVECTOR verts[4];  /* 0x50: corners when drawn through the GTE */
    RECT rect;         /* 0x70: VRAM area of the rendered text */
    u8 *pixels;        /* 0x78: render buffer (only labels[0] owns one) */
    u8 highlight;      /* 0x7c: the highlighted CLUT (text_plane1_clut), else the plain one */
    u8 buffer;         /* 0x7d: the quad of the buffer it was built for */
    u8 width;          /* 0x7e: rendered text width */
    u8 projected;      /* 0x7f: drawn through the GTE */
} MenuLabel;

typedef struct MenuState {
    MenuMover movers[3];        /* +0x0 */
    MenuBuffer buffers[2];      /* +0x6c */
    MenuBuffer *current;        /* +0x1d4: the buffer being built */
    SVECTOR angles;             /* +0x1d8: the 3D view's rotation */
    VECTOR offset;              /* +0x1e0: and translation */
    MATRIX matrix;              /* +0x1f0: their matrix */
    u8 unknown210[8];
    SVECTOR angles2;            /* +0x218: the model view (ovl2602's Gear) */
    VECTOR offset2;             /* +0x220 */
    MATRIX matrix2;             /* +0x230 */
    u8 unknown250[0x48];
    MenuLight light;            /* +0x298 */
    s32 frame_counter;          /* +0x2d8 */
    void *sheet;                /* +0x2dc: the sprite sheet (80026338) */
    void *label_text;           /* +0x2e0: the label text offset table */
    struct SoundBank *effects;  /* +0x2e4: the menu's sound effect bank */
    s32 word2e8;                /* +0x2e8 */
    s32 time[7];                /* +0x2ec: play time digits: hours (three), minutes, seconds */
    s32 buffer_index;           /* +0x308: 0/1, the buffer being built */
    u8 present[16];             /* +0x30c: per character: in the party bits, may join */
    u8 digits[9];               /* +0x31c: decimal digits of a number, leading zeros 0xff */
    u8 input;                   /* +0x325: the frame's decoded input */
    u8 card_poll_timer;         /* +0x326 */
    u8 drawing;                 /* +0x327: nonzero draws the screen each frame */
    u8 unknown328;
    u8 view_motion;             /* +0x329: 4/3 start zooming in/out, 2/1 zooming */
    u8 sounds;                  /* +0x32a: nonzero plays the menu's effects */
    u8 party_count;             /* +0x32b */
    struct MenuCard *card;      /* +0x32c: the memory card state */
    struct MenuTables *tables;  /* +0x330: the data table directory */
    u8 cards_present;           /* +0x334 */
    u8 unknown335;
    u8 cursor;                  /* +0x336: the top command cursor */
    u8 cursor_shown;            /* +0x337: the cursor the labels were last drawn for */
    u8 choice;                  /* +0x338: the list cursor */
    u8 choice_shown;            /* +0x339 */
    u8 choice_count;            /* +0x33a */
    u8 fighters;                /* +0x33b: party members with a gear */
    struct MenuFlags *flags;    /* +0x33c: what is shown, and the party */
    struct MenuFieldMenu *field_menu;   /* +0x340: slot39 */
    struct MenuFieldMenu2 *field_menu2; /* +0x344: slot39 */
    struct MenuPrims *prims;    /* +0x348: the highlight, fade and outline primitives */
    struct MenuFileInfo *file_info; /* +0x34c: slot39's save information views */
    struct MenuImages *images;  /* +0x350: the screen images */
    struct MenuSpriteLists *lists; /* +0x354: two sprite lists */
    struct MenuDetail *detail;  /* +0x358: slot39's detail panel */
    struct MenuEquipPanel *equip_panel; /* +0x35c: slot39's equipment panel */
    struct MenuEquipLabels *equip_labels; /* +0x360: slot39 */
    struct MenuPanel *panels[7];   /* +0x364: framed 3D panels */
    struct MenuGrowth *growth[7];  /* +0x380: their opening */
    struct MenuFieldBlock *field_blocks[3]; /* +0x39c: slot39 */
    struct MenuSlotImage *slots[32]; /* +0x3a8: slot39's file screen slots */
    struct MenuMarkers *markers;   /* +0x428 */
    struct MenuItemList *item_list; /* +0x42c: slot39 */
    struct MenuArtsList *arts_list; /* +0x430: slot39 */
    struct MenuEquipList *equip_list; /* +0x434: slot39 */
    struct MenuStatusList *status_list; /* +0x438: slot39 */
    struct MenuScrollBar *scroll;  /* +0x43c */
    struct MenuMarkerQuads *marks; /* +0x440 */
    struct MenuCursor *cursors[2]; /* +0x444: animated list cursors */
    struct MenuIndicator *indicator; /* +0x44c: slot39's card access indicator */
    struct ShopDetails *details;   /* +0x450: the shops' lists and prices */
    struct GearScreen *gear_screen; /* +0x454: ovl2602 */
    struct ModelParts *model_parts[2]; /* +0x458: ovl2602 */
    u8 unknown460[0xC];
    MenuSheetEntry sheet_entries[4]; /* +0x46c */
    s32 unknown4cc;
    s32 unknown4d0;
    s32 unknown4d4;
    u8 load_state;              /* +0x4d8 */
    u8 unknown4d9;
    u8 unknown4da[2];
    u8 first_member;            /* +0x4dc: the first occupied party slot */
    u8 unknown4dd[3];
    MenuLabel labels[4];        /* +0x4e0: the command labels; the first owns the pixel block */
    MenuLabel list_labels[8];   /* +0x6e0 */
    MenuLabel row_labels[6];    /* +0xae0 */
    MenuLabel extra_labels[6];  /* +0xde0 */
    MenuLabel labels10e0[8];    /* +0x10e0: slot39 */
    MenuLabel labels14e0[6];    /* +0x14e0: slot39 */
    MenuLabel labels17e0[2];    /* +0x17e0: slot39 */
    MenuLabel labels18e0[6];    /* +0x18e0: slot39 */
    MenuLabel sound_labels[4];  /* +0x1be0: slot39 */
    MenuLabel *message_labels[4]; /* +0x1de0 */
    struct MenuStatusPanel *member_panels[6]; /* +0x1df0: ovl2598 */
    struct MenuStatusPanel *party_panels[3];  /* +0x1e08 */
    u8 members[11];             /* +0x1e14: characters that may join, 0xff ends (ovl2598) */
    u8 unknown1e1f;
    struct NameEntry *name_entry; /* +0x1e20 */
    u8 *portrait_table;         /* +0x1e24: ovl2600 */
    u8 portraits[3];            /* +0x1e28: ovl2600 */
    u8 unknown1e2b;
    u8 *shop_tables;            /* +0x1e2c: per shop, three kinds of 30 ids (0x5c bytes) */
    u8 shop_items[0x30];        /* +0x1e30: the shop's item ids */
    u8 shop_kinds[0x30];        /* +0x1e60: their kind (0 weapon, 1 armour, 2 item) */
    u8 *gear_tables;            /* +0x1e90: ovl2602 */
    u8 debug_show;              /* +0x1e94 */
    u8 debug_value;             /* +0x1e95 */
} MenuState;

LAYOUT_CHECK(MenuStateLayout, sizeof(MenuBuffer) == 0xB4 && sizeof(MenuLabel) == 0x80 &&
                                  sizeof(MenuMover) == 0x24 && sizeof(MenuLight) == 0x40 &&
                                  OFFSET_OF(MenuState, frame_counter) == 0x2D8 &&
                                  OFFSET_OF(MenuState, card) == 0x32C &&
                                  OFFSET_OF(MenuState, panels) == 0x364 &&
                                  OFFSET_OF(MenuState, sheet_entries) == 0x46C &&
                                  OFFSET_OF(MenuState, labels) == 0x4E0 &&
                                  OFFSET_OF(MenuState, message_labels) == 0x1DE0 &&
                                  OFFSET_OF(MenuState, name_entry) == 0x1E20 &&
                                  OFFSET_OF(MenuState, debug_show) == 0x1E94 &&
                                  sizeof(MenuState) == 0x1E98);

extern MenuState *menu_state_current;

/* The menu's resource file (*menu_state_resource_file, file 1 of directory 16): packed
 * files by index, which the screens unpack (80032e88). */
typedef struct {
    s32 count;
    void *files[8];
} MenuResources;

extern u8 menu_state_debug_start;       /* debug start: choose the menu screen */
extern u8 menu_state_screen;       /* menu screen */
extern u8 menu_state_screen_parameter;       /* menu screen parameter */
extern char *menu_state_screen_names[7]; /* menu screen names */
extern void *menu_state_resource_file;    /* the menu's resource file (MenuResources) */
extern void *menu_state_debug_heap_marker;
extern void *menu_state_debug_heap_reservation;
extern u32 *menu_state_big_ots[2]; /* the menu's large ordering tables, one per draw buffer */

/* Menu overlay (801c5000) entries. */
void menu_main(void);
void member_change_main(void);
void name_entry_main(void);
void item_shop_main(void);
void gear_shop_main(void);

void menu_state_init_buffer(MenuBuffer *buffer);
void menu_state_init_display(void);
void menu_state_reset_views(void);
void menu_state_run_screen(void);
void menu_state_decode_input(void);
void menu_state_update_frame(void);

#endif
