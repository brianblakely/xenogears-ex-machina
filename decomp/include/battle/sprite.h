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

/* The battle slot a sprite stands for: the top two bits of its frame bits
 * (frame_bits.unknown30) low, the two lowest of its motion word
 * (motion.bits.unknown0) high, read in that order. */
#define SPRITE_SLOT(sprite) ({ s32 low_ = (sprite)->frame_bits.unknown30; (sprite)->motion.bits.unknown0 << 2 | low_; })

/* A sprite's frame bits (resident frame_bits) read as one word: the battle
 * shifts the motion state (bits 28-29) out of it. */
#define SPRITE_FRAME_WORD(sprite) (*(u32 *)&(sprite)->frame_bits)

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

/* A point on the ground passed by value. */
typedef struct {
    s16 x;
    s16 z;
} GroundPoint;

#endif
