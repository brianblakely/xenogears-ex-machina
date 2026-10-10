#ifndef MOVIE_MODE_H
#define MOVIE_MODE_H

/* Mode 6, the movie mode: its display buffers and sector header, the resident
 * objects no shared header declares and the unit's own functions and
 * forward-declared data. It plays movies through the movie library
 * (mdec/player.h). */

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
void cd_set_mode(s32 mode); /* resident read mode */
void stream_start_image_load(s32 file, void *ring, s32 mode, s32 unused, s32 mode_1200, s32 base_x_1200, s32 base_y_1200, s32 mode_1201, s32 base_x_1201,
                   s32 base_y_1201); /* stream a file through a ring */
s16 sound_sync_transfer(s32 wait);  /* sound transfer busy */

/* The movie request bytes 8004fe44-8004fe47 (cd.h's cd_movie_request_kind to
 * cd_movie_request_unskippable): kind (bit 7: last frame from 80062514), index, the
 * next mode, and whether buttons do not end the movie. This unit indexes them as
 * one array, loading [0] through the array's address in a register, which the four
 * scalars do not reproduce. */
extern u8 cd_movie_request[4] __asm__("cd_movie_request_kind");

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
extern const RECT movie_mode_screen_area; /* the screen area */

s32 movie_mode_play_movie(void);
void movie_mode_on_frame_loaded(u16 frame, u16 x, u16 y);
void movie_mode_read_playback_input(void);
void movie_mode_aim_camera(void);

/* CD-ROM monitor. */
void movie_mode_start_cd_check_sector_read(void);
void movie_mode_poll_cd_check_input(void);
void movie_mode_start_cd_check_command(s32 command);
s32 movie_mode_next_random(void);

/* FAT check. */
void movie_mode_draw_backdrop(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h);
void movie_mode_draw_menu_frame(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h);
s32 movie_mode_read_menu_input(s32 first, s32 last, s32 *button);
void movie_mode_clear_vram(void);
u32 movie_mode_get_file_index_size(s32 index);
extern const char movie_mode_monitor_newline_text[]; /* "\n" */
extern const char movie_mode_push_circle_text[]; /* "\nPUSH CIRCLE BUTTON TO MENU." */

/* CD-ROM monitor screen. */
extern const char movie_mode_cd_check_file_data_format[], movie_mode_cd_check_cancel_format[], movie_mode_cd_check_class_format[], movie_mode_cd_check_now_label[], movie_mode_cd_check_before_label[];
extern const char movie_mode_cd_check_total_format[], movie_mode_push_start_text[];
void movie_mode_verify_cd_check_read(void);

/* Mode entry and menu. */
void movie_mode_init_backdrop(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h);
void movie_mode_init_menu_frame(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h);
s32 movie_mode_find_frame_sector(s32 frame);
s32 movie_mode_find_last_frame(void);
void movie_mode_run_cd_monitor(void);
void movie_mode_run_fat_check(void);
void movie_mode_run_movie_test(void);
s32 movie_mode_play_requested_movie(u8 keep);
void movie_mode_run_cd_check(void);
void movie_mode_run_disc_change_test(void);

/* Disc change test. */
extern const char movie_mode_newline_text[]; /* "\n", first used by the menu (800704E8) */
void movie_mode_stop_disc_read(void);
s32 movie_mode_step_disc_change_test(s32 disc, s32 state, s32 *error, s32 *done);

#endif
