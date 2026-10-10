#ifndef XEM_PROTOTYPES_H
#define XEM_PROTOTYPES_H

#include "common.h"

/*
 * Canonical prototypes the port build gives every unit, for functions a unit
 * calls before it declares them (C89 implicit declarations, which clang
 * rejects when a conflicting prototype follows). Unprototyped calls elsewhere
 * go through the adapters game_module.py generates (build/game/adapters.txt).
 */

/* field.c; field_effect.c calls it before its prototype. */
void field_load_tim_at(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

#endif
