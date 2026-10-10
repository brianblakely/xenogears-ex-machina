#ifndef MENU_RESIDENT_VIEWS_H
#define MENU_RESIDENT_VIEWS_H

#include "common.h"
#include "resident/model.h"
#include "resident/window.h"

/* The menu's own declarations of resident functions and variables, where
 * its code was built against other ones than the shared headers give
 * (decomp/include/resident; the resident's own are in
 * decomp/src/resident/own_declarations.h). */

struct Light;

/* Calls whose callers convert arguments/result differently from the
 * resident definition: the menu passes words where the resident takes
 * narrow parameters (or the reverse), as each comment says. */
/* The resident takes u8 r, g, b. */
void model_set_color(s32 r, s32 g, s32 b); /* the back colour */
/* The resident takes a ModelBuffer and u8 ** (the world map passes four words). */
void model_alloc_packet_buffers(SpriteModel *buffer, void **first, void **second);
/* The resident takes u16 x, y. */
void model_set_clut_override(s32 x, s32 y);
/* The resident takes s16 modes and u16 second coordinates. */
void model_load_image_list(void *images, s32 mode, s32 x, s32 y, s32 mode2, s32 x2, s32 y2);
/* The resident takes a u16 index and a ModelLight. */
void model_set_light(s32 index, struct Light *light);
/* The resident takes an s16 kind. */
void heap_set_next_class(s32 kind); /* heap allocation tag */
/* The resident takes s16 and u16 coordinates and sizes. */
void window_open(Window *window, s32 vram_x, s32 vram_y, s32 x, s32 y, s32 columns, s32 rows);
/* The resident takes and returns u8 pointers. */
s32 text_get_resource_entry(s32 resource, s32 index); /* text string of an index */
/* The resident returns a u8. */
s32 window_get_wait_state(Window *window); /* chosen answer, 0 while open */
/* The resident takes u8 r, g, b. */
void window_set_color(Window *window, s32 r, s32 g, s32 b);
/* The resident takes a u8 value. */
void window_highlight_line(Window *window, s32 value);
/* The resident takes an s16 width. */
s32 window_render_text_line(s32 text, u8 *image, s32 width, s32 flags); /* returns width */
/* The resident takes s16 frames. */
void pad_run_actuator(s32 port, s32 frames);
/* The resident takes word volume and pan; the menu narrows them to s16. */
void sound_play_effect_on_channel_volume_pan(s32 effect, s32 channel, s16 volume, s16 pan); /* key on */

/* The resident's option bytes of the field and the menu (its u8[6] at
 * 8005061c) and the byte after them, each by its own name: indexed from the
 * array, arena_mode_task keeps the array's address in a register where the
 * original loads each byte absolutely. */
extern u8 mode_arena_task_parameters; /* nonzero keeps the options in game_data.options */
extern u8 mode_arena_entry_kind; /* entry kind (0 bout, 1 bout mode 4, 2 scene) */
extern u8 mode_arena_first_model; /* first actor's model id */
extern u8 mode_arena_second_model; /* second actor's model id */
extern u8 mode_arena_option6; /* option 6 */
extern u8 mode_arena_level; /* level */
extern u8 mode_arena_bout_outcome; /* result of the last menu battle */

/* The resident's vertical blank count (text_windows_and_pads.c counts it), volatile here:
 * the menu's frame loop reads it again at each use (80088e90). */
extern volatile s32 pad_vblank_count;

/* The resident's model colour (model_renderer.c's CVECTOR), which the map
 * drawing rewrites with its code byte as one word (8008779c). */
extern u32 model_color;

#endif
