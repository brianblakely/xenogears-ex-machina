#ifndef MENU_MENUS_H
#define MENU_MENUS_H

#include "common.h"
#include "psyq/libgpu.h"

/* The menus (menu4 8007F834-800802A4, 800808F4-80081ECC): eight choice
 * menus on translucent panels, among them the settings, vibration and
 * options pages, whose draw callbacks show the values; the two captions,
 * the copy of the shown screen and the screen fades. */

/* A centred one-line caption: its text image and one sprite per buffer. */
typedef struct {
    u8 *image;         /* 0x00 */
    SPRT sprite[2];  /* 0x04 */
    s16 width;         /* 0x2C */
    s16 x;             /* 0x2E */
} Caption;

/* One line of a choice menu. */
typedef struct {
    u8 flags;          /* 0x00: 1 = runs every frame, 2 = pad to the widest
                        * line, 4 = skipped by the cursor */
    u8 caption;        /* 0x01: caption text while selected */
    u8 unk2[2];
    s32 text;          /* 0x04 */
    void (*handler)(); /* 0x08: run on confirm with arg (some take none) */
    s32 arg;           /* 0x0C */
    u8 unk10[2];
    s16 half_width;    /* 0x12 */
} MenuItem;

typedef struct Menu Menu;

/* A choice menu drawn on a translucent panel (eight in arena_menu_pages, 0x3C
 * bytes each); a page's draw callback adds its values column. */
struct Menu {
    u8 title_width;    /* 0x00: nonzero shifts the lines right */
    u8 unk1[3];
    MenuItem *items;   /* 0x04 */
    s16 count;         /* 0x08 */
    s16 parent;        /* 0x0A: menu index returned to on cancel */
    void (*draw)(Menu *menu); /* 0x0C */
    u8 unk10[2];
    s16 cursor;        /* 0x12 */
    s16 y;             /* 0x14 */
    s16 x;             /* 0x16 */
    u8 unk18[4];
    TILE panel[2];  /* 0x1C: one per buffer */
};

extern s32 arena_menu_copy_one_more_frame;
extern char *arena_menu_level_names[];
extern u8 arena_menu_frame_rates[];       /* frame rate of each rate setting */
extern s32 arena_menu_motion_speeds[];      /* value of each speed setting */
extern char *arena_menu_ai_command_names[];    /* names of the entries of setting 10 */
extern s32 arena_menu_applied_pad_port;        /* pad port of the menu input */
extern MenuItem arena_menu_port1_items[2]; /* vibration choices, port 1 */
extern MenuItem arena_menu_port2_items[2]; /* port 2 */
extern Menu arena_menu_pages[8];
extern u8 arena_menu_driving_pad_port;         /* pad port driving the menus */
extern s32 arena_menu_screen_done;

void arena_menu_hide_captions(void);
void arena_menu_close(void);
void arena_menu_highlight_entry(Menu *page, s32 entry);
char *arena_menu_get_level_name(void);
s32 arena_menu_step_setting(s32 value, s32 max, s32 flags);
void arena_menu_show_page(s32 page);
s32 arena_menu_is_title_shown(void);
void arena_menu_enter_title(void);
void arena_select_drop_portraits(void);
void arena_menu_free_screen_copy(s32 forget);
void arena_menu_blur_screen_copy(void);
void arena_menu_store_screen_copy(void);
void arena_menu_open_pause(s32 mode);
void arena_text_drop_quads(void);
void arena_menu_draw_overlay(void *ot);
void arena_menu_init_captions(void);
void arena_menu_show_caption(s32 text, s32 lower);
void arena_menu_draw_captions(void *ot);
void arena_menu_init_pages(void);
void arena_menu_update_port_pages(void);
void arena_menu_update(void);
void arena_menu_dim_background(void);
void arena_menu_reset_background(void);

#endif
