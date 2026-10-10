/* Resident battle-mode entry (8001b6c4-8001b844, mode 2): runs the battle
 * overlay and chooses the next mode from its outcome. This unit owns the
 * entry flag mode_battle_debug_page, a small common it addresses through $gp, while the
 * setup flags belong to the following menu-support unit and are addressed
 * absolutely; that ownership makes it a unit of its own, whose end may lie
 * later than 8001b844 (no own $gp reference or rodata follows; see the
 * target yaml). GCC 2.6.3 and 2.7.2 build the same object. */
#include "common.h"
#include "battle/area.h"
#include "battle/setup.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/mode.h"

u8 mode_battle_debug_page;

/* 8001B6C4: Run the battle and choose the next mode from its outcome and the next
 * battle's formation mode_pending_battle_formation: another battle (mode 2) while it is set.
 * Outcome 0x81 installs scene 0x1EA before mode 1. */
void mode_run_battle(void) {
    u8 outcome;
    s32 mode;

    mode_battle_debug_page = 1;
    cd_sync_reads(0);
    cd_select_directory(12, 0);
    if (*mode_disc_mode_pointer != -1) {
        console_set_external_block(0x80200000);
        console_open(0x10, 0x10, 0x140, 0x100, 0x3E8, 0,
                     0x340, 0, 0x340, 0x20, NULL);
    }
    mode_battle_init_display();
    func_80070F40();
    outcome = D_800C3EB0.outcome;
    if (outcome == 1 || outcome == 0x40 || outcome == 0x21) {
        if (D_800D3338 != 0) {
            mode = 6;
        } else if (mode_pending_battle_formation == 0) {
            if ((game_data.map & 0x7FF) < 0x400) {
                mode = 1;
            } else {
                mode = 3;
            }
        } else {
            mode = 2;
        }
        mode_select_next_mode(mode);
    } else if (outcome == 0x81) {
        mode_clear_field_return();
        game_data.map = 0x1EA;
        game_data.entry[0] = 0;
        game_data.entry[1] = 0;
        game_data.entry[2] = 0;
        mode_select_next_mode(1);
    }
    if (mode_pending_battle_formation == 0) {
        mode_battle_standalone = 1;
    }
    mode_dispatch(0);
}
