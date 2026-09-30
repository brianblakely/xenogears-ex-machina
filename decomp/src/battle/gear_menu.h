#ifndef BATTLE_GEAR_MENU_H
#define BATTLE_GEAR_MENU_H

#include "battle_core.h"

/* The gear command menu (8008cde4-8008d598). */
void func_800930AC(u8 member, u8 *ids, u16 *costs); /* build its list */
void func_800939CC(u8 member, u8 kind);

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

extern GlyphPage *D_800C3000[]; /* per command panel page */
extern GlyphEntry *D_800C2F4C[]; /* glyph sets */

#endif
