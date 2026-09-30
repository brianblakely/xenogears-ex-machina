#ifndef BATTLE_CORE_H
#define BATTLE_CORE_H

#include "common.h"

/* PsyQ GPU types. */
typedef struct {
    s16 x, y, w, h;
} RECT;

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
} POLY_FT4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} POLY_F4;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_MODE;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LINE_F2;

/* Combatant record: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes,
 * addressed absolutely from 800ccce8. */
typedef struct {
    u16 unk0;
    u8 unk2;
    u8 unk3;
    u8 unk4[0x34 - 4];
    u16 unk34;         /* 0x800 reacts while down */
    u8 unk36[0x4C - 0x36];
    u16 hp;           /* +0x4C */
    u16 unk4E;
    u8 unk50[0x56 - 0x50];
    u8 unk56;
    u8 unk57[0x5A - 0x57];
    u8 speed;          /* +0x5A */
    u8 unk5B;
    u8 unk5C[0x7A - 0x5C];
    u16 unk7A;
    u16 flags7C;  /* 0x80 inactive, 0x1000 slow (ticks every other frame),
                   * 0x2000 delay counter +0x15C active */
    u16 unk7E;
    u16 flags80;  /* 0x1000 timer held */
    u16 unk82;
    u16 status84; /* 0x8000 (with +0x86) haste */
    u16 status86;
    u16 unk88;
    u16 unk8A;
    u16 unk8C;
    u16 unk8E;
    u8 unk90[0xA4 - 0x90];
    u8 unkA4[0xDC - 0xA4]; /* the slot's attacker block */
    u16 unkDC;
    u8 unkDE[0x104 - 0xDE];
    u32 unk104;
    s32 unk108;
    u8 unk10C[0x120 - 0x10C];
    u16 unk120;
    u8 unk122[0x13C - 0x122];
    u8 gearSpeed;      /* +0x13C */
    u8 unk13D[0x14C - 0x13D];
    s32 unk14C;
    u8 unk150[6];
    u16 unk156;
    u8 unk158[2];
    u8 unk15A;
    u8 unk15B;
    u8 delay15C;
    u8 unk15D[0x170 - 0x15D];
} BattleRecord;

extern BattleRecord D_800CCCE8[11];

/* Per-slot block of the turn state (0x40 bytes). */
typedef struct {
    u8 unk0[0x1C];
    u16 items[16];     /* menu item availability, 0 = available */
    u8 defaultTarget;  /* +0x3C */
    u8 unk3D[3];
} TurnSlot;

/* Turn and menu state: the heap block at *800c3eac. */
typedef struct {
    TurnSlot slots[11];
    u8 unk2C0[0x2CC - 0x2C0];
    u8 unk2CC[7];
    u8 actor;          /* +0x2D3 acting slot */
    u8 unk2D4[2];
    u8 unk2D6;
    u8 unk2D7[0x2DA - 0x2D7];
    u8 eventCount;     /* +0x2DA queued presentation events */
    u8 eventsDone;     /* +0x2DB */
    u8 unk2DC;         /* action index + 1 */
    u8 page;           /* +0x2DD command page */
    u8 menuDone;       /* +0x2DE */
    u8 unk2DF[0x2E8 - 0x2DF];
    u8 unk2E8;         /* attack page target */
    u8 unk2E9;
    u8 unk2EA;
    u8 reaction[3];    /* +0x2EB per party member */
    u8 unk2EE[0x2F6 - 0x2EE];
    u8 repeatArmed;    /* +0x2F6 */
} TurnState;

extern TurnState *D_800C3EAC;

/* Battle UI state: the heap block at *800d2d28. */
typedef struct {
    u8 unk0[0x30];
    u32 unk30;         /* CLUT cycle position */
    u8 unk34[0x7B - 0x34];
    u8 unk7B;          /* AP text part count */
    u8 reaction[3];    /* +0x7C */
    u8 unk7F[0x8E - 0x7F];
    u8 unk8E;
    u8 unk8F[0x93 - 0x8F];
    u8 unk93[3];
    u8 unk96;          /* file 3 block loaded */
    u8 unk97;          /* selected list row */
    u8 unk98;
    u8 unk99[3];       /* party panel digit buffers */
    u8 unk9C;
    u8 unk9D;
    u8 unk9E;
    u8 unk9F[0xA3 - 0x9F];
    u8 unkA3;
    u8 unkA4;
    u8 unkA5;
    u8 unkA6;
    u8 unkA7;
    u8 unkA8;
    u8 unkA9;
    u8 unkAA;          /* frame counter */
    u8 unkAB;
    u8 unkAC;
    u8 unkAD;
    u8 unkAE;          /* menu module block loaded */
    u8 unkAF;
    u8 windows[7];     /* +0xB0 window shown */
    u8 unkB7;          /* command window page */
    u8 unkB8[7];       /* window opening */
    u8 unkBF[7];       /* window fully open */
    u8 unkC6;
    u8 unkC7;
    u8 unkC8;
    u8 unkC9;
    u8 unkCA;
    u8 unkCB;
    u8 unkCC[4];
    s32 unkD0[2];
    u8 unkD8[0xE0 - 0xD8];
    s32 unkE0[3];
    s32 unkEC[3];
    s32 unkF8;
    s32 unkFC;
    s32 unk100;
} BattleUi;

extern BattleUi *D_800D2D28;

/* The 0x670-byte graphics block (*800c3ea4 + 0xa230). */
typedef struct {
    POLY_FT4 unk0[2];
    POLY_FT4 unk50[2];
    POLY_FT4 unkA0[2];
    POLY_FT4 unkF0[2];
    u8 unk140[0x280 - 0x140];
    POLY_FT4 unk280[2];
    POLY_FT4 unk2D0[2];
    POLY_FT4 unk320[2];
    POLY_FT4 unk370[2];
    u8 unk3C0[0x668 - 0x3C0];
    u8 unk668;         /* buffer of the +0x0..+0xf0 quads */
    u8 unk669;
    u8 buffer;         /* +0x66A */
    u8 unk66B;
    u8 unk66C;
    u8 unk66D;
    u8 unk66E[2];
} GraphicsBlock;

/* Party status panel state of the graphics block (0x1e4 bytes). */
typedef struct {
    u8 unk0[0x1E1];
    u8 unk1E1;         /* 1: show the 800d32a0 value */
    u8 unk1E2[2];
} PartyPanel;

/* A texture location of the graphics state (0x18 bytes). */
typedef struct {
    s32 mode;
    s32 clutX;
    s32 clutY;
    s32 x;
    s32 y;
    s32 unk14;
} GraphicsTexture;

/* Battle graphics state (*800c3ea4). */
typedef struct {
    u8 unk0[0x908];
    LINE_F2 unk908[12];
    POLY_FT4 unk9C8[6][2];
    POLY_FT4 unkBA8[120];
    POLY_FT4 unk1E68[60];
    POLY_FT4 unk27C8[1];
    u8 unk27F0[0x3A88 - 0x27F0];
    POLY_FT4 unk3A88[3][80]; /* party panel name glyphs */
    POLY_FT4 unk6008[3][8];  /* party panel value digits */
    POLY_F4 unk63C8[2];
    DR_MODE unk63F8[2];
    s32 unk6410;       /* shade */
    u8 unk6414;
    u8 unk6415;
    u8 unk6416;        /* fading down */
    u8 unk6417[5];
    POLY_FT4 unk641C[2][100];
    PartyPanel panels[3];    /* +0x835C */
    u8 unk8908[0x8950 - 0x8908];
    RECT unk8950[4];
    u32 unk8970[4][0x630 / 4]; /* four CLUT strips, cycled */
    GraphicsBlock *unkA230;
    u8 unkA234[4];
    GraphicsTexture textures[1]; /* +0xA238 */
} BattleGraphics;

extern BattleGraphics *D_800C3EA4;
extern u8 D_800C492A;

/* Battle drawing state (800ccb04). */
typedef struct {
    u32 *ot;           /* current ordering table */
    u8 unk4[0x2C];
    s32 buffer;        /* +0x30 draw buffer index */
} BattleDraw;

extern BattleDraw D_800CCB04;
extern void *D_800D2F5C; /* glyph table */
extern u8 D_800C3E4C;   /* battle end state */

/* The battle message image (800d39b8): upload rectangle and pixels. */
typedef struct {
    RECT rect;
    u32 *pixels;
    u8 unkC[2];
    s8 width;          /* +0xE */
} MessageImage;

extern MessageImage D_800D39B8;
extern void *D_800D39F0;   /* battle message table */
extern u8 D_800D2CAF;      /* pending battle message id */
extern s16 D_800D2C94;     /* committed target mask */
extern u16 D_800D2C96;     /* alive mask at commit */
extern s16 D_800D2C98;     /* committed animation */
extern u8 D_800D2CA9;      /* committing actor */
extern u8 D_800D2CAA;      /* committed action index */
extern s16 D_800C48E8;
extern u16 D_800D2C9E;     /* party members whose timers are held */

/* Eight 0x60-byte message entries from 800d36c8. */
typedef struct {
    POLY_FT4 prims[2];
    u8 unk50[0x5C - 0x50];
    u8 alternate;      /* +0x5C odd texture row */
    u8 shown;          /* +0x5D */
    u8 width;          /* +0x5E */
    u8 unk5F;
} BattleMessage;

extern BattleMessage D_800D36C8[8];
extern u8 D_800D3014;
/* Formation data (*800d3364). */
typedef struct {
    u8 distance;
    u8 unk1[7];
} GroupLink;

typedef struct {
    u8 unk0[0x140];
    GroupLink links[8][8]; /* +0x140 per formation-group pair */
} Formation;

extern Formation *D_800D3364;

/* Presentation event queue entry (0x48 bytes, 32 from 800c3fe8). */
typedef struct {
    u16 amounts[11];
    u16 targets;       /* +0x16 */
    u8 codes[11];      /* +0x18 */
    u8 actor;          /* +0x23 */
    u16 totals[11];    /* +0x24 */
    u16 param;         /* +0x3A */
    u8 totalCodes[11]; /* +0x3C */
    u8 type;           /* +0x47 */
} BattleEvent;

extern BattleEvent D_800C3FE8[32];

/* Action list entry (8 bytes, 32 from 800d2e5c). */
typedef struct {
    u8 type;
    u8 arg1;
    u8 animation;
    u8 named;
    u8 param;
    u8 unk5;
    u16 targets;
} BattleAction;

extern BattleAction D_800D2E5C[32];
extern u16 D_800D39E0;     /* mask of slots that act together */
extern u8 D_800D2CE0[0x30]; /* item ids */
extern u8 D_800D2CB0[0x30]; /* item counts */
extern u8 D_800D2C8B[8];
extern s32 D_800D2C60[8];

typedef struct {
    u8 unk0[0x34];
    u8 active;         /* +0x34 */
    u8 unk35[3];
} BattleUnk3278Entry;

typedef struct {
    BattleUnk3278Entry entries[16];
    u8 unk380[0x394 - 0x380];
    s16 unk394[(0x7A4 - 0x394) / 2];
    POLY_FT4 unk7A4[2];
    u8 unk7F4;
} BattleUnk3278;

extern BattleUnk3278 *D_800D3278;

typedef struct {
    u8 unk0[0x3AC0];
    POLY_FT4 unk3AC0[24];   /* list 0 */
    POLY_FT4 unk3E80[34];   /* list 2 */
    POLY_FT4 unk43D0[58];   /* list 10 */
    POLY_FT4 unk4CE0[10];
    POLY_FT4 unk4E70[8];    /* list 3 */
    POLY_FT4 unk4FB0[6];    /* list 4 */
    POLY_FT4 unk50A0[8];    /* list 5 */
    POLY_FT4 unk51E0[22];   /* list 6 */
    POLY_FT4 unk5550[6];    /* list 11 */
    POLY_FT4 unk5640[40];   /* list 12 */
    POLY_FT4 unk5C80[6];    /* list 13 */
    u8 unk5D70[5];
    u8 counts[15];          /* +0x5D75 primitive count per list */
    u8 buffers[15];         /* +0x5D84 draw buffer per list */
    u8 unk5D93[0x5D9C - 0x5D93];
    s16 unk5D9C;
    s16 unk5D9E;
    u8 unk5DA0;
    u8 unk5DA1;
    u16 unk5DA2;
} BattleUnk2DB4;

extern BattleUnk2DB4 *D_800D2DB4;
extern s32 D_800D2DAC;

/* Enemy AI block (0x40 bytes per enemy slot 3..10, from 800d3400). */
typedef struct {
    u8 *script;        /* +0x00 */
    u8 *unk4;
    u8 *reaction;      /* +0x08 reaction script */
    u8 unkC[4];
    s32 longs[4];      /* +0x10 */
    u16 vars[8];       /* +0x20 */
    u8 bytes[16];      /* +0x30 */
} EnemyAi;

extern EnemyAi D_800D3400[8];

/* Enemy reaction state (4 bytes per enemy from 800c3d18). */
typedef struct {
    u8 armed;          /* the reaction script runs */
    u8 unk1[2];
    u8 unk3;
} EnemyReaction;

extern EnemyReaction D_800C3D18[8];
extern u8 D_800C3E8C;      /* pending battle message + 1 */
extern u8 D_800D366C;      /* menu effects enabled */
extern u8 D_800D2CA4[5];
extern u8 D_800C204C;
extern u8 D_800C3E18;
extern u8 D_800C3D70[0x30];
extern u8 D_800D2D24[3];   /* party character ids */
extern void *D_800D367C;   /* menu module block */
extern void *D_800C3DE8;   /* file 3 block */
extern u8 D_800C3CF4[9];   /* decimal digits */

typedef struct {
    u8 unk0[0x14];
    u16 bank;
} SoundSystem;

extern SoundSystem *D_8005919C;
/* Window rectangle (*800d2d90[window]). */
typedef struct {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u16 curW;          /* +0x8 opening size */
    u16 curH;
    u8 style;          /* +0xC */
} WindowRect;

extern void *D_800D2E38[7]; /* window blocks */
extern WindowRect *D_800D2D90[7];

/* Command descriptor (0x28 bytes). */
typedef struct {
    u8 unk0[0xA];
    u16 unkA;
    u8 unkC[0x11 - 0xC];
    u8 unk11;
    u8 unk12[2];
    u8 unk14;
    u8 unk15[0x1C - 0x15];
    u8 unk1C;
    u8 unk1D;
    u16 unk1E;
    u8 unk20[7];
    s8 weight;         /* +0x27 turn timer penalty */
} CommandDescriptor;

/* Battle state (800ccce8): the records and per-action arrays. */
typedef struct {
    BattleRecord records[11];
    u8 unkFD0[0x1058 - 0xFD0];
    CommandDescriptor partyCommands[3][38]; /* +0x1058 */
    CommandDescriptor gearCommands[3][42];  /* +0x2228 */
    u8 unk35D8[0x5F54 - 0x35D8];
    s32 unk5F54[3];
    s32 unk5F60[3];
    s32 damage[12];    /* +0x5F6C */
    u8 unk5F9C[4];
    u8 resultCodes[12]; /* +0x5FA0 */
    u16 unk5FAC;       /* effect target mask */
    u8 unk5FAE[2];
    u16 unk5FB0;
    u8 unk5FB2[0x5FC2 - 0x5FB2];
    u8 command;        /* +0x5FC2 current command index */
    u8 unk5FC3[0x5FC7 - 0x5FC3];
    u8 unk5FC7;
} BattleState;

/* Effect table (0x10 bytes from 800d2200). */
typedef struct {
    u8 unk0[0xE];
    u16 unkE;
} EffectEntry;

extern EffectEntry D_800D2200[];
extern u8 D_800D2DC4;
extern u8 *D_800D2D6C;     /* attacker block */
extern BattleRecord *D_800C3E34; /* target record */
extern s32 D_800D2C54[12]; /* per-slot damage */
extern u8 D_800D2C88[12];  /* per-slot result code */
extern void *D_800D329C;   /* item name table */

/* Stepped line state (80088 87c). */
extern s32 D_800C3A7C;
extern s32 D_800C3A80;
extern s32 D_800C3A84;
extern s32 D_800C3A88;
extern s32 D_800C3A8C;
extern s32 D_800C3A90;
extern s8 D_800C3A94;
extern s8 D_800C3A98;
extern s32 D_800C3A9C;
extern s8 D_800C207C;
extern s32 D_800C2080;
extern s32 D_800C2084;

extern BattleState *D_800C34B0;

/* Resolver globals. */

extern BattleRecord *D_800C3E00; /* attacker record */
extern CommandDescriptor *D_800C3DFC;
extern u8 D_800C3E50;      /* target slot */
extern u8 D_800C3E90[12];  /* default-target candidates */
extern u8 D_800D3274;      /* candidate count */
extern u16 D_800C3D64;
extern u8 D_800C3E2C;
extern u16 D_800D2C30;
extern u8 D_800D2C38;
extern u8 D_800CCC58;
extern s32 D_800D3288;
extern u8 D_800D39D4;
extern u16 D_800C3608;     /* slots that still count while down */
extern u8 D_800D3280;      /* party panel layout */
extern s16 D_800C3254[][3]; /* party panel x per layout */
extern s16 D_800C3076[3][24]; /* party panel name glyph x */
extern u8 D_800D2D88[5];   /* name glyph codes */
extern u16 D_800D2C2A;
extern u8 D_800D2C35;
extern u8 D_800D2C36;
extern u16 D_800D2C3A;
extern u8 D_800C3200[][6]; /* list separator rows by row count */
extern u8 D_800D2C34;
extern u8 D_800C34CC[];    /* combo step flags */

/* Direction arrow block (*800c3e24, 0xec bytes). */
typedef struct {
    u8 unk0[0xE6];
    u8 arrows[4];      /* +0xE6 a target lies that way */
    u8 unkEA[2];
} DirectionArrows;

extern DirectionArrows *D_800C3E24;

/* Persistent character records (resident, 0x20 bytes from 8006ecf8). */
typedef struct {
    u16 combos;
    u8 unk2[0x1E];
} CharacterCombos;

extern CharacterCombos D_8006ECF8[];

/* Per-slot formation information (0x1c bytes from 800c3eb4). */
typedef struct {
    u8 group;          /* +0x0 formation group */
    u8 member;         /* +0x1 */
    u8 unk2;           /* 0x7f: none */
    u8 hidden;         /* +0x3 */
    u8 gear;           /* +0x4 fights in gear */
    u8 unk5[5];
    u16 unkA;
    u8 unkC[0x1C - 0xC];
} SlotInfo;

extern SlotInfo D_800C3EB4[11];

/* Per-slot flags (8 bytes from 800d32a0). */
typedef struct {
    u8 unk0;
    u8 unk1;
    u8 unk2[6];
} SlotFlags;

extern SlotFlags D_800D32A0[11];

/* Formation group entries (4 bytes from 800d301c). */
typedef struct {
    u8 count;
    u8 members;        /* member bits */
    u8 unk2[2];
} GroupEntry;

extern GroupEntry D_800D301C[16];
extern u16 D_800D39DC;     /* alive mask */
extern u16 D_800C3448[16]; /* slot bits */
extern u16 D_800C3468[16]; /* flag bits */
extern u8 D_800D2D5C[11];  /* running result code per slot */
extern s16 D_800D2D70[11]; /* running result amount per slot */
extern u8 D_800D2DC0;      /* forced next turn: slot + 1 */

extern u8 D_800C3D48;      /* the 801e5000 module is loaded */
extern u8 D_800C48EA;      /* battle outcome */
extern s32 D_800C3E54;
extern s32 D_800D3284;
extern s32 D_800D328C;
extern u8 D_800D3298;      /* ATB enabled */

/* Turn queue (800d2dcc). */
typedef struct {
    u8 present[11];    /* slot takes part */
    u8 cursor;         /* +0x0B turn order position */
    u8 order[11];      /* +0x0C slots in turn order */
    u8 unk17;
    u8 ready[11];      /* +0x18 slot ready to act (0xff: out) */
    u8 unk23;
    s16 timers[3][11]; /* +0x24 [0] reload values, [1] counters,
                        * [2] slow-status alternation */
} TurnQueue;

extern TurnQueue D_800D2DCC;
extern s32 *D_8005917C;
extern u8 D_8005959C;
extern s32 D_800595A0;
extern u16 D_80059414;
extern u16 D_800595D4;
extern s16 D_8005A3A0[];
extern s32 D_8006EF58;     /* party gold */

/* Resident services. */
void func_80039DB8(s32 effect);
void func_80028470(s32 a, s32 b);
void func_80032498(s32 owner, s32 b);
s32 func_80031BDC(s32 size, s32 mode);
s32 func_800286CC(void);
u8 func_8001BD40(u8 low, u8 high);
s32 func_8003FA38(void);
void func_80043B48(u32 *ot, void *prim);
void func_80043C24(void *prim, s32 abe);
void func_80043BFC(void *prim, s32 tge);
void func_80043CB0(void *prim);
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);
u16 func_80043A58(s32 x, s32 y);
void func_800445D0(s32 mode);
void func_8003F8E8(void *block, s32 size);
void func_800320E8(void *block);
s32 func_800288EC(s32 file);
void func_80034888(s32 arg0, u32 *ot, s32 buffer);
s32 func_8002675C(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y, s32 scale);
s32 func_800263E4(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 w, s32 scale, s32 a, s32 b);
void *func_80033728(void *table, s32 index);
s32 func_80034EAC(void *text, u32 *pixels, s32 width, s32 mode);
void func_80044894(RECT *rect, u32 *pixels);
void func_800295D8(s32, s32, s32, s32);
void func_8003A89C(s32, s32, s32);

/* Battle overlay. */
void func_8008AB4C(void);
s32 func_8008ABB8(s32 size, s32 mode);
void func_8008AC50(void);
void func_8008FAD8(void);
void func_80073FB8(void);
void func_80073538(void);
void func_80073F08(void);
void func_8007500C(void);
void func_80074F70(void);
void func_80088B80(void);
void func_80074AB8(void);
void func_80076418(void);
void func_800764B4(void);
void func_800764EC(void);
void func_80076AC8(POLY_FT4 *prim);
void func_800742A0(void);
void func_80075938(void);
void func_80073A58(void);
void func_80073B64(void);
void func_80073E88(void);
void func_80074EEC(void);
void func_800745EC(void);
void func_80074D4C(void);
void func_80077074(void);
void func_800785D4(u8 actor, u8 index);
void func_80078658(u8 index, u8 actor);
void func_800787E0(u8 value, u8 actor);
void func_8007887C(u8 actor);
void func_8007893C(u8 index, u8 actor);
void func_80085350(void);
void func_80085388(void);
void func_800B89FC(s32 mode, u8 actor, s32 arg2, s32 arg3);
s32 func_80080AE4(u8 actor);
void func_800793F0(u8 actor);
void func_80079674(u8 actor);
void func_80079948(u8 **pc);
u8 func_8007F8C0(u8 **pc, u8 enemy);
u8 func_8007EF6C(u8 **pc, u8 enemy, u8 count);
void func_80079934(u8 **pc);
void func_800728B8(POLY_FT4 *prims, s32 count, s32 first);
void func_80073380(s32 member);
u8 func_80072F38(s32 member, u8 flag);
void func_80072DA8(s32 member, s32 mode);
void func_80072A9C(s32 member, u8 value);
void func_80072938(POLY_FT4 *prims, s32 first, s32 last, u8 mode);
void func_80076C78(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, u8 w);
s32 func_80098AF8(s32 slot, s32 mode);
void func_80079E18(u8 index);
void func_80079E4C(u8 index);
s32 func_800716D8(void);
u8 func_80079E7C(u16 mask);
u8 func_8007A628(u8 slot, u8 any);
u8 func_8007A6C8(u8 slot, u8 arg1);
u8 func_8007A744(u8 slot);
u16 func_80089C08(u8 slot);
u16 func_80089BEC(u8 bit);
u16 func_80089C48(u8 slot);
void func_80098C6C(u16 param);
void func_80085454(u8 queue);
void func_80085618(u8 queue);
void func_800941A4(void);
void func_8008860C(void);
void func_80089038(void);
void func_80089110(void);
void func_800891E4(void);
void func_80089348(void);
void func_8008946C(void);
void func_8008963C(void);
void func_800897CC(void);
void func_8007FCE8(void);
void func_8007FDEC(void);
void func_800800E8(u8 member);
void func_8007FB70(u8 member);
s32 func_8009ADA0(u8 slot, s32 *amounts);
void func_800BE538(u8 slot, s32 a, s32 b, s32 c);
s32 func_80076A10(s32 id, POLY_FT4 *prims, s16 x, s16 y);
s32 func_800877E0(u8 actor, u8 target);
void func_80085D34(void);
void func_800879A8(u8 actor, u8 target);
void func_8008FA60(u8 window);
void func_8007765C(void);
void func_8008AB94(void);
void func_8008A684(u8 member);
void func_8008A274(u8 member);
void func_8008A3EC(u8 member);
void func_800BC404(u16 mask);
void func_800BCD98(u16 mask);
void func_80077980(void);
u8 func_80083FF4(u8 member, u8 slot);
void func_80098D2C(u8 slot, u8 param);
void func_8009AC48(u8 slot, s32 mode);
u8 func_80084854(u8 origin, u8 direction);
void func_80093B08(u8 member);
s32 func_8008AC00(s32 count);
void func_800769E8(RECT *rect, u32 *pixels);
void func_80097D08(void);
void func_80094D24(void);
u8 func_800841E0(u8 member);
u16 func_80089C6C(u16 mask, u8 bit);
void func_8008AAA0(u32 value);
void func_80076BF0(POLY_FT4 *prim);
void func_80089CCC(s32 mode);
void func_8008A144(void);
void func_8007171C(void);
void func_80076B68(POLY_FT4 *prim);
void func_8008FC1C(s16 x, s16 y, s16 w, s16 top, u8 rows);
void func_8008F6E4(u8 style, u16 x, u16 y, u16 w, u16 h);
void func_80077454(u8 window);
void func_80090310(u8 column, u8 row);
void func_8009070C(u8 column, u8 row);
void func_800904A0(u8 column, u8 row);
s8 func_80097964(u8 a, u8 b, u16 c);
void func_800995A0(u8 slot, u8 a, u16 b, s32 mode);
void func_80078508(u8 *order);
void func_80087EDC(u8 actor, u8 target);
void func_800883AC(u8 slot);
u16 func_80089B50(u16 low, u16 high);
u8 func_80079ED8(u8 slot, u8 attribute, u8 value, u8 write);
u16 func_8007A280(u8 slot, u8 attribute, u16 value, u8 write);
u16 func_80089C9C(u16 mask, u8 slot);
void func_80085AC4(u8 slot);
void func_80071B94(s32 mode);
void func_800BE790(void);

/* The 801e5000 module and the 80280000 module. */
void func_801E5160(void);
void func_801DE594(void);
s32 func_801E563C(void);
void func_801E879C(s32);
void func_8028022C(void);

#endif
