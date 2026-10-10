#ifndef OVL2598_PARTY_MENU_H
#define OVL2598_PARTY_MENU_H

#include "common.h"

/* The party screen's own declarations; its blocks are the menu screens'
 * (decomp/include/menu). */

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions
 * (decomp/src/resident/own_declarations.h). */
void sound_play_effect_on_last_channels(s32 effect); /* play a sound effect */
s32 sprite_sheet_draw_scaled(void *sheet, s32 id, void *prims, s32 index, s32 x, s32 y, s32 scale); /* a sprite */
s32 sprite_sheet_draw_scaled_flip(void *sheet, s32 id, void *prims, s32 index, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* a mirrored sprite */
void *text_get_resource_entry(void *resource, s32 index);                   /* message address */
u8 window_render_text_line(void *text, u8 *image, s32 width, s32 flags); /* render a text line; its width */

#endif
