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

u16 func_800A1B50(ModelPart *root, s16 *data);
void func_800A2ACC(EffectPool *pool, ModelPart *part);

#endif
