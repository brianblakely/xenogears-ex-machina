#ifndef DEBUG595_DEBUG595_H
#define DEBUG595_DEBUG595_H

#include "common.h"

/* libgte / libgpu types. */
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

/* A primitive's tag: the next primitive's address and this one's length. */
typedef struct {
    u32 addr : 24;
    u32 len : 8;
} P_TAG;

#define addPrim(ot, p) \
    (((P_TAG *)(p))->addr = ((P_TAG *)(ot))->addr, ((P_TAG *)(ot))->addr = (u32)(p))

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad;
    s16 x1, y1;
} LINE_G2;

/* One debug line: a segment in its own frame, drawn in both buffers. */
typedef struct {
    MATRIX matrix;   /* 0x00 */
    SVECTOR rot;     /* 0x20 */
    SVECTOR trans;   /* 0x28 */
    SVECTOR start;   /* 0x30 */
    SVECTOR end;     /* 0x38 */
    LINE_G2 line[2]; /* 0x40: one per draw buffer */
} DebugLine;         /* 0x68 */

/* The part of a field actor the monitor reads. */
typedef struct {
    u8 unk0[0x18];
    SVECTOR size; /* 0x18: collision half extents */
    VECTOR pos;   /* 0x20: 16.16 fixed point */
} DebugActor;

/* One CPU-time mark: scanlines spent before it. */
typedef struct {
    s32 time;
    char *name;
    s32 unk8;
} CpuMark;

typedef struct {
    s16 x, y, w, h;
} DebugRect;

/* libgte / libgpu (resident). */
void LoadImage(DebugRect *rect, u32 *data);
s32 VSync(s32 mode);
void func_800379C8(const char *format, ...); /* debug text print */
void func_80036DC8(s32 r, s32 g, s32 b);     /* debug text colour */
void PushMatrix(void);
void PopMatrix(void);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
s32 RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, s32 *sxy0, s32 *sxy1, s32 *sxy2, s32 *p,
                  s32 *flag);
void SetLineG2(LINE_G2 *p);
MATRIX *func_8003F738(SVECTOR *rot, MATRIX *m); /* RotMatrix */
MATRIX *func_80049BDC(MATRIX *m0, MATRIX *m1);
void func_8004A6DC(SVECTOR *v, s32 *t, SVECTOR *r);
void func_80037324(u32 *ot);

/* Field state. */
extern s32 D_800C268C;
extern u32 *D_800C426C;  /* field ordering tables */
extern s32 D_800ADB08;   /* current draw buffer */
extern MATRIX D_800AFA64;
extern u16 D_800AFEA0;   /* buttons held (shoulder bits) */
extern u16 D_800C3908;   /* buttons pressed or repeating */
extern u16 D_800AFE9C;   /* buttons held */
extern s32 D_80065850;   /* camera control mode */
extern s32 D_80065854;   /* analog steps */
extern s32 D_80065858;
extern s32 D_800ADB94;   /* camera distance */
extern s32 D_800ADB98;
extern s32 D_800AF984;
extern s32 D_800AF988;
extern s32 D_800AF9F0;
extern s16 D_800AF9E6;
extern s16 D_800AF9FC;
extern u16 D_800AF9FE;

/* Tool statics. */
extern s32 D_8028597C;
extern s32 D_80285980;
extern s32 D_80285988;   /* debug lines shown */
extern s32 D_8028598C;
extern s32 D_80285990;
extern s32 D_80285994;
extern s32 D_80285998;
extern s16 D_802859A4;   /* CPU-time marks this frame */
extern CpuMark D_802859AC[];
extern s32 D_800ADB9C;   /* scanline count at the last mark */
extern u16 D_80285B28[16];
extern DebugLine D_80285B48[16];

s32 func_80281B90(u32 *ot);
void func_802814D4(u32 *ot, DebugLine *line, MATRIX *m, s32 buffer);

#endif
