#ifndef BATTLE_MESH_H
#define BATTLE_MESH_H

#include "common.h"

/* A stage object's animated polygon mesh (0x24 bytes, built by 800A7064):
 * a grid of points drawn as Gouraud-textured triangles. */
typedef struct {
    u8 pad0[4];
    s16 pointCount; /* 0x04 */
    s16 polyCount;  /* 0x06: triangles, two per quad */
    s16 field8;     /* 0x08 */
    s16 keyCount;   /* 0x0A */
    u8 fieldC[6];   /* 0x0C */
    u8 pad12[2];
    void *points;   /* 0x14: 8 bytes per point; NULL when unallocated */
    void *keys;     /* 0x18: 0x10 bytes per key, NULL without */
    void **rows;    /* 0x1C: row pointers into one block of 0x18-byte vertices */
    void *polys;    /* 0x20: 0x58 bytes per triangle */
} StageMesh;

void func_800A8A88(StageMesh *mesh);

#endif
