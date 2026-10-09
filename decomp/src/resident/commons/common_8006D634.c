/* The resident's commons from 8006d634, after the PsyQ graphics library's,
 * up to the end of the BSS, the word below the overlay area at 8006faf0
 * that the entry point clears last (common_8005A39C.c). */
#include "common.h"
#include "psyq/libgte.h"

struct GameData D_8006D634; /* the saved game */
s32 D_8006F98C; /* unreferenced */
s32 D_8006F990[3];
VECTOR D_8006F99C; /* positions (16.16) of two field points */
VECTOR D_8006F9AC;
struct FileRequest D_8006F9BC[4]; /* the mode's sound files */
u8 D_8006F9DC[0x20]; /* scene state: [2] the scene selector */
struct SpuMemBlock D_8006F9FC[12]; /* the SPU memory map */
s32 D_8006FABC[3]; /* party members of the loaded field files */
u8 D_8006FAC8[0x28]; /* the SPU memory management table (SpuInitMalloc, 4 blocks) */

#include "../cd.h"
#include "../mode.h"
#include "../sound.h"
#include "../sprite.h"
