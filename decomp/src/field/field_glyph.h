#ifndef FIELD_FIELD_GLYPH_H
#define FIELD_FIELD_GLYPH_H

#include "field.h"
#include "psyq/libapi.h"

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

/* The text roll: 16 lines of four 128x16 8-bit sprites per draw buffer
 * showing the glyph rows at (300, 16 * line), scrolled up a pixel a frame
 * between a top and a bottom fade. */
typedef struct {
    DR_MODE modes[2][4]; /* 00 */
    SPRT sprites[2][4];  /* 60 */
} TextRollLine;

#endif
