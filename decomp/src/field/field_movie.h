#ifndef FIELD_FIELD_MOVIE_H
#define FIELD_FIELD_MOVIE_H

/* The field movie player (800a7c58, field_800A4748.c) and the VRAM it
 * borrows: party sprite blocks 1 and 2 (8005a414) hold the 320x256 area at
 * (200, 0) while a movie plays. The movie's sound effects run on a timeline
 * per sound-effect bank (field_800854D0.c). */

#include "common.h"
#include "resident/sound.h"

/* The movie request block at 800c3a20, one object: the instructions that
 * fill it keep their stores ahead of loads through the actor pointer, and
 * the movie player (800a7c58) hoists one base for its fields. */
typedef struct FieldMovieRequest {
    s16 file;        /* 20 */
    u16 x;           /* 22: display position */
    u16 y;           /* 24 */
    u16 source_x;    /* 26 */
    u16 source_y;    /* 28 */
    u16 unk2A;       /* 2A */
    u16 sound_start; /* 2C: movie sound time origin */
    u16 unk2E;       /* 2E */
    u16 mode;        /* 30: low nibble layout, 0x40/0xc0 fade */
    u16 width;       /* 32 */
    u16 height;      /* 34 */
    u16 depth24;     /* 36: 1 for a 24-bit display */
    s16 sound_bank;  /* 38: 0xff none */
    u16 unk3A;       /* 3A */
} FieldMovieRequest;

extern FieldMovieRequest D_800C3A20;
#define FIELD_MOVIE D_800C3A20

extern s32 D_800ADB6C;         /* movie stopped */
extern s32 D_800ADB74;         /* movie mode */
extern s32 D_800ADB80;         /* the request's fade bits (mode & 0xc0) */
extern s32 D_800ADB84;         /* nonzero ends a window movie (mode 2) */
extern s32 D_800ADB7C;         /* the player presents frames */
extern s32 D_800ADB78;         /* the draw block of the decoded buffer */
extern s32 D_800ADB50;         /* the panorama is not drawn after the movie */
extern s32 D_800B06A0;         /* the current movie frame */
extern s32 D_800B00E4;         /* cleared on each frame */
extern s32 D_800AFE74;         /* 1 while the text roll draws over frames 687..18e2 */

void func_800A7C58(void);      /* play the requested movie */
void func_800A77C4(s32 unused); /* convert the 24-bit screen to 15-bit pixels */

/* The 24-bit screen conversion reads a packed stream, four bytes a word. */
extern u32 *D_800C3904;        /* packed stream */
extern u32 D_800C2688;         /* current word */
extern s32 D_800B14A8;         /* nibble counter */
extern u32 *D_800C390C;        /* converted pixels */

/* The movie sound effects: a timeline of (time, sound) entries per bank. */
extern u16 D_800AE060[][2];    /* movie sound timeline: time, sound */
extern s32 D_800C3A64;         /* movie sound timeline position */
extern SoundBank *D_800B235C;  /* movie sound-effect bank */
void func_80085788(void);      /* load the request's bank, seek its timeline */
void func_80085678(void);      /* play the effects whose time has come */
void func_80085738(void);      /* release the bank */

/* The movie library (file 0xa9, at 801d0000). */
extern s32 D_801D68B4;
extern s32 D_801E89E0;         /* 1 lets it present frames itself */
void func_801D3538(s32 w, s32 h, s32, s32, s32, s32, s32 rgb24);
void func_801D37CC(s32 file, s32, s32, s32, s32, s32 mode, s32, s32, s32, s32, s32, s32 h,
                   void (*callback)(u16, s32, u16));
void func_801D3F7C(void);
void func_801D43B0(void);      /* close */

#endif
