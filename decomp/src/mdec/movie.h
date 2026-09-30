#ifndef MOVIE_H
#define MOVIE_H

#include "common.h"

/* Resident services used by the movie library. */
void func_80038D18(s32 left, s32 right);  /* CD input volume */
void func_8002A498(s32 offset);           /* end the resident read */
void func_8002A428(s32 mode);             /* resident read mode */
s32 func_80028A60(s32 mode);              /* wait for the disc */
s32 func_80028AAC(void);                  /* reset the resident stream ring */
void func_800320E8(void *block);          /* release a heap block */

/* PsyQ libpress / CD streaming members linked into this image. */
void DecDCTReset(s32 mode);
void DecDCToutCallback(void (*callback)(void));
void StUnSetRing(void);

/* Player statics (movie library image). */
extern s8 movie_player_state;      /* -1 stopped, 0 closed, 1, 2 */
extern u8 movie_host_stream;       /* resident host-file table in use */
extern void *movie_vlc_buffers[2];   /* the two run-level buffers */
extern void *movie_slice_buffer0;    /* the two decoded-slice buffers */
extern void *movie_slice_buffer1;
extern void *movie_ring_buffer;    /* the stream ring */

#endif
