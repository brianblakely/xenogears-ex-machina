#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libsn.h"
#include "movie.h"

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_slice_decoded);

/* Open the library for a `width` x `height` movie (16-bit VRAM units).
 * `scale` sizes the two run-level buffers (width * height * scale / 128
 * bytes), `slice` is the macroblock column width, `sectors` the ring length,
 * `limit` the decode-call limit; `mode` bit 0 selects 24-bit output.
 * Returns 0, or -1 when no ring could be allocated. */
#ifdef NON_MATCHING
/* Same instructions; parameters limit, mode and the masked height land in s7/s1/s6 in the original and s1/s6/s7 here. */
s32 movie_open(u16 width, u16 height, u16 scale, u16 slice, u16 sectors, u16 limit, u16 mode) {
    s32 product;
    s32 slice_bytes;

    if (func_8002C3D8() != 0) {
        movie_host_stream = 1;
    } else {
        movie_host_stream = 0;
    }
    movie_player_state = 0;
    if (movie_split_display != 0) {
        slice = width;
    }
    DecDCTReset(0);
    product = width * height * (scale << 1);
    movie_vlc_limit = limit;
    movie_color_mode = mode & 3;
    movie_vlc_buffers[0] = func_80031BDC(product / 256, 0);
    movie_vlc_buffers[1] = func_80031BDC(product / 256, 0);
    if (movie_color_mode & 1) {
        slice = (u32)(slice * 3) >> 1;
        width = (u32)(width * 3) >> 1;
    }
    movie_image_width = width;
    movie_image_height = height;
    slice_bytes = (slice * height) * 2;
    movie_slice_buffer0 = func_80031BDC(slice_bytes, 0);
    movie_slice_buffer1 = func_80031BDC(slice_bytes, 0);
    movie_display_buffers[0][0] = 0;
    movie_display_buffers[0][1] = 0;
    movie_display_buffers[0][2] = width;
    movie_display_buffers[0][3] = height;
    movie_display_buffers[1][0] = 0;
    movie_display_buffers[1][1] = 0;
    movie_display_buffers[1][2] = width;
    movie_display_buffers[1][3] = height;
    movie_slice_rects[0][0] = 0;
    movie_slice_rects[0][1] = 0;
    movie_slice_rects[0][2] = slice;
    movie_slice_rects[0][3] = height;
    movie_slice_rects[1][0] = 0;
    movie_slice_rects[1][1] = 0;
    movie_slice_rects[1][2] = slice;
    movie_slice_rects[1][3] = height;
    if (movie_host_stream != 0) {
        movie_ring_buffer = func_8002A260(sectors, 0);
    } else {
        movie_ring_buffer = func_80031BDC(sectors << 11, 0);
        StSetRing(movie_ring_buffer, sectors);
    }
    if (movie_ring_buffer == NULL) {
        return -1;
    }
    movie_player_state = 1;
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_open);
#endif

/* Start streaming `file` from `sector`: frames `first_frame` to `last_frame`,
 * CD-XA audio of `channel` when `select` bit 0 is set, `hold` keeps the first
 * frame, `x0, y0, x1, y1` place the two display buffers, `rows` limits the
 * rows a slice loads, and `callback` receives each loaded frame. */
#ifdef NON_MATCHING
/* Register pressure differs: the original keeps file in $fp and spills sector, both frame bounds and the callback (frame 0x70). */
void movie_start(u16 file, s32 sector, u16 first_frame, u16 last_frame, u16 channel, s32 select,
                 u16 hold, u16 x0, u16 y0, u16 x1, u16 y1, u16 rows, void (*callback)()) {
    CdlFILTER filter;

    if (movie_player_state == 0) {
        return;
    }
    func_800284B4(&movie_saved_directory[0], &movie_saved_directory[1]);
    DecDCToutCallback(movie_slice_decoded);
    if (hold != 0) {
        movie_player_state = 2;
    } else {
        movie_player_state = 1;
    }
    movie_restarted = 0;
    movie_row_limit = rows;
    movie_file = file;
    movie_start_sector = sector;
    if (movie_host_stream != 0) {
        if (select & 1) {
            movie_xa_channel = channel;
            movie_cd_mode = 0x248;
        } else {
            movie_xa_channel = 1;
            movie_cd_mode = 0x200;
        }
        func_80028AAC();
        D_8005A4B8 = 0;
        D_80062514 = 0;
    } else {
        if (select & 1) {
            /* Real-time CD-XA audio: the drive plays it into the SPU. */
            movie_xa_channel = channel;
            movie_cd_mode = 0x148;
            filter.file = 1;
            filter.chan = movie_xa_channel;
            while (CdControlB(CdlSetfilter, (u8 *)&filter, NULL) == 0) {
            }
        } else {
            movie_cd_mode = 0x100;
        }
        StSetStream(movie_color_mode & 1, first_frame, -1, NULL, NULL);
    }
    if (select & 2) {
        movie_cd_mode &= ~0x40;
    }
    movie_frame_callback = callback;
    if (movie_color_mode & 1) {
        /* Three bytes per pixel: VRAM units are two thirds of pixels. */
        movie_display_buffers[0][0] = (x0 * 3) >> 1;
        movie_display_buffers[1][0] = (x1 * 3) >> 1;
        movie_display_buffers[0][1] = y0;
        movie_slice_width = 24;
    } else {
        movie_display_buffers[0][0] = x0;
        movie_display_buffers[0][1] = y0;
        movie_display_buffers[1][0] = x1;
        movie_slice_width = 16;
    }
    movie_display_buffers[1][1] = y1;
    movie_mdec_idle = 1;
    movie_frame_waiting = 1;
    movie_shown_frame = -1;
    movie_load_restart = 1;
    movie_load_enabled = 1;
    movie_first_frame = first_frame;
    movie_vlc_buffer_index = 0;
    movie_slice_buffer_index = 0;
    movie_decode_display = 0;
    movie_load_display = 0;
    movie_frame_width = 0;
    movie_frame_height = 0;
    movie_stall_count = 0;
    movie_vlc_pending = 0;
    movie_skipped_frames = 0;
    movie_end_frame = last_frame;
    movie_restart(file, sector, movie_xa_channel, movie_cd_mode, NULL);
}
#else
INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_start);
#endif

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_next_bitstream);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_decode);

/* One step of playback: fade the CD audio in once past the first frame and
 * out three frames before the end, stop or loop at the end, decode, and seek
 * again from the ring's last sector when no frame arrived for 2161 polls. */
void movie_poll(void) {
    CdlLOC location;
    CdlLOC *seek;
    s32 frame;

    if (movie_player_state <= 0) {
        return;
    }
    if (movie_shown_frame > movie_first_frame && movie_fade_in_pending != 0) {
        movie_fade_in_pending = 0;
        func_80038D18(0x7FFF, 0x28);
    }
    if (movie_shown_frame >= movie_end_frame - 3 && movie_fade_out_pending != 0) {
        movie_fade_out_pending = 0;
        func_80038D18(0, 0x28);
    }
    if (movie_shown_frame >= movie_end_frame) {
        if (movie_player_state == 1) {
            movie_stop();
        } else {
            movie_restarted = 0;
            movie_shown_frame = -1;
            movie_restart(movie_file, movie_start_sector, movie_xa_channel, movie_cd_mode, NULL);
        }
    }
    if (movie_mdec_idle != 0 || movie_frame_waiting != 0 || movie_vlc_pending != 0) {
        movie_decode();
    }
    if (movie_stall_count >= 0x871) {
        movie_stall_count = 0;
        frame = StGetBackloc(&location);
        D_8005A4B8 = frame;
        D_8005A4DC++;
        D_8005A4A8 = CdPosToInt(&location);
        D_8005A4B4 = movie_start_sector;
        if (movie_end_frame < frame || frame <= 0) {
            seek = NULL;
        } else {
            seek = &location;
        }
        movie_shown_frame = -1;
        movie_restart(movie_file, movie_start_sector, movie_xa_channel, movie_cd_mode, seek);
    }
}

/* Seek the stream to `sector` of `file` (or to `location` when given) and read
 * from there in `mode` at double speed; the current directory is kept. The
 * host-file stream instead reopens the file and seeks within it. */
void movie_restart(s32 file, s32 sector, s32 arg2, s32 mode, CdlLOC *location) {
    CdlLOC position;
    CdlLOC *seek;
    s32 kept_directory;
    s32 kept_offset;

    func_80038D18(0, 0);
    func_8002A498(0);
    func_80028A60(0);
    func_800284B4(&kept_directory, &kept_offset);
    func_80028470(movie_saved_directory[0], movie_saved_directory[1]);
    movie_fade_in_pending = 1;
    movie_fade_out_pending = 1;
    if (movie_host_stream != 0) {
        func_800295D8(file, movie_ring_buffer, arg2, mode);
        if (mode & 8) {
            PClseek(D_8004FE4C, sector * 0x920, 0);
        } else {
            PClseek(D_8004FE4C, sector << 11, 0);
        }
    } else {
        mode |= 0x80;
        CdIntToPos(func_800289D0(file) + sector, &position);
        if (location != NULL) {
            seek = location;
        } else {
            seek = &position;
        }
        while (CdControlB(CdlSetloc, (u8 *)seek, NULL) == 0) {
        }
        while (CdRead2(mode) == 0) {
        }
    }
    func_80028470(kept_directory, kept_offset);
}

/* Stop the stream: silence the CD input, stop the MDEC, drop the ring's
 * callbacks, pause the drive and restore the resident read mode. */
void movie_stop(void) {
    func_80038D18(0, 0);
    func_8002A498(0);
    DecDCToutCallback(NULL);
    DecDCTReset(0);
    movie_player_state = -1;
    if (movie_host_stream != 0) {
        func_80028AAC();
    } else {
        StUnSetRing();
        while (CdControlB(CdlPause, NULL, NULL) == 0) {
        }
        func_8002A428(0xA0);
    }
    func_80028A60(0);
}

/* Stop, then release the run-level and slice buffers and the ring. */
void movie_close(void) {
    movie_stop();
    func_800320E8(movie_vlc_buffers[0]);
    func_800320E8(movie_vlc_buffers[1]);
    func_800320E8(movie_slice_buffer0);
    func_800320E8(movie_slice_buffer1);
    func_800320E8(movie_ring_buffer);
    movie_vlc_buffers[0] = NULL;
    movie_vlc_buffers[1] = NULL;
    movie_slice_buffer0 = NULL;
    movie_slice_buffer1 = NULL;
    movie_ring_buffer = NULL;
}

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlcBuild);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTReset);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTGetEnv);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTPutEnv);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTBufSize);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTin);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTout);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTinSync);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCToutSync);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTinCallback);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCToutCallback);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", MDEC_reset);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", MDEC_in);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", MDEC_out);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", MDEC_in_sync);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", MDEC_out_sync);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", MDEC_status);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", timeout);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4C94);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlcSize);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlc);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D502C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlc2);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", _EncSPU_encode);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", _EncSPU);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", EncSPU);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StSetRing);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", CdRead2);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StCdInterrupt2);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StClearRing);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StUnSetRing);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", data_ready_callback);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StGetBackloc);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StSetStream);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StFreeRing);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", init_ring_status);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StGetNext);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StSetMask);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StCdInterrupt);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", mem2mem);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", dma_execute);
