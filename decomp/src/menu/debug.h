#ifndef MENU_DEBUG_H
#define MENU_DEBUG_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* Debug drawing (menu5 80087E38-800884E0): 3D lines that stay for some
 * frames, and the recorded path with an axis cross at each point. */

/* A 3D debug line with its packets (one per buffer). */
typedef struct {
    LINE_F2 packets[2]; /* 0x00 */
    SVECTOR from;      /* 0x20 */
    SVECTOR to;        /* 0x28 */
    s16 timer;         /* 0x30: frames left, 0 = free */
    s16 pad;
} Line3D;

/* A recorded path position and its debug marker: three axis lines (red
 * x, green y, blue z) per buffer. */
typedef struct PathMarker {
    LINE_F2 axes[2][3]; /* 0x00 */
    s16 x, y, z;          /* 0x60 */
    u8 pad[2];
} PathMarker;

extern s32 D_800928F8; /* recorded path points */
/* Thirty records end the BSS the resident's mode table clears (8009b558);
 * func_80087E38 records up to 31, the last over the heap that follows. */
extern PathMarker D_8009A928[30];

#endif
