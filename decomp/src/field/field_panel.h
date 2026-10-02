#ifndef FIELD_FIELD_PANEL_H
#define FIELD_FIELD_PANEL_H

#include "field.h"

/* The field status panel: 0x6d textured quads per draw buffer (800afc60)
 * placed by a layout table and textured from a frame table in VRAM at
 * (380, 0). */

/* A texture frame (800aeb68). */
typedef struct {
    u16 u;
    u16 v;
    u16 w;
    u16 h;
} PanelFrame;

/* One piece of the panel layout (800aef10). */
typedef struct {
    s16 x;
    s16 y;
    u16 frame;
    u16 flags; /* 0-3: flip (1 h, 2 v, 3 both), 4-7: tpage mode */
} PanelPiece;

#define PANEL_PIECES 0x6D

extern PanelFrame D_800AEB68[];
extern PanelPiece D_800AEF10[PANEL_PIECES];
extern s32 D_800AEB60; /* frame counter */
extern s32 D_800AEB64; /* blinking frame 0..2 */

#endif
