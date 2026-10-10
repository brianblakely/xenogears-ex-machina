#ifndef MENU_TEXT_H
#define MENU_TEXT_H

#include "common.h"
#include "psyq/libgpu.h"
#include "mode.h"

/* The menu's text (menu4 8007E528-8007EEE8): the banner sprite, the font
 * and its text quads, the text cursor, scale and colour, and the message
 * table the captions and scenes draw from. */

/* Glyph of the menu font. */
typedef struct {
    u8 u, v;   /* texture position */
    u8 width;
    u8 height;
} Glyph;

/* A sprite with its texture page, one per draw buffer. */
typedef struct {
    DR_TPAGE tpage;
    SPRT sprite;
} SceneSprite;

extern Glyph arena_text_glyphs[];    /* menu font glyphs: digits, capitals, punctuation */
extern s32 arena_text_width_scale;   /* text width scale (0x100 = 1) */
extern s32 arena_text_message_table; /* the menu's message table (text_get_resource_entry) */

void arena_text_set_banner_timer(s32 state);
void arena_text_draw_banner(void *ot);
void arena_text_load_font_and_banner(MenuImageFile *files);
void arena_text_move_cursor(s32 x, s32 y);
Glyph *arena_text_find_glyph(s32 ch);
void arena_text_set_width_scale(s32 value);
s32 arena_text_draw_char(s32 ch);
s32 arena_text_measure_width(u8 *text);
void arena_text_draw_line(u8 *text);
void arena_text_draw_line_centered(u8 *text);
void arena_text_draw_line_right_aligned(u8 *text);
void arena_text_set_highlight(s32 highlight);
void arena_text_set_blue_highlight(s32 highlight);

#endif
