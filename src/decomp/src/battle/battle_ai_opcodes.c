/* Battle unit from 80079ED8 to 8008115C: the combatants' byte and halfword
 * attributes, the AI script actions (01-74) and conditions (81-9b) with their
 * interpreters (8007EF6C, 8007F8C0), and a party member's turn in the command
 * menu (8007FB70-80080C94: the menu blocks, the number strings, the menu
 * itself and automatic turns). Its rodata starts at 8006FB7C, 4 mod 8, right
 * after 800793F0's odd-length table at 0 mod 8 (docs/matching.md); the text
 * boundary lies after 800793F0. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/combatant.h"
#include "battle/command.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/graphics.h"
#include "battle/groups.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/scene.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/work.h"
#include "gear_menu.h"
#include "own_declarations.h"
#include "resident_views.h"

/* Callers convert arguments differently from the definition (800BAF40, in
 * 800B8098's unit, takes none): they pass a slot and a mode it ignores. */
void battle_empty_commit_step(u8 slot, s32 mode);

/* 80079ED8: Byte attribute `attribute` (0-23) of combatant `slot`: store `value` when
 * `read` is 0, else return it. */
u8 battle_access_combatant_attr8(u8 slot, u8 attribute, u8 value, u8 read) {
    u8 *field;
    u8 result;

    switch (attribute) {
    case 0:
        field = &battle_work_area.records[slot].pilot.entries[0].value4;
        break;
    case 1:
        field = &battle_work_area.records[slot].pilot.characterId;
        break;
    case 2:
        field = &battle_work_area.records[slot].gear.attack;
        break;
    case 3:
        field = &battle_work_area.records[slot].gear.attackScale;
        break;
    case 4:
        field = &battle_work_area.records[slot].pilot.attack;
        break;
    case 5:
        field = &battle_work_area.records[slot].pilot.defense;
        break;
    case 6:
        field = &battle_work_area.records[slot].pilot.speed;
        break;
    case 7:
        field = &battle_work_area.records[slot].pilot.bodyDefense;
        break;
    case 8:
        field = &battle_work_area.records[slot].pilot.field5D;
        break;
    case 9:
        field = &battle_work_area.records[slot].pilot.ether;
        break;
    case 10:
        field = &battle_work_area.records[slot].pilot.etherDefense;
        break;
    case 11:
        field = &battle_work_area.records[slot].pilot.field5E;
        break;
    case 12:
        field = &battle_work_area.records[slot].pilot.field5F;
        break;
    case 13:
        field = &battle_work_area.records[slot].pilot.field60;
        break;
    case 14:
        field = &battle_work_area.records[slot].pilot.field61;
        break;
    case 15:
        field = &battle_work_area.records[slot].pilot.field64[0];
        break;
    case 16:
        field = &battle_work_area.records[slot].pilot.field64[1];
        break;
    case 17:
        field = &battle_work_area.records[slot].pilot.field64[2];
        break;
    case 18:
        field = &battle_work_area.records[slot].pilot.field64[3];
        break;
    case 19:
        field = &battle_work_area.records[slot].gear.speed;
        break;
    case 20:
        field = &battle_work_area.records[slot].gear.entries[0].valueE;
        break;
    case 21:
        field = &battle_work_area.records[slot].gear.guard;
        break;
    case 22:
        field = &battle_work_area.records[slot].gear.field9D;
        break;
    case 23:
        field = &battle_work_area.records[slot].gear.frameFactor;
        break;
    }
    if (!read) {
        *field = value;
    } else {
        result = *field;
    }
    return result;
}

/* 8007A280: Halfword attribute `attribute` (0-23) of combatant `slot`: store `value`
 * when `read` is 0, else return it. */
u16 battle_access_combatant_attr16(u8 slot, u8 attribute, u16 value, u8 read) {
    u16 *field;
    u16 result;

    switch (attribute) {
    case 0:
        field = &battle_work_area.records[slot].pilot.maxHp;
        break;
    case 1:
        field = &battle_work_area.records[slot].pilot.hp;
        break;
    case 2:
        field = &battle_work_area.records[slot].pilot.status7C;
        break;
    case 3:
        field = &battle_work_area.records[slot].pilot.status7E;
        break;
    case 4:
        field = &battle_work_area.records[slot].pilot.status80;
        break;
    case 5:
        field = &battle_work_area.records[slot].pilot.status82;
        break;
    case 6:
        field = &battle_work_area.records[slot].pilot.status84.half.active;
        break;
    case 7:
        field = &battle_work_area.records[slot].pilot.status84.half.permanent;
        break;
    case 8:
        field = &battle_work_area.records[slot].pilot.status88.half.active;
        break;
    case 9:
        field = &battle_work_area.records[slot].pilot.status88.half.permanent;
        break;
    case 10:
        field = &battle_work_area.records[slot].pilot.status8C.half.active;
        break;
    case 11:
        field = &battle_work_area.records[slot].pilot.status8C.half.permanent;
        break;
    case 12:
        field = &battle_work_area.records[slot].gear.field6C;
        break;
    case 13:
        field = &battle_work_area.records[slot].gear.bodyDefense;
        break;
    case 14:
        field = &battle_work_area.records[slot].gear.armor;
        break;
    case 15:
        field = &battle_work_area.records[slot].gear.status7C;
        break;
    case 16:
        field = &battle_work_area.records[slot].gear.field7E;
        break;
    case 17:
        field = &battle_work_area.records[slot].gear.status80;
        break;
    case 18:
        field = &battle_work_area.records[slot].gear.status82;
        break;
    case 19:
        field = &battle_work_area.records[slot].gear.status84.half.active;
        break;
    case 20:
        field = &battle_work_area.records[slot].gear.status84.half.permanent;
        break;
    case 21:
        field = &battle_work_area.records[slot].pilot.flags34;
        break;
    case 22:
        field = &battle_work_area.records[slot].pilot.flags36;
        break;
    case 23:
        field = &battle_work_area.records[slot].pilot.weakness;
        break;
    }
    if (!read) {
        *field = value;
    } else {
        result = *field;
    }
    return result;
}

/* 8007A628: Whether `slot` can be targeted: present, visible and not down (+0x7c
 * 0xc002); without `any` also not flagged 0x20 at +0x84. */
u8 battle_is_slot_targetable(u8 slot, u8 any) {
    u8 result = 0;
    u16 status;

    if (battle_turn_queue.present[slot] != 0 && battle_area_slots[slot].hidden == 0 && !(battle_work_area.records[slot].pilot.status7C & 0xC002)) {
        if (any != 0) {
            result = 1;
        } else {
            status = battle_work_area.records[slot].pilot.status84.half.active & 0x20;
            result = status == 0;
        }
    }
    return result;
}

/* 8007A6C8: As 8007a628 without the visibility test. */
u8 battle_is_slot_targetable_even_hidden(u8 slot, u8 any) {
    u8 result = 0;
    u16 status;

    if (battle_turn_queue.present[slot] != 0 && !(battle_work_area.records[slot].pilot.status7C & 0xC002)) {
        result = 1;
        if (any == 0) {
            status = battle_work_area.records[slot].pilot.status84.half.active & 0x20;
            result = status == 0;
        }
    }
    return result;
}

/* 8007A744: Whether `slot` is present, visible and alive (+0x7c without 0xc000). */
u8 battle_is_slot_visible_and_alive(u8 slot) {
    u8 result = 0;

    if (battle_turn_queue.present[slot] != 0 && battle_area_slots[slot].hidden == 0) {
        result = (battle_work_area.records[slot].pilot.status7C & 0xC000) == 0;
    }
    return result;
}

/* 8007A7BC: AI action default (00, 6e, 6f, 75-7f): queue an entry of type 0x80
 * carrying the four opcode bytes, which 800793f0 rejects with its script
 * error (800792f8). Returns the new entry count. */
u8 battle_ai_queue_undefined_opcode(u8 **pc, u8 *list, u8 enemy, u8 count) {
    list[count * 8] = 0x80;
    list[count * 8 + 1] = (*pc)[0];
    list[count * 8 + 2] = (*pc)[1];
    list[count * 8 + 3] = (*pc)[2];
    list[count * 8 + 4] = (*pc)[3];
    return count + 1;
}

/* 8007A828: AI action 01: list entry byte b1 = b2; offset 0 starts the next entry. */
u8 battle_ai_list_set(u8 **pc, u8 *list, u8 count) {
    list[count * 8 + (*pc)[1]] = (*pc)[2];
    if ((*pc)[1] == 0) {
        count++;
    }
    return count;
}

/* 8007A874: AI action 02: list entry byte b1 = byte variable b2. */
void battle_ai_list_set_b(u8 **pc, u8 *list, u8 enemy, u8 count) {
    list[count * 8 + (*pc)[1]] = battle_enemy_ai_blocks[enemy].bytes[(*pc)[2]];
}

/* 8007A8B4: AI action 03: copy list entry b1 to entry b2. */
void battle_ai_list_copy(u8 **pc, u8 *list) {
    s32 i;

    for (i = 0; i < 8; i++) {
        list[(*pc)[2] * 8 + i] = list[(*pc)[1] * 8 + i];
    }
}

/* 8007A900: AI action 04: byte variable b1 = b2. */
void battle_ai_set_b(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = (*pc)[2];
}

/* 8007A92C: AI action 05: variable b1 = b2 | b3 << 8. */
void battle_ai_set_v(u8 **pc, u8 enemy) {
    u16 value = (*pc)[2] | ((*pc)[3] << 8);

    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = value;
}

/* 8007A968: AI action 06: long b1 = (b2 | b3 << 8) * 16. */
void battle_ai_set_l16(u8 **pc, u8 enemy) {
    s32 value = (((*pc)[3] << 8) + (*pc)[2]) * 16;

    battle_enemy_ai_blocks[enemy].longs[(*pc)[1]] = value;
}

/* 8007A9A8: AI action 07: resident halfword b1 = b2. */
void battle_ai_set_res(u8 **pc) {
    mode_battle_ai_variables[(*pc)[1]] = (*pc)[2];
}

/* 8007A9D0: AI action 08: byte variable b1 += b2, saturating at 0xff. */
void battle_ai_add_b(u8 **pc, u8 enemy) {
    u8 *value = &battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]];
    s32 sum = *value + (*pc)[2];
    u8 result = sum;

    if (sum >= 0x100) {
        result = 0xFF;
    }
    *value = result;
}

/* 8007AA1C: AI action 09: byte variable b1 -= b2, saturating at 0. */
void battle_ai_sub_b(u8 **pc, u8 enemy) {
    u8 *value = &battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]];
    s32 difference = *value - (*pc)[2];
    u8 result = difference;

    if (difference < 0) {
        result = 0;
    }
    *value = result;
}

/* 8007AA60: AI action 0a: byte variable b1 *= b2, saturating at 0xff. */
void battle_ai_mul_b(u8 **pc, u8 enemy) {
    u8 *value = &battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]];
    s16 product = *value * (*pc)[2];

    if (product >= 0x100) {
        product = 0xFF;
    }
    *value = product;
}

/* 8007AAB8: AI action 0b: byte variable b1 /= b2. */
void battle_ai_div_b(u8 **pc, u8 enemy) {
    u8 *value = &battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]];

    *value = *value / (*pc)[2];
}

/* 8007AAF4: AI action 0c: byte variable b1 %= b2. */
void battle_ai_mod_b(u8 **pc, u8 enemy) {
    u8 *value = &battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]];

    *value = *value % (*pc)[2];
}

/* 8007AB30: AI action 0d: byte variable b1 &= b2. */
void battle_ai_and_b(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] &= (*pc)[2];
}

/* 8007AB68: AI action 0e: byte variable b1 |= b2. */
void battle_ai_or_b(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] |= (*pc)[2];
}

/* 8007ABA0: AI action 0f: byte variable b1 ^= b2. */
void battle_ai_xor_b(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] ^= (*pc)[2];
}

/* 8007ABD8: AI action 10: variable b1 += b2 | b3 << 8, saturating at 0xffff. */
void battle_ai_add_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 value = battle_enemy_ai_blocks[enemy].vars[op[1]] + ((op[3] << 8) + op[2]);

    if (value > 0xFFFF) {
        value = 0xFFFF;
    }
    battle_enemy_ai_blocks[enemy].vars[op[1]] = value;
}

/* 8007AC30: AI action 11: variable b1 -= b2 | b3 << 8, saturating at 0. */
void battle_ai_sub_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 value = battle_enemy_ai_blocks[enemy].vars[op[1]] - ((op[3] << 8) + op[2]);

    if (value < 0) {
        value = 0;
    }
    battle_enemy_ai_blocks[enemy].vars[op[1]] = value;
}

/* 8007AC80: AI action 12: variable b1 *= b2 | b3 << 8, saturating at 0xffff. */
void battle_ai_mul_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 value = battle_enemy_ai_blocks[enemy].vars[op[1]] * ((op[3] << 8) + op[2]);

    if (value > 0xFFFF) {
        value = 0xFFFF;
    }
    battle_enemy_ai_blocks[enemy].vars[op[1]] = value;
}

/* 8007ACDC: AI action 13: variable b1 /= b2 | b3 << 8. */
void battle_ai_div_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    battle_enemy_ai_blocks[enemy].vars[op[1]] = battle_enemy_ai_blocks[enemy].vars[op[1]] / ((op[3] << 8) + op[2]);
}

/* 8007AD24: AI action 14: variable b1 %= b2 | b3 << 8. */
void battle_ai_mod_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    battle_enemy_ai_blocks[enemy].vars[op[1]] = battle_enemy_ai_blocks[enemy].vars[op[1]] % ((op[3] << 8) + op[2]);
}

/* 8007AD6C: AI action 15: variable b1 &= b2 | b3 << 8. */
void battle_ai_and_v(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] &= (*pc)[2] + ((*pc)[3] << 8);
}

/* 8007ADB0: AI action 16: variable b1 |= b2 | b3 << 8. */
void battle_ai_or_v(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] |= (*pc)[2] + ((*pc)[3] << 8);
}

/* 8007ADF4: AI action 17: variable b1 ^= b2 | b3 << 8. */
void battle_ai_xor_v(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] ^= (*pc)[2] + ((*pc)[3] << 8);
}

/* 8007AE38: AI action 18: byte variable b3 = byte b1 + byte b2, saturating at 0xff. */
void battle_ai_add_bb(u8 **pc, u8 enemy) {
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;
    s32 sum = bytes[(*pc)[1]] + bytes[(*pc)[2]];
    u8 result = sum;

    if (sum >= 0x100) {
        result = 0xFF;
    }
    bytes[(*pc)[3]] = result;
}

/* 8007AE98: AI action 19: byte variable b3 = byte b1 - byte b2, saturating at 0. */
void battle_ai_sub_bb(u8 **pc, u8 enemy) {
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;
    s32 difference = bytes[(*pc)[1]] - bytes[(*pc)[2]];
    u8 result = difference;

    if (difference < 0) {
        result = 0;
    }
    bytes[(*pc)[3]] = result;
}

/* 8007AEF0: AI action 1a: byte variable b3 = byte b1 * byte b2, saturating at 0xff. */
void battle_ai_mul_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;
    s16 product = bytes[op[1]] * bytes[op[2]];

    if (product >= 0x100) {
        product = 0xFF;
    }
    bytes[op[3]] = product;
}

/* 8007AF5C: AI action 1b: byte variable b3 = byte b1 / byte b2. */
void battle_ai_div_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] / bytes[op[2]];
}

/* 8007AFAC: AI action 1c: byte variable b3 = byte b1 % byte b2. */
void battle_ai_mod_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] % bytes[op[2]];
}

/* 8007AFFC: AI action 1d: byte variable b3 = byte b1 & byte b2. */
void battle_ai_and_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] & bytes[op[2]];
}

/* 8007B040: AI action 1e: byte variable b3 = byte b1 | byte b2. */
void battle_ai_or_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] | bytes[op[2]];
}

/* 8007B084: AI action 1f: byte variable b3 = byte b1 ^ byte b2. */
void battle_ai_xor_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] ^ bytes[op[2]];
}

/* 8007B0C8: AI action 20: variable b3 = var b1 + var b2, saturating at 0xffff. */
void battle_ai_add_vv(u8 **pc, u8 enemy) {
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;
    s32 sum = vars[(*pc)[1]] + vars[(*pc)[2]];

    if (sum > 0xFFFF) {
        sum = 0xFFFF;
    }
    vars[(*pc)[3]] = sum;
}

/* 8007B134: AI action 21: variable b3 = var b1 - var b2, saturating at 0. */
void battle_ai_sub_vv(u8 **pc, u8 enemy) {
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;
    s32 difference = vars[(*pc)[1]] - vars[(*pc)[2]];

    if (difference < 0) {
        difference = 0;
    }
    vars[(*pc)[3]] = difference;
}

/* 8007B198: AI action 22: variable b3 = var b1 * var b2, saturating at 0xffff. */
void battle_ai_mul_vv(u8 **pc, u8 enemy) {
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;
    s32 product = vars[(*pc)[1]] * vars[(*pc)[2]];

    if (product > 0xFFFF) {
        product = 0xFFFF;
    }
    vars[(*pc)[3]] = product;
}

/* 8007B208: AI action 23: variable b3 = var b1 / var b2. */
void battle_ai_div_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;

    vars[op[3]] = vars[op[1]] / vars[op[2]];
}

/* 8007B264: AI action 24: variable b3 = var b1 % var b2. */
void battle_ai_mod_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;

    vars[op[3]] = vars[op[1]] % vars[op[2]];
}

/* 8007B2C0: AI action 25: variable b3 = var b1 & var b2. */
void battle_ai_and_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[op[3]] = battle_enemy_ai_blocks[enemy].vars[op[1]] & battle_enemy_ai_blocks[enemy].vars[op[2]];
}

/* 8007B310: AI action 26: variable b3 = var b1 | var b2. */
void battle_ai_or_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[op[3]] = battle_enemy_ai_blocks[enemy].vars[op[1]] | battle_enemy_ai_blocks[enemy].vars[op[2]];
}

/* 8007B360: AI action 27: variable b3 = var b1 ^ var b2. */
void battle_ai_xor_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[op[3]] = battle_enemy_ai_blocks[enemy].vars[op[1]] ^ battle_enemy_ai_blocks[enemy].vars[op[2]];
}

/* 8007B3B0: AI action 28: the enemy's byte attribute b1 (80079ed8) = b2. */
void battle_ai_set_own_attr8(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_access_combatant_attr8(enemy + 3, op[1], op[2], 0);
}

/* 8007B3E4: AI action 29: the enemy's halfword attribute b1 (8007a280) = b2 | b3 << 8. */
void battle_ai_set_own_attr16(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_access_combatant_attr16(enemy + 3, op[1], op[2] | (op[3] << 8), 0);
}

/* 8007B424: AI action 2a: byte variable b1 = attribute b2 of the first slot in var b3. */
void battle_ai_get_attr8(u8 **pc, u8 enemy) {
    u8 slot = battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[3]]);

    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = battle_access_combatant_attr8(slot, (*pc)[2], 0, 1);
}

/* 8007B4B8: AI action 2b: set byte attribute b2 of the first slot in var b3 to byte
 * variable b1; for a party slot instead set list entry 0's type to 0x20
 * (800793f0 rejects it) and count an entry. */
u8 battle_ai_put_attr8(u8 **pc, u8 enemy, u8 count) {
    u8 slot = battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[3]]);

    if (slot < 3) {
        battle_action_list[0].type = 0x20;
        count++;
    } else {
        battle_access_combatant_attr8(slot, (*pc)[2], battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]], 0);
    }
    return count;
}

/* 8007B578: AI action 2c: variable b1 = 16-bit attribute b2 of the first slot in var b3. */
void battle_ai_get_attr16(u8 **pc, u8 enemy) {
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;
    u8 slot = battle_find_first_slot_in_mask(vars[(*pc)[3]]);

    vars[(*pc)[1]] = battle_access_combatant_attr16(slot, (*pc)[2], 0, 1);
}

/* 8007B608: AI action 2d: set 16-bit attribute b2 of the first slot in var b3 to var b1;
 * for a party slot instead set list entry 0's type to 0x20 and count an
 * entry. */
u8 battle_ai_put_attr16(u8 **pc, u8 enemy, u8 count) {
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;
    u8 slot = battle_find_first_slot_in_mask(vars[(*pc)[3]]);

    if (slot < 3) {
        battle_action_list[0].type = 0x20;
        count++;
    } else {
        battle_access_combatant_attr16(slot, (*pc)[2], vars[(*pc)[1]], 0);
    }
    return count;
}

/* 8007B6C0: AI action 2e: long b1 = (b2 1) record +0x104 of the first slot in var b3,
 * or (b2 2) the party's gold. */
void battle_ai_get_long(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    switch (op[2]) {
    case 1:
        battle_enemy_ai_blocks[enemy].longs[(*pc)[1]] =
            battle_work_area.records[battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[op[3]])].gear.hp;
        break;
    case 2:
        battle_enemy_ai_blocks[enemy].longs[op[1]] = game_data.gold;
        break;
    }
}

/* 8007B7B0: AI action 2f: record +0x108 (b2 0) or +0x104 of the first slot in var b3 =
 * long b1; for a party slot instead set list entry 0's type to 0x20 and count
 * an entry. */
u8 battle_ai_put_long(u8 **pc, u8 enemy, u8 count) {
    u8 slot = battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[3]]);

    if (slot < 3) {
        battle_action_list[0].type = 0x20;
        count++;
    } else if ((*pc)[2] == 0) {
        battle_work_area.records[slot].gear.maxHp = battle_enemy_ai_blocks[enemy].longs[(*pc)[1]];
    } else {
        battle_work_area.records[slot].gear.hp = battle_enemy_ai_blocks[enemy].longs[(*pc)[1]];
    }
    return count;
}

/* 8007B8D4: AI action 30: variable b1 = resident halfword b2. */
void battle_ai_get_res(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[op[1]] = mode_battle_ai_variables[op[2]];
}

/* 8007B914: AI action 31: resident halfword b2 = variable b1. */
void battle_ai_put_res(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    mode_battle_ai_variables[op[2]] = battle_enemy_ai_blocks[enemy].vars[op[1]];
}

/* 8007B958: AI action 32: byte variable b2 = byte variable b1. */
void battle_ai_copy_b(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    bytes[op[2]] = bytes[op[1]];
}

/* 8007B98C: AI action 33: variable b2 = variable b1. */
void battle_ai_copy_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[op[2]] = battle_enemy_ai_blocks[enemy].vars[op[1]];
}

/* 8007B9C8: AI action 34: long b2 = long b1. */
void battle_ai_copy_l(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].longs[op[2]] = battle_enemy_ai_blocks[enemy].longs[op[1]];
}

/* 8007BA04: AI action 35: variable b2 = byte variable b1. */
void battle_ai_b_to_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[op[2]] = battle_enemy_ai_blocks[enemy].bytes[op[1]];
}

/* 8007BA44: AI action 36: long b2 = variable b1. */
void battle_ai_v_to_l(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].longs[op[2]] = battle_enemy_ai_blocks[enemy].vars[op[1]];
}

/* 8007BA88: AI action 37: clear the byte variables. */
void battle_ai_clear_b(u8 enemy) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        battle_enemy_ai_blocks[enemy].bytes[i] = 0;
    }
}

/* 8007BAB8: AI action 38: clear the variables. */
void battle_ai_clear_v(u8 enemy) {
    s32 i;

    for (i = 7; i >= 0; i--) {
        battle_enemy_ai_blocks[enemy].vars[i] = 0;
    }
}

/* 8007BAE8: AI action 39: the enemy's experience (record +0x14c) = b1 | b2 << 8. */
void battle_ai_set_experience(u8 **pc, u8 enemy) {
    u16 value = ((*pc)[2] << 8) | (*pc)[1];

    battle_work_area.records[enemy + 3].field14C = value;
}

/* 8007BB2C: AI action 3a: the enemy's gold (record +0x156) = b1 | b2 << 8. */
void battle_ai_set_gold(u8 **pc, u8 enemy) {
    u16 value = (*pc)[1] | ((*pc)[2] << 8);

    battle_work_area.records[enemy + 3].field156 = value;
}

/* 8007BB70: AI action 3b: the enemy record's bytes +0x155, +0x153, +0x151 = b1, b2, b3. */
void battle_ai_set_second_drop(u8 **pc, u8 enemy) {
    battle_work_area.records[enemy + 3].field150[5] = (*pc)[1];
    battle_work_area.records[enemy + 3].field150[3] = (*pc)[2];
    battle_work_area.records[enemy + 3].field150[1] = (*pc)[3];
}

/* 8007BBD8: AI action 3c: the enemy's first drop: category (record +0x154), item
 * (+0x152) and chance (+0x150) = b1, b2, b3. */
void battle_ai_set_drop(u8 **pc, u8 enemy) {
    battle_work_area.records[enemy + 3].field150[4] = (*pc)[1];
    battle_work_area.records[enemy + 3].field150[2] = (*pc)[2];
    battle_work_area.records[enemy + 3].field150[0] = (*pc)[3];
}

/* 8007BC40: AI action 3d: list entry halfword at b1 = b2 | b3 << 8 (two byte stores). */
void battle_ai_list_set16(u8 **pc, u8 *list, u8 count) {
    list[count * 8 + (*pc)[1]] = (*pc)[2];
    (&list[count * 8 + (*pc)[1]])[1] = (*pc)[3];
}

/* 8007BC84: AI action 3e: byte variable b1 = random 0..b2. */
void battle_ai_random_b(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = mode_get_random_byte_in_range(0, (*pc)[2]);
}

/* 8007BCE8: AI action 3f: variable b1 = random 0..(b2 | b3 << 8). */
void battle_ai_random_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_random_range(0, op[2] | (op[3] << 8));
}

/* 8007BD5C: AI action 40: variable b1 = the bit of a random party slot passing the
 * targeting test b2 and on foot (800d32a1 clear); 0 when none does. */
void battle_ai_pick_party(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = mode_get_random_byte_in_range(0, 2);
        if (tried[slot] == 0) {
            if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_slot_flags[slot].unk1 == 0) {
                battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* 8007BEA8: AI action 41: as action 40, limited to party slots in the enemy's
 * formation group. */
void battle_ai_pick_party_own_group(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = mode_get_random_byte_in_range(0, 2);
        if (tried[slot] == 0) {
            if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_area_slots[enemy + 3].group == battle_area_slots[slot].group &&
                battle_slot_flags[slot].unk1 == 0) {
                battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* 8007C040: AI action 42: variable b1 = the bit of a random enemy slot passing the
 * targeting test b2 in the enemy's formation group and not flagged at
 * 800d32a1; 0 when none does. */
void battle_ai_pick_enemy_own_group(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_area_slots[enemy + 3].group == battle_area_slots[slot].group &&
            battle_slot_flags[slot].unk1 == 0) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007C1A4: AI action 43: as action 40, limited to party slots outside the enemy's
 * formation group. */
void battle_ai_pick_party_other_group(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = mode_get_random_byte_in_range(0, 2);
        if (tried[slot] == 0) {
            if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_area_slots[enemy + 3].group != battle_area_slots[slot].group &&
                battle_slot_flags[slot].unk1 == 0) {
                battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* 8007C33C: AI action 44: as action 42, limited to enemy slots outside the enemy's
 * formation group. */
void battle_ai_pick_enemy_other_group(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_area_slots[enemy + 3].group != battle_area_slots[slot].group &&
            battle_slot_flags[slot].unk1 == 0) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007C4A0: AI action 45: variable b1 = the bit of the targetable party slot with the
 * lowest turn timer. */
void battle_ai_party_lowest_timer(u8 **pc, u8 enemy) {
    s32 slot;
    u8 lowest = 0xFF;
    s32 target = 0;

    for (slot = 0; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && lowest >= battle_turn_queue.timers[1][slot]) {
            lowest = battle_turn_queue.timers[1][slot];
            target = slot;
        }
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(target);
}

/* 8007C580: AI action 46: variable b1 = the bit of the targetable other enemy slot
 * with the lowest turn timer. */
void battle_ai_enemy_lowest_timer(u8 **pc, u8 enemy) {
    s32 slot;
    u8 lowest = 0xFF;
    s32 target = 0;

    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && lowest >= battle_turn_queue.timers[1][slot] && slot != enemy + 3) {
            lowest = battle_turn_queue.timers[1][slot];
            target = slot;
        }
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(target);
}

/* 8007C678: AI action 47: variable b1 = the bit of the targetable party slot with the
 * lowest HP. */
void battle_ai_party_lowest_hp(u8 **pc, u8 enemy) {
    s32 slot;
    u16 lowest = 0xFFFF;
    s32 target = 0;

    for (slot = 0; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && lowest >= battle_work_area.records[slot].pilot.hp) {
            lowest = battle_work_area.records[slot].pilot.hp;
            target = slot;
        }
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(target);
}

/* 8007C75C: AI action 48: variable b1 = the bit of the targetable enemy slot with the
 * lowest HP. */
void battle_ai_enemy_lowest_hp(u8 **pc, u8 enemy) {
    s32 slot;
    u16 lowest = 0xFFFF;
    s32 target = 0;

    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && lowest >= battle_work_area.records[slot].pilot.hp) {
            lowest = battle_work_area.records[slot].pilot.hp;
            target = slot;
        }
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(target);
}

/* 8007C840: AI action 49: variable b1 = the bit of a random party slot passing the
 * targeting test b2, in a gear (800d32a1) and in the enemy's formation group. */
void battle_ai_pick_party_geared_own_group(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = mode_get_random_byte_in_range(0, 2);
        if (tried[slot] == 0) {
            if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_slot_flags[slot].unk1 != 0 &&
                battle_area_slots[enemy + 3].group == battle_area_slots[slot].group) {
                battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* 8007C9D4: AI action 4a: variable b1 = the bit of a random party slot passing the
 * targeting test b2 and in a gear (800d32a1). */
void battle_ai_pick_party_geared(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = mode_get_random_byte_in_range(0, 2);
        if (tried[slot] == 0) {
            if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_slot_flags[slot].unk1 != 0) {
                battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* 8007CB20: AI action 4b: variable b1 = the bit of a random enemy slot passing the
 * targeting test b2 and in a gear (800d32a1); 0 when none does. */
void battle_ai_pick_enemy_geared(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_slot_flags[slot].unk1 != 0) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007CC50: AI action 4c: byte variable b1 = the number of targetable party slots in
 * formation group b2. */
void battle_ai_count_party_in_group(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 0; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, 0) && battle_area_slots[slot].group == (*pc)[2]) {
            count++;
        }
    }
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = count;
}

/* 8007CD10: AI action 4d: byte variable b1 = the number of targetable enemy slots in
 * formation group b2. */
void battle_ai_count_enemies_in_group(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, 0) && battle_area_slots[slot].group == (*pc)[2]) {
            count++;
        }
    }
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = count;
}

/* 8007CDD0: AI action 4e: byte variable b1 = the formation's group distance from the
 * enemy's group to the group of the first slot in var b2. */
void battle_ai_group_distance(u8 **pc, u8 enemy) {
    u8 slot = battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[2]]);

    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] =
        battle_formation->links[battle_area_slots[enemy + 3].group][battle_area_slots[slot].group].distance;
}

/* 8007CEA4: AI action 4f: byte variable b1 = the number of targetable party slots in
 * the group of the first slot in var b2. */
void battle_ai_count_party_with(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 0; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, 0) &&
            battle_area_slots[slot].group == battle_area_slots[battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[2]])].group) {
            count++;
        }
    }
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = count;
}

/* 8007CFB8: AI action 50: byte variable b1 = the number of targetable enemy slots in
 * the group of the first slot in var b2. */
void battle_ai_count_enemies_with(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, 0) &&
            battle_area_slots[slot].group == battle_area_slots[battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[2]])].group) {
            count++;
        }
    }
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = count;
}

/* 8007D0CC: AI action 51: byte variable b1 = the count held for item b2 (0 when the
 * item is not held). */
void battle_ai_item_count(u8 **pc, u8 enemy) {
    s32 i;

    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = 0;
    for (i = 0; i < 0x30; i++) {
        if (battle_item_ids[i] == (*pc)[2]) {
            battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = battle_item_counts[i];
            return;
        }
    }
}

/* 8007D148: AI action 52: list entry halfword at b1 = variable b2 (two byte stores). */
void battle_ai_list_set_v(u8 **pc, u8 *list, u8 enemy, u8 count) {
    u16 value = battle_enemy_ai_blocks[enemy].vars[(*pc)[2]];

    list[count * 8 + (*pc)[1]] = value;
    (&list[count * 8 + (*pc)[1]])[1] = value >> 8;
}

/* 8007D1A8: AI action 53: long b1 = the party's gold. */
void battle_ai_get_gold(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].longs[(*pc)[1]] = game_data.gold;
}

/* 8007D1DC: AI action 54: variable b1 = the bit of a random slot passing 8007a744
 * whose record byte +0x56 is b2; 0 when none does. */
void battle_ai_pick_character(u8 **pc, u8 enemy) {
    u8 candidates[11];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (battle_is_slot_visible_and_alive(slot) && battle_work_area.records[slot].pilot.characterId == (*pc)[2]) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007D30C: AI action 55: long b1 = the enemy's result code (800d2c8b, the work
 * table's resultCode[3 + enemy]). */
void battle_ai_get_result_code(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].longs[(*pc)[1]] = battle_enemy_result_codes[enemy];
}

/* 8007D344: AI action 56: variable b1 = the bit of a random enemy slot passing
 * 8007a6c8(b2) with slot info +3 bit 0x80; 0 when none does. */
void battle_ai_pick_enemy_80(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (battle_is_slot_targetable_even_hidden(slot, (*pc)[2]) && (battle_area_slots[slot].hidden & 0x80)) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007D478: AI action 57: variable b1 = the bit of a random party slot flagged 0x8000
 * at +0x7c without 0x4002. */
void battle_ai_pick_knocked_out_party(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;
    u16 flags;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = mode_get_random_byte_in_range(0, 2);
        if (tried[slot] == 0) {
            flags = battle_work_area.records[slot].pilot.status7C;
            if ((flags & 0x8000) && !(flags & 0x4002)) {
                battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* 8007D5B0: AI action 58: byte variable b1 = the number of party slots not down
 * (+0x7c without 0xc000). */
void battle_ai_count_party_up(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 0; slot < 3; slot++) {
        if (!(battle_work_area.records[slot].pilot.status7C & 0xC000)) {
            count++;
        }
    }
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = count;
}

/* 8007D610: AI action 59: byte variable b1 = the number of present, visible enemy
 * slots not down. */
void battle_ai_count_enemies_up(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 3; slot < 11; slot++) {
        if (battle_turn_queue.present[slot] != 0 && !(battle_work_area.records[slot].pilot.status7C & 0xC000) && battle_area_slots[slot].hidden == 0) {
            count++;
        }
    }
    battle_enemy_ai_blocks[enemy].bytes[(*pc)[1]] = count;
}

/* 8007D6A8: AI action 5a: variable b1 = the bit of the targetable party slot flagged
 * at 800d32a1 with the lowest record +0x104. */
void battle_ai_party_geared_lowest_gear_hp(u8 **pc, u8 enemy) {
    s32 slot;
    u32 lowest = 0xFFFFFFFF;
    s32 target = 0;

    for (slot = 0; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_slot_flags[slot].unk1 != 0 && lowest >= battle_work_area.records[slot].gear.hp) {
            lowest = battle_work_area.records[slot].gear.hp;
            target = slot;
        }
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(target);
}

/* 8007D7B4: AI action 5b: variable b1 = the bit of the targetable enemy slot flagged
 * at 800d32a1 with the lowest HP. */
void battle_ai_enemy_geared_lowest_hp(u8 **pc, u8 enemy) {
    s32 slot;
    u32 lowest = 0xFFFFFFFF;
    s32 target = 0;

    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, (*pc)[2]) && battle_slot_flags[slot].unk1 != 0 && lowest >= battle_work_area.records[slot].pilot.hp) {
            lowest = battle_work_area.records[slot].pilot.hp;
            target = slot;
        }
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(target);
}

/* 8007D8C0: AI action 5c: variable b2 = the bit of a random targetable party slot whose
 * 16-bit attribute b1 shares a bit with var b3; 0 when none does. */
void battle_ai_pick_party_attr(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, 0) && (battle_enemy_ai_blocks[enemy].vars[(*pc)[3]] & battle_access_combatant_attr16(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007DA1C: AI action 5d: as action 5c for the enemy slots. */
void battle_ai_pick_enemy_attr(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = 0;
    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, 0) && (battle_enemy_ai_blocks[enemy].vars[(*pc)[3]] & battle_access_combatant_attr16(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007DB78: AI action 5e: as action 5c, limited to party slots in a gear (800d32a1). */
void battle_ai_pick_party_geared_attr(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, 0) && battle_slot_flags[slot].unk1 != 0 &&
            (battle_enemy_ai_blocks[enemy].vars[(*pc)[3]] & battle_access_combatant_attr16(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007DCF8: AI action 5f: as action 5d, limited to enemy slots in a gear (800d32a1). */
void battle_ai_pick_enemy_geared_attr(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 11; slot++) {
        if (battle_is_slot_targetable(slot, 0) && battle_slot_flags[slot].unk1 != 0 &&
            (battle_enemy_ai_blocks[enemy].vars[(*pc)[3]] & battle_access_combatant_attr16(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007DE78: AI action 60: as action 5c with any targetable party slot. */
void battle_ai_pick_party_attr_any(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, 1) && (battle_enemy_ai_blocks[enemy].vars[(*pc)[3]] & battle_access_combatant_attr16(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007DFD4: AI action 61: as action 5e with any targetable party slot. */
void battle_ai_pick_party_geared_attr_any(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (battle_is_slot_targetable(slot, 1) && battle_slot_flags[slot].unk1 != 0 &&
            (battle_enemy_ai_blocks[enemy].vars[(*pc)[3]] & battle_access_combatant_attr16(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        battle_enemy_ai_blocks[enemy].vars[(*pc)[2]] = battle_get_slot_bit(candidates[mode_get_random_byte_in_range(0, count - 1)]);
    }
}

/* 8007E154: AI action 63: set the enemy's slot info +3 to b1; with bit 0x80 the enemy
 * rejoins its own group, otherwise it leaves its formation group. */
void battle_ai_set_hidden(u8 **pc, u8 enemy) {
    battle_area_slots[enemy + 3].hidden = (*pc)[1];
    if ((*pc)[1] & 0x80) {
        battle_join_target_group(enemy + 3, enemy + 3);
    } else {
        battle_leave_formation_group(enemy + 3);
    }
}

/* 8007E1D0: AI action 64: variable b1 = the enemy's own slot bit. */
void battle_ai_self_bit(u8 **pc, u8 enemy) {
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = battle_get_slot_bit(enemy + 3);
}

/* 8007E234: AI action 65: variable b1 = the mask of party slots passing 8007a744 and
 * (b2 1) in a gear, (b2 2) on foot (800d32a1), or (other b2) any. */
void battle_ai_party_mask(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 2; slot >= 0; slot--) {
        if (battle_is_slot_visible_and_alive(slot)) {
            switch ((*pc)[2]) {
            case 1:
                if (battle_slot_flags[slot].unk1 != 0) {
                    mask |= 1;
                }
                break;
            case 2:
                if (battle_slot_flags[slot].unk1 == 0) {
                    mask |= 1;
                }
                break;
            default:
                mask |= 1;
                break;
            }
        }
        mask <<= 1;
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = mask >> 1;
}

/* 8007E334: AI action 66: variable b1 = the mask of enemy slots passing 8007a744 and
 * (b2 1) in a gear, (b2 2) on foot (800d32a1), or (other b2) any. */
void battle_ai_enemy_mask(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 10; slot >= 3; slot--) {
        if (battle_is_slot_visible_and_alive(slot)) {
            switch ((*pc)[2]) {
            case 1:
                if (battle_slot_flags[slot].unk1 != 0) {
                    mask |= 1;
                }
                break;
            case 2:
                if (battle_slot_flags[slot].unk1 == 0) {
                    mask |= 1;
                }
                break;
            default:
                mask |= 1;
                break;
            }
        }
        mask <<= 1;
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = mask << 2;
}

/* 8007E438: AI action 67: variable b1 = the mask of party slots passing 8007a744 in the
 * group of the first slot in var b2. */
void battle_ai_party_group_mask(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 2; slot >= 0; slot--) {
        if (battle_is_slot_visible_and_alive(slot) &&
            battle_area_slots[battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[2]])].group == battle_area_slots[slot].group) {
            mask |= 1;
        }
        mask <<= 1;
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = mask >> 1;
}

/* 8007E554: AI action 68: variable b1 = the mask of enemy slots passing 8007a744 in the
 * group of the first slot in var b2. */
void battle_ai_enemy_group_mask(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 10; slot >= 3; slot--) {
        if (battle_is_slot_visible_and_alive(slot) &&
            battle_area_slots[battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[2]])].group == battle_area_slots[slot].group) {
            mask |= 1;
        }
        mask <<= 1;
    }
    battle_enemy_ai_blocks[enemy].vars[(*pc)[1]] = mask << 2;
}

/* 8007E674: AI action 69: clear the enemy's pending amount (800d2c60, damage[3 +
 * enemy]) and set its result code (800d2c8b) to 4, which no results pass
 * applies. */
void battle_ai_clear_result(u8 enemy) {
    battle_enemy_damages[enemy] = 0;
    battle_enemy_result_codes[enemy] = 4;
}

/* 8007E6A0: AI action 6a: long b3 = long b1 + long b2. */
void battle_ai_add_ll(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].longs[op[3]] = battle_enemy_ai_blocks[enemy].longs[op[1]] + battle_enemy_ai_blocks[enemy].longs[op[2]];
}

/* 8007E6F0: AI action 6b: long b3 = long b1 - long b2. */
void battle_ai_sub_ll(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].longs[op[3]] = battle_enemy_ai_blocks[enemy].longs[op[1]] - battle_enemy_ai_blocks[enemy].longs[op[2]];
}

/* 8007E740: AI action 6c: long b1 *= b2. */
void battle_ai_mul_l(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].longs[op[1]] *= op[2];
}

/* 8007E780: AI action 6d: long b1 /= b2 (unsigned). */
void battle_ai_divu_l(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    battle_enemy_ai_blocks[enemy].longs[op[1]] = (u32)battle_enemy_ai_blocks[enemy].longs[op[1]] / op[2];
}

/* 8007E7C0: AI action 70: the battle event script's variable b1 (ovl3087's
 * ScriptState.vars) = b2. */
void battle_ai_set_event_variable(u8 **pc) {
    u8 *op = *pc;

    battle_state_of_event_script->vars[op[1]] = op[2];
}

/* 8007E7E4: AI action 71: set (b2 != 0) or clear flag b1 + 7 in every party record's
 * +0x7a. */
void battle_ai_set_party_command_seal(u8 **pc, u8 enemy) {
    s32 i;
    u8 set = (*pc)[2] != 0;

    for (i = 0; i < 3; i++) {
        if (set) {
            battle_work_area.records[i].pilot.status7A |= battle_get_flag_bit((*pc)[1] + 7);
        } else {
            battle_work_area.records[i].pilot.status7A &= ~battle_get_flag_bit((*pc)[1] + 7);
        }
    }
}

/* 8007E8AC: AI action 72: formation group distance b1 -> b2 = b3. */
void battle_ai_set_group_distance(u8 **pc) {
    u8 *op = *pc;

    battle_formation->links[op[1]][op[2]].distance = op[3];
}

/* 8007E8E0: AI action 73: the first slot of var b1 takes the next turn. */
void battle_ai_next_turn(u8 **pc, u8 enemy) {
    battle_forced_next_turn = battle_find_first_slot_in_mask(battle_enemy_ai_blocks[enemy].vars[(*pc)[1]]) + 1;
}

/* 8007E934: AI action 74: reset every slot's turn timers (80078508), its order output
 * going to a scratch buffer. */
void battle_ai_reset_timers(void) {
    u8 order[16];

    battle_reset_turn_timers(order);
}

/* 8007E954: AI condition 81: byte variable b1 == b2. */
s32 battle_ai_eq_b(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return battle_enemy_ai_blocks[enemy].bytes[op[1]] == op[2];
}

/* 8007E98C: AI condition 82: variable b1 == b2 | b3 << 8. */
s32 battle_ai_eq_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return battle_enemy_ai_blocks[enemy].vars[op[1]] == value;
}

/* 8007E9D0: AI condition 83: byte variable b1 <= b2. */
s32 battle_ai_le_b(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return battle_enemy_ai_blocks[enemy].bytes[op[1]] <= op[2];
}

/* 8007EA08: AI condition 84: variable b1 <= b2 | b3 << 8. */
s32 battle_ai_le_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return battle_enemy_ai_blocks[enemy].vars[op[1]] <= value;
}

/* 8007EA4C: AI condition 85: byte variable b1 >= b2. */
s32 battle_ai_ge_b(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return battle_enemy_ai_blocks[enemy].bytes[op[1]] >= op[2];
}

/* 8007EA84: AI condition 86: variable b1 >= b2 | b3 << 8. */
s32 battle_ai_ge_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return battle_enemy_ai_blocks[enemy].vars[op[1]] >= value;
}

/* 8007EAC8: AI condition 87: byte variable b1 == byte variable b2. */
s32 battle_ai_eq_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    return bytes[op[1]] == bytes[op[2]];
}

/* 8007EB08: AI condition 88: variable b1 == variable b2. */
s32 battle_ai_eq_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;

    return vars[op[1]] == vars[op[2]];
}

/* 8007EB50: AI condition 89: byte variable b1 <= byte variable b2. */
s32 battle_ai_le_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    return bytes[op[1]] <= bytes[op[2]];
}

/* 8007EB90: AI condition 8a: variable b1 <= variable b2. */
s32 battle_ai_le_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;

    return vars[op[1]] <= vars[op[2]];
}

/* 8007EBD8: AI condition 8b: byte variable b1 & b2. */
s32 battle_ai_test_b(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return (battle_enemy_ai_blocks[enemy].bytes[op[1]] & op[2]) != 0;
}

/* 8007EC10: AI condition 8c: variable b1 & (b2 | b3 << 8). */
s32 battle_ai_test_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] + (op[3] << 8);

    return (battle_enemy_ai_blocks[enemy].vars[op[1]] & value) != 0;
}

/* 8007EC54: AI condition 8d: byte variable b1 & byte variable b2. */
s32 battle_ai_test_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    return (bytes[op[1]] & bytes[op[2]]) != 0;
}

/* 8007EC94: AI condition 8e: variable b1 & variable b2. */
s32 battle_ai_test_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;

    return (vars[op[1]] & vars[op[2]]) != 0;
}

/* 8007ECDC: AI condition 8f: byte variable b1 != b2. */
s32 battle_ai_ne_b(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return battle_enemy_ai_blocks[enemy].bytes[op[1]] != op[2];
}

/* 8007ED14: AI condition 90: variable b1 != b2 | b3 << 8. */
s32 battle_ai_ne_v(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return battle_enemy_ai_blocks[enemy].vars[op[1]] != value;
}

/* 8007ED58: AI condition 91: byte variable b1 != byte variable b2. */
s32 battle_ai_ne_bb(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = battle_enemy_ai_blocks[enemy].bytes;

    return bytes[op[1]] != bytes[op[2]];
}

/* 8007ED98: AI condition 92: variable b1 != variable b2. */
s32 battle_ai_ne_vv(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = battle_enemy_ai_blocks[enemy].vars;

    return vars[op[1]] != vars[op[2]];
}

/* 8007EDE0: AI condition 93: long b1 == long b2. */
s32 battle_ai_eq_ll(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 *longs = battle_enemy_ai_blocks[enemy].longs;

    return longs[op[1]] == longs[op[2]];
}

/* 8007EE28: AI condition 94: long b1 <= long b2 (unsigned). */
s32 battle_ai_leu_ll(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u32 *longs = (u32 *)battle_enemy_ai_blocks[enemy].longs;

    return longs[op[1]] <= longs[op[2]];
}

/* 8007EE70: AI condition 95: slot b1's record +0x7c bit 0x8000. */
s32 battle_ai_slot_knocked_out(u8 **pc) {
    return battle_work_area.records[(*pc)[1]].pilot.status7C >> 15;
}

/* 8007EEA8: AI condition 96: formation group b1 is empty. */
s32 battle_ai_group_empty(u8 **pc) {
    return battle_formation_groups[(*pc)[1]].count == 0;
}

/* 8007EED0: AI condition 97: no party member (slots 0 and 1) is alive. */
s32 battle_ai_alive_low_clear(void) {
    return (battle_alive_mask & 3) == 0;
}

/* 8007EEE8: AI condition 98: false while any enemy without slot info bit 0x80 is
 * listed and the alive mask has a bit above 4 set. */
s32 battle_ai_alive_high_clear(void) {
    s32 result = 1;
    s32 i;

    for (i = 0; i < 8; i++) {
        if ((battle_alive_mask >> 5) != 0 && !(battle_area_slots[i + 3].hidden & 0x80)) {
            result = 0;
            break;
        }
    }
    return result;
}

/* 8007EF44: AI condition 9b: the enemy's slot info +3 bit 0x80. */
s32 battle_ai_own_hidden_80(u8 enemy) {
    return battle_area_slots[enemy + 3].hidden >> 7;
}

/* 8007EF6C: Run the AI action at *pc (opcodes 01-74; 62 does nothing here, 00, 6e,
 * 6f and 75-7f queue the opcode as an action, 8007a7bc) for `enemy`, with
 * `count` actions in the list at 800d2e5c, and step past it. Returns the new
 * action count. Instructions are op, b1, b2, b3 (docs/scripts/battle-ai.md,
 * tools/analysis/battle_ai.py). */
u8 battle_ai_run_action(u8 **pc, u8 enemy, u8 count) {
    u8 *list = (u8 *)battle_action_list;

    switch (**pc) {
    case 0x01:
        count = battle_ai_list_set(pc, list, count);
        break;
    case 0x02:
        battle_ai_list_set_b(pc, list, enemy, count);
        break;
    case 0x03:
        battle_ai_list_copy(pc, list);
        break;
    case 0x04:
        battle_ai_set_b(pc, enemy);
        break;
    case 0x05:
        battle_ai_set_v(pc, enemy);
        break;
    case 0x06:
        battle_ai_set_l16(pc, enemy);
        break;
    case 0x07:
        battle_ai_set_res(pc);
        break;
    case 0x08:
        battle_ai_add_b(pc, enemy);
        break;
    case 0x09:
        battle_ai_sub_b(pc, enemy);
        break;
    case 0x0A:
        battle_ai_mul_b(pc, enemy);
        break;
    case 0x0B:
        battle_ai_div_b(pc, enemy);
        break;
    case 0x0C:
        battle_ai_mod_b(pc, enemy);
        break;
    case 0x0D:
        battle_ai_and_b(pc, enemy);
        break;
    case 0x0E:
        battle_ai_or_b(pc, enemy);
        break;
    case 0x0F:
        battle_ai_xor_b(pc, enemy);
        break;
    case 0x10:
        battle_ai_add_v(pc, enemy);
        break;
    case 0x11:
        battle_ai_sub_v(pc, enemy);
        break;
    case 0x12:
        battle_ai_mul_v(pc, enemy);
        break;
    case 0x13:
        battle_ai_div_v(pc, enemy);
        break;
    case 0x14:
        battle_ai_mod_v(pc, enemy);
        break;
    case 0x15:
        battle_ai_and_v(pc, enemy);
        break;
    case 0x16:
        battle_ai_or_v(pc, enemy);
        break;
    case 0x17:
        battle_ai_xor_v(pc, enemy);
        break;
    case 0x18:
        battle_ai_add_bb(pc, enemy);
        break;
    case 0x19:
        battle_ai_sub_bb(pc, enemy);
        break;
    case 0x1A:
        battle_ai_mul_bb(pc, enemy);
        break;
    case 0x1B:
        battle_ai_div_bb(pc, enemy);
        break;
    case 0x1C:
        battle_ai_mod_bb(pc, enemy);
        break;
    case 0x1D:
        battle_ai_and_bb(pc, enemy);
        break;
    case 0x1E:
        battle_ai_or_bb(pc, enemy);
        break;
    case 0x1F:
        battle_ai_xor_bb(pc, enemy);
        break;
    case 0x20:
        battle_ai_add_vv(pc, enemy);
        break;
    case 0x21:
        battle_ai_sub_vv(pc, enemy);
        break;
    case 0x22:
        battle_ai_mul_vv(pc, enemy);
        break;
    case 0x23:
        battle_ai_div_vv(pc, enemy);
        break;
    case 0x24:
        battle_ai_mod_vv(pc, enemy);
        break;
    case 0x25:
        battle_ai_and_vv(pc, enemy);
        break;
    case 0x26:
        battle_ai_or_vv(pc, enemy);
        break;
    case 0x27:
        battle_ai_xor_vv(pc, enemy);
        break;
    case 0x28:
        battle_ai_set_own_attr8(pc, enemy);
        break;
    case 0x29:
        battle_ai_set_own_attr16(pc, enemy);
        break;
    case 0x2A:
        battle_ai_get_attr8(pc, enemy);
        break;
    case 0x2B:
        count = battle_ai_put_attr8(pc, enemy, count);
        break;
    case 0x2C:
        battle_ai_get_attr16(pc, enemy);
        break;
    case 0x2D:
        count = battle_ai_put_attr16(pc, enemy, count);
        break;
    case 0x2E:
        battle_ai_get_long(pc, enemy);
        break;
    case 0x2F:
        count = battle_ai_put_long(pc, enemy, count);
        break;
    case 0x30:
        battle_ai_get_res(pc, enemy);
        break;
    case 0x31:
        battle_ai_put_res(pc, enemy);
        break;
    case 0x32:
        battle_ai_copy_b(pc, enemy);
        break;
    case 0x33:
        battle_ai_copy_v(pc, enemy);
        break;
    case 0x34:
        battle_ai_copy_l(pc, enemy);
        break;
    case 0x35:
        battle_ai_b_to_v(pc, enemy);
        break;
    case 0x36:
        battle_ai_v_to_l(pc, enemy);
        break;
    case 0x37:
        battle_ai_clear_b(enemy);
        break;
    case 0x38:
        battle_ai_clear_v(enemy);
        break;
    case 0x39:
        battle_ai_set_experience(pc, enemy);
        break;
    case 0x3A:
        battle_ai_set_gold(pc, enemy);
        break;
    case 0x3B:
        battle_ai_set_second_drop(pc, enemy);
        break;
    case 0x3C:
        battle_ai_set_drop(pc, enemy);
        break;
    case 0x3D:
        battle_ai_list_set16(pc, list, count);
        break;
    case 0x3E:
        battle_ai_random_b(pc, enemy);
        break;
    case 0x3F:
        battle_ai_random_v(pc, enemy);
        break;
    case 0x40:
        battle_ai_pick_party(pc, enemy);
        break;
    case 0x41:
        battle_ai_pick_party_own_group(pc, enemy);
        break;
    case 0x42:
        battle_ai_pick_enemy_own_group(pc, enemy);
        break;
    case 0x43:
        battle_ai_pick_party_other_group(pc, enemy);
        break;
    case 0x44:
        battle_ai_pick_enemy_other_group(pc, enemy);
        break;
    case 0x45:
        battle_ai_party_lowest_timer(pc, enemy);
        break;
    case 0x46:
        battle_ai_enemy_lowest_timer(pc, enemy);
        break;
    case 0x47:
        battle_ai_party_lowest_hp(pc, enemy);
        break;
    case 0x48:
        battle_ai_enemy_lowest_hp(pc, enemy);
        break;
    case 0x49:
        battle_ai_pick_party_geared_own_group(pc, enemy);
        break;
    case 0x4A:
        battle_ai_pick_party_geared(pc, enemy);
        break;
    case 0x4B:
        battle_ai_pick_enemy_geared(pc, enemy);
        break;
    case 0x4C:
        battle_ai_count_party_in_group(pc, enemy);
        break;
    case 0x4D:
        battle_ai_count_enemies_in_group(pc, enemy);
        break;
    case 0x4E:
        battle_ai_group_distance(pc, enemy);
        break;
    case 0x4F:
        battle_ai_count_party_with(pc, enemy);
        break;
    case 0x50:
        battle_ai_count_enemies_with(pc, enemy);
        break;
    case 0x51:
        battle_ai_item_count(pc, enemy);
        break;
    case 0x52:
        battle_ai_list_set_v(pc, list, enemy, count);
        break;
    case 0x53:
        battle_ai_get_gold(pc, enemy);
        break;
    case 0x54:
        battle_ai_pick_character(pc, enemy);
        break;
    case 0x55:
        battle_ai_get_result_code(pc, enemy);
        break;
    case 0x56:
        battle_ai_pick_enemy_80(pc, enemy);
        break;
    case 0x57:
        battle_ai_pick_knocked_out_party(pc, enemy);
        break;
    case 0x58:
        battle_ai_count_party_up(pc, enemy);
        break;
    case 0x59:
        battle_ai_count_enemies_up(pc, enemy);
        break;
    case 0x5A:
        battle_ai_party_geared_lowest_gear_hp(pc, enemy);
        break;
    case 0x5B:
        battle_ai_enemy_geared_lowest_hp(pc, enemy);
        break;
    case 0x5C:
        battle_ai_pick_party_attr(pc, enemy);
        break;
    case 0x5D:
        battle_ai_pick_enemy_attr(pc, enemy);
        break;
    case 0x5E:
        battle_ai_pick_party_geared_attr(pc, enemy);
        break;
    case 0x5F:
        battle_ai_pick_enemy_geared_attr(pc, enemy);
        break;
    case 0x60:
        battle_ai_pick_party_attr_any(pc, enemy);
        break;
    case 0x61:
        battle_ai_pick_party_geared_attr_any(pc, enemy);
        break;
    case 0x62:
        /* only noted by the reaction script runner (80079ab0) */
        break;
    case 0x63:
        battle_ai_set_hidden(pc, enemy);
        break;
    case 0x64:
        battle_ai_self_bit(pc, enemy);
        break;
    case 0x65:
        battle_ai_party_mask(pc, enemy);
        break;
    case 0x66:
        battle_ai_enemy_mask(pc, enemy);
        break;
    case 0x67:
        battle_ai_party_group_mask(pc, enemy);
        break;
    case 0x68:
        battle_ai_enemy_group_mask(pc, enemy);
        break;
    case 0x69:
        battle_ai_clear_result(enemy);
        break;
    case 0x6A:
        battle_ai_add_ll(pc, enemy);
        break;
    case 0x6B:
        battle_ai_sub_ll(pc, enemy);
        break;
    case 0x6C:
        battle_ai_mul_l(pc, enemy);
        break;
    case 0x6D:
        battle_ai_divu_l(pc, enemy);
        break;
    case 0x70:
        battle_ai_set_event_variable(pc);
        break;
    case 0x71:
        battle_ai_set_party_command_seal(pc, enemy);
        break;
    case 0x72:
        battle_ai_set_group_distance(pc);
        break;
    case 0x73:
        battle_ai_next_turn(pc, enemy);
        break;
    case 0x74:
        battle_ai_reset_timers();
        break;
    default:
        count = battle_ai_queue_undefined_opcode(pc, list, enemy, count);
        break;
    }
    battle_ai_step_instruction(pc);
    return count;
}

/* 8007F8C0: Evaluate the AI condition at *pc (opcodes 80-9b; 80 and 9a always hold)
 * and step past it. After opcode 99 the following conditions are or-ed
 * together up to the next non-condition byte. */
u8 battle_ai_evaluate_condition(u8 **pc, u8 enemy) {
    u8 result;
    u8 chained;
    u8 any;

    result = 0;
    chained = 0;
    any = 0;
    for (;;) {
        switch (**pc) {
        case 0x80:
        case 0x9A:
            result = 1;
            break;
        case 0x99:
            chained = 1;
            break;
        case 0x81:
            result = battle_ai_eq_b(pc, enemy);
            break;
        case 0x82:
            result = battle_ai_eq_v(pc, enemy);
            break;
        case 0x83:
            result = battle_ai_le_b(pc, enemy);
            break;
        case 0x84:
            result = battle_ai_le_v(pc, enemy);
            break;
        case 0x85:
            result = battle_ai_ge_b(pc, enemy);
            break;
        case 0x86:
            result = battle_ai_ge_v(pc, enemy);
            break;
        case 0x87:
            result = battle_ai_eq_bb(pc, enemy);
            break;
        case 0x88:
            result = battle_ai_eq_vv(pc, enemy);
            break;
        case 0x89:
            result = battle_ai_le_bb(pc, enemy);
            break;
        case 0x8A:
            result = battle_ai_le_vv(pc, enemy);
            break;
        case 0x8B:
            result = battle_ai_test_b(pc, enemy);
            break;
        case 0x8C:
            result = battle_ai_test_v(pc, enemy);
            break;
        case 0x8D:
            result = battle_ai_test_bb(pc, enemy);
            break;
        case 0x8E:
            result = battle_ai_test_vv(pc, enemy);
            break;
        case 0x8F:
            result = battle_ai_ne_b(pc, enemy);
            break;
        case 0x90:
            result = battle_ai_ne_v(pc, enemy);
            break;
        case 0x91:
            result = battle_ai_ne_bb(pc, enemy);
            break;
        case 0x92:
            result = battle_ai_ne_vv(pc, enemy);
            break;
        case 0x93:
            result = battle_ai_eq_ll(pc, enemy);
            break;
        case 0x94:
            result = battle_ai_leu_ll(pc, enemy);
            break;
        case 0x95:
            result = battle_ai_slot_knocked_out(pc);
            break;
        case 0x96:
            result = battle_ai_group_empty(pc);
            break;
        case 0x97:
            result = battle_ai_alive_low_clear();
            break;
        case 0x98:
            result = battle_ai_alive_high_clear();
            break;
        case 0x9B:
            result = battle_ai_own_hidden_80(enemy);
            break;
        }
        battle_ai_step_instruction(pc);
        if (!chained) {
            break;
        }
        any |= result;
        if (**pc < 0x80) {
            result = any;
            break;
        }
    }
    return result;
}

/* 8007FB70: For party character 4 with its UI flag +0x8e set: close window 0 and
 * release the graphics block. */
void battle_ammo_window_release(u8 member) {
    if (battle_party_character_ids[member] == 4 && battle_ui->unk8E != 0) {
        battle_window_close(0);
        battle_release_list_page_block();
        battle_ui->unk8E = 0;
    }
}

/* 8007FBE0: Place the separator lines of a `count`-row list (rows from the 800c3214
 * table) and remember the selected row (clamped below `count`). */
void battle_command_menu_place_ap_separators(u8 count, u8 selected) {
    s32 i;

    if (count == selected) {
        selected--;
    }
    for (i = 0; i < count - 1; i++) {
        setXY2(&battle_graphics->unk908[i * 2 + battle_drawing_state.buffer], 0xC, battle_separator_rows_by_list_size[count - 3][i] + 0x5E, 0x12,
               battle_separator_rows_by_list_size[count - 3][i] + 0x5E);
    }
    battle_ui->unk97 = selected;
    battle_ui->unk98 = battle_drawing_state.buffer;
}

/* 8007FCE8: Release the loaded menu module block (UI +0xae). */
void battle_command_menu_release_module_block(void) {
    if (battle_ui->unkAE != 0) {
        heap_free(battle_command_menu_module_block);
        battle_ui->unkAE = 0;
    }
}

/* 8007FD38: Load file 2 (a member in a gear (800d32a1)) or 1 into a new heap block
 * unless loaded (UI +0xae). */
void battle_command_menu_load_module_block(u8 member) {
    s32 file;

    if (battle_ui->unkAE == 0) {
        battle_cd_wait_for_reads();
        file = 2;
        if (battle_slot_flags[member].unk1 == 0) {
            file = 1;
        }
        battle_cd_select_menu_directory();
        battle_command_menu_module_block = (void *)battle_heap_alloc(cd_get_aligned_file_size(file), 0);
        cd_read_file(file, battle_command_menu_module_block, 0, 0x80);
        battle_cd_wait_for_reads();
        battle_ui->unkAE = 1;
    }
}

/* 8007FDEC: Release the loaded file-3 block (UI +0x96). */
void battle_command_menu_release_file3_block(void) {
    if (battle_ui->unk96 != 0) {
        heap_free(battle_command_menu_file3_block);
        battle_ui->unk96 = 0;
    }
}

/* 8007FE3C: Load file 3 into a new heap block unless loaded (UI +0x96). */
void battle_command_menu_load_file3_block(void) {
    if (battle_ui->unk96 == 0) {
        battle_cd_wait_for_reads();
        battle_cd_select_menu_directory();
        battle_command_menu_file3_block = (void *)battle_heap_alloc(cd_get_aligned_file_size(3), 0);
        cd_read_file(3, battle_command_menu_file3_block, 0, 0x80);
        battle_cd_wait_for_reads();
        battle_ui->unk96 = 1;
    }
}

/* 8007FEC4: Allocate and clear the 0x5da4-byte *800d2db4 block. */
void battle_alloc_hud_lists(void) {
    battle_hud_primitive_lists = (ListPrims *)battle_heap_alloc(0x5DA4, 0);
    bzero((u_char *)battle_hud_primitive_lists, 0x5DA4);
    battle_hud_primitive_lists->lineX = 0xA0;
    battle_hud_primitive_lists->lineY = 0x64;
}

/* The digit strings of a turn slot as one byte run (TurnSlot.digits),
 * addressed by shifting the slot index (slots are 0x40 bytes). */
#define SLOT_DIGITS(m) ((u8 *)battle_turn_state + ((m) << 6) + 8)

/* 8007FF14: Build the member's number strings 2-4 (glyphs 0x83 + digit, no leading
 * zeros) from 800d2c0c, set up the list block and, unless the member is
 * character 7, the menu lists; then show them. */
void battle_gear_hud_show(u8 member) {
    s32 k;
    s32 n;
    u8 value;
    u8 digit;

    battle_gear_hud_fill(member);
    for (k = 2; k < 5; k++) {
        value = *battle_gear_hud[k - 2];
        SLOT_DIGITS(member)[k * 4] = 0xFF;
        SLOT_DIGITS(member)[k * 4 + 1] = 0xFF;
        SLOT_DIGITS(member)[k * 4 + 2] = 0xFF;
        n = 0;
        digit = value / 100;
        if (digit != 0) {
            value -= digit * 100;
            n = 1;
            SLOT_DIGITS(member)[k * 4] = digit + 0x83;
        }
        digit = value / 10;
        if (digit != 0 || n != 0) {
            value -= digit * 10;
            SLOT_DIGITS(member)[k * 4 + n++] = digit + 0x83;
        }
        SLOT_DIGITS(member)[k * 4 + n] = value + 0x83;
    }
    battle_alloc_hud_lists();
    if (battle_party_character_ids[member] != 7) {
        battle_gear_hud_build_lists(member);
        battle_wait_frame();
        battle_gear_hud_build_fuel_glyphs(member);
        battle_wait_frame();
        battle_ui->unkAD = 1;
        battle_ui->unkC7 = 1;
    }
}

/* 800800E8: Leave a member's menu: clear UI +0xad, +0xc7, +0xa8, turn a 2 at 800d32a1
 * into 1 and release *800d2db4. */
void battle_leave_member_menu(u8 member) {
    battle_ui->unkAD = 0;
    battle_ui->unkC7 = 0;
    battle_ui->unkA8 = 0;
    if (battle_slot_flags[member].unk1 == 2) {
        battle_slot_flags[member].unk1 = 1;
    }
    heap_free(battle_hud_primitive_lists);
}

/* 80080160: Run party member `member`'s command menu: reset the turn state's menu
 * fields, take the AP (800d32a0 +4) as spent and available, wait for its
 * panel to open, mark the unavailable menu items (status +0x7a bits; with
 * character 7 in a gear two more, with character 4 items 0/2 while its
 * first/fourth ammo is spent), open the first page (1 attack, 2 when the
 * status bars it, 4 with item 9 blocked; 0x10/0x13 in a gear) and frame the
 * member and its target. Then, until the menu is done, the battle ends or
 * an event runs, redraw a changed page (8008d598) and run the page's
 * handler (pages 0x64/0x65 redraw 5/0x19). Finally close the panel, return
 * unspent AP when +0x2e3 is set (up to 28), tidy the windows when done and
 * reload the member's turn timer. */
void battle_command_menu_run(u8 member) {
    s32 i;
    u8 target;

    battle_turn_state->unk2EA = 1;
    battle_turn_state->unk2E9 = 0;
    battle_turn_state->menuDone = 0;
    battle_turn_state->unk2E0 = 0;
    battle_turn_state->unk2E1[0] = 0;
    battle_turn_state->unk2E1[1] = 0xFF;
    battle_turn_state->unk2E1[2] = 0;
    battle_turn_state->unk2E1[3] = 0;
    battle_turn_state->unk2DC = 0;
    battle_turn_state->unk2D4[0] = battle_turn_state->unk2D4[1] = battle_slot_flags[member].unk2[2];
    battle_ui->unk90[member] = 3;
    battle_turn_state->unk2E7 = 0;
    battle_direction_input[1] = 0xFF;
    battle_unread_gear_attack_step_flag = 0;
    while (battle_ui->unk90[member] != 1) {
        battle_wait_frame();
    }
    battle_command_menu_sounds_enabled = 1;
    battle_play_menu_sound(0x5A);
    battle_command_menu_sounds_enabled = 0;
    for (i = 0; i < 16; i++) {
        battle_turn_state->slots[member].items[i] = battle_work_area.records[member].pilot.status7A & battle_command_seal_bits[i];
    }
    if (battle_party_character_ids[member] == 7 && battle_slot_flags[member].unk1 != 0) {
        battle_turn_state->slots[member].items[13] = battle_command_seal_bits[13];
        battle_turn_state->slots[member].items[4] = battle_command_seal_bits[4];
    }
    if (battle_party_character_ids[member] == 4) {
        if (battle_slot_flags[member].unk1 == 0) {
            if (game_data.ammo[game_data.characters[4].entryItems[0] - 50] == 0) {
                battle_turn_state->slots[member].items[0] = battle_command_seal_bits[0];
            }
            if (game_data.ammo[game_data.characters[4].entryItems[3] - 50] == 0) {
                battle_turn_state->slots[member].items[2] = battle_command_seal_bits[2];
            }
        } else {
            if (game_data.gearAmmo[game_data.gears[game_data.characters[4].gearId].partItems[0] - 50] == 0) {
                battle_turn_state->slots[member].items[0] = battle_command_seal_bits[0];
            }
            if (game_data.gearAmmo[game_data.gears[game_data.characters[battle_party_character_ids[member]].gearId].partItems[3] - 50] == 0) {
                battle_turn_state->slots[member].items[2] = battle_command_seal_bits[2];
            }
        }
    }
    if (battle_slot_flags[member].unk1 == 0) {
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 1;
            battle_direction_input[0] = 0;
        } else {
            battle_turn_state->page = 4;
            battle_direction_input[0] = 3;
        }
        if (battle_work_area.records[member].pilot.status7C & 2) {
            battle_turn_state->page = 2;
            battle_direction_input[0] = 1;
        }
        battle_alloc_hud_lists();
    } else {
        if (battle_turn_state->slots[member].items[9] == 0) {
            battle_turn_state->page = 0x10;
            battle_direction_input[0] = 0;
        } else {
            battle_turn_state->page = 0x13;
            battle_direction_input[0] = 3;
        }
        battle_gear_hud_show(member);
    }
    battle_combo_reset_history();
    battle_end_defending(member);
    battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
    target = battle_turn_state->slots[member].defaultTarget;
    if (battle_turn_state->slots[member].defaultTarget == 0xFF) {
        target = 0;
    }
    battle_highlight_slots(battle_get_slot_bit(target));
    while (battle_turn_state->menuDone == 0 && battle_area_outcome == 0 && battle_turn_state->eventsDone == 0) {
        if (battle_slot_flags[member].unk1 == 0) {
            battle_command_menu_place_ap_separators(battle_turn_state->unk2D4[1], battle_turn_state->unk2D4[0]);
        }
        if (battle_turn_state->unk2E1[1] != battle_turn_state->page) {
            battle_turn_state->repeatArmed = 0;
            battle_turn_state->unk2E1[1] = battle_turn_state->page;
            battle_ui->unkCB = 0;
            battle_ui->unkA3 = battle_command_panel_build_page(member, battle_turn_state->page, battle_turn_state->unk2E0);
            battle_ui->unkCB = 1;
        }
        if (battle_pressed_key == 0xFF) {
            break;
        }
        battle_wait_frame();
        if (battle_turn_state->eventsDone != 0) {
            break;
        }
        if (battle_turn_state->unk2E0 == 0) {
            battle_command_menu_sounds_enabled = 1;
        }
        switch (battle_turn_state->page) {
        case 1:
            battle_command_menu_page_01_attack(member);
            break;
        case 2:
            battle_command_menu_page_02_item(member);
            break;
        case 3:
            battle_command_menu_page_03_defend(member);
            break;
        case 4:
            battle_command_menu_page_04_art(member);
            break;
        case 0x64:
            battle_turn_state->unk2E1[1] = 0xFF;
        case 5:
            battle_command_menu_page_05_attack_inputs(member);
            break;
        case 7:
            battle_command_menu_page_07_combo(member);
            break;
        case 8:
            battle_command_menu_page_08_item(member);
            break;
        case 9:
            battle_command_menu_page_09_escape(member);
            break;
        case 0xA:
            battle_command_menu_page_0a_board_gear(member);
            break;
        case 0x10:
            battle_command_menu_page_10_gear_attack(member);
            break;
        case 0x11:
            battle_command_menu_page_11_gear_item(member);
            break;
        case 0x12:
            battle_command_menu_page_12_gear_charge(member);
            break;
        case 0x13:
            battle_command_menu_page_13_gear_art(member);
            break;
        case 0x15:
            battle_command_menu_page_15_gear_haste(member);
            break;
        case 0x16:
            battle_command_menu_page_16_gear_item(member);
            break;
        case 0x17:
            battle_command_menu_page_17_gear_escape(member);
            break;
        case 0x18:
            battle_command_menu_page_18_gear_menu(member);
            break;
        case 0x65:
            battle_turn_state->unk2E1[1] = 0xFF;
        case 0x19:
            battle_command_menu_page_19_gear_attack_inputs(member);
            break;
        }
    }
    battle_ui->unkCB = 0;
    battle_ui->unkAF = 0;
    battle_ui->unk90[member] = 0;
    battle_ui->barShown[member] = 0;
    if (battle_turn_state->unk2E1[2] != 0) {
        battle_slot_flags[member].unk0 += battle_turn_state->unk2D4[0];
        if (battle_slot_flags[member].unk0 >= 29) {
            battle_slot_flags[member].unk0 = 28;
        }
    }
    battle_command_menu_sounds_enabled = 0;
    battle_empty_commit_step(member, 0x100);
    if (battle_turn_state->menuDone != 0 || battle_area_outcome != 0) {
        battle_command_menu_release_module_block();
        battle_command_menu_release_file3_block();
        battle_leave_member_menu(member);
        battle_ammo_window_release(member);
    }
    battle_turn_queue.timers[1][member] = battle_turn_queue.timers[0][member];
    battle_ui->reaction[member] = 1;
    battle_acting_with_partner = 0;
}

/* 80080AE4: The next slot in turn order, other than `actor`, with the lowest turn
 * timer (undefined when there is none). */
s32 battle_find_next_turn_slot(u8 actor) {
    u8 lowest = 0xFF;
    s32 position = battle_turn_queue.cursor;
    s32 slot;
    u8 next;

    do {
        slot = battle_turn_queue.order[position];
        position++;
        if (battle_turn_queue.timers[1][slot] < lowest && slot != actor) {
            next = slot;
            lowest = battle_turn_queue.timers[1][slot];
        }
        if (position == 11) {
            position = 0;
        }
    } while (position != battle_turn_queue.cursor);
    return next;
}

/* 80080B64: Close the actor's event queue (event 0xfe); menu effects off. */
void battle_close_actor_event_queue(u8 actor) {
    battle_area_events[battle_turn_state->eventCount].type = 0xFE;
    battle_area_events[battle_turn_state->eventCount].actor = actor;
    battle_command_menu_sounds_enabled = 0;
}

/* 80080BD0: Events done; for a party actor outside 800c204c refresh its menu state. */
void battle_mark_actor_events_done(void) {
    battle_turn_state->eventsDone = 1;
    if (battle_turn_state->actor < 3 && battle_in_automatic_turn == 0) {
        battle_command_menu_release_module_block();
        battle_command_menu_release_file3_block();
        battle_leave_member_menu(battle_turn_state->actor);
        battle_ammo_window_release(battle_turn_state->actor);
    }
}

/* 80080C6C: Set the 0x38-byte entry `index` of *800d3278 active (+0x34). */
void battle_mark_event_thread_effect_done(u8 index) {
    battle_state_of_event_script->threads[index].memberState = 1;
}

/* 80080C94: Take an automatic turn for party member `member` (8009bac4 chooses):
 * an attack (1) at a random reachable target other than itself, a skill (2)
 * when its EP cover the cost, or defend in the gear (4); anything else, or
 * too little EP, passes the turn. Then wait for the menu or the events to
 * finish. */
void battle_take_automatic_turn(u8 member) {
    u8 choice[2];
    u8 targets[11];
    s32 i;
    u8 *next;
    u8 pass = 1;
    u8 cost;

    battle_wait_frame();
    for (i = 0; i < 11; i++) {
        targets[i] = 0xFF;
    }
    battle_turn_state->unk2EA = 1;
    battle_turn_state->menuDone = 0;
    battle_end_defending(member);
    battle_choose_automatic_action(member, choice, (s16 *)&battle_turn_state->slots[member].items[7]);
    if (choice[0] == 1 || choice[0] == 2) {
        for (i = 0, next = targets; i < 11; i++) {
            if (battle_can_attack_slot(member, i)) {
                *next++ = i;
            }
        }
        while ((battle_target_cursor_slot = targets[mode_get_random_byte_in_range(0, 10)]) == 0xFF || battle_target_cursor_slot == member) {
        }
    }
    battle_reset_events_for_target(member, battle_target_cursor_slot);
    battle_turn_state->slots[member].defaultTarget = battle_target_cursor_slot;
    battle_camera_start_move(battle_get_slot_bit(member) | battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget));
    battle_wait_frame();
    battle_in_automatic_turn = 1;
    switch (choice[0]) {
    case 1:
        battle_attack_approach_done = 0;
        if (battle_party_character_ids[member] != 4) {
            battle_area_events[battle_turn_state->eventCount].actor = member;
            battle_area_events[battle_turn_state->eventCount].type = 0xFD;
            battle_area_events[battle_turn_state->eventCount].parameter = 0;
            battle_area_events[battle_turn_state->eventCount].targetMask = battle_get_slot_bit(battle_turn_state->slots[member].defaultTarget);
            battle_turn_state->eventCount++;
        }
        if (battle_slot_flags[member].unk1 == 0) {
            battle_turn_state->unk2DC = 0;
        } else {
            battle_turn_state->unk2DC = choice[1];
        }
        battle_menu_open_turn(battle_plan_approach_route(member, battle_turn_state->slots[member].defaultTarget), member,
                      battle_turn_state->slots[member].defaultTarget, battle_find_next_turn_slot(member));
        battle_execute_attack_step(member, choice[1] + 1);
        battle_close_actor_event_queue(member);
        pass = 0;
        break;
    case 2:
        cost = battle_work_area.partyCommands[member][choice[1] + 0x16].cost;
        if (battle_work_area.records[member].pilot.ep >= cost) {
            battle_work_area.records[member].pilot.ep -= cost;
            battle_turn_state->unk2E6 = choice[1];
            battle_execute_chosen_art(member);
            pass = 0;
        }
        break;
    case 4:
        battle_board_gear(member);
        pass = 0;
        battle_turn_state->menuDone = 1;
        break;
    }
    if (pass) {
        battle_start_defending(member);
        battle_turn_state->unk2EA = 0;
        battle_turn_state->menuDone = 1;
    }
    while (battle_turn_state->menuDone == 0 && battle_area_outcome == 0 && battle_turn_state->eventsDone == 0) {
        battle_wait_frame();
    }
    battle_in_automatic_turn = 0;
}
