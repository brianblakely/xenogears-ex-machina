#ifndef BATTLE_EFFECT_SCRIPT_H
#define BATTLE_EFFECT_SCRIPT_H

/* An effect script entry's primitive commands (800B1720-800B2AEC). Each
 * command starts with a 4-byte header: the words of the GPU primitive it
 * builds less one, its own words less one, flags (bit 0 lit) and the GPU
 * code. Then, by primitive:
 *   flat (F3/F4):      0x04 colour
 *   gouraud (G3/G4):   0x04, 0x08, 0x0C (, 0x10) colours
 *   textured (FT3/FT4, GT3/GT4): 0x04 uv0, 0x06 clut, 0x08 uv1, 0x0A tpage,
 *                      0x0C uv2 (, 0x10 uv3); lit: FT3 0x10, FT4 0x14 colour,
 *                      GT3 0x10.., GT4 0x14.. colours 4 bytes apart. */

#include "common.h"
#include "psyq.h"
#include "objects.h"

#define SCRIPT_CMD_U16(cmd, offset) (*(u16 *)((cmd) + (offset)))

#endif
