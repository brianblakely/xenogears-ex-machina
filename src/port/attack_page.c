/*
 * A host choice that changes what a battle shows (docs/runtime.md, The
 * game's interface context): after the attack page's first attack the
 * command panel fades out (battle_command_panel_fade, 8008D328) while the
 * attack sequence goes on. When the player began the sequence by pointing
 * at the attacks, the host keeps the panel shown until the sequence ends,
 * the attacks staying where they were pointed at. The fade only shades the
 * panel's quads (the menu's loop shows the panel again every frame), so
 * nothing else of the battle changes. The host records its choice with the
 * game's inputs; without it (0) the panel fades as the original's does.
 */
#include "common.h"

/* Nonzero while the host keeps the attack page shown. */
u32 xem_host_attack_page(void);

void xem_original_battle_command_panel_fade(void);

void battle_command_panel_fade(void) {
    if (xem_host_attack_page() == 0) {
        xem_original_battle_command_panel_fade();
    }
}
