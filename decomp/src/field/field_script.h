#ifndef FIELD_FIELD_SCRIPT_H
#define FIELD_FIELD_SCRIPT_H

/* Event-script state addressed as scalars by the instruction handlers from
 * 80087af0 on (their scheduling needs them apart from the 800b2078 block). */

#include "field.h"

extern u16 D_800B236C; /* script flag set by instruction FE 99 (read by 80094xxx) */
extern u32 *D_800B1F74; /* TIM image held by instruction 0x77 */

/* Sound-effect bank instruction 0xb0 (resident sound state). */
extern s32 D_80062518[4];  /* loaded wave bank per slot */
extern s32 D_80062524;     /* slot 3 of D_80062518, read on its own */
extern s32 D_800595AC;     /* active slot-3 bank */
extern s32 D_8004F370;     /* 1 selects the alternate bank file */
extern void *D_800AFD08;   /* bank file being loaded */
extern s32 D_800AFD0C;     /* bank file number */
extern s32 D_800AFD18;     /* bank slot being loaded */

#endif
