#include "common.h"
#include "battle_core.h"

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80070F40);

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
        ready = D_800D2DE4;
        for (; slot < 11; ready++, slot++) {
            if (D_800D2DCC[slot] == 0 || *ready != 0) {
                continue;
            }
            step = 1;
            if ((D_800CCCE8[slot].status84 | D_800CCCE8[slot].status86) & 0x8000) {
                step = 2;
            }
            if (D_800CCCE8[slot].flags7C & 0x1000) {
                toggle = &D_800D2DF0[2][slot];
                if ((*toggle ^= 1) != 0) {
                    continue;
                }
            }
            flags = D_800CCCE8[slot].flags7C;
            if (flags & 0x2000) {
                delay = D_800CCCE8[slot].delay15C -= step;
                if (delay == 0) {
                    D_800CCCE8[slot].delay15C = 0;
                    D_800CCCE8[slot].flags7C &= 0xDFFF;
                }
                continue;
            }
            if ((flags & 0x80) || (D_800CCCE8[slot].flags80 & 0x1000)) {
                continue;
            }
            timer = &D_800D2DF0[1][slot];
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

    if (D_800D2DE4[actor] != 0xFF) {
        D_800D2DE4[actor] = 0;
    }
    D_800D2DF0[1][D_800C3EAC->actor] = func_80098AF8(D_800C3EAC->actor, 0);
    D_800D2DF0[0][D_800C3EAC->actor] = D_800D2DF0[1][D_800C3EAC->actor];
}

/* Render the pending battle message into its image, upload it and hold it
 * for three frames. */
void func_80071964(void) {
    s32 frames;

    if (D_800D2CAF != 0 && (D_800D2C94 & D_800C48E8) == 0) {
        frames = 3;
        D_800D39B8.width = func_80034EAC(func_80033728(D_800D39F0, D_800D2CAF),
                                         D_800D39B8.pixels, 0x39, 1);
        func_80044894(&D_800D39B8.rect, D_800D39B8.pixels);
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
    D_800D2D28->unkB5 = 0;
    D_800D2D28->unkB4 = 0;
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
                D_800D2DE4[member] = 0;
                D_800D2DF0[1][member] = D_800D2DF0[0][member];
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
            D_800D2DE4[D_800D2DC0 - 1] = 1;
            slot = D_800D2DC0;
            D_800D2DC0 = 0;
            D_800D2DF0[1][slot - 1] = 0;
        } else {
            D_800C3EAC->actor = 0;
            cursor = &D_800D2DD7;
            position = *cursor;
            do {
                slot = D_800D2DD8[position];
                if (D_800D2DE4[slot] == 1) {
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007252C);

/* Add every other primitive from `first` to the ordering table. */
void func_800728B8(POLY_FT4 *prims, s32 count, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        func_80043B48(D_800CCB04.ot + 1, &prims[first + i * 2]);
    }
}

/* Tint the current buffer's primitives from `first` to `last`: yellow for
 * mode 1, red otherwise. */
#ifdef NON_MATCHING
void func_80072938(POLY_FT4 *prims, s32 first, s32 last, u8 mode) {
    s32 i;

    for (i = first; i < last; i++) {
        func_80043C24(&prims[i * 2 + D_800CCB04.buffer], 0);
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072DA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072F38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073380);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073538);

/* When enabled, shade the current flat quad at graphics +0x63c8 grey by
 * +0x6410 and add it with its draw mode to the ordering table. */
#ifdef NON_MATCHING
void func_80073A58(void) {
    if (D_800C3EA4->unk6415 != 0) {
        D_800C3EA4->unk63C8[D_800C3EA4->unk6414].r0 = D_800C3EA4->unk6410;
        D_800C3EA4->unk63C8[D_800C3EA4->unk6414].g0 = D_800C3EA4->unk6410;
        D_800C3EA4->unk63C8[D_800C3EA4->unk6414].b0 = D_800C3EA4->unk6410;
        func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unk63C8[D_800C3EA4->unk6414]);
        func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unk63F8[D_800C3EA4->unk6414]);
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
            if (D_800C3EB4[member].unk2 != 0x7F) {
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
        func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk0[D_800C3EA4->unkA230->unk668]);
        func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk50[D_800C3EA4->unkA230->unk668]);
        func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unkA0[D_800C3EA4->unkA230->unk668]);
        func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unkF0[D_800C3EA4->unkA230->unk668]);
    }
}

/* Add the graphics block's current +0x320 and +0x370 quads to the ordering
 * table. */
void func_800744BC(void) {
    func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk320[D_800C3EA4->unkA230->buffer]);
    func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk370[D_800C3EA4->unkA230->buffer]);
}

/* Add the graphics block's current +0x280 and +0x2d0 quads to the ordering
 * table. */
void func_80074554(void) {
    func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk280[D_800C3EA4->unkA230->buffer]);
    func_80043B48(D_800CCB04.ot + 1, &D_800C3EA4->unkA230->unk2D0[D_800C3EA4->unkA230->buffer]);
}

INCLUDE_RODATA(".local/decomp/battle/asm/nonmatchings/battle", D_8006FAF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800745EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074AB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074D4C);

/* Draw the three primitive lists of *800d2db4 when UI +0xa8 is set. */
void func_80074EEC(void) {
    if (D_800D2D28->unkA8 != 0) {
        func_800728B8(D_800D2DB4->unk5550, D_800D2DB4->unk5D80[0], D_800D2DB4->unk5D8F[0]);
        func_800728B8(D_800D2DB4->unk5640, D_800D2DB4->unk5D80[1], D_800D2DB4->unk5D8F[1]);
        func_800728B8(D_800D2DB4->unk5C80, D_800D2DB4->unk5D80[2], D_800D2DB4->unk5D8F[2]);
    }
}

/* Add the current *800d3278 quad (UI +0xc8) and draw the 800d2dac object
 * (UI +0xc9). */
void func_80074F70(void) {
    if (D_800D2D28->unkC8 != 0) {
        func_80043B48(D_800CCB04.ot + 1, &D_800D3278->unk7A4[D_800D3278->unk7F4]);
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
            func_80043B48(D_800CCB04.ot + 1, &D_800D36C8[i].prims[D_800CCB04.buffer]);
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
    func_80044894(rect, pixels);
    func_800445D0(0);
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
    func_80043BFC(prim, 1);
    func_80043C24(prim, 0);
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
        func_80043CB0(&prims[i]);
        prims[i].r0 = 0x80;
        prims[i].g0 = 0x80;
        prims[i].b0 = 0x80;
        func_80043BFC(&prims[i], 0);
        func_80043C24(&prims[i], 1);
        switch (page) {
        case 0:
            prims[i].tpage = func_80043A1C(0, 0, 0x380, 0);
            break;
        case 1:
            prims[i].tpage = func_80043A1C(0, 0, 0x380, 0x100);
            break;
        case 2:
            prims[i].tpage = func_80043A1C(0, 0, 0x3C0, 0x100);
            break;
        case 3:
            prims[i].tpage = func_80043A1C(0, 0, 0x3C0, 0);
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
        func_80043CB0(&prims[i]);
        func_80043C24(&prims[i], 1);
        prims[i].r0 = 0xFF;
        prims[i].g0 = 0xFF;
        prims[i].b0 = 0xFF;
        prims[i].tpage = func_80043A1C(D_800C3EA4->textures[index].mode, 0, D_800C3EA4->textures[index].x,
                                       D_800C3EA4->textures[index].y);
        prims[i].clut = func_80043A58(D_800C3EA4->textures[index].clutX, D_800C3EA4->textures[index].clutY);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077454);

/* Allocate and clear the 0x670-byte graphics block, then initialise it. */
void func_80077610(void) {
    GraphicsBlock *block = (GraphicsBlock *)func_8008ABB8(0x670, 0);

    D_800C3EA4->unkA230 = block;
    func_8003F8E8(block, 0x670);
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
        func_80043CB0(&message->prims[i]);
        message->prims[i].r0 = 0x80;
        message->prims[i].g0 = 0x80;
        message->prims[i].b0 = 0x80;
        func_80043BFC(&message->prims[i], 0);
        func_80043C24(&message->prims[i], 1);
        message->alternate = row & 1;
        message->prims[i].clut = message->alternate ? D_80059414 : D_800595D4;
        message->prims[i].tpage = func_80043A1C(0, 0, 0x3C0, (s32)row / 2 * 13);
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
        if (D_800D2DCC[slot] != 0) {
            D_800D2DF0[0][slot] = D_800D2DF0[1][slot] = func_80098AF8(slot, 0);
        } else {
            D_800D2DF0[0][slot] = D_800D2DF0[1][slot] = 0xFF;
        }
        D_800D2DE4[slot] = 0;
        D_800D2DF0[2][slot] = 0;
        order[slot] = 0;
    }
}

/* Close the current event for `actor` with the action entry's parameter and
 * advance the event count. */
#ifdef NON_MATCHING
void func_800785D4(u8 actor, u8 index) {
    D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
    D_800C3FE8[D_800C3EAC->eventCount].param = D_800D2E5C[index].param | (D_800D2E5C[index].unk5 << 8);
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
    D_800C3FE8[D_800C3EAC->eventCount].param = value;
    D_800C3EAC->eventCount++;
}

/* Queue event type 0xf8 for `actor` with the pending message 800c3e8c - 1. */
void func_8007887C(u8 actor) {
    if (D_800C3E8C != 0) {
        D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0xF8;
        D_800C3FE8[D_800C3EAC->eventCount].param = D_800C3E8C - 1;
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
    D_800D2DE4[actor] = 0xFF;
    D_800C3D18[actor - 3].unk3 = 1;
    D_800CCCE8[actor].flags7C &= 0x8000;
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
    D_800C3FE8[D_800C3EAC->eventCount].targets = D_800D2E5C[index].targets;
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

    if (!(D_800CCCE8[enemy + 3].flags7C & 0x8000) || (D_800CCCE8[enemy + 3].unk34 & 0x800)) {
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
    D_800D2D28->unkB4 = 1;
    D_800D36C8[index].shown = 1;
}

/* Hide battle message window `index`. */
void func_80079E4C(u8 index) {
    D_800D2D28->unkB4 = 0;
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

    if (D_800D2DCC[slot] != 0 && D_800C3EB4[slot].hidden == 0 && !(D_800CCCE8[slot].flags7C & 0xC002)) {
        result = 1;
        if (any == 0) {
            status = D_800CCCE8[slot].status84 & 0x20;
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

    if (D_800D2DCC[slot] != 0 && !(D_800CCCE8[slot].flags7C & 0xC002)) {
        result = 1;
        if (any == 0) {
            status = D_800CCCE8[slot].status84 & 0x20;
            result = status == 0;
        }
    }
    return result;
}

/* Whether `slot` is present, visible and alive (+0x7c without 0xc000). */
u8 func_8007A744(u8 slot) {
    u8 result = 0;

    if (D_800D2DCC[slot] != 0 && D_800C3EB4[slot].hidden == 0) {
        result = (D_800CCCE8[slot].flags7C & 0xC000) == 0;
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
            D_800CCCE8[func_80079E7C(D_800D3400[enemy].vars[op[3]])].unk104;
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
        D_800CCCE8[slot].unk108 = D_800D3400[enemy].longs[(*pc)[1]];
    } else {
        D_800CCCE8[slot].unk104 = D_800D3400[enemy].longs[(*pc)[1]];
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

    D_800CCCE8[enemy + 3].unk14C = value;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BAE8);
#endif

/* AI action 3a: the enemy record's +0x156 = b1 | b2 << 8. */
void func_8007BB2C(u8 **pc, u8 enemy) {
    u16 value = (*pc)[1] | ((*pc)[2] << 8);

    D_800CCCE8[enemy + 3].unk156 = value;
}

/* AI action 3b: the enemy record's bytes +0x155, +0x153, +0x151 = b1, b2, b3. */
void func_8007BB70(u8 **pc, u8 enemy) {
    D_800CCCE8[enemy + 3].unk150[5] = (*pc)[1];
    D_800CCCE8[enemy + 3].unk150[3] = (*pc)[2];
    D_800CCCE8[enemy + 3].unk150[1] = (*pc)[3];
}

/* AI action 3c: the enemy record's bytes +0x154, +0x152, +0x150 = b1, b2, b3. */
void func_8007BBD8(u8 **pc, u8 enemy) {
    D_800CCCE8[enemy + 3].unk150[4] = (*pc)[1];
    D_800CCCE8[enemy + 3].unk150[2] = (*pc)[2];
    D_800CCCE8[enemy + 3].unk150[0] = (*pc)[3];
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
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800D2DF0[1][slot]) {
            lowest = D_800D2DF0[1][slot];
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
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800D2DF0[1][slot] && slot != enemy + 3) {
            lowest = D_800D2DF0[1][slot];
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
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800CCCE8[slot].hp) {
            lowest = D_800CCCE8[slot].hp;
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
        if (func_8007A628(slot, (*pc)[2]) && lowest >= D_800CCCE8[slot].hp) {
            lowest = D_800CCCE8[slot].hp;
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
        if (func_8007A744(slot) && D_800CCCE8[slot].unk56 == (*pc)[2]) {
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
            flags = D_800CCCE8[slot].flags7C;
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
        if (!(D_800CCCE8[slot].flags7C & 0xC000)) {
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
        if (D_800D2DCC[slot] != 0 && !(D_800CCCE8[slot].flags7C & 0xC000) && D_800C3EB4[slot].hidden == 0) {
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
        if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0 && lowest >= D_800CCCE8[slot].unk104) {
            lowest = D_800CCCE8[slot].unk104;
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
        if (func_8007A628(slot, (*pc)[2]) && D_800D32A0[slot].unk1 != 0 && lowest >= D_800CCCE8[slot].hp) {
            lowest = D_800CCCE8[slot].hp;
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
    BattleRecord *record = D_800CCCE8;
    u8 set = (*pc)[2] != 0;

    do {
        if (set) {
            record->unk7A |= func_80089BEC((*pc)[1] + 7);
        } else {
            record->unk7A &= ~func_80089BEC((*pc)[1] + 7);
        }
        record++;
    } while (record < &D_800CCCE8[3]);
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
    return D_800CCCE8[(*pc)[1]].flags7C >> 15;
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FB70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FBE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FCE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FD38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FDEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FE3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FEC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FF14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800800E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080AE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080B64);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080BD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080C6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080C94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008115C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081318);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081504);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800816F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008189C);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083FF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800841E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084548);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084750);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084B40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084D28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084DE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085084);

/* Whether slot b's slot-info +0xa is below slot a's. */
s32 func_80085310(u8 a, u8 b) {
    return D_800C3EB4[a].unkA > D_800C3EB4[b].unkA;
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
        D_800C3FE8[D_800C3EAC->eventCount].totals[slot] = 0;
        D_800C3FE8[D_800C3EAC->eventCount].totalCodes[slot] = 0xFF;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085618);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085B58);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085D34);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086B88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086C88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800877E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800879A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087A38);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008887C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088B80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089038);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089110);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800891E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089348);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008946C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008963C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800897CC);

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
        return func_8003FA38();
    }
    return low + (u16)func_8003FA38() % (span + 1);
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A144);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A274);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A3EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A684);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A9C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AA40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AA74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AAA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AB4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AB70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AB94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008ABB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AC00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AC88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008ADD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B168);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B224);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B478);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B908);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BC40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BC98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BD50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BED8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C360);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C3F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C4A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C81C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CCCC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CD28);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008F8F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FA60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FAD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FC1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FDE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FE18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009023C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090310);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800904A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009070C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009080C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009093C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090B90);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090E7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091064);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091604);

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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009413C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800941A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800946F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094D24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094EE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095690);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800957D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800958D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095A78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095B44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095BAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095D4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096494);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096824);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800968C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096AB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097610);

void func_8009795C(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097964);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097D08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097D5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009892C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098AF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098C6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098D2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099498);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800995A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099890);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099CF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099FB0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A074);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A1AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A2D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A7B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A7E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A9D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AA44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AB00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AB38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AC48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009ADA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AEFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AFD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B104);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B1E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B46C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B684);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009BAC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009BD94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009BE0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C050);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C0E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C198);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C4B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C9C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CA90);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CB68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D3A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DA04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DBFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E268);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E278);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E2EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E3C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E410);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E48C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E53C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E5C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E788);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E868);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009EBA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009EC4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009EF3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F1C4);

void func_8009F5B0(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F5B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F708);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F794);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A0838);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A1B50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A1CF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A216C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2234);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A22A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A22E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A23E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2434);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2704);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2ACC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2BB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2CA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2D1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2D5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2E88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2F94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2FD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A32D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3484);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3490);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3514);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A35C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3640);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3E98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A429C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4348);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A43F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A44C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4654);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4820);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A48EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4CF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4DB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A577C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A578C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A579C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5914);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5A48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5BE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5D54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5E9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5EB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6444);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A64E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6884);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6AE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A7064);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A7948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8A88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8B0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8BF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9540);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A96B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A979C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9F94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9FF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA320);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA384);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA514);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA650);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA6E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA760);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA788);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA79C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA7DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA820);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA898);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA934);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAA20);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAB34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAD54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800ADF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE1BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE220);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE2A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AEEEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AEEF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AEF68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF180);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF270);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF2C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF400);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF678);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFA98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFB4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFC68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFF9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0060);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B00D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B00F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0164);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B026C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0AB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0B14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0D70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0FF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B10EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B12D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B136C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B14B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B14CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B15D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B168C);

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
