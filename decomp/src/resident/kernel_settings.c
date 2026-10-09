/* Initialized small globals opening .sdata at _gp (0x80059170), ahead of the
 * sprite unit's own: the kernel menu's start parameters, the overlays'
 * battle-entry and battle-music flags and the pointer to the disc index's
 * first word. Every user loads and stores them absolutely: battle_mode and
 * main_8001B6C4, which reach small data of their own through $gp, and the
 * overlays (tools/data_users.py --range 80059170:80059184). GCC writes a -G8
 * unit's data, commons and .externs ahead of its code, so a one-pass ASPSX
 * would have seen a user's own definition first: they are no user's own.
 * The owner is inferred from the link position alone, an object linked
 * before sprite.o that never uses them. main.o, the only code object there,
 * is -G0 (main.c keeps its 4-byte initialized scalars, D_8004F2BC to
 * D_8004F380, in .data) and has no .sdata; the disc-index object (whose
 * first word D_8005917C points at) and the battle effect-script object
 * (8001C76C) sit there too. A data-only unit (GP 8 in the target fragment)
 * is the simplest such owner; whether D_80059180 ends it or opens sprite.o's
 * .sdata is undetermined. */
#include "common.h"
#include "resident/menu.h"
#include "resident/mode.h"

u8 D_80059170 = 0;  /* unreferenced; size from value and alignment */
u8 D_80059171 = 0;  /* menu screen parameter */
s16 D_80059172 = 0; /* unreferenced; size from value and alignment */
s32 D_80059174 = 1; /* unreferenced; size from value and alignment */
u8 D_80059178 = 1;  /* debug start: choose the menu screen */
u8 D_80059179 = 0;  /* battle-entry flag (field and world map set it, battle and slot39 read it) */
s32 *D_8005917C = &D_80010000;
u8 D_80059180 = 0; /* battle music playing (battle, ovl2596) */
