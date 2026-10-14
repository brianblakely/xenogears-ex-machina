/* First-person sprite facings for presentation. Run the recovered facing,
 * frame assembly and drawing routines on a copy, with private parts, upload
 * records and packets. Nothing reaches the game's pending list, heap or GPU.
 * The host attaches this alternative to the original draw's projections. */
#include "common.h"
#include "xem/gte.h"
#include "xem/gte_ops.h"
#include "resident/gpu.h"
#include "resident/sprite.h"
#include "../decomp/src/field/field.h"
#include "../decomp/src/field/field_camera.h"
#include "../decomp/src/field/field_resident.h"

void xem_host_present(u32 kind, void *data, u32 length);
void xem_original_sprite_request_frame(Sprite *sprite, s32 frame);
void xem_original_sprite_queue_upload(u_long *pixels, s16 x, s16 y, s16 w, s16 h);
DVECTOR xem_original_sprite_reserve_texture_columns(s32 width);
void xem_original_sprite_set_draw_matrix(Sprite *sprite);
void xem_original_sprite_draw_parts(Sprite *sprite, u_long *ot);
void xem_original_sprite_draw_parts_cut_below(Sprite *sprite, u_long *ot, s32 height);
void xem_original_sprite_draw_parts_cut_above(Sprite *sprite, u_long *ot, s32 height);

typedef struct { u32 pixels; s16 x, y, w, h; } FacingUpload;
typedef struct { u32 words[9]; XemProjection points[4]; } FacingPart;
typedef struct {
    u32 owner, first, last, part_count, parts, upload_count, uploads;
} FacingRecord;
typedef struct {
    u32 active;
    Sprite *pending;
    Sprite sprite;
    SpriteRenderer renderer;
    SpriteRendererEntry groups[8];
    SpritePart parts[63];
    POLY_FT4 packets[64];
    FacingPart drawn[63];
    FacingUpload uploads[65];
    FacingRecord record;
    u_long ot[9];
    XemGte gte;
    SVECTOR corners[4];
    u32 fifo[3];
} FacingArea;

/* Beside the other presentation rings, outside original RAM and snapshots. */
#ifdef __psx__
#define facing ((FacingArea *)0x70B00000u)
#else
static FacingArea facing_storage;
#define facing (&facing_storage)
#endif

s32 xem_present_sprite_facing_active(void) { return facing->active; }

void sprite_request_frame(Sprite *sprite, s32 frame) {
    if (facing->active && sprite == &facing->sprite) {
        Sprite *pending = sprite_pending_list;

        /* Facing replay can request several frames. The original applies
         * each preceding frame's group controls before replacing it, and
         * clears groups when the animation requested that. Keep its exact
         * semantics on our copy, with its own pending-list head. */
        sprite_pending_list = facing->pending;
        xem_original_sprite_request_frame(sprite, frame);
        facing->pending = sprite_pending_list;
        sprite_pending_list = pending;
    } else {
        xem_original_sprite_request_frame(sprite, frame);
    }
}

void sprite_queue_upload(u_long *pixels, s16 x, s16 y, s16 w, s16 h) {
    if (facing->active) {
        if (facing->record.upload_count < 65) {
            FacingUpload *u = &facing->uploads[facing->record.upload_count++];
            u->pixels = (u32)pixels;
            u->x = x; u->y = y; u->w = w; u->h = h;
        }
    } else {
        xem_original_sprite_queue_upload(pixels, x, y, w, h);
    }
}

DVECTOR sprite_reserve_texture_columns(s32 width) {
    if (facing->active) {
        return ((SpriteSource *)facing->sprite.image)->origin;
    }
    return xem_original_sprite_reserve_texture_columns(width);
}

void xem_present_sprite_facing(Sprite *sprite, u32 first, u32 last, s32 cut, s32 height) {
    FieldDescriptor *d, *eye;
    SpriteQueueEntry *queue;
    u8 *end;
    u32 i, j, stores, n, value, address, link;
    s32 angle, bucket;
    POLY_FT4 *packet;
    XemSxyStore *store;
    FacingPart *part;

    if (!xem_gte_ring->recording || !(xem_present_reveals->around & XEM_AROUND_LOOK) || facing->active ||
        sprite_in_battle || sprite_in_worldmap || !sprite->animations || !sprite->animation ||
        (sprite->render.word & 3) != 1 || field_work.controlled >= field_event_actor_count) {
        return;
    }
    for (i = 0; i < (u32)field_event_actor_count; i++) {
        d = &field_view.components.descriptors[i];
        if ((d->flags & 0x40) && d->model == sprite) { break; }
    }
    if (i == (u32)field_event_actor_count || (d->actor->layer_flags & 0x01000000)) { return; }
    eye = &field_view.components.descriptors[field_work.controlled];
    if (d == eye) { return; }
    /* The original adds view_angle to the actor's heading. In first person
     * the view ray runs from the controlled actor to this sprite. */
    angle = ratan2((sprite->z >> 16) - eye->matrix.t[2],
                   (sprite->x >> 16) - eye->matrix.t[0]) - 0x400 + d->actor->unk108;
    facing->sprite = *sprite;
    facing->renderer = *sprite->renderer;
    facing->sprite.renderer = &facing->renderer;
    facing->renderer.parts[0] = facing->renderer.parts[1] = facing->parts;
    for (i = 0; i < sizeof(facing->parts) / sizeof(u32); i++) { ((u32 *)facing->parts)[i] = 0; }
    for (i = 0; i < ((sprite->flags >> 2) & 0x3F); i++) {
        facing->parts[i] = sprite->renderer->parts[1][i];
    }
    for (i = 0; i < 8; i++) {
        if (sprite->renderer->pointer34) { facing->groups[i] = sprite->renderer->pointer34[i]; }
        else {
            facing->groups[i].byte0 = facing->groups[i].byte1 = 0;
            facing->groups[i].half2 = facing->groups[i].half4 = facing->groups[i].half6 = 0;
        }
    }
    facing->renderer.pointer34 = facing->groups;
    facing->gte = xem_gte;
    for (i = 0; i < 4; i++) { facing->corners[i] = sprite_quad_corners[i]; }
    for (i = 0; i < 3; i++) { facing->fifo[i] = xem_gte_ring->fifo[i]; }
    facing->record.upload_count = 0;
    facing->pending = NULL;
    facing->active = 1;
    sprite_set_facing(&facing->sprite, angle);
    /* A replay can end at the same frame after changing inherited group
     * offsets. Only an unchanged facing step skipped that replay. */
    if (facing->sprite.frame_bits.step == sprite->frame_bits.step && facing->sprite.render.bits.flip == sprite->render.bits.flip) {
        facing->active = 0;
        return;
    }
    if (facing->sprite.frame) { sprite_build_frame(&facing->sprite, facing->sprite.frame, facing->sprite.image); }
    else { facing->sprite.flags &= ~0xFC; }
    queue = sprite_queue_next_free;
    end = sprite_queue_block_end;
    sprite_queue_next_free = (SpriteQueueEntry *)facing->packets;
    sprite_queue_block_end = (u8 *)(facing->packets + 64);
    for (i = 0; i < 9; i++) { facing->ot[i] = 0xFFFFFF; }
    stores = xem_present_stores->count;
    xem_present_stores->private_begin = (u32)facing->packets;
    xem_present_stores->private_end = (u32)(facing->packets + 64);
    xem_original_sprite_set_draw_matrix(&facing->sprite);
    if (cut == 1) { xem_original_sprite_draw_parts_cut_below(&facing->sprite, facing->ot + 8, height); }
    else if (cut == 2) { xem_original_sprite_draw_parts_cut_above(&facing->sprite, facing->ot + 8, height); }
    else { xem_original_sprite_draw_parts(&facing->sprite, facing->ot + 8); }
    facing->record.part_count = 0;
    /* Follow the same bucket order and per-bucket LIFO links as the GPU.
     * Grouped parts can occupy different buckets; clipped parts can have
     * allocated a packet without ever linking it. */
    for (bucket = 8; bucket >= 0; bucket--) {
      for (link = facing->ot[bucket] & 0xFFFFFF; link != 0xFFFFFF && facing->record.part_count < 63;) {
        address = ((u32)facing->packets & 0xFF000000) | link;
        if (address < (u32)facing->packets || address >= (u32)sprite_queue_next_free ||
            (address - (u32)facing->packets) % sizeof(POLY_FT4)) { break; }
        packet = (POLY_FT4 *)address;
        link = packet->tag & 0xFFFFFF;
        part = &facing->drawn[facing->record.part_count];
        for (i = 0; i < 9; i++) { part->words[i] = ((u32 *)packet)[i + 1]; }
        for (j = 0; j < 4; j++) {
            address = (u32)packet + 8 + j * 8;
            value = ((u32 *)packet)[2 + j * 2];
            for (n = xem_present_stores->count; n != stores; n--) {
                store = &xem_present_stores->entries[(n - 1) % XEM_PRESENT_STORES];
                if (store->address == (address & 0xFFFFFF) && store->value == value && store->source) {
                    part->points[j] = xem_gte_ring->entries[(store->source - 1) % XEM_GTE_PROJECTIONS];
                    break;
                }
            }
            if (n == stores) { break; }
        }
        if (j == 4) { facing->record.part_count++; }
      }
    }
    xem_present_stores->private_begin = xem_present_stores->private_end = 0;
    sprite_queue_next_free = queue;
    sprite_queue_block_end = end;
    for (i = 0; i < 4; i++) { sprite_quad_corners[i] = facing->corners[i]; }
    for (i = 0; i < 3; i++) { xem_gte_ring->fifo[i] = facing->fifo[i]; }
    xem_gte = facing->gte;
    facing->active = 0;
    facing->record.owner = (u32)sprite;
    facing->record.first = first;
    facing->record.last = last;
    facing->record.parts = (u32)facing->drawn;
    facing->record.uploads = (u32)facing->uploads;
    xem_host_present(4, &facing->record, sizeof(facing->record));
}
