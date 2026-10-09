#ifndef MDEC_PLAYER_H
#define MDEC_PLAYER_H

/* The movie library's player (disc file 19, linked at 0x801d3000; its source
 * is decomp/src/mdec), as the movie mode (decomp/src/movie) and the field's
 * movie player (field_800A4748.c) use it. Their links take these names from
 * their symbol files. movie_start is not here: the movie mode passes its row
 * limit as an s16, so each caller declares it (movie_mode.h, field_movie.h). */

#include "common.h"

/* Open the library for a `width` x `height` movie (16-bit VRAM units).
 * `scale` sizes the two run-level buffers (width * height * scale / 128
 * bytes), `slice` is the macroblock column width, `sectors` the ring length,
 * `limit` the decode-call limit; `mode` bit 0 selects 24-bit output.
 * Returns 0, or -1 when no ring could be allocated. */
s32 movie_open(u16 width, u16 height, u16 scale, u16 slice, u16 sectors, u16 limit, u16 mode);
void movie_poll(void);  /* one step of playback */
void movie_stop(void);  /* stop the stream */
void movie_close(void); /* stop, then release the buffers and the ring */

extern s32 movie_split_display;  /* frames span both display buffers */
extern s32 movie_skipped_frames; /* ring frames that did not follow the one before */
extern s32 movie_load_restart;   /* whether the next frame's slices go to VRAM */

#endif
