#ifndef RESIDENT_STREAM_H
#define RESIDENT_STREAM_H

#include "common.h"

/* Disc stream ring (EVID: analysis/formats/disc-stream-source.md). The ring
 * header holds the slot count, then eight bytes per slot. */
typedef struct {
    u16 state;
    u16 sequence;
    u16 length; /* run of free slots starting here */
    u16 w6;
} StreamSlot;

typedef struct {
    s32 count;
    StreamSlot slots[1];
} StreamRing;

/* Header of a movie frame's first sector: the frame spans `sectors` ring
 * slots; the headers of all its sectors come first (0x20 bytes each), then
 * their 0x7e0-byte payloads. */
typedef struct {
    u8 unknown0[6];
    u16 sectors; /* +0x6 */
    u16 word8;   /* +0x8 */
} StreamFrame;

extern StreamRing *stream_current_ring; /* the ring */
extern StreamSlot *stream_slots; /* its slots */
extern s32 stream_slot_count;         /* its slot count */
extern u16 stream_next_chunk_sequence;
extern u16 stream_next_store_sequence;
extern u16 stream_next_complete_sequence;
extern u16 stream_frame_number;

StreamRing *stream_select_ring(StreamRing *ring);
s32 stream_reset_ring(void);
void cd_copy_file_sector(u8 intr, u8 *result);
void stream_store_sector(u8 intr, u8 *result);
void stream_mark_sector_complete(void);
void stream_store_image_sector(u8 intr, u8 *result);
void stream_load_image_strip(void);
/* Defined without parameters; 80029EB0 calls it with the (0, 0) of a CD
 * callback. */
void stream_read_pc_sector();
/* Defined without parameters; 80029EB0 calls it like a CD callback. */
void stream_load_pc_image_strip();
void cd_advance_command_state(u8 intr, u8 *result);
void cd_copy_list_sector(u8 intr, u8 *result);
void cd_update_pending_read_count(void);

/* More of the stream services. */
s32 cd_has_pc_file_server(void);
s32 stream_get_next_movie_frame(u8 **data, StreamFrame **frame);
StreamRing *stream_create_ring(s32 count, s32 mode);

#endif
