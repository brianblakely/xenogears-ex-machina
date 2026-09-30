/* Battle unit from 800792F8: 800793F0's jump table (8006FB38) is at 0 mod 8
 * after the previous unit's at 4 mod 8 (docs/matching.md). Its rodata starts
 * at 8006FB08 with 800792F8's strings, or at 8006FB38 if those belong to the
 * previous unit; both fit, as does any text boundary after 800745EC. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "menu_pages.h"
#include "resolver.h"
#include "action_resolve.h"

/* Script error screen: clear the event types and, on a debug build (the
 * 8005917c flag), print "Language Error" with the actor and script number
 * forever, the text shifted one column every three frames. */
#ifdef NON_MATCHING
void func_800792F8(actor, number)
u8 actor;
u8 number;
{
    s32 offset;
    s32 i;
    s32 column;
    s32 frames;

    for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
        ((BattleEvent *)((u8 *)D_800C3FE8 + offset))->type = 0xFF;
    }
    frames = 0;
    column = 0;
    if (*D_8005917C != -1) {
        while (1) {
            for (i = 0; i < column; i++) {
                func_8003700C(" ");
            }
            frames++;
            func_8003700C("\n\n\n\n\n\nLanguage Error\n");
            func_8003700C("\t\t\tActor%X\t\tNo%x\n\n", actor, number);
            func_800716D8();
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
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800792F8", func_800792F8);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800792F8", func_800793F0);

/* Close the actor's event queue: event 0x1b for a flagged actor, then the
 * closing event 0xfe; menu effects off. */
void func_80079674(u8 actor) {
    if (D_800D32A0[actor].unk1 != 0) {
        D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0x1B;
        D_800C3EAC->eventCount++;
    }
    D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xFE;
    D_800D366C = 0;
}

/* Execute the actor's action list with its attack model and wait until the
 * queued events are done. */
void func_80079778(u8 actor) {
    D_800C3EAC->eventCount = 0;
    func_80085350();
    func_80085388();
    D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
    func_800716D8();
    func_800B89FC(1, actor, 0, func_80080AE4(actor));
    func_800793F0(actor);
    func_80079674(actor);
    while (D_800C3EAC->eventsDone == 0) {
        func_800716D8();
    }
}

/* Tell enemy `target` who acts on it: var 7 = the actor's bit, bytes 9..13
 * from 800d2ca4 and byte 14 whether the actor's default target is in the
 * 800c48e8 mask. */
void func_80079840(u8 actor, u8 target) {
    s32 i;
    u8 enemy;

    if (target >= 3) {
        enemy = target - 3;
        D_800D3400[enemy].vars[7] = func_80089C08(actor);
        for (i = 0; i < 5; i++) {
            D_800D3400[enemy].bytes[9 + i] = D_800D2C94.enemyBytes[i];
        }
        if ((u16)D_800C48E8 & func_80089C08(D_800C3EAC->slots[actor].defaultTarget)) {
            D_800D3400[enemy].bytes[14] = 1;
        } else {
            D_800D3400[enemy].bytes[14] = 0;
        }
    }
}

/* Advance the AI script by one four-byte instruction. */
void func_80079934(u8 **pc) {
    *pc += 4;
}

/* Skip the script's conditions (0x80 and up), then everything but
 * actions 0x80..0xef. */
void func_80079948(u8 **pc) {
    while (**pc >= 0x80) {
        func_80079934(pc);
    }
    while ((u8)(**pc - 0x80) >= 0x70) {
        func_80079934(pc);
    }
}

/* Run enemy `slot`'s AI script: clear the action list and event types, then
 * evaluate conditions and actions until 0xfd or 0xff. */
void func_800799C8(u8 slot, u16 attacking) {
    u8 *pc;
    u8 count;
    u8 enemy;
    u8 *p;
    s32 offset;

    count = 0;
    enemy = slot - 3;
    pc = D_800D3400[enemy].script;
    p = (u8 *)D_800D2E5C;
    do {
        *p++ = 0;
    } while (p < (u8 *)D_800D2E5C + 0x100);
    for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
        ((BattleEvent *)((u8 *)D_800C3FE8 + offset))->type = 0xFF;
    }
    while (*pc != 0xFD && *pc != 0xFF) {
        if (*pc >= 0x80) {
            if (!func_8007F8C0(&pc, enemy)) {
                func_80079948(&pc);
            }
        } else {
            count = func_8007EF6C(&pc, enemy, count);
        }
    }
}

/* Run enemy `slot`'s reaction script when armed (and the enemy is not down,
 * unless +0x34 bit 0x800 lets it react), then execute its action list.
 * Returns whether the script ran action 0x62. */
s32 func_80079AB0(u8 slot) {
    u8 *pc;
    s32 ranAction62 = 0;
    u8 enemy = slot - 3;
    u8 count = 0;
    u8 *p;

    if (!(D_800CCCE8.records[enemy + 3].pilot.status7C & 0x8000) || (D_800CCCE8.records[enemy + 3].pilot.flags34 & 0x800)) {
        D_800D2E5C[0].type = 0;
        if (D_800C3D18[enemy].armed != 0) {
            pc = D_800D3400[enemy].reaction;
            p = (u8 *)D_800D2E5C;
            do {
                *p++ = 0;
            } while (p < (u8 *)D_800D2E5C + 0x100);
            while (*pc != 0xFD && *pc != 0xFF) {
                if (*pc >= 0x80) {
                    if (!func_8007F8C0(&pc, enemy)) {
                        func_80079948(&pc);
                    }
                } else {
                    if (*pc == 0x62) {
                        ranAction62 = 1;
                    }
                    count = func_8007EF6C(&pc, enemy, count);
                }
            }
        }
        if (D_800D2E5C[0].type != 0) {
            func_800793F0(enemy + 3);
        }
    }
    return ranAction62;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800792F8", func_80079C24);

/* Show battle message window `index`. */
void func_80079E18(u8 index) {
    D_800D2D28->windows[4] = 1;
    D_800D36C8[index].shown = 1;
}

/* Hide battle message window `index`. */
void func_80079E4C(u8 index) {
    D_800D2D28->windows[4] = 0;
    D_800D36C8[index].shown = 0;
}

/* The first slot in `mask`; 11 when none. */
u8 func_80079E7C(u16 mask) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (func_80089C9C(mask, slot)) {
            break;
        }
    }
    return slot;
}
