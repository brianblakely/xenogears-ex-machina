#ifndef WORLDMAP_SCREEN_H
#define WORLDMAP_SCREEN_H

/* What the world map draws over the scene (worldmap_steering_camera_terrain): the
 * full-screen fade every mode starts as an actor, the open map's path and
 * destination name windows with the destination marker, and the palette
 * fades of the area image and the terrain. */

#include "worldmap.h"
#include "resident/window.h"

/* The screen fade: its quad per display buffer and blend page; its rate and
 * brightness step are worldmap_screen_fade_rate and worldmap_screen_fade_step. */
extern POLY_G4 worldmap_screen_fade_quads[2];
extern DR_TPAGE worldmap_screen_fade_blend;

s32 worldmap_screen_fade_start(s32 index); /* start: the fade quads */
s32 worldmap_screen_fade_update(s32 index); /* update: commands 12 fade in, 13 fade out */

/* The name windows: the current path's and the destination's, the
 * destination marker (per display buffer), and the path and destination
 * whose names show (-1 none; the area's name table, worldmap_name_table). */
extern Window worldmap_path_window;
extern Window worldmap_destination_window;
extern POLY_FT4 worldmap_destination_marker[2];
extern s16 worldmap_path_name_id; /* path */
extern s16 worldmap_destination_name_id; /* destination */

s32 worldmap_path_window_open(void);      /* open the path name window */
s32 worldmap_path_window_update(s32 index); /* show the path's name */
void worldmap_path_window_close(void);     /* close it */
s32 worldmap_destination_window_open(void);      /* open the destination window */
s32 worldmap_destination_window_update(s32 index); /* show the destination's name and marker */
void worldmap_destination_window_close(void);     /* close it */

/* Resident text calls the world map declares itself: its calls pass words
 * where the resident's definitions take halfwords, and it reads the text's
 * address as a word. */
void window_open(Window *window, s32 vram_x, s32 vram_y, s32 x, s32 y, s32 columns, s32 rows);
s32 text_get_resource_entry(void *table, s32 id); /* text by id */

void worldmap_build_palette_fades(u16 *clut, u16 *out, s32 steps, u8 *colour); /* fade a palette */

#endif
