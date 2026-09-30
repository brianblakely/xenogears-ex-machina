#ifndef OVL3087_OVL3087_H
#define OVL3087_OVL3087_H

#include "common.h"

/* Battle event script interpreter (loaded at 801e5000 by the battle
 * overlay when the formation sets 800c3d48). Scripts run as up to 16
 * threads; each thread has eight priority levels with their own program
 * counter. Opcode handlers take the thread index and the instruction bytes
 * and return the instruction length (0 when the thread yields). */

/* 0x38 bytes per script thread. Each level runs one of the thread's
 * script entries (the 16-byte entry table of the script file). */
typedef struct {
    u16 pc[8];      /* 0x00 program counter per level (0xffff free) */
    u8 priority[8]; /* 0x10 level priority (0xff free) */
    u8 entry[8];    /* 0x18 entry index running at each level (0xff none) */
    u8 level;       /* 0x20 running level */
    u8 runningEntry; /* 0x21 entry index of the running level */
    u8 unk22;
    u8 request; /* 0x23 entry this thread requested of another (0xff none) */
    u8 unk24;
    u8 pad25[0x38 - 0x25];
} ScriptThread;

/* libgpu textured flat quad (POLY_FT4). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} PolyFT4;

/* The interpreter state (0x828 bytes, pointer 800d3278). */
typedef struct {
    ScriptThread threads[16]; /* 0x000 */
    u16 operands[4];          /* 0x380 decoded operands */
    u8 pad388[8];
    u8 *code;       /* 0x390 script bytecode */
    u16 vars[0x200]; /* 0x394 script variables */
    u8 order[16];   /* 0x794 thread run order */
    PolyFT4 quads[2]; /* 0x7a4 */
    u8 unk7F4;
    u8 unk7F5;
    u16 unk7F6[5];
    u8 pad800[4];
    u8 unk804[16];
    u8 pad814[4];
    void *soundBank; /* 0x818 */
    u8 pad81C[3];
    u8 unk81F;
    u8 soundBankLoaded; /* 0x820 */
    u8 pad821[7];
} ScriptState;

/* The script file: thread count at 0x40, then 16 bytes per thread with its
 * level entry points, then the bytecode. */
typedef struct {
    u16 entry[8];
} ScriptEntry;

typedef struct {
    u8 pad0[0x40];
    u32 threadCount;
    ScriptEntry entries[1];
} ScriptFile;

/* A variable addressed by its byte offset in the variable area. */
#define SCRIPT_VAR(state, offset) \
    (*(u16 *)((u8 *)(state)->vars + ((offset) & 0xFFFE)))
/* The variable named by the instruction's first operand. */
#define INSN_VAR(insn) SCRIPT_VAR(D_800D3278, ((insn)[2] << 8) | (insn)[1])

extern ScriptState *D_800D3278;
extern ScriptFile *D_800D39D0;

/* Battle graphics state (pointer 800c3ea4): one portrait quad per draw
 * buffer at 0x27c8. */
typedef struct {
    u8 pad0[0x27C8];
    PolyFT4 portrait[2];
} BattleGraphics;

/* Battle UI state (pointer 800d2d28), only the fields this module uses. */
typedef struct {
    u8 pad0[0x9E];
    u8 portraitShown; /* 0x9e */
    u8 pad9F[8];
    u8 portraitBuffer; /* 0xa7 */
    u8 padA8[0x22];
    u8 unkCA;
    u8 padCB[4];
    u8 unkCF;
    u8 padD0[0x30];
    s32 portraitHandle; /* 0x100 */
} BattleUi;

extern BattleGraphics *D_800C3EA4;
extern BattleUi *D_800D2D28;
extern s32 D_800CCB34; /* current draw buffer */
extern u8 D_800D2D24[3]; /* battle party character ids (0xff none) */
extern s32 D_801E9C1C;

/* Resident / battle services. */
s32 func_80076A10(s32 id, PolyFT4 *quads, s16 x, s16 y);
u16 func_80089B50(u16 low, u16 high);
u16 func_80089C9C(u16 flag, u8 bit);

/* This module. */
u16 func_801E5768(ScriptThread *thread);
u16 func_801E57C4(ScriptThread *thread);
void func_801E57F8(u8 *insn, u8 count, u8 immediateMask, u8 signedForm);
u8 func_801E58EC(s16 a, s16 b, u8 op);
s32 func_801E5DCC(s32 thread, u8 *insn);

#endif
