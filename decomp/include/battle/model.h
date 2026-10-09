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
    u8 field1;
    u8 field2;
    u8 kind;       /* +3: 0xFF persistent */
    u16 params[6]; /* +4 */
    u16 field10;
    u16 field12;
} EffectEntry;

/* A pool of effect entries; next is the first entry that may be free. */
typedef struct EffectPool {
    EffectEntry *entries;
    u16 next;
    u16 count;
} EffectPool;

/* A posed part of a model hierarchy (0x7C bytes); a hierarchy is a root part
 * followed by one part per (model, parent) pair. */
typedef struct ModelPart {
    struct ModelPart *parent; /* 0x00 */
    u8 flag4;
    u8 flag5;
    u8 flag6;
    u8 flag7;
    u16 modelId;      /* 0x08: 0xFFFF none */
    u16 index;        /* 0x0A: the root holds the part count */
    MATRIX transform; /* 0x0C: rotation and translation (the root's scaled) */
    MATRIX world;     /* 0x2C: composed with the parents' */
    s16 scale[3];     /* 0x4C: 4.12 */
    u16 field52;
    SVECTOR rotation;        /* 0x54: angles */
    s32 translation[3];      /* 0x5C */
    void *packets[2];        /* 0x68: one buffer per frame */
    EffectEntry *effects[3]; /* 0x70: attached effects */
} ModelPart;

u16 func_8009EF3C(ModelPart *part, s32 scale); /* pose a model hierarchy */

#endif
