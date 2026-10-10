#ifndef WORLDMAP_STREAM_H
#define WORLDMAP_STREAM_H

/* The world map's stream reader (worldmap_movement_terrain), which reads the terrain
 * blocks around the camera in the background: a ring of 16 request lists,
 * from disc (sectors read through the CD callbacks) or from the host's files
 * (PC file server), each list sorted by position. */

#include "worldmap.h"
#include "psyq/libcd.h"

/* A disc read request: sector, bytes and destination. */
typedef struct DiscReadRequest {
    s32 sector;
    s32 bytes;
    u8 *destination;
} DiscReadRequest;

/* A host-file read request: name, offset, bytes and destination. */
typedef struct HostReadRequest {
    char *path;
    s32 offset;
    s32 bytes;
    u8 *destination;
} HostReadRequest;

extern void *worldmap_stream_disc_buffer, *worldmap_stream_host_buffer; /* disc and host-file request buffers */
extern DiscReadRequest *worldmap_stream_disc_lists[16]; /* submitted disc request lists */
extern HostReadRequest *worldmap_stream_host_lists[16]; /* submitted host-file request lists */
extern s32 worldmap_stream_request_count, worldmap_stream_write_slot, worldmap_stream_read_slot; /* ring positions */
extern s8 worldmap_cd_sync_result[8];
extern s32 worldmap_stream_state, worldmap_stream_wait; /* reader state and its wait */

/* The disc request being read. */
extern DiscReadRequest *volatile worldmap_stream_next_request; /* next disc request (shared with the CD callbacks) */
extern s32 worldmap_stream_unread_cleared_word_1, worldmap_stream_unread_cleared_word_2, worldmap_stream_status_error_count, worldmap_stream_read_error_count;
extern s32 worldmap_stream_drive_sector, worldmap_stream_wanted_sector, worldmap_stream_bytes_left;
extern u8 *worldmap_stream_destination;                        /* destination of the next sector's data */
extern u32 worldmap_stream_sectors_left;                       /* sectors left */
extern s32 worldmap_stream_sector_header[3];                   /* sector header */
extern CdlLOC worldmap_stream_seek_position;                   /* request position */
extern void *worldmap_stream_drain_buffer;                     /* unused bytes drained from the final CD sector */

void worldmap_stream_reset(void);         /* reset the queue, allocate its buffers */
void worldmap_stream_free(void);          /* free them */
void worldmap_stream_wait_for_slot(void); /* wait for a free write slot */
s32 worldmap_stream_add_disc_request(s32 sector, s32 bytes, u8 *destination);
s32 worldmap_stream_add_host_request(char *path, s32 offset, s32 bytes, u8 *destination);
s32 worldmap_stream_submit_disc_list(void);
s32 worldmap_stream_submit_host_list(void);
s32 worldmap_stream_count_queued(void);   /* lists queued */
void worldmap_stream_drain(void);         /* drain the queue */
s32 worldmap_stream_step(void);           /* stream step: worldmap_stream_step_reader status */

#endif
