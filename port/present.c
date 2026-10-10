/*
 * Presentation capture (docs/runtime.md, Presentation): read-only records of
 * the pre-projection data at the handbook's seams (docs/later-phases.md,
 * Presentation), so renderers can re-traverse a frame for other views without
 * running game draw code again. Each wrapper records, then runs the original
 * unchanged; nothing here writes game state.
 */
#include "common.h"
#include "xem/gte.h"
#include "resident/model.h"
#include "resident/sprite.h"

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
void xem_original_sprite_set_draw_matrix(Sprite *sprite);
void xem_original_sprite_draw_parts(Sprite *sprite, u_long *ot);

/* A model drawn with the transform its caller left in the GTE: the mesh (in
 * game memory), its packets, the ordering table and the sort mode. */
s32 model_draw_sprite_model(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode) {
    struct {
        u32 model, packets, ot;
        s32 mode;
        XemTransform transform;
    } record;

    record.model = (u32)model;
    record.packets = (u32)packets;
    record.ot = (u32)ot;
    record.mode = mode;
    xem_capture_transform(&record.transform);
    xem_host_present(XEM_PRESENT_MODEL, &record, sizeof(record));
    return xem_original_model_draw_sprite_model(model, packets, ot, mode);
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

/* A sprite's parts, projected with the transform in the GTE at the call. */
void sprite_draw_parts(Sprite *sprite, u_long *ot) {
    struct {
        u32 sprite, ot;
        XemTransform transform;
    } record;

    record.sprite = (u32)sprite;
    record.ot = (u32)ot;
    xem_capture_transform(&record.transform);
    xem_host_present(XEM_PRESENT_SPRITE_PARTS, &record, sizeof(record));
    xem_original_sprite_draw_parts(sprite, ot);
}
