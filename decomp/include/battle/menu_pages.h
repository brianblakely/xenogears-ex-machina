#ifndef BATTLE_MENU_PAGES_H
#define BATTLE_MENU_PAGES_H

#include "common.h"

/* The command menu's list pages (item, technique and gear pages): the
 * command names' text images (80070E2C's unit, 80076EA4) and the item,
 * technique and combo list pages (8008CCCC's, 8008FE18-80092B74). */

/* A text image's pixels. */
typedef struct TextImage {
    u32 *pixels;
} TextImage;

extern TextImage battle_digit_text_images[10]; /* text images of battle messages 0-9: the decimal digits */
extern u32 *battle_blank_text_image;           /* blank text image */

extern u8 battle_combo_menu_glyph_ids[16];  /* combo page glyph ids */
extern s32 battle_combo_menu_glyph_x[16];   /* their x */
extern s32 battle_combo_menu_glyph_y[16];   /* their y */

extern u8 battle_combo_step_buttons[8];     /* the entered combo steps' buttons */

void battle_upload_command_name_images(void);                       /* render the command names' text images */
void battle_item_menu_build_page(u8 column, u8 row, u8 open);       /* build the item list page */
void battle_item_menu_build_row_glyphs(s32 y);
void battle_item_menu_show_cell(u8 column, u8 row, u8 other);
void battle_item_menu_point_list(s32 v);
void battle_art_menu_build_page(u8 member);
void battle_art_menu_build_row_glyphs(s32 y);
void battle_art_menu_show_entry(u8 member, u8 column, u8 row);
void battle_combo_menu_build_page(u8 member, u8 *steps, u8 *costs);
void battle_combo_menu_build_entry_display(u8 member, u8 ap);

#endif
