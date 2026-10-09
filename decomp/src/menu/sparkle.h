#ifndef MENU_SPARKLE_H
#define MENU_SPARKLE_H

#include "common.h"
#include "psyq/libgpu.h"

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
    POLY_FT4 prim[2];     /* 0x00 */
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

/* Frame tables of the sparkle kinds: kind 0's are filled from its twelve
 * TIMs (menu3's D_800947E8, D_800947F4 and D_80094800); kinds 1-4 use
 * fixed tables whose rows are offset once by their TIM's row. */
extern u8 D_800911D8[16];
extern u8 D_800911E8[16];
extern u8 D_800911F8[16];
extern u8 D_80091208[16]; /* 12 used */
extern u8 D_80091218[16];

/* Texture of the trail and line sprites. */
extern s32 D_800928E8;   /* owner of the segments started now */
extern s16 D_80091228[]; /* trail sizes */

#endif
