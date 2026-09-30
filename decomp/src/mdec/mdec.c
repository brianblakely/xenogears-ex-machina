#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libsn.h"
#include "movie.h"

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_slice_decoded);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_open);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_start);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_next_bitstream);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_decode);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_poll);

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
