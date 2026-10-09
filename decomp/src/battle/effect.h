#ifndef BATTLE_EFFECT_H
#define BATTLE_EFFECT_H

/* Keyframe tracks, sprite drawing, colour fades, image animations and
 * surfaces of the battle effect library (8009E53C's unit, 800A3490-800A4654
 * and 800A7064-800A8A88; the same code is linked into overlay 2143 at
 * +0x13D3C0/+0x13D684). */


#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/scene.h"

/* An animation frame header (0x18 bytes; read as halfwords by 800A1B50). */
typedef struct {
    u8 pad0[2];
    u16 duration;         /* 0x02 */
    u16 flags;            /* 0x04: bit 0 no rotations, bit 1 no translations */
    u16 packed;           /* 0x06: 0: the values follow a skipped base block */
    u8 pad8[4];
    u16 rotationCount;    /* 0x0C */
    u16 translationCount; /* 0x0E */
    u8 pad10[8];
} AnimationFrame; /* followed by one TrackEntry per part, or packed values */

/* A part's tracks of a frame: byte offsets of its rotation and translation
 * tracks (0xFFFF none) and their entry types. */
typedef struct {
    u16 offsets[2];
    u8 types[2];
} TrackEntry;

/* An effect entry seen as a tween of a part (the EffectEntry layout): start
 * values and deltas or targets, or a keyframe track cursor. */
typedef struct {
    u8 used;
    u8 field1;     /* +1: smooth / looping */
    u8 field2;     /* +2: type: 3 rotation, 7 + n movement, 0-2 tracks */
    u8 kind;       /* +3: 0xFF persistent */
    union {
        s16 values[6]; /* +4 */
        struct {
            u8 *start;  /* +4: track data */
            u8 *cursor; /* +8 */
        } track;
    } u;
    s16 time;     /* +0x10 */
    s16 duration; /* +0x12 */
} Tween;

/* An object's trail channel (0x70 bytes): a sprite following two points of a
 * model part, with fading colours. Screen-space trails retain eight projected
 * positions per endpoint; world-space trails retain two full vectors. */
typedef struct ColorFade {
    s16 field0; /* model part; negative when idle */
    u8 field2;  /* 0 screen-space history, otherwise world-space */
    u8 field3;  /* sprite semi-transparency */
    SpritePool *pool; /* 0x04 */
    EffectSprite *sprite; /* 0x08: the currently extended quad */
    s16 fieldC;
    s16 fieldE;
    s16 field10;
    u8 pad12[2];
    s16 field14;
    s16 field16;
    s16 field18;
    u8 pad1A[2];
    union {
        struct {
            DVECTOR first[8];
            DVECTOR second[8];
        } screen;
        struct {
            VECTOR first[2];
            VECTOR second[2];
        } world;
    } history; /* 0x1C-0x5B */
    s16 time;     /* 0x5C: history cursor, decremented modulo 8 */
    s16 field5E;  /* 0x5E: age of the current quad, -1 before the first tick */
    s16 field60;  /* 0x60: quad extension interval, at most 7 */
    s16 duration; /* 0x62 */
    s16 color[3]; /* 0x64: 10.6 fixed point */
    s16 step[3];  /* 0x6A */
} ColorFade;

typedef char ColorFadeLayoutCheck[sizeof(ColorFade) == 0x70 ? 1 : -1];

/* A frame curve mapping time to a frame (800A3490-800A35C8, called without
 * a prototype: time, divisor, base); negative ends the animation. */
typedef s32 (*FrameCurve)();

/* A row of three colours. */
typedef struct {
    u16 c[3];
} ColorRow;

/* An image animation (0x30 bytes): a VRAM rectangle whose pixels are rebuilt
 * each time the curve selects another frame. */
typedef struct ImageAnim {
    struct ImageAnim *target; /* 0x00: image the frames are copied into */
    u16 *pixels;              /* 0x04 */
    u16 *pixels2;             /* 0x08 */
    u16 *work;                /* 0x0C */
    u8 mode;                  /* 0x10: 0/1 resident decoders, 4/5 fades */
    u8 dirty;                 /* 0x11 */
    s16 size;                 /* 0x12: pixel count */
    u16 time;                 /* 0x14 */
    u16 speed;                /* 0x16 */
    u16 frame;                /* 0x18 */
    u16 active;               /* 0x1A */
    ColorRow *colors;         /* 0x1C */
    s16 divisor;              /* 0x20 */
    s16 base;                 /* 0x22 */
    FrameCurve curve;         /* 0x24 */
    RECT rect;                /* 0x28 */
} ImageAnim;

/* A collision sphere of a surface (0x10 bytes). */
typedef struct {
    s16 h0, h2, h4, h6, h8, hA, hC, hE;
} SurfaceEntry;

/* A point of a surface strand (0x18 bytes): the length of its segment to the
 * next point (0 ends the strand), a sag added to that segment, its position
 * and the normal accumulated from its triangles. */
typedef struct {
    s16 length;
    s16 sag;
    s16 pos[3];
    u16 normalCount; /* 0x0A */
    s32 normal[3];   /* 0x0C */
} SurfacePoint;

/* Two triangles' textured primitives (one per frame buffer) and their vertex
 * indices (0x58 bytes). */
typedef struct {
    s16 index[3];
    u8 pad6[2];
    POLY_GT3 prim[2];
} SurfacePoly;

/* A battle object's surface (0x24 bytes; hair or cloth): rings of point
 * strands. */
typedef struct Surface {
    u16 h0;
    u8 pad2[2];
    s16 rings;              /* 0x04 */
    s16 polys;              /* 0x06: twice the rings' first point counts */
    s16 points;             /* 0x08 */
    s16 entryCount;         /* 0x0A */
    u8 b[6];                /* 0x0C */
    u8 pad12[2];
    SVECTOR *centres;       /* 0x14: a centre per ring */
    SurfaceEntry *entries;  /* 0x18 */
    SurfacePoint **strands; /* 0x1C: each ring's first point */
    SurfacePoly *polyList;  /* 0x20 */
} Surface;

extern ImageAnim D_800D3600;  /* the stage's image animation */

#endif
