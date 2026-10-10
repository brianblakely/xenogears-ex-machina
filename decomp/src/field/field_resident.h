#ifndef FIELD_FIELD_RESIDENT_H
#define FIELD_FIELD_RESIDENT_H

/* Resident calls and variables the field declares itself.
 *
 * The functions' field callers convert arguments/results differently from
 * the resident definition (narrow parameters where the resident has words,
 * or the reverse; another result), so the shared headers leave them out and
 * decomp/src/resident/own_declarations.h holds the resident's own copies.
 * The variables are resident objects the shared headers do not declare,
 * with the field's views of them. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "resident/window.h"

/* sprite.h: callers convert arguments/result differently from the resident definition. */
void sprite_set_part_color(void *sprite, s32 red, s32 green, s32 blue);                                    /* colour the one-sided parts */
void sprite_set_gravity_divisor(Sprite *sprite, u16 divisor);                                        /* the gravity divisor */
void sprite_set_direction(Sprite *sprite, s32 direction);                                            /* turn a sprite */
void sprite_set_facing(Sprite *sprite, s16 angle);                                                 /* face a sprite at `angle` */
void sprite_upload_images_side_by_side(void *tim, s32 x, s32 y);                                  /* upload an image list */
Sprite *sprite_create(void *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s32 unused);                             /* build a sprite */
Sprite *sprite_create_with_palette_bank(void *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s32 unused, s32 extra); /* with a bank */

/* gpu.h and cd.h: callers convert arguments/result differently from the resident definition. */
Panorama *gpu_create_panorama(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y, s32 mode,
                        s32 turn, s32 *position, u8 *colours, s32 fill_scale, s32 fade_range,
                        s32 fade_start); /* create the panorama */
void gpu_init_texture_scroll(TextureScroll *scroll, s16 x, s16 y, s16 w, s16 h, s16 count, s16 source_x, s16 source_y,
                   u8 *speeds); /* set a texture scroll up */
void stream_start_image_load(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32); /* start a stream */

/* model.h: callers convert arguments/result differently from the resident definition. */
void model_set_color(s32 r, s32 g, s32 b);                                        /* the model (fog) colour */
void model_alloc_packet_buffers(SpriteModel *buffer, void **first, void **second); /* allocate its packets */
void model_set_light(s32 index, ModelLight *light);                               /* set a light */
void model_set_back_color_16bit(s32 r, s32 g, s32 b);                             /* the background colour */

/* heap.h, text.h and window.h: callers convert arguments/result differently from the resident definition. */
void heap_free_tag(s32 tag);                                /* release the blocks with `tag` */
void window_open(Window *window, s32 vram_x, s32 vram_y, s32 x, s32 y, s32 columns, s32 rows);
void text_load_palette(s32 x, s32 y);                       /* upload the text palette */
s32 text_get_resource_entry(void *messages, s32 index);   /* a message of a resource */
s32 text_get_message_columns(void *table, s32 index);     /* message columns */
s32 text_get_message_rows(void *table, s32 index);        /* message rows */
s32 window_get_wait_state(Window *window);                  /* chosen answer, 0 while open */
void window_set_color(Window *window, s32 r, s32 g, s32 b); /* colour the lines */
void window_highlight_line(Window *window, s32 value);
void pad_set_unread_byte(s32 value);

/* sound.h: callers convert arguments/result differently from the resident definition. */
void sound_play_effect_on_channel_volume_pan(s32 effect, s16 channel, s16 volume, s16 pan); /* play a sound effect */
void sound_sync_transfer(s32 wait);                                                   /* wait for the SPU transfer */

/* Resident objects the shared headers do not declare. */
extern s32 pad_vblank_count;             /* vertical blank count (text_windows_and_pads.c) */
extern CVECTOR model_color;              /* the model (fog) colour 8002c6e0 sets */
extern u8 mode_arena_task_parameters[6]; /* option bytes of the field and the menu */
extern u8 mode_arena_bout_outcome;       /* the arena bout's outcome (arena_fighters_bout_and_effects writes it) */
extern u8 mode_snapshot_block[];         /* the field snapshot (0x22fc bytes, 800a3f4c) */

#endif
