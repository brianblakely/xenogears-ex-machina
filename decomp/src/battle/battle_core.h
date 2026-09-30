#ifndef BATTLE_CORE_H
#define BATTLE_CORE_H

#include "common.h"

/* Combatant record: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes,
 * addressed absolutely from 800ccce8. */
typedef struct {
    u8 unk0[0x7C];
    u16 flags7C;  /* 0x80 inactive, 0x1000 slow (ticks every other frame),
                   * 0x2000 delay counter +0x15C active */
    u16 unk7E;
    u16 flags80;  /* 0x1000 timer held */
    u16 unk82;
    u16 status84; /* 0x8000 (with +0x86) haste */
    u16 status86;
    u8 unk88[0x15C - 0x88];
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
    u8 unk2C0[0x2D3 - 0x2C0];
    u8 actor;          /* +0x2D3 acting slot */
    u8 unk2D4[0x2DA - 0x2D4];
    u8 eventCount;     /* +0x2DA queued presentation events */
    u8 eventsDone;     /* +0x2DB */
    u8 unk2DC;
    u8 page;           /* +0x2DD command page */
    u8 menuDone;       /* +0x2DE */
    u8 unk2DF[0x2EB - 0x2DF];
    u8 reaction[3];    /* +0x2EB per party member */
    u8 unk2EE[0x2F6 - 0x2EE];
    u8 repeatArmed;    /* +0x2F6 */
} TurnState;

extern TurnState *D_800C3EAC;

/* Battle UI state: the heap block at *800d2d28. */
typedef struct {
    u8 unk0[0x7C];
    u8 reaction[3];    /* +0x7C */
    u8 unk7F[0xB4 - 0x7F];
    u8 unkB4;
    u8 unkB5;
} BattleUi;

extern BattleUi *D_800D2D28;

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

extern u32 *D_800CCB04; /* current ordering table */
extern s32 D_800CCB34;  /* draw buffer index */

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
extern s16 D_800C48E8;
extern u16 D_800D2C9E;     /* party members whose timers are held */

typedef struct {
    u8 unk0[5];
    u8 unk5;
    u8 unk6[0x60 - 6];
} BattleUnk3720;

extern BattleUnk3720 D_800D3720[8];
extern u8 D_800D3014;

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
extern u8 D_800D2DC0;      /* forced next turn: slot + 1 */
extern u8 D_800D2DD7;      /* turn order cursor */
extern u8 D_800D2DD8[11];  /* turn order */      /* decoded menu input code; 8 = none */

extern u8 D_800C3D48;      /* the 801e5000 module is loaded */
extern u8 D_800C48EA;      /* battle outcome */
extern s32 D_800C3E54;
extern s32 D_800D3284;
extern s32 D_800D328C;
extern u8 D_800D3298;      /* ATB enabled */
extern u8 D_800D2DCC[11];  /* slot present */
extern u8 D_800D2DE4[11];  /* slot ready to act */
extern s16 D_800D2DF0[2][11]; /* turn timers: [0] reload values, [1] counters */
extern u16 D_800D2E1C[11]; /* slow-status alternation */
extern s32 *D_8005917C;

/* Resident services. */
void func_80043B48(u32 *ot, void *prim);
void func_80043C24(void *prim, s32 abe);
void *func_80033728(void *table, s32 index);
s32 func_80034EAC(void *text, u32 *pixels, s32 width, s32 mode);
void func_80044894(RECT *rect, u32 *pixels);
void func_800295D8(s32, s32, s32, s32);
void func_8003A89C(s32, s32, s32);

/* Battle overlay. */
void func_8008AB4C(void);
s32 func_8008ABB8(s32 size, s32 mode);
void func_8008AC50(void);
s32 func_80098AF8(s32 slot, s32 mode);
void func_80079E18(s32);
void func_80079E4C(s32);
s32 func_800716D8(void);
u16 func_80089C9C(u16 mask, u8 slot);
void func_80085AC4(u8 slot);
void func_80071B94(s32 mode);
void func_800BE790(void);

/* The 801e5000 module and the 80280000 module. */
void func_801E5160(void);
s32 func_801E563C(void);
void func_801E879C(s32);
void func_8028022C(void);

#endif
