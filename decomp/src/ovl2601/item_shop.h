#ifndef OVL2601_ITEM_SHOP_H
#define OVL2601_ITEM_SHOP_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"

/* The item shop (ovl2601): what its two units share, the menu screen code
 * (item_shop_framework.c) and the shop's own screens (shop.c). Its blocks are the menu
 * screens' (decomp/include/menu). */

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions
 * (decomp/src/resident/own_declarations.h). */
void sound_play_effect_on_last_channels(s32 effect);                                                   /* play a sound effect */
void text_load_palette(s32 x, s32 y);                                                                  /* text palettes */
u8 *text_get_resource_entry(void *table, s32 index);                                                   /* entry of a text table */
u8 *text_get_weapon_name(s32 index);                                                                      /* equipment name */
u8 *text_get_accessory_name(s32 index);                                                                   /* accessory name */
u8 *text_get_item_name(s32 index);                                                                        /* item name */
void text_decode_codes(u8 *codes, u8 *out, s32 count);                                                /* codes to text */
s32 window_render_text_line(u8 *text, void *pixels, s32 width, s32 flags);                              /* render a text line */
s32 sprite_sheet_draw_scaled(void *sheet, s32 id, void *packets, s32 index, s32 x, s32 y, s32 scale); /* sprite */
s32 sprite_sheet_draw_scaled_flip(void *sheet, s32 id, void *packets, s32 index, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */

/* The screen code's data and calls the shop's screens use (item_shop_framework.c). */
extern s32 item_shop_stock_count;            /* items the shop sells */
extern u8 item_shop_sell_label_ids[];        /* sell list label text ids */
extern u8 item_shop_buy_label_ids[];         /* buy list label text ids */
extern s32 item_shop_sell_label_x_offsets[]; /* sell list label x offsets */
extern s32 item_shop_portrait_x_table[];     /* member portrait x */
void item_shop_split_digits(u32 value);
void item_shop_label_init_quads(MenuLabel *label, s32 index, s32 row, s32 mode);
void item_shop_scroll_bar_hide(void);
void item_shop_list_cursor_free(u8 index);
void item_shop_panel_close(u8 index);
void item_shop_panel_open(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void item_shop_draw_projected_quads(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);
void item_shop_draw_quads(s32 count, POLY_FT4 *packets, s32 first);
void item_shop_play_sound(u8 sound);
void item_shop_run_frame(void);
void item_shop_view_start_zoom_in(void);
void item_shop_notice_open(u8 first);
void item_shop_notice_close(void);
void item_shop_label_render_or_clear_shown(u8 render, u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown);
void item_shop_label_place(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row,
                   u8 mode);
void item_shop_choice_window_open(u8 menu);
void item_shop_choice_window_set_cursor(u8 menu);

/* The shop's data and calls the screen code uses (shop.c). */
extern s32 item_shop_total_x; /* second number x */
extern s32 item_shop_total_y; /* second number y */
void item_shop_draw_details(void);
u8 item_shop_buy_command_run(void);
u8 item_shop_sell_command_run(void);
void item_shop_finish_top_command(void);

#endif
