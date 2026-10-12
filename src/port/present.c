/*
 * Presentation capture (docs/runtime.md, Presentation): read-only records of
 * the pre-projection data at the handbook's seams (docs/later-phases.md,
 * Presentation), so renderers can re-traverse a frame for other views without
 * running game draw code again. Each wrapper records, then runs the original
 * unchanged; nothing here writes game state.
 */
#include "common.h"
#include "xem/gte.h"
#include "xem/memory.h"
#include "resident/gpu.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "../decomp/src/field/field.h"
#include "../decomp/src/field/field_camera.h"
#include "../decomp/src/field/field_draw.h"

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
void xem_original_sprite_draw_parts(Sprite *sprite, u_long *ot);
void xem_original_sprite_draw_parts_cut_below(Sprite *sprite, u_long *ot, s32 height);
void xem_original_sprite_draw_parts_cut_above(Sprite *sprite, u_long *ot, s32 height);
void xem_original_worldmap_map_overlay_draw(void);
s32 xem_original_gpu_draw_panorama(Panorama *panorama, SVECTOR *eye, SVECTOR *target, MATRIX *view, u_long *ot, s32 buffer);
void xem_original_gpu_draw_panorama_strip(Panorama *panorama, s32 start, s32 bottom, s32 zoom, u_long *ot, s32 buffer);
void xem_original_battle_draw_stage_sky(void *sky, SVECTOR *eye, SVECTOR *target, MATRIX *view, u32 *ot, s32 buffer);

/* Whether the host wants the faces the game culls (xem/gte.h), and no model
 * is being drawn again for the record. */
static s32 xem_reveals_wanted(void) {
    return xem_present_reveals->wanted && !xem_present_capture->active;
}

/* Presentation only (xem/gte.h): a model the game did not draw because a
 * bounding test found it off its screen, drawn again for the record. With
 * the GTE as `placed` (what the drawing would have used), the box test off
 * and the renderers' links captured instead of made, its faces become
 * culled-face records at the entries they would have been linked at. Its
 * packets, the scratchpad words the renderers spill to, the renderers'
 * state, the GTE and the ring's bookkeeping are put back afterwards, so the
 * game sees nothing of it. */
static void xem_reveal_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode, XemGte *placed) {
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
 * matrix) reaches that camera's screen. */
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
        if (xem_present_reveals->pull_back != 0) {
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

/* The field's models: first, when the host wants culled faces (a view
 * wider or farther out than the original's), the depth of the camera's
 * target through the sprites' view (field_view.scaled_world) as an
 * XEM_PROJECTION_FOCUS entry of the ring: a view pulled back moves the
 * camera by multiples of it. Written to the ring only. */
void field_draw_models(void) {
    XemProjectionRing *ring = xem_gte_ring;
    XemProjection *p;
    MATRIX *m = &field_view.scaled_world;
    s32 x, y, z, depth;

    if (xem_reveals_wanted()) {
        x = field_view.target.vx >> 16;
        y = field_view.target.vy >> 16;
        z = field_view.target.vz >> 16;
        depth = ((m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12) + m->t[2];
        p = &ring->entries[ring->count % XEM_GTE_PROJECTIONS];
        p->sxy = 0;
        p->sz = (u32)(depth < 0 ? 0 : depth > 0xFFFF ? 0xFFFF : depth) | (u32)XEM_PROJECTION_FOCUS << 16;
        p->x = 0;
        p->y = 0;
        p->offset = ((u32)((s32)xem_gte_read_control(24) >> 16) & 0xFFFF) | (u32)((s32)xem_gte_read_control(25) >> 16) << 16;
        p->h = xem_gte_read_control(26) & 0xFFFF;
        p->owner = 0;
        p->view[0] = 0;
        p->view[1] = 0;
        p->view[2] = depth;
        ring->count++;
    }
    xem_original_field_draw_models();
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

    record.sprite = (u32)sprite;
    record.ot = (u32)ot;
    xem_capture_transform(&record.transform);
    xem_host_present(XEM_PRESENT_SPRITE_PARTS, &record, sizeof(record));
    xem_gte_ring->owner = (u32)sprite;
    xem_record_ground(sprite);
    xem_gte_ring->class_ = XEM_PROJECTION_SPRITE;
    if (cut == 1) {
        xem_original_sprite_draw_parts_cut_below(sprite, ot, height);
    } else if (cut == 2) {
        xem_original_sprite_draw_parts_cut_above(sprite, ot, height);
    } else {
        xem_original_sprite_draw_parts(sprite, ot);
    }
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
 * the same rows and colours, as culled faces at that entry, in the order
 * they were linked. */
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
            xem_present_reveal(entry, 0, xem_gte_ring->count, words, count);
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

/* The battle stage's sky: the original, then its horizon bands and fills
 * on either side (xem_reveal_backdrop_fills). */
void battle_draw_stage_sky(void *sky, SVECTOR *eye, SVECTOR *target, MATRIX *view, u32 *ot, s32 buffer) {
    u32 head = *ot;
    s32 reach = xem_backdrop_reach();

    xem_original_battle_draw_stage_sky(sky, eye, target, view, ot, buffer);
    if (reach != 0) {
        xem_reveal_backdrop_fills(XEM_ADDRESS_OF(*ot), head, reach);
    }
}
