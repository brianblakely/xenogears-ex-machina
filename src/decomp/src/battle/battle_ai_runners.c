/* Battle unit from 800792F8 to 80079ED8: the script error screen, the actor's
 * action list execution, the enemies' AI scripts (turn, reaction and
 * after-turn scripts) and the battle message windows. 800793F0's jump table
 * (8006FB38) is at 0 mod 8 after the previous unit's at 4 mod 8
 * (docs/matching.md). Its rodata starts at 8006FB08 with 800792F8's strings,
 * or at 8006FB38 if those belong to the previous unit; both fit, as does any
 * text boundary after 800745EC. */
#include "common.h"
#include "resident/console.h"
#include "resident/mode.h"
#include "battle/actions.h"
#include "battle/area.h"
#include "battle/command.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/item_command.h"
#include "battle/scene.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "own_declarations.h"

/* 800792F8: Script error screen: clear the event types and, on a debug build (the
 * 8005917c flag), print "Language Error" with the actor and script number
 * forever, the text shifted one column every three frames. */
void battle_show_script_error(actor, number)
u8 actor;
u8 number;
{
    s32 offset;
    u8 end = 0xFF;
    s32 i;
    s32 column;
    s32 frames;

    for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
        ((BattleEvent *)((u8 *)battle_area_events + offset))->type = end;
    }
    frames = 0;
    column = 0;
    if (*mode_disc_mode_pointer != -1) {
        while (1) {
            for (i = 0; i < column; i++) {
                console_printf(" ");
            }
            frames++;
            console_printf("\n\n\n\n\n\nLanguage Error\n");
            console_printf("\t\t\tActor%X\t\tNo%x\n\n", actor, number);
            battle_wait_frame();
            if (frames >= 3) {
                column++;
                frames = 0;
                if (column >= 21) {
                    column = 0;
                }
            }
        }
    }
}

/* 800793F0: Execute the actor's action list: each entry's handler by type, with the
 * first slot it targets; type 0 ends the list (entries after it run only
 * while fewer than 32 have), an unknown type shows the script error. */
void battle_action_list_execute(u8 actor) {
    s32 i;
    s32 slot;
    u8 target;
    u8 running;

    i = 0;
    running = 1;
    do {
        slot = 0;
        target = 0;
        for (; slot < 11; slot++) {
            if (battle_is_slot_in_mask(battle_action_list[i].targets, slot)) {
                target = slot;
                break;
            }
        }
        battle_clear_event_results();
        switch (battle_action_list[i].type) {
        case 0:
            running = 0;
            break;
        case 1:
            battle_action_list_act(actor, i, target);
            break;
        case 2:
            battle_action_list_approach(actor, i, target);
            break;
        case 3:
            battle_action_list_event_fc(actor, i, target);
            break;
        case 4:
            battle_action_list_event(actor, i, target);
            break;
        case 5:
            battle_action_list_together(actor, i, target);
            break;
        case 6:
            battle_action_list_leave(actor, i, target);
            break;
        case 7:
            battle_action_list_split(actor, i, target);
            break;
        case 8:
            battle_action_list_set_attr8(actor, i, target);
            break;
        case 9:
            battle_action_list_add_attr8(actor, i, target);
            break;
        case 10:
            battle_action_list_set_attr16(actor, i, target);
            break;
        case 11:
            battle_action_list_add_attr16(actor, i, target);
            break;
        case 12:
            battle_action_list_name(i, actor);
            break;
        case 13:
            battle_action_list_named_f4(actor, i, target);
            break;
        case 14:
            battle_action_list_event_f7(battle_action_list[i].param, actor);
            break;
        case 15:
            battle_action_list_event_f6(actor, i, target);
            break;
        case 16:
            battle_action_list_message_f8(actor);
            break;
        default:
            battle_show_script_error(actor, (u8)i);
            break;
        }
        i++;
    } while (i < 32 || running);
}

/* 80079674: Close the actor's event queue: event 0x1b for a flagged actor, then the
 * closing event 0xfe; menu effects off. */
void battle_action_list_close_events(u8 actor) {
    if (battle_slot_flags[actor].unk1 != 0) {
        battle_area_events[battle_turn_state->eventCount].actor = actor;
        battle_area_events[battle_turn_state->eventCount].type = 0x1B;
        battle_turn_state->eventCount++;
    }
    battle_area_events[battle_turn_state->eventCount].actor = actor;
    battle_area_events[battle_turn_state->eventCount].type = 0xFE;
    battle_command_menu_sounds_enabled = 0;
}

/* 80079778: Execute the actor's action list with its attack model and wait until the
 * queued events are done. */
void battle_action_list_play(u8 actor) {
    battle_turn_state->eventCount = 0;
    battle_reset_running_results();
    battle_clear_event_results();
    battle_area_events[battle_turn_state->eventCount].actor = actor;
    battle_wait_frame();
    battle_menu_open_turn(1, actor, 0, battle_find_next_turn_slot(actor));
    battle_action_list_execute(actor);
    battle_action_list_close_events(actor);
    while (battle_turn_state->eventsDone == 0) {
        battle_wait_frame();
    }
}

/* 80079840: Tell enemy `target` who acts on it: var 7 = the actor's bit, bytes 9..13
 * from 800d2ca4 and byte 14 whether the actor's default target is in the
 * 800c48e8 mask. */
void battle_ai_tell_target_about_actor(u8 actor, u8 target) {
    s32 i;
    u8 enemy;

    if (target >= 3) {
        enemy = target - 3;
        battle_enemy_ai_blocks[enemy].vars[7] = battle_get_slot_bit(actor);
        for (i = 0; i < 5; i++) {
            battle_enemy_ai_blocks[enemy].bytes[9 + i] = battle_committed_action.enemyBytes[i];
        }
        if ((u16)battle_area_knocked_out & battle_get_slot_bit(battle_turn_state->slots[actor].defaultTarget)) {
            battle_enemy_ai_blocks[enemy].bytes[14] = 1;
        } else {
            battle_enemy_ai_blocks[enemy].bytes[14] = 0;
        }
    }
}

/* 80079934: Advance the AI script by one four-byte instruction. */
void battle_ai_step_instruction(u8 **pc) {
    *pc += 4;
}

/* 80079948: After a false condition skip the remaining conditions (0x80 and up), then
 * every opcode outside 0x80..0xef: the rule's actions and its closing fd/ff,
 * up to the next rule's condition. */
void battle_ai_skip_rule(u8 **pc) {
    while (**pc >= 0x80) {
        battle_ai_step_instruction(pc);
    }
    while ((u8)(**pc - 0x80) >= 0x70) {
        battle_ai_step_instruction(pc);
    }
}

/* 800799C8: Run enemy `slot`'s turn script (AI block +0x00): clear the action list and
 * event types, then evaluate conditions (8007f8c0) and actions (8007ef6c)
 * until 0xfd or 0xff; a false condition skips its rule (80079948). */
void battle_ai_run_turn_script(u8 slot, u16 attacking) {
    u8 *pc;
    u8 count;
    u8 enemy;
    u8 *p;
    s32 offset;

    count = 0;
    enemy = slot - 3;
    pc = battle_enemy_ai_blocks[enemy].script;
    p = (u8 *)battle_action_list;
    do {
        *p++ = 0;
    } while (p < (u8 *)battle_action_list + 0x100);
    for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
        ((BattleEvent *)((u8 *)battle_area_events + offset))->type = 0xFF;
    }
    while (*pc != 0xFD && *pc != 0xFF) {
        if (*pc >= 0x80) {
            if (!battle_ai_evaluate_condition(&pc, enemy)) {
                battle_ai_skip_rule(&pc);
            }
        } else {
            count = battle_ai_run_action(&pc, enemy, count);
        }
    }
}

/* 80079AB0: Run enemy `slot`'s reaction script when armed (and the enemy is not down,
 * unless +0x34 bit 0x800 lets it react), then execute its action list.
 * Returns whether the script ran action 0x62. */
s32 battle_ai_run_reaction_script(u8 slot) {
    u8 *pc;
    s32 ranAction62 = 0;
    u8 enemy = slot - 3;
    u8 count = 0;
    u8 *p;

    if (!(battle_work_area.records[enemy + 3].pilot.status7C & 0x8000) || (battle_work_area.records[enemy + 3].pilot.flags34 & 0x800)) {
        battle_action_list[0].type = 0;
        if (battle_enemy_reactions[enemy].armed != 0) {
            pc = battle_enemy_ai_blocks[enemy].reaction;
            p = (u8 *)battle_action_list;
            do {
                *p++ = 0;
            } while (p < (u8 *)battle_action_list + 0x100);
            while (*pc != 0xFD && *pc != 0xFF) {
                if (*pc >= 0x80) {
                    if (!battle_ai_evaluate_condition(&pc, enemy)) {
                        battle_ai_skip_rule(&pc);
                    }
                } else {
                    if (*pc == 0x62) {
                        ranAction62 = 1;
                    }
                    count = battle_ai_run_action(&pc, enemy, count);
                }
            }
        }
        if (battle_action_list[0].type != 0) {
            battle_action_list_execute(enemy + 3);
        }
    }
    return ranAction62;
}

/* 80079C24: Run each armed enemy's AI script at +0xc (reaction bytes 1 and 2 set, the
 * enemy not down unless +0x34 bit 0x800 lets it act) as the acting slot,
 * then execute its action list; restore the acting slot. */
void battle_ai_run_targeted_scripts(void) {
    u8 *pc[1]; /* Mutable cursor shared with the AI opcode handlers. */
    u8 count;
    s32 enemy;
    u8 actor;
    u8 *p;
    u8 *end;
    s32 offset;

    count = 0;
    actor = battle_turn_state->actor;
    for (enemy = 0; enemy < 8; enemy++) {
        if (battle_enemy_reactions[enemy].unk1[1] != 0 && battle_enemy_reactions[enemy].unk1[0] != 0) {
            battle_turn_state->actor = enemy + 3;
            pc[0] = battle_enemy_ai_blocks[enemy].turnScript;
            p = (u8 *)battle_action_list;
            if (!(battle_work_area.records[enemy + 3].pilot.status7C & 0x8000) ||
                (battle_work_area.records[enemy + 3].pilot.flags34 & 0x800)) {
                for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
                    ((BattleEvent *)((u8 *)battle_area_events + offset))->type = 0xFF;
                }
                end = p + 0x100;
                do {
                    *p++ = 0;
                } while (p < end);
                while (*pc[0] != 0xFD && *pc[0] != 0xFF) {
                    if (*pc[0] >= 0x80) {
                        if (!battle_ai_evaluate_condition(pc, enemy)) {
                            battle_ai_skip_rule(pc);
                        }
                    } else {
                        count = battle_ai_run_action(pc, enemy, count);
                    }
                }
                battle_turn_state->eventsDone = 0;
                battle_action_list_play(enemy + 3);
                battle_run_event_script(0);
            }
        }
    }
    battle_turn_state->actor = actor;
}

/* 80079E18: Show battle message window `index`. */
void battle_show_message_window(u8 index) {
    battle_ui->windows[4] = 1;
    battle_message_entries[index].shown = 1;
}

/* 80079E4C: Hide battle message window `index`. */
void battle_hide_message_window(u8 index) {
    battle_ui->windows[4] = 0;
    battle_message_entries[index].shown = 0;
}

/* 80079E7C: The first slot in `mask`; 11 when none. */
u8 battle_find_first_slot_in_mask(u16 mask) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (battle_is_slot_in_mask(mask, slot)) {
            break;
        }
    }
    return slot;
}
