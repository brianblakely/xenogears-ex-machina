/* The menu overlay's common (uninitialized global) variables. The original
 * linker allocated them after every unit's own variables, at the end of the
 * file (zero there), so this unit, linked last, defines them. */
#include "menu.h"

u8 D_801EA8FC;     /* the last choice was cancelled */
s32 D_801EA900[2]; /* per port */
