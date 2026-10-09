#ifndef MOVIE_MODE_H
#define MOVIE_MODE_H

/* Mode 6, the movie mode: its display buffers and sector header, its view of
 * the movie library's start (mdec/player.h), the resident objects no shared
 * header declares and the unit's own functions and forward-declared data. */

#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/stream.h"
#include "mdec/player.h"

/* Callers convert arguments/result differently from the resident definition
 * (u8 mode; u16 stream parameters; the result read as s16). */
void func_8002A428(s32 mode); /* resident read mode */
void func_80029EB0(s32 file, void *ring, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8,
                   s32 a9); /* stream a file through a ring */
s16 func_8003BDFC(s32 wait);  /* sound transfer busy */

extern u16 D_80062514; /* the requested movie's last frame */

/* The movie request bytes 8004fe44-8004fe47 (cd.h's D_8004FE44-D_8004FE47):
 * kind (bit 7: last frame from 80062514), index, the next mode, and whether
 * buttons do not end the movie. This unit indexes them as one array, loading
 * [0] through the array's address in a register, which the four scalars do
 * not reproduce. */
extern u8 D_8004FE44_request[4] __asm__("D_8004FE44");

/* One display buffer: its drawing and display environments, the ordering
 * table and the two frame primitives drawn over it. */
typedef struct MovieBuffer {
    DRAWENV draw;
    DISPENV disp;
    u_long ot[32];
    u8 box[0x24];
    u8 frame[0x24];
} MovieBuffer;

/* The header that starts each movie (STR) sector. */
typedef struct MovieSector {
    u16 magic; /* 0x160 */
    u16 type;
    u16 sector;
    u16 sectors;
    u32 frame;
} MovieSector;

/* Movie playback. */
extern const RECT D_800704E0; /* the screen area */

/* The library's start (mdec/player.h): u16 rows would load D_800773A0 lhu, not lh. */
void movie_start(s32 file, s32 sector, u16 first_frame, u16 last_frame, u16 channel, s32 select,
                 u16 hold, u16 x0, u16 y0, u16 x1, u16 y1, s16 rows,
                 void (*callback)(u16 frame, u16 x, u16 y));

s32 func_80076488(void);
void func_800768D8(u16 frame, u16 x, u16 y);
void func_800769A4(void);
void func_80076CA4(void);

/* CD-ROM monitor. */
void func_80071BA0(void);
void func_80070DCC(void);
void func_80071C34(s32 command);
s32 func_80074AF0(void);

/* FAT check. */
void func_80072F98(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h);
void func_800734B8(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h);
s32 func_800747AC(s32 first, s32 last, s32 *button);
void func_80074B58(void);
u32 func_80075D4C(s32 index);
extern const char D_8007042C[]; /* "\n" */
extern const char D_80070430[]; /* "\nPUSH CIRCLE BUTTON TO MENU." */

/* CD-ROM monitor screen. */
extern const char D_8006FC70[], D_8006FC8C[], D_8006FC98[], D_8006FCA4[], D_8006FCAC[];
extern const char D_8006FCB4[], D_8006FCD8[];
void func_800712C4(void);

/* Mode entry and menu. */
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
extern const char D_8006FC6C[]; /* "\n", first used by the menu (800704E8) */
void func_8007293C(void);
s32 func_80072A08(s32 disc, s32 state, s32 *error, s32 *done);

#endif
