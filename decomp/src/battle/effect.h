#ifndef BATTLE_EFFECT_H
#define BATTLE_EFFECT_H

/* Keyframe tracks, sprite drawing, colour fades and image animations of the
 * battle effect library (8009F1C4-800A44C0; the same code is linked into
 * overlay 2143 at +0x13D3C0/+0x13D684). */

#include "common.h"
#include "model.h"
#include "scene.h"

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

/* An effect entry playing a keyframe track (the EffectEntry layout with the
 * track cursor in place of its parameters). */
typedef struct {
    u8 used;
    u8 field1;
    u8 field2;
    u8 kind;       /* +3: 0xFF persistent */
    u8 *start;     /* +4: track data */
    u8 *cursor;    /* +8 */
    u8 pad0C[4];
    u16 field10;
    u16 field12;
} EffectTrack;

/* An effect sprite (the SpriteRecord of a sprite pool, 0x7C bytes): a
 * quadrilateral of four vertices, a colour fading each tick, and its
 * primitive for both frame buffers. */
typedef struct {
    s16 x0, y0, z0, pad06;
    s16 x1, y1, z1;
    s16 projected; /* 0x0E: the vertices are 3D, projected with the GTE */
    s16 x2, y2, z2;
    s16 age;       /* 0x16: -1 free */
    s16 x3, y3, z3;
    s16 lifetime;  /* 0x1E */
    u16 color[3];  /* 0x20: 10.6 fixed point */
    s16 fade[3];   /* 0x26: per tick */
    POLY_FT4 packets[2]; /* 0x2C */
} Sprite;

/* A colour fade record (fields as far as recovered). */
typedef struct {
    s16 field0;
    u8 field2;
    u8 field3;
    s32 field4;
    s32 field8;
    s16 fieldC;
    s16 fieldE;
    s16 field10;
    u8 pad12[2];
    s16 field14;
    s16 field16;
    s16 field18;
    u8 pad1A[0x42];
    s16 time;     /* 0x5C */
    s16 field5E;  /* 0x5E */
    s16 field60;  /* 0x60: at most 7 */
    s16 duration; /* 0x62 */
    s16 color[3]; /* 0x64: 10.6 fixed point */
    s16 step[3];  /* 0x6A */
} ColorFade;

extern s32 D_80050100;

s32 func_8003F8CC(s32 angle); /* cosine (4096 = 1.0) */ /* ordering-table depth shift */

u16 func_800A1B50(ModelPart *root, s16 *data);
void func_800A2ACC(EffectPool *pool, ModelPart *part);

#endif
