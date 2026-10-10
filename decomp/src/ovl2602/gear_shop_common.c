/* The overlay's common (uninitialized global) variables. The original linker
 * allocated them after both units' own variables, at the end of the file
 * (zero there), each in a slot of whole words (decomp/Makefile), so this
 * unit, linked last, defines them: 801D9050-801D90A0, the end of the file.
 * Both units read all but gear_shop_name_pixels (tools/data_users.py). GCC emits
 * tentative definitions in the order of their first declaration, so they
 * are defined ahead of the header that declares them, the structure by its
 * tag; the header then completes it. */
#include "common.h"

struct CameraMove gear_shop_camera_move; /* 801D9050 */
u8 gear_shop_edited_gear;                /* 801D9084: gear being edited */
u8 *gear_shop_name_pixels;               /* 801D9088: name pixel buffer */
s32 gear_shop_stock_list_counts[5];      /* 801D908C: entries in each of the five gear part lists */

#include "gear_shop.h"
