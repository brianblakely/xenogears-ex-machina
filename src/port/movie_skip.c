/*
 * A host option that changes the game (docs/runtime.md, Movies can be
 * skipped): the original ends a movie early only where its request lets
 * it. A field movie whose request has fade bit 0x80 ends on Confirm
 * (field_movie_play, 800A7C58: the loop drains the pad, fades the CD
 * volume, waits five vertical blanks and closes the movie as at its end),
 * and the movie mode's (6) ends on Cross or Start unless its request kept
 * it (movie_mode_read_playback_input, 800769A4); the story's movies are
 * requested without the bit, or kept. Where the host lets movies be
 * skipped (xem_host_movie_skip nonzero), a field movie gains bit 0x80 as
 * its frames run (field_movie_run_frames, 800A732C) and the movie mode's
 * keep flag is cleared as it reads the pad, so the player's press takes
 * the original's own early exit. Window movies (field movie mode 2), which
 * the field's scripts end, stay as they are. Armed, a field movie's loop
 * drains the pad queue on each pass, so the host records its choice with
 * the game's inputs; 0 is the original's. The port keeps no data of its
 * own.
 */
#include "common.h"
#include "../decomp/src/field/field_movie.h"

/* Nonzero while the host lets movies be skipped. */
u32 xem_host_movie_skip(void);

/* The movie mode's request keeps its movie from the buttons (movie.c). */
extern s32 movie_mode_unskippable;

void xem_original_field_movie_run_frames(s32 frames);
void xem_original_movie_mode_read_playback_input(void);

void field_movie_run_frames(s32 frames) {
    if (field_movie_mode != 2 && xem_host_movie_skip() != 0) {
        field_movie_fade_bits |= 0x80;
    }
    xem_original_field_movie_run_frames(frames);
}

void movie_mode_read_playback_input(void) {
    if (xem_host_movie_skip() != 0) {
        movie_mode_unskippable = 0;
    }
    xem_original_movie_mode_read_playback_input();
}
