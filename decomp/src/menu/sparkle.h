#ifndef MENU_SPARKLE_H
#define MENU_SPARKLE_H

#include "common.h"


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
        struct {
            u8 unk60[0xC];
            struct Sparkle *prev; /* 0x6C */
            u16 stamp;            /* 0x70: frame counter when started */
            s16 owner;            /* 0x72 */
            s32 size;             /* 0x74 */
            s32 key;              /* 0x78 */
        } trail;
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
extern SparkleKind D_80092A74[];
extern Color D_800926B8; /* colour of kind-2 sparkles */
extern s32 D_800926A4;   /* frame counter */
extern s32 D_800926B0; /* scene lines added this frame */
extern s32 D_800926B4;


/* Palette of the menu's two-colour sprites and where it is loaded. */
extern u16 D_800926A8[4];
extern s16 D_80092698;
extern s16 D_8009269C;
extern u16 D_800926A0; /* its CLUT id */
u16 func_800438C0(u16 *clut, s32 x, s32 y); /* load a CLUT, return its id */

/* Texture of the trail and line sprites. */
extern u16 D_80092694;   /* texture page */
extern s32 D_800928E8;   /* owner of the segments started now */
extern s16 D_80091228[]; /* trail sizes */

#endif
