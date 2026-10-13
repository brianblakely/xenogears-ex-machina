/* What the hosts' controls need to know of the game: whether the battle's
 * attack inputs take the player's buttons (the keyboard's layout), and in a
 * field whether the player walks and Select shows the map's picture (the
 * first-person view's toggle and the buttons it holds back). */
#include "common.h"
#include "battle/turn.h"
#include "resident/mode.h"
#include "../decomp/src/field/field_mode.h"
#include "../decomp/src/field/field_pad.h"
#include "../decomp/src/field/field_picture.h"

s32 field_inventory_is_item_not_held(s32 item);

/* The pictures field_picture_table holds before its end (field_effect.c):
 * the hosts' query reads no further, whatever the table's memory holds
 * while the field loads (it runs outside the game, where a fault would
 * stop the host). */
#define XEM_FIELD_PICTURES 10

/* The on-foot and Gear attack pages (5 and 0x19, entered again as 0x64 and
 * 0x65 after an attack) until the combo closes (turn state 0x2e1) or its
 * events finish. Battle overlay memory is only meaningful while the battle
 * mode (2) runs. */
u32 xem_game_combo_input(u32 mode) {
    TurnState *turn = battle_turn_state;

    if (mode != 2 || turn == NULL || turn->unk2E1[0] != 0 || turn->eventsDone != 0) {
        return 0;
    }
    return turn->page == 5 || turn->page == 0x64 || turn->page == 0x19 || turn->page == 0x65;
}

/* In a field: bit 0 while the player walks (the player control read the
 * pad in the last event pass, which it does with no dialogue open, and no
 * menu runs), bit 1 where Select shows the map's picture (the map has one
 * and its item is held, as field_picture_show finds them), bit 2 while a
 * menu runs (the menu button's or a script's). Elsewhere in a field Select
 * has nothing to show, and the hosts toggle first-person inspection with
 * it. Field overlay memory is only meaningful while the field mode (1)
 * runs. */
u32 xem_game_field_controls(u32 mode) {
    u32 controls;
    s32 i;

    if (mode != 1) {
        return 0;
    }
    controls = field_menu_request != 0xFF ? 4 : field_player_control_polled == 1;
    for (i = 0; i < XEM_FIELD_PICTURES && field_picture_table[i * PICTURE_WORDS + PICTURE_MAP] != 0xFFFF; i++) {
        if ((mode_field_map_id & 0x3FFF) == field_picture_table[i * PICTURE_WORDS + PICTURE_MAP]) {
            if (field_inventory_is_item_not_held(field_picture_table[i * PICTURE_WORDS + PICTURE_ITEM]) == 0) {
                controls |= 2;
            }
            break;
        }
    }
    return controls;
}
