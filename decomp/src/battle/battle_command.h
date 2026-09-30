#ifndef BATTLE_COMMAND_H
#define BATTLE_COMMAND_H

/* Battle command menu: window confirms, target selection and turn flow
 * (80080160-80086f98). */

#include "battle_core.h"

u8 func_8008C4A8(u8 member); /* the member has a special available */
u8 func_8008CFB8(u8 member); /* the gear has a part list */
u8 func_80084548(u8 side, u8 any, u8 partyFirst);
u8 func_80084750(u8 member);
u16 func_80084DE4(u16 selection, u16 fallback, u8 member, u8 mode, u8 own);

extern u8 D_800C2050;       /* healing ignores the gear */
extern u8 *D_800C3160[13]; /* combo input patterns (seven inputs each) */
extern u8 *D_800C31AC[];   /* per character: the deathblow of each combo */

/* A slot's ground position, read unsigned. */
#define SLOT_X(slot) ((u16)D_800C3EB4[slot].x)
#define SLOT_Z(slot) ((u16)D_800C3EB4[slot].z)
/* The square of a difference, taken of its magnitude. */
#define SQUARE(x) ((x) < 0 ? (-(x)) * (-(x)) : (x) * (x))

#endif
