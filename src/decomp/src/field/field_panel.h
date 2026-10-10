#ifndef FIELD_FIELD_PANEL_H
#define FIELD_FIELD_PANEL_H

/* The field status panel (800a8314-800a8eac, field_screen.c): 0x6d
 * textured quads per draw buffer (800afc60) placed by a layout table and
 * textured from a frame table in VRAM at (380, 0). */

#include "common.h"
#include "psyq/libgpu.h"

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

extern PanelFrame field_status_panel_texture_frames[];
extern PanelPiece field_status_panel_pieces[PANEL_PIECES];
extern s32 field_status_panel_draw_count;         /* frame counter */
extern s32 field_status_panel_blink_phase;        /* blinking frame 0..2 */
extern POLY_FT4 *field_status_panel_quads[2];     /* the quads per draw buffer */

void field_status_panel_build(void);      /* build the panel's quads */
void field_status_panel_draw(void);       /* draw the panel */
void field_status_panel_release(void);    /* release the quads once allocated */

#endif
