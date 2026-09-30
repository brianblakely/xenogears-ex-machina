#ifndef MENU_H
#define MENU_H

#include "common.h"

/* libgte-layout vector: three 32-bit components and padding. */
typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} Vector;

/* libgpu types (PsyQ). */
typedef struct {
    s16 x, y;
    s16 w, h;
} Rect;

typedef struct {
    u32 mode;
    Rect *crect;
    u32 *caddr;
    Rect *prect;
    u32 *paddr;
} TimImage;

typedef struct {
    u32 tag;
    u32 code[1];
} DrTpage;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
} Sprt16;

/* One of the two display buffers: draw and display environments, then
 * packets linked every frame. */
typedef struct {
    u8 envs[0x74];
    u32 offset_prim[3]; /* 0x74: last word is x | y << 16 */
} MenuFrame;

/* A character moved in the menu scene. */
typedef struct {
    Vector pos;          /* 0x00 */
    u8 unk10[0x38];
    s32 state;           /* 0x48 */
    u8 unk4C[0x8];
    s32 angle;           /* 0x54: facing, 4096 = full turn */
    s32 target_angle;    /* 0x58 */
    u8 unk5C[0x58];
    u16 unkB4;
    u8 unkB6[0x6];
    u16 unkBC;
    u8 unkBE[0x10];
    s16 unkCE;
    s32 flags;           /* 0xD0 */
} Actor;

/* Menu camera: eye position (D_8009867C) and look-at point (D_8009871C). */
extern Vector D_8009867C;
extern Vector D_8009871C;
extern s32 D_800925F4; /* vertical camera lift of the current view */
extern Actor D_80097010;
extern Actor D_8009872C;
extern Vector D_80099078;
extern u8 D_80092954[];

/* PsyQ SDK (resident). */
void func_80043B48(void *ot, void *prim);                   /* AddPrim */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);           /* GetTPage */
u16 func_80043A58(s32 x, s32 y);                            /* GetClut */
void func_80043E20(DrTpage *p, s32 dfe, s32 dtd, s32 tpage); /* SetDrawTPage */
void func_80044894(Rect *rect, u32 *data);                  /* LoadImage */
void func_800471B4(u32 *tim);                               /* OpenTIM */
TimImage *func_800471C4(TimImage *image);                   /* ReadTIM */

s32 func_8003F8B0(s32 angle); /* sine, 4096 = 1.0 */
s32 func_8003F8CC(s32 angle); /* cosine, 4096 = 1.0 */
void func_800346D4(void *arg);
void func_80083C0C(s32 arg);
void func_80083738(Actor *actor, Actor *other);
void func_800828F8(Vector *position, Vector *step, s32 limit);
s32 func_80082488(Vector *position, s32 arg);
s32 func_8004B32C(s32 x, s32 z); /* angle of a direction, 4096 = full turn */
void func_8007E24C(void);

extern s32 D_800925F8;
extern s32 D_800925FC;
extern s32 D_80092934;
extern s32 D_800925D4;
extern s32 D_800925D8;
extern s32 D_800925E0; /* screen offset x, y */
extern s32 D_800925E4;
extern s32 D_800925E8;
extern s32 D_800925EC;
extern s16 D_80092600;
extern u8 D_80092608;
extern MenuFrame *D_80092868; /* frame being built */
extern u8 D_800928A0;         /* index of the frame being built */
extern DrTpage D_800929E4[2];
extern s32 D_8009105C[];
extern Sprt16 D_8009A14C;
extern Sprt16 D_8009A244;

/* Menu window (resident window code at 80032f54). */
typedef struct {
    s16 unk0[3];
    s16 unk6;
    s16 unk8[2];
    s16 unkC;
} MenuWindow;

extern MenuWindow D_8009868C;

void func_80032F54(MenuWindow *window, s32 x, s32 y, s32 w, s32 h, s32 a5, s32 a6);
void func_8008EB4C(s32 id);
void func_80070F80(s32 arg);

#endif
