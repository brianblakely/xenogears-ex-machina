#ifndef MOVIE_MODE_H
#define MOVIE_MODE_H

#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libsn.h"

/* PsyQ libgpu / libgte / libetc structures as the overlay uses them. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
    u32 code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter;
    u8 isrgb24;
    u8 pad0, pad1;
} DISPENV;

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

/* libgpu, libgte, libetc and libsn members linked into the resident. */
void ClearImage(RECT *rect, s32 r, s32 g, s32 b);
void MoveImage(RECT *rect, s32 x, s32 y);
s32 DrawSync(s32 mode);
s32 VSync(s32 mode);
void SetDispMask(s32 mask);
DRAWENV *SetDefDrawEnv(DRAWENV *env, s32 x, s32 y, s32 w, s32 h);
DISPENV *SetDefDispEnv(DISPENV *env, s32 x, s32 y, s32 w, s32 h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
void ClearOTagR(u32 *ot, s32 n);
void DrawOTag(u32 *ot);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetGeomScreen(s32 h);
void SetGeomOffset(s32 ofx, s32 ofy);
void SetColorMatrix(MATRIX *m);
void SetBackColor(s32 r, s32 g, s32 b);
MATRIX *RotMatrixZ(s32 r, MATRIX *m);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
s32 SquareRoot0(s32 a);
s32 ratan2(s32 y, s32 x);
int PCopen(char *name, int flags, int perms);
int PCclose(int fd);

/* Resident services. */
void func_80028470(s32 directory, s32 offset); /* select a directory */
s16 func_80028928(s32 list);                   /* files in a directory list */
s32 func_80028A60(s32 mode);                   /* wait for the disc */
void func_8002A428(s32 mode);                  /* resident read mode */
void func_8002A498(s32 offset);                /* end the resident read */
s32 func_8002C3D8(void);                       /* host-file table in use */
void *func_80031BDC(s32 size, s32 mode);       /* allocate a heap block */
void func_8002954C(s32 file, void *buffer, s32 size, s32 arg3, s32 arg4); /* read a file */
s32 func_8003569C(s32 port);                   /* controller buttons */
void func_80038D18(s32 volume, s32 speed);     /* CD input volume */
void func_80039A80(s32 arg0, s32 arg1, s32 arg2);
int func_8004C398(int fd, void *buffer, int size); /* libsn host-file write */
void func_8003F738(SVECTOR *rotation, MATRIX *m); /* rotation matrix */
void func_80049BDC(MATRIX *m0, MATRIX *m1);    /* matrix product */

/* One display buffer: its drawing and display environments, the ordering
 * table and the two frame primitives drawn over it. */
typedef struct MovieBuffer {
    DRAWENV draw;
    DISPENV disp;
    u32 ot[32];
    u8 box[0x24];
    u8 frame[0x24];
} MovieBuffer;

extern MovieBuffer *D_80077120; /* buffer being drawn */
extern MovieBuffer D_80077124[2];

/* Debug statistics. */
extern s32 D_80076E5C;          /* vertical blanks counted */
extern s32 D_80076EC8;          /* seconds counted */
extern s32 D_80076ECC;          /* frames of the current second */
extern s32 D_80076EF4;          /* random number state */
extern s32 D_80076EF8;
extern void *D_80076EA0;        /* FAT check read buffer */
extern s32 D_80076E4C;
extern s32 D_80076EAC;
extern s32 D_80076EB0;
extern s32 D_80076F3C[13];      /* reads per result class */
extern u8 D_80076F84[8];        /* CD command result */
extern s32 D_8007700C;
extern u8 *D_8004FDF0;          /* disc directory records, 7 bytes each */

/* The playback camera (unused by the movie path). */
extern VECTOR D_8007702C;                      /* eye */
extern VECTOR D_8007703C;                      /* target */
extern s32 D_8007704C;                         /* roll */
extern s32 D_80076F2C, D_80076F30, D_80076F34; /* translation */
extern MATRIX D_80077050;                      /* world to screen */
extern MATRIX D_80077070;                      /* light colors */
extern MATRIX D_80077090;                      /* light directions */
extern SVECTOR D_800770B0;                     /* camera rotation */
extern MATRIX D_800770B8;                      /* camera translation */
extern MATRIX D_800770D8;                      /* camera rotation */
extern MATRIX D_800770F8;

/* Movie playback. */
extern s32 D_80076F04;          /* frames the buttons were held */
extern s32 D_80076F08;          /* frames until they repeat */
extern s32 D_80077010;          /* last frame the library loaded */
extern s32 D_80077014;          /* 1: stop; 2..5: frames until then */
extern s32 D_80077018;          /* buffer the frame went to */
extern s32 D_8007701C;          /* buffer on display */
extern s32 D_80077020;          /* decoding paused */
extern s32 D_80077024;          /* the first buffer's y */
extern s32 D_80077028;          /* buttons do not end the movie */
extern s32 D_8007711C;          /* movie index */
extern s32 D_80077398;          /* XA channel */
extern s32 D_8007739C;          /* last frame */
extern s32 D_800773A0;          /* rows */
extern s32 D_800773A4;          /* first frame */
extern s32 D_800773A8;          /* start sector */
extern s32 D_800773AC;          /* buttons */
extern s32 D_800773B4;          /* previous buttons */
extern s32 D_80077438;          /* split display */
extern s32 D_80077448;          /* movie kind */
extern s32 D_801D68B4;          /* movie library: split display */

void func_80076488(void);
void func_80076CA4(void);

#endif
