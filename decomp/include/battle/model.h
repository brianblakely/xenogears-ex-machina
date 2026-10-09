#ifndef BATTLE_MODEL_H
#define BATTLE_MODEL_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"

/* The battle objects' models: their model tables, hierarchies of posed parts
 * and the effect pools that animate the parts (8009E53C's unit, 8009EF3C-
 * 8009F794 and 800A0838-800A2BB8), and the model files they are built from. */

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

/* A model file's description of its object (800A8BF0 reads a stage object's,
 * ovl2143's 801E742C an actor's): its extents and scale, flags and record
 * counts, then its meshes' descriptions. */
typedef struct {
    u8 pad0[2];
    s16 size[3];       /* 0x02: height, x and z extents (the draws 8009F844 and
                        * 801DCEC8 place the shadow at the x and z extents;
                        * 800AA600/800AA650 and 801E8430/801E8480 scale them) */
    s16 scale;         /* 0x08 */
    u8 field2A;        /* 0x0A: the object's byte 0x2A, whose low 7 bits 800AF518
                        * and 801E6910 take as the animation of ids 0xFE/0xFF
                        * and bit 7 as a flag */
    u8 padB;
    u16 flags;         /* 0x0C: the object's flags4A */
    u8 channelCount;   /* 0x0E */
    u8 padF;
    u8 imageAnimCount; /* 0x10 */
    u8 pad11;
    u8 meshCount;      /* 0x12: its surfaces (800A7064) */
    u8 pad13;
    s16 meshes[1];     /* 0x14: per surface its h0, 16 parameter words for
                        * 800A7064, its key count and its keys (five halfwords
                        * each) */
} ObjectDesc;

/* The header of a model file. */
typedef struct {
    u8 pad0[4];
    ObjectDesc *desc;  /* 0x04 */
    void *meshData[1]; /* 0x08: per mesh, its surface's table (800A7064) */
} ObjectHeader;

/* A model file (relocated by 8003342C). */
typedef struct {
    u8 pad0[4];
    void *images;         /* 0x04 */
    u8 *models;           /* 0x08: the model group */
    u16 *hierarchy;       /* 0x0C: after the models: (model, parent) pairs
                           * (8009EC4C) */
    ObjectHeader *header; /* 0x10 */
} ObjectModelFile;

#endif
