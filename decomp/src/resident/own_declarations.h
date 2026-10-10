#ifndef RESIDENT_OWN_DECLARATIONS_H
#define RESIDENT_OWN_DECLARATIONS_H

/* The resident's declarations of its functions whose callers in other
 * targets were built against different ones: narrow parameters or results
 * where the resident has words (or the reverse), another parameter count
 * (8002cb54: the world map passes four), a by-value structure split
 * differently (80022224, 80023124). The shared headers in
 * decomp/include/resident leave these out, and each target declares them
 * for its own calls. They also leave out the overlay area mode_overlay_area
 * (8006faf0), which link.ld names: each mode overlay's first unit defines
 * that address as its number (a const s32). */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/window.h"

/* cd.h */
void cd_set_mode(u8 mode);

/* gpu.h */
Panorama *gpu_create_panorama(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y,
                        s32 mode, s32 turn, VECTOR *position, u8 *colours, u16 fill_scale,
                        u16 fade_range, u16 fade_start);

/* heap.h */
void heap_free_tag(u8 tag);
void heap_set_next_class(s16 kind);

/* mode.h */
extern u8 mode_overlay_area[]; /* 8006faf0: the overlay area the modes load at */

/* model.h */
void model_alloc_packet_buffers(ModelBuffer *buffer, u8 **first, u8 **second);
s32 model_load_image_list(s32 *images, s16 mode, s32 x, s32 y, s16 mode2, u16 x2, u16 y2); /* upload an image list */

/* sound.h */
SoundSequence *sound_find_wave_bank(s32 key); /* the loaded wave bank with `key` */
SoundSeq *sound_create_and_play_seq(SoundSeqHeader *header, s32 fade, s32 frames); /* start a sequence */
s32 sound_sync_transfer(s32 wait);

/* sprite.h */
void sprite_vm_run_generic_command(Sprite *sprite, u8 op, u8 *args); /* run a script command */
s32 sprite_add_clamp_byte(s32 value, s32 delta);
void sprite_set_svector(SVECTOR *vector, s16 x, s16 y, s16 z);
u8 sprite_stack_pop_byte(Sprite *sprite);
void sprite_set_direction(Sprite *sprite, s16 direction);
void sprite_resolve_resource(SpriteResource *resource, s32 *data, SVECTOR origin, s32 mode);
void sprite_set_facing(Sprite *sprite, s16 angle);
s32 sprite_get_ground_direction(DVECTOR from, DVECTOR to); /* the direction from `from` to `to` */
Sprite *sprite_create(s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused);

/* text.h */
u8 *text_get_resource_entry(u8 *resource, s32 index);

/* window.h */
void window_open(Window *window, s16 vram_x, s16 vram_y, s16 x, u16 y,
                   u16 columns, u16 rows); /* allocate and initialize */

#endif
