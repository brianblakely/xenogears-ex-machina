/* The menu overlay's common (uninitialized global) variables, 801EA8FC-801EA908.
 * The original linker allocated them after every unit's own variables, at
 * the end of the file (zero there), so this unit, linked last, defines them.
 * No header is included that declares them (file.h does): GCC emits these
 * tentative definitions in the order of their first declaration, which is
 * then this order, the address order. */
#include "common.h"

u8 D_801EA8FC;     /* the last choice was cancelled */
s32 D_801EA900[2]; /* per port: blocks the listed files use (15 fill a card) */
