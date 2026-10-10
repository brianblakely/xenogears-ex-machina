#ifndef FIELD_FIELD_MOVIE_H
#define FIELD_FIELD_MOVIE_H

/* The field movie player (800a7c58, field_screen.c) and the VRAM it
 * borrows: party sprite blocks 1 and 2 (8005a414) hold the 320x256 area at
 * (200, 0) while a movie plays. The movie's sound effects run on a timeline
 * per sound-effect bank (field_event.c). */

#include "common.h"
#include "resident/sound.h"
#include "mdec/player.h"

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

extern FieldMovieRequest field_movie_request;
#define FIELD_MOVIE field_movie_request

extern s32 field_movie_stopped;           /* movie stopped */
extern s32 field_movie_mode;              /* movie mode */
extern s32 field_movie_fade_bits;         /* the request's fade bits (mode & 0xc0) */
extern s32 field_movie_end_count;         /* nonzero ends a window movie (mode 2) */
extern s32 field_movie_presenting;        /* the player presents frames */
extern s32 field_movie_decoded_block;     /* the draw block of the decoded buffer */
extern s32 field_panorama_hidden;         /* the panorama is not drawn after the movie */
extern s32 field_movie_frame;             /* the current movie frame */
extern s32 field_movie_awaiting_frame;    /* cleared on each frame */
extern s32 field_staff_roll_drawing;      /* 1 while the text roll draws over frames 687..18e2 */

void field_movie_play(void);                          /* play the requested movie */
void field_screen_convert_24bit_to_15bit(s32 unused); /* convert the 24-bit screen to 15-bit pixels */

/* The 24-bit screen conversion reads a packed stream, four bytes a word. */
extern u32 *field_screen_convert_stream;        /* packed stream */
extern u32 field_screen_convert_word;           /* current word */
extern s32 field_screen_convert_nibble_count;   /* nibble counter */
extern u32 *field_screen_convert_pixels;        /* converted pixels */

/* The movie sound effects: a timeline of (time, sound) entries per bank. */
extern u16 field_movie_sound_timelines[][2];    /* movie sound timeline: time, sound */
extern s32 field_movie_sound_timeline_index;    /* movie sound timeline position */
extern SoundBank *field_movie_sound_bank;       /* movie sound-effect bank */
void field_movie_load_sound_bank(void);         /* load the request's bank, seek its timeline */
void field_movie_play_due_sounds(void);         /* play the effects whose time has come */
void field_movie_release_sound_bank(void);      /* release the bank */

#endif
