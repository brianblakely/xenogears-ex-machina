#ifndef OVL3087_OVL3087_H
#define OVL3087_OVL3087_H

#include "common.h"
#include "resident/window.h"
#include "battle/event_script.h"

/* Battle event script interpreter (loaded at 801e5000 by the battle
 * overlay when the formation sets 800c3d48). Scripts run as up to 16
 * threads; each thread has eight priority levels with their own program
 * counter. Opcode handlers take the thread index and the instruction bytes
 * and return the instruction length (0 when the thread yields). The
 * interpreter's state and script file are battle/event_script.h's. */

/* A variable addressed by its byte offset in the variable area. */
#define SCRIPT_VAR(state, offset) \
    (*(u16 *)((u8 *)(state)->vars + ((offset) & 0xFFFE)))
/* The variable named by the instruction's first operand. */
#define INSN_VAR(insn) SCRIPT_VAR(D_800D3278, ((insn)[2] << 8) | (insn)[1])

/* Script file 2: per script set, the compressed script and its data. */
typedef struct {
    s32 count;
    struct {
        void *script;
        void *data;
    } sets[1];
} ScriptArchive;

/* Set n of the script archive as the original addresses it: from
 * archive + n * 8, past the count. */
typedef struct {
    s32 count;
    void *script;
    void *data;
} ScriptSet;

/* Script file 3: the model archive, entry offsets from +4. */
typedef struct {
    s32 count;
    void *entries[1];
} ModelArchive;

/* This overlay's data. */
extern u8 D_801E9B5C[];   /* portrait file per actor (normal, mirrored) */
extern u16 D_801E9C10[5]; /* default message window layout */
extern s32 D_801E9C1C;    /* the cursor glyph's frame, cycled 4..0 */
extern u8 D_801E9C20[16]; /* actor action started by the script */
extern s32 D_801E9C30;    /* text origin */
extern s32 D_801E9C34;
extern ModelArchive *D_801E9C38;

/* Resident data no shared header declares: the movie's last frame, which
 * the script sets for the movie it starts. */
extern u16 D_80062514;

/* Resident functions whose callers convert arguments/result differently
 * from the resident definition (decomp/src/resident/own_declarations.h). */
void func_80032F54(Window *window, s32 vramX, s32 vramY, s32 x, s32 y, s32 columns, s32 lines);
void *func_80033728(void *messages, u16 message);
s32 func_800397FC(u8 *sequence, u8 volume, s32 arg2);
void func_80039F18(s32 sound, s16 arg1, s16 arg2);
void func_8003A2E4(s32 sound, u16 arg1);
void func_8003BDFC(s32 arg);

/* Battle functions the shared battle headers leave out: those whose callers
 * convert arguments/result differently from the battle's definition
 * (decomp/src/battle/own_declarations.h; 8007FF14, 800AA320, 800AA384,
 * 800B838C and 8009C0E0 are declared in their units), and 8008AB70. */
void func_800716D8(void);
void func_8007FF14(s32 arg);
void func_800800E8(s32 arg);
void func_800883AC(s32 arg);
u16 func_80089C08(u8 id);
void func_8008AB70(void);
void *func_8008ABB8(s32 size, s32 top);
void func_8008F8F4(s32 id, u16 x, u16 y, u16 width, u16 height, s32 style, s32 arg6);
void func_8008FA60(s32 id);
void func_8009C0E0(s32 arg);
void func_800AA320(u16 member, u16 target, u16 arg);
void func_800AA384(u16 member, u16 target, u16 arg);
void func_800B3658(u16 *position, u16 arg);
void func_800B39C0(u16 actor, s32 mode, s32 r, s32 g, s32 b);
void func_800B838C(u16 arg0, u16 arg1);
void func_800BCD98(s32 arg);

/* This module. */
u16 func_801E5768(ScriptThread *thread);
u16 func_801E57C4(ScriptThread *thread);
void func_801E57F8(u8 *insn, u8 count, u8 immediateMask, u8 signedForm);
u8 func_801E58EC(s16 a, s16 b, u8 op);
s32 func_801E5DCC(s32 thread, u8 *insn);
void func_801E5B00(); /* K&R (s16 x, s16 y); callers pass ints unconverted */
void func_801E6750(u8 actor, s32 flags, s32 x, s32 y, s32 width);
u8 func_801E6CE8(u16 message, u8 actor, u16 flags);
void func_801E7A5C(s32 thread, u8 *insn);
s32 func_801E84A4(s32 thread, u8 *insn);

#endif
