/* Battle code from 8008115C to 8008B478 (the earlier units have files of
 * their own): the command menu's windows, attack page and combos, target
 * selection, approach routes and formation groups, committing and presenting
 * results, the HUD's glyph lists and stepped line, slot masks and random
 * values, the battle input, the result step, the battle heap and the
 * technique and item commits. Its rodata starts at 80070010, where the jump
 * tables return to 0 mod 8 right after 80080160's (docs/matching.md); the
 * text boundary lies after 80080160. The next unit's tables start at 80070314
 * at 4 mod 8. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/command.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/groups.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/resolver.h"
#include "battle/scene.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/work.h"
#include "action_resolve.h"
#include "overlays.h"
#include "own_declarations.h"
#include "resident_views.h"

/* This unit's functions, declared before their first use. */
u8 battle_find_candidate_in_direction(u8 origin, u8 direction);
void battle_attack_page_init_target(u8 member);
void battle_attack_page_build_ap_glyphs(void);
u8 battle_is_known_deathblow_combo(u8 mode, u8 member);
void battle_combo_record_input(u8 code, u8 member);
s32 battle_can_use_combo_step(s32 step, u8 member); /* the member can use combo step `step` now */
u8 battle_combo_record_gear_step(u8 step, u8 member);
void battle_play_system_sound(u8 id); /* play a sound effect */

/* The unit's own uninitialized variables, each in a slot of whole words
 * (decomp/Makefile). Its .bss opens the overlay's at 800c3a70, where the
 * resident's mode table starts the clear. */
static u32 *battle_combo_text_image_blocks[3]; /* 800C3A70: combo text image blocks */
static s32 battle_stepped_line_start_x;     /* 800C3A7C: stepped line state (8008887c) */
static s32 battle_stepped_line_start_y; /* 800C3A80 */
static s32 battle_stepped_line_end_x; /* 800C3A84 */
static s32 battle_stepped_line_end_y; /* 800C3A88 */
static s32 battle_stepped_line_step_x; /* 800C3A8C */
static s32 battle_stepped_line_step_y; /* 800C3A90 */
static u8 battle_stepped_line_x_decreasing; /* 800C3A94 */
static u8 battle_stepped_line_y_decreasing; /* 800C3A98 */
static s32 battle_stepped_line_speed; /* 800C3A9C */
static u8 battle_console_opened; /* 800C3AA0: the debug console is open */

/* 8008115C: Confirm the selected entry of the member's on-foot window: entries 4, 6
 * and 7 open the attack page (5) when the member has a target; 1 and 0 open
 * pages 2 and 7 unless their item is unavailable (buzzer 0x4f); 2 opens page
 * 3; 3 opens page 4 when available, else on a second press of the repeat
 * entry (800c3e29 = 3) page 0xa. */
void battle_command_menu_page_01_attack(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_turn_state->slots[member].defaultTarget != 0xFF) {
            battle_command_menu_sounds_enabled = 0;
            battle_attack_page_enter(member);
            battle_attack_page_init_target(member);
            battle_show_direction_arrows();
            battle_turn_state->page = 5;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 2;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 3;
        break;
    case 3:
        if (battle_turn_state->slots[member].items[10] == 0) {
            battle_turn_state->page = 4;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[7] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0xA;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[5] == 0) {
            battle_turn_state->page = 7;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80081318: Confirm the selected entry of the member's item command window: entries
 * 4, 6 and 7 open the item list when the member has one (else page 2); 0
 * opens page 1 when available, else on a repeat press (800c3e29 = 0) page 7;
 * 2 opens page 3; 3 opens page 4, else on a repeat press page 0xa; 1 opens
 * page 8 unless unavailable (buzzer 0x4f). */
void battle_command_menu_page_02_item(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_item_menu_run(member)) {
            battle_execute_chosen_item(member);
        } else {
            battle_turn_state->page = 2;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 1;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[5] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 7;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 3;
        break;
    case 3:
        if (battle_turn_state->slots[member].items[10] == 0) {
            battle_turn_state->page = 4;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[7] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0xA;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 8;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80081504: Confirm the selected entry of the member's charge command window:
 * entries 4, 6 and 7 charge and end the menu; 0 opens page 1 when available,
 * else on a repeat press (800c3e29 = 0) page 7; 1 and 2 open pages 2 and 9
 * unless unavailable (buzzer 0x4f); 3 opens page 4, else on a repeat press
 * page 0xa. */
void battle_command_menu_page_03_defend(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        battle_start_defending(member);
        battle_turn_state->unk2EA = 0;
        battle_turn_state->menuDone = 1;
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 1;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[5] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 7;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 2;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[10] == 0) {
            battle_turn_state->page = 4;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[7] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0xA;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 9;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 800816F8: Confirm the selected entry of the member's gear-list command window:
 * entries 4, 6 and 7 open the gear list when the member has one (else page
 * 4); 0 opens page 1 when available, else on a repeat press (800c3e29 = 0)
 * page 7; 1 and 3 open pages 2 and 0xa unless unavailable (buzzer 0x4f); 2
 * opens page 3. */
void battle_command_menu_page_04_art(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        battle_command_menu_load_module_block(member);
        if (battle_art_menu_run(member)) {
            battle_execute_chosen_art(member);
        } else {
            battle_turn_state->page = 4;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 1;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[5] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 7;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 2;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 3;
        break;
    case 3:
        if (battle_turn_state->slots[member].items[7] == 0) {
            battle_turn_state->page = 0xA;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 8008189C: Frame the camera and target cursor on the attack page target, mark the
 * four directions that lead to another target and show its name. */
void battle_attack_page_frame_target(u8 member) {
    s32 direction;

    if (battle_turn_state->unk2E9 == 0) {
        battle_camera_start_move(battle_get_slot_bit(battle_turn_state->unk2E8));
        battle_highlight_slots(battle_get_slot_bit(battle_turn_state->unk2E8));
        for (direction = 0; direction < 4; direction++) {
            if (battle_find_candidate_in_direction(battle_turn_state->unk2E8, direction) != battle_turn_state->unk2E8) {
                battle_direction_arrows->arrows[direction] = 1;
            } else {
                battle_direction_arrows->arrows[direction] = 0;
            }
        }
        if (battle_gear_hud_attack_level != 4) {
            battle_ammo_window_open(member);
        }
    }
}

/* 800819A4: Confirm the attack page's target (once): make it the member's default
 * target, close the page, highlight member and target and (except for
 * character 4) queue a move event toward it. */
void battle_attack_page_confirm_target(member)
u8 member;
{
    if (battle_turn_state->unk2E9 == 0) {
        battle_turn_state->unk2E9 = 1;
        battle_highlight_slots(0);
        battle_turn_state->slots[member].defaultTarget = battle_turn_state->unk2E8;
        battle_command_menu_release_module_block();
        battle_command_menu_release_file3_block();
        battle_hide_direction_arrows();
        battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
        if (battle_party_character_ids[member] != 4) {
            battle_area_events[battle_turn_state->eventCount].actor = member;
            battle_area_events[battle_turn_state->eventCount].type = 0xFD;
            battle_area_events[battle_turn_state->eventCount].parameter = 0;
            battle_area_events[battle_turn_state->eventCount].targetMask = battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget);
            battle_turn_state->eventCount++;
        }
        battle_ammo_window_hide(member, 0);
    }
}

/* 80081B58: The attack page (5) of the member's turn. The turn state bytes 0x2d4/0x2d5
 * hold the AP left and the maximum, 0x2df the cost of the pressed attack,
 * 0x2e0-0x2e5 the execution, closed, shown page, attacked, combo armed and
 * reacted flags. Cancel (5) leaves the page while no AP were spent, else
 * closes the command; attacks whose item is blocked beep; directions retarget
 * until the combo starts. Attacks 6/7/4 cost 1/2/3 AP (4 also arms a known
 * combo): with enough AP the combo is recorded and executed and the timer
 * reload set from the AP left; the command closes when no AP remain or the
 * target reacted. It is an int function that returns no value (the return
 * register stays live at the exits). */
s32 battle_command_menu_page_05_attack_inputs(u8 member) {
    battle_command_menu_sounds_enabled = 0;
    if (battle_turn_state->unk2E1[0] != 0) {
        return;
    }
    battle_turn_state->unk2DF = 0;
    battle_attack_page_frame_target(member);
    switch (battle_pressed_key) {
    case 5:
        if (battle_turn_state->unk2D4[0] != battle_turn_state->unk2D4[1]) {
            battle_close_actor_event_queue(member);
            battle_turn_state->unk2E1[0] = 1;
        } else {
            battle_cancel_turn();
            battle_command_menu_sounds_enabled = 1;
            battle_ui->unk7B = 0;
            battle_ui->unkAF = 0;
            battle_turn_state->page = 1;
            battle_hide_direction_arrows();
            battle_ammo_window_hide(member, 1);
        }
        break;
    case 4:
        if (battle_turn_state->slots[member].items[2] != 0) {
            battle_pressed_key = 5;
            battle_play_system_sound(0x4F);
        }
        break;
    case 7:
        if (battle_turn_state->slots[member].items[1] != 0) {
            battle_pressed_key = 5;
            battle_play_system_sound(0x4F);
        }
        break;
    case 6:
        if (battle_turn_state->slots[member].items[0] != 0) {
            battle_pressed_key = 5;
            battle_play_system_sound(0x4F);
        }
        break;
    }
    if (battle_turn_state->unk2E9 == 0) {
        switch (battle_pressed_key) {
        case 0:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 0);
            battle_reset_events_for_target(battle_turn_state->actor, battle_turn_state->unk2E8);
            battle_play_system_sound(0x4C);
            break;
        case 1:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 1);
            battle_reset_events_for_target(battle_turn_state->actor, battle_turn_state->unk2E8);
            battle_play_system_sound(0x4C);
            break;
        case 2:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 2);
            battle_reset_events_for_target(battle_turn_state->actor, battle_turn_state->unk2E8);
            battle_play_system_sound(0x4C);
            break;
        case 3:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 3);
            battle_reset_events_for_target(battle_turn_state->actor, battle_turn_state->unk2E8);
            battle_play_system_sound(0x4C);
            break;
        }
        battle_plan_approach_route(member, battle_turn_state->unk2E8);
    }
    battle_ui->unkAF = 1;
    switch (battle_pressed_key) {
    case 4:
        if (battle_is_known_deathblow_combo(4, member) && battle_turn_state->unk2D4[0] - 3 >= 0) {
            battle_turn_state->unk2E1[3]++;
        }
        battle_turn_state->unk2DF++;
    case 7:
        battle_turn_state->unk2DF++;
    case 6:
        battle_turn_state->unk2DF++;
        if (battle_turn_state->unk2D4[0] - battle_turn_state->unk2DF >= 0) {
            battle_play_system_sound(0x4D);
            battle_attack_page_confirm_target(member);
            if (battle_ui->unkCB != 0) {
                battle_turn_state->page = 0x64;
                battle_turn_state->unk2E1[1] = 0xFF;
                battle_turn_state->unk2E0 = 1;
            } else {
                battle_turn_state->page = 5;
                battle_turn_state->unk2E1[1] = 5;
            }
            battle_turn_state->unk2D4[0] -= battle_turn_state->unk2DF;
            battle_attack_page_build_ap_glyphs();
            battle_combo_record_input(battle_pressed_key, member);
            battle_turn_state->unk2E1[4] = battle_execute_attack_step(member, battle_turn_state->unk2DF);
            battle_turn_queue.timers[1][member] = battle_ap_timer_reload_table_by_max_ap[battle_turn_state->unk2D4[1]][battle_turn_state->unk2D4[0]] * 100 / 56;
            battle_turn_state->unk2E1[2] = 1;
        } else {
            battle_play_system_sound(0x4F);
        }
        if (battle_turn_state->unk2D4[0] == 0) {
            battle_close_actor_event_queue(member);
            battle_turn_state->unk2E1[0] = 1;
        }
        if (battle_turn_state->unk2E1[4] != 0) {
            battle_close_actor_event_queue(member);
            battle_turn_state->unk2E1[0] = 1;
        }
        break;
    case 5:
        break;
    }
}

/* 800820A4: Confirm the selected entry of the member's special command window:
 * entries 4, 6 and 7 start the selection (800c3eac +0x2e1) when the member
 * has a target and a special is available, else open page 7; 1 and 0 open
 * pages 8 and 1 unless unavailable (buzzer 0x4f); 2 opens page 9, else on a
 * repeat press (800c3e29 = 2) page 3; 3 opens page 0xa, else on a repeat
 * press page 4. */
void battle_command_menu_page_07_combo(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_turn_state->slots[member].defaultTarget != 0xFF) {
            if (battle_combo_command_run(member)) {
                battle_turn_state->unk2E1[0] = 1;
            } else {
                battle_turn_state->page = 7;
                battle_turn_state->unk2E1[1] = 0xFF;
            }
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 8;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 9;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 2) {
            battle_turn_state->page = 3;
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[7] == 0) {
            battle_turn_state->page = 0xA;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[10] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 4;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 1;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 800822C4: Confirm the selected entry of the member's item window: entries 4, 6 and 7
 * open the item list when the member has one (else page 8); 0 opens page 7,
 * else on a repeat press (800c3e29 = 0) page 1; 2 opens page 9, else on a
 * repeat press (= 2) page 3; 3 opens page 0xa, else on a repeat press (= 3)
 * page 4; 1 opens page 2 unless unavailable (buzzer 0x4f). */
void battle_command_menu_page_08_item(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_item_menu_run(member)) {
            battle_execute_chosen_item(member);
        } else {
            battle_turn_state->page = 8;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[5] == 0) {
            battle_turn_state->page = 7;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[9] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 1;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 9;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 2) {
            battle_turn_state->page = 3;
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[7] == 0) {
            battle_turn_state->page = 0xA;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[10] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 4;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 2;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80082504: Confirm the selected entry of the escape command window: entries 4, 6 and
 * 7 try to flee (outcome 0x40 on success) and end the menu; 0 opens page 7
 * when available, else on a repeat press (800c3e29 = 0) page 1; 3 opens page
 * 0xa, else on a repeat press (800c3e29 = 3) page 4; 1 opens page 8 unless
 * unavailable (buzzer 0x4f); 2 opens page 3. */
void battle_command_menu_page_09_escape(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        /* 8009a9d0 takes no arguments; the call passes the actor. */
        if (((s32 (*)())battle_try_escape)(battle_turn_state->actor)) {
            battle_area_outcome = 0x40;
        }
        battle_turn_state->menuDone = 1;
        break;
    case 0:
        if (battle_turn_state->slots[member].items[5] == 0) {
            battle_turn_state->page = 7;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[9] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 1;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 8;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[7] == 0) {
            battle_turn_state->page = 0xA;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[10] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 4;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 3;
        break;
    }
}

/* 800826CC: The member boards its gear: it takes a formation group of its own, its
 * records and panel switch to the gear, and the game data notes that the
 * party member entered a gear unless 80059179 is set. */
void battle_board_gear(u8 member) {
    s32 i;

    battle_give_slot_own_group(member);
    battle_revive_slot_record(member);
    battle_slot_swap_sprite_for_gear(member);
    battle_work_area.records[member].flags15A |= 0x80;
    battle_leave_formation_group(member);
    battle_slot_flags[member].unk1 = 2;
    if (battle_party_character_ids[member] != 7) {
        battle_graphics->panels[member].state = 2;
    }
    battle_area_slots[member].gear = 1;
    battle_turn_state->reaction[member] = 1;
    for (i = 0; i < 3; i++) {
        if (game_data.party[i] == battle_party_character_ids[member] && mode_gear_riding_lock == 0) {
            game_data.inGear[i] = 1;
        }
    }
}

/* 80082820: Confirm the selected entry of the member's main command window: entries
 * 4, 6 and 7 board the gear and end the menu; 0 opens page 7 when available,
 * else on a second press of the repeat entry (800c3e29 = 0) page 1; 2 opens
 * page 9, else on a repeat press (800c3e29 = 2) page 3; 1 and 3 open pages 8
 * and 4 unless their item is unavailable (buzzer 0x4f). */
void battle_command_menu_page_0a_board_gear(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        battle_board_gear(member);
        battle_turn_state->menuDone = 1;
        break;
    case 0:
        if (battle_turn_state->slots[member].items[5] == 0) {
            battle_turn_state->page = 7;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[9] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 1;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[11] == 0) {
            battle_turn_state->page = 8;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 9;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 2) {
            battle_turn_state->page = 3;
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[10] == 0) {
            battle_turn_state->page = 4;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 800829F4: Confirm the selected entry of the member's on-foot command window: entries
 * 4, 6 and 7 open the attack page (0x19) when the member has a target; 0 and
 * 1 open pages 0x15 and 0x11 unless their item is unavailable (buzzer 0x4f);
 * 2 opens page 0x12; 3 opens page 0x13 when available, else on a second
 * press of the repeat entry page 0x18. */
void battle_command_menu_page_10_gear_attack(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_turn_state->slots[member].defaultTarget != 0xFF) {
            battle_attack_page_enter(member);
            battle_attack_page_init_target(member);
            battle_turn_state->page = 0x19;
            battle_show_direction_arrows();
            battle_command_menu_sounds_enabled = 0;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x11;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 0x12;
        break;
    case 3:
        if (battle_turn_state->slots[member].items[6] == 0) {
            battle_turn_state->page = 0x13;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[4] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x18;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[13] == 0) {
            battle_turn_state->page = 0x15;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80082BB0: Confirm the selected entry of the member's gear command window: entries 4,
 * 6 and 7 open the part list when the gear has one (else page 0x11); 0 and 1
 * open pages 0x10 and 0x16 unless their item is unavailable (buzzer 0x4f);
 * 2 opens page 0x12; 3 opens page 0x13 when available, else on a second
 * press of the repeat entry (800c3e29 = 3) page 0x18. */
void battle_command_menu_page_11_gear_item(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_item_menu_run(member)) {
            battle_execute_chosen_item(member);
        } else {
            battle_turn_state->page = 0x11;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 0x10;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 0x12;
        break;
    case 3:
        if (battle_turn_state->slots[member].items[6] == 0) {
            battle_turn_state->page = 0x13;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[4] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x18;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x16;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80082D4C: Confirm the selected entry of the member's gear window with charge:
 * entries 4, 6 and 7 charge the gear's fuel (by 800d2c32, capped) and end
 * the member's turn; 0, 1 and 2 open pages 0x10, 0x11 and 0x17 unless their
 * item is unavailable (buzzer 0x4f); 3 opens page 0x13 when available, else
 * on a second press of the repeat entry page 0x18. */
void battle_command_menu_page_12_gear_charge(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        battle_start_defending(member);
        battle_work_area.records[member].gear.fuel += battle_gear_hud_charge;
        if (battle_work_area.records[member].gear.fuel > battle_work_area.records[member].gear.maxFuel) {
            battle_work_area.records[member].gear.fuel = battle_work_area.records[member].gear.maxFuel;
        }
        battle_turn_state->reaction[member] = 1;
        battle_turn_state->unk2EA = 0;
        battle_turn_state->menuDone = 1;
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 0x10;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x11;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[6] == 0) {
            battle_turn_state->page = 0x13;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[4] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x18;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 0x17;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80082F7C: Confirm the selected entry of the member's command window (800d3014):
 * entries 4, 6 and 7 open the gear list when the member has one (else page
 * 0x13); 0, 1 and 3 open pages 0x10, 0x11 and 0x18 unless their item is
 * unavailable (buzzer 0x4f); 2 opens page 0x12. */
void battle_command_menu_page_13_gear_art(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        battle_command_menu_load_module_block(member);
        if (battle_art_menu_run(member)) {
            battle_execute_chosen_art(member);
        } else {
            battle_turn_state->page = 0x13;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 0x10;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x11;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 0x12;
        break;
    case 3:
        if (battle_turn_state->slots[member].items[4] == 0) {
            battle_turn_state->page = 0x18;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 800830A8: Confirm the selected entry of the member's gear command window with the
 * guard toggle: entries 4, 6 and 7 toggle the guard (pilot and gear status
 * bit 0x8000; setting it clears gear bit 0x20 and pilot bit 0x1000) and end
 * the menu; 1 and 0 open pages 0x16 and 0x10 unless unavailable (buzzer
 * 0x4f); 2 opens page 0x17, else on a repeat press (800c3e29 = 2) page 0x12;
 * 3 opens page 0x18, else on a repeat press page 0x13. */
void battle_command_menu_page_15_gear_haste(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_work_area.records[member].gear.status80 & 0x8000) {
            battle_work_area.records[member].gear.status80 &= 0x7FFF;
            battle_work_area.records[member].pilot.status84.half.active &= 0x7FFF;
        } else {
            battle_work_area.records[member].gear.status7C &= ~0x20;
            battle_work_area.records[member].pilot.status7C &= ~0x1000;
            battle_work_area.records[member].gear.status80 |= 0x8000;
            battle_work_area.records[member].pilot.status84.half.active |= 0x8000;
        }
        battle_turn_state->menuDone = 1;
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x16;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 0x17;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 2) {
            battle_turn_state->page = 0x12;
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[4] == 0) {
            battle_turn_state->page = 0x18;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[6] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x13;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 0x10;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80083340: Confirm the selected entry of the member's item window (gear pages):
 * entries 4, 6 and 7 open the item list when the member has one (else page
 * 0x16); 0 opens page 0x15, else on a repeat press (800c3e29 = 0) page 0x10;
 * 2 opens page 0x17, else on a repeat press page 0x12; 3 opens page 0x18,
 * else on a repeat press page 0x13; 1 opens page 0x11 unless unavailable
 * (buzzer 0x4f). */
void battle_command_menu_page_16_gear_item(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        if (battle_item_menu_run(member)) {
            battle_execute_chosen_item(member);
        } else {
            battle_turn_state->page = 0x16;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[13] == 0) {
            battle_turn_state->page = 0x15;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[9] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x10;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 0x17;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 2) {
            battle_turn_state->page = 0x12;
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[4] == 0) {
            battle_turn_state->page = 0x18;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[6] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x13;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x11;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80083580: Confirm the selected entry of the escape window (gear pages): entries 4,
 * 6 and 7 try to flee (outcome 0x40 on success) and end the menu; 0 opens
 * page 0x15, else on a repeat press (800c3e29 = 0) page 0x10; 1 opens page
 * 0x16 unless unavailable (buzzer 0x4f); 3 opens page 0x18, else on a repeat
 * press page 0x13; 2 opens page 0x12. */
void battle_command_menu_page_17_gear_escape(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        /* 8009a9d0 takes no arguments; the call passes the actor. */
        if (((s32 (*)())battle_try_escape)(battle_turn_state->actor)) {
            battle_area_outcome = 0x40;
        }
        battle_turn_state->menuDone = 1;
        break;
    case 0:
        if (battle_turn_state->slots[member].items[13] == 0) {
            battle_turn_state->page = 0x15;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[9] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x10;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x16;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[4] == 0) {
            battle_turn_state->page = 0x18;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 3) {
            if (battle_turn_state->slots[member].items[6] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x13;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        battle_turn_state->page = 0x12;
        break;
    }
}

/* 80083748: Confirm the selected entry of the member's gear part window: entries 4, 6
 * and 7 load file 3 and open the part list when the gear has one (else page
 * 0x18); 0 opens page 0x15, else on a repeat press (800c3e29 = 0) page 0x10;
 * 1 and 3 open pages 0x16 and 0x13 unless unavailable (buzzer 0x4f); 2 opens
 * page 0x17, else on a repeat press page 0x12. */
void battle_command_menu_page_18_gear_menu(member)
u8 member;
{
    switch (battle_pressed_key) {
    case 4:
    case 6:
    case 7:
        /* 8007fe3c takes no arguments; the call passes the member. */
        ((void (*)())battle_command_menu_load_file3_block)(member);
        if (battle_gear_menu_run(member)) {
            battle_execute_chosen_art(member);
        } else {
            battle_turn_state->page = 0x18;
        }
        break;
    case 0:
        if (battle_turn_state->slots[member].items[13] == 0) {
            battle_turn_state->page = 0x15;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 0) {
            if (battle_turn_state->slots[member].items[9] != 0) {
                battle_play_menu_sound(0x4F);
            } else {
                battle_turn_state->page = 0x10;
            }
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 1:
        if (battle_turn_state->slots[member].items[12] == 0) {
            battle_turn_state->page = 0x16;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    case 2:
        if (battle_turn_state->slots[member].items[8] == 0) {
            battle_turn_state->page = 0x17;
        } else if (battle_turn_state->repeatArmed != 0 && battle_direction_input[1] == 2) {
            battle_turn_state->page = 0x12;
            battle_turn_state->repeatArmed = 0;
        } else {
            battle_turn_state->repeatArmed = 1;
            battle_play_menu_sound(0x4F);
        }
        break;
    case 3:
        if (battle_turn_state->slots[member].items[6] == 0) {
            battle_turn_state->page = 0x13;
        } else {
            battle_play_menu_sound(0x4F);
        }
        break;
    }
}

/* 80083948: The gear attack page (0x19): like the attack page (80081b58) but paid in
 * fuel. The gear HUD level (4: free combos) and the chain (+0x2d6, history
 * +0x2cc) pick the step's fuel cost from the gear HUD table, indexed like the
 * combo flags (80086b88). Cancel (5) leaves the page unless steps were
 * taken; the command closes once the chain cannot continue, the target is
 * down or the attack ended the chain. Int function without a value. */
s32 battle_command_menu_page_19_gear_attack_inputs(u8 member) {
    s32 steps = 0;
    u16 fuel = battle_work_area.records[member].gear.fuel;
    u8 enough = 0;
    u8 leave;
    s32 index;
    s32 cost;

    battle_command_menu_sounds_enabled = 0;
    if (battle_turn_state->unk2E1[0] != 0) {
        return;
    }
    battle_attack_page_frame_target(member);
    switch (battle_pressed_key) {
    case 5:
        leave = 1;
        if (battle_turn_state->unk2E7 != 0 && battle_work_area.gearHud.level != 4) {
            battle_close_actor_event_queue(member);
            battle_turn_state->unk2E1[0] = leave;
            leave = 0;
        }
        if (leave) {
            battle_ui->unkA8 = 0;
            battle_cancel_turn();
            battle_command_menu_sounds_enabled = 1;
            battle_turn_state->page = 0x10;
            battle_hide_direction_arrows();
            if (battle_work_area.gearHud.level != 4) {
                battle_ammo_window_hide(member, 1);
            } else {
                battle_turn_state->unk2E7 = 0;
                battle_turn_state->unk2D6 = 0;
            }
        }
        break;
    case 4:
        if (battle_turn_state->slots[member].items[2] != 0) {
            battle_pressed_key = 5;
            battle_play_system_sound(0x4F);
        }
        break;
    case 7:
        if (battle_turn_state->slots[member].items[1] != 0) {
            battle_pressed_key = 5;
            battle_play_system_sound(0x4F);
        }
        break;
    case 6:
        if (battle_turn_state->slots[member].items[0] != 0) {
            battle_pressed_key = 5;
            battle_play_system_sound(0x4F);
        }
        break;
    }
    if (battle_turn_state->unk2E9 == 0) {
        switch (battle_pressed_key) {
        case 0:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 0);
            battle_play_system_sound(0x4C);
            break;
        case 1:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 1);
            battle_play_system_sound(0x4C);
            break;
        case 2:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 2);
            battle_play_system_sound(0x4C);
            break;
        case 3:
            battle_turn_state->unk2E8 = battle_find_candidate_in_direction(battle_turn_state->unk2E8, 3);
            battle_play_system_sound(0x4C);
            break;
        }
    }
    battle_ui->reaction[member] = 1;
    if (battle_turn_state->unk2E7 != 0) {
        if (battle_work_area.gearHud.level != 4 &&
            (battle_work_area.gearHud.level == 0 || battle_work_area.gearHud.level - 1 < battle_turn_state->unk2CC[0])) {
            battle_close_actor_event_queue(member);
            battle_turn_state->unk2E1[0] = 1;
            return;
        }
        if (battle_is_slot_in_mask(battle_area_knocked_out, battle_turn_state->slots[member].defaultTarget)) {
            battle_close_actor_event_queue(member);
            battle_turn_state->unk2E1[0] = 1;
            return;
        }
    }
    switch (battle_pressed_key) {
    default:
        if (battle_work_area.gearHud.level == 4 && battle_turn_state->unk2E7 == 0) {
            battle_turn_state->unk2E7++;
            battle_combo_record_gear_step(0xFF, member);
        }
        break;
    case 5:
        break;
    case 4:
        steps++;
    case 7:
        steps++;
    case 6:
        steps++;
        if (battle_work_area.gearHud.level == 4 && battle_turn_state->unk2E7 == 0) {
            battle_turn_state->unk2E7++;
            battle_combo_record_gear_step(0xFF, member);
            break;
        }
        /* The call narrows like a u8 (u8, u8) prototype. */
        if ((u8)battle_can_use_combo_step((u8)(steps - 1), member)) {
            if (battle_turn_state->unk2D6 == 0) {
                cost = STEP_FUEL[steps];
            } else if (battle_work_area.gearHud.level == 4) {
                index = steps + 12;
                cost = STEP_FUEL[index];
            } else {
                cost = STEP_FUEL[steps + (battle_turn_state->unk2CC[0] + 1) * 3];
            }
            if (fuel - cost >= 0) {
                enough = 1;
            }
            if (enough) {
                battle_play_system_sound(0x4D);
                battle_unread_gear_attack_step_flag = 1;
                battle_attack_page_confirm_target(member);
                if (battle_turn_state->unk2D6 != 0) {
                    battle_turn_state->unk2E1[3]++;
                }
                if (battle_ui->unkCB != 0) {
                    battle_turn_state->page = 0x65;
                    battle_turn_state->unk2E1[1] = 0xFF;
                    battle_turn_state->unk2E0 = 1;
                } else {
                    battle_turn_state->page = 0x19;
                    battle_turn_state->unk2E1[1] = 0x19;
                }
                battle_turn_state->unk2E1[3] = battle_combo_record_gear_step(steps - 1, member) == 0;
                battle_execute_attack_step(member, steps);
                battle_work_area.records[member].gear.fuel -= STEP_FUEL[battle_turn_state->unk2DC];
                battle_gear_hud_build_fuel_glyphs(member);
                battle_turn_state->unk2E7++;
            } else {
                battle_command_menu_sounds_enabled = 1;
                battle_play_menu_sound(0x4F);
                battle_command_menu_sounds_enabled = 0;
            }
            if (battle_turn_state->unk2E1[3] != 0) {
                battle_close_actor_event_queue(member);
                battle_turn_state->unk2E1[0] = 1;
            }
        }
        break;
    }
}

/* 80083FF4: Whether the member can attack `slot`: present and visible; an unflagged
 * slot must be adjacent (formation distance 0) and not down, a flagged slot
 * not down by +0x120. */
u8 battle_can_attack_slot(u8 member, u8 slot) {
    u8 result = 0;

    if (battle_turn_queue.present[slot] != 0 && battle_area_slots[slot].hidden == 0) {
        if (battle_slot_flags[slot].unk1 == 0) {
            if (battle_formation->links[battle_area_slots[member].group][battle_area_slots[slot].group].distance == 0) {
                result = (battle_work_area.records[slot].pilot.status7C & 0xC001) == 0;
            }
        } else if (!(battle_work_area.records[slot].gear.status7C & 0xC001)) {
            result = 1;
        }
    }
    return result;
}

/* 80084108: Whether `slot` can be targeted by a party attack: present and visible;
 * unflagged slots also not down (+0x7c 0xc001) unless `any`, flagged slots
 * not down by +0x120 unless `any`. */
u8 battle_can_target_slot(u8 slot, u8 any) {
    u8 result = 0;

    if (battle_turn_queue.present[slot] != 0 && battle_area_slots[slot].hidden == 0) {
        if (battle_slot_flags[slot].unk1 == 0) {
            result = 1;
            if (any == 0) {
                result = (battle_work_area.records[slot].pilot.status7C & 0xC001) == 0;
            }
        } else if (any != 0 || !(battle_work_area.records[slot].gear.status7C & 0xC001)) {
            result = 1;
        }
    }
    return result;
}

/* 800841E0: Order the member's attack candidates: the reachable opposing slots, those
 * in its own formation group first, and the lowest-HP one (within the group
 * when there is one) moved to the front. Returns the default target. */
u8 battle_order_attack_candidates(u8 member) {
    u8 slots[12];
    u8 grouped[12];
    s32 i;
    s32 count;
    s32 n;
    u8 swap;

    battle_target_candidate_count = 0;
    for (i = 0; i < 12; i++) {
        slots[i] = 0xFF;
        battle_target_candidates[i] = 0xFF;
        grouped[i] = 0;
    }
    if (member < 3) {
        i = 3;
        count = 0;
        for (; i < 11; i++) {
            if (battle_can_attack_slot(member, i)) {
                slots[count++] = i;
                battle_target_candidate_count++;
            }
        }
    } else {
        i = 0;
        count = 0;
        for (; i < 3; i++) {
            if (battle_can_attack_slot(member, i)) {
                slots[count++] = i;
                battle_target_candidate_count++;
            }
        }
    }
    n = 0;
    for (i = 0; i < count; i++) {
        if (battle_area_slots[member].group == battle_area_slots[slots[i]].group) {
            battle_target_candidates[n] = slots[i];
            slots[i] = 0xFF;
            grouped[n] = 1;
            n++;
        }
    }
    for (i = 0; i < count; i++) {
        if (slots[i] != 0xFF) {
            battle_target_candidates[n] = slots[i];
            n++;
        }
    }
    if (grouped[0] != 0) {
        for (i = 1; i < count; i++) {
            if (grouped[i] != 0 && battle_work_area.records[battle_target_candidates[0]].pilot.hp > battle_work_area.records[battle_target_candidates[i]].pilot.hp) {
                swap = battle_target_candidates[0];
                battle_target_candidates[0] = battle_target_candidates[i];
                battle_target_candidates[i] = swap;
            }
        }
    } else {
        for (i = 1; i < count; i++) {
            if (battle_work_area.records[battle_target_candidates[0]].pilot.hp > battle_work_area.records[battle_target_candidates[i]].pilot.hp) {
                swap = battle_target_candidates[0];
                battle_target_candidates[0] = battle_target_candidates[i];
                battle_target_candidates[i] = swap;
            }
        }
    }
    return battle_target_candidates[0];
}

/* 80084548: Collect the slots a party attack can target (80084108, `any` includes
 * downed ones) as candidates with their mask: side 0 the enemies, 1 the
 * party, 2 both (the party first unless `partyFirst` is clear). Returns the
 * first candidate. */
u8 battle_collect_target_candidates(u8 side, u8 any, u8 partyFirst) {
    s32 i;
    s32 count;
    s32 n1;
    s32 n2 = 0;
    u8 first1;
    u8 first2;
    s32 slot;

    for (i = 0; i < 12; i++) {
        battle_target_candidates[i] = 0xFF;
    }
    switch (side) {
    case 0:
        first1 = 3;
        n1 = 8;
        break;
    case 1:
        first1 = 0;
        n1 = 3;
        break;
    case 2:
        if (partyFirst == 0) {
            first1 = 3;
            n1 = 8;
            first2 = 0;
            n2 = 3;
        } else {
            first1 = 0;
            n1 = 3;
            first2 = 3;
            n2 = 8;
        }
        break;
    }
    count = 0;
    battle_target_candidate_count = 0;
    battle_target_candidate_mask = 0;
    i = first1;
    while (--n1 >= 0) {
        slot = i;
        if (battle_can_target_slot(slot, any)) {
            battle_target_candidates[count] = slot;
            battle_target_candidate_mask |= battle_get_slot_bit(slot);
            battle_target_candidate_count++;
            count++;
        }
        i++;
    }
    i = first2;
    while (--n2 >= 0) {
        slot = i;
        if (battle_can_target_slot(slot, any)) {
            battle_target_candidates[count] = slot;
            battle_target_candidate_mask |= battle_get_slot_bit(slot);
            battle_target_candidate_count++;
            count++;
        }
        i++;
    }
    return battle_target_candidates[0];
}

/* 80084750: Collect the enemy slots the member can target (80083ff4) as candidates
 * and their mask; returns the first candidate. */
u8 battle_collect_attackable_enemies(u8 member) {
    s32 i;
    s32 n;
    s32 count;
    s32 slot;

    n = 8;
    for (i = 0; i < 12; i++) {
        battle_target_candidates[i] = 0xFF;
    }
    count = 0;
    battle_target_candidate_count = 0;
    battle_target_candidate_mask = 0;
    i = 3;
    while (--n >= 0) {
        slot = i;
        if (battle_can_attack_slot(member, slot)) {
            battle_target_candidates[count] = slot;
            battle_target_candidate_mask |= battle_get_slot_bit(slot);
            battle_target_candidate_count++;
            count++;
        }
        i++;
    }
    return battle_target_candidates[0];
}

/* 80084854: The candidate nearest to `origin` that lies in screen direction
 * `direction` (0-3, each a quarter turn around it); `origin` when none. */
u8 battle_find_candidate_in_direction(u8 origin, u8 direction) {
    s32 best = 0xFFFFFF;
    s32 i;
    s32 angle;
    s32 inside;
    s32 distance;
    u8 nearest = origin;

    for (i = 0; i < 11; i++) {
        if (battle_target_candidates[i] != 0xFF && battle_target_candidates[i] != origin) {
            inside = 0;
            angle = ratan2(SLOT_Z(battle_target_candidates[i]) - SLOT_Z(origin), SLOT_X(battle_target_candidates[i]) - SLOT_X(origin));
            switch (direction) {
            case 0:
                if ((u16)(angle + 0x200) < 0x400) {
                    inside = 1;
                }
                break;
            case 1:
                if ((u16)(angle + 0x600) < 0x400) {
                    inside = 1;
                }
                break;
            case 2:
                if ((u16)(angle + 0x800) < 0x200) {
                    inside = 1;
                }
                if ((u16)(angle - 0x600) <= 0x200) {
                    inside = 1;
                }
                break;
            case 3:
                if ((u16)(angle - 0x200) < 0x400) {
                    inside = 1;
                }
                break;
            }
            if (inside) {
                distance = SQUARE(SLOT_Z(battle_target_candidates[i]) - SLOT_Z(origin));
                distance += SQUARE(SLOT_X(battle_target_candidates[i]) - SLOT_X(origin));
                if (distance < best) {
                    best = distance;
                    nearest = battle_target_candidates[i];
                }
            }
        }
    }
    return nearest;
}

/* 80084A7C: Keep the member's default target as the attack page target when it is
 * still a candidate, else take the first candidate. */
void battle_attack_page_init_target(u8 member) {
    s32 found = 0;
    s32 i;

    battle_turn_state->unk2E8 = battle_turn_state->slots[member].defaultTarget;
    battle_order_attack_candidates(member);
    for (i = 0; i < battle_target_candidate_count; i++) {
        if (battle_target_candidates[i] == battle_turn_state->slots[member].defaultTarget) {
            found++;
            break;
        }
    }
    if (found == 0) {
        battle_turn_state->unk2E8 = battle_target_candidates[0];
    }
}

/* 80084B40: Pick a target with the direction keys, starting from the attack page
 * target: highlight member and target each frame; 0-3 move to the nearest
 * candidate that way, 4/6/7 confirm it as the member's default target (1)
 * and 5 cancels back to the default target (0). */
u8 battle_pick_default_target(u8 member) {
    u8 state = 2;
    u8 target = battle_turn_state->unk2E8;

    battle_pressed_key = 8;
    do {
        while (battle_pressed_key == 8) {
            battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(target));
            battle_highlight_slots(battle_get_slot_bit(target));
            battle_wait_frame();
        }
        switch (battle_pressed_key) {
        case 4:
        case 6:
        case 7:
            state = 1;
            battle_turn_state->slots[member].defaultTarget = target;
            break;
        case 5:
            battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
            state = 0;
            battle_highlight_slots(battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
            break;
        case 0:
            target = battle_find_candidate_in_direction(target, 0);
            break;
        case 1:
            target = battle_find_candidate_in_direction(target, 1);
            break;
        case 2:
            target = battle_find_candidate_in_direction(target, 2);
            break;
        case 3:
            target = battle_find_candidate_in_direction(target, 3);
            break;
        }
        battle_pressed_key = 8;
    } while (state == 2);
    return state;
}

/* 80084D28: The mask of slots in 800c3d64 in the formation group of slot 800c3e2c. */
u16 battle_get_candidates_in_target_group(void) {
    u16 mask = 0;
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (battle_is_slot_in_mask(battle_target_candidate_mask, slot) && battle_area_slots[battle_target_cursor_slot].group == battle_area_slots[slot].group) {
            mask |= battle_get_slot_bit(slot);
        }
    }
    return mask;
}

/* 80084DE4: Set up the target candidates for a target selection word (mode 0: the
 * fallback's, 1: 0x3000, 2: 0x2001). Bit 0x1000 selects the enemies, else
 * the party; 0x2000 both; 0x8000 keeps only downed slots (+0x7c bit
 * 0x8000); 0x4000 the member alone. `own` takes the member's reachable
 * enemies (80084750). Sets the current target 800c3e2c and returns the
 * selection. Each downed test keeps its own store and release (merged
 * again by the compiler). */
u16 battle_init_target_candidates(u16 selection, u16 fallback, u8 member, u8 mode, u8 own) {
    u8 downed = 0;
    u8 side;
    u8 partyFirst;
    u16 mask;
    s32 count;
    s32 i;
    u8 slot;

    switch (mode) {
    case 0:
        selection = fallback;
        break;
    case 1:
        selection = 0x3000;
        break;
    case 2:
        selection = 0x2001;
        break;
    }
    if (selection & 0x8000) {
        downed = 1;
    }
    if (selection & 0x1000) {
        partyFirst = 0;
        side = 0;
        mask = 0xFFF8;
    } else {
        partyFirst = 1;
        side = 1;
        mask = 7;
    }
    if (selection & 0x2000) {
        side = 2;
        mask = 0xFFFF;
    }
    if (own) {
        battle_target_cursor_slot = battle_collect_attackable_enemies(member);
    } else {
        battle_target_cursor_slot = battle_collect_target_candidates(side, downed, partyFirst);
    }
    battle_target_candidate_mask &= mask;
    if (selection & 0x8000) {
        count = 0;
        battle_target_cursor_slot = 0xFF;
        for (i = 0; i < 11; i++) {
            slot = i;
            if (battle_is_slot_in_mask(battle_target_candidate_mask, slot)) {
                if (battle_slot_flags[i].unk1 == 0) {
                    if (battle_work_area.records[i].pilot.status7C & 0x8000) {
                        battle_target_cursor_slot = slot;
                        battle_target_candidates[count] = slot;
                        count++;
                    } else {
                        battle_target_candidate_mask &= battle_get_other_slot_bits(slot);
                    }
                } else if (battle_work_area.records[i].gear.status7C & 0x8000) {
                    battle_target_cursor_slot = slot;
                    battle_target_candidates[count] = slot;
                    count++;
                } else {
                    battle_target_candidate_mask &= battle_get_other_slot_bits(slot);
                }
            }
        }
        for (; count < 11; count++) {
            battle_target_candidates[count] = 0xFF;
        }
    } else if (selection & 0x4000) {
        battle_target_cursor_slot = member;
        battle_target_candidate_mask = battle_get_slot_bit(member);
        for (count = 0; count < 11; count++) {
            battle_target_candidates[count] = 0xFF;
        }
    }
    return selection;
}

/* 80085084: Select a target for selection word `target` (80084de4): each frame
 * highlight the current target (mode 0), all candidates (1) or those in the
 * current target's formation group (2); keys 0-3 move to the nearest
 * candidate that way, 4 confirms (1) and 5 cancels (0). The direction
 * arrows are refreshed whenever the key changes. Returns 0 when there is no
 * candidate. */
u8 battle_choose_target(u16 target, u8 member, s32 mode) {
    u8 lastKey = 0xFE;
    u8 state;
    u16 group;
    u8 next;
    s32 direction;

    battle_init_target_candidates(target, target, member, 0, mode);
    group = battle_target_candidate_mask;
    state = battle_target_cursor_slot == 0xFF;
    while (state == 0) {
        battle_wait_frame();
        switch (target & 0xF) {
        case 0:
            battle_highlight_slots(battle_get_slot_bit(battle_target_cursor_slot));
            battle_camera_start_move(battle_get_slot_bit(battle_target_cursor_slot));
            break;
        case 1:
            battle_highlight_slots(battle_target_candidate_mask);
            battle_camera_start_move(battle_target_candidate_mask);
            break;
        case 2:
            group = battle_get_candidates_in_target_group();
            battle_highlight_slots(group);
            battle_camera_start_move(group);
            break;
        }
        switch (battle_pressed_key) {
        case 5:
            state = 1;
            break;
        case 4:
            battle_target_candidate_mask = group;
            state = 2;
            break;
        case 0:
            next = battle_find_candidate_in_direction(battle_target_cursor_slot, 0);
            if (battle_is_slot_in_mask(battle_target_candidate_mask, next)) {
                battle_target_cursor_slot = next;
            }
            break;
        case 1:
            next = battle_find_candidate_in_direction(battle_target_cursor_slot, 1);
            if (battle_is_slot_in_mask(battle_target_candidate_mask, next)) {
                battle_target_cursor_slot = next;
            }
            break;
        case 2:
            next = battle_find_candidate_in_direction(battle_target_cursor_slot, 2);
            if (battle_is_slot_in_mask(battle_target_candidate_mask, next)) {
                battle_target_cursor_slot = next;
            }
            break;
        case 3:
            next = battle_find_candidate_in_direction(battle_target_cursor_slot, 3);
            if (battle_is_slot_in_mask(battle_target_candidate_mask, next)) {
                battle_target_cursor_slot = next;
            }
            break;
        case 6:
        case 7:
            break;
        }
        if (battle_pressed_key != lastKey) {
            lastKey = battle_pressed_key;
            for (direction = 0; direction < 4; direction++) {
                if (battle_find_candidate_in_direction(battle_target_cursor_slot, direction) != battle_target_cursor_slot) {
                    battle_direction_arrows->arrows[direction] = 1;
                } else {
                    battle_direction_arrows->arrows[direction] = 0;
                }
            }
        }
    }
    return state - 1;
}

/* 80085310: Whether slot b's slot-info +0xa is below slot a's. */
s32 battle_is_target_at_lower_x(u8 a, u8 b) {
    return (u16)battle_area_slots[a].x > (u16)battle_area_slots[b].x;
}

/* 80085350: Reset the running result accumulation of every slot. */
void battle_reset_running_results(void) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        battle_running_result_codes[slot] = 0xFF;
        battle_running_result_amounts[slot] = 0;
    }
}

/* 80085388: Clear the current event's per-slot results (the event index is re-read for
 * every store). */
void battle_clear_event_results(void) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        battle_area_events[battle_turn_state->eventCount].amounts[slot] = 0;
        battle_area_events[battle_turn_state->eventCount].codes[slot] = 0xFF;
        battle_area_events[battle_turn_state->eventCount].accumulated[slot] = 0;
        battle_area_events[battle_turn_state->eventCount].accumulatedCodes[slot] = 0xFF;
    }
}

/* 80085454: Copy the resolver's damage and result codes into event `queue` and
 * accumulate them into the running amount and code of each slot: damage
 * (codes 0 and 5) and healing (code 2) add up or cancel out, any other code
 * replaces the running result. */
void battle_accumulate_event_results(u8 queue) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        battle_area_events[queue].amounts[slot] = battle_slot_damages[slot];
        battle_area_events[queue].codes[slot] = battle_slot_result_codes[slot];
        switch (battle_slot_result_codes[slot]) {
        case 0:
        case 5:
            if (battle_running_result_codes[slot] == 0 || battle_running_result_codes[slot] == 5) {
                battle_running_result_amounts[slot] += battle_slot_damages[slot];
            } else if (battle_running_result_codes[slot] == 2) {
                if ((s16)battle_slot_damages[slot] - battle_running_result_amounts[slot] < 0) {
                    battle_running_result_amounts[slot] = battle_running_result_amounts[slot] - (s16)battle_slot_damages[slot];
                } else {
                    battle_running_result_amounts[slot] = (s16)battle_slot_damages[slot] - battle_running_result_amounts[slot];
                    battle_running_result_codes[slot] = 0;
                }
            } else {
                battle_running_result_amounts[slot] = battle_slot_damages[slot];
                battle_running_result_codes[slot] = battle_slot_result_codes[slot];
            }
            break;
        case 2:
            if (battle_running_result_codes[slot] == 2) {
                battle_running_result_amounts[slot] += battle_slot_damages[slot];
            } else if (battle_running_result_codes[slot] == 0 || battle_running_result_codes[slot] == 5) {
                if ((s16)battle_slot_damages[slot] - battle_running_result_amounts[slot] < 0) {
                    battle_running_result_amounts[slot] = battle_running_result_amounts[slot] - (s16)battle_slot_damages[slot];
                } else {
                    battle_running_result_amounts[slot] = (s16)battle_slot_damages[slot] - battle_running_result_amounts[slot];
                    battle_running_result_codes[slot] = 2;
                }
            } else {
                battle_running_result_amounts[slot] = battle_slot_damages[slot];
                battle_running_result_codes[slot] = battle_slot_result_codes[slot];
            }
            break;
        }
        battle_area_events[queue].accumulated[slot] = battle_running_result_amounts[slot];
        battle_area_events[queue].accumulatedCodes[slot] = battle_running_result_codes[slot];
    }
}

/* 80085618: Apply the results of event `queue` to every present slot: damage (codes
 * 0, 5, 7, 8) to HP or, in a gear, gear HP (knocking the slot out at 0),
 * healing (2) up to the maximum, EP loss (1, 9) and gain (3), fuel loss
 * (10) and gain (11). A knocked-out slot only joins the 800c48e8 mask.
 * Slots whose values changed get their refresh flag (+0x2eb). Pilot damage
 * treats both HP and amount as signed halfwords; healing wraps to the
 * stored width before the maximum check. The event queue, slot table and
 * knocked-out mask are members of battle_area (one symbol, so every base is
 * formed from the events address), while the healing stores use the
 * BATTLE_AREA view of the work table. The signed halfword locals (have and
 * amount, ep and cost) give the original's 0x58 frame and register copies.
 * Fuel gain stores the sum into the work table inside its maximum test, so
 * the store address is formed before the sum (loop.c then reduces the work
 * pointer before the amounts and fuel pointers, as the original's latch
 * shows), and sets the refresh flag itself; cross-jumping merges that store
 * with the shared one, but its extra slot reference ranks slot above the
 * amounts pointer in global allocation ($s2/$s3). */
void battle_apply_event_results(u8 queue) {
    s32 slot;
    s32 left;
    s16 have;
    s16 amount;
    s16 ep;
    s16 cost;
    u32 total;
    u16 value;

    for (slot = 0; slot < 11; slot++) {
        if (battle_turn_queue.present[slot] == 0) {
            continue;
        }
        if (battle_work_area.records[slot].pilot.status7C & 0x8000) {
            battle_area.knockedOut |= battle_get_slot_bit(slot);
            continue;
        }
        switch (battle_area.events[queue].codes[slot]) {
        case 0:
        case 5:
        case 7:
        case 8:
            if (battle_area.slots[slot].gear == 0) {
                have = battle_work_area.records[slot].pilot.hp;
                amount = battle_area.events[queue].amounts[slot];
                if (have - amount > 0) {
                    battle_work_area.records[slot].pilot.hp = have - amount;
                    break;
                }
                battle_work_area.records[slot].pilot.hp = 0;
                battle_area.knockedOut |= battle_get_slot_bit(slot);
                battle_work_area.records[slot].pilot.status7C |= 0x8000;
                if (slot >= 3) {
                    battle_leave_formation_group(slot);
                }
            } else {
                left = battle_work_area.records[slot].gear.hp - battle_area.events[queue].amounts[slot];
                if (left > 0) {
                    battle_work_area.records[slot].gear.hp = left;
                    break;
                }
                battle_work_area.records[slot].gear.hp = 0;
                battle_area.knockedOut |= battle_get_slot_bit(slot);
                battle_work_area.records[slot].gear.status7C |= 0x8000;
                battle_work_area.records[slot].pilot.status7C |= 0x8000;
                if (slot >= 3) {
                    battle_leave_formation_group(slot);
                }
            }
            break;
        case 2:
            if (battle_area.slots[slot].gear != 0 && battle_applying_item_results == 0) {
                total = battle_work_area.records[slot].gear.hp + battle_area.events[queue].amounts[slot];
                BATTLE_AREA.work.records[slot].gear.hp = total;
                if (battle_work_area.records[slot].gear.maxHp < total) {
                    battle_work_area.records[slot].gear.hp = battle_work_area.records[slot].gear.maxHp;
                }
            } else {
                value = battle_work_area.records[slot].pilot.hp + battle_area.events[queue].amounts[slot];
                BATTLE_AREA.work.records[slot].pilot.hp = value;
                if (battle_work_area.records[slot].pilot.maxHp < value) {
                    battle_work_area.records[slot].pilot.hp = battle_work_area.records[slot].pilot.maxHp;
                }
            }
            break;
        case 1:
        case 9:
            ep = battle_work_area.records[slot].pilot.ep;
            cost = battle_area.events[queue].amounts[slot];
            if (ep - cost > 0) {
                battle_work_area.records[slot].pilot.ep = ep - cost;
            } else {
                battle_work_area.records[slot].pilot.ep = 0;
            }
            continue;
        case 3:
            if (battle_area.slots[slot].gear != 0 && battle_applying_item_results == 0) {
                continue;
            }
            value = battle_work_area.records[slot].pilot.ep + battle_area.events[queue].amounts[slot];
            BATTLE_AREA.work.records[slot].pilot.ep = value;
            if (battle_work_area.records[slot].pilot.maxEp < value) {
                battle_work_area.records[slot].pilot.ep = battle_work_area.records[slot].pilot.maxEp;
            }
            continue;
        case 10:
            if (battle_work_area.records[slot].gear.fuel - battle_area.events[queue].amounts[slot] > 0) {
                battle_work_area.records[slot].gear.fuel -= battle_area.events[queue].amounts[slot];
            } else {
                battle_work_area.records[slot].gear.fuel = 0;
            }
            break;
        case 11:
            if (battle_work_area.records[slot].gear.maxFuel <
                (BATTLE_AREA.work.records[slot].gear.fuel =
                     battle_work_area.records[slot].gear.fuel + battle_area.events[queue].amounts[slot])) {
                battle_work_area.records[slot].gear.fuel = battle_work_area.records[slot].gear.maxFuel;
            }
            battle_turn_state->reaction[slot] = 1;
            continue;
        default:
            continue;
        }
        battle_turn_state->reaction[slot] = 1;
    }
}

/* 80085AC4: Revive slot at full HP and clear its timed statuses (the active halves of
 * the status words 0x7c-0x80 and 0x84-0x8c). */
void battle_revive_slot(slot)
u8 slot;
{
    s32 i;
    u16 *status;

    battle_work_area.records[slot].pilot.hp = battle_work_area.records[slot].pilot.maxHp;
    for (i = 2, status = &battle_work_area.records[slot].pilot.status80; i >= 0; i -= 2, status -= 2) {
        *status = 0;
    }
    for (i = 4, status = &battle_work_area.records[slot].pilot.status8C.half.active; i >= 0; i -= 2, status -= 2) {
        *status = 0;
    }
}

/* 80085B58: Apply up to three recovery amounts from 8009ada0 to `slot` as separate
 * events (codes 8..10) and show them. */
void battle_apply_status_drains(u8 slot) {
    s32 amounts[3];
    s32 i;

    amounts[2] = 0;
    amounts[1] = 0;
    amounts[0] = 0;
    if (battle_get_status_drain_amounts(slot, amounts) != 0) {
        for (i = 0; i < 3; i++) {
            if (amounts[i] != 0) {
                battle_turn_state->eventCount = 0;
                battle_clear_event_results();
                battle_area_events[0].codes[slot] = i + 8;
                battle_area_events[0].amounts[slot] = amounts[i];
                battle_apply_event_results(battle_turn_state->eventCount);
            }
        }
        battle_show_status_drain_amounts(slot, amounts[0], amounts[1], amounts[2]);
    }
}

/* 80085C48: Commit the targets for an item/effect and run 80098c6c with `param`. */
void battle_commit_item_targets(actor, targets, param)
u8 actor;
s16 targets;
u16 param;
{
    battle_area_knocked_out = 0;
    battle_committed_action.targets = targets;
    battle_committed_action.alive = battle_alive_mask;
    battle_resolve_item_effect(param);
}

/* 80085C88: Accumulate and apply event `queue`'s results. */
void battle_accumulate_and_apply_results(u8 queue) {
    battle_accumulate_event_results(queue);
    battle_apply_event_results(queue);
    battle_ui->unkAD = 0;
}

/* 80085CCC: Commit an action (attacker, target mask, animation) and resolve it. */
void battle_commit_action(u8 actor, u16 targets, u16 animation) {
    u8 action; /* 1-based */

    battle_area_knocked_out = 0;
    battle_committed_action.actor = actor;
    action = battle_turn_state->unk2DC;
    battle_committed_action.targets = targets;
    battle_committed_action.animation = animation;
    battle_committed_action.alive = battle_alive_mask;
    battle_committed_action.action = action - 1;
    battle_resolve_action();
}

/* 80085D34: Build the attack page's AP text (current AP, '/', maximum) as glyph
 * primitives and remember the draw buffer. */
void battle_attack_page_build_ap_glyphs(void) {
    battle_ui->unk7B = 0;
    battle_ui->unk7B +=
        battle_build_glyph(battle_turn_state->unk2D4[0] + 0xF, battle_graphics->unk9C8[battle_ui->unk7B], 0x2A, 0xD0);
    battle_ui->unk7B += battle_build_glyph(0x19, battle_graphics->unk9C8[battle_ui->unk7B], 0x32, 0xD0);
    battle_ui->unk7B +=
        battle_build_glyph(battle_turn_state->unk2D4[1] + 0xF, battle_graphics->unk9C8[battle_ui->unk7B], 0x3A, 0xD0);
    battle_ui->unkA4 = battle_drawing_state.buffer;
}

/* 80085E78: Reset the turn state's seven +0x2cc bytes to 0xff and clear +0x2d6. */
void battle_combo_reset_history(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        battle_turn_state->unk2CC[i] = 0xFF;
    }
    battle_turn_state->unk2D6 = 0;
}

/* 80085EB4: Mode 4: whether the combo input history (+0x2cc) matches one of the 13
 * combo patterns whose deathblow the member's character knows. */
u8 battle_is_known_deathblow_combo(u8 mode, u8 member) {
    u8 result = 0;
    s32 match;
    s32 combo;
    s32 i;

    if (battle_turn_state->unk2D6 != 0 && mode == 4) {
        for (combo = 0; combo < 13; combo++) {
            for (i = 0; i < 7; i++) {
                if (battle_turn_state->unk2CC[i] == battle_combo_patterns[combo][i]) {
                    match = 1;
                } else {
                    match = 0;
                    break;
                }
            }
            i = 0;
            if (match) {
                break;
            }
        }
        /* The deathblow index counts down from the last combo (the switch
         * reuses the match flag's variable). */
        match = combo;
        switch (match) {
        case 0:
            i++;
        case 1:
            i++;
        case 2:
            i++;
        case 3:
            i++;
        case 4:
            i++;
        case 5:
            i++;
        case 6:
            i++;
        case 7:
            i++;
        case 8:
            i++;
        case 9:
            i++;
        case 10:
            i++;
        case 11:
            i++;
        case 12:
            if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].counterSkills,
                              battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][12 - i])) {
                result = 1;
            }
            break;
        }
    }
    return result;
}

/* 80086028: Add entry `index` to list 11 (the combo chain display): render the
 * member's text `id` into the shared image (two entries per image cell),
 * upload it and place its quad after `column` + `offset` + 1 steps.
 * Returns the next index. */
s32 battle_combo_chain_add_name(member, index, column, id, pixels, offset)
u8 member;
s32 index;
s32 column;
u8 id;
u32 **pixels;
u8 offset;
{
    s32 cell;
    s32 odd;
    RECT rect;
    s32 width;

    cell = index / 2;
    odd = index % 2;
    battle_init_text_quad_pair(&battle_hud_primitive_lists->list11[index * 2], odd, 3);
    width = window_render_text_line(text_get_system_resource_entry(battle_party_character_ids[member], id), *pixels, 0x1B, odd);
    rect.x = cell * 30 + 0x3C0;
    rect.y = 0x1A;
    rect.w = 0x1E;
    rect.h = 13;
    LoadImage(&rect, (u_long *)*pixels);
    battle_quad_place_text_row(&battle_hud_primitive_lists->list11[index * 2 + battle_drawing_state.buffer], (column + 1 + offset) * 16 + 0x50 + index * 4,
                  0xC8 - index * 16, cell * 0x78, 0x1A, width);
    battle_hud_primitive_lists->counts[11]++;
    index++;
    return index;
}

/* 800861D0: Record attack input `code` in the combo history (+0x2cc, length +0x2d6)
 * and show it (list 12) with the deathblows it completes or leads into:
 * the combo it spells when its character knows it and the attack is
 * available, else (unless the chain is armed) the longer combos it
 * continues. With an armed chain a completed deathblow becomes the combo step
 * (+0x2dc). The three text image blocks live for one frame. */
void battle_combo_record_input(u8 code, u8 member) {
    s32 index = 0;
    s32 shown = 0;
    s32 block;
    s32 combo;
    s32 match;
    s32 i;
    u8 id;

    /* The text-image slots are walked by byte offset. */
    for (block = 0; block < (s32)(3 * sizeof(u32 *)); block += sizeof(u32 *)) {
        *((u32 **)((u8 *)battle_combo_text_image_blocks + block)) = (u32 *)battle_heap_alloc_text_image(0x1E);
    }
    battle_turn_state->unk2CC[battle_turn_state->unk2D6] = code - 4;
    battle_turn_state->unk2D6++;
    if (battle_turn_state->unk2E1[3] != 0) {
        battle_turn_state->unk2CC[battle_turn_state->unk2D6 - 1] = 0xFF;
    }
    for (combo = 0; combo < 13; combo++) {
        for (i = 0; i < 7; i++) {
            if (battle_turn_state->unk2CC[i] == battle_combo_patterns[combo][i]) {
                match = 1;
            } else {
                match = 0;
                break;
            }
        }
        if (match) {
            break;
        }
    }
    /* From here the match flag's variable holds the combo. */
    match = combo;
    if (battle_turn_state->unk2E1[3] != 0) {
        battle_turn_state->unk2CC[battle_turn_state->unk2D6 - 1] = code - 4;
    }
    battle_hud_primitive_lists->counts[12] = 0;
    battle_hud_primitive_lists->counts[11] = 0;
    for (combo = 0; combo < battle_turn_state->unk2D6; combo++) {
        switch (battle_turn_state->unk2CC[combo]) {
        case 0:
            id = 0x5D;
            break;
        case 2:
            id = 0x5E;
            break;
        case 3:
            id = 0x5F;
            break;
        }
        battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(id, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x50 + combo * 16, 0xD0 - index * 16);
    }
    /* The deathblow index counts down from the last combo. */
    i = 0;
    switch (match) {
    case 0:
        i++;
    case 1:
        i++;
    case 2:
        i++;
    case 3:
        i++;
    case 4:
        i++;
    case 5:
        i++;
    case 6:
        i++;
    case 7:
        i++;
    case 8:
        i++;
    case 9:
        i++;
    case 10:
        i++;
    case 11:
        i++;
    case 12:
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].counterSkills, battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][12 - i]) &&
            battle_turn_state->slots[member].items[2] == 0) {
            if (battle_turn_state->unk2E1[3] == 0) {
                battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(7, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x50 + combo * 16, 0xD0 - index * 16);
            } else {
                combo--;
            }
            index = battle_combo_chain_add_name(member, index, combo, battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][12 - i], &battle_combo_text_image_blocks[index / 2], 0);
            shown = 1;
        }
        break;
    }
    if (battle_turn_state->unk2E1[3] != 0 && shown) {
        battle_hud_primitive_lists->buffers[12] = battle_drawing_state.buffer;
        battle_turn_state->unk2DC = battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][12 - i] + 8;
        battle_hud_primitive_lists->buffers[11] = battle_drawing_state.buffer;
        battle_ui->unkA8 = 1;
        battle_wait_frame();
        for (block = 0; block < (s32)(3 * sizeof(u32 *)); block += sizeof(u32 *)) {
            heap_free(*((u32 **)((u8 *)battle_combo_text_image_blocks + block)));
        }
        return;
    }
    /* The second, longer deathblow (index 13-18, or 19 after combo 8). */
    i = 0;
    switch (match) {
    case 0:
        i++;
    case 1:
        i++;
    case 2:
        i++;
    case 3:
        i++;
    case 4:
        i++;
    case 5:
        i++;
    case 8:
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].counterSkills, battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][19 - i]) &&
            battle_turn_state->slots[member].items[0] == 0 && battle_turn_state->slots[member].items[2] == 0) {
            battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(8, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x54 + combo * 16, 0xD0 - index * 16);
            battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(7, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x54 + (combo + 1) * 16, 0xD0 - index * 16);
            index = battle_combo_chain_add_name(member, index, combo, battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][19 - i], &battle_combo_text_image_blocks[index / 2], 1);
        }
        break;
    case 6:
    case 7:
        break;
    }
    /* The third deathblow (index 20-22) after combos 0, 1 and 3. */
    i = 0;
    switch (match) {
    case 0:
        i++;
    case 1:
        i++;
    case 3:
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].counterSkills, battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][22 - i]) &&
            battle_turn_state->slots[member].items[1] == 0 && battle_turn_state->slots[member].items[2] == 0) {
            battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(9, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x58 + combo * 16, 0xD0 - index * 16);
            battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(7, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x58 + (combo + 1) * 16, 0xD0 - index * 16);
            battle_combo_chain_add_name(member, index, combo, battle_combo_deathblows_by_character[battle_work_area.records[member].pilot.characterId][22 - i], &battle_combo_text_image_blocks[index / 2], 1);
        }
        break;
    }
    battle_hud_primitive_lists->buffers[12] = battle_drawing_state.buffer;
    battle_hud_primitive_lists->buffers[11] = battle_drawing_state.buffer;
    battle_ui->unkA8 = 1;
    battle_wait_frame();
    for (block = 0; block < (s32)(3 * sizeof(u32 *)); block += sizeof(u32 *)) {
        heap_free(*((u32 **)((u8 *)battle_combo_text_image_blocks + block)));
    }
}

/* 80086B88: Whether the member can use combo step `step` now: without a combo chain
 * (+0x2d6) always; otherwise its character must know the combo flag
 * (800c34cc) and the chain must allow another step. 800d2c34 is the gear
 * HUD's level byte of the battle work area (the original addresses it through
 * 800ccce8). */
s32 battle_can_use_combo_step(s32 step, u8 member) {
    u8 index = step + (battle_turn_state->unk2CC[0] + 1) * 3;
    s32 result = 1;

    if (battle_turn_state->unk2D6 != 0) {
        if (battle_work_area.gearHud.level == 4) {
            index = step + 12;
        }
        if (!battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksA, battle_combo_step_flags[index])) {
            result = 0;
        } else if (battle_turn_state->unk2CC[0] != 0xFF && battle_work_area.gearHud.level != 4 &&
                   battle_work_area.gearHud.level < battle_turn_state->unk2CC[0] + 1) {
            result = 0;
        }
    }
    return result;
}

/* Place entry `index`'s fuel cost quad (`count` digits) in list 13. */
#define PLACE_FUEL_COST(index, column, count)                                                                     \
    do {                                                                                                          \
        battle_quad_place_text_row(&battle_hud_primitive_lists->list13[(index) * 2 + battle_drawing_state.buffer], (index) * 4 + ((column) + 1) * 16 + 0xEA, \
                      0xC8 - (index) * 16, (index) * 32 + 0x78, 0, (count) * 8);                                  \
    } while (0)

/* 80086C88: Add entry `index` to lists 11 and 13 (the gear's combo chain display):
 * render the gear's text for combo step `step` into the shared image (two
 * entries per image cell) and place its quad after `column` + 1 steps, then
 * upload the step's fuel cost digits and place their quad. Returns the next
 * index. The fuel cost is read through the draw state (800ccb34 + 0x60d8):
 * the draw state and the work area form one aggregate. The digit rectangle
 * is filled with setRECT (stored from the frame base, apart from the call's
 * rectangle address); the cost quad is placed by a statement macro (its
 * loop block weights the index for register allocation as in the
 * original). */
s32 battle_combo_chain_add_gear_step(u8 member, s32 index, s32 column, u8 step, u32 **pixels) {
    RECT rect;
    RECT digits[4];
    s32 cell;
    s32 odd;
    s32 width;
    s32 count;
    s32 i;
    u8 *fuel;
    s16 x;

    count = 0;
    fuel = &battle_decimal_digits[5];
    cell = index / 2;
    odd = index % 2;
    battle_init_text_quad_pair(&battle_hud_primitive_lists->list11[index * 2], odd, 3);
    width = window_render_text_line(text_get_gear_resource_entry(battle_work_area.records[member].pilot.gearId, battle_combo_step_flags[step]), *pixels, 0x1B, odd);
    rect.x = cell * 30 + 0x3C0;
    rect.y = 0x1A;
    rect.w = 0x1E;
    rect.h = 0xD;
    LoadImage(&rect, (u_long *)*pixels);
    battle_quad_place_text_row(&battle_hud_primitive_lists->list11[index * 2 + battle_drawing_state.buffer], index * 4 + (column + 1) * 16 + 0x86,
                  0xC8 - index * 16, cell * 0x78, 0x1A, width);
    battle_init_text_quad_pair(&battle_hud_primitive_lists->list13[index * 2], 0, 3);
    battle_split_decimal_digits(battle_drawing_state.work.gearHud.commands[step]);
    x = index * 8 + 0x3DE;
    for (i = 0; i < 4; i++) {
        if (fuel[i] != 0xFF) {
            setRECT(&digits[count], x, 0, 6, 0xD);
            battle_upload_image_and_wait(&digits[count], battle_digit_text_images[fuel[i]].pixels);
            x += 2;
            count++;
        }
    }
    PLACE_FUEL_COST(index, column, count);
    battle_hud_primitive_lists->counts[11]++;
    battle_hud_primitive_lists->counts[13]++;
    return index + 1;
}

/* 80086F98: Record gear combo step `step` for the member: unless the target fights in
 * a gear (record +0x15a bit 0x80) or the attack level is 4, the step's flag
 * becomes the combo step (+0x2dc) at once. Otherwise, when the attack level
 * allows the step (or +0x2e7 is set), add it to the combo history (+0x2cc,
 * length +0x2d6), show the history (list 12) and the deathblows it leads to
 * (80086c88, lists 11 and 13) and, with an armed chain (+0x2e4), take a
 * completed deathblow as the combo step. Returns whether a deathblow was
 * shown (0 with an armed chain), else 1. */
u8 battle_combo_record_gear_step(u8 step, u8 member) {
    s32 index;
    s32 i;
    s32 block;
    s32 shown;
    u8 flag;
    u8 id;
    u8 next;

    flag = (step + 1) * 3;
    index = 0;
    shown = 0;
    if (!(battle_work_area.records[battle_turn_state->slots[member].defaultTarget].flags15A & 0x80) && battle_work_area.gearHud.level != 4) {
        battle_turn_state->unk2DC = battle_combo_step_flags[step];
        return 1;
    }
    if ((battle_work_area.gearHud.level != 0 && battle_work_area.gearHud.level - 1 >= step) || battle_turn_state->unk2E7 != 0) {
        /* The text-image slots are walked by byte offset. */
        for (block = 0; block < (s32)(3 * sizeof(u32 *)); block += sizeof(u32 *)) {
            *((u32 **)((u8 *)battle_combo_text_image_blocks + block)) = (u32 *)battle_heap_alloc_text_image(0x1E);
        }
        battle_turn_state->unk2CC[battle_turn_state->unk2D6] = step;
        battle_turn_state->unk2D6++;
        if (battle_turn_state->unk2E1[3] != 0) {
            battle_turn_state->unk2CC[battle_turn_state->unk2D6 - 1] = 0xFF;
            battle_turn_state->unk2CC[battle_turn_state->unk2D6 - 1] = step;
            flag = step + (battle_turn_state->unk2CC[0] + 1) * 3;
        }
        battle_hud_primitive_lists->counts[12] = 0;
        battle_hud_primitive_lists->counts[11] = 0;
        battle_hud_primitive_lists->counts[13] = 0;
        for (i = 0; i < battle_turn_state->unk2D6; i++) {
            if (battle_turn_state->unk2CC[i] != 0xFF) {
                switch (battle_turn_state->unk2CC[i]) {
                case 0:
                    id = 0x5E;
                    break;
                case 1:
                    id = 0x5F;
                    break;
                case 2:
                    id = 0x5D;
                    break;
                }
                battle_hud_primitive_lists->counts[12] +=
                    battle_build_glyph(id, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x80 + i * 16, 0xD0 - index * 16);
            }
        }
        if (battle_work_area.gearHud.level == 4) {
            if (step == 0xFF) {
                flag = 12;
            } else {
                flag = step + 12;
            }
        }
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksA, battle_combo_step_flags[flag])) {
            if (battle_turn_state->unk2E1[3] == 0) {
                if (battle_turn_state->slots[member].items[0] == 0) {
                    battle_hud_primitive_lists->counts[12] +=
                        battle_build_glyph(8, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x80 + i * 16, 0xD0 - index * 16);
                    index = battle_combo_chain_add_gear_step(member, index, i - 1, flag, &battle_combo_text_image_blocks[index / 2]);
                }
                shown = 1;
            } else {
                index = battle_combo_chain_add_gear_step(member, index, i - 1, flag, &battle_combo_text_image_blocks[index / 2]);
                shown = 1;
            }
        }
        if (battle_turn_state->unk2E1[3] != 0 && shown) {
            shown = 0;
            if (battle_turn_state->unk2CC[0] == 0xFF) {
                flag = step + 12;
            }
            battle_hud_primitive_lists->buffers[12] = battle_drawing_state.buffer;
            battle_turn_state->unk2DC = battle_combo_step_flags[flag];
            battle_hud_primitive_lists->buffers[11] = battle_drawing_state.buffer;
            battle_hud_primitive_lists->buffers[13] = battle_drawing_state.buffer;
            battle_ui->unkA8 = 1;
            battle_wait_frame();
            for (block = 0; block < (s32)(3 * sizeof(u32 *)); block += sizeof(u32 *)) {
                heap_free(*((u32 **)((u8 *)battle_combo_text_image_blocks + block)));
            }
            goto done;
        }
        battle_turn_state->unk2DC = step;
        if (battle_work_area.gearHud.level != 4) {
            next = (step + 1) * 3 + 1;
        } else {
            next = 13;
        }
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksA, battle_combo_step_flags[next]) &&
            battle_turn_state->slots[member].items[1] == 0) {
            battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(9, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x84 + i * 16, 0xD0 - index * 16);
            index = battle_combo_chain_add_gear_step(member, index, i, next, &battle_combo_text_image_blocks[index / 2]);
            shown = 1;
        }
        if (battle_work_area.gearHud.level != 4) {
            next = (step + 1) * 3 + 2;
        } else {
            next = 14;
        }
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksA, battle_combo_step_flags[next]) &&
            battle_turn_state->slots[member].items[2] == 0) {
            battle_hud_primitive_lists->counts[12] +=
                battle_build_glyph(7, &battle_hud_primitive_lists->list12[battle_hud_primitive_lists->counts[12] * 2], 0x88 + i * 16, 0xD0 - index * 16);
            battle_combo_chain_add_gear_step(member, index, i, next, &battle_combo_text_image_blocks[index / 2]);
            shown = 1;
        }
        battle_hud_primitive_lists->buffers[12] = battle_drawing_state.buffer;
        battle_hud_primitive_lists->buffers[11] = battle_drawing_state.buffer;
        battle_hud_primitive_lists->buffers[13] = battle_drawing_state.buffer;
        battle_ui->unkA8 = 1;
        battle_wait_frame();
        for (block = 0; block < (s32)(3 * sizeof(u32 *)); block += sizeof(u32 *)) {
            heap_free(*((u32 **)((u8 *)battle_combo_text_image_blocks + block)));
        }
    } else {
        battle_turn_state->unk2DC = step;
        shown = 1;
    }
done:
    return shown;
}

/* A formation group's row of links: rows are 0x40 bytes, addressed from the
 * formation start, so the links (at +0x140) begin 40 entries in. */
#define LINK_ROW(formation, group) ((GroupLink *)((u8 *)(formation) + ((group) << 6)))

/* 800877E0: Plan the approach route from `actor` to `target`: the actor's position,
 * then the route points the formation lists between their groups. Returns
 * 1 when the actor's character is 4. The formation is read once per point
 * and the link row is taken anew for each read. */
s32 battle_plan_approach_route(u8 actor, u8 target) {
    s32 result = 0;
    s32 i;
    GroupLink *row;
    Formation *formation;

    for (i = 0; i < 9; i++) {
        battle_area.path[i].x = 0xFFFF;
        battle_area.path[i].z = 0xFFFF;
    }
    battle_area.path[0].x = battle_area.slots[actor].x;
    battle_area.path[0].z = battle_area.slots[actor].z;
    battle_area.path[0].run = 0;
    for (i = 1; i < 8; i++) {
        formation = battle_formation;
        row = LINK_ROW(formation, battle_area.slots[actor].group);
        if (row[battle_area.slots[target].group + 40].points[i - 1] == 0xFF) {
            break;
        }
        row = LINK_ROW(formation, battle_area.slots[actor].group);
        battle_area.path[i].x = formation->areas[row[battle_area.slots[target].group + 40].points[i - 1] & 7].centre.x;
        row = LINK_ROW(formation, battle_area.slots[actor].group);
        battle_area.path[i].z = formation->areas[row[battle_area.slots[target].group + 40].points[i - 1] & 7].centre.z;
        row = LINK_ROW(formation, battle_area.slots[actor].group);
        battle_area.path[i].run = row[battle_area.slots[target].group + 40].points[i - 1] & 0x80;
    }
    if (battle_party_character_ids[actor] == 4) {
        result = 1;
    }
    return result;
}

/* 800879A8: Reset every event to type 0xff for `actor` against `target`'s bit. */
void battle_reset_events_for_target(u8 actor, u8 target) {
    s32 i;

    for (i = 0; i < 32; i++) {
        battle_area_events[i].type = 0xFF;
        battle_area_events[i].actor = actor;
        battle_area_events[i].targetMask = battle_get_slot_bit(target);
    }
}

/* 80087A38: Enter the attack page: AP text, events against the default target, and
 * the member's attack model. */
void battle_attack_page_enter(u8 member) {
    s32 route;

    battle_attack_page_build_ap_glyphs();
    battle_reset_events_for_target(member, battle_turn_state->slots[member].defaultTarget);
    battle_command_menu_sounds_enabled = 0;
    route = battle_plan_approach_route(member, battle_turn_state->slots[member].defaultTarget);
    battle_menu_open_turn(route, member, battle_turn_state->slots[member].defaultTarget, battle_find_next_turn_slot(member));
    battle_command_menu_sounds_enabled = 1;
    battle_attack_approach_done = 0;
}

/* 80087AF0: Execute the member's attack step (paying `cost` AP, 1-3): once per
 * turn move into the target's group (unless the character is 4; flagged
 * slots use 800881b8), reset the events, advance the combo step through the
 * step table (a step from 8 is a deathblow the character must know, else
 * step 7), queue an event 0xf3 when the target reacts while down, commit
 * the step against the target, queue its event, let the enemy remember the
 * attack and run its reaction script, and apply the results. Returns
 * whether the combo completed a known deathblow or the reaction ran action
 * 0x62. */
u8 battle_execute_attack_step(u8 member, u8 cost) {
    u8 queue;
    u8 reacted = 0;
    s32 payment;

    if (battle_attack_approach_done == 0) {
        if (battle_slot_flags[member].unk1 == 0) {
            if (battle_party_character_ids[member] != 4) {
                battle_join_target_group(member, battle_turn_state->slots[member].defaultTarget);
            }
        } else {
            battle_join_empty_target_group(member, battle_turn_state->slots[member].defaultTarget);
        }
        battle_attack_approach_done = 1;
    }
    battle_clear_event_results();
    if (battle_slot_flags[member].unk1 == 0) {
        if (battle_turn_state->unk2DC < 8) {
            payment = cost;
            battle_turn_state->unk2DC = battle_combo_next_step_table_by_paid[battle_turn_state->unk2DC][payment];
        } else if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].counterSkills, battle_turn_state->unk2DC - 8)) {
            reacted = 1;
        } else {
            battle_turn_state->unk2DC = 7;
        }
    } else {
        battle_turn_state->unk2DC++;
    }
    if (battle_work_area.records[battle_turn_state->slots[member].defaultTarget].pilot.flags34 & 0x800) {
        battle_area_events[battle_turn_state->eventCount].actor = member;
        battle_area_events[battle_turn_state->eventCount].type = 0xF3;
        battle_area_events[battle_turn_state->eventCount].parameter = battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget);
        battle_turn_state->eventCount++;
    }
    battle_commit_action(member, battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget), battle_turn_state->unk2DC - 1);
    queue = battle_turn_state->eventCount;
    battle_area_events[battle_turn_state->eventCount].actor = member;
    battle_area_events[battle_turn_state->eventCount].targetMask = battle_committed_action.targets;
    battle_area_events[battle_turn_state->eventCount].type = battle_committed_action.animation;
    battle_turn_state->eventCount++;
    battle_ai_tell_target_about_actor(member, battle_turn_state->slots[member].defaultTarget);
    if (battle_turn_state->slots[member].defaultTarget >= 3 && (u8)battle_ai_run_reaction_script(battle_turn_state->slots[member].defaultTarget)) {
        reacted = 1;
    }
    battle_accumulate_and_apply_results(queue);
    return reacted;
}

/* 80087EDC: Move `actor` into `target`'s formation group when it is another group
 * with room (under four members): leave the old group, take the first free
 * member place and stand at that place of the group's area. The entries
 * are offset by the actor's side (8 for an enemy, as in 800883AC) only when
 * the target is a party member. fold turns `side * (target < 3)` into
 * `target < 3 ? side * 1 : 0` and keeps `side * 1` as a non-lvalue of the
 * promoted u8, which the narrowing into `base` cannot strip: the arm
 * expands to a zero_extend and a subreg, so no jump pass before reload can
 * hoist the else or make a store-flag mask of it. combine leaves a copy,
 * jump2 hoists the zero above the branch and reorg fills the delay slot
 * with it. A ternary or if/else over `side`, or an s32 `side`, leaves a
 * one-insn arm that jump1 folds into `side & -(target < 3)`. */
void battle_join_target_group(u8 actor, u8 target) {
    u8 base;
    u8 side;
    s32 member;

    if (battle_area_slots[actor].group != battle_area_slots[target].group) {
        side = (actor >= 3) * 8;
        base = side * (target < 3);
        if (battle_formation_groups[battle_area_slots[target].group + base].count < 4) {
            battle_leave_formation_group(actor);
            battle_formation_groups[battle_area_slots[target].group + base].count++;
            for (member = 0; member < 4; member++) {
                if (battle_is_slot_in_mask(battle_formation_groups[battle_area_slots[target].group + base].members, member) == 0) {
                    break;
                }
            }
            battle_area_slots[actor].group = battle_area_slots[target].group;
            battle_area_slots[actor].member = member;
            battle_formation_groups[battle_area_slots[actor].group + base].members |= battle_get_slot_bit(battle_area_slots[actor].member);
            if (actor < 3) {
                battle_area_slots[actor].x = battle_formation->areas[battle_area_slots[actor].group].party[battle_area_slots[actor].member].x;
                battle_area_slots[actor].z = battle_formation->areas[battle_area_slots[actor].group].party[battle_area_slots[actor].member].z;
            } else {
                battle_area_slots[actor].x = battle_formation->areas[battle_area_slots[actor].group].enemies[battle_area_slots[actor].member].x;
                battle_area_slots[actor].z = battle_formation->areas[battle_area_slots[actor].group].enemies[battle_area_slots[actor].member].z;
            }
        }
    }
}

/* 800881B8: Move `actor` alone into `target`'s formation group when that group is
 * another one and empty (entries from 0x10, or 0x18 for an enemy joining a
 * party member): it becomes the only member, at the group's position.
 * The slots are read as members of the battle area (battle_area+4), which
 * keeps the group reload below the count store. */
void battle_join_empty_target_group(u8 actor, u8 target) {
    u8 base;
    s32 party;

    if (battle_area.slots[actor].group != battle_area.slots[target].group) {
        party = actor < 3;
        if (target < 3) {
            base = party ? 0x10 : 0x18;
        } else {
            base = 0x10;
        }
        if (battle_formation_groups[battle_area.slots[target].group + base].count == 0) {
            battle_leave_formation_group(actor);
            battle_area.slots[actor].group = battle_area.slots[target].group;
            battle_area.slots[actor].member = 0;
            battle_formation_groups[battle_area.slots[actor].group + base].count = 1;
            battle_formation_groups[battle_area.slots[actor].group + base].members = 1;
            if (actor < 3) {
                battle_area.slots[actor].x = battle_formation->positions[battle_area.slots[actor].group].x;
                battle_area.slots[actor].z = battle_formation->positions[battle_area.slots[actor].group].z;
            } else {
                battle_area.slots[actor].x = battle_formation->positions[battle_area.slots[actor].group].enemyX;
                battle_area.slots[actor].z = battle_formation->positions[battle_area.slots[actor].group].enemyZ;
            }
        }
    }
}

/* 800883AC: Drop a slot from its formation group (enemy entries from 8, flagged slots
 * add 0x10). */
void battle_leave_formation_group(u8 slot) {
    u8 base = (slot >= 3) * 8;

    if (battle_slot_flags[slot].unk1 != 0) {
        base |= 0x10;
    }
    battle_formation_groups[battle_area_slots[slot].group + base].count--;
    battle_formation_groups[battle_area_slots[slot].group + base].members &= battle_get_other_slot_bits(battle_area_slots[slot].member);
}

/* 80088490: Give slot a formation group of its own: keep its group when that is empty,
 * else take the first empty one; it becomes the group's only member and is
 * placed at the group's position. */
void battle_give_slot_own_group(slot)
u8 slot;
{
    u8 group;
    s32 i;

    if (battle_formation_groups[battle_area_slots[slot].group + 16].count == 0) {
        group = battle_area_slots[slot].group;
    } else {
        for (i = 0; i < 8; i++) {
            if (battle_formation_groups[16 + i].count == 0) {
                group = i;
                break;
            }
        }
    }
    battle_area_slots[slot].group = group;
    battle_area_slots[slot].member = 0;
    battle_formation_groups[battle_area_slots[slot].group + 16].members = 1;
    battle_formation_groups[battle_area_slots[slot].group + 16].count = 1;
    battle_area_slots[slot].x = battle_formation->positions[battle_area_slots[slot].group].x;
    battle_area_slots[slot].z = battle_formation->positions[battle_area_slots[slot].group].z;
}

/* 800885D0: The member count of the slot's group among the flagged enemy groups. */
u8 battle_count_enemy_gear_group_members(u8 slot) {
    return battle_formation_groups[battle_area_slots[slot].group + 0x18].count;
}

/* 8008860C: Build the glyph lists at +0x1720 (glyphs 800c33b0[0..1]), +0 (glyph 0xa8)
 * and +0x2530 (800c33b0[2..3]) at (0xa0, 0x64) and initialise the current
 * buffer's quads. The first two keep their count and buffer at +0x5d74 /
 * +0x5d83 and +0x5d70 / +0x5d92. */
void battle_gear_hud_build_fixed_glyphs(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        battle_hud_primitive_lists->extraCounts[4] +=
            battle_build_glyph(battle_gear_hud_fixed_glyph_ids[i], &battle_hud_primitive_lists->extra4[battle_hud_primitive_lists->extraCounts[4] * 2], 0xA0, 0x64);
    }
    battle_hud_primitive_lists->extraBuffer4 = battle_drawing_state.buffer;
    battle_hud_primitive_lists->extraCounts[0] = battle_build_glyph(0xA8, &battle_hud_primitive_lists->extra0[battle_hud_primitive_lists->extraCounts[0] * 2], 0xA0, 0x64);
    battle_hud_primitive_lists->extraBuffers[0] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->extraCounts[4]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->extra4[i * 2 + battle_hud_primitive_lists->extraBuffer4]);
    }
    for (i = 0; i < battle_hud_primitive_lists->extraCounts[0]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->extra0[i * 2 + battle_hud_primitive_lists->extraBuffers[0]]);
    }
    for (i = 2; i < 4; i++) {
        battle_hud_primitive_lists->counts[9] +=
            battle_build_glyph(battle_gear_hud_fixed_glyph_ids[i], &battle_hud_primitive_lists->list9[battle_hud_primitive_lists->counts[9] * 2], 0xA0, 0x64);
    }
    battle_hud_primitive_lists->buffers[9] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->counts[9]; i++) {
        battle_quad_init_full_subtractive(&battle_hud_primitive_lists->list9[i * 2 + battle_hud_primitive_lists->buffers[9]]);
    }
}

/* 8008887C: Set up a stepped line from (x0, y0) to (x1, y1): directions, the 8.8 steps
 * of the minor axis and a random speed 1..8. */
void battle_stepped_line_start(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 dx;
    s32 dy;

    battle_stepped_line_start_x = x0;
    battle_stepped_line_start_y = y0;
    battle_stepped_line_end_x = x1;
    battle_stepped_line_end_y = y1;
    if (x1 != x0 && y1 != y0) {
        if (x1 < x0) {
            battle_stepped_line_x_decreasing = 1;
            dx = x0 - x1;
        } else {
            dx = x1 - x0;
            battle_stepped_line_x_decreasing = 0;
        }
        if (y1 < y0) {
            battle_stepped_line_y_decreasing = 1;
            dy = y0 - y1;
        } else {
            dy = y1 - y0;
            battle_stepped_line_y_decreasing = 0;
        }
        if (dx >= dy) {
            battle_stepped_line_step_x = 0x100;
            battle_stepped_line_step_y = (dy << 8) / dx;
        } else {
            battle_stepped_line_step_y = 0x100;
            battle_stepped_line_step_x = (dx << 8) / dy;
        }
        battle_stepped_line_progress_x = 0;
        battle_stepped_line_progress_y = 0;
        battle_stepped_line_speed = mode_get_random_byte_in_range(1, 8);
        battle_stepped_line_ended = 0;
    }
}

/* 80088990: Advance the stepped line by its speed and flag its end (800c207c) once
 * the major axis passes the end point. */
void battle_stepped_line_advance(void) {
    s32 i;

    for (i = 0; i < battle_stepped_line_speed; i++) {
        if (battle_stepped_line_x_decreasing) {
            battle_stepped_line_progress_x -= battle_stepped_line_step_x;
        } else {
            battle_stepped_line_progress_x += battle_stepped_line_step_x;
        }
        if (battle_stepped_line_y_decreasing) {
            battle_stepped_line_progress_y -= battle_stepped_line_step_y;
        } else {
            battle_stepped_line_progress_y += battle_stepped_line_step_y;
        }
    }
    if (battle_stepped_line_step_x == 0x100) {
        if (battle_stepped_line_x_decreasing) {
            if (battle_stepped_line_progress_x / 256 + battle_stepped_line_start_x < battle_stepped_line_end_x) {
                battle_stepped_line_ended = 1;
            }
        } else if (battle_stepped_line_progress_x / 256 + battle_stepped_line_start_x > battle_stepped_line_end_x) {
            battle_stepped_line_ended = 1;
        }
    } else if (battle_stepped_line_y_decreasing) {
        if (battle_stepped_line_progress_y / 256 + battle_stepped_line_start_y < battle_stepped_line_end_y) {
            battle_stepped_line_ended = 1;
        }
    } else if (battle_stepped_line_progress_y / 256 + battle_stepped_line_start_y > battle_stepped_line_end_y) {
        battle_stepped_line_ended = 1;
    }
}

/* 80088B80: Draw the stepped line effect while UI +0xad is set: once a line ends,
 * start another towards a random end point (a 4 in 100 chance per frame);
 * otherwise advance it. Place its glyphs at the current point (lists
 * extra1-extra3 and +0x32a0) and, while the line runs, twenty random
 * glyphs (+0x46a0) in two rows. */
void battle_stepped_line_draw(void) {
    s32 i;
    s32 j;
    s32 n;

    if (battle_ui->unkAD != 0) {
        if (battle_stepped_line_ended != 0 && mode_get_random_byte_in_range(0, 99) >= 0x60) {
            battle_stepped_line_start(battle_hud_primitive_lists->lineX, battle_hud_primitive_lists->lineY, battle_stepped_line_end_points[0][mode_get_random_byte_in_range(0, 4)],
                          battle_stepped_line_end_points[1][mode_get_random_byte_in_range(0, 4)]);
        }
        if (battle_stepped_line_ended == 0) {
            battle_stepped_line_advance();
            battle_hud_primitive_lists->lineX = battle_stepped_line_start_x + battle_stepped_line_progress_x / 256;
            battle_hud_primitive_lists->lineY = battle_stepped_line_start_y + battle_stepped_line_progress_y / 256;
        }
        battle_hud_primitive_lists->extraCounts[2] = battle_build_glyph(0xB9, battle_hud_primitive_lists->extra2, battle_hud_primitive_lists->lineX, battle_hud_primitive_lists->lineY);
        battle_hud_primitive_lists->extraBuffers[2] = battle_drawing_state.buffer;
        battle_hud_primitive_lists->extraCounts[1] = battle_build_glyph((battle_hud_primitive_lists->lineX & 0xF) + 0xA9, battle_hud_primitive_lists->extra1, 0xA0, 0x64);
        battle_hud_primitive_lists->extraBuffers[1] = battle_drawing_state.buffer;
        battle_hud_primitive_lists->extraCounts[3] = battle_build_glyph(0x82, battle_hud_primitive_lists->extra3, battle_hud_primitive_lists->lineX, battle_hud_primitive_lists->lineY);
        battle_hud_primitive_lists->extraBuffers[3] = battle_drawing_state.buffer;
        for (i = 0; i < battle_hud_primitive_lists->extraCounts[2]; i++) {
            battle_quad_init_full_additive(&battle_hud_primitive_lists->extra2[i * 2 + battle_hud_primitive_lists->extraBuffers[2]]);
        }
        for (i = 0; i < battle_hud_primitive_lists->extraCounts[3]; i++) {
            battle_quad_init_full_additive(&battle_hud_primitive_lists->extra3[i * 2 + battle_hud_primitive_lists->extraBuffers[3]]);
        }
        for (i = 0; i < battle_hud_primitive_lists->extraCounts[1]; i++) {
            battle_quad_init_full_additive(&battle_hud_primitive_lists->extra1[i * 2 + battle_hud_primitive_lists->extraBuffers[1]]);
        }
        battle_hud_primitive_lists->count32A0 = battle_build_glyph((battle_hud_primitive_lists->lineY & 0xF) + 0xC9, battle_hud_primitive_lists->unk32A0,
                                              battle_hud_primitive_lists->lineX, battle_hud_primitive_lists->lineY);
        battle_hud_primitive_lists->buffer32A0 = battle_drawing_state.buffer;
        for (i = 0; i < battle_hud_primitive_lists->count32A0; i++) {
            battle_quad_init_full_subtractive(&battle_hud_primitive_lists->unk32A0[i * 2 + battle_hud_primitive_lists->buffer32A0]);
        }
        if (battle_stepped_line_ended == 0) {
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 10; j++) {
                    n = i * 10 + j;
                    battle_build_glyph(mode_get_random_byte_in_range(0, 9) + 0xBA, &battle_hud_primitive_lists->unk46A0[n * 2], 0x82 + j * 6, 0xA + i * 0xBD);
                    battle_quad_init_full_additive(&battle_hud_primitive_lists->unk46A0[n * 2 + battle_drawing_state.buffer]);
                }
            }
            battle_hud_primitive_lists->buffer46A0 = battle_drawing_state.buffer;
        }
    }
}

/* 80089038: Build the glyphs of the flags set in 800d2c30 (up to five, 10 pixels
 * apart from y 0x6e) into the +0x4ce0 primitives. */
void battle_gear_hud_build_warning_glyphs(void) {
    s32 i;
    s32 y; /* 16.16 */

    i = 0;
    y = 0x6E << 16;
    battle_hud_primitive_lists->count4CE0 = 0;
    for (; i < 5; i++) {
        if (battle_is_flag_in_mask(battle_gear_hud_warning_flags, i)) {
            battle_hud_primitive_lists->count4CE0 += battle_build_glyph(i + 0xC4, &battle_hud_primitive_lists->unk4CE0[battle_hud_primitive_lists->count4CE0 * 2], 0xE0, y >> 16);
            y += 10 << 16;
        }
    }
    battle_hud_primitive_lists->buffer4CE0 = battle_drawing_state.buffer;
    battle_hud_primitive_lists->blink = 0;
}

/* 80089110: Build glyph 0xa0 (0xa1 with 800d2c38) into the +0x3ac0 primitives and
 * initialise the current buffer's quads. */
void battle_gear_hud_build_overheat_glyph(void) {
    s32 id = 0xA0;
    s32 i;

    if (battle_gear_hud_overheat != 0) {
        id = 0xA1;
    }
    battle_hud_primitive_lists->counts[0] = battle_build_glyph(id, battle_hud_primitive_lists->list0, 0xA0, 0x64);
    battle_hud_primitive_lists->buffers[0] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->counts[0]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->list0[i * 2 + battle_hud_primitive_lists->buffers[0]]);
    }
}

/* 800891E4: Build the two glyphs of the escape/limit page (800d2c34 - 0x5d and
 * - 0x25) into lists 2 and 10 and initialise their quads. */
void battle_gear_hud_build_level_glyphs(void) {
    s32 i;
    u8 first;
    u8 second;

    first = battle_gear_hud_attack_level - 0x5D;
    second = battle_gear_hud_attack_level - 0x25;
    battle_hud_primitive_lists->counts[2] = battle_build_glyph(first, battle_hud_primitive_lists->list2, 0xA0, 0x64);
    battle_hud_primitive_lists->buffers[2] = battle_drawing_buffer_byte;
    battle_hud_primitive_lists->counts[10] = battle_build_glyph(second, battle_hud_primitive_lists->list10, 0xA0, 0x64);
    battle_hud_primitive_lists->buffers[10] = battle_drawing_buffer_byte;
    for (i = 0; i < battle_hud_primitive_lists->counts[2]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->list2[i * 2 + battle_hud_primitive_lists->buffers[2]]);
    }
    for (i = 0; i < battle_hud_primitive_lists->counts[10]; i++) {
        battle_quad_init_full_subtractive(&battle_hud_primitive_lists->list10[i * 2 + battle_hud_primitive_lists->buffers[10]]);
    }
}

/* 80089348: Build the five-digit value 800d2c2a as glyphs into list 3 at (0x11a, 0x46)
 * and initialise its quads. */
void battle_gear_hud_build_attack_glyphs(void) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    x = 0x11A << 16;
    battle_split_decimal_digits(battle_gear_hud_attack);
    for (; i < 5; i++) {
        digit = battle_decimal_digits[i + 4];
        if (digit != 0xFF) {
            battle_hud_primitive_lists->counts[3] +=
                battle_build_glyph(digit + 0x92, &battle_hud_primitive_lists->list3[battle_hud_primitive_lists->counts[3] * 2], x >> 16, 0x46);
            x += 6 << 16;
        }
    }
    battle_hud_primitive_lists->buffers[3] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->counts[3]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->list3[i * 2 + battle_hud_primitive_lists->buffers[3]]);
    }
}

/* 8008946C: Build the two-digit value 800d2c3a and a '%' glyph into list 4 at
 * (0x11a, 0x4e), or glyph 0xa2 from page 4 on, and initialise its quads. */
void battle_gear_hud_build_boost_chance_glyphs(void) {
    s32 i;
    s32 x; /* 16.16 */
    s32 digits;
    u8 digit;

    if (battle_gear_hud_attack_level < 4) {
        battle_split_decimal_digits(battle_gear_hud_boost_chance);
        i = 0;
        digits = 0;
        x = 0x11A << 16;
        for (; i < 2; i++) {
            digit = battle_decimal_digits[i + 7];
            if (digit != 0xFF) {
                    battle_hud_primitive_lists->counts[4] +=
                    battle_build_glyph(digit + 0x92, &battle_hud_primitive_lists->list4[battle_hud_primitive_lists->counts[4] * 2], x >> 16, 0x4E);
                digits++;
                x += 6 << 16;
            }
        }
        battle_hud_primitive_lists->counts[4] +=
            battle_build_glyph(0x9D, &battle_hud_primitive_lists->list4[battle_hud_primitive_lists->counts[4] * 2], digits * 6 + 0x11A, 0x4E);
    } else {
        battle_hud_primitive_lists->counts[4] = battle_build_glyph(0xA2, battle_hud_primitive_lists->list4, 0x11A, 0x4E);
    }
    battle_hud_primitive_lists->buffers[4] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->counts[4]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->list4[i * 2 + battle_hud_primitive_lists->buffers[4]]);
    }
}

/* 8008963C: Build the three-digit value 800d2c36 and a '%' glyph into list 5 at
 * (0x11a, 0x56) and initialise its quads. */
void battle_gear_hud_build_defense_glyphs(void) {
    s32 i;
    s32 x; /* 16.16 */
    s32 digits;
    u8 digit;

    i = 0;
    digits = 0;
    x = 0x11A << 16;
    battle_split_decimal_digits(battle_gear_hud_defense);
    for (; i < 3; i++) {
        digit = battle_decimal_digits[i + 6];
        if (digit != 0xFF) {
            battle_hud_primitive_lists->counts[5] +=
                battle_build_glyph(digit + 0x92, &battle_hud_primitive_lists->list5[battle_hud_primitive_lists->counts[5] * 2], x >> 16, 0x56);
            digits++;
            x += 6 << 16;
        }
    }
    battle_hud_primitive_lists->counts[5] +=
        battle_build_glyph(0x9D, &battle_hud_primitive_lists->list5[battle_hud_primitive_lists->counts[5] * 2], digits * 6 + 0x11A, 0x56);
    battle_hud_primitive_lists->buffers[5] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->counts[5]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->list5[i * 2 + battle_hud_primitive_lists->buffers[5]]);
    }
}

/* 800897CC: Build the two-digit value 800d2c35 as glyphs into list 6 at (0x11a,
 * 0x5e) and initialise its quads. */
void battle_gear_hud_build_speed_glyphs(void) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    x = 0x11A << 16;
    battle_split_decimal_digits(battle_gear_hud_speed);
    for (; i < 2; i++) {
        digit = battle_decimal_digits[i + 7];
        if (digit != 0xFF) {
            battle_hud_primitive_lists->counts[6] +=
                battle_build_glyph(digit + 0x92, &battle_hud_primitive_lists->list6[battle_hud_primitive_lists->counts[6] * 2], x >> 16, 0x5E);
            x += 6 << 16;
        }
    }
    battle_hud_primitive_lists->buffers[6] = battle_drawing_state.buffer;
    for (i = 0; i < battle_hud_primitive_lists->counts[6]; i++) {
        battle_quad_init_full_additive(&battle_hud_primitive_lists->list6[i * 2 + battle_hud_primitive_lists->buffers[6]]);
    }
}

/* 800898F0: Build the member's gear fuel as glyphs at y 0xcc: the fuel's last four
 * digits into list 7 (from x 0x20, blanks keep their place) and the maximum
 * fuel's into list 8 (from x 0x48, packed), then the separator glyph 0x9c
 * at x 0x41. */
void battle_gear_hud_build_fuel_glyphs(u8 member) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    battle_hud_primitive_lists->counts[7] = 0;
    x = 0x20 << 16;
    battle_split_decimal_digits(battle_work_area.records[member].gear.fuel);
    for (; i < 4; i++) {
        digit = battle_decimal_digits[i + 5];
        if (digit != 0xFF) {
            battle_hud_primitive_lists->counts[7] +=
                battle_build_glyph(digit + 0x92, &battle_hud_primitive_lists->list7[battle_hud_primitive_lists->counts[7] * 2], x >> 16, 0xCC);
        }
        x += 8 << 16;
    }
    i = 0;
    battle_hud_primitive_lists->buffers[7] = battle_drawing_state.buffer;
    battle_hud_primitive_lists->counts[8] = 0;
    x = 0x48 << 16;
    battle_split_decimal_digits(battle_work_area.records[member].gear.maxFuel);
    for (; i < 4; i++) {
        digit = battle_decimal_digits[i + 5];
        if (digit != 0xFF) {
            battle_hud_primitive_lists->counts[8] +=
                battle_build_glyph(digit + 0x92, &battle_hud_primitive_lists->list8[battle_hud_primitive_lists->counts[8] * 2], x >> 16, 0xCC);
            x += 8 << 16;
        }
    }
    battle_hud_primitive_lists->counts[8] += battle_build_glyph(0x9C, &battle_hud_primitive_lists->list8[battle_hud_primitive_lists->counts[8] * 2], 0x41, 0xCC);
    battle_hud_primitive_lists->buffers[8] = battle_drawing_state.buffer;
}

/* 80089AF8: Run the eight 8008860c..800897cc steps (the member is unused). */
void battle_gear_hud_build_lists(u8 member) {
    battle_gear_hud_build_fixed_glyphs();
    battle_gear_hud_build_warning_glyphs();
    battle_gear_hud_build_overheat_glyph();
    battle_gear_hud_build_level_glyphs();
    battle_gear_hud_build_attack_glyphs();
    battle_gear_hud_build_boost_chance_glyphs();
    battle_gear_hud_build_defense_glyphs();
    battle_gear_hud_build_speed_glyphs();
}

/* 80089B50: A random value in low..high (0xffff for low 0xffff, 0 for high 0). */
u16 battle_random_range(u16 low, u16 high) {
    s32 span;

    if (low == 0xFFFF) {
        return 0xFFFF;
    }
    if (high == 0) {
        return 0;
    }
    if (low == high) {
        return low;
    }
    span = high - low;
    if (span >= 0xFFFF) {
        return rand();
    }
    return low + (u16)rand() % (span + 1);
}

/* 80089BEC: The mask bit `bit`. */
u16 battle_get_flag_bit(u8 bit) {
    return battle_flag_bits[bit];
}

/* 80089C08: The mask bit of `slot`. */
u16 battle_get_slot_bit(u8 slot) {
    return battle_slot_bits[slot];
}

/* 80089C24: Every mask bit but `bit`. */
u16 battle_get_other_flag_bits(u8 bit) {
    return ~battle_flag_bits[bit];
}

/* 80089C48: Every slot bit but `slot`'s. */
u16 battle_get_other_slot_bits(u8 slot) {
    return ~battle_slot_bits[slot];
}

/* 80089C6C: Bit `bit` of `mask`; 0 for bits past 15. */
u16 battle_is_flag_in_mask(u16 mask, u8 bit) {
    u16 result;

    if (bit < 16) {
        result = battle_flag_bits[bit] & mask;
    } else {
        result = 0;
    }
    return result;
}

/* 80089C9C: The bit of `slot` in `mask`; 0 for slots past 15. */
u16 battle_is_slot_in_mask(u16 mask, u8 slot) {
    u16 result;

    if (slot < 16) {
        result = battle_slot_bits[slot] & mask;
    } else {
        result = 0;
    }
    return result;
}

/* 80089CCC: Read the battle input into 800d3014: wait for a controller as 8008a3ec
 * does, then dequeue pad entries until one matters. Directions (0-3,
 * remembered in 800c3e28) and the face buttons (4-7) play their sounds;
 * with the debug flag, select refills the party's AP and button 2 opens the
 * debug console; start (0x800, while 800ccc58) pauses or resumes, and while
 * paused holding both 4 and 8 on a debug build ends the battle. A finished
 * battle or event returns 0xff. Loops while paused. */
void battle_read_input(s32 mode) {
    u8 code = 8;
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;
    u8 *directions;
    u8 *outcome;

    do {
        if (pad_get_controller_kind(0) == 0) {
            if (paused == 0) {
                sprite_upload_pause_image(0x88, 0x64);
                sprite_upload_pause_image(0x88, 0x144);
                paused++;
                sound_silence_voices();
                vsyncs = pad_vblank_count;
            }
        } else {
            waiting = 0;
            if (paused) {
                sound_restore_voices();
                pad_vblank_count = vsyncs;
            }
        }
    } while (waiting);
    directions = battle_direction_input;
    outcome = &battle_area_outcome;
    do {
        if (pad_has_queue_overflowed()) {
            pad_clear_queue();
        } else {
            while (pad_dequeue_state()) {
                if (*outcome != 0 || battle_turn_state->eventsDone != 0) {
                    code = 0xFF;
                    break;
                }
                if (battle_paused != 0) {
                    if (*mode_disc_mode_pointer != -1 && (pad_port0_repeated & 4) && (pad_port0_repeated & 8)) {
                        *outcome = 1;
                        goto resume;
                    }
                } else if (pad_port0_repeated & 0x2000) {
                    battle_play_menu_sound(0x4C);
                    code = 0;
                    directions[0] = directions[1];
                    directions[1] = code;
                    break;
                } else if (pad_port0_repeated & 0x4000) {
                    battle_play_menu_sound(0x4C);
                    code = 1;
                    directions[0] = directions[1];
                    directions[1] = code;
                    break;
                } else if (pad_port0_repeated & 0x8000) {
                    battle_play_menu_sound(0x4C);
                    code = 2;
                    directions[0] = directions[1];
                    directions[1] = code;
                    break;
                } else if (pad_port0_repeated & 0x1000) {
                    battle_play_menu_sound(0x4C);
                    code = 3;
                    directions[0] = directions[1];
                    directions[1] = code;
                    break;
                } else if (pad_port0_pressed & 0x20) {
                    code = 4;
                    battle_play_menu_sound(0x4D);
                    break;
                } else if (pad_port0_pressed & 0x40) {
                    code = 5;
                    battle_play_menu_sound(0x4E);
                    break;
                } else if (pad_port0_pressed & 0x80) {
                    code = 6;
                    battle_play_menu_sound(0x4D);
                    break;
                } else if (pad_port0_pressed & 0x10) {
                    code = 7;
                    battle_play_menu_sound(0x4D);
                    break;
                } else if (pad_port0_pressed & 1) {
                    code = 0xC;
                    if (*mode_disc_mode_pointer != -1) {
                        battle_slot_flags[0].unk0 = 28;
                        battle_slot_flags[1].unk0 = 28;
                        battle_slot_flags[2].unk0 = 28;
                        battle_work_area.records[0].field148 = 4;
                        battle_work_area.records[1].field148 = 4;
                        battle_work_area.records[2].field148 = 4;
                        battle_work_area.records[0].statusTimers[6] = 0xFF;
                        battle_work_area.records[1].statusTimers[6] = 0xFF;
                        battle_work_area.records[2].statusTimers[6] = 0xFF;
                    }
                    break;
                } else if (pad_port0_pressed & 2) {
                    if (*mode_disc_mode_pointer != -1) {
                        code = 0xB;
                        if (++mode_battle_debug_page >= 5) {
                            mode_battle_debug_page = 0;
                        }
                        if (battle_console_opened == 0) {
                            console_set_external_block(0x80200000);
                            console_open(0x10, 0x10, 0x140, 0x100, 0x3E8, 0, 0x340, 0, 0x340, 0x20, 0);
                            battle_console_opened++;
                        }
                    }
                    break;
                } else if (pad_port0_pressed & 0x100) {
                    code = 0xD;
                    break;
                }
                if (pad_port0_pressed & 0x800) {
                    code = 0xE;
                    if (battle_turns_active != 0) {
                        if (battle_paused == 0) {
                            sprite_upload_pause_image(0x88, 0x64);
                            sprite_upload_pause_image(0x88, 0x144);
                            sound_silence_voices();
                            vsyncs = pad_vblank_count;
                            battle_paused = 1;
                        } else {
                        resume:
                            sound_restore_voices();
                            pad_vblank_count = vsyncs;
                            battle_paused = 0;
                        }
                    }
                    break;
                }
            }
        }
    } while (battle_paused != 0);
    battle_pressed_key = code;
}

/* 8008A144: Every other frame upload the next step of the four cycling CLUT strips. */
void battle_step_clut_cycle(void) {
    battle_ui->unkAA++;
    if (battle_ui->unkAA & 1) {
        LoadImage(&battle_graphics->unk8950[0], &battle_graphics->unk8970[0][battle_ui->unk30]);
        LoadImage(&battle_graphics->unk8950[1], &battle_graphics->unk8970[1][battle_ui->unk30]);
        LoadImage(&battle_graphics->unk8950[2], &battle_graphics->unk8970[2][battle_ui->unk30]);
        LoadImage(&battle_graphics->unk8950[3], &battle_graphics->unk8970[3][battle_ui->unk30]);
        battle_ui->unk30 += 4;
        if (battle_ui->unk30 >= 0xC7) {
            battle_ui->unk30 = 0;
        }
    }
}

/* 8008A274: Result-screen tick of end state 1: the ATB while 800ccc58, input for the
 * member, counters, the pulsing shade, the scroll by 800d39d4 and the CLUT
 * cycle. */
void battle_tick_turns(u8 member) {
    if (battle_turns_active != 0) {
        battle_tick_atb();
    }
    if (member == 0) {
        battle_read_input(0);
    }
    battle_ui->unkA9 += 6;
    battle_ui->unkAB++;
    if (battle_graphics->unk6415 != 0) {
        if (battle_graphics->unk6416 == 0) {
            battle_graphics->panelAlpha += 4;
            if (battle_graphics->panelAlpha > 0x80) {
                battle_graphics->panelAlpha = 0x7C;
                battle_graphics->unk6416 = 1;
            }
        } else {
            battle_graphics->panelAlpha -= 4;
            if (battle_graphics->panelAlpha < 0) {
                battle_graphics->panelAlpha = 4;
                battle_graphics->unk6416 = 0;
            }
        }
    }
    switch (battle_list_page_scroll_request) {
    case 1:
    case 3:
        battle_list_page_scroll++;
        break;
    case 2:
    case 4:
        battle_list_page_scroll--;
        break;
    }
    battle_step_clut_cycle();
}

/* 8008A3EC: Wait for a controller (pausing the sound and the vsync count while there
 * is none), run a result-screen tick, count down the active 800d3278 entry
 * timers, then read the input: an overflowed queue is reset; otherwise
 * entries are dequeued until one matters. Confirm (0x20) sets input code 4;
 * start (0x800, while 800ccc58) pauses or resumes the battle, and while
 * paused holding both 4 and 8 with a disc ends it (outcome 1). Loops while
 * paused. */
void battle_tick_event_script(u8 member) {
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;
    s32 i;

    do {
        if (pad_get_controller_kind(0) == 0) {
            if (paused == 0) {
                sprite_upload_pause_image(0x88, 0x64);
                sprite_upload_pause_image(0x88, 0x144);
                paused++;
                sound_silence_voices();
                vsyncs = pad_vblank_count;
            }
        } else {
            waiting = 0;
            if (paused) {
                sound_restore_voices();
                pad_vblank_count = vsyncs;
            }
        }
    } while (waiting);
    battle_step_clut_cycle();
    if (battle_ui->scriptLoaded != 0) {
        for (i = 0; i < 16; i++) {
            if (battle_state_of_event_script->threads[i].waiting != 0) {
                if (--battle_state_of_event_script->threads[i].waitTimer < 0) {
                    battle_state_of_event_script->threads[i].waitTimer = 0;
                }
            }
        }
    }
    do {
        if (battle_ui->waitingCross == 0) {
            battle_pressed_key = 0xFF;
        }
        if (pad_has_queue_overflowed()) {
            pad_clear_queue();
        } else {
            while (pad_dequeue_state()) {
                if (battle_paused != 0) {
                    if (*mode_disc_mode_pointer != -1 && (pad_port0_repeated & 4) && (pad_port0_repeated & 8)) {
                        battle_area_outcome = 1;
                        goto resume;
                    }
                } else if (pad_port0_pressed & 0x20) {
                    battle_pressed_key = 4;
                    break;
                }
                if (pad_port0_pressed & 0x800) {
                    if (battle_turns_active != 0) {
                        if (battle_paused == 0) {
                            sound_silence_voices();
                            sprite_upload_pause_image(0x88, 0x64);
                            sprite_upload_pause_image(0x88, 0x144);
                            vsyncs = pad_vblank_count;
                            battle_paused = 1;
                        } else {
                        resume:
                            sound_restore_voices();
                            pad_vblank_count = vsyncs;
                            battle_paused = 0;
                        }
                    }
                    break;
                }
            }
        }
    } while (battle_paused != 0);
}

/* 8008A684: Result screen input: wait for a controller as 8008a3ec does, read the
 * input (confirm sets input code 4, start pauses or resumes) while paused,
 * then count each member's two result values one step; while any is still
 * counting redraw the panels (801df270, 801df4c0), else stop counting.
 * Reads use the work table; stores use its position in the battle area.
 * These are two views of the same counters at 800cdcb8/800cdcd0. */
void battle_tick_result_screens(u8 member) {
    u8 done = 1;
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;
    s32 i;

    do {
        if (pad_get_controller_kind(0) == 0) {
            if (paused == 0) {
                sprite_upload_pause_image(0x88, 0x64);
                sprite_upload_pause_image(0x88, 0x144);
                paused++;
                sound_silence_voices();
                vsyncs = pad_vblank_count;
            }
        } else {
            waiting = 0;
            if (paused) {
                sound_restore_voices();
                pad_vblank_count = vsyncs;
            }
        }
    } while (waiting);
    if (battle_ui->waitingCross == 0) {
        battle_pressed_key = 0xFF;
    }
    do {
        if (pad_has_queue_overflowed()) {
            pad_clear_queue();
        } else {
            while (pad_dequeue_state()) {
                if (pad_port0_pressed & 0x20) {
                    battle_pressed_key = 4;
                    break;
                }
                if (pad_port0_pressed & 0x800) {
                    if (battle_paused == 0) {
                        sound_silence_voices();
                        sprite_upload_pause_image(0x88, 0x64);
                        sprite_upload_pause_image(0x88, 0x144);
                        vsyncs = pad_vblank_count;
                        battle_paused = 1;
                    } else {
                        sound_restore_voices();
                        pad_vblank_count = vsyncs;
                        battle_paused = 0;
                    }
                    break;
                }
            }
        }
    } while (battle_paused != 0);
    if (battle_ui->showCards != 0 && battle_member_cards[0]->counting != 0) {
        for (i = 0; i < 3; i++) {
            if (battle_member_cards[i]->done[0] == 0) {
                if (battle_work_area.toCount[i][0] == 0) {
                    battle_member_cards[i]->done[0] = 1;
                } else {
                    BATTLE_AREA.work.toCount[i][0] = battle_work_area.toCount[i][0] - 1;
                    BATTLE_AREA.work.expTotals[i][0] = battle_work_area.expTotals[i][0] + 1;
                }
            }
            if (battle_member_cards[i]->done[1] == 0) {
                if (battle_work_area.toCount[i][1] == 0) {
                    battle_member_cards[i]->done[1] = 1;
                } else {
                    BATTLE_AREA.work.toCount[i][1] = battle_work_area.toCount[i][1] - 1;
                    BATTLE_AREA.work.expTotals[i][1] = battle_work_area.expTotals[i][1] + 1;
                }
            }
            done &= battle_member_cards[i]->done[0];
            if (battle_member_cards[0]->secondValue != 0) {
                done &= battle_member_cards[i]->done[1];
            }
        }
        if (!done) {
            battle_results_build_card_exp_totals();
            battle_results_build_card_exp_to_count();
        } else {
            battle_member_cards[0]->counting = 0;
        }
    }
}

/* 8008A9C0: Result-screen step by the battle end state 800c3e4c. */
void battle_tick_frame(u8 member) {
    switch (battle_frame_mode) {
    case 0:
        battle_tick_result_screens(member);
        break;
    case 1:
        battle_tick_turns(member);
        break;
    case 2:
        battle_tick_event_script(member);
        break;
    }
}

/* 8008AA40: Play menu sound effect `id` of the system effect bank. */
void battle_play_system_sound(u8 id) {
    sound_play_effect_on_last_channels((sprite_script_sound_bank->bank << 16) | id);
}

/* 8008AA74: Play menu sound effect `id` while menu effects are enabled. */
void battle_play_menu_sound(u8 id) {
    if (battle_command_menu_sounds_enabled != 0) {
        battle_play_system_sound(id);
    }
}

/* 8008AAA0: Split `value` into nine decimal digits at 800c3cf4, leading zeros 0xff. */
void battle_split_decimal_digits(u32 value) {
    u32 divisor = 100000000;
    s32 i;

    for (i = 0; i < 9; i++) {
        battle_decimal_digits[i] = value / divisor;
        value %= divisor;
        divisor /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (battle_decimal_digits[i] != 0) {
            if (battle_decimal_digits[i - 1] == 0) {
                battle_decimal_digits[i - 1] = 0xFF;
            }
            break;
        }
        battle_decimal_digits[i - 1] = 0xFF;
    }
}

/* 8008AB4C: Heap mode 0x20/0. */
void battle_cd_select_event_script_directory(void) {
    cd_select_directory(0x20, 0);
}

/* 8008AB70: Heap mode 0x20/2. */
void battle_cd_select_music_directory(void) {
    cd_select_directory(0x20, 2);
}

/* 8008AB94: Heap mode 0x20/3. */
void battle_cd_select_menu_directory(void) {
    cd_select_directory(0x20, 3);
}

/* 8008ABB8: Allocate a battle heap block (owner tag 2). */
s32 battle_heap_alloc(s32 size, s32 mode) {
    heap_select_owner_tag(2, 0);
    return (s32)heap_alloc(size, mode);
}

/* 8008AC00: Allocate a text image block for `count` characters. */
s32 battle_heap_alloc_text_image(s32 count) {
    heap_select_owner_tag(2, 0);
    return (s32)heap_alloc((count + 3) * 26, 0);
}

/* 8008AC50: Wait frames until the disc reads finish. */
void battle_cd_wait_for_reads(void) {
    while (cd_get_pending_read_count() != 0) {
        battle_wait_frame();
    }
}

/* 8008AC88: Queue event 0xf3 for `actor` with the enemies of `mask` whose +0x34 bit
 * 0x800 is set (low three bits dropped). */
void battle_queue_reacting_enemies_event(u16 mask, u8 actor) {
    u16 targets = mask & 0xFFF8;
    s32 i;

    for (i = 0; i < 8; i++) {
        if (battle_is_slot_in_mask(targets, i + 3) && !(battle_work_area.records[i + 3].pilot.flags34 & 0x800)) {
            targets &= battle_get_other_slot_bits(i + 3);
        }
    }
    if (targets) {
        battle_area_events[battle_turn_state->eventCount].actor = actor;
        battle_area_events[battle_turn_state->eventCount].type = 0xF3;
        battle_area_events[battle_turn_state->eventCount].parameter = targets;
        battle_turn_state->eventCount++;
    }
}

/* 8008ADD0: Execute the chosen technique (turn state +0x2e6) for the member: reset
 * the events, commit the command (from 23, or the gear's from 22 in a gear;
 * command bits 0-2 target the candidates, else the chosen target), apply
 * its results, face the camera, show the member's model, queue its event
 * with the targeted enemies' reactions and wait for the presentation. */
void battle_execute_chosen_art(member)
u8 member;
{
    u16 targets;
    u16 command;
    s32 i;

    battle_command_menu_sounds_enabled = 0;
    battle_highlight_slots(0);
    for (i = 0; i < 32; i++) {
        battle_area_events[i].type = 0xFF;
    }
    if (battle_slot_flags[member].unk1 == 0) {
        battle_turn_state->unk2DC = battle_turn_state->unk2E6 + 23;
        command = battle_work_area.partyCommands[member][battle_turn_state->unk2E6 + 22].state;
    } else {
        battle_turn_state->unk2DC = battle_turn_state->unk2E6 + 22;
        command = battle_work_area.gearCommands[member][battle_turn_state->unk2E6 + 21].state;
    }
    if (command & 7) {
        targets = battle_target_candidate_mask;
    } else {
        targets = battle_get_slot_bit(battle_target_cursor_slot);
    }
    battle_queue_reacting_enemies_event(targets, member);
    battle_clear_event_results();
    battle_commit_action(member, targets, battle_turn_state->unk2DC - 1);
    battle_accumulate_and_apply_results(battle_turn_state->eventCount);
    battle_camera_start_move(battle_committed_action.targets | battle_get_slot_bit(member));
    battle_menu_open_turn(1, member, battle_turn_state->slots[member].defaultTarget, battle_find_next_turn_slot(member));
    battle_area_events[battle_turn_state->eventCount].type = battle_committed_action.animation;
    battle_area_events[battle_turn_state->eventCount].actor = member;
    battle_area_events[battle_turn_state->eventCount].targetMask = battle_committed_action.targets;
    for (i = 3; i < 11; i++) {
        if (battle_is_slot_in_mask(battle_committed_action.targets, i)) {
            battle_ai_tell_target_about_actor(member, i);
        }
    }
    battle_turn_state->eventCount++;
    battle_close_actor_event_queue(member);
    while (battle_turn_state->eventsDone == 0) {
        battle_wait_frame();
    }
}

/* 8008B108: Hide the command windows (four panels); without `keep` show the
 * +0x641c lists. */
void battle_hide_command_windows(u8 keep) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 0;
    battle_ui->windows[0] = battle_ui->windows[1] = battle_ui->windows[2] = battle_ui->windows[3] = 0;
    battle_ui->unkB7 = 0;
    if (keep == 0) {
        battle_ui->unkCB = 1;
    }
}

/* 8008B168: Show the command windows (four panels, page 1) and frame the camera on
 * the member and its default target. */
void battle_show_command_windows(u8 member) {
    battle_ui->unk9C = battle_ui->unk9D = battle_ui->cursorShown = 1;
    battle_ui->windows[0] = battle_ui->windows[1] = battle_ui->windows[2] = battle_ui->windows[3] = 1;
    battle_ui->unkB7 = 1;
    battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
    battle_highlight_slots(battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
}

/* 8008B224: Confirm the technique in list cell (column, row) for the member: its
 * command (from 22, or the gear's from 21 when the member is in a gear) needs
 * the character's permission bit and the EP it costs. Hides the command
 * windows and commits it, paying the EP; if the commit fails the windows
 * come back. A refused technique plays the error sound. Returns 1 when
 * committed. */
u8 battle_confirm_art(u8 member, u8 column, u8 row) {
    u8 committed = 0;
    u8 refused = 1;
    u8 allowed = 0;
    u16 command;
    u8 cost;

    if (battle_slot_flags[member].unk1 == 0) {
        command = battle_work_area.partyCommands[member][row * 2 + column + 22].state;
        cost = battle_work_area.partyCommands[member][row * 2 + column + 22].cost;
        allowed = battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].levelSkills, column + row * 2) != 0;
    } else {
        command = battle_work_area.gearCommands[member][row * 2 + column + 21].state;
        cost = battle_work_area.gearCommands[member][row * 2 + column + 21].cost;
        if (battle_is_flag_in_mask(game_data.skills[battle_party_character_ids[member]].unlocksB, column + row * 2) != 0) {
            allowed = 1;
        }
    }
    if (battle_work_area.records[member].pilot.ep >= cost && allowed) {
        battle_hide_command_windows(1);
        battle_ui->unkC6 = 1;
        if (battle_choose_target(command, member, 0)) {
            battle_work_area.records[member].pilot.ep -= cost;
            committed = 1;
        } else {
            battle_ui->unkC6 = 0;
            battle_show_command_windows(member);
        }
        refused = 0;
    }
    if (refused) {
        battle_play_menu_sound(0x4F);
    }
    return committed;
}
