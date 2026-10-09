#ifndef BATTLE_EVENT_SCRIPT_H
#define BATTLE_EVENT_SCRIPT_H

#include "common.h"
#include "psyq/libgpu.h"
#include "resident/sound.h"

/* The battle event script interpreter (ovl3087, loaded at 801e5000 when the
 * formation sets 800c3d48): its state, which the battle overlay also reads
 * and writes (thread waits, variables, the halted flag, the portrait quads),
 * the interpreter's entries the battle calls, and the battle's side of it. */

/* 0x38 bytes per script thread. Each level runs one of the thread's script
 * entries (the 16-byte entry table of the script file). */
typedef struct ScriptThread {
    u16 pc[8];      /* 0x00 program counter per level (0xffff free) */
    u8 priority[8]; /* 0x10 level priority (0xff free) */
    u8 entry[8];    /* 0x18 entry index running at each level (0xff none) */
    u8 level;       /* 0x20 running level */
    u8 runningEntry; /* 0x21 entry index of the running level */
    u8 order; /* 0x22 run-order request (0xfe: run first, opcode 2c) */
    u8 request; /* 0x23 entry this thread requested of another (0xff none) */
    u8 speaker; /* 0x24 actor whose messages this thread shows */
    u8 pad25;
    s16 waitTimer; /* 0x26 frames left of a wait (opcode 2b); the battle
                    * counts it down while `waiting` */
    u8 waiting;    /* 0x28 */
    u8 pad29[3];
    void *modelFile; /* 0x2c model data loaded for this slot (opcode 35) */
    s32 model;       /* 0x30 model instance */
    u8 memberState;  /* 0x34 opcode 23's effect for object n: 2 started, 1 done
                      * (set by the effect VM's 02/03 through 80080c6c) */
    u8 modelLoaded;  /* 0x35 */
    u8 pad36[2];
} ScriptThread;

/* The interpreter state (0x828 bytes, pointer 800d3278). */
typedef struct ScriptState {
    ScriptThread threads[16]; /* 0x000 */
    u16 operands[8];          /* 0x380 decoded operands */
    u8 *code;       /* 0x390 script bytecode */
    u16 vars[0x200]; /* 0x394 script variables */
    u8 order[16];   /* 0x794 thread run order */
    POLY_FT4 quads[2]; /* 0x7a4 */
    u8 portraitBuffer; /* 0x7f4 draw buffer the portrait quad was laid out for */
    u8 unk7F5;
    u16 window[5]; /* 0x7f6 message window layout (opcode 1a) */
    u8 halted;     /* 0x800 the script ended the battle */
    u8 unk801;
    u8 windowOpen; /* 0x802 a message window is open */
    u8 pad803;
    u8 actionRunning[16]; /* 0x804 actor action in progress (cleared on completion) */
    u8 *music;            /* 0x814 music sequence buffer */
    SoundBank *soundBank; /* 0x818 the script's sound effect bank */
    s16 musicId;          /* 0x81c */
    u8 musicVolume;       /* 0x81e */
    u8 musicPlaying;      /* 0x81f */
    u8 soundBankLoaded;   /* 0x820 */
    u8 pad821[7];
} ScriptState;

/* The script file: thread count at 0x40, then 16 bytes per thread with its
 * level entry points, then the bytecode. */
typedef struct {
    u16 entry[8];
} EventScriptEntry;

typedef struct EventScriptFile {
    u8 pad0[0x40];
    u32 threadCount;
    EventScriptEntry entries[1];
} EventScriptFile;

extern ScriptState *D_800D3278;
extern EventScriptFile *D_800D39D0; /* the script file */
extern void *D_800D3340;            /* the script set's data */

/* The battle's side: the module block and its load, a byte forwarded to the
 * module (80070E2C's unit) and a thread's member state (80079ED8's). */
extern u8 D_800C3D48;      /* the 801e5000 module is loaded */
extern s32 D_800D3284;     /* its block */
extern s32 D_800D328C;

void func_80070EB0(s32 value); /* forward a byte to the module when it is loaded */
void func_80080C6C(u8 index);  /* set thread index's member state to done */

/* The interpreter's entries (ovl3087). */
void func_801E5160(void); /* load the script set and set up the threads */
s32 func_801E563C(void);  /* release everything; whether it handled the music */

#endif
