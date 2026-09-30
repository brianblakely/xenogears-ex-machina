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
void func_80049BDC(MATRIX *m0, MATRIX *m1);            /* multiply m1 by m0 */
s32 func_80048D7C(VECTOR *v, VECTOR *out);             /* VectorNormal */
void func_8004A480(VECTOR *a, VECTOR *b, VECTOR *out); /* OuterProduct12 */

#endif
