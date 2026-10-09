#ifndef MENU_HELPERS_H
#define MENU_HELPERS_H

#include "common.h"
#include "psyq/libgte.h"

/* Helpers: menu2's handwritten GTE vector and matrix scaling, colour filter
 * and word copy (80073064-800732CC), menu5's vector lengths and scaling
 * (800884E0-800888B0), menu7's whole-file load (800891C0) and angle steps
 * (8008B5FC-8008B730). */

/* GTE scaling; all three write x/y/z and preserve out->pad. */
void func_80073064(SVECTOR *dir, SVECTOR *out, s32 scale); /* GPF, sf=1 */
void func_800730AC(SVECTOR *dir, SVECTOR *out, s32 scale); /* GPF, sf=0 */
void func_800730F4(VECTOR *dir, SVECTOR *out, s32 scale);  /* low signed halfwords, sf=1 */
/* In-place RGB555 sliding box filter; second row is 0x280 bytes ahead.
 * end is the terminating read cursor, and a pair there is prefetched. */
void func_8007313C(void *pixels, void *end);
/* Scale each rotation column through the GTE, preserving m's translation
 * and leaving the original rotation loaded in the GTE. */
void func_800731F8(MATRIX *m, s16 *scale);
/* Forward word copy, with a positive count divisible by four. */
void func_800732AC(void *dst, void *src, s32 size);
void func_8008859C(VECTOR *vector, void *out);
s32 func_800886FC(VECTOR *vector);
s32 func_80088754(VECTOR *vector);
s32 func_800887A4(VECTOR *from, VECTOR *to);
s32 func_80088838(VECTOR *from, VECTOR *to);
void *func_800891C0(s32 id);
s32 func_8008B650(s32 from, s32 to, s32 step); /* turn an angle toward a target */

#endif
