/* The movie library (disc file 19, loaded at 0x801d3000 by the movie mode,
 * decomp/src/movie): a CD or host-file STR stream player. It decodes each
 * frame's bitstream into run-level data, lets the MDEC expand it slice by
 * slice into two VRAM display buffers, reports each loaded frame to the
 * caller and fades the CD-XA audio in and out.
 *
 * The player is this unit's C: text 801D30C4-801D444C, its initialized
 * statics (data 801D68B4-801D68D0) and its uninitialized ones (801E8910-
 * 801E89AC). The PsyQ libpress, libds and libcd streaming members that the
 * linker placed after its code (801D444C-801D68B4, located by
 * tools/psyq_signatures.py) stay SDK assembly below, and its five commons
 * are linked among libcd's from commons/ (mdec.classification.txt). */
#include "movie.h"

/* The player's initialized statics open the image's .data (all zero). */
s32 movie_split_display = 0;        /* frames span both display buffers */
u16 movie_frame_width = 0;          /* last frame header width */
u16 movie_frame_height = 0;
u16 movie_image_width = 0;          /* 16-bit VRAM units */
u16 movie_image_height = 0;
volatile s32 movie_vlc_pending = 0; /* a frame's decode is resumable */
s32 movie_vlc_limit = 0;            /* halfwords per decode call */
s32 movie_color_mode = 0;           /* bit 0: 24-bit output */
s32 movie_end_frame = 0;

/* The player's uninitialized variables (801e8910..801e89ac) open the .bss,
 * which the file holds as zeros after the libraries' .data and ahead of the
 * libcd modules' .bss, each in a slot of whole words (decomp/Makefile): the u8
 * flags at 801e8958..801e8968 sit four bytes apart. */
static MovieSectorHeader *movie_decoded_bitstream;
static u_long *movie_frame_bitstream;
static MovieDecoder movie_decoder;
static u8 movie_mdec_idle;         /* the MDEC finished a frame */
static u8 movie_frame_waiting;     /* no frame was in the ring */
static u8 movie_restarted;
static s8 movie_player_state;
static u8 movie_host_stream;       /* resident host-file table in use */
static s32 movie_start_sector;
static s32 movie_cd_mode;
static u16 movie_file;
static u16 movie_xa_channel;
static s16 movie_row_limit;        /* rows a slice loads at most */
static s32 movie_loaded_frame;     /* frame the MDEC decodes */
static s32 movie_first_frame;
static s32 movie_shown_frame;      /* last frame fully loaded */
static void *movie_ring_buffer;    /* the stream ring */
static void (*movie_frame_callback)(u16 frame, u16 x, u16 y); /* a frame is loaded */
static s32 movie_ring_frame;       /* frame of the last ring bitstream */
static s32 movie_load_enabled;     /* slices go to VRAM */
static s32 movie_saved_directory; /* directory group of the movie files */
static s32 movie_saved_index;     /* and index */
static s32 movie_fade_in_pending;
static s32 movie_fade_out_pending;

/* The MDEC output DMA's completion callback: load the slice just decoded into
 * VRAM (column by column for a split display), then start the MDEC on the
 * next slice or, past the frame's last column, report the loaded frame. */
void movie_slice_decoded(void) {
    RECT column;
    s16 rows;
    s32 i;
    s32 k;

    if (movie_host_stream == 0 && (movie_color_mode & 1) && movie_stream_deferred != 0) {
        StCdInterrupt();
        movie_stream_deferred = 0;
    }
    rows = movie_decoder.slice[movie_decoder.load_display].h;
    if (movie_row_limit >= 0 && movie_row_limit < rows) {
        movie_decoder.slice[movie_decoder.load_display].h = movie_row_limit;
    }
    if (movie_split_display != 0) {
        column.y = movie_decoder.slice[movie_decoder.load_display].y;
        column.w = movie_slice_width;
        column.h = movie_decoder.slice[movie_decoder.load_display].h;
        for (i = 0; i < movie_decoder.slice[movie_decoder.load_display].w / (s16)movie_slice_width; i++) {
            column.x = movie_decoder.slice[movie_decoder.load_display].x;
            movie_decoder.slice[movie_decoder.load_display].x += movie_slice_width;
            if (movie_load_enabled != 0) {
                LoadImage(&column,
                          (u_long *)((u16 *)movie_decoder.slice_buffers[movie_decoder.slice_index] +
                                  i * rows * (s16)movie_slice_width));
            }
        }
    } else {
        if (movie_load_enabled != 0) {
            LoadImage(&movie_decoder.slice[movie_decoder.load_display],
                      movie_decoder.slice_buffers[movie_decoder.slice_index]);
        }
        movie_decoder.slice[movie_decoder.load_display].x +=
            movie_decoder.slice[movie_decoder.load_display].w;
    }
    movie_decoder.slice[movie_decoder.load_display].h = rows;
    movie_decoder.slice_index = 1 - movie_decoder.slice_index;
    k = movie_decoder.load_display;
    if (movie_decoder.slice[k].x >= movie_decoder.display[k].right) {
        if (movie_frame_callback != NULL) {
            if (movie_color_mode & 1) {
                /* Back from VRAM units to pixels: two thirds. */
                movie_frame_callback(movie_loaded_frame, (movie_decoder.display[k].x * 2) / 3,
                                     movie_decoder.display[k].y);
            } else {
                movie_frame_callback(movie_loaded_frame, movie_decoder.display[k].x,
                                     movie_decoder.display[k].y);
            }
        }
        movie_mdec_idle = 1;
        movie_load_enabled = movie_load_restart;
        movie_shown_frame = movie_loaded_frame;
        movie_decoder.load_display = 1 - movie_decoder.load_display;
    } else {
        DecDCTout(movie_decoder.slice_buffers[movie_decoder.slice_index],
                  (movie_decoder.slice[k].w * movie_decoder.slice[k].h) / 2);
    }
}

/* Open the library for a `width` x `height` movie (16-bit VRAM units).
 * `scale` sizes the two run-level buffers (width * height * scale / 128
 * bytes), `slice` is the macroblock column width, `sectors` the ring length,
 * `limit` the decode-call limit; `mode` bit 0 selects 24-bit output.
 * Returns 0, or -1 when no ring could be allocated. */
s32 movie_open(u16 width, u16 height, u16 scale, u16 slice, u16 sectors, u16 limit, u16 mode) {
    s32 product;
    s32 slice_bytes;

    if (cd_has_pc_file_server() != 0) {
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
    movie_color_mode = mode & 3;
    movie_vlc_limit = limit;
    movie_decoder.vlc_buffers[0] = heap_alloc(product / 256, 0);
    movie_decoder.vlc_buffers[1] = heap_alloc(product / 256, 0);
    if (movie_color_mode & 1) {
        slice = (u32)(slice * 3) >> 1;
        width = (u32)(width * 3) >> 1;
    }
    movie_image_width = width;
    movie_image_height = height;
    slice_bytes = (slice * height) * 2;
    movie_decoder.slice_buffers[0] = heap_alloc(slice_bytes, 0);
    movie_decoder.slice_buffers[1] = heap_alloc(slice_bytes, 0);
    movie_decoder.display[0].x = 0;
    movie_decoder.display[0].y = 0;
    movie_decoder.display[0].right = width;
    movie_decoder.display[0].bottom = height;
    movie_decoder.display[1].x = 0;
    movie_decoder.display[1].y = 0;
    movie_decoder.display[1].right = width;
    movie_decoder.display[1].bottom = height;
    movie_decoder.slice[0].x = 0;
    movie_decoder.slice[0].y = 0;
    movie_decoder.slice[0].w = slice;
    movie_decoder.slice[0].h = height;
    movie_decoder.slice[1].x = 0;
    movie_decoder.slice[1].y = 0;
    movie_decoder.slice[1].w = slice;
    movie_decoder.slice[1].h = height;
    if (movie_host_stream != 0) {
        movie_ring_buffer = stream_create_ring(sectors, 0);
    } else {
        movie_ring_buffer = heap_alloc(sectors << 11, 0);
        StSetRing(movie_ring_buffer, sectors);
    }
    if (movie_ring_buffer == NULL) {
        return -1;
    }
    movie_player_state = 1;
    return 0;
}

/* Start streaming `file` from `sector`: frames `first_frame` to `last_frame`,
 * CD-XA audio of `channel` when `select` bit 0 is set, `hold` keeps the first
 * frame, `x0, y0, x1, y1` place the two display buffers, `rows` limits the
 * rows a slice loads (none when negative), and `callback` receives each
 * loaded frame. */
void movie_start(s32 file, s32 sector, u16 first_frame, u16 last_frame, u16 channel, s32 select,
                 u16 hold, u16 x0, u16 y0, u16 x1, u16 y1, s16 rows,
                 void (*callback)(u16 frame, u16 x, u16 y)) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    CdlFILTER filter;

    if (movie_player_state == 0) {
        return;
    }
    cd_get_selected_directory(&movie_saved_directory, &movie_saved_index);
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
        stream_reset_ring();
        stream_frame_number = 0;
        cd_movie_request_last_frame = 0;
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
        movie_decoder.display[0].x = x0 * 3 / 2;
        movie_decoder.display[0].y = y0;
        movie_decoder.display[1].x = x1 * 3 / 2;
        movie_decoder.display[1].y = y1;
        movie_slice_width = 24;
    } else {
        movie_decoder.display[0].x = x0;
        movie_decoder.display[0].y = y0;
        movie_decoder.display[1].x = x1;
        movie_decoder.display[1].y = y1;
        movie_slice_width = 16;
    }
    movie_mdec_idle = 1;
    movie_frame_waiting = 1;
    movie_shown_frame = -1;
    movie_load_restart = 1;
    movie_load_enabled = 1;
    movie_first_frame = first_frame;
    movie_decoder.vlc_index = 0;
    movie_decoder.slice_index = 0;
    movie_decoder.decode_display = 0;
    movie_decoder.load_display = 0;
    movie_frame_width = 0;
    movie_frame_height = 0;
    movie_stall_count = 0;
    movie_vlc_pending = 0;
    movie_skipped_frames = 0;
    movie_end_frame = last_frame;
    movie_restart(file, sector, movie_xa_channel, movie_cd_mode, NULL);
}

/* The next frame's bitstream from the ring, or NULL when no frame is complete;
 * its first sector's header goes to `header`. A frame of another size moves
 * the display buffers' far corners and the rows each slice loads. */
u_long *movie_next_bitstream(u32 end_frame, MovieSectorHeader **header) {
    u_long *data;
    MovieSectorHeader *sector;
    u32 columns;

    if (movie_host_stream != 0) {
        if (stream_get_next_movie_frame((u8 **)&data, (StreamFrame **)&sector) != 0) {
            return NULL;
        }
        movie_ring_frame = sector->frame;
        if (movie_ring_frame >= end_frame) {
            cd_stop_read(0);
        }
    } else {
        if (StGetNext(&data, (u_long **)&sector) != 0) {
            movie_stall_count++;
            return NULL;
        }
        movie_stall_count = 0;
        movie_previous_frame = movie_ring_frame;
        movie_ring_frame = sector->frame;
        if (movie_previous_frame + 1 < movie_ring_frame) {
            movie_skipped_frames++;
        }
    }
    if (movie_frame_width != sector->width || movie_frame_height != sector->height) {
        movie_frame_width = sector->width;
        movie_frame_height = sector->height;
        if (movie_color_mode & 1) {
            columns = (movie_frame_width * 3) >> 1;
            movie_decoder.display[0].right = movie_decoder.display[0].x + columns;
            movie_decoder.display[1].right = movie_decoder.display[1].x + columns;
        } else {
            movie_decoder.display[0].right = movie_decoder.display[0].x + movie_frame_width;
            movie_decoder.display[1].right = movie_decoder.display[1].x + movie_frame_width;
        }
        movie_decoder.display[0].bottom = movie_decoder.display[0].y + movie_frame_height;
        movie_decoder.display[1].bottom = movie_decoder.display[1].y + movie_frame_height;
        if (movie_frame_height > movie_image_height) {
            movie_decoder.slice[0].h = movie_image_height;
            movie_decoder.slice[1].h = movie_image_height;
        } else {
            movie_decoder.slice[0].h = movie_frame_height;
            movie_decoder.slice[1].h = movie_frame_height;
        }
    }
    *header = sector;
    return data;
}

/* Start the MDEC on the frame decoded last when it is idle, then decode the
 * next frame's bitstream into a run-level buffer, or continue a partial
 * decode; a finished bitstream's ring sectors are freed. */
void movie_decode(void) {
    u_long *bitstream;
    void *output;

    if (movie_vlc_pending == 0) {
        if (movie_frame_waiting == 0) {
            movie_decoder.slice[movie_decoder.decode_display].x = movie_decoder.display[movie_decoder.decode_display].x;
            movie_decoder.slice[movie_decoder.decode_display].y = movie_decoder.display[movie_decoder.decode_display].y;
            movie_loaded_frame = movie_ring_frame;
            DecDCTin(movie_decoder.vlc_buffers[movie_decoder.vlc_index], movie_color_mode);
            DecDCTout(movie_decoder.slice_buffers[movie_decoder.slice_index],
                      (movie_decoder.slice[movie_decoder.decode_display].w * movie_decoder.slice[movie_decoder.decode_display].h) / 2);
            movie_mdec_idle = 0;
            movie_decoder.decode_display = 1 - movie_decoder.decode_display;
            movie_decoder.vlc_index = 1 - movie_decoder.vlc_index;
        }
        movie_frame_bitstream = movie_next_bitstream(movie_end_frame, &movie_decoded_bitstream);
        if (movie_frame_bitstream == NULL) {
            movie_frame_waiting = 1;
            return;
        }
        movie_frame_waiting = 0;
        DecDCTvlcSize(movie_vlc_limit);
        bitstream = movie_frame_bitstream;
        output = movie_decoder.vlc_buffers[movie_decoder.vlc_index];
    } else {
        bitstream = NULL;
        output = NULL;
    }
    movie_vlc_pending = DecDCTvlc(bitstream, output);
    if (movie_vlc_pending != 0) {
        return;
    }
    if (movie_host_stream != 0) {
        stream_release_movie_frame((u8 *)movie_decoded_bitstream);
    } else {
        StFreeRing(movie_frame_bitstream);
    }
}

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
        sound_set_cd_volume(0x7FFF, 0x28);
    }
    if (movie_shown_frame >= movie_end_frame - 3 && movie_fade_out_pending != 0) {
        movie_fade_out_pending = 0;
        sound_set_cd_volume(0, 0x28);
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
        stream_frame_number = frame;
        cd_error_count++;
        cd_stat_stop_ok_count = CdPosToInt(&location);
        cd_stat_stop_fail_count = movie_start_sector;
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

    sound_set_cd_volume(0, 0);
    cd_stop_read(0);
    cd_sync_reads(0);
    cd_get_selected_directory(&kept_directory, &kept_offset);
    cd_select_directory(movie_saved_directory, movie_saved_index);
    movie_fade_in_pending = 1;
    movie_fade_out_pending = 1;
    if (movie_host_stream != 0) {
        cd_read_file(file, movie_ring_buffer, arg2, mode);
        if (mode & 8) {
            PClseek(cd_pc_file_descriptor, sector * 0x920, 0);
        } else {
            PClseek(cd_pc_file_descriptor, sector << 11, 0);
        }
    } else {
        mode |= 0x80;
        CdIntToPos(cd_get_file_sector(file) + sector, &position);
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
    cd_select_directory(kept_directory, kept_offset);
}

/* Stop the stream: silence the CD input, stop the MDEC, drop the ring's
 * callbacks, pause the drive and restore the resident read mode. */
void movie_stop(void) {
    sound_set_cd_volume(0, 0);
    cd_stop_read(0);
    DecDCToutCallback(NULL);
    DecDCTReset(0);
    movie_player_state = -1;
    if (movie_host_stream != 0) {
        stream_reset_ring();
    } else {
        StUnSetRing();
        while (CdControlB(CdlPause, NULL, NULL) == 0) {
        }
        cd_set_mode(0xA0);
    }
    cd_sync_reads(0);
}

/* Stop, then release the run-level and slice buffers and the ring. */
void movie_close(void) {
    movie_stop();
    heap_free(movie_decoder.vlc_buffers[0]);
    heap_free(movie_decoder.vlc_buffers[1]);
    heap_free(movie_decoder.slice_buffers[0]);
    heap_free(movie_decoder.slice_buffers[1]);
    heap_free(movie_ring_buffer);
    movie_decoder.vlc_buffers[0] = NULL;
    movie_decoder.vlc_buffers[1] = NULL;
    movie_decoder.slice_buffers[0] = NULL;
    movie_decoder.slice_buffers[1] = NULL;
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

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", D_801D4C94);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlcSize);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlc);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", D_801D502C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTvlcSize2);

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
