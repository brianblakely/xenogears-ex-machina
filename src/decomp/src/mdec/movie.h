#ifndef MOVIE_H
#define MOVIE_H

/* The movie library (disc file 19 at 0x801d3000): its sector header and
 * decoder state, its commons and the resident objects no shared header
 * declares. */

#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libpress.h"
#include "psyq/libsn.h"
#include "resident/cd.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "resident/stream.h"
#include "mdec/player.h"

/* Callers convert arguments/result differently from the resident definition
 * (u8 there): the resident read mode. */
void cd_set_mode(s32 mode);

/* A movie sector's header (the first 32 bytes of each STR sector). */
typedef struct MovieSectorHeader {
    u16 magic;
    u16 type;
    u16 sector;
    u16 sectors;
    u32 frame;
    u32 bytes;
    u16 width;
    u16 height;
    u32 unused[3];
} MovieSectorHeader;

/* The player's own functions (the entries others call: mdec/player.h). */
void movie_slice_decoded(void);
void movie_decode(void);
u_long *movie_next_bitstream(u32 end_frame, MovieSectorHeader **header);
void movie_restart(s32 file, s32 sector, s32 channel, s32 mode, CdlLOC *location);

/* Decoder double-buffering state (801e8918..801e8958), one aggregate like the
 * PsyQ movie samples' DECENV: movie_decode keeps &decode_display in $s0 and
 * reaches vlc_buffers[i] as s0 - 0x14 + 4i and slice_buffers[i] as s0 - 8 + 4i,
 * which GCC only does for members of one object. */
typedef struct MovieArea {
    s16 x, y;
    s16 right, bottom; /* far corner of the frame drawn at (x, y) */
} MovieArea;

typedef struct MovieDecoder {
    s32 vlc_index;           /* run-level buffer the next frame decodes into */
    u_long *vlc_buffers[2];
    s32 slice_index;         /* slice buffer the MDEC writes next */
    void *slice_buffers[2];
    s32 decode_display;      /* display buffer the next frame fills */
    MovieArea display[2];    /* VRAM area of each display buffer */
    s32 load_display;        /* display buffer slices load into */
    RECT slice[2];           /* the slice each buffer loads next */
} MovieDecoder;

/* Player commons, placed among libcd's (commons/; movie_skipped_frames and
 * movie_load_restart in mdec/player.h). */
extern u16 movie_slice_width;      /* 16 or 24 */
extern s32 movie_previous_frame;
extern s32 movie_stall_count;      /* polls without a ring frame */

/* libcd's StCdIntrFlag: a ring interrupt waits for the MDEC DMA. */
extern s32 movie_stream_deferred;

#endif
