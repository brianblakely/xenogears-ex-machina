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
#include "screen.h"

#define SCRIPT_CMD_U16(cmd, offset) (*(u16 *)((cmd) + (offset)))

/* The vertex (or normal) a command's index at offset names. */
#define SCRIPT_CMD_VERTEX(list, cmd, offset) ((SVECTOR *)(SCRIPT_CMD_U16(cmd, offset) * 8 + (s32)(list)))

/* RotAverageNclip4: the quad's screen points, its depth and flag; its
 * winding (> 0 facing). */
s32 func_8004A83C(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2,
                  long *sxy3, long *p, long *otz, long *flag);
void func_800B1F0C(u32 *ot);

#endif
