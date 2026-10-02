#ifndef MENU_SPARKLE_H
#define MENU_SPARKLE_H

#include "common.h"


/* Kind of sparkle (20-byte records at D_80092A74). */
typedef struct {
    u8 *u;          /* 0x00: texture column of each frame */
    u8 *v;          /* 0x04: texture row of each frame */
    u8 w, h;        /* 0x08: frame size */
    u8 frame_count; /* 0x0A */
    u8 unkB;
    u16 clut;       /* 0x0C */
    u16 tpage;      /* 0x0E */
    s16 gravity;    /* 0x10 */
    u8 unk12[2];
} SparkleKind;

/* Trail state of a type-1 sparkle (at 0x60). */
typedef struct {
    s16 screen[6];        /* 0x60: projected leading edge (x0, y0, x1, y1)
                           * and centre (x, y) */
    struct Sparkle *prev; /* 0x6C */
    u16 stamp;            /* 0x70: frame counter when started */
    s16 owner;            /* 0x72 */
    s32 size;             /* 0x74 */
    s32 key;              /* 0x78 */
} SparkleTrail;

/* A sprite particle of the menu scene: a quad per draw buffer. Type 0
 * falls; types 1 and 2 are trail and line segments linked to the previous
 * frame's segment of the same key and owner. */
typedef struct Sparkle {
    PolyFT4 prim[2];     /* 0x00 */
    u8 active;           /* 0x50 */
    s8 frame;            /* 0x51 */
    u8 frame_count;      /* 0x52 */
    u8 type;             /* 0x53 */
    s16 x, y, z;         /* 0x54 */
    u8 unk5A[6];
    union {
        struct {
            SparkleKind *kind; /* 0x60 */
            u16 gravity;       /* 0x64 */
            u16 fall_speed;    /* 0x66 */
        } fall;
        SparkleTrail trail;
        struct {
            struct Sparkle *prev; /* 0x60 */
            s16 x, y, z;          /* 0x64: the other end */
            u16 stamp;            /* 0x6A */
            s16 owner;            /* 0x6C */
            u8 unk6E[2];
            s32 key;              /* 0x70 */
        } line;
    } u;
} Sparkle;

#define SPARKLE_COUNT 60

extern Sparkle D_80092AD8[SPARKLE_COUNT];
extern SparkleKind D_80092A74[5];

/* Frame tables of the sparkle kinds: kind 0's are filled from its twelve
 * TIMs; kinds 1-4 use fixed tables whose rows are offset once by their
 * TIM's row. */
extern u8 D_800947E8[12];
extern u8 D_800947F4[12];
extern u16 D_80094800[12]; /* kind 0's CLUT of each frame */
extern u8 D_800911D8[16];
extern u8 D_800911E8[16];
extern u8 D_800911F8[16];
extern u8 D_80091208[12];
extern u8 D_80091218[16];

/* Other effect textures (position, CLUT, texture page). */
extern u16 D_80092678, D_8009267C;
extern s16 D_80092680, D_80092684;
extern s16 D_80092688, D_8009268C, D_80092690;
extern Color D_800926B8; /* colour of kind-2 sparkles */
extern s32 D_800926A4;   /* frame counter */
extern s32 D_800926B0; /* scene lines added this frame */
extern s32 D_800926B4;


/* Palette of the menu's two-colour sprites and where it is loaded. */
extern u16 D_800926A8[4];
extern s16 D_80092698;
extern s16 D_8009269C;
extern u16 D_800926A0; /* its CLUT id */
u16 LoadClut2(u16 *clut, s32 x, s32 y); /* load a CLUT, return its id */

/* Texture of the trail and line sprites. */
extern u16 D_80092694;   /* texture page */
extern s32 D_800928E8;   /* owner of the segments started now */
extern s16 D_80091228[]; /* trail sizes */

#endif
