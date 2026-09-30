/* ovl3087 (Disc 1 slot 3087 / Disc 2 slot 3082), loaded at 801e5000: the
 * battle event script interpreter. The battle overlay loads it (80070e2c,
 * file list entry 1 through 800295d8) only when the formation sets
 * 800c3d48 (formation flag 0x20), then calls 801e5160 once to load the
 * script files and set up the threads, 801e879c every frame to run the
 * script threads, and 801e563c at the end to release everything. Opcode
 * handlers use the battle overlay's actor, camera and message services
 * (8007xxxx-800cxxxx) and resident file/heap/sound helpers. */
#include "ovl3087.h"

/* Load the script set of 8006f9df (script archive file 2) and the model
 * archive (file 3), set up the interpreter state and its threads, the
 * portrait quads and the script's sound bank (file 4). */
void func_801E5160(void) {
    FileRequest files[3];
    ScriptArchive *archive;
    ScriptFile *script;
    s32 i;
    s32 level;

    func_800716D8();
    func_8008AB4C();
    archive = func_8008ABB8(func_800288EC(2), 1);
    files[0].file = 2;
    files[0].dest = archive;
    D_801E9C38 = func_8008ABB8(func_800288EC(3), 0);
    files[1].file = 3;
    files[1].dest = D_801E9C38;
    files[2].file = 0;
    files[2].dest = NULL;
    func_80029AFC(files, 0, 0x80);
    func_8008AC50();
    func_8003342C(archive);
    func_8003342C(D_801E9C38);
    script = func_80032E88(((ScriptSet *)((u8 *)archive + D_8006F9DF.scriptSet * 8))->script, 0);
    D_800D3340 = func_80032E88(((ScriptSet *)((u8 *)archive + D_8006F9DF.scriptSet * 8))->data, 0);
    func_800320E8(archive);
    D_800D3278 = func_8008ABB8(sizeof(ScriptState), 0);
    func_8003F8E8(D_800D3278, sizeof(ScriptState));
    D_800D2DAC = func_8008ABB8(0x98, 0);
    func_8003F8E8(D_800D2DAC, 0x78);
    D_800D39D0 = script;
    D_800D3278->code = (u8 *)D_800D39D0 + D_800D39D0->threadCount * 16 + 0x44;
    for (i = 0; i < 16; i++) {
        D_800D3278->order[i] = 0xFF;
    }
    for (i = 0; i < D_800D39D0->threadCount; i++) {
        for (level = 0; level < 8; level++) {
            D_800D3278->threads[i].pc[level] = 0xFFFF;
            D_800D3278->threads[i].priority[level] = 0xFF;
            D_800D3278->threads[i].entry[level] = 0xFF;
        }
        D_800D3278->order[i] = i;
        D_800D3278->threads[i].pc[0] = D_800D39D0->entries[i].entry[0];
        D_800D3278->threads[i].priority[0] = 0;
        D_800D3278->threads[i].order = 0xFF;
        D_800D3278->threads[i].entry[0] = 0;
        D_800D3278->threads[i].level = 0;
        D_800D3278->threads[i].request = 0xFF;
        D_800D3278->threads[i].speaker = 0xFF;
    }
    D_800D3278->unk7F5 = 4;
    for (i = 0; i < 5; i++) {
        D_800D3278->window[i] = D_801E9C10[i];
    }
    for (i = 0; i < 2; i++) {
        func_80043CB0(&D_800D3278->quads[i]);
        setRGB0(&D_800D3278->quads[i], 0x80, 0x80, 0x80);
        func_80043BFC(&D_800D3278->quads[i], 0);
        func_80043C24(&D_800D3278->quads[i], 1);
        D_800D3278->quads[i].clut = func_80043A58(0, 0x1D0);
        D_800D3278->quads[i].tpage = func_80043A1C(1, 0, 0x3C0, 0x100);
    }
    D_800D2D28->unkCA = 1;
    D_800D2D28->unkCF = 0;
    for (i = 0; i < 16; i++) {
        D_800D3278->actionRunning[i] = 0;
        D_801E9C20[i] = 0;
    }
    D_800D3278->musicPlaying = 0;
    func_8008AB4C();
    D_800D3278->soundBank = func_8008ABB8(func_800288EC(4), 0);
    func_800295D8(4, D_800D3278->soundBank, 0, 0x80);
    func_8008AC50();
    func_80038428(D_800D3278->soundBank);
    func_8003BDFC(0x10);
    D_800D3278->soundBankLoaded = 1;
    D_800C4924 = D_800D3278->soundBank;
    func_800BFBA0();
}

/* Release the interpreter state, the script and model files, stop the
 * music and release the sound bank. Returns whether music was playing. */
s32 func_801E563C(void) {
    s32 musicWasPlaying;

    func_800320E8(D_800D3278);
    musicWasPlaying = 0;
    func_800320E8(D_800D2DAC);
    func_800320E8(D_800D39D0);
    func_800320E8(D_800D3340);
    func_800320E8(D_801E9C38);
    if (D_800D3278->musicPlaying != 0) {
        musicWasPlaying = 1;
        func_80039C4C(D_800C3E54);
        func_800716D8();
        func_800399D4(D_800C3E54);
        func_800716D8();
    }
    if (D_800D3278->soundBankLoaded != 0) {
        func_8003A094(D_800D3278->soundBank);
        func_8003852C(D_800D3278->soundBank);
        func_800716D8();
        func_800320E8(D_800D3278->soundBank);
        D_800D3278->soundBankLoaded = 0;
    }
    return musicWasPlaying;
}

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

/* Show the next of five portraits at (x, y) and mirror the current
 * buffer's quad horizontally by swapping its second and third vertices.
 */
void func_801E5B00(s16 x, s16 y) {
    BattleGraphics *graphics;
    s32 x1;
    s32 y1;

    if (--D_801E9C1C < 0) {
        D_801E9C1C = 4;
    }
    D_800D2D28->portraitHandle =
        func_80076A10(D_801E9C1C + 0xE0, D_800C3EA4->portrait, x, y);
    graphics = D_800C3EA4;
    x1 = graphics->portrait[D_800CCB34.index].x1;
    y1 = graphics->portrait[D_800CCB34.index].y1;
    graphics->portrait[D_800CCB34.index].x1 = graphics->portrait[D_800CCB34.index].x2;
    graphics->portrait[D_800CCB34.index].y1 = graphics->portrait[D_800CCB34.index].y2;
    graphics->portrait[D_800CCB34.index].x2 = x1;
    graphics->portrait[D_800CCB34.index].y2 = y1;
    D_800D2D28->portraitBuffer = D_800CCB34.index;
    D_800D2D28->portraitShown = 1;
}

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

/* Load the portrait TIM of an actor (mirrored or not) into VRAM (clut
 * 0,1d0; pixels 3c0,100) and lay the current buffer's portrait quad out
 * in a 64x64 box inside the window at (x, y). */
void func_801E6750(u8 actor, s32 flags, s32 x, s32 y, s32 width) {
    TimImage tim;
    s32 mirrored = flags & 1;
    s32 file = D_801E9B5C[actor * 2 + mirrored] + 0x46;
    void *data;

    func_80028470(4, 0);
    data = func_8008ABB8(func_800288EC(file), 1);
    func_800295D8(file, data, 0, 0x80);
    func_8008AC50();
    func_800471B4(data);
    func_800471C4(&tim);
    tim.crect->x = 0;
    tim.crect->y = 0x1D0;
    tim.prect->x = 0x3C0;
    tim.prect->y = 0x100;
    func_80044894(tim.crect, tim.caddr);
    func_80044894(tim.prect, tim.paddr);
    func_800445D0(0);
    func_800320E8(data);
    if (mirrored) {
        setXY4(&D_800D3278->quads[D_800CCB34.index], x + width - 4, y + 4, x + width - 0x44, y + 4,
               x + width - 4, y + 0x44, x + width - 0x44, y + 0x44);
        setUV4(&D_800D3278->quads[D_800CCB34.index], 0, 0, 0x3F, 0, 0, 0x40, 0x3F, 0x40);
    } else {
        setXY4(&D_800D3278->quads[D_800CCB34.index], x + 4, y + 4, x + 0x44, y + 4, x + 4, y + 0x44,
               x + 0x44, y + 0x44);
        setUV4(&D_800D3278->quads[D_800CCB34.index], 0, 0, 0x40, 0, 0, 0x40, 0x40, 0x40);
    }
    D_800D3278->portraitBuffer = D_800CCB34.index;
}

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

/* Opcode 35: load model n of the model archive into actor slot a (thread
 * a + 13) unless one is loaded. */
s32 func_801E7914(s32 thread, u8 *insn) {
    s32 info[2];
    s32 slot;
    void *file;

    func_801E57F8(insn, 2, 0, 1);
    slot = (u8)(D_800D3278->operands[0] + 13);
    if (D_800D3278->threads[slot].modelLoaded == 0) {
        file = func_80032E88(D_801E9C38->entries[D_800D3278->operands[1]], 0);
        D_800D3278->threads[slot].modelFile = file;
        D_800D3278->threads[slot].model = func_801E9978(file, info);
        D_800D3278->threads[slot].modelLoaded = 1;
    }
    return 5;
}

/* Opcode 2a: play an animation on the slot's loaded model. */
s32 func_801E79E0(s32 thread, u8 *insn) {
    s32 slot;

    func_801E57F8(insn, 2, 0, 1);
    slot = (u8)(D_800D3278->operands[0] + 13);
    if (D_800D3278->threads[slot].modelLoaded != 0) {
        func_801E9958(D_800D3278->threads[slot].model, D_800D3278->operands[1]);
    }
    return 5;
}

/* Release the slot's loaded model and its data. */
void func_801E7A5C(s32 thread, u8 *insn) {
    s32 slot;

    func_801E57F8(insn, 1, 0, 1);
    slot = (u8)(D_800D3278->operands[0] + 13);
    if (D_800D3278->threads[slot].modelLoaded != 0) {
        func_801E9AD4(D_800D3278->threads[slot].model);
        func_800320E8(D_800D3278->threads[slot].modelFile);
        D_800D3278->threads[slot].modelLoaded = 0;
    }
}

/* Opcode 36: release a model. */
s32 func_801E7B08(s32 thread, u8 *insn) {
    func_801E7A5C(thread, insn);
    return 3;
}

/* Opcode 40: release a model and 801e9b2c's state. */
s32 func_801E7B2C(s32 thread, u8 *insn) {
    func_801E7A5C(thread, insn);
    func_801E9B2C();
    return 3;
}

/* Opcode 2b: wait n half-frames. */
s32 func_801E7B58(s32 thread, u8 *insn) {
    s32 length = 0;

    if (D_800D3278->threads[thread].waiting == 0) {
        func_801E57F8(insn, 1, 0, 1);
        D_800D3278->threads[thread].waitTimer = D_800D3278->operands[0] * 2;
        D_800D3278->threads[thread].waiting = 1;
    }
    if (D_800D3278->threads[thread].waitTimer == 0) {
        D_800D3278->threads[thread].waiting = 0;
        length = 3;
    }
    return length;
}

/* Opcode 2c: set the thread's run-order request; fe moves it to the front
 * of the run order at once. */
s32 func_801E7C0C(s32 thread, u8 *insn) {
    u8 order[16];
    s32 i;
    s32 next;

    D_800D3278->threads[thread].order = insn[1];
    if (insn[1] == 0xFE) {
        for (i = 0; i < 16; i++) {
            order[i] = D_800D3278->order[i];
        }
        next = 1;
        D_800D3278->order[0] = thread;
        for (i = 0; i < 16; i++) {
            if (order[i] != thread) {
                D_800D3278->order[next++] = order[i];
            }
        }
    }
    return 3;
}

/* Stop the current music and start sequence music (file music + 4) at a
 * volume. */
void func_801E7CD0(s16 music, u8 volume) {
    s32 size;

    func_8001B66C();
    if (D_800D3278->musicPlaying != 0) {
        func_800399D4(D_800C3E54);
        func_800716D8();
    }
    func_8008AB70();
    size = func_800288EC(music + 4);
    D_800D3278->music = D_80062648;
    func_800295D8(music + 4, D_80062648, 0, 0x80);
    func_8008AC50();
    func_8003F99C(D_80062648, D_800D3278->music, size);
    D_800D3278->musicPlaying = 1;
    D_800D3278->musicId = music;
    D_800D3278->musicVolume = volume;
    D_800C3E54 = func_800397FC(D_80062648, volume, 0);
}

/* Fade the music to a volume. */
void func_801E7DE4(s32 volume, s32 time) {
    func_8003A89C(D_800C3E54, volume, time);
}

/* Opcode 2d: start music at full volume. */
s32 func_801E7E14(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E7CD0(D_800D3278->operands[0], 0x7F);
    return 3;
}

/* Opcode 2e: start music silent. */
s32 func_801E7E5C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E7CD0(D_800D3278->operands[0], 0);
    return 3;
}

/* Opcode 2f: fade the music to a volume over a time. */
s32 func_801E7EA4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    D_800D3278->musicVolume = D_800D3278->operands[0];
    func_801E7DE4(D_800D3278->operands[0], D_800D3278->operands[1]);
    return 5;
}

/* Opcode 30: set the music volume to its stored level (operand 0) or
 * silence it. */
s32 func_801E7F08(s32 thread, u8 *insn) {
    u8 volume = 0;

    func_801E57F8(insn, 1, 0, 1);
    if (D_800D3278->operands[0] == 0) {
        volume = D_800D3278->musicVolume;
    }
    func_801E7DE4(volume, 0);
    return 3;
}

/* Opcode 31: play a sound effect from the script bank (or the resident
 * bank when the fourth operand is set). */
s32 func_801E7F70(s32 thread, u8 *insn) {
    SoundBank *bank;

    func_801E57F8(insn, 4, 0, 1);
    if (D_800D3278->operands[3] == 0) {
        bank = D_800D3278->soundBank;
    } else {
        bank = D_8005919C;
    }
    func_80039F18((bank->id << 16) | D_800D3278->operands[0], D_800D3278->operands[1],
                  D_800D3278->operands[2]);
    return 9;
}

/* Opcode 41: stop a sound effect of the script or resident bank. */
s32 func_801E7FF4(s32 thread, u8 *insn) {
    SoundBank *bank;

    func_801E57F8(insn, 3, 0, 1);
    if (D_800D3278->operands[2] == 0) {
        bank = D_800D3278->soundBank;
    } else {
        bank = D_8005919C;
    }
    func_8003A2E4((bank->id << 16) | D_800D3278->operands[0], D_800D3278->operands[1]);
    return 7;
}

/* Opcode 32: no operation. */
s32 func_801E8074(s32 thread, u8 *insn) {
    return 1;
}

/* Opcode 33: stop the music. */
s32 func_801E807C(s32 thread, u8 *insn) {
    if (D_800D3278->musicPlaying != 0) {
        func_80039C4C(D_800C3E54);
        func_800716D8();
        func_800399D4(D_800C3E54);
        D_800D3278->musicPlaying = 0;
    }
    return 1;
}

/* Opcode 34: no operation. */
s32 func_801E80E8(s32 thread, u8 *insn) {
    return 1;
}

/* Opcode 39. */
s32 func_801E80F0(s32 thread, u8 *insn) {
    D_800CCD88 = 0;
    D_8006D940 = 0;
    func_80088490(0);
    func_800BAF48(0);
    D_800C3EAC->unk2EB = 1;
    D_800CCE42 |= 0x80;
    func_800883AC(0);
    D_800D32A1 = 2;
    D_800C3EA4->unk853D = 2;
    D_800C3EB8 = 1;
    return 1;
}

/* Opcode 3a: start animation b on actor a. */
s32 func_801E818C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    func_801E9430(func_801E5A98((u8)D_800D3278->operands[0]), D_800D3278->operands[1]);
    return 5;
}

/* Opcode 3b. */
s32 func_801E81EC(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E950C(func_801E5A98((u8)D_800D3278->operands[0]));
    return 3;
}

/* Opcode 3c. */
s32 func_801E823C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E9550(func_801E5A98((u8)D_800D3278->operands[0]));
    return 3;
}

/* Opcode 3d. */
s32 func_801E828C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E958C(func_801E5A98((u8)D_800D3278->operands[0]));
    return 3;
}

/* Opcode 3e: start an actor action with three arguments and wait for it. */
s32 func_801E82DC(s32 thread, u8 *insn) {
    s32 slot;
    s32 length = 0;

    func_801E57F8(insn, 4, 0, 1);
    slot = func_801E5A98((u8)D_800D3278->operands[0]);
    if (D_801E9C20[slot] == 0) {
        D_800D3278->actionRunning[slot] = 1;
        D_801E9C20[slot] = 1;
        func_801E95E4(slot, D_800D3278->operands[1], D_800D3278->operands[2], D_800D3278->operands[3]);
    } else if (D_800D3278->actionRunning[slot] == 0) {
        D_801E9C20[slot] = 0;
        length = 9;
    }
    return length;
}

/* Opcode 3f: start the other actor action and wait for it. */
s32 func_801E83C0(s32 thread, u8 *insn) {
    s32 slot;
    s32 length = 0;

    func_801E57F8(insn, 4, 0, 1);
    slot = func_801E5A98((u8)D_800D3278->operands[0]);
    if (D_801E9C20[slot] == 0) {
        D_800D3278->actionRunning[slot] = 1;
        D_801E9C20[slot] = 1;
        func_801E9694(slot, D_800D3278->operands[1], D_800D3278->operands[2], D_800D3278->operands[3]);
    } else if (D_800D3278->actionRunning[slot] == 0) {
        D_801E9C20[slot] = 0;
        length = 9;
    }
    return length;
}

#ifdef NON_MATCHING
/* Opcode 45: actor a attacks actor b (animation c, value d) and waits.
 * (The original masks the actor ids separately at each use.) */
s32 func_801E84A4(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 attacker;
    u8 target;

    func_801E57F8(insn, 4, 0, 1);
    attacker = func_801E5A98((u8)D_800D3278->operands[0]);
    target = func_801E5A98((u8)D_800D3278->operands[1]);
    D_800C3EAC->unk2DA = 0;
    func_80085388();
    D_800C4000[target] = D_800D3278->operands[3];
    if (D_801E9C20[attacker] == 0) {
        D_800D3278->actionRunning[attacker] = 1;
        D_801E9C20[attacker] = 1;
        func_801E9894(attacker, target);
        while (func_800286CC() != 0) {
            func_800716D8();
        }
        func_801E9430(attacker, D_800D3278->operands[2]);
    } else if (D_800D3278->actionRunning[attacker] == 0) {
        D_801E9C20[attacker] = 0;
        length = 9;
    }
    return length;
}
#else
INCLUDE_ASM(".local/decomp/ovl3087/asm/nonmatchings/ovl3087", func_801E84A4);
#endif

/* Opcode 46. */
s32 func_801E8600(s32 thread, u8 *insn) {
    u8 actor;
    u8 target;

    D_800C3EAC->unk2DA = 0;
    func_80085388();
    func_801E57F8(insn, 3, 0, 1);
    actor = func_801E5A98((u8)D_800D3278->operands[0]);
    target = func_801E5A98((u8)D_800D3278->operands[1]);
    func_801E9700(actor, D_800D3278->operands[2]);
    func_801E9760(actor, target);
    return 7;
}

/* Opcode 47. */
s32 func_801E86AC(s32 thread, u8 *insn) {
    func_800B8D7C();
    return 1;
}

/* Opcode 42. */
s32 func_801E86D0(s32 thread, u8 *insn) {
    func_8007FF14(0);
    return 1;
}

/* Opcode 43. */
s32 func_801E86F4(s32 thread, u8 *insn) {
    func_800800E8(0);
    return 1;
}

/* Opcode 44: clear byte 0x35 of the eleven battle objects. */
s32 func_801E8718(s32 thread, u8 *insn) {
    s32 i;

    for (i = 0; i < 11; i++) {
        if (D_800D3368[i] != NULL) {
            D_800D3368[i]->unk35 = 0;
        }
    }
    return 1;
}

/* Opcode 48. */
s32 func_801E8750(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    func_800B838C(D_800D3278->operands[0], D_800D3278->operands[1]);
    return 5;
}

/* Run one frame of the script: each thread in run order executes up to
 * four instructions (fewer when one yields or ends); opcode 22 counts down
 * frames in which the whole pass repeats. */
void func_801E879C(void) {
    s32 length;
    u8 steps;
    u32 i;
    u8 thread;
    u8 again;
    u16 pc;

    func_800BFBA0();
    func_800BCD98(0);
    again = 1;
    if (D_800D3278->halted == 0) {
        do {
            for (i = 0; i < D_800D39D0->threadCount; i++) {
                thread = D_800D3278->order[i];
                steps = 4;
                func_800716D8();
                do {
                    pc = func_801E5768(&D_800D3278->threads[thread]);
                    switch (D_800D3278->code[pc]) {
                    case 0x00:
                        length = func_801E5C1C(thread);
                        steps = 1;
                        break;
                    case 0x01:
                        length = func_801E5CE4(thread, D_800D3278->code + pc);
                        break;
                    case 0x02:
                        length = func_801E5D24(thread, D_800D3278->code + pc);
                        break;
                    case 0x03:
                        length = func_801E5DCC(thread, D_800D3278->code + pc);
                        break;
                    case 0x04:
                        length = func_801E5EF8(thread, D_800D3278->code + pc);
                        break;
                    case 0x05:
                        length = func_801E5F8C(thread, D_800D3278->code + pc);
                        break;
                    case 0x06:
                        length = func_801E6084(thread, D_800D3278->code + pc);
                        break;
                    case 0x07:
                        length = func_801E60E8(thread, D_800D3278->code + pc);
                        break;
                    case 0x08:
                        length = func_801E6118(thread, D_800D3278->code + pc);
                        break;
                    case 0x09:
                        length = func_801E6144(thread, D_800D3278->code + pc);
                        break;
                    case 0x0A:
                        length = func_801E61B4(thread, D_800D3278->code + pc);
                        break;
                    case 0x0B:
                        length = func_801E6224(thread, D_800D3278->code + pc);
                        break;
                    case 0x0C:
                        length = func_801E6294(thread, D_800D3278->code + pc);
                        break;
                    case 0x0D:
                        length = func_801E6304(thread, D_800D3278->code + pc);
                        break;
                    case 0x0E:
                        length = func_801E633C(thread, D_800D3278->code + pc);
                        break;
                    case 0x0F:
                        length = func_801E6374(thread, D_800D3278->code + pc);
                        break;
                    case 0x10:
                        length = func_801E63E4(thread, D_800D3278->code + pc);
                        break;
                    case 0x11:
                        length = func_801E6454(thread, D_800D3278->code + pc);
                        break;
                    case 0x12:
                        length = func_801E64C4(thread, D_800D3278->code + pc);
                        break;
                    case 0x13:
                        length = func_801E6534(thread, D_800D3278->code + pc);
                        break;
                    case 0x14:
                        length = func_801E65A4(thread, D_800D3278->code + pc);
                        break;
                    case 0x15:
                        length = func_801E65FC(thread, D_800D3278->code + pc);
                        break;
                    case 0x16:
                        length = func_801E6660(thread, D_800D3278->code + pc);
                        break;
                    case 0x17:
                        length = func_801E66D8(thread, D_800D3278->code + pc);
                        break;
                    case 0x18:
                        length = func_801E71D4(thread, D_800D3278->code + pc);
                        break;
                    case 0x19:
                        length = func_801E7230(thread, D_800D3278->code + pc);
                        break;
                    case 0x1A:
                        length = func_801E7278(thread, D_800D3278->code + pc);
                        break;
                    case 0x1B:
                        length = func_801E7314(thread, D_800D3278->code + pc);
                        break;
                    case 0x1C:
                        length = func_801E7358(thread, D_800D3278->code + pc);
                        break;
                    case 0x1D:
                        length = func_801E736C(thread, D_800D3278->code + pc);
                        break;
                    case 0x1E:
                        length = func_801E7380(thread, D_800D3278->code + pc);
                        break;
                    case 0x1F:
                        length = func_801E73D4(thread, D_800D3278->code + pc);
                        break;
                    case 0x20:
                        length = func_801E746C(thread, D_800D3278->code + pc);
                        break;
                    case 0x21:
                        length = func_801E748C(thread, D_800D3278->code + pc);
                        break;
                    case 0x22:
                        length = func_801E74A0(thread, D_800D3278->code + pc);
                        break;
                    case 0x23:
                        length = func_801E74E0(thread, D_800D3278->code + pc);
                        break;
                    case 0x24:
                        length = func_801E7700(thread, D_800D3278->code + pc);
                        break;
                    case 0x25:
                        length = func_801E775C(thread, D_800D3278->code + pc);
                        break;
                    case 0x26:
                        length = func_801E7770(thread, D_800D3278->code + pc);
                        break;
                    case 0x27:
                        length = func_801E77E4(thread, D_800D3278->code + pc);
                        break;
                    case 0x28:
                        length = func_801E786C(thread, D_800D3278->code + pc);
                        break;
                    case 0x29:
                        length = func_801E78A8(thread, D_800D3278->code + pc);
                        break;
                    case 0x2A:
                        length = func_801E79E0(thread, D_800D3278->code + pc);
                        break;
                    case 0x2B:
                        length = func_801E7B58(thread, D_800D3278->code + pc);
                        break;
                    case 0x2C:
                        length = func_801E7C0C(thread, D_800D3278->code + pc);
                        break;
                    case 0x2D:
                        length = func_801E7E14(thread, D_800D3278->code + pc);
                        break;
                    case 0x2E:
                        length = func_801E7E5C(thread, D_800D3278->code + pc);
                        break;
                    case 0x2F:
                        length = func_801E7EA4(thread, D_800D3278->code + pc);
                        break;
                    case 0x30:
                        length = func_801E7F08(thread, D_800D3278->code + pc);
                        break;
                    case 0x31:
                        length = func_801E7F70(thread, D_800D3278->code + pc);
                        break;
                    case 0x32:
                        length = func_801E8074(thread, D_800D3278->code + pc);
                        break;
                    case 0x33:
                        length = func_801E807C(thread, D_800D3278->code + pc);
                        break;
                    case 0x34:
                        length = func_801E80E8(thread, D_800D3278->code + pc);
                        break;
                    case 0x35:
                        length = func_801E7914(thread, D_800D3278->code + pc);
                        break;
                    case 0x36:
                        length = func_801E7B08(thread, D_800D3278->code + pc);
                        break;
                    case 0x37:
                        length = func_801E74B8(thread, D_800D3278->code + pc);
                        break;
                    case 0x38:
                        length = func_801E75F0(thread, D_800D3278->code + pc);
                        break;
                    case 0x39:
                        length = func_801E80F0(thread, D_800D3278->code + pc);
                        break;
                    case 0x3A:
                        length = func_801E818C(thread, D_800D3278->code + pc);
                        break;
                    case 0x3B:
                        length = func_801E81EC(thread, D_800D3278->code + pc);
                        break;
                    case 0x3C:
                        length = func_801E823C(thread, D_800D3278->code + pc);
                        break;
                    case 0x3D:
                        length = func_801E828C(thread, D_800D3278->code + pc);
                        break;
                    case 0x3E:
                        length = func_801E82DC(thread, D_800D3278->code + pc);
                        break;
                    case 0x3F:
                        length = func_801E83C0(thread, D_800D3278->code + pc);
                        break;
                    case 0x40:
                        length = func_801E7B2C(thread, D_800D3278->code + pc);
                        break;
                    case 0x41:
                        length = func_801E7FF4(thread, D_800D3278->code + pc);
                        break;
                    case 0x42:
                        length = func_801E86D0(thread, D_800D3278->code + pc);
                        break;
                    case 0x43:
                        length = func_801E86F4(thread, D_800D3278->code + pc);
                        break;
                    case 0x44:
                        length = func_801E8718(thread, D_800D3278->code + pc);
                        break;
                    case 0x45:
                        length = func_801E84A4(thread, D_800D3278->code + pc);
                        break;
                    case 0x46:
                        length = func_801E8600(thread, D_800D3278->code + pc);
                        break;
                    case 0x47:
                        length = func_801E86AC(thread, D_800D3278->code + pc);
                        break;
                    case 0x48:
                        length = func_801E8750(thread, D_800D3278->code + pc);
                        break;
                    case 0x49:
                        length = func_801E7424(thread, D_800D3278->code + pc);
                        break;
                    case 0x4A:
                        length = func_801E7660(thread, D_800D3278->code + pc);
                        break;
                    case 0x4B:
                        length = func_801E7684(thread, D_800D3278->code + pc);
                        break;
                    }
                    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] =
                        length + D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level];
                } while (--steps != 0);
            }
            if (D_800D3278->unk801 != 0 && --D_800D3278->unk801 == 1) {
                again = 0;
            }
        } while (again);
    }
}

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
