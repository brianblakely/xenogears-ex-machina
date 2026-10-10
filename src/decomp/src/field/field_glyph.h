#ifndef FIELD_FIELD_GLYPH_H
#define FIELD_FIELD_GLYPH_H

/* Text lines of file 0xab drawn as 9x16 8-bit glyph cells into VRAM:
 * two-byte codes 8540..887f come from the font image at (380, 100), seven
 * glyphs per row; the rest from the ROM kanji font. */

#include "common.h"
#include "psyq/libgpu.h"

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

/* The text sequence (files 0xab and 0xac) that a movie shows over its
 * frames 687..18e2 (800a7948). */
void field_staff_roll_start(void);        /* start it when enabled */
void field_staff_roll_advance(void);      /* advance it one pass */
void field_staff_roll_draw(void);         /* draw the text roll */
void field_staff_roll_upload_font(void);  /* upload file 0xac's image */
void field_staff_roll_release(void);      /* release its buffers when enabled */

#endif
