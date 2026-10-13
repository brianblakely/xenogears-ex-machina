/* Native fixture for the presentation frame-request wrapper. The test appends
 * the wrapper and the recovered request/control routines verbatim. Source
 * tables live below 2 GB so their original 32-bit pointer arithmetic applies. */
#include "common.h"
#include "resident/sprite.h"
#include "resident/heap.h"

#pragma clang diagnostic ignored "-Wpointer-to-int-cast"
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#pragma clang diagnostic ignored "-Wunused-variable"

static struct { u32 active; Sprite *pending; Sprite sprite; } facing_storage;
#define facing (&facing_storage)

Sprite *sprite_pending_list;
u8 sprite_effect_source[1], sprite_shared_source[1];
static Sprite live, head, saved_live;
static SpriteRenderer live_renderer, head_renderer, private_renderer;
static SpriteRenderer saved_renderer, saved_head;
static SpriteRendererEntry live_groups[8], private_groups[8], saved_groups[8];
static SpriteSource source;
static s32 allocations;

void xem_original_sprite_request_frame(Sprite *sprite, s32 frame);

void *heap_alloc(s32 size, s32 mode) {
    allocations++;
    return NULL;
}

static s32 equal_bytes(const void *a, const void *b, u32 count) {
    const u8 *left = a, *right = b;
    while (count--) { if (*left++ != *right++) { return 0; } }
    return 1;
}

void test_request_setup(void *frames, s32 clear) {
    u32 i;
    live = (Sprite){0};
    head = (Sprite){0};
    live_renderer = (SpriteRenderer){0};
    head_renderer = (SpriteRenderer){0};
    for (i = 0; i < 8; i++) {
        live_groups[i] = (SpriteRendererEntry){41, -42, 43, 44, 45};
        private_groups[i] = live_groups[i];
        saved_groups[i] = live_groups[i];
    }
    source.frames = frames;
    live.image = &source;
    live.frame = 3;
    live.render.word = 1;
    live.flags = 0x20000 | (clear ? 0x100000 : 0);
    live.renderer = &live_renderer;
    live_renderer.pointer34 = live_groups;
    head.renderer = &head_renderer;
    head_renderer.next_pending = &live;
    sprite_pending_list = &head;
    facing->sprite = live;
    private_renderer = live_renderer;
    private_renderer.pointer34 = private_groups;
    facing->sprite.renderer = &private_renderer;
    facing->pending = NULL;
    facing->active = 1;
    saved_live = live;
    saved_renderer = live_renderer;
    saved_head = head_renderer;
    allocations = 0;
}

void test_request(s32 frame) { sprite_request_frame(&facing->sprite, frame); }

s32 test_request_preserved_live_state(void) {
    return sprite_pending_list == &head &&
        equal_bytes(&live, &saved_live, sizeof(live)) &&
        equal_bytes(&live_renderer, &saved_renderer, sizeof(live_renderer)) &&
        equal_bytes(&head_renderer, &saved_head, sizeof(head_renderer)) &&
        equal_bytes(live_groups, saved_groups, sizeof(live_groups)) && !allocations;
}

s32 test_request_private_pending(void) {
    return facing->pending == &facing->sprite && private_renderer.next_pending == NULL &&
        (facing->sprite.flags & 0x20000) && !(facing->sprite.flags & 0x100000);
}

s32 test_request_group(s32 group, s32 component) {
    SpriteRendererEntry *entry = &private_groups[group];
    if (component == 0) { return entry->byte0; }
    if (component == 1) { return entry->byte1; }
    return entry->half6;
}

s32 test_request_frame(void) { return facing->sprite.frame; }
