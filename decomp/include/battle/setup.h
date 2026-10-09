#ifndef BATTLE_SETUP_H
#define BATTLE_SETUP_H

#include "common.h"

/* The battle's set-up and end (80070e2c's unit from 80070f40): the scene
 * settings, the battle's outcome and exit, the party's battle masks, the
 * music, the modules it runs (801de000, 801e0000, the 80280000 debugger) and
 * the battle heap and disc helpers (battle.c 8008ab4c-8008ac50). */

extern u8 D_800C3D44;
extern u8 D_800C3D5C;
extern s32 D_800C3DEC;
extern s32 D_800C3E54;     /* the battle music's sequence */
extern u8 D_800C3E4C;      /* battle end state */
extern u8 D_800C48EA;      /* battle outcome (the battle area's outcome) */
extern u8 D_800C492A;      /* keep the battle's resources at its end (the battle area's +0xa7a) */
extern s32 D_800D2D3C;     /* 801de000 module blocks */
extern s32 D_800D2F60;
extern u8 D_800D2D50;
extern u8 D_800D2FC4;      /* battle exit requested */

/* Party members' battle masks (from the character battle data). */
typedef struct MemberMasks {
    u16 mask0;
    u16 mask2;
} MemberMasks;

extern MemberMasks D_800C3E0C[3];

void func_80076544(void);
/* The battle heap and the disc. */
void func_8008AB4C(void);
void func_8008AB94(void);
s32 func_8008ABB8(s32 size, s32 mode); /* allocate a battle heap block */
void func_8008AC50(void);              /* wait until the disc reads finish */
void func_8009892C(void); /* the party's adjustments at battle start */

#endif
