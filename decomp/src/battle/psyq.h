#ifndef BATTLE_PSYQ_H
#define BATTLE_PSYQ_H

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The PsyQ libgte entry points of the resident executable that its symbol
 * map does not name. */
void func_8003F738(SVECTOR *angles, MATRIX *m);        /* RotMatrix */

#endif
