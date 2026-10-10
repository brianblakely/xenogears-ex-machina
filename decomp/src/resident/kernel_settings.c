/* Initialized small globals opening .sdata at _gp (0x80059170), ahead of the
 * sprite unit's own: the kernel menu's start parameters, the overlays'
 * battle-entry and battle-music flags and the pointer to the disc index's
 * first word. Every user loads and stores them absolutely: battle_mode and
 * mode_battle_and_menu, which reach small data of their own through $gp, and the
 * overlays (tools/data_users.py --range 80059170:80059184). GCC writes a -G8
 * unit's data, commons and .externs ahead of its code, so a one-pass ASPSX
 * would have seen a user's own definition first: they are no user's own.
 * The owner is inferred from the link position alone, an object linked
 * before sprite.o that never uses them. main.o, the only code object there,
 * is -G0 (main.c keeps its 4-byte initialized scalars, mode_fatal_error_count to
 * mode_debug_hide_layer, in .data) and has no .sdata; the disc-index object (whose
 * first word mode_disc_mode_pointer points at) and the battle effect-script object
 * (8001C76C) sit there too. A data-only unit (GP 8 in the target fragment)
 * is the simplest such owner; whether mode_result_fanfare_started ends it or opens sprite.o's
 * .sdata is undetermined. */
#include "common.h"
#include "resident/menu.h"
#include "resident/mode.h"

u8 mode_unreferenced_setting_byte = 0;  /* 80059170: unreferenced; size from value and alignment */
u8 menu_state_screen_parameter = 0;  /* 80059171: menu screen parameter */
s16 mode_unreferenced_setting_halfword = 0; /* 80059172: unreferenced; size from value and alignment */
s32 mode_unreferenced_setting_word = 1; /* 80059174: unreferenced; size from value and alignment */
u8 menu_state_debug_start = 1;  /* 80059178: debug start: choose the menu screen */
u8 mode_gear_riding_lock = 0;  /* 80059179: battle-entry flag (field and world map set it, battle and slot39 read it) */
s32 *mode_disc_mode_pointer = &mode_disc_mode; /* 8005917C */
u8 mode_result_fanfare_started = 0; /* 80059180: battle music playing (battle, ovl2596) */
