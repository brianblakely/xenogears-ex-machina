#ifndef RESIDENT_CD_H
#define RESIDENT_CD_H

#include "common.h"
#include "psyq/libcd.h"

/* Resident disc access (0x80028230-0x8002a68c): the file index, reads of
 * files and file lists, the CD command state and the PC file server. */

/* A file-list entry for 80029afc: file index and destination. */
typedef struct FileRequest {
    u16 file;
    void *destination;
} FileRequest;

/* One loaded file: its file number and the heap block holding its data. A
 * zero id terminates a table. */
typedef struct {
    u16 id;
    void *data;
} FileEntry;

extern s32 cd_max_list_gap_sectors;         /* largest sector gap read through between list files */
extern s32 cd_file_out_of_order_count;      /* out-of-order sectors in single-file reads */
extern s32 cd_list_out_of_order_count;      /* out-of-order sectors in list reads */
extern s32 cd_stream_out_of_order_count;    /* out-of-order sectors in stream reads */
extern u8 *cd_file_index;                   /* file index: 7 bytes per file */
extern u16 *cd_directory_table;             /* directory table: first file of each directory, 1-based */
extern s32 cd_read_bytes_left;              /* bytes of the current read */
extern s32 cd_pending_read_count;
extern s32 cd_remaining_list_file_count;    /* files in the current list */
extern s32 cd_next_sector;                  /* sector of the current read */
extern void *cd_read_destination;           /* destination of the current read */
extern FileRequest *cd_current_file_list;   /* the file list being read */
extern s32 cd_read_cursor;
extern s32 cd_selected_directory;           /* selected directory (first file - 1) */
extern s32 cd_reading_directory;            /* second directory selection */
extern s32 cd_command_state;                /* CD command state (8002a68c) */
extern s32 cd_retry_reason;                 /* retry reason of the failed command */
extern s32 cd_stop_requested;
/* The current read's mode (the read calls' mode argument, or a stop's
 * reason): when the read ends or stops, 8002a394 seeks to that file, or
 * pauses for 0, as nearly every caller passes (the movie mode's read check
 * passes 1, the movie library its XA channel); the XA filter takes its low
 * byte as the channel (8002a68c). */
extern s32 cd_read_mode;
extern s32 cd_list_skipping_gap;
extern u8 cd_movie_request_kind;
extern u8 cd_movie_request_index;
extern u8 cd_movie_request_next_mode;
extern u8 cd_movie_request_unskippable;
/* The requested movie's last frame, a common the resident defines and no
 * resident code addresses: ovl3087's movie request sets it
 * (battle_event_script_request_movie), the movie mode takes it as the last
 * frame when the request kind's bit 7 is set (movie_mode_main, lhu), and mdec's
 * movie_start clears it when it streams from the PC file server. */
extern u16 cd_movie_request_last_frame;
extern char *cd_pc_file_names;   /* PC file server name table (64 bytes per file), or NULL */
extern s32 cd_pc_file_descriptor;
extern s32 cd_stat_setloc_count, cd_stat_command_ok_count, cd_stat_command_fail_count, cd_stat_retry_setloc_count, cd_stat_retry_fail_count, cd_stat_lesmem_count;
extern s32 cd_stat_error_limit_count, cd_stat_stop_ok_count, cd_stat_stop_fail_count;
extern s32 cd_error_count;

void cd_init_disc_access(u8 *files, u16 *directories, u32 mode);
void cd_shutdown_disc_access(void);
s32 cd_select_directory(s32 group, s32 index);
s32 cd_get_selected_directory(s32 *group, s32 *index);
s32 cd_get_disc_number(void);
s32 cd_get_relative_directory(s32 group, s32 index);
s32 cd_get_pending_read_count(void); /* disc busy */
s32 cd_get_file_size(s32 file);
s32 cd_get_aligned_file_size(s32 file); /* file size rounded up to words */
s16 cd_get_directory_file_count(s32 file);
char *cd_get_pc_file_name(s32 file);
s32 cd_get_file_sector(s32 file);
s32 cd_sync_reads(s32 mode);
void stream_merge_free_slots(s32 index);
s32 cd_read_raw_sectors(s32 sector, void *destination, s32 size, s32 mode, s32 flags);
s32 cd_read_file(s32 file, void *destination, s32 mode, s32 flags);
s32 cd_start_read(s32 file, void *destination, s32 mode, s32 flags);
s32 cd_read_file_list(FileRequest *list, s32 mode, s32 unused);
void cd_seek_or_pause_if_idle(s32 file);
void cd_seek_or_pause(s32 file);
void cd_stop_read(s32 reason);
void cd_free_file_table(FileEntry *table);
FileEntry *cd_alloc_directory_file_table(s32 first, FileEntry *table);

/* More of the disc and file services. */
void *cd_load_pc_file(char *name, s32 *size);
s32 cd_get_read_bytes_left(void);
u8 *stream_get_next_chunk(void);
u16 stream_release_chunk(u8 *chunk);
u16 stream_release_movie_frame(u8 *chunk);

#endif
