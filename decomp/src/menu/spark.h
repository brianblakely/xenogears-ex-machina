#ifndef MENU_SPARK_H
#define MENU_SPARK_H

#include "menu.h"

/* libgpu primitive layouts used by the menu's small screen effects. */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
    u8 r0;
    u8 g0;
    u8 b0;
    u8 code;
} PrimTag;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LineF2;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    u32 pad;
} LineF3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
    u32 pad;
} LineF4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} Tile;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
} Tile1;

#define setlen(p, _len) (((PrimTag *)(p))->len = (u8)(_len))
#define setcode(p, _code) (((PrimTag *)(p))->code = (u8)(_code))
#define getcode(p) (u8)(((PrimTag *)(p))->code)
#define setSemiTrans(p, abe) \
    ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))
#define setRGB0(p, _r0, _g0, _b0) ((p)->r0 = _r0, (p)->g0 = _g0, (p)->b0 = _b0)

/* PsyQ inline_c.h GTE macros. */
#define gte_ldv0(r0) \
    __asm__ volatile("lwc2 $0, 0(%0);" \
                     "lwc2 $1, 4(%0)" \
                     : \
                     : "r"(r0))
#define gte_ldv3c(r0) \
    __asm__ volatile("lwc2 $0, 0(%0);" \
                     "lwc2 $1, 4(%0);" \
                     "lwc2 $2, 8(%0);" \
                     "lwc2 $3, 12(%0);" \
                     "lwc2 $4, 16(%0);" \
                     "lwc2 $5, 20(%0)" \
                     : \
                     : "r"(r0))
#define gte_rtps() \
    __asm__ volatile("nop;" \
                     "nop;" \
                     ".word 0x4A180001")
#define gte_rtpt() \
    __asm__ volatile("nop;" \
                     "nop;" \
                     ".word 0x4A280030")
#define gte_stsxy(r0) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsz(r0) __asm__ volatile("swc2 $19, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsxy3(r0, r1, r2) \
    __asm__ volatile("swc2 $12, 0(%0);" \
                     "swc2 $13, 0(%1);" \
                     "swc2 $14, 0(%2)" \
                     : \
                     : "r"(r0), "r"(r1), "r"(r2) \
                     : "memory")
#define gte_stszotz(r0) \
    __asm__ volatile("mfc2 $12, $19;" \
                     "nop;" \
                     "sra $12, $12, 2;" \
                     "sw $12, 0(%0)" \
                     : \
                     : "r"(r0) \
                     : "$12", "memory")

#define gte_SetRotMatrix(r0) \
    __asm__ volatile("lw $12, 0(%0);" \
                     "lw $13, 4(%0);" \
                     "ctc2 $12, $0;" \
                     "ctc2 $13, $1;" \
                     "lw $12, 8(%0);" \
                     "lw $13, 12(%0);" \
                     "lw $14, 16(%0);" \
                     "ctc2 $12, $2;" \
                     "ctc2 $13, $3;" \
                     "ctc2 $14, $4" \
                     : \
                     : "r"(r0) \
                     : "$12", "$13", "$14")
#define gte_SetTransMatrix(r0) \
    __asm__ volatile("lw $12, 20(%0);" \
                     "lw $13, 24(%0);" \
                     "ctc2 $12, $5;" \
                     "lw $14, 28(%0);" \
                     "ctc2 $13, $6;" \
                     "ctc2 $14, $7" \
                     : \
                     : "r"(r0) \
                     : "$12", "$13", "$14")
#define gte_rtv0() \
    __asm__ volatile("nop;" \
                     "nop;" \
                     ".word 0x4A486012")
#define gte_gpf12() \
    __asm__ volatile("nop;" \
                     "nop;" \
                     ".word 0x4B98003D")
#define gte_lddp(r0) __asm__ volatile("mtc2 %0, $8" : : "r"(r0))
#define gte_stsv(r0) \
    __asm__ volatile("mfc2 $12, $9;" \
                     "mfc2 $13, $10;" \
                     "mfc2 $14, $11;" \
                     "sh $12, 0(%0);" \
                     "sh $13, 2(%0);" \
                     "sh $14, 4(%0)" \
                     : \
                     : "r"(r0) \
                     : "$12", "$13", "$14", "memory")
#define gte_ldsv(r0) \
    __asm__ volatile("lhu $12, 0(%0);" \
                     "lhu $13, 2(%0);" \
                     "lhu $14, 4(%0);" \
                     "mtc2 $12, $9;" \
                     "mtc2 $13, $10;" \
                     "mtc2 $14, $11" \
                     : \
                     : "r"(r0) \
                     : "$12", "$13", "$14")

/* A model's part list, as far as the menu reads it. */
typedef struct {
    u8 unk0[0x4C];
    Matrix matrix; /* 0x4C: the part's local transform */
} ModelPart;

typedef struct {
    u32 unk0;
    ModelPart **parts;
} ModelPartList;

typedef struct {
    u32 unk0;
    ModelPartList *list;
} Model;

/* The common head of every spark record. */
typedef struct {
    SVector pos; /* pos.pad: frames left to live, 0 = idle */
    SVector vel;
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
    SVector base;           /* 0x14: added to the rotation's translation */
    SVector origin;         /* 0x1C: where sparks are emitted */
    SVector range;          /* 0x24: random spread per axis */
    SVector offset;         /* 0x2C: subtracted from the random spread */
    SVector angles;         /* 0x34: launch direction, applied first */
    SVector turn;           /* 0x3C: rotation applied after the caller's */
    s16 spread;             /* 0x44: random launch angle range */
    s16 unk46;
    s16 speed;              /* 0x48: minimum launch speed */
    s16 speed_range;        /* 0x4A: random extra launch speed */
    s16 unk4C;
    s16 placement;          /* 0x4E: index into the placement rules */
    void (*reset)(void *spark);          /* 0x50 */
    void (*draw)(void *spark, u32 *ot);  /* 0x54 */
    void (*setup)(void *spark, Emitter *emitter); /* 0x58 */
    void (*place)(Emitter *emitter, SVector *pos); /* 0x5C */
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
extern void (*D_80091CC4[])(Emitter *emitter, SVector *pos);
extern void (*D_80091CDC[1])(Spark *spark);
extern Emitter *D_80092834; /* the menu's spark emitter */
extern Emitter *D_80092644; /* the menu's glow emitter */
extern s32 D_80092838;      /* spark burst strength, fading by 4 per frame */

/* Sparks drawn as a line through their last positions, with one primitive
 * per draw buffer. */
typedef struct {
    SVector pos;
    SVector vel;
    SVector trail[3];
    LineF4 line[2];
} SparkLine4;

typedef struct {
    SVector pos;
    SVector vel;
    SVector trail[2];
    LineF3 line[2];
} SparkLine3;

typedef struct {
    SVector pos;
    SVector vel;
    SVector trail[1];
    LineF2 line[2];
} SparkLine2;

typedef struct {
    SVector pos;
    SVector vel;
    Tile tile[2];
} SparkTile;

typedef struct {
    SVector pos;
    SVector vel;
    Tile1 dot[2];
} SparkDot;

extern SVector *D_8009282C;   /* scratch vectors for GTE loads */
extern SVector *D_80092830;   /* view origin subtracted before projection */

/* Glow field buffers: bytes, previous and current halfword fields. */
extern u8 *D_80092844;
extern s16 *D_8009283C;
extern s16 *D_80092840;
extern u16 D_80091CE0[]; /* glow palette (256 entries) */

typedef struct {
    u32 tag;
    u32 code[2];
} DrawMode;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} PolyFT4;

#define setShadeTex(p, tge) \
    ((tge) ? setcode(p, getcode(p) | 0x01) : setcode(p, getcode(p) & ~0x01))

extern PolyFT4 D_80096D90[2];  /* glow field quad per draw buffer */
extern Tile D_80096DE0[2];     /* full-screen shade tile per draw buffer */
extern DrawMode D_80096E00[2]; /* its blend mode per draw buffer */
u16 func_80043A58(s32 x, s32 y);                  /* GetClut */
void func_800454DC(DrawMode *p, s32 dfe, s32 dtd, s32 tpage, Rect *tw); /* SetDrawMode */

int abs(int x);
void func_800324B8(s32 tag);                 /* heap allocation tag */
void *func_80031BDC(s32 size, s32 arg);      /* heap allocation */
void func_80032C18(void *block, s32 arg);    /* heap release */
void func_800495DC(SVector *v, Vector *out); /* rotate by the current GTE matrix */
s32 func_8004A64C(SVector *v, s32 *sxy, s32 *p, s32 *flag);
s32 func_8004A67C(SVector *v0, SVector *v1, s32 *v2, s32 *sxy0, s32 *sxy1, s32 *sxy2, s32 *p,
                  s32 *flag);
s32 func_8004A73C(SVector *v0, SVector *v1, SVector *v2, SVector *v3, s32 *sxy0, s32 *sxy1,
                  s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag);
void func_800316C0(u32 *ot, LineF2 *prim);
void func_80031708(u32 *ot, LineF3 *prim);
void func_80031750(u32 *ot, LineF4 *prim);
void func_80031804(u32 *ot, Tile *prim);
void func_80031870(u32 *ot, Tile1 *prim);
void func_80048E94(Matrix *m, Matrix *out); /* transpose */
Matrix *func_8003F738(SVector *angles, Matrix *m); /* rotation matrix from angles */
Matrix *func_80049ACC(Matrix *m0, Matrix *m1);     /* m0 = m0 * m1 */
void func_80049EFC(Matrix *m);                      /* load the GTE rotation */
Emitter *func_8008D3F4(s32 shape, s32 placement);
void func_8008D5C0(Emitter *emitter, s32 count);
void func_8008D680(Emitter *emitter, Matrix *rotation, s32 count);

#endif
