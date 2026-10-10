#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"

/* The menu overlay's screen framework (slot39; the field menu, kind 0, the
 * title screen's file screen, kind 2, and the CD change, kind 6): the menu
 * loops and blocks, the frame and input, labels, sprites and quads, panels,
 * the command windows and the view, and the overlay data they share. The
 * memory card and file screen are in file.h, the field menu's character
 * screens in field.h. */

/* The sheet images of one command of a command window. */
typedef struct MenuCommandImages {
    s32 cursor; /* 0: cursor image (+d lit) */
    s32 label; /* 4 */
} MenuCommandImages;

/* The menu data archive (file 2 of directory 10h): packed files by index. */
typedef struct MenuDataArchive {
    s32 count; /* 0 */
    void *items; /* 4 */
    void *weapons; /* 8 */
    void *accessories; /* C */
    void *effects[11]; /* 10: per character */
    void *unk3C; /* 3C */
    void *unk40; /* 40 */
    void *engines; /* 44 */
    void *frames; /* 48 */
    void *parts; /* 4C */
    void *unk50; /* 50 */
    void *unk54; /* 54 */
    void *unk58; /* 58 */
    void *gears[20]; /* 5C: per gear */
    void *unkAC; /* AC */
    void *unkB0; /* B0 */
    void *padB4[8];
    void *unkD4[4]; /* D4 */
} MenuDataArchive;

extern s32 menu_choice_window_images[]; /* per command: four choices of cursor and label images */
/* Sheet positions (u / 4, v) of the name images 801e8da8 renders: 0-2 the
 * party slots' characters, 3-5 their gears, from 6 the file views. */
extern s32 menu_name_image_vram_x_table[19];
extern s32 menu_name_image_vram_y_table[19];
extern s32 menu_highlight_x_table[];        /* highlight positions: x */
extern s32 menu_highlight_y_table[];        /* y */
extern s32 menu_label_mode1_x_table[8];     /* label x (mode 1) */
extern u16 menu_label_mode1_y;              /* label y (mode 1) */
extern s32 menu_label_mode2_x_table[];      /* label x (modes 2, 5 from 8) */
extern s32 menu_label_mode2_y_table[2];     /* label y per row (mode 2) */
extern s32 menu_label_mode3_y_table[];      /* label y per row (mode 3) */
extern s32 menu_label_mode6_x_table[2];     /* label x (mode 6) */
extern s32 menu_label_mode6_y_table[];      /* label y (mode 6) */

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions (decomp/src/resident/own_declarations.h). */
void cd_set_mode(s32 mode);
void mode_get_random_byte_in_range(s32 low, s32 high);
void sprite_sheet_draw_scaled_flip(void *sheet, s32 id, void *dst, s32 index, s32 x, s32 y, s32 scale, s32 flip_x, s32 flip_y);
s32 sprite_sheet_draw_scaled(void *sheet, s32 id, void *dst, s32 index, s32 x, s32 y, s32 scale);
void text_load_palette(s32 x, s32 y);
void sound_play_effect_on_last_channels(s32 id, s32 sound);              /* play a sound effect */
u8 *text_get_accessory_name(u8 index);
u8 *text_get_weapon_name(u8 index);                                         /* weapon name */
u8 *text_get_gear_accessory_name(u8 index);                                 /* gear accessory name */
u8 *text_get_gear_part_name(u8 index);                                      /* gear part name */
u8 *text_get_resource_entry(u8 *resource, s32 index);                       /* message of a table */
u8 *text_get_item_name(u8 index);                                         /* item name text */
u8 window_render_text_line(u8 *text, void *pixels, s32 width, s32 flags); /* render a text line; its width */
s32 text_decode_codes(u8 *codes, u8 *out, s32 count);                   /* decode a name */

/* The framework's functions that another unit calls, or its own before
 * defining them. */
void menu_load_or_release_data_set(u8 code);
void menu_init_screen(void);
void menu_run_frame(void);
void menu_read_input(void);
void menu_split_play_time(u32 frames);
void menu_split_digits(u32 value);
void menu_init_gradient_quad(POLY_G4 *poly, u8 r, u8 g, u8 b);
void menu_set_rect_verts(SVECTOR *v, u16 x, u16 y, u16 w, u16 h);
void menu_play_sound(s32 sound);
u16 menu_test_bit_msb_first(u16 flags, u8 bit);
u16 menu_test_bit(u16 flags, u8 bit);
u32 menu_test_bit32(u32 flags, u8 bit);
void menu_draw_screen(void);
void menu_view_update(void);
void menu_view_start_zoom_in(void);
void menu_view_start_zoom_out(void);
void menu_highlight_place(s32 index, u8 outline);
void menu_scroll_bar_show(s32 x, s32 y, s32 h);
void menu_scroll_bar_hide(void);
void menu_member_marks_hide(void);
void menu_name_label_layout(MenuLabel *label, u8 slot, u8 gear, u8 mode);
void menu_panel_open(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void menu_panel_grow_opening(void);
void menu_panel_layout(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar);
void menu_panel_close(u8 slot);
void menu_money_window_layout_digits(s32 x, s32 y);
void menu_play_time_window_layout_digits(s32 x, s32 y);
void menu_panel_init(u8 index);
void menu_label_init_quads(MenuLabel *label, s32 index, s32 first, u8 mode);
void menu_label_render_pairs(MenuLabel *labels, u8 *layout, s32 first, s32 count);
void menu_label_render_table(u8 count, MenuLabel *labels, u8 *table, u8 *flags);
void menu_label_clear_shown(u8 count, u8 *flags);
void menu_label_place(u8 count, MenuLabel *labels, u8 *table, s32 *offsets, u8 *flags, u8 selected, u8 row,
                   u8 mode);
void menu_command_window_open(s32 count, MenuCommandImages *images);
void menu_choice_window_open(u8 offset);
void menu_command_window_set_cursor(u8 count, u8 cursor, MenuCommandImages *images);
void menu_choice_window_set_cursor(u8 offset);
void menu_name_image_render(u8 image, u8 row);
void menu_quad_set_blending(POLY_FT4 *poly, u8 mode);
void menu_panel_set_dimmed(u8 index, u8 mode);
void menu_quad_set_semi_transparent(POLY_FT4 *poly);
void menu_quad_place(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h);
void menu_quad_init(POLY_FT4 *poly);

#endif
