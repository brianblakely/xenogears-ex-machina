#ifndef BATTLE_MESH_H
#define BATTLE_MESH_H

#include "common.h"

/* A stage object's animated polygon mesh (0x24 bytes, built by 800A7064):
 * a grid of points drawn as Gouraud-textured triangles. */
typedef struct {
    u16 field0;
    u8 pad2[2];
    s16 pointCount; /* 0x04 */
    s16 polyCount;  /* 0x06: triangles, two per quad */
    s16 field8;     /* 0x08 */
    s16 keyCount;   /* 0x0A */
    u8 fieldC[6];   /* 0x0C */
    u8 pad12[2];
    void *points;   /* 0x14: 8 bytes per point; NULL when unallocated */
    struct MeshKey *keys; /* 0x18: NULL without */
    void **rows;    /* 0x1C: row pointers into one block of 0x18-byte vertices */
    void *polys;    /* 0x20: 0x58 bytes per triangle */
} StageMesh;

/* A key of a stage mesh (0x10 bytes). */
typedef struct MeshKey {
    s16 field0;
    s16 field2;
    s16 field4;
    s16 field6;
    u8 pad8[6];
    s16 fieldE;
} MeshKey;

void func_800A8A88(StageMesh *mesh);
void func_800A7064(StageMesh *mesh, void *data, s32 a, s32 b, s32 c, s32 d, s32 e, s32 keyCount, u16 x,
                   u16 y, u16 f, u16 g, u16 z, u16 w, u8 b0, u8 b1, u8 b2, u8 b3, u8 b4, u8 b5);

#endif
