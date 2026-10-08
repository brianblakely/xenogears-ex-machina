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

typedef struct {
    u8 r, g, b, cd;
} CVECTOR;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;

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

/* libcd commands not yet in psyq/libcd.h. */
#ifndef CdlNop
#define CdlNop 0x01
#endif
#ifndef CdlStop
#define CdlStop 0x08
#endif
#ifndef CdlGetTN
#define CdlGetTN 0x13
#endif
#ifndef CdlSeekL
#define CdlSeekL 0x15
#endif

extern void *D_8004FDF4;        /* disc directory counts */
extern void *D_8004FE48;        /* disc file names (host builds) */

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
void func_8003F738(SVECTOR *rotation, MATRIX *m); /* rotation matrix */
void MulMatrix2(MATRIX *m0, MATRIX *m1);    /* matrix product */

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

/* Debug statistics (the unit's .data is defined in movie.c). The .bss starts
 * at 80076f3c with these counters; the decoded image ends 7 bytes into them. */
extern s32 D_80076F3C[16];      /* reads per result class */
extern u8 D_80076F84[8];        /* CD command result */
extern s32 D_8007700C;
extern u8 *D_8004FDF0;          /* disc directory records, 7 bytes each */

/* Menu backdrop: each corner's color fades from one random color to the
 * next over a random number of frames. */
extern CVECTOR D_80076F8C[4];   /* from */
extern CVECTOR D_80076F9C[4];   /* to */
extern s32 D_80076FAC[4];       /* frames into the fade */
extern s32 D_80076FBC[4];       /* frames of the fade */
extern CVECTOR D_80076FCC[4];   /* menu frame: from */
extern CVECTOR D_80076FDC[4];   /* to */
extern s32 D_80076FEC[4];
extern s32 D_80076FFC[4];
void SetPolyG4(POLY_G4 *p);
void SetSemiTrans(void *p, s32 abe);

/* The playback camera (unused by the movie path). */
extern VECTOR D_8007702C;                      /* eye */
extern VECTOR D_8007703C;                      /* target */
extern s32 D_8007704C;                         /* roll */
extern MATRIX D_80077050;                      /* world to screen */
extern MATRIX D_80077070;                      /* light colors */
extern MATRIX D_80077090;                      /* light directions */
extern SVECTOR D_800770B0;                     /* camera rotation */
extern MATRIX D_800770B8;                      /* camera translation */
extern MATRIX D_800770D8;                      /* camera rotation */
extern MATRIX D_800770F8;

/* The header that starts each movie (STR) sector. */
typedef struct MovieSector {
    u16 magic; /* 0x160 */
    u16 type;
    u16 sector;
    u16 sectors;
    u32 frame;
} MovieSector;

s32 func_800288EC(s32 file);   /* a file's size */
char *func_80028998(s32 file); /* a file's host name */
s32 func_800289D0(s32 file);   /* a file's first sector */

/* Menu. */
extern s32 D_80077118;          /* cursor */
extern s32 D_800773B0;          /* monitor shown */
extern s32 D_80077394;          /* statistics shown */
void *func_80028570(char *name, s32 mode);  /* load a host file */
void func_800320E8(void *block);            /* release a heap block */
void func_80037FD8(void *bank, s32 arg1);   /* transfer a sound bank */
s16 func_8003BDFC(s32 arg0);                /* sound transfer busy */

/* Movie playback. */
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

extern s32 D_800773B8[32];      /* VSync(1) before and after each decode step */
extern s32 D_80077454;          /* library output mode (bit 0: 24-bit) */
extern const RECT D_800704E0;   /* the screen area */

/* Movie library (disc file 19 at 0x801d3000, see decomp/src/mdec). */
s32 func_801D3538(u16 width, u16 height, u16 scale, u16 slice, u16 sectors, u16 limit, u16 mode);
void func_801D37CC(s32 file, s32 sector, u16 first_frame, u16 last_frame, u16 channel, s32 select,
                   u16 hold, u16 x0, u16 y0, u16 x1, u16 y1, s16 rows,
                   void (*callback)(u16 frame, u16 x, u16 y));
void func_801D3F7C(void); /* poll */
void func_801D4318(void); /* stop */
void func_801D43B0(void); /* close */

s32 func_80028738(s32 file);  /* a file's size */
void func_80019CA0(void);     /* soft reset check */

s32 func_80076488(void);
void func_800768D8(u16 frame, u16 x, u16 y);
void func_800769A4(void);
void func_80076CA4(void);

/* CD-ROM monitor. */
typedef struct StreamEntry {
    u16 file; /* 0 ends the list */
    s32 *dest;
} StreamEntry;

extern s32 D_80076F7C;
extern s32 D_80076F80;
s32 *func_80028B14(void);             /* next arrived stream chunk */
void func_8002945C(void *chunk);      /* release a stream chunk */
s32 func_80028F30(s32 *arg0, s32 *arg1);
void func_800294B4(s32 arg0);
void func_80071BA0(void);
void func_80070DCC(void);
void func_80071C34(s32 command);
s32 func_80074AF0(void);

/* FAT check. */
extern s32 D_8007744C;                           /* buffer index */
void func_800284B4(s32 *directory, s32 *offset); /* current directory */
void func_8003700C(char *format, ...);           /* debug font print */
void func_8003278C(s32 a, s32 value, s32 c, s32 d);
void func_80037324(u32 *ot);                     /* draw the debug font */
void func_80072F98(u32 *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h);
void func_800734B8(u32 *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h);
s32 func_800747AC(s32 first, s32 last, s32 *button);
void func_80074B58(void);
u32 func_80075D4C(s32 index);
extern char D_8007042C[]; /* "\n", shared with the asm-backed FAT check */
extern char D_80070430[]; /* "\nPUSH CIRCLE BUTTON TO MENU." */
s32 func_80039850(void *sequence); /* load a music sequence */

/* CD-ROM monitor screen. */
void func_8002A524(StreamEntry *list); /* release a file list's buffers */
extern s32 D_8005A4DC;          /* resident read error count */
extern s32 D_8004FE1C;          /* resident read status */
extern s32 D_8005A488, D_8005A48C, D_8005A490, D_8005A494, D_8005A498, D_8005A49C;
extern s32 D_8005A4A4, D_8005A4A8, D_8005A4B4; /* resident CD event counters */
extern s32 D_8004FDE4, D_8004FDE8, D_8004FDEC;
extern u16 D_8004FE26, D_8004FE28;
extern char D_8006FC70[], D_8006FC8C[], D_8006FC98[], D_8006FCA4[], D_8006FCAC[];
extern char D_8006FCB4[], D_8006FCD8[]; /* strings shared with the asm-backed functions */
void *func_80028A94(void *ring); /* replace the stream ring */
s32 func_800286CC(void);         /* files left to read */
s32 func_800286BC(void);         /* bytes left to read */
void func_800712C4(void);
StreamEntry *func_8002A57C(s32 list, s32 mode);          /* a directory's file list */
void func_80029AFC(StreamEntry *list, s32 mode, s32 flags); /* stream a file list */
void func_800295D8(s32 file, void *dest, s32 mode, s32 flags); /* host-file stream */
void func_80029EB0(s32 file, void *ring, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8,
                   s32 a9);
void *func_8002A260(s32 blocks, s32 mode);                /* allocate a stream ring */

/* Mode entry and menu. */
/* Movie request: kind (bit 7: last frame from 80062514), index, the next
 * mode, and whether buttons do not end the movie. */
extern u8 D_8004FE44[4];
extern u16 D_80062514;          /* the requested movie's last frame */
extern s16 D_8005A4B8;
extern s32 D_801E89D4;          /* movie library: frames skipped */
extern s32 D_80077440;          /* menu shown */
extern s32 D_80077444;          /* start frame: 1 changed, 2 sought */
extern s32 D_8007743C;          /* end frame: 0 changed, 1 found, 2 not found */
extern s32 D_80077450;          /* disc mode: 0, -1 or host */
void func_80032498(s32 arg0, s32 arg1);
void func_800374E8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9,
                   s32 a10); /* debug font setup */
void func_8001996C(s32 mode); /* select the next mode */
void func_80019ACC(s32 arg0); /* leave the mode */
void func_80072D84(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h);
void func_80073328(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h);
s32 func_80074BA4(s32 frame);
s32 func_8007519C(void);
void func_80075534(void);
void func_80075D8C(void);
void func_8007625C(void);
s32 func_800763BC(u8 keep);
void func_800704E8(void);
void func_80072480(void);

/* Disc change test. */
extern char D_8006FC6C[]; /* "\n", first used by the menu (800704E8) */
s32 func_80028530(void);  /* the disc in the drive */
void func_8007293C(void);
s32 func_80072A08(s32 disc, s32 state, s32 *error, s32 *done);

#endif
