/* Alternatives of a field sprite for presentation: its first-person facing,
 * and a party character's full-size frame in a large-scale field. Run the
 * recovered facing, frame assembly and drawing routines on a copy, with
 * private parts, upload records and packets. Nothing reaches the game's
 * pending list, heap or GPU. The host attaches each alternative to the
 * original draw's projections. */
#include "common.h"
#include "xem/gte.h"
#include "xem/gte_ops.h"
#include "resident/gpu.h"
#include "resident/sprite.h"
#include "../decomp/src/field/field.h"
#include "../decomp/src/field/field_camera.h"
#include "../decomp/src/field/field_party.h"
#include "../decomp/src/field/field_resident.h"

void xem_host_present(u32 kind, void *data, u32 length);
/* The field sprite block of `character` (directory 4, file character + 5,
 * unpacked) where the host keeps it, 0 while it has none. */
u32 xem_host_full_sprite(u32 character);
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
    /* Nonzero while the field's actor pass draws (activity.c). */
    u32 field_actors;
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
    /* A full-size frame's source: the character's block, its own texture
     * and palette places. */
    SpriteResource source;
} FacingArea;

/* Beside the other presentation rings, outside original RAM and snapshots. */
#ifdef __psx__
#define facing ((FacingArea *)0x70B00000u)
#else
static FacingArea facing_storage;
#define facing (&facing_storage)
#endif

/* Record kinds (src/runtime/crates/xem-core/src/presentation.rs). */
#define XEM_PRESENT_SPRITE_FACING 4
#define XEM_PRESENT_SPRITE_FULL_SIZE 5

s32 xem_present_sprite_facing_active(void) { return facing->active; }

void xem_present_field_actors(u32 drawing) { facing->field_actors = drawing; }

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

/* The field descriptor whose model is `sprite`, NULL for none. */
static FieldDescriptor *xem_sprite_descriptor(Sprite *sprite) {
    FieldDescriptor *d;
    u32 i;

    for (i = 0; i < (u32)field_event_actor_count; i++) {
        d = &field_view.components.descriptors[i];
        if ((d->flags & 0x40) && d->model == sprite) { return d; }
    }
    return NULL;
}

/* Copy `sprite` into the area, with its renderer, its parts (and its group
 * entries when `groups`, else none), the GTE state of the original's draw
 * kept to put back. The copy's frame requests and uploads stay private. */
static void xem_alternative_begin(Sprite *sprite, s32 groups) {
    u32 i;

    facing->sprite = *sprite;
    facing->renderer = *sprite->renderer;
    facing->sprite.renderer = &facing->renderer;
    facing->renderer.parts[0] = facing->renderer.parts[1] = facing->parts;
    for (i = 0; i < sizeof(facing->parts) / sizeof(u32); i++) { ((u32 *)facing->parts)[i] = 0; }
    for (i = 0; i < ((sprite->flags >> 2) & 0x3F); i++) {
        facing->parts[i] = sprite->renderer->parts[1][i];
    }
    for (i = 0; i < 8; i++) {
        if (groups && sprite->renderer->pointer34) { facing->groups[i] = sprite->renderer->pointer34[i]; }
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
}

/* Draw the copy as the original drew `sprite` (`cut` 0 whole, 1 cut below
 * `height`, 2 cut above), into private packets; hand the host its parts
 * with their projections and its uploads as record `kind`, for the
 * original's projections `first` to `last`; put the GTE state back. */
static void xem_alternative_end(Sprite *sprite, u32 first, u32 last, s32 cut, s32 height, u32 kind) {
    SpriteQueueEntry *queue;
    u8 *end;
    u32 i, j, stores, n, value, address, link;
    s32 bucket;
    POLY_FT4 *packet;
    XemSxyStore *store;
    FacingPart *part;

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
    xem_host_present(kind, &facing->record, sizeof(facing->record));
}

void xem_present_sprite_facing(Sprite *sprite, u32 first, u32 last, s32 cut, s32 height) {
    FieldDescriptor *d, *eye;
    s32 angle;

    if (!xem_gte_ring->recording || !(xem_present_reveals->around & XEM_AROUND_LOOK) || facing->active ||
        sprite_in_battle || sprite_in_worldmap || !sprite->animations || !sprite->animation ||
        (sprite->render.word & 3) != 1 || field_work.controlled >= field_event_actor_count) {
        return;
    }
    d = xem_sprite_descriptor(sprite);
    if (d == NULL || (d->actor->layer_flags & 0x01000000)) { return; }
    eye = &field_view.components.descriptors[field_work.controlled];
    if (d == eye) { return; }
    /* The original adds view_angle to the actor's heading. In first person
     * the view ray runs from the controlled actor to this sprite. */
    angle = ratan2((sprite->z >> 16) - eye->matrix.t[2],
                   (sprite->x >> 16) - eye->matrix.t[0]) - 0x400 + d->actor->unk108;
    xem_alternative_begin(sprite, 1);
    sprite_set_facing(&facing->sprite, angle);
    /* A replay can end at the same frame after changing inherited group
     * offsets. Only an unchanged facing step skipped that replay. */
    if (facing->sprite.frame_bits.step == sprite->frame_bits.step && facing->sprite.render.bits.flip == sprite->render.bits.flip) {
        facing->active = 0;
        return;
    }
    if (facing->sprite.frame) { sprite_build_frame(&facing->sprite, facing->sprite.frame, facing->sprite.image); }
    else { facing->sprite.flags &= ~0xFC; }
    xem_alternative_end(sprite, first, last, cut, height, XEM_PRESENT_SPRITE_FACING);
}

/* The facing table `k` of animation `animation` of a sprite block (its
 * section 1, the animation directory), NULL unless the animation has the
 * five tables of eight facing groups. */
static u16 *xem_facing_table(s32 *block, u32 animation, u32 k) {
    u16 *directory = (u16 *)((u8 *)block + block[1]);
    u16 *header;

    if (animation >= (directory[0] & 0x3F)) { return NULL; }
    header = (u16 *)((u8 *)directory + directory[animation + 1]);
    if ((header[0] & 3) != 2) { return NULL; }
    return (u16 *)((u8 *)&header[k + 2] + header[k + 2]);
}

/* The frame of `full` that shows what frame `frame` of `tiny` shows, while
 * `tiny` plays animation `playing`: the small sets have the standing (0),
 * walking (1) and running (2) animations, each facing's frames in a run
 * from its table's first (the jump shows the running frames, the others
 * the standing ones); the frame takes the same place in the run of the
 * full set's same animation and facing, whose runs can be longer. The
 * animation playing is looked in first. 0 for none. */
static u32 xem_full_size_frame(s32 *tiny, s32 *full, u32 frame, u32 playing) {
    u16 *tables[2][5];
    u32 animation, k, n, first, length, full_length, count;

    count = *(u16 *)((u8 *)full + full[2]) & 0x1FF;
    playing = playing < 3 ? playing : playing == 3 ? 2 : 0;
    for (n = 0; n < 4; n++) {
        animation = n == 0 ? playing : n - 1;
        if (n != 0 && animation == playing) { continue; }
        for (k = 0; k < 5; k++) {
            tables[0][k] = xem_facing_table(tiny, animation, k);
            tables[1][k] = xem_facing_table(full, animation, k);
            if (tables[0][k] == NULL || tables[1][k] == NULL) { break; }
        }
        if (k < 5) { continue; }
        length = (tables[0][1][0] & 0x1FF) - (tables[0][0][0] & 0x1FF);
        full_length = (tables[1][1][0] & 0x1FF) - (tables[1][0][0] & 0x1FF);
        if ((s32)length <= 0 || length > 32) { length = 1; }
        if ((s32)full_length <= 0 || full_length > 32) { full_length = 1; }
        for (k = 0; k < 5; k++) {
            first = tables[0][k][0] & 0x1FF;
            if (frame >= first && frame < first + length) {
                frame = (tables[1][k][0] & 0x1FF) + (frame - first) * full_length / length;
                return frame >= 1 && frame <= count ? frame : 0;
            }
        }
    }
    return 0;
}

void xem_present_sprite_full_size(Sprite *sprite, u32 first, u32 last, s32 cut, s32 height) {
    FieldDescriptor *d;
    s32 *tiny, *full;
    s32 *sprites;
    s32 x, y;
    u32 character, index, frame;

    /* First person keeps the small sprites, which it faces to the eyes. */
    if (!xem_gte_ring->recording || !facing->field_actors || facing->active || sprite_in_battle || sprite_in_worldmap ||
        (xem_present_reveals->around & XEM_AROUND_LOOK) ||
        field_work.unk2268 == 0 || sprite->frame == 0 || !sprite->animations ||
        sprite->animations != sprite->resource_block || (sprite->render.word & 3) != 1) {
        return;
    }
    /* A party character's sprite from the map's alternate set (event 16). */
    d = xem_sprite_descriptor(sprite);
    if (d == NULL) { return; }
    character = (u32)d->actor->unkE4;
    if (character >= 11) { return; }
    index = field_character_sprite_ids[character] + field_work.unk2268;
    sprites = field_view.components.sprites;
    tiny = (s32 *)((u8 *)sprites + sprites[index + 1]);
    if (d->actor->unk126 != (index | 0x80) || sprite->animations != tiny ||
        !(*(u16 *)((u8 *)tiny + tiny[2]) & 0x8000)) {
        return;
    }
    full = (s32 *)xem_host_full_sprite(character);
    if (full == NULL || (*(u16 *)((u8 *)full + full[2]) & 0x8000)) { return; }
    frame = xem_full_size_frame(tiny, full, sprite->frame, sprite->motion.bytes[3]);
    if (frame == 0) { return; }
    xem_alternative_begin(sprite, 0);
    /* Its cells and palette where the original's party sprite keeps them
     * (field_actor_create_sprite's slot 0: VRAM 0x280,0x100, palette
     * 0x100,0xE0), uploaded only to the host's picture, which puts back
     * what it covers once the frame is drawn. */
    facing->source.section1 = (u16 *)((u8 *)full + full[1]);
    facing->source.section2 = (u8 *)full + full[2];
    facing->source.section3 = (u8 *)full + full[3];
    facing->source.origin.vx = 0x280;
    facing->source.origin.vy = 0x100;
    facing->source.origin.vz = 0x100;
    facing->source.origin.pad = 0xE0;
    facing->sprite.image = &facing->source;
    facing->sprite.resource_block = facing->sprite.animations = full;
    /* A quarter of its size, as the small art: scale shift 1 for the
     * original's 3, the screen offset in the full art's pixels. */
    facing->sprite.flags = (facing->sprite.flags & ~0x1F00) | 0x100;
    x = sprite->renderer->offset.x * 4;
    y = sprite->renderer->offset.y * 4;
    facing->renderer.offset.x = (s8)(x < -128 ? -128 : x > 127 ? 127 : x);
    facing->renderer.offset.y = (s8)(y < -128 ? -128 : y > 127 ? 127 : y);
    facing->sprite.render.word |= 0x40000000;
    sprite_build_frame(&facing->sprite, frame, (SpriteSource *)&facing->source);
    xem_alternative_end(sprite, first, last, cut, height * 4, XEM_PRESENT_SPRITE_FULL_SIZE);
}
