/* Initialized small globals ahead of the sprite unit's own, opening .sdata
 * at _gp (0x80059170): the kernel menu's start parameters and the pointer
 * to the disc index's first word. The units that use them (battle_mode,
 * main_8001B6C4) address them absolutely while they address small data of
 * their own through $gp, and GCC writes a unit's data ahead of its code, so
 * they are another unit's; no code unit's .sdata lies here, so a data-only
 * unit (GP 8 in the target fragment) defines them. */
#include "common.h"
#include "mode.h"
#include "menu.h"

u8 D_80059170 = 0;  /* unreferenced */
u8 D_80059171 = 0;  /* menu screen parameter */
s16 D_80059172 = 0; /* unreferenced */
s32 D_80059174 = 1; /* unreferenced */
u8 D_80059178 = 1;  /* debug start: choose the menu screen */
s32 *D_8005917C = &D_80010000;
s32 D_80059180 = 0; /* unreferenced */
