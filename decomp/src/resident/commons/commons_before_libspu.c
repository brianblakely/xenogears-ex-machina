/* The resident's commons that the original linker allocated after every
 * unit's own larger variables, from 8005a39c, in an order of its own: the
 * first run, up to the PsyQ sound library's commons at 8005a4c8. This unit
 * and the three after it (commons_before_libds.c, commons_before_libgpu.c,
 * commons_after_libgpu.c), linked between the libraries' generated commons,
 * define them in that order, each in a slot of whole words
 * (decomp/Makefile). GCC emits tentative definitions in the order of their
 * first declaration, so they are defined ahead of the headers that declare
 * them, structures by their tags. Names the code uses for parts of these
 * objects are in link.ld. Variables only overlays address are marked with
 * them; those nothing addresses are marked unreferenced. */
#include "common.h"

struct GameData *game_current_data; /* 8005A39C: the game data in use */
u16 mode_battle_ai_variables[16]; /* 8005A3A0: battle script variables, also read by the menu */
struct SoundVolumes sound_volumes; /* 8005A3C0: the sound driver's SPU common attributes */
s32 mode_snapshot_party_in_gear[3]; /* 8005A408: field */
void *mode_party_sprite_blocks[3]; /* 8005A414: party field sprite blocks */
void *mode_field_layer_script_files[4]; /* 8005A420: field */
s32 commons_unused_5_words[5]; /* 8005A430: unreferenced */
s32 mode_party_actors[3]; /* 8005A444 */
void *mode_field_layer_model_files[4]; /* 8005A450: field */
s32 commons_unused_4_words_a[4]; /* 8005A460: unreferenced */
s32 commons_libcd_stream_headerless; /* 8005A470: the movie player */
u8 sprite_effect_source[0x14]; /* 8005A474: sprites, battle effects */
s32 cd_stat_setloc_count; /* 8005A488 */
s32 cd_stat_command_ok_count; /* 8005A48C */
s32 cd_stat_command_fail_count; /* 8005A490 */
s32 cd_stat_retry_setloc_count; /* 8005A494 */
s32 cd_stat_retry_fail_count; /* 8005A498 */
s32 cd_stat_lesmem_count; /* 8005A49C */
void *mode_preloaded_text_images; /* 8005A4A0: file 0xa7 block */
s32 cd_stat_error_limit_count; /* 8005A4A4 */
s32 cd_stat_stop_ok_count; /* 8005A4A8 */
u32 *menu_state_big_ots[2]; /* 8005A4AC: the menu's large ordering tables, one per draw buffer */
s32 cd_stat_stop_fail_count; /* 8005A4B4 */
u16 stream_frame_number; /* 8005A4B8 */
void *mode_preloaded_effect_bank; /* 8005A4BC: file 0xa8 block */
s32 mode_read_ahead_size; /* 8005A4C0: map read-ahead size */
s32 commons_unused_word_d; /* 8005A4C4: unreferenced */

#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/stream.h"
