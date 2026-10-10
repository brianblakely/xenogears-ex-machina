#ifndef OVL2143_SURFACE_H
#define OVL2143_SURFACE_H

/* An actor's surfaces (battle/effect.h's Surface): rings of points around a
 * strand, lit and drawn as gouraud textured triangles. */

#include "common.h"
#include "psyq/libgte.h"
#include "battle/effect.h"

void gear_model_build_surface(Surface *record, u16 *table, s32 angle_base, s32 scale, s16 ox, s16 oy, s16 oz,
                   s32 count, s16 tx, s16 ty, s16 u_span, s16 v_span, s16 clut_x, s16 clut_y, u8 b0,
                   u8 b1, u8 b2, u8 b3, u8 b4, u8 b5);
void gear_model_simulate_and_draw_surface(Surface *record, SVECTOR *wind, MATRIX *m, u32 *ot, s32 buffer, s32 scale,
                   s16 floor);
void gear_model_free_surface(Surface *record);

#endif
