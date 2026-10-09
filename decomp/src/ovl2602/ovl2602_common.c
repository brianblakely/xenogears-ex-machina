/* The overlay's common (uninitialized global) variables. The original linker
 * allocated them after both units' own variables, at the end of the file
 * (zero there), each in a slot of whole words (decomp/Makefile), so this
 * unit, linked last, defines them: 801D9050-801D90A0, the end of the file.
 * Both units read all but D_801D9088 (tools/data_users.py). GCC emits
 * tentative definitions in the order of their first declaration, so they
 * are defined ahead of the header that declares them, the structure by its
 * tag; the header then completes it. */
#include "common.h"

struct CameraMove D_801D9050;
u8 D_801D9084;     /* gear being edited */
u8 *D_801D9088;    /* name pixel buffer */
s32 D_801D908C[5]; /* entries in each of the five gear part lists */

#include "gear_shop.h"
