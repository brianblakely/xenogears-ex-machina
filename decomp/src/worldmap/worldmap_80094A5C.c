#include "worldmap.h"
#include "psyq/libsn.h"

/* Declared here only: other units call it without a prototype. */
void func_80093354(VECTOR *position);
/* Defined returning s16 (worldmap_80083A00); this unit uses the value as an int. */
s32 func_80084D00(s32 probe, s16 *hit);

void func_80099708(u32 *heights, u32 *ot, s32 packets, SVECTOR *origin);

/* World tables of the whole overlay (this unit's .data): area selection,
 * per area scene objects, path regions, the map dots, terrain visibility
 * and the horizon. */

/* The open map's areas by position (thresholds; area i-1 below entry i),
 * and the encounter level brackets. */
u16 D_8009B564[10] = {0, 24, 54, 135, 149, 186, 198, 204, 237, 0xFFFF};
u16 D_8009B578[5] = {0, 54, 201, 340, 0xFFFF};

/* Area file sets: the nine open map areas, then one per scene mode (8-18). */
WorldmapArea D_8009B584[20] = {
    {43, 16, 16, 2}, {54, 16, 16, 2}, {65, 16, 16, 2}, {76, 16, 16, 2}, {87, 16, 16, 2},
    {98, 16, 16, 2}, {109, 16, 16, 2}, {120, 16, 16, 2}, {131, 16, 16, 2}, {43, 16, 16, 2},
    {142, 16, 16, 2}, {142, 16, 16, 2}, {43, 16, 16, 2}, {164, 16, 16, 2}, {186, 16, 16, 2},
    {153, 16, 16, 2}, {175, 16, 16, 2}, {197, 16, 16, 2}, {208, 16, 16, 2}, {219, 16, 16, 2},
};

/* Per area: the scene objects of the area's actors (a spinning pair, a
 * further pair and single objects). */
u16 D_8009B624[10][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {39, 42}, {39, 42}, {39, 42}, {39, 42}, {39, 42}, {0, 0},
};
u16 D_8009B64C[10][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {44, 45}, {44, 45}, {44, 45}, {44, 45}, {44, 45}, {0, 0},
};
u16 D_8009B674[10] = {0, 0, 0, 46, 51, 51, 51, 51, 0, 0};
u16 D_8009B688[10] = {0, 0, 0, 47, 52, 52, 52, 52, 0, 0};
u16 D_8009B69C[10] = {0, 0, 0, 0, 0, 0, 67, 79, 64, 0};
u16 D_8009B6B0[10] = {0, 0, 0, 0, 43, 43, 43, 43, 43, 0};

/* Fixed path regions (scene, entry, path link): func_8008E078 selects the
 * first two for scenes 15 and 16, func_800712D0 the third. */
PathRegion D_8009B6C4[3] = {
    {0, 0, 0, 0, 0x138, 1, 14, 0},
    {0, 0, 0, 0, 0x1B8, 1, 29, 0},
    {0, 0, 0, 0, 0x122, 3, -1, 0},
};

/* The map screen's 32 dots: x and z, interleaved (24-26 are placed from the
 * saved state, 27-31 unused). */
u16 D_8009B6F4[64] = {
    84, 36,  68, 41,  55, 34,  58, 24,  62, 20,  54, 20,  78, 20,  77, 9,
    57, 17,  29, 81,  24, 68,  10, 71,  31, 57,  15, 58,  51, 48,  59, 89,
    92, 82,  17, 6,  29, 35,  8, 9,  88, 69,  90, 49,  94, 31,  95, 29,
    0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,
};

/* Unreferenced: the border of the 5x5 terrain block grid. */
s16 D_8009B774[5 * 5] = {
    1, 1, 1, 1, 1,
    1, 0, 0, 0, 1,
    1, 0, 0, 0, 1,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 1,
};

/* Per quadrant of the camera's block: the quarters of the 5x5 blocks that
 * are always visible (-1), four per block (func_800983A0). */
s16 D_8009B7A8[4][25][4] = {
    {
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, -1, -1}, {0, 0, -1, -1}, {0, 0, -1, -1}, {0, 0, -1, -1}, {0, -1, -1, -1},
    },
    {
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, -1}, {0, 0, -1, -1}, {0, 0, -1, -1}, {0, 0, -1, -1}, {0, 0, -1, -1},
    },
    {
        {-1, -1, 0, 0}, {-1, -1, 0, 0}, {-1, -1, 0, 0}, {-1, -1, 0, 0}, {-1, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, -1, 0, -1},
    },
    {
        {-1, -1, -1, 0}, {-1, -1, 0, 0}, {-1, -1, 0, 0}, {-1, -1, 0, 0}, {-1, -1, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {-1, 0, -1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    },
};

/* The table of func_80094060: per movement mode (row) and terrain class at
 * a position (column, func_80093F18). */
s16 D_8009BAC8[8 * 8] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 1, 1, 0, 0, 0,
    1, 0, 0, 1, 0, 0, 0, 0,
    1, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 0, 0, 0,
    1, 1, 1, 1, 1, 0, 0, 0,
};

/* Background colour. */
u8 D_8009BB48[3] = {0xC0, 0xD0, 0xF0};

/* View edge vectors whose outer products give the four horizon plane
 * normals (func_80098044). */
VECTOR D_8009BB4C = {0, -1, 0};
VECTOR D_8009BB5C = {1, 0, 0};
VECTOR D_8009BB6C = {-2170, 0, 3474};
VECTOR D_8009BB7C = {2170, 0, 3474};
VECTOR D_8009BB8C = {0, -2170, 3474};
VECTOR D_8009BB9C = {0, 2170, 0};

/* Grid corner cells. */
s16 D_8009BBAC[4] = {0, 8, 72, 80};

/* Move a position along a direction across the terrain cells: probe the
 * cell boundaries crossed (by the corner's side for diagonal moves); 1 when
 * the target cell is walkable (step[0] = target), else the boundary result.
 * The cell probes take the mode as an s16 row. */
s32 func_80094A5C(VECTOR *position, VECTOR *direction, s32 scale, s32 mode) {
    CellProbe *probe;
    s32 from;
    s32 to;
    u32 crossing;
    s32 side;
    s16 row = mode;

    SCRATCH_VECTOR[1].vx = position->vx + ((direction->vx * scale) >> 12);
    SCRATCH_VECTOR[1].vz = position->vz + ((direction->vz * scale) >> 12);
    crossing = 0;
    from = position->vx;
    from >>= 19;
    to = SCRATCH_VECTOR[1].vx >> 19;
    ((SVECTOR *)0x1F8000A0)->vx = from;
    ((SVECTOR *)0x1F8000A0)->vz = position->vz >> 19;
    ((SVECTOR *)0x1F8000A8)->vx = to;
    ((SVECTOR *)0x1F8000A8)->vz = SCRATCH_VECTOR[1].vz >> 19;
    probe = CELL_PROBE;
    scale = 0;
    if (to < from) {
        crossing = 2;
    } else if (from < to) {
        crossing = 1;
    }
    if (probe->cell[0].vz > probe->cell[1].vz) {
        crossing |= 8;
    } else if (probe->cell[0].vz < probe->cell[1].vz) {
        crossing |= 4;
    }
    switch (crossing) {
    case 0:
    case 3:
    case 7:
        break;
    case 1:
        scale = func_8009443C(position, direction, probe->step, row);
        break;
    case 2:
        scale = func_800945C8(position, direction, probe->step, row);
        break;
    case 4:
        scale = func_80094750(position, direction, probe->step, row);
        break;
    case 8:
        scale = func_800948D8(position, direction, probe->step, row);
        break;
    case 5:
        probe->step[0].vx = (position->vx & 0xFFF80000) + 0x80000;
        probe->step[0].vz = (position->vz & 0xFFF80000) + 0x80000;
        probe->step[2].vx = (position->vx - probe->step[0].vx) >> 12;
        probe->step[2].vz = (position->vz - probe->step[0].vz) >> 12;
        probe->step[3].vx = (probe->step[1].vx - probe->step[0].vx) >> 12;
        probe->step[3].vz = (probe->step[1].vz - probe->step[0].vz) >> 12;
        probe->step[4].vy = 0;
        probe->step[4].vx = (probe->step[2].vz << 16) | (probe->step[2].vx & 0xFFFF);
        probe->step[4].vz = (probe->step[3].vz << 16) | (probe->step[3].vx & 0xFFFF);
        side = func_8004A70C(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side < 0) {
            scale = func_8009443C(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_80094750(position, direction, probe->step, row);
        } else if (side > 0) {
            scale = func_80094750(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_8009443C(position, direction, probe->step, row);
        } else {
            scale = func_8009443C(position, direction, probe->step, row);
        }
        break;
    case 6:
        probe->step[0].vx = (position->vx & 0xFFF80000);
        probe->step[0].vz = (position->vz & 0xFFF80000) + 0x80000;
        probe->step[2].vx = (position->vx - probe->step[0].vx) >> 12;
        probe->step[2].vz = (position->vz - probe->step[0].vz) >> 12;
        probe->step[3].vx = (probe->step[1].vx - probe->step[0].vx) >> 12;
        probe->step[3].vz = (probe->step[1].vz - probe->step[0].vz) >> 12;
        probe->step[4].vy = 0;
        probe->step[4].vx = (probe->step[2].vz << 16) | (probe->step[2].vx & 0xFFFF);
        probe->step[4].vz = (probe->step[3].vz << 16) | (probe->step[3].vx & 0xFFFF);
        side = func_8004A70C(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side > 0) {
            scale = func_800945C8(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_80094750(position, direction, probe->step, row);
        } else if (side < 0) {
            scale = func_80094750(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_800945C8(position, direction, probe->step, row);
        } else {
            scale = func_800945C8(position, direction, probe->step, row);
        }
        break;
    case 9:
        probe->step[0].vx = (position->vx & 0xFFF80000) + 0x80000;
        probe->step[0].vz = (position->vz & 0xFFF80000);
        probe->step[2].vx = (position->vx - probe->step[0].vx) >> 12;
        probe->step[2].vz = (position->vz - probe->step[0].vz) >> 12;
        probe->step[3].vx = (probe->step[1].vx - probe->step[0].vx) >> 12;
        probe->step[3].vz = (probe->step[1].vz - probe->step[0].vz) >> 12;
        probe->step[4].vy = 0;
        probe->step[4].vx = (probe->step[2].vz << 16) | (probe->step[2].vx & 0xFFFF);
        probe->step[4].vz = (probe->step[3].vz << 16) | (probe->step[3].vx & 0xFFFF);
        side = func_8004A70C(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side > 0) {
            scale = func_8009443C(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_800948D8(position, direction, probe->step, row);
        } else if (side < 0) {
            scale = func_800948D8(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_8009443C(position, direction, probe->step, row);
        } else {
            scale = func_8009443C(position, direction, probe->step, row);
        }
        break;
    case 10:
        probe->step[0].vx = (position->vx & 0xFFF80000);
        probe->step[0].vz = (position->vz & 0xFFF80000);
        probe->step[2].vx = (position->vx - probe->step[0].vx) >> 12;
        probe->step[2].vz = (position->vz - probe->step[0].vz) >> 12;
        probe->step[3].vx = (probe->step[1].vx - probe->step[0].vx) >> 12;
        probe->step[3].vz = (probe->step[1].vz - probe->step[0].vz) >> 12;
        probe->step[4].vy = 0;
        probe->step[4].vx = (probe->step[2].vz << 16) | (probe->step[2].vx & 0xFFFF);
        probe->step[4].vz = (probe->step[3].vz << 16) | (probe->step[3].vx & 0xFFFF);
        side = func_8004A70C(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side < 0) {
            scale = func_800945C8(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_800948D8(position, direction, probe->step, row);
        } else if (side > 0) {
            scale = func_800948D8(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = func_800945C8(position, direction, probe->step, row);
        } else {
            scale = func_800945C8(position, direction, probe->step, row);
        }
        break;
    }
    if (scale == 0) {
        func_80093354(&probe->step[1]);
        from = func_80093F18(&probe->step[1]);
        if (func_80094060(row, from) == 0) {
            probe->step[0] = probe->step[1];
            return 1;
        }
        return 0;
    }
    return scale;
}

/* Probe a move and choose the direction to slide along: 1 when free, 0 when
 * the obstacle deflects it (out holds the slide direction). */
/* Old-style definition: callers pass `mode` as an int, unconverted. */
s32 func_800951A8(position, direction, out, scale, mode)
    VECTOR *position;
    VECTOR *direction;
    VECTOR *out;
    s32 scale;
    s16 mode;
{
    s32 result;

    switch (func_80094A5C(position, direction, scale, mode)) {
    case 0:
        result = 1;
        break;
    case 1:
        if (func_80094088(SCRATCH_VECTOR, direction, out) == 0) {
            out->vz = 0;
            out->vy = 0;
            out->vx = 0;
        }
        result = 0;
        break;
    case 2:
        out->vx = direction->vx < 0 ? -0x1000 : 0x1000;
        out->vz = 0;
        out->vy = 0;
        result = 0;
        break;
    case 3:
        out->vy = 0;
        out->vx = 0;
        out->vz = direction->vz < 0 ? -0x1000 : 0x1000;
        result = 0;
        break;
    }
    return result;
}

/* Orient `normal` towards `direction` on the ground plane (zero when
 * perpendicular). */
void func_800952B0(VECTOR *direction, VECTOR *out, VECTOR *normal) {
    s32 dot;

    dot = normal->vx * direction->vx + normal->vz * direction->vz;
    if (dot < 0) {
        out->vx = -normal->vx;
        out->vz = -normal->vz;
    } else if (dot > 0) {
        out->vx = normal->vx;
        out->vz = normal->vz;
    } else {
        out->vz = 0;
        out->vx = 0;
    }
    out->vy = 0;
}

/* Orient the horizontal tangent of a wall normal towards `direction` (zero
 * when perpendicular). */
void func_80095324(VECTOR *normal, VECTOR *direction, VECTOR *out) {
    s32 tangent;
    s32 dot;

    SCRATCH_VECTOR[1].vz = 0;
    SCRATCH_VECTOR[1].vx = 0;
    SCRATCH_VECTOR[1].vy = -0x1000;
    func_8004A480(normal, &SCRATCH_VECTOR[1], &SCRATCH_VECTOR[2]);
    VectorNormal(&SCRATCH_VECTOR[2], &SCRATCH_VECTOR[0]);
    tangent = SCRATCH_VECTOR[0].vx;
    dot = tangent * direction->vx + SCRATCH_VECTOR[0].vz * direction->vz;
    if (dot < 0) {
        out->vx = -tangent;
        out->vz = -SCRATCH_VECTOR[0].vz;
    } else if (dot > 0) {
        out->vx = tangent;
        out->vz = SCRATCH_VECTOR[0].vz;
    } else {
        out->vz = 0;
        out->vx = 0;
    }
    out->vy = 0;
}

/* Scratchpad work area of the structure walk. */
typedef struct {
    VECTOR p[3];    /* 0x00: transformed face corners */
    VECTOR side[3]; /* 0x30: normalised face edge directions */
    VECTOR probe;   /* 0x60 */
    VECTOR offset;  /* 0x70: object-relative point; vy is the face height */
    VECTOR normal;  /* 0x80 */
} WalkScratch;

#define WALK_SCRATCH ((WalkScratch *)0x1F800000)

void func_80085158(VECTOR *position, VECTOR *offset, VECTOR *normal, u16 index, u16 face);
s32 func_80085760(VECTOR *from, VECTOR *to, s32 index, s32 face);

/* Move a walking position over the solid scene objects. Off a structure, look
 * for a face under the probe at about the current height and step onto it;
 * on one, follow the move across the face edges (bit n of the edge test: the
 * move leaves through edge n) onto the neighbouring faces, sliding along an
 * edge whose neighbour is a wall (kind 1) and dropping back to the terrain
 * when an edge has no neighbour. Returns 1 when the position stands on a face. */
s32 func_80095414(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode) {
    s16 object;
    WalkScratch *scratch;
    MeshFace *faces;
    s32 state;
    s32 count;
    s32 walking;
    s32 cleared;
    s32 i;
    s32 height;
    s32 result;
    u16 face;
    s16 first;
    s16 second;
    s32 edges;
    s32 walls;

    scratch = WALK_SCRATCH;
    scratch->probe.vx = position->vx + ((direction->vx * scale) >> 12);
    scratch->probe.vz = position->vz + ((direction->vz * scale) >> 12);
    func_80093354(&scratch->probe);
    scratch->probe.vy = func_80093978(scratch->probe.vx, scratch->probe.vz) - 0x4000;
    out->vx = position->vx + ((direction->vx * scale) >> 12);
    out->vz = position->vz + ((direction->vz * scale) >> 12);
    func_80093354(out);
    out->vy = func_80093978(out->vx, out->vz) - 0x4000;
    state = 1;
    if (D_8009C840 != -1) {
        state = 3;
    }
    switch (state) {
    case 0:
        result = func_800951A8(position, direction, out, scale, mode);
        D_8009C16C = -1;
        D_8009C840 = -1;
        break;
    case 1:
        count = func_80084D00((s32)&scratch->probe, &object);
        cleared = 0;
        if (count != 0) {
            for (i = 0; i < count; i += 2) {
                if (func_80085418(&scratch->probe, 0x70, object, D_8009D718[i]) == 0) {
                    D_8009D718[i] = -1;
                    cleared += 2;
                }
            }
            if (cleared != count) {
                result = 0;
                for (i = 0; i < count; i += 2) {
                    if (D_8009D718[i] != -1 && D_8009D718[i + 1] != 1) {
                        func_80085158(&scratch->probe, &scratch->offset, &scratch->normal, object, D_8009D718[i]);
                        height = scratch->offset.vy - (position->vy >> 12);
                        if (height < 0) {
                            height = -height;
                        }
                        if (height < 0xB) {
                            out->vy = scratch->offset.vy << 12;
                            D_8009C840 = object;
                            D_8009C16C = D_8009D718[i];
                            result = 1;
                            break;
                        }
                    }
                }
                if (result == 0) {
                    func_80095324(&scratch->normal, direction, out);
                }
                break;
            }
        }
        result = func_800951A8(position, direction, out, scale, mode);
        D_8009C16C = -1;
        D_8009C840 = -1;
        break;
    case 2:
    case 3:
        walking = 1;
        object = D_8009C840;
        face = D_8009C16C;
        faces = ((Mesh *)D_8009C620[object].unk44)->faces;
        do {
            i = func_80085760(position, &scratch->probe, object, (s16)face);
            switch (i) {
            case 0:
                func_80085158(&scratch->probe, &scratch->offset, &scratch->normal, object, face);
                result = 1;
                walking = 0;
                D_8009C16C = (s16)face;
                out->vy = scratch->offset.vy << 12;
                D_8009C840 = object;
                break;
            case 1:
                if (faces[(s16)face].next[0] == -1) {
                    result = func_800951A8(position, direction, out, scale, mode);
                    D_8009C16C = -1;
                    D_8009C840 = -1;
                    walking = 0;
                    break;
                }
                face = faces[(s16)face].next[0];
                if (faces[(s16)face].kind == 1) {
                    result = 0;
                    walking = 0;
                    func_800952B0(direction, out, &scratch->side[0]);
                }
                break;
            case 2:
                if (faces[(s16)face].next[1] == -1) {
                    result = func_800951A8(position, direction, out, scale, mode);
                    D_8009C16C = -1;
                    D_8009C840 = -1;
                    walking = 0;
                    break;
                }
                face = faces[(s16)face].next[1];
                if (faces[(s16)face].kind == 1) {
                    result = 0;
                    walking = 0;
                    func_800952B0(direction, out, &scratch->side[1]);
                }
                break;
            case 4:
                if (faces[(s16)face].next[2] == -1) {
                    result = func_800951A8(position, direction, out, scale, mode);
                    D_8009C16C = -1;
                    D_8009C840 = -1;
                    walking = 0;
                    break;
                }
                face = faces[(s16)face].next[2];
                if (faces[(s16)face].kind == 1) {
                    result = 0;
                    walking = 0;
                    func_800952B0(direction, out, &scratch->side[2]);
                }
                break;
            case 3: {
                s32 first_word;
                s32 second_word;

                edges = 0;
                first_word = faces[(s16)face].next[0];
                if (first_word != -1) {
                    edges |= 1;
                }
                first = faces[(s16)face].next[0];
                second_word = faces[(s16)face].next[1];
                second = faces[(s16)face].next[1];
                if (second_word != -1) {
                    edges |= 2;
                }
                switch (edges) {
                case 0:
                    result = func_800951A8(position, direction, out, scale, mode);
                    D_8009C16C = -1;
                    D_8009C840 = -1;
                    walking = 0;
                    break;
                case 1:
                    face = first;
                    if (faces[first].kind == edges) {
                        result = 0;
                        walking = 0;
                        func_800952B0(direction, out, &scratch->side[0]);
                    }
                    break;
                case 2:
                    face = second;
                    if (faces[second].kind == 1) {
                        result = 0;
                        walking = 0;
                        func_800952B0(direction, out, &scratch->side[1]);
                    }
                    break;
                case 3:
                    walls = faces[first].kind == 0;
                    if (faces[second].kind == 0) {
                        walls |= 2;
                    }
                    switch (walls) {
                    case 0:
                        result = 0;
                        walking = 0;
                        out->vx = out->vy = out->vz = 0;
                        break;
                    case 1:
                    case 3:
                        face = faces[(s16)face].next[0];
                        break;
                    case 2:
                        face = second;
                        break;
                    }
                    break;
                }
                break;
            }
            case 5: {
                s32 first_word;
                s32 second_word;

                edges = 0;
                first_word = faces[(s16)face].next[0];
                if (first_word != -1) {
                    edges |= 1;
                }
                first = faces[(s16)face].next[0];
                second_word = faces[(s16)face].next[2];
                second = faces[(s16)face].next[2];
                if (second_word != -1) {
                    edges |= 2;
                }
                switch (edges) {
                case 0:
                    result = func_800951A8(position, direction, out, scale, mode);
                    D_8009C16C = -1;
                    D_8009C840 = -1;
                    walking = 0;
                    break;
                case 1:
                    face = first;
                    if (faces[first].kind == edges) {
                        result = 0;
                        walking = 0;
                        func_800952B0(direction, out, &scratch->side[0]);
                    }
                    break;
                case 2:
                    face = second;
                    if (faces[second].kind == 1) {
                        result = 0;
                        walking = 0;
                        func_800952B0(direction, out, &scratch->side[2]);
                    }
                    break;
                case 3:
                    walls = faces[first].kind == 0;
                    if (faces[second].kind == 0) {
                        walls |= 2;
                    }
                    switch (walls) {
                    case 0:
                        result = 0;
                        walking = 0;
                        out->vx = out->vy = out->vz = 0;
                        break;
                    case 1:
                    case 3:
                        face = faces[(s16)face].next[0];
                        break;
                    case 2:
                        face = second;
                        break;
                    }
                    break;
                }
                break;
            }
            case 6: {
                s32 first_word;
                s32 second_word;

                edges = 0;
                first_word = faces[(s16)face].next[1];
                if (first_word != -1) {
                    edges |= 1;
                }
                first = faces[(s16)face].next[1];
                second_word = faces[(s16)face].next[2];
                second = faces[(s16)face].next[2];
                if (second_word != -1) {
                    edges |= 2;
                }
                switch (edges) {
                case 0:
                    result = func_800951A8(position, direction, out, scale, mode);
                    D_8009C16C = -1;
                    D_8009C840 = -1;
                    walking = 0;
                    break;
                case 1:
                    face = first;
                    if (faces[first].kind == edges) {
                        result = 0;
                        walking = 0;
                        func_800952B0(direction, out, &scratch->side[1]);
                    }
                    break;
                case 2:
                    face = second;
                    if (faces[second].kind == 1) {
                        result = 0;
                        walking = 0;
                        func_800952B0(direction, out, &scratch->side[2]);
                    }
                    break;
                case 3:
                    walls = faces[first].kind == 0;
                    if (faces[second].kind == 0) {
                        walls |= 2;
                    }
                    switch (walls) {
                    case 0:
                        result = 0;
                        walking = 0;
                        out->vx = out->vy = out->vz = 0;
                        break;
                    case 1:
                    case 3:
                        face = faces[(s16)face].next[1];
                        break;
                    case 2:
                        face = second;
                        break;
                    }
                    break;
                }
                break;
            }
            case 7:
                break;
            }
        } while (walking);
        break;
    }
    return result;
}

/* Move a flying position: clamp its height between the ground and the
 * ceiling, bounce back off solid objects, else slide along the terrain. */
s32 func_80095CD4(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode) {
    s16 hit;
    s32 floor;
    s32 count;
    s32 i;
    s32 cleared;
    s32 result;
    WalkScratch *scratch;

    scratch = WALK_SCRATCH;
    scratch->probe.vx = position->vx + ((direction->vx * scale) >> 12);
    scratch->probe.vy = position->vy + ((direction->vy * scale) >> 12);
    scratch->probe.vz = position->vz + ((direction->vz * scale) >> 12);
    func_80093354(&scratch->probe);
    out->vx = position->vx + ((direction->vx * scale) >> 12);
    out->vy = position->vy + ((direction->vy * scale) >> 12);
    out->vz = position->vz + ((direction->vz * scale) >> 12);
    func_80093354(out);
    floor = func_80093978(scratch->probe.vx, scratch->probe.vz) - 0x20000;
    if (floor > 0x20000) {
        floor = 0x20000;
    }
    if ((floor < scratch->probe.vy) | (floor < -0x280000)) {
        scratch->probe.vy = floor;
        out->vy = floor;
    }
    if ((floor > -0x280000) & (scratch->probe.vy < -0x280000)) {
        scratch->probe.vy = -0x280000;
        out->vy = -0x280000;
    }
    count = func_80084D00((s32)&scratch->probe, &hit);
    if (count != 0) {
        cleared = 0;
        for (i = 0; i < count; i += 2) {
            if (func_80085418(&scratch->probe, 0x70, hit, D_8009D718[i]) == 0) {
                D_8009D718[i] = -1;
                cleared += 2;
            }
        }
        result = 0;
        if (cleared != count) {
            out->vx = -direction->vx >> 1;
            out->vy = -direction->vy >> 1;
            out->vz = -direction->vz >> 1;
        } else {
            result = func_800951A8(position, direction, out, scale, mode);
        }
    } else {
        result = func_800951A8(position, direction, out, scale, mode);
    }
    return result;
}

/* Reset the stream queue and allocate its command buffers (disc or host). */
void func_80095F78(void) {
    s32 first;
    s32 second;
    s32 i;
    s8 *flag;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        D_8009BCB8 = 0;
        D_8009BE44 = 0;
        D_8009CD44 = 0;
        for (i = 0xF; i >= 0; i--) {
            D_8009D788[i] = NULL;
        }
        D_8009BE08 = func_80031BDC(0x4200, 0);
        D_8009D7D4 = func_80031BDC(0x800, 0);
        D_8009D808 = 0;
        for (i = 7, flag = &D_8009C588[7]; i >= 0; i--) {
            *flag-- = 0;
        }
    } else {
        D_8009BCB8 = 0;
        D_8009BE44 = 0;
        D_8009CD44 = 0;
        for (i = 0xF; i >= 0; i--) {
            D_8009C624[i] = NULL;
        }
        D_8009D3C0 = func_80031BDC(0x5800, 0);
        D_8009D7D4 = func_80031BDC(0x800, 0);
        D_8009D808 = 0;
        for (i = 7, flag = &D_8009C588[7]; i >= 0; i--) {
            *flag-- = 0;
        }
    }
}

/* Free the effect command buffers. */
void func_800960BC(void) {
    s32 first;
    s32 second;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        func_800320E8(D_8009BE08);
    } else {
        func_800320E8(D_8009D3C0);
    }
    func_800320E8(D_8009D7D4);
}

/* Wait until the current write slot of the stream queue is free. */
void func_80096130(void) {
    s32 first;
    s32 second;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        while (D_8009D788[D_8009BE44] != NULL) {
            VSync(0);
            func_800967E4();
        }
    } else {
        while (D_8009C624[D_8009BE44] != NULL) {
            VSync(0);
            func_800967E4();
        }
    }
}

/* Append a disc read request to the current frame's list. */
s32 func_8009623C(s32 sector, s32 bytes, u8 *destination) {
    DiscReadRequest *request;
    s32 count;

    count = D_8009D808;
    if (count < 0x58) {
        D_8009D808 = count + 1;
        request = (DiscReadRequest *)((u8 *)D_8009BE08 + D_8009BE44 * 0x420) + count;
        request->sector = sector;
        request->bytes = bytes;
        request->destination = destination;
        return 0;
    }
    return -1;
}

/* Append a host-file read request to the current frame's list. */
s32 func_800962B0(char *path, s32 offset, s32 bytes, u8 *destination) {
    HostReadRequest *request;
    s32 count;

    count = D_8009D808;
    if (count < 0x58) {
        D_8009D808 = count + 1;
        request = (HostReadRequest *)((u8 *)D_8009D3C0 + D_8009BE44 * 0x580) + count;
        request->path = path;
        request->offset = offset;
        request->bytes = bytes;
        request->destination = destination;
        return 0;
    }
    return -1;
}

/* Submit the current disc request list; -1 when there is nothing to
 * send or its ring slot is still busy. */
s32 func_80096328(void) {
    DiscReadRequest *list;

    list = (DiscReadRequest *)((u8 *)D_8009BE08 + D_8009BE44 * 0x420);
    if (list->sector != 0 && D_8009D788[D_8009BE44] == NULL) {
        func_800963E4(list);
        D_8009D808 = 0;
        D_8009D788[D_8009BE44] = list;
        D_8009BE44 = (D_8009BE44 + 1) & 0xF;
        return 0;
    }
    D_8009D808 = 0;
    return -1;
}

/* Sort a disc request list by sector (insertion sort in place). */
void func_800963E4(DiscReadRequest *list) {
    DiscReadRequest *first;
    DiscReadRequest *p;
    DiscReadRequest swap;

    p = list;
    first = p;
    while (p[1].sector != 0) {
        if ((u32)p[0].sector > (u32)p[1].sector) {
            swap.sector = p[0].sector;
            swap.bytes = p[0].bytes;
            swap.destination = p[0].destination;
            p[0].sector = p[1].sector;
            p[0].bytes = p[1].bytes;
            p[0].destination = p[1].destination;
            p[1].sector = swap.sector;
            p[1].bytes = swap.bytes;
            p[1].destination = swap.destination;
            if (first < p) {
                p--;
            }
        } else {
            p++;
        }
    }
}

/* Sort a host-file request list by offset (insertion sort in place). */
void func_800964B0(HostReadRequest *list) {
    HostReadRequest *first;
    HostReadRequest *p;
    HostReadRequest swap;

    p = list;
    first = p;
    while (p[1].path != NULL) {
        if ((u32)p[0].offset > (u32)p[1].offset) {
            swap.path = p[0].path;
            swap.offset = p[0].offset;
            swap.bytes = p[0].bytes;
            swap.destination = p[0].destination;
            p[0].path = p[1].path;
            p[0].offset = p[1].offset;
            p[0].bytes = p[1].bytes;
            p[0].destination = p[1].destination;
            p[1].path = swap.path;
            p[1].offset = swap.offset;
            p[1].bytes = swap.bytes;
            p[1].destination = swap.destination;
            if (first < p) {
                p--;
            }
        } else {
            p++;
        }
    }
}

/* Submit the current host-file request list; -1 when there is nothing to
 * send or its ring slot is still busy. */
s32 func_800965A4(void) {
    HostReadRequest *list;

    list = (HostReadRequest *)((u8 *)D_8009D3C0 + D_8009BE44 * 0x580);
    if (list->path != NULL && D_8009C624[D_8009BE44] == NULL) {
        func_800964B0(list);
        D_8009D808 = 0;
        D_8009C624[D_8009BE44] = list;
        D_8009BE44 = (D_8009BE44 + 1) & 0xF;
        return 0;
    }
    D_8009D808 = 0;
    return -1;
}

/* Frames queued between the writer and reader (ring of 16). */
s32 func_80096668(void) {
    s32 pending;

    pending = D_8009BE44 - D_8009BCB8;
    if (pending < 0) {
        pending += 0x10;
    }
    return pending;
}

/* Drain the queued frames, waiting for vertical sync between them. */
void func_80096694(void) {
    do {
        VSync(0);
        func_800967E4();
    } while (func_80096668() != 0);
}

/* Read a host-file request list, retrying each call up to eight times. */
void func_800966CC(HostReadRequest *request) {
    s32 fd;
    s32 i;

    D_8009BE48 = 0;
    D_8009CCB0 = 0;
    D_8009CCA8 = 0;
    D_8009CCA0 = 0;
    for (; request->path != NULL; request++) {
        for (i = 0; i < 8; i++) {
            fd = PCopen(request->path, 0, 0);
            if (fd != -1) {
                break;
            }
        }
        if (fd == -1) {
            continue;
        }
        PClseek(fd, request->offset, 0);
        for (i = 0; i < 8; i++) {
            if (func_8004C398(fd, request->destination, request->bytes) != 0) {
                break;
            }
        }
        for (i = 0; i < 8; i++) {
            if (PCclose(fd) == 0) {
                break;
            }
        }
    }
}

/* Step the stream queue: from disc, advance the reader and start the next
 * queued list when idle; from the host, read the next list at once. */
s32 func_800967E4(void) {
    s32 first;
    s32 second;
    s32 status;

    status = 0;
    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        status = func_800968E0();
        if (status == 0 && D_8009D788[D_8009BCB8] != NULL) {
            func_8009699C(D_8009D788[D_8009BCB8]);
        }
    } else if (D_8009C624[D_8009BCB8] != NULL) {
        func_800966CC(D_8009C624[D_8009BCB8]);
        D_8009C624[D_8009BCB8] = NULL;
        D_8009BCB8 = (D_8009BCB8 + 1) & 0xF;
    }
    return status;
}

/* Step the stream reader: 0 idle, 1 busy, 2 finished a frame, 3 error. */
s32 func_800968E0(void) {
    switch (D_8009CD44) {
    case 0:
        return 0;
    case 4:
        if (--D_8009BD2C == 0) {
            D_8009CD44++;
        }
    case 1:
    case 2:
    case 3:
        return 1;
    case 5:
        D_8009CD44 = 0;
        D_8009D788[D_8009BCB8] = NULL;
        D_8009BCB8 = (D_8009BCB8 + 1) & 0xF;
        return 2;
    default:
        return 3;
    }
}

/* Start reading a disc request list: seek to its first sector. */
void func_8009699C(DiscReadRequest *request) {
    s32 sector;

    sector = request->sector;
    D_8009CD44 = 1;
    D_8009D3BC = request;
    D_8009D3BC = request + 1;
    D_8009BE48 = 0;
    D_8009CCB0 = 0;
    D_8009CCA8 = 0;
    D_8009CCA0 = 0;
    D_8009D7F4 = sector;
    D_8009D614 = sector;
    D_8009D56C = (u32)(request->bytes + 0x7FF) >> 11;
    D_8009CEB8 = request->bytes;
    D_8009C590 = request->destination;
    CdIntToPos(sector, &D_8009CEBC);
    CdSyncCallback(func_80096A6C);
    CdControlF(CdlSetloc, (u8 *)&D_8009CEBC);
}

/* CD command-complete callback of the stream reader: after the seek start
 * reading, and recover from errors by pausing and seeking again. */
void func_80096A6C(s32 status, u8 *result) {
    if (status == 2) {
        switch (D_8009CD44) {
        case 1:
            D_8009CD44 = 2;
            D_8009BCCC[2] = 0;
            D_8009BCCC[1] = 0;
            D_8009BCCC[0] = 0;
            CdReadyCallback(func_80096C0C);
            CdControlF(0x1B, NULL);
            break;
        case 3:
            if (D_8009D614 == 0) {
                D_8009CD44 = 4;
                D_8009BD2C = 1;
                CdSyncCallback(NULL);
            }
            break;
        case 10:
            if (result[0] & 0x10) {
                CdControlF(1, NULL);
            } else {
                CdControlF(0x13, NULL);
                D_8009CD44 = 0xB;
            }
            break;
        case 11:
            D_8009CD44 = 0xC;
            CdControlF(CdlPause, NULL);
            break;
        case 12:
            D_8009CD44 = 1;
            CdIntToPos(D_8009D7F4, &D_8009CEBC);
            CdControlF(CdlSetloc, (u8 *)&D_8009CEBC);
            break;
        }
    } else if (result[0] & 0x10) {
        D_8009CD44 = 0xA;
        D_8009CCA8++;
        CdControlF(1, NULL);
    } else {
        D_8009CD44 = 0xB;
        CdControlF(0x13, NULL);
    }
}

/* CD data-ready callback of the stream reader: copy the sector to the
 * request's destination and continue with the next request, seeking when it
 * is not close ahead; pause at the end of the list. */
void func_80096C0C(s32 status, u8 *result) {
    DiscReadRequest *request;
    s32 sector;
    s32 next;

    if (status == 1) {
        CdGetSector(D_8009BCCC, 3);
        sector = CdPosToInt((CdlLOC *)D_8009BCCC);
        if (sector == D_8009D7F4) {
            if (D_8009D614 == sector) {
                if (D_8009CEB8 < 0x800) {
                    CdGetSector(D_8009C590, D_8009CEB8 / 4);
                    CdGetSector(D_8009D7D4, (0x800 - D_8009CEB8) / 4);
                } else {
                    CdGetSector(D_8009C590, 0x200);
                    D_8009CEB8 -= 0x800;
                }
                if (--D_8009D56C != 0) {
                    D_8009D614++;
                    D_8009C590 += 0x800;
                } else {
                    request = D_8009D3BC++;
                    next = request->sector;
                    D_8009D614 = next;
                    D_8009D56C = (u32)(request->bytes + 0x7FF) >> 11;
                    D_8009CEB8 = request->bytes;
                    D_8009C590 = request->destination;
                    if (next != 0) {
                        if (next - D_8009D7F4 >= 0x13) {
                            D_8009D7F4 = next;
                            D_8009CD44 = 1;
                            CdIntToPos(next, &D_8009CEBC);
                            CdControlF(CdlSetloc, (u8 *)&D_8009CEBC);
                            return;
                        }
                    } else {
                        D_8009CD44 = 3;
                        CdReadyCallback(NULL);
                        CdControlF(CdlPause, NULL);
                    }
                }
            }
            D_8009D7F4++;
            return;
        }
        D_8009CCA0++;
        CdReadyCallback(NULL);
        if (result[0] & 0x10) {
            D_8009CD44 = 0xA;
            D_8009CCA8++;
            CdControlF(1, NULL);
        } else {
            D_8009CD44 = 0xB;
            CdControlF(0x13, NULL);
        }
    } else {
        D_8009CCA0++;
        CdReadyCallback(NULL);
        if (result[0] & 0x10) {
            D_8009CD44 = 0xA;
            D_8009CCA8++;
            CdControlF(1, NULL);
        } else {
            D_8009CD44 = 0xB;
            CdControlF(0x13, NULL);
        }
    }
}

/* Scratchpad work area of the orbit camera. */
typedef struct {
    VECTOR offset;    /* 0x00 */
    VECTOR eye;       /* 0x10 */
    u8 pad20[0x80];
    SVECTOR angle;    /* 0xA0 */
    u8 padA8[0x48];
    MATRIX rotation;  /* 0xF0 */
} OrbitScratch;

#define ORBIT_SCRATCH ((OrbitScratch *)0x1F800000)

/* Place a camera orbiting above a position: look at its height from
 * `distance` along the angle, with the up direction rolled by the angle. */
void func_80096F18(u8 *buffer, Camera *camera, s32 distance, SVECTOR *angle) {
    LookAt *view = (LookAt *)buffer;
    SVECTOR *rotation;

    view->target.vx = 0;
    view->target.vy = camera->target.vy >> 12;
    view->target.vz = 0;
    ORBIT_SCRATCH->angle.vx = angle->vx;
    rotation = &ORBIT_SCRATCH->angle;
    ORBIT_SCRATCH->angle.vy = angle->vy;
    ORBIT_SCRATCH->angle.vz = 0;
    func_8004A92C(&ORBIT_SCRATCH->angle, &ORBIT_SCRATCH->rotation);
    ORBIT_SCRATCH->offset.vx = 0;
    ORBIT_SCRATCH->offset.vy = 0;
    ORBIT_SCRATCH->offset.vz = -(distance >> 12);
    ApplyMatrixLV(&ORBIT_SCRATCH->rotation, &ORBIT_SCRATCH->offset, &ORBIT_SCRATCH->eye);
    view->eye.vx = ORBIT_SCRATCH->eye.vx;
    view->eye.vy = view->target.vy + ORBIT_SCRATCH->eye.vy;
    view->eye.vz = ORBIT_SCRATCH->eye.vz;
    rotation->vx = 0;
    rotation->vy = angle->vy;
    rotation->vz = angle->vz;
    func_8004A92C(rotation, &ORBIT_SCRATCH->rotation);
    rotation->vx = 0;
    rotation->vy = -0x1000;
    rotation->vz = 0;
    ApplyMatrix(&ORBIT_SCRATCH->rotation, rotation, &view->up);
}

/* Recover rotation angles (yaw, then pitch, then roll) from a matrix. */
void func_80097070(MATRIX *m, SVECTOR *angle) {
    if (m->m[2][0] | m->m[2][2]) {
        angle->vy = ratan2(m->m[2][0], m->m[2][2]) & 0xFFF;
        *SCRATCH_MATRIX_A = *m;
        *SCRATCH_MATRIX_B = *(MATRIX *)&D_8009A180;
        func_8004AFEC(angle->vy, SCRATCH_MATRIX_B);
        MulMatrix0(SCRATCH_MATRIX_A, SCRATCH_MATRIX_B, SCRATCH_MATRIX_C);
        angle->vx = ratan2(SCRATCH_MATRIX_C->m[1][2], SCRATCH_MATRIX_C->m[1][1]);
        *SCRATCH_MATRIX_B = *(MATRIX *)&D_8009A180;
        func_8004AE4C(angle->vx, SCRATCH_MATRIX_B);
        MulMatrix0(SCRATCH_MATRIX_C, SCRATCH_MATRIX_B, SCRATCH_MATRIX_A);
        angle->vz = -ratan2(SCRATCH_MATRIX_A->m[1][0], SCRATCH_MATRIX_A->m[1][1]);
    }
}

/* Build the camera matrix looking from the eye to the target. */
void func_80097244(void *arg) {
    LookAt *view;

    view = arg;
    LOOKAT_SCRATCH->work.vx = -view->eye.vx + view->target.vx;
    LOOKAT_SCRATCH->work.vy = -view->eye.vy + view->target.vy;
    LOOKAT_SCRATCH->work.vz = -view->eye.vz + view->target.vz;
    VectorNormal(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->forward);
    func_8004A480(&LOOKAT_SCRATCH->forward, &view->up, &LOOKAT_SCRATCH->work);
    VectorNormal(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->right);
    func_8004A480(&LOOKAT_SCRATCH->forward, &LOOKAT_SCRATCH->right, &LOOKAT_SCRATCH->work);
    VectorNormal(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->up);
    D_8009C808.m[0][0] = LOOKAT_SCRATCH->right.vx;
    D_8009C808.m[0][1] = LOOKAT_SCRATCH->right.vy;
    D_8009C808.m[0][2] = LOOKAT_SCRATCH->right.vz;
    D_8009C808.m[1][0] = LOOKAT_SCRATCH->up.vx;
    D_8009C808.m[1][1] = LOOKAT_SCRATCH->up.vy;
    D_8009C808.m[1][2] = LOOKAT_SCRATCH->up.vz;
    D_8009C808.m[2][0] = LOOKAT_SCRATCH->forward.vx;
    D_8009C808.m[2][1] = LOOKAT_SCRATCH->forward.vy;
    D_8009C808.m[2][2] = LOOKAT_SCRATCH->forward.vz;
    LOOKAT_SCRATCH->eye.vx = -view->eye.vx;
    LOOKAT_SCRATCH->eye.vy = -view->eye.vy;
    LOOKAT_SCRATCH->eye.vz = -view->eye.vz;
    LOOKAT_SCRATCH->view = D_8009C808;
    ApplyMatrix(&LOOKAT_SCRATCH->view, &LOOKAT_SCRATCH->eye, &LOOKAT_SCRATCH->work);
    TransMatrix(&D_8009C808, &LOOKAT_SCRATCH->work);
}

/* Build the camera matrix from the camera angle and eye position. */
void func_80097440(void *arg) {
    SVECTOR *eye;

    eye = arg;
    *SCRATCH_MATRIX_A = *(MATRIX *)&D_8009A180;
    *SCRATCH_MATRIX_B = *SCRATCH_MATRIX_A;
    *SCRATCH_MATRIX_C = *SCRATCH_MATRIX_A;
    func_8004AE4C(-D_8009BD38.vx, SCRATCH_MATRIX_A);
    func_8004AFEC(-D_8009BD38.vy, SCRATCH_MATRIX_B);
    RotMatrixZ(-D_8009BD38.vz, SCRATCH_MATRIX_C);
    MulMatrix0(SCRATCH_MATRIX_A, SCRATCH_MATRIX_B, SCRATCH_MATRIX_D);
    MulMatrix0(SCRATCH_MATRIX_C, SCRATCH_MATRIX_D, &D_8009C808);
    SCRATCH_SVECTOR->vx = -eye->vx;
    SCRATCH_SVECTOR->vy = -eye->vy;
    SCRATCH_SVECTOR->vz = -eye->vz;
    *SCRATCH_MATRIX_A = D_8009C808;
    ApplyMatrix(SCRATCH_MATRIX_A, SCRATCH_SVECTOR, SCRATCH_VECTOR);
    TransMatrix(&D_8009C808, SCRATCH_VECTOR);
}

/* Allocate and clear the 64 actor slots. */
void func_8009766C(void) {
    D_8009BE24 = func_80031BDC(0x2000, 0);
    func_800976C8();
}

/* Free the actor slots. */
void func_800976A0(void) {
    func_800320E8(D_8009BE24);
}

/* Mark every actor slot free. */
void func_800976C8(void) {
    WorldmapActor *actor;
    s32 i;

    for (i = 0; i < 0x40; i++) {
        actor = &D_8009BE24[i];
        actor->handle = 0;
        actor->kind = 0;
        actor->update = 0;
    }
}

/* Change an actor's kind and clear its command. */
void func_800976FC(s32 kind, s32 index) {
    D_8009BE24[index].command = 0;
    D_8009BE24[index].kind = kind;
}

/* Start an actor in the first free slot. */
void func_80097718(s32 kind, s32 update) {
    WorldmapActor *actor;
    s32 i;

    for (i = 0; i < 0x40; i++) {
        actor = &D_8009BE24[i];
        if (actor->update == 0) {
            actor->command = 0;
            actor->command_arg = 0;
            actor->unk4 = 0;
            actor->kind = kind;
            actor->update = update;
            actor->state = 0;
            actor->wait = 0;
            return;
        }
    }
}

/* Send command 1 with an argument unless one is pending; 1 when sent. */
s32 func_80097770(s32 index, s32 arg) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    if (actor->unk4 == 0) {
        actor->command = 1;
        actor->unk4 = arg;
        return 1;
    }
    return 0;
}

/* Send command 3. */
void func_800977A8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->command = 3;
}

/* Send command 4. */
void func_800977C4(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->command = 4;
}

/* Send command 2 with an argument. */
void func_800977E0(s32 index, s16 arg) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->command = 2;
    actor->command_arg = arg;
}

/* Run every active actor's pending command: 0 start, 1 step, 2 wait, 3 idle,
 * 4 release its handle. */
void func_80097800(void) {
    WorldmapActor *actor;
    s32 i;

    actor = D_8009BE24;
    for (i = 0; i < 0x40; i++, actor++) {
        if (actor->update != 0) {
            switch (actor->command) {
            case 3:
                break;
            case 0:
                actor->command = ((ActorFunc)actor->kind)(i);
                break;
            case 1:
                actor->command = ((ActorFunc)actor->update)(i);
                break;
            case 2:
                if (--actor->command_arg <= 0) {
                    actor->command = 1;
                }
                break;
            case 4:
                if (actor->handle != 0) {
                    func_800230A8(actor->handle);
                }
                break;
            }
        }
    }
}

/* The 2048-triangle terrain packet buffer, copied as a whole. */
typedef struct {
    POLY_FT3 prims[0x800];
} TriangleBuffer;

/* Allocate both 2048-triangle terrain packet buffers and initialise them. */
void func_800978FC(void) {
    POLY_FT3 *prim;
    s32 i;

    D_8009BBC8[0].packets = func_80031BDC(0x10000, 1);
    D_8009BBC8[1].packets = func_80031BDC(0x10000, 1);
    prim = D_8009BBC8[0].packets;
    for (i = 0; i < 0x800; i++, prim++) {
        setlen(prim, 7);
        setcode(prim, 0x24);
        setRGB0(prim, 0x80, 0x80, 0x80);
    }
    *(TriangleBuffer *)D_8009BBC8[1].packets = *(TriangleBuffer *)D_8009BBC8[0].packets;
}

/* Upload the terrain texture image (its buffer is then reused for the
 * palettes), build the faded terrain palettes and their CLUT and texture
 * page ids. */
void func_800979C8(void) {
    RECT rect;
    u16 *cluts;
    u16 *faded;
    s32 i;
    s32 x;
    s32 y;

    cluts = func_80032E88(D_8009C59C, 1);
    func_8002DD20(cluts);
    DrawSync(0);
    func_800320E8(cluts);
    func_800320E8(D_8009C59C);
    cluts = func_80031BDC(0x400, 1);
    faded = func_80031BDC(0x8000, 1);
    rect.x = 0;
    rect.y = 0x1E0;
    rect.w = 0x100;
    rect.h = 2;
    StoreImage(&rect, cluts);
    DrawSync(0);
    func_800931D8(cluts, faded, 0x20, D_8009BB48);
    func_800931D8(cluts + 0x100, faded + 0x2000, 0x20, D_8009BB48);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x100;
    rect.h = 0x40;
    LoadImage(&rect, faded);
    DrawSync(0);
    for (i = 0; i < 0x40; i++) {
        D_8009CCB4[i] = GetClut(rect.x, rect.y);
        rect.y++;
    }
    for (x = 0x200, y = 0, i = 0; i < 4; i++) {
        D_8009CD54[i] = GetTPage(1, 0, x, y);
        x += 0x80;
    }
    for (x = 0x180, y = 0x100, i = 4; i < 7; i++) {
        D_8009CD54[i] = GetTPage(1, 0, x, y);
        x += 0x80;
    }
    func_800320E8(faded);
    func_800320E8(cluts);
}

/* Reset the terrain loader around a position. */
void func_80097BC0(VECTOR *position) {
    s32 i;

    D_8009D534 = D_8009A180;
    for (i = 0xFF; i >= 0; i--) {
        D_8009C184[i] = NULL;
    }
    TERRAIN_ORIGIN.vx = position->vx & 0x7FFFFF;
    TERRAIN_ORIGIN.vy = 0;
    D_8009C5BC = 0;
    D_8009C618 = 0x400;
    TERRAIN_ORIGIN.vz = position->vz & 0x7FFFFF;
    D_8009C838.vx = 2;
    D_8009C838.vy = 0;
    D_8009C838.vz = 2;
    func_800981C8((Camera *)position);
    func_80097DC0();
}

/* Reset the terrain loader around the camera. */
void func_80097CB8(Camera *camera) {
    s32 i;

    D_8009D534 = D_8009A180;
    for (i = 0xFF; i >= 0; i--) {
        D_8009C184[i] = NULL;
    }
    D_8009C5BC = 0;
    D_8009C618 = 0x400;
    func_800981C8(camera);
    func_80097DC0();
}

/* Free every loaded terrain block. */
void func_80097D64(void) {
    s32 i;

    for (i = 0; i < 0x100; i++) {
        if (D_8009C184[i] != NULL) {
            func_800320E8(D_8009C184[i]);
        }
    }
}

/* Queue reads of the terrain blocks around the camera that are not loaded:
 * from disc the centre 3x3 first, then the whole 9x9 grid. */
void func_80097DC0(void) {
    s32 first;
    s32 second;
    s32 row;
    s32 column;
    s32 block;
    void *buffer;
    s32 sector;
    char *path;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        sector = func_800289D0(D_8009BCD8);
        for (row = 3; row < 6; row++) {
            for (column = 3; column < 6; column++) {
                block = D_8009D570.cells[row * 9 + column];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + block, 0x710, buffer);
                }
            }
        }
        func_8009623C(0, 0, NULL);
        func_80096328();
        for (row = 0; row < 9; row++) {
            for (column = 0; column < 9; column++) {
                block = D_8009D570.cells[row * 9 + column];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + block, 0x710, buffer);
                }
            }
        }
        func_8009623C(0, 0, NULL);
        func_80096328();
    } else {
        path = func_80028998(D_8009BCD8);
        for (row = 0; row < 9; row++) {
            for (column = 0; column < 9; column++) {
                block = D_8009D570.cells[row * 9 + column];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_800962B0(path, block << 11, 0x710, buffer);
                }
            }
        }
        func_800962B0(NULL, 0, 0, NULL);
        func_800965A4();
    }
}

/* Compute the four horizon plane normals. */
void func_80098044(void) {
    OuterProduct0(&D_8009BB6C, &D_8009BB4C, &D_8009C828);
    OuterProduct0(&D_8009BB4C, &D_8009BB7C, &D_8009C844);
    OuterProduct0(&D_8009BB8C, &D_8009BB5C, &D_8009C874);
    OuterProduct0(&D_8009BB5C, &D_8009BB9C, &D_8009C7F0);
}

/* Wrap a position into the map and note the crossed edges (8/4 in x,
 * 2/1 in z); update the camera's block cell. */
void func_800980D4(void *arg) {
    VECTOR *position;
    s32 x;
    s32 z;

    position = arg;
    x = position->vx;
    z = position->vz;
    D_8009D558 = 0;
    if (x < -0x800000) {
        position->vx = x + 0x800000;
        D_8009D558 = 4;
    } else if (x > 0x800000) {
        position->vx = x - 0x800000;
        D_8009D558 = 8;
    }
    if (z < -0x800000) {
        position->vz += 0x800000;
        D_8009D558 |= 1;
    } else if (z > 0x800000) {
        position->vz -= 0x800000;
        D_8009D558 |= 2;
    }
    D_8009C838.vx = (position->vx >> 23) + 2;
    D_8009C838.vz = (position->vz >> 23) + 2;
}

/* Recompute the 9x9 grid of terrain blocks around the camera (keeping the
 * previous grid), wrapping around the map edges. */
void func_800981C8(Camera *camera) {
    s32 width;
    s32 height;
    s32 x;
    s32 z;
    s32 left;
    s32 base;
    s32 i;
    s32 j;
    s16 *cell;

    width = D_8009D160;
    height = D_8009D2B4;
    x = (camera->target.vx >> 12) / 8 - ((D_8009C838.vx + 2) << 8);
    z = (camera->target.vz >> 12) / 8 - ((D_8009C838.vz + 2) << 8);
    if (x < 0) {
        x += width << 8;
    } else if (x > width << 8) {
        x -= width << 8;
    }
    if (z < 0) {
        z += height << 8;
    } else if (z > height << 8) {
        z -= height << 8;
    }
    x >>= 8;
    z >>= 8;
    D_8009D318 = D_8009D570;
    left = x;
    cell = D_8009D570.cells;
    for (j = 8; j != -1; j--) {
        x = left;
        if (z >= height) {
            z = 0;
        }
        base = z * width;
        for (i = 8; i != -1; i--) {
            if (x >= width) {
                x = 0;
            }
            *cell++ = base + x;
            x++;
        }
        z++;
    }
}

/* Classify the 5x5 terrain blocks around the camera: test each block's
 * quad for visibility, and its four quarters when partly visible; blocks
 * near the camera are always visible. The block pointer, row and column
 * are reused for the always-visible pass, as the original does. */
void func_800983A0(Camera *camera) {
    s16 *block;
    u32 *quarters;
    u32 visible;
    s32 row;
    s32 column;
    s16 result;
    GridScratch *scratch;

    scratch = GRID_SCRATCH;
    GRID_SCRATCH->local = D_8009D534;
    CompMatrix(&D_8009C808, &GRID_SCRATCH->local, &GRID_SCRATCH->world);
    SetRotMatrix(&GRID_SCRATCH->world);
    SetTransMatrix(&GRID_SCRATCH->world);
    block = D_8009D618;
    quarters = (u32 *)D_8009D650[0];
    GRID_SCRATCH->x0 = -((camera->target.vx >> 12) & 0x7FF) - 0x1000;
    GRID_SCRATCH->z0 = -((camera->target.vz >> 12) & 0x7FF) - 0x1000;
    GRID_SCRATCH->v[8].vy = 0;
    GRID_SCRATCH->v[7].vy = 0;
    GRID_SCRATCH->v[6].vy = 0;
    GRID_SCRATCH->v[5].vy = 0;
    GRID_SCRATCH->v[4].vy = 0;
    GRID_SCRATCH->v[3].vy = 0;
    GRID_SCRATCH->v[2].vy = 0;
    GRID_SCRATCH->v[1].vy = 0;
    GRID_SCRATCH->v[0].vy = 0;
    GRID_SCRATCH->v[0].vz = -GRID_SCRATCH->z0;
    for (row = 0; row < 5; row++) {
        scratch->v[0].vx = scratch->x0;
        for (column = 0; column < 5; quarters += 2, scratch->v[0].vx += 0x800, block++, column++) {
            scratch->v[1].vx = scratch->v[0].vx + 0x800;
            scratch->v[1].vz = scratch->v[0].vz;
            scratch->v[2].vx = scratch->v[0].vx;
            scratch->v[2].vz = scratch->v[0].vz - 0x800;
            scratch->v[3].vx = scratch->v[0].vx + 0x800;
            scratch->v[3].vz = scratch->v[0].vz - 0x800;
            result = func_800987AC(&scratch->v[0], &scratch->v[1], &scratch->v[2],
                                   &scratch->v[3]);
            *block = result;
            if (result == 0) {
                scratch->v[4].vx = scratch->v[0].vx + 0x400;
                scratch->v[4].vz = scratch->v[0].vz;
                scratch->v[5].vx = scratch->v[0].vx;
                scratch->v[5].vz = scratch->v[0].vz - 0x400;
                scratch->v[6].vx = scratch->v[1].vx;
                scratch->v[6].vz = scratch->v[5].vz;
                scratch->v[7].vx = scratch->v[4].vx;
                scratch->v[7].vz = scratch->v[2].vz;
                scratch->v[8].vx = scratch->v[4].vx;
                scratch->v[8].vz = scratch->v[5].vz;
                visible = func_800987AC(&scratch->v[0], &scratch->v[4], &scratch->v[5],
                                        &scratch->v[8]) & 0xFFFF;
                visible |= func_800987AC(&scratch->v[4], &scratch->v[1], &scratch->v[8],
                                         &scratch->v[6]) << 16;
                quarters[0] = visible;
                visible = func_800987AC(&scratch->v[5], &scratch->v[8], &scratch->v[2],
                                        &scratch->v[7]) & 0xFFFF;
                visible |= func_800987AC(&scratch->v[8], &scratch->v[6], &scratch->v[7],
                                         &scratch->v[3]) << 16;
            } else {
                visible = result & 0xFFFF;
                visible |= visible << 16;
                quarters[0] = visible;
            }
            quarters[1] = visible;
        }
        scratch->v[0].vz -= 0x800;
    }
    /* The camera's quadrant within its block picks the always-visible set. */
    column = 0;
    if (((camera->target.vx >> 12) & 0x7FF) >= 0x400) {
        column = 1;
    }
    if (((camera->target.vz >> 12) & 0x7FF) >= 0x400) {
        column |= 2;
    }
    block = (s16 *)D_8009B7A8[column][0];
    quarters = (u32 *)D_8009D650[0];
    for (row = 0; row < 25; row++) {
        quarters[0] |= ((u32 *)block)[0];
        quarters[1] |= ((u32 *)block)[1];
        block += 4;
        quarters += 2;
    }
}

/* Classify the quad a, b, c, d (two rows of two corners, so its edges are
 * a-b, b-d, d-c and c-a) against the four horizon planes of 80098044,
 * after RT with the loaded matrix: -1 when all four corners lie outside one
 * plane, 1 when no plane has a whole edge outside it, else 0. Each plane
 * counts the edges whose two corners are both on its negative side; the
 * first two planes use x and z, the other two y and z. */
s16 func_800987AC(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *d) {
    VECTOR *view = GRID_SCRATCH->view;
    s16 out0;
    s16 out1;
    s16 out2;
    s16 out3;
    s32 d0;
    s32 d1;
    s32 d2;
    s32 d3;

    gte_ldv0(a);
    gte_rt();
    gte_stlvnl(&view[0]);
    gte_ldv0(b);
    gte_rt();
    gte_stlvnl(&view[1]);
    gte_ldv0(c);
    gte_rt();
    gte_stlvnl(&view[2]);
    gte_ldv0(d);
    gte_rt();
    gte_stlvnl(&view[3]);

    out0 = 0;
    d0 = (view[0].vx * D_8009C828.vx + view[0].vz * D_8009C828.vz) >> 12;
    d1 = (view[1].vx * D_8009C828.vx + view[1].vz * D_8009C828.vz) >> 12;
    d2 = (view[2].vx * D_8009C828.vx + view[2].vz * D_8009C828.vz) >> 12;
    d3 = (view[3].vx * D_8009C828.vx + view[3].vz * D_8009C828.vz) >> 12;
    if (d0 < 0 && d1 < 0) {
        out0++;
    }
    if (d1 < 0 && d3 < 0) {
        out0++;
    }
    if (d3 < 0 && d2 < 0) {
        out0++;
    }
    if (d2 < 0 && d0 < 0) {
        out0++;
    }
    if (out0 == 4) {
        return -1;
    }

    out1 = 0;
    d0 = (view[0].vx * D_8009C844.vx + view[0].vz * D_8009C844.vz) >> 12;
    d1 = (view[1].vx * D_8009C844.vx + view[1].vz * D_8009C844.vz) >> 12;
    d2 = (view[2].vx * D_8009C844.vx + view[2].vz * D_8009C844.vz) >> 12;
    d3 = (view[3].vx * D_8009C844.vx + view[3].vz * D_8009C844.vz) >> 12;
    if (d0 < 0 && d1 < 0) {
        out1++;
    }
    if (d1 < 0 && d3 < 0) {
        out1++;
    }
    if (d3 < 0 && d2 < 0) {
        out1++;
    }
    if (d2 < 0 && d0 < 0) {
        out1++;
    }
    if (out1 == 4) {
        return -1;
    }

    out2 = 0;
    d0 = (view[0].vy * D_8009C874.vy + view[0].vz * D_8009C874.vz) >> 12;
    d1 = (view[1].vy * D_8009C874.vy + view[1].vz * D_8009C874.vz) >> 12;
    d2 = (view[2].vy * D_8009C874.vy + view[2].vz * D_8009C874.vz) >> 12;
    d3 = (view[3].vy * D_8009C874.vy + view[3].vz * D_8009C874.vz) >> 12;
    if (d0 < 0 && d1 < 0) {
        out2++;
    }
    if (d1 < 0 && d3 < 0) {
        out2++;
    }
    if (d3 < 0 && d2 < 0) {
        out2++;
    }
    if (d2 < 0 && d0 < 0) {
        out2++;
    }
    if (out2 == 4) {
        return -1;
    }

    out3 = 0;
    d0 = (view[0].vy * D_8009C7F0.vy + view[0].vz * D_8009C7F0.vz) >> 12;
    d1 = (view[1].vy * D_8009C7F0.vy + view[1].vz * D_8009C7F0.vz) >> 12;
    d2 = (view[2].vy * D_8009C7F0.vy + view[2].vz * D_8009C7F0.vz) >> 12;
    d3 = (view[3].vy * D_8009C7F0.vy + view[3].vz * D_8009C7F0.vz) >> 12;
    if (d0 < 0 && d1 < 0) {
        out3++;
    }
    if (d1 < 0 && d3 < 0) {
        out3++;
    }
    if (d3 < 0 && d2 < 0) {
        out3++;
    }
    if (d2 < 0 && d0 < 0) {
        out3++;
    }
    if (out3 != 4) {
        return (out1 | out0 | out2 | out3) == 0;
    }
    return -1;
}

/* After the grid moved: free the blocks that left it and queue reads of the
 * new edge blocks, rows from the row-major file and columns from the
 * column-major file; corners from whichever edge changed. n first holds the
 * read-from-disc test and then each edge's first cell; the pass counter j
 * also holds each corner's block number, and a corner reloads the source
 * into sector/path. Each branch keeps its own edge flags. */
void func_80098CC0(void) {
    s32 first;
    s32 second;
    s32 i;
    s32 j;
    s32 k;
    s32 n;
    s32 block;
    s32 sector;
    char *path;
    s32 changed;
    void *buffer;

    for (i = 0; i < 81; i++) {
        block = D_8009D318.cells[i];
        if (D_8009C184[block] != NULL) {
            for (j = 0; j < 81; j++) {
                if (D_8009D570.cells[j] == block) {
                    break;
                }
            }
            if (j == 81) {
                func_800320E8(D_8009C184[block]);
                D_8009C184[block] = NULL;
            }
        }
    }
    changed = 0;
    first = func_8002C3D8();
    second = func_8002C3D8();
    n = first == 0;
    n |= second == -1;
    if (n) {
        s32 rows;
        s32 cols;

        sector = func_800289D0(D_8009BCD8);
        n = 1;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n++) {
                block = D_8009D570.cells[n];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + block, 0x710, buffer);
                    changed |= 2;
                }
            }
            n = 0x49;
        }
        sector = func_800289D0(D_8009BD08);
        n = 9;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n += 9) {
                block = D_8009D570.cells[n];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_8009623C(sector + ((block % D_8009D160) * D_8009D2B4 + block / D_8009D160),
                                  0x710, buffer);
                    changed |= 1;
                }
            }
            n = 0x11;
        }
        for (k = 0; k < 4; k++) {
            j = D_8009D570.cells[D_8009BBAC[k]];
            rows = changed & 2;
            cols = changed & 1;
            if (D_8009C184[j] == NULL) {
                D_8009C184[j] = func_80031BDC(0x710, 0);
                if (rows) {
                    sector = func_800289D0(D_8009BCD8);
                    func_8009623C(sector + j, 0x710, D_8009C184[j]);
                } else if (cols) {
                    sector = func_800289D0(D_8009BD08);
                    func_8009623C(sector + ((j % D_8009D160) * D_8009D2B4 + j / D_8009D160), 0x710,
                                  D_8009C184[j]);
                }
            }
        }
        func_8009623C(0, 0, NULL);
        func_80096328();
    } else {
        s32 rows;
        s32 cols;

        path = func_80028998(D_8009BCD8);
        n = 1;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n++) {
                block = D_8009D570.cells[n];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_800962B0(path, block << 11, 0x710, buffer);
                    changed |= 2;
                }
            }
            n = 0x49;
        }
        path = func_80028998(D_8009BD08);
        n = 9;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n += 9) {
                block = D_8009D570.cells[n];
                if (D_8009C184[block] == NULL) {
                    buffer = func_80031BDC(0x710, 0);
                    D_8009C184[block] = buffer;
                    func_800962B0(path,
                                  ((block % D_8009D160) << 11) * D_8009D2B4 + ((block / D_8009D160) << 11),
                                  0x710, buffer);
                    changed |= 1;
                }
            }
            n = 0x11;
        }
        for (k = 0; k < 4; k++) {
            rows = changed & 2;
            cols = changed & 1;
            j = D_8009D570.cells[D_8009BBAC[k]];
            if (D_8009C184[j] == NULL) {
                D_8009C184[j] = func_80031BDC(0x710, 0);
                if (rows) {
                    path = func_80028998(D_8009BCD8);
                    func_800962B0(path, j << 11, 0x710, D_8009C184[j]);
                } else if (cols) {
                    path = func_80028998(D_8009BD08);
                    func_800962B0(path,
                                  ((j % D_8009D160) << 11) * D_8009D2B4 + ((j / D_8009D160) << 11),
                                  0x710, D_8009C184[j]);
                }
            }
        }
        func_800962B0(NULL, 0, 0, NULL);
        func_800965A4();
    }
}

/* Draw the visible 5x5 terrain blocks around the camera: all four quarters
 * of a block, or only the quarters whose flag differs when the combined
 * flags are all set. */
void func_8009932C(u32 *ot, s32 packets, Camera *camera) {
    TerrainDrawScratch *scratch;
    u8 *data;
    s32 row;
    s32 column;
    s32 cell;
    s16 all;

    scratch = (TerrainDrawScratch *)0x1F800000;
    for (column = 0; column < 0x40; column++) {
        scratch->clut[column] = D_8009CCB4[column];
    }
    for (column = 0; column < 7; column++) {
        scratch->tpage[column] = D_8009CD54[column];
    }
    scratch->local = D_8009D534;
    CompMatrix(&D_8009C808, &scratch->local, &scratch->world);
    SetRotMatrix(&scratch->world);
    SetTransMatrix(&scratch->world);
    scratch->x0 = -((camera->target.vx >> 12) & 0x7FF) - 0x1000;
    cell = 0;
    scratch->z0 = -((camera->target.vz >> 12) & 0x7FF) - 0x1000;
    D_8009D7DC = 0;
    scratch->corner[0].vz = -scratch->z0;
    for (row = 0; row < 5; row++) {
        scratch->corner[0].vx = scratch->x0;
        for (column = 0; column < 5; column++, cell++, scratch->corner[0].vx += 0x800) {
            if (D_8009D618[cell] != -1) {
                data = D_8009C184[D_8009D570.cells[(row + D_8009C838.vz) * 9 + column + D_8009C838.vx]];
                scratch->corner[3].vx = scratch->corner[1].vx = scratch->corner[0].vx + 0x400;
                scratch->corner[1].vz = scratch->corner[0].vz;
                scratch->corner[2].vx = scratch->corner[0].vx;
                scratch->corner[3].vz = scratch->corner[2].vz = scratch->corner[0].vz - 0x400;
                all = (u16)D_8009D650[cell][3] |
                      ((u16)D_8009D650[cell][2] |
                       ((u16)D_8009D650[cell][0] | (u16)D_8009D650[cell][1]));
                if (all != -1) {
                    func_80099708((u32 *)data, ot, packets + (D_8009D7DC << 5), &scratch->corner[0]);
                    func_80099708((u32 *)(data + 0x144), ot, packets + (D_8009D7DC << 5), &scratch->corner[1]);
                    func_80099708((u32 *)(data + 0x288), ot, packets + (D_8009D7DC << 5), &scratch->corner[2]);
                    func_80099708((u32 *)(data + 0x3CC), ot, packets + (D_8009D7DC << 5), &scratch->corner[3]);
                } else {
                    if (D_8009D650[cell][0] != all) {
                        func_80099708((u32 *)data, ot, packets + (D_8009D7DC << 5), &scratch->corner[0]);
                    }
                    if (D_8009D650[cell][1] != all) {
                        func_80099708((u32 *)(data + 0x144), ot, packets + (D_8009D7DC << 5), &scratch->corner[1]);
                    }
                    if (D_8009D650[cell][2] != all) {
                        func_80099708((u32 *)(data + 0x288), ot, packets + (D_8009D7DC << 5), &scratch->corner[2]);
                    }
                    if (D_8009D650[cell][3] != all) {
                        func_80099708((u32 *)(data + 0x3CC), ot, packets + (D_8009D7DC << 5), &scratch->corner[3]);
                    }
                }
            }
        }
        scratch->corner[0].vz -= 0x800;
    }
}

/* Build a terrain block's 9x9 vertices in the scratchpad (heights of
 * water cells follow two travelling sine waves), then draw the block. */
void func_80099708(u32 *heights, u32 *ot, s32 packets, SVECTOR *origin) {
    SVECTOR *vertex;
    u32 *cell;
    s32 j;
    s16 (*sine)[2];
    s32 row_phase;
    s32 left;
    s32 z;
    s32 x;
    s32 i;
    s32 phase;
    s32 swell;
    s32 height;
    s32 wave;

    vertex = (SVECTOR *)0x1F800000;
    cell = heights;
    j = 8;
    sine = D_800523F0;
    row_phase = D_8009C618;
    left = origin->vx;
    z = origin->vz;
    for (; j != -1; j--) {
        x = left;
        i = 8;
        swell = sine[row_phase & 0xFFF][0] * 2;
        phase = D_8009C5BC;
        for (; i != -1; i--) {
            if (*cell & 0x1000) {
                height = (s32)(*cell << 24) >> 21;
                wave = (sine[phase & 0xFFF][0] * swell) >> 20;
                *(s32 *)&vertex->vx = ((wave + height) << 16) | (x & 0xFFFF);
            } else {
                *(s32 *)&vertex->vx = ((s32)(*cell << 24) >> 5) | (x & 0xFFFF);
            }
            vertex->vz = z;
            x += 0x80;
            vertex++;
            cell++;
            phase += 0x200;
        }
        z -= 0x80;
        row_phase += 0x200;
    }
    func_8009980C(heights, ot, packets);
}

INCLUDE_ASM("decomp/src/worldmap", func_8009980C);

INCLUDE_ASM("decomp/src/worldmap", func_80099BFC);
