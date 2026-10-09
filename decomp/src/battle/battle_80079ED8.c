/* Battle unit from 80079ED8: its rodata starts at 8006FB7C, 4 mod 8, right
 * after 800793F0's odd-length table at 0 mod 8 (docs/matching.md); the text
 * boundary lies after 800793F0. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "menu_pages.h"
#include "resolver.h"
#include "action_resolve.h"
#include "hud_draw.h"
#include "battle_command.h"
#include "window_draw.h"
#include "formation_route.h"
#include "gear_menu.h"
#include "glyph_lists.h"
#include "item_command.h"
#include "result_input.h"
#include "area.h"

/* Byte attribute `attribute` (0-23) of combatant `slot`: store `value` when
 * `read` is 0, else return it. */
u8 func_80079ED8(u8 slot, u8 attribute, u8 value, u8 read) {
    u8 *field;
    u8 result;

    switch (attribute) {
    case 0:
        field = &D_800CCCE8.records[slot].pilot.entries[0].value4;
        break;
    case 1:
        field = &D_800CCCE8.records[slot].pilot.characterId;
        break;
    case 2:
        field = &D_800CCCE8.records[slot].gear.attack;
        break;
    case 3:
        field = &D_800CCCE8.records[slot].gear.attackScale;
        break;
    case 4:
        field = &D_800CCCE8.records[slot].pilot.attack;
        break;
    case 5:
        field = &D_800CCCE8.records[slot].pilot.defense;
        break;
    case 6:
        field = &D_800CCCE8.records[slot].pilot.speed;
        break;
    case 7:
        field = &D_800CCCE8.records[slot].pilot.bodyDefense;
        break;
    case 8:
        field = &D_800CCCE8.records[slot].pilot.field5D;
        break;
    case 9:
        field = &D_800CCCE8.records[slot].pilot.accuracy;
        break;
    case 10:
        field = &D_800CCCE8.records[slot].pilot.etherDefense;
        break;
    case 11:
        field = &D_800CCCE8.records[slot].pilot.field5E;
        break;
    case 12:
        field = &D_800CCCE8.records[slot].pilot.field5F;
        break;
    case 13:
        field = &D_800CCCE8.records[slot].pilot.field60;
        break;
    case 14:
        field = &D_800CCCE8.records[slot].pilot.field61;
        break;
    case 15:
        field = &D_800CCCE8.records[slot].pilot.field64[0];
        break;
    case 16:
        field = &D_800CCCE8.records[slot].pilot.field64[1];
        break;
    case 17:
        field = &D_800CCCE8.records[slot].pilot.field64[2];
        break;
    case 18:
        field = &D_800CCCE8.records[slot].pilot.field64[3];
        break;
    case 19:
        field = &D_800CCCE8.records[slot].gear.speed;
        break;
    case 20:
        field = &D_800CCCE8.records[slot].gear.entries[0].valueE;
        break;
    case 21:
        field = &D_800CCCE8.records[slot].gear.guard;
        break;
    case 22:
        field = &D_800CCCE8.records[slot].gear.field9D;
        break;
    case 23:
        field = &D_800CCCE8.records[slot].gear.frameFactor;
        break;
    }
    if (!read) {
        *field = value;
    } else {
        result = *field;
    }
    return result;
}

/* Halfword attribute `attribute` (0-23) of combatant `slot`: store `value`
 * when `read` is 0, else return it. */
u16 func_8007A280(u8 slot, u8 attribute, u16 value, u8 read) {
    u16 *field;
    u16 result;

    switch (attribute) {
    case 0:
        field = &D_800CCCE8.records[slot].pilot.maxHp;
        break;
    case 1:
        field = &D_800CCCE8.records[slot].pilot.hp;
        break;
    case 2:
        field = &D_800CCCE8.records[slot].pilot.status7C;
        break;
    case 3:
        field = &D_800CCCE8.records[slot].pilot.status7E;
        break;
    case 4:
        field = &D_800CCCE8.records[slot].pilot.status80;
        break;
    case 5:
        field = &D_800CCCE8.records[slot].pilot.status82;
        break;
    case 6:
        field = &D_800CCCE8.records[slot].pilot.status84.half.active;
        break;
    case 7:
        field = &D_800CCCE8.records[slot].pilot.status84.half.permanent;
        break;
    case 8:
        field = &D_800CCCE8.records[slot].pilot.status88.half.active;
        break;
    case 9:
        field = &D_800CCCE8.records[slot].pilot.status88.half.permanent;
        break;
    case 10:
        field = &D_800CCCE8.records[slot].pilot.status8C.half.active;
        break;
    case 11:
        field = &D_800CCCE8.records[slot].pilot.status8C.half.permanent;
        break;
    case 12:
        field = &D_800CCCE8.records[slot].gear.field6C;
        break;
    case 13:
        field = &D_800CCCE8.records[slot].gear.bodyDefense;
        break;
    case 14:
        field = &D_800CCCE8.records[slot].gear.armor;
        break;
    case 15:
        field = &D_800CCCE8.records[slot].gear.status7C;
        break;
    case 16:
        field = &D_800CCCE8.records[slot].gear.field7E;
        break;
    case 17:
        field = &D_800CCCE8.records[slot].gear.status80;
        break;
    case 18:
        field = &D_800CCCE8.records[slot].gear.status82;
        break;
    case 19:
        field = &D_800CCCE8.records[slot].gear.status84.half.active;
        break;
    case 20:
        field = &D_800CCCE8.records[slot].gear.status84.half.permanent;
        break;
    case 21:
        field = &D_800CCCE8.records[slot].pilot.flags34;
        break;
    case 22:
        field = &D_800CCCE8.records[slot].pilot.flags36;
        break;
    case 23:
        field = &D_800CCCE8.records[slot].pilot.weakness;
        break;
    }
    if (!read) {
        *field = value;
    } else {
        result = *field;
    }
    return result;
}

/* Whether `slot` can be targeted: present, visible and not down (+0x7c
 * 0xc002); without `any` also not flagged 0x20 at +0x84. */
u8 func_8007A628(u8 slot, u8 any) {
    u8 result = 0;
    u16 status;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0 && !(D_800CCCE8.records[slot].pilot.status7C & 0xC002)) {
        if (any != 0) {
            result = 1;
        } else {
            status = D_800CCCE8.records[slot].pilot.status84.half.active & 0x20;
            result = status == 0;
        }
    }
    return result;
}

/* As 8007a628 without the visibility test. */
u8 func_8007A6C8(u8 slot, u8 any) {
    u8 result = 0;
    u16 status;

    if (D_800D2DCC.present[slot] != 0 && !(D_800CCCE8.records[slot].pilot.status7C & 0xC002)) {
        result = 1;
        if (any == 0) {
            status = D_800CCCE8.records[slot].pilot.status84.half.active & 0x20;
            result = status == 0;
        }
    }
    return result;
}

/* Whether `slot` is present, visible and alive (+0x7c without 0xc000). */
u8 func_8007A744(u8 slot) {
    u8 result = 0;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0) {
        result = (D_800CCCE8.records[slot].pilot.status7C & 0xC000) == 0;
    }
    return result;
}

/* AI action default (00, 6e, 6f, 75-7f): queue an entry of type 0x80
 * carrying the four opcode bytes, which 800793f0 rejects with its script
 * error (800792f8). Returns the new entry count. */
u8 func_8007A7BC(u8 **pc, u8 *list, u8 enemy, u8 count) {
    list[count * 8] = 0x80;
    list[count * 8 + 1] = (*pc)[0];
    list[count * 8 + 2] = (*pc)[1];
    list[count * 8 + 3] = (*pc)[2];
    list[count * 8 + 4] = (*pc)[3];
    return count + 1;
}

/* AI action 01: list entry byte b1 = b2; offset 0 starts the next entry. */
u8 func_8007A828(u8 **pc, u8 *list, u8 count) {
    list[count * 8 + (*pc)[1]] = (*pc)[2];
    if ((*pc)[1] == 0) {
        count++;
    }
    return count;
}

/* AI action 02: list entry byte b1 = byte variable b2. */
void func_8007A874(u8 **pc, u8 *list, u8 enemy, u8 count) {
    list[count * 8 + (*pc)[1]] = D_800D3400[enemy].bytes[(*pc)[2]];
}

/* AI action 03: copy list entry b1 to entry b2. */
void func_8007A8B4(u8 **pc, u8 *list) {
    s32 i;

    for (i = 0; i < 8; i++) {
        list[(*pc)[2] * 8 + i] = list[(*pc)[1] * 8 + i];
    }
}

/* AI action 04: byte variable b1 = b2. */
void func_8007A900(u8 **pc, u8 enemy) {
    D_800D3400[enemy].bytes[(*pc)[1]] = (*pc)[2];
}

/* AI action 05: variable b1 = b2 | b3 << 8. */
void func_8007A92C(u8 **pc, u8 enemy) {
    u16 value = (*pc)[2] | ((*pc)[3] << 8);

    D_800D3400[enemy].vars[(*pc)[1]] = value;
}

/* AI action 06: long b1 = (b2 | b3 << 8) * 16. */
void func_8007A968(u8 **pc, u8 enemy) {
    s32 value = (((*pc)[3] << 8) + (*pc)[2]) * 16;

    D_800D3400[enemy].longs[(*pc)[1]] = value;
}

/* AI action 07: resident halfword b1 = b2. */
void func_8007A9A8(u8 **pc) {
    D_8005A3A0[(*pc)[1]] = (*pc)[2];
}

/* AI action 08: byte variable b1 += b2, saturating at 0xff. */
void func_8007A9D0(u8 **pc, u8 enemy) {
    u8 *value = &D_800D3400[enemy].bytes[(*pc)[1]];
    s32 sum = *value + (*pc)[2];
    u8 result = sum;

    if (sum >= 0x100) {
        result = 0xFF;
    }
    *value = result;
}

/* AI action 09: byte variable b1 -= b2, saturating at 0. */
void func_8007AA1C(u8 **pc, u8 enemy) {
    u8 *value = &D_800D3400[enemy].bytes[(*pc)[1]];
    s32 difference = *value - (*pc)[2];
    u8 result = difference;

    if (difference < 0) {
        result = 0;
    }
    *value = result;
}

/* AI action 0a: byte variable b1 *= b2, saturating at 0xff. */
void func_8007AA60(u8 **pc, u8 enemy) {
    u8 *value = &D_800D3400[enemy].bytes[(*pc)[1]];
    s16 product = *value * (*pc)[2];

    if (product >= 0x100) {
        product = 0xFF;
    }
    *value = product;
}

/* AI action 0b: byte variable b1 /= b2. */
void func_8007AAB8(u8 **pc, u8 enemy) {
    u8 *value = &D_800D3400[enemy].bytes[(*pc)[1]];

    *value = *value / (*pc)[2];
}

/* AI action 0c: byte variable b1 %= b2. */
void func_8007AAF4(u8 **pc, u8 enemy) {
    u8 *value = &D_800D3400[enemy].bytes[(*pc)[1]];

    *value = *value % (*pc)[2];
}

/* AI action 0d: byte variable b1 &= b2. */
void func_8007AB30(u8 **pc, u8 enemy) {
    D_800D3400[enemy].bytes[(*pc)[1]] &= (*pc)[2];
}

/* AI action 0e: byte variable b1 |= b2. */
void func_8007AB68(u8 **pc, u8 enemy) {
    D_800D3400[enemy].bytes[(*pc)[1]] |= (*pc)[2];
}

/* AI action 0f: byte variable b1 ^= b2. */
void func_8007ABA0(u8 **pc, u8 enemy) {
    D_800D3400[enemy].bytes[(*pc)[1]] ^= (*pc)[2];
}

/* AI action 10: variable b1 += b2 | b3 << 8, saturating at 0xffff. */
void func_8007ABD8(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 value = D_800D3400[enemy].vars[op[1]] + ((op[3] << 8) + op[2]);

    if (value > 0xFFFF) {
        value = 0xFFFF;
    }
    D_800D3400[enemy].vars[op[1]] = value;
}

/* AI action 11: variable b1 -= b2 | b3 << 8, saturating at 0. */
void func_8007AC30(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 value = D_800D3400[enemy].vars[op[1]] - ((op[3] << 8) + op[2]);

    if (value < 0) {
        value = 0;
    }
    D_800D3400[enemy].vars[op[1]] = value;
}

/* AI action 12: variable b1 *= b2 | b3 << 8, saturating at 0xffff. */
void func_8007AC80(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 value = D_800D3400[enemy].vars[op[1]] * ((op[3] << 8) + op[2]);

    if (value > 0xFFFF) {
        value = 0xFFFF;
    }
    D_800D3400[enemy].vars[op[1]] = value;
}

/* AI action 13: variable b1 /= b2 | b3 << 8. */
void func_8007ACDC(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    D_800D3400[enemy].vars[op[1]] = D_800D3400[enemy].vars[op[1]] / ((op[3] << 8) + op[2]);
}

/* AI action 14: variable b1 %= b2 | b3 << 8. */
void func_8007AD24(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    D_800D3400[enemy].vars[op[1]] = D_800D3400[enemy].vars[op[1]] % ((op[3] << 8) + op[2]);
}

/* AI action 15: variable b1 &= b2 | b3 << 8. */
void func_8007AD6C(u8 **pc, u8 enemy) {
    D_800D3400[enemy].vars[(*pc)[1]] &= (*pc)[2] + ((*pc)[3] << 8);
}

/* AI action 16: variable b1 |= b2 | b3 << 8. */
void func_8007ADB0(u8 **pc, u8 enemy) {
    D_800D3400[enemy].vars[(*pc)[1]] |= (*pc)[2] + ((*pc)[3] << 8);
}

/* AI action 17: variable b1 ^= b2 | b3 << 8. */
void func_8007ADF4(u8 **pc, u8 enemy) {
    D_800D3400[enemy].vars[(*pc)[1]] ^= (*pc)[2] + ((*pc)[3] << 8);
}

/* AI action 18: byte variable b3 = byte b1 + byte b2, saturating at 0xff. */
void func_8007AE38(u8 **pc, u8 enemy) {
    u8 *bytes = D_800D3400[enemy].bytes;
    s32 sum = bytes[(*pc)[1]] + bytes[(*pc)[2]];
    u8 result = sum;

    if (sum >= 0x100) {
        result = 0xFF;
    }
    bytes[(*pc)[3]] = result;
}

/* AI action 19: byte variable b3 = byte b1 - byte b2, saturating at 0. */
void func_8007AE98(u8 **pc, u8 enemy) {
    u8 *bytes = D_800D3400[enemy].bytes;
    s32 difference = bytes[(*pc)[1]] - bytes[(*pc)[2]];
    u8 result = difference;

    if (difference < 0) {
        result = 0;
    }
    bytes[(*pc)[3]] = result;
}

/* AI action 1a: byte variable b3 = byte b1 * byte b2, saturating at 0xff. */
void func_8007AEF0(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;
    s16 product = bytes[op[1]] * bytes[op[2]];

    if (product >= 0x100) {
        product = 0xFF;
    }
    bytes[op[3]] = product;
}

/* AI action 1b: byte variable b3 = byte b1 / byte b2. */
void func_8007AF5C(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] / bytes[op[2]];
}

/* AI action 1c: byte variable b3 = byte b1 % byte b2. */
void func_8007AFAC(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] % bytes[op[2]];
}

/* AI action 1d: byte variable b3 = byte b1 & byte b2. */
void func_8007AFFC(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] & bytes[op[2]];
}

/* AI action 1e: byte variable b3 = byte b1 | byte b2. */
void func_8007B040(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] | bytes[op[2]];
}

/* AI action 1f: byte variable b3 = byte b1 ^ byte b2. */
void func_8007B084(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    bytes[op[3]] = bytes[op[1]] ^ bytes[op[2]];
}

/* AI action 20: variable b3 = var b1 + var b2, saturating at 0xffff. */
void func_8007B0C8(u8 **pc, u8 enemy) {
    u16 *vars = D_800D3400[enemy].vars;
    s32 sum = vars[(*pc)[1]] + vars[(*pc)[2]];

    if (sum > 0xFFFF) {
        sum = 0xFFFF;
    }
    vars[(*pc)[3]] = sum;
}

/* AI action 21: variable b3 = var b1 - var b2, saturating at 0. */
void func_8007B134(u8 **pc, u8 enemy) {
    u16 *vars = D_800D3400[enemy].vars;
    s32 difference = vars[(*pc)[1]] - vars[(*pc)[2]];

    if (difference < 0) {
        difference = 0;
    }
    vars[(*pc)[3]] = difference;
}

/* AI action 22: variable b3 = var b1 * var b2, saturating at 0xffff. */
void func_8007B198(u8 **pc, u8 enemy) {
    u16 *vars = D_800D3400[enemy].vars;
    s32 product = vars[(*pc)[1]] * vars[(*pc)[2]];

    if (product > 0xFFFF) {
        product = 0xFFFF;
    }
    vars[(*pc)[3]] = product;
}

/* AI action 23: variable b3 = var b1 / var b2. */
void func_8007B208(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    vars[op[3]] = vars[op[1]] / vars[op[2]];
}

/* AI action 24: variable b3 = var b1 % var b2. */
void func_8007B264(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    vars[op[3]] = vars[op[1]] % vars[op[2]];
}

/* AI action 25: variable b3 = var b1 & var b2. */
void func_8007B2C0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[op[3]] = D_800D3400[enemy].vars[op[1]] & D_800D3400[enemy].vars[op[2]];
}

/* AI action 26: variable b3 = var b1 | var b2. */
void func_8007B310(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[op[3]] = D_800D3400[enemy].vars[op[1]] | D_800D3400[enemy].vars[op[2]];
}

/* AI action 27: variable b3 = var b1 ^ var b2. */
void func_8007B360(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[op[3]] = D_800D3400[enemy].vars[op[1]] ^ D_800D3400[enemy].vars[op[2]];
}

/* AI action 28: the enemy's byte attribute b1 (80079ed8) = b2. */
void func_8007B3B0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    func_80079ED8(enemy + 3, op[1], op[2], 0);
}

/* AI action 29: the enemy's halfword attribute b1 (8007a280) = b2 | b3 << 8. */
void func_8007B3E4(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    func_8007A280(enemy + 3, op[1], op[2] | (op[3] << 8), 0);
}

/* AI action 2a: byte variable b1 = attribute b2 of the first slot in var b3. */
void func_8007B424(u8 **pc, u8 enemy) {
    u8 slot = func_80079E7C(D_800D3400[enemy].vars[(*pc)[3]]);

    D_800D3400[enemy].bytes[(*pc)[1]] = func_80079ED8(slot, (*pc)[2], 0, 1);
}

/* AI action 2b: set byte attribute b2 of the first slot in var b3 to byte
 * variable b1; for a party slot instead set list entry 0's type to 0x20
 * (800793f0 rejects it) and count an entry. */
u8 func_8007B4B8(u8 **pc, u8 enemy, u8 count) {
    u8 slot = func_80079E7C(D_800D3400[enemy].vars[(*pc)[3]]);

    if (slot < 3) {
        D_800D2E5C[0].type = 0x20;
        count++;
    } else {
        func_80079ED8(slot, (*pc)[2], D_800D3400[enemy].bytes[(*pc)[1]], 0);
    }
    return count;
}

/* AI action 2c: variable b1 = 16-bit attribute b2 of the first slot in var b3. */
void func_8007B578(u8 **pc, u8 enemy) {
    u16 *vars = D_800D3400[enemy].vars;
    u8 slot = func_80079E7C(vars[(*pc)[3]]);

    vars[(*pc)[1]] = func_8007A280(slot, (*pc)[2], 0, 1);
}

/* AI action 2d: set 16-bit attribute b2 of the first slot in var b3 to var b1;
 * for a party slot instead set list entry 0's type to 0x20 and count an
 * entry. */
u8 func_8007B608(u8 **pc, u8 enemy, u8 count) {
    u16 *vars = D_800D3400[enemy].vars;
    u8 slot = func_80079E7C(vars[(*pc)[3]]);

    if (slot < 3) {
        D_800D2E5C[0].type = 0x20;
        count++;
    } else {
        func_8007A280(slot, (*pc)[2], vars[(*pc)[1]], 0);
    }
    return count;
}

/* AI action 2e: long b1 = (b2 1) record +0x104 of the first slot in var b3,
 * or (b2 2) the party's gold. */
void func_8007B6C0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    switch (op[2]) {
    case 1:
        D_800D3400[enemy].longs[(*pc)[1]] =
            D_800CCCE8.records[func_80079E7C(D_800D3400[enemy].vars[op[3]])].gear.hp;
        break;
    case 2:
        D_800D3400[enemy].longs[op[1]] = D_8006EF58;
        break;
    }
}

/* AI action 2f: record +0x108 (b2 0) or +0x104 of the first slot in var b3 =
 * long b1; for a party slot instead set list entry 0's type to 0x20 and count
 * an entry. */
u8 func_8007B7B0(u8 **pc, u8 enemy, u8 count) {
    u8 slot = func_80079E7C(D_800D3400[enemy].vars[(*pc)[3]]);

    if (slot < 3) {
        D_800D2E5C[0].type = 0x20;
        count++;
    } else if ((*pc)[2] == 0) {
        D_800CCCE8.records[slot].gear.maxHp = D_800D3400[enemy].longs[(*pc)[1]];
    } else {
        D_800CCCE8.records[slot].gear.hp = D_800D3400[enemy].longs[(*pc)[1]];
    }
    return count;
}

/* AI action 30: variable b1 = resident halfword b2. */
void func_8007B8D4(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[op[1]] = D_8005A3A0[op[2]];
}

/* AI action 31: resident halfword b2 = variable b1. */
void func_8007B914(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_8005A3A0[op[2]] = D_800D3400[enemy].vars[op[1]];
}

/* AI action 32: byte variable b2 = byte variable b1. */
void func_8007B958(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    bytes[op[2]] = bytes[op[1]];
}

/* AI action 33: variable b2 = variable b1. */
void func_8007B98C(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[op[2]] = D_800D3400[enemy].vars[op[1]];
}

/* AI action 34: long b2 = long b1. */
void func_8007B9C8(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].longs[op[2]] = D_800D3400[enemy].longs[op[1]];
}

/* AI action 35: variable b2 = byte variable b1. */
void func_8007BA04(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[op[2]] = D_800D3400[enemy].bytes[op[1]];
}

/* AI action 36: long b2 = variable b1. */
void func_8007BA44(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].longs[op[2]] = D_800D3400[enemy].vars[op[1]];
}

/* AI action 37: clear the byte variables. */
void func_8007BA88(u8 enemy) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        D_800D3400[enemy].bytes[i] = 0;
    }
}

/* AI action 38: clear the variables. */
void func_8007BAB8(u8 enemy) {
    s32 i;

    for (i = 7; i >= 0; i--) {
        D_800D3400[enemy].vars[i] = 0;
    }
}

/* AI action 39: the enemy's experience (record +0x14c) = b1 | b2 << 8. */
void func_8007BAE8(u8 **pc, u8 enemy) {
    u16 value = ((*pc)[2] << 8) | (*pc)[1];

    D_800CCCE8.records[enemy + 3].field14C = value;
}

/* AI action 3a: the enemy's gold (record +0x156) = b1 | b2 << 8. */
void func_8007BB2C(u8 **pc, u8 enemy) {
    u16 value = (*pc)[1] | ((*pc)[2] << 8);

    D_800CCCE8.records[enemy + 3].field156 = value;
}

/* AI action 3b: the enemy record's bytes +0x155, +0x153, +0x151 = b1, b2, b3. */
void func_8007BB70(u8 **pc, u8 enemy) {
    D_800CCCE8.records[enemy + 3].field150[5] = (*pc)[1];
    D_800CCCE8.records[enemy + 3].field150[3] = (*pc)[2];
    D_800CCCE8.records[enemy + 3].field150[1] = (*pc)[3];
}

/* AI action 3c: the enemy's first drop: category (record +0x154), item
 * (+0x152) and chance (+0x150) = b1, b2, b3. */
void func_8007BBD8(u8 **pc, u8 enemy) {
    D_800CCCE8.records[enemy + 3].field150[4] = (*pc)[1];
    D_800CCCE8.records[enemy + 3].field150[2] = (*pc)[2];
    D_800CCCE8.records[enemy + 3].field150[0] = (*pc)[3];
}

/* AI action 3d: list entry halfword at b1 = b2 | b3 << 8 (two byte stores). */
void func_8007BC40(u8 **pc, u8 *list, u8 count) {
    list[count * 8 + (*pc)[1]] = (*pc)[2];
    (&list[count * 8 + (*pc)[1]])[1] = (*pc)[3];
}

/* AI action 3e: byte variable b1 = random 0..b2. */
void func_8007BC84(u8 **pc, u8 enemy) {
    D_800D3400[enemy].bytes[(*pc)[1]] = func_8001BD40(0, (*pc)[2]);
}

/* AI action 3f: variable b1 = random 0..(b2 | b3 << 8). */
void func_8007BCE8(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].vars[(*pc)[1]] = func_80089B50(0, op[2] | (op[3] << 8));
}

/* AI action 40: variable b1 = the bit of a random party slot passing the
 * targeting test b2 and on foot (800d32a1 clear); 0 when none does. */
void func_8007BD5C(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = func_8001BD40(0, 2);
        if (tried[slot] == 0) {
            if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 == 0) {
                D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* AI action 41: as action 40, limited to party slots in the enemy's
 * formation group. */
void func_8007BEA8(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = func_8001BD40(0, 2);
        if (tried[slot] == 0) {
            if (func_8007A628(slot, (*pc)[2]) && D_800C3EB4[enemy + 3].group == D_800C3EB4[slot].group &&
                D_800D32A0[slot].unk1 == 0) {
                D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* AI action 42: variable b1 = the bit of a random enemy slot passing the
 * targeting test b2 in the enemy's formation group and not flagged at
 * 800d32a1; 0 when none does. */
void func_8007C040(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && D_800C3EB4[enemy + 3].group == D_800C3EB4[slot].group &&
            D_800D32A0[slot].unk1 == 0) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 43: as action 40, limited to party slots outside the enemy's
 * formation group. */
void func_8007C1A4(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = func_8001BD40(0, 2);
        if (tried[slot] == 0) {
            if (func_8007A628(slot, (*pc)[2]) && D_800C3EB4[enemy + 3].group != D_800C3EB4[slot].group &&
                D_800D32A0[slot].unk1 == 0) {
                D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* AI action 44: as action 42, limited to enemy slots outside the enemy's
 * formation group. */
void func_8007C33C(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && D_800C3EB4[enemy + 3].group != D_800C3EB4[slot].group &&
            D_800D32A0[slot].unk1 == 0) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 45: variable b1 = the bit of the targetable party slot with the
 * lowest turn timer. */
void func_8007C4A0(u8 **pc, u8 enemy) {
    s32 slot;
    u8 lowest = 0xFF;
    s32 target = 0;

    for (slot = 0; slot < 3; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800D2DCC.timers[1][slot]) {
            lowest = D_800D2DCC.timers[1][slot];
            target = slot;
        }
    }
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(target);
}

/* AI action 46: variable b1 = the bit of the targetable other enemy slot
 * with the lowest turn timer. */
void func_8007C580(u8 **pc, u8 enemy) {
    s32 slot;
    u8 lowest = 0xFF;
    s32 target = 0;

    for (slot = 3; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800D2DCC.timers[1][slot] && slot != enemy + 3) {
            lowest = D_800D2DCC.timers[1][slot];
            target = slot;
        }
    }
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(target);
}

/* AI action 47: variable b1 = the bit of the targetable party slot with the
 * lowest HP. */
void func_8007C678(u8 **pc, u8 enemy) {
    s32 slot;
    u16 lowest = 0xFFFF;
    s32 target = 0;

    for (slot = 0; slot < 3; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800CCCE8.records[slot].pilot.hp) {
            lowest = D_800CCCE8.records[slot].pilot.hp;
            target = slot;
        }
    }
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(target);
}

/* AI action 48: variable b1 = the bit of the targetable enemy slot with the
 * lowest HP. */
void func_8007C75C(u8 **pc, u8 enemy) {
    s32 slot;
    u16 lowest = 0xFFFF;
    s32 target = 0;

    for (slot = 3; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800CCCE8.records[slot].pilot.hp) {
            lowest = D_800CCCE8.records[slot].pilot.hp;
            target = slot;
        }
    }
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(target);
}

/* AI action 49: variable b1 = the bit of a random party slot passing the
 * targeting test b2, in a gear (800d32a1) and in the enemy's formation group. */
void func_8007C840(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = func_8001BD40(0, 2);
        if (tried[slot] == 0) {
            if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0 &&
                D_800C3EB4[enemy + 3].group == D_800C3EB4[slot].group) {
                D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* AI action 4a: variable b1 = the bit of a random party slot passing the
 * targeting test b2 and in a gear (800d32a1). */
void func_8007C9D4(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = func_8001BD40(0, 2);
        if (tried[slot] == 0) {
            if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0) {
                D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* AI action 4b: variable b1 = the bit of a random enemy slot passing the
 * targeting test b2 and in a gear (800d32a1); 0 when none does. */
void func_8007CB20(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 4c: byte variable b1 = the number of targetable party slots in
 * formation group b2. */
void func_8007CC50(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 0; slot < 3; slot++) {
        if (func_8007A628(slot, 0) && D_800C3EB4[slot].group == (*pc)[2]) {
            count++;
        }
    }
    D_800D3400[enemy].bytes[(*pc)[1]] = count;
}

/* AI action 4d: byte variable b1 = the number of targetable enemy slots in
 * formation group b2. */
void func_8007CD10(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 3; slot < 11; slot++) {
        if (func_8007A628(slot, 0) && D_800C3EB4[slot].group == (*pc)[2]) {
            count++;
        }
    }
    D_800D3400[enemy].bytes[(*pc)[1]] = count;
}

/* AI action 4e: byte variable b1 = the formation's group distance from the
 * enemy's group to the group of the first slot in var b2. */
void func_8007CDD0(u8 **pc, u8 enemy) {
    u8 slot = func_80079E7C(D_800D3400[enemy].vars[(*pc)[2]]);

    D_800D3400[enemy].bytes[(*pc)[1]] =
        D_800D3364->links[D_800C3EB4[enemy + 3].group][D_800C3EB4[slot].group].distance;
}

/* AI action 4f: byte variable b1 = the number of targetable party slots in
 * the group of the first slot in var b2. */
void func_8007CEA4(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 0; slot < 3; slot++) {
        if (func_8007A628(slot, 0) &&
            D_800C3EB4[slot].group == D_800C3EB4[func_80079E7C(D_800D3400[enemy].vars[(*pc)[2]])].group) {
            count++;
        }
    }
    D_800D3400[enemy].bytes[(*pc)[1]] = count;
}

/* AI action 50: byte variable b1 = the number of targetable enemy slots in
 * the group of the first slot in var b2. */
void func_8007CFB8(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 3; slot < 11; slot++) {
        if (func_8007A628(slot, 0) &&
            D_800C3EB4[slot].group == D_800C3EB4[func_80079E7C(D_800D3400[enemy].vars[(*pc)[2]])].group) {
            count++;
        }
    }
    D_800D3400[enemy].bytes[(*pc)[1]] = count;
}

/* AI action 51: byte variable b1 = the count held for item b2 (0 when the
 * item is not held). */
void func_8007D0CC(u8 **pc, u8 enemy) {
    s32 i;

    D_800D3400[enemy].bytes[(*pc)[1]] = 0;
    for (i = 0; i < 0x30; i++) {
        if (D_800D2CE0[i] == (*pc)[2]) {
            D_800D3400[enemy].bytes[(*pc)[1]] = D_800D2CB0[i];
            return;
        }
    }
}

/* AI action 52: list entry halfword at b1 = variable b2 (two byte stores). */
void func_8007D148(u8 **pc, u8 *list, u8 enemy, u8 count) {
    u16 value = D_800D3400[enemy].vars[(*pc)[2]];

    list[count * 8 + (*pc)[1]] = value;
    (&list[count * 8 + (*pc)[1]])[1] = value >> 8;
}

/* AI action 53: long b1 = the party's gold. */
void func_8007D1A8(u8 **pc, u8 enemy) {
    D_800D3400[enemy].longs[(*pc)[1]] = D_8006EF58;
}

/* AI action 54: variable b1 = the bit of a random slot passing 8007a744
 * whose record byte +0x56 is b2; 0 when none does. */
void func_8007D1DC(u8 **pc, u8 enemy) {
    u8 candidates[11];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A744(slot) && D_800CCCE8.records[slot].pilot.characterId == (*pc)[2]) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 55: long b1 = the enemy's result code (800d2c8b, the work
 * table's resultCode[3 + enemy]). */
void func_8007D30C(u8 **pc, u8 enemy) {
    D_800D3400[enemy].longs[(*pc)[1]] = D_800D2C8B[enemy];
}

/* AI action 56: variable b1 = the bit of a random enemy slot passing
 * 8007a6c8(b2) with slot info +3 bit 0x80; 0 when none does. */
void func_8007D344(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A6C8(slot, (*pc)[2]) && (D_800C3EB4[slot].hidden & 0x80)) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 57: variable b1 = the bit of a random party slot flagged 0x8000
 * at +0x7c without 0x4002. */
void func_8007D478(u8 **pc, u8 enemy) {
    u8 tried[3];
    u8 slot;
    u16 flags;

    tried[2] = 0;
    tried[1] = 0;
    tried[0] = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    while (!(tried[2] & (tried[0] & tried[1]))) {
        slot = func_8001BD40(0, 2);
        if (tried[slot] == 0) {
            flags = D_800CCCE8.records[slot].pilot.status7C;
            if ((flags & 0x8000) && !(flags & 0x4002)) {
                D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(slot);
                return;
            }
            tried[slot] = 1;
        }
    }
}

/* AI action 58: byte variable b1 = the number of party slots not down
 * (+0x7c without 0xc000). */
void func_8007D5B0(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 0; slot < 3; slot++) {
        if (!(D_800CCCE8.records[slot].pilot.status7C & 0xC000)) {
            count++;
        }
    }
    D_800D3400[enemy].bytes[(*pc)[1]] = count;
}

/* AI action 59: byte variable b1 = the number of present, visible enemy
 * slots not down. */
void func_8007D610(u8 **pc, u8 enemy) {
    s32 slot;
    u8 count = 0;

    for (slot = 3; slot < 11; slot++) {
        if (D_800D2DCC.present[slot] != 0 && !(D_800CCCE8.records[slot].pilot.status7C & 0xC000) && D_800C3EB4[slot].hidden == 0) {
            count++;
        }
    }
    D_800D3400[enemy].bytes[(*pc)[1]] = count;
}

/* AI action 5a: variable b1 = the bit of the targetable party slot flagged
 * at 800d32a1 with the lowest record +0x104. */
void func_8007D6A8(u8 **pc, u8 enemy) {
    s32 slot;
    u32 lowest = 0xFFFFFFFF;
    s32 target = 0;

    for (slot = 0; slot < 3; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0 && lowest >= D_800CCCE8.records[slot].gear.hp) {
            lowest = D_800CCCE8.records[slot].gear.hp;
            target = slot;
        }
    }
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(target);
}

/* AI action 5b: variable b1 = the bit of the targetable enemy slot flagged
 * at 800d32a1 with the lowest HP. */
void func_8007D7B4(u8 **pc, u8 enemy) {
    s32 slot;
    u32 lowest = 0xFFFFFFFF;
    s32 target = 0;

    for (slot = 3; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0 && lowest >= D_800CCCE8.records[slot].pilot.hp) {
            lowest = D_800CCCE8.records[slot].pilot.hp;
            target = slot;
        }
    }
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(target);
}

/* AI action 5c: variable b2 = the bit of a random targetable party slot whose
 * 16-bit attribute b1 shares a bit with var b3; 0 when none does. */
void func_8007D8C0(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (func_8007A628(slot, 0) && (D_800D3400[enemy].vars[(*pc)[3]] & func_8007A280(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[2]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 5d: as action 5c for the enemy slots. */
void func_8007DA1C(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    count = 0;
    D_800D3400[enemy].vars[(*pc)[2]] = 0;
    for (slot = 3; slot < 11; slot++) {
        if (func_8007A628(slot, 0) && (D_800D3400[enemy].vars[(*pc)[3]] & func_8007A280(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[2]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 5e: as action 5c, limited to party slots in a gear (800d32a1). */
void func_8007DB78(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (func_8007A628(slot, 0) && D_800D32A0[slot].unk1 != 0 &&
            (D_800D3400[enemy].vars[(*pc)[3]] & func_8007A280(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[2]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 5f: as action 5d, limited to enemy slots in a gear (800d32a1). */
void func_8007DCF8(u8 **pc, u8 enemy) {
    u8 candidates[8];
    s32 count;
    s32 slot;

    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A628(slot, 0) && D_800D32A0[slot].unk1 != 0 &&
            (D_800D3400[enemy].vars[(*pc)[3]] & func_8007A280(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[2]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 60: as action 5c with any targetable party slot. */
void func_8007DE78(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (func_8007A628(slot, 1) && (D_800D3400[enemy].vars[(*pc)[3]] & func_8007A280(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[2]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 61: as action 5e with any targetable party slot. */
void func_8007DFD4(u8 **pc, u8 enemy) {
    u8 candidates[3];
    s32 count;
    s32 slot;

    slot = 0;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[2]] = 0;
    for (; slot < 3; slot++) {
        if (func_8007A628(slot, 1) && D_800D32A0[slot].unk1 != 0 &&
            (D_800D3400[enemy].vars[(*pc)[3]] & func_8007A280(slot, (*pc)[1], 0, 1))) {
            candidates[count++] = slot;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[2]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}

/* AI action 63: set the enemy's slot info +3 to b1; with bit 0x80 the enemy
 * rejoins its own group, otherwise it leaves its formation group. */
void func_8007E154(u8 **pc, u8 enemy) {
    D_800C3EB4[enemy + 3].hidden = (*pc)[1];
    if ((*pc)[1] & 0x80) {
        func_80087EDC(enemy + 3, enemy + 3);
    } else {
        func_800883AC(enemy + 3);
    }
}

/* AI action 64: variable b1 = the enemy's own slot bit. */
void func_8007E1D0(u8 **pc, u8 enemy) {
    D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(enemy + 3);
}

/* AI action 65: variable b1 = the mask of party slots passing 8007a744 and
 * (b2 1) in a gear, (b2 2) on foot (800d32a1), or (other b2) any. */
void func_8007E234(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 2; slot >= 0; slot--) {
        if (func_8007A744(slot)) {
            switch ((*pc)[2]) {
            case 1:
                if (D_800D32A0[slot].unk1 != 0) {
                    mask |= 1;
                }
                break;
            case 2:
                if (D_800D32A0[slot].unk1 == 0) {
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
    D_800D3400[enemy].vars[(*pc)[1]] = mask >> 1;
}

/* AI action 66: variable b1 = the mask of enemy slots passing 8007a744 and
 * (b2 1) in a gear, (b2 2) on foot (800d32a1), or (other b2) any. */
void func_8007E334(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 10; slot >= 3; slot--) {
        if (func_8007A744(slot)) {
            switch ((*pc)[2]) {
            case 1:
                if (D_800D32A0[slot].unk1 != 0) {
                    mask |= 1;
                }
                break;
            case 2:
                if (D_800D32A0[slot].unk1 == 0) {
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
    D_800D3400[enemy].vars[(*pc)[1]] = mask << 2;
}

/* AI action 67: variable b1 = the mask of party slots passing 8007a744 in the
 * group of the first slot in var b2. */
void func_8007E438(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 2; slot >= 0; slot--) {
        if (func_8007A744(slot) &&
            D_800C3EB4[func_80079E7C(D_800D3400[enemy].vars[(*pc)[2]])].group == D_800C3EB4[slot].group) {
            mask |= 1;
        }
        mask <<= 1;
    }
    D_800D3400[enemy].vars[(*pc)[1]] = mask >> 1;
}

/* AI action 68: variable b1 = the mask of enemy slots passing 8007a744 in the
 * group of the first slot in var b2. */
void func_8007E554(u8 **pc, u8 enemy) {
    s32 slot;
    u16 mask = 0;

    for (slot = 10; slot >= 3; slot--) {
        if (func_8007A744(slot) &&
            D_800C3EB4[func_80079E7C(D_800D3400[enemy].vars[(*pc)[2]])].group == D_800C3EB4[slot].group) {
            mask |= 1;
        }
        mask <<= 1;
    }
    D_800D3400[enemy].vars[(*pc)[1]] = mask << 2;
}

/* AI action 69: clear the enemy's pending amount (800d2c60, damage[3 +
 * enemy]) and set its result code (800d2c8b) to 4, which no results pass
 * applies. */
void func_8007E674(u8 enemy) {
    D_800D2C60[enemy] = 0;
    D_800D2C8B[enemy] = 4;
}

/* AI action 6a: long b3 = long b1 + long b2. */
void func_8007E6A0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].longs[op[3]] = D_800D3400[enemy].longs[op[1]] + D_800D3400[enemy].longs[op[2]];
}

/* AI action 6b: long b3 = long b1 - long b2. */
void func_8007E6F0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].longs[op[3]] = D_800D3400[enemy].longs[op[1]] - D_800D3400[enemy].longs[op[2]];
}

/* AI action 6c: long b1 *= b2. */
void func_8007E740(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].longs[op[1]] *= op[2];
}

/* AI action 6d: long b1 /= b2 (unsigned). */
void func_8007E780(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    D_800D3400[enemy].longs[op[1]] = (u32)D_800D3400[enemy].longs[op[1]] / op[2];
}

/* AI action 70: the battle event script's variable b1 (ovl3087's
 * ScriptState.vars) = b2. */
void func_8007E7C0(u8 **pc) {
    u8 *op = *pc;

    D_800D3278->vars[op[1]] = op[2];
}

/* AI action 71: set (b2 != 0) or clear flag b1 + 7 in every party record's
 * +0x7a. */
void func_8007E7E4(u8 **pc, u8 enemy) {
    s32 i;
    u8 set = (*pc)[2] != 0;

    for (i = 0; i < 3; i++) {
        if (set) {
            D_800CCCE8.records[i].pilot.status7A |= func_80089BEC((*pc)[1] + 7);
        } else {
            D_800CCCE8.records[i].pilot.status7A &= ~func_80089BEC((*pc)[1] + 7);
        }
    }
}

/* AI action 72: formation group distance b1 -> b2 = b3. */
void func_8007E8AC(u8 **pc) {
    u8 *op = *pc;

    D_800D3364->links[op[1]][op[2]].distance = op[3];
}

/* AI action 73: the first slot of var b1 takes the next turn. */
void func_8007E8E0(u8 **pc, u8 enemy) {
    D_800D2DC0 = func_80079E7C(D_800D3400[enemy].vars[(*pc)[1]]) + 1;
}

/* AI action 74: reset every slot's turn timers (80078508), its order output
 * going to a scratch buffer. */
void func_8007E934(void) {
    u8 order[16];

    func_80078508(order);
}

/* AI condition 81: byte variable b1 == b2. */
s32 func_8007E954(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].bytes[op[1]] == op[2];
}

/* AI condition 82: variable b1 == b2 | b3 << 8. */
s32 func_8007E98C(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return D_800D3400[enemy].vars[op[1]] == value;
}

/* AI condition 83: byte variable b1 <= b2. */
s32 func_8007E9D0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].bytes[op[1]] <= op[2];
}

/* AI condition 84: variable b1 <= b2 | b3 << 8. */
s32 func_8007EA08(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return D_800D3400[enemy].vars[op[1]] <= value;
}

/* AI condition 85: byte variable b1 >= b2. */
s32 func_8007EA4C(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].bytes[op[1]] >= op[2];
}

/* AI condition 86: variable b1 >= b2 | b3 << 8. */
s32 func_8007EA84(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return D_800D3400[enemy].vars[op[1]] >= value;
}

/* AI condition 87: byte variable b1 == byte variable b2. */
s32 func_8007EAC8(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    return bytes[op[1]] == bytes[op[2]];
}

/* AI condition 88: variable b1 == variable b2. */
s32 func_8007EB08(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    return vars[op[1]] == vars[op[2]];
}

/* AI condition 89: byte variable b1 <= byte variable b2. */
s32 func_8007EB50(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    return bytes[op[1]] <= bytes[op[2]];
}

/* AI condition 8a: variable b1 <= variable b2. */
s32 func_8007EB90(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    return vars[op[1]] <= vars[op[2]];
}

/* AI condition 8b: byte variable b1 & b2. */
s32 func_8007EBD8(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return (D_800D3400[enemy].bytes[op[1]] & op[2]) != 0;
}

/* AI condition 8c: variable b1 & (b2 | b3 << 8). */
s32 func_8007EC10(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] + (op[3] << 8);

    return (D_800D3400[enemy].vars[op[1]] & value) != 0;
}

/* AI condition 8d: byte variable b1 & byte variable b2. */
s32 func_8007EC54(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    return (bytes[op[1]] & bytes[op[2]]) != 0;
}

/* AI condition 8e: variable b1 & variable b2. */
s32 func_8007EC94(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    return (vars[op[1]] & vars[op[2]]) != 0;
}

/* AI condition 8f: byte variable b1 != b2. */
s32 func_8007ECDC(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].bytes[op[1]] != op[2];
}

/* AI condition 90: variable b1 != b2 | b3 << 8. */
s32 func_8007ED14(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 value = op[2] | (op[3] << 8);

    return D_800D3400[enemy].vars[op[1]] != value;
}

/* AI condition 91: byte variable b1 != byte variable b2. */
s32 func_8007ED58(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u8 *bytes = D_800D3400[enemy].bytes;

    return bytes[op[1]] != bytes[op[2]];
}

/* AI condition 92: variable b1 != variable b2. */
s32 func_8007ED98(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    return vars[op[1]] != vars[op[2]];
}

/* AI condition 93: long b1 == long b2. */
s32 func_8007EDE0(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 *longs = D_800D3400[enemy].longs;

    return longs[op[1]] == longs[op[2]];
}

/* AI condition 94: long b1 <= long b2 (unsigned). */
s32 func_8007EE28(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u32 *longs = (u32 *)D_800D3400[enemy].longs;

    return longs[op[1]] <= longs[op[2]];
}

/* AI condition 95: slot b1's record +0x7c bit 0x8000. */
s32 func_8007EE70(u8 **pc) {
    return D_800CCCE8.records[(*pc)[1]].pilot.status7C >> 15;
}

/* AI condition 96: formation group b1 is empty. */
s32 func_8007EEA8(u8 **pc) {
    return D_800D301C[(*pc)[1]].count == 0;
}

/* AI condition 97: no party member (slots 0 and 1) is alive. */
s32 func_8007EED0(void) {
    return (D_800D39DC & 3) == 0;
}

/* AI condition 98: false while any enemy without slot info bit 0x80 is
 * listed and the alive mask has a bit above 4 set. */
s32 func_8007EEE8(void) {
    s32 result = 1;
    s32 i;

    for (i = 0; i < 8; i++) {
        if ((D_800D39DC >> 5) != 0 && !(D_800C3EB4[i + 3].hidden & 0x80)) {
            result = 0;
            break;
        }
    }
    return result;
}

/* AI condition 9b: the enemy's slot info +3 bit 0x80. */
s32 func_8007EF44(u8 enemy) {
    return D_800C3EB4[enemy + 3].hidden >> 7;
}

/* Run the AI action at *pc (opcodes 01-74; 62 does nothing here, 00, 6e,
 * 6f and 75-7f queue the opcode as an action, 8007a7bc) for `enemy`, with
 * `count` actions in the list at 800d2e5c, and step past it. Returns the new
 * action count. Instructions are op, b1, b2, b3 (docs/scripts/battle-ai.md,
 * tools/analysis/battle_ai.py). */
u8 func_8007EF6C(u8 **pc, u8 enemy, u8 count) {
    u8 *list = (u8 *)D_800D2E5C;

    switch (**pc) {
    case 0x01:
        count = func_8007A828(pc, list, count);
        break;
    case 0x02:
        func_8007A874(pc, list, enemy, count);
        break;
    case 0x03:
        func_8007A8B4(pc, list);
        break;
    case 0x04:
        func_8007A900(pc, enemy);
        break;
    case 0x05:
        func_8007A92C(pc, enemy);
        break;
    case 0x06:
        func_8007A968(pc, enemy);
        break;
    case 0x07:
        func_8007A9A8(pc);
        break;
    case 0x08:
        func_8007A9D0(pc, enemy);
        break;
    case 0x09:
        func_8007AA1C(pc, enemy);
        break;
    case 0x0A:
        func_8007AA60(pc, enemy);
        break;
    case 0x0B:
        func_8007AAB8(pc, enemy);
        break;
    case 0x0C:
        func_8007AAF4(pc, enemy);
        break;
    case 0x0D:
        func_8007AB30(pc, enemy);
        break;
    case 0x0E:
        func_8007AB68(pc, enemy);
        break;
    case 0x0F:
        func_8007ABA0(pc, enemy);
        break;
    case 0x10:
        func_8007ABD8(pc, enemy);
        break;
    case 0x11:
        func_8007AC30(pc, enemy);
        break;
    case 0x12:
        func_8007AC80(pc, enemy);
        break;
    case 0x13:
        func_8007ACDC(pc, enemy);
        break;
    case 0x14:
        func_8007AD24(pc, enemy);
        break;
    case 0x15:
        func_8007AD6C(pc, enemy);
        break;
    case 0x16:
        func_8007ADB0(pc, enemy);
        break;
    case 0x17:
        func_8007ADF4(pc, enemy);
        break;
    case 0x18:
        func_8007AE38(pc, enemy);
        break;
    case 0x19:
        func_8007AE98(pc, enemy);
        break;
    case 0x1A:
        func_8007AEF0(pc, enemy);
        break;
    case 0x1B:
        func_8007AF5C(pc, enemy);
        break;
    case 0x1C:
        func_8007AFAC(pc, enemy);
        break;
    case 0x1D:
        func_8007AFFC(pc, enemy);
        break;
    case 0x1E:
        func_8007B040(pc, enemy);
        break;
    case 0x1F:
        func_8007B084(pc, enemy);
        break;
    case 0x20:
        func_8007B0C8(pc, enemy);
        break;
    case 0x21:
        func_8007B134(pc, enemy);
        break;
    case 0x22:
        func_8007B198(pc, enemy);
        break;
    case 0x23:
        func_8007B208(pc, enemy);
        break;
    case 0x24:
        func_8007B264(pc, enemy);
        break;
    case 0x25:
        func_8007B2C0(pc, enemy);
        break;
    case 0x26:
        func_8007B310(pc, enemy);
        break;
    case 0x27:
        func_8007B360(pc, enemy);
        break;
    case 0x28:
        func_8007B3B0(pc, enemy);
        break;
    case 0x29:
        func_8007B3E4(pc, enemy);
        break;
    case 0x2A:
        func_8007B424(pc, enemy);
        break;
    case 0x2B:
        count = func_8007B4B8(pc, enemy, count);
        break;
    case 0x2C:
        func_8007B578(pc, enemy);
        break;
    case 0x2D:
        count = func_8007B608(pc, enemy, count);
        break;
    case 0x2E:
        func_8007B6C0(pc, enemy);
        break;
    case 0x2F:
        count = func_8007B7B0(pc, enemy, count);
        break;
    case 0x30:
        func_8007B8D4(pc, enemy);
        break;
    case 0x31:
        func_8007B914(pc, enemy);
        break;
    case 0x32:
        func_8007B958(pc, enemy);
        break;
    case 0x33:
        func_8007B98C(pc, enemy);
        break;
    case 0x34:
        func_8007B9C8(pc, enemy);
        break;
    case 0x35:
        func_8007BA04(pc, enemy);
        break;
    case 0x36:
        func_8007BA44(pc, enemy);
        break;
    case 0x37:
        func_8007BA88(enemy);
        break;
    case 0x38:
        func_8007BAB8(enemy);
        break;
    case 0x39:
        func_8007BAE8(pc, enemy);
        break;
    case 0x3A:
        func_8007BB2C(pc, enemy);
        break;
    case 0x3B:
        func_8007BB70(pc, enemy);
        break;
    case 0x3C:
        func_8007BBD8(pc, enemy);
        break;
    case 0x3D:
        func_8007BC40(pc, list, count);
        break;
    case 0x3E:
        func_8007BC84(pc, enemy);
        break;
    case 0x3F:
        func_8007BCE8(pc, enemy);
        break;
    case 0x40:
        func_8007BD5C(pc, enemy);
        break;
    case 0x41:
        func_8007BEA8(pc, enemy);
        break;
    case 0x42:
        func_8007C040(pc, enemy);
        break;
    case 0x43:
        func_8007C1A4(pc, enemy);
        break;
    case 0x44:
        func_8007C33C(pc, enemy);
        break;
    case 0x45:
        func_8007C4A0(pc, enemy);
        break;
    case 0x46:
        func_8007C580(pc, enemy);
        break;
    case 0x47:
        func_8007C678(pc, enemy);
        break;
    case 0x48:
        func_8007C75C(pc, enemy);
        break;
    case 0x49:
        func_8007C840(pc, enemy);
        break;
    case 0x4A:
        func_8007C9D4(pc, enemy);
        break;
    case 0x4B:
        func_8007CB20(pc, enemy);
        break;
    case 0x4C:
        func_8007CC50(pc, enemy);
        break;
    case 0x4D:
        func_8007CD10(pc, enemy);
        break;
    case 0x4E:
        func_8007CDD0(pc, enemy);
        break;
    case 0x4F:
        func_8007CEA4(pc, enemy);
        break;
    case 0x50:
        func_8007CFB8(pc, enemy);
        break;
    case 0x51:
        func_8007D0CC(pc, enemy);
        break;
    case 0x52:
        func_8007D148(pc, list, enemy, count);
        break;
    case 0x53:
        func_8007D1A8(pc, enemy);
        break;
    case 0x54:
        func_8007D1DC(pc, enemy);
        break;
    case 0x55:
        func_8007D30C(pc, enemy);
        break;
    case 0x56:
        func_8007D344(pc, enemy);
        break;
    case 0x57:
        func_8007D478(pc, enemy);
        break;
    case 0x58:
        func_8007D5B0(pc, enemy);
        break;
    case 0x59:
        func_8007D610(pc, enemy);
        break;
    case 0x5A:
        func_8007D6A8(pc, enemy);
        break;
    case 0x5B:
        func_8007D7B4(pc, enemy);
        break;
    case 0x5C:
        func_8007D8C0(pc, enemy);
        break;
    case 0x5D:
        func_8007DA1C(pc, enemy);
        break;
    case 0x5E:
        func_8007DB78(pc, enemy);
        break;
    case 0x5F:
        func_8007DCF8(pc, enemy);
        break;
    case 0x60:
        func_8007DE78(pc, enemy);
        break;
    case 0x61:
        func_8007DFD4(pc, enemy);
        break;
    case 0x62:
        /* only noted by the reaction script runner (80079ab0) */
        break;
    case 0x63:
        func_8007E154(pc, enemy);
        break;
    case 0x64:
        func_8007E1D0(pc, enemy);
        break;
    case 0x65:
        func_8007E234(pc, enemy);
        break;
    case 0x66:
        func_8007E334(pc, enemy);
        break;
    case 0x67:
        func_8007E438(pc, enemy);
        break;
    case 0x68:
        func_8007E554(pc, enemy);
        break;
    case 0x69:
        func_8007E674(enemy);
        break;
    case 0x6A:
        func_8007E6A0(pc, enemy);
        break;
    case 0x6B:
        func_8007E6F0(pc, enemy);
        break;
    case 0x6C:
        func_8007E740(pc, enemy);
        break;
    case 0x6D:
        func_8007E780(pc, enemy);
        break;
    case 0x70:
        func_8007E7C0(pc);
        break;
    case 0x71:
        func_8007E7E4(pc, enemy);
        break;
    case 0x72:
        func_8007E8AC(pc);
        break;
    case 0x73:
        func_8007E8E0(pc, enemy);
        break;
    case 0x74:
        func_8007E934();
        break;
    default:
        count = func_8007A7BC(pc, list, enemy, count);
        break;
    }
    func_80079934(pc);
    return count;
}

/* Evaluate the AI condition at *pc (opcodes 80-9b; 80 and 9a always hold)
 * and step past it. After opcode 99 the following conditions are or-ed
 * together up to the next non-condition byte. */
u8 func_8007F8C0(u8 **pc, u8 enemy) {
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
            result = func_8007E954(pc, enemy);
            break;
        case 0x82:
            result = func_8007E98C(pc, enemy);
            break;
        case 0x83:
            result = func_8007E9D0(pc, enemy);
            break;
        case 0x84:
            result = func_8007EA08(pc, enemy);
            break;
        case 0x85:
            result = func_8007EA4C(pc, enemy);
            break;
        case 0x86:
            result = func_8007EA84(pc, enemy);
            break;
        case 0x87:
            result = func_8007EAC8(pc, enemy);
            break;
        case 0x88:
            result = func_8007EB08(pc, enemy);
            break;
        case 0x89:
            result = func_8007EB50(pc, enemy);
            break;
        case 0x8A:
            result = func_8007EB90(pc, enemy);
            break;
        case 0x8B:
            result = func_8007EBD8(pc, enemy);
            break;
        case 0x8C:
            result = func_8007EC10(pc, enemy);
            break;
        case 0x8D:
            result = func_8007EC54(pc, enemy);
            break;
        case 0x8E:
            result = func_8007EC94(pc, enemy);
            break;
        case 0x8F:
            result = func_8007ECDC(pc, enemy);
            break;
        case 0x90:
            result = func_8007ED14(pc, enemy);
            break;
        case 0x91:
            result = func_8007ED58(pc, enemy);
            break;
        case 0x92:
            result = func_8007ED98(pc, enemy);
            break;
        case 0x93:
            result = func_8007EDE0(pc, enemy);
            break;
        case 0x94:
            result = func_8007EE28(pc, enemy);
            break;
        case 0x95:
            result = func_8007EE70(pc);
            break;
        case 0x96:
            result = func_8007EEA8(pc);
            break;
        case 0x97:
            result = func_8007EED0();
            break;
        case 0x98:
            result = func_8007EEE8();
            break;
        case 0x9B:
            result = func_8007EF44(enemy);
            break;
        }
        func_80079934(pc);
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

/* For party character 4 with its UI flag +0x8e set: close window 0 and
 * release the graphics block. */
void func_8007FB70(u8 member) {
    if (D_800D2D24[member] == 4 && D_800D2D28->unk8E != 0) {
        func_8008FA60(0);
        func_8007765C();
        D_800D2D28->unk8E = 0;
    }
}

/* Place the separator lines of a `count`-row list (rows from the 800c3214
 * table) and remember the selected row (clamped below `count`). */
void func_8007FBE0(u8 count, u8 selected) {
    s32 i;

    if (count == selected) {
        selected--;
    }
    for (i = 0; i < count - 1; i++) {
        setXY2(&D_800C3EA4->unk908[i * 2 + D_800CCB04.buffer], 0xC, D_800C3214[count - 3][i] + 0x5E, 0x12,
               D_800C3214[count - 3][i] + 0x5E);
    }
    D_800D2D28->unk97 = selected;
    D_800D2D28->unk98 = D_800CCB04.buffer;
}

/* Release the loaded menu module block (UI +0xae). */
void func_8007FCE8(void) {
    if (D_800D2D28->unkAE != 0) {
        func_800320E8(D_800D367C);
        D_800D2D28->unkAE = 0;
    }
}

/* Load file 2 (a member in a gear (800d32a1)) or 1 into a new heap block
 * unless loaded (UI +0xae). */
void func_8007FD38(u8 member) {
    s32 file;

    if (D_800D2D28->unkAE == 0) {
        func_8008AC50();
        file = 2;
        if (D_800D32A0[member].unk1 == 0) {
            file = 1;
        }
        func_8008AB94();
        D_800D367C = (void *)func_8008ABB8(func_800288EC(file), 0);
        func_800295D8(file, (s32)D_800D367C, 0, 0x80);
        func_8008AC50();
        D_800D2D28->unkAE = 1;
    }
}

/* Release the loaded file-3 block (UI +0x96). */
void func_8007FDEC(void) {
    if (D_800D2D28->unk96 != 0) {
        func_800320E8(D_800C3DE8);
        D_800D2D28->unk96 = 0;
    }
}

/* Load file 3 into a new heap block unless loaded (UI +0x96). */
void func_8007FE3C(void) {
    if (D_800D2D28->unk96 == 0) {
        func_8008AC50();
        func_8008AB94();
        D_800C3DE8 = (void *)func_8008ABB8(func_800288EC(3), 0);
        func_800295D8(3, (s32)D_800C3DE8, 0, 0x80);
        func_8008AC50();
        D_800D2D28->unk96 = 1;
    }
}

/* Allocate and clear the 0x5da4-byte *800d2db4 block. */
void func_8007FEC4(void) {
    D_800D2DB4 = (ListPrims *)func_8008ABB8(0x5DA4, 0);
    bzero(D_800D2DB4, 0x5DA4);
    D_800D2DB4->lineX = 0xA0;
    D_800D2DB4->lineY = 0x64;
}

/* The digit strings of a turn slot as one byte run (TurnSlot.digits),
 * addressed by shifting the slot index (slots are 0x40 bytes). */
#define SLOT_DIGITS(m) ((u8 *)D_800C3EAC + ((m) << 6) + 8)

/* Build the member's number strings 2-4 (glyphs 0x83 + digit, no leading
 * zeros) from 800d2c0c, set up the list block and, unless the member is
 * character 7, the menu lists; then show them. */
void func_8007FF14(u8 member) {
    s32 k;
    s32 n;
    u8 value;
    u8 digit;

    func_8009A2D4(member);
    for (k = 2; k < 5; k++) {
        value = *D_800D2C0C[k - 2];
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
    func_8007FEC4();
    if (D_800D2D24[member] != 7) {
        func_80089AF8(member);
        func_800716D8();
        func_800898F0(member);
        func_800716D8();
        D_800D2D28->unkAD = 1;
        D_800D2D28->unkC7 = 1;
    }
}

/* Leave a member's menu: clear UI +0xad, +0xc7, +0xa8, turn a 2 at 800d32a1
 * into 1 and release *800d2db4. */
void func_800800E8(u8 member) {
    D_800D2D28->unkAD = 0;
    D_800D2D28->unkC7 = 0;
    D_800D2D28->unkA8 = 0;
    if (D_800D32A0[member].unk1 == 2) {
        D_800D32A0[member].unk1 = 1;
    }
    func_800320E8(D_800D2DB4);
}

/* Run party member `member`'s command menu: reset the turn state's menu
 * fields, take the AP (800d32a0 +4) as spent and available, wait for its
 * panel to open, mark the unavailable menu items (status +0x7a bits; with
 * character 7 in a gear two more, with character 4 the items whose
 * equipment slots are broken), open the first page (1 attack, 2 when the
 * status bars it, 4 with item 9 blocked; 0x10/0x13 in a gear) and frame the
 * member and its target. Then, until the menu is done, the battle ends or
 * an event runs, redraw a changed page (8008d598) and run the page's
 * handler (pages 0x64/0x65 redraw 5/0x19). Finally close the panel, return
 * unspent AP when +0x2e3 is set (up to 28), tidy the windows when done and
 * reload the member's turn timer. */
void func_80080160(u8 member) {
    s32 i;
    u8 target;

    D_800C3EAC->unk2EA = 1;
    D_800C3EAC->unk2E9 = 0;
    D_800C3EAC->menuDone = 0;
    D_800C3EAC->unk2E0 = 0;
    D_800C3EAC->unk2E1[0] = 0;
    D_800C3EAC->unk2E1[1] = 0xFF;
    D_800C3EAC->unk2E1[2] = 0;
    D_800C3EAC->unk2E1[3] = 0;
    D_800C3EAC->unk2DC = 0;
    D_800C3EAC->unk2D4[0] = D_800C3EAC->unk2D4[1] = D_800D32A0[member].unk2[2];
    D_800D2D28->unk90[member] = 3;
    D_800C3EAC->unk2E7 = 0;
    D_800C3E28[1] = 0xFF;
    D_800C4929 = 0;
    while (D_800D2D28->unk90[member] != 1) {
        func_800716D8();
    }
    D_800D366C = 1;
    func_8008AA74(0x5A);
    D_800D366C = 0;
    for (i = 0; i < 16; i++) {
        D_800C3EAC->slots[member].items[i] = D_800CCCE8.records[member].pilot.status7A & D_800C3234[i];
    }
    if (D_800D2D24[member] == 7 && D_800D32A0[member].unk1 != 0) {
        D_800C3EAC->slots[member].items[13] = D_800C3234[13];
        D_800C3EAC->slots[member].items[4] = D_800C3234[4];
    }
    if (D_800D2D24[member] == 4) {
        if (D_800D32A0[member].unk1 == 0) {
            if (D_8006F8BA[D_8006D634.characters[4].entryItems[0]] == 0) {
                D_800C3EAC->slots[member].items[0] = D_800C3234[0];
            }
            if (D_8006F8BA[D_8006D634.characters[4].entryItems[3]] == 0) {
                D_800C3EAC->slots[member].items[2] = D_800C3234[2];
            }
        } else {
            if (D_8006F8EA[D_8006D634.gears[D_8006D634.characters[4].gearId].partItems[0]] == 0) {
                D_800C3EAC->slots[member].items[0] = D_800C3234[0];
            }
            if (D_8006F8EA[D_8006D634.gears[D_8006D634.characters[D_800D2D24[member]].gearId].partItems[3]] == 0) {
                D_800C3EAC->slots[member].items[2] = D_800C3234[2];
            }
        }
    }
    if (D_800D32A0[member].unk1 == 0) {
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 1;
            D_800C3E28[0] = 0;
        } else {
            D_800C3EAC->page = 4;
            D_800C3E28[0] = 3;
        }
        if (D_800CCCE8.records[member].pilot.status7C & 2) {
            D_800C3EAC->page = 2;
            D_800C3E28[0] = 1;
        }
        func_8007FEC4();
    } else {
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 0x10;
            D_800C3E28[0] = 0;
        } else {
            D_800C3EAC->page = 0x13;
            D_800C3E28[0] = 3;
        }
        func_8007FF14(member);
    }
    func_80085E78();
    func_8009AB00(member);
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    target = D_800C3EAC->slots[member].defaultTarget;
    if (D_800C3EAC->slots[member].defaultTarget == 0xFF) {
        target = 0;
    }
    func_800BCD98(func_80089C08(target));
    while (D_800C3EAC->menuDone == 0 && D_800C48EA == 0 && D_800C3EAC->eventsDone == 0) {
        if (D_800D32A0[member].unk1 == 0) {
            func_8007FBE0(D_800C3EAC->unk2D4[1], D_800C3EAC->unk2D4[0]);
        }
        if (D_800C3EAC->unk2E1[1] != D_800C3EAC->page) {
            D_800C3EAC->repeatArmed = 0;
            D_800C3EAC->unk2E1[1] = D_800C3EAC->page;
            D_800D2D28->unkCB = 0;
            D_800D2D28->unkA3 = func_8008D598(member, D_800C3EAC->page, D_800C3EAC->unk2E0);
            D_800D2D28->unkCB = 1;
        }
        if (D_800D3014 == 0xFF) {
            break;
        }
        func_800716D8();
        if (D_800C3EAC->eventsDone != 0) {
            break;
        }
        if (D_800C3EAC->unk2E0 == 0) {
            D_800D366C = 1;
        }
        switch (D_800C3EAC->page) {
        case 1:
            func_8008115C(member);
            break;
        case 2:
            func_80081318(member);
            break;
        case 3:
            func_80081504(member);
            break;
        case 4:
            func_800816F8(member);
            break;
        case 0x64:
            D_800C3EAC->unk2E1[1] = 0xFF;
        case 5:
            func_80081B58(member);
            break;
        case 7:
            func_800820A4(member);
            break;
        case 8:
            func_800822C4(member);
            break;
        case 9:
            func_80082504(member);
            break;
        case 0xA:
            func_80082820(member);
            break;
        case 0x10:
            func_800829F4(member);
            break;
        case 0x11:
            func_80082BB0(member);
            break;
        case 0x12:
            func_80082D4C(member);
            break;
        case 0x13:
            func_80082F7C(member);
            break;
        case 0x15:
            func_800830A8(member);
            break;
        case 0x16:
            func_80083340(member);
            break;
        case 0x17:
            func_80083580(member);
            break;
        case 0x18:
            func_80083748(member);
            break;
        case 0x65:
            D_800C3EAC->unk2E1[1] = 0xFF;
        case 0x19:
            func_80083948(member);
            break;
        }
    }
    D_800D2D28->unkCB = 0;
    D_800D2D28->unkAF = 0;
    D_800D2D28->unk90[member] = 0;
    D_800D2D28->barShown[member] = 0;
    if (D_800C3EAC->unk2E1[2] != 0) {
        D_800D32A0[member].unk0 += D_800C3EAC->unk2D4[0];
        if (D_800D32A0[member].unk0 >= 29) {
            D_800D32A0[member].unk0 = 28;
        }
    }
    D_800D366C = 0;
    func_800BAF40(member, 0x100);
    if (D_800C3EAC->menuDone != 0 || D_800C48EA != 0) {
        func_8007FCE8();
        func_8007FDEC();
        func_800800E8(member);
        func_8007FB70(member);
    }
    D_800D2DCC.timers[1][member] = D_800D2DCC.timers[0][member];
    D_800D2D28->reaction[member] = 1;
    D_800C4928 = 0;
}

/* The next slot in turn order, other than `actor`, with the lowest turn
 * timer (undefined when there is none). */
s32 func_80080AE4(u8 actor) {
    u8 lowest = 0xFF;
    s32 position = D_800D2DCC.cursor;
    s32 slot;
    u8 next;

    do {
        slot = D_800D2DCC.order[position];
        position++;
        if (D_800D2DCC.timers[1][slot] < lowest && slot != actor) {
            next = slot;
            lowest = D_800D2DCC.timers[1][slot];
        }
        if (position == 11) {
            position = 0;
        }
    } while (position != D_800D2DCC.cursor);
    return next;
}

/* Close the actor's event queue (event 0xfe); menu effects off. */
void func_80080B64(u8 actor) {
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xFE;
    D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
    D_800D366C = 0;
}

/* Events done; for a party actor outside 800c204c refresh its menu state. */
void func_80080BD0(void) {
    D_800C3EAC->eventsDone = 1;
    if (D_800C3EAC->actor < 3 && D_800C204C == 0) {
        func_8007FCE8();
        func_8007FDEC();
        func_800800E8(D_800C3EAC->actor);
        func_8007FB70(D_800C3EAC->actor);
    }
}

/* Set the 0x38-byte entry `index` of *800d3278 active (+0x34). */
void func_80080C6C(u8 index) {
    D_800D3278->entries[index].active = 1;
}

/* Take an automatic turn for party member `member` (8009bac4 chooses):
 * an attack (1) at a random reachable target other than itself, a skill (2)
 * when its EP cover the cost, or defend in the gear (4); anything else, or
 * too little EP, passes the turn. Then wait for the menu or the events to
 * finish. */
void func_80080C94(u8 member) {
    u8 choice[2];
    u8 targets[11];
    s32 i;
    u8 *next;
    u8 pass = 1;
    u8 cost;

    func_800716D8();
    for (i = 0; i < 11; i++) {
        targets[i] = 0xFF;
    }
    D_800C3EAC->unk2EA = 1;
    D_800C3EAC->menuDone = 0;
    func_8009AB00(member);
    func_8009BAC4(member, choice, (s16 *)&D_800C3EAC->slots[member].items[7]);
    if (choice[0] == 1 || choice[0] == 2) {
        for (i = 0, next = targets; i < 11; i++) {
            if (func_80083FF4(member, i)) {
                *next++ = i;
            }
        }
        while ((D_800C3E2C = targets[func_8001BD40(0, 10)]) == 0xFF || D_800C3E2C == member) {
        }
    }
    func_800879A8(member, D_800C3E2C);
    D_800C3EAC->slots[member].defaultTarget = D_800C3E2C;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800716D8();
    D_800C204C = 1;
    switch (choice[0]) {
    case 1:
        D_800C3E18 = 0;
        if (D_800D2D24[member] != 4) {
            D_800C3FE8[D_800C3EAC->eventCount].actor = member;
            D_800C3FE8[D_800C3EAC->eventCount].type = 0xFD;
            D_800C3FE8[D_800C3EAC->eventCount].parameter = 0;
            D_800C3FE8[D_800C3EAC->eventCount].targetMask = func_80089C08(D_800C3EAC->slots[member].defaultTarget);
            D_800C3EAC->eventCount++;
        }
        if (D_800D32A0[member].unk1 == 0) {
            D_800C3EAC->unk2DC = 0;
        } else {
            D_800C3EAC->unk2DC = choice[1];
        }
        func_800B89FC(func_800877E0(member, D_800C3EAC->slots[member].defaultTarget), member,
                      D_800C3EAC->slots[member].defaultTarget, func_80080AE4(member));
        func_80087AF0(member, choice[1] + 1);
        func_80080B64(member);
        pass = 0;
        break;
    case 2:
        cost = D_800CCCE8.partyCommands[member][choice[1] + 0x16].cost;
        if (D_800CCCE8.records[member].pilot.ep >= cost) {
            D_800CCCE8.records[member].pilot.ep -= cost;
            D_800C3EAC->unk2E6 = choice[1];
            func_8008ADD0(member);
            pass = 0;
        }
        break;
    case 4:
        func_800826CC(member);
        pass = 0;
        D_800C3EAC->menuDone = 1;
        break;
    }
    if (pass) {
        func_8009AA44(member);
        D_800C3EAC->unk2EA = 0;
        D_800C3EAC->menuDone = 1;
    }
    while (D_800C3EAC->menuDone == 0 && D_800C48EA == 0 && D_800C3EAC->eventsDone == 0) {
        func_800716D8();
    }
    D_800C204C = 0;
}
