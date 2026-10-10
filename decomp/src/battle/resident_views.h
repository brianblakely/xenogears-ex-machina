#ifndef BATTLE_RESIDENT_VIEWS_H
#define BATTLE_RESIDENT_VIEWS_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/sprite.h"

/* The battle's own declarations of resident functions and data. Callers
 * convert arguments/result differently from the resident definition: narrow
 * parameters or results where the resident has words (or the reverse), other
 * parameter types, structures passed by value as other types; the resident
 * keeps its copies in its own_declarations.h, and the shared headers in
 * decomp/include/resident leave these out. The data are resident variables no
 * shared header declares, typed as the battle reads them. */

/* The random byte in low..high (8001bd40). */
u8 mode_get_random_byte_in_range(u8 low, u8 high);

/* Sprites (8001fbe4-800242f4). */
void sprite_vm_run_generic_command(); /* run a sprite script command (the sprite VM calls it unprototyped) */
u8 sprite_add_clamp_byte(u8 value, s32 delta); /* add, clamped to 0-255 */
void sprite_set_svector(SVECTOR *v, s32 x, s32 y, s32 z);
s32 sprite_stack_pop_byte(); /* a u8, taken as int */
void sprite_set_idle_animation(Sprite *sprite, s32 mode); /* set the idle mode */
void sprite_set_direction(Sprite *sprite, s32 direction); /* turn a sprite to a direction */
/* Upload an image: resource, image, its place and its CLUT's place (four-byte
 * points by value), mode. */
void sprite_resolve_resource();
void sprite_set_facing(Sprite *sprite, s32 direction);
s16 sprite_get_ground_direction(GroundPoint to, GroundPoint from); /* the direction between points */
void sprite_construct_with_palette_bank(Sprite *sprite, s32 a, s16 b, s16 c, s32 d, s32 e, s32 f, s32 g);

/* Glyphs and images (80025fa8-80026dcc, 8002dde4). */
/* 80025fa8: the u16 coordinates, scale and angle passed as full words. */
s32 sprite_sheet_draw_rotated(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y,
                  s32 scaleX, s32 scaleY, s32 angle);
s32 sprite_sheet_draw_scaled_flip(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 w, s32 scale, s32 a, s32 b);
s32 sprite_sheet_draw_scaled(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y, s32 scale);
s32 sprite_sheet_fill_parts(void *font, s32 character, SpritePart *out, s16 x, s32 y); /* glyphs added */
void model_load_image_list(void *images, s16 on, s32 a, s32 b, s16 c, s32 d, s32 e); /* upload images */

/* Disc streams (80029eb0). */
void stream_start_image_load(s32 file, void *ring, s32 mode, s32 unused, s32 mode_1200, s32 base_x_1200, s32 base_y_1200, s32 mode_1201, s32 base_x_1201, s32 base_y_1201);

/* Models (8002cb54-8002cc74). */
void model_alloc_packet_buffers(SpriteModel *model, void **packets0, void **packets1); /* allocate packets */
void model_set_tpage_override(s16 x, s16 y);
void model_set_clut_override(s16 x, s16 y);

/* Text (80033728-80034eac). */
void *text_get_resource_entry(void *table, s32 index);
void *text_get_system_resource_entry(u8 character, u8 id); /* a character text */
u8 *text_get_item_name(s32 id); /* item name */
u8 *text_get_weapon_name(s32 id); /* equipment name */
s32 window_render_text_line(void *text, u32 *pixels, s32 width, s32 mode);

/* Sound (800383ec-8003bdfc). */
s32 sound_find_wave_bank(u16 id);
s32 sound_find_effect_bank(SoundBank *bank, s32 mode); /* whether a sound bank is loaded */
s32 sound_create_and_play_seq(u8 *a, s32 b, s32 c); /* start the battle music */
void sound_play_effect_on_last_channels(s32 effect);
void sound_set_effect_volume(s32 sound, s32 volume); /* set its volume */
s16 sound_sync_transfer(s32 wait); /* sound transfer busy */

/* Resident data no shared header declares. */
extern u8 model_slot_ring_tmd[];                 /* a TMD model (an effect script file, battle/effect_script.h) */
extern u8 sprite_vm_command_lengths_by_opcode[]; /* the byte widths of sprite VM commands 80-FF */
extern s32 pad_vblank_count;                     /* the vertical blank count */
extern s16 mode_battle_ai_variables[];           /* the AI scripts' shared variables */

#endif
