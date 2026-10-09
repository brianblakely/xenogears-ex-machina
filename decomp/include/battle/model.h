#ifndef BATTLE_MODEL_H
#define BATTLE_MODEL_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"

/* The battle objects' models: their model tables, hierarchies of posed parts
 * and the effect pools that animate the parts (8009E53C's unit, 8009EF3C-
 * 8009F794 and 800A0838-800A2BB8). */

/* A table of the models of a relocated model group: the group's records
 * (resident SpriteModels, 0x38 bytes each from group +0x10) and their
 * count. */
typedef struct ModelTable {
    SpriteModel **models;
    u32 count;
} ModelTable;

/* An effect entry (0x14 bytes) of an effect pool. */
typedef struct {
    u8 used;
    u8 field1;     /* +1: smooth / looping (battle/effect.h's Tween view) */
    u8 kind;       /* +2: 3 rotation, 7 + n movement, 0-2 tracks */
    u8 tag;        /* +3: 0xFF persistent */
    u16 params[6]; /* +4 */
    u16 time;      /* +0x10 */
    u16 duration;  /* +0x12 */
} EffectEntry;

/* A pool of effect entries; next is the first entry that may be free. */
typedef struct EffectPool {
    EffectEntry *entries;
    u16 next;
    u16 count;
} EffectPool;

/* A posed part of a model hierarchy (0x7C bytes); a hierarchy is a root part
 * followed by one part per (model, parent) pair. The matrix updates 8009EF3C
 * and 8009F1C4 and the object draw 8009F844 read the flags (in ovl2143
 * 801DC5C0, 801DC848 and 801DCEC8). */
typedef struct ModelPart {
    struct ModelPart *parent; /* 0x00 */
    u8 dirty;         /* 0x04: the world matrix must be recomposed */
    u8 rotate;        /* 0x05: the transform must be rebuilt from the rotation */
    u8 yxz;           /* 0x06: rebuilt by RotMatrixYXZ (8004A92C), else RotMatrix */
    u8 visible;       /* 0x07: drawn */
    u16 modelId;      /* 0x08: 0xFFFF none */
    u16 index;        /* 0x0A: the root holds the part count */
    MATRIX transform; /* 0x0C: rotation and translation (the root's scaled) */
    MATRIX world;     /* 0x2C: composed with the parents' */
    s16 scale[3];     /* 0x4C: 4.12 */
    u16 field52;      /* 0x52: read signed, 1 an upright billboard and 2 one
                       * facing the view (8009F844); 4-7 draw modes 2-5
                       * (800A48EC, which needs it unsigned) */
    SVECTOR rotation;        /* 0x54: angles */
    s32 translation[3];      /* 0x5C */
    void *packets[2];        /* 0x68: one buffer per frame */
    EffectEntry *effects[3]; /* 0x70: attached effects */
} ModelPart;

#endif
