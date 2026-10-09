/* ovl3087 (Disc 1 slot 3087 / Disc 2 slot 3082), loaded at 801e5000: the
 * battle event script interpreter. The battle overlay loads it (80070e2c,
 * file list entry 1 through 800295d8) only when the formation sets
 * 800c3d48 (formation flag 0x20), then calls 801e5160 once to load the
 * script files and set up the threads, 801e879c (through 80070eb0, at the
 * start and between turns) to run the script threads until opcode 22 hands
 * back, and 801e563c at the end to release everything. Opcode
 * handlers use the battle overlay's actor, camera and message services
 * (8007xxxx-800cxxxx) and resident file/heap/sound helpers.
 *
 * This unit holds the overlay's rodata (801e5000), the interpreter and its
 * opcode handlers (801e5160-801e93e8) and the overlay's data (801e9b5c); it
 * ends at 801e93e8, where the actor helpers' compiler takes over
 * (script_actor.c, ovl3087.mk). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "resident/window.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "ovl3087.h"

/* Portrait file per actor, normal then mirrored (file 0x46 + n). */
u8 D_801E9B5C[] = {
    0,  0,  6,  6,  17, 17, 19, 20, 21, 21, 23, 23, 24, 24, 28, 28,
    27, 27, 17, 17, 25, 25, 34, 34, 35, 35, 36, 36, 37, 37, 79, 79,
    82, 82, 83, 83, 26, 26, 52, 52, 81, 81, 77, 77, 78, 78, 33, 33,
    41, 41, 29, 29, 43, 43, 50, 51, 42, 42, 53, 53, 56, 56, 38, 38,
    1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  7,  7,  8,  8,  9,  9,
    10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 18, 18,
    30, 30, 31, 31, 32, 32, 39, 39, 40, 40, 44, 44, 45, 45, 46, 46,
    47, 47, 48, 48, 49, 49, 54, 54, 22, 22, 57, 57, 58, 58, 59, 59,
    60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 65, 65, 66, 66, 67, 67,
    68, 68, 69, 69, 70, 70, 71, 71, 72, 72, 73, 73, 74, 74, 75, 75,
    76, 76, 80, 80, 55, 55, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88,
    89, 89, 90, 90,
};
/* Default message window layout. */
u16 D_801E9C10[5] = {0x7FFF, 0x7FFF, 16, 8, 0x1F0};
s32 D_801E9C1C = 4; /* the cursor glyph's frame, cycled 4..0 */
u8 D_801E9C20[16] = {0};
s32 D_801E9C30 = 0;
s32 D_801E9C34 = 0;
ModelArchive *D_801E9C38 = NULL;

/* The actor and model helpers (script_actor.c) as this unit declares them:
 * its calls convert the actor, animation and target arguments differently
 * from the definitions (bytes and halfwords where those take words). */
void func_801E9958(s32 model, u16 animation);
s32 func_801E9978(void *file, s32 *info);
void func_801E9430(u8 actor, s16 animation);
void func_801E950C(u8 actor);
void func_801E9550(u8 actor);
void func_801E958C(u8 actor);
void func_801E95E4(u8 actor, s16 arg1, s16 arg2, s16 arg3);
void func_801E9694(u8 actor, s16 arg1, s16 arg2, s16 arg3);
void func_801E9700(u8 actor, u16 arg1);
void func_801E9760(u8 actor, u8 target);
void func_801E9894(u8 actor, u16 target);
void func_801E9AD4(s32 model);
void func_801E9B2C(void);

/* Load the script set of 8006f9df (script archive file 2) and the model
 * archive (file 3), set up the interpreter state and its threads, the
 * portrait quads and the script's sound bank (file 4). */
void func_801E5160(void) {
    FileRequest files[3];
    ScriptArchive *archive;
    EventScriptFile *script;
    s32 i;
    s32 level;

    func_800716D8();
    func_8008AB4C();
    archive = func_8008ABB8(func_800288EC(2), 1);
    files[0].file = 2;
    files[0].destination = archive;
    D_801E9C38 = func_8008ABB8(func_800288EC(3), 0);
    files[1].file = 3;
    files[1].destination = D_801E9C38;
    files[2].file = 0;
    files[2].destination = NULL;
    func_80029AFC(files, 0, 0x80);
    func_8008AC50();
    func_8003342C(archive);
    func_8003342C(D_801E9C38);
    script = func_80032E88(((ScriptSet *)((u8 *)archive + D_8006F9DC[3] * 8))->script, 0);
    D_800D3340 = func_80032E88(((ScriptSet *)((u8 *)archive + D_8006F9DC[3] * 8))->data, 0);
    func_800320E8(archive);
    D_800D3278 = func_8008ABB8(sizeof(ScriptState), 0);
    bzero((u8 *)D_800D3278, sizeof(ScriptState));
    D_800D2DAC = func_8008ABB8(0x98, 0);
    bzero((u8 *)D_800D2DAC, 0x78);
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
        SetPolyFT4(&D_800D3278->quads[i]);
        setRGB0(&D_800D3278->quads[i], 0x80, 0x80, 0x80);
        SetSemiTrans(&D_800D3278->quads[i], 0);
        SetShadeTex(&D_800D3278->quads[i], 1);
        D_800D3278->quads[i].clut = GetClut(0, 0x1D0);
        D_800D3278->quads[i].tpage = GetTPage(1, 0, 0x3C0, 0x100);
    }
    D_800D2D28->scriptLoaded = 1;
    D_800D2D28->unkCC[3] = 0;
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
        func_80039C4C((SoundSeq *)D_800C3E54);
        func_800716D8();
        func_800399D4((SoundSeq *)D_800C3E54);
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

/* Show the next frame of the five-frame cursor glyph (glyphs 0xe0-0xe4, the
 * battle graphics' cursor quads) at (x, y), mirrored horizontally by
 * swapping the current buffer's second and third vertices. */
void func_801E5B00(x, y)
s16 x;
s16 y;
{
    BattleGraphics *graphics;
    s32 x1;
    s32 y1;

    if (--D_801E9C1C < 0) {
        D_801E9C1C = 4;
    }
    D_800D2D28->cursorParts =
        func_80076A10(D_801E9C1C + 0xE0, D_800C3EA4->cursor, x, y);
    graphics = D_800C3EA4;
    x1 = graphics->cursor[D_800CCB04.buffer].x1;
    y1 = graphics->cursor[D_800CCB04.buffer].y1;
    graphics->cursor[D_800CCB04.buffer].x1 = graphics->cursor[D_800CCB04.buffer].x2;
    graphics->cursor[D_800CCB04.buffer].y1 = graphics->cursor[D_800CCB04.buffer].y2;
    graphics->cursor[D_800CCB04.buffer].x2 = x1;
    graphics->cursor[D_800CCB04.buffer].y2 = y1;
    D_800D2D28->cursorBuffer = D_800CCB04.buffer;
    D_800D2D28->cursorShown = 1;
}

/* Opcode 00 (end, 1 byte): drop the running level and restart the thread's
 * base level at its idle entry (1). Yields. */
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

/* Opcode 01 (jump, 3 bytes): continue the running level at the bytecode
 * offset (u16 at byte 1). */
s32 func_801E5CE4(s32 thread, u8 *insn) {
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = insn[1] + (insn[2] << 8);
    return 0;
}

/* Opcode 02 (branch unless, 8 bytes): compare a (byte 1) and b (byte 3),
 * immediates by bits 0x80 and 0x40 of byte 5, by its low four bits
 * (801e58ec); when the comparison fails jump to the offset at byte 6. */
s32 func_801E5D24(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    if (func_801E58EC(D_800D3278->operands[0], D_800D3278->operands[1], insn[5])) {
        return 8;
    }
    D_800D3278->threads[thread].pc[D_800D3278->threads[thread].level] = insn[6] + (insn[7] << 8);
    return 0;
}

/* Opcode 03 (request, 3 bytes): start entry (byte 2, low five bits) of
 * thread (byte 1) on a free level with the priority in byte 2's top three
 * bits. Retries (length 0) while that thread has no free level. */
s32 func_801E5DCC(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 level = func_801E57C4(&D_800D3278->threads[insn[1]]);

    if (level != 8) {
        D_800D3278->threads[thread].request = insn[2] & 0x1F;
        D_800D3278->threads[insn[1]].priority[level] = insn[2] >> 5;
        D_800D3278->threads[insn[1]].pc[level] =
            (D_800D39D0->entries + insn[1])->entry[D_800D3278->threads[thread].request];
        length = 3;
        D_800D3278->threads[insn[1]].entry[level] = D_800D3278->threads[thread].request;
    }
    return length;
}

/* Opcode 04 (request and wait for start, 3 bytes as 03): issue the request,
 * then wait until the other thread is running the requested entry. */
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

/* Opcode 05 (request and wait for end, 3 bytes as 03): issue the request,
 * then wait until the requested entry is neither queued nor running on the
 * other thread. */
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

/* Opcode 06 (6 bytes): var (byte 1) = value (byte 3, an immediate when byte
 * 5 has bit 0x40). */
s32 func_801E6084(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) = D_800D3278->operands[1];
    return 6;
}

/* Opcode 07 (3 bytes): var (byte 1) = 1. */
s32 func_801E60E8(s32 thread, u8 *insn) {
    INSN_VAR(insn) = 1;
    return 3;
}

/* Opcode 08 (3 bytes): var (byte 1) = 0. */
s32 func_801E6118(s32 thread, u8 *insn) {
    INSN_VAR(insn) = 0;
    return 3;
}

/* Opcode 09 (6 bytes): var (byte 1) += value (byte 3, immediate by bit 0x40
 * of byte 5). */
s32 func_801E6144(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) += D_800D3278->operands[1];
    return 6;
}

/* Opcode 0a (6 bytes): var (byte 1) -= value (as 09). */
s32 func_801E61B4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) -= D_800D3278->operands[1];
    return 6;
}

/* Opcode 0b (6 bytes): var (byte 1) |= value (as 09). */
s32 func_801E6224(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) |= D_800D3278->operands[1];
    return 6;
}

/* Opcode 0c (6 bytes): var (byte 1) &= ~value (as 09). */
s32 func_801E6294(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) &= ~D_800D3278->operands[1];
    return 6;
}

/* Opcode 0d (3 bytes): var (byte 1)++. */
s32 func_801E6304(s32 thread, u8 *insn) {
    INSN_VAR(insn)++;
    return 3;
}

/* Opcode 0e (3 bytes): var (byte 1)--. */
s32 func_801E633C(s32 thread, u8 *insn) {
    INSN_VAR(insn)--;
    return 3;
}

/* Opcode 0f (6 bytes): var (byte 1) &= value (as 09). */
s32 func_801E6374(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) &= D_800D3278->operands[1];
    return 6;
}

/* Opcode 10 (6 bytes): var (byte 1) |= value (as 09; the same as 0b). */
s32 func_801E63E4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) |= D_800D3278->operands[1];
    return 6;
}

/* Opcode 11 (6 bytes): var (byte 1) ^= value (as 09). */
s32 func_801E6454(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) ^= D_800D3278->operands[1];
    return 6;
}

/* Opcode 12 (5 bytes): var (byte 1) <<= the variable at byte 3. */
s32 func_801E64C4(s32 thread, u8 *insn) {
    s32 index = ((insn[2] << 8) | insn[1]) >> 1;

    func_801E57F8(insn, 2, 0, 0);
    D_800D3278->vars[index] <<= D_800D3278->operands[1];
    return 5;
}

/* Opcode 13 (5 bytes): var (byte 1) >>= the variable at byte 3. */
s32 func_801E6534(s32 thread, u8 *insn) {
    s32 index = ((insn[2] << 8) | insn[1]) >> 1;

    func_801E57F8(insn, 2, 0, 0);
    D_800D3278->vars[index] >>= D_800D3278->operands[1];
    return 5;
}

/* Opcode 14 (3 bytes): var (byte 1) = random 0..7fff. */
s32 func_801E65A4(s32 thread, u8 *insn) {
    INSN_VAR(insn) = func_80089B50(0, 0x7FFF);
    return 3;
}

/* Opcode 15 (5 bytes): var (byte 3) = random 0..limit (u16 at byte 1). */
s32 func_801E65FC(s32 thread, u8 *insn) {
    SCRIPT_VAR(D_800D3278, (insn[4] << 8) | insn[3]) = func_80089B50(0, insn[1] | (insn[2] << 8));
    return 5;
}

/* Opcode 16 (6 bytes): var (byte 1) = a * b, a the var operand itself (an
 * immediate by bit 0x80 of byte 5), b at byte 3 (bit 0x40). */
s32 func_801E6660(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) = D_800D3278->operands[0] * D_800D3278->operands[1];
    return 6;
}

/* Opcode 17 (6 bytes): var (byte 1) = a / b (signed; operands as 16). */
s32 func_801E66D8(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, insn[5], 0);
    INSN_VAR(insn) = (s32)D_800D3278->operands[0] / (s32)D_800D3278->operands[1];
    return 6;
}

/* Load the portrait TIM of an actor (mirrored or not) into VRAM (clut
 * 0,1d0; pixels 3c0,100) and lay the current buffer's portrait quad out
 * in a 64x64 box inside the window at (x, y). */
void func_801E6750(u8 actor, s32 flags, s32 x, s32 y, s32 width) {
    TIM_IMAGE tim;
    s32 mirrored = flags & 1;
    s32 file = D_801E9B5C[actor * 2 + mirrored] + 0x46;
    void *data;

    func_80028470(4, 0);
    data = func_8008ABB8(func_800288EC(file), 1);
    func_800295D8(file, data, 0, 0x80);
    func_8008AC50();
    OpenTIM(data);
    ReadTIM(&tim);
    tim.crect->x = 0;
    tim.crect->y = 0x1D0;
    tim.prect->x = 0x3C0;
    tim.prect->y = 0x100;
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    DrawSync(0);
    func_800320E8(data);
    if (mirrored) {
        setXY4(&D_800D3278->quads[D_800CCB04.buffer], x + width - 4, y + 4, x + width - 0x44, y + 4,
               x + width - 4, y + 0x44, x + width - 0x44, y + 0x44);
        setUV4(&D_800D3278->quads[D_800CCB04.buffer], 0, 0, 0x3F, 0, 0, 0x40, 0x3F, 0x40);
    } else {
        setXY4(&D_800D3278->quads[D_800CCB04.buffer], x + 4, y + 4, x + 0x44, y + 4, x + 4, y + 0x44,
               x + 0x44, y + 0x44);
        setUV4(&D_800D3278->quads[D_800CCB04.buffer], 0, 0, 0x40, 0, 0, 0x40, 0x40, 0x40);
    }
    D_800D3278->portraitBuffer = D_800CCB04.buffer;
}

/* Show message of the script's message file in the layout of opcode 1a,
 * with the speaker's portrait unless flag 2 is set; returns 1 once the
 * message has been dismissed. Flags: 1 portrait left, 2 no portrait,
 * 4 lower window, 8 no window, 0x10 window style. */
u8 func_801E6CE8(u16 message, u8 actor, u16 flags) {
    u16 x;
    u16 y;
    u16 width;
    u16 height;
    u8 portrait;
    s32 i;
    s32 done;

    portrait = 0;
    done = 0;
    x = D_800D3278->window[0];
    y = D_800D3278->window[1];
    width = D_800D3278->window[2] * 12 + 0x18;
    if (flags == 0) {
        flags = D_800D3278->window[4];
    }
    if (D_800D3278->windowOpen == 0) {
        D_801E9C1C = 4;
        if (D_800D3278->window[1] == 0x7FFF) {
            y = 0x10;
            if (flags & 4) {
                y = 0x8C;
            }
        }
        if (D_800D3278->window[3] >= 5) {
            height = 4;
        } else {
            height = D_800D3278->window[3];
        }
        height = height * 13 + 0x14;
        if (D_800D3278->window[0] == 0x7FFF) {
            x = 0xA0 - (width >> 1);
        }
        if (!(flags & 8)) {
            if (!(flags & 2) && actor != 0xFF) {
                width += 0x40;
                portrait = 1;
            }
            if (D_800D3278->window[0] == 0x7FFF) {
                x = 0xA0 - (width >> 1);
            }
            if (!portrait) {
                func_8008F8F4(0, x, y, width, height, ((flags >> 4) ^ 1) & 1, 1);
                while (D_800D2D28->windowOpen[0] == 0) {
                    func_800716D8();
                }
            } else {
                func_801E6750(actor, flags, x, y, width);
                func_8008F8F4(0, x, y, width, height, ((flags >> 4) ^ 1) & 1, 1);
                while (D_800D2D28->windowOpen[0] == 0) {
                    func_800716D8();
                }
                D_800D2D28->scriptPortraitShown = 1;
                if (!(flags & 1)) {
                    x += 0x40;
                }
            }
        }
        D_801E9C34 = y + 8;
        D_801E9C30 = x + 12;
        func_80032F54(D_800D2DAC, 0x380, 0x100, D_801E9C30, D_801E9C34, D_800D3278->window[2] * 3,
                      D_800D3278->window[3]);
        *(u8 *)&D_800D2DAC->tile[1] = 4;
        D_800D2DAC->flags |= 2;
        func_80034614(D_800D2DAC);
        func_80034714(D_800D2DAC, (s32)func_80033728(D_800D3340, message));
        D_800D2D28->messageShown = 1;
        D_800D3278->windowOpen = 1;
        func_800716D8();
    }
    if (D_800D2DAC->flags & 8) {
        if (!(flags & 8)) {
            func_801E5B00(D_800D2DAC->x * 4 + D_801E9C30 + 2, D_800D2DAC->y * 14 + D_801E9C34 + 5);
        }
        D_800D2D28->unkCC[3] = 1;
        if (D_800D3014 == 4) {
            func_800345E0(D_800D2DAC);
            D_800D2D28->unkCC[3] = 0;
            D_800D2D28->cursorShown = 0;
        }
    }
    if (!(D_800D2DAC->flags & 4)) {
        D_800D2D28->messageShown = 0;
        func_800346D4(D_800D2DAC);
        func_800716D8();
        D_800D2D28->scriptPortraitShown = 0;
        if (!(flags & 8)) {
            func_8008FA60(0);
        }
        done = 1;
        D_800D3278->windowOpen = 0;
        for (i = 0; i < 5; i++) {
            D_800D3278->window[i] = D_801E9C10[i];
        }
    }
    return done;
}

/* Opcode 18 (4 bytes): show message (u16 at byte 1) from the thread's
 * speaker with flags (byte 3; 0 takes the layout's); repeats until the
 * message is done. */
s32 func_801E71D4(s32 thread, u8 *insn) {
    return (func_801E6CE8(insn[1] | (insn[2] << 8), D_800D3278->threads[thread].speaker, insn[3]) != 0) * 4;
}

/* Opcode 19 (5 bytes): show message (u16 at byte 2) from actor (byte 1) with
 * flags (byte 4); repeats until done. */
s32 func_801E7230(s32 thread, u8 *insn) {
    return func_801E6CE8(insn[2] | (insn[3] << 8), insn[1], insn[4]) ? 5 : 0;
}

/* Opcode 1a (11 bytes): set the message window layout from five signed-form
 * operands (x, y, width, height, flags); zero x..height take the defaults. */
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

/* Opcode 1b (2 bytes): set the thread's speaker to actor (byte 1; f3-f5 name
 * the party members). */
s32 func_801E7314(s32 thread, u8 *insn) {
    u8 actor = insn[1];

    if (actor >= 0xF3) {
        actor = D_800D2D24[actor - 0xF3];
    }
    D_800D3278->threads[thread].speaker = actor;
    return 2;
}

/* Opcode 1c (1 byte): battle end state (800c3e4c) = 2. */
s32 func_801E7358(s32 thread, u8 *insn) {
    D_800C3E4C = 2;
    return 1;
}

/* Opcode 1d (1 byte): battle end state (800c3e4c) = 1. */
s32 func_801E736C(s32 thread, u8 *insn) {
    D_800C3E4C = 1;
    return 1;
}

/* Opcode 1e (3 bytes): fade the screen to white over 2 * a frames (signed
 * operand a; blend mode 2, 800b39c0). */
s32 func_801E7380(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_800B39C0(D_800D3278->operands[0], 2, 0xFF, 0xFF, 0xFF);
    return 3;
}

/* Opcode 1f (3 bytes): fade the screen to black over 2 * a frames (as 1e). */
s32 func_801E73D4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_800B39C0(D_800D3278->operands[0], 2, 0, 0, 0);
    return 3;
}

/* Opcode 49 (3 bytes): 8005942c = signed operand a. */
s32 func_801E7424(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    D_8005942C = D_800D3278->operands[0];
    return 3;
}

/* Opcode 20 (1 byte): end the battle (800c3d44) and halt the script
 * (801e879c checks it when next called). */
s32 func_801E746C(s32 thread, u8 *insn) {
    D_800C3D44 = 1;
    D_800D3278->halted = 1;
    return 1;
}

/* Opcode 21 (1 byte): 800d2d50 = 1. */
s32 func_801E748C(s32 thread, u8 *insn) {
    D_800D2D50 = 1;
    return 1;
}

/* Opcode 22 (1 byte): make this pass of 801e879c its last, returning to the
 * battle (it repeats its passes until then). */
s32 func_801E74A0(s32 thread, u8 *insn) {
    D_800D3278->unk801 = 2;
    return 1;
}

/* Opcode 37 (1 byte): request the battle exit (800d2fc4), set the outcome
 * (800c48ea) to 1 and halt the script. */
s32 func_801E74B8(s32 thread, u8 *insn) {
    D_800D2FC4 = 1;
    D_800D3278->halted = 1;
    D_800C48EA = 1;
    return 1;
}

/* Opcode 23 (7 bytes; signed operands a, b, c): object a - f3 acts on slot b
 * + 13 with its effect script c (800aa384), then waits until that effect
 * reports it is done (the effect VM's 02/03 set this thread's 0x34 through
 * 80080c6c) and finishes the battle's loads (800b8d04). */
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

/* Opcode 38 (7 bytes; signed operands a, b, c): object a - f3 starts its
 * effect script c on slot b + 13 (800aa320) without waiting. */
s32 func_801E75F0(s32 thread, u8 *insn) {
    func_801E57F8(insn, 3, 0, 1);
    func_800AA320(D_800D3278->operands[0] - 0xF3, func_80089C08(D_800D3278->operands[1] + 0xD),
                  D_800D3278->operands[2]);
    return 7;
}

/* Opcode 4a (1 byte): put slot 0 into state 4 with timer 6 (8009c0e0(0)). */
s32 func_801E7660(s32 thread, u8 *insn) {
    func_8009C0E0(0);
    return 1;
}

/* Opcode 4b (3 bytes): set bit 0 of actor a's battle record flags (0x36;
 * signed operand a). */
s32 func_801E7684(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    D_800CCCE8.records[func_801E5A98((u8)D_800D3278->operands[0])].pilot.flags36 |= 1;
    return 3;
}

/* Opcode 24 (5 bytes; signed operands a, b): the pending scene (8005947c) =
 * a + 1 and the battle kind (8005954c) = b, for the next battle start. */
s32 func_801E7700(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    D_8005947C = D_800D3278->operands[0] + 1;
    D_8005954C = D_800D3278->operands[1];
    return 5;
}

/* Opcode 25 (1 byte): 800c3d5c = 1. */
s32 func_801E775C(s32 thread, u8 *insn) {
    D_800C3D5C = 1;
    return 1;
}

/* Opcode 26 (9 bytes): store four signed operands at 8006f94e and clear the
 * state word 8004f30c (8001ac94). */
s32 func_801E7770(s32 thread, u8 *insn) {
    func_801E57F8(insn, 4, 0, 1);
    D_8006D634.map = D_800D3278->operands[0];
    D_8006D634.entry[0] = D_800D3278->operands[1];
    D_8006D634.entry[1] = D_800D3278->operands[2];
    D_8006D634.entry[2] = D_800D3278->operands[3];
    func_8001AC94();
    return 9;
}

/* Opcode 27 (9 bytes; signed operands a-d): 8004fe44 = a | 0x80, b, 1, c;
 * 800d3338 = 1; 80062514 = d. */
s32 func_801E77E4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 4, 0, 1);
    (&D_8004FE44)[0] = D_800D3278->operands[0] | 0x80;
    (&D_8004FE44)[1] = D_800D3278->operands[1];
    (&D_8004FE44)[2] = 1;
    (&D_8004FE44)[3] = D_800D3278->operands[2];
    D_800D3338 = 1;
    D_80062514 = D_800D3278->operands[3];
    return 9;
}

/* Opcode 28 (6 bytes): fade the screen to colour (bytes 2-4) in blend mode
 * (byte 1) over 2 * (byte 5) frames (800b39c0). */
s32 func_801E786C(s32 thread, u8 *insn) {
    func_800B39C0(insn[5], insn[1], insn[2], insn[3], insn[4]);
    return 6;
}

/* Opcode 29 (9 bytes; signed operands x, y, z, n): quake the view towards
 * amplitude (x, y, z) over 2 * n frames (800b3658). */
s32 func_801E78A8(s32 thread, u8 *insn) {
    u16 position[3];

    func_801E57F8(insn, 4, 0, 1);
    position[0] = D_800D3278->operands[0];
    position[1] = D_800D3278->operands[1];
    position[2] = D_800D3278->operands[2];
    func_800B3658(position, D_800D3278->operands[3]);
    return 9;
}

/* Opcode 35 (5 bytes; signed operands a, n): load model n of the model
 * archive into actor slot a (thread a + 13) unless one is loaded. */
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

/* Opcode 2a (5 bytes; signed operands a, n): play animation n on the loaded
 * model of slot a. */
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

/* Opcode 36 (3 bytes): release slot a's model (signed operand a). */
s32 func_801E7B08(s32 thread, u8 *insn) {
    func_801E7A5C(thread, insn);
    return 3;
}

/* Opcode 40 (3 bytes): release slot a's model and reset the script camera
 * (801e9b2c: 800c367c = 0, camera mode 0, effects disabled). */
s32 func_801E7B2C(s32 thread, u8 *insn) {
    func_801E7A5C(thread, insn);
    func_801E9B2C();
    return 3;
}

/* Opcode 2b (3 bytes): wait until the thread's timer, set to 2 * n (signed
 * operand n), runs out. */
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

/* Opcode 2c (3 bytes): set the thread's run-order request (byte 1; byte 2
 * unused); fe moves it to the front of the run order at once. */
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
        func_800399D4((SoundSeq *)D_800C3E54);
        func_800716D8();
    }
    func_8008AB70();
    size = func_800288EC(music + 4);
    D_800D3278->music = D_80062648;
    func_800295D8(music + 4, D_80062648, 0, 0x80);
    func_8008AC50();
    memmove(D_80062648, D_800D3278->music, size);
    D_800D3278->musicPlaying = 1;
    D_800D3278->musicId = music;
    D_800D3278->musicVolume = volume;
    D_800C3E54 = func_800397FC(D_80062648, volume, 0);
}

/* Fade the music to a volume. */
void func_801E7DE4(s32 volume, s32 time) {
    func_8003A89C((SoundSeq *)D_800C3E54, volume, time);
}

/* Opcode 2d (3 bytes): start music a (file a + 4; signed operand) at full
 * volume. */
s32 func_801E7E14(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E7CD0(D_800D3278->operands[0], 0x7F);
    return 3;
}

/* Opcode 2e (3 bytes): start music a at volume 0. */
s32 func_801E7E5C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E7CD0(D_800D3278->operands[0], 0);
    return 3;
}

/* Opcode 2f (5 bytes): fade the music to volume a over time b and keep a as
 * its stored volume (signed operands). */
s32 func_801E7EA4(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    D_800D3278->musicVolume = D_800D3278->operands[0];
    func_801E7DE4(D_800D3278->operands[0], D_800D3278->operands[1]);
    return 5;
}

/* Opcode 30 (3 bytes): set the music to its stored volume when a is 0, else
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

/* Opcode 31 (9 bytes; signed operands a-d): play sound a of the script bank
 * (the resident bank when d is set) with volume b and pan c (80039f18). */
s32 func_801E7F70(s32 thread, u8 *insn) {
    SoundBank *bank;

    func_801E57F8(insn, 4, 0, 1);
    if (D_800D3278->operands[3] == 0) {
        bank = D_800D3278->soundBank;
    } else {
        bank = (SoundBank *)D_8005919C;
    }
    func_80039F18((bank->id << 16) | D_800D3278->operands[0], D_800D3278->operands[1],
                  D_800D3278->operands[2]);
    return 9;
}

/* Opcode 41 (7 bytes; signed operands a-c): set the volume of playing sound
 * a of the script bank (the resident one when c is set) to b (8003a2e4). */
s32 func_801E7FF4(s32 thread, u8 *insn) {
    SoundBank *bank;

    func_801E57F8(insn, 3, 0, 1);
    if (D_800D3278->operands[2] == 0) {
        bank = D_800D3278->soundBank;
    } else {
        bank = (SoundBank *)D_8005919C;
    }
    func_8003A2E4((bank->id << 16) | D_800D3278->operands[0], D_800D3278->operands[1]);
    return 7;
}

/* Opcode 32 (1 byte): no operation. */
s32 func_801E8074(s32 thread, u8 *insn) {
    return 1;
}

/* Opcode 33 (1 byte): stop the music. */
s32 func_801E807C(s32 thread, u8 *insn) {
    if (D_800D3278->musicPlaying != 0) {
        func_80039C4C((SoundSeq *)D_800C3E54);
        func_800716D8();
        func_800399D4((SoundSeq *)D_800C3E54);
        D_800D3278->musicPlaying = 0;
    }
    return 1;
}

/* Opcode 34 (1 byte): no operation. */
s32 func_801E80E8(s32 thread, u8 *insn) {
    return 1;
}

/* Opcode 39 (1 byte): party slot 0 changes to its gear: a formation group of
 * its own (80088490), its sprite sent off and the gear object loaded
 * (800baf48), then out of its group (800883ac); also sets 800ccd88/8006d940
 * = 0, 800cce42 bit 7, 800d32a1 = 2, 800c3eb8 = 1 and two battle state
 * bytes. */
s32 func_801E80F0(s32 thread, u8 *insn) {
    D_800CCCE8.records[0].pilot.gearId = 0;
    D_8006D634.characters[0].gearId = 0;
    func_80088490(0);
    func_800BAF48(0);
    D_800C3EAC->reaction[0] = 1;
    D_800CCCE8.records[0].flags15A |= 0x80;
    func_800883AC(0);
    D_800D32A0[0].unk1 = 2;
    D_800C3EA4->panels[0].state = 2;
    D_800C3EB0.slots[0].gear = 1;
    return 1;
}

/* Opcode 3a (5 bytes; signed operands a, b): start animation b on actor a
 * (801e9430). */
s32 func_801E818C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    func_801E9430(func_801E5A98((u8)D_800D3278->operands[0]), D_800D3278->operands[1]);
    return 5;
}

/* Opcode 3b (3 bytes): return actor a to its idle animation (801e950c). */
s32 func_801E81EC(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E950C(func_801E5A98((u8)D_800D3278->operands[0]));
    return 3;
}

/* Opcode 3c (3 bytes): clear actor a's bytes 0x9e and 0x34 and bits 2-7 of
 * its flags at 0x40 (801e9550). */
s32 func_801E823C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E9550(func_801E5A98((u8)D_800D3278->operands[0]));
    return 3;
}

/* Opcode 3d (3 bytes): clear actor a's byte 0x9e (801e958c). */
s32 func_801E828C(s32 thread, u8 *insn) {
    func_801E57F8(insn, 1, 0, 1);
    func_801E958C(func_801E5A98((u8)D_800D3278->operands[0]));
    return 3;
}

/* Opcode 3e (9 bytes; signed operands a, x, y, z): move actor a to (x, y, z)
 * (801e95e4) and wait for it. */
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

/* Opcode 3f (9 bytes; signed operands a, x, y, z): run actor a's action 3
 * with (x, y, z) (801e9694) and wait for it. */
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

/* Opcode 45 (9 bytes; signed operands a-d): actor a attacks actor b
 * (animation c, value d) and waits; value d becomes b's code in the first
 * presentation event. */
s32 func_801E84A4(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 attacker;
    u8 target;

    func_801E57F8(insn, 4, 0, 1);
    attacker = func_801E5A98((u8)D_800D3278->operands[0]);
    target = func_801E5A98((u8)D_800D3278->operands[1]);
    D_800C3EAC->eventCount = 0;
    func_80085388();
    D_800C3EB0.events[0].codes[target] = D_800D3278->operands[3];
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

/* Opcode 46 (7 bytes; signed operands a, b, c): clear the event results,
 * give actor a its idle animation with command c (801e9700) and run its
 * attack on actor b (801e9760). */
s32 func_801E8600(s32 thread, u8 *insn) {
    u8 actor;
    u8 target;

    D_800C3EAC->eventCount = 0;
    func_80085388();
    func_801E57F8(insn, 3, 0, 1);
    actor = func_801E5A98((u8)D_800D3278->operands[0]);
    target = func_801E5A98((u8)D_800D3278->operands[1]);
    func_801E9700(actor, D_800D3278->operands[2]);
    func_801E9760(actor, target);
    return 7;
}

/* Opcode 47 (1 byte): stop the disc read and finish the loads (800b8d7c). */
s32 func_801E86AC(s32 thread, u8 *insn) {
    func_800B8D7C();
    return 1;
}

/* Opcode 42 (1 byte): show member 0's number lists (8007ff14(0)). */
s32 func_801E86D0(s32 thread, u8 *insn) {
    func_8007FF14(0);
    return 1;
}

/* Opcode 43 (1 byte): leave member 0's menu (800800e8(0)). */
s32 func_801E86F4(s32 thread, u8 *insn) {
    func_800800E8(0);
    return 1;
}

/* Opcode 44 (1 byte): clear byte 0x35 of the eleven battle objects. */
s32 func_801E8718(s32 thread, u8 *insn) {
    s32 i;

    for (i = 0; i < 11; i++) {
        if (D_800D3368[i] != NULL) {
            D_800D3368[i]->field35 = 0;
        }
    }
    return 1;
}

/* Opcode 48 (5 bytes; signed operands a, b): play battle sound a (variant b)
 * to its end (800b838c). */
s32 func_801E8750(s32 thread, u8 *insn) {
    func_801E57F8(insn, 2, 0, 1);
    func_800B838C(D_800D3278->operands[0], D_800D3278->operands[1]);
    return 5;
}

/* Run the script: each pass gives every thread in run order a battle frame
 * (800716d8) and then up to four instructions (fewer when one ends it).
 * Passes repeat until opcode 22 makes one the last. Opcodes 4c-ff have no
 * case: the previous length is applied again. */
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
