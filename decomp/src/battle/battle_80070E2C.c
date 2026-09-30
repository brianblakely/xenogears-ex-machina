/* Battle unit from 80070E2C: its rodata starts at 8006FAF4, after the
 * overlay's number, with 800745EC's jump table at 4 mod 8 (docs/matching.md). */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "menu_pages.h"
#include "resolver.h"
#include "action_resolve.h"

/* Start the 801e5000 module: reserve its heap span and load it. */
void func_80070E2C(void) {
    s32 block;

    if (D_800C3D48 != 0) {
        func_8008AB4C();
        block = func_8008ABB8(4, 1);
        D_800D3284 = block;
        D_800D328C = func_8008ABB8(block - 0x801E5000, 1);
        func_800295D8(1, 0x801E5000, 0, 0x80);
        func_8008AC50();
        func_801E5160();
    }
}

/* Forward a byte argument to the 801e5000 module when it is loaded. */
void func_80070EB0(s32 value) {
    if (D_800C3D48 != 0) {
        func_801E879C(value & 0xFF);
    }
}

/* Fade the battle music out once the escape outcome is set, unless the
 * 801e5000 module handled it. */
void func_80070EDC(void) {
    s32 handled = 0;

    if (D_800C3D48 != 0) {
        handled = func_801E563C();
    }
    if ((handled & 0xFF) == 0 && D_800C48EA == 0x81) {
        func_8003A89C(D_800C3E54, 0, 0xF0);
    }
}

/* The battle: allocate the battle state, load the modules and the scene,
 * run turns until an outcome or an exit is set, then settle the outcome
 * (result code in D_800594D0) and hand over to the 801de000 module. */
void func_80070F40(void) {
    s32 span;
    s32 block;
    u8 result;
    u8 mode = 1;
    u8 *state;
    s32 i;
    u8 outcome;

    D_800C3EA4 = (BattleGraphics *)func_8008ABB8(0xA2B4, 0);
    D_800D2D28 = (BattleUi *)func_8008ABB8(0x10C, 0);
    D_800C3EAC = (TurnState *)func_8008ABB8(0x2F8, 0);
    bzero(D_800C3EA4, 0xA2B4);
    bzero(D_800D2D28, 0x10C);
    bzero(D_800C3EAC, 0x2F8);
    D_8005959C = 0;
    D_800C3E29 = 0xFF;
    D_800C3E28 = 0xFF;
    D_800D366C = 0;
    D_800C3E54 = D_80062528;
    if (D_8005947C != 0) {
        D_80059508 = D_8005947C - 1;
        if (D_80059180 != 0) {
            D_80059180 = 0;
            func_8003A89C(D_80062528, 0x7F, 0x3C);
        }
        if (D_800594F8 == 0) {
            D_8005947C = 0;
        }
    }
    if (D_800594F8 != 0) {
        func_80028470(0x10, 2);
        block = func_8008ABB8(4, 1);
        span = func_8008ABB8(block - 0x801E0000, 1);
        func_800295D8(1, 0x801E0000, 0, 0x80);
        func_80028A60(0);
        func_800320E8((void *)block);
        func_800320E8((void *)span);
        if (D_8005947C == 0) {
            func_801E0A34();
        } else {
            D_80059508 = D_8005947C - 1;
            D_8005947C = 0;
        }
    }
    if (*D_8005917C != -1) {
        func_80028470(0x10, 2);
        func_800295D8(6, 0x80280000, 0, 0x80);
        func_80028A60(0);
    }
    memmove(D_8006F9DC, D_800658DC[D_80059508], 0x20);
    func_800B8098(D_8005954C);
    func_8007252C();
    D_800C3E4C = 2;
    func_800B81BC(D_800C3DEC);
    func_800B39C0(0, 2, 0xFF, 0xFF, 0xFF);
    if (D_800C3D48 == 0) {
        func_800B39C0(0x14, 2, 0, 0, 0);
    }
    if (D_800594F8 != 0) {
        D_800C3E54 = func_800397FC(D_80062648, 0x7F, 0);
    }
    D_800D3364 = D_8005949C;
    D_800C3EB0 = D_8005949C;
    func_80077990();
    D_800D3298 = 1;
    func_800BC404(D_800D39DC);
    func_800716D8();
    func_8007819C();
    while (D_800CCC58 == 0) {
        func_800716D8();
    }
    func_800320E8(D_800595D0);
    if (D_8005954C != 4) {
        func_8003A094(D_800595D0);
    }
    func_8003852C(D_800595D0);
    func_800320E8(D_80059480);
    func_800320E8(D_800594AC);
    func_80070E2C();
    func_80070EB0(1);
    if (D_800D2FC4 == 0) {
        D_800C3E4C = 1;
    }
    func_8009892C();
    for (i = 0; i < 3; i++) {
        if (D_800D2D24[i] != 0x7F) {
            D_800C3E0C[i].mask0 = D_8006ECF4[D_800D2D24[i]].mask0;
            D_800C3E0C[i].mask2 = D_8006ECF4[D_800D2D24[i]].mask2;
        }
    }
    while (D_800C48EA == 0 && D_800D2FC4 == 0) {
        if (D_800CCC58 != 0) {
            func_800723E0();
        }
        func_800716D8();
    }
    state = &D_800C48EA;
    if (!(*state & 0xC0)) {
        result = 0;
    } else if (*state & 0x40) {
        result = 1;
    } else if (D_800C3D48 == 0) {
        result = 2;
    } else if (D_800C3D5C != 0) {
        result = 3;
        D_800594D0 = result;
        *state = 1;
    }
    switch (result) {
    case 0:
        D_800594D0 = 0;
        mode = 0;
    case 3:
        if (D_800C3D48 != 0) {
            D_800D3278->unk394[0] = 0xFF;
            if (D_800C3D44 != 0 || D_800D2FC4 == 0) {
                u8 *battleOutcome = &D_800C48EA;

                D_800D3278->unk800 = 0;
                outcome = *battleOutcome;
                *battleOutcome = 0;
                for (i = 0; i < 3; i++) {
                    D_800D3278->unk394[0x10 + i] = D_800CCCE8.records[i].pilot.status7C & 0x8000;
                    D_800D3278->unk394[0x10 + i] |= D_800CCCE8.records[i].gear.status7C & 0x8000;
                }
                func_800C0F70();
                func_80070EB0(1);
                D_800C48EA = outcome;
            }
        }
        break;
    case 1:
        D_800594D0 = 2;
        mode = 2;
        break;
    case 2:
        D_800594D0 = 1;
        mode = 1;
        break;
    }
    func_800B8D7C();
    if (D_800D2D50 != 0) {
        mode = 1;
    }
    func_80028470(0x10, 0);
    D_800D2D3C = func_8008ABB8(4, 1);
    D_800D2F60 = func_8008ABB8(D_800D2D3C - 0x801DE000, 1);
    func_800295D8(4, 0x801DE000, 0, 0x80);
    func_800B853C(mode);
    while (D_800CCC58 != 0) {
        func_800716D8();
    }
    if (D_800C3D48 == 0 && !(D_800C48EA & 0x40) && !(D_8006F9DC[1] & 8)) {
        func_800B39C0(0x40, 2, 0x40, 0x40, 0x40);
    }
    func_80070EDC();
    func_801E252C();
    func_800320E8((void *)D_800D2D3C);
    func_800320E8((void *)D_800D2F60);
}

/* One battle frame: the 80280000 module's hook when present, then the task
 * runner. */
s32 func_800716D8(void) {
    if (*D_8005917C != -1) {
        func_8028022C();
    }
    func_800BE790();
    return 0;
}

/* One ATB tick for every present slot that is not yet ready. */
void func_8007171C(void) {
    s32 slot;
    s32 step;
    u8 *ready;
    u16 flags;
    u8 delay;
    s16 *toggle;
    s16 *timer;

    if (D_800D3298 != 0) {
        slot = 0;
        ready = D_800D2DCC.ready;
        for (; slot < 11; ready++, slot++) {
            if (D_800D2DCC.present[slot] == 0 || *ready != 0) {
                continue;
            }
            step = 1;
            if ((D_800CCCE8.records[slot].pilot.status84.half.active | D_800CCCE8.records[slot].pilot.status84.half.permanent) & 0x8000) {
                step = 2;
            }
            if (D_800CCCE8.records[slot].pilot.status7C & 0x1000) {
                toggle = &D_800D2DCC.timers[2][slot];
                if ((*toggle ^= 1) != 0) {
                    continue;
                }
            }
            flags = D_800CCCE8.records[slot].pilot.status7C;
            if (flags & 0x2000) {
                delay = D_800CCCE8.records[slot].statusTimers[0] -= step;
                if (delay == 0) {
                    D_800CCCE8.records[slot].statusTimers[0] = 0;
                    D_800CCCE8.records[slot].pilot.status7C &= 0xDFFF;
                }
                continue;
            }
            if ((flags & 0x80) || (D_800CCCE8.records[slot].pilot.status80 & 0x1000)) {
                continue;
            }
            timer = &D_800D2DCC.timers[1][slot];
            if ((*timer -= step) <= 0) {
                *ready = 1;
                *timer = 0;
            }
        }
    }
}

/* Reload the acting slot's turn timer and clear its ready flag. */
void func_800718BC(void) {
    u8 actor = D_800C3EAC->actor;

    if (D_800D2DCC.ready[actor] != 0xFF) {
        D_800D2DCC.ready[actor] = 0;
    }
    D_800D2DCC.timers[1][D_800C3EAC->actor] = func_80098AF8(D_800C3EAC->actor, 0);
    D_800D2DCC.timers[0][D_800C3EAC->actor] = D_800D2DCC.timers[1][D_800C3EAC->actor];
}

/* Render the pending battle message into its image, upload it and hold it
 * for three frames. */
void func_80071964(void) {
    s32 frames;

    if (D_800D2C94.message != 0 && (D_800D2C94.targets & D_800C48E8) == 0) {
        frames = 3;
        D_800D39B8.width = func_80034EAC(func_80033728(D_800D39F0, D_800D2C94.message),
                                         D_800D39B8.pixels, 0x39, 1);
        LoadImage(&D_800D39B8.rect, D_800D39B8.pixels);
        do {
            frames--;
            func_800716D8();
        } while (frames != 0);
    }
}

/* Clear the party's reaction flags. */
void func_80071A08(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800C3EAC->reaction[i] = 0;
    }
}

/* Publish the party's reaction flags to the UI, then wait a frame. */
void func_80071A38(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800D2D28->reaction[i] = D_800C3EAC->reaction[i];
    }
    func_800716D8();
}

/* Hide the eight battle messages and clear two UI bytes, then wait a
 * frame. */
/* Hide the eight battle messages and close windows 4 and 5, then run a
 * frame. */
void func_80071A8C(void) {
    s32 offset;

    /* The loop steps a byte offset through the entries. */
    for (offset = 7 * sizeof(BattleMessage); offset >= 0; offset -= sizeof(BattleMessage)) {
        ((BattleMessage *)((u8 *)D_800D36C8 + offset))->shown = 0;
    }
    D_800D2D28->windows[5] = 0;
    D_800D2D28->windows[4] = 0;
    func_800716D8();
}

/* Show the pending battle message (window 7) until a button is pressed or
 * 59 frames pass. */
void func_80071AE0(void) {
    s32 frames;

    if (D_800D2C94.message != 0 && (D_800D2C94.targets & D_800C48E8) == 0) {
        func_80079E18(7);
        frames = 0x3B;
        D_800C3EAC->eventsDone = 0;
        do {
            func_800716D8();
        } while (D_800D3014 == 8 && --frames != 0);
        D_800C3EAC->eventsDone = 1;
        func_80079E4C(7);
        func_800716D8();
    }
}

/* Start the turn of the acting slot (turn state actor + 1; none when 0). An
 * enemy runs its AI script (unless mode is set) and shows its name; a party
 * member gets its panel highlight and its command menu. Then each slot's
 * default target is chosen and the turn's actions play out. */
#ifdef NON_MATCHING
void func_80071B94(u8 mode) {
    s32 i;
    s32 offset;
    u8 actor;
    u16 flags;
    s32 layout;
    u8 *message;
    u8 *bytes;

    D_800C3E8C = 1;
    message = &D_800D2C94.message;
    *message = 0;
    if (D_800C3EAC->actor == 0) {
        return;
    }
    func_80085350();
    func_80071A08();
    D_800D3298 = 0;
    D_800C3EAC->actor--;
    D_800C4922 = D_800C3EAC->actor;
    D_800C3EAC->eventCount = 0;
    D_800C3EAC->eventsDone = 0;
    for (i = 4, bytes = message - 7; i >= 0; i--) {
        *bytes-- = 0;
    }
    D_800D2C94.targets = 0;
    func_80099890(D_800C3EAC->actor);
    if (D_800C3EAC->actor >= 3) {
        if (mode == 0) {
            func_800799C8(D_800C3EAC->actor, D_800CCCE8.records[D_800C3EAC->actor].pilot.status80 & 0x2000);
            actor = D_800C3EAC->actor;
            if (!(D_800C3EB4[actor].hidden & 0x80) && !(D_800CCCE8.records[actor].pilot.flags34 & 0x400)) {
                D_800D2D28->windows[5] = 1;
                D_800D36C8[0].width = func_80034EAC(func_80033728(D_800C3DDC, D_800C3E3D[D_800C3EAC->actor]),
                                                    D_800D36C8[0].pixels, 0x39, 0);
                func_800769E8(&D_800D36C8[0].rect, D_800D36C8[0].pixels);
                D_800D36C8[0].shown = 1;
            }
        }
        actor = D_800C3EAC->actor;
        if (!(D_800CCCE8.records[actor].pilot.status7C & 0x2080) &&
            !(D_800CCCE8.records[actor].pilot.status80 & 0x1000)) {
            func_80079778(actor);
            func_80071A8C();
            func_80071964();
            func_80071AE0();
        }
        func_80071A8C();
        func_80070EB0(0);
    } else {
        /* The loop steps a byte offset through the entries. */
        for (offset = 7 * sizeof(EnemyReaction); offset >= 0; offset -= sizeof(EnemyReaction)) {
            ((EnemyReaction *)((u8 *)D_800C3D18 + offset))->unk1[1] = 0;
        }
        layout = D_800D3280 * 3;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].x0 = D_800C3254[layout + D_800C3EAC->actor] + 0x10 + D_800C3EAC->actor * 0x60;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].y0 = 8;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].x1 = D_800C3254[layout + D_800C3EAC->actor] + D_800C3EAC->actor * 0x60 + 0x28;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].y1 = 8;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].x2 = D_800C3254[layout + D_800C3EAC->actor] + 0x10 + D_800C3EAC->actor * 0x60;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].y2 = 0x20;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].x3 = D_800C3254[layout + D_800C3EAC->actor] + D_800C3EAC->actor * 0x60 + 0x28;
        D_800C3EA4->unk63C8[D_800CCB04.buffer].y3 = 0x20;
        D_800C3EA4->unk6414 = D_800CCB04.buffer;
        D_800C3EA4->unk6415 = 1;
        actor = D_800C3EAC->actor;
        if (!(D_800CCCE8.records[actor].pilot.status7C & 0x2080)) {
            flags = D_800CCCE8.records[actor].pilot.status80;
            if (!(flags & 0x1000)) {
                if (!(flags & 0x2000)) {
                    func_80080160(actor);
                } else {
                    func_80080C94(actor);
                }
                if (D_800C3EAC->unk2EA != 0) {
                    D_8005941C++;
                }
                D_800D36C0 = D_800C3EAC->actor;
                D_800D2D28->unk97 = 0;
                func_800BCD98(0);
            }
        }
        func_80071964();
        func_80071AE0();
        func_80071A8C();
        func_80070EB0(0);
        for (i = 0; i < 8; i++) {
            if (func_80089C9C(D_800D2C94.targets, i + 3)) {
                D_800C3D18[i].unk1[1] = 1;
            }
        }
        func_80079C24();
    }
    func_80071A38();
    func_80072270();
    D_800C3EA4->unk6415 = 0;
    for (i = 0; i < 11; i++) {
        D_800C3EAC->slots[i].defaultTarget = func_800841E0(i);
        D_800C3EB4[i].targetCode = func_80085310(i, D_800C3EAC->slots[i].defaultTarget);
    }
    func_800BA4E0(func_80080AE4(D_800C3EAC->actor));
    func_80071A08();
    func_8007252C();
    if (D_800C48EA == 0) {
        func_80085B58(D_800C3EAC->actor);
    }
    func_8007252C();
    func_80071A38();
    func_800BFE48();
    func_800718BC();
    D_800D3298 = 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80071B94);
#endif

/* Party members held by the mask 800d2c9e lose their ready flag, restart
 * their turn timer from its reload value and show the held marker. */
void func_80072270(void) {
    s32 member;

    if (D_800D2C94.held & 7) {
        for (member = 0; member < 3; member++) {
            if (func_80089C9C(D_800D2C94.held, member)) {
                D_800D2DCC.ready[member] = 0;
                D_800D2DCC.timers[1][member] = D_800D2DCC.timers[0][member];
                D_800D2D28->reaction[member] = 1;
            }
        }
    }
    D_800D2C94.held = 0;
}

/* Give every enemy in the act-together mask its turn: clear the event types,
 * queue action 0x17 and run the turn procedure. The cleared 0x100-byte action
 * buffer pointer is never initialised in the original. */
#ifdef NON_MATCHING
void func_80072324(void) {
    s32 slot;
    s32 offset;
    u8 *p;
    u8 *end;
    u8 *actions;

    for (slot = 3; slot < 11; slot++) {
        if (func_80089C9C(D_800D39E0, slot)) {
            p = actions;
            end = p + 0x100;
            do {
                *p++ = 0;
            } while (p < end);
            for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
                ((BattleEvent *)((u8 *)D_800C3FE8 + offset))->type = 0xFF;
            }
            D_800D2E5C[0].type = 4;
            D_800D2E5C[0].param = 0x17;
            func_80085AC4(slot);
            func_80071B94(1);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80072324);
#endif

/* Select the next slot to act: the forced slot, else the next ready slot in
 * the turn order from the cursor; then run the turn procedure. With slots
 * acting together, run their pass instead. */
#ifdef NON_MATCHING
void func_800723E0(void) {
    s32 position;
    u8 *cursor;
    s32 slot;
    u8 next;

    if (D_800D39E0 == 0) {
        if (D_800D2DC0 != 0) {
            D_800C3EAC->actor = D_800D2DC0;
            D_800D2DCC.ready[D_800D2DC0 - 1] = 1;
            slot = D_800D2DC0;
            D_800D2DC0 = 0;
            D_800D2DCC.timers[1][slot - 1] = 0;
        } else {
            D_800C3EAC->actor = 0;
            cursor = &D_800D2DCC.cursor;
            position = *cursor;
            do {
                slot = D_800D2DCC.order[position];
                if (D_800D2DCC.ready[slot] == 1) {
                    D_800C3EAC->actor = slot + 1;
                    *cursor = next = position + 1;
                    if (next == 11) {
                        *cursor = 0;
                    }
                }
                position++;
                if (position == 11) {
                    position = 0;
                }
            } while (position != *cursor);
        }
        func_80071B94(0);
    } else {
        func_80072324();
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_800723E0);
#endif

/* Rebuild the alive mask: knocked-out slots lose their HP (gear +0x104) and
 * leave the turn order unless held by 800c3608; set the outcome when a side
 * is defeated; when every alive slot waits (+0x80 bit 0x1000), release the
 * first. */
#ifdef NON_MATCHING
void func_8007252C(void) {
    s32 slot;
    u16 waiting;

    D_800D39DC = 0;
    for (slot = 0; slot < 3; slot++) {
        if (D_800D2DCC.present[slot] != 0) {
            if (D_800CCCE8.records[slot].pilot.status7C & 0xC000) {
                if (D_800CCCE8.records[slot].pilot.status7C & 0x8000) {
                    D_800CCCE8.records[slot].pilot.hp = 0;
                }
                D_800D2DCC.ready[slot] = 0xFF;
            } else if (D_800C3EB4[slot].hidden == 0) {
                D_800D39DC |= func_80089C08(slot);
            }
        }
    }
    for (slot = 3; slot < 11; slot++) {
        if (D_800D2DCC.present[slot] != 0) {
            if (D_800C3EB4[slot].gear == 0) {
                if (D_800CCCE8.records[slot].pilot.status7C & 0xC000) {
                    if (!func_80089C9C(D_800C3608, slot)) {
                        D_800CCCE8.records[slot].pilot.hp = 0;
                        D_800D2DCC.ready[slot] = 0xFF;
                        continue;
                    }
                    D_800D39DC |= func_80089C08(slot);
                    continue;
                }
            } else if (D_800CCCE8.records[slot].gear.status7C & 0xC000) {
                if (!func_80089C9C(D_800C3608, slot)) {
                    D_800CCCE8.records[slot].gear.hp = 0;
                    D_800D2DCC.ready[slot] = 0xFF;
                    continue;
                }
                D_800D39DC |= func_80089C08(slot);
                continue;
            }
            if (D_800C3EB4[slot].hidden == 0 && D_800C3D18[slot - 3].unk3 == 0) {
                D_800D39DC |= func_80089C08(slot);
            }
        }
    }
    if (!(D_800D39DC & 0x7F8)) {
        D_800C48EA = 1;
    }
    if (!(D_800D39DC & 7)) {
        D_800C48EA = 0x81;
    }
    if (D_800C48EA == 0) {
        waiting = D_800D39DC;
        for (slot = 0; slot < 11; slot++) {
            if (func_80089C9C(waiting, slot) && (D_800CCCE8.records[slot].pilot.status80 & 0x1000)) {
                waiting &= func_80089C48(slot);
            }
        }
        if (waiting == 0) {
            for (slot = 0; slot < 11; slot++) {
                if (func_80089C9C(D_800D39DC, slot) && (D_800CCCE8.records[slot].pilot.status80 & 0x1000)) {
                    D_800CCCE8.records[slot].pilot.status80 &= 0xEFFF;
                    return;
                }
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_8007252C);
#endif

/* Add every other primitive from `first` to the ordering table. */
void func_800728B8(POLY_FT4 *prims, s32 count, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(D_800CCB04.ot + 1, &prims[first + i * 2]);
    }
}

/* Tint the current buffer's primitives from `first` to `last`: yellow for
 * mode 1, red otherwise. */
#ifdef NON_MATCHING
void func_80072938(POLY_FT4 *prims, s32 first, s32 last, u8 mode) {
    s32 i;

    for (i = first; i < last; i++) {
        SetShadeTex(&prims[i * 2 + D_800CCB04.buffer], 0);
        if (mode != 1) {
            prims[i * 2 + D_800CCB04.buffer].r0 = 0x80;
            prims[i * 2 + D_800CCB04.buffer].g0 = 0;
        } else {
            prims[i * 2 + D_800CCB04.buffer].r0 = 0x80;
            prims[i * 2 + D_800CCB04.buffer].g0 = 0x80;
        }
        prims[i * 2 + D_800CCB04.buffer].b0 = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80072938);
#endif

/* Build the member's panel HP glyphs: the current value's digits (800c3e08),
 * a '/', and the maximum's digits (800d2d54 from index 4), and tint them by
 * `mode`. */
void func_80072A9C(member, mode)
s32 member;
u8 mode;
{
    s32 i;
    s32 column;
    s32 first;

    first = D_800D2D28->unkE0[member];
    for (i = 0; i < 3; i++) {
        if (D_800C3E08[i] != 0xFF) {
            D_800D2D28->unkE0[member] +=
                func_80076A10(D_800C3E08[i] + 0x67, &D_800C3EA4->unk3A88[member][D_800D2D28->unkE0[member] * 2],
                              D_800C3068[member * 24 + i] + D_800C3254[D_800D3280 * 3 + member], 0x10);
        }
    }
    D_800D2D28->unkE0[member] +=
        func_80076A10(0x71, &D_800C3EA4->unk3A88[member][D_800D2D28->unkE0[member] * 2],
                      D_800C3068[member * 24 + 3] + D_800C3254[D_800D3280 * 3 + member], 0x10);
    for (i = 4, column = 4; i < 7; i++) {
        if (D_800D2D54[i] != 0xFF) {
            D_800D2D28->unkE0[member] +=
                func_80076A10(D_800D2D54[i] + 0x67, &D_800C3EA4->unk3A88[member][D_800D2D28->unkE0[member] * 2],
                              D_800C3068[member * 24 + column] + D_800C3254[D_800D3280 * 3 + member], 0x10);
            column++;
        }
    }
    if (mode != 0) {
        func_80072938(D_800C3EA4->unk3A88[member], first, D_800D2D28->unkE0[member], mode);
    }
}

/* Build the member's panel name glyphs (800d2d88) and tint them by `mode`. */
void func_80072DA8(member, mode)
s32 member;
u8 mode;
{
    s32 i;
    s32 first;

    i = 0;
    first = D_800D2D28->unkE0[member];
    for (; i < 5; i++) {
        if (D_800D2D88[i] != 0xFF) {
            D_800D2D28->unkE0[member] +=
                func_80076A10(D_800D2D88[i] + 0x67, &D_800C3EA4->unk3A88[member][D_800D2D28->unkE0[member] * 2],
                              D_800C3076[member * 24 + i] + D_800C3254[D_800D3280 * 3 + member], 0x10);
        }
    }
    if (mode != 0) {
        func_80072938(D_800C3EA4->unk3A88[member], first, D_800D2D28->unkE0[member], mode);
    }
}

/* Split the member's panel values into digit glyph codes: HP (3 digits) and
 * maximum HP (3), or in a gear its HP (5); leading zeros become blanks (0xff).
 * Returns the warning level: 2 at an eighth of the maximum or less, 1 at a
 * quarter or less, else 0. */
#ifdef NON_MATCHING
u8 func_80072F38(s32 member, u8 inGear) {
    u8 warning;
    u8 *digit;
    s16 hp100;
    s16 hp10;
    s16 max100;
    s16 max10;
    s32 gear10000;
    s32 gear1000;
    s32 gear100;
    s32 gear10;

    D_800D2FE0 = D_800D3330 = D_800CCCE8.records[member].pilot.maxHp;
    D_800D2D38 = D_800D2E58 = D_800CCCE8.records[member].pilot.hp;
    D_800D3018 = D_800D333C = D_800CCCE8.records[member].gear.hp;
    D_800D3668 = D_800CCCE8.records[member].gear.maxHp;
    warning = 0;
    if (inGear) {
        if (D_800D3668 / 8 >= D_800D333C) {
            warning = 2;
        } else if (D_800D3668 / 4 >= D_800D333C) {
            warning = 1;
        }
    } else {
        if (D_800D3330 / 8 >= D_800D2E58) {
            warning = 2;
        } else if (D_800D3330 / 4 >= D_800D2E58) {
            warning = 1;
        }
    }
    hp100 = D_800D2D38 / 100;
    D_800D2D38 %= 100;
    hp10 = D_800D2D38 / 10;
    D_800D2D38 %= 10;
    max100 = D_800D2FE0 / 100;
    D_800D2FE0 %= 100;
    max10 = D_800D2FE0 / 10;
    D_800D2FE0 %= 10;
    gear10000 = D_800D3018 / 10000;
    D_800D3018 -= gear10000 * 10000;
    gear1000 = D_800D3018 / 1000;
    D_800D3018 -= gear1000 * 1000;
    gear100 = D_800D3018 / 100;
    D_800D3018 -= gear100 * 100;
    gear10 = D_800D3018 / 10;
    D_800D3018 -= gear10 * 10;
    D_800C3E08[0] = hp100;
    D_800C3E08[1] = hp10;
    D_800D2D54[4] = max100;
    D_800D2D54[5] = max10;
    D_800D2D88[0] = gear10000;
    D_800D2D88[1] = gear1000;
    D_800D2D88[2] = gear100;
    D_800D2D88[3] = gear10;
    D_800C3E08[2] = D_800D2D38;
    D_800D2D54[6] = D_800D2FE0;
    D_800D2D88[4] = D_800D3018;
    for (digit = D_800C3E08; digit < D_800C3E08 + 2; digit++) {
        if (*digit != 0) {
            break;
        }
        *digit = 0xFF;
    }
    for (digit = &D_800D2D54[4]; digit < &D_800D2D54[6]; digit++) {
        if (*digit != 0) {
            break;
        }
        *digit = 0xFF;
    }
    for (digit = D_800D2D88; digit < D_800D2D88 + 4; digit++) {
        if (*digit != 0) {
            break;
        }
        *digit = 0xFF;
    }
    return warning;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80072F38);
#endif

/* Build the member's four-digit panel value (the 800d32a0 value or record
 * +0xdc) as glyphs. */
#ifdef NON_MATCHING
void func_80073380(s32 member) {
    s16 value;
    s32 i;
    s32 x;
    u8 digit;

    if (D_800C3EA4->panels[member].unk1E1 == 1) {
        value = D_800D32A0[member].unk0;
    } else {
        value = D_800CCCE8.records[member].gear.fuel;
    }
    func_8008AAA0(value);
    i = 0;
    x = 0;
    for (; i < 4; i++) {
        digit = D_800C3CF4[i + 5];
        if (digit != 0xFF) {
            D_800D2D28->unkEC[member] +=
                func_80076A10(digit + 0x83, &D_800C3EA4->unk6008[member][D_800D2D28->unkEC[member] * 2],
                              member * 0x60 + (D_800C3254[D_800D3280 * 3 + member] + 0x4A) + x, 0x25);
        }
        x += 6;
    }
    D_800D2D28->unk99[member] = D_800CCB04.buffer;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80073380);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80073538);

/* When enabled, shade the current flat quad at graphics +0x63c8 grey by
 * +0x6410 and add it with its draw mode to the ordering table. */
#ifdef NON_MATCHING
void func_80073A58(void) {
    if (D_800C3EA4->unk6415 != 0) {
        D_800C3EA4->unk63C8[D_800C3EA4->unk6414].r0 = D_800C3EA4->unk6410;
        D_800C3EA4->unk63C8[D_800C3EA4->unk6414].g0 = D_800C3EA4->unk6410;
        D_800C3EA4->unk63C8[D_800C3EA4->unk6414].b0 = D_800C3EA4->unk6410;
        AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unk63C8[D_800C3EA4->unk6414]);
        AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unk63F8[D_800C3EA4->unk6414]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80073A58);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80073B64);

/* Draw both 100-primitive lists at graphics +0x641c when UI +0xcb is set. */
void func_80073E88(void) {
    s32 i;

    if (D_800D2D28->unkCB != 0) {
        for (i = 0; i < 2; i++) {
            func_800728B8(D_800C3EA4->unk641C[i], D_800D2D28->unkD0[i], D_800D2D28->unkA3);
        }
    }
}

/* Draw the three primitive lists the UI enables at +0x9c..+0x9e. */
void func_80073F08(void) {
    if (D_800D2D28->unk9C != 0) {
        func_800728B8(D_800C3EA4->unkBA8, D_800D2D28->unkF8, D_800D2D28->unkA5);
    }
    if (D_800D2D28->unk9E != 0) {
        func_800728B8(D_800C3EA4->unk27C8, D_800D2D28->unk100, D_800D2D28->unkA7);
    }
    if (D_800D2D28->unk9D != 0) {
        func_800728B8(D_800C3EA4->unk1E68, D_800D2D28->unkFC, D_800D2D28->unkA6);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80073FB8);

/* Redraw the party markers flagged in UI +0x7c. */
void func_800742A0(void) {
    s32 member;
    u8 value;

    for (member = 0; member < 3; member++) {
        if (D_800D2D28->reaction[member] != 0) {
            D_800D2D28->unkE0[member] = 0;
            D_800D2D28->unkEC[member] = 0;
            if (D_800C3EB4[member].field2 != 0x7F) {
                func_80073380(member);
                value = func_80072F38(member, D_800D32A0[member].unk1);
                if (D_800D32A0[member].unk1 != 0) {
                    func_80072DA8(member, value);
                } else {
                    func_80072A9C(member, value);
                }
                D_800D2D28->unk93[member] = D_800CCB04.buffer;
                D_800D2D28->reaction[member] = 0;
            }
        }
    }
}

/* When enabled, add the graphics block's four current quads to the ordering
 * table. */
void func_800743A4(void) {
    if (D_800C3EA4->unkA230->unk669 != 0) {
        AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk0[D_800C3EA4->unkA230->unk668]);
        AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk50[D_800C3EA4->unkA230->unk668]);
        AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unkA0[D_800C3EA4->unkA230->unk668]);
        AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unkF0[D_800C3EA4->unkA230->unk668]);
    }
}

/* Add the graphics block's current +0x320 and +0x370 quads to the ordering
 * table. */
void func_800744BC(void) {
    AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk320[D_800C3EA4->unkA230->buffer]);
    AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk370[D_800C3EA4->unkA230->buffer]);
}

/* Add the graphics block's current +0x280 and +0x2d0 quads to the ordering
 * table. */
void func_80074554(void) {
    AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk280[D_800C3EA4->unkA230->buffer]);
    AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk2D0[D_800C3EA4->unkA230->buffer]);
}

/* Add the command window page's quads to the ordering table: the page
 * frame (800743a4), then per page (800d2d28 +0xb7) its EP digits, icons,
 * title and cost digits, or its gradient box. */
void func_800745EC(void) {
    switch (D_800D2D28->unkB7) {
    case 1:
        func_800743A4();
        if (D_800C3EA4->unkA230->unk669) {
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk410[D_800C3EA4->unkA230->unk66C]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk460[D_800C3EA4->unkA230->unk66C]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk4B0[D_800C3EA4->unkA230->unk66C]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk500[D_800C3EA4->unkA230->unk66C]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk550[D_800C3EA4->unkA230->unk66C]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk5A0[D_800C3EA4->unkA230->unk66C]);
        }
        if (D_800C3EA4->unkA230->unk66B) {
            if (D_800C3EA4->unkA230->unk66E) {
                AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk3C0[D_800C3EA4->unkA230->unk66C]);
            }
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk140[D_800C3EA4->unkA230->buffer]);
            func_800728B8(D_800C3EA4->unkA230->unk190, D_800C3EA4->unkA230->unk66E, D_800C3EA4->unkA230->buffer);
            func_80074554();
            func_800744BC();
        }
        break;
    case 2:
        func_800743A4();
        if (D_800C3EA4->unkA230->unk66B) {
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk140[D_800C3EA4->unkA230->buffer]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk190[D_800C3EA4->unkA230->buffer]);
        }
        if (D_800C3EA4->unkA230->unk66D) {
            func_800744BC();
            func_80074554();
        }
        break;
    case 3:
        func_800743A4();
        if (D_800C3EA4->unkA230->unk66F) {
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk5F0[D_800C3EA4->unkA230->unk66C]);
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk638[D_800C3EA4->unkA230->unk66C]);
        }
        break;
    case 4:
        func_800743A4();
        if (D_800C3EA4->unkA230->unk66B) {
            if (D_800C3EA4->unkA230->unk66E) {
                AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk3C0[D_800C3EA4->unkA230->unk66C]);
            }
            AddPrim(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk140[D_800C3EA4->unkA230->buffer]);
            func_80074554();
            func_800744BC();
        }
        break;
    case 5:
        func_800743A4();
        break;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80074AB8);

/* While a target is being chosen, pulse the direction arrows (red between
 * 0x40 and 0xfc) and draw the ones pointing at targets. The block is
 * addressed as its leading primitive array. */
void func_80074D4C(void) {
    s32 i;

    if (D_800D2D28->unkC6 != 0) {
        if (D_800C3E24->fading) {
            D_800C3E24->shade -= 4;
            if (D_800C3E24->shade < 0x40) {
                D_800C3E24->fading = 0;
                D_800C3E24->shade = 0x40;
            }
        } else {
            D_800C3E24->shade += 4;
            if (D_800C3E24->shade >= 0x100) {
                D_800C3E24->fading = 1;
                D_800C3E24->shade = 0xFC;
            }
        }
        for (i = 0; i < 4; i++) {
            if (D_800C3E24->arrows[i]) {
                ((POLY_G3 *)D_800C3E24)[i * 2 + D_800C3E24->buffer].r0 = D_800C3E24->shade;
                ((POLY_G3 *)D_800C3E24)[i * 2 + D_800C3E24->buffer].g0 = 0;
                ((POLY_G3 *)D_800C3E24)[i * 2 + D_800C3E24->buffer].b0 = 0;
                AddPrim(D_800CCB04.ot + 1, &((POLY_G3 *)D_800C3E24)[i * 2 + D_800C3E24->buffer]);
            }
        }
    }
}

/* Draw the three primitive lists of *800d2db4 when UI +0xa8 is set. */
void func_80074EEC(void) {
    if (D_800D2D28->unkA8 != 0) {
        func_800728B8(D_800D2DB4->unk5550, D_800D2DB4->counts[11], D_800D2DB4->buffers[11]);
        func_800728B8(D_800D2DB4->unk5640, D_800D2DB4->counts[12], D_800D2DB4->buffers[12]);
        func_800728B8(D_800D2DB4->unk5C80, D_800D2DB4->counts[13], D_800D2DB4->buffers[13]);
    }
}

/* Add the current *800d3278 quad (UI +0xc8) and draw the 800d2dac object
 * (UI +0xc9). */
void func_80074F70(void) {
    if (D_800D2D28->unkC8 != 0) {
        AddPrim(D_800CCB04.ot + 1, &D_800D3278->unk7A4[D_800D3278->unk7F4]);
    }
    if (D_800D2D28->unkC9 != 0) {
        func_80034888(D_800D2DAC, D_800CCB04.ot + 1, D_800CCB04.buffer);
    }
}

/* Place and add the shown battle messages: the first centred at (0x40,
 * 0x2c), the others centred at (0x9a, 0xca), texture rows 13 apart per
 * pair. */
#ifdef NON_MATCHING
void func_8007500C(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800D36C8[i].shown != 0) {
            if (i == 0) {
                func_80076C78(&D_800D36C8[0].prims[D_800CCB04.buffer], 0x40 - (D_800D36C8[0].width >> 1), 0x2C, 0, 0,
                              D_800D36C8[0].width);
            } else {
                func_80076C78(&D_800D36C8[i].prims[D_800CCB04.buffer], 0x9A - (D_800D36C8[i].width >> 1), 0xCA, 0,
                              (i / 2) * 13, D_800D36C8[i].width);
            }
            AddPrim(D_800CCB04.ot + 1, &D_800D36C8[i].prims[D_800CCB04.buffer]);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_8007500C);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80075168);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80075938);

/* Battle end, outcome state 1: unless 800c492a is set, release every battle
 * resource. */
void func_80076418(void) {
    if (D_800C492A == 0) {
        func_8008FAD8();
        func_800742A0();
        func_80075938();
        func_80073538();
        func_80073A58();
        func_80073B64();
        func_80073E88();
        func_80073F08();
        func_8007500C();
        func_80074EEC();
        func_800745EC();
        func_80074D4C();
        func_80073FB8();
        func_80088B80();
        func_80074AB8();
    }
}

/* Battle end, outcome state 0: leave the result screens (8008fad8), run the
 * post-battle module's exit and release the battle display. */
void func_800764B4(void) {
    D_8005959C = 0;
    func_8008FAD8();
    func_801DE594();
    func_80073FB8();
}

/* Battle end, outcome state 2: release the battle's resources. */
void func_800764EC(void) {
    func_8008FAD8();
    func_80073538();
    func_80073F08();
    func_8007500C();
    func_80074F70();
    func_80073FB8();
    func_80088B80();
    func_80074AB8();
}

/* Battle end by outcome state 800c3e4c. */
void func_80076544(void) {
    switch (D_800C3E4C) {
    case 0:
        func_800764B4();
        break;
    case 1:
        func_80076418();
        break;
    case 2:
        func_800764EC();
        break;
    }
}

/* Start the panel cursor window's opening over the member's panel (wider
 * for a member with the 800d32a0 flag unless it is character 7). */
#ifdef NON_MATCHING
void func_800765C4(s32 member) {
    u16 *panelX = &D_800C3254[D_800D3280 * 3 + member];

    D_800D2D28->unk44 = 0x1C;
    D_800D2D28->unk34 = member * 0x60 + (*panelX + 0x48);
    if (D_800D32A0[member].unk1 != 0 && D_800D2D24[member] != 7) {
        D_800D2D28->unk44 = 0x24;
        D_800D2D28->unk34 = member * 0x60 + (*panelX + 0x44);
    }
    D_800D2D28->unk3C = 0x10;
    D_800D2D28->unk4C = 0x98;
    D_800D2D28->unk54 = D_800D2D28->unk34 - (D_800D2D28->unk3C + 5);
    D_800D2D28->unk5C = D_800D2D28->unk4C - (D_800D2D28->unk44 + 5);
    D_800D2D28->unk54 = (D_800D2D28->unk54 << 8) / D_800D2D28->unk5C;
    D_800D2D28->unk104 = 0x800;
    D_800D2D28->unkA9 = 6;
    D_800D2D28->unk5C = 0x100;
    D_800D2D28->unk64 = 0;
    D_800D2D28->unk6C = 0;
    D_800D2D28->unk106 = 0;
    D_800D2D28->unkAB = 1;
    D_800D2D28->unk90[member]--;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_800765C4);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80076710);

/* Upload an image and wait for the transfer. */
void func_800769E8(RECT *rect, u32 *pixels) {
    LoadImage(rect, pixels);
    DrawSync(0);
}

/* Build glyph `id` as primitives at `prims`, full scale. */
s32 func_80076A10(s32 id, POLY_FT4 *prims, s16 x, s16 y) {
    return func_8002675C(D_800D2F5C, id, prims, D_800CCB04.buffer, x, y, 0x1000);
}

/* Build glyph `id` as primitives at `prims`, half scale. */
s32 func_80076A6C(s32 id, POLY_FT4 *prims, s16 x, s16 y) {
    return func_8002675C(D_800D2F5C, id, prims, D_800CCB04.buffer, x, y, 0x800);
}

/* Initialise a textured quad: raw texture, opaque. */
void func_80076AC8(POLY_FT4 *prim) {
    SetSemiTrans(prim, 1);
    SetShadeTex(prim, 0);
}

/* Initialise a textured quad at full brightness; tpage bit 0x40 follows
 * 800595a0. */
void func_80076B00(POLY_FT4 *prim) {
    func_80076AC8(prim);
    prim->r0 = 0x80;
    prim->g0 = 0x80;
    prim->b0 = 0x80;
    if (D_800595A0 != 0) {
        prim->tpage |= 0x40;
    } else {
        prim->tpage &= ~0x40;
    }
}

/* Initialise a textured quad at full brightness with tpage bit 0x20. */
void func_80076B68(POLY_FT4 *prim) {
    func_80076AC8(prim);
    prim->r0 = 0x80;
    prim->g0 = 0x80;
    prim->b0 = 0x80;
    prim->tpage |= 0x20;
}

/* Initialise a textured quad at half brightness with tpage bit 0x20. */
void func_80076BAC(POLY_FT4 *prim) {
    func_80076AC8(prim);
    prim->r0 = 0x40;
    prim->g0 = 0x40;
    prim->b0 = 0x40;
    prim->tpage |= 0x20;
}

/* Initialise a textured quad at full brightness with tpage bit 0x40. */
void func_80076BF0(POLY_FT4 *prim) {
    func_80076AC8(prim);
    prim->r0 = 0x80;
    prim->g0 = 0x80;
    prim->b0 = 0x80;
    prim->tpage |= 0x40;
}

/* Initialise a textured quad at half brightness with tpage bit 0x40. */
void func_80076C34(POLY_FT4 *prim) {
    func_80076AC8(prim);
    prim->r0 = 0x40;
    prim->g0 = 0x40;
    prim->b0 = 0x40;
    prim->tpage |= 0x40;
}

/* Place a quad of width `w` and height 13 at (x, y) with texture (u, v). */
void func_80076C78(POLY_FT4 *prim, u16 x, u16 y, u8 u, u8 v, u8 w) {
    prim->x0 = x;
    prim->y0 = y;
    prim->y1 = y;
    prim->x2 = x;
    prim->y2 = y + 13;
    prim->y3 = y + 13;
    prim->u0 = u;
    prim->u2 = u;
    prim->x1 = x + w;
    prim->x3 = x + w;
    prim->v0 = v;
    prim->u1 = u + w;
    prim->v1 = v;
    prim->v2 = v + 13;
    prim->u3 = u + w;
    prim->v3 = v + 13;
}

/* Place a quad of `w` x `h` at (x, y) with texture (u, v). */
void func_80076CE8(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
    prim->x0 = x;
    prim->y0 = y;
    prim->y1 = y;
    prim->x2 = x;
    prim->u0 = u;
    prim->u2 = u;
    prim->x1 = x + w;
    prim->y2 = y + h;
    prim->x3 = x + w;
    prim->y3 = y + h;
    prim->v0 = v;
    prim->u1 = u + w;
    prim->v1 = v;
    prim->v2 = v + h;
    prim->u3 = u + w;
    prim->v3 = v + h;
}

/* Initialise a quad pair: semi-transparent, raw texture, texture page by
 * `page` (0/1 at x 0x380, 2/3 at 0x3c0; odd pages at y 0x100) and the CLUT
 * chosen by `alternate`. */
#ifdef NON_MATCHING
void func_80076D58(POLY_FT4 *prims, u8 alternate, u8 page) {
    s32 i;

    for (i = 0; i < 2; i++) {
        SetPolyFT4(&prims[i]);
        prims[i].r0 = 0x80;
        prims[i].g0 = 0x80;
        prims[i].b0 = 0x80;
        SetSemiTrans(&prims[i], 0);
        SetShadeTex(&prims[i], 1);
        switch (page) {
        case 0:
            prims[i].tpage = GetTPage(0, 0, 0x380, 0);
            break;
        case 1:
            prims[i].tpage = GetTPage(0, 0, 0x380, 0x100);
            break;
        case 2:
            prims[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
            break;
        case 3:
            prims[i].tpage = GetTPage(0, 0, 0x3C0, 0);
            break;
        }
        if (alternate != 0) {
            prims[i].clut = D_80059414;
        } else {
            prims[i].clut = D_800595D4;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80076D58);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80076EA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80077074);

/* Initialise four quads, white and semi-transparent, with the texture page
 * and CLUT of graphics texture entry `index`. */
void func_80077364(POLY_FT4 *prims, u8 index) {
    s32 i;

    for (i = 0; i < 4; i++) {
        SetPolyFT4(&prims[i]);
        SetShadeTex(&prims[i], 1);
        prims[i].r0 = 0xFF;
        prims[i].g0 = 0xFF;
        prims[i].b0 = 0xFF;
        prims[i].tpage = GetTPage(D_800C3EA4->textures[index].mode, 0, D_800C3EA4->textures[index].x,
                                       D_800C3EA4->textures[index].y);
        prims[i].clut = GetClut(D_800C3EA4->textures[index].clutX, D_800C3EA4->textures[index].clutY);
    }
}

/* Initialise a window's primitives: its semi-transparent background in the
 * window colour (one per draw buffer, with its draw mode) and its four edge
 * piece sets (textures 1-4). The draw mode's texture window argument is the
 * UI block pointer. */
void func_80077454(u8 window) {
    WindowBlock *block;
    u8 i;

    block = D_800D2E38[window];
    for (i = 0; i < 2; i++) {
        SetPolyG4(&block->shade[i]);
        (block->shade + i)->r0 = D_800594D4[0];
        (block->shade + i)->g0 = D_800594D4[1];
        (block->shade + i)->b0 = D_800594D4[2];
        (block->shade + i)->r1 = D_800594D4[0];
        (block->shade + i)->g1 = D_800594D4[1];
        (block->shade + i)->b1 = D_800594D4[2];
        (block->shade + i)->r2 = D_800594D4[0];
        (block->shade + i)->g2 = D_800594D4[1];
        (block->shade + i)->b2 = D_800594D4[2];
        (block->shade + i)->r3 = D_800594D4[0];
        (block->shade + i)->g3 = D_800594D4[1];
        (block->shade + i)->b3 = D_800594D4[2];
        SetSemiTrans(&block->shade[i], 1);
        SetDrawMode(&block->mode[i], 0, 0,
                    GetTPage(0, D_800595A0, D_800C3EA4->textures[1].x, D_800C3EA4->textures[1].y),
                    (RECT *)D_800D2D28);
    }
    func_80077364(block->frame[0], 1);
    func_80077364(block->frame[1], 2);
    func_80077364(block->frame[2], 3);
    func_80077364(block->frame[3], 4);
}

/* Allocate and clear the 0x670-byte graphics block, then initialise it. */
void func_80077610(void) {
    GraphicsBlock *block = (GraphicsBlock *)func_8008ABB8(0x670, 0);

    D_800C3EA4->unkA230 = block;
    bzero(block, 0x670);
    func_80077074();
}

/* Wait a frame, then release the graphics block. */
void func_8007765C(void) {
    func_800716D8();
    func_800320E8(D_800C3EA4->unkA230);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80077698);

/* Clear UI byte +0xc6. */
void func_80077980(void) {
    D_800D2D28->unkC6 = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80077990);

/* Initialise a battle message's quad pair for texture row `row` (13 pixels
 * per pair of rows; odd rows use the alternate CLUT) and hide it. */
void func_800780A8(BattleMessage *message, u32 row) {
    s32 i;

    for (i = 0; i < 2; i++) {
        SetPolyFT4(&message->prims[i]);
        message->prims[i].r0 = 0x80;
        message->prims[i].g0 = 0x80;
        message->prims[i].b0 = 0x80;
        SetSemiTrans(&message->prims[i], 0);
        SetShadeTex(&message->prims[i], 1);
        message->alternate = row & 1;
        message->prims[i].clut = message->alternate ? D_80059414 : D_800595D4;
        message->prims[i].tpage = GetTPage(0, 0, 0x3C0, (s32)row / 2 * 13);
    }
    message->shown = 0;
}

/* Set up windows 5 and 4 (closed) and the eight battle messages: each pair
 * shares a text image block and a VRAM rectangle row. */
#ifdef NON_MATCHING
void func_8007819C(void) {
    s32 i;

    func_8008F8F4(5, 8, 0x2A, 0x70, 0x12, 0, 0);
    D_800D2D28->windows[5] = 0;
    func_8008F8F4(4, 0x20, 0xC8, 0xF4, 0x12, 0, 0);
    D_800D2D28->windows[4] = 0;
    for (i = 0; i < 8; i += 2) {
        D_800D36C8[i].pixels = (u32 *)func_8008AC00(0x39);
        D_800D36C8[i + 1].pixels = D_800D36C8[i].pixels;
        D_800D36C8[i].rect.x = 0x3C0;
        D_800D36C8[i].rect.y = (i / 2) * 13;
        D_800D36C8[i].rect.w = 0x3C;
        D_800D36C8[i].rect.h = 13;
        D_800D36C8[i + 1].rect = D_800D36C8[i].rect;
        func_800780A8(&D_800D36C8[i], i);
        func_800780A8(&D_800D36C8[i + 1], i + 1);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_8007819C);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80078310);

/* Reset every slot's turn timers from its speed (unused slots 0xff), its
 * ready flag and slow alternation, and clear the order buffer. */
void func_80078508(u8 *order) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (D_800D2DCC.present[slot] != 0) {
            D_800D2DCC.timers[0][slot] = D_800D2DCC.timers[1][slot] = func_80098AF8(slot, 0);
        } else {
            D_800D2DCC.timers[0][slot] = D_800D2DCC.timers[1][slot] = 0xFF;
        }
        D_800D2DCC.ready[slot] = 0;
        D_800D2DCC.timers[2][slot] = 0;
        order[slot] = 0;
    }
}

/* Close the current event for `actor` with the action entry's parameter and
 * advance the event count. */
#ifdef NON_MATCHING
void func_800785D4(u8 actor, u8 index) {
    D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
    D_800C3FE8[D_800C3EAC->eventCount].parameter = D_800D2E5C[index].param | (D_800D2E5C[index].unk5 << 8);
    D_800C3EAC->eventCount++;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_800785D4);
#endif

/* Show the name of action `index` in the next battle message (up to eight)
 * and queue its event (0xfa) for `actor`. */
void func_80078658(u8 index, u8 actor) {
    if (D_800C3E8C < 9) {
        D_800D36C8[D_800C3E8C].width =
            func_80034EAC(func_80033728(D_800C3DDC, D_800D2E5C[index].named), D_800D36C8[D_800C3E8C].pixels, 0x39,
                          D_800C3E8C & 1);
        func_800769E8(&D_800D36C8[D_800C3E8C].rect, D_800D36C8[D_800C3E8C].pixels);
        D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0xFA;
        D_800C3FE8[D_800C3EAC->eventCount].parameter = D_800C3E8C++;
        D_800C3EAC->eventCount++;
    }
}

/* Queue event type 0xf7 for `actor` with parameter `value`. */
void func_800787E0(u8 value, u8 actor) {
    D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xF7;
    D_800C3FE8[D_800C3EAC->eventCount].parameter = value;
    D_800C3EAC->eventCount++;
}

/* Queue event type 0xf8 for `actor` with the pending message 800c3e8c - 1. */
void func_8007887C(u8 actor) {
    if (D_800C3E8C != 0) {
        D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0xF8;
        D_800C3FE8[D_800C3EAC->eventCount].parameter = D_800C3E8C - 1;
        D_800C3EAC->eventCount++;
    }
}

/* For a named action entry: its name text (80078658), event 0xf7 with 0x1e
 * and the pending message event. */
void func_8007893C(u8 index, u8 actor) {
    if (D_800D2E5C[index].named != 0) {
        func_80078658(index, actor);
        func_800787E0(0x1E, actor);
        func_8007887C(actor);
    }
}

/* Execute the actor's action `index`: commit it, queue its animation event
 * with the committed targets and, when those differ from the action's own,
 * retarget the queued move event (0xfd). */
void func_80078998(u8 actor, u8 index) {
    s32 i;

    func_8007893C(index, actor);
    D_800C3EAC->unk2DC = D_800D2E5C[index].arg1 + 1;
    func_80085CCC(actor, D_800D2E5C[index].targets, D_800D2E5C[index].animation);
    func_80085C88(D_800C3EAC->eventCount);
    D_800C3FE8[D_800C3EAC->eventCount].type = D_800D2E5C[index].animation;
    D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2C94.targets;
    func_800785D4(actor, index);
    if (D_800D2E5C[index].targets != D_800D2C94.targets) {
        for (i = 0; i < D_800C3EAC->eventCount; i++) {
            if (D_800C3FE8[i].type == 0xFD) {
                D_800C3FE8[i].targetMask = D_800D2C94.targets;
                return;
            }
        }
    }
}

/* Queue a move event (0xfd) for the actor's action `index` toward target:
 * compute the move, then (unless the action's parameter is 1) choose the
 * on-foot or gear approach, and redraw the slots involved. */
void func_80078B34(u8 actor, u8 index, u8 target) {
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xFD;
    D_800C3FE8[D_800C3EAC->eventCount].parameter = 0;
    D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2E5C[index].targets;
    func_800877E0(actor, target);
    if (D_800D2E5C[index].param != 1) {
        if (D_800D32A0[actor].unk1 == 0) {
            func_80087EDC(actor, target);
        } else {
            func_800881B8(actor, target);
        }
    }
    func_800BC404(D_800D2E5C[index].targets | func_80089C08(actor));
    func_800785D4(actor, index);
}

/* Queue event type 0xfc for the actor. */
void func_80078C9C(u8 actor, u8 index) {
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xFC;
    func_800785D4(actor, index);
}

/* Queue the action entry's parameter as the event type for the actor. */
void func_80078CEC(u8 actor, u8 index) {
    D_800C3FE8[D_800C3EAC->eventCount].type = D_800D2E5C[index].param;
    func_800785D4(actor, index);
}

/* The action entry's targets act together. */
void func_80078D48(u8 actor, u8 index) {
    D_800D39E0 = D_800D2E5C[index].targets;
}

/* Action entry type: the actor leaves the battle (event 0xf9); its reaction
 * reaction byte +3 is set and only its 0x8000 flag is kept. */
void func_80078D6C(u8 actor, u8 index) {
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xF9;
    func_800785D4(actor, index);
    D_800D2DCC.ready[actor] = 0xFF;
    D_800C3D18[actor - 3].unk3 = 1;
    D_800CCCE8.records[actor].pilot.status7C &= 0x8000;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_80070E2C", func_80078E24);

/* Set the actor's attribute arg1 to the entry's parameter byte. */
void func_80079054(u8 actor, u8 index) {
    func_80079ED8(actor, D_800D2E5C[index].arg1, D_800D2E5C[index].param, 0);
}

/* Add the entry's parameter byte to the actor's attribute arg1. */
void func_80079098(u8 actor, u8 index) {
    func_80079ED8(actor, D_800D2E5C[index].arg1,
                  D_800D2E5C[index].param + func_80079ED8(actor, D_800D2E5C[index].arg1, 0, 1), 0);
}

/* Set the actor's 16-bit attribute arg1 to the entry's parameter halfword. */
void func_80079114(u8 actor, u8 index) {
    func_8007A280(actor, D_800D2E5C[index].arg1, D_800D2E5C[index].param | (D_800D2E5C[index].unk5 << 8), 0);
}

/* Add the entry's parameter halfword to the actor's 16-bit attribute arg1. */
void func_8007916C(u8 actor, u8 index) {
    func_8007A280(actor, D_800D2E5C[index].arg1,
                  D_800D2E5C[index].param +
                      (func_8007A280(actor, D_800D2E5C[index].arg1, 0, 1) + (D_800D2E5C[index].unk5 << 8)),
                  0);
}

/* Named-action text, then event 0xf4 for the actor. */
void func_800791FC(u8 actor, u8 index) {
    func_8007893C(index, actor);
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xF4;
    func_800785D4(actor, index);
}

/* Event 0xf6 for the actor with the entry's targets. */
void func_80079270(u8 actor, u8 index) {
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xF6;
    D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2E5C[index].targets;
    func_800785D4(actor, index);
}
