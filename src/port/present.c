/*
 * Presentation capture (docs/runtime.md, Presentation): read-only records of
 * the pre-projection data at the handbook's seams (docs/later-phases.md,
 * Presentation), so renderers can re-traverse a frame for other views without
 * running game draw code again. Each wrapper records, then runs the original
 * unchanged; nothing here writes game state.
 */
#include "common.h"
#include "xem/gte.h"
#include "xem/gte_ops.h"
#include "xem/memory.h"
#include "resident/gpu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "battle/actor.h"
#include "../decomp/src/field/field.h"
#include "../decomp/src/field/field_camera.h"
#include "../decomp/src/field/field_draw.h"
#include "../decomp/src/field/field_resident.h"

/* Hand a record to the host: `kind`, then `length` bytes at `data`. */
void xem_host_present(unsigned int kind, void *data, unsigned int length);

enum {
    XEM_PRESENT_MODEL = 1,         /* model_draw_sprite_model */
    XEM_PRESENT_SPRITE_MATRIX = 2, /* sprite_set_draw_matrix */
    XEM_PRESENT_SPRITE_PARTS = 3,  /* sprite_draw_parts */
};

/* The GTE's transform and projection: rotation (5 words), translation (3),
 * screen offset (2), projection distance, depth cue (2). */
typedef struct {
    u32 rotation[5];
    s32 translation[3];
    s32 offset[2];
    u32 h;
    s32 depth_cue[2];
} XemTransform;

static void xem_capture_transform(XemTransform *t) {
    s32 i;

    for (i = 0; i < 5; i++) {
        t->rotation[i] = xem_gte_read_control(i);
    }
    for (i = 0; i < 3; i++) {
        t->translation[i] = xem_gte_read_control(5 + i);
    }
    t->offset[0] = xem_gte_read_control(24);
    t->offset[1] = xem_gte_read_control(25);
    t->h = xem_gte_read_control(26);
    t->depth_cue[0] = xem_gte_read_control(27);
    t->depth_cue[1] = xem_gte_read_control(28);
}

s32 xem_original_model_draw_sprite_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode);
s32 xem_original_field_instance_is_off_screen(FieldInstance *instance);
void xem_original_field_draw_models(void);
void xem_original_sprite_set_draw_matrix(Sprite *sprite);
void xem_original_sprite_set_view_matrix(MATRIX *view);
void xem_original_sprite_draw_parts(Sprite *sprite, u_long *ot);
void xem_original_sprite_draw_parts_cut_below(Sprite *sprite, u_long *ot, s32 height);
void xem_original_sprite_draw_parts_cut_above(Sprite *sprite, u_long *ot, s32 height);
void xem_original_worldmap_map_overlay_draw(void);
void xem_original_battle_popup_draw_glyph(SpritePart *glyph, s32 colour);
s32 xem_original_gpu_draw_panorama(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot, s32 buffer);
void xem_original_gpu_draw_panorama_strip(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer);
void xem_original_battle_draw_stage_sky(void *sky, SVECTOR *eye, SVECTOR *target, MATRIX *view, u32 *ot, s32 buffer);
void field_sprite_set_color_unless_fog(void *target, u8 *color);
void xem_present_sprite_facing(Sprite *sprite, u32 first, u32 last, s32 cut, s32 height);
void xem_present_battle_stage(void);
void xem_record_entry(u32 class_, s32 sz, u32 owner, s32 x, s32 y, s32 z);
/* The battle camera's look-at point (battle/objects.h, whose other headers
 * the port does not need). */
extern SVECTOR battle_camera_view_target;

/* Whether the host wants the faces the game culls (xem/gte.h), and no model
 * is being drawn again for the record. */
static s32 xem_reveals_wanted(void) {
    return xem_present_reveals->wanted && !xem_present_capture->active;
}

/* Presentation only (xem/gte.h): the transform the GTE holds, as the
 * PLACEMENT entries of the model or sprite `owner` it draws next (a model
 * into `packets`, one of its instance's two buffers of `size` bytes; 0 for
 * a sprite). Written to the ring only, while a trace takes it. */
static void xem_record_placement(u32 owner, u32 packets, u32 size) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p;
    s32 m[9], k;

    if (!ring->recording || xem_present_capture->active) {
        return;
    }
    /* RT's halfwords, row by row: R11 R12 / R13 R21 / R22 R23 / R31 R32 / R33. */
    for (k = 0; k < 9; k++) {
        m[k] = (s16)(xem_gte_read_control(k / 2) >> (k % 2 * 16));
    }
    for (k = 0; k < 3; k++) {
        xem_record_entry(XEM_PROJECTION_PLACEMENT, k, owner, m[k], m[3 + k], m[6 + k]);
    }
    xem_record_entry(XEM_PROJECTION_PLACEMENT, 3, owner, (s32)xem_gte_read_control(5), (s32)xem_gte_read_control(6),
                     (s32)xem_gte_read_control(7));
    p = &ring->entries[(ring->count - 1) % XEM_GTE_PROJECTIONS];
    p->x = (s32)packets;
    p->y = (s32)size;
}

/* Presentation only (xem/gte.h): a model the game did not draw because a
 * bounding test found it off its screen, drawn again for the record. With
 * the GTE as `placed` (what the drawing would have used), the box test off
 * and the renderers' links captured instead of made, its faces become
 * culled-face records at the entries they would have been linked at. Its
 * packets, the scratchpad words the renderers spill to, the renderers'
 * state, the GTE and the ring's bookkeeping are put back afterwards, so the
 * game sees nothing of it. */
void xem_reveal_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode, XemGte *placed) {
    XemCaptureArea *area = xem_present_capture;
    XemGte after = xem_gte;
    RenderPacket *packet = model_current_packet;
    u32 *model_table = model_ot;
    PrimitiveGroup *group = model_current_primitive_group;
    s32 *cache = model_lit_color_cache;
    SVECTOR *normals = model_current_normals, *vertices = model_current_vertices;
    s32 submitted = model_submitted_primitive_count, drawn = model_drawn_primitive_count, box = model_box_test_mode;
    u32 fifo[3], spill[3], words[12], class_ = xem_gte_ring->class_, owner = xem_gte_ring->owner;
    u32 size = (u32)model->packet_size / 4, at = (u32)packets, i, k, count, points;
    XemCapture *c;

    if (size > XEM_PRESENT_BACKUP / 4) {
        return;
    }
    for (i = 0; i < size; i++) {
        area->backup[i] = XEM_U32(at + i * 4);
    }
    for (i = 0; i < 3; i++) {
        fifo[i] = xem_gte_ring->fifo[i];
        spill[i] = XEM_U32(0x1F800000u + i * 4);
    }
    xem_gte = *placed;
    xem_record_placement((u32)model, (u32)packets, (u32)model->packet_size);
    model_box_test_mode = 0;
    xem_gte_ring->class_ = XEM_PROJECTION_MODEL;
    xem_gte_ring->owner = (u32)model;
    area->count = 0;
    area->active = 1;
    xem_original_model_draw_sprite_model(model, packets, ot, mode);
    area->active = 0;
    for (i = 0; i < area->count; i++) {
        c = &area->entries[i];
        count = c->tag >> 24;
        if (count == 0 || count > 12) {
            continue;
        }
        for (k = 0; k < count; k++) {
            words[k] = XEM_U32(c->slot + 4 + k * 4);
        }
        if (words[0] >> 29 != 1) {
            continue; /* not a polygon */
        }
        points = words[0] & 0x08000000 ? 4 : 3;
        if (xem_present_face_visible(c->projections, points, 320, 240)) {
            xem_present_reveal(c->entry, points, c->projections, words, count);
        }
    }
    for (i = 0; i < size; i++) {
        XEM_U32(at + i * 4) = area->backup[i];
    }
    for (i = 0; i < 3; i++) {
        xem_gte_ring->fifo[i] = fifo[i];
        XEM_U32(0x1F800000u + i * 4) = spill[i];
    }
    model_current_packet = packet;
    model_ot = model_table;
    model_current_primitive_group = group;
    model_lit_color_cache = cache;
    model_current_normals = normals;
    model_current_vertices = vertices;
    model_submitted_primitive_count = submitted;
    model_drawn_primitive_count = drawn;
    model_box_test_mode = box;
    xem_gte_ring->class_ = class_;
    xem_gte_ring->owner = owner;
    xem_gte = after;
}

/* A model drawn with the transform its caller left in the GTE: the mesh (in
 * game memory), its packets, the ordering table and the sort mode. One its
 * box test leaves undrawn is drawn again for the record when the host wants
 * culled faces. */
s32 model_draw_sprite_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode) {
    struct {
        u32 model, packets, ot;
        s32 mode;
        XemTransform transform;
    } record;
    u32 previous = xem_gte_ring->class_;
    u32 owner = xem_gte_ring->owner;
    s32 result, reveal = xem_reveals_wanted();
    XemGte placed;

    record.model = (u32)model;
    record.packets = (u32)packets;
    record.ot = (u32)ot;
    record.mode = mode;
    xem_capture_transform(&record.transform);
    xem_host_present(XEM_PRESENT_MODEL, &record, sizeof(record));
    if (reveal) {
        placed = xem_gte;
    }
    xem_record_placement((u32)model, (u32)packets, (u32)model->packet_size);
    xem_gte_ring->class_ = XEM_PROJECTION_MODEL;
    xem_gte_ring->owner = (u32)model;
    result = xem_original_model_draw_sprite_model(model, packets, ot, mode);
    xem_gte_ring->class_ = previous;
    xem_gte_ring->owner = owner;
    if (result == 0 && reveal) {
        xem_reveal_model(model, packets, ot, mode, &placed);
    }
    return result;
}

/* A field scenery piece the field's bounding test (field.c
 * field_draw_models) finds off screen is drawn again for the record, into
 * the table its descriptor selects, when the host wants culled faces and
 * the same test with the screen's margins widened by the host's reach
 * finds it within (that test runs with the GTE, its matrix and the margins
 * put back afterwards), or, with the camera pulled back, its bounding
 * square (around its centre in view space, which the test leaves in its
 * matrix) reaches that camera's screen (any piece, looking around). */
s32 field_instance_is_off_screen(FieldInstance *instance) {
    XemGte placed, tested;
    MATRIX cull;
    u32 *ot, reach = xem_present_reveals->wanted;
    s32 off, i, within, margin_x = field_model_cull_margin_x, margin_y = field_model_cull_margin_y;

    if (!xem_reveals_wanted()) {
        return xem_original_field_instance_is_off_screen(instance);
    }
    placed = xem_gte;
    off = xem_original_field_instance_is_off_screen(instance);
    if (off != 0) {
        tested = xem_gte;
        cull = field_instance_cull_matrix;
        if (xem_present_elsewhere()) {
            within = xem_present_square_visible(cull.t[0], cull.t[1], cull.t[2], instance->radius, 0x140, 0xE0, margin_x, margin_y);
        } else {
            xem_gte = placed;
            field_model_cull_margin_x = margin_x + (s32)(reach & 0xFFFF);
            field_model_cull_margin_y = margin_y + (s32)(reach >> 16);
            within = xem_original_field_instance_is_off_screen(instance) == 0;
            field_model_cull_margin_x = margin_x;
            field_model_cull_margin_y = margin_y;
            field_instance_cull_matrix = cull;
            xem_gte = tested;
        }
        if (!within) {
            return off;
        }
        ot = (u32 *)field_current_draw_block->ot;
        for (i = 0; i < field_view.components.descriptor_count; i++) {
            if (field_view.components.descriptors[i].instance == instance && (field_view.components.descriptors[i].flags & 0x8000)) {
                ot = (u32 *)field_current_draw_block->ot2;
            }
        }
        xem_reveal_model(instance->mesh, (RenderPacket *)instance->packets[field_draw_buffer_index], ot, instance->mode, &placed);
    }
    return off;
}

/* A ring entry that is no drawn point (xem/gte.h): `class_`, the depth
 * `sz`, `owner` and the point (x, y, z) as its `view`, with the GTE's
 * screen offset and projection distance. Written to the ring only. */
void xem_record_entry(u32 class_, s32 sz, u32 owner, s32 x, s32 y, s32 z) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p = &ring->entries[ring->count % XEM_GTE_PROJECTIONS];

    p->sxy = 0;
    p->sz = (u32)(sz < 0 ? 0 : sz > 0xFFFF ? 0xFFFF : sz) | class_ << 16;
    p->x = 0;
    p->y = 0;
    p->offset = ((u32)((s32)xem_gte_read_control(24) >> 16) & 0xFFFF) | (u32)((s32)xem_gte_read_control(25) >> 16) << 16;
    p->h = xem_gte_read_control(26) & 0xFFFF;
    p->owner = owner;
    p->view[0] = x;
    p->view[1] = y;
    p->view[2] = z;
    ring->count++;
}

/* Whether the cameras frames are drawn with are recorded (xem/gte.h): while
 * a trace takes the records, except for a model drawn again for them. */
s32 xem_present_cameras(void) {
    return xem_gte_ring->recording && !xem_present_capture->active;
}

/* The camera `m` a frame of `mode` (XEM_CAMERA_*) is drawn with, as the
 * world's axes through its rotation (its columns) and its translation
 * (XEM_PROJECTION_AXIS entries), then the point (x, y, z) it looks at and
 * that point's depth through it (XEM_PROJECTION_CAMERA, with the host's
 * `around` bits). Written to the ring only. */
void xem_present_camera(MATRIX *m, s32 x, s32 y, s32 z, u32 mode) {
    s32 k;

    for (k = 0; k < 3; k++) {
        xem_record_entry(XEM_PROJECTION_AXIS, 0, (u32)k, m->m[0][k], m->m[1][k], m->m[2][k]);
    }
    xem_record_entry(XEM_PROJECTION_AXIS, 0, 3, m->t[0], m->t[1], m->t[2]);
    xem_record_entry(XEM_PROJECTION_CAMERA, ((m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12) + m->t[2],
                     mode | (xem_present_reveals->around & 0xFF) << 8, x, y, z);
}

/* For a view looking around from the controlled character (xem/gte.h
 * `around`), after the field camera's record: the character's position
 * (its descriptor's translation) through the camera as an
 * XEM_PROJECTION_EYE entry its sprite owns. */
static void xem_record_eye(void) {
    MATRIX *m = &field_view.scaled_world;
    FieldDescriptor *d;
    long long v[3];
    s32 i, k;

    if (field_work.controlled < 0 || field_work.controlled >= field_event_actor_count) {
        return;
    }
    d = &field_view.components.descriptors[field_work.controlled];
    for (i = 0; i < 3; i++) {
        v[i] = 0;
        for (k = 0; k < 3; k++) {
            v[i] += (long long)m->m[i][k] * d->matrix.t[k];
        }
        v[i] = (v[i] >> 12) + m->t[i];
    }
    xem_record_entry(XEM_PROJECTION_EYE, (s32)v[2], (u32)d->model, (s32)v[0], (s32)v[1], (s32)v[2]);
}

/* The field's models: first, when the host wants culled faces (a view
 * wider or farther out than the original's), the depth of the camera's
 * target through the sprites' view (field_view.scaled_world) as an
 * XEM_PROJECTION_FOCUS entry of the ring: a view pulled back moves the
 * camera by multiples of it; then that camera with its target and, looking
 * from within or around the scene, the controlled character's place.
 * Written to the ring only. */
void field_draw_models(void) {
    MATRIX *m = &field_view.scaled_world;
    s32 x, y, z, depth;

    if (xem_present_cameras()) {
        x = field_view.target.vx >> 16;
        y = field_view.target.vy >> 16;
        z = field_view.target.vz >> 16;
        if (xem_reveals_wanted()) {
            depth = ((m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12) + m->t[2];
            xem_record_entry(XEM_PROJECTION_FOCUS, depth, 0, 0, 0, depth);
        }
        xem_present_camera(m, x, y, z, XEM_CAMERA_FIELD);
        if (xem_present_reveals->around != 0) {
            xem_record_eye();
        }
    }
    xem_original_field_draw_models();
}

/* The sprites' view matrix: the original, then, in a battle, its camera
 * (the matrix every battle frame sets it from, once its stage is drawn)
 * recorded with the point it looks at, after its stage (battle_present.c:
 * its combatants and start view) while a view from anywhere around the
 * scene wants culled faces (xem/gte.h XEM_AROUND_ANYWHERE). A field
 * records its camera with its models (field_draw_models), the world map
 * with its scene objects (worldmap_present.c), which every one of its
 * frames draws. Written to the ring only. */
void sprite_set_view_matrix(MATRIX *view) {
    xem_original_sprite_set_view_matrix(view);
    if (xem_present_cameras() && sprite_in_battle && view == &battle_camera.matrix) {
        if (xem_reveals_wanted() && (xem_present_reveals->around & XEM_AROUND_ANYWHERE)) {
            xem_present_battle_stage();
        }
        xem_present_camera(view, battle_camera_view_target.vx, battle_camera_view_target.vy, battle_camera_view_target.vz,
                           XEM_CAMERA_BATTLE);
    }
}

/* A sprite's draw matrix: its position through the view matrix, as the GTE
 * holds it after the original. */
void sprite_set_draw_matrix(Sprite *sprite) {
    struct {
        u32 sprite;
        XemTransform transform;
    } record;

    xem_original_sprite_set_draw_matrix(sprite);
    record.sprite = (u32)sprite;
    xem_capture_transform(&record.transform);
    xem_host_present(XEM_PRESENT_SPRITE_MATRIX, &record, sizeof(record));
}

/* The point on the floor under `sprite` (its x and z at its ground height)
 * through the sprite view matrix, recorded as a XEM_PROJECTION_GROUND
 * projection it owns (xem/gte.h), so effects lay its shadow on the floor
 * however high it jumps. The GTE and the ring's record of its screen points
 * are saved and restored around the RTPS: the game sees the GTE exactly as
 * without it. */
static void xem_record_ground(Sprite *sprite) {
    XemGte saved = xem_gte;
    u32 fifo[3], *rotation = (u32 *)sprite_view_matrix.m;
    s32 i;

    if (!xem_gte_ring->recording) {
        return;
    }
    for (i = 0; i < 3; i++) {
        fifo[i] = xem_gte_ring->fifo[i];
    }
    for (i = 0; i < 5; i++) {
        xem_gte_write_control(i, rotation[i]);
    }
    for (i = 0; i < 3; i++) {
        xem_gte_write_control(5 + i, (u32)sprite_view_matrix.t[i]);
    }
    xem_gte_write_data(0, ((u32)(sprite->x >> 16) & 0xFFFF) | (u32)(u16)sprite->ground << 16);
    xem_gte_write_data(1, (u32)(sprite->z >> 16) & 0xFFFF);
    xem_gte_ring->class_ = XEM_PROJECTION_GROUND;
    xem_gte_execute(0x0180001); /* RTPS */
    xem_gte = saved;
    for (i = 0; i < 3; i++) {
        xem_gte_ring->fifo[i] = fifo[i];
    }
}

/* A sprite's parts, projected with the transform in the GTE at the call:
 * the record, then the original (`cut` 0 whole, 1 cut below `height`, 2
 * cut above) with the class and owner of the projections it makes. */
static void xem_present_sprite_parts(Sprite *sprite, u_long *ot, s32 cut, s32 height) {
    struct {
        u32 sprite, ot;
        XemTransform transform;
    } record;
    u32 previous = xem_gte_ring->class_;
    u32 owner = xem_gte_ring->owner;
    u32 first;

    record.sprite = (u32)sprite;
    record.ot = (u32)ot;
    xem_capture_transform(&record.transform);
    xem_host_present(XEM_PRESENT_SPRITE_PARTS, &record, sizeof(record));
    xem_record_placement((u32)sprite, 0, 0);
    xem_gte_ring->owner = (u32)sprite;
    xem_record_ground(sprite);
    xem_gte_ring->class_ = XEM_PROJECTION_SPRITE;
    first = xem_gte_ring->count;
    if (cut == 1) {
        xem_original_sprite_draw_parts_cut_below(sprite, ot, height);
    } else if (cut == 2) {
        xem_original_sprite_draw_parts_cut_above(sprite, ot, height);
    } else {
        xem_original_sprite_draw_parts(sprite, ot);
    }
    xem_present_sprite_facing(sprite, first, xem_gte_ring->count, cut, height);
    xem_gte_ring->class_ = previous;
    xem_gte_ring->owner = owner;
}

void sprite_draw_parts(Sprite *sprite, u_long *ot) {
    xem_present_sprite_parts(sprite, ot, 0, 0);
}

void sprite_draw_parts_cut_below(Sprite *sprite, u_long *ot, s32 height) {
    xem_present_sprite_parts(sprite, ot, 1, height);
}

void sprite_draw_parts_cut_above(Sprite *sprite, u_long *ot, s32 height) {
    xem_present_sprite_parts(sprite, ot, 2, height);
}

/* The world map's overlay: its player marker is four triangles the GTE
 * turns with the camera, interface for the renderer (XEM_PROJECTION_INTERFACE
 * projections). */
void worldmap_map_overlay_draw(void) {
    u32 previous = xem_gte_ring->class_;

    xem_gte_ring->class_ = XEM_PROJECTION_INTERFACE;
    xem_original_worldmap_map_overlay_draw();
    xem_gte_ring->class_ = previous;
}

/* Damage and running-total glyphs use the GTE to animate screen-space
 * text. Their artificial depth (twice H, with rotation and scale) is not
 * scene depth: keep them out of postprocessing and focus measurements. */
void battle_popup_draw_glyph(SpritePart *glyph, s32 colour) {
    u32 previous = xem_gte_ring->class_;

    xem_gte_ring->class_ = XEM_PROJECTION_INTERFACE;
    xem_original_battle_popup_draw_glyph(glyph, colour);
    xem_gte_ring->class_ = previous;
}

/* ---------------------------------------------------------------- backdrops in the sides */

/* How far beyond the screen's sides the host wants the backdrops (pixels;
 * 0 for none), kept within the GPU's coordinates. */
static s32 xem_backdrop_reach(void) {
    u32 reach = xem_present_reveals->wanted & 0xFFFF;

    return !xem_reveals_wanted() ? 0 : reach > 512 ? 512 : (s32)reach;
}

/* Whether the untextured quad at `packet` spans the screen: a flat or
 * Gouraud quad (POLY_F4, POLY_G4) whose points lie at x 0, 320, 0, 320. */
static s32 xem_spans_screen(u32 packet) {
    u32 count = XEM_U32(packet) >> 24, code = XEM_U32(packet + 4) >> 24, step, k;

    if (!((code & 0xFC) == 0x28 && count == 5) && !((code & 0xFC) == 0x38 && count == 8)) {
        return 0;
    }
    step = count == 5 ? 1 : 2;
    for (k = 0; k < 4; k++) {
        if ((s16)(XEM_U32(packet + 8 + k * step * 4) & 0xFFFF) != (k % 2 ? 320 : 0)) {
            return 0;
        }
    }
    return 1;
}

/* Presentation only (xem/gte.h): the backdrop polygons just linked at
 * `entry` (since its word was `head`) that span the screen (the sky, fade
 * and ground fills, horizon bands) again on either side, `reach` wide with
 * the same rows and colours, as culled faces drawn just before each one:
 * over what the original draws before it and under what it draws after,
 * with the drawing mode it is drawn in (a translucent band's blending, set
 * by the textured polygons before it). */
static void xem_reveal_backdrop_fills(u32 entry, u32 head, s32 reach) {
    u32 spans[16], words[8], at = XEM_U32(entry) & 0x00FFFFFF, count, n = 0, walked, i, k, side;
    s32 x;

    for (walked = 0; at != (head & 0x00FFFFFF) && at != 0x00FFFFFF && walked < 256; walked++) {
        if (n < 16 && xem_spans_screen(at)) {
            spans[n++] = at;
        }
        at = XEM_U32(at) & 0x00FFFFFF;
    }
    while (n > 0) {
        at = spans[--n];
        count = XEM_U32(at) >> 24;
        for (side = 0; side < 2; side++) {
            for (k = 0; k < count; k++) {
                words[k] = XEM_U32(at + 4 + k * 4);
            }
            /* The points: every word after the first (F4), every other (G4). */
            for (k = 1; k < count; k += count == 5 ? 1 : 2) {
                x = (s16)(words[k] & 0xFFFF) == 0 ? (side ? 320 : -reach) : (side ? 320 + reach : 0);
                words[k] = (words[k] & 0xFFFF0000) | ((u32)x & 0xFFFF);
            }
            xem_present_reveal_at(at, 0, xem_gte_ring->count, 0, words, count);
        }
    }
}

/* One quad of a panorama's strip as gpu_draw_panorama_strip makes it, from
 * texture column `u` at screen column `x` (clipped at `end`): its GP0
 * words (code word `code`, palette `clut`) into `words`; returns how many
 * texture columns it covers, its width in pixels in `*x1`. */
static s32 xem_strip_quad(Panorama *panorama, s32 u, s32 x, s32 end, s32 bottom, s32 top, s32 zoom, u32 code, u32 clut,
                          u32 *words, s32 *x1) {
    s32 page_u = (panorama->tex_x % 64) << (2 - panorama->mode);
    s32 page_x = panorama->tex_x + (u >> (2 - panorama->mode));
    s32 cols = (u + page_u) & ((0x100 >> panorama->mode) - 1);
    s32 w = 0x100 - cols;
    u32 v0 = (u32)panorama->v & 0xFF, v2 = (u32)(panorama->v + panorama->height) & 0xFF;
    u32 tpage = getTPage(panorama->mode, 0, page_x / 64 * 64, panorama->tex_y / 256 * 256);

    if (u + w > panorama->width) {
        w = panorama->width - u;
    }
    *x1 = (w << 8) / (zoom + 0x100);
    if (x + *x1 > end) {
        *x1 = end - x;
        w = *x1 * (zoom + 0x100) / 256;
    }
    words[0] = code;
    words[1] = ((u32)x & 0xFFFF) | (u32)(bottom - top) << 16;
    words[2] = ((u32)cols & 0xFF) | v0 << 8 | clut << 16;
    words[3] = ((u32)(x + *x1) & 0xFFFF) | (u32)(bottom - top) << 16;
    words[4] = ((u32)(cols + w - 1) & 0xFF) | v0 << 8 | tpage << 16;
    words[5] = ((u32)x & 0xFFFF) | (u32)bottom << 16;
    words[6] = ((u32)cols & 0xFF) | v2 << 8;
    words[7] = ((u32)(x + *x1) & 0xFFFF) | (u32)bottom << 16;
    words[8] = ((u32)(cols + w - 1) & 0xFF) | v2 << 8;
    return w;
}

/* A panorama's strip: the original, then (presentation only, xem/gte.h)
 * the strip continued past both sides of the screen as far as the host's
 * reach, as culled faces at the strip's entry: from the column the
 * original ends at on the right, and on the left from as many columns
 * before the one it starts at. */
void gpu_draw_panorama_strip(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer) {
    POLY_FT4 *first = &panorama->quads[buffer & 1][0];
    u32 words[9], code, clut;
    s32 reach = xem_backdrop_reach(), left, u, top, x, x1, w, n;

    xem_original_gpu_draw_panorama_strip(panorama, start, bottom, zoom, ot, buffer);
    if (reach == 0 || panorama->width <= 0) {
        return;
    }
    left = (320 - (panorama->width << 8) / (zoom + 0x100)) / 2;
    left += left * zoom / 256;
    u = (s16)(start - left) % panorama->width;
    if (u < 0) {
        u += panorama->width;
    }
    if (bottom < 0 || panorama->height + 240 < bottom) {
        return;
    }
    top = (panorama->height << 8) / (zoom + 0x100);
    if (top <= 0) {
        return;
    }
    code = XEM_U32(XEM_ADDRESS_OF(first->r0));
    clut = first->clut;
    /* The left side, from as many columns before the first. */
    w = (reach * (zoom + 0x100) + 255) / 256;
    n = ((u - w) % panorama->width + panorama->width) % panorama->width;
    for (x = -(w * 256 / (zoom + 0x100)); x < 0;) {
        w = xem_strip_quad(panorama, n, x, 0, bottom, top, zoom, code, clut, words, &x1);
        if (x1 <= 0) {
            break;
        }
        xem_present_reveal(XEM_ADDRESS_OF(*ot), 0, xem_gte_ring->count, words, 9);
        n = (n + w) % panorama->width;
        x += x1;
    }
    /* The right side, from the column the original's last quad ends at. */
    for (x = 0; x < 320;) {
        w = xem_strip_quad(panorama, u, x, 320, bottom, top, zoom, code, clut, words, &x1);
        if (x1 <= 0) {
            return;
        }
        u = (s16)(u + w) % panorama->width;
        x += x1;
    }
    while (x < 320 + reach) {
        w = xem_strip_quad(panorama, u, x, 320 + reach, bottom, top, zoom, code, clut, words, &x1);
        if (x1 <= 0) {
            break;
        }
        xem_present_reveal(XEM_ADDRESS_OF(*ot), 0, xem_gte_ring->count, words, 9);
        u = (s16)(u + w) % panorama->width;
        x += x1;
    }
}

/* A panoramic backdrop: the original, then its fills on either side
 * (xem_reveal_backdrop_fills). */
s32 gpu_draw_panorama(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot, s32 buffer) {
    u32 head = (u32)*ot;
    s32 reach = xem_backdrop_reach(), result;

    result = xem_original_gpu_draw_panorama(panorama, eye, target, view, ot, buffer);
    if (reach != 0) {
        xem_reveal_backdrop_fills(XEM_ADDRESS_OF(*ot), head, reach);
    }
    return result;
}

/* The battle stage's sky: the original, its projections the backdrop's
 * (XEM_PROJECTION_BACKDROP: a view of the scene from elsewhere shows them
 * behind it), then its horizon bands and fills on either side
 * (xem_reveal_backdrop_fills). */
void battle_draw_stage_sky(void *sky, SVECTOR *eye, SVECTOR *target, MATRIX *view, u32 *ot, s32 buffer) {
    u32 head = *ot, previous = xem_gte_ring->class_;
    s32 reach = xem_backdrop_reach();

    xem_gte_ring->class_ = XEM_PROJECTION_BACKDROP;
    xem_original_battle_draw_stage_sky(sky, eye, target, view, ot, buffer);
    xem_gte_ring->class_ = previous;
    if (reach != 0) {
        xem_reveal_backdrop_fills(XEM_ADDRESS_OF(*ot), head, reach);
    }
}

/* ---------------------------------------------------------------- sprites the field skips */

/* Game memory a drawing for the record writes, kept in the capture area's
 * backup to be put back. */
#define XEM_SAVED 8

typedef struct {
    u32 address[XEM_SAVED], words[XEM_SAVED], start[XEM_SAVED];
    u32 count, used;
} XemSaved;

static s32 xem_save(XemSaved *saved, u32 address, u32 bytes) {
    u32 words = (bytes + 3) / 4, i;

    if (saved->count >= XEM_SAVED || saved->used + words > XEM_PRESENT_BACKUP / 4) {
        return 0;
    }
    for (i = 0; i < words; i++) {
        xem_present_capture->backup[saved->used + i] = XEM_U32(address + i * 4);
    }
    saved->address[saved->count] = address;
    saved->words[saved->count] = words;
    saved->start[saved->count++] = saved->used;
    saved->used += words;
    return 1;
}

/* Put back the regions saved from the `first` on, and forget them. */
static void xem_restore(XemSaved *saved, u32 first) {
    u32 r, i;

    for (r = first; r < saved->count; r++) {
        for (i = 0; i < saved->words[r]; i++) {
            XEM_U32(saved->address[r] + i * 4) = xem_present_capture->backup[saved->start[r] + i];
        }
    }
    if (first < saved->count) {
        saved->used = saved->start[first];
        saved->count = first;
    }
}

/* The projection that made the screen point stored at `address` since the
 * store ring's count was `stores` (1 + its index, 0 when none did). */
static u32 xem_stored_by(u32 address, u32 stores) {
    XemStoreRing *ring = xem_present_stores;
    XemSxyStore *s;
    u32 n;

    for (n = ring->count; n != stores && ring->count - n < XEM_PRESENT_STORES; n--) {
        s = &ring->entries[(n - 1) % XEM_PRESENT_STORES];
        if (s->address == (address & 0x00FFFFFF)) {
            return s->value == XEM_U32(address) ? s->source : 0;
        }
    }
    return 0;
}

/* The textured quads a drawing for the record linked into the ordering
 * table saved as region 0 (each entry whose word changed, from its new
 * head back to its old one), recorded oldest first as culled faces drawn
 * just before the packet they were linked in front of, when the GTE
 * projected their four points (since the store ring's count was
 * `stores`); the entries are put back. */
static void xem_reveal_linked(XemSaved *saved, u32 stores) {
    u32 packets[64], words[9], index[4], j, k, n, at, old, base, order, entry;

    for (j = 0; j < saved->words[0]; j++) {
        entry = saved->address[0] + j * 4;
        old = xem_present_capture->backup[saved->start[0] + j];
        if (XEM_U32(entry) == old) {
            continue;
        }
        n = 0;
        for (at = XEM_U32(entry) & 0x00FFFFFF; at != (old & 0x00FFFFFF) && at != 0x00FFFFFF && n < 64;) {
            packets[n++] = at;
            at = XEM_U32(at) & 0x00FFFFFF;
        }
        while (n > 0) {
            at = packets[--n];
            if (XEM_U32(at) >> 24 != 9 || (XEM_U32(at + 4) >> 24 & 0xFC) != 0x2C) {
                continue;
            }
            base = 0;
            for (k = 0; k < 4; k++) {
                index[k] = xem_stored_by(at + 8 + k * 8, stores);
                base = index[k] > base ? index[k] : base;
            }
            order = 0;
            for (k = 0; k < 4; k++) {
                if (index[k] == 0 || base - index[k] + 1 > 0xFF) {
                    break;
                }
                order |= (base - index[k] + 1) << (k * 8);
            }
            if (k < 4) {
                continue;
            }
            for (k = 0; k < 9; k++) {
                words[k] = XEM_U32(at + 4 + k * 4);
            }
            xem_present_reveal_at((old & 0x00FFFFFF) == 0x00FFFFFF ? entry : old, 4, base, order, words, 9);
        }
        XEM_U32(entry) = old;
    }
}

/* One sprite actor drawn for the record as field_draw_sprite_actors draws
 * it (from its scale on) into `ot`, `depth` its raised point's depth, with
 * the GTE as the pass leaves it after that point's projection; what the
 * drawing writes is put back. */
static void xem_record_sprite(XemSaved *saved, FieldActor *actor, Sprite *sprite, MATRIX *orient, s32 octant, s32 elevation,
                              s32 depth, u_long *ot) {
    SpriteRenderer *renderer = sprite->renderer;
    u32 parts = (u8)sprite->flags >> 2, queue = (u32)sprite_queue_next_free, stores = xem_present_stores->count, first = saved->count;
    u32 room = (u32)sprite_queue_block_end - queue, rgb;
    VECTOR scale;
    SVECTOR v;
    long sxy, interpolation, flag;
    s32 side, upper;

    if (!xem_save(saved, (u32)sprite, sizeof(Sprite)) || !xem_save(saved, (u32)renderer, sizeof(SpriteRenderer)) ||
        !xem_save(saved, (u32)renderer->parts[1], parts * sizeof(SpritePart)) ||
        !xem_save(saved, XEM_ADDRESS_OF(sprite_quad_corners[0]), sizeof(sprite_quad_corners)) ||
        !xem_save(saved, XEM_ADDRESS_OF(sprite_shadow_corners[0]), sizeof(sprite_shadow_corners)) ||
        !xem_save(saved, XEM_ADDRESS_OF(sprite_queue_next_free), 4) ||
        !xem_save(saved, queue, parts * sizeof(POLY_FT4) * 4 < room ? parts * sizeof(POLY_FT4) * 4 : room)) {
        xem_restore(saved, first);
        return;
    }
    scale.vx = actor->scale[0] * 3 >> 2;
    scale.vy = actor->scale[1] * 3 >> 2;
    scale.vz = actor->scale[2] * 3 >> 2;
    if (actor->unkE4 == 7 && field_work.unk2268 != 0) {
        scale.vx = scale.vx * 5 >> 2;
        scale.vy = scale.vy * 5 >> 2;
        scale.vz = scale.vz * 5 >> 2;
    }
    renderer->matrix = *orient;
    ScaleMatrix(&renderer->matrix, &scale);
    if (actor->floor_attribute & 0x200000) {
        side = (octant - (((actor->floor_attribute >> 11) - 2) & 7)) & 7;
        if (side != 0 && (side < 4 || side >= 5)) {
            v.vx = 0;
            v.vy = side < 4 ? -0x80 : 0x80;
            v.vz = 0;
            depth = RotTransPers(&v, &sxy, &interpolation, &flag);
        }
    }
    if (field_work.unk2357 == 0 && field_work.sprite_gate != 0) {
        xem_lwc2(XEM_GTE_RGBC, XEM_ADDRESS_OF(model_color)); /* gte_ldrgb, gte_dpcs, gte_strgb */
        xem_gte_execute(XEM_GTE_DPCS);
        rgb = xem_gte_read_data(XEM_GTE_RGB2);
        sprite_set_part_color(sprite, rgb & 0xFF, rgb >> 8 & 0xFF, rgb >> 16 & 0xFF);
    }
    depth >>= model_ot_depth_shift;
    if (depth >= 2) {
        depth -= 2;
    }
    if ((u16)(actor->unkE8 + 0x22) < 2) {
        sprite_set_part_color(sprite, actor->color0[0], actor->color0[1], actor->color0[2]);
        sprite->render.bytes[1] = 0xEF;
        sprite_draw(sprite, ot + depth - 0x10);
        v.vx = 0;
        v.vy = 300;
        v.vz = 0;
        upper = RotTransPers(&v, &sxy, &interpolation, &flag) >> model_ot_depth_shift;
        sprite_set_part_color(sprite, actor->color1[0], actor->color1[1], actor->color1[2]);
        sprite->render.bytes[1] = 0xF7;
        sprite_draw(sprite, ot + upper);
    } else {
        sprite->render.bytes[1] = 0;
        if (!(actor->unk134 & 0x60)) {
            field_sprite_set_color_unless_fog(sprite, actor->color0);
            sprite_draw(sprite, ot + depth);
        } else {
            if ((actor->unk134 >> 5) & 1) {
                field_sprite_set_color_unless_fog(sprite, actor->color0);
                v.vx = 0;
                v.vy = (actor->unkEE - elevation / 3) * 2;
                v.vz = 0;
                upper = RotTransPers(&v, &sxy, &interpolation, &flag) >> model_ot_depth_shift;
                if (upper >= 2) {
                    upper -= 2;
                }
                sprite_draw_cut_below(sprite, ot + upper, actor->unkEE);
            }
            if ((actor->unk134 >> 5) & 2) {
                field_sprite_set_color_unless_fog(sprite, actor->color1);
                sprite_draw_cut_above(sprite, ot + depth, actor->unkEE);
            }
        }
    }
    xem_reveal_linked(saved, stores);
    xem_restore(saved, first);
}

/* Presentation only (xem/gte.h), while a camera pulled back (or a view
 * looking around) wants culled faces: the field's sprite actors the
 * character pass (field.c field_draw_sprite_actors) leaves undrawn because
 * the projection of their raised point overflowed (`flag < 0`: far off the
 * original screen, or near or behind its camera) and that camera sees are
 * drawn again for the record into `ot`, as the pass draws them, everything
 * that writes (the sprite, its renderer and parts, the sprite queue, the
 * ordering table, the scratch corners, the GTE and the ring's bookkeeping)
 * put back; their parts become culled faces where they were linked.
 * Intentionally hidden actors stay hidden. */
void xem_present_skipped_sprites(u_long *ot) {
    XemProjectionRing *ring = xem_gte_ring;
    FieldDescriptor *d;
    XemProjection *p;
    XemSaved saved;
    XemGte gte;
    MATRIX orient, placed;
    SVECTOR raised;
    long sxy, interpolation, flag;
    u32 fifo[3], class_ = ring->class_, owner = ring->owner, i;
    s32 depth, elevation, octant;

    if (!xem_reveals_wanted() || !xem_present_elsewhere()) {
        return;
    }
    gte = xem_gte;
    for (i = 0; i < 3; i++) {
        fifo[i] = ring->fifo[i];
    }
    saved.count = saved.used = 0;
    elevation = (s16)field_view.elevation;
    octant = field_camera_get_octant() & 0xFFFF;
    field_matrix_copy_rotation(&orient, &field_view.orbit);
    raised.vx = 0;
    raised.vz = 0;
    raised.vy = -(elevation / 3 * 2);
    for (i = 0; i < (u32)field_event_actor_count; i++) {
        d = &field_view.components.descriptors[i];
        if (!(d->flags & 0x40) || (d->flags & 0x20) || mode_debug_hide_sprites != 0 || (d->actor->layer_flags & 0x02002000)) {
            continue;
        }
        CompMatrix(&field_view.scaled_world, &d->matrix, &placed);
        SetRotMatrix(&placed);
        SetTransMatrix(&placed);
        depth = RotTransPers(&raised, &sxy, &interpolation, &flag);
        p = &ring->entries[(ring->count - 1) % XEM_GTE_PROJECTIONS];
        if (flag >= 0 || !xem_present_square_visible(p->view[0], p->view[1], p->view[2], 0, 0x140, 0xE0, 0x27, 0x5A)) {
            continue;
        }
        /* The ordering table, and the entries the pass may link below it. */
        if (saved.count == 0 && !xem_save(&saved, (u32)(ot - 0x18), (0x18 + 0x1001) * 4)) {
            break;
        }
        ring->class_ = XEM_PROJECTION_OTHER;
        xem_record_sprite(&saved, d->actor, d->model, &orient, octant, elevation, depth, ot);
    }
    xem_restore(&saved, 0);
    xem_gte = gte;
    for (i = 0; i < 3; i++) {
        ring->fifo[i] = fifo[i];
    }
    ring->class_ = class_;
    ring->owner = owner;
}
