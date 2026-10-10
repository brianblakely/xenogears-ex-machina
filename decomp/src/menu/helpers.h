#ifndef MENU_HELPERS_H
#define MENU_HELPERS_H

#include "common.h"
#include "psyq/libgte.h"

/* Helpers: menu2's handwritten GTE vector and matrix scaling, colour filter
 * and word copy (80073064-800732CC), menu5's vector lengths and scaling
 * (800884E0-800888B0), menu7's whole-file load (800891C0) and angle steps
 * (8008B5FC-8008B730). */

/* GTE scaling; all three write x/y/z and preserve out->pad. */
void arena_gte_scale_svector(SVECTOR *dir, SVECTOR *out, s32 scale); /* GPF, sf=1 */
void arena_gte_multiply_svector(SVECTOR *dir, SVECTOR *out, s32 scale); /* GPF, sf=0 */
void arena_gte_scale_vector_low_halves(VECTOR *dir, SVECTOR *out, s32 scale);  /* low signed halfwords, sf=1 */
/* In-place RGB555 sliding box filter; second row is 0x280 bytes ahead.
 * end is the terminating read cursor, and a pair there is prefetched. */
void arena_box_filter_rgb555(void *pixels, void *end);
/* Scale each rotation column through the GTE, preserving m's translation
 * and leaving the original rotation loaded in the GTE. */
void arena_gte_scale_matrix_columns(MATRIX *m, s16 *scale);
/* Forward word copy, with a positive count divisible by four. */
void arena_copy_words(void *dst, void *src, s32 size);
void arena_vector_normalize_to_svector(VECTOR *vector, void *out);
s32 arena_vector_get_length(VECTOR *vector);
s32 arena_vector_get_flat_length(VECTOR *vector);
s32 arena_vector_get_distance(VECTOR *from, VECTOR *to);
s32 arena_vector_get_flat_distance(VECTOR *from, VECTOR *to);
void *arena_load_whole_file(s32 id);
s32 arena_angle_turn_toward(s32 from, s32 to, s32 step); /* turn an angle toward a target */

#endif
