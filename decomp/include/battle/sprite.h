#ifndef BATTLE_SPRITE_H
#define BATTLE_SPRITE_H

#include "common.h"
#include "resident/sprite.h"

/* How the battle overlay, its modules (0x801fc000) and the event script
 * overlay read the resident sprites (resident/sprite.h's Sprite): views
 * the resident's own code does not use. */

/* The whole part of a 16.16 coordinate, loaded as its own halfword (the
 * resident shifts the word instead). */
#define FIXED_WHOLE(value) (((s16 *)&(value))[1])

/* The bounds of a sprite frame's drawn parts (the modules ovl3385 and
 * ovl3386 measure them with the same code, 801fc000). */
typedef struct {
    s16 y0, x0, y1, x1;
} SpriteBounds;

/* Where the gear objects' images go in VRAM: three 64x256 places (data of
 * the battle's 800B8098 unit). The module ovl3387 saves the last two and
 * puts them back around its effect. */
typedef struct {
    s16 x;
    s16 y;
} ImagePlace;

extern ImagePlace D_800C3668[3];

#endif
