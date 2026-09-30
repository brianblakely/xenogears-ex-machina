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
    thread->levelCaller = thread->caller[level];
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
    D_800D3278->threads[thread].caller[D_800D3278->threads[thread].level] = 0xFF;
    D_800D3278->threads[thread].priority[D_800D3278->threads[thread].level] = 0xFF;
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = 0xFFFF;
    D_800D3278->threads[thread].caller[0] = 1;
    D_800D3278->threads[thread].level = 0;
    D_800D3278->threads[thread].priority[0] = 7;
    D_800D3278->threads[thread].pc[0] = D_800D39D0->entries[thread].entry[1];
    D_800D3278->threads[thread].waitThread = 0xFF;
    return 0;
}

/* Opcode 01 (jump): continue the running level at the operand. */
s32 func_801E5CE4(s32 thread, u8 *insn) {
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = insn[1] + (insn[2] << 8);
    return 0;
}

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5D24);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5DCC);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5EF8);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E5F8C);

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

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E71D4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7230);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7278);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7314);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7358);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E736C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7380);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E73D4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7424);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E746C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E748C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E74A0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E74B8);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E74E0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E75F0);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7660);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7684);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7700);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E775C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E7770);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E77E4);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E786C);

INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E78A8);

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
