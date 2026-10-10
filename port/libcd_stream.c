/*
 * The CD data stream library (PsyQ libcd's St* members, linked into the
 * movie library, decomp/src/mdec): a ring of movie sectors filled by the CD
 * ready callback. The ring holds `size` 32-byte slots (each sector's STR
 * header; its first halfword becomes the slot's state: 0 free, 1 wrap to
 * the start, 2 frame complete, 3 filling, 4 handed out) followed by `size`
 * data areas of 0x7E0 bytes. A frame's sectors land in consecutive slots;
 * after its last one the data callback marks the frame complete for
 * StGetNext. The library's state keeps its original addresses.
 *
 * The ring's transfers read the drive's sector buffer (libcd.c
 * xem_cd_transfer) as DMA3 does: the header's 8 words without an
 * interrupt, then the 0x1F8 data words, interrupting after a frame's last
 * sector. The original's linear mode is left out: only StSetStream sets it,
 * to 0. In headerless mode (no Setmode 0x20, every shipped movie) the
 * original stores four uninitialized stack bytes as the slot's position;
 * here they are zero, and StGetBackloc does not read them in that mode.
 */
#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libapi.h"
#include "xem/port.h"
#include "xem/mdec.h"

void xem_cd_transfer(void *madr, s32 words, s32 interrupt);
long EnterCriticalSection(void);
void ExitCriticalSection(void);
int CdReady(int mode, u_char *result);

/* A ring slot: the STR sector header. */
typedef struct {
    u16 state;   /* 0x160 as read; then the slot's state */
    u16 kind;    /* bits 10-14: the channel */
    u16 sector;  /* sector of the frame */
    u16 sectors; /* sectors in the frame */
    u32 frame;
    u8 rest[0x10];
    u8 location[4]; /* where the sector was read */
} StSlot;

extern s32 commons_libcd_stream_headerless;
extern u8 movie_stream_location[4];
extern u32 movie_stream_frame;
extern s32 movie_stream_deferred;
extern StSlot *libcd_stream_current_slot;
extern u32 libcd_stream_frame_number;
extern s16 libcd_stream_sector_offset;
extern s32 libcd_stream_rgb24;
extern void (*libcd_stream_ready_callback)(void);
extern void (*libcd_stream_end_callback)(void);
extern s32 libcd_stream_reset_channel;
extern u32 libcd_stream_start_frame;
extern u32 libcd_stream_channel;
extern s32 libcd_stream_callback_busy;
extern s32 libcd_stream_linear_sector_count;
extern u32 libcd_stream_ring_write_index;
extern u32 libcd_stream_frame_start_index;
extern u32 libcd_stream_ring_read_index;
extern s32 libcd_stream_linear_mode;
extern u32 libcd_stream_end_frame;
extern s32 libcd_stream_mask;
extern u8 *libcd_stream_linear_buffer;
extern StSlot *libcd_stream_ring_address;
extern u32 libcd_stream_ring_size;
extern s32 libcd_stream_last_result;

#define ST_DATA_BYTES 0x7E0

static u8 *st_data(u32 index) {
    return (u8 *)&libcd_stream_ring_address[libcd_stream_ring_size] + index * ST_DATA_BYTES;
}

/* Free `count` slots from `first`. */
void init_ring_status(u32 first, u32 count) {
    u32 i;

    for (i = 0; i < count; i++) {
        *(u32 *)&libcd_stream_ring_address[first + i] = 0;
    }
}

void StClearRing(void) {
    libcd_stream_ring_read_index = 0;
    libcd_stream_frame_start_index = 0;
    libcd_stream_ring_write_index = 0;
    libcd_stream_callback_busy = 0;
    init_ring_status(0, libcd_stream_ring_size);
    movie_stream_deferred = 0;
    libcd_stream_sector_offset = 0;
    libcd_stream_frame_number = 0;
}

void StSetRing(u_long *ring_addr, u_long ring_size) {
    libcd_stream_ring_address = (StSlot *)ring_addr;
    libcd_stream_ring_size = ring_size;
    StClearRing();
}

/* Drop the stream's callbacks (and reset the data FIFO). */
void StUnSetRing(void) {
    EnterCriticalSection();
    CdDataCallback(NULL);
    CdReadyCallback(NULL);
    ExitCriticalSection();
}

/* Frames from `start` (0: any) to `end` (0: wrap the ring); `mask` 1 skips
 * sectors until the start frame. */
void StSetMask(u_long mask, u_long start, u_long end) {
    libcd_stream_mask = mask;
    libcd_stream_start_frame = start;
    libcd_stream_end_frame = end;
}

void StSetStream(u_long mode, u_long start_frame, u_long end_frame, void (*func1)(), void (*func2)()) {
    StSetMask(1, start_frame, end_frame);
    libcd_stream_linear_mode = 0;
    libcd_stream_ready_callback = (void (*)(void))func1;
    libcd_stream_rgb24 = mode & 1;
    libcd_stream_channel = 0;
    libcd_stream_reset_channel = 0;
    libcd_stream_sector_offset = 0;
    libcd_stream_frame_number = 0;
    libcd_stream_end_callback = (void (*)(void))func2;
}

/* The DMA callback after a frame's last sector: the frame is complete. */
void data_ready_callback(void) {
    StSlot *slot = &libcd_stream_ring_address[libcd_stream_frame_start_index];
    s32 i;

    slot->state = 2;
    for (i = 0; i < 4; i++) {
        movie_stream_location[i] = slot->location[i];
    }
    movie_stream_frame = slot->frame;
    libcd_stream_frame_start_index = libcd_stream_ring_write_index;
    if (libcd_stream_ready_callback != NULL) {
        libcd_stream_ready_callback();
    }
    libcd_stream_callback_busy = 0;
}

/* Give up the current frame's slots, from its first one. */
static void st_drop_frame(void) {
    libcd_stream_frame_number = 0;
    libcd_stream_sector_offset = 0;
    init_ring_status(libcd_stream_frame_start_index,
                     libcd_stream_ring_write_index - libcd_stream_frame_start_index);
    libcd_stream_ring_write_index = libcd_stream_frame_start_index;
    libcd_stream_current_slot->state = 0;
}

/* The CD ready callback: put the sector in the next slot. */
void StCdInterrupt(void) {
    u8 result[8];
    u8 location[4];
    StSlot *slot;
    u32 i;

    if (libcd_stream_callback_busy == 1) {
        return;
    }
    if (libcd_stream_rgb24 && xem_host_mdec_out_busy()) {
        /* The MDEC's output DMA holds the bus: its callback calls again. */
        movie_stream_deferred = 1;
        libcd_stream_last_result = 1;
        return;
    }
    if (CdReady(1, result) == 5) {
        return;
    }
    if (result[0] & 0x04) {
        libcd_stream_last_result = 3;
        return;
    }
    slot = &libcd_stream_ring_address[libcd_stream_ring_write_index];
    libcd_stream_current_slot = slot;
    if (slot->state != 0) {
        libcd_stream_last_result = 4;
        return;
    }
    for (i = 0; i < 4; i++) {
        location[i] = 0;
    }
    if (commons_libcd_stream_headerless == 0) {
        u8 header[12];

        xem_cd_transfer(header, 3, 0);
        for (i = 0; i < 4; i++) {
            location[i] = header[i];
        }
    }
    xem_cd_transfer(slot, 8, 0);
    for (i = 0; i < 4; i++) {
        slot->location[i] = location[i];
    }
    if (libcd_stream_mask == 1 && libcd_stream_start_frame != 0) {
        u16 frame = slot->frame;

        slot->state = 0;
        if (libcd_stream_start_frame != frame) {
            return;
        }
        libcd_stream_mask = 0;
    }
    slot = libcd_stream_current_slot;
    if (slot->state != 0x160 || ((slot->kind >> 10) & 0x1F) != libcd_stream_channel) {
        libcd_stream_last_result = 5;
        slot->state = 0;
        return;
    }
    if ((u16)libcd_stream_sector_offset != slot->sector ||
        (libcd_stream_frame_number != 0 && libcd_stream_frame_number != (u16)slot->frame)) {
        st_drop_frame();
        libcd_stream_last_result = 6;
        return;
    }
    if (slot->sector == 0) {
        libcd_stream_sector_offset = 0;
        libcd_stream_frame_number = (u16)slot->frame;
        if (libcd_stream_end_frame != 0 && libcd_stream_frame_number >= libcd_stream_end_frame) {
            st_drop_frame();
            libcd_stream_mask = 1;
            if (libcd_stream_end_callback != NULL) {
                libcd_stream_end_callback();
            }
            libcd_stream_last_result = 7;
            return;
        }
        if (libcd_stream_ring_size - libcd_stream_ring_write_index - 1 < slot->sectors) {
            /* The frame does not fit before the ring's end. */
            if (libcd_stream_end_frame == 0) {
                slot->state = 1;
                libcd_stream_mask = 1;
                if (libcd_stream_end_callback != NULL) {
                    libcd_stream_end_callback();
                }
                libcd_stream_last_result = 8;
                return;
            }
            if ((s16)libcd_stream_ring_address[0].state != 0) {
                slot->state = 0;
                libcd_stream_last_result = 9;
                return;
            }
            /* Wrap: the slot is marked and its header moves to the start. */
            slot->state = 1;
            libcd_stream_ring_write_index = 0;
            for (i = 0; i < 8; i++) {
                ((u32 *)libcd_stream_ring_address)[i] = ((u32 *)slot)[i];
            }
            libcd_stream_current_slot = libcd_stream_ring_address;
        }
        libcd_stream_frame_start_index = libcd_stream_ring_write_index;
    }
    libcd_stream_last_result = 10;
    libcd_stream_sector_offset++;
    libcd_stream_linear_buffer = st_data(libcd_stream_ring_write_index);
    slot = libcd_stream_current_slot;
    if (slot->sectors - 1 == slot->sector) {
        /* The frame's last sector: its transfer's end completes the frame. */
        libcd_stream_callback_busy = 1;
        xem_cd_transfer(libcd_stream_linear_buffer, 0x1F8, 1);
        libcd_stream_sector_offset = 0;
        libcd_stream_frame_number = 0;
        libcd_stream_channel = libcd_stream_reset_channel;
    } else {
        xem_cd_transfer(libcd_stream_linear_buffer, 0x1F8, 0);
    }
    libcd_stream_current_slot->state = 3;
    libcd_stream_ring_write_index++;
}

/* The ready callback CdRead2 installs (its arguments are unused). */
void StCdInterrupt2(u8 intr, u8 *result) {
    StCdInterrupt();
}

/* The next complete frame: its data and first slot; 1 when none is ready. */
u_long StGetNext(u_long **addr, u_long **header) {
    StSlot *slot = &libcd_stream_ring_address[libcd_stream_ring_read_index];

    if (slot->state == 1) {
        libcd_stream_ring_read_index = 0;
        slot->state = 0;
        slot = &libcd_stream_ring_address[0];
    }
    if (slot->state != 2) {
        return 1;
    }
    slot->state = 4;
    *addr = (u_long *)st_data(libcd_stream_ring_read_index);
    *header = (u_long *)slot;
    return 0;
}

/* Free the frame whose data is at `base`; 1 when it was not handed out. */
u_long StFreeRing(u_long *base) {
    s32 index = ((u8 *)base - (u8 *)&libcd_stream_ring_address[libcd_stream_ring_size]) / ST_DATA_BYTES;
    StSlot *slot = &libcd_stream_ring_address[index];
    s32 count;
    s32 i;

    if ((s16)slot->state != 4) {
        return 1;
    }
    count = (s16)slot->sectors;
    for (i = 0; i < count; i++) {
        libcd_stream_ring_address[index + i].state = 0;
    }
    libcd_stream_ring_read_index = index + (count > 0 ? count : 0);
    return 0;
}

/* The position after the last complete frame and its number; -1 without
 * sector headers. */
int StGetBackloc(CdlLOC *loc) {
    if (commons_libcd_stream_headerless != 0) {
        return -1;
    }
    CdIntToPos(CdPosToInt((CdlLOC *)movie_stream_location) + 1, loc);
    return movie_stream_frame;
}

/* Setmode `mode`; with bit 0x100 the ring's callbacks (sector headers
 * stored with Setmode 0x20); then ReadS. */
int CdRead2(long mode) {
    u8 param[4];

    param[0] = mode;
    CdControl(0x0E, param, NULL);
    if (mode & 0x100) {
        commons_libcd_stream_headerless = (mode & 0x20) ? 0 : 1;
        CdDataCallback(data_ready_callback);
        CdReadyCallback(StCdInterrupt2);
    }
    return CdControl(0x1B, NULL, NULL);
}
