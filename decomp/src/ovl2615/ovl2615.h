#ifndef OVL2615_SETUP_PHASES_H
#define OVL2615_SETUP_PHASES_H

/* The battle setup's phase step (ovl2615.c), which the load modes
 * (load_modes.c, burst_modes.c) run between their frames: the prototype
 * keeps the callers' u8 conversion. */

#include "common.h"

void func_801E5840(u8 phase);

#endif
