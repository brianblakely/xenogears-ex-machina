#ifndef BATTLE_COMMAND_H
#define BATTLE_COMMAND_H

/* Battle command menu: window confirms, target selection and turn flow
 * (80080160-80086f98). */

#include "battle_core.h"

u8 func_8008C4A8(u8 member); /* the member has a special available */
u8 func_8008CFB8(u8 member); /* the gear has a part list */

/* A slot's ground position, read unsigned. */
#define SLOT_X(slot) ((u16)D_800C3EB4[slot].x)
#define SLOT_Z(slot) ((u16)D_800C3EB4[slot].z)
/* The square of a difference, taken of its magnitude. */
#define SQUARE(x) ((x) < 0 ? (-(x)) * (-(x)) : (x) * (x))

#endif
