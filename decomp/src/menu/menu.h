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
    u8 unk5C[0x54];
    s32 floor_y;         /* 0xB0 */
    s16 hp;              /* 0xB4 */
    u8 unkB6[0x6];
    s16 max_hp;          /* 0xBC */
    u8 unkBE[0x10];
    s16 unkCE;
    s32 flags;           /* 0xD0 */
    u8 unkD4[0x14];
    s32 unkE8;
} Actor;

/* Menu window (resident window code at 80032f54). */
typedef struct {
    s16 unk0[3];
    s16 unk6;
    s16 unk8[2];
    s16 unkC;
    u8 unkE[0x5A];
    u8 unk68;
} MenuWindow;

/* A step in one of eight directions on the floor plane. */
typedef struct {
    s32 x;
    s32 z;
} FloorStep;

/* Scene actors and camera: eye position (D_8009867C) and look-at point
 * (D_8009871C). */
extern Actor D_80097010;
extern Actor D_8009872C;
extern Vector D_8009867C;
extern Vector D_8009871C;
extern Vector D_80099078;
extern s32 D_800925F4; /* vertical camera lift of the current view */

extern MenuWindow D_8009868C; /* message window */
extern MenuWindow D_80092954;

extern u8 *D_800925F8;   /* running scene script */
extern u8 *D_8009105C[]; /* scene scripts */
extern u8 D_80090F38[];
extern u8 D_800910C4[];

extern s32 D_800925D4;
extern s32 D_800925D8;
extern s32 D_800925DC;
extern s32 D_800925E0; /* screen offset x, y */
extern s32 D_800925E4;
extern s32 D_800925E8;
extern s32 D_800925EC;
extern s32 D_800925FC;
extern s16 D_80092600;
extern s8 D_80092604; /* scene choice cursor */
extern u8 D_80092608;
extern s32 D_8009284C;
extern MenuFrame *D_80092868; /* frame being built */
extern s32 D_80092880;
extern u8 D_80092884;
extern u8 D_800928A0; /* index of the frame being built */
extern s32 D_800928C8;
extern u8 D_800928D4;
extern s32 D_80092900;
extern s32 D_80092904;
extern s32 D_80092934;
extern u8 D_8009293C;
extern s32 D_80092948;
extern u8 D_800929BC;
extern u8 D_800925F0;
extern s32 D_800928E8; /* frame counter */
extern u32 *D_80092938; /* ordering table being built */
extern FloorStep D_80091084[8];
extern u16 D_8005948C; /* pad buttons newly pressed */
extern u16 D_800594A4; /* pad buttons repeating */
extern DrTpage D_800929E4[2];
extern s32 D_80092A00;
extern s32 D_80092A10;
extern s32 D_80092A20;
extern u8 D_80099D9D;
extern u8 D_80099D9E;
extern Sprt16 D_8009A14C;
extern Sprt16 D_8009A244;

/* PsyQ SDK (resident). */
void func_80043B48(void *ot, void *prim);                   /* AddPrim */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);           /* GetTPage */
u16 func_80043A58(s32 x, s32 y);                            /* GetClut */
void func_80043E20(DrTpage *p, s32 dfe, s32 dtd, s32 tpage); /* SetDrawTPage */
void func_80044894(Rect *rect, u32 *data);                  /* LoadImage */
void func_800471B4(u32 *tim);                               /* OpenTIM */
TimImage *func_800471C4(TimImage *image);                   /* ReadTIM */
s32 func_8004B32C(s32 x, s32 z);                            /* ratan2 */

/* Resident game code. */
void func_80032F54(MenuWindow *window, s32 x, s32 y, s32 w, s32 h, s32 a5, s32 a6);
s32 func_80033728(s32 table, s32 index);
void func_800346D4(MenuWindow *window);
void func_80034714(MenuWindow *window, s32 text);
void func_80039C4C(s32 arg);
void func_80039FF8(void);
void func_800346A4(MenuWindow *window);
void func_80034800(MenuWindow *window, s32 colour, s32 a2, s32 a3);
void func_80034874(MenuWindow *window, s32 cursor);
void func_80034888(MenuWindow *window, u32 *ot, s32 frame);
void func_80036420(void);
s32 func_8003F8B0(s32 angle); /* sine, 4096 = 1.0 */
s32 func_8003F8CC(s32 angle); /* cosine, 4096 = 1.0 */

/* This overlay. */
s32 func_800707D8(s32 target, s32 current, s32 steps);
void func_8007099C(u32 mode);
void func_80070F80(u8 *script);
void func_8007107C(void);
void func_80071724(u32 *ot);
void func_80079DF0(Actor *actor, Actor *other);
u32 func_800828C4(Actor *actor);
void func_8007191C(s32 scene);
void func_80071DA4(Actor *actor);
void func_8007E24C(void);
s32 func_80082488(Vector *position, s32 arg);
void func_800828F8(Vector *position, Vector *step, s32 limit);
void func_80083738(Actor *actor, Actor *other);
void func_80083C0C(s32 arg);
void func_8008EB4C(s32 id);

#endif
