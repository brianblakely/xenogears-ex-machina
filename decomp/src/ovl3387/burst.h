#ifndef OVL3387_BURST_H
#define OVL3387_BURST_H

#include "common.h"

/* libgte's field-by-field vector copy. */
#define copyVector(v0, v1) (v0)->vx = (v1)->vx, (v0)->vy = (v1)->vy, (v0)->vz = (v1)->vz

/* Resident task system: a task node (update) followed by its drawing node;
 * both callbacks receive their node, whose +4 names the node's object. */
typedef struct TaskNode {
    u32 unk0;
    void *object;
    void (*update)(struct TaskNode *node);
    void (*destroy)(struct TaskNode *node);
    u32 unk10;
    u32 unk14;
    struct TaskNode *next;
} TaskNode;

void func_8001CB48(TaskNode *node);        /* unlink a drawing node */
void func_8001CD94(TaskNode *node);        /* unlink a task */
void func_80025180(void *block);           /* release a block after the frame */
void *func_80031BDC(s32 size, s32 mode);   /* allocate a heap block */
void func_800320E8(void *block);           /* release a heap block */
s32 DrawSync(s32 mode);

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} POLY_F4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad3;
} POLY_GT3;

/* libgpu drawing and display environments. */
typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg;
    u8 r0, g0, b0;     /* +19: background colour */
    u32 dr_env[16];
} DRAWENV;             /* 0x5c */

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;             /* 0x14 */

s32 VSync(s32 mode);
s32 ClearOTagR(u32 *ot, s32 n);
void DrawOTag(u32 *ot);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
s32 StoreImage(RECT *rect, void *p);
s32 LoadImage(RECT *rect, void *p);
s32 ClearImage(RECT *rect, u8 r, u8 g, u8 b);
void SetPolyF4(POLY_F4 *p);
void SetPolyGT3(POLY_GT3 *p);
void SetShadeTex(void *p, s32 tge);
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
void AddPrim(void *ot, void *p);
void ReadGeomOffset(s32 *ofx, s32 *ofy);
s32 ReadGeomScreen(void);
void SetGeomOffset(s32 ofx, s32 ofy);
void SetGeomScreen(s32 h);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
s32 RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, s32 *sxy0, s32 *sxy1, s32 *sxy2,
                  s32 *p, s32 *flag);
s32 SquareRoot0(s32 a);
MATRIX *func_8003F738(SVECTOR *rotation, MATRIX *m); /* RotMatrix */
void func_8004A414(VECTOR *v0, VECTOR *v1);         /* Square0 */
s32 func_8003F8B0(s32 angle);                       /* sine (4096 = 1.0) */
s32 func_8003F8CC(s32 angle);                       /* cosine (4096 = 1.0) */
u8 func_80021AD8(u8 value, s32 delta);              /* add, clamped to 0..255 */

extern u8 *D_80059580; /* next free primitive */

/* One battle display buffer (0x4070 bytes). */
typedef struct {
    DRAWENV draw;      /* +0000 */
    DISPENV disp;      /* +005c */
    u32 ot[0x1000];    /* +0070: reverse ordering table */
} DrawBuffer;

/* Battle overlay work area (800c3eb0); only the members the module uses. */
typedef struct {
    u8 unk0[0xB70];
    DrawBuffer buffers[2]; /* +0b70 */
    DrawBuffer *current;   /* +8c50 */
    u32 *ot;               /* +8c54 */
    u8 unk8C58[0x2C];
    s32 buffer;            /* +8c84: double-buffer index being drawn */
} BattleArea;
extern BattleArea D_800C3EB0;

/* The battle's texture pages to keep (two 64x256 areas). */
typedef struct {
    u8 unk0[4];
    s16 x0, y0;        /* +4 */
    s16 x1, y1;        /* +8 */
} BattleVram;
extern BattleVram D_800C3668;

/* One cell of the captured screen: two triangles' primitives per display
 * buffer, the corners and each corner's distance from the centre. */
typedef struct {
    u32 unk0;
    POLY_GT3 prim[2];  /* +04 */
    SVECTOR corner[3]; /* +54 */
    s32 distance[3];   /* +6c */
    u32 unk78;
} BurstCell;           /* 0x7c */

/* The effect's state (0x10fa4 bytes): laid out as a task node pair, but run
 * directly by func_801FC8F4's own frame loop. */
typedef struct {
    TaskNode task;    /* +00 */
    TaskNode draw;    /* +1c */
    s32 brightness;   /* +38: 0x80 neutral, fades out at the end */
    s32 angle;        /* +3c */
    s32 twist;        /* +40 */
    s32 frame;        /* +44 */
    s32 speed;        /* +48 */
    VECTOR trans;     /* +4c */
    SVECTOR rot;      /* +5c */
    BurstCell cells[2][14][20]; /* +64: two triangles per 16x16 cell */
} Burst;

extern SVECTOR D_801FCE18[3]; /* first triangle of a cell */
extern SVECTOR D_801FCE30[3]; /* second triangle */
extern u32 *D_801FCE48;       /* ordering table being filled */

extern u8 D_801FCE14; /* the effect's variant (1 in the module's data) */

void func_801FC000(TaskNode *node);
void func_801FC11C(TaskNode *node);
void func_801FC400(Burst *burst);
Burst *func_801FC470(void);
Burst *func_801FC4A8(Burst *burst);
void func_801FC8F4(void);

#endif
