#ifndef FIELD_FIELD_H
#define FIELD_FIELD_H

#include "common.h"

/* GTE vector/matrix layouts (PsyQ libgte). */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* libgpu environments. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    u32 dr_env[16];
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

/* One of the two draw-buffer blocks (800b249c, 800ba590). */
typedef struct {
    DRAWENV draw;
    DRAWENV draw2;
    DISPENV disp;
    u8 rest[0x80F4 - 0xCC];
} FieldDrawBlock;

/* One screen fade channel (800b20c4 + 0x58 * channel). Levels are 8.8. */
typedef struct {
    u8 modes[2][12]; /* DR_MODE per draw buffer */
    u8 tiles[2][16]; /* TILE per draw buffer */
    s32 level[3];
    s32 step[3];
    u16 abr;
    s16 active;
    s16 steps;
    u16 pad;
} FadeChannel;

/* One of the three field light/lookup slots at 800b06a4. */
typedef struct {
    s16 a;
    s16 b;
    s16 c;
} FieldSlot6;

/* Resident services. */
extern void func_80028A60(s32);
extern void func_80029EB0(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32);
extern void *func_8002A260(s32 sectors, s32);
extern void func_800320E8(void *);
extern void func_80032EB4(s32 index, void *destination);
extern void func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
extern void func_800445D0(s32);

/* Field overlay. */
extern void func_8007D93C(s32 channel);
extern void func_8007AD8C(void *pad0, void *pad1);
extern void func_8007ADA4(s32 left, s32 right, s32 top, s32 bottom);
extern void func_8007AE14(s32 x_divisor, s32 y_divisor);
extern void func_8007AE2C(s32 port, s32 x, s32 y);
extern void func_8007DA44(void *ot, s32 buffer);
extern void func_80078C5C(void);

/* Resident state. */
extern s32 D_8004F34C; /* current map */

extern u8 D_800625FC[2][0x22]; /* pad buffers */

/* Field state. */
extern s32 D_800ADB08;
extern s32 D_800ADC04; /* fade mode; fades start only in mode 2 */
extern s16 D_800ADC08; /* fade started */
extern FadeChannel D_800B20C4[2];
extern FieldDrawBlock D_800B249C[2];
extern s32 D_800ADB0C;
extern s32 D_800ADB60; /* field stream running */
extern void *D_800ADC14; /* field stream ring */
extern FieldSlot6 D_800B06A4[3];

#endif
