/* The resident's commons from 80062648, after the PsyQ data-stream
 * library's, up to the PsyQ graphics library's at 8006be34
 * (common_8005A39C.c). */
#include "common.h"

u8 D_80062648[0x3200]; /* battle, field, world map, ovl2606, ovl3087 */
s32 D_80065848[5]; /* field; debug595 reads [2]-[4] */
s32 D_8006585C[27]; /* unreferenced */
u8 *D_800658C8; /* battle scene data */
void *D_800658CC;
s32 D_800658D0[3]; /* unreferenced */
struct EncounterSet D_800658DC; /* battle, field, world map, ovl2606 */
u8 D_80065ADC[16]; /* field, debug595 */
s32 D_80065AEC[4]; /* unreferenced */
void *D_80065AFC[3]; /* party character file blocks */
s32 D_80065B08; /* field */
u8 D_80065B0C[0x6300]; /* the sound driver's memory pool */
s32 D_8006BE0C; /* unreferenced */
u8 D_8006BE10[0x14]; /* the shared sprite source (a SpriteSource record): sprites, battle */
void *D_8006BE24;
s32 D_8006BE28; /* unreferenced */
s16 D_8006BE2C[3]; /* field */

#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "../sound_driver.h"
