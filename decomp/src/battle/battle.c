#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"

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

    if (D_800D2CAF != 0 && (D_800D2C94 & D_800C48E8) == 0) {
        frames = 3;
        D_800D39B8.width = func_80034EAC(func_80033728(D_800D39F0, D_800D2CAF),
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
#ifdef NON_MATCHING
void func_80071A8C(void) {
    s32 i;

    for (i = 7; i >= 0; i--) {
        D_800D36C8[i].shown = 0;
    }
    D_800D2D28->windows[5] = 0;
    D_800D2D28->windows[4] = 0;
    func_800716D8();
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80071A8C);
#endif

/* Show the pending battle message (window 7) until a button is pressed or
 * 59 frames pass. */
void func_80071AE0(void) {
    s32 frames;

    if (D_800D2CAF != 0 && (D_800D2C94 & D_800C48E8) == 0) {
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80071B94);

/* Party members held by the mask 800d2c9e lose their ready flag, restart
 * their turn timer from its reload value and show the held marker. */
void func_80072270(void) {
    s32 member;

    if (D_800D2C9E & 7) {
        for (member = 0; member < 3; member++) {
            if (func_80089C9C(D_800D2C9E, member)) {
                D_800D2DCC.ready[member] = 0;
                D_800D2DCC.timers[1][member] = D_800D2DCC.timers[0][member];
                D_800D2D28->reaction[member] = 1;
            }
        }
    }
    D_800D2C9E = 0;
}

/* Give every enemy in the act-together mask its turn: clear the event types,
 * queue action 0x17 and run the turn procedure. The cleared 0x100-byte action
 * buffer pointer is never initialised in the original. */
#ifdef NON_MATCHING
void func_80072324(void) {
    s32 slot;
    s32 i;
    u8 *p;
    u8 *actions;

    for (slot = 3; slot < 11; slot++) {
        if (func_80089C9C(D_800D39E0, slot)) {
            p = actions;
            do {
                *p++ = 0;
            } while (p < actions + 0x100);
            for (i = 31; i >= 0; i--) {
                D_800C3FE8[i].type = 0xFF;
            }
            D_800D2E5C[0].type = 4;
            D_800D2E5C[0].param = 0x17;
            func_80085AC4(slot);
            func_80071B94(1);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072324);
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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800723E0);
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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007252C);
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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072938);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072A9C);

/* Build the member's panel name glyphs (800d2d88) and tint them by `mode`. */
#ifdef NON_MATCHING
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
                              D_800C3076[member][i] + D_800C3254[D_800D3280][member], 0x10);
        }
    }
    if (mode != 0) {
        func_80072938(D_800C3EA4->unk3A88[member], first, D_800D2D28->unkE0[member], mode);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072DA8);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072F38);

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
                              member * 0x60 + (D_800C3254[D_800D3280][member] + 0x4A) + x, 0x25);
        }
        x += 6;
    }
    D_800D2D28->unk99[member] = D_800CCB04.buffer;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073380);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073538);

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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073A58);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073B64);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073FB8);

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

INCLUDE_RODATA(".local/decomp/battle/asm/nonmatchings/battle", D_8006FAF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800745EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074AB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074D4C);

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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007500C);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80075168);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80075938);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800765C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076710);

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
void func_80076C78(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, u8 w) {
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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076D58);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076EA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077074);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077454);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077698);

/* Clear UI byte +0xc6. */
void func_80077980(void) {
    D_800D2D28->unkC6 = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077990);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007819C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078310);

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
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800785D4);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078658);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078B34);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078E24);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800792F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800793F0);

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
            D_800D3400[enemy].bytes[9 + i] = D_800D2CA4[i];
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
#ifdef NON_MATCHING
void func_800799C8(u8 slot) {
    u8 *pc;
    u8 count;
    u8 enemy;
    u8 *p;
    s32 i;

    count = 0;
    enemy = slot - 3;
    pc = D_800D3400[enemy].script;
    for (p = (u8 *)D_800D2E5C; p < (u8 *)D_800D2E5C + 0x100; p++) {
        *p = 0;
    }
    for (i = 31; i >= 0; i--) {
        D_800C3FE8[i].type = 0xFF;
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
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800799C8);
#endif

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079C24);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079ED8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A280);

/* Whether `slot` can be targeted: present, visible and not down (+0x7c
 * 0xc002); without `any` also not flagged 0x20 at +0x84. */
#ifdef NON_MATCHING
u8 func_8007A628(u8 slot, u8 any) {
    u8 result = 0;
    u16 status;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0 && !(D_800CCCE8.records[slot].pilot.status7C & 0xC002)) {
        result = 1;
        if (any == 0) {
            status = D_800CCCE8.records[slot].pilot.status84.half.active & 0x20;
            result = status == 0;
        }
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A628);
#endif

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

/* AI action default: queue an entry of type 0x80 carrying the four opcode
 * bytes. Returns the new entry count. */
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
#ifdef NON_MATCHING
void func_8007ABD8(u8 **pc, u8 enemy) {
    u16 *value = &D_800D3400[enemy].vars[(*pc)[1]];
    s32 sum = *value + (((*pc)[3] << 8) + (*pc)[2]);

    if (sum > 0xFFFF) {
        sum = 0xFFFF;
    }
    *value = sum;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ABD8);
#endif

/* AI action 11: variable b1 -= b2 | b3 << 8, saturating at 0. */
#ifdef NON_MATCHING
void func_8007AC30(u8 **pc, u8 enemy) {
    u16 *value = &D_800D3400[enemy].vars[(*pc)[1]];
    s32 difference = *value - (((*pc)[3] << 8) + (*pc)[2]);

    if (difference < 0) {
        difference = 0;
    }
    *value = difference;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AC30);
#endif

/* AI action 12: variable b1 *= b2 | b3 << 8, saturating at 0xffff. */
#ifdef NON_MATCHING
void func_8007AC80(u8 **pc, u8 enemy) {
    u16 *value = &D_800D3400[enemy].vars[(*pc)[1]];
    s32 product = *value * (((*pc)[3] << 8) + (*pc)[2]);

    if (product > 0xFFFF) {
        product = 0xFFFF;
    }
    *value = product;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AC80);
#endif

/* AI action 13: variable b1 /= b2 | b3 << 8. */
#ifdef NON_MATCHING
void func_8007ACDC(u8 **pc, u8 enemy) {
    u16 *value = &D_800D3400[enemy].vars[(*pc)[1]];

    *value = *value / (((*pc)[3] << 8) + (*pc)[2]);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ACDC);
#endif

/* AI action 14: variable b1 %= b2 | b3 << 8. */
#ifdef NON_MATCHING
void func_8007AD24(u8 **pc, u8 enemy) {
    u16 *value = &D_800D3400[enemy].vars[(*pc)[1]];

    *value = *value % (((*pc)[3] << 8) + (*pc)[2]);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AD24);
#endif

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
#ifdef NON_MATCHING
void func_8007B2C0(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    vars[op[3]] = vars[op[1]] & vars[op[2]];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B2C0);
#endif

/* AI action 26: variable b3 = var b1 | var b2. */
#ifdef NON_MATCHING
void func_8007B310(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    vars[op[3]] = vars[op[1]] | vars[op[2]];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B310);
#endif

/* AI action 27: variable b3 = var b1 ^ var b2. */
#ifdef NON_MATCHING
void func_8007B360(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    u16 *vars = D_800D3400[enemy].vars;

    vars[op[3]] = vars[op[1]] ^ vars[op[2]];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B360);
#endif

/* AI action 28: 80079ed8 for the enemy's slot with b1, b2. */
void func_8007B3B0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    func_80079ED8(enemy + 3, op[1], op[2], 0);
}

/* AI action 29: 8007a280 for the enemy's slot with b1 and b2 | b3 << 8. */
void func_8007B3E4(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    func_8007A280(enemy + 3, op[1], op[2] | (op[3] << 8), 0);
}

/* AI action 2a: byte variable b1 = attribute b2 of the first slot in var b3. */
void func_8007B424(u8 **pc, u8 enemy) {
    u8 slot = func_80079E7C(D_800D3400[enemy].vars[(*pc)[3]]);

    D_800D3400[enemy].bytes[(*pc)[1]] = func_80079ED8(slot, (*pc)[2], 0, 1);
}

/* AI action 2b: set attribute b1 of the first slot in var b3 to byte b1's
 * value; a party slot instead queues action type 0x20. */
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
 * a party slot instead queues action type 0x20. */
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
 * long b1; a party slot instead queues action type 0x20. */
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

/* AI action 39: the enemy record's +0x14c = b1 | b2 << 8. */
#ifdef NON_MATCHING
void func_8007BAE8(u8 **pc, u8 enemy) {
    s32 value = (*pc)[1] | ((*pc)[2] << 8);

    D_800CCCE8.records[enemy + 3].field14C = value;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BAE8);
#endif

/* AI action 3a: the enemy record's +0x156 = b1 | b2 << 8. */
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

/* AI action 3c: the enemy record's bytes +0x154, +0x152, +0x150 = b1, b2, b3. */
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
 * targeting test b2 and not flagged at 800d32a1; 0 when none does. */
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
 * targeting test b2, flagged at 800d32a1 and in the enemy's formation group. */
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
 * targeting test b2 and flagged at 800d32a1. */
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
 * targeting test b2 and flagged at 800d32a1; 0 when none does. */
#ifdef NON_MATCHING
void func_8007CB20(u8 **pc, u8 enemy) {
    u8 candidates[8];
    u8 *next;
    s32 count;
    s32 slot;

    next = candidates;
    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0) {
            *next++ = slot;
            count++;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CB20);
#endif

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
#ifdef NON_MATCHING
void func_8007D1DC(u8 **pc, u8 enemy) {
    u8 candidates[11];
    u8 *next;
    s32 count;
    s32 slot;

    next = candidates;
    slot = 0;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A744(slot) && D_800CCCE8.records[slot].pilot.characterId == (*pc)[2]) {
            *next++ = slot;
            count++;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D1DC);
#endif

/* AI action 55: long b1 = the enemy's byte at 800d2c8b. */
void func_8007D30C(u8 **pc, u8 enemy) {
    D_800D3400[enemy].longs[(*pc)[1]] = D_800D2C8B[enemy];
}

/* AI action 56: variable b1 = the bit of a random enemy slot passing
 * 8007a6c8(b2) with slot info +3 bit 0x80; 0 when none does. */
#ifdef NON_MATCHING
void func_8007D344(u8 **pc, u8 enemy) {
    u8 candidates[8];
    u8 *next;
    s32 count;
    s32 slot;

    next = candidates;
    slot = 3;
    count = 0;
    D_800D3400[enemy].vars[(*pc)[1]] = 0;
    for (; slot < 11; slot++) {
        if (func_8007A6C8(slot, (*pc)[2]) && (D_800C3EB4[slot].hidden & 0x80)) {
            *next++ = slot;
            count++;
        }
    }
    if (count != 0) {
        D_800D3400[enemy].vars[(*pc)[1]] = func_80089C08(candidates[func_8001BD40(0, count - 1)]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D344);
#endif

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

/* AI action 5e: as action 5c, limited to party slots flagged at 800d32a1. */
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

/* AI action 5f: as action 5d, limited to enemy slots flagged at 800d32a1. */
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
 * (b2 1) flagged, (b2 2) not flagged at 800d32a1, or (other b2) any. */
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
 * (b2 1) flagged, (b2 2) not flagged at 800d32a1, or (other b2) any. */
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

/* AI action 69: clear the enemy's 800d2c60 long and set its 800d2c8b byte
 * to 4. */
void func_8007E674(u8 enemy) {
    D_800D2C60[enemy] = 0;
    D_800D2C8B[enemy] = 4;
}

/* AI action 6a: long b3 = long b1 + long b2. */
#ifdef NON_MATCHING
void func_8007E6A0(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 *longs = D_800D3400[enemy].longs;

    longs[op[3]] = longs[op[1]] + longs[op[2]];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E6A0);
#endif

/* AI action 6b: long b3 = long b1 - long b2. */
#ifdef NON_MATCHING
void func_8007E6F0(u8 **pc, u8 enemy) {
    u8 *op = *pc;
    s32 *longs = D_800D3400[enemy].longs;

    longs[op[3]] = longs[op[1]] - longs[op[2]];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E6F0);
#endif

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

/* AI action 6e: halfword b1 of the table at *800d3278 + 0x394 = b2. */
void func_8007E7C0(u8 **pc) {
    u8 *op = *pc;

    D_800D3278->unk394[op[1]] = op[2];
}

/* AI action 6f: set (b2 != 0) or clear flag b1 + 7 in every party record's
 * +0x7a. */
#ifdef NON_MATCHING
void func_8007E7E4(u8 **pc) {
    Combatant *record = D_800CCCE8.records;
    u8 set = (*pc)[2] != 0;

    do {
        if (set) {
            record->pilot.status7A |= func_80089BEC((*pc)[1] + 7);
        } else {
            record->pilot.status7A &= ~func_80089BEC((*pc)[1] + 7);
        }
        record++;
    } while (record < &D_800CCCE8.records[3]);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E7E4);
#endif

/* AI action 70: formation group distance b1 -> b2 = b3. */
void func_8007E8AC(u8 **pc) {
    u8 *op = *pc;

    D_800D3364->links[op[1]][op[2]].distance = op[3];
}

/* AI action 71: the first slot of var b1 takes the next turn. */
void func_8007E8E0(u8 **pc, u8 enemy) {
    D_800D2DC0 = func_80079E7C(D_800D3400[enemy].vars[(*pc)[1]]) + 1;
}

/* AI action 72: rebuild the turn order (80078508) into a scratch buffer. */
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
#ifdef NON_MATCHING
s32 func_8007E98C(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].vars[op[1]] == (op[2] | (op[3] << 8));
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E98C);
#endif

/* AI condition 83: byte variable b1 <= b2. */
s32 func_8007E9D0(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].bytes[op[1]] <= op[2];
}

/* AI condition 84: variable b1 <= b2 | b3 << 8. */
#ifdef NON_MATCHING
s32 func_8007EA08(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].vars[op[1]] <= (op[2] | (op[3] << 8));
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EA08);
#endif

/* AI condition 85: byte variable b1 >= b2. */
s32 func_8007EA4C(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].bytes[op[1]] >= op[2];
}

/* AI condition 86: variable b1 >= b2 | b3 << 8. */
#ifdef NON_MATCHING
s32 func_8007EA84(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].vars[op[1]] >= (op[2] | (op[3] << 8));
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EA84);
#endif

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
#ifdef NON_MATCHING
s32 func_8007EC10(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return (D_800D3400[enemy].vars[op[1]] & (op[2] + (op[3] << 8))) != 0;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EC10);
#endif

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
#ifdef NON_MATCHING
s32 func_8007ED14(u8 **pc, u8 enemy) {
    u8 *op = *pc;

    return D_800D3400[enemy].vars[op[1]] != (op[2] | (op[3] << 8));
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ED14);
#endif

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EF6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007F8C0);

/* For party character 4 with its UI flag +0x8e set: close window 0 and
 * release the graphics block. */
void func_8007FB70(u8 member) {
    if (D_800D2D24[member] == 4 && D_800D2D28->unk8E != 0) {
        func_8008FA60(0);
        func_8007765C();
        D_800D2D28->unk8E = 0;
    }
}

/* Place the separator lines of a `count`-row list (rows from the 800c3200
 * table) and remember the selected row (clamped below `count`). */
#ifdef NON_MATCHING
void func_8007FBE0(u8 count, u8 selected) {
    BattleGraphics *gfx;
    s32 i;

    if (count == selected) {
        selected--;
    }
    for (i = 0; i < count - 1; i++) {
        gfx = D_800C3EA4;
        gfx->unk908[i * 2 + D_800CCB04.buffer].x0 = 0xC;
        gfx->unk908[i * 2 + D_800CCB04.buffer].y0 = D_800C3200[count][i + 2] + 0x5E;
        gfx->unk908[i * 2 + D_800CCB04.buffer].x1 = 0x12;
        gfx->unk908[i * 2 + D_800CCB04.buffer].y1 = D_800C3200[count][i + 2] + 0x5E;
    }
    D_800D2D28->unk97 = selected;
    D_800D2D28->unk98 = D_800CCB04.buffer;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FBE0);
#endif

/* Release the loaded menu module block (UI +0xae). */
void func_8007FCE8(void) {
    if (D_800D2D28->unkAE != 0) {
        func_800320E8(D_800D367C);
        D_800D2D28->unkAE = 0;
    }
}

/* Load file 2 (a member flagged at 800d32a1) or 1 into a new heap block
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
    D_800D2DB4 = (BattleUnk2DB4 *)func_8008ABB8(0x5DA4, 0);
    bzero(D_800D2DB4, 0x5DA4);
    D_800D2DB4->unk5D9C = 0xA0;
    D_800D2DB4->unk5D9E = 0x64;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FF14);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080160);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080C94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008115C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081318);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081504);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800816F8);

/* Frame the camera and target cursor on the attack page target, mark the
 * four directions that lead to another target and show its name. */
void func_8008189C(u8 member) {
    s32 direction;

    if (D_800C3EAC->unk2E9 == 0) {
        func_800BC404(func_80089C08(D_800C3EAC->unk2E8));
        func_800BCD98(func_80089C08(D_800C3EAC->unk2E8));
        for (direction = 0; direction < 4; direction++) {
            if (func_80084854(D_800C3EAC->unk2E8, direction) != D_800C3EAC->unk2E8) {
                D_800C3E24->arrows[direction] = 1;
            } else {
                D_800C3E24->arrows[direction] = 0;
            }
        }
        if (D_800D2C34 != 4) {
            func_80093B08(member);
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800819A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081B58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800820A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800822C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082504);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800826CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082820);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800829F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082BB0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082D4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082F7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800830A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083340);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083580);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083748);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083948);

/* Whether the member can attack `slot`: present and visible; an unflagged
 * slot must be adjacent (formation distance 0) and not down, a flagged slot
 * not down by +0x120. */
u8 func_80083FF4(u8 member, u8 slot) {
    u8 result = 0;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0) {
        if (D_800D32A0[slot].unk1 == 0) {
            if (D_800D3364->links[D_800C3EB4[member].group][D_800C3EB4[slot].group].distance == 0) {
                result = (D_800CCCE8.records[slot].pilot.status7C & 0xC001) == 0;
            }
        } else if (!(D_800CCCE8.records[slot].gear.status7C & 0xC001)) {
            result = 1;
        }
    }
    return result;
}

/* Whether `slot` can be targeted by a party attack: present and visible;
 * unflagged slots also not down (+0x7c 0xc001) unless `any`, flagged slots
 * not down by +0x120 unless `any`. */
u8 func_80084108(u8 slot, u8 any) {
    u8 result = 0;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0) {
        if (D_800D32A0[slot].unk1 == 0) {
            result = 1;
            if (any == 0) {
                result = (D_800CCCE8.records[slot].pilot.status7C & 0xC001) == 0;
            }
        } else if (any != 0 || !(D_800CCCE8.records[slot].gear.status7C & 0xC001)) {
            result = 1;
        }
    }
    return result;
}

/* Order the member's attack candidates: the reachable opposing slots, those
 * in its own formation group first, and the lowest-HP one (within the group
 * when there is one) moved to the front. Returns the default target. */
#ifdef NON_MATCHING
u8 func_800841E0(u8 member) {
    u8 slots[12];
    u8 grouped[12];
    s32 i;
    s32 count;
    s32 n;
    u8 swap;

    D_800D3274 = 0;
    for (i = 0; i < 12; i++) {
        slots[i] = 0xFF;
        D_800C3E90[i] = 0xFF;
        grouped[i] = 0;
    }
    if (member < 3) {
        i = 3;
        count = 0;
        for (; i < 11; i++) {
            if (func_80083FF4(member, i)) {
                slots[count++] = i;
                D_800D3274++;
            }
        }
    } else {
        i = 0;
        count = 0;
        for (; i < 3; i++) {
            if (func_80083FF4(member, i)) {
                slots[count++] = i;
                D_800D3274++;
            }
        }
    }
    n = 0;
    for (i = 0; i < count; i++) {
        if (D_800C3EB4[member].group == D_800C3EB4[slots[i]].group) {
            D_800C3E90[n] = slots[i];
            slots[i] = 0xFF;
            grouped[n] = 1;
            n++;
        }
    }
    for (i = 0; i < count; i++) {
        if (slots[i] != 0xFF) {
            D_800C3E90[n++] = slots[i];
        }
    }
    if (grouped[0] != 0) {
        for (i = 1; i < count; i++) {
            if (grouped[i] != 0 && D_800CCCE8.records[D_800C3E90[i]].pilot.hp < D_800CCCE8.records[D_800C3E90[0]].pilot.hp) {
                swap = D_800C3E90[0];
                D_800C3E90[0] = D_800C3E90[i];
                D_800C3E90[i] = swap;
            }
        }
    } else {
        for (i = 1; i < count; i++) {
            if (D_800CCCE8.records[D_800C3E90[i]].pilot.hp < D_800CCCE8.records[D_800C3E90[0]].pilot.hp) {
                swap = D_800C3E90[0];
                D_800C3E90[0] = D_800C3E90[i];
                D_800C3E90[i] = swap;
            }
        }
    }
    return D_800C3E90[0];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800841E0);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084548);

/* Collect the enemy slots the member can target (80083ff4) as candidates
 * and their mask; returns the first candidate. */
#ifdef NON_MATCHING
u8 func_80084750(u8 member) {
    s32 i;
    s32 n;
    s32 count;
    s32 slot;

    n = 8;
    for (i = 11; i >= 0; i--) {
        D_800C3E90[i] = 0xFF;
    }
    count = 0;
    D_800D3274 = 0;
    D_800C3D64 = 0;
    i = 3;
    while (--n >= 0) {
        slot = i++;
        if (func_80083FF4(member, slot)) {
            D_800C3E90[count++] = slot;
            D_800C3D64 |= func_80089C08(slot);
            D_800D3274++;
        }
    }
    return D_800C3E90[0];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084750);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084854);

/* Keep the member's default target as the attack page target when it is
 * still a candidate, else take the first candidate. */
void func_80084A7C(u8 member) {
    s32 found = 0;
    s32 i;

    D_800C3EAC->unk2E8 = D_800C3EAC->slots[member].defaultTarget;
    func_800841E0(member);
    for (i = 0; i < D_800D3274; i++) {
        if (D_800C3E90[i] == D_800C3EAC->slots[member].defaultTarget) {
            found++;
            break;
        }
    }
    if (found == 0) {
        D_800C3EAC->unk2E8 = D_800C3E90[0];
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084B40);

/* The mask of slots in 800c3d64 in the formation group of slot 800c3e2c. */
u16 func_80084D28(void) {
    u16 mask = 0;
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (func_80089C9C(D_800C3D64, slot) && D_800C3EB4[D_800C3E2C].group == D_800C3EB4[slot].group) {
            mask |= func_80089C08(slot);
        }
    }
    return mask;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084DE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085084);

/* Whether slot b's slot-info +0xa is below slot a's. */
s32 func_80085310(u8 a, u8 b) {
    return (u16)D_800C3EB4[a].x > (u16)D_800C3EB4[b].x;
}

/* Reset the running result accumulation of every slot. */
void func_80085350(void) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        D_800D2D5C[slot] = 0xFF;
        D_800D2D70[slot] = 0;
    }
}

/* Clear the current event's per-slot results (the event index is re-read for
 * every store). */
void func_80085388(void) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        D_800C3FE8[D_800C3EAC->eventCount].amounts[slot] = 0;
        D_800C3FE8[D_800C3EAC->eventCount].codes[slot] = 0xFF;
        D_800C3FE8[D_800C3EAC->eventCount].accumulated[slot] = 0;
        D_800C3FE8[D_800C3EAC->eventCount].accumulatedCodes[slot] = 0xFF;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085618);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085AC4);

/* Apply up to three recovery amounts from 8009ada0 to `slot` as separate
 * events (codes 8..10) and show them. */
void func_80085B58(u8 slot) {
    s32 amounts[3];
    s32 i;

    amounts[2] = 0;
    amounts[1] = 0;
    amounts[0] = 0;
    if (func_8009ADA0(slot, amounts) != 0) {
        for (i = 0; i < 3; i++) {
            if (amounts[i] != 0) {
                D_800C3EAC->eventCount = 0;
                func_80085388();
                D_800C3FE8[0].codes[slot] = i + 8;
                D_800C3FE8[0].amounts[slot] = amounts[i];
                func_80085618(D_800C3EAC->eventCount);
            }
        }
        func_800BE538(slot, amounts[0], amounts[1], amounts[2]);
    }
}

/* Commit the targets for an item/effect and run 80098c6c with `param`. */
void func_80085C48(u8 actor, s16 targets, u16 param) {
    D_800C48E8 = 0;
    D_800D2C94 = targets;
    D_800D2C96 = D_800D39DC;
    func_80098C6C(param);
}

/* Accumulate and apply event `queue`'s results. */
void func_80085C88(u8 queue) {
    func_80085454(queue);
    func_80085618(queue);
    D_800D2D28->unkAD = 0;
}

/* Commit an action (attacker, target mask, animation) and resolve it. */
void func_80085CCC(u8 actor, s16 targets, s16 animation) {
    u8 action; /* 1-based */

    D_800C48E8 = 0;
    D_800D2CA9 = actor;
    action = D_800C3EAC->unk2DC;
    D_800D2C94 = targets;
    D_800D2C98 = animation;
    D_800D2C96 = D_800D39DC;
    D_800D2CAA = action - 1;
    func_800941A4();
}

/* Build the attack page's AP text (current AP, '/', maximum) as glyph
 * primitives and remember the draw buffer. */
void func_80085D34(void) {
    D_800D2D28->unk7B = 0;
    D_800D2D28->unk7B +=
        func_80076A10(D_800C3EAC->unk2D4[0] + 0xF, D_800C3EA4->unk9C8[D_800D2D28->unk7B], 0x2A, 0xD0);
    D_800D2D28->unk7B += func_80076A10(0x19, D_800C3EA4->unk9C8[D_800D2D28->unk7B], 0x32, 0xD0);
    D_800D2D28->unk7B +=
        func_80076A10(D_800C3EAC->unk2D4[1] + 0xF, D_800C3EA4->unk9C8[D_800D2D28->unk7B], 0x3A, 0xD0);
    D_800D2D28->unkA4 = D_800CCB04.buffer;
}

/* Reset the turn state's seven +0x2cc bytes to 0xff and clear +0x2d6. */
void func_80085E78(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        D_800C3EAC->unk2CC[i] = 0xFF;
    }
    D_800C3EAC->unk2D6 = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085EB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086028);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800861D0);

/* Whether the member can use combo step `step` now: without a combo chain
 * (+0x2d6) always; otherwise its character must know the combo flag
 * (800c34cc) and the chain must allow another step. */
#ifdef NON_MATCHING
s32 func_80086B88(s32 step, u8 member) {
    u8 index = step + (D_800C3EAC->unk2CC[0] + 1) * 3;
    s32 result = 1;

    if (D_800C3EAC->unk2D6 != 0) {
        if (D_800D2C34 == 4) {
            index = step + 12;
        }
        if (!func_80089C6C(D_8006ECF8[D_800D2D24[member]].combos, D_800C34CC[index])) {
            result = 0;
        } else if (D_800C3EAC->unk2CC[0] != 0xFF && D_800D2C34 != 4 && D_800D2C34 < D_800C3EAC->unk2CC[0] + 1) {
            result = 0;
        }
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086B88);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086C88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800877E0);

/* Reset every event to type 0xff for `actor` against `target`'s bit. */
void func_800879A8(u8 actor, u8 target) {
    s32 i;

    for (i = 0; i < 32; i++) {
        D_800C3FE8[i].type = 0xFF;
        D_800C3FE8[i].actor = actor;
        D_800C3FE8[i].targetMask = func_80089C08(target);
    }
}

/* Enter the attack page: AP text, events against the default target, and
 * the member's attack model. */
void func_80087A38(u8 member) {
    s32 route;

    func_80085D34();
    func_800879A8(member, D_800C3EAC->slots[member].defaultTarget);
    D_800D366C = 0;
    route = func_800877E0(member, D_800C3EAC->slots[member].defaultTarget);
    func_800B89FC(route, member, D_800C3EAC->slots[member].defaultTarget, func_80080AE4(member));
    D_800D366C = 1;
    D_800C3E18 = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087AF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800881B8);

/* Drop a slot from its formation group (enemy entries from 8, flagged slots
 * add 0x10). */
void func_800883AC(u8 slot) {
    u8 base = (slot >= 3) * 8;

    if (D_800D32A0[slot].unk1 != 0) {
        base |= 0x10;
    }
    D_800D301C[D_800C3EB4[slot].group + base].count--;
    D_800D301C[D_800C3EB4[slot].group + base].members &= func_80089C48(D_800C3EB4[slot].member);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088490);

/* The member count of the slot's group among the flagged enemy groups. */
u8 func_800885D0(u8 slot) {
    return D_800D301C[D_800C3EB4[slot].group + 0x18].count;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008860C);

/* Set up a stepped line from (x0, y0) to (x1, y1): directions, the 8.8 steps
 * of the minor axis and a random speed 1..8. */
void func_8008887C(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 dx;
    s32 dy;

    D_800C3A7C = x0;
    D_800C3A80 = y0;
    D_800C3A84 = x1;
    D_800C3A88 = y1;
    if (x1 != x0 && y1 != y0) {
        if (x1 < x0) {
            D_800C3A94 = 1;
            dx = x0 - x1;
        } else {
            dx = x1 - x0;
            D_800C3A94 = 0;
        }
        if (y1 < y0) {
            D_800C3A98 = 1;
            dy = y0 - y1;
        } else {
            dy = y1 - y0;
            D_800C3A98 = 0;
        }
        if (dx >= dy) {
            D_800C3A8C = 0x100;
            D_800C3A90 = (dy << 8) / dx;
        } else {
            D_800C3A90 = 0x100;
            D_800C3A8C = (dx << 8) / dy;
        }
        D_800C2080 = 0;
        D_800C2084 = 0;
        D_800C3A9C = func_8001BD40(1, 8);
        D_800C207C = 0;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088B80);

/* Build the glyphs of the flags set in 800d2c30 (up to five, 10 pixels
 * apart from y 0x6e) into the +0x4ce0 primitives. */
void func_80089038(void) {
    s32 i;
    s32 y; /* 16.16 */

    i = 0;
    y = 0x6E << 16;
    D_800D2DB4->unk5DA1 = 0;
    for (; i < 5; i++) {
        if (func_80089C6C(D_800D2C30, i)) {
            D_800D2DB4->unk5DA1 += func_80076A10(i + 0xC4, &D_800D2DB4->unk4CE0[D_800D2DB4->unk5DA1 * 2], 0xE0, y >> 16);
            y += 10 << 16;
        }
    }
    D_800D2DB4->unk5DA0 = D_800CCB04.buffer;
    D_800D2DB4->unk5DA2 = 0;
}

/* Build glyph 0xa0 (0xa1 with 800d2c38) into the +0x3ac0 primitives and
 * initialise the current buffer's quads. */
void func_80089110(void) {
    s32 id = 0xA0;
    s32 i;

    if (D_800D2C38 != 0) {
        id = 0xA1;
    }
    D_800D2DB4->counts[0] = func_80076A10(id, D_800D2DB4->unk3AC0, 0xA0, 0x64);
    D_800D2DB4->buffers[0] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[0]; i++) {
        func_80076B68(&D_800D2DB4->unk3AC0[i * 2 + D_800D2DB4->buffers[0]]);
    }
}

/* Build the two glyphs of the escape/limit page (800d2c34 - 0x5d and
 * - 0x25) into lists 2 and 10 and initialise their quads. */
#ifdef NON_MATCHING
void func_800891E4(void) {
    s32 i;
    u8 second = D_800D2C34 - 0x25;

    D_800D2DB4->counts[2] = func_80076A10((u8)(D_800D2C34 - 0x5D), D_800D2DB4->unk3E80, 0xA0, 0x64);
    D_800D2DB4->buffers[2] = D_800CCB04.buffer;
    D_800D2DB4->counts[10] = func_80076A10(second, D_800D2DB4->unk43D0, 0xA0, 0x64);
    D_800D2DB4->buffers[10] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[2]; i++) {
        func_80076B68(&D_800D2DB4->unk3E80[i * 2 + D_800D2DB4->buffers[2]]);
    }
    for (i = 0; i < D_800D2DB4->counts[10]; i++) {
        func_80076BF0(&D_800D2DB4->unk43D0[i * 2 + D_800D2DB4->buffers[10]]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800891E4);
#endif

/* Build the five-digit value 800d2c2a as glyphs into list 3 at (0x11a, 0x46)
 * and initialise its quads. */
void func_80089348(void) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    x = 0x11A << 16;
    func_8008AAA0(D_800D2C2A);
    for (; i < 5; i++) {
        digit = D_800C3CF4[i + 4];
        if (digit != 0xFF) {
            D_800D2DB4->counts[3] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->unk4E70[D_800D2DB4->counts[3] * 2], x >> 16, 0x46);
            x += 6 << 16;
        }
    }
    D_800D2DB4->buffers[3] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[3]; i++) {
        func_80076B68(&D_800D2DB4->unk4E70[i * 2 + D_800D2DB4->buffers[3]]);
    }
}

/* Build the two-digit value 800d2c3a and a '%' glyph into list 4 at
 * (0x11a, 0x4e), or glyph 0xa2 from page 4 on, and initialise its quads. */
void func_8008946C(void) {
    s32 i;
    s32 x; /* 16.16 */
    s32 digits;
    u8 digit;

    if (D_800D2C34 < 4) {
        func_8008AAA0(D_800D2C3A);
        i = 0;
        digits = 0;
        x = 0x11A << 16;
        for (; i < 2; i++) {
            digit = D_800C3CF4[i + 7];
            if (digit != 0xFF) {
                    D_800D2DB4->counts[4] +=
                    func_80076A10(digit + 0x92, &D_800D2DB4->unk4FB0[D_800D2DB4->counts[4] * 2], x >> 16, 0x4E);
                digits++;
                x += 6 << 16;
            }
        }
        D_800D2DB4->counts[4] +=
            func_80076A10(0x9D, &D_800D2DB4->unk4FB0[D_800D2DB4->counts[4] * 2], digits * 6 + 0x11A, 0x4E);
    } else {
        D_800D2DB4->counts[4] = func_80076A10(0xA2, D_800D2DB4->unk4FB0, 0x11A, 0x4E);
    }
    D_800D2DB4->buffers[4] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[4]; i++) {
        func_80076B68(&D_800D2DB4->unk4FB0[i * 2 + D_800D2DB4->buffers[4]]);
    }
}

/* Build the three-digit value 800d2c36 and a '%' glyph into list 5 at
 * (0x11a, 0x56) and initialise its quads. */
void func_8008963C(void) {
    s32 i;
    s32 x; /* 16.16 */
    s32 digits;
    u8 digit;

    i = 0;
    digits = 0;
    x = 0x11A << 16;
    func_8008AAA0(D_800D2C36);
    for (; i < 3; i++) {
        digit = D_800C3CF4[i + 6];
        if (digit != 0xFF) {
            D_800D2DB4->counts[5] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->unk50A0[D_800D2DB4->counts[5] * 2], x >> 16, 0x56);
            digits++;
            x += 6 << 16;
        }
    }
    D_800D2DB4->counts[5] +=
        func_80076A10(0x9D, &D_800D2DB4->unk50A0[D_800D2DB4->counts[5] * 2], digits * 6 + 0x11A, 0x56);
    D_800D2DB4->buffers[5] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[5]; i++) {
        func_80076B68(&D_800D2DB4->unk50A0[i * 2 + D_800D2DB4->buffers[5]]);
    }
}

/* Build the two-digit value 800d2c35 as glyphs into list 6 at (0x11a,
 * 0x5e) and initialise its quads. */
void func_800897CC(void) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    x = 0x11A << 16;
    func_8008AAA0(D_800D2C35);
    for (; i < 2; i++) {
        digit = D_800C3CF4[i + 7];
        if (digit != 0xFF) {
            D_800D2DB4->counts[6] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->unk51E0[D_800D2DB4->counts[6] * 2], x >> 16, 0x5E);
            x += 6 << 16;
        }
    }
    D_800D2DB4->buffers[6] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[6]; i++) {
        func_80076B68(&D_800D2DB4->unk51E0[i * 2 + D_800D2DB4->buffers[6]]);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800898F0);

/* Run the eight 8008860c..800897cc steps. */
void func_80089AF8(void) {
    func_8008860C();
    func_80089038();
    func_80089110();
    func_800891E4();
    func_80089348();
    func_8008946C();
    func_8008963C();
    func_800897CC();
}

/* A random value in low..high (0xffff for low 0xffff, 0 for high 0). */
#ifdef NON_MATCHING
u16 func_80089B50(u16 low, u16 high) {
    s32 span;

    if (low == 0xFFFF) {
        return 0xFFFF;
    }
    if (high == 0) {
        return 0;
    }
    span = high - low;
    if (low == high) {
        return low;
    }
    if (span >= 0xFFFF) {
        return rand();
    }
    return low + (u16)rand() % (span + 1);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089B50);
#endif

/* The mask bit `bit`. */
u16 func_80089BEC(u8 bit) {
    return D_800C3468[bit];
}

/* The mask bit of `slot`. */
u16 func_80089C08(u8 slot) {
    return D_800C3448[slot];
}

/* Every mask bit but `bit`. */
u16 func_80089C24(u8 bit) {
    return ~D_800C3468[bit];
}

/* Every slot bit but `slot`'s. */
u16 func_80089C48(u8 slot) {
    return ~D_800C3448[slot];
}

/* Bit `bit` of `mask`; 0 for bits past 15. */
u16 func_80089C6C(u16 mask, u8 bit) {
    u16 result;

    if (bit < 16) {
        result = D_800C3468[bit] & mask;
    } else {
        result = 0;
    }
    return result;
}

/* The bit of `slot` in `mask`; 0 for slots past 15. */
u16 func_80089C9C(u16 mask, u8 slot) {
    u16 result;

    if (slot < 16) {
        result = D_800C3448[slot] & mask;
    } else {
        result = 0;
    }
    return result;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089CCC);

/* Every other frame upload the next step of the four cycling CLUT strips. */
void func_8008A144(void) {
    D_800D2D28->unkAA++;
    if (D_800D2D28->unkAA & 1) {
        LoadImage(&D_800C3EA4->unk8950[0], &D_800C3EA4->unk8970[0][D_800D2D28->unk30]);
        LoadImage(&D_800C3EA4->unk8950[1], &D_800C3EA4->unk8970[1][D_800D2D28->unk30]);
        LoadImage(&D_800C3EA4->unk8950[2], &D_800C3EA4->unk8970[2][D_800D2D28->unk30]);
        LoadImage(&D_800C3EA4->unk8950[3], &D_800C3EA4->unk8970[3][D_800D2D28->unk30]);
        D_800D2D28->unk30 += 4;
        if (D_800D2D28->unk30 >= 0xC7) {
            D_800D2D28->unk30 = 0;
        }
    }
}

/* Result-screen tick of end state 1: the ATB while 800ccc58, input for the
 * member, counters, the pulsing shade, the scroll by 800d39d4 and the CLUT
 * cycle. */
void func_8008A274(u8 member) {
    if (D_800CCC58 != 0) {
        func_8007171C();
    }
    if (member == 0) {
        func_80089CCC(0);
    }
    D_800D2D28->unkA9 += 6;
    D_800D2D28->unkAB++;
    if (D_800C3EA4->unk6415 != 0) {
        if (D_800C3EA4->unk6416 == 0) {
            D_800C3EA4->unk6410 += 4;
            if (D_800C3EA4->unk6410 > 0x80) {
                D_800C3EA4->unk6410 = 0x7C;
                D_800C3EA4->unk6416 = 1;
            }
        } else {
            D_800C3EA4->unk6410 -= 4;
            if (D_800C3EA4->unk6410 < 0) {
                D_800C3EA4->unk6410 = 4;
                D_800C3EA4->unk6416 = 0;
            }
        }
    }
    switch (D_800D39D4) {
    case 1:
    case 3:
        D_800D3288++;
        break;
    case 2:
    case 4:
        D_800D3288--;
        break;
    }
    func_8008A144();
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A3EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A684);

/* Result-screen step by the battle end state 800c3e4c. */
void func_8008A9C0(u8 member) {
    switch (D_800C3E4C) {
    case 0:
        func_8008A684(member);
        break;
    case 1:
        func_8008A274(member);
        break;
    case 2:
        func_8008A3EC(member);
        break;
    }
}

/* Play menu sound effect `id` of the system effect bank. */
void func_8008AA40(u8 id) {
    func_80039DB8((D_8005919C->bank << 16) | id);
}

/* Play menu sound effect `id` while menu effects are enabled. */
void func_8008AA74(u8 id) {
    if (D_800D366C != 0) {
        func_8008AA40(id);
    }
}

/* Split `value` into nine decimal digits at 800c3cf4, leading zeros 0xff. */
void func_8008AAA0(u32 value) {
    u32 divisor = 100000000;
    s32 i;

    for (i = 0; i < 9; i++) {
        D_800C3CF4[i] = value / divisor;
        value %= divisor;
        divisor /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (D_800C3CF4[i] != 0) {
            if (D_800C3CF4[i - 1] == 0) {
                D_800C3CF4[i - 1] = 0xFF;
            }
            break;
        }
        D_800C3CF4[i - 1] = 0xFF;
    }
}

/* Heap mode 0x20/0. */
void func_8008AB4C(void) {
    func_80028470(0x20, 0);
}

/* Heap mode 0x20/2. */
void func_8008AB70(void) {
    func_80028470(0x20, 2);
}

/* Heap mode 0x20/3. */
void func_8008AB94(void) {
    func_80028470(0x20, 3);
}

/* Allocate a battle heap block (owner tag 2). */
s32 func_8008ABB8(s32 size, s32 mode) {
    func_80032498(2, 0);
    return (s32)func_80031BDC(size, mode);
}

/* Allocate a text image block for `count` characters. */
s32 func_8008AC00(s32 count) {
    func_80032498(2, 0);
    return (s32)func_80031BDC((count + 3) * 26, 0);
}

/* Wait frames until the disc reads finish. */
void func_8008AC50(void) {
    while (func_800286CC() != 0) {
        func_800716D8();
    }
}

/* Queue event 0xf3 for `actor` with the enemies of `mask` whose +0x34 bit
 * 0x800 is set (low three bits dropped). */
void func_8008AC88(u16 mask, u8 actor) {
    u16 targets = mask & 0xFFF8;
    s32 i;

    for (i = 0; i < 8; i++) {
        if (func_80089C9C(targets, i + 3) && !(D_800CCCE8.records[i + 3].pilot.flags34 & 0x800)) {
            targets &= func_80089C48(i + 3);
        }
    }
    if (targets) {
        D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0xF3;
        D_800C3FE8[D_800C3EAC->eventCount].parameter = targets;
        D_800C3EAC->eventCount++;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008ADD0);

/* Hide the command windows (four panels); without `keep` show the
 * +0x641c lists. */
void func_8008B108(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = D_800D2D28->windows[3] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (four panels, page 1) and frame the camera on
 * the member and its default target. */
void func_8008B168(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = D_800D2D28->windows[3] = 1;
    D_800D2D28->unkB7 = 1;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B224);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B478);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B908);

/* Hide the command windows (two panels); without `keep` show the +0x641c
 * lists. */
void func_8008BC40(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (two panels, page 2) and frame the camera on the
 * member and its default target. */
void func_8008BC98(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 1;
    D_800D2D28->unkB7 = 2;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BD50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BED8);

/* Hide the command windows; with `close` also close windows 0 and 1 and
 * release the graphics block. */
void func_8008C360(u8 close) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->unkB7 = 0;
    if (close != 0) {
        D_800D2D28->unkC6 = 0;
        func_8008FA60(0);
        func_8008FA60(1);
        func_800716D8();
        func_8007765C();
        func_80077980();
    } else {
        D_800D2D28->windows[0] = D_800D2D28->windows[1] = 0;
    }
}

/* Show the command windows (two panels, page 3) and frame the camera on the
 * member and its default target. */
void func_8008C3F0(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->unkB7 = 3;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 1;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C4A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C81C);

/* Hide the command windows (three panels); without `keep` show the +0x641c
 * lists. */
void func_8008CCCC(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (three panels, page 4) and frame the camera on
 * the member and its default target. */
void func_8008CD28(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = 1;
    D_800D2D28->unkB7 = 4;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CDE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CFB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008D328);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008D598);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008DC34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008DE04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008E430);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008EA70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008F0A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008F6E4);

/* Open window `window` at (x, y) of w x h: allocate its blocks when it is not
 * shown; `animate` grows it open, otherwise it is drawn at once (and a frame
 * waited with `wait`). */
void func_8008F8F4(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait) {
    WindowRect *rect;

    if (D_800D2D28->windows[window] == 0) {
        D_800D2E38[window] = (void *)func_8008ABB8(0x5A8, 0);
        bzero(D_800D2E38[window], 0x5A8);
        D_800D2D90[window] = (WindowRect *)func_8008ABB8(0xE, 0);
        bzero(D_800D2D90[window], 0xE);
        func_80077454(window);
    }
    if (animate != 0) {
        rect = D_800D2D90[window];
        rect->style = window;
        rect->x = x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        rect->curW = 0;
        rect->curH = 0;
        D_800D2D28->unkBF[window] = 0;
        D_800D2D28->unkB8[window] = 1;
    } else {
        func_8008F6E4(window, x, y, w, h);
        if (wait != 0) {
            func_800716D8();
        }
    }
}

/* Close window `window` and release its two blocks after a frame. */
void func_8008FA60(u8 window) {
    D_800D2D28->windows[window] = 0;
    D_800D2D28->unkB8[window] = 0;
    func_800716D8();
    func_800320E8(D_800D2E38[window]);
    func_800320E8(D_800D2D90[window]);
}

/* Grow every opening window by 32 pixels per frame up to its size, centred,
 * and mark it open when both sides are complete. */
void func_8008FAD8(void) {
    s32 i;
    WindowRect *rect;
    u8 done;

    for (i = 0; i < 7; i++) {
        rect = D_800D2D90[i];
        if (D_800D2D28->unkB8[i] != 0 && D_800D2D28->unkBF[i] == 0) {
            done = 0;
            if (rect->curW + 32 >= rect->w) {
                rect->curW = rect->w;
                done = 1;
            } else {
                rect->curW += 32;
            }
            if (rect->curH + 32 >= rect->h) {
                rect->curH = rect->h;
                done++;
            } else {
                rect->curH += 32;
            }
            if (done == 2) {
                D_800D2D28->unkBF[i] = 1;
            }
            func_8008F6E4(rect->style, rect->x + (rect->w >> 1) - (rect->curW >> 1),
                          rect->y + (rect->h >> 1) - (rect->curH >> 1), rect->curW, rect->curH);
        }
    }
}

/* Build a message frame from glyphs into the +0x1e68 list: `rows` side
 * glyphs down from `top`, the corner at (x, y) and a stretched edge of
 * width `w`. */
#ifdef NON_MATCHING
void func_8008FC1C(s16 x, s16 y, s16 w, s16 top, u8 rows) {
    s32 i;
    s32 row;

    i = 0;
    D_800D2D28->unkFC = 0;
    if (rows != 0) {
        row = top;
        for (; i < rows; i++) {
            D_800D2D28->unkFC += func_80076A10(0x65, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2], x, row);
            row += 8;
        }
    }
    D_800D2D28->unkFC = func_80076A10(0x64, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2], x, y) + D_800D2D28->unkFC;
    D_800D2D28->unkFC += func_800263E4(D_800D2F5C, 0x64, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2],
                                       D_800CCB04.buffer, x, w, 0x1000, 0, 1);
    D_800D2D28->unkA6 = D_800CCB04.buffer;
    D_800D2D28->unk9D = 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FC1C);
#endif

/* Open the standard message window (0x20, 0x5c, 0xcc x 0x60, style 0xe). */
void func_8008FDE4(void) {
    func_8008FC1C(0x20, 0x5C, 0xCC, 0x60, 0xE);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FE18);

/* Build nine glyph rows (0x66) from y + 0x64 into the +0xba8 primitives. */
void func_8009023C(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x68;
    D_800D2D28->unkF8 = 0;
    for (; i < 9; i++) {
        D_800D2D28->unkF8 += func_80076A10(0x66, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x20, (offset - 4) + y);
        offset += 8;
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090310);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800904A0);

/* Render the name of the item in the list cell (column, row) into a text
 * image and place it on the graphics block's +0x280 quad. */
void func_8009070C(u8 column, u8 row) {
    RECT rect;
    u8 item = D_800D2CE0[row * 2 + column];
    u32 *pixels = (u32 *)func_8008AC00(0x39);
    s32 width;

    bzero(pixels, 0x618);
    width = func_80034EAC(func_80033728(D_800D329C, item), pixels, 0x39, 0);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x30, 0x42, 0, 0, width);
    func_800320E8(pixels);
}

/* Show the item list cell (column, row) of the item list or (from the
 * 800c3d70 list) the other list: frame, name and count. */
void func_8009080C(u8 column, u8 row, u8 other) {
    u8 item;

    if (other == 0) {
        item = D_800D2CE0[row * 2 + column];
    } else {
        item = D_800C3D70[row * 2 + column];
    }
    D_800C3EA4->unkA230->unk66D = 0;
    func_80090310(column, row);
    if (item != 0 && other == 0) {
        func_8009070C(column, row);
        func_800904A0(column, row);
        D_800C3EA4->unkA230->unk66D = 1;
    }
    D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk66B = 1;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009093C);

/* Animate the five-frame cursor glyph at (x, y): advance `frame` every third
 * tick. */
void func_80090B90(s32 x, s32 y, s32 *frame, u8 *ticks) {
    if (++*ticks >= 3) {
        *frame -= 1;
        if (*frame < 0) {
            *frame = 4;
        }
        *ticks = 0;
    }
    D_800D2D28->unk100 = func_80076A10(*frame + 0xE0, D_800C3EA4->unk27C8, x, y);
    D_800D2D28->unkA7 = D_800CCB04.buffer;
    D_800D2D28->unk9E = 1;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090E7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091064);

/* Build eight glyph rows (0x66) from y + 0x38 into the +0xba8 primitives. */
void func_80091604(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x38;
    D_800D2D28->unkF8 = 0;
    for (; i < 8; i++) {
        D_800D2D28->unkF8 += func_80076A10(0x66, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x20, offset + y);
        offset += 8;
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800916D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009187C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091B38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091D38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091EC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009209C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092298);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092784);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092B74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800930AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80093578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009382C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800939CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80093B08);

/* For party character 4: close its command window and, with `release`,
 * its extra resources. */
void func_8009413C(u8 member, u8 release) {
    if (D_800D2D24[member] == 4) {
        D_800D2D28->unkB7 = 0;
        D_800D2D28->windows[0] = 0;
        if (release != 0) {
            func_8007FB70(member);
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800941A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800946F4);

/* Add the damage dealt to the target to each hit party member's running
 * total (+0x5f60 with record flag 0x80 at +0x15a, else +0x5f54). */
void func_80094C78(void) {
    u8 member;

    for (member = 0; member < 3; member++) {
        if (D_800C34B0->resultCode[member] == 0) {
            if (D_800C34B0->records[member].flags15A & 0x80) {
                D_800C34B0->field5F60[member] += D_800C34B0->damage[D_800C3E50];
            } else {
                D_800C34B0->field5F54[member] += D_800C34B0->damage[D_800C3E50];
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094D24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094EE4);

/* Formula: amount = attacker +0x5b times the descriptor's +0x11 (doubled
 * with attacker +0x8a bit 0x2000), scaled 0.7 / 1.3 by the target's
 * +0x8c|+0x8e bits 0x100 / 0x200, none for a +0x15a 0x80 target; code 2. */
void func_80095690(void) {
    s16 amount = D_800C3E00->pilot.accuracy * D_800C3DFC->power;
    u16 status;

    if (D_800C3E00->pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    status = D_800C3E34->pilot.status8C.half.active | D_800C3E34->pilot.status8C.half.permanent;
    if (status & 0x100) {
        amount = amount * 7 / 10;
    }
    if (status & 0x200) {
        amount = amount * 13 / 10;
    }
    if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
        amount = 0;
    }
    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* The target defends: a +0x56 state 2 target first leaves it (8009ac48),
 * its status words clear, result code 2 and a tenth of its +0x4e times the
 * descriptor's +0x11 as the amount; its timer is held. */
#ifdef NON_MATCHING
void func_800957D8(void) {
    if (D_800CCCE8.records[D_800C3E50].pilot.characterId == 2) {
        func_8009AC48(D_800C3E50, 1);
    }
    D_800C3E34->pilot.status8C.half.active = D_800C3E34->pilot.status88.half.active = D_800C3E34->pilot.status84.half.active = D_800C3E34->pilot.status80 = D_800C3E34->pilot.status7C = 0;
    D_800D2C88[D_800C3E50] = 2;
    D_800D2C54[D_800C3E50] = (D_800C3E34->pilot.maxHp * D_800C3DFC->power) / 10;
    D_800D2C9E |= 1 << D_800C3E50;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800957D8);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800958D8);

/* Status effect of the descriptor on the target (mode +0x11, or 5 with
 * +0xa bit 0x4000); a refused status marks the target's result 6. */
void func_80095A78(void) {
    s8 accepted = func_80097964(D_800C3DFC->field1C, D_800C3DFC->field1D, D_800C3DFC->field1E);

    if (D_800C3DFC->flagsA & 0x4000) {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, 5);
    } else {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, D_800C3DFC->power);
        if (accepted != 1) {
            D_800C34B0->resultCode[D_800C3E50] = 6;
        }
    }
}

/* When 80097964 accepts the attacker's +2/+3/+0 values, run 800995a0 on the
 * target with the descriptor's +0x1d/+0x1e and mode 5. */
void func_80095B44(void) {
    if (func_80097964(D_800C3E00->pilot.entries[0].value2, D_800C3E00->pilot.entries[0].value3, D_800C3E00->pilot.entries[0].field0) == 1) {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, 5);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095BAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095D4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096494);

/* Ether check: unless rand % 100 falls below the attacker's +0x5b plus the
 * descriptor's +0x14, the action fails (code 0x38 at +0x5fc7). */
void func_80096824(void) {
    s32 chance = D_800C3E00->pilot.accuracy + D_800C3DFC->accuracy;

    if (rand() % 100 >= chance) {
        D_800C34B0->message = 0x38;
        D_800D2DC4 = 1;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800968C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096AB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097610);

void func_8009795C(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097964);

/* Clear the per-slot damage and result codes (12 entries). */
void func_80097D08(void) {
    s16 slot = 11;

    do {
        D_800C34B0->resultCode[slot] = 0xFF;
        D_800C34B0->damage[slot] = 0;
    } while (--slot != -1);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097D5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009892C);

/* A slot's turn timer from its speed (party: less the current command's
 * weight, gear speed for a slot in gear), capped, with a random -3..4
 * spread. Sets the attacker globals and, for the party, the command
 * descriptor. Defined old-style: callers pass the slot unnarrowed. */
s32 func_80098AF8(slot, mode)
u8 slot;
s32 mode;
{
    Combatant *record = &D_800C34B0->records[slot];
    u16 speed;

    D_800D2D6C = &record->gear;
    D_800C3E00 = record;
    if (slot < 3) {
        if (!(record->flags15A & 0x80)) {
            D_800C3DFC = &D_800C34B0->partyCommands[slot][D_800C34B0->commandIndex];
            if (record->pilot.speed > D_800C3DFC->weight) {
                speed = (record->pilot.speed - D_800C3DFC->weight) * 9;
            } else {
                speed = 9;
            }
        } else {
            D_800C3DFC = &D_800C34B0->gearCommands[slot][D_800C34B0->commandIndex];
            if (record->gear.speed > D_800C3DFC->weight) {
                speed = (record->gear.speed - D_800C3DFC->weight) * 9;
            } else {
                speed = 9;
            }
        }
    } else {
        speed = record->pilot.speed * 9;
    }
    if (speed >= 0xA6) {
        speed = 0xA0;
    }
    speed = 0xA5 - speed;
    speed -= rand() % 8 - 4;
    func_80094D24();
    return (u8)speed;
}

/* Resolve an item/effect `param` on every slot in the +0x5fac mask, then
 * set its animation from the effect table. */
#ifdef NON_MATCHING
void func_80098C6C(u16 param) {
    s32 slot;
    s32 bit;

    func_80097D08();
    bit = 1;
    for (slot = 0; (u8)slot < 11; slot++) {
        if (bit & D_800C34B0->targetMask) {
            func_80098D2C(slot, param);
        }
        bit <<= 1;
    }
    D_800C34B0->shownCommand = D_800D2200[param & 0xFF].animation;
    if (D_800D2C9E & 0x8000) {
        D_800D2C98 = 0xC2;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098C6C);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098D2C);

/* Party gate on the formation mode: 1 when mode 2 has no member with status
 * bits 0xC002, or mode 3 does not have exactly two; otherwise 0. */
s32 func_80099498(void) {
    s32 result = 0;
    u8 i;
    u8 count;

    switch (D_800C34AD) {
    case 2:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (D_800C34B0->records[i].pilot.status7C & 0xC002) {
                count++;
            }
        }
        if (count == 0) {
            result = 1;
        }
        break;
    case 3:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (D_800C34B0->records[i].pilot.status7C & 0xC002) {
                count++;
            }
        }
        if (count != 2) {
            result = 1;
        }
        break;
    case 1:
        break;
    }
    return result;
}

/* Store a timed status's duration in slot's timer table. The status is named by
 * its kind and flag bit; the attacker's flag 0x2000 doubles the amount, and for
 * kinds 5, 7 and 9 the target's status bits 0x5 double and 0xA halve it, while
 * the slot's flag 0x40 doubles it again. Unknown statuses are ignored. */
void func_800995A0(u8 slot, u8 kind, u16 flag, u8 amount) {
    Combatant *record;
    u8 index = 0xF;
    u16 state;

    if (D_800CCCE8.records[D_800C3E04].pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    record = &D_800CCCE8.records[slot];
    if (kind == 0) {
        index = (flag == 0x1000) ? 1 : (flag == 0x2000) ? 0 : 0xF;
    }
    if (kind == 2) {
        switch (flag) {
        case 0x1000:
            index = 2;
            break;
        case 0x800:
            index = 3;
            break;
        }
    }
    if (kind == 5) {
        switch (flag) {
        case 0x8000:
            index = 4;
            break;
        case 0x4000:
            index = 5;
            break;
        }
    }
    if (kind == 7) {
        switch (flag) {
        case 0x8000:
            index = 9;
            break;
        case 0x4000:
            index = 0xA;
            break;
        case 0x1000:
            index = 0xB;
            break;
        }
    }
    if (kind == 9) {
        switch (flag) {
        case 0x1000:
        case 0x2000:
        case 0x4000:
        case 0x8000:
            index = 0xC;
            break;
        case 0x100:
        case 0x200:
        case 0x400:
        case 0x800:
            index = 0xD;
            break;
        }
    }
    if (index == 0xF) {
        return;
    }
    switch (kind) {
    case 5:
    case 7:
    case 9:
        state = D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent;
        if (state & 5) {
            amount *= 2;
        }
        if (state & 0xA) {
            amount /= 2;
        }
        break;
    }
    if (record->pilot.flags32 & 0x40) {
        switch (kind) {
        case 5:
        case 7:
        case 9:
            amount *= 2;
            break;
        }
    }
    D_800CCCE8.records[slot].statusTimers[index] = amount;
}

/* Count down slot's timed statuses at the start of its turn, clearing each one
 * whose timer runs out; a slot in a gear counts down its gear's statuses
 * instead (80099CF0). Returns a mask of the statuses that ended. */
u16 func_80099890(u8 slot) {
    Combatant *record = &D_800CCCE8.records[slot];
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    volatile u8 *timers = D_800CCCE8.records[slot].statusTimers;
    u16 ended;

    if (D_800CCCE8.records[slot].flags15A & 0x80) {
        func_80099CF0(gear, record, timers);
        return 0;
    }
    ended = 0;
    if (record->pilot.status7C & 0x1000) {
        timers[1] += -1;
        if (timers[1] == 0) {
            ended = 0x4000;
            record->pilot.status7C &= ~0x1000;
        }
    }
    if (record->pilot.status80 & 0x1000) {
        timers[2] += -1;
        if (timers[2] == 0) {
            ended |= 0x2000;
            record->pilot.status80 &= ~0x1000;
        }
    }
    if (record->pilot.status80 & 0x800) {
        timers[3] += -1;
        if (timers[3] == 0) {
            ended |= 0x1000;
            record->pilot.status80 &= ~0x800;
            record->pilot.status7A &= ~0x20;
        }
    }
    if ((record->pilot.status84.word & 0x80008000) == 0x8000) {
        timers[4] += -1;
        if (timers[4] == 0) {
            ended |= 0x800;
            record->pilot.status84.half.active &= ~0x8000;
        }
    }
    if ((record->pilot.status84.word & 0x40004000) == 0x4000) {
        timers[5] += -1;
        if (timers[5] == 0) {
            ended |= 0x400;
            record->pilot.status84.half.active &= ~0x4000;
        }
    }
    if ((record->pilot.status84.word & 0x20002000) == 0x2000) {
        timers[6] += -1;
        if (timers[6] == 0) {
            ended |= 0x200;
            record->pilot.status84.half.active &= ~0x2000;
        }
    }
    if ((record->pilot.status84.word & 0x10001000) == 0x1000) {
        timers[7] += -1;
        if (timers[7] == 0) {
            ended |= 0x100;
            record->pilot.status84.half.active &= ~0x1000;
        }
    }
    if ((record->pilot.status84.word & 0x08000800) == 0x800) {
        timers[8] += -1;
        if (timers[8] == 0) {
            ended |= 0x80;
            record->pilot.status84.half.active &= ~0x800;
        }
    }
    if ((record->pilot.status88.word & 0x80008000) == 0x8000) {
        timers[9] += -1;
        if (timers[9] == 0) {
            ended |= 0x40;
            record->pilot.status88.half.active &= ~0x8000;
        }
    }
    if ((record->pilot.status88.word & 0x40004000) == 0x4000) {
        timers[10] += -1;
        if (timers[10] == 0) {
            ended |= 0x20;
            record->pilot.status88.half.active &= ~0x4000;
        }
    }
    if ((record->pilot.status88.word & 0x10001000) == 0x1000) {
        timers[11] += -1;
        if (timers[11] == 0) {
            ended |= 0x10;
            record->pilot.status88.half.active &= ~0x1000;
        }
    }
    if ((record->pilot.status8C.half.active & 0xF000) && !(record->pilot.status8C.half.permanent & 0xF000)) {
        timers[12] += -1;
        if (timers[12] == 0) {
            ended |= 8;
            record->pilot.status8C.half.active &= ~0xF000;
        }
    }
    if ((record->pilot.status8C.half.active & 0xF00) && !(record->pilot.status8C.half.permanent & 0xF00)) {
        timers[13] += -1;
        if (timers[13] == 0) {
            ended |= 4;
            record->pilot.status8C.half.active &= ~0xF00;
        }
    }
    return ended;
}

/* Count down the timed statuses of a gear, clearing each one whose timer runs
 * out; the end of gear status 0x20 also ends the pilot's status 0x1000. */
void func_80099CF0(GearRecord *gear, Combatant *record, volatile u8 *timers) {
    if (gear->status7C & 0x200) {
        timers[1] += -1;
        if (timers[1] == 0) {
            gear->status7C &= ~0x200;
        }
    }
    if (gear->status7C & 0x100) {
        timers[2] += -1;
        if (timers[2] == 0) {
            gear->status7C &= ~0x100;
        }
    }
    if (gear->status7C & 0x80) {
        timers[3] += -1;
        if (timers[3] == 0) {
            gear->status7C &= ~0x80;
        }
    }
    if (gear->status7C & 0x20) {
        timers[4] += -1;
        if (timers[4] == 0) {
            gear->status7C &= ~0x20;
            record->pilot.status7C &= ~0x1000;
        }
    }
    if (gear->status7C & 0x10) {
        timers[5] += -1;
        if (timers[5] == 0) {
            gear->status7C &= ~0x10;
        }
    }
    if (gear->status7C & 0xF000) {
        timers[7] += -1;
        if (timers[7] == 0) {
            gear->status7C &= ~0xF000;
        }
    }
    if (gear->status7C & 0xF00) {
        timers[8] += -1;
        if (timers[8] == 0) {
            gear->status7C &= ~0xF00;
        }
    }
    if (gear->status80 & 0x1000) {
        timers[10] += -1;
        if (timers[10] == 0) {
            gear->status80 &= ~0x1000;
        }
    }
    if (gear->status80 & 0x40) {
        timers[11] += -1;
        if (timers[11] == 0) {
            gear->status80 &= ~0x40;
        }
    }
    if (gear->status80 & 0x20) {
        timers[12] += -1;
        if (timers[12] == 0) {
            gear->status80 &= ~0x20;
        }
    }
}

/* Make the current command descriptor the battle's current command: copy its
 * attribute bytes and index, and give a command without an element the
 * attacker's element statuses. */
void func_80099FB0(void) {
    u16 elements = (D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) >> 12;

    D_800C34B0->commandAttributes[0] = D_800C3DFC->attributes[0];
    D_800C34B0->commandAttributes[1] = D_800C3DFC->attributes[1];
    D_800C34B0->commandAttributes[2] = D_800C3DFC->attributes[2];
    D_800C34B0->commandAttributes[3] = D_800C3DFC->attributes[3];
    D_800C34B0->commandIndexCopy = D_800C34B0->commandIndex;
    if ((D_800C34B0->commandAttributes[2] & 0x3F) == 0) {
        D_800C34B0->commandAttributes[2] |= elements;
    }
}

/* Restrict the target mask to the allowed targets of its side: the low three
 * bits (party) and bits 3-10 (enemies). */
void func_8009A074(void) {
    if (D_800C34B0->targetMask & 7) {
        D_800C34B0->targetMask = D_800C34B0->targetMask2 & 7;
    }
    if (D_800C34B0->targetMask & 0x7F8) {
        D_800C34B0->targetMask = D_800C34B0->targetMask2 & 0x7F8;
    }
}

/* The condition shown for slot, by priority: 8, 1, 2 for status bits 0x4000,
 * 0x8000, 0x2000; 3 and 4 for the second word's 0x1000 and 0x2000; 5, 6, 3 for
 * 0x800, 0x1000, 2; 7 for status pair 0x84 bit 0x8000; 5 below an eighth of
 * the maximum HP; otherwise 0. */
s32 func_8009A0DC(u8 slot) {
    Combatant *record = &D_800C34B0->records[slot];

    if (record->pilot.status7C & 0x4000) {
        return 8;
    }
    if (record->pilot.status7C & 0x8000) {
        return 1;
    }
    if (record->pilot.status7C & 0x2000) {
        return 2;
    }
    if (record->pilot.status80 & 0x1000) {
        return 3;
    }
    if (record->pilot.status80 & 0x2000) {
        return 4;
    }
    if (record->pilot.status7C & 0x800) {
        return 5;
    }
    if (record->pilot.status7C & 0x1000) {
        return 6;
    }
    if (record->pilot.status7C & 2) {
        return 3;
    }
    if ((record->pilot.status84.half.active | record->pilot.status84.half.permanent) & 0x8000) {
        return 7;
    }
    if (record->pilot.hp < record->pilot.maxHp >> 3) {
        return 5;
    }
    return 0;
}

/* Mask of slot's timed conditions for display (0 once status bit 0x8000 is
 * set): 0x8000, 0x4000 and 0x2000 for three statuses, plus the element
 * statuses (pair 0x8C bits 8-15) shifted down by three. */
u16 func_8009A1AC(u8 slot) {
    Combatant *record = &D_800C34B0->records[slot];
    u16 mask = 0;
    u16 elements;

    if (record->pilot.status7C & 0x8000) {
        return 0;
    }
    if (record->pilot.status80 & 0x1000) {
        mask = 0x8000;
    }
    if (record->pilot.status80 & 0x2000) {
        mask |= 0x4000;
    }
    if (record->pilot.status7C & 0x800) {
        mask |= 0x2000;
    }
    elements = record->pilot.status8C.half.active | record->pilot.status8C.half.permanent;
    if (elements & 0xF00) {
        mask |= (elements & 0xF00) >> 3;
    }
    if (elements & 0xF000) {
        mask |= (elements & 0xF000) >> 3;
    }
    return mask;
}

/* Accuracy of member's command: the descriptor's accuracy plus the member's
 * bonus, capped at 100. */
u8 func_8009A258(u8 member, u8 command) {
    CommandDescriptor *descriptor = &D_800C34B0->partyCommands[member][command];
    u8 accuracy = descriptor->accuracy + (D_800C34B0->records + member)->pilot.accuracy;

    if (accuracy > 100) {
        accuracy = 100;
    }
    return accuracy;
}

/* Fill the gear HUD for member: its first commands' states, charge rate,
 * attack, defense, the chance of a boost (from the gear's damage, when the
 * boost flag of D_8006F8EA is on), warning bits and overheat; count down an
 * active boost (level 4) or, at level 3, try to start one. */
void func_8009A2D4(u8 member) {
    Combatant *record = &D_800CCCE8.records[member];
    GearRecord *gear = &D_800CCCE8.records[member].gear;
    u8 *level = &D_800CCCE8.records[member].field148;
    CommandDescriptor *commands = D_800CCCE8.gearCommands[member];
    GearHud *hud = &D_800CCCE8.gearHud;
    u8 i;
    u8 chance;

    for (i = 0; i < 15; i++) {
        hud->commands[i] = commands->hudState;
        commands++;
    }
    if (gear->chargeRate != 0) {
        hud->charge = gear->chargeRate * hud->commands[0];
    } else {
        hud->charge = 30;
    }
    switch (*level) {
    case 1:
    case 2:
    case 3:
        hud->charge += *level * 20;
        break;
    case 4:
        hud->charge *= 10;
        break;
    }
    hud->attack = gear->attack * gear->attackScale;
    hud->attack = gear->entries[0].valueE + hud->attack;
    if (record->pilot.characterId == 4) {
        hud->attack = gear->entries[2].valueE + hud->attack;
    }
    hud->field29 = gear->speed;
    hud->defense = gear->defense;
    if (gear->maxHp != gear->hp) {
        chance = (gear->maxHp - gear->hp) / (gear->maxHp / 10);
        if (chance == 0) {
            chance++;
        }
    } else {
        chance++; /* uninitialised in the original */
    }
    chance *= record->pilot.field54 + 5;
    if (!(*(u16 *)D_8006F8EA & 0x4000)) {
        chance = 0;
    }
    if (record->pilot.field62 < 50) {
        chance = 0;
    }
    if (record->pilot.gearId == 3) {
        chance = 0;
    }
    if (record->pilot.gearId == 15) {
        chance = 99;
    }
    if (chance >= 100) {
        chance = 99;
    }
    hud->boostChance = chance;
    hud->status = 0;
    if (gear->status7C & 0x100) {
        hud->status = 0x8000;
    }
    if (gear->status7C & 0x200) {
        hud->status |= 0x4000;
    }
    if (gear->status7C & 0x80) {
        hud->status |= 0x2000;
    }
    if (gear->status7C & 0x10) {
        hud->status |= 0x1000;
    }
    if (gear->fuel < gear->maxFuel >> 3) {
        hud->status |= 0x800;
    } else {
        hud->status &= ~0x800;
    }
    if (gear->fuel == 0) {
        gear->status80 &= ~0x8000;
        record->pilot.status84.half.active &= ~0x8000;
    }
    if (gear->status80 & 0x8000) {
        hud->overheat = 1;
    } else {
        hud->overheat = 0;
    }
    if (*level == 4) {
        hud->level = 4;
        if (--D_800CCCE8.records[member].statusTimers[6] == 0) {
            gear->status80 &= ~0x4000;
            *level = 0;
            record->pilot.field54 = 0;
        }
    } else {
        hud->level = *level;
        if (*level == 3 && (*(u16 *)D_8006F8EA & 0x4000) && rand() % 100 < chance) {
            gear->status80 |= 0x4000;
            D_800CCCE8.records[member].statusTimers[6] = 3;
            if (D_800CCCE8.records[member].pilot.flags32 & 0x40) {
                D_800CCCE8.records[member].statusTimers[6] = 6;
            }
            (*level)++;
        }
    }
}

/* Byte 5 of game-data unit record id. */
u8 func_8009A7B8(u8 id) {
    return D_8006D8A0.characters[id].entries[0].pad5;
}

/* Whether gear item index is one of character 4's four entries. */
s32 func_8009A7E4(u8 index) {
    BattleItem *item = &D_800C34B0->lists.items.members[index];

    if (D_8006D8A0.characters[4].entries[0].id == item->id
        || D_8006D8A0.characters[4].entries[1].id == item->id
        || D_8006D8A0.characters[4].entries[2].id == item->id) {
        return 1;
    }
    return D_8006D8A0.characters[4].entries[3].id == item->id;
}

/* Put battle item index into character 4's entry holding its id (entry k when
 * none does): copy its values, record the item slot and durability, and update
 * the battle copies of character 4. */
void func_8009A854(u8 index, u8 k) {
    BattleItem *item = &D_800C34B0->lists.items.list[index];
    u8 i;

    if (D_8006D8A0.characters[4].entries[0].id == item->id) {
        k = 0;
    }
    if (D_8006D8A0.characters[4].entries[1].id == item->id) {
        k = 1;
    }
    if (D_8006D8A0.characters[4].entries[2].id == item->id) {
        k = 2;
    }
    if (D_8006D8A0.characters[4].entries[3].id == item->id) {
        k = 3;
    }
    D_8006D8A0.characters[4].entries[k].value4 = item->valueC;
    D_8006D8A0.characters[4].entries[k].value3 = item->valueB;
    D_8006D8A0.characters[4].entries[k].value2 = item->valueA;
    D_8006D8A0.characters[4].entries[k].value3 = item->valueB;
    D_8006D8A0.characters[4].entryItems[k] = index;
    D_8006F8BA[index] = item->durability;
    D_8006D8A0.characters[4].entryItems[k] = index;
    for (i = 0; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];

        if (record->pilot.characterId == 4) {
            record->pilot.entries[k].value4 = item->valueC;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entries[k].value2 = item->valueA;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entryItems[k] = index;
        }
    }
}

/* The Escape command: succeeds on half of the rolls, writing the party back
 * to the game data (8009BE0C). */
s32 func_8009A9D0(void) {
    D_800C34B0->commandIndex = 0;
    if (rand() % 100 < 50) {
        func_8009BE0C();
        return 1;
    }
    return 0;
}

/* The Defense command for member: mark it defending; a gear with status 0x10
 * in its second word drops its gear statuses 0x1B0 and the pilot's 0x1000. */
void func_8009AA44(u8 member) {
    D_800C34B0->commandIndex = 0;
    D_800C34B0->records[member].flags15A |= 1;
    if ((D_800C34B0->records[member].flags15A & 0x80) && (D_800C34B0->records[member].gear.status82 & 0x10)) {
        D_800C34B0->records[member].gear.status7C &= 0xFE4F;
        D_800C34B0->records[member].pilot.status7C &= ~0x1000;
    }
    if (D_800D2C34 == 4) {
        D_800C34B0->message = 0x3D;
    }
}

/* End member's defending. */
void func_8009AB00(u8 member) {
    D_800C34B0->records[member].flags15A &= ~1;
}

/* Enable member's Deathblow commands (descriptors 22 and 24-32; in a gear, gear
 * descriptors 21 and 23-28) when its command flag is set. */
void func_8009AB38(u8 member) {
    s32 flag;

    if (D_800C34B0->records[member].flags15A & 0x80) {
        flag = D_800C34B0->records[member].gear.status80 & 0x2000;
    } else {
        flag = D_800C34B0->records[member].pilot.status88.half.active & 0x400;
    }
    if (flag) {
        if (D_800C34B0->records[member].flags15A & 0x80) {
            CommandDescriptor *commands = D_800C34B0->gearCommands[member];

            commands[21].state = 1;
            commands[23].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
        } else {
            CommandDescriptor *commands = D_800C34B0->partyCommands[member];

            commands[22].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
            commands[29].state = 1;
            commands[30].state = 1;
            commands[31].state = 1;
            commands[32].state = 1;
        }
    }
}

/* Seal the same commands again and clear member's command flag; unless
 * checked is 0, only for a current command with flag 0x100. */
void func_8009AC48(u8 member, u8 checked) {
    s32 flag;

    if (checked == 0 || (D_800C3DFC->flagsA & 0x100)) {
        if (D_800C34B0->records[member].flags15A & 0x80) {
            flag = D_800C34B0->records[member].gear.status80 & 0x2000;
        } else {
            flag = D_800C34B0->records[member].pilot.status88.half.active & 0x400;
        }
        if (flag) {
            if (D_800C34B0->records[member].flags15A & 0x80) {
                CommandDescriptor *commands = D_800C34B0->gearCommands[member];

                commands[21].state = 0x2000;
                commands[23].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                D_800C34B0->records[member].gear.status80 &= ~0x2000;
            } else {
                CommandDescriptor *commands = D_800C34B0->partyCommands[member];

                commands[22].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                commands[29].state = 0x2000;
                commands[30].state = 0x2000;
                commands[31].state = 0x2000;
                commands[32].state = 0x2000;
                D_800C34B0->records[member].pilot.status88.half.active &= ~0x400;
            }
        }
    }
}

/* Regeneration amounts of slot for its turn: HP (maxHp / 20), EP (maxEp / 20,
 * from the pilot's or the gear's status) and fuel (maxFuel / 50 for each of
 * two gear statuses). Returns whether any applies; nothing once KO'd. */
s32 func_8009ADA0(u8 slot, s32 *amounts) {
    Combatant *record = &D_800C34B0->records[slot];
    s32 any = 0;

    if (record->pilot.status7C & 0x8000) {
        return 0;
    }
    if (record->pilot.status7C & 0x800) {
        any = 1;
        *amounts = (u16)(record->pilot.maxHp / 20);
    }
    amounts++;
    if (record->pilot.status80 & 0x200) {
        any = 1;
        *amounts = (u16)(record->pilot.maxEp / 20);
    }
    if (record->gear.status7C & 0x200) {
        any = 1;
        *amounts = (u16)(record->pilot.maxEp / 20);
    }
    amounts++;
    *amounts = 0;
    if (record->gear.status7C & 0x80) {
        any = 1;
        *amounts = (u16)(record->gear.maxFuel / 50);
    }
    if (record->gear.status80 & 0x8000) {
        any = 1;
        *amounts += (u16)(record->gear.maxFuel / 50);
    }
    return any;
}

/* Revive slot's record: clear its statuses (keeping status 0x2000 of the
 * second word), size Chu-Chu's gear HP (8009B104), and when character 3 is
 * revived clear status 0x20 of every enemy. */
void func_8009AEFC(u8 slot) {
    Combatant *record;
    u8 i;

    D_800C34B0->commandIndex = 0;
    record = &D_800C34B0->records[slot];
    record->pilot.status7C = 0;
    record->pilot.status84.half.active = 0;
    record->pilot.status88.half.active = 0;
    record->pilot.status8C.half.active = 0;
    record->pilot.status80 &= 0x2000;
    if (record->pilot.characterId == 7) {
        func_8009B104(slot, record);
    }
    if (record->pilot.characterId == 3) {
        for (i = 3; i < 11; i++) {
            D_800CCCE8.records[i].pilot.status80 &= ~0x20;
        }
    }
}

/* Wear the attacker's weapon items for the current command: commands 0-3 the
 * first entry's, 6 the fourth's, 7-19 both. */
void func_8009AFD8(void) {
    switch (D_800C34B0->commandIndex) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[0]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[0]] += -1;
        }
        break;
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[0]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[0]] += -1;
        }
        /* fallthrough */
    case 6:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[3]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[3]] += -1;
        }
        break;
    }
}

/* Debug party: members 1 and 2 get 100/100 HP and fixed stats. */
void func_8009B098(void) {
    u8 i;

    for (i = 1; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];

        record->pilot.hp = 100;
        record->pilot.maxHp = 100;
        record->pilot.field5E = 20;
        record->pilot.field5F = 15;
        record->pilot.status7A = 0x1FBF;
        record->field149 = 0;
    }
}

/* Chu-Chu's gear HP: fifty times her HP and maximum HP, capped at 99999. */
void func_8009B104(u8 slot, Combatant *chuchu) {
    Combatant *record = &D_800C34B0->records[slot];
    GearRecord *gear = &record->gear;

    if (chuchu->pilot.maxHp * 50 > 99999) {
        if (chuchu->pilot.hp * 50 > 99999) {
            record->gear.hp = 99999;
        } else {
            record->gear.hp = chuchu->pilot.hp * 50;
        }
        gear->maxHp = 99999;
    } else {
        record->gear.hp = chuchu->pilot.hp * 50;
        record->gear.maxHp = chuchu->pilot.maxHp * 50;
    }
}

/* Debug setup: 99 of every item 1-47, and fixed skill masks for all eleven
 * characters. */
void func_8009B1E4(void) {
    u8 i;

    for (i = 1; i < 48; i++) {
        D_8006F65A[i] = i;
        D_8006F5C4[i] = 99;
    }
    D_8006ECF4[0].mask0 = 0xFFF8;
    D_8006ECF4[0].mask2 = 0xFF00;
    D_8006ECF4[0].mask4 = 0xFFFF;
    D_8006ECF4[0].mask6 = 0xFE00;
    D_8006ECF4[0].flags1A = 0xF000;
    D_8006ECF4[0].field17 = 7;
    D_8006ECF4[1].mask0 = 0xFFE0;
    D_8006ECF4[1].mask2 = 0xFFF0;
    D_8006ECF4[1].mask4 = 0xFFFF;
    D_8006ECF4[1].mask6 = 0xFFF0;
    D_8006ECF4[1].flags1A = 0xC000;
    D_8006ECF4[1].field17 = 7;
    D_8006ECF4[2].mask0 = 0xFFE0;
    D_8006ECF4[2].mask2 = 0xFFE0;
    D_8006ECF4[2].mask4 = 0xFFFF;
    D_8006ECF4[2].mask6 = 0xFF00;
    D_8006ECF4[2].flags1A = 0x8000;
    D_8006ECF4[2].field17 = 7;
    D_8006ECF4[3].mask0 = 0xFFE0;
    D_8006ECF4[3].mask2 = 0xFFC0;
    D_8006ECF4[3].mask4 = 0xFFFF;
    D_8006ECF4[3].mask6 = 0xFF00;
    D_8006ECF4[3].flags1A = 0xF000;
    D_8006ECF4[3].field17 = 7;
    D_8006ECF4[4].mask0 = 0xFFC0;
    D_8006ECF4[4].mask2 = 0xFFC0;
    D_8006ECF4[4].mask4 = 0xFFFF;
    D_8006ECF4[4].mask6 = 0xFC00;
    D_8006ECF4[4].flags1A = 0xE000;
    D_8006ECF4[4].field17 = 7;
    D_8006ECF4[5].mask0 = 0xFFC0;
    D_8006ECF4[5].mask2 = 0xF000;
    D_8006ECF4[5].mask4 = 0xFFFF;
    D_8006ECF4[5].mask6 = 0xF000;
    D_8006ECF4[5].flags1A = 0x8000;
    D_8006ECF4[5].field17 = 7;
    D_8006ECF4[6].mask0 = 0xFFC0;
    D_8006ECF4[6].mask2 = 0xFF00;
    D_8006ECF4[6].mask4 = 0xFFFF;
    D_8006ECF4[6].mask6 = 0xFF00;
    D_8006ECF4[6].flags1A = 0x8000;
    D_8006ECF4[6].field17 = 7;
    D_8006ECF4[7].mask0 = 0;
    D_8006ECF4[7].mask2 = 0xFF00;
    D_8006ECF4[7].mask4 = 0;
    D_8006ECF4[7].mask6 = 0xFF00;
    D_8006ECF4[7].flags1A = 0;
    D_8006ECF4[7].field17 = 7;
    D_8006ECF4[8].mask0 = 0;
    D_8006ECF4[8].mask2 = 0xF800;
    D_8006ECF4[8].mask4 = 0xFFFF;
    D_8006ECF4[8].mask6 = 0;
    D_8006ECF4[8].flags1A = 0xE000;
    D_8006ECF4[8].field17 = 7;
    D_8006ECF4[9].mask0 = 0xFFE0;
    D_8006ECF4[9].mask2 = 0xFFE0;
    D_8006ECF4[9].mask4 = 0xFFFF;
    D_8006ECF4[9].mask6 = 0xFFE0;
    D_8006ECF4[9].flags1A = 0x8000;
    D_8006ECF4[9].field17 = 7;
    D_8006ECF4[10].mask0 = 0xFFC0;
    D_8006ECF4[10].mask2 = 0xFF00;
    D_8006ECF4[10].mask4 = 0xFFFF;
    D_8006ECF4[10].mask6 = 0xFF00;
    D_8006ECF4[10].flags1A = 0xC000;
    D_8006ECF4[10].field17 = 7;
}

#ifdef NON_MATCHING
/* Raise an attack's damage: by half for each of characters 0 and 3 in the
 * party below half HP (gear HP in a gear) and again below a quarter; then a
 * critical chance (10%, 60% with attacker flag 0x200) multiplies it by 1.5
 * (2 with attacker flag 0x400). */
void func_8009B46C(u16 *damage) {
    u8 count = 0;
    u8 i;
    u32 hp;
    u32 maxHp;
    s32 chance;
    s32 scale;

    for (i = 0; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];
        GearRecord *gear = &record->gear;

        if (record->pilot.characterId == 0) {
            if (D_800CCCE8.records[i].flags15A & 0x80) {
                maxHp = record->gear.maxHp;
                hp = record->gear.hp;
            } else {
                maxHp = record->pilot.maxHp;
                hp = record->pilot.hp;
            }
            if (hp < maxHp >> 1) {
                count++;
            }
            if (hp < maxHp >> 2) {
                count++;
            }
        }
        if (record->pilot.characterId == 3) {
            if (D_800CCCE8.records[i].flags15A & 0x80) {
                maxHp = gear->maxHp;
                hp = gear->hp;
            } else {
                maxHp = record->pilot.maxHp;
                hp = record->pilot.hp;
            }
            if (hp < maxHp >> 1) {
                count++;
            }
            if (hp < maxHp >> 2) {
                count++;
            }
        }
    }
    chance = 10;
    if (count) {
        *damage += count * (*damage >> 1);
    }
    scale = 3;
    if (D_800C3E00->pilot.flags32 & 0x400) {
        scale = 4;
    }
    if (D_800C3E00->pilot.flags32 & 0x200) {
        chance = 60;
    }
    if (rand() % 100 < chance) {
        *damage = scale * *damage >> 1;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B46C);
#endif

/* Show the message for an applied status, named by its kind and flag bit. */
void func_8009B684(u8 kind, u16 flag) {
    switch (kind) {
    case 0:
        switch (flag) {
        case 0x2000:
            D_800C34B0->message = 0x1;
            break;
        case 0x1000:
            D_800C34B0->message = 0x2;
            break;
        case 0x800:
            D_800C34B0->message = 0x3;
            break;
        case 0x400:
            D_800C34B0->message = 0x4;
            break;
        case 0x200:
            D_800C34B0->message = 0x5;
            break;
        case 0x1:
            D_800C34B0->message = 0x8;
            break;
        }
        break;
    case 2:
        switch (flag) {
        case 0x2000:
            D_800C34B0->message = 0x9;
            break;
        case 0x1000:
            D_800C34B0->message = 0xA;
            break;
        case 0x800:
            D_800C34B0->message = 0xB;
            break;
        case 0x400:
            D_800C34B0->message = 0xC;
            break;
        case 0x1:
            D_800C34B0->message = 0xD;
            break;
        case 0x20:
            D_800C34B0->message = 0x7;
            break;
        }
        break;
    case 5:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0xE;
            break;
        case 0x4000:
            D_800C34B0->message = 0xF;
            break;
        case 0x2000:
            D_800C34B0->message = 0x10;
            break;
        case 0x1000:
            D_800C34B0->message = 0x11;
            break;
        case 0x800:
            D_800C34B0->message = 0x12;
            break;
        case 0x1800:
            D_800C34B0->message = 0x13;
            break;
        }
        break;
    case 7:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x15;
            break;
        case 0x4000:
            D_800C34B0->message = 0x16;
            break;
        case 0x1000:
            D_800C34B0->message = 0x18;
            break;
        case 0x2:
        case 0x8:
            D_800C34B0->message = 0x19;
            break;
        case 0x1:
        case 0x4:
            D_800C34B0->message = 0x1A;
            break;
        }
        break;
    case 9:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x1B;
            break;
        case 0x4000:
            D_800C34B0->message = 0x1C;
            break;
        case 0x2000:
            D_800C34B0->message = 0x1D;
            break;
        case 0x1000:
            D_800C34B0->message = 0x1E;
            break;
        case 0x400:
            D_800C34B0->message = 0x1F;
            break;
        case 0x800:
            D_800C34B0->message = 0x20;
            break;
        case 0x100:
            D_800C34B0->message = 0x21;
            break;
        case 0x200:
            D_800C34B0->message = 0x22;
            break;
        }
        break;
    }
}

/* Choose an automatic action for slot (confusion or auto-battle): choice[0]
 * is the kind (4 defend, 2 a skill with choice[1] its index among the
 * character's usable skills, 0/1 an attack with choice[1] its strength). Out of
 * a gear: defend on 10% unless flagged, a known skill on 25%, then Chu-Chu's
 * basic attack; otherwise attack weak 48%, medium 32%, strong 20%. */
void func_8009BAC4(u8 slot, u8 *choice, s16 *busy) {
    Combatant *record = &D_800CCCE8.records[slot];
    u16 skills;
    u8 count;
    u8 skill;

    if (!(D_800CCCE8.records[slot].flags15A & 0x80)) {
        if (rand() % 100 < 10 && !(record->pilot.status7A & 0x100) && *busy == 0) {
            choice[0] = 4;
            return;
        }
        switch (record->pilot.characterId) {
        case 3:
            skills = 0xC3C0;
            count = 10;
            break;
        case 4:
            skills = 0xDF80;
            count = 10;
            break;
        case 5:
            skills = 0x1000;
            count = 4;
            break;
        case 7:
            skills = 0xE000;
            count = 3;
            break;
        case 0:
        case 8:
            skills = 0xC000;
            count = 2;
            break;
        case 2:
        case 9:
            skills = 0xBFE0;
            count = 11;
            break;
        case 1:
        case 6:
        case 10:
            skills = 0xF000;
            count = 4;
            break;
        }
        if (rand() % 100 < 25 && !(record->pilot.status7A & 0x20)) {
            choice[0] = 2;
            skill = rand() % count;
            if (D_8006ECF4[record->pilot.characterId].mask2 & ((0x8000 >> skill) & skills)) {
                choice[1] = skill;
                return;
            }
        }
        if (record->pilot.characterId == 8) {
            choice[0] = 0;
            choice[1] = 0;
            return;
        }
    }
    choice[0] = 1;
    if (rand() % 100 < 80) {
        if (rand() % 100 >= 60) {
            choice[1] = 1;
        } else {
            choice[1] = 0;
        }
    } else {
        choice[1] = 2;
    }
}

/* Unit note: a new translation unit starts after 8009DBFC and at or before
 * 8009E788. GCC aligns jump tables to 8 within a unit's rodata; the tables up
 * to 8009BAC4's (0x800704A0, ending at 0x800704CC) sit at 0 mod 8, those from
 * 8009E788's (0x800704CC) at 4 mod 8, so that unit's rodata starts at
 * 0x800704CC. Its code divides with ASPSX's checked division (break 7/6,
 * 8009F1C4 through 800B10EC), the code up to 8009DBFC without. */
/* Damage the target by the command's power in twentieths of its gear's
 * maximum HP. */
void func_8009BD94(void) {
    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = D_800C3DFC->power * D_800D2DC8->maxHp / 20;
}

/* Write the party back to the game data: each present member's HP (Chu-Chu in
 * a gear takes hers from the gear's HP), EP, use counts and field 0x3A, with
 * HP 1 when KO'd, and the HP and fuel of its gear (ids 0-6 and 8-16), a tenth
 * of the maximum when the gear is wrecked. */
void func_8009BE0C(void) {
    u8 i;
    u8 j;
    s32 unused[2]; /* never used; it gives the original its 8-byte frame */

    for (i = 0; i < 3; i++) {
        Combatant *record;
        CharacterRecord *character;
        GearRecord *gearRecord;
        GearRecord *gear;

        if (D_800D2D24[i] == 0x7F) {
            continue;
        }
        record = &D_800CCCE8.records[i];
        character = &D_8006D8A0.characters[record->pilot.characterId];
        gear = &D_800CCCE8.records[i].gear;
        gearRecord = &D_8006D8A0.gears[record->pilot.gearId];
        if (record->pilot.characterId == 7 && (D_800CCCE8.records[i].flags15A & 0x80)) {
            record->pilot.hp = (gear->hp + 1) / 50;
            if (record->pilot.hp == 0) {
                record->pilot.hp = 1;
            }
        }
        character->hp = record->pilot.hp;
        character->ep = record->pilot.ep;
        if (character->hp > character->maxHp) {
            character->hp = character->maxHp;
        }
        if (character->ep > character->maxEp) {
            character->ep = character->maxEp;
        }
        for (j = 0; j < 7; j++) {
            character->useCounts[j] = record->pilot.useCounts[j];
        }
        character->field3A = record->pilot.field3A;
        if (record->pilot.status7C & 0xC000) {
            character->hp = 1;
        }
        switch (record->pilot.gearId) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            gearRecord->hp = gear->hp;
            gearRecord->fuel = gear->fuel;
            if (gearRecord->hp > gearRecord->maxHp) {
                gearRecord->hp = gearRecord->maxHp;
            }
            if (gearRecord->fuel > gearRecord->maxFuel) {
                gearRecord->fuel = gearRecord->maxFuel;
            }
            if (gear->status7C & 0x8000) {
                gearRecord->hp = gearRecord->maxHp / 10;
            }
            break;
        }
    }
}

/* Gear warning flags of slot: 1 gear status 0x400, 2 gear HP below an eighth
 * (unless the pilot's flag 1 at +0x36), 4 when field 0x148 is 4. */
s32 func_8009C050(u8 slot) {
    Combatant *record = &D_800CCCE8.records[slot];
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    u8 *state = &D_800CCCE8.records[slot].field148;
    u8 flags = 0;

    if (gear->status7C & 0x400) {
        flags = 1;
    }

    if (gear->hp < gear->maxHp >> 3 && !(record->pilot.flags36 & 1)) {
        flags |= 2;
    }
    if (*state == 4) {
        flags |= 4;
    }
    return flags;
}

/* Put slot into state 4 with timer 6 at three turns, and set flag 0x4000 of
 * character 0. */
void func_8009C0E0(u8 slot) {
    D_800CCCE8.records[slot].field148 = 4;
    D_800CCCE8.records[slot].statusTimers[6] = 3;
    D_8006ECF4[0].flags1A |= 0x4000;
}

/* Target the party member that is character 3. */
void func_8009C134(void) {
    u8 i;

    for (i = 0; i < 3; i++) {
        if (D_800CCCE8.records[i].pilot.characterId == 3) {
            D_800CCCE8.targetMask = 1 << i;
        }
    }
}

/* Resolve a gear's action against every target in the target mask: select the
 * current command descriptor, run its gear formula (table D_800C34DC) per
 * target with the status step it asks for (8009DBFC), then the per-target
 * follow-ups and the command's wear and element. */
void func_8009C198(void) {
    s32 bit;

    if (D_800C3E04 < 3) {
        D_800C3DFC = &D_800C34B0->gearCommands[D_800C34B0->attackerIndex][D_800C34B0->commandIndex];
    } else {
        D_800C3DFC = &D_800C34B0->enemyCommands[D_800C34B0->commandIndex];
    }
    /* Called without its second argument (an unprototyped call). */
    ((void (*)())func_8009AC48)(D_800C3E04);
    D_800C3D3C = &D_800C34B0->records[D_800C34B0->attackerIndex].field148;
    D_800D2DC4 = 0;
    if ((D_800C3DFC->flagsA & 0x100) && D_800C3E00->pilot.characterId == 1) {
        func_80096824();
    }
    if ((D_800C3E00->pilot.gearId == 5 || D_800C3E00->pilot.gearId == 13) && D_800C34B0->commandIndex == 1) {
        D_800C3DFC->attributes[2] = D_800D2D6C->entries[1].valueE;
    }
    bit = 1;
    for (D_800C3E50 = 0; D_800C3E50 < 11; D_800C3E50++, bit <<= 1) {
        if (bit & D_800C34B0->targetMask) {
            D_800C3E34 = &D_800C34B0->records[D_800C3E50];
            D_800D2DC8 = &D_800C34B0->records[D_800C3E50].gear;
            D_800C3D60 = &D_800C34B0->records[D_800C3E50].field148;
            func_8009CA90();
            D_800C34DC[D_800C3DFC->formula]();
            if (D_800C34B0->resultCode[D_800C3E50] == 0) {
                if (D_800C3DFC->flagsA & 0x800) {
                    func_8009DBFC(1);
                } else if (D_800C3DFC->flagsA & 0x4000) {
                    func_8009DBFC(0);
                }
            }
            D_800C34B0->shownCommand = D_800C3DFC->name;
            func_8009C4B4();
            func_8009CB68(D_800C3E50);
        }
    }
    if (D_800C3E00->pilot.characterId == 4) {
        func_8009E788();
    }
    func_8009C9C4();
}

/* Per-target follow-up of a gear action: a party gear's attack level (up by
 * one with commands 0-2, set below D_800D2C34 by the level-3/6/9 command
 * groups, which also raise the pilot's field 0x54), then the target's
 * reactions: breaking status 0x2000 on 80%, fuel-drain immunity, halving or
 * raising damage by its flag 0x80, reflecting it with flag 0x20, and nullifying
 * with status 0x200. */
void func_8009C4B4(void) {
    s32 chance;

    if (D_800C3E04 < 3) {
        if (D_800C34B0->commandIndex < 3) {
            if (++D_800C3D3C[0] > D_800C3D3C[1]) {
                D_800C3D3C[0]--;
            }
        }
        if ((u32)(D_800C34B0->commandIndex - 3) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 1;
        }
        if ((u32)(D_800C34B0->commandIndex - 6) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 2;
        }
        if ((u32)(D_800C34B0->commandIndex - 9) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 3;
        }
        if (D_800C34B0->commandIndex >= 3 && D_800C34B0->commandIndex < 12) {
            D_800C3E00->pilot.field54 += D_800C34B0->commandIndex / 3;
        }
        if (D_800C34B0->commandIndex == 5) {
            D_800C3E00->pilot.field54 += 1;
        }
        if (D_800C34B0->commandIndex == 8) {
            D_800C3E00->pilot.field54 += 2;
        }
        if (D_800C34B0->commandIndex == 11) {
            D_800C3E00->pilot.field54 += 3;
        }
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 0 && (D_800C3E34->pilot.status80 & 0x2000)
        && rand() % 100 < 80) {
        D_800C3E34->pilot.status80 &= ~0x2000;
        D_800D2DC8->status7C &= ~0x1000;
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 10 && (D_800D2DC8->field7E & 0x80)) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 0) {
        if (D_800C3E34->pilot.flags32 & 0x80) {
            chance = 60;
            if (D_800C3E34->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                D_800C34B0->damage[D_800C3E50] >>= 1;
            } else {
                D_800C34B0->damage[D_800C3E50] += D_800C34B0->damage[D_800C3E50] >> 1;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x20) {
            D_800C34B0->resultCode[D_800C3E04] = 0;
            D_800C34B0->damage[D_800C3E04] = D_800C34B0->damage[D_800C3E50];
        }
    }
    if ((D_800C3E34->pilot.status88.half.permanent & 0x200) && D_800C34B0->resultCode[D_800C3E50] == 1) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
}

/* The gear version of 80099FB0: make the current command descriptor the
 * battle's current command, a command without an element taking the gear's
 * element statuses. */
void func_8009C9C4(void) {
    u16 elements = (D_800D2D6C->status84.half.active | D_800C3E00->pilot.status88.half.permanent) >> 12;

    D_800C34B0->commandAttributes[0] = D_800C3DFC->attributes[0];
    D_800C34B0->commandAttributes[1] = D_800C3DFC->attributes[1];
    D_800C34B0->commandAttributes[2] = D_800C3DFC->attributes[2];
    D_800C34B0->commandAttributes[3] = D_800C3DFC->attributes[3];
    D_800C34B0->commandIndexCopy = D_800C34B0->commandIndex;
    if ((D_800C34B0->commandAttributes[2] & 0x3F) == 0) {
        D_800C34B0->commandAttributes[2] |= elements;
    }
}

/* Against a target on foot, switch the current descriptor to its on-foot
 * variant: commands 12-14 three descriptors on, commands 0-2 fifteen. */
void func_8009CA90(void) {
    if ((u32)(D_800C34B0->commandIndex - 12) < 3 && !(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        D_800C3DFC += 3;
    } else if (D_800C34B0->commandIndex < 3 && !(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        D_800C3DFC += 15;
    }
}

/* Mirror the gear's status 0x200 (second word) as the pilot's status 0x20. */
void func_8009CB68(u8 slot) {
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    Combatant *record = &D_800CCCE8.records[slot];

    if (gear->status80 & 0x200) {
        record->pilot.status84.half.active |= 0x20;
    } else {
        record->pilot.status84.half.active &= ~0x20;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CBC4);

/* Mark the target missed (result 6) when 8009DBFC finds no hit. */
void func_8009D354(void) {
    if (func_8009DBFC(0) == 0) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D3A0);

/* Damage of a gear attack (80096FBC); a command with flag 0x100 scales it by
 * the gear's frame factor in quarters (4 when unset or with status 0x100), and
 * status 0x40 adds half again. */
u16 func_8009D948(void) {
    u16 damage = func_80096FBC();
    u8 factor;

    if (D_800C3DFC->flagsA & 0x100) {
        factor = D_800D2D6C->frameFactor;
        if ((D_800D2D6C->status7C & 0x100) || factor == 0) {
            factor = 4;
        }
        damage = factor * damage / 4;
        if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x40) {
            damage += damage >> 1;
        }
    }
    return damage;
}

/* Damage of an attack on a gear (80097610): a command with flag 0x100 adds
 * the target gear's armor (reduced by its defense percentage) when the target
 * is in a gear, and half again with status 0x20; otherwise the damage itself
 * is reduced by the defense percentage. */
u16 func_8009DA04(void) {
    u16 damage = func_80097610();
    u16 armor;
    u8 defense;

    if (D_800C3DFC->flagsA & 0x100) {
        defense = D_800D2DC8->defense;
        armor = D_800D2DC8->armor;
        if (defense != 0 && armor != 0) {
            armor = armor * (100 - defense) / 100;
        }
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            damage += armor;
        }
        if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 0x20) {
            damage += damage >> 1;
        }
    } else if (D_800D2DC8->defense != 0) {
        damage = damage * (100 - D_800D2DC8->defense) / 100;
    }
    return damage;
}

/* Scale damage by the target gear's resistance (in twentieths, 20 or more
 * nullifies) to the command's first element. */
s32 func_8009DB54(s32 damage) {
    u8 i;
    u8 element;
    u8 resistance;

    for (i = 0; i < 16; i++) {
        if (D_800C3DFC->elements & (0x8000 >> i)) {
            element = i;
            break;
        }
    }
    resistance = D_800D2DC8->resistances[element];
    if (resistance != 0) {
        if (resistance < 20) {
            damage = damage * (20 - resistance) / 20;
        } else {
            damage = 0;
        }
    }
    return damage;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DBFC);

/* Clear the target gear's defense percentage. */
void func_8009E268(void) {
    D_800D2DC8->defense = 0;
}

/* Damage the target gear by field 0x4F tenths of its maximum HP. */
void func_8009E278(void) {
    u32 damage = D_800D2DC8->field4F * D_800D2DC8->maxHp / 10;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Damage the target gear by the command's power in twentieths of its maximum
 * HP. */
void func_8009E2EC(void) {
    u32 damage = D_800C3DFC->power * D_800D2DC8->maxHp / 20;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Damage the target by the attacker's accuracy times the command's power. */
void func_8009E364(void) {
    u32 damage = D_800C3E00->pilot.accuracy * D_800C3DFC->power;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Put the target into state 4 with timer 6 at three turns. */
void func_8009E3C8(void) {
    *D_800C3D60 = 4;
    D_800CCCE8.records[D_800C3E50].statusTimers[6] = 3;
}

/* Drain the target gear's fuel (result 10) by the command's power in
 * twentieths of its maximum fuel. */
void func_8009E410(void) {
    s32 amount = D_800D2DC8->maxFuel * D_800C3DFC->power / 20;

    D_800C34B0->resultCode[D_800C3E50] = 10;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Restore the target gear's fuel (result 11) by the command's power in
 * twentieths of its maximum fuel. */
void func_8009E48C(void) {
    s32 amount = D_800D2DC8->maxFuel * D_800C3DFC->power / 20;

    D_800C34B0->resultCode[D_800C3E50] = 11;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Clear the target gear's statuses 0x7F4 and the target's status 0x20. */
void func_8009E508(void) {
    D_800D2DC8->status7C &= 0xF80B;
    D_800C3E34->pilot.status7A &= ~0x20;
}

/* Whether gear part 50 + index is one of the parts of character 4's gear. */
s32 func_8009E53C(u8 index) {
    BattlePart *part = &D_800C34B0->lists.parts.members[index];

    if (D_8006D8A0.gears[D_8006D8A0.characters[4].gearId].entries[0].id == part->id || D_8006D8A0.gears[D_8006D8A0.characters[4].gearId].entries[1].id == part->id) {
        return 1;
    }
    return D_8006D8A0.gears[D_8006D8A0.characters[4].gearId].entries[2].id == part->id;
}

/* Put battle gear part index into character 4's gear entry holding its id
 * (entry k when none does; the third entry's match selects entry 3): copy its
 * values, record the part slot and durability, and update the battle copies of
 * character 4's gear. */
void func_8009E5C8(u8 index, u8 k) {
    BattlePart *part = &D_800C34B0->lists.parts.list[index];
    u8 gearId = D_8006D8A0.characters[4].gearId;
    u8 i;

    if (D_8006D8A0.gears[gearId].entries[0].id == part->id) {
        k = 0;
    }
    if (D_8006D8A0.gears[gearId].entries[1].id == part->id) {
        k = 1;
    }
    if (D_8006D8A0.gears[gearId].entries[2].id == part->id) {
        k = 3;
    }
    D_8006D8A0.gears[gearId].entries[k].valueE = part->valueE;
    D_8006D8A0.gears[gearId].entries[k].value11 = part->value11;
    D_8006D8A0.gears[gearId].entries[k].value10 = part->value10;
    D_8006D8A0.gears[gearId].entries[k].value11 = part->value11;
    D_8006D8A0.gears[gearId].partItems[k] = index;
    D_8006F8BA[index] = part->durability;
    for (i = 0; i < 3; i++) {
        if ((D_800C34B0->records + i)->pilot.characterId == 4) {
            D_800C34B0->records[i].gear.entries[k].valueE = part->valueE;
            D_800C34B0->records[i].gear.entries[k].value11 = part->value11;
            D_800C34B0->records[i].gear.entries[k].value10 = part->value10;
            D_800C34B0->records[i].gear.entries[k].value11 = part->value11;
            D_800C34B0->records[i].gear.partItems[k] = index;
        }
    }
}

#ifdef NON_MATCHING
/* Matches instruction for instruction; its jump table must start a new unit's
 * rodata at 0x800704CC (see the unit note above func_8009BD94).
 * Wear the attacker gear's parts for the current command: command 0 the first
 * part's, 2 and 17 the fourth's, 3-14 both, 15 the first's. */
void func_8009E788(void) {
    switch (D_800C34B0->commandIndex) {
    case 0:
        if (D_8006F8EA[D_800D2D6C->partItems[0]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[0]] += -1;
        }
        break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        if (D_8006F8EA[D_800D2D6C->partItems[0]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[0]] += -1;
        }
        if (D_8006F8EA[D_800D2D6C->partItems[3]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[3]] += -1;
        }
        break;
    case 15:
        if (D_8006F8EA[D_800D2D6C->partItems[0]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[0]] += -1;
        }
        break;
    case 2:
    case 17:
        if (D_8006F8EA[D_800D2D6C->partItems[3]] != 0) {
            D_8006F8EA[D_800D2D6C->partItems[3]] += -1;
        }
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E788);
#endif

/* Show the message for a gear status that was applied, named by its kind and
 * flag bit (the gear counterpart of 8009B684). */
void func_8009E868(u8 kind, u16 flag) {
    switch (kind) {
    case 0:
        switch (flag) {
        case 0x400:
            D_800C34B0->message = 0x24;
            break;
        case 0x200:
            D_800C34B0->message = 0x25;
            break;
        case 0x100:
            D_800C34B0->message = 0x26;
            break;
        case 0x80:
            D_800C34B0->message = 0x27;
            break;
        case 0x40:
            D_800C34B0->message = 0x28;
            break;
        case 0x20:
            D_800C34B0->message = 0x29;
            break;
        case 0x10:
            D_800C34B0->message = 0x2A;
            break;
        case 0x4:
            D_800C34B0->message = 0x2B;
            break;
        }
        break;
    case 1:
        switch (flag) {
        case 0x1000:
            D_800C34B0->message = 0x2D;
            break;
        case 0x800:
            D_800C34B0->message = 0x2E;
            break;
        case 0x400:
            D_800C34B0->message = 0x2F;
            break;
        case 0x40:
            D_800C34B0->message = 0x15;
            break;
        case 0x20:
            D_800C34B0->message = 0x16;
            break;
        case 0x2:
        case 0x8:
            D_800C34B0->message = 0x19;
            break;
        case 0x1:
        case 0x4:
            D_800C34B0->message = 0x1A;
            break;
        }
        break;
    case 3:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x1B;
            break;
        case 0x4000:
            D_800C34B0->message = 0x1C;
            break;
        case 0x2000:
            D_800C34B0->message = 0x1D;
            break;
        case 0x1000:
            D_800C34B0->message = 0x1E;
            break;
        case 0x400:
            D_800C34B0->message = 0x1F;
            break;
        case 0x800:
            D_800C34B0->message = 0x20;
            break;
        case 0x100:
            D_800C34B0->message = 0x21;
            break;
        case 0x200:
            D_800C34B0->message = 0x22;
            break;
        }
        break;
    }
}

/* Relocate a model group and list its models in a new table. */
ModelList *func_8009EBA8(u8 *group, ModelList *list) {
    u32 count;
    u32 i;

    func_80032498(4, 0);
    count = func_8002C3E8(group);
    list->models = func_80031BDC(count * 4, 0);
    list->count = count;
    if (list->models != NULL) {
        for (i = 0; i < count; i++) {
            list->models[i] = (Model *)(group + 0x10 + i * 0x38);
        }
    }
    return list;
}

/* Build a model hierarchy from (model, parent) pairs, up to the first pair
 * naming neither a listed model nor 0xFFFF: a root part, then one part per
 * pair with its packets for both buffers (built with mode; offset by (x0, y0)
 * and (x1, y1) when offset is set). Returns the root, NULL on failure. */
ModelPart *func_8009EC4C(ModelList *list, u16 *hierarchy, s32 mode, s32 offset, u16 x0, u16 y0,
                         u16 x1, u16 y1) {
    ModelPart *root;
    ModelPart *part;
    u16 *pair;
    s32 count;
    s16 index;
    u16 id;
    u16 parent;

    func_80032498(4, 0);
    pair = hierarchy;
    count = 0;
    while (pair[0] < list->count || pair[0] == 0xFFFF) {
        count++;
        pair += 2;
    }
    if (count == 0) {
        return NULL;
    }
    count++;
    root = func_80031BDC(count * sizeof(ModelPart), 0);
    pair = hierarchy;
    if (root == NULL) {
        return NULL;
    }
    part = root + 1;
    index = 1;
    id = pair[0];
    parent = pair[1];
    root->flag4 = 1;
    root->flag5 = 1;
    root->flag6 = 1;
    root->scale[0] = 0x1000;
    root->scale[1] = 0x1000;
    root->scale[2] = 0x1000;
    root->parent = NULL;
    root->flag7 = 0;
    root->modelId = 0xFFFF;
    root->index = count;
    root->packets[0] = NULL;
    root->packets[1] = NULL;
    root->rotation.vx = 0;
    root->rotation.vy = 0;
    root->rotation.vz = 0;
    root->translation[0] = 0;
    root->translation[1] = 0;
    root->translation[2] = 0;
    root->effects[0] = NULL;
    root->effects[1] = NULL;
    root->effects[2] = NULL;
    while (id < list->count || id == 0xFFFF) {
        if (parent == 0xFFFF) {
            part->parent = NULL;
        } else {
            part->parent = root + parent + 1;
        }
        part->index = index;
        index++;
        part->flag4 = 1;
        part->flag5 = 1;
        part->flag7 = 1;
        part->scale[0] = 0x1000;
        part->scale[1] = 0x1000;
        part->scale[2] = 0x1000;
        part->flag6 = 0;
        part->field52 = 0;
        part->modelId = id;
        if (id != 0xFFFF) {
            func_8002CB54(list->models[id], &part->packets[0], &part->packets[1]);
            if (part->packets[0] == NULL) {
                func_8009F708(root);
                return NULL;
            }
            if (offset) {
                func_8002CC10(x0, y0);
                func_8002CC74(x1, y1);
            }
            func_8002C8CC(list->models[id], part->packets[0], mode);
            memcpy(part->packets[1], part->packets[0], list->models[id]->packetSize);
            part->rotation.vx = 0;
        } else {
            part->packets[0] = NULL;
            part->packets[1] = NULL;
            part->rotation.vx = 0;
        }
        part->rotation.vy = 0;
        part->rotation.vz = 0;
        part->translation[0] = 0;
        part->translation[1] = 0;
        part->translation[2] = 0;
        part->effects[0] = NULL;
        part->effects[1] = NULL;
        part->effects[2] = NULL;
        part++;
        pair += 2;
        id = pair[0];
        parent = pair[1];
    }
    return root;
}

/* Pose a model hierarchy: the root's rotation (YXZ order when flag6 is set)
 * with its translation, scaled by scale (4.12) per axis into its transform;
 * each changed part (flag5) its rotation, and each part marked (flag4, or
 * under a marked parent) its translation and world matrix. Clears the marks
 * and returns the part count. */
u16 func_8009EF3C(ModelPart *part, s32 scale) {
    Matrix *diagonal = (Matrix *)0x1F800000;
    ModelPart *root = part;
    u32 count = root->index;
    u32 i;

    root->world.t[0] = root->translation[0];
    root->world.t[1] = root->translation[1];
    root->world.t[2] = root->translation[2];
    if (root->flag6) {
        func_8004A92C(&root->rotation, &root->world);
    } else {
        func_8003F738(&root->rotation, &root->world);
    }
    diagonal->m[0][0] = scale * part->scale[0] >> 12;
    diagonal->m[0][1] = 0;
    diagonal->m[0][2] = 0;
    diagonal->m[1][0] = 0;
    diagonal->m[1][1] = scale * part->scale[1] >> 12;
    diagonal->m[1][2] = 0;
    diagonal->m[2][0] = 0;
    diagonal->m[2][1] = 0;
    diagonal->m[2][2] = scale * part->scale[2] >> 12;
    MulMatrix0(&part->world, diagonal, &part->transform);
    part->transform.t[0] = part->world.t[0];
    part->transform.t[1] = part->world.t[1];
    part->transform.t[2] = part->world.t[2];
    for (i = 1; i < count; i++) {
        part++;
        if (part->flag5) {
            if (part->flag6) {
                func_8004A92C(&part->rotation, &part->transform);
            } else {
                func_8003F738(&part->rotation, &part->transform);
            }
            part->flag5 = 0;
        }
        if (part->parent != NULL && part->parent->flag4 == 1) {
            part->flag4 = 1;
        }
        if (part->flag4) {
            part->transform.t[0] = part->translation[0];
            part->transform.t[1] = part->translation[1];
            part->transform.t[2] = part->translation[2];
            if (part->parent != NULL) {
                CompMatrix(&part->parent->world, &part->transform, &part->world);
            } else {
                part->world = part->transform;
            }
        }
    }
    for (i = 1; i < count; i++) {
        root++;
        root->flag4 = 0;
    }
    return count;
}

#ifdef NON_MATCHING
/* Pose a model hierarchy with per-part scales: as 8009EF3C, but a changed
 * part's transform is scaled by its own scale and by the inverse of its
 * parent's, and a part changes or is marked with its parent. Clears the marks
 * and returns the part count. */
u16 func_8009F1C4(ModelPart *part, s32 scale) {
    Matrix *diagonal = (Matrix *)0x1F800000;
    ModelPart *root = part;
    u32 count = root->index;
    u32 i;

    root->world.t[0] = root->translation[0];
    root->world.t[1] = root->translation[1];
    root->world.t[2] = root->translation[2];
    if (root->flag6) {
        func_8004A92C(&root->rotation, &root->world);
    } else {
        func_8003F738(&root->rotation, &root->world);
    }
    diagonal->m[0][0] = scale * part->scale[0] >> 12;
    diagonal->m[0][1] = 0;
    diagonal->m[0][2] = 0;
    diagonal->m[1][0] = 0;
    diagonal->m[1][1] = scale * part->scale[1] >> 12;
    diagonal->m[1][2] = 0;
    diagonal->m[2][0] = 0;
    diagonal->m[2][1] = 0;
    diagonal->m[2][2] = scale * part->scale[2] >> 12;
    MulMatrix0(&part->world, diagonal, &part->transform);
    part->transform.t[0] = part->world.t[0];
    part->transform.t[1] = part->world.t[1];
    part->transform.t[2] = part->world.t[2];
    for (i = 1; i < count; i++) {
        part++;
        if (part->parent != NULL) {
            if (part->parent->flag5 == 1) {
                part->flag5 = 1;
            }
            if (part->parent->flag4 == 1) {
                part->flag4 = 1;
            }
        }
        if (part->flag5) {
            if (part->flag6) {
                func_8004A92C(&part->rotation, &part->transform);
            } else {
                func_8003F738(&part->rotation, &part->transform);
            }
            diagonal->m[0][0] = part->scale[0];
            diagonal->m[0][1] = 0;
            diagonal->m[0][2] = 0;
            diagonal->m[1][0] = 0;
            diagonal->m[1][1] = part->scale[1];
            diagonal->m[1][2] = 0;
            diagonal->m[2][0] = 0;
            diagonal->m[2][1] = 0;
            diagonal->m[2][2] = part->scale[2];
            MulMatrix0(&part->transform, diagonal, &part->transform);
            if (part->parent != NULL) {
                diagonal->m[0][0] = 0x1000000 / part->parent->scale[0];
                diagonal->m[0][1] = 0;
                diagonal->m[0][2] = 0;
                diagonal->m[1][0] = 0;
                diagonal->m[1][1] = 0x1000000 / part->parent->scale[1];
                diagonal->m[1][2] = 0;
                diagonal->m[2][0] = 0;
                diagonal->m[2][1] = 0;
                diagonal->m[2][2] = 0x1000000 / part->parent->scale[2];
                MulMatrix0(diagonal, &part->transform, &part->transform);
            }
        }
        if (part->flag4) {
            part->transform.t[0] = part->translation[0];
            part->transform.t[1] = part->translation[1];
            part->transform.t[2] = part->translation[2];
            if (part->parent != NULL) {
                CompMatrix(&part->parent->world, &part->transform, &part->world);
            } else {
                part->world = part->transform;
            }
        }
    }
    for (i = 1; i < count; i++) {
        root++;
        root->flag5 = 0;
        root->flag4 = 0;
    }
    return count;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F1C4);
#endif

void func_8009F5B0(void) {
}

/* Draw a posed hierarchy's parts into packet buffer `buffer`: each part's light
 * matrix from `light` and its world matrix, and its rotation and translation
 * composed with `view` and the root's transform, then its model (8002C700). */
void func_8009F5B8(ModelList *list, ModelPart *part, Matrix *view, Matrix *light, s32 arg4, s32 arg5,
                   s32 buffer) {
    Matrix *scratch = (Matrix *)0x1F800000;
    Matrix *lighting = (Matrix *)0x1F800020;
    Matrix *camera = (Matrix *)0x1F800040;
    u32 count;
    u32 i;

    MulMatrix0(light, &part->world, lighting);
    CompMatrix(view, &part->transform, camera);
    count = part->index;
    part++;
    for (i = 1; i < count; i++, part++) {
        if (part->modelId != 0xFFFF) {
            MulMatrix0(lighting, &part->world, scratch);
            SetLightMatrix(scratch);
            CompMatrix(camera, &part->world, scratch);
            SetRotMatrix(scratch);
            SetTransMatrix(scratch);
            func_8002C700(list->models[part->modelId], part->packets[buffer], arg5, arg4);
        }
    }
}

/* Free a model hierarchy: every part's packets, then the parts. */
void func_8009F708(ModelPart *root) {
    ModelPart *part;
    s32 i;

    if (root != NULL) {
        part = root;
        for (i = 0; i < root->index; i++, part++) {
            if (part->packets[0] != NULL) {
                func_800320E8(part->packets[0]);
                part->packets[0] = NULL;
                part->packets[1] = NULL;
            }
        }
        root->index = 0;
        func_800320E8(root);
    }
}

/* Free a model list's table, releasing its models first when release is
 * set. */
void func_8009F794(ModelList *list, s32 release) {
    u32 i;

    if (list != NULL) {
        for (i = 0; i < list->count; i++) {
            if (list->models != NULL && list->models[i] != NULL && release) {
                func_8002CBBC(list->models[i]);
            }
        }
        if (list->models != NULL) {
            func_800320E8(list->models);
            list->models = NULL;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A0838);

/* Apply an animation frame to a hierarchy's parts: the listed rotations (unless
 * flag 1) and translations (unless flag 2), marking each changed part; parts
 * with a persistent effect attached keep theirs. The frame holds halfwords:
 * [2] flags, [3] base flag, [6] rotation count, [7] translation count, then
 * from [12] (x, y, z) triples, after the base rotations when [3] is 0.
 * Returns the part count less the root. */
u16 func_800A1B50(ModelPart *root, s16 *data) {
    u16 rotations;
    u16 translations;
    u16 rotationCount;
    u16 translationCount;
    u16 flags;
    ModelPart *part;
    u16 count;
    s32 i;
    s32 x;
    s32 y;
    s32 z;

    rotations = 0;
    translations = 0;
    i = data[3]; /* the base flag */
    rotationCount = data[6];
    translationCount = data[7];
    flags = data[2];
    data += 12;
    if (i == 0) {
        data += (rotationCount + 1) * 3;
    }
    count = root->index - 1;
    part = root;
    for (i = 0; i < count; i++) {
        part++;
        if (!(flags & 1) && rotations < rotationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            rotations++;
            if ((part->rotation.vx != x || part->rotation.vy != y || part->rotation.vz != z)
                && (part->effects[0] == NULL || part->effects[0]->kind != 0xFF)) {
                part->rotation.vx = x;
                part->rotation.vy = y;
                part->rotation.vz = z;
                part->flag4 = 1;
                part->flag5 = 1;
            }
        }
        if (!(flags & 2) && translations < translationCount) {
            x = *data++;
            y = *data++;
            z = *data++;
            translations++;
            if ((part->translation[0] != x || part->translation[1] != y || part->translation[2] != z)
                && (part->effects[1] == NULL || part->effects[1]->kind != 0xFF)) {
                part->translation[0] = x;
                part->translation[1] = y;
                part->translation[2] = z;
                part->flag4 = 1;
            }
        }
    }
    return count;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A1CF4);

/* Release the effects attached to part index of a hierarchy, those selected by
 * mask (bit n: attachment n). */
void func_800A216C(EffectPool *pool, ModelPart *part, s32 index, s32 mask) {
    if (index < part->index) {
        part += index;
        if (part->effects[0] != NULL && (mask & 1)) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && (mask & 2)) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && (mask & 4)) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Create a pool of count effect entries. */
EffectPool *func_800A2234(EffectPool *pool, s32 count) {
    if (count <= 0) {
        return NULL;
    }
    pool->count = count;
    func_80032498(4, 0);
    pool->entries = func_80031BDC(count * sizeof(EffectEntry), 0);
    if (pool->entries != NULL) {
        func_800A22E8(pool);
        return pool;
    }
    return NULL;
}

/* Free a pool's entries. */
void func_800A22A8(EffectPool *pool) {
    pool->next = 0;
    if (pool->entries != NULL) {
        func_800320E8(pool->entries);
    }
    pool->entries = NULL;
}

/* Mark every entry of a pool free. */
void func_800A22E8(EffectPool *pool) {
    EffectEntry *entry;
    s32 i;

    if (pool->entries != NULL) {
        entry = pool->entries;
        pool->next = 0;
        for (i = 0; i < pool->count; i++) {
            entry->used = 0;
            entry++;
        }
    }
}

/* Take the first free entry of a pool (NULL when none), advancing the free
 * index past the entries in use. */
EffectEntry *func_800A2330(EffectPool *pool) {
    EffectEntry *entry;
    u32 count;

    if (pool->next < pool->count) {
        entry = &pool->entries[pool->next];
        if (entry->used) {
            return NULL;
        }
        pool->next++;
        count = pool->count;
        while (pool->next < count && pool->entries[pool->next].used != 0) {
            pool->next++;
        }
        return entry;
    }
    return NULL;
}

/* Return an entry to its pool; its index, or -1 for none. */
s32 func_800A23E8(EffectPool *pool, EffectEntry *entry) {
    s32 index;

    if (entry == NULL) {
        return -1;
    }
    index = ((u32)entry - (u32)pool->entries) / sizeof(EffectEntry);
    if (index < pool->next) {
        pool->next = index;
    }
    entry->used = 0;
    return index;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2434);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2704);

/* Release every part's attached effects that are not persistent. */
void func_800A2ACC(EffectPool *pool, ModelPart *part) {
    u16 count = part->index;
    s32 i;

    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL && part->effects[0]->kind != 0xFF) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && part->effects[1]->kind != 0xFF) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && part->effects[2]->kind != 0xFF) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Release every part's attached effects of kind. */
void func_800A2BB8(EffectPool *pool, ModelPart *part, u8 kind) {
    u16 count = part->index;
    s32 i;

    for (i = 0; i < count; i++, part++) {
        if (part->effects[0] != NULL && part->effects[0]->kind == kind) {
            func_800A23E8(pool, part->effects[0]);
            part->effects[0] = NULL;
        }
        if (part->effects[1] != NULL && part->effects[1]->kind == kind) {
            func_800A23E8(pool, part->effects[1]);
            part->effects[1] = NULL;
        }
        if (part->effects[2] != NULL && part->effects[2]->kind == kind) {
            func_800A23E8(pool, part->effects[2]);
            part->effects[2] = NULL;
        }
    }
}

/* Create a pool of count sprite records (and a spare). */
SpritePool *func_800A2CA4(SpritePool *pool, s32 count) {
    func_80032498(4, 0);
    pool->count = count;
    pool->next = 0;
    pool->records = func_80031BDC((count + 1) * sizeof(SpriteRecord), 0);
    if (pool->records != NULL) {
        func_800A2D5C(pool);
        return pool;
    }
    return NULL;
}

/* Free a sprite pool's records. */
void func_800A2D1C(SpritePool *pool) {
    pool->count = 0;
    pool->next = 0;
    if (pool->records != NULL) {
        func_800320E8(pool->records);
    }
    pool->records = NULL;
}

/* Mark every record of a sprite pool free and set up both of its
 * semi-transparent quadrilaterals. */
void func_800A2D5C(SpritePool *pool) {
    SpriteRecord *record = pool->records;
    s32 i;
    s32 j;

    for (i = 0; i < pool->count + 1; i++) {
        record->id = -1;
        record->field1E = 0;
        for (j = 0; j < 2; j++) {
            SetPolyFT4(&record->packets[j]);
            SetSemiTrans(&record->packets[j], 1);
            record->packets[j].clut = GetClut(0, 0x1CD);
            record->packets[j].tpage = GetTPage(0, 1, 0x380, 0);
            record->packets[j].u0 = 0;
            record->packets[j].v0 = 0xC1;
            record->packets[j].u1 = 0;
            record->packets[j].v1 = 0xC1;
            record->packets[j].u2 = 0xF;
            record->packets[j].v2 = 0xC1;
            record->packets[j].u3 = 0xF;
            record->packets[j].v3 = 0xC1;
        }
        record++;
    }
}

/* Take the first free record of a sprite pool with its quadrilaterals'
 * semi-transparency set to abe, advancing the free index past the records in
 * use; the spare record when none is free. */
SpriteRecord *func_800A2E88(SpritePool *pool, s16 abe) {
    SpriteRecord *record;
    s16 next = pool->next;

    if (next < pool->count) {
        record = &pool->records[next];
        if (record->id == -1) {
            pool->next = next + 1;
            while (pool->next < pool->count && pool->records[pool->next].id != -1) {
                pool->next++;
            }
            SetSemiTrans(&record->packets[0], abe);
            SetSemiTrans(&record->packets[1], abe);
            return record;
        }
    }
    return &pool->records[pool->count];
}

/* Return a record to its sprite pool; its index. */
s32 func_800A2F94(SpritePool *pool, SpriteRecord *record) {
    s32 index = ((u32)record - (u32)pool->records) / sizeof(SpriteRecord);

    if (index <= pool->next) {
        pool->next = index;
    }
    record->id = -1;
    return index;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2FD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A32D8);

/* Mark a halfword slot empty. */
void func_800A3484(s16 *slot) {
    *slot = -1;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3490);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3514);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A35C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3640);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3E98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A429C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4348);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A43F8);

/* Update the active trackers' positions: an offset from a part of a stage
 * object's hierarchy when the object exists, else the offset itself. */
void func_800A44C0(BattleObject **objects) {
    Matrix *m = (Matrix *)0x1F800000;
    s32 i;
    ModelPart *root;
    Vector position;

    for (i = 0; i < 2; i++) {
        if (D_800D3304[i].active != 0) {
            if (D_800D3304[i].object >= 0 && objects[D_800D3304[i].object] != NULL) {
                root = objects[D_800D3304[i].object]->hierarchy;
                CompMatrix(&root->transform, &root[D_800D3304[i].part + 1].world, m);
                SetRotMatrix(m);
                SetTransMatrix(m);
                gte_ldv0(&D_800D3304[i].offset);
                gte_rtv0tr();
                gte_stlvnl(&position);
                D_800D3304[i].x = position.vx;
                D_800D3304[i].y = position.vy;
                D_800D3304[i].z = position.vz;
            } else {
                D_800D3304[i].x = D_800D3304[i].offset.vx;
                D_800D3304[i].y = D_800D3304[i].offset.vy;
                D_800D3304[i].z = D_800D3304[i].offset.vz;
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4654);

/* Free the battle scene's resources: the stage objects, the scene data, both
 * resident handles of D_800C3D50 (80027D40), the block D_800C3EA0 and both
 * records of D_800C3DA0 (8002800C). */
void func_800A4820(void) {
    s32 i;

    func_800A9FF0(31);
    D_800C3E38 = 0;
    if (D_800658C8 != NULL) {
        func_800320E8(D_800658C8);
    }
    D_800658C8 = NULL;
    for (i = 0; i < 2; i++) {
        if (D_800C3D50[i] != NULL) {
            func_80027D40(D_800C3D50[i]);
        }
        D_800C3D50[i] = NULL;
    }
    if (D_800C3EA0 != NULL) {
        func_800320E8(D_800C3EA0);
    }
    D_800C3EA0 = NULL;
    for (i = 0; i < 2; i++) {
        func_8002800C(&D_800C3DA0[i]);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A48EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4B3C);

/* Place a copy of stage object index (800B10EC) at every listed point, its
 * height raised by the object's size. */
void func_800A4CF8(s32 index) {
    u16 *point = D_800D2FD0;
    s32 i;
    s16 x;
    s16 z;
    s16 y;

    for (i = 0; i < D_800D2FC8; i++) {
        x = *point++;
        z = *point++;
        y = *point++;
        func_800B10EC(index, x, z, func_800AA650(index) + y);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4DB8);

/* The scene's points. */
SVector *func_800A577C(void) {
    return D_800D3344;
}

/* The scene's triangles. */
SceneTriangle *func_800A578C(void) {
    return D_800D39CC;
}

/* The first scene triangle containing point (800A5A48 gives -1), -1 for
 * none. */
s32 func_800A579C(SVector *point) {
    s32 i;

    if (D_800D3344 != NULL && D_800D39CC != NULL) {
        for (i = 0; i < D_800D3348; i++) {
            if (func_800A5A48(&D_800D3344[D_800D39CC[i].vertices[0]], &D_800D3344[D_800D39CC[i].vertices[1]],
                              &D_800D3344[D_800D39CC[i].vertices[2]], point)
                == -1) {
                return i;
            }
        }
    }
    return -1;
}

/* Relate point to scene triangle index (800A5BE8, into out); the triangle's
 * id, or -1 without scene geometry. */
s32 func_800A5870(SVector *point, s32 index, void *out) {
    SceneTriangle *triangle;

    if (D_800D3344 == NULL || D_800D39CC == NULL || index < 0) {
        return -1;
    }
    triangle = &D_800D39CC[index];
    func_800A5BE8(&D_800D3344[triangle->vertices[0]], &D_800D3344[triangle->vertices[1]],
                  &D_800D3344[triangle->vertices[2]], point, out);
    return D_800D39CC[index].id;
}

/* The scene triangle containing point, searched from triangle through its
 * neighbours (800A5D54, depth levels, up to depth tries); -1 for none. Each
 * search uses a new visit stamp; when the stamp wraps the marks are cleared. */
s32 func_800A5914(SVector *point, s32 triangle, s32 depth) {
    s32 found;
    s32 i;

    if (D_800D3344 == NULL) {
        return -1;
    }
    if (D_800D39CC == NULL) {
        return -1;
    }
    if (triangle >= D_800D3348) {
        return -1;
    }
    for (i = 0; i < depth; i++) {
        found = func_800A5D54(point, triangle, depth);
        if (found >= 0) {
            break;
        }
    }
    D_800D2F64++;
    if (D_800D2F64 == 0) {
        D_800D2F64 = 1;
        for (i = 0; i < D_800D3348; i++) {
            D_800D39CC[i].visited = 0;
        }
    }
    return found;
}

/* Whether point lies within triangle (a, b, c) in the ground plane: -1 when
 * it is on the inner side of all three edges (cross products, 8004A4D8),
 * otherwise 0. */
s32 func_800A5A48(SVector *a, SVector *b, SVector *c, SVector *point) {
    Vector edge;
    Vector toPoint;
    Vector cross;

    edge.vx = b->vx - a->vx;
    edge.vy = 0;
    edge.vz = b->vz - a->vz;
    toPoint.vx = point->vx - a->vx;
    toPoint.vy = 0;
    toPoint.vz = point->vz - a->vz;
    OuterProduct0(&edge, &toPoint, &cross);
    if (cross.vy < 0) {
        return 0;
    }
    edge.vx = c->vx - b->vx;
    edge.vy = 0;
    edge.vz = c->vz - b->vz;
    toPoint.vx = point->vx - b->vx;
    toPoint.vy = 0;
    toPoint.vz = point->vz - b->vz;
    OuterProduct0(&edge, &toPoint, &cross);
    if (cross.vy < 0) {
        return 0;
    }
    edge.vx = a->vx - c->vx;
    edge.vy = 0;
    edge.vz = a->vz - c->vz;
    toPoint.vx = point->vx - c->vx;
    toPoint.vy = 0;
    toPoint.vz = point->vz - c->vz;
    OuterProduct0(&edge, &toPoint, &cross);
    return -(cross.vy >= 0);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5BE8);

#ifdef NON_MATCHING
/* Search triangle and, up to depth levels, its unvisited neighbours for the one
 * containing point (800A5A48 gives -1); -1 for none. */
s32 func_800A5D54(SVector *point, s32 triangle, s32 depth) {
    s32 found;

    if (triangle >= 0) {
        if (D_800D39CC[triangle].visited != D_800D2F64) {
            D_800D39CC[triangle].visited = D_800D2F64;
            if (func_800A5A48(&D_800D3344[D_800D39CC[triangle].vertices[0]],
                              &D_800D3344[D_800D39CC[triangle].vertices[1]],
                              &D_800D3344[D_800D39CC[triangle].vertices[2]], point)
                == -1) {
                return triangle;
            }
            if (depth > 0) {
                found = func_800A5D54(point, D_800D39CC[triangle].neighbours[0], depth - 1);
                if (found >= 0) {
                    return found;
                }
                found = func_800A5D54(point, D_800D39CC[triangle].neighbours[1], depth - 1);
                if (found >= 0) {
                    return found;
                }
                found = func_800A5D54(point, D_800D39CC[triangle].neighbours[2], depth - 1);
                if (found >= 0) {
                    return found;
                }
            }
        }
    }
    return -1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5D54);
#endif

/* Set the two words D_800D2D40 and D_800D2D48. */
void func_800A5E9C(s32 first, s32 second) {
    D_800D2D40 = first;
    D_800D2D48 = second;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5EB4);

/* Set light slot index (0-3) to a color and two values; a negative red turns
 * it off. */
void func_800A6444(s32 index, s32 r, s32 g, s32 b, s32 field4, s32 field5) {
    if (index < 4) {
        if (r >= 0) {
            D_800D3611 = 1;
            D_800C3AAC[index].active = 1;
            D_800C3AAC[index].r = r;
            D_800C3AAC[index].g = g;
            D_800C3AAC[index].b = b;
            D_800C3AAC[index].field4 = field4;
            D_800C3AAC[index].field5 = field5;
        } else {
            D_800C3AAC[index].active = 0;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A64E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6884);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6AE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A7064);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A7948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8A88);

#ifdef NON_MATCHING
/* Reset the battle scene: its flags, the effect and sprite pools sized by the
 * scene data, and the object and slot tables. */
void func_800A8B0C(void) {
    s32 i;
    s32 j;
    s32 k;

    D_800C3E88 = 0;
    D_800C3CF0 = 0;
    D_800C3D6C = 0;
    D_800C3D68 = 0;
    D_800C3B7C = 0;
    D_800C3B74 = 1;
    func_800A2234(&D_800C3D0C, D_800658C8->effectCount);
    func_800A2CA4(&D_800C3D04, D_800658C8->spriteCount);
    func_800B00D0();
    for (i = 31; i >= 0; i--) {
        D_800D3368[i] = NULL;
    }
    for (j = 19; j >= 0; j--) {
        D_800C3ACC[j].value = 0;
    }
    for (k = 1; k >= 0; k--) {
        D_800D3304[k].active = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8B0C);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8BF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9540);

/* Load the battle's sound banks for set: banks 2 * set + 1 and 2 * set + 2,
 * each with a buffer of its size (800288EC), into a new record D_800C3B78. */
void func_800A96B4(s32 set) {
    s32 saved0;
    s32 saved1;
    SoundBanks *banks;
    s32 bank;

    func_800284B4(&saved0, &saved1);
    func_80028470(0x28, 0);
    func_80032498(4, 0);
    banks = func_80031BDC(sizeof(SoundBanks), 1);
    set *= 2;
    bank = set + 1;
    D_800C3B78 = banks;
    func_80028998(bank);
    banks->bank0 = bank;
    banks->data0 = func_80031BDC(func_800288EC(bank), 1);
    bank = set + 2;
    banks->bank1 = bank;
    banks->data1 = func_80031BDC(func_800288EC(bank), 1);
    banks->field10 = 0;
    banks->field14 = 0;
    func_80029AFC(D_800C3B78, 0, 0);
    func_80028470(saved0, saved1);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A979C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9A50);

/* Free the stage objects (800A9FF0), the effect pool and the sprite pool. */
void func_800A9F94(void) {
    s32 i;

    for (i = 0; i < 31; i++) {
        func_800A9FF0(i);
    }
    func_800A22A8(&D_800C3D0C);
    func_800A2D1C(&D_800C3D04);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9FF0);

/* Select stage object index with slot mask, and start its effect (800AA934). */
void func_800AA320(u16 index, s16 mask, s32 arg2) {
    BattleObject *object = D_800D3368[index];

    D_800C3D40 = index;
    D_800C3E30 = mask;
    object->field35 = 0;
    if (D_800D3368[index] != NULL) {
        func_800AA934(D_800D3368[index], D_800D3368[index], &D_800C3D0C, arg2);
    }
}

/* Make stage object index the acting object with slot mask: mark it, reset
 * the camera state (800BF85C) for the first selected slot, and start its
 * effect (800AA934). */
void func_800AA384(u16 index, u16 mask, s32 arg2) {
    BattleObject *object;

    D_800C3D40 = index;
    object = D_800D3368[index];
    D_800C3E30 = mask;
    D_800C3D68 = 1;
    object->field35 = 1;
    D_80059464 = 0;
    D_800591AC = 1;
    func_800BF85C(index, func_800AF400());
    D_800C360C = 1;
    D_800C4000[func_800AF400()] = 0;
    if (D_800D3368[index] != NULL) {
        func_800AA934(D_800D3368[index], D_800D3368[index], &D_800C3D0C, arg2);
    }
}

/* Select stage object index with slot mask and start its effect (800AA934)
 * unless it is the first selected slot's object; the selection is restored. */
void func_800AA454(u16 index, u16 mask, s32 arg2) {
    u16 savedIndex = D_800C3D40;
    u16 savedMask = D_800C3E30;
    BattleObject *object = D_800D3368[index];

    D_800C3D40 = index;
    D_800C3E30 = mask;
    object->field35 = 0;
    if (D_800D3368[index] != NULL && index != func_800AF400()) {
        func_800AA934(D_800D3368[index], D_800D3368[index], &D_800C3D0C, arg2);
    }
    D_800C3D40 = savedIndex;
    D_800C3E30 = savedMask;
}

/* c plus a * b / 256, capped at 255. */
u8 func_800AA514(s16 a, s16 b, s32 c) {
    s16 value = c + b * a / 256;

    if (value > 255) {
        value = 255;
    }
    return value;
}

/* Select stage object index with slot mask and start its effect on target
 * (800AA934) unless it is the first selected slot's object. */
void func_800AA564(BattleObject *target, u16 index, u16 mask, s32 arg3) {
    BattleObject *object = D_800D3368[index];

    D_800C3D40 = index;
    D_800C3E30 = mask;
    object->field35 = 0;
    if (D_800D3368[index] != NULL && index != func_800AF400()) {
        func_800AA934(D_800D3368[index], target, &D_800C3D0C, arg3);
    }
}

/* The scaled size of stage object index (0 when absent). */
s32 func_800AA600(s32 index) {
    BattleObject *object = D_800D3368[index];
    s32 size = 0;

    if (object != NULL) {
        size = object->scale24 * (object->scale1C * object->hierarchy->scale[1] >> 12) >> 12;
    }
    return size;
}

/* The scaled size of stage object index along its hierarchy's first axis
 * (flag 8) or its third; 0 when absent. */
s32 func_800AA650(s32 index) {
    BattleObject *object = D_800D3368[index];
    s32 size = 0;
    s32 scale;
    s32 axis;

    if (object != NULL) {
        if (object->flags4A & 8) {
            scale = object->scale26;
            axis = object->hierarchy->scale[0];
        } else {
            scale = object->scale28;
            axis = object->hierarchy->scale[2];
        }
        size = scale * (D_800D3368[index]->scale1C * axis >> 12) >> 12;
    }
    return size;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA6E0);

/* Set stage object index's byte 0x2A, when it exists. */
void func_800AA760(s32 index, u8 value) {
    if (D_800D3368[index] != NULL) {
        D_800D3368[index]->field2A = value;
    }
}

/* Set the flag D_800C3B74 to the low bit of value. */
void func_800AA788(s32 value) {
    D_800C3B74 = value & 1;
}

/* Swap stage objects a and b, deactivating a and activating b first. */
void func_800AA79C(s32 a, s32 b) {
    BattleObject **objects = D_800D3368;
    BattleObject **first = &objects[a];
    BattleObject **second = &objects[b];
    BattleObject *swap;

    (*first)->active = 0;
    (*second)->active = 1;
    swap = *first;
    *first = *second;
    *second = swap;
}

/* The type of the first event from index on that is not a continuation
 * (0xF7); 0xFE for the end (0xFF). */
u8 func_800AA7DC(s32 index) {
    u8 type;

    do {
        type = D_800C3FE8[index++].type;
    } while (type == 0xF7);
    if (type == 0xFF) {
        type = 0xFE;
    }
    return type;
}

/* The effect step handler for mode (1-3; any other the default). */
void *func_800AA820(s32 mode) {
    switch (mode) {
    case 1:
        return func_800A3514;
    case 2:
        return func_800A3578;
    case 3:
        return func_800A35C8;
    default:
        return func_800A3490;
    }
}

#ifdef NON_MATCHING
/* Reset a battle object's state. */
void func_800AA898(BattleObject *object, s32 arg1, s32 field8, u8 **animations) {
    object->field3C = 0xFFFF;
    object->field5C = 0xFF;
    object->field39 = 0x6B;
    object->field8 = field8;
    object->packets = NULL;
    object->field10 = 0;
    object->animations = animations;
    object->moreAnimations = NULL;
    object->field2B = 0;
    object->animation = -1;
    object->field58 = 0;
    object->field35 = 0;
    object->field37 = 0;
    object->field38 = 0;
    object->field3A = -1;
    object->motion[0] = 0;
    object->motion[1] = 0;
    object->motion[2] = 0;
    object->motion[3] = 0;
    object->motion[4] = 0;
    object->motion[5] = 0;
    object->motion[6] = 0;
    object->motion[7] = 0;
    object->motion[8] = 0;
    object->motion[9] = 0;
    object->motion[10] = 0;
    object->motion[11] = 0;
    object->position[0] = 0;
    object->position[1] = 0;
    object->position[2] = 0;
    object->field8E = 1;
    object->field36 = 0;
    object->field1E = -1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA898);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA934);

/* Update a battle object for steps frames: put it on the ground, pose its
 * hierarchy (per-part scales when field37 is set) and animate it (800A0838,
 * 800AE2A4) each step, then run its effects (800AAD54). Returns the combined
 * animation flags. */
s32 func_800AAA20(BattleObject *object, ModelList *models, s32 steps, s32 arg3, s32 arg4) {
    s32 flags;
    s32 i;

    if (object->field0 != 0) {
        flags = 0;
        if (object->active) {
            func_800AFF9C(object);
            if (object->field37) {
                func_8009F1C4(object->hierarchy, object->scale1C);
            } else {
                func_8009EF3C(object->hierarchy, object->scale1C);
            }
            for (i = 0; i < steps; i++) {
                flags |= func_800A0838(models, object->hierarchy, object->field3C, object->scale1C);
                func_800AE2A4(object, models, arg3);
            }
        }
        func_800AAD54(object, models, flags, steps, arg4);
    }
    return flags;
}

/* Carry a battle object along with its parent object (index field5C; it
 * becomes 0xFF once the parent is gone): take its active state unless flag
 * 0x10, turn it with the parent (or a part of the parent) when field5D is set,
 * and place its hierarchy's root at its offset from there. */
void func_800AAB34(BattleObject *object) {
    Matrix *m = (Matrix *)0x1F800000;
    SVector offset;

    if (D_800D3368[object->field5C] != NULL) {
        if (!(object->flags4A & 0x10)) {
            object->active = D_800D3368[object->field5C]->active;
        }
        if (object->active) {
            if (object->field5D) {
                if (object->parentPart != 0) {
                    MulMatrix0(&D_800D3368[object->field5C]->hierarchy->world,
                                  &D_800D3368[object->field5C]->hierarchy[object->parentPart].world, m);
                } else {
                    m = &D_800D3368[object->field5C]->hierarchy->world;
                }
                func_80049BDC(m, &object->hierarchy->transform);
                func_80049BDC(m, &object->hierarchy->world);
            }
            if (object->parentPart != 0) {
                CompMatrix(&D_800D3368[object->field5C]->hierarchy->transform,
                              &D_800D3368[object->field5C]->hierarchy[object->parentPart].world, m);
            } else {
                m = &D_800D3368[object->field5C]->hierarchy->transform;
            }
            SetRotMatrix(m);
            SetTransMatrix(m);
            offset.vx = object->offset2[0];
            offset.vy = object->offset2[1];
            offset.vz = object->offset2[2];
            gte_ldv0(&offset);
            gte_rtv0tr();
            gte_stlvnl(object->hierarchy->transform.t);
            gte_stlvnl(object->hierarchy->translation);
        }
    } else {
        object->field5C = 0xFF;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAD54);

/* Turn a part to rotation (x, y, z): at once for a duration below 2, else by
 * a turning effect (kind 0xFE) over duration frames along the shortest way
 * (x, y, z become the turns). */
void func_800ADF1C(EffectPool *pool, ModelPart *part, s32 duration, s32 x, s32 y, s32 z) {
    EffectEntry *entry;

    if (duration < 2) {
        part->rotation.vx = x;
        part->rotation.vy = y;
        part->rotation.vz = z;
        part->flag5 = 1;
        return;
    }
    if (part->rotation.vx != x || part->rotation.vy != y || part->rotation.vz != z) {
        if (part->effects[0] != NULL) {
            entry = part->effects[0];
        } else {
            entry = func_800A2330(pool);
        }
        if (entry != NULL) {
            entry->used = 1;
            entry->field2 = 3;
            entry->field1 = 0;
            entry->kind = 0xFE;
            entry->params[0] = part->rotation.vx;
            entry->params[1] = part->rotation.vy;
            entry->params[2] = part->rotation.vz;
            x = (x - part->rotation.vx) & 0xFFF;
            if (x >= 0x800) {
                x -= 0x1000;
            }
            entry->params[3] = x;
            y = (y - part->rotation.vy) & 0xFFF;
            if (y >= 0x800) {
                y -= 0x1000;
            }
            entry->params[4] = y;
            z = (z - part->rotation.vz) & 0xFFF;
            if (z >= 0x800) {
                z -= 0x1000;
            }
            entry->params[5] = z;
            entry->field10 = 0;
            entry->field12 = duration;
            part->effects[0] = entry;
        }
    }
}

/* Attach a travelling effect (kind 0xFE) to a part: from its translation to
 * (x, y, z), over a length of its distance plus one. */
void func_800AE098(EffectPool *pool, ModelPart *part, s32 type, s32 param1, s32 param2, s32 field12, s32 x,
                   s32 y, s32 z) {
    EffectEntry *entry;
    s32 dx;
    s32 dy;
    s32 dz;

    if (part->effects[0] != NULL) {
        entry = part->effects[0];
    } else {
        entry = func_800A2330(pool);
    }
    if (entry != NULL) {
        entry->used = 1;
        entry->field2 = type + 7;
        entry->field1 = 0;
        entry->kind = 0xFE;
        dx = x - part->translation[0];
        dy = y - part->translation[1];
        dz = z - part->translation[2];
        entry->params[0] = SquareRoot0(dx * dx + dy * dy + dz * dz) + 1;
        entry->params[1] = param1;
        entry->params[2] = param2;
        entry->params[3] = x;
        entry->params[4] = y;
        entry->params[5] = z;
        entry->field10 = 0;
        entry->field12 = field12;
        part->effects[0] = entry;
    }
}

/* Start an animation on a battle object (looping when loop is set); an empty
 * animation stops it. */
void func_800AE1BC(BattleObject *object, Animation *animation, s32 loop) {
    u8 *data;

    if (animation->length != 0) {
        object->animation = 0;
        if (loop) {
            object->animationLoop = animation->loop;
        } else {
            object->animationLoop = -1;
        }
        object->animationFrame = 0;
        object->animationLength = animation->length;
        data = (u8 *)animation + animation->dataOffset;
        object->animationStart = data;
        object->animationCursor = data;
    } else {
        object->animation = -1;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE220);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE2A4);

/* Stop a battle object's animation. */
void func_800AEEEC(BattleObject *object) {
    object->animation = -1;
}

/* Distance from a battle object's position to its hierarchy's translation. */
s32 func_800AEEF8(BattleObject *object) {
    ModelPart *root = object->hierarchy;
    s32 dx = object->position[0] - root->translation[0];
    s32 dy = object->position[1] - root->translation[1];
    s32 dz = object->position[2] - root->translation[2];

    return SquareRoot0(dx * dx + dy * dy + dz * dz);
}

/* Place a battle object at its offset from its target (code 0xFF the first
 * slot of its mask, 0xFE the selected object, 0xFD/0xFC its own slots, 0xFA
 * slot 31, 1-127 the slot below): through the target's hierarchy (or a part
 * of it) when the target exists and is not the object's own slot, carrying a
 * following effect along; otherwise at the slot's battle position. */
void func_800AEF68(BattleObject *object) {
    s32 slot = 0;
    BattleObject *target;
    Matrix *m;
    Vector position;
    EffectEntry *entry;

    if (object->field58 == 0xFF) {
        for (slot = 0; slot < 13; slot++) {
            if ((object->slotMask >> slot) & 1) {
                break;
            }
        }
    }
    if (object->field58 == 0xFE) {
        slot = D_800C3D40;
    }
    if (object->field58 == 0xFD) {
        slot = object->slot;
    }
    if (object->field58 == 0xFC) {
        slot = object->slot2;
    }
    if (object->field58 == 0xFA) {
        slot = 31;
    }
    if (object->field58 > 0 && object->field58 < 0x80) {
        slot = object->field58 - 1;
    }
    target = D_800D3368[slot];
    if (target != NULL && slot != object->slot) {
        m = (Matrix *)0x1F800000;
        if (object->targetPart != 0) {
            CompMatrix(&target->hierarchy->transform, &target->hierarchy[object->targetPart].world, m);
        } else {
            m = &target->hierarchy->transform;
        }
        SetRotMatrix(m);
        SetTransMatrix(m);
        gte_ldv0(object->offset);
        gte_rtv0tr();
        gte_stlvnl(&position);
        object->position[0] = position.vx;
        object->position[1] = position.vy;
        object->position[2] = position.vz;
        entry = object->hierarchy->effects[0];
        if (entry != NULL && (u32)(entry->field2 - 7) < 2) {
            entry->params[3] = position.vx;
            entry->params[4] = position.vy;
            entry->params[5] = position.vz;
        }
    } else {
        object->position[0] = D_800C3EB4[slot].x;
        object->position[1] = D_800C3EB4[slot].y;
        object->position[2] = D_800C3EB4[slot].z;
    }
}

#ifdef NON_MATCHING
/* Detach part index of a hierarchy and its descendants: move their marks to
 * the same parts of another hierarchy and release their effects. */
void func_800AF180(EffectPool *pool, s32 index, ModelPart *from, ModelPart *to) {
    ModelPart *part = from + index;
    u16 count = from->index;
    ModelPart *child;
    s32 i;

    part->flag7 = 0;
    to[index].flag7 = 1;
    func_800A23E8(pool, part->effects[0]);
    part->effects[0] = NULL;
    func_800A23E8(pool, part->effects[1]);
    part->effects[1] = NULL;
    func_800A23E8(pool, part->effects[2]);
    part->effects[2] = NULL;
    child = from;
    for (i = 1; i < count; i++) {
        child++;
        if (child->parent == part) {
            func_800AF180(pool, child->index, from, to);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF180);
#endif

/* Move the parts' marks (flag7) of a hierarchy to another of the same
 * shape. */
void func_800AF270(ModelPart *from, ModelPart *to) {
    s32 count = from->index;
    s32 i;

    for (i = 1; i < count; i++) {
        from++;
        to++;
        if (from->flag7) {
            from->flag7 = 0;
            to->flag7 = 1;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF2C4);

/* The lowest slot (0-12) set in the mask D_800C3E30; 13 when none. */
s32 func_800AF400(void) {
    s32 slot;

    for (slot = 0; slot < 13; slot++) {
        if ((D_800C3E30 >> slot) & 1) {
            break;
        }
    }
    return slot;
}

#ifdef NON_MATCHING
/* The slot mask of a target code: 0xFF the first selected slot, 0xFE the
 * selected object, 0xFD/0xF9 and 0xFC the object's slots, 0xFA slot 31, 0xF8
 * and 0xF7 slots derived from the object's slot; any other code is a slot. */
void func_800AF438(BattleObject *object, s32 code, u16 *mask) {
    u8 slot = code;

    if (slot == 0xFF) {
        slot = func_800AF400();
    } else if (slot == 0xFE) {
        slot = D_800C3D40;
    } else if (slot == 0xFD || slot == 0xF9) {
        slot = object->slot;
    } else if (slot == 0xFC) {
        slot = object->slot2;
    } else if (slot == 0xFA) {
        slot = 31;
    } else if (slot == 0xF8) {
        slot = object->slot * 2 + 13;
    } else if (slot == 0xF7) {
        slot = object->slot * 2 + 14;
    }
    *mask = 1 << slot;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF438);
#endif

/* The animation of a battle object for index; 0xFE repeats the object's
 * current one and 0xFF chooses it from the slot's gear warnings (0x1B, 6 or
 * 1; bit 0x80 when the gear status is on), reporting that bit in flag. */
u8 *func_800AF518(BattleObject *object, u8 index, s32 *flag) {
    s32 warnings;

    *flag = 0;
    if (index >= 0xFE) {
        if (index == 0xFF) {
            warnings = func_8009C050(object->slot);
            if ((warnings & 4) && (object->flags4A & 0x100)) {
                object->field2A = 0x1B;
            } else if ((warnings & 2) && (object->flags4A & 0x80)) {
                if (!(D_800CCCE8.records[object->slot].pilot.flags36 & 1)) {
                    object->field2A = 6;
                }
            } else if (!(object->flags4A & 0x400)) {
                object->field2A = 1;
            }
            if (warnings & 1) {
                object->field2A |= 0x80;
            }
        }
        index = object->field2A & 0x7F;
        *flag = object->field2A & 0x80;
    }
    if (index < 0x40) {
        return object->animations[index + 1];
    }
    return object->moreAnimations[index - 0x3F];
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF678);

/* Mark (flags bit 0) or unmark part of a battle object's hierarchy, and with
 * flags bit 0x80 its descendants. */
void func_800AFA98(BattleObject *object, ModelPart *part, s32 flags) {
    ModelPart *child;
    s32 i;

    part->flag7 = flags & 1;
    if (flags & 0x80) {
        child = object->hierarchy;
        for (i = 1; i < object->hierarchy->index; i++) {
            child++;
            if (child->parent == part) {
                func_800AFA98(object, child, flags);
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFB4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFC68);

/* Set (or with mode bit 0x20 add to) a part's rotation (mode & 7 == 0),
 * translation (1) or scale (other) and mark it changed; with mode bit 0x80
 * also its descendants. */
void func_800AFD98(BattleObject *object, ModelPart *part, u8 mode, s16 x, s16 y, s16 z) {
    ModelPart *child;
    s32 i;

    if ((mode & 7) == 0) {
        if (mode & 0x20) {
            part->rotation.vx += x;
            part->rotation.vy += y;
            part->rotation.vz += z;
        } else {
            part->rotation.vx = x;
            part->rotation.vy = y;
            part->rotation.vz = z;
        }
    } else if ((mode & 7) == 1) {
        if (mode & 0x20) {
            part->translation[0] += x;
            part->translation[1] += y;
            part->translation[2] += z;
        } else {
            part->translation[0] = x;
            part->translation[1] = y;
            part->translation[2] = z;
        }
    } else if (mode & 0x20) {
        part->scale[0] += x;
        part->scale[1] += y;
        part->scale[2] += z;
    } else {
        part->scale[0] = x;
        part->scale[1] = y;
        part->scale[2] = z;
    }
    part->flag4 = 1;
    part->flag5 = 1;
    if (mode & 0x80) {
        child = object->hierarchy;
        for (i = 1; i < object->hierarchy->index; i++) {
            child++;
            if (child->parent == part) {
                func_800AFD98(object, child, mode, x, y, z);
            }
        }
    }
}

/* Put a battle object on the ground: find the scene triangle under its
 * hierarchy's translation and take its height (into the translation unless
 * field36 is set). */
void func_800AFF9C(BattleObject *object) {
    u8 out[16];
    SVector point;

    point.vx = object->hierarchy->translation[0];
    point.vy = object->hierarchy->translation[1];
    point.vz = object->hierarchy->translation[2];
    object->field1E = func_800A5914(&point, object->field1E, 4);
    if (object->field1E < 0) {
        object->field1E = func_800A579C(&point);
    }
    func_800A5870(&point, object->field1E, out);
    object->groundY = point.vy;
    if (object->field36 == 0) {
        object->hierarchy->translation[1] = point.vy;
    }
}

/* Free a battle object's packets (and its texture when it has one). */
void func_800B0060(BattleObject *object) {
    if (object->packets != NULL) {
        if (object->hasTexture) {
            func_8003852C(*(u8 **)(object->textureInfo + 8));
            object->hasTexture = 0;
        }
        func_800320E8(object->packets);
        object->packets = NULL;
        object->moreAnimations = NULL;
    }
}

/* Clear the words D_800C3BAC[0..8]. */
void func_800B00D0(void) {
    s32 i;

    for (i = 8; i >= 0; i--) {
        D_800C3BAC[i] = NULL;
    }
}

/* Release the nine effect entries of D_800C3BAC to pool, then clear them. */
void func_800B00F4(EffectPool *pool) {
    s32 i;

    for (i = 0; i < 9; i++) {
        if (D_800C3BAC[i] != NULL) {
            func_800A23E8(pool, D_800C3BAC[i]);
        }
    }
    func_800B00D0();
}

/* Start effect index (D_800C3BAC, taken from pool when unset) with its kind
 * and parameters, unless effects are disabled (D_800C37C8). */
void func_800B0164(EffectPool *pool, s32 index, u8 field2, u8 kind, u16 p0, u16 p1, u16 p2, u16 p3, u16 p4,
                   u16 p5, u16 field12) {
    EffectEntry **slots;
    EffectEntry **slot;
    EffectEntry *entry;

    if (D_800C37C8 == 0) {
        slots = D_800C3BAC;
        slot = &slots[index];
        if (*slot == NULL) {
            *slot = func_800A2330(pool);
        }
        entry = *slot;
        if (entry != NULL) {
            entry->used = 1;
            entry->field1 = 0;
            entry->field2 = field2;
            entry->kind = kind;
            entry->params[0] = p0;
            entry->params[1] = p1;
            entry->params[2] = p2;
            entry->params[3] = p3;
            entry->params[4] = p4;
            entry->params[5] = p5;
            entry->field10 = 0;
            entry->field12 = field12;
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B026C);

/* Whether a point (x at [0], z at [2]) lies strictly inside the scene's
 * bounds. */
s32 func_800B0AB4(s16 *point) {
    BattleSceneData *scene = D_800658C8;
    s16 x = point[0];
    s16 z;

    if (scene->minX < x && x < scene->maxX) {
        z = point[2];
        if (z > scene->minZ && z < scene->maxZ) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0B14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0D70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0FF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B10EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B12D0);

#ifdef NON_MATCHING
/* Wait frames (800BE790) until no stage object is busy (field38) and 800BF6F8
 * reports nothing pending; then, when an object holds packets, stop the
 * resident transfer (8002A498) and free their packets once it is idle. */
void func_800B136C(void) {
    s32 i;
    s32 busy = 0;
    s32 loaded = 0;
    s32 pending;

    for (;;) {
        for (i = 0; i < 11; i++) {
            if (D_800D3368[i] != NULL) {
                if (D_800D3368[i]->field38) {
                    busy = 1;
                }
                if (D_800D3368[i]->packets != NULL) {
                    loaded = 1;
                }
            }
        }
        if (func_800BF6F8() != 0) {
            busy = 1;
        }
        if (!busy) {
            break;
        }
        busy = 0;
        func_800BE790();
    }
    if (loaded) {
        func_8002A498(0);
        pending = 1;
        for (;;) {
            if (func_800286CC() == 0) {
                for (i = 0; i < 11; i++) {
                    if (D_800D3368[i] != NULL && D_800D3368[i]->packets != NULL) {
                        func_800B0060(D_800D3368[i]);
                    }
                }
                pending = 0;
            }
            if (!pending) {
                break;
            }
            func_800BE790();
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B136C);
#endif

/* Set the flag D_800C3D6C. */
void func_800B14B8(void) {
    D_800C3D6C = 1;
}

/* End the party members' stage objects other than keep: start their exit
 * effect (5), wait frames (800BE790) until none is active or busy and one more,
 * then free them. */
void func_800B14CC(s32 keep) {
    s32 i;
    s32 busy;

    for (i = 0; i < 3; i++) {
        if (i != keep) {
            func_800AA934(D_800D3368[i], D_800D3368[i], &D_800C3D0C, 5);
        }
    }
    do {
        busy = 0;
        for (i = 0; i < 3; i++) {
            if (i != keep && D_800D3368[i] != NULL && (D_800D3368[i]->active || D_800D3368[i]->field38)) {
                busy = 1;
            }
        }
        func_800BE790();
    } while (busy);
    func_800BE790();
    for (i = 0; i < 3; i++) {
        if (i != keep) {
            func_800A9FF0(i);
        }
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B15D8);

/* Address of entry index (0x1C bytes each) of a table with a 0xC-byte
 * header. */
u8 *func_800B168C(u8 *table, s32 index) {
    return table + (index * 0x1C + 0xC);
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B16A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B16F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1EA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1F6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B2AEC);

void func_800B3348(void) {
}

void func_800B3350(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3358);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B35C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3658);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B36BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B383C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3878);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B397C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B39C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3B6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3B94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3C2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3C74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3CD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3E04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3F04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B4EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B4F88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B50D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B51B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B56E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B572C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B57E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5924);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B59BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5C18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5CC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5DC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5DF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6004);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B61B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B61F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B626C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B62C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B639C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B63F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6464);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B64D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B65B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6808);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6930);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B69E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6B98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6BFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6C98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6CEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6DC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6E84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B73A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B73EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7424);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7C28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7E94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8048);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8054);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8068);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B81BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B838C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B853C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8774);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B88C4);

void func_800B89F4(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B89FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8D04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8D7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8DA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8EBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9020);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B905C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9B30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9B54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9C00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9F78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA4E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA59C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA614);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA768);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA8F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA984);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAB0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BABDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BACBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BADD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAEB8);

void func_800BAF40(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAF48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB080);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB13C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB248);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB350);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB540);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB620);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB690);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB6E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB760);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB7F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB9D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BBAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BBEE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC158);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC2F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC3F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC404);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC460);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCAA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCAD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCAFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCBB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCC60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCD8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCEAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD024);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD1FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD2E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD3AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD7A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD810);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD974);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDB74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDC14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDC78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDCF8);

void func_800BDD34(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDD3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDE58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE11C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE1C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE538);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE6A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE6E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE790);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEB04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEC18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BED30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BED4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEDE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEE2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEEB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEF24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEF8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEFF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF0B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF0C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF1EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF2B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF3A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF3E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF4F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF5E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF6CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF6F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF730);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF73C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF7C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF85C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF8CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF954);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF9EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFA9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFBA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFC80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFD88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFDA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C06E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0758);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C07CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0828);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C08CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0D18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0F70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0FAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C1140);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C11CC);
