#ifndef BATTLE_WINDOW_DRAW_H
#define BATTLE_WINDOW_DRAW_H

#include "battle_core.h"

/* Texture page coordinates of window texture `t` (8-bit mode: two texels
 * per VRAM halfword). */
#define WINDOW_TEX_U(t) (((u8)D_800C3EA4->textures[t].x & 0x3F) * 2)
#define WINDOW_TEX_V(t) (D_800C3EA4->textures[t].y)

#endif
