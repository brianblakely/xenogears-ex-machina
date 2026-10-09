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
void func_8002C6E0(s32 r, s32 g, s32 b); /* the back colour */
/* The resident takes a ModelBuffer and u8 ** (the world map passes four words). */
void func_8002CB54(SpriteModel *model, void **first, void **second);
/* The resident takes u16 x, y. */
void func_8002CC74(s32 x, s32 y);
/* The resident takes s16 modes and u16 second coordinates. */
void func_8002DDE4(void *target, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);
/* The resident takes a u16 index and a ModelLight. */
void func_80030A30(s32 index, struct Light *light);
/* The resident takes an s16 kind. */
void func_800324B8(s32 tag); /* heap allocation tag */
/* The resident takes s16 and u16 coordinates and sizes. */
void func_80032F54(Window *window, s32 x, s32 y, s32 w, s32 h, s32 a5, s32 a6);
/* The resident takes and returns u8 pointers. */
s32 func_80033728(s32 table, s32 index); /* text string of an index */
/* The resident returns a u8. */
s32 func_80033CD0(Window *window); /* chosen answer, 0 while open */
/* The resident takes u8 r, g, b. */
void func_80034800(Window *window, s32 colour, s32 a2, s32 a3);
/* The resident takes a u8 value. */
void func_80034874(Window *window, s32 cursor);
/* The resident takes an s16 width. */
s32 func_80034EAC(s32 string, u8 *image, s32 colour, s32 arg); /* returns width */
/* The resident takes s16 frames. */
void func_80036258(s32 port, s32 arg);
/* The resident takes word volume and pan; the menu narrows them to s16. */
void func_80039F9C(s32 sound, s32 voice, s16 volume, s16 pan); /* key on */

/* The resident's option bytes of the field and the menu (its u8[6] at
 * 8005061c) and the byte after them, each by its own name: indexed from the
 * array, func_800852C4 keeps the array's address in a register where the
 * original loads each byte absolutely. */
extern u8 D_8005061C; /* nonzero keeps the options in D_8006D634.options */
extern u8 D_8005061D; /* entry kind (0 bout, 1 bout mode 4, 2 scene) */
extern u8 D_8005061E; /* first actor's model id */
extern u8 D_8005061F; /* second actor's model id */
extern u8 D_80050620; /* option 6 */
extern u8 D_80050621; /* level */
extern u8 D_80050622; /* result of the last menu battle */

/* The resident's vertical blank count (main2.c counts it), volatile here:
 * the menu's frame loop reads it again at each use (80088e90). */
extern volatile s32 D_80059488;

/* The resident's model colour (main_8002C3E8.c's CVECTOR), which the map
 * drawing rewrites with its code byte as one word (8008779c). */
extern u32 D_80059598;

#endif
