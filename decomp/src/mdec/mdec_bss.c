/* The movie player's uninitialized statics (801e8910..801e89ac). The original
 * linked them as the start of the image's .bss, after the libraries' .data
 * and ahead of the libcd modules' .bss; the image includes the .bss as zeros,
 * so they are defined here with zero initializers. The original linker gave
 * each .bss object a 4-byte slot, so the byte and halfword variables are
 * word-aligned (SLOT). */
#include "common.h"
#include "movie.h"

#define SLOT __attribute__((aligned(4)))

MovieSectorHeader *movie_decoded_bitstream = NULL;
u32 *movie_frame_bitstream = NULL;
MovieDecoder movie_decoder = {0};
u8 movie_mdec_idle SLOT = 0;
u8 movie_frame_waiting SLOT = 0;
u8 movie_restarted SLOT = 0;
s8 movie_player_state SLOT = 0;
u8 movie_host_stream SLOT = 0;
s32 movie_start_sector = 0;
s32 movie_cd_mode = 0;
u16 movie_file SLOT = 0;
u16 movie_xa_channel SLOT = 0;
s16 movie_row_limit SLOT = 0;
s32 movie_loaded_frame = 0;
s32 movie_first_frame = 0;
s32 movie_shown_frame = 0;
void *movie_ring_buffer = NULL;
void (*movie_frame_callback)(u16 frame, u16 x, u16 y) = NULL;
s32 movie_ring_frame = 0;
s32 movie_load_enabled = 0;
s32 movie_saved_directory = 0;
s32 movie_saved_index = 0;
s32 movie_fade_in_pending = 0;
s32 movie_fade_out_pending = 0;
