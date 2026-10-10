/* The resident's commons from 8005a4dc, after the PsyQ sound library's,
 * up to the PsyQ data-stream library's at 80062640 (commons_before_libspu.c). */
#include "common.h"

s32 cd_error_count; /* 8005A4DC */
void *mode_read_ahead_block; /* 8005A4E0: map read-ahead block */
/* The field's and the world map's state, saved here across a scene change;
 * the world map's is 0x22fc bytes. */
u8 mode_snapshot_block[0x22FC]; /* 8005A4E4 */
u8 commons_unused_block[0x5D34]; /* 8005C7E0: unreferenced */
u16 cd_movie_request_last_frame; /* 80062514: the requested movie's last frame (resident/cd.h) */
s32 mode_wave_bank_slots[4]; /* 80062518: loaded wave bank per slot */
s32 mode_music_seq; /* 80062528: the active sequence */
struct SoundChannel *sound_voice_owners[24]; /* 8006252C: channel of each voice */
struct SoundSequence *mode_music_wave_bank; /* 8006258C: the transferred wave bank */
s32 mode_party_members[3]; /* 80062590: party members */
struct SoundBank *sound_effect_bank; /* 8006259C: the effect sound bank: field, world map and the menus */
struct MenuState *menu_state_current; /* 800625A0 */
struct FileRequest mode_party_file_list[4]; /* 800625A4: party file list, zero-terminated */
s32 commons_unused_14_words[14]; /* 800625C4: unreferenced */
struct PadBuffer pad_receive_buffers[2]; /* 800625FC: controller receive buffers */

#include "resident/cd.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
