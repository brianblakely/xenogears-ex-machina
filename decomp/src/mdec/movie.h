#ifndef MOVIE_H
#define MOVIE_H

#include "common.h"
#include "psyq/libcd.h"

/* Resident services used by the movie library. */
void func_800284B4(s32 *directory, s32 *offset); /* current directory */
void func_80028470(s32 directory, s32 offset);   /* select a directory */
s32 func_800289D0(s32 file);                     /* a file's first sector */
void func_800295D8(s32 file, void *ring, s32 arg2, s32 mode); /* host-file stream */
void func_80038D18(s32 left, s32 right);  /* CD input volume */
void func_8002A498(s32 offset);           /* end the resident read */
void func_8002A428(s32 mode);             /* resident read mode */
s32 func_80028A60(s32 mode);              /* wait for the disc */
s32 func_80028AAC(void);                  /* reset the resident stream ring */
void func_800320E8(void *block);          /* release a heap block */
void *func_80031BDC(s32 size, s32 mode);  /* allocate a heap block */
s32 func_8002C3D8(void);                  /* resident host-file table in use */
void *func_8002A260(s32 blocks, s32 mode); /* resident stream ring */

s32 func_80028F30(u32 **data, void *header); /* next host-stream frame */
void func_800294B4(void *header);         /* release a host-stream frame */

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

/* PsyQ libpress / CD streaming members linked into this image. */
void DecDCTReset(s32 mode);
void DecDCToutCallback(void (*callback)(void));
void StUnSetRing(void);
void StSetRing(void *ring, s32 sectors);
s32 StGetBackloc(CdlLOC *location);
void StSetStream(s32 mode, s32 start_frame, s32 end_frame, void *func1, void *func2);
void movie_slice_decoded(void);
void movie_stop(void);
void movie_decode(void);
void DecDCTin(void *buffer, s32 mode);
void DecDCTout(void *buffer, s32 size);
s32 DecDCTvlcSize(s32 size);
s32 DecDCTvlc(u32 *bitstream, void *buffer);
s32 StGetNext(u32 **data, void *header);
void StFreeRing(void *data);
void StCdInterrupt(void);
u32 *movie_next_bitstream(u32 end_frame, MovieSectorHeader **header);
void movie_restart(s32 file, s32 sector, s32 channel, s32 mode, CdlLOC *location);

/* Decoder double-buffering state (801e8918..801e8958), one aggregate like the
 * PsyQ movie samples' DECENV: movie_decode keeps &decode_display in $s0 and
 * reaches vlc_buffers[i] as s0 - 0x14 + 4i and slice_buffers[i] as s0 - 8 + 4i,
 * which GCC only does for members of one object. */
typedef struct MovieArea {
    s16 x, y;
    s16 right, bottom; /* far corner of the frame drawn at (x, y) */
} MovieArea;

typedef struct MovieRect {
    s16 x, y, w, h;
} MovieRect;

typedef struct MovieDecoder {
    s32 vlc_index;           /* run-level buffer the next frame decodes into */
    u32 *vlc_buffers[2];
    s32 slice_index;         /* slice buffer the MDEC writes next */
    void *slice_buffers[2];
    s32 decode_display;      /* display buffer the next frame fills */
    MovieArea display[2];    /* VRAM area of each display buffer */
    s32 load_display;        /* display buffer slices load into */
    MovieRect slice[2];      /* the slice each buffer loads next */
} MovieDecoder;

void LoadImage(MovieRect *rect, u32 *data); /* libgpu */

/* Player commons, placed among libcd's (commons/). */
extern u16 movie_slice_width;      /* 16 or 24 */
extern s32 movie_skipped_frames;
extern s32 movie_load_restart;
extern s32 movie_previous_frame;
extern s32 movie_stall_count;      /* polls without a ring frame */

/* libcd's StCdIntrFlag: a ring interrupt waits for the MDEC DMA. */
extern s32 movie_stream_deferred;

extern s16 D_8005A4B8;             /* resident stream state */
extern s32 D_8005A4DC;
extern s32 D_8005A4A8;
extern s32 D_8005A4B4;
extern s16 D_80062514;      /* -1 stopped, 0 closed, 1, 2 */
extern s32 D_8004FE4C;             /* resident host-file handle */

#endif
