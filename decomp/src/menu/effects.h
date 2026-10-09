#ifndef MENU_EFFECTS_H
#define MENU_EFFECTS_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The scene's particle effects (menu3 800732CC-80073424, 8007B270-8007E528):
 * the glow emitter, the sparkles (sprite quads that fall or trail), the
 * bolts, the ground particles, the scene cells thrown up along a segment,
 * and the queued 3D lines, drawn in the scene passes. */

/* Scratchpad work area of the scene drawing. */
typedef struct {
    VECTOR camera; /* 0x00: camera position of this frame */
    SVECTOR point; /* 0x10: particle position relative to the camera */
    SVECTOR from;  /* 0x18: line end points relative to the camera */
    SVECTOR to;    /* 0x20 */
    SVECTOR extra; /* 0x28: fourth corner of a projected quad */
    VECTOR corner[6]; /* 0x30: view-rotated sprite corner offsets */
    u8 unk90[0x20];
    s32 depth;     /* 0xB0: projected depth */
} SceneScratch;

#define SCENE_SCRATCH ((SceneScratch *)0x1F800000)

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

/* A scene cell (12 bytes; table at D_800926C8). */
typedef struct {
    s16 unk0, unk2, unk4; /* position */
    s16 unk6;             /* remaining life */
    s8 unk8;              /* x speed */
    s8 unk9;              /* z speed */
    s16 unkA;             /* vertical speed */
} SceneCell12;

/* A ground particle of the scene (10 bytes; table at D_800926BC). */
typedef struct {
    s16 unk0; /* x */
    s16 unk2; /* height */
    s16 unk4; /* z */
    s8 unk6;  /* remaining life */
    s8 unk7;  /* vertical speed */
    s16 unk8; /* ground height */
} SceneCell10;

/* A 3D line segment of the scene, projected into its LINE_F2 each frame. */
typedef struct {
    LINE_F2 line;
    SVECTOR from; /* 0x10 */
    SVECTOR to;   /* 0x18 */
} SceneLine;

/* Frame tables of the sparkle kinds: kind 0's are filled from its twelve
 * TIMs (menu3's D_800947E8, D_800947F4 and D_80094800); kinds 1-4 use
 * fixed tables whose rows are offset once by their TIM's row. */
extern u8 D_800911D8[16];
extern u8 D_800911E8[16];
extern u8 D_800911F8[16];
extern u8 D_80091208[16]; /* 12 used */
extern u8 D_80091218[16];
extern s16 D_80091228[];  /* trail sizes */
extern s32 D_800928E8;    /* owner of the segments started now */

void func_800732CC(void);
void func_8007334C(u32 *ot, MATRIX *view);
void func_8007B270(CVECTOR *first, CVECTOR *second);
void func_8007B388(u_long **files);
void func_8007BB7C(void);
void func_8007BBA0(MATRIX *view, MATRIX *local, u32 *ot);
void func_8007C100(CVECTOR *color);
void func_8007C280(MATRIX *view, MATRIX *local, u32 *ot);
void func_8007C880(s32 column, VECTOR *pos, s32 key, s32 size);
void func_8007CAA4(MATRIX *view, MATRIX *local, u32 *ot);
u32 func_8007CD14(s32 flag, s32 top, s32 middle, s32 low);
void func_8007CD44(s32 column, VECTOR *from, VECTOR *to, s32 key);
void func_8007CF78(MATRIX *view, u32 *ot);
void func_8007D068(void *arg);
s32 func_8007D190(VECTOR *pos, u32 kind);
s32 func_8007D25C(s32 type);
void func_8007D274(VECTOR *from, VECTOR *to);
void func_8007D334(VECTOR *from, VECTOR *to, s32 kind);
void func_8007D65C(VECTOR *from, VECTOR *to, s32 code);
void func_8007D7A8(VECTOR *pos, s32 count);
void func_8007D918(u32 *ot);
void func_8007E020(u32 *ot);
void func_8007E24C(void);
void func_8007E31C(VECTOR *from, VECTOR *to, CVECTOR *color);
void func_8007E3CC(u32 *ot);

#endif
