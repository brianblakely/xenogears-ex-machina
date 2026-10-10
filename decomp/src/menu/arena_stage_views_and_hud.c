/* arena_stage_views_and_hud: text 80081ECC-80088BFC, rodata 800701B0-80070284, data
 * 8009178C-80091964, variables 80092768-800927F0 and 80095580-80096D88.
 * The arena (the backdrop, ground heights, stage colours, floor, wall,
 * shadows and map), the idle and pair cameras, the drawing of the 3D
 * views, the actors' model setup, the menu task (arena_mode_task) and its
 * exits, the HUD and map overlay, debug lines and path markers, vector
 * helpers, and the progress flags and option settings kept in the game
 * data. Its jump tables lie at 0 mod 8 (800701C0, 80070260); its first
 * function is the first reading its variables, and its data opens with
 * the stage colours arena_stage_color_table, which only its arena_stage_apply_colors reads. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "resident/window.h"
#include "actor.h"
#include "bout.h"
#include "brain.h"
#include "camera.h"
#include "debug.h"
#include "display.h"
#include "effects.h"
#include "glow.h"
#include "helpers.h"
#include "hud.h"
#include "menus.h"
#include "mode.h"
#include "node.h"
#include "resident_views.h"
#include "script.h"
#include "select.h"
#include "sound.h"
#include "stage.h"
#include "task.h"
#include "text.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static SVECTOR arena_stage_ground_normal; /* 80092768: stored map position */
static s32 arena_camera_radius; /* 80092770 */
static s32 arena_camera_pair_lift; /* 80092774 */
static s32 arena_camera_unused_word; /* 80092778: unreferenced */
static s32 arena_camera_unread_pair_heading; /* 8009277C: framing heading */
static s16 arena_mode_fade_level; /* 80092780: fade level */
static s32 arena_mode_glow_in_vblank; /* 80092784 */
static POLY_FT4 *arena_stage_wall_quads[2]; /* 80092788: floor quad pools: template, working copy */
static s32 arena_mode_state; /* 80092790 */
static s32 arena_mode_next_state; /* 80092794: scene mode */
static s32 arena_mode_first_model_id; /* 80092798: first actor's model id */
static s32 arena_mode_second_model_id; /* 8009279C: second actor's model id */
static u16 arena_stage_wall_clut; /* 800927A0: floor palette */
static u16 arena_stage_wall_tpage; /* 800927A4: floor texture page */
static u16 arena_stage_wall_texture_v; /* 800927A8: floor texture row */
static s32 arena_camera_orbit_angle; /* 800927AC: orbit angle */
static s32 arena_camera_orbit_speed; /* 800927B0: orbit speed */
static void *arena_actor_model_files[2]; /* 800927B4: loaded model of each actor slot */
static s32 arena_unused_pair_after_model_files[2]; /* 800927BC: unreferenced */
static SoundBank *arena_mode_sound_bank; /* 800927C4 */
static s32 arena_mode_unused_word; /* 800927C8: unreferenced */
static u8 *arena_stage_span_right_columns; /* 800927CC: per map row: right edge of the drawn span */
static u8 *arena_stage_span_left_columns; /* 800927D0: per map row: left edge of the drawn span */
static u16 arena_stage_shadow_tpage; /* 800927D4: backdrop texture page */
static u16 arena_stage_shadow_clut; /* 800927D8: backdrop palette */
static u8 arena_stage_shadow_u; /* 800927DC: backdrop texel u */
static u8 arena_stage_shadow_v; /* 800927E0: backdrop texel v */
static s32 arena_stage_unused_word_pair[2]; /* 800927E4: unreferenced */
static u8 arena_progress_unlock_added; /* 800927EC */

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk). Nothing addresses the words
 * marked unreferenced; each is the size of one more per-buffer pair of the
 * array before it. */
static POLY_G4 arena_stage_sky_gradients[2]; /* 80095580: sky gradient, one per buffer */
static DR_TPAGE arena_stage_backdrop_tpages[4]; /* 800955C8: backdrop texture pages: two, one per buffer each */
static s32 arena_stage_unused_tpage_pair[4]; /* 800955E8: unreferenced */
static SPRT arena_stage_backdrop_sprites[6]; /* 800955F8: backdrop sprites: three parts, one per buffer each */
static s32 arena_stage_unused_sprite_pair[10]; /* 80095670: unreferenced */
static Hud arena_hud_packets; /* 80095698 */
static DR_TPAGE arena_hud_tpages[4]; /* 80095918: HUD texture page modes, two per buffer */
static Line3D arena_debug_lines[100]; /* 80095938 */

/* Stage colours, read by arena_stage_apply_colors alone. */
Environment arena_stage_color_table[] = { /* 8009178C */
    { { 0x10, 0x60, 0x80 }, 0, 0x38, 0x38, 0x38, 0, { 0x70, 0x70, 0x70 }, 0, { 0x80, 0x80, 0x80 }, 0, 1 },
    { { 0x30, 0x60, 0x40 }, 0, 0x40, 0x40, 0x40, 0, { 0xE0, 0xB0, 0x70 }, 0, { 0x90, 0x90, 0x90 }, 0, 1 },
    { { 0x08, 0x30, 0x3F }, 0, 0x20, 0x20, 0x30, 0, { 0x40, 0x40, 0x50 }, 0, { 0x30, 0x30, 0x38 }, 0, 0 },
};

/* Files of the menu mode, loaded by cd_read_file_list up to the zero file. */
FileRequest arena_mode_files[6] = { { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 0 } }; /* 800917C0 */

s32 arena_mode_own_seq = 0; /* 800917F0 */

DVECTOR arena_hud_gauge_frame_layout[8] = { /* 800917F4 */
    { 0, 1 }, { 0x7F, 1 }, { 0x77, 0x10 }, { 0x40, 0x10 },
    { 0x34, 8 }, { -4, 8 }, { 0x48, 1 }, { 0x38, 1 },
};

u16 arena_hud_gauge_palette[16] = { /* 80091814 */
    0x8000, 0x8421, 0x8842, 0x8C63, 0x9084, 0x94A5, 0x98C6, 0x9CE7,
    0xA108, 0xA529, 0xA94A, 0xAD6B, 0xB18C, 0xB5AD, 0xB9CE, 0x8000,
};

/* The round map: per row, the leftmost and rightmost allowed columns. */
u8 arena_stage_row_left_limits[128] = { /* 80091834 */
    0x32, 0x2E, 0x2B, 0x28, 0x26, 0x23, 0x21, 0x20, 0x1E, 0x1C, 0x1B, 0x1A, 0x18, 0x17, 0x16, 0x15,
    0x14, 0x12, 0x11, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0C, 0x0B, 0x0A, 0x09, 0x09, 0x08, 0x08,
    0x07, 0x06, 0x06, 0x05, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x07, 0x08,
    0x08, 0x09, 0x09, 0x0A, 0x0B, 0x0C, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x11, 0x12, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x1A, 0x1B, 0x1C, 0x1E, 0x20, 0x21, 0x23, 0x26, 0x28, 0x2B, 0x2E, 0x32, 0x38,
};
u8 arena_stage_row_right_limits[128] = { /* 800918B4 */
    0x4C, 0x50, 0x53, 0x56, 0x58, 0x5B, 0x5D, 0x5E, 0x60, 0x62, 0x63, 0x64, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x6C, 0x6D, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x72, 0x73, 0x74, 0x75, 0x75, 0x76, 0x76,
    0x77, 0x78, 0x78, 0x79, 0x79, 0x79, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C, 0x7D, 0x7D,
    0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7D, 0x7D, 0x7D,
    0x7D, 0x7C, 0x7C, 0x7C, 0x7B, 0x7B, 0x7B, 0x7A, 0x7A, 0x79, 0x79, 0x79, 0x78, 0x78, 0x77, 0x76,
    0x76, 0x75, 0x75, 0x74, 0x73, 0x72, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6D, 0x6C, 0x6A, 0x69,
    0x68, 0x67, 0x66, 0x64, 0x63, 0x62, 0x60, 0x5E, 0x5D, 0x5B, 0x58, 0x56, 0x53, 0x50, 0x4C, 0x46,
};

/* Map tile texture positions per orientation; the icon pages are set at
 * run time. */
MapTable arena_stage_ground_draw_table = { /* 80091934 */
    {
        { 0x0000, 0x000F, 0x0F00, 0x0F0F },
        { 0x000F, 0x0000, 0x0F0F, 0x0F00 },
        { 0x0F00, 0x0F0F, 0x0000, 0x000F },
        { 0x0F0F, 0x0F00, 0x000F, 0x0000 },
    },
};

/* 80081ECC: Build the menu backdrop packets: the sky gradient quads, the backdrop
 * texture pages, the six backdrop sprites; scale the map heights and set
 * up the map drawing pools. */
void arena_stage_init(void) {
    POLY_G4 *sky;
    s16 *height;
    s32 i;

    arena_hud_build_overlay_buffers();
    sky = &arena_stage_sky_gradients[0];
    setlen(sky, 8);
    sky->code = 0x38;
    sky->r0 = 0x10;
    sky->g0 = 0x60;
    sky->b0 = 0x7F;
    *(u16 *)&sky->r1 = 0x6010;
    sky->b1 = 0x7F;
    *(u16 *)&sky->r2 = 0x7F7F;
    sky->b2 = 0x7F;
    *(u16 *)&sky->r3 = 0x7F7F;
    sky->b3 = 0x7F;
    *(u32 *)&sky->x0 = 0;
    *(u32 *)&sky->x1 = 0x140;
    *(u32 *)&sky->x2 = 0x600000;
    *(u32 *)&sky->x3 = 0x600140;
    arena_stage_sky_gradients[1] = arena_stage_sky_gradients[0];
    SetDrawTPage(&arena_stage_backdrop_tpages[0], 0, 0, GetTPage(2, 2, 0, 0x100));
    SetDrawTPage(&arena_stage_backdrop_tpages[1], 0, 0, GetTPage(2, 2, 0, 0));
    SetDrawTPage(&arena_stage_backdrop_tpages[2], 0, 0, GetTPage(2, 2, 0x100, 0x100));
    SetDrawTPage(&arena_stage_backdrop_tpages[3], 0, 0, GetTPage(2, 2, 0x100, 0));
    setlen(&arena_stage_backdrop_sprites[0], 4);
    *(u32 *)&arena_stage_backdrop_sprites[0].r0 = 0x64707070;
    arena_stage_backdrop_sprites[0].code &= ~1; /* texture not shaded */
    arena_stage_backdrop_sprites[0].code |= 2;  /* semi-transparent */
    *(u32 *)&arena_stage_backdrop_sprites[0].x0 = 0;
    *(u16 *)&arena_stage_backdrop_sprites[0].u0 = 0;
    *(u32 *)&arena_stage_backdrop_sprites[0].w = 0xDB0080;
    arena_copy_words(&arena_stage_backdrop_sprites[1], &arena_stage_backdrop_sprites[0], sizeof(SPRT) * 5);
    arena_stage_backdrop_sprites[3].u0 = 0x80;
    arena_stage_backdrop_sprites[2].u0 = 0x80;
    arena_stage_backdrop_sprites[3].x0 = 0x80;
    arena_stage_backdrop_sprites[2].x0 = 0x80;
    arena_stage_backdrop_sprites[5].x0 = 0x100;
    arena_stage_backdrop_sprites[4].x0 = 0x100;
    arena_stage_backdrop_sprites[5].w = 0x40;
    arena_stage_backdrop_sprites[4].w = 0x40;
    height = (s16 *)arena_stage_height_map;
    for (i = 0; i < 0x4000; i++) {
        *height *= 12;
        height += 2;
    }
    arena_stage_init_ground_pools();
}

/* 80082178: Draw the large direction arrow at a map position (8.8 fixed point). */
void arena_stage_mark_wide_view_cells(s32 x, s32 z, s32 direction) {
    s32 start_x;
    s32 start_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    x >>= 8;
    z >>= 8;
    last_x = start_x = x + ((gpu_get_sin(direction + 0x280) * 10) >> 12);
    last_z = start_z = z + ((gpu_get_cos(direction + 0x280) * 10) >> 12);
    angle = direction + 0x580;
    for (i = 0; i < 6; i++) {
        next_x = x + ((gpu_get_sin(angle) * 24) >> 12);
        next_z = z + ((gpu_get_cos(angle) * 24) >> 12);
        arena_stage_widen_row_spans(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x100;
    }
    next_x = x + ((gpu_get_sin(direction - 0x280) * 10) >> 12);
    next_z = z + ((gpu_get_cos(direction - 0x280) * 10) >> 12);
    arena_stage_widen_row_spans(last_x, last_z, next_x, next_z);
    arena_stage_widen_row_spans(start_x, start_z, next_x, next_z);
}

/* 80082300: Draw the small direction arrow at a map position (8.8 fixed point). */
void arena_stage_mark_narrow_view_cells(s32 x, s32 z, s32 direction) {
    s32 start_x;
    s32 start_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    x >>= 8;
    z >>= 8;
    last_x = start_x = x + ((gpu_get_sin(direction + 0x100) * 16) >> 12);
    last_z = start_z = z + ((gpu_get_cos(direction + 0x100) * 16) >> 12);
    angle = direction + 0x78A;
    for (i = 0; i < 3; i++) {
        next_x = x + ((gpu_get_sin(angle) * 32) >> 12);
        next_z = z + ((gpu_get_cos(angle) * 32) >> 12);
        arena_stage_widen_row_spans(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x75;
    }
    next_x = x + ((gpu_get_sin(direction - 0x100) * 16) >> 12);
    next_z = z + ((gpu_get_cos(direction - 0x100) * 16) >> 12);
    arena_stage_widen_row_spans(last_x, last_z, next_x, next_z);
    arena_stage_widen_row_spans(start_x, start_z, next_x, next_z);
}

/* 80082458: Copy the stored map position. */
void arena_stage_get_ground_normal(SVECTOR *out) {
    *out = arena_stage_ground_normal;
}

/* Raise a ground corner by its square's kind: 1 by 0x100, 3 by 0x40. */
#define GROUND_KIND_LIFT(corner, x, z)                                          \
    switch (((u32 *)arena_stage_height_map)[(z) * 128 + (x)] & 0x3000000) {                \
    case 0x1000000:                                                            \
        (corner).vy += 0xC0;                                                   \
    case 0x3000000:                                                            \
        (corner).vy += 0x40;                                                   \
    }

/* 80082488: Ground height under a position: the plane through the triangle of its
 * 256-unit square that contains it (corners optionally raised by their
 * square's kind); the plane's normal is kept in arena_stage_ground_normal.
 * Once the triangle is copied, its wide plane point reuses the last
 * corner's scratch slot and the following eight bytes. */
s32 arena_stage_get_ground_height(VECTOR *pos, s32 lift) {
    struct {
        s32 unused0[2];
        union {
            SVECTOR corner[5]; /* four corners and room for the later VECTOR */
            struct {
                SVECTOR unused[3];
                VECTOR point;
            } plane;
        } geometry;
        SVECTOR tri[3];
        s32 unused1[2];
    } scratch;
    GroundSquare *square;
    s32 x;
    s32 z;
    s32 x0;
    s32 z0;

    x = pos->vx;
    z = pos->vz;
    x0 = x & ~0xFF;
    x >>= 8;
    z0 = z & ~0xFF;
    z >>= 8;
    square = (GroundSquare *)((z * 128 + x) * sizeof(GroundSquare) +
                             (s32)arena_stage_height_map);
    scratch.geometry.corner[0].vx = x0;
    scratch.geometry.corner[0].vy = square[0].height;
    scratch.geometry.corner[0].vz = z0;
    scratch.geometry.corner[1].vx = x0 + 0x100;
    scratch.geometry.corner[1].vy = square[129].height;
    scratch.geometry.corner[1].vz = z0 + 0x100;
    scratch.geometry.corner[2].vx = x0 + 0x100;
    scratch.geometry.corner[2].vy = square[1].height;
    scratch.geometry.corner[2].vz = z0;
    scratch.geometry.corner[3].vx = x0;
    scratch.geometry.corner[3].vy = square[128].height;
    scratch.geometry.corner[3].vz = z0 + 0x100;
    if (lift) {
        GROUND_KIND_LIFT(scratch.geometry.corner[0], x, z);
        GROUND_KIND_LIFT(scratch.geometry.corner[1], x + 1, z + 1);
        GROUND_KIND_LIFT(scratch.geometry.corner[2], x + 1, z);
        GROUND_KIND_LIFT(scratch.geometry.corner[3], x, z + 1);
    }
    if ((scratch.geometry.corner[0].vz - scratch.geometry.corner[1].vz) * pos->vx +
            (scratch.geometry.corner[1].vx - scratch.geometry.corner[0].vx) * pos->vz +
            scratch.geometry.corner[0].vx * scratch.geometry.corner[1].vz -
            scratch.geometry.corner[1].vx * scratch.geometry.corner[0].vz < 0) {
        scratch.tri[0] = scratch.geometry.corner[0];
        scratch.tri[1] = scratch.geometry.corner[1];
        scratch.tri[2] = scratch.geometry.corner[2];
    } else {
        scratch.tri[0] = scratch.geometry.corner[0];
        scratch.tri[1] = scratch.geometry.corner[3];
        scratch.tri[2] = scratch.geometry.corner[1];
    }
    model_compute_face_normal(&scratch.tri[0], &scratch.tri[1], &scratch.tri[2], &arena_stage_ground_normal);
    {
        scratch.geometry.plane.point.vx = scratch.tri[0].vx;
        scratch.geometry.plane.point.vy = scratch.tri[0].vy;
        scratch.geometry.plane.point.vz = scratch.tri[0].vz;
        return pos->vy +
               (scratch.geometry.plane.point.vx * arena_stage_ground_normal.vx +
                scratch.geometry.plane.point.vy * arena_stage_ground_normal.vy +
                scratch.geometry.plane.point.vz * arena_stage_ground_normal.vz -
                (pos->vx * arena_stage_ground_normal.vx + pos->vy * arena_stage_ground_normal.vy +
                 pos->vz * arena_stage_ground_normal.vz)) / arena_stage_ground_normal.vy;
    }
}

/* 80082880: Ground height of the map cell under a position (cells of 256 units). */
s32 arena_stage_get_cell_height(SVECTOR *pos) {
    VECTOR unused[3]; /* the original frame has 0x30 unused bytes */
    s16 x, z;

    x = pos->vx >> 8;
    z = pos->vz >> 8;
    return *(s16 *)&((s32 *)arena_stage_height_map)[x + z * 128];
}

/* 800828C4: The map cell word under a position (cells of 256 units). */
s32 arena_stage_get_ground_square(VECTOR *pos) {
    s32 x = pos->vx >> 8;
    s32 z = pos->vz >> 8;

    return ((s32 *)arena_stage_height_map)[z * 128 + x];
}

/* 800828F8: Keep a moving position inside the circular arena of the given radius
 * around the scene centre: when the step would leave it, turn the step
 * along the rim and shorten it until the end point is inside. */
void arena_stage_keep_step_inside(VECTOR *pos, VECTOR *step, s32 radius) {
    VECTOR local;
    VECTOR next;
    VECTOR square;
    MATRIX rim;
    MATRIX back;
    SVECTOR dir;
    s32 distance;

    local.vx = pos->vx + step->vx - 0x3F80;
    local.vz = pos->vz + step->vz - 0x3F80;
    Square0(&local, &square);
    if (radius < SquareRoot0(square.vx + square.vz)) {
        VectorNormalS(&local, &dir);
        rim.m[2][1] = 0;
        rim.m[1][2] = 0;
        rim.m[1][0] = 0;
        rim.m[0][1] = 0;
        rim.m[1][1] = 0x1000;
        rim.m[2][2] = dir.vz;
        rim.m[0][0] = dir.vz;
        rim.m[0][2] = -dir.vx;
        rim.m[2][0] = dir.vx;
        ApplyMatrixLV(&rim, step, &local);
        libgte_transpose_matrix(&rim, &back);
        SetRotMatrix(&back);
        local.vz = 0;
        for (;;) {
            libgte_rotate_vector(&local, step);
            next.vx = pos->vx + step->vx - 0x3F80;
            next.vz = pos->vz + step->vz - 0x3F80;
            Square0(&next, &square);
            distance = SquareRoot0(square.vx + square.vz);
            if (radius >= distance) {
                break;
            }
            local.vz -= distance - radius - 8;
        }
    }
}

/* 80082A70: Apply the current stage's colours: sky gradient (top and bottom), back
 * and far (fog) colours, fade tiles and the GTE primitive colour. */
void arena_stage_apply_colors(void) {
    Environment *env;
    s32 top_r;
    s32 top_g;
    s32 top_b;
    s32 bottom_r;
    s32 bottom_g;
    s32 bottom_b;

    env = &arena_stage_color_table[arena_stage_index];
    arena_current_stage_colors = env;
    top_r = env->top[0];
    top_g = env->top[1];
    top_b = env->top[2];
    arena_stage_back_color_red = env->unk4;
    arena_stage_back_color_green = env->unk5;
    arena_stage_back_color_blue = env->unk6;
    bottom_r = env->bottom[0];
    bottom_g = env->bottom[1];
    bottom_b = env->bottom[2];
    model_set_color(env->back[0], env->back[1], env->back[2]);
    SetFarColor(bottom_r, bottom_g, bottom_b);
    arena_stage_sky_gradients[0].r0 = top_r;
    arena_stage_sky_gradients[1].r0 = top_r;
    arena_stage_sky_gradients[0].g0 = top_g;
    arena_stage_sky_gradients[1].g0 = top_g;
    arena_stage_sky_gradients[0].b0 = top_b;
    arena_stage_sky_gradients[1].b0 = top_b;
    *(u16 *)&arena_stage_sky_gradients[0].r1 = top_r | (top_g << 8);
    arena_stage_sky_gradients[0].b1 = top_b;
    *(u16 *)&arena_stage_sky_gradients[1].r1 = top_r | (top_g << 8);
    arena_stage_sky_gradients[1].b1 = top_b;
    *(u16 *)&arena_stage_sky_gradients[0].r2 = bottom_r | (bottom_g << 8);
    arena_stage_sky_gradients[0].b2 = bottom_b;
    *(u16 *)&arena_stage_sky_gradients[1].r2 = bottom_r | (bottom_g << 8);
    arena_stage_sky_gradients[1].b2 = bottom_b;
    *(u16 *)&arena_stage_sky_gradients[0].r3 = bottom_r | (bottom_g << 8);
    arena_stage_sky_gradients[0].b3 = bottom_b;
    *(u16 *)&arena_stage_sky_gradients[1].r3 = bottom_r | (bottom_g << 8);
    arena_stage_sky_gradients[1].b3 = bottom_b;
    arena_display_buffers[0].background.r0 = bottom_r;
    arena_display_buffers[0].background.g0 = bottom_g;
    arena_display_buffers[0].background.b0 = bottom_b;
    arena_display_buffers[1].background.r0 = bottom_r;
    arena_display_buffers[1].background.g0 = bottom_g;
    arena_display_buffers[1].background.b0 = bottom_b;
    SetFogNearFar(0x800, 0x1800, 0xC0);
    model_color = (model_color & 0xFFFFFF) | 0x28000000;
    gte_ldrgb(&model_color);
}

/* 80082C4C: Load the stage's floor texture (a TIM, palette made semi-transparent)
 * and build the two pools of 64 textured floor quads, alternating the two
 * halves of the texture. */
void arena_stage_load_wall(MenuImageFile *files) {
    TIM_IMAGE tim;
    POLY_FT4 *quad;
    s16 *clut;
    s32 i;

    OpenTIM(files->floor);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    for (i = 0; i < 0x100; i++) {
        *clut++ |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    arena_stage_wall_clut = GetClut(tim.crect->x, tim.crect->y);
    arena_stage_wall_tpage = GetTPage(1, 0, tim.prect->x, tim.prect->y);
    arena_stage_wall_texture_v = (u8)tim.prect->y;
    arena_stage_wall_quads[0] = heap_alloc(0xA00, 0);
    arena_stage_wall_quads[1] = heap_alloc(0xA00, 0);
    quad = arena_stage_wall_quads[0];
    for (i = 0; i < 0x40; i += 2) {
        setlen(&quad[0], 9);
        quad[0].code = 0x2C;
        setlen(&quad[1], 9);
        quad[1].code = 0x2C;
        quad->clut = arena_stage_wall_clut;
        quad->tpage = arena_stage_wall_tpage;
        quad->u0 = 0x7F;
        quad->v0 = arena_stage_wall_texture_v + 0x3F;
        quad->u1 = 0x7F;
        quad->v1 = arena_stage_wall_texture_v;
        quad->u2 = 0x3F;
        quad->v2 = arena_stage_wall_texture_v + 0x3F;
        quad->u3 = 0x3F;
        quad->v3 = arena_stage_wall_texture_v;
        quad++;
        quad->clut = arena_stage_wall_clut;
        quad->tpage = arena_stage_wall_tpage;
        quad->u0 = 0x3F;
        quad->v0 = arena_stage_wall_texture_v + 0x3F;
        quad->u1 = 0x3F;
        quad->v1 = arena_stage_wall_texture_v;
        quad->u2 = 0;
        quad->v2 = arena_stage_wall_texture_v + 0x3F;
        quad->u3 = 0;
        quad->v3 = arena_stage_wall_texture_v;
        quad++;
    }
    arena_copy_words(arena_stage_wall_quads[1], arena_stage_wall_quads[0], 0xA00);
}

/* 80082E60: Draw the arena wall: a ring of 32 two-storey textured segments around
 * the scene centre, starting behind the given position, depth-cued and
 * skipped when too far away. The wall's corners are taken relative to the
 * camera as 16-bit offsets. */
void arena_stage_draw_wall(u32 *ot, VECTOR *pos) {
    VECTOR centre;
    SVECTOR base0;
    SVECTOR base1;
    SVECTOR mid0;
    SVECTOR mid1;
    SVECTOR top0;
    SVECTOR top1;
    s32 z[4];
    POLY_FT4 *quad;
    POLY_FT4 *next;
    s32 angle;
    s32 depth;
    s32 i;

    centre = *pos;
    i = 0;
    quad = arena_stage_wall_quads[arena_draw_buffer_index];
    centre.vx -= 0x3F80;
    centre.vz -= 0x3F80;
    angle = ratan2(centre.vx, centre.vz) & 0xFFF0;
    angle -= 0x100;
    mid0.vy = mid1.vy = -0x290;
    base0.vy = base1.vy = 0;
    top0.vy = top1.vy = -0x520;
    base0.vx = ((gpu_get_sin(angle) * 0x3F80) >> 12) - (s16)(arena_view_origin.vx - 0x3F80);
    base0.vz = ((gpu_get_cos(angle) * 0x3F80) >> 12) - (s16)(arena_view_origin.vz - 0x3F80);
    angle += 0x10;
    for (; i < 32; i++) {
        top0.vx = mid0.vx = base0.vx;
        top0.vz = mid0.vz = base0.vz;
        top1.vx = mid1.vx = base1.vx = ((gpu_get_sin(angle) * 0x3F80) >> 12) - (s16)(arena_view_origin.vx - 0x3F80);
        top1.vz = mid1.vz = base1.vz = ((gpu_get_cos(angle) * 0x3F80) >> 12) - (s16)(arena_view_origin.vz - 0x3F80);
        gte_ldv3(&base0, &base1, &mid0);
        gte_rtpt();
        gte_dpcs();
        gte_stsxy3(&quad[0].x0, &quad[0].x1, &quad[0].x2);
        gte_stsz3(&z[0], &z[1], &z[2]);
        gte_ldv3(&mid1, &top0, &top1);
        gte_rtpt();
        next = &quad[1];
        depth = z[0];
        if (depth < z[1]) {
            depth = z[1];
        }
        if (depth <= z[2]) {
            depth = z[2];
        }
        *(u32 *)&next->x0 = *(u32 *)&quad[0].x2;
        gte_stsxy(&quad[0].x3);
        gte_stsxy3(&quad[0].x3, &next->x2, &next->x3);
        gte_stsz(&z[3]);
        *(u32 *)&next->x1 = *(u32 *)&quad[0].x3;
        if (depth <= z[3]) {
            depth = z[3];
        }
        if (depth < 0x1C00) {
            depth >>= 4;
            gte_strgb(&quad[0].r0);
            gte_strgb(&next->r0);
            setlen(&quad[0], 9);
            quad[0].code = 0x2C;
            setlen(next, 9);
            next->code = 0x2C;
            AddPrim(&ot[depth], &quad[0]);
            AddPrim(&ot[depth], next);
        }
        quad += 2;
        angle += 0x10;
        base0.vx = base1.vx;
        base0.vz = base1.vz;
    }
}

/* 800831C8: Put the look-at point somewhere random around the scene centre and set
 * the idle camera motion parameters. */
void arena_camera_place_at_random(void) {
    s32 radius;
    s32 angle;

    radius = (rand() & 0x1FFF) + 0x800;
    angle = rand() % 0x600 + 0x500;
    arena_camera_position.vx = ((gpu_get_sin(angle) * radius) >> 12) + 0x4000;
    arena_camera_position.vz = ((gpu_get_cos(angle) * radius) >> 12) + 0x4000;
    arena_camera_position.vy = -((rand() & 0x7FF) + 0x400);
    arena_camera_radius = 0x100;
    arena_camera_pair_lift = 0x40;
    arena_camera_ease_step_count = 0x40;
    arena_camera_side_angle = 0x400;
}

/* 800832C0: Turn the idle camera with the left/right buttons. */
void arena_camera_turn_orbit(s32 buttons) {
    if (buttons & 0x8000) {
        arena_camera_orbit_angle += 0x20;
    }
    if (buttons & 0x2000) {
        arena_camera_orbit_angle -= 0x20;
    }
}

/* 80083310: Idle orbit camera: move the eye toward a point between the two actors
 * (further toward the other actor late in the orbit, a third of the way
 * when smoothing) and swing the look-at point around it, kept inside the
 * arena and above the ground. */
void arena_camera_update_orbit(s32 smooth) {
    VECTOR look;
    VECTOR step;
    VECTOR offset;
    VECTOR unused;   /* the original frame has 0x18 unused bytes */
    SVECTOR unused2;
    Actor *subject;
    Actor *other;
    s32 value; /* the other actor's share, then the orbit angle, then the ground */

    if (arena_bout_round_winner != 0) {
        subject = &arena_second_actor;
        other = &arena_first_actor;
    } else {
        subject = &arena_first_actor;
        other = &arena_second_actor;
    }
    ratan2(subject->pos.vx - other->pos.vx, subject->pos.vz - other->pos.vz);
    if (arena_bout_replay_timer > 0xB0) {
        value = 0x100;
    } else if (arena_bout_replay_timer > 0xA0) {
        value = (arena_bout_replay_timer - 0xA0) << 4;
    } else {
        value = 0;
    }
    offset.vx = other->pos.vx;
    offset.vy = other->pos.vy;
    offset.vz = other->pos.vz;
    offset.vx -= subject->pos.vx;
    offset.vy -= subject->pos.vy;
    offset.vz -= subject->pos.vz;
    offset.vx *= value;
    offset.vy *= value;
    offset.vz *= value;
    offset.vx /= 256;
    offset.vy /= 256;
    offset.vz /= 256;
    offset.vx += subject->pos.vx;
    offset.vy += subject->pos.vy;
    offset.vz += subject->pos.vz;
    offset.vy -= 0xA0;
    offset.vx -= arena_camera_focus.vx;
    offset.vy -= arena_camera_focus.vy;
    offset.vz -= arena_camera_focus.vz;
    if (smooth) {
        offset.vx /= 3;
        offset.vy /= 3;
        offset.vz /= 3;
    }
    arena_camera_radius = 0xC00;
    arena_camera_focus.vx += offset.vx;
    arena_camera_focus.vy += offset.vy;
    arena_camera_focus.vz += offset.vz;
    value = arena_camera_orbit_angle + arena_bout_replay_timer * arena_camera_orbit_speed;
    look.vx = (gpu_get_sin(value) * arena_camera_radius) >> 12;
    look.vz = (gpu_get_cos(value) * arena_camera_radius) >> 12;
    look.vy = -(arena_bout_replay_timer * 6 + 0x200);
    look.vx += arena_camera_focus.vx;
    look.vy += arena_camera_focus.vy;
    look.vz += arena_camera_focus.vz;
    step.vx = look.vx - arena_camera_position.vx;
    step.vz = look.vz - arena_camera_position.vz;
    arena_stage_keep_step_inside(&arena_camera_position, &step, 0x3D00);
    arena_camera_position.vx += step.vx;
    arena_camera_position.vz += step.vz;
    value = arena_stage_get_ground_height(&arena_camera_position, 0);
    if (value < look.vy) {
        look.vy = value;
    }
    arena_camera_position.vy = look.vy;
}

/* 8008369C: Start an idle camera orbit at a random angle, speed and direction. */
void arena_camera_start_orbit(void) {
    arena_camera_orbit_angle = rand();
    arena_camera_orbit_speed = rand() % 12 + 4;
    if (rand() & 1) {
        arena_camera_orbit_speed = -arena_camera_orbit_speed;
    }
    arena_camera_update_orbit(0);
}

/* 80083738: Frame two actors: put the eye between them, pick the side of the pair
 * the look-at point is nearer to, and move the look-at point toward a spot
 * beside the pair (further back when they are far apart), kept inside the
 * arena and above the ground. */
void arena_camera_frame_actors_for_scene(Actor *first, Actor *second) {
    VECTOR side;
    VECTOR other_side;
    VECTOR unused[2]; /* the original frame has 0x20 unused bytes */
    s32 heading;
    s32 distance;
    s32 angle;
    s32 value; /* the second angle, then a side's distance, then the ground */

    heading = ratan2(first->pos.vx - second->pos.vx, first->pos.vz - second->pos.vz);
    distance = arena_vector_get_distance(&first->pos, &second->pos);
    angle = heading - 0x400;
    arena_camera_radius = distance * 2 / 3 + 0xC0;
    arena_camera_focus.vx = (first->pos.vx + second->pos.vx) / 2;
    arena_camera_focus.vy = (first->pos.vy + second->pos.vy) / 2 - 0xA0;
    arena_camera_focus.vz = (first->pos.vz + second->pos.vz) / 2;
    side.vx = arena_camera_focus.vx + ((gpu_get_sin(angle) * arena_camera_radius) >> 12);
    side.vz = arena_camera_focus.vz + ((gpu_get_cos(angle) * arena_camera_radius) >> 12);
    value = heading + 0x400;
    other_side.vx = arena_camera_focus.vx + ((gpu_get_sin(value) * arena_camera_radius) >> 12);
    other_side.vz = arena_camera_focus.vz + ((gpu_get_cos(value) * arena_camera_radius) >> 12);
    side.vx -= arena_camera_position.vx;
    side.vy -= arena_camera_position.vy;
    side.vz -= arena_camera_position.vz;
    other_side.vx -= arena_camera_position.vx;
    other_side.vy -= arena_camera_position.vy;
    other_side.vz -= arena_camera_position.vz;
    value = arena_vector_get_flat_length(&side);
    if (arena_vector_get_flat_length(&other_side) < value) {
        arena_camera_side_angle = 0x400;
        arena_camera_side_flipped = 0;
    } else {
        arena_camera_side_angle = -0x400;
        arena_camera_side_flipped = 1;
    }
    distance /= 4;
    if (distance > 0x300) {
        distance = 0x300;
    }
    side.vy = arena_camera_focus.vy - arena_camera_pair_lift - distance;
    side.vx = arena_camera_focus.vx + ((gpu_get_sin(heading + arena_camera_side_angle) * arena_camera_radius) >> 12);
    side.vz = arena_camera_focus.vz + ((gpu_get_cos(heading + arena_camera_side_angle) * arena_camera_radius) >> 12);
    value = arena_stage_get_ground_height(&side, 0) - 0x100;
    if (value < side.vy) {
        side.vy = value;
    }
    side.vx = (side.vx - arena_camera_position.vx) / arena_camera_ease_step_count;
    side.vy = (side.vy - arena_camera_position.vy) / arena_camera_ease_step_count;
    side.vz = (side.vz - arena_camera_position.vz) / arena_camera_ease_step_count;
    arena_camera_unread_pair_heading = heading;
    arena_stage_keep_step_inside(&arena_camera_position, &side, 0x3D00);
    arena_camera_ease_step_count = 100;
    arena_camera_position.vx += side.vx;
    arena_camera_position.vy += side.vy;
    arena_camera_position.vz += side.vz;
}

/* 80083B54: Read the camera's look-at point and eye. */
void arena_camera_get_position_and_focus(VECTOR *position, VECTOR *focus) {
    *position = arena_camera_position;
    *focus = arena_camera_focus;
}

/* 80083BB4: Clear the display area (one or both 320-wide buffers) and wait. */
void arena_display_clear_buffers(s32 both) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    if (both) {
        rect.w = 0x280;
    } else {
        rect.w = 0x140;
    }
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* 80083C0C: Enter a camera/scene mode, running its setup. */
void arena_mode_set_state(s32 mode) {
    arena_mode_next_state = mode;
    switch (mode) {
    case 3:
        arena_menu_reset_background();
        break;
    case 4:
        arena_bout_start_replay(arena_bout_round_frame_count);
        break;
    case 8:
        arena_bout_start_result_view();
        break;
    case 6:
        if (arena_first_actor.unkF2 < arena_second_actor.unkF2) {
            arena_winner_open_screen(&arena_second_actor);
        } else {
            arena_winner_open_screen(&arena_first_actor);
        }
        break;
    }
}

/* 80083CD8: The scene state word. */
s32 arena_mode_get_state(void) {
    return arena_mode_state;
}

/* 80083CE8: Draw the elapsed time (frames at 30 per second) as minutes, seconds and
 * hundredths. */
void arena_bout_draw_elapsed_time(void) {
    char text[32];
    s32 minutes;
    s32 seconds;

    seconds = arena_bout_fight_frame_count % 1800;
    minutes = arena_bout_fight_frame_count / 1800;
    sprintf(text, "%02d'%02d''%02d", minutes, seconds / 30, arena_bout_fight_frame_count % 30 * 99 / 30);
    arena_text_draw_line(text);
}

/* 80083DCC: Update an actor's glow light (fading it) at its position relative to its
 * opponent, and the spot light at its position relative to the camera. */
void arena_view_light_actor(LightRig *rig, Actor *actor, s32 index) {
    MATRIX unused; /* unused in the original; reserves 32 bytes */
    Node *light = rig->lights[index];
    u8 glow = actor->glow;
    s32 level = glow;

    if (level != 0) {
        actor->glow = glow - 0x18;
        if (level < actor->glow) {
            actor->glow = 0;
        }
    }
    if (actor->unkD4 & 0x20) {
        NODE_LIGHT(light)->colour[0] = actor->opponent->colour.r * level / 16;
        NODE_LIGHT(light)->colour[1] = actor->opponent->colour.g * level / 16;
        NODE_LIGHT(light)->colour[2] = actor->opponent->colour.b * level / 16;
    } else {
        NODE_LIGHT(light)->colour[0] = level << 4;
        NODE_LIGHT(light)->colour[1] = level << 3;
        NODE_LIGHT(light)->colour[2] = 0;
    }
    if (arena_current_stage_colors->dim) {
        NODE_LIGHT(light)->colour[0] /= 2;
        NODE_LIGHT(light)->colour[1] /= 2;
        NODE_LIGHT(light)->colour[2] /= 2;
    }
    NODE_LIGHT(light)->direction[0] = actor->pos.vx;
    NODE_LIGHT(light)->direction[1] = actor->pos.vy;
    NODE_LIGHT(light)->direction[2] = actor->pos.vz;
    NODE_LIGHT(light)->direction[0] -= actor->opponent->pos.vx;
    NODE_LIGHT(light)->direction[1] -= actor->opponent->pos.vy;
    NODE_LIGHT(light)->direction[2] -= actor->opponent->pos.vz;
    model_set_light(index, NODE_LIGHT(light));
    light = rig->lights[2];
    NODE_LIGHT(light)->colour[0] = NODE_LIGHT(light)->colour[1] = NODE_LIGHT(light)->colour[2] = 0;
    NODE_LIGHT(light)->direction[0] = actor->pos.vx;
    NODE_LIGHT(light)->direction[1] = actor->pos.vy;
    NODE_LIGHT(light)->direction[2] = actor->pos.vz;
    NODE_LIGHT(light)->direction[0] -= arena_camera_position.vx;
    NODE_LIGHT(light)->direction[1] -= arena_camera_position.vy;
    NODE_LIGHT(light)->direction[2] -= arena_camera_position.vz;
    model_set_light(2, NODE_LIGHT(light));
}

/* 800840CC: Draw the 3D arena: aim the camera, pose the actors, then draw the floor,
 * the actors and their shadows, the look-at marker and the sky. */
s32 arena_view_draw_arena(LightRig *rig) {
    MATRIX floor;
    MATRIX camera;
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    OtPair *layer = rig->layer;
    Light *light;

    arena_display_start_layer(layer);
    arena_node_aim_rig_camera(rig, &arena_camera_position, &arena_camera_focus);
    if (arena_mode_state != 4 && arena_play_mode != 4 && arena_mode_state != 8) {
        arena_hud_draw(&arena_first_actor, &arena_second_actor);
    } else if (arena_play_mode == 4) {
        arena_actor_draw_move_info(&arena_first_actor);
        if (arena_debug_enabled != 0) {
            arena_actor_draw_move_info(&arena_second_actor);
        }
    }
    arena_text_draw_banner(arena_current_ot);
    arena_menu_draw_overlay(arena_current_ot);
    camera = rig->camera->view;
    ((Node *)arena_first_actor.object)->view = ((Node *)arena_second_actor.object)->view = camera;
    light = NODE_LIGHT(rig->lights[0]);
    light->colour[0] = light->colour[1] = light->colour[2] = 0x800;
    model_set_light(0, NODE_LIGHT(rig->lights[0]));
    gte_SetBackColor(arena_stage_back_color_red, arena_stage_back_color_green, arena_stage_back_color_blue);
    arena_view_light_actor(rig, &arena_first_actor, 1);
    arena_node_draw_tree(arena_first_actor.node);
    arena_view_light_actor(rig, &arena_second_actor, 1);
    arena_node_draw_tree(arena_second_actor.node);
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    arena_sound_update_voices();
    arena_effect_draw_all(&camera, layer->ot[arena_draw_buffer_index]);
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    arena_effect_emit_hit_sparks(layer->ot[arena_draw_buffer_index], &rig->camera->view);
    floor = arena_identity_matrix;
    floor.t[1] = -arena_view_origin.vy;
    CompMatrix(&camera, &floor, &floor);
    gte_SetRotMatrix(&floor);
    gte_SetTransMatrix(&floor);
    arena_stage_apply_colors();
    arena_stage_draw_wall(layer->ot[arena_draw_buffer_index], &arena_camera_position);
    arena_stage_draw_shadow(&arena_first_actor, layer->ot[arena_draw_buffer_index], &floor);
    arena_stage_draw_shadow(&arena_second_actor, layer->ot[arena_draw_buffer_index], &floor);
    gte_SetRotMatrix(&floor);
    gte_SetTransMatrix(&floor);
    arena_stage_clear_row_spans();
    if (arena_stage_uses_narrow_view != 0) {
        arena_stage_mark_narrow_view_cells(arena_camera_position.vx, arena_camera_position.vz,
                      ratan2(arena_camera_position.vx - arena_camera_focus.vx, arena_camera_position.vz - arena_camera_focus.vz));
    } else {
        arena_stage_mark_wide_view_cells(arena_camera_position.vx, arena_camera_position.vz,
                      ratan2(arena_camera_position.vx - arena_camera_focus.vx, arena_camera_position.vz - arena_camera_focus.vz));
    }
    arena_stage_draw_ground(layer->ot[arena_draw_buffer_index], arena_camera_focus.vx, arena_camera_focus.vz);
    arena_display_link_layer(layer);
    arena_hud_link_overlay_tpage();
    gpu_ot_link_poly_g4((u_long *)arena_current_ot, &arena_stage_sky_gradients[arena_draw_buffer_index]);
    return 0;
}

/* 800846A0: Draw the 3D scene: aim the camera, give both actors the camera matrix,
 * light and draw them, then the view's layer and the backdrop sprites. */
s32 arena_view_draw_actors_on_backdrop(LightRig *rig) {
    MATRIX unused0; /* unused in the original; reserves 32 bytes */
    MATRIX camera;
    VECTOR unused1; /* unused in the original; reserves 16 bytes */
    OtPair *layer = rig->layer;
    Light *light;

    arena_display_start_layer(layer);
    arena_node_aim_rig_camera(rig, &arena_camera_position, &arena_camera_focus);
    arena_text_drop_quads();
    camera = rig->camera->view;
    ((Node *)arena_first_actor.object)->view = ((Node *)arena_second_actor.object)->view = camera;
    arena_node_disable_color_overrides();
    light = NODE_LIGHT(rig->lights[0]);
    light->colour[0] = light->colour[1] = light->colour[2] = 0x800;
    model_set_light(0, NODE_LIGHT(rig->lights[0]));
    gte_SetBackColor(arena_stage_back_color_red, arena_stage_back_color_green, arena_stage_back_color_blue);
    arena_view_light_actor(rig, &arena_first_actor, 1);
    arena_node_draw_tree(arena_first_actor.node);
    arena_view_light_actor(rig, &arena_second_actor, 1);
    arena_node_draw_tree(arena_second_actor.node);
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    arena_effect_emit_hit_sparks(layer->ot[arena_draw_buffer_index], &rig->camera->view);
    arena_effect_draw_lines_only(layer->ot[arena_draw_buffer_index]);
    arena_display_link_layer(layer);
    arena_hud_link_overlay_tpage();
    AddPrim(arena_current_ot, &arena_stage_backdrop_sprites[4 + arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_stage_backdrop_tpages[2 + arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_stage_backdrop_sprites[2 + arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_stage_backdrop_sprites[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_stage_backdrop_tpages[arena_draw_buffer_index]);
    return 0;
}

/* 800849E0: Draw a 3D view: update its layer, link this buffer's ordering table, finish. */
s32 arena_view_draw_menu(LightRig *rig) {
    OtPair *layer = rig->layer;

    arena_display_start_layer(layer);
    arena_menu_draw_overlay(&layer->ot[arena_draw_buffer_index][1]);
    arena_display_link_layer(layer);
    arena_hud_link_overlay_tpage();
    return 0;
}

/* 80084A40: Update a 3D view's layer without drawing it. */
void arena_view_start_layer(LightRig *rig) {
    arena_display_start_layer(rig->layer);
}

/* 80084A64: Draw a 3D view with its shading packet at brightness 0xC0. */
s32 arena_view_draw_glow_and_menu(LightRig *rig) {
    OtPair *layer = rig->layer;

    arena_glow_draw(&layer->ot[arena_draw_buffer_index][2], 0xC0, 0);
    arena_menu_draw_overlay(&layer->ot[arena_draw_buffer_index][1]);
    arena_display_link_layer(layer);
    arena_hud_link_overlay_tpage();
    return 0;
}

/* 80084AE0: Draw the fading overlay while a fade is running. */
void arena_mode_draw_fade_frame(void) {
    if (arena_mode_fade_level != 0) {
        arena_glow_step();
        arena_select_draw_wheels(arena_current_ot, 0);
        arena_menu_draw_overlay(arena_current_ot);
        arena_glow_draw(arena_current_ot, 0xC0, 1);
        arena_task_yield();
    }
}

/* 80084B48: Step the overlay fade down by 4; when it ends, reset it. */
void arena_mode_step_fade(void) {
    if (arena_mode_fade_level != 0) {
        arena_mode_fade_level -= 4;
        if (arena_mode_fade_level <= 0) {
            arena_mode_fade_level = 0;
            arena_mode_glow_in_vblank = 0;
            sound_set_seq_tempo(arena_mode_music_seq, 0x100, 0);
            arena_bout_motion_speed = 0x100;
            arena_glow_free_buffers();
        } else {
            arena_glow_draw(arena_current_ot, arena_mode_fade_level, 1);
        }
    } else {
        arena_mode_glow_in_vblank = 0;
    }
}

/* 80084BEC: Attach an extra object (model arena_actor_extra_model) to the actor's model, turned
 * by (0, 0xC00, 0x400). */
void arena_actor_attach_extra_model(Actor *actor) {
    Node *parent = ((ModelSet *)actor->node->data)->nodes[12];
    Node *object = arena_node_alloc();
    NodeModel *part = arena_node_alloc_model();

    arena_node_set_model(object, part);
    arena_node_init_model(part, &arena_actor_extra_model);
    arena_node_add_child(parent, object);
    object->angles.vy = 0xC00;
    object->angles.vx = 0;
    object->angles.vz = 0x400;
}

/* 80084C88: Set up an actor from its loaded model file on one side of the scene:
 * opponent link, model object, kind flags from the model id, part counts,
 * and the palette/emblem images in VRAM (mirrored for side 0). */
void arena_actor_init_from_model_file(Actor *actor, ModelFile *data, s32 side) {
    RECT rect;
    void *block; /* the model object, later the mirrored emblem */
    SceneHeader *header;
    u8 *source;
    s32 i;
    s32 j;
    u8 *out;
    s32 row;

    actor->flags = (actor->flags & ~0x08000000) | ((side & 1) << 27);
    if (side) {
        arena_node_set_texture_overrides(0x380, 0, 0, 0x1FE);
        actor->opponent = &arena_first_actor;
    } else {
        arena_node_set_texture_overrides(0x3C0, 0, 0, 0x1FF);
        actor->opponent = &arena_second_actor;
    }
    arena_node_relocate_model_file(data);
    block = arena_node_build_model_set(data);
    actor->object = arena_node_alloc();
    arena_node_add_child(actor->object, block);
    actor->node = (Node *)block;
    actor->moves = &arena_actor_move_lists[actor->model_id];
    actor->kind = 0;
    switch (actor->model_id) {
    case 36:
    case 37:
        actor->kind |= 1;
    case 38:
        actor->kind |= 2;
        break;
    case 3:
    case 14:
    case 27:
    case 34:
    case 35:
    case 39:
    case 41:
    case 42:
        actor->kind |= 4;
        break;
    case 13:
        arena_node_clear_texture_overrides();
        arena_actor_attach_extra_model(actor);
        break;
    }
    arena_sound_choose_command_table(actor);
    header = data->header;
    actor->header = header;
    actor->unk7C = data->unk14;
    actor->move_slots = data->slots;
    actor->unk900 = (u8 *)header + 0x34;
    actor->visible = (u8 *)(header->unk30 + (s32)header);
    actor->visible_count = header->unkE;
    actor->colour.r = header->unk10[0];
    actor->colour.g = header->unk10[1];
    actor->colour.b = header->unk10[2];
    actor->move_count = 0;
    actor->parts_b = 0;
    for (i = 0; i < 14; i++) {
        if (actor->moves->learned[i]) {
            if (actor->move_slots[i].usable) {
                actor->move_count++;
            } else {
                actor->parts_b++;
            }
        }
    }
    row = 0x100;
    rect.x = 0;
    rect.y = side + 0x1F6;
    rect.w = row;
    rect.h = 1;
    LoadImage(&rect, (u_long *)data->image);
    rect.x = side * 16 + 0x380;
    rect.y = row;
    rect.w = 0xB;
    rect.h = 0x16;
    if (side) {
        LoadImage(&rect, (u_long *)(data->image + 0x200));
    } else {
        source = data->image + 0x200;
        block = heap_alloc(0x1E4, 0);
        out = block;
        for (i = 0; i < 0x16; i++) {
            for (j = 0; j < 0x16; j++) {
                *out++ = source[0x15 - j];
            }
            source += 0x16;
        }
        LoadImage(&rect, block);
        heap_delay_free(block, 1);
    }
    rect.x = 0x3A0;
    rect.y = side * 8 + 0x100;
    rect.w = 0x10;
    rect.h = 8;
    LoadImage(&rect, (u_long *)(data->image + 0x3E4));
}

/* 80084FD0: The vertical-blank hook, run by the resident handler 8003634c: while the
 * overlay fade runs, step the glow field (8008e120, which calls rand) on odd
 * blank counts. */
void arena_mode_vblank_hook(void) {
    if (arena_mode_glow_in_vblank != 0 && (pad_vblank_count & 1)) {
        arena_glow_step();
    }
}

/* 80085014: Leave the menu screen for scene mode 5. */
void arena_mode_start_demo(void) {
    arena_menu_screen_flags &= ~1;
    arena_display_clear_buffers(0);
    arena_mode_set_state(5);
    arena_settings.driven = 1;
    arena_settings.com1 = 1;
    arena_play_mode = 6;
}

/* 80085070: Return from scene mode 5 to the menu screen. */
void arena_mode_end_demo(void) {
    arena_settings.driven = 0;
    arena_settings.com1 = 0;
    arena_menu_screen_flags |= 1;
}

/* 8008509C: Give actor slot `which` (1 = arena_second_actor, 0 = arena_first_actor) a new model
 * id and load its model, replacing the previous one. */
void arena_actor_load_model(s32 which, s32 id) {
    if (which != 0) {
        arena_second_actor.model_id = id;
    } else {
        arena_first_actor.model_id = id;
    }
    if (arena_actor_model_files[which] != NULL) {
        heap_free(arena_actor_model_files[which]);
        arena_actor_model_files[which] = NULL;
    }
    cd_select_directory(0x30, 1);
    arena_actor_model_files[which] = arena_load_whole_file(id + 2);
    cd_select_directory(0x30, 0);
}

/* 80085134: Release actor slot `which`'s model. */
void arena_actor_release_model(s32 which) {
    cd_sync_reads(0);
    if (arena_actor_model_files[which] != NULL) {
        heap_free(arena_actor_model_files[which]);
        arena_actor_model_files[which] = NULL;
    }
}

/* 8008518C: Load a resource by its file number. */
void arena_mode_alloc_file(FileRequest *resource, s32 arg) {
    resource->destination = heap_alloc(cd_get_aligned_file_size(resource->file), arg);
}

/* 800851D4: Leave the menu mode: stop its sound and streams, wait for drawing and
 * dispatch the next mode. */
void arena_mode_exit(void) {
    sound_remove_effect_bank(arena_mode_sound_bank);
    if (arena_mode_own_seq != 0) {
        sound_stop_seq(arena_mode_music_seq);
        sound_release_seq(arena_mode_music_seq);
    }
    arena_settings_save_to_game_data();
    mode_select_next_mode(1);
    DrawSync(0);
    VSync(2);
    mode_arena_task_parameters = 1;
    mode_dispatch(0);
}

/* 80085264: Whether the scene is in a state that ends the menu mode. */
s32 arena_mode_is_exiting(void) {
    s32 done = 0;

    if (arena_mode_next_state == 5 || arena_mode_next_state == 7 ||
        (arena_mode_next_state == 1 && arena_play_mode == 4 && mode_arena_task_parameters == 0)) {
        done = 1;
    }
    return done;
}

/* 800852C4: The menu mode: load its resources, then run the title/options screens
 * (until a choice starts a bout or the demo idles out) and the bouts
 * themselves, one scene mode (arena_mode_state) per frame. */
void arena_mode_task(s32 arg) {
    LightRig *rig;
    void *file;
    void *model;
    SoundSeqHeader *sequence;
    s32 step;
    s32 idle;
    s32 hold; /* never initialised: the first held frame counts from garbage */

    arena_debug_display_flags = 0;
    arena_menu_screen_flags |= 1;
    arena_stage_height_map = heap_alloc(0x10010, 0);
    rig = arena_node_alloc_light_rig(arena_display_alloc_layer(0x1000));
    model_relocate_sprite_model(&arena_actor_extra_model);
    arena_mode_alloc_file(&arena_mode_files[0], 0);
    arena_mode_alloc_file(&arena_mode_files[1], 0);
    arena_mode_alloc_file(&arena_mode_files[2], 1);
    arena_mode_alloc_file(&arena_mode_files[3], 1);
    arena_mode_alloc_file(&arena_mode_files[4], 1);
    cd_read_file_list(arena_mode_files, 0, 0);
    sequence = arena_mode_files[0].destination;
    arena_mode_sound_bank = arena_mode_files[1].destination;
    arena_display_set_resolution(0x140, 0xDA);
    arena_debug_stop_lines();
    model_set_envmap_mapping(1, 1, 0x40, 0x40);
    arena_mode_glow_in_vblank = 0;
    pad_set_vblank_hook(arena_mode_vblank_hook);
    console_load_font_cluts(0x7FFF, 0x8000);
    text_load_palette(0x140, 0xFF);
    arena_menu_init_pages();
    arena_bout_init();
    arena_glow_forget_buffers();
    arena_actor_model_files[1] = NULL;
    arena_actor_model_files[0] = NULL;
    arena_second_actor.object = NULL;
    arena_first_actor.object = NULL;
    arena_menu_free_screen_copy(1);
    cd_sync_reads(0);
    sound_add_effect_bank(arena_mode_sound_bank);
    if (arena_mode_own_seq != 0) {
        arena_mode_music_seq = sound_create_seq(sequence);
        sound_play_seq(arena_mode_music_seq, 0x7F, 0);
    } else {
        arena_mode_music_seq = (SoundSeq *)mode_music_seq;
    }
    text_unpack_lzss(arena_mode_files[3].destination, arena_stage_height_map);
    heap_free(arena_mode_files[3].destination);
    arena_stage_init();
    file = text_unpack_lzss_alloc(arena_mode_files[2].destination, 0);
    heap_free(arena_mode_files[2].destination);
    text_relocate_offset_table(file);
    arena_text_message_table = ((s32 *)file)[1];
    arena_actor_move_lists = (MoveList *)((s32 *)file)[2];
    arena_select_build_list(mode_arena_task_parameters == 1);
    file = text_unpack_lzss_alloc(arena_mode_files[4].destination, 1);
    heap_free(arena_mode_files[4].destination);
    text_relocate_offset_table(file);
    arena_stage_load_wall(file);
    arena_effect_load_textures(file);
    arena_text_load_font_and_banner(file);
    arena_stage_load_images(file);
    arena_hud_build_packets(file);
    arena_scene_load_sprite_sheet(file);
    heap_free(file);
    arena_select_alloc_portrait_slots();
    arena_scene_open_message_window();
    arena_task_yield();
    arena_node_compose_parent_view = 0;
restart:
    idle = 0x4650;
    heap_flush_delayed_frees();
    arena_node_free_tree(arena_first_actor.object);
    arena_node_free_tree(arena_second_actor.object);
    heap_flush_delayed_frees();
    arena_menu_enter_title();
    arena_mode_fade_level = 0xFF;
    arena_mode_glow_in_vblank = 0;
    arena_glow_init();
    arena_bout_clear_counters();
    window_reset(&arena_scene_message_window);
    arena_mode_next_state = -1;
    arena_retreat_rule_enabled = 0;
    arena_stage_index = 0;
    if (mode_arena_task_parameters == 1) {
        arena_mode_vblanks_per_frame = 0;
        step = 0;
    } else {
        if (mode_arena_task_parameters == 2) {
            arena_retreat_rule_enabled = 1;
            arena_rubber_band_enabled = 0;
        }
        mode_arena_task_parameters = 0;
        arena_settings.level = mode_arena_level;
        arena_settings.option6 = mode_arena_option6;
        arena_select_drop_portraits();
        arena_settings.com1 = 0;
        arena_mode_first_model_id = mode_arena_first_model;
        arena_mode_second_model_id = mode_arena_second_model;
        switch (mode_arena_entry_kind) {
        case 0:
            arena_settings.driven = 1;
            arena_mode_set_state(1);
            arena_play_mode = 1;
            arena_select_load_pick_portraits(arena_mode_first_model_id, arena_mode_second_model_id);
            if (arena_mode_second_model_id == 5) {
                arena_stage_index = 1;
            }
            break;
        case 1:
            arena_settings.driven = 1;
            arena_mode_set_state(1);
            arena_play_mode = 4;
            break;
        case 2:
            arena_settings.driven = 0;
            arena_mode_set_state(7);
            arena_play_mode = 5;
            arena_scene_start_tutorial();
            break;
        }
        arena_actor_load_model(0, arena_mode_first_model_id);
        step = 1;
        arena_actor_load_model(1, arena_mode_second_model_id);
        arena_menu_screen_done = 0;
        arena_mode_vblanks_per_frame = 2;
    }
    if (arena_mode_is_exiting()) {
        cd_sync_reads(0);
        step = 10;
    }
    if (step != 10) {
        do {
            if ((pad_port0_held & ~1) || (pad_port1_held & ~1)) {
                idle = 0x4650;
            }
            if ((pad_port0_held & 1) && arena_menu_is_title_shown()) {
                if (++hold == 0x78) {
                    idle = 0;
                }
            } else {
                hold = 0;
            }
            if (idle != 0) {
                idle--;
            } else if (arena_mode_fade_level != 0) {
                arena_select_load_picked_models();
                arena_mode_fade_level = 0;
                cd_sync_reads(0);
                arena_mode_start_demo();
                break;
            }
            arena_view_start_layer(rig);
            switch (step) {
            case 0:
                arena_menu_update();
                if (arena_menu_screen_done != 0) {
                    arena_mode_vblanks_per_frame = 2;
                    step = 1;
                }
                if (arena_frame_count & 1) {
                    arena_glow_step();
                }
                break;
            case 1:
                pad_merge_queued_states();
                if (!arena_mode_is_exiting()) {
                    arena_glow_step();
                    arena_select_draw_wheels(arena_current_ot, 0);
                }
                if (!cd_sync_reads(1)) {
                    step = 10;
                }
                break;
            }
            arena_view_draw_glow_and_menu(rig);
            arena_task_yield();
        } while (step != 10);
    }
    arena_mode_vblanks_per_frame = 0;
    if (arena_mode_is_exiting()) {
        arena_mode_fade_level = 0;
        arena_mode_glow_in_vblank = 0;
        arena_glow_free_buffers();
        arena_select_drop_portraits();
    }
    model = text_unpack_lzss_alloc(arena_actor_model_files[0], 0);
    heap_free(arena_actor_model_files[0]);
    arena_actor_model_files[0] = model;
    arena_mode_draw_fade_frame();
    model = text_unpack_lzss_alloc(arena_actor_model_files[1], 0);
    heap_free(arena_actor_model_files[1]);
    arena_actor_model_files[1] = model;
    arena_mode_draw_fade_frame();
    arena_actor_init_from_model_file(&arena_first_actor, arena_actor_model_files[0], 0);
    arena_mode_draw_fade_frame();
    arena_actor_init_from_model_file(&arena_second_actor, arena_actor_model_files[1], 1);
    arena_mode_draw_fade_frame();
    arena_effect_load_side_palette(&arena_first_actor.colour, &arena_second_actor.colour);
    arena_menu_dim_background();
    if (arena_mode_fade_level != 0) {
        arena_mode_glow_in_vblank = 1;
        sound_set_seq_tempo(arena_mode_music_seq, 0x158, 0);
        arena_bout_motion_speed = 0x200;
    }
    arena_bout_round_winner = 2;
new_bout:
    arena_bout_start_round();
    arena_first_actor.pos.vx = 0x3E80;
    arena_first_actor.pos.vy = 0;
    arena_first_actor.pos.vz = 0x3F80;
    arena_second_actor.pos.vx = 0x4080;
    arena_second_actor.pos.vy = 0;
    arena_second_actor.pos.vz = 0x3F80;
    for (;;) {
        console_set_color(0, 0xFF, 0);
        arena_mode_vblanks_per_frame = 2;
        arena_stage_uses_narrow_view = 0;
        arena_mode_state = arena_mode_next_state;
        arena_mode_step_fade();
        window_draw_frame(&arena_scene_message_window, (u_long *)arena_current_ot, arena_draw_buffer_index);
        switch (arena_mode_state) {
        case 1:
            arena_bout_update(&arena_first_actor, &arena_second_actor);
            SetGeomScreen(0xC0);
            if (arena_mode_next_state == 1) {
                arena_camera_frame_actors_for_bout(&arena_first_actor, &arena_second_actor);
            }
            arena_view_draw_arena(rig);
            break;
        case 6:
            arena_winner_update_screen(rig);
            break;
        case 7:
            arena_scene_update();
            SetGeomScreen(0xC0);
            arena_view_draw_arena(rig);
            if (arena_mode_next_state == 3) {
                arena_mode_vblanks_per_frame = 0;
                arena_task_yield();
                goto leave;
            }
            break;
        case 4:
            SetGeomScreen(0x800);
            arena_bout_update_replay(&arena_first_actor, &arena_second_actor);
            if (arena_mode_next_state != 8) {
                arena_camera_update_orbit(1);
                arena_stage_uses_narrow_view = 1;
            }
            arena_view_draw_arena(rig);
            break;
        case 8:
            SetGeomScreen(0x200);
            arena_bout_update_result_view(&arena_first_actor, &arena_second_actor);
            arena_view_draw_arena(rig);
            break;
        case 0:
            arena_menu_update();
            arena_view_draw_menu(rig);
            arena_mode_vblanks_per_frame = 0;
            break;
        case 5:
            pad_merge_queued_states();
            if (pad_port0_pressed != 0) {
                pad_port0_pressed = 0;
                pad_port0_repeated = 0;
                arena_mode_end_demo();
                arena_mode_vblanks_per_frame = 0;
                arena_menu_reset_background();
                arena_task_yield();
                goto restart;
            }
            arena_bout_update(&arena_first_actor, &arena_second_actor);
            SetGeomScreen(0xC0);
            arena_camera_frame_actors_for_bout(&arena_first_actor, &arena_second_actor);
            arena_view_draw_actors_on_backdrop(rig);
            break;
        case 3:
        leave:
            if (mode_arena_task_parameters == 0) {
                arena_mode_exit();
            }
            arena_menu_reset_background();
            goto restart;
        case 2:
            if (arena_settings.option6 != 0) {
                if (arena_first_actor.unkF2 == arena_settings.option6) {
                    if (arena_settings.com1) {
                        goto leave;
                    }
                    arena_mode_set_state(6);
                    break;
                }
                if (arena_second_actor.unkF2 == arena_settings.option6) {
                    if (arena_settings.driven) {
                        goto leave;
                    }
                    arena_mode_set_state(6);
                    break;
                }
            }
            arena_mode_set_state(1);
            goto new_bout;
        }
        console_place_cursor_and_line_start(0x9E, 0);
        console_set_color(0xFF, 0xFF, 0);
        arena_task_yield();
    }
}

/* 80085E34: Screen position of the left-hand gauge for a layout point. */
void arena_hud_place_left_gauge_point(DVECTOR *point, DVECTOR *out) {
    out->vx = point->vx + 0x18;
    out->vy = point->vy + 6;
    out->vx += 0x4F;
}

/* 80085E60: Screen position of the right-hand (mirrored) gauge for a layout point. */
void arena_hud_place_right_gauge_point(DVECTOR *point, DVECTOR *out) {
    out->vx = 0x8B - point->vx;
    out->vy = 0x20 - point->vy;
    out->vx += 0x4F;
}

/* 80085E90: Gauge x for a side (nonzero = mirrored). */
void arena_hud_set_gauge_x(s32 mirrored, s16 *out, s32 x) {
    if (mirrored) {
        *out = 0xDA - x;
    } else {
        *out = x + 0x67;
    }
}

/* 80085EAC: Gauge y for a side (nonzero = mirrored). */
void arena_hud_set_gauge_y(s32 mirrored, s16 *out, s32 y) {
    if (mirrored) {
        *out = 0x20 - y;
    } else {
        *out = y + 6;
    }
}

/* 80085EC8: Build one buffer's overlay packets: texture page modes, the frame
 * outlines and gauge quads of both sides (left from the corner layout,
 * right mirrored), the arrow triangles and the marks. The mirror loop runs
 * over six arrows and so also writes three past the array into the marks,
 * which are set afterwards; the second mirrored arrow then gets its first
 * corner one pixel up and its third one pixel left. Both black marks' heads
 * come first, then the first two marks' coordinates, one shared value at a
 * time (x0 = x2, x1, x3, y0 = y1, y2 = y3). So their shared code word
 * 0x28000000 is dead before any coordinate and takes $v0, while the length 5
 * (all four marks) and the 0x1E (marks[0].x0/x2, marks[1].y2/y3) live across
 * the coordinates and take $a0/$v1; the two decrement loads, register births
 * that sched1 puts late, take $v0/$v1, and sched2 lifts each load to just
 * after the last use of its register. */
void arena_hud_build_overlay_buffer(OverlayBuffer *buf) {
    s32 i;

    SetDrawTPage(&buf->tpage[0], 0, 1, GetTPage(0, 2, 0, 0));
    SetDrawTPage(&buf->tpage[1], 0, 0, GetTPage(0, 1, 0, 0));
    setlen(&buf->frame[0], 6);
    *(u32 *)&buf->frame[0].r0 = 0x4C000000;
    buf->frame[0].pad = 0x55555555;
    setlen(&buf->frame[1], 6);
    *(u32 *)&buf->frame[1].r0 = 0x4C000000;
    buf->frame[1].pad = 0x55555555;
    setlen(&buf->frame[2], 6);
    *(u32 *)&buf->frame[2].r0 = 0x4C000000;
    buf->frame[2].pad = 0x55555555;
    setlen(&buf->frame[3], 6);
    *(u32 *)&buf->frame[3].r0 = 0x4C000000;
    buf->frame[3].pad = 0x55555555;
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[0], (DVECTOR *)&buf->frame[0].x0);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[1], (DVECTOR *)&buf->frame[0].x1);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[2], (DVECTOR *)&buf->frame[0].x2);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->frame[0].x3);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->frame[1].x0);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[4], (DVECTOR *)&buf->frame[1].x1);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[5], (DVECTOR *)&buf->frame[1].x2);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[0], (DVECTOR *)&buf->frame[1].x3);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[0], (DVECTOR *)&buf->frame[2].x0);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[1], (DVECTOR *)&buf->frame[2].x1);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[2], (DVECTOR *)&buf->frame[2].x2);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->frame[2].x3);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->frame[3].x0);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[4], (DVECTOR *)&buf->frame[3].x1);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[5], (DVECTOR *)&buf->frame[3].x2);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[0], (DVECTOR *)&buf->frame[3].x3);
    MargePrim(&buf->frame[0], &buf->frame[1]);
    MargePrim(&buf->frame[2], &buf->frame[3]);
    buf->frame[0].x0 = 0x1D;
    buf->frame[2].x0 = 0x121;
    setlen(&buf->bars[0], 5);
    *(u32 *)&buf->bars[0].r0 = 0x280000FF;
    setlen(&buf->bars[1], 5);
    *(u32 *)&buf->bars[1].r0 = 0x280000FF;
    setlen(&buf->bars[2], 5);
    *(u32 *)&buf->bars[2].r0 = 0x280000FF;
    setlen(&buf->bars[3], 5);
    *(u32 *)&buf->bars[3].r0 = 0x280000FF;
    setlen(&buf->bars[4], 5);
    *(u32 *)&buf->bars[4].r0 = 0x280000FF;
    setlen(&buf->bars[5], 5);
    *(u32 *)&buf->bars[5].r0 = 0x280000FF;
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[0], (DVECTOR *)&buf->bars[0].x0);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[7], (DVECTOR *)&buf->bars[0].x1);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[5], (DVECTOR *)&buf->bars[0].x2);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[4], (DVECTOR *)&buf->bars[0].x3);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[7], (DVECTOR *)&buf->bars[1].x0);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[6], (DVECTOR *)&buf->bars[1].x1);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[4], (DVECTOR *)&buf->bars[1].x2);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->bars[1].x3);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[6], (DVECTOR *)&buf->bars[2].x0);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[1], (DVECTOR *)&buf->bars[2].x1);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->bars[2].x2);
    arena_hud_place_left_gauge_point(&arena_hud_gauge_frame_layout[2], (DVECTOR *)&buf->bars[2].x3);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[0], (DVECTOR *)&buf->bars[3].x0);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[7], (DVECTOR *)&buf->bars[3].x1);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[5], (DVECTOR *)&buf->bars[3].x2);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[4], (DVECTOR *)&buf->bars[3].x3);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[7], (DVECTOR *)&buf->bars[4].x0);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[6], (DVECTOR *)&buf->bars[4].x1);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[4], (DVECTOR *)&buf->bars[4].x2);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->bars[4].x3);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[6], (DVECTOR *)&buf->bars[5].x0);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[1], (DVECTOR *)&buf->bars[5].x1);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[3], (DVECTOR *)&buf->bars[5].x2);
    arena_hud_place_right_gauge_point(&arena_hud_gauge_frame_layout[2], (DVECTOR *)&buf->bars[5].x3);
    SetDrawTPage(&buf->bar_tpage, 0, 1, GetTPage(0, 1, 0, 0));
    arena_copy_words(buf->bars_dim, buf->bars, sizeof(buf->bars));
    arena_copy_words(buf->bars_lit, buf->bars, sizeof(buf->bars));
    for (i = 0; i < 6; i++) {
        setlen(&buf->bars_dim[i], 5);
        *(u32 *)&buf->bars_dim[i].r0 = 0x28806060;
    }
    for (i = 0; i < 6; i++) {
        setlen(&buf->bars_lit[i], 5);
        *(u32 *)&buf->bars_lit[i].r0 = 0x280000FF;
    }
    *(u32 *)&buf->arrows[0][0].x0 = 0x200014;
    *(u32 *)&buf->arrows[0][0].x1 = 0x20001C;
    *(u32 *)&buf->arrows[0][0].x2 = 0x28001C;
    *(u32 *)&buf->arrows[0][1].x0 = 0x200013;
    *(u32 *)&buf->arrows[0][1].x1 = 0x290013;
    *(u32 *)&buf->arrows[0][1].x2 = 0x29001B;
    *(u32 *)&buf->arrows[0][2].x0 = 0x320013;
    *(u32 *)&buf->arrows[0][2].x1 = 0x2A0013;
    *(u32 *)&buf->arrows[0][2].x2 = 0x2A001B;
    for (i = 0; i < 6; i++) {
        setlen(&buf->arrows[0][i], 4);
        *(u32 *)&buf->arrows[0][i].r0 = 0x2000FF00;
        setlen(&buf->arrows[1][i], 4);
        *(u32 *)&buf->arrows[1][i].r0 = 0x2000FF00;
        buf->arrows[1][i].y0 = buf->arrows[0][i].y0;
        buf->arrows[1][i].y1 = buf->arrows[0][i].y1;
        buf->arrows[1][i].y2 = buf->arrows[0][i].y2;
        buf->arrows[1][i].x0 = 0x140 - buf->arrows[0][i].x0;
        buf->arrows[1][i].x1 = 0x140 - buf->arrows[0][i].x1;
        buf->arrows[1][i].x2 = 0x140 - buf->arrows[0][i].x2;
    }
    buf->arrows[1][1].y0--;
    buf->arrows[1][1].x2--;
    setlen(&buf->marks[0], 5);
    *(u32 *)&buf->marks[0].r0 = 0x28000000;
    setlen(&buf->marks[1], 5);
    *(u32 *)&buf->marks[1].r0 = 0x28000000;
    buf->marks[0].x0 = buf->marks[0].x2 = 0x1E;
    buf->marks[0].x1 = 0x63;
    buf->marks[0].x3 = 0x5E;
    buf->marks[0].y0 = buf->marks[0].y1 = 9;
    buf->marks[0].y2 = buf->marks[0].y3 = 0x14;
    buf->marks[1].x0 = buf->marks[1].x2 = 0x122;
    buf->marks[1].x1 = 0xE3;
    buf->marks[1].x3 = 0xDE;
    buf->marks[1].y0 = buf->marks[1].y1 = 0x13;
    buf->marks[1].y2 = buf->marks[1].y3 = 0x1E;
    setlen(&buf->marks[2], 5);
    *(u32 *)&buf->marks[2].r0 = 0x280000FF;
    *(u32 *)&buf->marks[2].x0 = 0x320006;
    *(u32 *)&buf->marks[2].x1 = 0x36000A;
    *(u32 *)&buf->marks[2].x2 = 0x4C0006;
    *(u32 *)&buf->marks[2].x3 = 0x48000A;
    setlen(&buf->marks[3], 5);
    *(u32 *)&buf->marks[3].r0 = 0x280000FF;
    *(u32 *)&buf->marks[3].x0 = 0x32013A;
    *(u32 *)&buf->marks[3].x1 = 0x360136;
    *(u32 *)&buf->marks[3].x2 = 0x4C013A;
    *(u32 *)&buf->marks[3].x3 = 0x480136;
}

/* 800864B4: Build a textured quad (and its second-buffer copy) showing a whole TIM
 * image at (x, y); `depth` is the TIM colour mode (0 = 4-bit, 1 = 8-bit,
 * 2 = 16-bit), which sets how many pixels one VRAM word holds. */
void arena_hud_build_tim_quad(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth) {
    s32 scale;
    s32 right;

    switch (depth) {
    case 0:
        scale = 4;
        break;
    case 1:
        scale = 2;
        break;
    case 2:
        scale = 1;
        break;
    }
    setlen(quad, 9);
    quad->code = 0x2D;
    quad->clut = GetClut(tim->crect->x, tim->crect->y);
    quad->tpage = GetTPage(depth, 0, tim->prect->x, tim->prect->y);
    quad->x0 = quad->x2 = x;
    right = x + tim->prect->w * scale;
    if (scale == 2) {
        quad->x3 = right + 1;
    } else {
        quad->x3 = right;
    }
    quad->y0 = quad->y1 = y;
    quad->x1 = quad->x3 = quad->x3; /* the original stores x3 again */
    quad->y2 = quad->y3 = tim->prect->h + y;
    quad->u0 = quad->u2 = tim->prect->x * scale;
    quad->u1 = quad->u3 = (tim->prect->x + tim->prect->w) * scale;
    quad->v0 = quad->v1 = tim->prect->y;
    quad->v2 = quad->v3 = tim->prect->y + tim->prect->h;
    quad[1] = quad[0];
}

/* 800866D4: The same quad mirrored horizontally (texture u runs right to left). */
void arena_hud_build_mirrored_tim_quad(TIM_IMAGE *tim, s32 x, s32 y, POLY_FT4 *quad, s32 depth) {
    s32 scale;
    s32 right;

    switch (depth) {
    case 0:
        scale = 4;
        break;
    case 1:
        scale = 2;
        break;
    case 2:
        scale = 1;
        break;
    }
    setlen(quad, 9);
    quad->code = 0x2D;
    quad->clut = GetClut(tim->crect->x, tim->crect->y);
    quad->tpage = GetTPage(depth, 0, tim->prect->x, tim->prect->y);
    right = x + tim->prect->w * scale;
    quad->x1 = quad->x3 = x;
    quad->x0 = quad->x2 = right;
    quad->u0 = quad->u2 = tim->prect->x * scale - 1;
    quad->u1 = quad->u3 = (tim->prect->x + tim->prect->w) * scale - 1;
    quad->y0 = quad->y1 = y;
    quad->y2 = quad->y3 = tim->prect->h + y;
    quad->v0 = quad->v1 = tim->prect->y;
    quad->v2 = quad->v3 = tim->prect->y + tim->prect->h;
    quad[1] = quad[0];
}

/* 800868E0: Build the HUD packets: the two name plates (left and mirrored right)
 * from the name TIM, the icon and gauge sprites, the gauge bar quads from
 * the bar TIM, the HUD texture page modes and the gauge palette. One sprite
 * pair pointer walks the icons and then the gauges (the original keeps it
 * in $s2). Each bar quad gets its colour/code word and then its length, and
 * the gauge sprite its length, code, size and texture position. */
void arena_hud_build_packets(MenuImageFile *files) {
    TIM_IMAGE tim;
    RECT rect;
    s16 *clut;
    Hud *hud = &arena_hud_packets;
    SpritePair *pair;
    POLY_FT4 *bar;

    OpenTIM(files->name);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    clut[1] = 0x8000;
    LoadImage(tim.crect, tim.caddr);
    pair = hud->icon;
    LoadImage(tim.prect, tim.paddr);
    arena_hud_build_tim_quad(&tim, 6, 7, hud->name_l, 0);
    arena_hud_build_mirrored_tim_quad(&tim, 0x13A - tim.prect->w * 4, 7, hud->name_r, 0);
    SetDrawTPage(&arena_hud_tpages[0], 0, 1, GetTPage(1, 0, 0x380, 0x100));
    arena_hud_tpages[1] = arena_hud_tpages[0];
    SetDrawTPage(&arena_hud_tpages[2], 0, 1, GetTPage(0, 0, 0x380, 0x100));
    arena_hud_tpages[3] = arena_hud_tpages[2];
    setlen(&pair[0].s[0], 4);
    pair[0].s[0].code = 0x65;
    *(u32 *)&pair[0].s[0].x0 = 0x90007;
    *(u16 *)&pair[0].s[0].u0 = 0;
    *(u32 *)&pair[0].s[0].w = 0x160016;
    pair[0].s[0].clut = GetClut(0, 0x1F6);
    arena_hud_packets.icon[2] = arena_hud_packets.icon[1] = pair[0];
    pair = &arena_hud_packets.icon[2];
    pair[0].s[0].x0 = 0x123;
    pair[0].s[0].u0 = 0x20;
    pair[0].s[0].clut = GetClut(0, 0x1F7);
    pair[1] = pair[0];
    OpenTIM(files->bar);
    ReadTIM(&tim);
    bar = arena_hud_packets.bar_l;
    pair = arena_hud_packets.gauge;
    LoadImage(tim.prect, tim.paddr);
    arena_hud_build_tim_quad(&tim, 6, 0x20, bar, 0);
    arena_hud_build_mirrored_tim_quad(&tim, 0x13A - tim.prect->w * 4, 0x20, arena_hud_packets.bar_r, 0);
    arena_hud_left_charge_bar_v = arena_hud_packets.bar_l[0].v0;
    arena_hud_unread_right_charge_bar_v = arena_hud_packets.bar_r[0].v0;
    *(u32 *)&arena_hud_packets.bar_l[0].r0 = 0x2C000080;
    setlen(&arena_hud_packets.bar_l[0], 9);
    *(u32 *)&arena_hud_packets.bar_l[1].r0 = 0x2C000080;
    setlen(&arena_hud_packets.bar_l[1], 9);
    *(u32 *)&arena_hud_packets.bar_r[0].r0 = 0x2C000080;
    setlen(&arena_hud_packets.bar_r[0], 9);
    *(u32 *)&arena_hud_packets.bar_r[1].r0 = 0x2C000080;
    setlen(&arena_hud_packets.bar_r[1], 9);
    setlen(&pair[0].s[0], 4);
    pair[0].s[0].code = 0x65;
    *(u32 *)&pair[0].s[0].w = 0x80040;
    *(u16 *)&pair[0].s[0].u0 = 0x80;
    arena_hud_packets.bar_l[1].clut = arena_hud_packets.name_l[0].clut;
    arena_hud_packets.bar_l[0].clut = arena_hud_packets.name_l[0].clut;
    arena_hud_packets.bar_r[1].clut = arena_hud_packets.name_l[0].clut;
    arena_hud_packets.bar_r[0].clut = arena_hud_packets.name_l[0].clut;
    pair[0].s[0].clut = GetClut(0x3A0, 0x110);
    *(u32 *)&pair[0].s[0].x0 = 0xB001E;
    pair[2] = pair[0];
    pair = &arena_hud_packets.gauge[2];
    *(u16 *)&pair[0].s[0].u0 = 0x880;
    *(u32 *)&pair[0].s[0].x0 = 0x1500E3;
    pair[-1] = pair[-2];
    arena_hud_packets.gauge[3] = arena_hud_packets.gauge[2];
    rect.x = 0x3A0;
    rect.y = 0x110;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, (u_long *)arena_hud_gauge_palette);
}

/* 80086E24: Link this buffer's overlay packets into the overlay ordering table. */
void arena_hud_link_overlay_tpage(void) {
    AddPrim(arena_current_ot, &arena_hud_overlay_buffers[arena_draw_buffer_index].tpage[1]);
}

/* 80086E70: Link a gauge bar filled to `value`: the first part up to 0x38, a sloped
 * second part up to 0x48, then the third part. */
void arena_hud_draw_gauge_bar(void *ot, GaugeBar *bar, s32 value, s32 mirrored) {
    s32 over;

    if (value >= 0x38) {
        arena_hud_set_gauge_x(mirrored, &bar->parts[0].x1, 0x38);
        arena_hud_set_gauge_x(mirrored, &bar->parts[0].x3, 0x34);
        AddPrim(ot, &bar->parts[0]);
    } else {
        arena_hud_set_gauge_x(mirrored, &bar->parts[0].x1, value);
        arena_hud_set_gauge_x(mirrored, &bar->parts[0].x3, value - 4);
        AddPrim(ot, &bar->parts[0]);
        return;
    }
    if (value >= 0x48) {
        arena_hud_set_gauge_x(mirrored, &bar->parts[1].x1, 0x48);
        arena_hud_set_gauge_x(mirrored, &bar->parts[1].x3, 0x40);
        arena_hud_set_gauge_y(mirrored, &bar->parts[1].y3, 0x10);
        AddPrim(ot, &bar->parts[1]);
    } else {
        arena_hud_set_gauge_x(mirrored, &bar->parts[1].x1, value);
        over = value - 0x38;
        arena_hud_set_gauge_x(mirrored, &bar->parts[1].x3, value - (over / 4 + 4));
        arena_hud_set_gauge_y(mirrored, &bar->parts[1].y3, over / 2 + 8);
        AddPrim(ot, &bar->parts[1]);
        return;
    }
    arena_hud_set_gauge_x(mirrored, &bar->parts[2].x1, value);
    arena_hud_set_gauge_x(mirrored, &bar->parts[2].x3, value - 8);
    AddPrim(ot, &bar->parts[2]);
}

/* 80086FF8: Colour a marker packet by an actor's state: none (returns 0),
 * yellow when set, red otherwise. */
s32 arena_hud_color_charge_mark(Actor *actor, POLY_F4 *packet) {
    if (arena_brain_check_special_charge(actor, 0)) {
        return 0;
    }
    if (arena_brain_check_special_charge(actor, 1)) {
        setlen(packet, 5);
        *(u32 *)&packet->r0 = 0x2800FFFF;
    } else {
        setlen(packet, 5);
        *(u32 *)&packet->r0 = 0x280000FF;
    }
    return 1;
}

/* 80087068: Link the HUD and map overlay for this frame: each side's arrows (one per
 * point), state marks, name plates, icons, gauges, the charge bars (flashing
 * when nearly full) and the level bars (tinted by the level). The level
 * tints are written as loop-invariant expressions inside the loops (the
 * original's moved invariants include a copy of the repeated green term). */
void arena_hud_draw(Actor *left, Actor *right) {
    OverlayBuffer *buf;
    POLY_FT4 *bar;
    s32 n;
    s32 level;

    buf = &arena_hud_overlay_buffers[arena_draw_buffer_index];
    n = left->unkF2;
    if (n > 0) {
        AddPrim(arena_current_ot, &buf->arrows[0][0]);
    }
    if (n > 1) {
        AddPrim(arena_current_ot, &buf->arrows[0][1]);
    }
    if (n > 2) {
        AddPrim(arena_current_ot, &buf->arrows[0][2]);
    }
    n = right->unkF2;
    if (n > 0) {
        AddPrim(arena_current_ot, &buf->arrows[1][0]);
    }
    if (n > 1) {
        AddPrim(arena_current_ot, &buf->arrows[1][1]);
    }
    if (n > 2) {
        AddPrim(arena_current_ot, &buf->arrows[1][2]);
    }
    if (arena_hud_color_charge_mark(left, &buf->marks[2])) {
        AddPrim(arena_current_ot, &buf->marks[2]);
    }
    if (arena_hud_color_charge_mark(right, &buf->marks[3])) {
        AddPrim(arena_current_ot, &buf->marks[3]);
    }
    AddPrim(arena_current_ot, &arena_hud_packets.name_l[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.name_r[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.icon[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.icon[2 + arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_tpages[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.gauge[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.gauge[2 + arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_tpages[2 + arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.bar_r[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &arena_hud_packets.bar_l[arena_draw_buffer_index]);
    AddPrim(arena_current_ot, &buf->marks[0]);
    AddPrim(arena_current_ot, &buf->marks[1]);
    AddPrim(arena_current_ot, &buf->frame[0]);
    AddPrim(arena_current_ot, &buf->frame[2]);
    arena_hud_draw_gauge_bar(arena_current_ot, (GaugeBar *)&buf->bars[0], arena_first_actor.unkC0, 0);
    arena_hud_draw_gauge_bar(arena_current_ot, (GaugeBar *)&buf->bars[3], arena_second_actor.unkC0, 1);
    n = left->charge >> 6;
    bar = &arena_hud_packets.bar_l[arena_draw_buffer_index];
    if (n > 0x38) {
        bar->r0 = pad_vblank_count << 3;
    } else {
        bar->r0 = 0xFF;
    }
    bar->y0 = bar->y1 = 0x60 - n;
    bar->v0 = bar->v1 = arena_hud_left_charge_bar_v - (n - 0x40);
    n = right->charge >> 6;
    bar = &arena_hud_packets.bar_r[arena_draw_buffer_index];
    if (n > 0x38) {
        bar->r0 = pad_vblank_count << 3;
    } else {
        bar->r0 = 0xFF;
    }
    bar->y0 = bar->y1 = 0x60 - n;
    bar->v0 = bar->v1 = arena_hud_left_charge_bar_v - (n - 0x40);
    if (left->level != 0) {
        level = 0x100 - left->level;
        for (n = 0; n < 3; n++) {
            buf->bars_lit[n].r0 = ((level * 3) >> 3) + ((left->level * 255) >> 8);
            buf->bars_lit[n].g0 = (level * 3) >> 3;
            buf->bars_lit[n].b0 = level >> 1;
        }
        arena_hud_draw_gauge_bar(arena_current_ot, (GaugeBar *)&buf->bars_lit[0], left->unkC1, 0);
    }
    if (right->level != 0) {
        level = 0x100 - right->level;
        for (n = 3; n < 6; n++) {
            buf->bars_lit[n].r0 = ((level * 3) >> 3) + ((right->level * 255) >> 8);
            buf->bars_lit[n].g0 = (level * 3) >> 3;
            buf->bars_lit[n].b0 = level >> 1;
        }
        arena_hud_draw_gauge_bar(arena_current_ot, (GaugeBar *)&buf->bars_lit[3], right->unkC1, 1);
    }
    for (n = 0; n < 6; n++) {
        AddPrim(arena_current_ot, &buf->bars_dim[n]);
    }
    AddPrim(arena_current_ot, buf);
}

/* 800875EC: Build the overlay packets for buffer 0 and copy them to buffer 1. */
void arena_hud_build_overlay_buffers(void) {
    arena_hud_build_overlay_buffer(&arena_hud_overlay_buffers[0]);
    arena_hud_overlay_buffers[1] = arena_hud_overlay_buffers[0];
}

/* 80087650: Empty the map's row spans (left 0xFF, right 0). */
void arena_stage_clear_row_spans(void) {
    s32 row;

    for (row = 0; row < 0x80; row++) {
        arena_stage_span_right_columns[row] = 0;
        arena_stage_span_left_columns[row] = 0xFF;
    }
}

/* 80087698: Widen the map's row spans along a line, clamped to each row's limits. */
void arena_stage_widen_row_spans(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 x;
    s32 step;
    s32 row;
    s32 column;
    s32 swap;

    if (y0 == y1) {
        return;
    }
    if (y1 < y0) {
        swap = x1;
        x1 = x0;
        x0 = swap;
        swap = y1;
        y1 = y0;
        y0 = swap;
    }
    x = x0 << 8;
    step = ((x1 - x0) << 8) / (y1 - y0);
    for (row = y0; row < y1; row++, x += step) {
        if (row < 0) {
            continue;
        }
        if (row >= 0x80) {
            return;
        }
        column = x >> 8;
        if (column < arena_stage_span_left_columns[row]) {
            if (column < arena_stage_row_left_limits[row]) {
                column = arena_stage_row_left_limits[row];
            }
            arena_stage_span_left_columns[row] = column;
        }
        if (arena_stage_span_right_columns[row] < column) {
            if (arena_stage_row_right_limits[row] + 1 < column) {
                column = arena_stage_row_right_limits[row] + 1;
            }
            arena_stage_span_right_columns[row] = column;
        }
    }
}

/* 8008779C: Draw the map triangles: load the map colour (as a textured-triangle
 * code) into the GTE, copy the 0x30-byte map table into the scratchpad and
 * run the triangle loop. */
void arena_stage_draw_ground(u32 *ot, s32 originX, s32 originZ) {
    model_color = (model_color & 0xFFFFFF) | 0x24000000;
    gte_ldrgb(&model_color);
    arena_copy_words((void *)0x1F800120, &arena_stage_ground_draw_table, sizeof(MapTable));
    arena_stage_draw_ground_cells(ot, originX, originZ);
}

/* 80087830: Set up the map row spans in the scratchpad and the two textured
 * triangle packet pools (0x708 triangles each). */
void arena_stage_init_ground_pools(void) {
    POLY_FT3 *poly;
    s32 i;

    arena_stage_span_right_columns = (u8 *)0x1F800000;
    arena_stage_span_left_columns = (u8 *)0x1F800080;
    arena_stage_ground_triangles[0] = heap_alloc(0xE100, 0);
    arena_stage_ground_triangles[1] = heap_alloc(0xE100, 0);
    poly = arena_stage_ground_triangles[0];
    for (i = 0; i < 0x708; i++) {
        setlen(poly, 7);
        poly->code = 0x24;
        poly++;
    }
    arena_copy_words(arena_stage_ground_triangles[1], arena_stage_ground_triangles[0], 0xE100);
}

/* 800878DC: Load the stage's icon, backdrop and extra TIM images into VRAM, noting
 * the icon and backdrop palettes and texture pages; the backdrop palette's
 * first entry is transparent and the rest semi-transparent. */
void arena_stage_load_images(MenuImageFile *files) {
    TIM_IMAGE tim;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s16 *clut;
    s32 i;

    for (i = 0; i < 4; i++) {
        OpenTIM(files->icons[i]);
        ReadTIM(&tim);
        arena_stage_ground_draw_table.icons[i * 2 + 1] = GetClut(tim.crect->x, tim.crect->y);
        arena_stage_ground_draw_table.icons[i * 2] = GetTPage(1, 1, tim.prect->x, tim.prect->y);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
    OpenTIM(files->backdrop);
    ReadTIM(&tim);
    arena_stage_shadow_clut = GetClut(tim.crect->x, tim.crect->y);
    arena_stage_shadow_tpage = GetTPage(0, 2, tim.prect->x, tim.prect->y);
    arena_stage_shadow_u = tim.prect->x * 4;
    arena_stage_shadow_v = tim.prect->y;
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    for (i = 0x1C; i < 0x25; i++) {
        OpenTIM(files->extra[i - 0x1C]);
        ReadTIM(&tim);
        LoadImage(tim.crect, tim.caddr);
        LoadImage(tim.prect, tim.paddr);
    }
}

/* 80087AB0: Build an actor's textured backdrop quad (64x64 texels) for both buffers. */
void arena_stage_build_shadow_quad(Actor *actor) {
    POLY_FT4 *quad = &actor->backdrop[0];

    *(u32 *)&quad->r0 = 0x2C101010;
    setlen(quad, 9);
    quad->code |= 2;
    quad->clut = arena_stage_shadow_clut;
    quad->tpage = arena_stage_shadow_tpage;
    *(u16 *)&quad->u0 = arena_stage_shadow_u | (arena_stage_shadow_v << 8);
    *(u16 *)&quad->u1 = (arena_stage_shadow_u + 0x3F) | (arena_stage_shadow_v << 8);
    *(u16 *)&quad->u2 = arena_stage_shadow_u | ((arena_stage_shadow_v + 0x3F) << 8);
    *(u16 *)&quad->u3 = (arena_stage_shadow_u + 0x3F) | ((arena_stage_shadow_v + 0x3F) << 8);
    actor->backdrop[1] = actor->backdrop[0];
}

/* 80087B74: Draw an actor's ground shadow: a square sized by its height, centred
 * under it and tilted to the ground normal there. */
void arena_stage_draw_shadow(Actor *actor, u32 *ot, MATRIX *view) {
    SVECTOR corners[4];
    VECTOR centre;
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    SVECTOR normal;
    MATRIX m;
    s32 otz;
    s32 z0, z1, z2, z3;
    POLY_FT4 *quad;
    s32 size;
    s32 min;

    if ((actor->flags & 0x60000000) != 0x20000000) {
        size = (actor->home.vy + actor->unk92C.vy) / 32 + 0x90;
        if (size > 0) {
            centre.vx = (actor->home.vx + actor->unk92C.vx) / 2;
            centre.vz = (actor->home.vz + actor->unk92C.vz) / 2;
            centre.vy = 0;
            corners[0].vx = corners[1].vx = corners[0].vz = corners[2].vz = -size;
            corners[3].vx = corners[2].vx = corners[1].vz = corners[3].vz = size;
            corners[0].vy = corners[1].vy = corners[2].vy = corners[3].vy = 0;
            quad = &actor->backdrop[arena_draw_buffer_index];
            m.t[0] = centre.vx - arena_view_origin.vx;
            m.t[1] = arena_stage_get_ground_height(&centre, 0);
            m.t[2] = centre.vz - arena_view_origin.vz;
            arena_stage_get_ground_normal(&normal);
            m.m[0][0] = 0x1000;
            m.m[0][1] = 0;
            m.m[0][2] = 0;
            m.m[1][0] = normal.vx;
            m.m[1][1] = normal.vy;
            m.m[1][2] = normal.vz;
            m.m[2][0] = 0;
            m.m[2][1] = 0;
            m.m[2][2] = 0x1000;
            CompMatrix(view, &m, &m);
            gte_SetRotMatrix(&m);
            gte_SetTransMatrix(&m);
            gte_ldv3c(corners);
            gte_rtpt();
            gte_nclip();
            gte_stopz(&otz);
            if (otz >= 0) {
                gte_stsz3(&z0, &z1, &z2);
                gte_stsxy3_ft4(quad);
                gte_ldv0(&corners[3]);
                gte_rtps();
                gte_stsz(&z3);
                gte_stsxy(&quad->x3);
                min = z0;
                if (z1 < min) {
                    min = z1;
                }
                if (z2 < min) {
                    min = z2;
                }
                if (z3 < min) {
                    min = z3;
                }
                otz = min >> 4;
                AddPrim(ot + otz, quad);
            }
        }
    }
}

/* 80087E38: Record a position in the path list (up to 31 entries). */
void arena_debug_record_path_point(VECTOR *pos) {
    s32 count = arena_debug_path_marker_count;
    s16 *base;
    s16 *at;

    if (count < 0x1F) {
        base = &arena_debug_path_markers[0].x;
        at = base + count * (sizeof(PathMarker) / sizeof(s16));
        at[0] = pos->vx;
        at[1] = pos->vy;
        arena_debug_path_marker_count = count + 1;
        at[2] = pos->vz;
    }
}

/* 80087EA0: Draw the recorded path points as axis crosses (64 units long), then
 * clear the list. */
void arena_debug_draw_path_markers(u32 *ot) {
    SVECTOR ends[6];
    PathMarker *point;
    s32 i;
    s32 j;

    for (i = 0; i < arena_debug_path_marker_count; i++) {
        point = &arena_debug_path_markers[i];
        ends[0].vx = point->x - 0x20;
        ends[0].vy = point->y;
        ends[0].vz = point->z;
        ends[1].vx = point->x + 0x20;
        ends[1].vy = point->y;
        ends[1].vz = point->z;
        ends[2].vx = point->x;
        ends[2].vy = point->y - 0x20;
        ends[2].vz = point->z;
        ends[3].vx = point->x;
        ends[3].vy = point->y + 0x20;
        ends[3].vz = point->z;
        ends[4].vx = point->x;
        ends[4].vy = point->y;
        ends[4].vz = point->z - 0x20;
        ends[5].vx = point->x;
        ends[5].vy = point->y;
        ends[5].vz = point->z + 0x20;
        for (j = 0; j < 6; j++) {
            ends[j].vx -= arena_view_origin.vx;
            ends[j].vy -= arena_view_origin.vy;
            ends[j].vz -= arena_view_origin.vz;
        }
        gte_ldv3(&ends[0], &ends[1], &ends[2]);
        gte_rtpt();
        gte_stsxy3(&point->axes[arena_draw_buffer_index][0].x0, &point->axes[arena_draw_buffer_index][0].x1,
                   &point->axes[arena_draw_buffer_index][1].x0);
        gte_ldv3(&ends[3], &ends[4], &ends[5]);
        gte_rtpt();
        gte_stsxy3(&point->axes[arena_draw_buffer_index][1].x1, &point->axes[arena_draw_buffer_index][2].x0,
                   &point->axes[arena_draw_buffer_index][2].x1);
        setlen(&point->axes[arena_draw_buffer_index][0], 3);
        *(u32 *)&point->axes[arena_draw_buffer_index][0].r0 = 0x400000FF;
        setlen(&point->axes[arena_draw_buffer_index][1], 3);
        *(u32 *)&point->axes[arena_draw_buffer_index][1].r0 = 0x4000FF00;
        setlen(&point->axes[arena_draw_buffer_index][2], 3);
        *(u32 *)&point->axes[arena_draw_buffer_index][2].r0 = 0x40FF0000;
        gpu_ot_link_line_f2((u_long *)ot, &point->axes[arena_draw_buffer_index][0]);
        gpu_ot_link_line_f2((u_long *)ot, &point->axes[arena_draw_buffer_index][1]);
        gpu_ot_link_line_f2((u_long *)ot, &point->axes[arena_draw_buffer_index][2]);
    }
    arena_debug_path_marker_count = 0;
}

/* 8008820C: Start a debug line between two points in one of eight colours (bit 0
 * blue, bit 1 red, bit 2 green). Returns the line, or NULL when all 100
 * are in use. */
Line3D *arena_debug_start_line(VECTOR *from, VECTOR *to, s32 colour) {
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        line = &arena_debug_lines[i];
        if (line->timer == 0) {
            line->timer = 1;
            line->from.vx = from->vx;
            line->from.vy = from->vy;
            line->from.vz = from->vz;
            line->to.vx = to->vx;
            line->to.vy = to->vy;
            line->to.vz = to->vz;
            line->packets[0].r0 = (colour & 2) * 0x7F;
            line->packets[0].g0 = (colour & 4) * 0x3F;
            line->packets[0].b0 = (colour & 1) * 0xFF;
            line->packets[1].r0 = (colour & 2) * 0x7F;
            line->packets[1].g0 = (colour & 4) * 0x3F;
            line->packets[1].b0 = (colour & 1) * 0xFF;
            return line;
        }
    }
    return NULL;
}

/* 800882D4: Start a debug line that stays for the given number of frames. */
void arena_debug_start_timed_line(VECTOR *from, VECTOR *to, s32 colour, s32 frames) {
    Line3D *line = arena_debug_start_line(from, to, colour);

    if (line != NULL) {
        line->timer = frames;
    }
}

/* 80088308: Stop every debug line. */
void arena_debug_stop_lines(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        arena_debug_lines[i].timer = 0;
    }
}

/* 8008832C: Project and link every live debug line, counting its frames down. */
void arena_debug_draw_lines(void *ot) {
    SVECTOR ends[2];
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        if (arena_debug_lines[i].timer != 0) {
            line = &arena_debug_lines[i];
            line->timer--;
            ends[0] = line->from;
            ends[1] = line->to;
            ends[0].vx -= arena_view_origin.vx;
            ends[0].vy -= arena_view_origin.vy;
            ends[0].vz -= arena_view_origin.vz;
            ends[1].vx -= arena_view_origin.vx;
            ends[1].vy -= arena_view_origin.vy;
            ends[1].vz -= arena_view_origin.vz;
            gte_ldv01(&ends[0], &ends[1]);
            gte_rtpt();
            gte_stsxy01(&line->packets[arena_draw_buffer_index].x0, &line->packets[arena_draw_buffer_index].x1);
            setlen(&line->packets[arena_draw_buffer_index], 3);
            setcode(&line->packets[arena_draw_buffer_index], 0x40);
            gpu_ot_link_line_f2(ot, &line->packets[arena_draw_buffer_index]);
        }
    }
}

/* 800884E0: Scale a vector down by the square root of its (absolute) length measure
 * and pass it on. */
void arena_vector_normalize(VECTOR *vector, void *out) {
    VECTOR scaled = *vector;
    s32 square;
    s32 length;

    square = model_get_largest_component(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormal(&scaled, out);
}

/* 8008859C: Scale a vector down by the square root of its (absolute) length measure
 * and pass it to VectorNormalS. */
void arena_vector_normalize_to_svector(VECTOR *vector, void *out) {
    VECTOR scaled = *vector;
    s32 square;
    s32 length;

    square = model_get_largest_component(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormalS(&scaled, out);
}

/* 80088658: The same for a short vector. */
void arena_vector_normalize_svector(SVECTOR *vector, void *out) {
    VECTOR scaled;
    s32 square;
    s32 length;

    scaled.vx = vector->vx;
    scaled.vy = vector->vy;
    scaled.vz = vector->vz;
    square = model_get_largest_component(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = SquareRoot0(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    VectorNormalS(&scaled, out);
}

/* 800886FC: Length of a vector. */
s32 arena_vector_get_length(VECTOR *vector) {
    VECTOR square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return SquareRoot0(square.vx + square.vy + square.vz);
}

/* 80088754: Horizontal (x/z) length of a vector. */
s32 arena_vector_get_flat_length(VECTOR *vector) {
    VECTOR square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return SquareRoot0(square.vx + square.vz);
}

/* 800887A4: Distance between two points. */
s32 arena_vector_get_distance(VECTOR *from, VECTOR *to) {
    VECTOR delta;

    delta.vx = to->vx - from->vx;
    delta.vy = to->vy - from->vy;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return SquareRoot0(delta.vx + delta.vy + delta.vz);
}

/* 80088838: Horizontal (x/z) distance between two points. */
s32 arena_vector_get_flat_distance(VECTOR *from, VECTOR *to) {
    VECTOR delta;

    delta.vx = to->vx - from->vx;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return SquareRoot0(delta.vx + delta.vz);
}

/* 800888B0: Set a bit of the resident flag array. */
void arena_progress_set_flag(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    game_data.progress[flag >> 3] |= bit;
}

/* 800888E4: Test a bit of the resident flag array. */
s32 arena_progress_is_flag_set(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    return game_data.progress[flag >> 3] & bit;
}

/* 80088908: Clear a bit of the resident flag array. */
void arena_progress_clear_flag(s32 flag) {
    s32 bit;

    bit = 1;
    bit <<= flag & 7;
    game_data.progress[flag >> 3] &= ~bit;
}

/* 80088940: Set bit 16 of the resident state word. */
void arena_progress_mark_complete(void) {
    game_data.options.complete = 1;
}

/* 8008895C: Once bit 16 of the resident state word is set, queue list entry 22
 * (ARGENTO, only once). */
void arena_progress_add_unlocked_entry(void) {
    if (game_data.options.complete && arena_progress_unlock_added == 0) {
        arena_progress_unlock_added = 1;
        arena_select_entries[arena_select_entry_count++] = &arena_select_gears[22];
    }
}

/* 800889C8: Once every progress flag 0..48 except 22 is set, mark the options
 * complete and apply the unlock. */
s32 arena_progress_check_complete(void) {
    s32 flag;

    for (flag = 0; flag < 49; flag++) {
        if (flag != 22 && !arena_progress_is_flag_set(flag)) {
            return 0;
        }
    }
    game_data.options.complete = 1;
    arena_progress_add_unlocked_entry();
    return 0;
}

/* 80088A40: Store the current option settings in the saved options word. */
void arena_settings_save_to_game_data(void) {
    if (mode_arena_task_parameters) {
        game_data.options.version = 1;
        game_data.options.option4 = arena_settings.option4;
        game_data.options.option5 = arena_settings.option5;
        game_data.options.option6 = arena_settings.option6;
        game_data.options.option13 = arena_settings.level;
    }
}

/* 80088AF8: Load the option settings from the saved options word, or write the
 * defaults when it was never written; a completed word clears the flags. */
void arena_settings_load_from_game_data(void) {
    s32 i;

    if (mode_arena_task_parameters) {
        arena_progress_unlock_added = 0;
        if (game_data.options.version == 1) {
            arena_settings.option4 = game_data.options.option4;
            arena_settings.option5 = game_data.options.option5;
            arena_settings.option6 = game_data.options.option6;
            arena_settings.level = game_data.options.option13;
            if (game_data.options.complete) {
                for (i = 0; i < 8; i++) {
                    game_data.progress[i] = 0;
                }
            }
        } else {
            arena_settings.option4 = 0;
            arena_settings.option5 = 0;
            arena_settings.option6 = 2;
            arena_settings.level = 0;
            arena_settings_save_to_game_data();
        }
    }
}

/* 80088BD4: Set a progress flag, then check for completion. */
void arena_progress_set_flag_and_check(s32 flag) {
    arena_progress_set_flag(flag);
    arena_progress_check_complete();
}
