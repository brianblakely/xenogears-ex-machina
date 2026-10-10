/* World map unit 80094A5C-80099E8C (rodata 80070C50-80070CFC, data
 * 8009B564-8009BBB4): movement over the terrain and the solid scene objects,
 * the stream reader, the camera matrices, the actor slots, the terrain
 * loader, its visibility and drawing (two routines handwritten), and the
 * world tables of the whole overlay in its data.
 *
 * worldmap_camera_follow_update's 17-entry table ends at 80070c50 and worldmap_terrain_probe_move's
 * follows at once, 0 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after worldmap_camera_follow_update, at or before
 * worldmap_terrain_probe_move. Its text and data end the program. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "resident/stream.h"
#include "resident/text.h"
#include "worldmap.h"
#include "camera.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* Defined returning s16 (worldmap_objects_effects_party); this unit uses the value as an int. */
s32 worldmap_objects_probe_solid(s32 probe, s16 *hit);

/* This unit's functions it calls or passes ahead of their definitions. */
void worldmap_stream_sort_disc_list(DiscReadRequest *list);
void worldmap_stream_sort_host_list(HostReadRequest *list);
void worldmap_stream_read_host_list(HostReadRequest *request);
s32 worldmap_stream_step_reader(void);
void worldmap_stream_start_disc_list(DiscReadRequest *request);
void worldmap_stream_on_command_done(s32 status, u8 *result);
void worldmap_stream_on_data_ready(s32 status, u8 *result);
void worldmap_terrain_queue_missing_blocks(void);
s16 worldmap_terrain_classify_quad(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *d); /* quad visibility */
void worldmap_terrain_build_and_draw_quarter(u32 *heights, u_long *ot, s32 packets, SVECTOR *origin);
void worldmap_terrain_draw_quarter_block(u32 *cells, u_long *ot, s32 packets); /* draw a terrain quarter block (assembly) */

/* World tables of the whole overlay (this unit's .data): area selection,
 * per area scene objects, path regions, the map dots, terrain visibility
 * and the horizon. */

/* The open map's areas by position (thresholds; area i-1 below entry i),
 * and the encounter level brackets. */
u16 worldmap_area_thresholds[10] = {0, 24, 54, 135, 149, 186, 198, 204, 237, 0xFFFF}; /* 8009B564 */
u16 worldmap_encounter_level_brackets[5] = {0, 54, 201, 340, 0xFFFF}; /* 8009B578 */

/* Area file sets: the nine open map areas, then one per scene mode (8-18). */
WorldmapArea worldmap_area_file_sets[20] = { /* 8009B584 */
    {43, 16, 16, 2}, {54, 16, 16, 2}, {65, 16, 16, 2}, {76, 16, 16, 2}, {87, 16, 16, 2},
    {98, 16, 16, 2}, {109, 16, 16, 2}, {120, 16, 16, 2}, {131, 16, 16, 2}, {43, 16, 16, 2},
    {142, 16, 16, 2}, {142, 16, 16, 2}, {43, 16, 16, 2}, {164, 16, 16, 2}, {186, 16, 16, 2},
    {153, 16, 16, 2}, {175, 16, 16, 2}, {197, 16, 16, 2}, {208, 16, 16, 2}, {219, 16, 16, 2},
};

/* Per area: the scene objects of the area's actors (a spinning pair, a
 * further pair and single objects). */
u16 worldmap_area_spinning_pair_objects[10][2] = { /* 8009B624 */
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {39, 42}, {39, 42}, {39, 42}, {39, 42}, {39, 42}, {0, 0},
};
u16 worldmap_area_rolling_pair_objects[10][2] = { /* 8009B64C */
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {44, 45}, {44, 45}, {44, 45}, {44, 45}, {44, 45}, {0, 0},
};
u16 worldmap_area_ferry_objects[10] = {0, 0, 0, 46, 51, 51, 51, 51, 0, 0}; /* 8009B674 */
u16 worldmap_area_airship_objects[10] = {0, 0, 0, 47, 52, 52, 52, 52, 0, 0}; /* 8009B688 */
u16 worldmap_area_raised_placement_objects[10] = {0, 0, 0, 0, 0, 0, 67, 79, 64, 0}; /* 8009B69C */
u16 worldmap_area_ground_placement_objects[10] = {0, 0, 0, 0, 43, 43, 43, 43, 43, 0}; /* 8009B6B0 */

/* Fixed path regions (scene, entry, path link): worldmap_select_actor_path_region selects the
 * first two for scenes 15 and 16, worldmap_run_frame_loop the third. */
PathRegion worldmap_fixed_path_regions[3] = { /* 8009B6C4 */
    {0, 0, 0, 0, 0x138, 1, 14, 0},
    {0, 0, 0, 0, 0x1B8, 1, 29, 0},
    {0, 0, 0, 0, 0x122, 3, -1, 0},
};

/* The map screen's 32 dots: x and z, interleaved (24-26 are placed from the
 * saved state, 27-31 unused). */
u16 worldmap_map_dot_positions[64] = { /* 8009B6F4 */
    84, 36,  68, 41,  55, 34,  58, 24,  62, 20,  54, 20,  78, 20,  77, 9,
    57, 17,  29, 81,  24, 68,  10, 71,  31, 57,  15, 58,  51, 48,  59, 89,
    92, 82,  17, 6,  29, 35,  8, 9,  88, 69,  90, 49,  94, 31,  95, 29,
    0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,  0xFFFF, 0,
};

/* Unreferenced: the border of the 5x5 terrain block grid. */
s16 worldmap_unused_grid_border[5 * 5] = { /* 8009B774 */
    1, 1, 1, 1, 1,
    1, 0, 0, 0, 1,
    1, 0, 0, 0, 1,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 1,
};

/* Per quadrant of the camera's block: the quarters of the 5x5 blocks that
 * are always visible (-1), four per block (worldmap_terrain_classify_blocks). */
s16 worldmap_terrain_always_visible_quarters[4][25][4] = { /* 8009B7A8 */
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

/* The table of worldmap_terrain_can_mode_enter_layer: per movement mode (row) and terrain class at
 * a position (column, worldmap_terrain_get_layer). */
s16 worldmap_terrain_passable_layers[8 * 8] = { /* 8009BAC8 */
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
u8 worldmap_background_color[3] = {0xC0, 0xD0, 0xF0}; /* 8009BB48 */

/* View edge vectors whose outer products give the four horizon plane
 * normals (worldmap_terrain_compute_cull_normals). */
VECTOR worldmap_terrain_cull_axis_y = {0, -1, 0}; /* 8009BB4C */
VECTOR worldmap_terrain_cull_axis_x = {1, 0, 0}; /* 8009BB5C */
VECTOR worldmap_terrain_cull_edge_0 = {-2170, 0, 3474}; /* 8009BB6C */
VECTOR worldmap_terrain_cull_edge_1 = {2170, 0, 3474}; /* 8009BB7C */
VECTOR worldmap_terrain_cull_edge_2 = {0, -2170, 3474}; /* 8009BB8C */
VECTOR worldmap_terrain_cull_edge_3 = {0, 2170, 0}; /* 8009BB9C */

/* Grid corner cells. */
s16 worldmap_terrain_grid_corner_cells[4] = {0, 8, 72, 80}; /* 8009BBAC */

/* Scratchpad work area of the cell-crossing probe: step[0] result,
 * step[1] target, step[2..4] corner test; cells crossed from and to. */
typedef struct {
    VECTOR step[5];
    u8 pad50[0x50];
    SVECTOR cell[2]; /* 0xA0 */
} CellProbe;

#define CELL_PROBE ((CellProbe *)0x1F800000)

/* Scratchpad matrices of the angle and camera helpers. */
#define SCRATCH_MATRIX_A ((MATRIX *)0x1F8000F0)
#define SCRATCH_MATRIX_B ((MATRIX *)0x1F800110)
#define SCRATCH_MATRIX_C ((MATRIX *)0x1F800130)
#define SCRATCH_MATRIX_D ((MATRIX *)0x1F800150)
#define SCRATCH_SVECTOR ((SVECTOR *)0x1F8000A0)
#define SCRATCH_VECTOR ((VECTOR *)0x1F800000)

/* 80094A5C: Move a position along a direction across the terrain cells: probe the
 * cell boundaries crossed (by the corner's side for diagonal moves); 1 when
 * the target cell is walkable (step[0] = target), else the boundary result.
 * The cell probes take the mode as an s16 row. */
s32 worldmap_terrain_probe_move(VECTOR *position, VECTOR *direction, s32 scale, s32 mode) {
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
        scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
        break;
    case 2:
        scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
        break;
    case 4:
        scale = worldmap_terrain_step_boundary_plus_z(position, direction, probe->step, row);
        break;
    case 8:
        scale = worldmap_terrain_step_boundary_neg_z(position, direction, probe->step, row);
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
        side = NormalClip(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side < 0) {
            scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_plus_z(position, direction, probe->step, row);
        } else if (side > 0) {
            scale = worldmap_terrain_step_boundary_plus_z(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
        } else {
            scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
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
        side = NormalClip(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side > 0) {
            scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_plus_z(position, direction, probe->step, row);
        } else if (side < 0) {
            scale = worldmap_terrain_step_boundary_plus_z(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
        } else {
            scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
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
        side = NormalClip(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side > 0) {
            scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_neg_z(position, direction, probe->step, row);
        } else if (side < 0) {
            scale = worldmap_terrain_step_boundary_neg_z(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
        } else {
            scale = worldmap_terrain_step_boundary_plus_x(position, direction, probe->step, row);
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
        side = NormalClip(probe->step[4].vx, probe->step[4].vy, probe->step[4].vz);
        if (side < 0) {
            scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_neg_z(position, direction, probe->step, row);
        } else if (side > 0) {
            scale = worldmap_terrain_step_boundary_neg_z(position, direction, probe->step, row);
            if (scale != 0) {
                return scale;
            }
            scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
        } else {
            scale = worldmap_terrain_step_boundary_neg_x(position, direction, probe->step, row);
        }
        break;
    }
    if (scale == 0) {
        worldmap_wrap_position(&probe->step[1]);
        from = worldmap_terrain_get_layer(&probe->step[1]);
        if (worldmap_terrain_can_mode_enter_layer(row, from) == 0) {
            probe->step[0] = probe->step[1];
            return 1;
        }
        return 0;
    }
    return scale;
}

/* 800951A8: Probe a move and choose the direction to slide along: 1 when free, 0 when
 * the obstacle deflects it (out holds the slide direction). */
/* Old-style definition: callers pass `mode` as an int, unconverted. */
s32 worldmap_terrain_probe_slide(position, direction, out, scale, mode)
    VECTOR *position;
    VECTOR *direction;
    VECTOR *out;
    s32 scale;
    s16 mode;
{
    s32 result;

    switch (worldmap_terrain_probe_move(position, direction, scale, mode)) {
    case 0:
        result = 1;
        break;
    case 1:
        if (worldmap_terrain_slide_on_slope(SCRATCH_VECTOR, direction, out) == 0) {
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

/* 800952B0: Orient `normal` towards `direction` on the ground plane (zero when
 * perpendicular). */
void worldmap_orient_to_direction(VECTOR *direction, VECTOR *out, VECTOR *normal) {
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

/* 80095324: Orient the horizontal tangent of a wall normal towards `direction` (zero
 * when perpendicular). */
void worldmap_orient_wall_tangent(VECTOR *normal, VECTOR *direction, VECTOR *out) {
    s32 tangent;
    s32 dot;

    SCRATCH_VECTOR[1].vz = 0;
    SCRATCH_VECTOR[1].vx = 0;
    SCRATCH_VECTOR[1].vy = -0x1000;
    OuterProduct12(normal, &SCRATCH_VECTOR[1], &SCRATCH_VECTOR[2]);
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

/* 80095414: Move a walking position over the solid scene objects. Off a structure, look
 * for a face under the probe at about the current height and step onto it;
 * on one, follow the move across the face edges (bit n of the edge test: the
 * move leaves through edge n) onto the neighbouring faces, sliding along an
 * edge whose neighbour is a wall (kind 1) and dropping back to the terrain
 * when an edge has no neighbour. Returns 1 when the position stands on a face. */
s32 worldmap_move_walking(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode) {
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
    worldmap_wrap_position(&scratch->probe);
    scratch->probe.vy = worldmap_terrain_get_height(scratch->probe.vx, scratch->probe.vz) - 0x4000;
    out->vx = position->vx + ((direction->vx * scale) >> 12);
    out->vz = position->vz + ((direction->vz * scale) >> 12);
    worldmap_wrap_position(out);
    out->vy = worldmap_terrain_get_height(out->vx, out->vz) - 0x4000;
    state = 1;
    if (worldmap_walker_object != -1) {
        state = 3;
    }
    switch (state) {
    case 0:
        result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
        worldmap_walker_face = -1;
        worldmap_walker_object = -1;
        break;
    case 1:
        count = worldmap_objects_probe_solid((s32)&scratch->probe, &object);
        cleared = 0;
        if (count != 0) {
            for (i = 0; i < count; i += 2) {
                if (worldmap_mesh_is_face_plane_crossed(&scratch->probe, 0x70, object, worldmap_mesh_probe_hits[i]) == 0) {
                    worldmap_mesh_probe_hits[i] = -1;
                    cleared += 2;
                }
            }
            if (cleared != count) {
                result = 0;
                for (i = 0; i < count; i += 2) {
                    if (worldmap_mesh_probe_hits[i] != -1 && worldmap_mesh_probe_hits[i + 1] != 1) {
                        worldmap_mesh_project_onto_face(&scratch->probe, &scratch->offset, &scratch->normal, object, worldmap_mesh_probe_hits[i]);
                        height = scratch->offset.vy - (position->vy >> 12);
                        if (height < 0) {
                            height = -height;
                        }
                        if (height < 0xB) {
                            out->vy = scratch->offset.vy << 12;
                            worldmap_walker_object = object;
                            worldmap_walker_face = worldmap_mesh_probe_hits[i];
                            result = 1;
                            break;
                        }
                    }
                }
                if (result == 0) {
                    worldmap_orient_wall_tangent(&scratch->normal, direction, out);
                }
                break;
            }
        }
        result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
        worldmap_walker_face = -1;
        worldmap_walker_object = -1;
        break;
    case 2:
    case 3:
        walking = 1;
        object = worldmap_walker_object;
        face = worldmap_walker_face;
        faces = ((Mesh *)worldmap_objects[object].unk44)->faces;
        do {
            i = worldmap_mesh_classify_face_exit(position, &scratch->probe, object, (s16)face);
            switch (i) {
            case 0:
                worldmap_mesh_project_onto_face(&scratch->probe, &scratch->offset, &scratch->normal, object, face);
                result = 1;
                walking = 0;
                worldmap_walker_face = (s16)face;
                out->vy = scratch->offset.vy << 12;
                worldmap_walker_object = object;
                break;
            case 1:
                if (faces[(s16)face].next[0] == -1) {
                    result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
                    worldmap_walker_face = -1;
                    worldmap_walker_object = -1;
                    walking = 0;
                    break;
                }
                face = faces[(s16)face].next[0];
                if (faces[(s16)face].kind == 1) {
                    result = 0;
                    walking = 0;
                    worldmap_orient_to_direction(direction, out, &scratch->side[0]);
                }
                break;
            case 2:
                if (faces[(s16)face].next[1] == -1) {
                    result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
                    worldmap_walker_face = -1;
                    worldmap_walker_object = -1;
                    walking = 0;
                    break;
                }
                face = faces[(s16)face].next[1];
                if (faces[(s16)face].kind == 1) {
                    result = 0;
                    walking = 0;
                    worldmap_orient_to_direction(direction, out, &scratch->side[1]);
                }
                break;
            case 4:
                if (faces[(s16)face].next[2] == -1) {
                    result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
                    worldmap_walker_face = -1;
                    worldmap_walker_object = -1;
                    walking = 0;
                    break;
                }
                face = faces[(s16)face].next[2];
                if (faces[(s16)face].kind == 1) {
                    result = 0;
                    walking = 0;
                    worldmap_orient_to_direction(direction, out, &scratch->side[2]);
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
                    result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
                    worldmap_walker_face = -1;
                    worldmap_walker_object = -1;
                    walking = 0;
                    break;
                case 1:
                    face = first;
                    if (faces[first].kind == edges) {
                        result = 0;
                        walking = 0;
                        worldmap_orient_to_direction(direction, out, &scratch->side[0]);
                    }
                    break;
                case 2:
                    face = second;
                    if (faces[second].kind == 1) {
                        result = 0;
                        walking = 0;
                        worldmap_orient_to_direction(direction, out, &scratch->side[1]);
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
                    result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
                    worldmap_walker_face = -1;
                    worldmap_walker_object = -1;
                    walking = 0;
                    break;
                case 1:
                    face = first;
                    if (faces[first].kind == edges) {
                        result = 0;
                        walking = 0;
                        worldmap_orient_to_direction(direction, out, &scratch->side[0]);
                    }
                    break;
                case 2:
                    face = second;
                    if (faces[second].kind == 1) {
                        result = 0;
                        walking = 0;
                        worldmap_orient_to_direction(direction, out, &scratch->side[2]);
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
                    result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
                    worldmap_walker_face = -1;
                    worldmap_walker_object = -1;
                    walking = 0;
                    break;
                case 1:
                    face = first;
                    if (faces[first].kind == edges) {
                        result = 0;
                        walking = 0;
                        worldmap_orient_to_direction(direction, out, &scratch->side[1]);
                    }
                    break;
                case 2:
                    face = second;
                    if (faces[second].kind == 1) {
                        result = 0;
                        walking = 0;
                        worldmap_orient_to_direction(direction, out, &scratch->side[2]);
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

/* 80095CD4: Move a flying position: clamp its height between the ground and the
 * ceiling, bounce back off solid objects, else slide along the terrain. */
s32 worldmap_move_flying(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode) {
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
    worldmap_wrap_position(&scratch->probe);
    out->vx = position->vx + ((direction->vx * scale) >> 12);
    out->vy = position->vy + ((direction->vy * scale) >> 12);
    out->vz = position->vz + ((direction->vz * scale) >> 12);
    worldmap_wrap_position(out);
    floor = worldmap_terrain_get_height(scratch->probe.vx, scratch->probe.vz) - 0x20000;
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
    count = worldmap_objects_probe_solid((s32)&scratch->probe, &hit);
    if (count != 0) {
        cleared = 0;
        for (i = 0; i < count; i += 2) {
            if (worldmap_mesh_is_face_plane_crossed(&scratch->probe, 0x70, hit, worldmap_mesh_probe_hits[i]) == 0) {
                worldmap_mesh_probe_hits[i] = -1;
                cleared += 2;
            }
        }
        result = 0;
        if (cleared != count) {
            out->vx = -direction->vx >> 1;
            out->vy = -direction->vy >> 1;
            out->vz = -direction->vz >> 1;
        } else {
            result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
        }
    } else {
        result = worldmap_terrain_probe_slide(position, direction, out, scale, mode);
    }
    return result;
}

/* 80095F78: Reset the stream queue and allocate its command buffers (disc or host). */
void worldmap_stream_reset(void) {
    s32 first;
    s32 second;
    s32 i;
    s8 *flag;

    first = cd_has_pc_file_server();
    second = cd_has_pc_file_server();
    if ((first == 0) | (second == -1)) {
        worldmap_stream_read_slot = 0;
        worldmap_stream_write_slot = 0;
        worldmap_stream_state = 0;
        for (i = 0xF; i >= 0; i--) {
            worldmap_stream_disc_lists[i] = NULL;
        }
        worldmap_stream_disc_buffer = heap_alloc(0x4200, 0);
        worldmap_stream_drain_buffer = heap_alloc(0x800, 0);
        worldmap_stream_request_count = 0;
        for (i = 7, flag = &worldmap_cd_sync_result[7]; i >= 0; i--) {
            *flag-- = 0;
        }
    } else {
        worldmap_stream_read_slot = 0;
        worldmap_stream_write_slot = 0;
        worldmap_stream_state = 0;
        for (i = 0xF; i >= 0; i--) {
            worldmap_stream_host_lists[i] = NULL;
        }
        worldmap_stream_host_buffer = heap_alloc(0x5800, 0);
        worldmap_stream_drain_buffer = heap_alloc(0x800, 0);
        worldmap_stream_request_count = 0;
        for (i = 7, flag = &worldmap_cd_sync_result[7]; i >= 0; i--) {
            *flag-- = 0;
        }
    }
}

/* 800960BC: Free the stream queue's request buffers. */
void worldmap_stream_free(void) {
    s32 first;
    s32 second;

    first = cd_has_pc_file_server();
    second = cd_has_pc_file_server();
    if ((first == 0) | (second == -1)) {
        heap_free(worldmap_stream_disc_buffer);
    } else {
        heap_free(worldmap_stream_host_buffer);
    }
    heap_free(worldmap_stream_drain_buffer);
}

/* 80096130: Wait until the current write slot of the stream queue is free. */
void worldmap_stream_wait_for_slot(void) {
    s32 first;
    s32 second;

    first = cd_has_pc_file_server();
    second = cd_has_pc_file_server();
    if ((first == 0) | (second == -1)) {
        while (worldmap_stream_disc_lists[worldmap_stream_write_slot] != NULL) {
            VSync(0);
            worldmap_stream_step();
        }
    } else {
        while (worldmap_stream_host_lists[worldmap_stream_write_slot] != NULL) {
            VSync(0);
            worldmap_stream_step();
        }
    }
}

/* 8009623C: Append a disc read request to the current frame's list. */
s32 worldmap_stream_add_disc_request(s32 sector, s32 bytes, u8 *destination) {
    DiscReadRequest *request;
    s32 count;

    count = worldmap_stream_request_count;
    if (count < 0x58) {
        worldmap_stream_request_count = count + 1;
        request = (DiscReadRequest *)((u8 *)worldmap_stream_disc_buffer + worldmap_stream_write_slot * 0x420) + count;
        request->sector = sector;
        request->bytes = bytes;
        request->destination = destination;
        return 0;
    }
    return -1;
}

/* 800962B0: Append a host-file read request to the current frame's list. */
s32 worldmap_stream_add_host_request(char *path, s32 offset, s32 bytes, u8 *destination) {
    HostReadRequest *request;
    s32 count;

    count = worldmap_stream_request_count;
    if (count < 0x58) {
        worldmap_stream_request_count = count + 1;
        request = (HostReadRequest *)((u8 *)worldmap_stream_host_buffer + worldmap_stream_write_slot * 0x580) + count;
        request->path = path;
        request->offset = offset;
        request->bytes = bytes;
        request->destination = destination;
        return 0;
    }
    return -1;
}

/* 80096328: Submit the current disc request list; -1 when there is nothing to
 * send or its ring slot is still busy. */
s32 worldmap_stream_submit_disc_list(void) {
    DiscReadRequest *list;

    list = (DiscReadRequest *)((u8 *)worldmap_stream_disc_buffer + worldmap_stream_write_slot * 0x420);
    if (list->sector != 0 && worldmap_stream_disc_lists[worldmap_stream_write_slot] == NULL) {
        worldmap_stream_sort_disc_list(list);
        worldmap_stream_request_count = 0;
        worldmap_stream_disc_lists[worldmap_stream_write_slot] = list;
        worldmap_stream_write_slot = (worldmap_stream_write_slot + 1) & 0xF;
        return 0;
    }
    worldmap_stream_request_count = 0;
    return -1;
}

/* 800963E4: Sort a disc request list by sector (insertion sort in place). */
void worldmap_stream_sort_disc_list(DiscReadRequest *list) {
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

/* 800964B0: Sort a host-file request list by offset (insertion sort in place). */
void worldmap_stream_sort_host_list(HostReadRequest *list) {
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

/* 800965A4: Submit the current host-file request list; -1 when there is nothing to
 * send or its ring slot is still busy. */
s32 worldmap_stream_submit_host_list(void) {
    HostReadRequest *list;

    list = (HostReadRequest *)((u8 *)worldmap_stream_host_buffer + worldmap_stream_write_slot * 0x580);
    if (list->path != NULL && worldmap_stream_host_lists[worldmap_stream_write_slot] == NULL) {
        worldmap_stream_sort_host_list(list);
        worldmap_stream_request_count = 0;
        worldmap_stream_host_lists[worldmap_stream_write_slot] = list;
        worldmap_stream_write_slot = (worldmap_stream_write_slot + 1) & 0xF;
        return 0;
    }
    worldmap_stream_request_count = 0;
    return -1;
}

/* 80096668: Frames queued between the writer and reader (ring of 16). */
s32 worldmap_stream_count_queued(void) {
    s32 pending;

    pending = worldmap_stream_write_slot - worldmap_stream_read_slot;
    if (pending < 0) {
        pending += 0x10;
    }
    return pending;
}

/* 80096694: Drain the queued frames, waiting for vertical sync between them. */
void worldmap_stream_drain(void) {
    do {
        VSync(0);
        worldmap_stream_step();
    } while (worldmap_stream_count_queued() != 0);
}

/* 800966CC: Read a host-file request list, retrying each call up to eight times. */
void worldmap_stream_read_host_list(HostReadRequest *request) {
    s32 fd;
    s32 i;

    worldmap_stream_unread_cleared_word_1 = 0;
    worldmap_stream_unread_cleared_word_2 = 0;
    worldmap_stream_status_error_count = 0;
    worldmap_stream_read_error_count = 0;
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
            if (PCread(fd, request->destination, request->bytes) != 0) {
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

/* 800967E4: Step the stream queue: from disc, advance the reader and start the next
 * queued list when idle; from the host, read the next list at once. */
s32 worldmap_stream_step(void) {
    s32 first;
    s32 second;
    s32 status;

    status = 0;
    first = cd_has_pc_file_server();
    second = cd_has_pc_file_server();
    if ((first == 0) | (second == -1)) {
        status = worldmap_stream_step_reader();
        if (status == 0 && worldmap_stream_disc_lists[worldmap_stream_read_slot] != NULL) {
            worldmap_stream_start_disc_list(worldmap_stream_disc_lists[worldmap_stream_read_slot]);
        }
    } else if (worldmap_stream_host_lists[worldmap_stream_read_slot] != NULL) {
        worldmap_stream_read_host_list(worldmap_stream_host_lists[worldmap_stream_read_slot]);
        worldmap_stream_host_lists[worldmap_stream_read_slot] = NULL;
        worldmap_stream_read_slot = (worldmap_stream_read_slot + 1) & 0xF;
    }
    return status;
}

/* 800968E0: Step the stream reader: 0 idle, 1 busy, 2 finished a frame, 3 error. */
s32 worldmap_stream_step_reader(void) {
    switch (worldmap_stream_state) {
    case 0:
        return 0;
    case 4:
        if (--worldmap_stream_wait == 0) {
            worldmap_stream_state++;
        }
    case 1:
    case 2:
    case 3:
        return 1;
    case 5:
        worldmap_stream_state = 0;
        worldmap_stream_disc_lists[worldmap_stream_read_slot] = NULL;
        worldmap_stream_read_slot = (worldmap_stream_read_slot + 1) & 0xF;
        return 2;
    default:
        return 3;
    }
}

/* 8009699C: Start reading a disc request list: seek to its first sector. */
void worldmap_stream_start_disc_list(DiscReadRequest *request) {
    s32 sector;

    sector = request->sector;
    worldmap_stream_state = 1;
    worldmap_stream_next_request = request;
    worldmap_stream_next_request = request + 1;
    worldmap_stream_unread_cleared_word_1 = 0;
    worldmap_stream_unread_cleared_word_2 = 0;
    worldmap_stream_status_error_count = 0;
    worldmap_stream_read_error_count = 0;
    worldmap_stream_drive_sector = sector;
    worldmap_stream_wanted_sector = sector;
    worldmap_stream_sectors_left = (u32)(request->bytes + 0x7FF) >> 11;
    worldmap_stream_bytes_left = request->bytes;
    worldmap_stream_destination = request->destination;
    CdIntToPos(sector, &worldmap_stream_seek_position);
    CdSyncCallback((CdlCB)worldmap_stream_on_command_done);
    CdControlF(CdlSetloc, (u8 *)&worldmap_stream_seek_position);
}

/* 80096A6C: CD command-complete callback of the stream reader: after the seek start
 * reading, and recover from errors by pausing and seeking again. */
void worldmap_stream_on_command_done(s32 status, u8 *result) {
    if (status == 2) {
        switch (worldmap_stream_state) {
        case 1:
            worldmap_stream_state = 2;
            worldmap_stream_sector_header[2] = 0;
            worldmap_stream_sector_header[1] = 0;
            worldmap_stream_sector_header[0] = 0;
            CdReadyCallback((CdlCB)worldmap_stream_on_data_ready);
            CdControlF(0x1B, NULL);
            break;
        case 3:
            if (worldmap_stream_wanted_sector == 0) {
                worldmap_stream_state = 4;
                worldmap_stream_wait = 1;
                CdSyncCallback(NULL);
            }
            break;
        case 10:
            if (result[0] & 0x10) {
                CdControlF(1, NULL);
            } else {
                CdControlF(0x13, NULL);
                worldmap_stream_state = 0xB;
            }
            break;
        case 11:
            worldmap_stream_state = 0xC;
            CdControlF(CdlPause, NULL);
            break;
        case 12:
            worldmap_stream_state = 1;
            CdIntToPos(worldmap_stream_drive_sector, &worldmap_stream_seek_position);
            CdControlF(CdlSetloc, (u8 *)&worldmap_stream_seek_position);
            break;
        }
    } else if (result[0] & 0x10) {
        worldmap_stream_state = 0xA;
        worldmap_stream_status_error_count++;
        CdControlF(1, NULL);
    } else {
        worldmap_stream_state = 0xB;
        CdControlF(0x13, NULL);
    }
}

/* 80096C0C: CD data-ready callback of the stream reader: copy the sector to the
 * request's destination and continue with the next request, seeking when it
 * is not close ahead; pause at the end of the list. */
void worldmap_stream_on_data_ready(s32 status, u8 *result) {
    DiscReadRequest *request;
    s32 sector;
    s32 next;

    if (status == 1) {
        CdGetSector(worldmap_stream_sector_header, 3);
        sector = CdPosToInt((CdlLOC *)worldmap_stream_sector_header);
        if (sector == worldmap_stream_drive_sector) {
            if (worldmap_stream_wanted_sector == sector) {
                if (worldmap_stream_bytes_left < 0x800) {
                    CdGetSector(worldmap_stream_destination, worldmap_stream_bytes_left / 4);
                    CdGetSector(worldmap_stream_drain_buffer, (0x800 - worldmap_stream_bytes_left) / 4);
                } else {
                    CdGetSector(worldmap_stream_destination, 0x200);
                    worldmap_stream_bytes_left -= 0x800;
                }
                if (--worldmap_stream_sectors_left != 0) {
                    worldmap_stream_wanted_sector++;
                    worldmap_stream_destination += 0x800;
                } else {
                    request = worldmap_stream_next_request++;
                    next = request->sector;
                    worldmap_stream_wanted_sector = next;
                    worldmap_stream_sectors_left = (u32)(request->bytes + 0x7FF) >> 11;
                    worldmap_stream_bytes_left = request->bytes;
                    worldmap_stream_destination = request->destination;
                    if (next != 0) {
                        if (next - worldmap_stream_drive_sector >= 0x13) {
                            worldmap_stream_drive_sector = next;
                            worldmap_stream_state = 1;
                            CdIntToPos(next, &worldmap_stream_seek_position);
                            CdControlF(CdlSetloc, (u8 *)&worldmap_stream_seek_position);
                            return;
                        }
                    } else {
                        worldmap_stream_state = 3;
                        CdReadyCallback(NULL);
                        CdControlF(CdlPause, NULL);
                    }
                }
            }
            worldmap_stream_drive_sector++;
            return;
        }
        worldmap_stream_read_error_count++;
        CdReadyCallback(NULL);
        if (result[0] & 0x10) {
            worldmap_stream_state = 0xA;
            worldmap_stream_status_error_count++;
            CdControlF(1, NULL);
        } else {
            worldmap_stream_state = 0xB;
            CdControlF(0x13, NULL);
        }
    } else {
        worldmap_stream_read_error_count++;
        CdReadyCallback(NULL);
        if (result[0] & 0x10) {
            worldmap_stream_state = 0xA;
            worldmap_stream_status_error_count++;
            CdControlF(1, NULL);
        } else {
            worldmap_stream_state = 0xB;
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

/* 80096F18: Place a camera orbiting above a position: look at its height from
 * `distance` along the angle, with the up direction rolled by the angle. */
void worldmap_camera_place_orbit(ViewSetup *view, Camera *camera, s32 distance, SVECTOR *angle) {
    SVECTOR *rotation;

    view->at.vx = 0;
    view->at.vy = camera->target.vy >> 12;
    view->at.vz = 0;
    ORBIT_SCRATCH->angle.vx = angle->vx;
    rotation = &ORBIT_SCRATCH->angle;
    ORBIT_SCRATCH->angle.vy = angle->vy;
    ORBIT_SCRATCH->angle.vz = 0;
    RotMatrixYXZ(&ORBIT_SCRATCH->angle, &ORBIT_SCRATCH->rotation);
    ORBIT_SCRATCH->offset.vx = 0;
    ORBIT_SCRATCH->offset.vy = 0;
    ORBIT_SCRATCH->offset.vz = -(distance >> 12);
    ApplyMatrixLV(&ORBIT_SCRATCH->rotation, &ORBIT_SCRATCH->offset, &ORBIT_SCRATCH->eye);
    view->eye.vx = ORBIT_SCRATCH->eye.vx;
    view->eye.vy = view->at.vy + ORBIT_SCRATCH->eye.vy;
    view->eye.vz = ORBIT_SCRATCH->eye.vz;
    rotation->vx = 0;
    rotation->vy = angle->vy;
    rotation->vz = angle->vz;
    RotMatrixYXZ(rotation, &ORBIT_SCRATCH->rotation);
    rotation->vx = 0;
    rotation->vy = -0x1000;
    rotation->vz = 0;
    ApplyMatrix(&ORBIT_SCRATCH->rotation, rotation, &view->up);
}

/* 80097070: Recover rotation angles (yaw, then pitch, then roll) from a matrix. */
void worldmap_get_matrix_angles(MATRIX *m, SVECTOR *angle) {
    if (m->m[2][0] | m->m[2][2]) {
        angle->vy = ratan2(m->m[2][0], m->m[2][2]) & 0xFFF;
        *SCRATCH_MATRIX_A = *m;
        *SCRATCH_MATRIX_B = worldmap_identity_matrix;
        RotMatrixY(angle->vy, SCRATCH_MATRIX_B);
        MulMatrix0(SCRATCH_MATRIX_A, SCRATCH_MATRIX_B, SCRATCH_MATRIX_C);
        angle->vx = ratan2(SCRATCH_MATRIX_C->m[1][2], SCRATCH_MATRIX_C->m[1][1]);
        *SCRATCH_MATRIX_B = worldmap_identity_matrix;
        RotMatrixX(angle->vx, SCRATCH_MATRIX_B);
        MulMatrix0(SCRATCH_MATRIX_C, SCRATCH_MATRIX_B, SCRATCH_MATRIX_A);
        angle->vz = -ratan2(SCRATCH_MATRIX_A->m[1][0], SCRATCH_MATRIX_A->m[1][1]);
    }
}

/* Scratchpad work area of the look-at camera. */
typedef struct {
    VECTOR work;
    VECTOR right;
    VECTOR up;
    VECTOR forward;
    SVECTOR eye;
    MATRIX view;
} LookAtScratch;

#define LOOKAT_SCRATCH ((LookAtScratch *)0x1F800000)

/* 80097244: Build the camera matrix looking from the eye to the target. */
void worldmap_camera_build_look_at(void *arg) {
    ViewSetup *view;

    view = arg;
    LOOKAT_SCRATCH->work.vx = -view->eye.vx + view->at.vx;
    LOOKAT_SCRATCH->work.vy = -view->eye.vy + view->at.vy;
    LOOKAT_SCRATCH->work.vz = -view->eye.vz + view->at.vz;
    VectorNormal(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->forward);
    OuterProduct12(&LOOKAT_SCRATCH->forward, &view->up, &LOOKAT_SCRATCH->work);
    VectorNormal(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->right);
    OuterProduct12(&LOOKAT_SCRATCH->forward, &LOOKAT_SCRATCH->right, &LOOKAT_SCRATCH->work);
    VectorNormal(&LOOKAT_SCRATCH->work, &LOOKAT_SCRATCH->up);
    worldmap_camera_matrix.m[0][0] = LOOKAT_SCRATCH->right.vx;
    worldmap_camera_matrix.m[0][1] = LOOKAT_SCRATCH->right.vy;
    worldmap_camera_matrix.m[0][2] = LOOKAT_SCRATCH->right.vz;
    worldmap_camera_matrix.m[1][0] = LOOKAT_SCRATCH->up.vx;
    worldmap_camera_matrix.m[1][1] = LOOKAT_SCRATCH->up.vy;
    worldmap_camera_matrix.m[1][2] = LOOKAT_SCRATCH->up.vz;
    worldmap_camera_matrix.m[2][0] = LOOKAT_SCRATCH->forward.vx;
    worldmap_camera_matrix.m[2][1] = LOOKAT_SCRATCH->forward.vy;
    worldmap_camera_matrix.m[2][2] = LOOKAT_SCRATCH->forward.vz;
    LOOKAT_SCRATCH->eye.vx = -view->eye.vx;
    LOOKAT_SCRATCH->eye.vy = -view->eye.vy;
    LOOKAT_SCRATCH->eye.vz = -view->eye.vz;
    LOOKAT_SCRATCH->view = worldmap_camera_matrix;
    ApplyMatrix(&LOOKAT_SCRATCH->view, &LOOKAT_SCRATCH->eye, &LOOKAT_SCRATCH->work);
    TransMatrix(&worldmap_camera_matrix, &LOOKAT_SCRATCH->work);
}

/* 80097440: Build the camera matrix from the camera angle and eye position. */
void worldmap_camera_build_from_angles(void *arg) {
    SVECTOR *eye;

    eye = arg;
    *SCRATCH_MATRIX_A = worldmap_identity_matrix;
    *SCRATCH_MATRIX_B = *SCRATCH_MATRIX_A;
    *SCRATCH_MATRIX_C = *SCRATCH_MATRIX_A;
    RotMatrixX(-worldmap_camera_angle.vx, SCRATCH_MATRIX_A);
    RotMatrixY(-worldmap_camera_angle.vy, SCRATCH_MATRIX_B);
    RotMatrixZ(-worldmap_camera_angle.vz, SCRATCH_MATRIX_C);
    MulMatrix0(SCRATCH_MATRIX_A, SCRATCH_MATRIX_B, SCRATCH_MATRIX_D);
    MulMatrix0(SCRATCH_MATRIX_C, SCRATCH_MATRIX_D, &worldmap_camera_matrix);
    SCRATCH_SVECTOR->vx = -eye->vx;
    SCRATCH_SVECTOR->vy = -eye->vy;
    SCRATCH_SVECTOR->vz = -eye->vz;
    *SCRATCH_MATRIX_A = worldmap_camera_matrix;
    ApplyMatrix(SCRATCH_MATRIX_A, SCRATCH_SVECTOR, SCRATCH_VECTOR);
    TransMatrix(&worldmap_camera_matrix, SCRATCH_VECTOR);
}

/* 8009766C: Allocate and clear the 64 actor slots. */
void worldmap_actor_alloc_slots(void) {
    worldmap_actor_slots = heap_alloc(0x2000, 0);
    worldmap_actor_clear_slots();
}

/* 800976A0: Free the actor slots. */
void worldmap_actor_free_slots(void) {
    heap_free(worldmap_actor_slots);
}

/* 800976C8: Mark every actor slot free. */
void worldmap_actor_clear_slots(void) {
    WorldmapActor *actor;
    s32 i;

    for (i = 0; i < 0x40; i++) {
        actor = &worldmap_actor_slots[i];
        actor->handle = NULL;
        actor->kind = 0;
        actor->update = 0;
    }
}

/* 800976FC: Change an actor's kind and clear its command. */
void worldmap_actor_set_kind(s32 kind, s32 index) {
    worldmap_actor_slots[index].command = 0;
    worldmap_actor_slots[index].kind = kind;
}

/* 80097718: Start an actor in the first free slot. */
void worldmap_actor_spawn(s32 kind, s32 update) {
    WorldmapActor *actor;
    s32 i;

    for (i = 0; i < 0x40; i++) {
        actor = &worldmap_actor_slots[i];
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

/* 80097770: Send command 1 with an argument unless one is pending; 1 when sent. */
s32 worldmap_actor_request(s32 index, s32 arg) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    if (actor->unk4 == 0) {
        actor->command = 1;
        actor->unk4 = arg;
        return 1;
    }
    return 0;
}

/* 800977A8: Send command 3. */
void worldmap_actor_send_idle(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->command = 3;
}

/* 800977C4: Send command 4. */
void worldmap_actor_send_release(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->command = 4;
}

/* 800977E0: Send command 2 with an argument. */
void worldmap_actor_send_wait(s32 index, s16 arg) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->command = 2;
    actor->command_arg = arg;
}

/* 80097800: Run every active actor's pending command: 0 start, 1 step, 2 wait, 3 idle,
 * 4 release its handle. */
void worldmap_actor_run_all(void) {
    WorldmapActor *actor;
    s32 i;

    actor = worldmap_actor_slots;
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
                if (actor->handle != NULL) {
                    sprite_destroy(actor->handle);
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

/* 800978FC: Allocate both 2048-triangle terrain packet buffers and initialise them. */
void worldmap_terrain_alloc_packets(void) {
    POLY_FT3 *prim;
    s32 i;

    worldmap_display_buffers[0].packets = heap_alloc(0x10000, 1);
    worldmap_display_buffers[1].packets = heap_alloc(0x10000, 1);
    prim = worldmap_display_buffers[0].packets;
    for (i = 0; i < 0x800; i++, prim++) {
        setlen(prim, 7);
        setcode(prim, 0x24);
        setRGB0(prim, 0x80, 0x80, 0x80);
    }
    *(TriangleBuffer *)worldmap_display_buffers[1].packets = *(TriangleBuffer *)worldmap_display_buffers[0].packets;
}

/* 800979C8: Upload the terrain texture image (its buffer is then reused for the
 * palettes), build the faded terrain palettes and their CLUT and texture
 * page ids. */
void worldmap_terrain_upload_image(void) {
    RECT rect;
    u16 *cluts;
    u16 *faded;
    s32 i;
    s32 x;
    s32 y;

    cluts = text_unpack_lzss_alloc(worldmap_terrain_image, 1);
    model_load_tim_list((u32 *)cluts);
    DrawSync(0);
    heap_free(cluts);
    heap_free(worldmap_terrain_image);
    cluts = heap_alloc(0x400, 1);
    faded = heap_alloc(0x8000, 1);
    rect.x = 0;
    rect.y = 0x1E0;
    rect.w = 0x100;
    rect.h = 2;
    StoreImage(&rect, (u_long *)cluts);
    DrawSync(0);
    worldmap_build_palette_fades(cluts, faded, 0x20, worldmap_background_color);
    worldmap_build_palette_fades(cluts + 0x100, faded + 0x2000, 0x20, worldmap_background_color);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x100;
    rect.h = 0x40;
    LoadImage(&rect, (u_long *)faded);
    DrawSync(0);
    for (i = 0; i < 0x40; i++) {
        worldmap_terrain_cluts[i] = GetClut(rect.x, rect.y);
        rect.y++;
    }
    for (x = 0x200, y = 0, i = 0; i < 4; i++) {
        worldmap_terrain_tpages[i] = GetTPage(1, 0, x, y);
        x += 0x80;
    }
    for (x = 0x180, y = 0x100, i = 4; i < 7; i++) {
        worldmap_terrain_tpages[i] = GetTPage(1, 0, x, y);
        x += 0x80;
    }
    heap_free(faded);
    heap_free(cluts);
}

/* 80097BC0: Reset the terrain loader around a position. */
void worldmap_terrain_reset_at(VECTOR *position) {
    s32 i;

    worldmap_terrain_matrix = worldmap_identity_matrix;
    for (i = 0xFF; i >= 0; i--) {
        worldmap_terrain_blocks[i] = NULL;
    }
    TERRAIN_ORIGIN.vx = position->vx & 0x7FFFFF;
    TERRAIN_ORIGIN.vy = 0;
    worldmap_wave_phase_x = 0;
    worldmap_wave_phase_z = 0x400;
    TERRAIN_ORIGIN.vz = position->vz & 0x7FFFFF;
    worldmap_camera_block_cell.vx = 2;
    worldmap_camera_block_cell.vy = 0;
    worldmap_camera_block_cell.vz = 2;
    worldmap_terrain_update_grid((Camera *)position);
    worldmap_terrain_queue_missing_blocks();
}

/* 80097CB8: Reset the terrain loader around the camera. */
void worldmap_terrain_reset_at_camera(Camera *camera) {
    s32 i;

    worldmap_terrain_matrix = worldmap_identity_matrix;
    for (i = 0xFF; i >= 0; i--) {
        worldmap_terrain_blocks[i] = NULL;
    }
    worldmap_wave_phase_x = 0;
    worldmap_wave_phase_z = 0x400;
    worldmap_terrain_update_grid(camera);
    worldmap_terrain_queue_missing_blocks();
}

/* 80097D64: Free every loaded terrain block. */
void worldmap_terrain_free_blocks(void) {
    s32 i;

    for (i = 0; i < 0x100; i++) {
        if (worldmap_terrain_blocks[i] != NULL) {
            heap_free(worldmap_terrain_blocks[i]);
        }
    }
}

/* 80097DC0: Queue reads of the terrain blocks around the camera that are not loaded:
 * from disc the centre 3x3 first, then the whole 9x9 grid. */
void worldmap_terrain_queue_missing_blocks(void) {
    s32 first;
    s32 second;
    s32 row;
    s32 column;
    s32 block;
    void *buffer;
    s32 sector;
    char *path;

    first = cd_has_pc_file_server();
    second = cd_has_pc_file_server();
    if ((first == 0) | (second == -1)) {
        sector = cd_get_file_sector(worldmap_terrain_row_file);
        for (row = 3; row < 6; row++) {
            for (column = 3; column < 6; column++) {
                block = worldmap_terrain_grid.cells[row * 9 + column];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_disc_request(sector + block, 0x710, buffer);
                }
            }
        }
        worldmap_stream_add_disc_request(0, 0, NULL);
        worldmap_stream_submit_disc_list();
        for (row = 0; row < 9; row++) {
            for (column = 0; column < 9; column++) {
                block = worldmap_terrain_grid.cells[row * 9 + column];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_disc_request(sector + block, 0x710, buffer);
                }
            }
        }
        worldmap_stream_add_disc_request(0, 0, NULL);
        worldmap_stream_submit_disc_list();
    } else {
        path = cd_get_pc_file_name(worldmap_terrain_row_file);
        for (row = 0; row < 9; row++) {
            for (column = 0; column < 9; column++) {
                block = worldmap_terrain_grid.cells[row * 9 + column];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_host_request(path, block << 11, 0x710, buffer);
                }
            }
        }
        worldmap_stream_add_host_request(NULL, 0, 0, NULL);
        worldmap_stream_submit_host_list();
    }
}

/* 80098044: Compute the four horizon plane normals. */
void worldmap_terrain_compute_cull_normals(void) {
    OuterProduct0(&worldmap_terrain_cull_edge_0, &worldmap_terrain_cull_axis_y, &worldmap_terrain_cull_normal_0);
    OuterProduct0(&worldmap_terrain_cull_axis_y, &worldmap_terrain_cull_edge_1, &worldmap_terrain_cull_normal_1);
    OuterProduct0(&worldmap_terrain_cull_edge_2, &worldmap_terrain_cull_axis_x, &worldmap_terrain_cull_normal_2);
    OuterProduct0(&worldmap_terrain_cull_axis_x, &worldmap_terrain_cull_edge_3, &worldmap_terrain_cull_normal_3);
}

/* 800980D4: Wrap a position into the map and note the crossed edges (8/4 in x,
 * 2/1 in z); update the camera's block cell. */
void worldmap_terrain_wrap_origin(void *arg) {
    VECTOR *position;
    s32 x;
    s32 z;

    position = arg;
    x = position->vx;
    z = position->vz;
    worldmap_terrain_crossed_edges = 0;
    if (x < -0x800000) {
        position->vx = x + 0x800000;
        worldmap_terrain_crossed_edges = 4;
    } else if (x > 0x800000) {
        position->vx = x - 0x800000;
        worldmap_terrain_crossed_edges = 8;
    }
    if (z < -0x800000) {
        position->vz += 0x800000;
        worldmap_terrain_crossed_edges |= 1;
    } else if (z > 0x800000) {
        position->vz -= 0x800000;
        worldmap_terrain_crossed_edges |= 2;
    }
    worldmap_camera_block_cell.vx = (position->vx >> 23) + 2;
    worldmap_camera_block_cell.vz = (position->vz >> 23) + 2;
}

/* 800981C8: Recompute the 9x9 grid of terrain blocks around the camera (keeping the
 * previous grid), wrapping around the map edges. */
void worldmap_terrain_update_grid(Camera *camera) {
    s32 width;
    s32 height;
    s32 x;
    s32 z;
    s32 left;
    s32 base;
    s32 i;
    s32 j;
    s16 *cell;

    width = worldmap_area_blocks_x;
    height = worldmap_area_blocks_z;
    x = (camera->target.vx >> 12) / 8 - ((worldmap_camera_block_cell.vx + 2) << 8);
    z = (camera->target.vz >> 12) / 8 - ((worldmap_camera_block_cell.vz + 2) << 8);
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
    worldmap_terrain_previous_grid = worldmap_terrain_grid;
    left = x;
    cell = worldmap_terrain_grid.cells;
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

/* Scratchpad work area of the terrain visibility test. */
typedef struct {
    VECTOR view[4];  /* 0x00: the tested quad's corners after RT */
    s32 x0;          /* 0x40: grid corner x */
    s32 pad44;
    s32 z0;          /* 0x48: grid corner z */
    u8 pad4C[0x54];
    SVECTOR v[9];    /* 0xA0: cell corners and midpoints */
    u8 padE8[8];
    MATRIX local;    /* 0xF0 */
    MATRIX world;    /* 0x110 */
} GridScratch;

#define GRID_SCRATCH ((GridScratch *)0x1F800000)

/* 800983A0: Classify the 5x5 terrain blocks around the camera: test each block's
 * quad for visibility, and its four quarters when partly visible; blocks
 * near the camera are always visible. The block pointer, row and column
 * are reused for the always-visible pass, as the original does. */
void worldmap_terrain_classify_blocks(Camera *camera) {
    s16 *block;
    u32 *quarters;
    u32 visible;
    s32 row;
    s32 column;
    s16 result;
    GridScratch *scratch;

    scratch = GRID_SCRATCH;
    GRID_SCRATCH->local = worldmap_terrain_matrix;
    CompMatrix(&worldmap_camera_matrix, &GRID_SCRATCH->local, &GRID_SCRATCH->world);
    SetRotMatrix(&GRID_SCRATCH->world);
    SetTransMatrix(&GRID_SCRATCH->world);
    block = worldmap_terrain_visible_blocks;
    quarters = (u32 *)worldmap_terrain_quarter_visibility[0];
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
            result = worldmap_terrain_classify_quad(&scratch->v[0], &scratch->v[1], &scratch->v[2],
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
                visible = worldmap_terrain_classify_quad(&scratch->v[0], &scratch->v[4], &scratch->v[5],
                                        &scratch->v[8]) & 0xFFFF;
                visible |= worldmap_terrain_classify_quad(&scratch->v[4], &scratch->v[1], &scratch->v[8],
                                         &scratch->v[6]) << 16;
                quarters[0] = visible;
                visible = worldmap_terrain_classify_quad(&scratch->v[5], &scratch->v[8], &scratch->v[2],
                                        &scratch->v[7]) & 0xFFFF;
                visible |= worldmap_terrain_classify_quad(&scratch->v[8], &scratch->v[6], &scratch->v[7],
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
    block = (s16 *)worldmap_terrain_always_visible_quarters[column][0];
    quarters = (u32 *)worldmap_terrain_quarter_visibility[0];
    for (row = 0; row < 25; row++) {
        quarters[0] |= ((u32 *)block)[0];
        quarters[1] |= ((u32 *)block)[1];
        block += 4;
        quarters += 2;
    }
}

/* 800987AC: Classify the quad a, b, c, d (two rows of two corners, so its edges are
 * a-b, b-d, d-c and c-a) against the four horizon planes of 80098044,
 * after RT with the loaded matrix: -1 when all four corners lie outside one
 * plane, 1 when no plane has a whole edge outside it, else 0. Each plane
 * counts the edges whose two corners are both on its negative side; the
 * first two planes use x and z, the other two y and z. */
s16 worldmap_terrain_classify_quad(SVECTOR *a, SVECTOR *b, SVECTOR *c, SVECTOR *d) {
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
    d0 = (view[0].vx * worldmap_terrain_cull_normal_0.vx + view[0].vz * worldmap_terrain_cull_normal_0.vz) >> 12;
    d1 = (view[1].vx * worldmap_terrain_cull_normal_0.vx + view[1].vz * worldmap_terrain_cull_normal_0.vz) >> 12;
    d2 = (view[2].vx * worldmap_terrain_cull_normal_0.vx + view[2].vz * worldmap_terrain_cull_normal_0.vz) >> 12;
    d3 = (view[3].vx * worldmap_terrain_cull_normal_0.vx + view[3].vz * worldmap_terrain_cull_normal_0.vz) >> 12;
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
    d0 = (view[0].vx * worldmap_terrain_cull_normal_1.vx + view[0].vz * worldmap_terrain_cull_normal_1.vz) >> 12;
    d1 = (view[1].vx * worldmap_terrain_cull_normal_1.vx + view[1].vz * worldmap_terrain_cull_normal_1.vz) >> 12;
    d2 = (view[2].vx * worldmap_terrain_cull_normal_1.vx + view[2].vz * worldmap_terrain_cull_normal_1.vz) >> 12;
    d3 = (view[3].vx * worldmap_terrain_cull_normal_1.vx + view[3].vz * worldmap_terrain_cull_normal_1.vz) >> 12;
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
    d0 = (view[0].vy * worldmap_terrain_cull_normal_2.vy + view[0].vz * worldmap_terrain_cull_normal_2.vz) >> 12;
    d1 = (view[1].vy * worldmap_terrain_cull_normal_2.vy + view[1].vz * worldmap_terrain_cull_normal_2.vz) >> 12;
    d2 = (view[2].vy * worldmap_terrain_cull_normal_2.vy + view[2].vz * worldmap_terrain_cull_normal_2.vz) >> 12;
    d3 = (view[3].vy * worldmap_terrain_cull_normal_2.vy + view[3].vz * worldmap_terrain_cull_normal_2.vz) >> 12;
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
    d0 = (view[0].vy * worldmap_terrain_cull_normal_3.vy + view[0].vz * worldmap_terrain_cull_normal_3.vz) >> 12;
    d1 = (view[1].vy * worldmap_terrain_cull_normal_3.vy + view[1].vz * worldmap_terrain_cull_normal_3.vz) >> 12;
    d2 = (view[2].vy * worldmap_terrain_cull_normal_3.vy + view[2].vz * worldmap_terrain_cull_normal_3.vz) >> 12;
    d3 = (view[3].vy * worldmap_terrain_cull_normal_3.vy + view[3].vz * worldmap_terrain_cull_normal_3.vz) >> 12;
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

/* 80098CC0: After the grid moved: free the blocks that left it and queue reads of the
 * new edge blocks, rows from the row-major file and columns from the
 * column-major file; corners from whichever edge changed. n first holds the
 * read-from-disc test and then each edge's first cell; the pass counter j
 * also holds each corner's block number, and a corner reloads the source
 * into sector/path. Each branch keeps its own edge flags. */
void worldmap_terrain_load_new_edges(void) {
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
        block = worldmap_terrain_previous_grid.cells[i];
        if (worldmap_terrain_blocks[block] != NULL) {
            for (j = 0; j < 81; j++) {
                if (worldmap_terrain_grid.cells[j] == block) {
                    break;
                }
            }
            if (j == 81) {
                heap_free(worldmap_terrain_blocks[block]);
                worldmap_terrain_blocks[block] = NULL;
            }
        }
    }
    changed = 0;
    first = cd_has_pc_file_server();
    second = cd_has_pc_file_server();
    n = first == 0;
    n |= second == -1;
    if (n) {
        s32 rows;
        s32 cols;

        sector = cd_get_file_sector(worldmap_terrain_row_file);
        n = 1;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n++) {
                block = worldmap_terrain_grid.cells[n];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_disc_request(sector + block, 0x710, buffer);
                    changed |= 2;
                }
            }
            n = 0x49;
        }
        sector = cd_get_file_sector(worldmap_terrain_column_file);
        n = 9;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n += 9) {
                block = worldmap_terrain_grid.cells[n];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_disc_request(sector + ((block % worldmap_area_blocks_x) * worldmap_area_blocks_z + block / worldmap_area_blocks_x),
                                  0x710, buffer);
                    changed |= 1;
                }
            }
            n = 0x11;
        }
        for (k = 0; k < 4; k++) {
            j = worldmap_terrain_grid.cells[worldmap_terrain_grid_corner_cells[k]];
            rows = changed & 2;
            cols = changed & 1;
            if (worldmap_terrain_blocks[j] == NULL) {
                worldmap_terrain_blocks[j] = heap_alloc(0x710, 0);
                if (rows) {
                    sector = cd_get_file_sector(worldmap_terrain_row_file);
                    worldmap_stream_add_disc_request(sector + j, 0x710, worldmap_terrain_blocks[j]);
                } else if (cols) {
                    sector = cd_get_file_sector(worldmap_terrain_column_file);
                    worldmap_stream_add_disc_request(sector + ((j % worldmap_area_blocks_x) * worldmap_area_blocks_z + j / worldmap_area_blocks_x), 0x710,
                                  worldmap_terrain_blocks[j]);
                }
            }
        }
        worldmap_stream_add_disc_request(0, 0, NULL);
        worldmap_stream_submit_disc_list();
    } else {
        s32 rows;
        s32 cols;

        path = cd_get_pc_file_name(worldmap_terrain_row_file);
        n = 1;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n++) {
                block = worldmap_terrain_grid.cells[n];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_host_request(path, block << 11, 0x710, buffer);
                    changed |= 2;
                }
            }
            n = 0x49;
        }
        path = cd_get_pc_file_name(worldmap_terrain_column_file);
        n = 9;
        for (j = 1; j != -1; j--) {
            for (k = 6; k != -1; k--, n += 9) {
                block = worldmap_terrain_grid.cells[n];
                if (worldmap_terrain_blocks[block] == NULL) {
                    buffer = heap_alloc(0x710, 0);
                    worldmap_terrain_blocks[block] = buffer;
                    worldmap_stream_add_host_request(path,
                                  ((block % worldmap_area_blocks_x) << 11) * worldmap_area_blocks_z + ((block / worldmap_area_blocks_x) << 11),
                                  0x710, buffer);
                    changed |= 1;
                }
            }
            n = 0x11;
        }
        for (k = 0; k < 4; k++) {
            rows = changed & 2;
            cols = changed & 1;
            j = worldmap_terrain_grid.cells[worldmap_terrain_grid_corner_cells[k]];
            if (worldmap_terrain_blocks[j] == NULL) {
                worldmap_terrain_blocks[j] = heap_alloc(0x710, 0);
                if (rows) {
                    path = cd_get_pc_file_name(worldmap_terrain_row_file);
                    worldmap_stream_add_host_request(path, j << 11, 0x710, worldmap_terrain_blocks[j]);
                } else if (cols) {
                    path = cd_get_pc_file_name(worldmap_terrain_column_file);
                    worldmap_stream_add_host_request(path,
                                  ((j % worldmap_area_blocks_x) << 11) * worldmap_area_blocks_z + ((j / worldmap_area_blocks_x) << 11),
                                  0x710, worldmap_terrain_blocks[j]);
                }
            }
        }
        worldmap_stream_add_host_request(NULL, 0, 0, NULL);
        worldmap_stream_submit_host_list();
    }
}

/* Scratchpad work area of the terrain draw. */
typedef struct {
    u8 pad0[0x288];
    u16 clut[0x40];  /* 0x288 */
    u16 tpage[8];    /* 0x308 */
    s32 x0;          /* 0x318 */
    s32 pad31C;
    s32 z0;          /* 0x320 */
    s32 pad324;
    SVECTOR corner[4]; /* 0x328: quarter origins */
    u8 pad348[8];
    MATRIX local;    /* 0x350 */
    MATRIX world;    /* 0x370 */
} TerrainDrawScratch;

/* 8009932C: Draw the visible 5x5 terrain blocks around the camera: all four quarters
 * of a block, or only the quarters whose flag differs when the combined
 * flags are all set. */
void worldmap_terrain_draw(u_long *ot, s32 packets, Camera *camera) {
    TerrainDrawScratch *scratch;
    u8 *data;
    s32 row;
    s32 column;
    s32 cell;
    s16 all;

    scratch = (TerrainDrawScratch *)0x1F800000;
    for (column = 0; column < 0x40; column++) {
        scratch->clut[column] = worldmap_terrain_cluts[column];
    }
    for (column = 0; column < 7; column++) {
        scratch->tpage[column] = worldmap_terrain_tpages[column];
    }
    scratch->local = worldmap_terrain_matrix;
    CompMatrix(&worldmap_camera_matrix, &scratch->local, &scratch->world);
    SetRotMatrix(&scratch->world);
    SetTransMatrix(&scratch->world);
    scratch->x0 = -((camera->target.vx >> 12) & 0x7FF) - 0x1000;
    cell = 0;
    scratch->z0 = -((camera->target.vz >> 12) & 0x7FF) - 0x1000;
    worldmap_terrain_packet_count = 0;
    scratch->corner[0].vz = -scratch->z0;
    for (row = 0; row < 5; row++) {
        scratch->corner[0].vx = scratch->x0;
        for (column = 0; column < 5; column++, cell++, scratch->corner[0].vx += 0x800) {
            if (worldmap_terrain_visible_blocks[cell] != -1) {
                data = worldmap_terrain_blocks[worldmap_terrain_grid.cells[(row + worldmap_camera_block_cell.vz) * 9 + column + worldmap_camera_block_cell.vx]];
                scratch->corner[3].vx = scratch->corner[1].vx = scratch->corner[0].vx + 0x400;
                scratch->corner[1].vz = scratch->corner[0].vz;
                scratch->corner[2].vx = scratch->corner[0].vx;
                scratch->corner[3].vz = scratch->corner[2].vz = scratch->corner[0].vz - 0x400;
                all = (u16)worldmap_terrain_quarter_visibility[cell][3] |
                      ((u16)worldmap_terrain_quarter_visibility[cell][2] |
                       ((u16)worldmap_terrain_quarter_visibility[cell][0] | (u16)worldmap_terrain_quarter_visibility[cell][1]));
                if (all != -1) {
                    worldmap_terrain_build_and_draw_quarter((u32 *)data, ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[0]);
                    worldmap_terrain_build_and_draw_quarter((u32 *)(data + 0x144), ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[1]);
                    worldmap_terrain_build_and_draw_quarter((u32 *)(data + 0x288), ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[2]);
                    worldmap_terrain_build_and_draw_quarter((u32 *)(data + 0x3CC), ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[3]);
                } else {
                    if (worldmap_terrain_quarter_visibility[cell][0] != all) {
                        worldmap_terrain_build_and_draw_quarter((u32 *)data, ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[0]);
                    }
                    if (worldmap_terrain_quarter_visibility[cell][1] != all) {
                        worldmap_terrain_build_and_draw_quarter((u32 *)(data + 0x144), ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[1]);
                    }
                    if (worldmap_terrain_quarter_visibility[cell][2] != all) {
                        worldmap_terrain_build_and_draw_quarter((u32 *)(data + 0x288), ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[2]);
                    }
                    if (worldmap_terrain_quarter_visibility[cell][3] != all) {
                        worldmap_terrain_build_and_draw_quarter((u32 *)(data + 0x3CC), ot, packets + (worldmap_terrain_packet_count << 5), &scratch->corner[3]);
                    }
                }
            }
        }
        scratch->corner[0].vz -= 0x800;
    }
}

/* 80099708: Build a terrain block's 9x9 vertices in the scratchpad (heights of
 * water cells follow two travelling sine waves), then draw the block. */
void worldmap_terrain_build_and_draw_quarter(u32 *heights, u_long *ot, s32 packets, SVECTOR *origin) {
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
    sine = rcossin_tbl;
    row_phase = worldmap_wave_phase_z;
    left = origin->vx;
    z = origin->vz;
    for (; j != -1; j--) {
        x = left;
        i = 8;
        swell = sine[row_phase & 0xFFF][0] * 2;
        phase = worldmap_wave_phase_x;
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
    worldmap_terrain_draw_quarter_block(heights, ot, packets);
}

/* 8009980C */
INCLUDE_ASM("decomp/src/worldmap", worldmap_terrain_draw_quarter_block);

/* 80099BFC */
INCLUDE_ASM("decomp/src/worldmap", worldmap_billboards_draw_block);
