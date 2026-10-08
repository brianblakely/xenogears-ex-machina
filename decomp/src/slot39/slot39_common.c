/* The menu overlay's common variables. They follow every unit's own
 * variables at the end of the image (zero there), so no unit's .data can
 * hold them; this data-only unit defines them, zero-initialized. */
#include "menu.h"

u8 D_801EA8FC = 0;          /* the last choice was cancelled */
s32 D_801EA900[2] = { 0 }; /* per port */
