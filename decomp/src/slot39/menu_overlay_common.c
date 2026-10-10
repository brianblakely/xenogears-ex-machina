/* The menu overlay's common (uninitialized global) variables, 801EA8FC-801EA908.
 * The original linker allocated them after every unit's own variables, at
 * the end of the file (zero there), so this unit, linked last, defines them.
 * No header is included that declares them (file.h does): GCC emits these
 * tentative definitions in the order of their first declaration, which is
 * then this order, the address order. */
#include "common.h"

u8 menu_yes_no_cancelled;     /* 801EA8FC: the last choice was cancelled */
s32 menu_card_blocks_used[2]; /* 801EA900: per port: blocks the listed files use (15 fill a card) */
