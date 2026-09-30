#ifndef FIELD_FIELD_SCRIPT_H
#define FIELD_FIELD_SCRIPT_H

/* Event-script state addressed as scalars by the instruction handlers from
 * 80087af0 on (their scheduling needs them apart from the 800b2078 block). */

#include "field.h"

extern u16 D_800B236C; /* script flag set by instruction FE 99 (read by 80094xxx) */
extern u32 *D_800B1F74; /* TIM image held by instruction 0x77 */

#endif
