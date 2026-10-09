/* The resident's commons from 8005a4dc, after the PsyQ sound library's,
 * up to the PsyQ data-stream library's at 80062640 (common_8005A39C.c). */
#include "common.h"

s32 D_8005A4DC;
void *D_8005A4E0; /* map read-ahead block */
/* The field's and the world map's state, saved here across a scene change;
 * the world map's is 0x22fc bytes. */
u8 D_8005A4E4[0x22FC];
u8 D_8005C7E0[0x5D34]; /* unreferenced */
s16 D_80062514; /* movie: the requested movie's last frame */
s32 D_80062518[4]; /* loaded wave bank per slot */
s32 D_80062528; /* the active sequence */
struct SoundChannel *D_8006252C[24]; /* channel of each voice */
struct SoundSequence *D_8006258C; /* the transferred wave bank */
s32 D_80062590[3]; /* party members */
struct SoundBank *D_8006259C; /* the effect sound bank: field, world map and the menus */
struct MenuState *D_800625A0;
struct FileRequest D_800625A4[4]; /* party file list, zero-terminated */
s32 D_800625C4[14]; /* unreferenced */
struct PadBuffer D_800625FC[2]; /* controller receive buffers */

#include "resident/cd.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/stream.h"
