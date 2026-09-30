#ifndef OVL2606_SCENE_SELECT_H
#define OVL2606_SCENE_SELECT_H

#include "common.h"

/* libgpu (resident) */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
    u32 code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter;
    u8 isrgb24;
    u8 pad0, pad1;
} DISPENV;

extern void func_80044534(s32 mask);         /* SetDispMask */
extern void func_80044C44(DRAWENV *env);     /* PutDrawEnv */
extern void func_80044E9C(DISPENV *env);     /* PutDispEnv */

/* The battle's two frame buffers (resident BSS, 0x4070 bytes each): the
 * environments come first, then the frame's ordering table and packets. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u32 ot[0x1000];
} BattleFrame;

extern BattleFrame D_800C4A20[2];
extern BattleFrame *D_800CCB00; /* the frame being built */

/* Game data party order: three character ids. */
extern u8 D_8006F368[3];

/* Selector state (overlay data). The four rows are the scene number, the
 * three party slots, ... ; each row has three decimal digit columns. */
extern s32 D_801E1DA0[4][3];
extern u8 D_801E1DD0[4][3];

#endif
