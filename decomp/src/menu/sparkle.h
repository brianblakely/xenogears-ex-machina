#ifndef MENU_SPARKLE_H
#define MENU_SPARKLE_H

#include "common.h"

/* libgpu POLY_FT4 layout (0x28 bytes). */
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

/* libgpu CVECTOR layout. */
typedef struct {
    u8 r, g, b, cd;
} Color;

/* Kind of sparkle (20-byte records at D_80092A74). */
typedef struct {
    u8 unk0[0xA];
    u8 frame_count; /* 0x0A */
    u8 unkB;
    u16 clut;       /* 0x0C */
    u16 tpage;      /* 0x0E */
    u16 gravity;    /* 0x10 */
    u8 unk12[2];
} SparkleKind;

/* A sprite particle of the menu scene: a quad per draw buffer. */
typedef struct {
    PolyFT4 prim[2];     /* 0x00 */
    u8 active;           /* 0x50 */
    s8 frame;            /* 0x51 */
    u8 frame_count;      /* 0x52 */
    u8 still;            /* 0x53: nonzero keeps its height */
    s16 x, y, z;         /* 0x54 */
    u8 unk5A[6];
    SparkleKind *kind;   /* 0x60 */
    u16 gravity;         /* 0x64 */
    u16 fall_speed;      /* 0x66 */
    u8 unk68[0x14];
} Sparkle;

#define SPARKLE_COUNT 60

extern Sparkle D_80092AD8[SPARKLE_COUNT];
extern SparkleKind D_80092A74[];
extern Color D_800926B8; /* colour of kind-2 sparkles */
extern s32 D_800926A4;   /* frame counter */
extern s32 D_800926B0;
extern s32 D_800926B4;

#endif
