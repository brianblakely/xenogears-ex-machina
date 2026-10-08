/* The overlay's number (field 4, worldmap 5, battle 6, menu 7, movie 8), ahead
 * of the first unit's rodata, whose jump tables sit at 4 mod 8 after it
 * (docs/matching.md). */
#include "common.h"

const s32 D_8006FAF0 = 6;
