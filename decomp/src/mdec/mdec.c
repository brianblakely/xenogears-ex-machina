#include "common.h"
#include "psyq/libcd.h"
#include "movie.h"

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_slice_decoded);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_open);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_start);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_next_bitstream);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_decode);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_poll);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", movie_restart);

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

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D444C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCTReset);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D456C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D45F8);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4694);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D46A0);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D471C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D473C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4778);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D47B4);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", DecDCToutCallback);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D47FC);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D48F8);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D498C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4A1C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4AB4);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4B4C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4B64);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4C94);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4C98);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D4CC8);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D502C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5060);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D53C0);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5544);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D57DC);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D583C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D586C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5900);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5920);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", StUnSetRing);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5A04);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5A94);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5AF4);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5B7C);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5C34);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5C70);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5D34);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D5D54);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D66C4);

INCLUDE_ASM(".local/decomp/mdec/asm/nonmatchings/mdec", func_801D66F8);
