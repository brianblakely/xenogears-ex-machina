#ifndef MENU_GLOW_H
#define MENU_GLOW_H

#include "common.h"

/* The glow field (menu7 8008DF30-8008E620): a field of heat seeded at its
 * two bottom rows, where each cell takes the cooled average of its
 * neighbours below every step (a fire), drawn over the screen through a
 * 256-colour palette; and the full-screen shade tile. */

extern u16 D_80091CE0[]; /* glow palette (256 entries) */

void func_8008DF30(void);
void func_8008DF50(void);
void func_8008E064(void);
void func_8008E120(void);
void func_8008E2B8(u32 *ot, s32 level, s32 subtract);
void func_8008E3CC(u32 *ot, s32 level, s32 brighten);

#endif
