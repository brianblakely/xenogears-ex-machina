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
    u8 order; /* 0x22 run-order request (0xfe: run first, opcode 2c) */
    u8 request; /* 0x23 entry this thread requested of another (0xff none) */
    u8 speaker; /* 0x24 actor whose messages this thread shows */
    u8 pad25;
    s16 waitTimer; /* 0x26 frames left of a wait (opcode 2b) */
    u8 waiting;    /* 0x28 */
    u8 pad29[3];
    void *modelFile; /* 0x2c model data loaded for this slot (opcode 35) */
    s32 model;       /* 0x30 model instance */
    u8 memberState;  /* 0x34 move state of party member n (opcode 23) */
    u8 modelLoaded;  /* 0x35 */
    u8 pad36[2];
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

/* libgpu primitive colour macro. */
#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3) \
    ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), \
     (p)->x2 = (_x2), (p)->y2 = (_y2), (p)->x3 = (_x3), (p)->y3 = (_y3))
#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3) \
    ((p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), \
     (p)->u2 = (_u2), (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3))

typedef struct {
    s16 x, y, w, h;
} Rect;

/* libgs TIM_IMAGE (ReadTIM). */
typedef struct {
    u32 mode;
    Rect *crect;
    u32 *caddr;
    Rect *prect;
    u32 *paddr;
} TimImage;

/* The interpreter state (0x828 bytes, pointer 800d3278). */
typedef struct {
    ScriptThread threads[16]; /* 0x000 */
    u16 operands[8];          /* 0x380 decoded operands */
    u8 *code;       /* 0x390 script bytecode */
    u16 vars[0x200]; /* 0x394 script variables */
    u8 order[16];   /* 0x794 thread run order */
    PolyFT4 quads[2]; /* 0x7a4 */
    u8 portraitBuffer; /* 0x7f4 draw buffer the portrait quad was laid out for */
    u8 unk7F5;
    u16 window[5]; /* 0x7f6 message window layout (opcode 1a) */
    u8 halted;     /* 0x800 the script ended the battle */
    u8 unk801;
    u8 windowOpen; /* 0x802 a message window is open */
    u8 pad803;
    u8 actionRunning[16]; /* 0x804 actor action in progress (cleared on completion) */
    u8 *music;            /* 0x814 music sequence buffer */
    struct SoundBank *soundBank; /* 0x818 the script's sound effect bank */
    s16 musicId;          /* 0x81c */
    u8 musicVolume;       /* 0x81e */
    u8 musicPlaying;      /* 0x81f */
    u8 soundBankLoaded;   /* 0x820 */
    u8 pad821[7];
} ScriptState;

typedef struct SoundBank {
    u8 pad0[0x14];
    u16 id; /* 0x14 bank number, the high half of a sound id */
} SoundBank;

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
    u8 pad2818[0x853D - 0x2818];
    u8 unk853D;
} BattleGraphics;

/* Battle state at pointer 800c3eac, only the field this module uses. */
typedef struct {
    u8 pad0[0x2DA];
    u8 unk2DA;
    u8 pad2DB[0x2EB - 0x2DB];
    u8 unk2EB;
} BattleState;

/* Battle objects (pointer table 800d3368, 11 entries). */
typedef struct {
    u8 pad0[0x35];
    u8 unk35;
} BattleObject;
extern BattleObject *D_800D3368[11];
extern u8 D_800C4000[];
extern u8 D_801E9C20[16]; /* actor action started by the script */
extern BattleState *D_800C3EAC;

/* Battle UI state (pointer 800d2d28), only the fields this module uses. */
typedef struct {
    u8 pad0[0x9E];
    u8 portraitShown; /* 0x9e */
    u8 pad9F[8];
    u8 portraitBuffer; /* 0xa7 */
    u8 padA8[0xBF - 0xA8];
    u8 windowReady; /* 0xbf */
    u8 padC0[8];
    u8 unkC8;
    u8 unkC9;
    u8 unkCA;
    u8 padCB[4];
    u8 unkCF;
    u8 padD0[0x30];
    s32 portraitHandle; /* 0x100 */
} BattleUi;

extern BattleGraphics *D_800C3EA4;
extern BattleUi *D_800D2D28;
/* The current draw buffer is field 0x34 of the battle draw state at
 * 800ccb00; the original accesses it as a structure field. */
typedef struct {
    s32 index;
} DrawBufferIndex;
extern DrawBufferIndex D_800CCB34;
extern u8 D_800D2D24[3]; /* battle party character ids (0xff none) */
extern s32 D_801E9C1C;
extern u16 D_801E9C10[5]; /* default message window layout */
extern u8 D_8005942C;
extern u8 D_801E9B5C[]; /* portrait file per actor (normal, mirrored) */
extern u8 D_800C3D44; /* battle ends */
extern u8 D_800C3E4C;
extern u8 D_800C48EA;
extern u8 D_800D2D50;
extern u8 D_800D2FC4;
extern u8 D_800C3D5C;
extern u8 D_8005947C;
extern u8 D_8005954C;
extern u16 D_8006F94E[4];
extern u8 D_8004FE44[4];
extern u16 D_80062514;
extern u8 D_800D3338;
/* The battle actor records are 0x170 bytes each from 800ccce8; this views
 * them from their flags halfword at 0x36. */
typedef struct {
    u16 flags;
    u8 pad2[0x170 - 2];
} ActorFlags;
extern ActorFlags D_800CCD1E[];

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

/* A file list entry for the resident loader (80029afc); file 0 ends it. */
typedef struct {
    s16 file;
    void *dest;
} FileRequest;

/* The script set of this battle is a field of the resident game data
 * (base not yet named); the original addresses it as a structure field. */
typedef struct {
    u8 scriptSet;
} GameDataScriptSet;
extern GameDataScriptSet D_8006F9DF;
/* The message text window (0x98 bytes, pointer 800d2dac). */
typedef struct {
    s16 column; /* 0x00 cursor */
    s16 row;    /* 0x02 */
    u8 pad4[0xC];
    u16 flags; /* 0x10 2 open, 4 printing, 8 waiting for a key */
    u8 pad12[0x58 - 0x12];
    u8 unk58;
} TextWindow;
extern TextWindow *D_800D2DAC;
extern s32 D_801E9C30; /* text origin */
extern s32 D_801E9C34;
extern u8 D_800D3014;  /* pressed key */
extern void *D_800D3340;
extern SoundBank *D_800C4924;

/* Script file 3: the model archive, entry offsets from +4. */
typedef struct {
    s32 count;
    void *entries[1];
} ModelArchive;
extern ModelArchive *D_801E9C38;

extern u8 D_80062648[];       /* resident music sequence buffer */
extern s32 D_800C3E54;        /* music sequence handle */
extern SoundBank *D_8005919C; /* resident sound effect bank */
extern u8 D_8006D940;
extern u8 D_800C3EB8;
extern u8 D_800CCD88;
extern u8 D_800CCE42;
extern u8 D_800D32A1;

/* Resident / battle services. */
void func_8001AC94(void);
void func_80032F54(TextWindow *window, s32 vramX, s32 vramY, s32 x, s32 y, s32 columns, s32 lines);
void *func_80033728(void *messages, u16 message);
void func_800345E0(TextWindow *window);
void func_80034614(TextWindow *window);
void func_800346D4(TextWindow *window);
void func_80034714(TextWindow *window, void *text);
void func_8008F8F4(s32 id, u16 x, u16 y, u16 width, u16 height, s32 style, s32 arg6);
void func_8008FA60(s32 id);
void func_80028470(s32 arg0, s32 arg1);
void DrawSync(s32 mode);
void LoadImage(Rect *rect, u32 *data);
void OpenTIM(void *tim);
void ReadTIM(TimImage *image);
void func_8001B66C(void);
s32 func_800288EC(s32 file);
void func_800295D8(s32 file, void *dest, s32 arg2, s32 arg3);
s32 func_800397FC(u8 *sequence, u8 volume, s32 arg2);
void func_800399D4(s32 handle);
void func_80039C4C(s32 handle);
void func_80039F18(s32 sound, s16 arg1, s16 arg2);
void func_8003A2E4(s32 sound, u16 arg1);
void func_8003A89C(s32 handle, s32 volume, s32 time);
void memmove(void *src, void *dest, s32 size);
s32 func_800286CC(void);
void func_800716D8(void);
void func_8007FF14(s32 arg);
void func_800800E8(s32 arg);
void func_80085388(void);
void func_800B838C(u16 arg0, u16 arg1);
void func_800B8D7C(void);
void func_800883AC(s32 arg);
void func_80088490(s32 arg);
void func_8008AB70(void);
void func_8008AC50(void);
void func_800BAF48(s32 arg);
void func_800320E8(void *block);
void *func_80032E88(void *data, s32 mode);
void func_80029AFC(FileRequest *files, s32 arg1, s32 arg2);
void func_8003342C(void *block);
void func_80038428(SoundBank *bank);
void func_8003852C(SoundBank *bank);
void func_8003A094(SoundBank *bank);
void func_8003BDFC(s32 arg);
void bzero(void *dest, s32 size);
u16 GetTPage(s32 mode, s32 rate, s32 x, s32 y);
u16 GetClut(s32 x, s32 y);
void SetSemiTrans(PolyFT4 *quad, s32 on);
void SetShadeTex(PolyFT4 *quad, s32 on);
void SetPolyFT4(PolyFT4 *quad);
void func_8008AB4C(void);
void *func_8008ABB8(s32 size, s32 top);
s32 func_80076A10(s32 id, PolyFT4 *quads, s16 x, s16 y);
u16 func_80089C08(u8 id);
void func_8009C0E0(s32 arg);
void func_800AA320(u16 member, u16 target, u16 arg);
void func_800AA384(u16 member, u16 target, u16 arg);
void func_800B3658(u16 *position, u16 arg);
void func_800B8D04(void);
void func_800BFBA0(void);
void func_800B39C0(u16 actor, s32 mode, s32 r, s32 g, s32 b);
u16 func_80089B50(u16 low, u16 high);
u16 func_80089C9C(u16 flag, u8 bit);

/* This module. */
u16 func_801E5768(ScriptThread *thread);
u16 func_801E57C4(ScriptThread *thread);
void func_801E57F8(u8 *insn, u8 count, u8 immediateMask, u8 signedForm);
u8 func_801E58EC(s16 a, s16 b, u8 op);
s32 func_801E5DCC(s32 thread, u8 *insn);
void func_801E5B00(s16 x, s16 y);
void func_801E6750(u8 actor, s32 flags, s32 x, s32 y, s32 width);
u8 func_801E6CE8(u16 message, u8 actor, u16 flags);
void func_801E7A5C(s32 thread, u8 *insn);
s32 func_801E84A4(s32 thread, u8 *insn);
void func_800BCD98(s32 arg);

#endif
