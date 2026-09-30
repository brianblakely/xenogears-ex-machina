#ifndef BATTLE_PSYQ_H
#define BATTLE_PSYQ_H

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The PsyQ libgte entry points of the resident executable that its symbol
 * map does not name. */
void func_8003F738(SVECTOR *angles, MATRIX *m);        /* RotMatrix */
void func_8004A92C(SVECTOR *angles, MATRIX *m);        /* RotMatrixYXZ */
void func_8004A480(VECTOR *a, VECTOR *b, VECTOR *out); /* OuterProduct12 */
void func_8004A414(VECTOR *in, VECTOR *out);          /* Square0 */
MATRIX *func_8004ABBC(SVECTOR *angles, MATRIX *m);     /* RotMatrix */

#endif
