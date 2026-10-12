/*
 * Presentation records of the world map's scene objects (docs/runtime.md,
 * Presentation; present.c): read-only, nothing here writes game state.
 */
#include "common.h"
#include "xem/gte.h"
#include "resident/model.h"
#include "../decomp/src/worldmap/worldmap.h"
#include "../decomp/src/worldmap/camera.h"
#include "../decomp/src/worldmap/scene.h"
#include "../decomp/src/worldmap/terrain.h"

extern s16 worldmap_object_draw_modes[10];
void xem_original_worldmap_objects_draw(void);
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
