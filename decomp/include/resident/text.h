#ifndef RESIDENT_TEXT_H
#define RESIDENT_TEXT_H

#include "common.h"
#include "psyq/libgpu.h"
#include "heap.h"

/* Resident message text: the font ("MES FONT") and system data ("MES
 * SYSDATA") resources, and the conversion between text bytes and the
 * two-byte character codes the message system draws. */

/* The text bytes of a character code (first is 0 for one-byte text). */
typedef struct {
    u8 first;
    u8 second;
} CharPair;

extern u16 text_plane1_clut;            /* text CLUTs */
extern u16 text_plane0_clut;
extern u16 text_palette[];              /* text palette */
extern u16 text_special_glyph_rows[11]; /* special 0xFFFF glyph */

/* Packed data (0x80032e7c-0x80032f54); window setup follows. */
s32 text_get_lzss_unpacked_size(s32 *packed);
void *text_unpack_lzss_alloc(void *data, s32 mode); /* allocate (heap_alloc mode) and unpack */
void *text_unpack_lzss(void *source, void *destination); /* unpack; returns destination */

void text_install_font(u16 *font);
void text_install_system_data(u8 *data);
s32 text_find_char_code(u8 first, u8 second);
s32 text_get_glyph_width(u16 first, u16 second);
void text_draw_glyph(s32 first, u16 second, u16 *image, s16 stride, s32 plane);

/* More of the text services. */
u32 text_relocate_offset_table(void *data);
u8 *text_get_battle_message(s32 index);
u8 *text_get_character_art_name(s32 index);
u8 *text_get_gear_resource_entry(s32 table, s32 index);
u8 *text_get_gear_art_name(s32 index);
u8 *text_get_gear_fuel_art_name(s32 index);
s32 text_encode(u8 *text, u16 *codes);

#endif
