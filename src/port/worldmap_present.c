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

extern s16 worldmap_object_draw_modes[10];
extern void *worldmap_cloud_quads[2];
void xem_original_worldmap_objects_draw(void);
void xem_original_worldmap_clouds_draw(void);
void xem_original_worldmap_terrain_draw(u_long *ot, s32 packets, Camera *camera);
void xem_worldmap_terrain_reveal_quarter(u32 cells, u32 ot, u32 packet, const u32 *vertices);
void xem_reveal_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode, XemGte *placed);

/* The scene objects: the original, then, while a camera pulled back wants
 * culled faces (xem/gte.h), the ones it leaves undrawn because their
 * origin's projection overflowed (near or behind its camera, or far off its
 * screen) that the pulled-back camera sees within the original's draw
 * distance (depth 0xD80), drawn again for the record (present.c
 * xem_reveal_model) with the transform the original builds for them (its
 * parent chain, the offset from the camera's target wrapped round the map,
 * half scale). The GTE and the ring's register provenance are put back. */
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
    s32 i, x, z, radius;

    xem_original_worldmap_objects_draw();
    if (!xem_present_reveals->wanted || xem_present_capture->active || xem_present_reveals->pull_back == 0) {
        return;
    }
    gte = xem_gte;
    for (i = 0; i < 3; i++) {
        fifo[i] = ring->fifo[i];
    }
    x = worldmap_camera.target.vx >> 12;
    z = worldmap_camera.target.vz >> 12;
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

/* The clouds' work area: the quads added this frame (DriftScratch.count). */
#define XEM_CLOUD_COUNT 0x1F8002F0u
/* Slots of the screen point -> projection table (in the capture area's
 * backup, which nothing uses meanwhile). */
#define XEM_CLOUD_SLOTS 4096

/* The clouds: the original, then (presentation only) the screen points of
 * the quads it filled this frame recorded as stored with the projections
 * that made them, as swc2 records a point: the original reads them from
 * the GTE (mfc2) and stores them with plain writes, so without this the
 * renderer could not tell them from 2D points and would keep them inside
 * the original screen. Each point takes the latest projection of the pass
 * with its value. */
void worldmap_clouds_draw(void) {
    XemProjectionRing *ring = xem_gte_ring;
    u32 *table = xem_present_capture->backup;
    u32 first = ring->count, n, i, k, slot, word, at, quads;

    xem_original_worldmap_clouds_draw();
    if (!ring->recording || ring->count - first > XEM_CLOUD_SLOTS / 2 || xem_present_capture->active) {
        return;
    }
    for (i = 0; i < XEM_CLOUD_SLOTS * 2; i++) {
        table[i] = 0;
    }
    for (n = first; n != ring->count; n++) {
        word = ring->entries[n % XEM_GTE_PROJECTIONS].sxy;
        for (slot = (word * 2654435761u) >> 20; table[slot * 2 + 1] != 0 && table[slot * 2] != word;) {
            slot = (slot + 1) % XEM_CLOUD_SLOTS;
        }
        table[slot * 2] = word;
        table[slot * 2 + 1] = n + 1;
    }
    quads = (u32)worldmap_cloud_quads[worldmap_display_buffer_index];
    n = XEM_U32(XEM_CLOUD_COUNT);
    for (i = 0; i < n && i < 0x120; i++) {
        for (k = 0; k < 4; k++) {
            at = quads + i * sizeof(POLY_FT4) + 8 + k * 8;
            word = XEM_U32(at);
            for (slot = (word * 2654435761u) >> 20; table[slot * 2 + 1] != 0; slot = (slot + 1) % XEM_CLOUD_SLOTS) {
                if (table[slot * 2] == word) {
                    xem_present_store(at, word, table[slot * 2 + 1]);
                    break;
                }
            }
        }
    }
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
