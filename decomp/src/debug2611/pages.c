/* debug2611 state pages (text 8028022C-80280844, rodata 80280000-80280088):
 * text pages of battle state chosen by the resident debug page number. A
 * unit of its own: its positive li are ori (ASPSX 2.34) where the tools
 * unit's are addiu, and GCC 2.6.3 and 2.7.2 build it where the tools unit's
 * 2.7.2-cdk does not (debug2611.mk). */
#include "pages.h"

/* "\nChar#%d:", linked as original rodata below its user (INCLUDE_RODATA). */
extern char battle_debug_state_page_char_format[];

/* 8028022C: Print the battle state page chosen by the resident debug page number:
 * 1 the enemies' HP (their gear's when they fight in one) and the action
 * list, 2 the presentation events (type, parameter, target mask), 3 the
 * acting enemy's AI flags, 4 the party's and the characters' progress
 * counters. The action list's row y is computed from the index, (i + 2) * 8. */
void battle_debug_print_state_page(void) {
    s32 i, j;
    s32 x;
    s32 hp;

    switch (mode_battle_debug_page) {
    case 1:
        console_place_cursor(0, 0);
        for (i = 3; i < 11; i++) {
            if (battle_area.slots[i].gear == 0) {
                hp = battle_work_area.records[i].pilot.hp;
            } else {
                hp = battle_work_area.records[i].gear.hp;
            }
            console_printf("%d,", hp);
        }
        console_printf("\n");
        console_printf("No  Cd  Cl  An  P1  P2  P3  Tg\n");
        for (i = 0; i < 23; i++) {
            console_place_cursor(0, (i + 2) * 8);
            console_printf("%X", i);
            console_place_cursor(0x24, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].type);
            console_place_cursor(0x48, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].arg1);
            console_place_cursor(0x6C, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].animation);
            console_place_cursor(0x90, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].named);
            console_place_cursor(0xB4, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].param);
            console_place_cursor(0xD8, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].unk5);
            console_place_cursor(0xFC, (i + 2) * 8);
            console_printf("%X", battle_action_list[i].targets);
        }
        break;
    case 2:
        console_place_cursor(0, 0x20);
        console_printf("No  An  Sb  Tg  No  An  Sb  Tg  \n");
        for (i = 0; i < 32; i++) {
            x = (i % 2) * 0x90;
            console_place_cursor(x, (i / 2 + 5) * 8);
            console_printf("%X", i);
            console_place_cursor(x + 0x24, (i / 2 + 5) * 8);
            console_printf("%X", battle_area.events[i].type);
            console_place_cursor(x + 0x48, (i / 2 + 5) * 8);
            console_printf("%X", battle_area.events[i].parameter);
            console_place_cursor(x + 0x6C, (i / 2 + 5) * 8);
            console_printf("%X", battle_area.events[i].targetMask);
        }
        break;
    case 3:
        if (battle_turn_state->actor < 3) {
            break;
        }
        console_place_cursor(0, 0x50);
        console_printf("bFlag\n");
        for (i = 0; i < 8; i++) {
            console_printf("%X ", battle_enemy_ai_blocks[battle_turn_state->actor - 3].bytes[i]);
        }
        console_printf("\n");
        for (i = 0; i < 8; i++) {
            console_printf("%X ", battle_enemy_ai_blocks[battle_turn_state->actor - 3].bytes[i + 8]);
        }
        console_printf("\nhFlag\n");
        for (i = 0; i < 4; i++) {
            console_printf("%X ", battle_enemy_ai_blocks[battle_turn_state->actor - 3].vars[i]);
        }
        console_printf("\n");
        for (i = 0; i < 4; i++) {
            console_printf("%X ", battle_enemy_ai_blocks[battle_turn_state->actor - 3].vars[i + 4]);
        }
        console_printf("\nlFlag\n");
        for (i = 0; i < 2; i++) {
            console_printf("%X ", battle_enemy_ai_blocks[battle_turn_state->actor - 3].longs[i]);
        }
        console_printf("\n");
        for (i = 0; i < 2; i++) {
            console_printf("%X ", battle_enemy_ai_blocks[battle_turn_state->actor - 3].longs[i + 2]);
        }
        break;
    case 4:
        console_place_cursor(0, 0x20);
        for (i = 0; i < 3; i++) {
            console_printf("\nWork#%d:", i);
            for (j = 0; j < 7; j++) {
                console_printf(" %d", battle_work_area.records[i].pilot.useCounts[j]);
            }
        }
        console_printf("\n");
        for (i = 0; i < 11; i++) {
            console_printf(battle_debug_state_page_char_format, i);
            for (j = 0; j < 7; j++) {
                console_printf(" %d", game_data.characters[i].useCounts[j]);
            }
        }
        break;
    }
}

/* "\nChar#%d:". A stray byte (0x2c) follows the string at the end of the
 * unit's rodata, so the literal is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/debug2611/asm/nonmatchings/pages", battle_debug_state_page_char_format); /* 8028007C */
