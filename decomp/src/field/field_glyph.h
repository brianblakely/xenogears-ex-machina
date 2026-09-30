#ifndef FIELD_FIELD_GLYPH_H
#define FIELD_FIELD_GLYPH_H

#include "field.h"

/* Text lines of file 0xab drawn as 9x16 8-bit glyph cells into VRAM:
 * two-byte codes 8540..887f come from the font image at (380, 100), seven
 * glyphs per row; the rest from the ROM kanji font. */

#define GLYPH_OWN_FIRST 0x8540
#define GLYPH_OWN_COUNT 0x340
#define GLYPH_LINE_CELLS 28
#define GLYPH_CELL_BYTES (16 * 18)

/* A glyph cell: 16 rows of 18 8-bit pixels. */
typedef struct {
    u8 pixels[16][18]; /* GLYPH_CELL_BYTES */
} GlyphCell;

u16 *func_800405C4(s32 code); /* resident: 16x15 ROM font glyph of a code */

#endif
