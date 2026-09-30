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

/* A frame curve mapping time to a frame (800A3490-800A35C8, called without
 * a prototype: time, divisor, base); negative ends the animation. */
typedef s16 (*FrameCurve)();

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

/* Resident image decoders. */
void func_80026F44(s32 size, s32 frame, u16 *out, u16 *pixels);
void func_80026FE8(s32 size, s32 frame, u16 *out, u16 *pixels2, u16 *pixels);

s16 func_800A3E98(ImageAnim *anim, s32 ticks);
void func_800A429C(ImageAnim *anim);
void func_800A4348(ImageAnim *anim, s16 level);
void func_800A43F8(ImageAnim *anim, s16 level);

extern s32 D_80050100;

s32 func_8003F8CC(s32 angle); /* cosine (4096 = 1.0) */ /* ordering-table depth shift */

u16 func_800A1B50(ModelPart *root, s16 *data);
void func_800A2ACC(EffectPool *pool, ModelPart *part);

#endif
