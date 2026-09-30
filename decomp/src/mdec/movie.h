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
void movie_restart(s32 file, s32 sector, s32 channel, s32 mode, CdlLOC *location);

/* Player statics (movie library image). */
extern s32 movie_split_display;    /* frames span both display buffers */
extern u16 movie_image_width;      /* 16-bit VRAM units */
extern u16 movie_image_height;
extern s32 movie_vlc_limit;        /* halfwords per decode call */
extern s32 movie_color_mode;       /* bit 0: 24-bit output */
extern s16 movie_display_buffers[2][4]; /* VRAM rectangle of each display buffer */
extern s16 movie_slice_rects[2][4];     /* the slice each buffer loads next */
extern s8 movie_player_state;
extern u8 movie_mdec_idle;         /* the MDEC finished a frame */
extern u8 movie_frame_waiting;     /* no frame was in the ring */
extern u8 movie_restarted;
extern s32 movie_start_sector;
extern s32 movie_cd_mode;
extern u16 movie_file;
extern u16 movie_xa_channel;
extern s16 movie_row_limit;        /* rows a slice loads at most */
extern s32 movie_first_frame;
extern s32 movie_shown_frame;      /* last frame fully loaded */
extern s32 movie_end_frame;
extern void (*movie_frame_callback)(); /* (frame, x, y) when loaded */
extern s32 movie_load_enabled;     /* slices go to VRAM */
extern s32 movie_load_restart;
extern s32 movie_vlc_buffer_index;
extern s32 movie_slice_buffer_index;
extern s32 movie_decode_display;   /* display buffer the next frame fills */
extern s32 movie_load_display;     /* display buffer slices load into */
extern u16 movie_frame_width;      /* last frame header width */
extern u16 movie_frame_height;
extern u16 movie_slice_width;      /* 16 or 24 */
extern s32 movie_stall_count;      /* polls without a ring frame */
extern s32 movie_vlc_pending;      /* a frame's decode is resumable */
extern s32 movie_skipped_frames;
extern s16 D_8005A4B8;             /* resident stream state */
extern s32 D_8005A4DC;
extern s32 D_8005A4A8;
extern s32 D_8005A4B4;
extern s16 D_80062514;      /* -1 stopped, 0 closed, 1, 2 */
extern u8 movie_host_stream;       /* resident host-file table in use */
extern void *movie_vlc_buffers[2];   /* the two run-level buffers */
extern void *movie_slice_buffer0;    /* the two decoded-slice buffers */
extern void *movie_slice_buffer1;
extern void *movie_ring_buffer;    /* the stream ring */
extern s32 movie_saved_directory[2]; /* directory of the movie files */
extern s32 movie_fade_in_pending;
extern s32 movie_fade_out_pending;
extern s32 D_8004FE4C;             /* resident host-file handle */

#endif
