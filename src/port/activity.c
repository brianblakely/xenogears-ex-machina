/*
 * A host option that changes the game (docs/runtime.md, Actors in a wider
 * view): the field's character pass marks each sprite actor whose raised
 * point projects outside the original screen (x -39..359, y -9..313) as off
 * screen (layer_flags 0x200), and an off-screen actor stops moving, turning
 * and animating (docs/later-phases.md, Presentation: Projection feeds
 * gameplay). While the host's view shows more than the original's (zoomed
 * out or wider, its xem_host_actor_view), an actor that view still shows is
 * marked on screen again, judged as its camera sees it. The host records
 * the view with the game's inputs; without one (0) the game runs as the
 * original.
 */
#include "common.h"
#include "xem/gte.h"
#include "../decomp/src/field/field.h"

/* The host's view: the field camera's pull-back in percent of its target's
 * depth plus 100 (bits 0-15, 100 none), and the pixels it shows beyond
 * each side of the original screen (bits 16-31); 0 for the original's. */
u32 xem_host_actor_view(void);

void xem_original_field_draw_sprite_actors(u_long *ot, s32 buffer);
void xem_present_skipped_sprites(u_long *ot);

/* `m` times `n` (rotations 4.12, translations whole), as CompMatrix forms
 * them, in plain arithmetic: row `i`'s second column and translation. */
static void xem_actor_place(MATRIX *m, MATRIX *n, s32 i, long long *column, long long *translation) {
    s32 k;

    *column = 0;
    *translation = 0;
    for (k = 0; k < 3; k++) {
        *column += (long long)m->m[i][k] * n->m[k][1];
        *translation += (long long)m->m[i][k] * n->t[k];
    }
    *column >>= 12;
    *translation = (*translation >> 12) + m->t[i];
}

/* The actors the pass marked off screen that the host's `view` shows are
 * marked on screen again. */
static void xem_keep_actors_in_view(u32 view) {
    MATRIX *world = &field_view.scaled_world;
    FieldDescriptor *descriptor;
    FieldActor *actor;
    long long point[3], column, translation, h, back, target, depth, x, y;
    s32 raised, side, zoom, i, k;

    zoom = (s32)(view & 0xFFFF);
    side = (s32)(view >> 16);
    /* The pull-back: the zoom's share of the depth of the camera's target
     * (as present.c records it for the renderer). */
    x = field_view.target.vx >> 16;
    y = field_view.target.vy >> 16;
    target = ((world->m[2][0] * x + world->m[2][1] * y + world->m[2][2] * (field_view.target.vz >> 16)) >> 12) + world->t[2];
    back = zoom > 100 && target > 0 ? target * (zoom - 100) / 100 : 0;
    h = (long long)(xem_gte_read_control(26) & 0xFFFF);
    raised = -((s16)field_view.elevation / 3 * 2);
    for (i = 0; i < field_event_actor_count; i++) {
        descriptor = &field_view.components.descriptors[i];
        actor = descriptor->actor;
        if (!(descriptor->flags & 0x40) || (actor->layer_flags & 0x2200) != 0x200) {
            continue;
        }
        /* The raised point the original projects, in view space. */
        for (k = 0; k < 3; k++) {
            xem_actor_place(world, &descriptor->matrix, k, &column, &translation);
            point[k] = (column * raised >> 12) + translation;
        }
        depth = point[2] + back;
        if (depth * 2 <= h) {
            continue;
        }
        x = ((s32)xem_gte_read_control(24) >> 16) + point[0] * h / depth;
        y = ((s32)xem_gte_read_control(25) >> 16) + point[1] * h / depth;
        if (x >= -0x27 - side && x < 0x168 + side && y >= -9 && y < 0x13A) {
            actor->layer_flags &= ~0x200;
        }
    }
}

/* The original pass, the actors the host's view shows kept moving, and
 * (presentation only, present.c) the sprites it left undrawn for overflow
 * that a pulled-back camera sees drawn for the record. */
void field_draw_sprite_actors(u_long *ot, s32 buffer) {
    u32 view;

    xem_original_field_draw_sprite_actors(ot, buffer);
    view = xem_host_actor_view();
    if (view != 0) {
        xem_keep_actors_in_view(view);
    }
    xem_present_skipped_sprites(ot);
}
