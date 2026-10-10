#ifndef MENU_GLOW_H
#define MENU_GLOW_H

#include "common.h"

/* The glow field (arena_scene_graph_and_opponent 8008DF30-8008E620): a field of heat seeded at its
 * two bottom rows, where each cell takes the cooled average of its
 * neighbours below every step (a fire), drawn over the screen through a
 * 256-colour palette; and the full-screen shade tile. */

extern u16 arena_glow_palette[]; /* glow palette (256 entries) */

void arena_glow_forget_buffers(void);
void arena_glow_init(void);
void arena_glow_free_buffers(void);
void arena_glow_step(void);
void arena_glow_draw_shade_tile(u32 *ot, s32 level, s32 subtract);
void arena_glow_draw(u32 *ot, s32 level, s32 brighten);

#endif
