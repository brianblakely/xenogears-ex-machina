/*
 * Presentation records of the world map's scene objects (docs/runtime.md,
 * Presentation; present.c): read-only, nothing here writes game state.
 */
#include "common.h"
#include "xem/gte.h"
#include "xem/gte_ops.h"
#include "xem/memory.h"
#include "resident/model.h"
#include "../decomp/src/worldmap/worldmap.h"
#include "../decomp/src/worldmap/camera.h"
#include "../decomp/src/worldmap/scene.h"
#include "../decomp/src/worldmap/terrain.h"
#include "../decomp/src/worldmap/effect.h"

extern s16 worldmap_object_draw_modes[10];
extern u16 worldmap_cloud_texture_origins[8];
extern SVECTOR worldmap_cloud_far_corners[12];
extern SVECTOR worldmap_cloud_near_corners[48];
void xem_original_worldmap_objects_draw(void);
void xem_original_worldmap_clouds_draw(void);
void xem_original_worldmap_terrain_draw(u_long *ot, s32 packets, Camera *camera);
void xem_worldmap_terrain_reveal_quarter(u32 cells, u32 ot, u32 packet, const u32 *vertices);
void xem_reveal_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode, XemGte *placed);
s32 xem_present_cameras(void);
void xem_present_camera(MATRIX *m, s32 x, s32 y, s32 z, u32 mode);
void xem_record_entry(u32 class_, s32 sz, u32 owner, s32 x, s32 y, s32 z);

/* The ground's height (world units, y down) at (x, z) (20.12) into
 * `ground`: the plane of the terrain triangle there, its cell split along
 * one diagonal as worldmap_terrain_get_normal splits it, which
 * worldmap_terrain_get_height takes with the GTE and the scratchpad; here
 * only read, without its rounding. 0 where no terrain block is loaded. */
static s32 xem_worldmap_ground(s32 x, s32 z, s32 *ground) {
    s32 block, u, v, h00, h10, h01, h11, h;
    u8 *cell;

    if (x < 0 || z < 0) {
        return 0;
    }
    block = ((z >> 20) / 8) * worldmap_area_blocks_x + (x >> 20) / 8;
    if (block >= 0x100 || worldmap_terrain_blocks[block] == NULL) {
        return 0;
    }
    cell = worldmap_terrain_get_cell(x, z);
    /* The point across its cell (0x10000 each way) and the heights of the
     * cell's corners, in steps of 8 units: x, then z, then both. */
    u = (u16)(x / 8);
    v = (u16)(z / 8);
    h00 = (s8)cell[0];
    h10 = (s8)cell[4];
    h01 = (s8)cell[0x24];
    h11 = (s8)cell[0x28];
    if (cell[1] & 0x80) {
        h = u < v ? h00 * 0x10000 + u * (h11 - h01) + v * (h01 - h00) : h00 * 0x10000 + u * (h10 - h00) + v * (h11 - h10);
    } else {
        h = u + v < 0x10000 ? h00 * 0x10000 + u * (h10 - h00) + v * (h01 - h00)
                            : (h10 + h01 - h11) * 0x10000 + u * (h11 - h01) + v * (h11 - h10);
    }
    *ground = h / 0x2000;
    return 1;
}

/* The scene objects: the original, then the camera every world-map frame
 * draws its scene objects and terrain with (worldmap_camera_matrix, which
 * places points relative to the camera's target, z negated) and the point
 * it looks at (the target, at (0, its height, 0) there:
 * worldmap_camera_place_orbit), after the world's point at that target and
 * (a view from anywhere) the ground under it (STAGE entries, xem/gte.h);
 * then, while a camera pulled back or a view looking from around the scene
 * wants culled faces, the objects it leaves undrawn because their origin's projection
 * overflowed (near or behind its camera, or far off its screen) that the
 * view sees (any, looking around) within the original's draw distance
 * (depth 0xD80), drawn again for the record (present.c xem_reveal_model)
 * with the transform the original builds for them (its parent chain, the
 * offset from the camera's target wrapped round the map, half scale). The
 * GTE and the ring's register provenance are put back. */
void worldmap_objects_draw(void) {
    XemProjectionRing *ring = xem_gte_ring;
    SceneObject *object, *parent;
    XemProjection *p;
    XemGte gte, placed;
    MATRIX m, link, out;
    VECTOR offset, scale;
    SVECTOR origin;
    long sxy, interpolation, flag;
    u32 fifo[3];
    s32 i, x, z, radius, ground;

    xem_original_worldmap_objects_draw();
    x = worldmap_camera.target.vx >> 12;
    z = worldmap_camera.target.vz >> 12;
    if (xem_present_cameras()) {
        xem_record_entry(XEM_PROJECTION_STAGE, 0, XEM_STAGE_ORIGIN, x, 0, -z);
        if (xem_present_reveals->wanted && (xem_present_reveals->around & XEM_AROUND_ANYWHERE) &&
            xem_worldmap_ground(worldmap_camera.target.vx, worldmap_camera.target.vz, &ground)) {
            xem_record_entry(XEM_PROJECTION_STAGE, 0, XEM_STAGE_GROUND, 0, ground, 0);
        }
        xem_present_camera(&worldmap_camera_matrix, 0, worldmap_camera.target.vy >> 12, 0, XEM_CAMERA_WORLDMAP);
    }
    if (!xem_present_reveals->wanted || xem_present_capture->active || !xem_present_elsewhere()) {
        return;
    }
    gte = xem_gte;
    for (i = 0; i < 3; i++) {
        fifo[i] = ring->fifo[i];
    }
    scale.vx = scale.vy = scale.vz = 0x800;
    origin.vx = origin.vy = origin.vz = 0;
    for (i = 0; i < worldmap_object_count; i++) {
        object = &worldmap_objects[i];
        if (object->visible != 0 || object->def == NULL) {
            continue;
        }
        m = object->matrix;
        m.t[0] = object->position.vx;
        m.t[1] = object->position.vy;
        m.t[2] = -object->position.vz;
        for (parent = object; parent->parent != NULL;) {
            parent = parent->parent;
            link = parent->matrix;
            link.t[0] = parent->position.vx;
            link.t[1] = parent->position.vy;
            link.t[2] = -parent->position.vz;
            CompMatrix(&link, &m, &m);
        }
        offset.vx = m.t[0] - x;
        offset.vy = 0;
        offset.vz = -m.t[2] - z;
        worldmap_wrap_world_offset(&offset);
        m.t[0] = offset.vx;
        m.t[2] = -offset.vz;
        ScaleMatrix(&m, &scale);
        CompMatrix(&worldmap_camera_matrix, &m, &out);
        SetRotMatrix(&out);
        SetTransMatrix(&out);
        RotTransPers(&origin, &sxy, &interpolation, &flag);
        if (flag >= 0) {
            continue; /* drawn, or beyond the draw distance */
        }
        p = &ring->entries[(ring->count - 1) % XEM_GTE_PROJECTIONS];
        radius = object->def->box_max.vx - object->def->box_min.vx;
        radius = radius > object->def->box_max.vy - object->def->box_min.vy ? radius : object->def->box_max.vy - object->def->box_min.vy;
        radius = radius > object->def->box_max.vz - object->def->box_min.vz ? radius : object->def->box_max.vz - object->def->box_min.vz;
        if (p->view[2] >= 0xD80 || !xem_present_square_visible(p->view[0], p->view[1], p->view[2], radius, 320, 216, 0, 0)) {
            continue;
        }
        placed = xem_gte;
        xem_reveal_model(object->def, (RenderPacket *)(&object->prims)[worldmap_display_buffer_index],
                         (u32 *)worldmap_current_display_buffer->ot, worldmap_object_draw_modes[(s16)object->flags], &placed);
    }
    xem_gte = gte;
    for (i = 0; i < 3; i++) {
        ring->fifo[i] = fifo[i];
    }
}

/* The cloud pass's work area words the reveal reads: its view cone's edges
 * and view point, and its rotation (DriftScratch.edge, .view and .local,
 * whose translation alone changes from cloud to cloud). */
#define XEM_CLOUD_EDGES 0x1F800230u
#define XEM_CLOUD_VIEW 0x1F800240u
#define XEM_CLOUD_LOCAL 0x1F800250u
/* The flags that drop a cloud, a near layer or a near quad. */
#define XEM_CLOUD_ERRORS 0x7F85E000u
/* Half the size of a cloud (its quads span -192..192). */
#define XEM_CLOUD_RADIUS 0x180

/* A vector into V0, V1 or V2 (`reg` its xy register), as lwc2 loads it. */
static void xem_cloud_load(s32 reg, const SVECTOR *v) {
    xem_gte_write_data(reg, (u32)(u16)v->vx | (u32)(u16)v->vy << 16);
    xem_gte_write_data(reg + 1, (u32)(u16)v->vz);
}

/* A matrix's rotation and translation into the GTE, as gte_SetRotMatrix and
 * gte_SetTransMatrix load them. */
static void xem_cloud_set_matrix(const MATRIX *m) {
    xem_gte_write_control(0, (u32)(u16)m->m[0][0] | (u32)(u16)m->m[0][1] << 16);
    xem_gte_write_control(1, (u32)(u16)m->m[0][2] | (u32)(u16)m->m[1][0] << 16);
    xem_gte_write_control(2, (u32)(u16)m->m[1][1] | (u32)(u16)m->m[1][2] << 16);
    xem_gte_write_control(3, (u32)(u16)m->m[2][0] | (u32)(u16)m->m[2][1] << 16);
    xem_gte_write_control(4, (u32)(u16)m->m[2][2]);
    xem_gte_write_control(5, (u32)m->t[0]);
    xem_gte_write_control(6, (u32)m->t[1]);
    xem_gte_write_control(7, (u32)m->t[2]);
}

/* gte_CompMatrix(&worldmap_camera_matrix, local, screen) on the GTE: the
 * product's columns through IR, then the translation through RT and TR. */
static void xem_cloud_matrix(const MATRIX *local, MATRIX *screen) {
    s32 c, r;

    xem_cloud_set_matrix(&worldmap_camera_matrix);
    for (c = 0; c < 3; c++) {
        for (r = 0; r < 3; r++) {
            xem_gte_write_data(XEM_GTE_IR1 + r, (u16)local->m[r][c]);
        }
        xem_gte_execute(XEM_GTE_MVMVA(1, 0, 3, 3, 0));
        for (r = 0; r < 3; r++) {
            screen->m[r][c] = (s16)xem_gte_read_data(XEM_GTE_IR1 + r);
        }
    }
    xem_gte_write_data(XEM_GTE_VXY0, (u32)(u16)local->t[0] | (u32)(u16)local->t[1] << 16);
    xem_gte_write_data(XEM_GTE_VZ0, (u32)local->t[2]);
    xem_gte_execute(XEM_GTE_MVMVA(1, 0, 0, 0, 0));
    for (r = 0; r < 3; r++) {
        screen->t[r] = (s32)xem_gte_read_data(XEM_GTE_MAC1 + r);
    }
}

/* A quad projected as the original projects it: RTPT of its first three
 * corners, then RTPS of the fourth unless the first raised `errors`. Its
 * screen points into `sxy`; 0 when a projection raised `errors`. */
static s32 xem_cloud_project(const SVECTOR *v, u32 errors, u32 *sxy) {
    xem_cloud_load(XEM_GTE_VXY0, &v[0]);
    xem_cloud_load(XEM_GTE_VXY1, &v[1]);
    xem_cloud_load(XEM_GTE_VXY2, &v[2]);
    xem_gte_execute(XEM_GTE_RTPT);
    if (xem_gte_read_control(31) & errors) {
        return 0;
    }
    sxy[0] = xem_gte_read_data(XEM_GTE_SXY0);
    sxy[1] = xem_gte_read_data(XEM_GTE_SXY1);
    sxy[2] = xem_gte_read_data(XEM_GTE_SXY2);
    xem_cloud_load(XEM_GTE_VXY0, &v[3]);
    xem_gte_execute(XEM_GTE_RTPS);
    if (xem_gte_read_control(31) & errors) {
        return 0;
    }
    sxy[3] = xem_gte_read_data(XEM_GTE_SXY2);
    return 1;
}

/* The original's screen test (ON_SCREEN): some corner's x and some
 * corner's y on its 320x216 screen. */
static s32 xem_cloud_on_screen(const u32 *sxy) {
    return ((u16)sxy[0] < 0x140 || (u16)sxy[1] < 0x140 || (u16)sxy[2] < 0x140 || (u16)sxy[3] < 0x140) &&
           (sxy[0] >> 16 < 0xD8 || sxy[1] >> 16 < 0xD8 || sxy[2] >> 16 < 0xD8 || sxy[3] >> 16 < 0xD8);
}

/* The depth a quad is sorted at, as the original takes the largest of its
 * corners' (SZ0-SZ3, the third and fourth compared only when the third is
 * not the larger). */
static s32 xem_cloud_depth(void) {
    s32 sz0 = (s32)xem_gte_read_data(XEM_GTE_SZ0), sz1 = (s32)xem_gte_read_data(XEM_GTE_SZ1);
    s32 sz2 = (s32)xem_gte_read_data(XEM_GTE_SZ2), sz3 = (s32)xem_gte_read_data(XEM_GTE_SZ3);
    s32 z = sz0 > sz1 ? sz0 : sz1;

    if (sz2 > z) {
        z = sz2;
    } else if (sz3 > z) {
        z = sz3;
    }
    return z;
}

/* A quad the original leaves out recorded as the POLY_FT4 it would have
 * filled (the pool's colour, CLUT and page; its texture origin `uv` and
 * size `span`), at the ordering-table entry of `depth`, its corners the
 * last four projections. */
static void xem_cloud_reveal(u32 quads, u32 ot, s32 depth, u32 uv, u32 span, const u32 *sxy) {
    u32 words[9];

    words[0] = XEM_U32(quads + 4);
    words[1] = sxy[0];
    words[2] = (XEM_U32(quads + 12) & 0xFFFF0000u) | (uv & 0xFFFF);
    words[3] = sxy[1];
    words[4] = (XEM_U32(quads + 20) & 0xFFFF0000u) | ((uv | span) & 0xFFFF);
    words[5] = sxy[2];
    words[6] = (uv | span << 8) & 0xFFFF;
    words[7] = sxy[3];
    words[8] = (uv | span << 8 | span) & 0xFFFF;
    xem_present_reveal(ot + ((u32)depth >> 4 << 2), 4, xem_gte_ring->count, words, 9);
}

/* Presentation only (xem/gte.h), while the host wants culled faces: the
 * cloud quads the original leaves out as outside its view cone or screen
 * that the wider (or pulled-back) view reaches, recorded as the quads it
 * would have filled. The original's pass is replayed on the GTE as it ran
 * (its matrices, view-cone test, distances, flags and quad budget), so the
 * quads it filled are known and not recorded again, and the depths past
 * which it stops a far cloud's layers stop the recorded ones too. The GTE
 * and the ring's register provenance are put back. */
static void xem_clouds_reveal(void) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p;
    XemGte saved;
    MATRIX local, screen;
    SVECTOR corner[4];
    const SVECTOR *base = &worldmap_cloud_far_corners[0];
    u32 fifo[3], sxy[4], quads, ot, uv, cone;
    s32 i, k, x, z, x0, z0, y, layer, row, column, count, depth, index, drawn, ended;

    if (!xem_present_reveals->wanted || xem_present_capture->active || !ring->recording) {
        return;
    }
    saved = xem_gte;
    for (k = 0; k < 3; k++) {
        fifo[k] = ring->fifo[k];
    }
    local = XEM_AT(MATRIX, XEM_CLOUD_LOCAL);
    quads = (u32)worldmap_cloud_quads[worldmap_display_buffer_index];
    ot = (u32)worldmap_current_display_buffer->ot;
    count = 0;
    for (i = 0; i < 0x50 && count <= 0xF0; i++) {
        x = (worldmap_cloud_positions[i].x - (worldmap_camera.target.vx & 0x1FFFFFF)) >> 12;
        z = (worldmap_cloud_positions[i].z - (worldmap_camera.target.vz & 0x1FFFFFF)) >> 12;
        if (x < -0x1000) {
            x += 0x2000;
        } else if (x >= 0x1000) {
            x -= 0x2000;
        }
        if (z < -0x1000) {
            z += 0x2000;
        } else if (z >= 0x1000) {
            z -= 0x2000;
        }
        z = -z;
        /* The view cone, as the original tests it. */
        cone = ((u32)(z - (s32)XEM_U32(XEM_CLOUD_VIEW + 8)) << 16) | ((u32)(x - (s32)XEM_U32(XEM_CLOUD_VIEW)) & 0xFFFF);
        xem_gte_write_data(XEM_GTE_SXY0, cone);
        xem_gte_write_data(XEM_GTE_SXY1, XEM_U32(XEM_CLOUD_EDGES + 4));
        xem_gte_write_data(XEM_GTE_SXY2, 0);
        xem_gte_execute(XEM_GTE_NCLIP);
        drawn = (s32)xem_gte_read_data(XEM_GTE_MAC0) <= 0;
        xem_gte_write_data(XEM_GTE_SXY0, 0);
        xem_gte_write_data(XEM_GTE_SXY1, XEM_U32(XEM_CLOUD_EDGES));
        xem_gte_write_data(XEM_GTE_SXY2, cone);
        xem_gte_execute(XEM_GTE_NCLIP);
        drawn = drawn && (s32)xem_gte_read_data(XEM_GTE_MAC0) <= 0;
        local.t[0] = x;
        local.t[1] = worldmap_cloud_positions[i].unk4 >> 12;
        local.t[2] = z;
        xem_cloud_matrix(&local, &screen);
        xem_cloud_set_matrix(&screen);
        xem_gte_write_data(XEM_GTE_VXY0, 0);
        xem_gte_write_data(XEM_GTE_VZ0, 0);
        xem_gte_execute(XEM_GTE_RTPS);
        drawn = drawn && !(xem_gte_read_control(31) & XEM_CLOUD_ERRORS);
        /* One the original leaves out that the wider view does not reach
         * is left out (one it draws is replayed for its quads' count). */
        p = &ring->entries[(ring->count - 1) % XEM_GTE_PROJECTIONS];
        if (!drawn && !xem_present_square_visible(p->view[0], p->view[1], p->view[2], XEM_CLOUD_RADIUS, 0x140, 0xD8, 0, 0)) {
            continue;
        }
        index = worldmap_cloud_velocities[i].unk2 * 4;
        depth = (s32)xem_gte_read_data(XEM_GTE_SZ3);
        if (depth > 0x580) {
            /* Far: up to three 64-texel quads. */
            for (layer = 0, ended = 0; layer < 3 && !ended; layer++) {
                uv = worldmap_cloud_texture_origins[index++];
                if (!xem_cloud_project(&worldmap_cloud_far_corners[layer * 4], 0x80000000u, sxy)) {
                    continue;
                }
                depth = xem_cloud_depth();
                if (drawn && xem_cloud_on_screen(sxy)) {
                    ended = depth > 0xB00;
                    count += depth <= 0xD00;
                    continue;
                }
                if (!xem_present_points_visible(ring->count, 4, 0x140, 0xD8)) {
                    continue;
                }
                ended = depth > 0xB00;
                if (depth <= 0xD00) {
                    xem_cloud_reveal(quads, ot, depth, uv, 0x3F, sxy);
                }
            }
        } else if (depth > 0x400) {
            /* Middle: three layers of four 32-texel quads. */
            for (layer = 0; layer < 3; layer++) {
                for (column = 0; column < 4; column++) {
                    uv = worldmap_cloud_texture_origins[layer + index] + ((column & 2) << 12) + ((column & 1) << 5);
                    if (!xem_cloud_project(&worldmap_cloud_near_corners[(layer * 4 + column) * 4], 0x80000000u, sxy)) {
                        continue;
                    }
                    depth = xem_cloud_depth();
                    if (drawn && xem_cloud_on_screen(sxy)) {
                        count++;
                    } else if (xem_present_points_visible(ring->count, 4, 0x140, 0xD8)) {
                        xem_cloud_reveal(quads, ot, depth, uv, 0x1F, sxy);
                    }
                }
            }
        } else {
            /* Near: three layers of 4x4 16-texel quads over a 0x180 square. */
            x0 = base->vx;
            z0 = base->vz;
            for (layer = 0; layer < 3; layer++) {
                y = base->vy - layer * 8;
                for (k = 0; k < 4; k++) {
                    corner[k].vx = (s16)(x0 + (k & 1) * 0x180);
                    corner[k].vy = (s16)y;
                    corner[k].vz = (s16)(z0 - (k >> 1) * 0x180);
                }
                xem_cloud_load(XEM_GTE_VXY0, &corner[0]);
                xem_gte_execute(XEM_GTE_RTPS);
                if (xem_gte_read_control(31) & XEM_CLOUD_ERRORS) {
                    continue;
                }
                xem_cloud_load(XEM_GTE_VXY0, &corner[1]);
                xem_cloud_load(XEM_GTE_VXY1, &corner[2]);
                xem_cloud_load(XEM_GTE_VXY2, &corner[3]);
                xem_gte_execute(XEM_GTE_RTPT);
                if (xem_gte_read_control(31) & XEM_CLOUD_ERRORS) {
                    continue;
                }
                for (row = 0; row < 4; row++) {
                    for (column = 0; column < 4; column++) {
                        for (k = 0; k < 4; k++) {
                            corner[k].vx = (s16)(x0 + column * 0x60 + (k & 1) * 0x60);
                            corner[k].vz = (s16)(z0 - row * 0x60 - (k >> 1) * 0x60);
                        }
                        uv = worldmap_cloud_texture_origins[layer + index] + (row << 12) + (column << 4);
                        if (!xem_cloud_project(corner, XEM_CLOUD_ERRORS, sxy)) {
                            continue;
                        }
                        depth = xem_cloud_depth();
                        if (drawn && xem_cloud_on_screen(sxy)) {
                            count++;
                        } else if (xem_present_points_visible(ring->count, 4, 0x140, 0xD8)) {
                            xem_cloud_reveal(quads, ot, depth, uv, 0xF, sxy);
                        }
                    }
                }
            }
        }
    }
    xem_gte = saved;
    for (k = 0; k < 3; k++) {
        ring->fifo[k] = fifo[k];
    }
}

/* The clouds: the original (asm.c records its quad provenance at the
 * packet link), then, while the host wants culled faces, its omitted quads. */
void worldmap_clouds_draw(void) {
    xem_original_worldmap_clouds_draw();
    xem_clouds_reveal();
}

/* A quarter block's 9x9 vertices as worldmap_terrain_build_and_draw_quarter
 * builds them in the scratchpad (the water's heights following the two
 * travelling waves), as vxy, vz words into `out`. */
static void xem_terrain_quarter_vertices(u32 *out, const u32 *cell, s32 left, s32 z) {
    s32 row_phase = worldmap_wave_phase_z, phase, swell, height, wave, x, i, j;

    for (j = 0; j < 9; j++, z -= 0x80, row_phase += 0x200) {
        swell = rcossin_tbl[row_phase & 0xFFF][0] * 2;
        phase = worldmap_wave_phase_x;
        for (i = 0, x = left; i < 9; i++, x += 0x80, phase += 0x200, cell++, out += 2) {
            if (*cell & 0x1000) {
                height = (s32)(*cell << 24) >> 21;
                wave = (rcossin_tbl[phase & 0xFFF][0] * swell) >> 20;
                out[0] = ((u32)(wave + height) << 16) | ((u32)x & 0xFFFF);
            } else {
                out[0] = (u32)((s32)(*cell << 24) >> 5) | ((u32)x & 0xFFFF);
            }
            out[1] = (u32)z & 0xFFFF;
        }
    }
}

/* The terrain: the original, then (presentation only, while the host wants
 * culled faces) the quarter blocks of its 5x5 window it leaves undrawn as
 * outside its camera's view whose centre the wanted view reaches: built as
 * the original builds a quarter, into a local array instead of the
 * scratchpad, their triangles recorded as culled (worldmap_terrain.c). The
 * GTE (still holding the terrain's matrices) and the ring's register
 * provenance are put back. */
void worldmap_terrain_draw(u_long *ot, s32 packets, Camera *camera) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p;
    XemGte gte;
    u32 fifo[3], vertices[81 * 2], cells;
    u8 *data;
    s32 cell, quarter, culled, left, top, x0, z0, i;
    u16 all;

    xem_original_worldmap_terrain_draw(ot, packets, camera);
    if (!xem_present_reveals->wanted || xem_present_capture->active) {
        return;
    }
    gte = xem_gte;
    for (i = 0; i < 3; i++) {
        fifo[i] = ring->fifo[i];
    }
    x0 = -((camera->target.vx >> 12) & 0x7FF) - 0x1000;
    z0 = -((camera->target.vz >> 12) & 0x7FF) - 0x1000;
    for (cell = 0; cell < 25; cell++) {
        culled = worldmap_terrain_visible_blocks[cell] == -1;
        all = (u16)worldmap_terrain_quarter_visibility[cell][0] | (u16)worldmap_terrain_quarter_visibility[cell][1] |
              (u16)worldmap_terrain_quarter_visibility[cell][2] | (u16)worldmap_terrain_quarter_visibility[cell][3];
        if (!culled && all != 0xFFFF) {
            continue; /* all four drawn */
        }
        data = worldmap_terrain_blocks[worldmap_terrain_grid.cells[(cell / 5 + worldmap_camera_block_cell.vz) * 9 + cell % 5 +
                                                                   worldmap_camera_block_cell.vx]];
        for (quarter = 0; quarter < 4; quarter++) {
            if (!culled && worldmap_terrain_quarter_visibility[cell][quarter] != -1) {
                continue;
            }
            left = x0 + cell % 5 * 0x800 + (quarter & 1) * 0x400;
            top = -z0 - cell / 5 * 0x800 - (quarter >> 1) * 0x400;
            /* Its centre at sea level, and around it half its diagonal
             * with the highest relief (a cell's height byte, x8) above. */
            xem_gte_write_data(XEM_GTE_VXY0, (u32)(left + 0x200) & 0xFFFF);
            xem_gte_write_data(XEM_GTE_VZ0, (u32)(top - 0x200) & 0xFFFF);
            xem_gte_execute(XEM_GTE_RTPS);
            p = &ring->entries[(ring->count - 1) % XEM_GTE_PROJECTIONS];
            if (!xem_present_square_visible(p->view[0], p->view[1], p->view[2], 0x500, 320, 216, 0, 0)) {
                continue;
            }
            cells = (u32)(XemUintptr)(data + quarter * 0x144);
            xem_terrain_quarter_vertices(vertices, (const u32 *)(XemUintptr)cells, left, top);
            xem_worldmap_terrain_reveal_quarter(cells, (u32)(XemUintptr)ot, (u32)packets, vertices);
        }
    }
    xem_gte = gte;
    for (i = 0; i < 3; i++) {
        ring->fifo[i] = fifo[i];
    }
}
