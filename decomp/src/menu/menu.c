/* The overlay's number (menu 7), its first word: the mode overlays loaded
 * at 8006FAF0 open with theirs, ahead of their units' read-only data. A unit
 * of its own, as the next unit's rodata starts at 4 mod 8 (its jump tables'
 * phase). */
#include "common.h"

const s32 arena_overlay_number = 7; /* 8006FAF0 */
