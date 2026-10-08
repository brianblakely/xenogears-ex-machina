/* The overlay's number, which opens the image ahead of the movie unit's
 * rodata: the five mode overlays at 0x8006faf0 start with theirs (field 4,
 * worldmap 5, battle 6, menu 7, movie 8). A unit of its own, as the movie
 * unit's rodata starts at 4 mod 8 (its jump tables' phase). */
#include "common.h"

const s32 D_8006FAF0 = 8;
