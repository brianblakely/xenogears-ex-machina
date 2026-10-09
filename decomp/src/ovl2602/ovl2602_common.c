/* The overlay's common (uninitialized global) variables. The original linker
 * allocated them after both units' own variables, at the end of the file
 * (zero there), each in a slot of whole words (decomp/Makefile), so this
 * unit, linked last, defines them. Both units read all but D_801D9088
 * (tools/data_users.py). */
#include "menu_card.h"

CameraMove D_801D9050;
u8 D_801D9084;     /* gear being edited */
u8 *D_801D9088;    /* name pixel buffer */
s32 D_801D908C[5]; /* entries in each of the five gear part lists */
