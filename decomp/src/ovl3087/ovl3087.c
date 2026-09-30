/* ovl3087 (Disc 1 slot 3087 / Disc 2 slot 3082), loaded at 801e5000: the
 * battle event script interpreter. The battle overlay loads it (80070e2c,
 * file list entry 1 through 800295d8) only when the formation sets
 * 800c3d48 (formation flag 0x20), then calls 801e5160 once to load the
 * script files and set up the threads, 801e879c every frame to run the
 * script threads, and 801e563c at the end to release everything. Opcode
 * handlers use the battle overlay's actor, camera and message services
 * (8007xxxx-800cxxxx) and resident file/heap/sound helpers. */
#include "ovl3087.h"

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5160);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E563C);

/* Make the thread's highest occupied level current; return its pc. */
u16 func_801E5768(ScriptThread *thread) {
    s32 i;
    s32 level;
    u32 free;

    i = 0;
    free = 0xFF;
    for (; i < 8; i++) {
        if (thread->priority[i] < free) {
            level = i;
        }
    }
    thread->level = level;
    thread->runningEntry = thread->entry[level];
    return thread->pc[level];
}

/* First free level above the base level (8 when none is free). */
u16 func_801E57C4(ScriptThread *thread) {
    s32 i;

    for (i = 1; i < 8; i++) {
        if (thread->priority[i] == 0xFF) {
            break;
        }
    }
    return i;
}

/* Decode count 16-bit operands following the opcode into the operand
 * slots. Each is an immediate when its bit (from 0x80 down) is set in
 * immediateMask, otherwise a variable offset. In the signed form bit 15
 * marks an immediate instead. */
void func_801E57F8(u8 *insn, u8 count, u8 immediateMask, u8 signedForm) {
    s32 i;
    s16 raw;

    for (i = 0; i < count; i++) {
        if (signedForm) {
            raw = insn[i * 2 + 1] + (insn[i * 2 + 2] << 8);
            if (raw & 0x8000) {
                D_800D3278->operands[i] = raw & 0x7FFF;
            } else {
                D_800D3278->operands[i] = D_800D3278->vars[(s16)(raw / 2)];
            }
        } else if ((immediateMask << i) & 0x80) {
            D_800D3278->operands[i] = insn[i * 2 + 1] + (insn[i * 2 + 2] << 8);
        } else {
            D_800D3278->operands[i] =
                SCRIPT_VAR(D_800D3278, (insn[i * 2 + 2] << 8) | insn[i * 2 + 1]);
        }
    }
}

/* Compare two script values with condition op (low four bits). */
u8 func_801E58EC(s16 a, s16 b, u8 op) {
    s32 result = 0;

    switch (op & 0xF) {
    case 0:
        if (a == b) {
            result = 1;
        }
        break;
    case 1:
        if (a != b) {
            result = 1;
        }
        break;
    case 2:
        if (a > b) {
            result = 1;
        }
        break;
    case 3:
        if (a < b) {
            result = 1;
        }
        break;
    case 4:
        if (a >= b) {
            result = 1;
        }
        break;
    case 5:
        if (a <= b) {
            result = 1;
        }
        break;
    case 6:
        if (a & b) {
            result = 1;
        }
        break;
    case 7:
        if (a != b) {
            result = 1;
        }
        break;
    case 8:
        if (a | b) {
            result = 1;
        }
        break;
    case 9:
        if (func_80089C9C((u16)a, (u8)b)) {
            result = 1;
        }
        break;
    case 10:
        if (!func_80089C9C((u16)a, (u8)b)) {
            result = 1;
        }
        break;
    }
    return result;
}

/* Battle slot of a script actor id: party characters (ids below 16) by
 * their position in the party, enemies as id - 13. */
u8 func_801E5A98(s32 id) {
    s32 slot = 0;
    s32 i;

    if ((u8)id < 16) {
        for (i = 0; i < 3; i++) {
            if (D_800D2D24[i] != 0xFF && D_800D2D24[i] == (u8)id) {
                slot = i;
                break;
            }
        }
    } else {
        slot = id - 13;
    }
    return slot;
}

#ifdef NON_MATCHING
/* Show the next of five portraits at (x, y) and mirror the current
 * buffer's quad horizontally by swapping its second and third vertices.
 * (The original reloads the buffer index after every store.) */
void func_801E5B00(s16 x, s16 y) {
    BattleGraphics *graphics;
    s16 x1;
    s16 y1;

    if (--D_801E9C1C < 0) {
        D_801E9C1C = 4;
    }
    D_800D2D28->portraitHandle =
        func_80076A10(D_801E9C1C + 0xE0, D_800C3EA4->portrait, x, y);
    graphics = D_800C3EA4;
    x1 = graphics->portrait[D_800CCB34].x1;
    graphics->portrait[D_800CCB34].x1 = graphics->portrait[D_800CCB34].x2;
    y1 = graphics->portrait[D_800CCB34].y1;
    graphics->portrait[D_800CCB34].y1 = graphics->portrait[D_800CCB34].y2;
    graphics->portrait[D_800CCB34].x2 = x1;
    graphics->portrait[D_800CCB34].y2 = y1;
    D_800D2D28->portraitBuffer = D_800CCB34;
    D_800D2D28->portraitShown = 1;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5B00);
#endif

/* Opcode 00 (end): drop the running level and restart the thread's base
 * level at its idle entry. Yields. */
s32 func_801E5C1C(s32 thread) {
    D_800D3278->threads[thread].entry[D_800D3278->threads[thread].level] = 0xFF;
    D_800D3278->threads[thread].priority[D_800D3278->threads[thread].level] = 0xFF;
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = 0xFFFF;
    D_800D3278->threads[thread].entry[0] = 1;
    D_800D3278->threads[thread].level = 0;
    D_800D3278->threads[thread].priority[0] = 7;
    D_800D3278->threads[thread].pc[0] = D_800D39D0->entries[thread].entry[1];
    D_800D3278->threads[thread].request = 0xFF;
    return 0;
}

/* Opcode 01 (jump): continue the running level at the operand. */
s32 func_801E5CE4(s32 thread, u8 *insn) {
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = insn[1] + (insn[2] << 8);
    return 0;
}

/* Opcode 02 (branch unless): jump to the target when the comparison of the
 * two operands fails. */
s32 func_801E5D24(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    if (func_801E58EC(D_800D3278->operands[0], D_800D3278->operands[1], insn[5])) {
        return 8;
    }
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = insn[6] + (insn[7] << 8);
    return 0;
}

#ifdef NON_MATCHING
/* Opcode 03 (request): start entry (low five bits) of another thread on a
 * free level with the priority in the top three bits. Retries (length 0)
 * while that thread has no free level. (Register allocation of the entry
 * table lookup differs.) */
s32 func_801E5DCC(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 level = func_801E57C4(&D_800D3278->threads[insn[1]]);

    if (level != 8) {
        D_800D3278->threads[thread].request = insn[2] & 0x1F;
        D_800D3278->threads[insn[1]].priority[level] = insn[2] >> 5;
        D_800D3278->threads[insn[1]].pc[level] =
            D_800D39D0->entries[insn[1]].entry[D_800D3278->threads[thread].request];
        length = 3;
        D_800D3278->threads[insn[1]].entry[level] = D_800D3278->threads[thread].request;
    }
    return length;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5DCC);
#endif

/* Opcode 04 (request and wait for start): issue the request, then wait
 * until the other thread is running the requested entry. */
s32 func_801E5EF8(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 request = D_800D3278->threads[thread].request;

    if (request != (insn[2] & 0x1F)) {
        func_801E5DCC(thread, insn);
    } else if (request == D_800D3278->threads[insn[1]].runningEntry) {
        D_800D3278->threads[thread].request = 0xFF;
        length = 3;
    }
    return length;
}

/* Opcode 05 (request and wait for end): issue the request, then wait until
 * the requested entry is neither queued nor running on the other thread. */
s32 func_801E5F8C(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 request = D_800D3278->threads[thread].request;
    u8 finished = 1;
    s32 i;

    if (request != (insn[2] & 0x1F)) {
        func_801E5DCC(thread, insn);
    } else {
        for (i = 0; i < 8; i++) {
            if (D_800D3278->threads[insn[1]].entry[i] == request) {
                finished = 0;
                break;
            }
        }
        if (finished &&
            D_800D3278->threads[thread].request != D_800D3278->threads[insn[1]].runningEntry) {
            length = 3;
            D_800D3278->threads[thread].request = 0xFF;
        }
    }
    return length;
}

/* Opcode 06: var = value. */
s32 func_801E6084(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) = D_800D3278->operands[1];
    return 6;
}

/* Opcode 07: var = 1. */
s32 func_801E60E8(s32 thread, u8 *insn) {
    INSN_VAR(insn) = 1;
    return 3;
}

/* Opcode 08: var = 0. */
s32 func_801E6118(s32 thread, u8 *insn) {
    INSN_VAR(insn) = 0;
    return 3;
}

/* Opcode 09: var += value. */
s32 func_801E6144(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) += D_800D3278->operands[1];
    return 6;
}

/* Opcode 0a: var -= value. */
s32 func_801E61B4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) -= D_800D3278->operands[1];
    return 6;
}

/* Opcode 0b: var |= value. */
s32 func_801E6224(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) |= D_800D3278->operands[1];
    return 6;
}

/* Opcode 0c: var &= ~value. */
s32 func_801E6294(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) &= ~D_800D3278->operands[1];
    return 6;
}

/* Opcode 0d: var++. */
s32 func_801E6304(s32 thread, u8 *insn) {
    INSN_VAR(insn)++;
    return 3;
}

/* Opcode 0e: var--. */
s32 func_801E633C(s32 thread, u8 *insn) {
    INSN_VAR(insn)--;
    return 3;
}

/* Opcode 0f: var &= value. */
s32 func_801E6374(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) &= D_800D3278->operands[1];
    return 6;
}

/* Opcode 10: var |= value. */
s32 func_801E63E4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) |= D_800D3278->operands[1];
    return 6;
}

/* Opcode 11: var ^= value. */
s32 func_801E6454(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) ^= D_800D3278->operands[1];
    return 6;
}

/* Opcode 12: var <<= count. */
s32 func_801E64C4(s32 thread, u8 *insn) {
    s32 index = ((insn[2] << 8) | insn[1]) >> 1;

    func_801E57F8(insn, 2, 0, 0);
    D_800D3278->vars[index] <<= D_800D3278->operands[1];
    return 5;
}

/* Opcode 13: var >>= count. */
s32 func_801E6534(s32 thread, u8 *insn) {
    s32 index = ((insn[2] << 8) | insn[1]) >> 1;

    func_801E57F8(insn, 2, 0, 0);
    D_800D3278->vars[index] >>= D_800D3278->operands[1];
    return 5;
}

/* Opcode 14: var = random 0..7fff. */
s32 func_801E65A4(s32 thread, u8 *insn) {
    INSN_VAR(insn) = func_80089B50(0, 0x7FFF);
    return 3;
}

/* Opcode 15: var (second operand) = random 0..limit. */
s32 func_801E65FC(s32 thread, u8 *insn) {
    SCRIPT_VAR(D_800D3278, (insn[4] << 8) | insn[3]) = func_80089B50(0, insn[1] | (insn[2] << 8));
    return 5;
}

/* Opcode 16: var = a * b. */
s32 func_801E6660(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) = D_800D3278->operands[0] * D_800D3278->operands[1];
    return 6;
}

/* Opcode 17: var = a / b (signed). */
s32 func_801E66D8(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) = (s32)D_800D3278->operands[0] / (s32)D_800D3278->operands[1];
    return 6;
}

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E6750);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E6CE8);

/* Opcode 18: show a message from the thread's speaker; repeats until the
 * message is done. */
s32 func_801E71D4(s32 thread, u8 *insn) {
    return (func_801E6CE8(insn[1] | (insn[2] << 8), D_800D3278->threads[thread].speaker, insn[3]) != 0) * 4;
}

/* Opcode 19: show a message from the given actor; repeats until done. */
s32 func_801E7230(s32 thread, u8 *insn) {
    return func_801E6CE8(insn[2] | (insn[3] << 8), insn[1], insn[4]) ? 5 : 0;
}

/* Opcode 1a: set the message window layout; zero operands take the
 * defaults. */
s32 func_801E7278(s32 thread, u8 *insn) {
    s32 i;

    func_801E57F8(insn, 5, 0, 1);
    for (i = 0; i < 4; i++) {
        if (D_800D3278->operands[i] != 0) {
            D_800D3278->window[i] = D_800D3278->operands[i];
        } else {
            D_800D3278->window[i] = D_801E9C10[i];
        }
    }
    D_800D3278->window[4] = D_800D3278->operands[4];
    return 11;
}

/* Opcode 1b: set the thread's speaker (f3-f5 name the party members). */
s32 func_801E7314(s32 thread, u8 *insn) {
    u8 actor = insn[1];

    if (actor >= 0xF3) {
        actor = D_800D2D24[actor - 0xF3];
    }
    D_800D3278->threads[thread].speaker = actor;
    return 2;
}

/* Opcode 1c. */
s32 func_801E7358(s32 thread, u8 *insn) {
    D_800C3E4C = 2;
    return 1;
}

/* Opcode 1d. */
s32 func_801E736C(s32 thread, u8 *insn) {
    D_800C3E4C = 1;
    return 1;
}

/* Opcode 1e: flash the actor white. */
s32 func_801E7380(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_800B39C0(D_800D3278->operands[0], 2, 0xFF, 0xFF, 0xFF);
    return 3;
}

/* Opcode 1f: flash the actor black. */
s32 func_801E73D4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_800B39C0(D_800D3278->operands[0], 2, 0, 0, 0);
    return 3;
}

/* Opcode 49: set 8005942c. */
s32 func_801E7424(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    D_8005942C = D_800D3278->operands[0];
    return 3;
}

/* Opcode 20: end the battle (800c3d44) and halt the script. */
s32 func_801E746C(s32 thread, u8 *insn) {
    D_800C3D44 = 1;
    D_800D3278->halted = 1;
    return 1;
}

/* Opcode 21: set 800d2d50. */
s32 func_801E748C(s32 thread, u8 *insn) {
    D_800D2D50 = 1;
    return 1;
}

/* Opcode 22. */
s32 func_801E74A0(s32 thread, u8 *insn) {
    D_800D3278->unk801 = 2;
    return 1;
}

/* Opcode 37: end the battle through 800d2fc4 and 800c48ea and halt the script. */
s32 func_801E74B8(s32 thread, u8 *insn) {
    D_800D2FC4 = 1;
    D_800D3278->halted = 1;
    D_800C48EA = 1;
    return 1;
}

/* Opcode 23: move a party member (f3-f5) to a position and wait until the
 * move is done. */
s32 func_801E74E0(s32 thread, u8 *insn) {
    s32 length = 0;

    func_801E57F8(insn, 3, 0, 1);
    switch (D_800D3278->threads[D_800D3278->operands[0] - 0xF3].memberState) {
    case 0:
        D_800D3278->threads[D_800D3278->operands[0] - 0xF3].memberState = 2;
        func_800AA384(D_800D3278->operands[0] - 0xF3, func_80089C08(D_800D3278->operands[1] + 0xD),
                      D_800D3278->operands[2]);
        break;
    case 1:
        length = 7;
        func_800B8D04();
        func_800BFBA0();
        D_800D3278->threads[D_800D3278->operands[0] - 0xF3].memberState = 0;
        break;
    }
    return length;
}

/* Opcode 38: start a party member (f3-f5) moving without waiting. */
s32 func_801E75F0(s32 thread, u8 *insn) {
    func_801E57F8(insn, 3, 0, 1);
    func_800AA320(D_800D3278->operands[0] - 0xF3, func_80089C08(D_800D3278->operands[1] + 0xD),
                  D_800D3278->operands[2]);
    return 7;
}

/* Opcode 4a. */
s32 func_801E7660(s32 thread, u8 *insn) {
    func_8009C0E0(0);
    return 1;
}

/* Opcode 4b: set bit 0 of the actor's battle record flags (0x36). */
s32 func_801E7684(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    D_800CCD1E[func_801E5A98((u8)D_800D3278->operands[0])].flags |= 1;
    return 3;
}

/* Opcode 24. */
s32 func_801E7700(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    D_8005947C = D_800D3278->operands[0] + 1;
    D_8005954C = D_800D3278->operands[1];
    return 5;
}

/* Opcode 25. */
s32 func_801E775C(s32 thread, u8 *insn) {
    D_800C3D5C = 1;
    return 1;
}

/* Opcode 26: store four values at 8006f94e and apply them (8001ac94). */
s32 func_801E7770(s32 thread, u8 *insn) {
    func_801E57F8(insn, 4, 0, 1);
    D_8006F94E[0] = D_800D3278->operands[0];
    D_8006F94E[1] = D_800D3278->operands[1];
    D_8006F94E[2] = D_800D3278->operands[2];
    D_8006F94E[3] = D_800D3278->operands[3];
    func_8001AC94();
    return 9;
}

/* Opcode 27. */
s32 func_801E77E4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 4, 0, 1);
    D_8004FE44[0] = D_800D3278->operands[0] | 0x80;
    D_8004FE44[1] = D_800D3278->operands[1];
    D_8004FE44[2] = 1;
    D_8004FE44[3] = D_800D3278->operands[2];
    D_800D3338 = 1;
    D_80062514 = D_800D3278->operands[3];
    return 9;
}

/* Opcode 28: tint an actor with an explicit mode and colour. */
s32 func_801E786C(s32 thread, u8 *insn) {
    func_800B39C0(insn[5], insn[1], insn[2], insn[3], insn[4]);
    return 6;
}

/* Opcode 29. */
s32 func_801E78A8(s32 thread, u8 *insn) {
    u16 position[3];

    func_801E57F8(insn, 4, 0, 1);
    position[0] = D_800D3278->operands[0];
    position[1] = D_800D3278->operands[1];
    position[2] = D_800D3278->operands[2];
    func_800B3658(position, D_800D3278->operands[3]);
    return 9;
}

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7914);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E79E0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7A5C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7B08);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7B2C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7B58);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7C0C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7CD0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7DE4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7E14);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7E5C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7EA4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7F08);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7F70);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7FF4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E8074);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E807C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E80E8);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E80F0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E818C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E81EC);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E823C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E828C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E82DC);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E83C0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E84A4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E8600);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E86AC);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E86D0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E86F4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E8718);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E8750);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E879C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E93E8);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9430);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E950C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9550);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E958C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E95B0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E95E4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9694);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9700);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9760);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9894);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9958);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9978);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9AD4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E9B2C);
