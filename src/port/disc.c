/* The game's disc check, for the drive's changer
 * (src/runtime/crates/xem-core/src/devices/cd.rs): when the game waits for
 * the lid to open on its other disc, the host learns which disc it waits
 * for and whether this is the story's disc change, which the changer takes
 * five seconds over. */
#include "common.h"
#include "xem/cd.h"

/* resident/menu.h's (its layout checks build only for the console's 32 bits,
 * not the port's native tests). */
extern u8 menu_state_screen;
s32 xem_original_menu_cd_check_disc(s32 disc);

/* 801E93A0 (slot39), wrapped: the change-disc notice shows and the check
 * waits for the lid to open on disc `disc` (menu_cd_ask_for_disc). Menu kind
 * 6 (menu_cd_change_run) is the story's disc change; the title screen's file
 * screen (kind 2) asks for the disc a new game or a loaded file needs. */
s32 menu_cd_check_disc(s32 disc) {
    xem_host_cd_wanted(disc, menu_state_screen == 6);
    return xem_original_menu_cd_check_disc(disc);
}
