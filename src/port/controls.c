/* What the hosts' keyboard layout needs to know of the game: whether the
 * battle's attack inputs take the player's buttons. That is the on-foot and
 * Gear attack pages (5 and 0x19, entered again as 0x64 and 0x65 after an
 * attack) until the combo closes (turn state 0x2e1) or its events finish.
 * Battle overlay memory is only meaningful while the battle mode (2) runs. */
#include "common.h"
#include "battle/turn.h"

u32 xem_game_combo_input(u32 mode) {
    TurnState *turn = battle_turn_state;

    if (mode != 2 || turn == NULL || turn->unk2E1[0] != 0 || turn->eventsDone != 0) {
        return 0;
    }
    return turn->page == 5 || turn->page == 0x64 || turn->page == 0x19 || turn->page == 0x65;
}
