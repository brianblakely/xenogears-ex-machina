#ifndef OVL2598_PARTY_MENU_H
#define OVL2598_PARTY_MENU_H

#include "common.h"

/* The party screen's own declarations; its blocks are the menu screens'
 * (decomp/include/menu). */

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions
 * (decomp/src/resident/own_declarations.h). */
void func_80039DB8(s32 sound); /* play a sound effect */
s32 func_8002675C(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y, s32 scale); /* a sprite */
s32 func_800263E4(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* a mirrored sprite */
void *func_80033728(void *table, s32 index);                 /* message address */
u8 func_80034EAC(void *text, u8 *pixels, s32 width, s32 line); /* render a text line; its width */
void func_80033698(s32 x, s32 y);                            /* text palettes */

#endif
