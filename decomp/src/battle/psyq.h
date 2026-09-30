#ifndef BATTLE_PSYQ_H
#define BATTLE_PSYQ_H

#include "common.h"

/* The PsyQ library types and functions the battle overlay uses (the
 * libraries are linked into the resident executable). */

/* GPU primitives (libgpu). */
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
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
} POLY_G3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LINE_F2;

/* GTE rotation/translation matrix (PsyQ MATRIX). */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} Matrix;

/* PsyQ SVECTOR. */
typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVector;

/* PsyQ VECTOR. */
typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} Vector;

/* PsyQ CVECTOR. */
typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 cd;
} CVector;

/* libc. */
s32 rand(void); /* 0..0x7FFF */
void *memcpy(void *to, const void *from, u32 size);
void bzero(void *block, s32 size);

/* libgpu. */
void AddPrim(u32 *ot, void *prim);
void SetShadeTex(void *prim, s32 tge);
void SetSemiTrans(void *prim, s32 abe);
void SetPolyFT4(POLY_FT4 *prim);
void SetPolyG4(POLY_G4 *prim);
void SetDrawMode(DR_MODE *p, s32 dfe, s32 dtd, s32 tpage, RECT *tw);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
u16 GetClut(s32 x, s32 y);
void DrawSync(s32 mode);
void LoadImage(RECT *rect, u32 *pixels);
void StoreImage(RECT *rect, u32 *pixels);
/* A primitive's tag: the next primitive's address and the word count. */
typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
} P_TAG;
#define setaddr(p, _addr) (((P_TAG *)(p))->addr = (u32)(_addr))
#define getaddr(p) (u32)(((P_TAG *)(p))->addr)
#define addPrim(ot, p) setaddr(p, getaddr(ot)), setaddr(ot, p)

/* libgte. */
void func_8003F738(SVector *angles, Matrix *m);          /* RotMatrix */
void func_8004A92C(SVector *angles, Matrix *m);          /* RotMatrixYXZ */
void MulMatrix0(Matrix *m0, Matrix *m1, Matrix *out);
void CompMatrix(Matrix *m0, Matrix *m1, Matrix *out);
void SetRotMatrix(Matrix *m);
void func_80049BDC(Matrix *m0, Matrix *m1);              /* multiply m1 by m0 */
void OuterProduct0(Vector *v0, Vector *v1, Vector *out);
void SetLightMatrix(Matrix *m);
void SetTransMatrix(Matrix *m);
s32 SquareRoot0(s32 value);
s32 VectorNormalS(Vector *v, SVector *out);
void SetGeomScreen(s32 h);
s32 ratan2(s32 y, s32 x);
s32 func_80048D7C(Vector *v, Vector *out);                /* VectorNormal */
void func_8004A480(Vector *a, Vector *b, Vector *out);   /* OuterProduct12 */

#endif
