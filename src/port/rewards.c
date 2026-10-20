/*
 * A host option that changes the game (docs/runtime.md, Random battles in
 * platforming areas): while the host halves the field's random battles
 * (its xem_host_reward_scale is 2), every battle gives twice the progress.
 * The experience the results module shares out (battle_work_area.experience,
 * which battle_results_grant_rewards, 801E2280, totals from the defeated
 * enemies) is doubled before it is shared, and the gold it added to the
 * party's is added once more (to the original's 9999999 at most), the spoils
 * window showing the doubled gold the battle gave. Each party attack's raise
 * of its use count, the progress toward the attacker's deathblows
 * (battle_resolve_action, 800941A4), is made once more under the original's
 * rule (only while the count is at most 0xFDE7), so the menus, the results'
 * learning and an escaped battle's write-back all see it. The enemies'
 * records keep their amounts; Gear actions raise no count, as in the
 * original. Without the halving (1) a battle gives what the original's
 * gives. The host records the halving with the game's inputs. The port keeps
 * no data of its own.
 */
#include "common.h"
#include "resident/gamedata.h"
#include "battle/work.h"
#include "battle/combatant.h"

/* 2 while the host doubles what a battle gives, else 1. */
u32 xem_host_reward_scale(void);

u16 battle_get_slot_bit(u8 slot);
u8 xem_original_battle_resolve_action(void);
void xem_original_battle_results_grant_exp_skills_and_drops(void);
void xem_original_battle_results_run_screens(u32 gold);

u8 battle_resolve_action(void) {
    u8 slot;
    u8 command;
    u8 status;
    u16 before;
    u16 *count;

    slot = battle_work_ptr->attackerIndex;
    command = battle_work_ptr->commandIndex;
    if (xem_host_reward_scale() != 2 || slot >= 3 || command >= 7) {
        return xem_original_battle_resolve_action();
    }
    count = &battle_work_ptr->records[slot].pilot.useCounts[command];
    before = *count;
    status = xem_original_battle_resolve_action();
    if (*count != before && *count <= 0xFDE7) {
        *count += *count - before;
    }
    return status;
}

void battle_results_grant_exp_skills_and_drops(void) {
    s32 i;
    u32 gold;

    if (xem_host_reward_scale() == 2) {
        battle_work_area.experience *= 2;
        gold = 0;
        for (i = 0; i < 8; i++) {
            if (battle_work_area.defeated & battle_get_slot_bit(i)) {
                gold += battle_work_area.records[i + 3].field156;
            }
        }
        game_data.gold += gold;
        if (game_data.gold > 9999999) {
            game_data.gold = 9999999;
        }
    }
    xem_original_battle_results_grant_exp_skills_and_drops();
}

/* The spoils window's six digits are the gold the battle gave. */
void battle_results_run_screens(u32 gold) {
    xem_original_battle_results_run_screens(gold * xem_host_reward_scale());
}
