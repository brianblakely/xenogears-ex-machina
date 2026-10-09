#ifndef MENU_SPARK_H
#define MENU_SPARK_H

#include "menu.h"
#include "sparkle.h"
#include "scene.h"

/* A model's part list, as far as the menu reads it. */
typedef struct {
    u8 unk0[0x4C];
    MATRIX matrix; /* 0x4C: the part's local transform */
} ModelPart;

typedef struct {
    u32 unk0;
    ModelPart **parts;
} ModelPartList;

typedef struct {
    u32 unk0;
    ModelPartList *list;
} SparkModel;

/* The common head of every spark record. */
typedef struct {
    SVECTOR pos; /* pos.pad: frames left to live, 0 = idle */
    SVECTOR vel;
} Spark;

/* A spark emitter: a pool of sparks of one shape, their placement rule and
 * colour. Spark records start with their position; pos.pad is cleared when
 * a spark is (re)started. */
typedef struct Emitter Emitter;

typedef struct {
    void (*setup)(void *spark, Emitter *emitter);
    void (*reset)(void *spark);
    void (*draw)(void *spark, u32 *ot);
    s32 size;
} SparkShape;

struct Emitter {
    s16 gravity;            /* 0x00: added to vel.vy every frame */
    s16 unk2;
    s16 unk4;
    s16 unk6;
    s16 unk8;
    s16 count;              /* 0x0A: sparks in the pool */
    s16 size;               /* 0x0C: bytes per spark */
    s16 unkE;
    u8 *sparks;             /* 0x10 */
    SVECTOR base;           /* 0x14: added to the rotation's translation */
    SVECTOR origin;         /* 0x1C: where sparks are emitted */
    SVECTOR range;          /* 0x24: random spread per axis */
    SVECTOR offset;         /* 0x2C: subtracted from the random spread */
    SVECTOR angles;         /* 0x34: launch direction, applied first */
    SVECTOR turn;           /* 0x3C: rotation applied after the caller's */
    s16 spread;             /* 0x44: random launch angle range */
    s16 unk46;
    s16 speed;              /* 0x48: minimum launch speed */
    s16 speed_range;        /* 0x4A: random extra launch speed */
    s16 unk4C;
    s16 placement;          /* 0x4E: index into the placement rules */
    void (*reset)(void *spark);          /* 0x50 */
    void (*draw)(void *spark, u32 *ot);  /* 0x54 */
    void (*setup)(void *spark, Emitter *emitter); /* 0x58 */
    void (*place)(Emitter *emitter, SVECTOR *pos); /* 0x5C */
    void (*update)(Spark *spark);        /* 0x60 */
    s16 unk64;
    s16 shape;              /* 0x66: index into the spark shapes */
    s16 unk68;
    s16 life;               /* 0x6A: frames a launched spark lives */
    u8 unk6C[0x8];
    u8 r;                   /* 0x74 */
    u8 g;
    u8 b;
    u8 unk77[0x5];
};

extern SparkShape D_80091C74[];
extern void (*D_80091CC4[])(Emitter *emitter, SVECTOR *pos);
extern void (*D_80091CDC[1])(Spark *spark);

/* Sparks drawn as a line through their last positions, with one primitive
 * per draw buffer. */
typedef struct {
    SVECTOR pos;
    SVECTOR vel;
    SVECTOR trail[3];
    LINE_F4 line[2];
} SparkLine4;

typedef struct {
    SVECTOR pos;
    SVECTOR vel;
    SVECTOR trail[2];
    LINE_F3 line[2];
} SparkLine3;

typedef struct {
    SVECTOR pos;
    SVECTOR vel;
    SVECTOR trail[1];
    LINE_F2 line[2];
} SparkLine2;

typedef struct {
    SVECTOR pos;
    SVECTOR vel;
    TILE tile[2];
} SparkTile;

typedef struct {
    SVECTOR pos;
    SVECTOR vel;
    TILE_1 dot[2];
} SparkDot;

extern u16 D_80091CE0[]; /* glow palette (256 entries) */

void func_800324B8(s32 tag);                 /* heap allocation tag */
void *func_80031BDC(s32 size, s32 arg);      /* heap allocation */
void func_80032C18(void *block, s32 arg);    /* heap release */
void func_800316C0(u32 *ot, LINE_F2 *prim);
void func_80031708(u32 *ot, LINE_F3 *prim);
void func_80031750(u32 *ot, LINE_F4 *prim);
void func_80031804(u32 *ot, TILE *prim);
void func_80031870(u32 *ot, TILE_1 *prim);
Emitter *func_8008D3F4(s32 shape, s32 placement);
void func_8008D580(Emitter *emitter);
void func_8008D5C0(Emitter *emitter, s32 count);
void func_8008D680(Emitter *emitter, MATRIX *rotation, s32 count);
void func_8008DA48(Emitter *emitter, u32 *ot, MATRIX *view);
void func_8008DBC0(SparkModel *model, s16 part, MATRIX *out);
/* Rotate with the loaded GTE matrix, then scale through IR0 with GPF12.
 * Only out->vx/vy/vz are written; out->pad is preserved. */
void func_8008DDFC(SVECTOR *vector, SVECTOR *out, s32 scale);

#endif
