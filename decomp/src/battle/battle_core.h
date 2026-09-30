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

extern u8 D_800C3D48;      /* the 801e5000 module is loaded */
extern u8 D_800C48EA;      /* battle outcome */
extern s32 D_800C3E54;
extern s32 D_800D3284;
extern s32 D_800D328C;
extern u8 D_800D3298;      /* ATB enabled */
extern u8 D_800D2DCC[11];  /* slot present */
extern u8 D_800D2DE4[11];  /* slot ready to act */
extern s16 D_800D2E06[11]; /* turn timers */
extern u16 D_800D2E1C[11]; /* slow-status alternation */
extern s32 *D_8005917C;

/* Resident services. */
void func_800295D8(s32, s32, s32, s32);
void func_8003A89C(s32, s32, s32);

/* Battle overlay. */
void func_8008AB4C(void);
s32 func_8008ABB8(s32 size, s32 mode);
void func_8008AC50(void);
void func_800BE790(void);

/* The 801e5000 module and the 80280000 module. */
void func_801E5160(void);
s32 func_801E563C(void);
void func_801E879C(s32);
void func_8028022C(void);

#endif
