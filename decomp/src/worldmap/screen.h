#ifndef WORLDMAP_SCREEN_H
#define WORLDMAP_SCREEN_H

/* What the world map draws over the scene (worldmap_80090A84): the
 * full-screen fade every mode starts as an actor, the open map's path and
 * destination name windows with the destination marker, and the palette
 * fades of the area image and the terrain. */

#include "worldmap.h"
#include "resident/window.h"

/* The screen fade: its quad per display buffer and blend page; its rate and
 * brightness step are D_8009CCA4 and D_8009D3CC. */
extern POLY_G4 D_8009CE6C[2];
extern DR_TPAGE D_8009D310;

s32 func_800923A8(s32 index); /* start: the fade quads */
s32 func_800925A0(s32 index); /* update: commands 12 fade in, 13 fade out */

/* The name windows: the current path's and the destination's, the
 * destination marker (per display buffer), and the path and destination
 * whose names show (-1 none; the area's name table, D_8009D784). */
extern Window D_8009D498;
extern Window D_8009BD64;
extern POLY_FT4 D_8009D2B8[2];
extern s16 D_8009BD24; /* path */
extern s16 D_8009CE68; /* destination */

s32 func_80092BE4(void);      /* open the path name window */
s32 func_80092C70(s32 index); /* show the path's name */
void func_80092DD0(void);     /* close it */
s32 func_80092DF8(void);      /* open the destination window */
s32 func_80092FD8(s32 index); /* show the destination's name and marker */
void func_800931B0(void);     /* close it */

/* Resident text calls the world map declares itself: its calls pass words
 * where the resident's definitions take halfwords, and it reads the text's
 * address as a word. */
void window_open(Window *window, s32 vram_x, s32 vram_y, s32 x, s32 y, s32 columns, s32 rows);
s32 text_get_resource_entry(void *table, s32 id); /* text by id */

void func_800931D8(u16 *clut, u16 *out, s32 steps, u8 *colour); /* fade a palette */

#endif
