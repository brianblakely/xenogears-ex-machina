/* Mode 6, the movie mode (overlay image at 0x8006faf0, packed in disc 1 file
 * 40 / disc 2 file 35). The resident mode dispatcher loads it into the mode
 * overlay area and enters 800737ec: that loads the movie library (disc file
 * 19 at 0x801d3000), plays the movie that the request bytes 8004fe44..47
 * name and selects the next mode. Without a request it runs a development
 * menu: movie test settings, a CD-ROM monitor, a CD-ROM read check, a FAT
 * check and a disc change test, drawn with the resident debug font.
 *
 * Everything after the overlay number is this unit: rodata 8006FAF4-800704E8,
 * text 800704E8-80076E48, data 80076E48-80076F3C and the .bss to 80077458.
 * Its rodata starts at 4 mod 8, the phase of all six of its jump tables,
 * which is why the number is a unit of its own (movie_number.c). */
#include "common.h"
#include "movie_mode.h"

/* The unit's .data (0x80076e48..0x80076f3c); the .bss follows. */
s32 movie_mode_cd_check_command = 0;                     /* 80076E48: read check state: the command running */
s32 movie_mode_cd_check_error_count = 0;                 /* 80076E4C: verify errors */
s32 movie_mode_cd_check_unused_1 = 0;                    /* 80076E50: never read */
s32 movie_mode_cd_check_unused_2 = 0;                    /* 80076E54: never read */
s32 movie_mode_cd_check_unused_3 = 0;                    /* 80076E58: never read */
s32 movie_mode_vblank_count = 0;                         /* 80076E5C: vertical blanks counted */
s32 movie_mode_cd_check_unused_4 = 0;                    /* 80076E60: never read */
s32 movie_mode_cd_check_phase = 0;                       /* 80076E64: read phase: waiting, reading, verifying */
s32 movie_mode_cd_check_error_address = 0;               /* 80076E68: last error: offset */
s32 movie_mode_cd_check_error_size = 0;                  /* 80076E6C: size */
s32 movie_mode_cd_check_error_file_out_of_order = 0;     /* 80076E70: and the resident's stream counters */
s32 movie_mode_cd_check_error_list_out_of_order = 0;     /* 80076E74 */
s32 movie_mode_cd_check_error_stream_out_of_order = 0;   /* 80076E78 */
FileEntry *movie_mode_cd_check_file_table = NULL;        /* 80076E7C: stream list */
FileEntry *movie_mode_cd_check_verify_file_table = NULL; /* 80076E80: verify copy of the stream list */
s32 *movie_mode_cd_check_write_pointer = NULL;           /* 80076E84: stream destination */
s32 *movie_mode_cd_check_read_buffer = NULL;             /* 80076E88: read buffer */
s32 *movie_mode_cd_check_verify_buffer = NULL;           /* 80076E8C: verify copy of the read */
s32 movie_mode_cd_check_read_size = 0;                   /* 80076E90: read size */
s32 movie_mode_cd_check_stream_bytes_left = 0;           /* 80076E94: stream bytes left */
StreamRing *movie_mode_cd_check_stream_ring = NULL;      /* 80076E98: stream buffer */
s32 *movie_mode_cd_check_stream_chunk = NULL;            /* 80076E9C: arrived stream chunk */
void *movie_mode_cd_check_sector_buffer = NULL;          /* 80076EA0: FAT check read buffer */
s32 movie_mode_cd_check_cancel_count = 0;                /* 80076EA4: reads ended */
s32 movie_mode_cd_check_total_reads = 0;                 /* 80076EA8: reads in total */
s32 movie_mode_cd_check_before_class = -1;               /* 80076EAC: class marked " BEFORE" (-1: none) */
s32 movie_mode_cd_check_now_class = -1;                  /* 80076EB0: class marked " NOW" */
s32 movie_mode_cd_check_file_table_index = 0;            /* 80076EB4: stream list entry */
s32 movie_mode_cd_check_stream_mode = 0;                 /* 80076EB8: 1: stream copy; 2: host read */
s32 movie_mode_cd_check_random_mode = 0;                 /* 80076EBC: random commands */
s32 movie_mode_cd_check_unused_5 = 0;                    /* 80076EC0: never read */
s32 movie_mode_cd_check_unused_6 = 0;                    /* 80076EC4: never read */
s32 movie_mode_elapsed_seconds = 0;                      /* 80076EC8: seconds counted */
s32 movie_mode_frames_this_second = 0;                   /* 80076ECC: frames of the current second */
/* Never read: a string, three words and a record of 233, the string and the
 * record's own address. */
char movie_mode_unused_newline[] = "\n"; /* 80076ED0 */
s32 movie_mode_unused_numbers[3] = {20, 30, 40}; /* 80076ED4 */
struct {
    s32 value;
    char *text;
    void *self;
} movie_mode_unused_record = {233, movie_mode_unused_newline, &movie_mode_unused_record}; /* 80076EE0 */
s32 movie_mode_menu_hold_frames = 0;                                                      /* 80076EEC: menu: frames the buttons were held */
s32 movie_mode_menu_repeat_timer = 16;                                                    /* 80076EF0: frames until they repeat */
s32 movie_mode_random_state_a = 1234567890;                                               /* 80076EF4: random number state */
s32 movie_mode_random_state_b = 987654321;                                                /* 80076EF8 */
s32 movie_mode_cd_monitor_sector = 0;                                                     /* 80076EFC: monitor sector */
s32 movie_mode_cd_monitor_row = 0;                                                        /* 80076F00: monitor row */
s32 movie_mode_playback_hold_frames = 0;                                                  /* 80076F04: playback: frames the buttons were held */
s32 movie_mode_playback_repeat_timer = 90;                                                /* 80076F08: frames until they repeat */
/* Never read: the origin and three 128-long axes, beside the playback
 * camera's translation. */
SVECTOR movie_mode_unused_axes[4] = {{0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {0, 0, 128}}; /* 80076F0C */
VECTOR movie_mode_camera_offset = {0};                                                  /* 80076F2C: the playback camera's translation */

/* The unit's .bss (80076f3c-80077458), not in the file: the resident's mode
 * table clears it before entering the overlay (movie.bss.ld). The variables
 * are defined here in address order, the order of their first declaration,
 * in which GCC emits tentative definitions, each in a slot of whole words
 * (decomp/Makefile); the decoded image ends 7 bytes into the first. */
s32 movie_mode_cd_check_class_counts[16];      /* 80076F3C: reads per result class */
u8 *movie_mode_cd_check_host_frame_data;       /* 80076F7C: host stream: the next frame's data (80028f30) */
StreamFrame *movie_mode_cd_check_host_frame;   /* 80076F80: and its first sector header */
u8 movie_mode_cd_command_result[8];            /* 80076F84: CD command result */
/* Menu backdrop: each corner's color fades from one random color to the
 * next over a random number of frames. */
CVECTOR movie_mode_backdrop_from_colors[4];   /* 80076F8C: from */
CVECTOR movie_mode_backdrop_to_colors[4];     /* 80076F9C: to */
s32 movie_mode_backdrop_fade_elapsed[4];      /* 80076FAC: frames into the fade */
s32 movie_mode_backdrop_fade_durations[4];    /* 80076FBC: frames of the fade */
CVECTOR movie_mode_menu_frame_from_colors[4]; /* 80076FCC: menu frame: from */
CVECTOR movie_mode_menu_frame_to_colors[4];   /* 80076FDC: to */
s32 movie_mode_menu_frame_fade_elapsed[4];    /* 80076FEC: frames into the fade */
s32 movie_mode_menu_frame_fade_durations[4];  /* 80076FFC: frames of the fade */
SoundSeq *movie_mode_battle_music_seq;        /* 8007700C: battle music sequence (8007548c) */
/* Movie playback. */
s32 movie_mode_unread_loaded_frame;    /* 80077010: last frame the library loaded */
s32 movie_mode_stop_timer;             /* 80077014: 1: stop; 2..5: frames until then */
s32 movie_mode_loaded_buffer;          /* 80077018: buffer the frame went to */
s32 movie_mode_shown_buffer;           /* 8007701C: buffer on display */
s32 movie_mode_decode_paused;          /* 80077020: decoding paused */
s32 movie_mode_first_buffer_y;         /* 80077024: the first buffer's y */
s32 movie_mode_unskippable;            /* 80077028: buttons do not end the movie */
/* The playback camera (unused by the movie path). */
VECTOR movie_mode_camera_eye;                 /* 8007702C: eye */
VECTOR movie_mode_camera_target;              /* 8007703C: target */
s32 movie_mode_camera_roll;                   /* 8007704C: roll */
MATRIX movie_mode_world_to_screen;            /* 80077050: world to screen */
MATRIX movie_mode_light_colors;               /* 80077070: light colors */
MATRIX movie_mode_unread_light_directions;    /* 80077090: light directions */
SVECTOR movie_mode_camera_angles;             /* 800770B0: camera rotation */
MATRIX movie_mode_camera_translation;         /* 800770B8: camera translation */
MATRIX movie_mode_camera_rotation;            /* 800770D8: camera rotation */
MATRIX movie_mode_camera_base_rotation;       /* 800770F8: base rotation (identity) */
s32 movie_mode_menu_cursor;                   /* 80077118: menu cursor */
s32 movie_mode_movie_index;                   /* 8007711C: movie index */
MovieBuffer *movie_mode_current_buffer;       /* 80077120: buffer being drawn */
MovieBuffer movie_mode_buffers[2];            /* 80077124 */
s32 movie_mode_heap_report_shown;             /* 80077394: statistics shown */
s32 movie_mode_xa_channel;                    /* 80077398: XA channel */
s32 movie_mode_last_frame;                    /* 8007739C: last frame */
s32 movie_mode_draw_rows;                     /* 800773A0: rows */
s32 movie_mode_first_frame;                   /* 800773A4: first frame */
s32 movie_mode_start_sector;                  /* 800773A8: start sector */
s32 movie_mode_buttons;                       /* 800773AC: buttons */
s32 movie_mode_monitor_shown;                 /* 800773B0: monitor shown */
s32 movie_mode_previous_buttons;              /* 800773B4: previous buttons */
s32 movie_mode_unread_decode_vsync_times[32]; /* 800773B8: VSync(1) before and after each decode step */
s32 movie_mode_rewind_enabled;                /* 80077438: the menu's REWIND: movie_start's hold (restart at the end) */
s32 movie_mode_end_frame_state;               /* 8007743C: end frame: 0 changed, 1 found, 2 not found */
s32 movie_mode_unread_menu_shown;             /* 80077440: menu shown */
s32 movie_mode_start_frame_state;             /* 80077444: start frame: 1 changed, 2 sought */
s32 movie_mode_movie_kind;                    /* 80077448: movie kind */
s32 movie_mode_buffer_index;                  /* 8007744C: buffer index */
s32 movie_mode_disc_mode;                     /* 80077450: disc mode: 0, -1 or host */
s32 movie_mode_library_output_mode;           /* 80077454: library output mode (bit 0: 24-bit) */

/* 800704E8: The menu's CD-ROM check (line 10): at 640x240, show the read statistics, the
 * resident's error counters and stream state, a dump of the stream buffer
 * and the reads per result class, run the monitor's input every frame and
 * return to the 320-wide menu on Start once no read is running. The unused
 * locals (mode included) reproduce the original's frame; the original also
 * reads the menu cursor before the exit test and stores it back after. */
void movie_mode_run_cd_check(void) {
    u8 unused[8];
    char mode[3] = {0, 2, 2};
    u8 unused2[0x18];
    s32 button;
    s32 i;
    s32 hours;
    s32 cursor;
    MovieBuffer *buffer;
    u_long *ot;

    stream_select_ring(NULL);
    movie_mode_clear_vram();
    movie_mode_cd_check_sector_buffer = NULL;
    movie_mode_cd_check_stream_mode = 1;
    for (i = 15; i >= 0; i--) {
        movie_mode_cd_check_class_counts[i] = 0;
    }
    SetDefDrawEnv(&movie_mode_buffers[0].draw, 0, 0, 640, 240);
    SetDefDispEnv(&movie_mode_buffers[0].disp, 0, 240, 640, 240);
    SetDefDrawEnv(&movie_mode_buffers[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&movie_mode_buffers[1].disp, 0, 0, 640, 240);
    movie_mode_buffers[0].draw.isbg = 1;
    movie_mode_buffers[1].draw.isbg = 1;
    cd_select_directory(0xC, 3);
    for (;;) {
        if (movie_mode_current_buffer == &movie_mode_buffers[0]) {
            buffer = &movie_mode_buffers[1];
        } else {
            buffer = &movie_mode_buffers[0];
        }
        ot = buffer->ot;
        movie_mode_current_buffer = buffer;
        movie_mode_buffer_index = 1 - movie_mode_buffer_index;
        ClearOTagR(ot, 32);
        movie_mode_verify_cd_check_read();
        movie_mode_read_menu_input(0, 0, &button);
        movie_mode_poll_cd_check_input();
        if (movie_mode_cd_check_random_mode != 0) {
            console_printf("Random Mode\n");
        }
        if (movie_mode_cd_check_stream_mode == 0) {
            console_printf("Stream Pause\n");
        }
        console_printf("Read %3d Error %3d VSync %8d EC %2d ST %2d ", movie_mode_cd_check_command, movie_mode_cd_check_error_count,
                      movie_mode_vblank_count, cd_error_count, cd_command_state);
        switch (movie_mode_cd_check_phase) {
        case 0:
            console_printf("Waiting\n");
            break;
        case 1:
            console_printf("Reading\n");
            break;
        case 2:
            console_printf("Verifing\n");
            break;
        }
        console_printf("C1 %3d C2 %3d C3 %3d C4 %3d C5 %3d C6 %3d C7 %3d C8 %3d C9 %3d\n",
                      cd_stat_setloc_count, cd_stat_command_ok_count, cd_stat_command_fail_count, cd_stat_retry_setloc_count, cd_stat_retry_fail_count, cd_stat_lesmem_count,
                      cd_stat_error_limit_count, cd_stat_stop_ok_count, cd_stat_stop_fail_count);
        console_printf("RestFile %7d RestSize %7d N1 %4d N2 %4d N3 %4d R%3d D%3d\n",
                      cd_get_pending_read_count(), cd_get_read_bytes_left(), cd_file_out_of_order_count, cd_list_out_of_order_count, cd_stream_out_of_order_count,
                      stream_next_store_sequence, stream_next_complete_sequence);
        console_printf("ErrorAddress %8x ErrorSize %8x N%3d N%3d N%3d\n", movie_mode_cd_check_error_address, movie_mode_cd_check_error_size,
                      movie_mode_cd_check_error_file_out_of_order, movie_mode_cd_check_error_list_out_of_order, movie_mode_cd_check_error_stream_out_of_order);
        console_printf("FrdPtr1 %8x FrdPtr2 %8x Buf1 %8x Buf2 %8x\n", movie_mode_cd_check_file_table, movie_mode_cd_check_verify_file_table,
                      movie_mode_cd_check_read_buffer, movie_mode_cd_check_verify_buffer);
        if (movie_mode_cd_check_command >= 11 || movie_mode_cd_check_stream_bytes_left > 0) {
            console_printf("S %8x Adrs %8x Write %8x Rest %8x\n", movie_mode_cd_check_stream_ring, movie_mode_cd_check_stream_chunk,
                          movie_mode_cd_check_write_pointer, movie_mode_cd_check_stream_bytes_left);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)movie_mode_cd_check_stream_ring)[i]);
            }
            console_printf((char *)movie_mode_newline_text);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)movie_mode_cd_check_stream_ring)[i + 7]);
            }
            console_printf((char *)movie_mode_newline_text);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)movie_mode_cd_check_stream_ring)[i + 14]);
            }
            console_printf((char *)movie_mode_newline_text);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)movie_mode_cd_check_stream_ring)[i + 21]);
            }
            console_printf((char *)movie_mode_newline_text);
            if (movie_mode_cd_check_file_table != NULL) {
                console_printf((char *)movie_mode_cd_check_file_data_format, movie_mode_cd_check_file_table[0].data, movie_mode_cd_check_file_table[1].data,
                              movie_mode_cd_check_file_table[2].data, movie_mode_cd_check_file_table[3].data);
            }
        }
        movie_mode_cd_check_total_reads = 0;
        console_printf((char *)movie_mode_cd_check_cancel_format, movie_mode_cd_check_cancel_count);
        for (i = 0; i < 13; i++) {
            console_printf((char *)movie_mode_cd_check_class_format, i, movie_mode_cd_check_class_counts[i]);
            movie_mode_cd_check_total_reads += movie_mode_cd_check_class_counts[i];
            if (movie_mode_cd_check_now_class == i) {
                console_printf((char *)movie_mode_cd_check_now_label);
            }
            if (movie_mode_cd_check_before_class == i) {
                console_printf((char *)movie_mode_cd_check_before_label);
            }
            console_printf((char *)movie_mode_newline_text);
        }
        hours = movie_mode_elapsed_seconds / 3600;
        console_printf((char *)movie_mode_cd_check_total_format, movie_mode_cd_check_total_reads, hours, movie_mode_elapsed_seconds / 60 - hours * 60,
                      movie_mode_elapsed_seconds % 60);
        console_printf((char *)movie_mode_push_start_text);
        if (movie_mode_heap_report_shown > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(movie_mode_current_buffer->ot);
        movie_mode_draw_backdrop(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->box, 8, 12, 624, 216);
        movie_mode_draw_menu_frame(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->frame, 7, 11, 626, 218);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&movie_mode_current_buffer->draw);
        PutDispEnv(&movie_mode_current_buffer->disp);
        DrawOTag(&movie_mode_current_buffer->ot[31]);
        cursor = movie_mode_menu_cursor;
        if (!(movie_mode_previous_buttons & 0x800) && (movie_mode_buttons & 0x800) && movie_mode_cd_check_command == 0) {
            movie_mode_heap_report_shown = 0;
            cd_stop_read(0);
            cd_sync_reads(0);
            if (movie_mode_cd_check_sector_buffer != NULL) {
                heap_free(movie_mode_cd_check_sector_buffer);
            }
            movie_mode_cd_check_sector_buffer = NULL;
            if (movie_mode_cd_check_stream_ring != NULL) {
                heap_free(movie_mode_cd_check_stream_ring);
            }
            movie_mode_cd_check_stream_ring = NULL;
            movie_mode_clear_vram();
            SetDefDrawEnv(&movie_mode_buffers[0].draw, 0, 0, 320, 240);
            SetDefDispEnv(&movie_mode_buffers[0].disp, 0, 240, 320, 240);
            SetDefDrawEnv(&movie_mode_buffers[1].draw, 0, 240, 320, 240);
            SetDefDispEnv(&movie_mode_buffers[1].disp, 0, 0, 320, 240);
            movie_mode_buffers[0].disp.screen.x = 0;
            movie_mode_buffers[0].disp.screen.y = 10;
            movie_mode_buffers[0].disp.screen.w = 256;
            movie_mode_buffers[0].disp.screen.h = 216;
            movie_mode_buffers[1].disp.screen.x = 0;
            movie_mode_buffers[1].disp.screen.y = 10;
            movie_mode_buffers[1].disp.screen.w = 256;
            movie_mode_buffers[1].disp.screen.h = 216;
            movie_mode_buffers[0].draw.isbg = 1;
            movie_mode_buffers[1].draw.isbg = 1;
            return;
        }
        movie_mode_menu_cursor = cursor;
    }
}

/* 80070DCC: CD-ROM monitor input: newly pressed buttons issue the monitor's CD
 * commands or read steps; R1 toggles the stream copy (1) or host read (2),
 * Select random commands. A running stream copies each arrived chunk
 * into the destination and moves to the next list entry when it ends. */
void movie_mode_poll_cd_check_input(void) {
    s32 i;
    s32 command;

    if (!(movie_mode_previous_buttons & 0x20) && (movie_mode_buttons & 0x20)) {
        movie_mode_start_cd_check_command(1);
    }
    if (!(movie_mode_previous_buttons & 0x10) && (movie_mode_buttons & 0x10)) {
        movie_mode_start_cd_check_command(2);
    }
    if (!(movie_mode_previous_buttons & 0x80) && (movie_mode_buttons & 0x80)) {
        movie_mode_start_cd_check_command(3);
    }
    if (!(movie_mode_previous_buttons & 0x40) && (movie_mode_buttons & 0x40)) {
        movie_mode_cd_check_cancel_count++;
        cd_stop_read(0);
    }
    if (!(movie_mode_previous_buttons & 0x1000) && (movie_mode_buttons & 0x1000)) {
        movie_mode_start_cd_check_command(7);
    }
    if (!(movie_mode_previous_buttons & 0x4000) && (movie_mode_buttons & 0x4000)) {
        movie_mode_start_cd_check_command(9);
    }
    if (!(movie_mode_previous_buttons & 8) && (movie_mode_buttons & 8)) {
        movie_mode_cd_check_stream_mode = 1 - movie_mode_cd_check_stream_mode;
    }
    if (!(movie_mode_previous_buttons & 4) && (movie_mode_buttons & 4) && movie_mode_cd_check_command < 11) {
        movie_mode_start_cd_check_sector_read();
    }
    if (movie_mode_cd_check_stream_mode == 1) {
        movie_mode_cd_check_stream_chunk = (s32 *)stream_get_next_chunk();
        if (movie_mode_cd_check_stream_chunk != NULL) {
            i = 0;
            if (movie_mode_cd_check_stream_bytes_left > 0x800) {
                do {
                    *movie_mode_cd_check_write_pointer++ = movie_mode_cd_check_stream_chunk[i++];
                } while (i < 0x200);
            } else {
                while (i < movie_mode_cd_check_stream_bytes_left / 4) {
                    *movie_mode_cd_check_write_pointer++ = movie_mode_cd_check_stream_chunk[i++];
                }
            }
            movie_mode_cd_check_stream_bytes_left -= 0x800;
            if (movie_mode_cd_check_stream_bytes_left <= 0 && movie_mode_cd_check_command == 12) {
                i = movie_mode_cd_check_file_table[++movie_mode_cd_check_file_table_index].id;
                if (i != 0) {
                    movie_mode_cd_check_stream_bytes_left = cd_get_aligned_file_size(i);
                }
                movie_mode_cd_check_write_pointer = movie_mode_cd_check_file_table[movie_mode_cd_check_file_table_index].data;
            }
            stream_release_chunk((u8 *)movie_mode_cd_check_stream_chunk);
        }
    }
    if (movie_mode_cd_check_stream_mode == 2 && stream_get_next_movie_frame(&movie_mode_cd_check_host_frame_data, &movie_mode_cd_check_host_frame) == 0) {
        stream_release_movie_frame((u8 *)movie_mode_cd_check_host_frame);
    }
    if (!(movie_mode_previous_buttons & 0x2000) && (movie_mode_buttons & 0x2000)) {
        movie_mode_start_cd_check_command(11);
    }
    if (!(movie_mode_previous_buttons & 0x8000) && (movie_mode_buttons & 0x8000)) {
        movie_mode_start_cd_check_command(13);
    }
    if (movie_mode_cd_check_random_mode != 0) {
        command = movie_mode_next_random() & 0xFF;
        if (command == 0 && movie_mode_cd_check_command < 11) {
            movie_mode_start_cd_check_sector_read();
        }
        if ((u32)(command - 1) < 12) {
            movie_mode_start_cd_check_command(command);
        }
    }
    if (!(movie_mode_previous_buttons & 0x100) && (movie_mode_buttons & 0x100)) {
        movie_mode_cd_check_random_mode = 1 - movie_mode_cd_check_random_mode;
    }
}

/* movie_mode_run_cd_check's other strings, after its literals. The unit's literal "\n"
 * is movie_mode_main's, so this "\n" is an array, and the strings after it
 * are too. */
const char movie_mode_newline_text[] = "\n"; /* 8006FC6C */
const char movie_mode_cd_check_file_data_format[] = "%08x %08x %08x %08x %08x\n"; /* 8006FC70 */
const char movie_mode_cd_check_cancel_format[] = "Cancel%8d\n"; /* 8006FC8C */
const char movie_mode_cd_check_class_format[] = "CT%1x   %8d"; /* 8006FC98 */
const char movie_mode_cd_check_now_label[] = " NOW"; /* 8006FCA4 */
const char movie_mode_cd_check_before_label[] = " BEFORE"; /* 8006FCAC */
const char movie_mode_cd_check_total_format[] = "\nTOTAL %8d : Time %3d:%02d:%02d\n"; /* 8006FCB4 */
const char movie_mode_push_start_text[] = "\nPUSH START BUTTON TO MENU."; /* 8006FCD8 */

/* Fill `size` bytes of `words` with `value`, counting in `n`. */
#define FILL_WORDS(words, size, n, value)  \
    for (n = 0; n < (size) / 4; n++) {    \
        (words)[n] = (value);              \
    }

/* Record a verify mismatch at byte `offset` of a `size`-byte read: the
 * first one keeps its place and the resident's stream counters. */
#define VERIFY_ERROR(offset, size)         \
    {                                      \
        if (movie_mode_cd_check_error_count == 0) {             \
            movie_mode_cd_check_error_address = (offset);         \
            movie_mode_cd_check_error_size = (size);           \
            movie_mode_cd_check_error_file_out_of_order = cd_file_out_of_order_count;       \
            movie_mode_cd_check_error_list_out_of_order = cd_list_out_of_order_count;       \
            movie_mode_cd_check_error_stream_out_of_order = cd_stream_out_of_order_count;       \
        }                                  \
        movie_mode_cd_check_error_count++;                      \
    }

/* 800712C4: Monitor read check, run every frame: once the command's read (phase 1)
 * ends, read the same data again into a second buffer or file list filled
 * with -1 (phase 2); once that ends, compare both copies word by word,
 * record the first mismatch and release the copies. */
void movie_mode_verify_cd_check_read(void) {
    s32 i;
    s32 *dest;
    s32 index;
    s32 file;
    s32 *buffer;
    s32 *copy;

    if (movie_mode_cd_check_phase == 0) {
        return;
    }
    if (cd_get_pending_read_count() == 0 && movie_mode_cd_check_phase == 1) {
        movie_mode_cd_check_phase = 2;
        switch (movie_mode_cd_check_command) {
        case 1:
            movie_mode_cd_check_read_size = 0x2000;
            movie_mode_cd_check_verify_buffer = buffer = heap_alloc(0x2000, 0);
            FILL_WORDS(buffer, movie_mode_cd_check_read_size, i, -1);
            cd_read_raw_sectors(0x40, movie_mode_cd_check_verify_buffer, movie_mode_cd_check_read_size, 0, 0);
            break;
        case 2:
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(7);
            movie_mode_cd_check_verify_buffer = buffer = heap_alloc(movie_mode_cd_check_read_size, 0);
            FILL_WORDS(buffer, movie_mode_cd_check_read_size, i, -1);
            cd_read_file(7, movie_mode_cd_check_verify_buffer, 0, 0);
            break;
        case 3:
        case 12:
            movie_mode_cd_check_verify_file_table = cd_alloc_directory_file_table(2, 0);
            if (movie_mode_cd_check_verify_file_table == NULL) {
                movie_mode_cd_check_phase = 0;
                movie_mode_cd_check_command = 0;
                break;
            }
            for (index = 0; (file = movie_mode_cd_check_verify_file_table[index].id) > 0; index++) {
                copy = movie_mode_cd_check_verify_file_table[index].data;
                movie_mode_cd_check_read_size = cd_get_aligned_file_size(file);
                FILL_WORDS(copy, movie_mode_cd_check_read_size, i, -1);
            }
            cd_read_file_list((FileRequest *)movie_mode_cd_check_verify_file_table, 0, 0);
            break;
        case 4:
            movie_mode_cd_check_read_size = 0x2000;
            movie_mode_cd_check_verify_buffer = buffer = heap_alloc(0x2000, 0);
            FILL_WORDS(buffer, movie_mode_cd_check_read_size, i, -1);
            cd_read_raw_sectors(0x40, movie_mode_cd_check_verify_buffer, movie_mode_cd_check_read_size, 0, 0);
            break;
        case 5:
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(7);
            movie_mode_cd_check_verify_buffer = buffer = heap_alloc(movie_mode_cd_check_read_size, 0);
            FILL_WORDS(buffer, movie_mode_cd_check_read_size, i, -1);
            cd_read_file(7, movie_mode_cd_check_verify_buffer, 1, 0);
            break;
        case 6:
            movie_mode_cd_check_verify_file_table = cd_alloc_directory_file_table(2, 0);
            if (movie_mode_cd_check_verify_file_table == NULL) {
                movie_mode_cd_check_phase = 0;
                movie_mode_cd_check_command = 0;
                break;
            }
            for (index = 0; (file = movie_mode_cd_check_verify_file_table[index].id) > 0; index++) {
                copy = movie_mode_cd_check_verify_file_table[index].data;
                movie_mode_cd_check_read_size = cd_get_aligned_file_size(file);
                FILL_WORDS(copy, movie_mode_cd_check_read_size, i, -1);
            }
            cd_read_file_list((FileRequest *)movie_mode_cd_check_verify_file_table, 1, 0);
            break;
        case 7:
        case 8:
            movie_mode_cd_check_phase = 0;
            movie_mode_cd_check_command = 0;
            break;
        case 11:
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(6);
            movie_mode_cd_check_verify_buffer = buffer = heap_alloc(movie_mode_cd_check_read_size, 0);
            FILL_WORDS(buffer, movie_mode_cd_check_read_size, i, -1);
            cd_read_file(6, movie_mode_cd_check_verify_buffer, 0, 0);
            break;
        }
    }
    if (cd_get_pending_read_count() == 0 && movie_mode_cd_check_phase == 2) {
        movie_mode_cd_check_phase = 0;
        switch (movie_mode_cd_check_command) {
        case 1:
        case 4:
            for (i = 0; i < 0x800; i++) {
                if (movie_mode_cd_check_read_buffer[i] != movie_mode_cd_check_verify_buffer[i]) {
                    VERIFY_ERROR(i * 4, 0x2000);
                    break;
                }
            }
            heap_free(movie_mode_cd_check_read_buffer);
            heap_free(movie_mode_cd_check_verify_buffer);
            movie_mode_cd_check_phase = 0;
            break;
        case 2:
        case 5:
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(7);
            for (i = 0; i < movie_mode_cd_check_read_size / 4; i++) {
                if (movie_mode_cd_check_read_buffer[i] != movie_mode_cd_check_verify_buffer[i]) {
                    VERIFY_ERROR(i * 4, cd_get_aligned_file_size(7));
                    break;
                }
            }
            heap_free(movie_mode_cd_check_read_buffer);
            heap_free(movie_mode_cd_check_verify_buffer);
            movie_mode_cd_check_phase = 0;
            break;
        case 3:
        case 6:
        case 12:
            index = 0;
            file = movie_mode_cd_check_file_table[0].id;
            if (file > 0) {
                do {
                    copy = movie_mode_cd_check_verify_file_table[index].data;
                    dest = movie_mode_cd_check_file_table[index].data;
                    movie_mode_cd_check_read_size = cd_get_aligned_file_size(file);
                    for (i = 0; i < movie_mode_cd_check_read_size / 4; i++) {
                        if (dest[i] != copy[i]) {
                            VERIFY_ERROR(i * 4, cd_get_aligned_file_size(file));
                            break;
                        }
                    }
                } while ((file = movie_mode_cd_check_file_table[++index].id) > 0);
            }
            cd_free_file_table(movie_mode_cd_check_file_table);
            cd_free_file_table(movie_mode_cd_check_verify_file_table);
            heap_free(movie_mode_cd_check_file_table);
            heap_free(movie_mode_cd_check_verify_file_table);
            movie_mode_cd_check_phase = 0;
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            movie_mode_cd_check_phase = 0;
            break;
        case 11:
            if (movie_mode_cd_check_stream_bytes_left > 0) {
                movie_mode_cd_check_phase = 2;
            } else {
                movie_mode_cd_check_read_size = cd_get_aligned_file_size(6);
                for (i = 0; i < movie_mode_cd_check_read_size / 4; i++) {
                    if (movie_mode_cd_check_read_buffer[i] != movie_mode_cd_check_verify_buffer[i]) {
                        VERIFY_ERROR(i * 4, cd_get_aligned_file_size(6));
                        break;
                    }
                }
                heap_free(movie_mode_cd_check_read_buffer);
                heap_free(movie_mode_cd_check_verify_buffer);
            }
            movie_mode_cd_check_phase = 0;
            break;
        }
        movie_mode_cd_check_command = 0;
    }
}

/* 80071BA0: FAT check step: read file 40h into the check buffer (allocated once) and
 * count the read; a pass without errors keeps its tally. */
void movie_mode_start_cd_check_sector_read(void) {
    s32 tally;

    if (movie_mode_cd_check_sector_buffer == NULL) {
        movie_mode_cd_check_sector_buffer = heap_alloc(0x2000, 0);
    }
    if (movie_mode_cd_check_sector_buffer != NULL) {
        cd_read_raw_sectors(0x40, movie_mode_cd_check_sector_buffer, 0x2000, 0, 0);
    }
    movie_mode_cd_check_class_counts[0]++;
    if (movie_mode_cd_check_error_count == 0) {
        tally = movie_mode_cd_check_now_class;
        movie_mode_cd_check_now_class = 0;
        movie_mode_cd_check_before_class = tally;
    }
}

/* Clear `size` bytes of words at `dest`, counting in `n`. */
#define ZERO_WORDS(dest, size, n)          \
    {                                      \
        s32 *word;                         \
        n = 0;                             \
        word = (dest);                     \
        for (; n < (size) / 4; n++) {      \
            *word++ = 0;                   \
        }                                  \
    }

/* 80071C34: Start monitor command `command` when no read is running: 1/4 read file
 * 40h into a fresh 8 KB buffer, 2/5 file 7 through the host-file stream,
 * 3/6 the directory's file list, 7/8 the stream ring, 11 stream file 6 and
 * 12 the file list into the ring, 13 file 3 into a new 64-block ring. Each
 * destination is cleared first; the command counts in its class. */
void movie_mode_start_cd_check_command(s32 command) {
    s32 i;
    s32 index;
    s32 file;
    s32 *dest;

    if (movie_mode_cd_check_command != 0 || movie_mode_cd_check_phase != 0) {
        return;
    }
    cd_select_directory(0xC, 3);
    movie_mode_cd_check_phase = 1;
    movie_mode_cd_check_command = command;
    switch (command) {
    case 1:
        movie_mode_cd_check_read_size = 0x2000;
        movie_mode_cd_check_class_counts[1]++;
        movie_mode_cd_check_read_buffer = heap_alloc(0x2000, 0);
        ZERO_WORDS(movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, i);
        cd_read_raw_sectors(0x40, movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, 0, 0);
        break;
    case 2:
        movie_mode_cd_check_class_counts[2]++;
        movie_mode_cd_check_read_size = cd_get_aligned_file_size(7);
        movie_mode_cd_check_read_buffer = heap_alloc(movie_mode_cd_check_read_size, 0);
        ZERO_WORDS(movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, i);
        cd_read_file(7, movie_mode_cd_check_read_buffer, 0, 0);
        break;
    case 3:
        movie_mode_cd_check_class_counts[3]++;
        movie_mode_cd_check_file_table = cd_alloc_directory_file_table(2, 0);
        if (movie_mode_cd_check_file_table == NULL) {
            movie_mode_cd_check_phase = 0;
            movie_mode_cd_check_command = 0;
            break;
        }
        for (index = 0; (file = movie_mode_cd_check_file_table[index].id) > 0; index++) {
            dest = movie_mode_cd_check_file_table[index].data;
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(file);
            ZERO_WORDS(dest, movie_mode_cd_check_read_size, i);
        }
        cd_read_file_list((FileRequest *)movie_mode_cd_check_file_table, 0, 0);
        break;
    case 4:
        movie_mode_cd_check_read_size = 0x2000;
        movie_mode_cd_check_class_counts[4]++;
        movie_mode_cd_check_read_buffer = heap_alloc(0x2000, 0);
        ZERO_WORDS(movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, i);
        cd_read_raw_sectors(0x40, movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, 0, 0);
        break;
    case 5:
        movie_mode_cd_check_class_counts[5]++;
        movie_mode_cd_check_read_size = cd_get_aligned_file_size(7);
        movie_mode_cd_check_read_buffer = heap_alloc(movie_mode_cd_check_read_size, 0);
        ZERO_WORDS(movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, i);
        cd_read_file(7, movie_mode_cd_check_read_buffer, 1, 0);
        break;
    case 6:
        movie_mode_cd_check_class_counts[6]++;
        movie_mode_cd_check_file_table = cd_alloc_directory_file_table(2, 0);
        if (movie_mode_cd_check_file_table == NULL) {
            movie_mode_cd_check_phase = 0;
            movie_mode_cd_check_command = 0;
            break;
        }
        for (index = 0; (file = movie_mode_cd_check_file_table[index].id) > 0; index++) {
            dest = movie_mode_cd_check_file_table[index].data;
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(file);
            ZERO_WORDS(dest, movie_mode_cd_check_read_size, i);
        }
        cd_read_file_list((FileRequest *)movie_mode_cd_check_file_table, 1, 0);
        break;
    case 7:
        movie_mode_cd_check_class_counts[7]++;
        if (movie_mode_cd_check_stream_ring == NULL) {
            movie_mode_cd_check_stream_ring = stream_create_ring(4, 0);
        }
        stream_start_image_load(1, movie_mode_cd_check_stream_ring, 0, 0, 1, 0, 0, 0, 0, 0);
        break;
    case 8:
        movie_mode_cd_check_class_counts[8]++;
        if (movie_mode_cd_check_stream_ring == NULL) {
            movie_mode_cd_check_stream_ring = stream_create_ring(4, 0);
        }
        stream_start_image_load(1, movie_mode_cd_check_stream_ring, 1, 0, 1, 0, 0, 0, 0, 0);
        break;
    case 11:
        if (movie_mode_cd_check_stream_mode == 2) {
            movie_mode_cd_check_stream_mode = 1;
        }
        movie_mode_cd_check_class_counts[11]++;
        if (movie_mode_cd_check_stream_ring == NULL) {
            movie_mode_cd_check_stream_ring = stream_create_ring(4, 0);
        }
        movie_mode_cd_check_stream_bytes_left = movie_mode_cd_check_read_size = cd_get_aligned_file_size(6);
        movie_mode_cd_check_write_pointer = movie_mode_cd_check_read_buffer = heap_alloc(movie_mode_cd_check_read_size, 0);
        ZERO_WORDS(movie_mode_cd_check_read_buffer, movie_mode_cd_check_read_size, i);
        cd_read_file(6, movie_mode_cd_check_stream_ring, 1, 0x100);
        break;
    case 12:
        if (movie_mode_cd_check_stream_mode == 2) {
            movie_mode_cd_check_stream_mode = 1;
        }
        movie_mode_cd_check_class_counts[12]++;
        if (movie_mode_cd_check_stream_ring == NULL) {
            movie_mode_cd_check_stream_ring = stream_create_ring(4, 0);
        }
        movie_mode_cd_check_file_table = cd_alloc_directory_file_table(2, 0);
        if (movie_mode_cd_check_file_table == NULL) {
            movie_mode_cd_check_phase = 0;
            movie_mode_cd_check_command = 0;
            break;
        }
        file = movie_mode_cd_check_file_table[0].id;
        movie_mode_cd_check_stream_bytes_left = cd_get_aligned_file_size(file);
        movie_mode_cd_check_file_table_index = 0;
        movie_mode_cd_check_write_pointer = movie_mode_cd_check_file_table[0].data;
        for (index = 0; file > 0; file = movie_mode_cd_check_file_table[++index].id) {
            dest = movie_mode_cd_check_file_table[index].data;
            movie_mode_cd_check_read_size = cd_get_aligned_file_size(file);
            ZERO_WORDS(dest, movie_mode_cd_check_read_size, i);
        }
        stream_select_ring(movie_mode_cd_check_stream_ring);
        cd_read_file_list((FileRequest *)movie_mode_cd_check_file_table, 1, 0x100);
        break;
    case 13:
        movie_mode_cd_check_stream_mode = 2;
        movie_mode_cd_check_class_counts[13]++;
        if (movie_mode_cd_check_stream_ring != NULL) {
            heap_free(movie_mode_cd_check_stream_ring);
        }
        movie_mode_cd_check_stream_ring = stream_create_ring(0x40, 0);
        cd_select_directory(0x18, 0);
        cd_read_file(3, movie_mode_cd_check_stream_ring, 0, 0x200);
        break;
    }
    if (movie_mode_cd_check_error_count == 0) {
        movie_mode_cd_check_before_class = movie_mode_cd_check_now_class;
        movie_mode_cd_check_now_class = movie_mode_cd_check_command;
    }
}

/* 80072428: Vertical-blank tick: count frames and whole seconds. */
void movie_mode_count_vblank(void) {
    movie_mode_vblank_count++;
    if (++movie_mode_frames_this_second >= 60) {
        movie_mode_frames_this_second = 0;
        movie_mode_elapsed_seconds++;
    }
}

/* 80072480: The menu's disc change test: show the test state (the steps reached, the
 * error and the last CD command result); Start begins a test from a stopped
 * or failed state, Cross steps a waiting one, and each frame runs one step
 * of it for the other disc. Circle returns to the menu. */
void movie_mode_run_disc_change_test(void) {
    s32 button;
    s32 error;
    s32 done;
    s32 step;
    s32 state;
    s32 frames;
    MovieBuffer *buffer;
    u_long *ot;

    frames = 0;
    state = 0;
    error = 0;
    done = 1;
    for (;;) {
        if (movie_mode_current_buffer == &movie_mode_buffers[0]) {
            buffer = &movie_mode_buffers[1];
        } else {
            buffer = &movie_mode_buffers[0];
        }
        ot = buffer->ot;
        movie_mode_current_buffer = buffer;
        movie_mode_buffer_index = 1 - movie_mode_buffer_index;
        ClearOTagR(ot, 32);
        console_printf("\n[ DISC CHANGE TEST NOW DISC %2d ]\n\n", cd_get_disc_number());
        movie_mode_read_menu_input(0, 0, &button);
        console_printf("  STATUS ");
        if (error == 1) {
            console_printf("[ IT IS NOT PLAY STATION DISC ]\n");
        } else if (error == 2) {
            console_printf("[ NOT XENOGEARS DISC ]\n");
        } else if (error == 3) {
            console_printf("[ NO CHANGE DISC ]\n");
        } else if (error == 4) {
            console_printf("[ RETRY SET DISC ]\n");
        } else {
            console_printf("[ NOP ]\n");
        }
        console_printf((char *)movie_mode_newline_text);
        for (step = 0; step < 9; step++) {
            if (step < state) {
                switch (step) {
                case 0:
                    console_printf("  1 : NORMAL SPEED\n");
                    break;
                case 1:
                    console_printf("  2 : CD STOPED\n");
                    break;
                case 2:
                    console_printf("  3 : CD OPENED\n");
                    break;
                case 3:
                    console_printf("  4 : CD CLOSED\n");
                    break;
                case 4:
                    console_printf("  5 : SPINDLE OK\n");
                    break;
                case 5:
                    console_printf("  6 : TOC OK\n");
                    break;
                case 6:
                    console_printf("  7 : SET LOCATION OK\n");
                    break;
                case 7:
                    console_printf("  8 : PLAY STATION DISC OK\n");
                    break;
                case 8:
                    console_printf("  9 : XENOGEARS %2d DISC OK\n", cd_get_disc_number());
                    break;
                }
            } else {
                console_printf(" %2d :\n", step + 1);
            }
        }
        if (done) {
            console_printf("\n MODE %1d : NO ERROR  COUNT %6d\n", state, frames);
        } else {
            console_printf("\n MODE %1d : %2d ERROR COUNT %6d\n", state, done, frames);
        }
        console_printf(" RESULT %02x %02x %02x %02x %02x %02x %02x %02x\n", movie_mode_cd_command_result[0],
                      movie_mode_cd_command_result[1], movie_mode_cd_command_result[2], movie_mode_cd_command_result[3], movie_mode_cd_command_result[4], movie_mode_cd_command_result[5],
                      movie_mode_cd_command_result[6], movie_mode_cd_command_result[7]);
        console_printf("\n\n PUSH START TO TEST.\n");
        console_printf(" PUSH CIRCLE BUTTON TO MENU.\n");
        if (state > 0) {
            state = movie_mode_step_disc_change_test(3 - cd_get_disc_number(), state, &error, &done);
        }
        if ((movie_mode_buttons & 0x40) && !(movie_mode_previous_buttons & 0x40) && state > 0 && state < 8) {
            state++;
        }
        if ((movie_mode_buttons & 0x800) && !(movie_mode_previous_buttons & 0x800) &&
            (state == 0 || state == 9 || error != 0)) {
            error = 0;
            state = 2;
            movie_mode_stop_disc_read();
        }
        if (movie_mode_heap_report_shown > 0) {
            movie_mode_heap_report_shown = 0;
        }
        console_flush(movie_mode_current_buffer->ot);
        movie_mode_draw_backdrop(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->box, 8, 20, 304, 192);
        movie_mode_draw_menu_frame(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->frame, 7, 19, 306, 194);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&movie_mode_current_buffer->draw);
        PutDispEnv(&movie_mode_current_buffer->disp);
        DrawOTag(&movie_mode_current_buffer->ot[31]);
        if (button == 2) {
            break;
        }
        frames++;
    }
}

/* 8007293C: Stop the resident disc read and wait until the drive reports its status. */
void movie_mode_stop_disc_read(void) {
    if (cd_has_pc_file_server() == 0) {
        cd_stop_read(0);
        cd_sync_reads(0);
        cd_set_mode(0);
        cd_sync_reads(0);
        VSync(3);
        while (CdControlB(8, NULL, movie_mode_cd_command_result) == 0) {
        }
    }
}

/* 800729A8: Read `size` bytes of the host file `name` into `buffer`. */
void movie_mode_read_host_file(char *name, void *buffer, s32 size) {
    s32 fd;

    fd = PCopen(name, 0, 0);
    PCread(fd, buffer, size);
    PCclose(fd);
}

/* 80072A08: One step of the disc change test for disc `disc`: stop the drive, wait for
 * the lid to open and close and the spindle, read the TOC, seek sector 0 and
 * check the disc label, then reload the directory tables. With the host PC
 * the tables are read from its files instead. `*error` gets 1 (seek error),
 * 2 (not a Xenogears disc) or 3 (wrong disc); `*done` the command result.
 * Returns the next state. */
s32 movie_mode_step_disc_change_test(s32 disc, s32 state, s32 *error, s32 *done) {
    u32 label[4] = {0, 0, 0, 0};
    CdlLOC loc;
    s32 result;

    CdIntToPos(0, &loc);
    result = 1;
    if (*error == 0) {
        if (cd_has_pc_file_server() != 0 && state < 9) {
            if (disc == 1) {
                movie_mode_read_host_file("c:\\work\\cdrom.mdg", cd_file_index, 0x8000);
                movie_mode_read_host_file("c:\\work\\cdrom.fid", cd_directory_table, 0x7A);
                movie_mode_read_host_file("c:\\work\\cdrom.fnd", cd_pc_file_names, 0x40000);
            } else {
                movie_mode_read_host_file("c:\\work\\cdrom2.mdg", cd_file_index, 0x8000);
                movie_mode_read_host_file("c:\\work\\cdrom2.fid", cd_directory_table, 0x7A);
                movie_mode_read_host_file("c:\\work\\cdrom2.fnd", cd_pc_file_names, 0x40000);
            }
            state = 9;
        } else {
            switch (state) {
            case 1:
                result = CdControlB(CdlStop, NULL, movie_mode_cd_command_result);
                if (result != 0) {
                    state++;
                }
                break;
            case 2:
                CdControlB(CdlNop, NULL, movie_mode_cd_command_result);
                if (movie_mode_cd_command_result[0] & 0x10) {
                    state++;
                }
                break;
            case 3:
                CdControlB(CdlNop, NULL, movie_mode_cd_command_result);
                if (!(movie_mode_cd_command_result[0] & 0x10)) {
                    state++;
                }
                break;
            case 4:
                result = CdControlB(CdlNop, NULL, movie_mode_cd_command_result);
                if (movie_mode_cd_command_result[0] & 2) {
                    if (result != 0) {
                        state++;
                    }
                }
                break;
            case 5:
                result = CdControlB(CdlGetTN, NULL, movie_mode_cd_command_result);
                if (result != 0) {
                    state++;
                }
                break;
            case 6:
                result = CdControlB(CdlSetloc, (u8 *)&loc, movie_mode_cd_command_result);
                if (result != 0) {
                    state++;
                }
                break;
            case 7:
                result = CdControlB(CdlSeekL, NULL, movie_mode_cd_command_result);
                if ((movie_mode_cd_command_result[0] & 1) && (movie_mode_cd_command_result[1] & 0x40) && result == 0) {
                    *error = 1;
                } else if (result != 0) {
                    state++;
                }
                if (result == 0) {
                    state = 5;
                }
                break;
            case 8:
                cd_set_mode(0xA0);
                cd_sync_reads(0);
                VSync(3);
                cd_read_raw_sectors(0x17, label, 0x10, 0, 0);
                cd_sync_reads(0);
                if (label[1] == 0x4E45585F) { /* "_XEN" */
                    if (((u8 *)label)[3] == disc + '0') {
                        cd_read_raw_sectors(0x18, cd_file_index, 0x8000, 0, 0);
                        state++;
                        cd_sync_reads(0);
                        cd_read_raw_sectors(0x28, cd_directory_table, 0x7A, 0, 0);
                        cd_sync_reads(0);
                        *error = 0;
                    } else {
                        *error = 3;
                    }
                } else {
                    *error = 2;
                }
                break;
            }
        }
        *done = result;
    }
    return state;
}

/* 80072D84: Set up the menu backdrop quads of both buffers at (x, y), w by h, with
 * random dark blue corner fades. */
void movie_mode_init_backdrop(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h) {
    s32 i;

    for (i = 0; i < 4; i++) {
        movie_mode_backdrop_from_colors[i].r = (movie_mode_next_random() & 0xFF) / 32 + 8;
        movie_mode_backdrop_from_colors[i].g = 8;
        movie_mode_backdrop_from_colors[i].b = (movie_mode_next_random() & 0xFF) / 3 + 16;
        movie_mode_backdrop_to_colors[i].r = (movie_mode_next_random() & 0xFF) / 32 + 8;
        movie_mode_backdrop_to_colors[i].g = 8;
        movie_mode_backdrop_to_colors[i].b = (movie_mode_next_random() & 0xFF) / 3 + 16;
    }
    for (i = 0; i < 4; i++) {
        movie_mode_backdrop_fade_elapsed[i] = 0;
        movie_mode_backdrop_fade_durations[i] = (movie_mode_next_random() & 0xFF) + 32;
    }
    SetPolyG4(poly0);
    SetSemiTrans(poly0, 0);
    poly0->x0 = x;
    poly0->y0 = y;
    poly0->x1 = x + w;
    poly0->y1 = y;
    poly0->x2 = x;
    poly0->y2 = y + h;
    poly0->x3 = x + w;
    poly0->y3 = y + h;
    SetPolyG4(poly1);
    SetSemiTrans(poly1, 0);
    poly1->x0 = x;
    poly1->y0 = y;
    poly1->x1 = x + w;
    poly1->y1 = y;
    poly1->x2 = x;
    poly1->y2 = y + h;
    poly1->x3 = x + w;
    poly1->y3 = y + h;
}

/* 80072F98: Add the menu backdrop to `ot`: a gouraud quad at (x, y), w by h, whose
 * corner colors each fade toward a new random color (as movie_mode_draw_menu_frame).
 * Each channel's difference of two bytes is formed in the channel's int and
 * kept in a short for the fade step. */
void movie_mode_draw_backdrop(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    u8 from;
    s16 delta;
    s32 r;
    s32 g;
    s32 b;

    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    for (i = 0; i < 4; i++) {
        if (++movie_mode_backdrop_fade_elapsed[i] > movie_mode_backdrop_fade_durations[i]) {
            movie_mode_backdrop_fade_elapsed[i] = 0;
            movie_mode_backdrop_fade_durations[i] = (movie_mode_next_random() & 0xFF) + 32;
            movie_mode_backdrop_from_colors[i].r = movie_mode_backdrop_to_colors[i].r;
            movie_mode_backdrop_from_colors[i].g = movie_mode_backdrop_to_colors[i].g;
            movie_mode_backdrop_from_colors[i].b = movie_mode_backdrop_to_colors[i].b;
            movie_mode_backdrop_to_colors[i].r = (movie_mode_next_random() & 0xFF) / 32 + 8;
            movie_mode_backdrop_to_colors[i].g = 8;
            movie_mode_backdrop_to_colors[i].b = (movie_mode_next_random() & 0xFF) / 3 + 16;
        }
        from = movie_mode_backdrop_from_colors[i].r;
        r = movie_mode_backdrop_to_colors[i].r - from;
        delta = r;
        r = movie_mode_backdrop_from_colors[i].r + delta * movie_mode_backdrop_fade_elapsed[i] / movie_mode_backdrop_fade_durations[i];
        from = movie_mode_backdrop_from_colors[i].g;
        g = movie_mode_backdrop_to_colors[i].g - from;
        delta = g;
        g = movie_mode_backdrop_from_colors[i].g + delta * movie_mode_backdrop_fade_elapsed[i] / movie_mode_backdrop_fade_durations[i];
        from = movie_mode_backdrop_from_colors[i].b;
        b = movie_mode_backdrop_to_colors[i].b - from;
        delta = b;
        b = movie_mode_backdrop_from_colors[i].b + delta * movie_mode_backdrop_fade_elapsed[i] / movie_mode_backdrop_fade_durations[i];
        switch (i) {
        case 0:
            poly->r0 = r;
            poly->g0 = g;
            poly->b0 = b;
            break;
        case 1:
            poly->r1 = r;
            poly->g1 = g;
            poly->b1 = b;
            break;
        case 2:
            poly->r2 = r;
            poly->g2 = g;
            poly->b2 = b;
            break;
        case 3:
            poly->r3 = r;
            poly->g3 = g;
            poly->b3 = b;
            break;
        }
    }
    poly->tag = (poly->tag & 0xFF000000) | (*ot & 0xFFFFFF);
    *ot = (*ot & 0xFF000000) | ((u32)poly & 0xFFFFFF);
}

/* 80073328: Set up the menu frame quads of both buffers at (x, y), w by h, with random
 * pale yellow corner fades. */
void movie_mode_init_menu_frame(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h) {
    s32 i;

    for (i = 0; i < 4; i++) {
        movie_mode_menu_frame_from_colors[i].r = 0xFF;
        movie_mode_menu_frame_from_colors[i].g = 0xFF;
        movie_mode_menu_frame_from_colors[i].b = (movie_mode_next_random() & 0x3F) - 0x42;
        movie_mode_menu_frame_to_colors[i].r = 0xFF;
        movie_mode_menu_frame_to_colors[i].g = 0xFF;
        movie_mode_menu_frame_to_colors[i].b = (movie_mode_next_random() & 0x3F) - 0x42;
    }
    for (i = 0; i < 4; i++) {
        movie_mode_menu_frame_fade_elapsed[i] = 0;
        movie_mode_menu_frame_fade_durations[i] = (movie_mode_next_random() & 0xFF) + 32;
    }
    SetPolyG4(poly0);
    poly0->x0 = x;
    poly0->y0 = y;
    poly0->x1 = x + w;
    poly0->y1 = y;
    poly0->x2 = x;
    poly0->y2 = y + h;
    poly0->x3 = x + w;
    poly0->y3 = y + h;
    SetPolyG4(poly1);
    poly1->x0 = x;
    poly1->y0 = y;
    poly1->x1 = x + w;
    poly1->y1 = y;
    poly1->x2 = x;
    poly1->y2 = y + h;
    poly1->x3 = x + w;
    poly1->y3 = y + h;
}

/* 800734B8: Add the menu frame to `ot`: a gouraud quad at (x, y), w by h, whose corner
 * colors each fade toward a new random pale yellow. Each component is eased
 * from the byte `from` and added back to a fresh read of the from-colour. */
void movie_mode_draw_menu_frame(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    u8 from;
    s32 delta;
    s32 r;
    s32 g;
    s32 b;

    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    for (i = 0; i < 4; i++) {
        if (++movie_mode_menu_frame_fade_elapsed[i] > movie_mode_menu_frame_fade_durations[i]) {
            movie_mode_menu_frame_fade_elapsed[i] = 0;
            movie_mode_menu_frame_fade_durations[i] = (movie_mode_next_random() & 0xFF) + 32;
            movie_mode_menu_frame_from_colors[i].r = movie_mode_menu_frame_to_colors[i].r;
            movie_mode_menu_frame_from_colors[i].g = movie_mode_menu_frame_to_colors[i].g;
            movie_mode_menu_frame_from_colors[i].b = movie_mode_menu_frame_to_colors[i].b;
            movie_mode_menu_frame_to_colors[i].r = 0xFF;
            movie_mode_menu_frame_to_colors[i].g = 0xFF;
            movie_mode_menu_frame_to_colors[i].b = (movie_mode_next_random() & 0x3F) - 0x42;
        }
        from = movie_mode_menu_frame_from_colors[i].r;
        delta = (movie_mode_menu_frame_to_colors[i].r - from) * movie_mode_menu_frame_fade_elapsed[i] / movie_mode_menu_frame_fade_durations[i];
        r = movie_mode_menu_frame_from_colors[i].r + delta;
        from = movie_mode_menu_frame_from_colors[i].g;
        delta = (movie_mode_menu_frame_to_colors[i].g - from) * movie_mode_menu_frame_fade_elapsed[i] / movie_mode_menu_frame_fade_durations[i];
        g = movie_mode_menu_frame_from_colors[i].g + delta;
        from = movie_mode_menu_frame_from_colors[i].b;
        delta = (movie_mode_menu_frame_to_colors[i].b - from) * movie_mode_menu_frame_fade_elapsed[i] / movie_mode_menu_frame_fade_durations[i];
        b = movie_mode_menu_frame_from_colors[i].b + delta;
        switch (i) {
        case 0:
            poly->r0 = r;
            poly->g0 = g;
            poly->b0 = b;
            break;
        case 1:
            poly->r1 = r;
            poly->g1 = g;
            poly->b1 = b;
            break;
        case 2:
            poly->r2 = r;
            poly->g2 = g;
            poly->b2 = b;
            break;
        case 3:
            poly->r3 = r;
            poly->g3 = g;
            poly->b3 = b;
            break;
        }
    }
    poly->tag = (poly->tag & 0xFF000000) | (*ot & 0xFFFFFF);
    *ot = (*ot & 0xFF000000) | ((u32)poly & 0xFFFFFF);
}

/* 800737EC: Mode 6 entry. Load the movie library below the heap top and open it at
 * 320x256; with a movie request (8004fe44..47) play it and select the next
 * mode. Otherwise run the development menu: movie type, number, start and
 * end frame (Circle seeks them), channel, colour depth, rows drawn, rewind,
 * then the movie test, CD-ROM monitor, CD-ROM check, FAT check, disc change
 * test and a return to the kernel. Square and Cross speed up the frame
 * settings. The menu cursor is kept across the screens it opens. The unused
 * name reproduces the original's frame. The debug views are cleared and
 * 24-bit library output selected before the row and last-frame defaults:
 * stored in the order the binary shows (rows and last frame first), the
 * schedule differs. */
void movie_mode_main(void) {
    char name[8] = "trouble";
    s32 button;
    s32 step;
    s32 dir;
    s32 line;
    s32 last;
    s32 cursor;
    void *top;
    void *library;
    MovieBuffer *buffer;
    u_long *ot;

    heap_select_owner_tag(4, 0);
    cd_select_directory(0x18, 0);
    sound_set_cd_volume(0, 0);
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    top = heap_alloc(4, 1);
    library = heap_alloc(((u32)top & 0xFFFFFF) - 0x1D3008, 1);
    heap_free(top);
    cd_read_file(1, library, 0, 0);
    cd_sync_reads(0);
    movie_open(320, 256, 128, 16, 32, 0x800, 3);
    movie_mode_disc_mode = cd_has_pc_file_server();
    movie_mode_heap_report_shown = 0;
    movie_mode_monitor_shown = 0;
    movie_mode_library_output_mode = 1;
    movie_mode_draw_rows = -1;
    movie_mode_last_frame = 0xC80;
    movie_mode_movie_kind = 1;
    movie_mode_movie_index = 0;
    movie_mode_first_frame = 1;
    movie_mode_xa_channel = 1;
    movie_mode_start_frame_state = 2;
    movie_mode_end_frame_state = 0;
    movie_mode_start_sector = 0;
    movie_mode_rewind_enabled = 0;
    SetDefDrawEnv(&movie_mode_buffers[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&movie_mode_buffers[0].disp, 0, 240, 320, 240);
    SetDefDrawEnv(&movie_mode_buffers[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&movie_mode_buffers[1].disp, 0, 0, 320, 240);
    movie_mode_buffers[0].draw.dtd = 1;
    movie_mode_buffers[0].draw.isbg = 1;
    movie_mode_buffers[1].draw.dtd = 1;
    movie_mode_buffers[1].draw.isbg = 1;
    movie_mode_buffers[0].draw.r0 = 0;
    movie_mode_buffers[0].draw.g0 = 0;
    movie_mode_buffers[0].draw.b0 = 0;
    movie_mode_buffers[0].disp.isinter = 0;
    movie_mode_buffers[1].draw.r0 = 0;
    movie_mode_buffers[1].draw.g0 = 0;
    movie_mode_buffers[1].draw.b0 = 0;
    movie_mode_buffers[1].disp.isinter = 0;
    movie_mode_buffers[0].disp.screen.x = 0;
    movie_mode_buffers[0].disp.screen.y = 10;
    movie_mode_buffers[0].disp.screen.w = 256;
    movie_mode_buffers[0].disp.screen.h = 216;
    movie_mode_buffers[1].disp.screen.x = 0;
    movie_mode_buffers[1].disp.screen.y = 10;
    movie_mode_buffers[1].disp.screen.w = 256;
    movie_mode_buffers[1].disp.screen.h = 216;
    movie_mode_current_buffer = &movie_mode_buffers[0];
    movie_mode_buffer_index = 0;
    PutDrawEnv(&movie_mode_buffers[0].draw);
    PutDispEnv(&movie_mode_current_buffer->disp);
    if (movie_mode_disc_mode != 0) {
        VSync(2);
        movie_mode_buttons = pad_read_buttons(0);
    } else {
        movie_mode_buttons = 0;
    }
    if (cd_movie_request[0] != 0xFF && !(movie_mode_buttons & 0x100)) {
        movie_mode_unread_menu_shown = 0;
        movie_mode_xa_channel = 1;
        movie_mode_first_frame = 1;
        movie_mode_movie_kind = cd_movie_request[0] & 0x7F;
        movie_mode_movie_index = cd_movie_request[1];
        if (cd_movie_request[0] & 0x80) {
            movie_mode_last_frame = cd_movie_request_last_frame;
        } else {
            movie_mode_last_frame = 0xE9;
        }
        movie_mode_play_requested_movie(cd_movie_request[3]);
        movie_close();
        heap_free(library);
        mode_select_next_mode(cd_movie_request[2]);
        mode_dispatch(0);
    }
    movie_mode_init_backdrop((POLY_G4 *)movie_mode_buffers[0].box, (POLY_G4 *)movie_mode_buffers[1].box, 0, 0, 0, 0);
    movie_mode_init_menu_frame((POLY_G4 *)movie_mode_buffers[0].frame, (POLY_G4 *)movie_mode_buffers[1].frame, 0, 0, 0, 0);
    console_open(16, 16, 640, 240, 0x400, 0, 640, 0, 640, 256, 0);
    movie_mode_unread_menu_shown = 1;
    SetDispMask(1);
    for (;;) {
        if (movie_mode_current_buffer == &movie_mode_buffers[0]) {
            buffer = &movie_mode_buffers[1];
        } else {
            buffer = &movie_mode_buffers[0];
        }
        ot = buffer->ot;
        movie_mode_current_buffer = buffer;
        movie_mode_buffer_index = 1 - movie_mode_buffer_index;
        ClearOTagR(ot, 32);
        if (movie_mode_disc_mode == 0) {
            console_printf("  [ MOVIE CD-ROM MODE1 DISK %1d ]  \n\n", cd_get_disc_number());
        } else if (movie_mode_disc_mode == -1) {
            console_printf("  [ MOVIE CD-ROM MODE2 DISK %1d ]  \n\n", cd_get_disc_number());
        } else {
            console_printf("  [ MOVIE PC HDD MODE  DISK %1d ]  \n\n", cd_get_disc_number());
        }
        console_printf("    ERROR %2d Sect %2d:%2d FM%3d\n", cd_error_count, cd_stat_stop_ok_count, cd_stat_stop_fail_count,
                      (s16)stream_frame_number);
        step = 1;
        console_printf("    LesMem%2d NoMem%2d Skp%3d\n", cd_stat_lesmem_count, cd_stat_error_limit_count,
                      movie_skipped_frames, cd_movie_request_last_frame);
        dir = movie_mode_read_menu_input(0, 13, &button);
        if (movie_mode_buttons & 0x10) {
            step = 32;
        }
        if (movie_mode_buttons & 0x80) {
            step <<= 7;
        }
        if (movie_mode_menu_cursor == 0 && dir != 0) {
            movie_mode_movie_kind += dir;
            if (movie_mode_movie_kind < 0) {
                movie_mode_movie_kind = 2;
            }
            if (movie_mode_movie_kind >= 3) {
                movie_mode_movie_kind = 0;
            }
            movie_mode_first_frame = 1;
            movie_mode_start_sector = 0;
            movie_mode_end_frame_state = 0;
            movie_mode_start_frame_state = 2;
        }
        if (movie_mode_menu_cursor == 1 && dir != 0) {
            movie_mode_movie_index += dir;
            if (movie_mode_movie_index < 0) {
                movie_mode_movie_index = 63;
            }
            if (movie_mode_movie_index >= 64) {
                movie_mode_movie_index = 0;
            }
            movie_mode_first_frame = 1;
            movie_mode_start_sector = 0;
            movie_mode_end_frame_state = 0;
            movie_mode_start_frame_state = 2;
        }
        if (movie_mode_menu_cursor == 2 && dir != 0) {
            movie_mode_first_frame += dir * step;
            if (movie_mode_first_frame <= 0) {
                movie_mode_first_frame = 1;
            }
            if (movie_mode_first_frame >= 0x2000) {
                movie_mode_first_frame = 0x1FFF;
            }
            if (movie_mode_first_frame > movie_mode_last_frame) {
                movie_mode_first_frame = movie_mode_last_frame;
            }
            movie_mode_start_frame_state = 1;
        }
        if (movie_mode_menu_cursor == 2 && button == 2 && movie_mode_start_frame_state == 1) {
            movie_mode_start_sector = movie_mode_find_frame_sector(movie_mode_first_frame);
            if (movie_mode_first_frame >= movie_mode_last_frame) {
                movie_mode_last_frame = movie_mode_first_frame;
                movie_mode_end_frame_state = 0;
            }
            movie_mode_start_frame_state = 2;
        }
        if (movie_mode_menu_cursor == 3 && dir != 0) {
            movie_mode_last_frame += dir * step;
            if (movie_mode_last_frame <= 0) {
                movie_mode_last_frame = 1;
            }
            if (movie_mode_last_frame >= 0x2000) {
                movie_mode_last_frame = 0x1FFF;
            }
            if (movie_mode_last_frame < movie_mode_first_frame) {
                movie_mode_last_frame = movie_mode_first_frame;
            }
            movie_mode_end_frame_state = 0;
        }
        if (movie_mode_menu_cursor == 3 && button == 2 && movie_mode_end_frame_state == 0) {
            last = movie_mode_find_last_frame();
            if (last >= 0) {
                movie_mode_last_frame = last;
                movie_mode_end_frame_state = 1;
                if (movie_mode_first_frame >= last) {
                    movie_mode_first_frame = last;
                    movie_mode_start_frame_state = 1;
                }
            } else {
                movie_mode_end_frame_state = 2;
            }
        }
        if (movie_mode_menu_cursor == 4 && dir != 0) {
            movie_mode_xa_channel += dir;
            if (movie_mode_xa_channel < 0) {
                movie_mode_xa_channel = 7;
            }
            if (movie_mode_xa_channel >= 8) {
                movie_mode_xa_channel = 0;
            }
        }
        if (movie_mode_menu_cursor == 5 && dir != 0) {
            movie_mode_library_output_mode = 1 - movie_mode_library_output_mode;
        }
        if (movie_mode_menu_cursor == 6) {
            if (dir != 0) {
                movie_mode_draw_rows = (movie_mode_draw_rows + dir * step) & 0xFF;
            }
            if (button == 2) {
                movie_mode_draw_rows = -1;
            }
        }
        if (movie_mode_menu_cursor == 7 && dir != 0) {
            movie_mode_rewind_enabled = 1 - movie_mode_rewind_enabled;
        }
        for (line = 0; line < 14; line++) {
            console_printf(movie_mode_menu_cursor == line ? "  >" : "   ");
            switch (line) {
            case 0:
                console_printf(" MOVIE TYPE   ");
                if (movie_mode_movie_kind == 0) {
                    console_printf("PICTURE ONLY\n");
                } else if (movie_mode_movie_kind == 1) {
                    console_printf("PICTURE+ADPCM\n");
                } else if (movie_mode_movie_kind == 2) {
                    console_printf("ADPCM ONLY\n");
                }
                break;
            case 1:
                console_printf(" MOVIE NUMBER %4d\n\n", movie_mode_movie_index);
                break;
            case 2:
                console_printf(" START FRAME  %4d ", movie_mode_first_frame);
                if (movie_mode_start_frame_state == 1) {
                    console_printf("SET");
                }
                if (movie_mode_start_frame_state == 2) {
                    if (movie_mode_start_sector < 0) {
                        console_printf("EOF");
                    } else {
                        console_printf("+%4dSECT", movie_mode_start_sector);
                    }
                }
                console_printf("\n");
                break;
            case 3:
                console_printf(" END   FRAME  %4d ", movie_mode_last_frame);
                if (movie_mode_end_frame_state == 0) {
                    console_printf("SET");
                }
                if (movie_mode_end_frame_state == 2) {
                    console_printf("???");
                }
                console_printf("\n");
                break;
            case 4:
                console_printf(" MOVIE CHANNEL %3d\n", movie_mode_xa_channel);
                break;
            case 5:
                console_printf(" SCREEN MODE  ");
                console_printf(movie_mode_library_output_mode ? "24 BIT COLOR" : "16 BIT COLOR");
                console_printf("\n");
                break;
            case 6:
                console_printf(" SCREEN DRAW  ");
                if (movie_mode_draw_rows < 0) {
                    console_printf("ALL");
                } else {
                    console_printf("%3d", movie_mode_draw_rows);
                }
                console_printf("\n");
                break;
            case 7:
                console_printf(" REWIND       ");
                console_printf(movie_mode_rewind_enabled ? "ON" : "OFF");
                console_printf("\n\n");
                break;
            case 8:
                console_printf(" MOVIE START.\n\n");
                break;
            case 9:
                console_printf(" CD-ROM MONITOR.\n\n");
                break;
            case 10:
                console_printf(" CD-ROM CHECK.\n");
                break;
            case 11:
                console_printf(" FAT CHECK.\n\n");
                break;
            case 12:
                console_printf(" [DISC CHANGE.]\n");
                break;
            case 13:
                console_printf(" [RETURN TO KERNEL.]\n");
                break;
            }
        }
        if (movie_mode_heap_report_shown > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(movie_mode_current_buffer->ot);
        movie_mode_draw_backdrop(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->box, 20, 12, 284, 198);
        movie_mode_draw_menu_frame(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->frame, 19, 11, 286, 200);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&movie_mode_current_buffer->draw);
        PutDispEnv(&movie_mode_current_buffer->disp);
        DrawOTag(&movie_mode_current_buffer->ot[31]);
        cursor = movie_mode_menu_cursor;
        if ((cursor == 0 || cursor == 1 || cursor == 4 || cursor == 5 || cursor == 7 ||
             cursor == 8) &&
            button == 2) {
            cd_stat_lesmem_count = 0;
            cd_stat_error_limit_count = 0;
            cd_stat_stop_ok_count = 0;
            cd_stat_stop_fail_count = 0;
            movie_mode_run_movie_test();
            movie_mode_buttons = -1;
        }
        if (movie_mode_menu_cursor == 9 && button == 2) {
            movie_mode_run_cd_monitor();
        }
        if (movie_mode_menu_cursor == 10 && button == 2) {
            movie_mode_run_cd_check();
        }
        if (movie_mode_menu_cursor == 11 && button == 2) {
            movie_mode_run_fat_check();
        }
        if (movie_mode_menu_cursor == 12 && button == 2) {
            movie_mode_run_disc_change_test();
        }
        if (movie_mode_menu_cursor == 13 && button == 2) {
            heap_free(library);
            mode_dispatch(0);
        }
        movie_mode_menu_cursor = cursor;
        boot_check_soft_reset();
    }
}

/* 800747AC: Menu input: read controller port 0 with a repeat after 16 frames held; Up
 * and Down move the cursor between `first` and `last`, wrapping; the four
 * face buttons report 1..4 in `button`; Select toggles the monitor and Start
 * the statistics. Returns 1 for Right, -1 for Left, else 0. */
s32 movie_mode_read_menu_input(s32 first, s32 last, s32 *button) {
    movie_mode_previous_buttons = movie_mode_buttons;
    movie_mode_buttons = pad_read_buttons(0);
    if (movie_mode_previous_buttons == movie_mode_buttons && movie_mode_previous_buttons != 0) {
        movie_mode_menu_hold_frames++;
        if (movie_mode_menu_repeat_timer < movie_mode_menu_hold_frames) {
            movie_mode_previous_buttons = 0;
            movie_mode_menu_repeat_timer = 1;
            movie_mode_menu_hold_frames = 0;
        }
    } else {
        movie_mode_menu_repeat_timer = 16;
        movie_mode_menu_hold_frames = 0;
    }
    if (!(movie_mode_previous_buttons & 0x1000) && (movie_mode_buttons & 0x1000)) {
        if (--movie_mode_menu_cursor < first) {
            movie_mode_menu_cursor = last;
        }
    }
    if (!(movie_mode_previous_buttons & 0x4000) && (movie_mode_buttons & 0x4000)) {
        if (++movie_mode_menu_cursor > last) {
            movie_mode_menu_cursor = first;
        }
    }
    *button = 0;
    if (!(movie_mode_previous_buttons & 0x10) && (movie_mode_buttons & 0x10)) {
        *button = 1;
    }
    if (!(movie_mode_previous_buttons & 0x20) && (movie_mode_buttons & 0x20)) {
        *button = 2;
    }
    if (!(movie_mode_previous_buttons & 0x40) && (movie_mode_buttons & 0x40)) {
        *button = 3;
    }
    if (!(movie_mode_previous_buttons & 0x80) && (movie_mode_buttons & 0x80)) {
        *button = 4;
    }
    if (!(movie_mode_previous_buttons & 0x100) && (movie_mode_buttons & 0x100)) {
        movie_mode_monitor_shown = 1 - movie_mode_monitor_shown;
    }
    if (!(movie_mode_previous_buttons & 0x800) && (movie_mode_buttons & 0x800)) {
        movie_mode_heap_report_shown = 1 - movie_mode_heap_report_shown;
    }
    if (!(movie_mode_previous_buttons & 0x2000) && (movie_mode_buttons & 0x2000)) {
        return 1;
    }
    if (!(movie_mode_previous_buttons & 0x8000) && (movie_mode_buttons & 0x8000)) {
        return -1;
    }
    return 0;
}

/* 80074AF0: Next pseudo-random number (two mixed linear congruential sequences). */
s32 movie_mode_next_random(void) {
    movie_mode_random_state_a = movie_mode_random_state_a * 5 + 1;
    movie_mode_random_state_b = movie_mode_random_state_b * 7 + 3;
    movie_mode_random_state_a = (movie_mode_random_state_a ^ movie_mode_random_state_b) + 1;
    if (movie_mode_random_state_a < 0) {
        movie_mode_random_state_a = -movie_mode_random_state_a;
    }
    return movie_mode_random_state_a;
}

/* 80074B58: Clear all of VRAM to black. */
void movie_mode_clear_vram(void) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = 640;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* 80074BA4: The sector of the selected movie where `frame` starts: guess from the
 * first sector's sectors per frame, correct once, then step sector by
 * sector (frame by frame on headers) until a header names the frame.
 * Returns 0 for the first frame, -1 past the file; an index past the list
 * returns without a value, as the original does. */
s32 movie_mode_find_frame_sector(s32 frame) {
    u8 buffer[0x1000];
    s32 file;
    char *name;
    s32 sector_size;
    s32 read_size;
    s32 header;
    s32 lower;
    s32 fd;
    s32 count;
    s32 per_frame;
    s32 pos;
    s32 next;
    s32 total;
    MovieSector *h;

    lower = 0;
    if (movie_mode_movie_kind == 0) {
        cd_select_directory(0x18, 0);
        if (movie_mode_movie_index >= cd_get_directory_file_count(2)) {
            return;
        }
        file = movie_mode_movie_index + 3;
        name = cd_get_pc_file_name(file);
        sector_size = 0x800;
        read_size = 0x20;
        header = 0;
    } else if (movie_mode_movie_kind == 1) {
        cd_select_directory(0x18, 1);
        if (movie_mode_movie_index >= cd_get_directory_file_count(1)) {
            return;
        }
        file = movie_mode_movie_index + 2;
        name = cd_get_pc_file_name(file);
        sector_size = 0x920;
        read_size = 0x28;
        header = 8;
    } else if (movie_mode_movie_kind == 2) {
        return -1;
    }
    if (frame < 2) {
        return 0;
    }
    if (cd_has_pc_file_server() != 0) {
        fd = PCopen(name, 0, 0);
        PClseek(fd, 0, 2);
        PClseek(fd, 0, 0);
        PCread(fd, buffer, read_size);
        h = (MovieSector *)&buffer[header];
        per_frame = h->sectors;
        pos = (frame - 1) * per_frame - (frame - 1) / 4;
        PClseek(fd, pos * sector_size, 0);
        count = PCread(fd, buffer, read_size);
        if (h->frame == frame && count != 0) {
            if (h->sector == 0) {
                goto done;
            }
            pos -= h->sector;
            next = pos - 2;
        } else {
            if (h->frame < frame && lower < pos && count != 0) {
                lower = pos;
            }
            pos = (frame - 1) * per_frame - (frame - 1) / 4;
            pos += pos / 7;
            PClseek(fd, pos * sector_size, 0);
            count = PCread(fd, buffer, read_size);
            h = (MovieSector *)&buffer[header];
            if (h->frame == frame && count != 0) {
                if (h->sector == 0) {
                    goto done;
                }
                pos -= h->sector;
                next = pos - 2;
            } else {
                if (h->frame < frame && lower < pos && count != 0) {
                    lower = pos;
                }
                next = (per_frame - 1) * (frame - 1);
                if (lower > 0) {
                    next = lower;
                }
            }
        }
        do {
            pos = next;
            PClseek(fd, next * sector_size, 0);
            count = PCread(fd, buffer, read_size);
            h = (MovieSector *)&buffer[header];
            if (h->magic == 0x160) {
                next = pos + (h->sectors - h->sector);
            } else {
                next = pos + 1;
            }
        } while (h->frame != frame && count > 0);
        if (count == 0) {
            pos = -1;
        }
    done:
        PCclose(fd);
        return pos;
    }
    cd_read_raw_sectors(cd_get_file_sector(file), buffer, 0x800, 0, 0);
    cd_sync_reads(0);
    h = (MovieSector *)buffer;
    per_frame = h->sectors;
    total = (cd_get_aligned_file_size(file) + sector_size - 1) / sector_size;
    pos = (frame - 1) * per_frame - (frame - 1) / 4;
    cd_read_raw_sectors(cd_get_file_sector(file) + pos, buffer, 0x800, 0, 0);
    cd_sync_reads(0);
    if (h->frame == frame && pos < total) {
        if (h->sector == 0) {
            goto end;
        }
        pos -= h->sector;
        next = pos - 2;
    } else {
        if (h->frame < frame && lower < pos && pos < total) {
            lower = pos;
        }
        pos = (frame - 1) * per_frame - (frame - 1) / 4;
        pos += pos / 7;
        cd_read_raw_sectors(cd_get_file_sector(file) + pos, buffer, 0x800, 0, 0);
        cd_sync_reads(0);
        if (h->frame == frame && pos < total) {
            if (h->sector == 0) {
                goto end;
            }
            pos -= h->sector;
            next = pos - 2;
        } else {
            if (h->frame < frame && lower < pos && pos < total) {
                lower = pos;
            }
            next = (per_frame - 1) * (frame - 1);
            if (lower > 0) {
                next = lower;
            }
        }
    }
    do {
        pos = next;
        cd_read_raw_sectors(cd_get_file_sector(file) + pos, buffer, 0x800, 0, 0);
        cd_sync_reads(0);
        h = (MovieSector *)buffer;
        if (h->magic == 0x160) {
            next = pos + (h->sectors - h->sector);
        } else {
            next = pos + 1;
        }
    } while (h->frame != frame && next < total);
end:
    if (next >= total) {
        pos = -1;
    }
    return pos;
}

/* 8007519C: The last frame number of the selected movie, read from the header of its
 * last sector (host PC file or disc), or -1. An index past the list returns
 * without a value, and a kind above 2 reads with unset parameters, as the
 * original does. */
s32 movie_mode_find_last_frame(void) {
    u8 buffer[0x1000];
    s32 file;
    char *name;
    s32 sector_size;
    s32 read_size;
    s32 frames;
    s32 header;
    s32 fd;
    s32 size;

    frames = -1;
    if (movie_mode_movie_kind == 0) {
        cd_select_directory(0x18, 0);
        if (movie_mode_movie_index >= cd_get_directory_file_count(2)) {
            return;
        }
        file = movie_mode_movie_index + 3;
        name = cd_get_pc_file_name(file);
        sector_size = 0x800;
        read_size = 0x20;
        header = 0;
    } else if (movie_mode_movie_kind == 1) {
        cd_select_directory(0x18, 1);
        if (movie_mode_movie_index >= cd_get_directory_file_count(1)) {
            return;
        }
        file = movie_mode_movie_index + 2;
        name = cd_get_pc_file_name(file);
        sector_size = 0x920;
        read_size = 0x28;
        header = 8;
    } else if (movie_mode_movie_kind == 2) {
        return -1;
    }
    if (cd_has_pc_file_server() != 0) {
        fd = PCopen(name, 0, 0);
        size = PClseek(fd, 0, 2);
        PClseek(fd, 0, 0);
        PClseek(fd, size - sector_size, 0);
        PCread(fd, buffer, read_size);
        if (((MovieSector *)(buffer + header))->magic == 0x160) {
            frames = ((MovieSector *)(buffer + header))->frame;
        }
        PCclose(fd);
    } else {
        size = (cd_get_aligned_file_size(file) + sector_size - 1) / sector_size;
        cd_read_raw_sectors(cd_get_file_sector(file) + size - 1, buffer, 0x800, 0, 0);
        cd_sync_reads(0);
        if (((MovieSector *)buffer)->magic == 0x160) {
            frames = ((MovieSector *)buffer)->frame;
        }
    }
    return frames;
}

/* 800753B8: Load the three sound effect banks from the host PC, waiting for each
 * transfer to the sound memory. */
void movie_mode_load_host_effect_wave_banks(void) {
    void *bank;

    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\main_se.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\bat_se.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\gear_se.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
}

/* Two strings of this unit linked as original rodata below their users
 * (INCLUDE_RODATA, each after its function). */
extern char movie_mode_battle_music_path[];
extern char movie_mode_fat_check_size_format[];

/* 8007548C: Load the battle sound bank from the host PC, waiting for its transfer,
 * then the battle music sequence. */
void movie_mode_load_host_battle_music(void) {
    void *bank;

    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\battle2.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
    movie_mode_battle_music_seq = sound_create_seq(cd_load_pc_file(movie_mode_battle_music_path, 0));
}

/* "c:\\work\\cdrom\\sound\\music\\battle2.smd". The original assembler left a
 * stray byte (0x08) in its alignment padding; it is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", movie_mode_battle_music_path); /* 80070394 */

/* 80075508: Play the battle music sequence (8007548c loads it) from its start at full
 * volume. */
void movie_mode_play_battle_music(void) {
    sound_play_seq(movie_mode_battle_music_seq, 0x7F, 0);
}

/* 80075534: The menu's CD-ROM monitor (line 9), a sector monitor: at 640x240, dump 192 bytes of the current
 * sector (Up/Down by a row, Triangle/Cross by twelve) with its position;
 * Left/Right step the sector by one, L1/R1 by 75 (a second) and L2/R2 by
 * 4500 (a minute), rereading it when it changes. Circle returns to the
 * 320-wide menu. The original reads the menu cursor before the exit test and
 * stores it back after. */
void movie_mode_run_cd_monitor(void) {
    CdlLOC loc;
    s32 button;
    u8 *buffer;
    s32 sector;
    s32 row;
    s32 i;
    u8 *hex;
    u8 *text;
    u8 c;
    s32 cursor;
    MovieBuffer *draw;
    u_long *ot;

    movie_mode_clear_vram();
    buffer = heap_alloc(0x800, 0);
    if (buffer == NULL) {
        return;
    }
    SetDefDrawEnv(&movie_mode_buffers[0].draw, 0, 0, 640, 240);
    SetDefDispEnv(&movie_mode_buffers[0].disp, 0, 240, 640, 240);
    SetDefDrawEnv(&movie_mode_buffers[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&movie_mode_buffers[1].disp, 0, 0, 640, 240);
    movie_mode_buffers[0].draw.isbg = 1;
    movie_mode_buffers[1].draw.isbg = 1;
    movie_mode_buffers[0].disp.screen.x = 0;
    movie_mode_buffers[0].disp.screen.y = 10;
    movie_mode_buffers[0].disp.screen.w = 256;
    movie_mode_buffers[0].disp.screen.h = 216;
    movie_mode_buffers[1].disp.screen.x = 0;
    movie_mode_buffers[1].disp.screen.y = 10;
    movie_mode_buffers[1].disp.screen.w = 256;
    movie_mode_buffers[1].disp.screen.h = 216;
    cd_read_raw_sectors(movie_mode_cd_monitor_sector, buffer, 0x800, 0, 0);
    cd_sync_reads(0);
    for (;;) {
        if (movie_mode_current_buffer == &movie_mode_buffers[0]) {
            draw = &movie_mode_buffers[1];
        } else {
            draw = &movie_mode_buffers[0];
        }
        ot = draw->ot;
        movie_mode_current_buffer = draw;
        movie_mode_buffer_index = 1 - movie_mode_buffer_index;
        ClearOTagR(ot, 32);
        console_printf("\n[ MONITOR ]\n");
        movie_mode_read_menu_input(0, 0, &button);
        sector = movie_mode_cd_monitor_sector;
        if (!(movie_mode_previous_buttons & 0x1000) && (movie_mode_buttons & 0x1000) && movie_mode_cd_monitor_row > 0) {
            movie_mode_cd_monitor_row--;
        }
        if (!(movie_mode_previous_buttons & 0x10) && (movie_mode_buttons & 0x10)) {
            movie_mode_cd_monitor_row -= 12;
            if (movie_mode_cd_monitor_row < 0) {
                movie_mode_cd_monitor_row = 0;
            }
        }
        if (!(movie_mode_previous_buttons & 0x4000) && (movie_mode_buttons & 0x4000) && movie_mode_cd_monitor_row < 116) {
            movie_mode_cd_monitor_row++;
        }
        if (!(movie_mode_previous_buttons & 0x40) && (movie_mode_buttons & 0x40)) {
            movie_mode_cd_monitor_row += 12;
            if (movie_mode_cd_monitor_row >= 116) {
                movie_mode_cd_monitor_row = 116;
            }
        }
        if (!(movie_mode_previous_buttons & 0x8000) && (movie_mode_buttons & 0x8000) && movie_mode_cd_monitor_sector > 0) {
            movie_mode_cd_monitor_sector--;
        }
        if (!(movie_mode_previous_buttons & 0x2000) && (movie_mode_buttons & 0x2000)) {
            movie_mode_cd_monitor_sector++;
        }
        if (!(movie_mode_previous_buttons & 4) && (movie_mode_buttons & 4)) {
            movie_mode_cd_monitor_sector -= 75;
            if (movie_mode_cd_monitor_sector < 0) {
                movie_mode_cd_monitor_sector = 0;
            }
        }
        if (!(movie_mode_previous_buttons & 8) && (movie_mode_buttons & 8)) {
            movie_mode_cd_monitor_sector += 75;
        }
        if (!(movie_mode_previous_buttons & 1) && (movie_mode_buttons & 1)) {
            movie_mode_cd_monitor_sector -= 4500;
            if (movie_mode_cd_monitor_sector < 0) {
                movie_mode_cd_monitor_sector = 0;
            }
        }
        if (!(movie_mode_previous_buttons & 2) && (movie_mode_buttons & 2)) {
            movie_mode_cd_monitor_sector += 4500;
        }
        if (sector != movie_mode_cd_monitor_sector) {
            cd_read_raw_sectors(movie_mode_cd_monitor_sector, buffer, 0x800, 0, 0);
            cd_sync_reads(0);
        }
        CdIntToPos(movie_mode_cd_monitor_sector, &loc);
        console_printf("ABSPOS %8d POS %04x\nMINUTE %02x SECOND %02x SECTOR %02x\n\n", movie_mode_cd_monitor_sector,
                      movie_mode_cd_monitor_row * 16, loc.minute, loc.second, loc.sector);
        text = buffer + movie_mode_cd_monitor_row * 16;
        hex = text;
        for (row = 0; row < 12; row++) {
            console_printf("%03x:", (row + movie_mode_cd_monitor_row) * 16);
            for (i = 0; i < 15; i++) {
                console_printf("%02x ", *hex);
                hex++;
            }
            console_printf("%02x", *hex);
            hex++;
            console_printf(":");
            for (i = 0; i < 16; i++) {
                c = *text;
                if (c >= 0x20 && c < 0x7E) {
                    console_printf("%c", c);
                } else {
                    console_printf(" ");
                }
                text++;
            }
            console_printf((char *)movie_mode_monitor_newline_text);
        }
        console_printf((char *)movie_mode_push_circle_text);
        if (movie_mode_heap_report_shown > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(movie_mode_current_buffer->ot);
        movie_mode_draw_backdrop(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->box, 8, 20, 624, 160);
        movie_mode_draw_menu_frame(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->frame, 7, 19, 626, 162);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&movie_mode_current_buffer->draw);
        PutDispEnv(&movie_mode_current_buffer->disp);
        DrawOTag(&movie_mode_current_buffer->ot[31]);
        cursor = movie_mode_menu_cursor;
        if (button == 2) {
            break;
        }
        movie_mode_menu_cursor = cursor;
    }
    heap_free(buffer);
    movie_mode_clear_vram();
    SetDefDrawEnv(&movie_mode_buffers[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&movie_mode_buffers[0].disp, 0, 240, 320, 240);
    SetDefDrawEnv(&movie_mode_buffers[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&movie_mode_buffers[1].disp, 0, 0, 320, 240);
    movie_mode_buffers[0].draw.isbg = 1;
    movie_mode_buffers[1].draw.isbg = 1;
    movie_mode_buffers[0].disp.screen.x = 0;
    movie_mode_buffers[0].disp.screen.y = 10;
    movie_mode_buffers[0].disp.screen.w = 256;
    movie_mode_buffers[0].disp.screen.h = 216;
    movie_mode_buffers[1].disp.screen.x = 0;
    movie_mode_buffers[1].disp.screen.y = 10;
    movie_mode_buffers[1].disp.screen.w = 256;
    movie_mode_buffers[1].disp.screen.h = 216;
}

/* Strings of the monitor (movie_mode_run_cd_monitor) and the FAT check (movie_mode_run_fat_check):
 * arrays, the "\n" a copy of movie_mode_newline_text's. */
const char movie_mode_monitor_newline_text[] = "\n"; /* 8007042C */
const char movie_mode_push_circle_text[] = "\nPUSH CIRCLE BUTTON TO MENU."; /* 80070430 */

/* 80075D4C: The size field of file index record `index` (bytes 3..6): 0 for none,
 * negative for an entry the FAT check lists as [Pn] by its negation. */
u32 movie_mode_get_file_index_size(s32 index) {
    u8 *record;

    record = cd_file_index + index * 7;
    return ((record[6] << 24) + (record[5] << 16) + (record[4] << 8)) | record[3];
}

/* 80075D8C: The menu's FAT check: list twenty directory records from the cursor
 * (Up/Down by one, Triangle/Cross by twenty) with their first sector and
 * their size or, toggled by L1, their host file name; R1 switches between
 * decimal and hexadecimal. Circle returns to the menu. */
void movie_mode_run_fat_check(void) {
    s32 directory;
    s32 offset;
    s32 button;
    s32 top;
    s32 hex;
    s32 names;
    s32 index;
    s32 count;
    char *name;
    char *p;
    MovieBuffer *buffer;
    u_long *ot;

    movie_mode_clear_vram();
    top = 0;
    cd_get_selected_directory(&directory, &offset);
    cd_select_directory(0, 0);
    hex = 0;
    names = 0;
    do {
        count = 0x1249;
        if (movie_mode_current_buffer == &movie_mode_buffers[0]) {
            buffer = &movie_mode_buffers[1];
        } else {
            buffer = &movie_mode_buffers[0];
        }
        ot = buffer->ot;
        movie_mode_current_buffer = buffer;
        movie_mode_buffer_index = 1 - movie_mode_buffer_index;
        ClearOTagR(ot, 32);
        console_printf("\n[ FAT CHECK MODE ");
        console_printf(hex ? "HEX ]\n" : "DEC ]\n");
        movie_mode_read_menu_input(0, 0, &button);
        if (!(movie_mode_previous_buttons & 0x1000) && (movie_mode_buttons & 0x1000) && top > 0) {
            top--;
        }
        if (!(movie_mode_previous_buttons & 0x10) && (movie_mode_buttons & 0x10)) {
            top -= 20;
            if (top < 0) {
                top = 0;
            }
        }
        if (!(movie_mode_previous_buttons & 0x4000) && (movie_mode_buttons & 0x4000) && top < count - 20) {
            top++;
        }
        if (!(movie_mode_previous_buttons & 0x40) && (movie_mode_buttons & 0x40)) {
            top += 20;
            if (top > count - 20) {
                top = count - 20;
            }
        }
        if (!(movie_mode_previous_buttons & 8) && (movie_mode_buttons & 8)) {
            hex = 1 - hex;
        }
        if (!(movie_mode_previous_buttons & 4) && (movie_mode_buttons & 4)) {
            names = 1 - names;
        }
        index = top;
        do {
            if (movie_mode_get_file_index_size(index) == 0) {
                if (hex) {
                    console_printf("No %4x NullFile\n", index);
                    index++;
                } else {
                    console_printf("No %4d NullFile\n", index);
                    index++;
                }
            } else {
                if (hex) {
                    console_printf("No %4x Sect%6x ", index, cd_get_file_sector(index + 1));
                } else {
                    console_printf("No %4d Sect%6d ", index, cd_get_file_sector(index + 1));
                }
                if (names) {
                    if ((s32)movie_mode_get_file_index_size(index) < 0) {
                        console_printf("[P%3d]\n", -movie_mode_get_file_index_size(index));
                        index++;
                    } else {
                        name = cd_get_pc_file_name(index + 1);
                        if (name != NULL) {
                            if (*name != 0) {
                                p = name;
                                do {
                                    if (*p == '\\') {
                                        name = p + 1;
                                    }
                                    p++;
                                } while (*p != 0);
                            }
                            console_printf("%s\n", name);
                        } else {
                            console_printf((char *)movie_mode_monitor_newline_text);
                        }
                        index++;
                    }
                } else if (hex) {
                    console_printf("Size%9x\n", movie_mode_get_file_index_size(index));
                    index++;
                } else {
                    console_printf((char *)movie_mode_fat_check_size_format, movie_mode_get_file_index_size(index));
                    index++;
                }
            }
        } while (index < top + 20);
        console_printf((char *)movie_mode_push_circle_text);
        if (movie_mode_heap_report_shown > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(movie_mode_current_buffer->ot);
        movie_mode_draw_backdrop(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->box, 8, 20, 304, 192);
        movie_mode_draw_menu_frame(movie_mode_current_buffer->ot, (POLY_G4 *)movie_mode_current_buffer->frame, 7, 19, 306, 194);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&movie_mode_current_buffer->draw);
        PutDispEnv(&movie_mode_current_buffer->disp);
        DrawOTag(&movie_mode_current_buffer->ot[31]);
    } while (button != 2);
    cd_select_directory(directory, offset);
}

/* "Size%9d\n". The original assembler left stray bytes (0x94, 0x08) in its
 * alignment padding; it is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", movie_mode_fat_check_size_format); /* 800704D4 */

/* 8007625C: The menu's movie test: play the selected movie with the display blanked
 * until it starts, then clear the screen and restore the menu's buffers.
 * The unused array reproduces the original's frame. */
void movie_mode_run_movie_test(void) {
    u8 unused[0x18];
    RECT screen;

    screen = movie_mode_screen_area;
    movie_mode_unskippable = 0;
    movie_mode_buttons = -1;
    if (movie_mode_movie_kind == 0) {
        cd_select_directory(0x18, 0);
        if (movie_mode_movie_index >= cd_get_directory_file_count(2)) {
            return;
        }
        SetDispMask(0);
    } else if (movie_mode_movie_kind == 1) {
        cd_select_directory(0x18, 1);
        if (movie_mode_movie_index >= cd_get_directory_file_count(1)) {
            return;
        }
        SetDispMask(0);
    } else if (movie_mode_movie_kind == 2) {
        return;
    }
    movie_mode_buffers[0].draw.isbg = 0;
    movie_mode_buffers[1].draw.isbg = 0;
    if (movie_mode_library_output_mode != 0) {
        movie_mode_buffers[0].disp.isrgb24 = 1;
        movie_mode_buffers[1].disp.isrgb24 = 1;
    }
    movie_mode_play_movie();
    VSync(0);
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    movie_mode_buffers[0].draw.isbg = 1;
    movie_mode_buffers[1].draw.isbg = 1;
    if (movie_mode_library_output_mode != 0) {
        movie_mode_buffers[0].disp.isrgb24 = 0;
        movie_mode_buffers[1].disp.isrgb24 = 0;
    }
}

/* 800763BC: Play the requested movie when its directory list holds it; `keep` stops
 * the buttons from ending it. Declared int without a value, as the original
 * (its return register stays live on every path). */
s32 movie_mode_play_requested_movie(u8 keep) {
    u8 unused[0x18];

    movie_mode_unskippable = keep;
    movie_mode_buttons = -1;
    if (movie_mode_movie_kind == 0) {
        cd_select_directory(0x18, 0);
        if (movie_mode_movie_index >= cd_get_directory_file_count(2)) {
            return;
        }
    } else if (movie_mode_movie_kind == 1) {
        cd_select_directory(0x18, 1);
        if (movie_mode_movie_index >= cd_get_directory_file_count(1)) {
            return;
        }
    } else if (movie_mode_movie_kind == 2) {
        return;
    }
    movie_mode_buffers[0].draw.isbg = 0;
    movie_mode_buffers[1].draw.isbg = 0;
    movie_mode_buffers[0].disp.isrgb24 = 1;
    movie_mode_buffers[1].disp.isrgb24 = 1;
    movie_mode_play_movie();
}

/* 80076488: Clear the screen, reopen the movie library and stream the movie, running
 * three decode steps per frame (their VSync counters are kept for the
 * monitor) until the frame callback or a button ends it; a movie that ended
 * on the second buffer is copied to the first. The short-file exit falls
 * off the end without a return value, as in the original. */
s32 movie_mode_play_movie(void) {
    u8 unused0[0x90]; /* unused in the original; reserves 144 bytes */
    RECT screen;
    u8 unused1[0x200]; /* unused in the original; reserves 512 bytes */
    RECT copy;
    s32 file;
    s32 select;
    s32 budget;
    s32 i;
    s32 before;
    s32 after;

    screen = movie_mode_screen_area;
    movie_mode_stop_timer = 0;
    movie_mode_decode_paused = 0;
    if (movie_mode_movie_kind == 0) {
        select = 0;
        file = movie_mode_movie_index + 3;
    } else {
        select = 1;
        file = movie_mode_movie_index + 2;
    }
    if (cd_get_file_size(file) == 0x18) {
        SetDispMask(1);
        cd_movie_request[2] = 0;
    } else {
        VSync(0);
        ClearImage(&screen, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        movie_mode_loaded_buffer = 0;
        movie_mode_shown_buffer = 0;
        movie_close();
        if (movie_mode_draw_rows > 0) {
            movie_mode_first_buffer_y = (240 - movie_mode_draw_rows) / 2;
        } else {
            movie_mode_first_buffer_y = 0;
        }
        movie_open(320, 240, 0x80, 16, 32, 0x800, movie_mode_library_output_mode);
        movie_split_display = 0;
        if (movie_mode_rewind_enabled != 0) {
            movie_start(file, movie_mode_start_sector, movie_mode_first_frame, movie_mode_last_frame, movie_mode_xa_channel, select, 1, 0,
                        movie_mode_first_buffer_y, 0, movie_mode_first_buffer_y + 240, movie_mode_draw_rows, movie_mode_on_frame_loaded);
        } else {
            movie_start(file, movie_mode_start_sector, movie_mode_first_frame, movie_mode_last_frame, movie_mode_xa_channel, select, 0, 0,
                        movie_mode_first_buffer_y, 0, movie_mode_first_buffer_y + 240, movie_mode_draw_rows, movie_mode_on_frame_loaded);
        }
        budget = 30;
        PutDrawEnv(&movie_mode_buffers[movie_mode_loaded_buffer].draw);
        PutDispEnv(&movie_mode_buffers[movie_mode_loaded_buffer].disp);
        SetDispMask(1);
        while (1) {
            if (movie_mode_decode_paused == 0) {
                for (i = 0; i < budget / 10; i++) {
                    before = VSync(1);
                    movie_poll();
                    after = VSync(1);
                    if (i * 2 + 1 < 32) {
                        movie_mode_unread_decode_vsync_times[i * 2] = before;
                        movie_mode_unread_decode_vsync_times[i * 2 + 1] = after;
                    }
                }
                budget %= 10;
            }
            budget += 30;
            movie_mode_read_playback_input();
            boot_check_soft_reset();
            VSync(0);
            PutDispEnv(&movie_mode_buffers[movie_mode_shown_buffer].disp);
            movie_mode_shown_buffer = movie_mode_loaded_buffer;
            if (movie_mode_stop_timer == 1) {
                break;
            }
            if (movie_mode_stop_timer >= 2) {
                movie_mode_stop_timer--;
            }
        }
        movie_stop();
        if (movie_mode_shown_buffer == 0) {
            DrawSync(0);
            VSync(0);
            copy.x = 0;
            copy.y = 240;
            copy.w = 480;
            copy.h = 240;
            MoveImage(&copy, 0, 0);
            DrawSync(0);
            VSync(0);
            PutDispEnv(&movie_mode_buffers[1].disp);
        }
        return 0;
    }
}

/* 800768D8: The movie library's frame callback: the buffer the frame went to; the
 * last frame ends the movie. */
void movie_mode_on_frame_loaded(u16 frame, u16 x, u16 y) {
    movie_mode_unread_loaded_frame = frame;
    if (movie_split_display == 1) {
        if (y != movie_mode_first_buffer_y) {
            movie_mode_loaded_buffer = 0;
        } else {
            movie_mode_loaded_buffer = 1;
        }
    } else if (y != movie_mode_first_buffer_y) {
        movie_mode_loaded_buffer = 0;
        movie_mode_shown_buffer = 0;
    } else {
        movie_mode_loaded_buffer = 1;
        movie_mode_shown_buffer = 1;
    }
    if (frame >= movie_mode_last_frame && movie_mode_rewind_enabled == 0) {
        movie_mode_stop_timer = 1;
    }
}

/* 800769A4: Read controller port 0 with a repeat after 90 frames held; Circle or Start
 * ends the movie in five frames, fading the CD volume, unless it is kept. */
void movie_mode_read_playback_input(void) {
    movie_mode_previous_buttons = movie_mode_buttons;
    movie_mode_buttons = pad_read_buttons(0);
    if (movie_mode_previous_buttons == movie_mode_buttons && movie_mode_previous_buttons != 0) {
        movie_mode_playback_hold_frames++;
        if (movie_mode_playback_repeat_timer < movie_mode_playback_hold_frames) {
            movie_mode_previous_buttons = 0;
            movie_mode_playback_repeat_timer = 1;
            movie_mode_playback_hold_frames = 0;
        }
    } else {
        movie_mode_playback_repeat_timer = 90;
        movie_mode_playback_hold_frames = 0;
    }
    if (!(movie_mode_previous_buttons & 0x40) && (movie_mode_buttons & 0x40) && movie_mode_unskippable == 0) {
        sound_set_cd_volume(0, 10);
        movie_mode_stop_timer = 5;
    }
    if (!(movie_mode_previous_buttons & 0x800) && (movie_mode_buttons & 0x800) && movie_mode_unskippable == 0) {
        sound_set_cd_volume(0, 10);
        movie_mode_stop_timer = 5;
    }
}

/* 80076AF0: Set up the geometry screen and the light and color matrices. */
void movie_mode_init_geometry(void) {
    SetGeomScreen(512);
    SetGeomOffset(160, 120);
    movie_mode_camera_translation.m[0][0] = 0x1000;
    movie_mode_camera_translation.m[1][1] = 0x1000;
    movie_mode_camera_translation.m[2][2] = 0x1000;
    movie_mode_camera_base_rotation.m[0][0] = 0x1000;
    movie_mode_camera_base_rotation.m[1][1] = 0x1000;
    movie_mode_camera_base_rotation.m[2][2] = 0x1000;
    movie_mode_unread_light_directions.m[0][0] = 0x93D;
    movie_mode_unread_light_directions.m[0][1] = -0x93D;
    movie_mode_unread_light_directions.m[0][2] = -0x93D;
    movie_mode_camera_translation.m[0][1] = 0;
    movie_mode_camera_translation.m[0][2] = 0;
    movie_mode_camera_translation.m[1][0] = 0;
    movie_mode_camera_translation.m[1][2] = 0;
    movie_mode_camera_translation.m[2][0] = 0;
    movie_mode_camera_translation.m[2][1] = 0;
    movie_mode_camera_base_rotation.m[0][1] = 0;
    movie_mode_camera_base_rotation.m[0][2] = 0;
    movie_mode_camera_base_rotation.m[1][0] = 0;
    movie_mode_camera_base_rotation.m[1][2] = 0;
    movie_mode_camera_base_rotation.m[2][0] = 0;
    movie_mode_camera_base_rotation.m[2][1] = 0;
    movie_mode_unread_light_directions.m[1][0] = 0;
    movie_mode_unread_light_directions.m[1][1] = 0;
    movie_mode_unread_light_directions.m[1][2] = 0;
    movie_mode_unread_light_directions.m[2][0] = 0;
    movie_mode_unread_light_directions.m[2][1] = 0;
    movie_mode_unread_light_directions.m[2][2] = 0;
    movie_mode_light_colors.m[0][0] = 0x969;
    movie_mode_light_colors.m[0][1] = 0;
    movie_mode_light_colors.m[0][2] = 0;
    movie_mode_light_colors.m[1][0] = 0x969;
    movie_mode_light_colors.m[1][1] = 0;
    movie_mode_light_colors.m[1][2] = 0;
    movie_mode_light_colors.m[2][0] = 0x969;
    movie_mode_light_colors.m[2][1] = 0;
    movie_mode_light_colors.m[2][2] = 0;
    SetColorMatrix(&movie_mode_light_colors);
    SetBackColor(0x60, 0x60, 0x60);
}

/* 80076C68: Update the camera and load it as the rotation and translation. The unused
 * local reproduces the original's frame. */
void movie_mode_load_camera(void) {
    u8 unused[0x18];

    movie_mode_aim_camera();
    SetRotMatrix(&movie_mode_world_to_screen);
    SetTransMatrix(&movie_mode_world_to_screen);
}

/* 80076CA4: Aim the camera from the eye at the target, rolled, and compose the world
 * to screen matrix. */
void movie_mode_aim_camera(void) {
    s32 dz;
    s32 dx;
    s32 dy;
    s32 dz2;
    s32 dx2;

    dx = movie_mode_camera_target.vx - movie_mode_camera_eye.vx;
    dz = movie_mode_camera_target.vz - movie_mode_camera_eye.vz;
    dz2 = dz * dz;
    dx2 = dx * dx;
    dy = movie_mode_camera_target.vy - movie_mode_camera_eye.vy;
    movie_mode_camera_angles.vx = ratan2(dy, SquareRoot0(dz2 + dx2));
    movie_mode_camera_angles.vy = -ratan2(dx, dz);
    gpu_build_rotation_matrix(&movie_mode_camera_angles, &movie_mode_camera_rotation);
    RotMatrixZ(movie_mode_camera_roll, &movie_mode_camera_rotation);
    MulMatrix2(&movie_mode_camera_base_rotation, &movie_mode_camera_rotation);
    movie_mode_camera_rotation.t[0] = 0;
    movie_mode_camera_rotation.t[1] = 0;
    movie_mode_camera_rotation.t[2] = SquareRoot0(dx2 + dy * dy + dz2);
    movie_mode_camera_translation.t[0] = movie_mode_camera_offset.vx - movie_mode_camera_target.vx;
    movie_mode_camera_translation.t[1] = movie_mode_camera_offset.vy - movie_mode_camera_target.vy;
    movie_mode_camera_translation.t[2] = movie_mode_camera_offset.vz - movie_mode_camera_target.vz;
    CompMatrix(&movie_mode_camera_rotation, &movie_mode_camera_translation, &movie_mode_world_to_screen);
}

const RECT movie_mode_screen_area = {0, 0, 640, 512}; /* 800704E0 */
