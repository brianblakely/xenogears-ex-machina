#ifndef OVL2606_SCENE_SELECT_H
#define OVL2606_SCENE_SELECT_H

#include "common.h"
#include "resident/formation.h"

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

extern void DrawSync(s32 mode);         /* DrawSync */
extern void ClearOTagR(u32 *ot, s32 n);   /* ClearOTagR */
extern void DrawOTag(u32 *ot);          /* DrawOTag */
extern void VSync(s32 mode);         /* VSync */
extern void SetDispMask(s32 mask);         /* SetDispMask */
extern void PutDrawEnv(DRAWENV *env);     /* PutDrawEnv */
extern void PutDispEnv(DISPENV *env);     /* PutDispEnv */

/* The battle's two frame buffers (resident BSS, 0x4070 bytes each): the
 * environments come first, then the frame's ordering table and packets. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u32 ot[0x1000];
} BattleFrame;

extern BattleFrame D_800C4A20[2];
extern BattleFrame *D_800CCB00; /* the frame being built */

/* Persistent character record (game data, 0xa4 bytes). */
typedef struct {
    u8 unk0[0x4C];
    u16 hp;
    u16 maxHp;
    u16 ep;
    u16 maxEp;
    u8 unk54[0x4C];
    u8 gear; /* gear id, ff none */
    u8 unkA1[3];
} CharacterRecord;

extern CharacterRecord D_8006D8A0[11];

/* Game data party state: members who have joined, the three party character
 * ids (ff none) and whether each party slot starts in its gear. */
typedef struct {
    u16 joined;
    u16 available;
    u8 party[3];
    u8 unk7[0x57A];
    u8 inGear[3];
} PartyState;

extern PartyState D_8006F364;

extern u8 D_8005947C;
extern u8 D_8005954C;  /* battle mode */
extern u8 D_80062648[]; /* event data */
extern void *D_800D39D8;

extern void func_8001B66C(void);                /* stop the music */
extern void func_80028470(s32, s32);
extern s32 func_800288EC(s32 file);             /* file size in bytes */
extern void func_800295D8(s32 file, void *dst, s32 offset, s32 mode); /* read a file */
extern void func_80028A60(s32);                 /* wait for the disc */
extern void func_80032498(s32 tag, s32);        /* select the heap owner tag */
extern void func_800320E8(void *block);         /* free */
extern void func_8003748C(void);
extern void *memmove(void *dst, void *src, s32 n); /* memmove */
extern void func_8008AB70(void);
extern void *func_8008ABB8(s32 size, s32 mode); /* battle allocation */
extern void func_8009B1E4(void);

/* Resident debug text: print into the text stream, then draw the stream
 * into an ordering table. */
extern void func_8003700C(const char *fmt, ...);
extern void func_80037324(u32 *ot);

/* Battle menu input decode (battle overlay) and its decoded command code:
 * 0-3 right/down/left/up, 4 Circle, 5 Cross, 7 Triangle, 13, 14 Start. */
extern void func_80089CCC(s32);
extern u8 D_800D3014;

/* Row labels ("SceneNo ", "Party   ", "Robo    ", "FileNo  ") and the
 * character names (the twelfth is empty) the screen prints. */
extern char *D_801E1D40[4];
extern char *D_801E1D50[12];

/* Per-character gear for the gear columns 1 and 2. */
extern u8 D_801E1D80[16];
extern u8 D_801E1D90[16];

/* Selector values (overlay data): row 0 the scene (enemy set, 0-15), row 1
 * the three party characters (0-11, ff none), row 2 their gear mode (Off,
 * Nml, Bar), row 3 the file number (253-255 the three event files). The
 * second table marks the columns shown on each row. */
extern s32 D_801E1DA0[4][3];
extern u8 D_801E1DD0[4][3];

#endif
