#ifndef BATTLE_GEAR_MENU_H
#define BATTLE_GEAR_MENU_H

#include "common.h"

/* The command panel of the command menus (8008CCCC's unit, 8008D598-
 * 8008F8F4, 800930AC-80093B08): its pages' glyph lists and glyph sets. */

/* A command panel page's two glyph lists: which of the two +0x641c lists
 * each fills (0xFF: none) and from which glyph set. */
typedef struct {
    u8 lists[2];
    u8 sets[2];
} GlyphPage;

/* A glyph of a set (8 bytes): its id (from 0x4000 a byte of the turn slot),
 * position and shade (from 0x2000 chosen by a slot item's availability). */
typedef struct {
    u16 id;    /* 0xFFFF ends the set */
    s16 x;
    s16 y;
    u16 shade;
} GlyphEntry;

extern GlyphPage *battle_command_panel_pages[]; /* per command panel page */
extern GlyphEntry *battle_command_panel_glyph_sets[]; /* glyph sets */

s32 battle_command_panel_build_page(u8 member, u8 page, u8 fade); /* build or fade the command panel, return its buffer */

#endif
