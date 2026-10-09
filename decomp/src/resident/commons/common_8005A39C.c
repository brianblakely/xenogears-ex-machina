/* The resident's commons that the original linker allocated after every
 * unit's own larger variables, from 8005a39c, in an order of its own: the
 * first run, up to the PsyQ sound library's commons at 8005a4c8. This unit
 * and the three after it (common_8005A4DC.c, common_80062648.c,
 * common_8006D634.c), linked between the libraries' generated commons,
 * define them in that order, each in a slot of whole words
 * (decomp/Makefile). GCC emits tentative definitions in the order of their
 * first declaration, so they are defined ahead of the headers that declare
 * them, structures by their tags. Names the code uses for parts of these
 * objects are in link.ld. Variables only overlays address are marked with
 * them; those nothing addresses are marked unreferenced. */
#include "common.h"
#include "psyq/libspu.h"

struct GameData *D_8005A39C; /* the game data in use */
u16 D_8005A3A0[16]; /* battle script variables, also read by the menu */
struct SoundVolumes D_8005A3C0; /* the sound driver's SPU common attributes */
s32 D_8005A408[3]; /* field */
void *D_8005A414[3]; /* party field sprite blocks */
void *D_8005A420[4]; /* field */
s32 D_8005A430[5]; /* unreferenced */
s32 D_8005A444[3];
void *D_8005A450[4]; /* field */
s32 D_8005A460[4]; /* unreferenced */
s32 D_8005A470; /* the movie player */
u8 D_8005A474[0x14]; /* sprites, battle effects */
s32 D_8005A488;
s32 D_8005A48C;
s32 D_8005A490;
s32 D_8005A494;
s32 D_8005A498;
s32 D_8005A49C;
void *D_8005A4A0; /* file 0xa7 block */
s32 D_8005A4A4;
s32 D_8005A4A8;
u32 *D_8005A4AC[2]; /* the menu's large ordering tables, one per draw buffer */
s32 D_8005A4B4;
u16 D_8005A4B8;
void *D_8005A4BC; /* file 0xa8 block */
s32 D_8005A4C0; /* map read-ahead size */
s32 D_8005A4C4; /* unreferenced */

#include "../cd.h"
#include "../menu.h"
#include "../mode.h"
#include "../sound.h"
#include "../sprite.h"
#include "../stream.h"
