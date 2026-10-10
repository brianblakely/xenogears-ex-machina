/* Field unit 800A4748-800A9274: screen effects and transitions, the field
 * reload on a map change, the screen pieces and grid, movies, the status
 * panel and the saved VRAM column.
 *
 * 800a5c40's jump table (rodata 0x2bc) is 4 mod 8 between 800a1bd0's (0x268)
 * and 800ab748's (0x2e8), both 0 mod 8, so a unit starts after 800a1bd0 with
 * its rodata at 0x294 ("EVENTLOOP ERROR", 800a1ec8), 0x2ac ("SAVESIZE",
 * 800a3f4c) or 0x2bc, and another starts after 800a5c40 with its rodata at
 * 0x2d8 (800a9688) or 0x2e8. This file takes the 0x2bc start, the text from
 * 800a4748 where the event runner and state snapshots give way to the
 * screen effects; the next unit is placed at the particle effects. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "field/monitor.h"
#include "field.h"
#include "field_dialogue.h"
#include "field_draw.h"
#include "field_effect.h"
#include "field_event.h"
#include "field_glyph.h"
#include "field_layer.h"
#include "field_load.h"
#include "field_mode.h"
#include "field_motion.h"
#include "field_movie.h"
#include "field_pad.h"
#include "field_panel.h"
#include "field_screen.h"
#include "field_sound.h"

/* The saved strip sources (x, y) of the screen effects. */
DVECTOR field_distortion_strip_sources[15] = { /* 800AEB24 */
    {0, 0xB0}, {0x40, 0xB0}, {0x80, 0xB0}, {0xC0, 0xB0}, {0x100, 0xB0},
    {0, 0xC0}, {0x40, 0xC0}, {0x80, 0xC0}, {0xC0, 0xC0}, {0x100, 0xC0},
    {0, 0xD0}, {0x40, 0xD0}, {0x80, 0xD0}, {0xC0, 0xD0}, {0x100, 0xD0},
};

s32 field_status_panel_draw_count = 0; /* 800AEB60: panel frame counter */
s32 field_status_panel_blink_phase = 0; /* 800AEB64: panel blinking frame 0..2 */

/* The status panel's texture frames (u, v, w, h) and its layout pieces
 * (x, y, frame, flags). */
PanelFrame field_status_panel_texture_frames[117] = { /* 800AEB68 */
    {0, 0, 8, 8}, {8, 0, 8, 8}, {0x10, 0, 8, 8}, {0x18, 0, 8, 8},
    {0x20, 0, 8, 8}, {0x28, 0, 8, 8}, {0x30, 0, 8, 8}, {0x38, 0, 8, 8},
    {0x40, 0, 8, 8}, {0x48, 0, 8, 8}, {0x50, 0, 8, 8}, {0x58, 0, 8, 8},
    {0x60, 0, 8, 8}, {0x68, 0, 8, 8}, {0x70, 0, 8, 8}, {0x78, 0, 8, 8},
    {0, 8, 8, 8}, {8, 8, 8, 8}, {0x10, 8, 8, 8}, {0x18, 8, 8, 8},
    {0x20, 8, 8, 8}, {0x28, 8, 8, 8}, {0x30, 8, 8, 8}, {0x38, 8, 8, 8},
    {0x40, 8, 8, 8}, {0x48, 8, 8, 8}, {0x50, 8, 8, 8}, {0x58, 8, 8, 8},
    {0x60, 8, 8, 8}, {0x68, 8, 8, 8}, {0x70, 8, 8, 8}, {0x78, 8, 8, 8},
    {0, 0x10, 8, 8}, {8, 0x10, 8, 8}, {0x10, 0x10, 8, 8}, {0x18, 0x10, 8, 8},
    {0x20, 0x10, 8, 8}, {0x28, 0x10, 8, 8}, {0x30, 0x10, 8, 8}, {0x38, 0x10, 8, 8},
    {0x40, 0x10, 8, 8}, {0x48, 0x10, 8, 8}, {0x50, 0x10, 8, 8}, {0x58, 0x10, 8, 8},
    {0x60, 0x10, 8, 8}, {0x68, 0x10, 8, 8}, {0x70, 0x10, 8, 8}, {0x78, 0x10, 8, 8},
    {0, 0x18, 0x38, 0x18}, {0x40, 0x18, 8, 8}, {0x48, 0x18, 8, 8}, {0x50, 0x18, 8, 8},
    {0x58, 0x18, 8, 8}, {0x40, 0x20, 8, 8}, {0x48, 0x20, 8, 8}, {0x50, 0x20, 8, 8},
    {0x58, 0x20, 8, 8}, {0x40, 0x28, 0x20, 8}, {0x60, 0x18, 0x10, 0x10}, {0x70, 0x18, 0x10, 0x10},
    {0x60, 0x28, 0x20, 8}, {0, 0x30, 0x18, 0x18}, {0x18, 0x30, 0x20, 8}, {0x18, 0x38, 0x20, 8},
    {0x18, 0x40, 0x10, 8}, {0x38, 0x30, 0x10, 0x10}, {0x48, 0x30, 0x10, 0x10}, {0x58, 0x30, 0x10, 0x10},
    {0x68, 0x30, 0x10, 0x10}, {0x28, 0x40, 0x10, 8}, {0x38, 0x40, 0x10, 8}, {0x48, 0x40, 0x10, 8},
    {0x58, 0x40, 0x20, 8}, {0, 0x48, 0x10, 8}, {0, 0x50, 0x10, 8}, {0, 0x58, 0x10, 8},
    {0, 0x60, 8, 8}, {8, 0x60, 8, 8}, {0x10, 0x48, 0x28, 0x20}, {0x38, 0x48, 0x10, 0x10},
    {0x48, 0x48, 0x10, 0x10}, {0x38, 0x50, 0x10, 0x10}, {0x48, 0x50, 0x10, 0x10}, {0, 0x60, 0x58, 0x18},
    {0x58, 0x48, 0x20, 8}, {0x58, 0x50, 0x20, 8}, {0x58, 0x58, 0x20, 8}, {0x58, 0x60, 0x20, 0x18},
    {0x80, 0x10, 8, 8}, {0x78, 0x18, 0x10, 0x20}, {0x78, 0x38, 0x10, 0x20}, {0x78, 0x58, 0x10, 0x20},
    {0x88, 0, 0x38, 0x38}, {0x90, 0x38, 0x30, 0x30}, {0xC0, 8, 0x30, 0x30}, {0x88, 0x68, 0x18, 8},
    {0x88, 0x70, 0x18, 8}, {0x88, 0x78, 0x18, 8}, {0xA0, 0x68, 0x10, 0x10}, {0xB0, 0x68, 0x10, 2},
    {0xC0, 0x68, 0x10, 2}, {0xD0, 0x70, 0x10, 0x10}, {0xC8, 0x58, 0x18, 0x10}, {0xE8, 0x58, 0x10, 0x10},
    {0xD0, 0, 8, 8}, {0xD8, 0, 0x10, 8}, {0xE8, 0, 8, 8}, {0xF0, 0, 0x10, 0x20},
    {0xF0, 0x20, 0x10, 0x10}, {0x38, 0x28, 0x10, 8}, {0xF8, 0x38, 8, 0x10}, {0xB6, 0x70, 2, 0x10},
    {0xBE, 0x70, 2, 0x10}, {0xE0, 0x70, 0x10, 2}, {0xF6, 0x70, 2, 0x10}, {0xF0, 0x58, 0x10, 0x10},
    {0x58, 0x50, 0x10, 8},
};
PanelPiece field_status_panel_pieces[PANEL_PIECES] = { /* 800AEF10 */
    {0x1A, 0xC, 0x6D, 2}, {0x2A, 0xC, 0x6D, 2}, {0x3A, 0xC, 0x6D, 2}, {0x4A, 0xC, 0x6D, 2},
    {0x5A, 0xC, 0x6D, 2}, {0x6A, 0xC, 0x6D, 2}, {0x7A, 0xC, 0x6D, 2}, {0x8A, 0xC, 0x6D, 2},
    {0x9A, 0xC, 0x6D, 2}, {0xAA, 0xC, 0x6D, 2}, {0xBA, 0xC, 0x6D, 2}, {0xCA, 0xC, 0x6D, 2},
    {0xDA, 0xC, 0x6D, 2}, {0xEA, 0xC, 0x6D, 2}, {0xFA, 0xC, 0x6D, 2}, {0x10A, 0xC, 0x6D, 2},
    {0x10, 0x18, 0x6E, 0}, {0x10, 0x28, 0x6E, 0}, {0x10, 0x38, 0x6E, 0}, {0x10, 0x48, 0x6E, 0},
    {0x10, 0x58, 0x6E, 0}, {0x10, 0x68, 0x6E, 0}, {0x10, 0x78, 0x6E, 0}, {0x10, 0x88, 0x6E, 0},
    {0x10, 0x98, 0x6E, 0}, {0x10, 0xA8, 0x6E, 0}, {0x10, 0xB8, 0x6E, 0}, {0x10, 0xC8, 0x6E, 0},
    {0x10, 0xD8, 0x6E, 0}, {0xC1, 0x19, 0, 0}, {0xB9, 0x19, 0, 0}, {0xB1, 0x19, 0, 0},
    {0xA9, 0x19, 0, 0}, {0xA1, 0x19, 0, 0}, {0x38, 0x75, 0, 0}, {0x30, 0x75, 0, 0},
    {0x28, 0x75, 0, 0}, {0x20, 0x75, 0, 0}, {0x18, 0x75, 0, 0}, {0xDC, 0xBE, 0x5F, 2},
    {0xFC, 0xBE, 0x74, 0}, {0x10C, 0xBE, 0x74, 0}, {0xAA, 0x50, 0x66, 0}, {0xA1, 0x18, 0x67, 0x10},
    {0xB1, 0x18, 0x67, 0x10}, {0xC1, 0x18, 0x73, 0x10}, {0x18, 0x74, 0x67, 0x10}, {0x28, 0x74, 0x67, 0x10},
    {0x38, 0x74, 0x73, 0x10}, {0xDC, 0xBE, 0x67, 0x10}, {0xEC, 0xBE, 0x67, 0x10}, {0xFC, 0xBE, 0x67, 0x10},
    {0x10C, 0xBE, 0x67, 0x10}, {0x11C, 0xBE, 0x73, 0x10}, {0, 0x70, 0x63, 0}, {0x10, 0x70, 0x63, 0},
    {0x20, 0x70, 0x63, 0}, {0x30, 0x70, 0x63, 0}, {0x40, 0x70, 0x63, 0}, {0x50, 0x70, 0x63, 0},
    {0x60, 0x70, 0x63, 0}, {0x70, 0x70, 0x63, 0}, {0x80, 0x70, 0x63, 0}, {0x90, 0x70, 0x64, 0},
    {0xA0, 0x70, 0x64, 0}, {0xB0, 0x70, 0x63, 0}, {0xC0, 0x70, 0x63, 0}, {0xD0, 0x70, 0x63, 0},
    {0xE0, 0x70, 0x63, 0}, {0xF0, 0x70, 0x63, 0}, {0x100, 0x70, 0x63, 0}, {0x110, 0x70, 0x63, 0},
    {0x120, 0x70, 0x63, 0}, {0x130, 0x70, 0x63, 0}, {0x9F, 0, 0x6F, 0}, {0x9F, 0x10, 0x6F, 0},
    {0x9F, 0x20, 0x6F, 0}, {0x9F, 0x30, 0x6F, 0}, {0x9F, 0x40, 0x6F, 0}, {0x9F, 0x50, 0x6F, 0},
    {0x9F, 0x60, 0x70, 0}, {0x9F, 0x70, 0x70, 0}, {0x9F, 0x80, 0x6F, 0}, {0x9F, 0x90, 0x6F, 0},
    {0x9F, 0xA0, 0x6F, 0}, {0x9F, 0xB0, 0x6F, 0}, {0x9F, 0xC0, 0x6F, 0}, {0x9F, 0xD0, 0x6F, 0},
    {0x9F, 0xE0, 0x6F, 0}, {0x60, 0x40, 0x71, 0}, {0x80, 0x40, 0x71, 0}, {0x90, 0x40, 0x71, 0},
    {0xA0, 0x40, 0x71, 0}, {0xB0, 0x40, 0x71, 0}, {0xD0, 0x40, 0x71, 0}, {0x60, 0x9F, 0x71, 0},
    {0x80, 0x9F, 0x71, 0}, {0x90, 0x9F, 0x71, 0}, {0xA0, 0x9F, 0x71, 0}, {0xB0, 0x9F, 0x71, 0},
    {0xD0, 0x9F, 0x71, 0}, {0x50, 0x40, 0x65, 1}, {0xE0, 0x40, 0x65, 0}, {0x50, 0x90, 0x65, 3},
    {0xE0, 0x90, 0x65, 2}, {0x4F, 0x50, 0x72, 0}, {0xEE, 0x50, 0x72, 0}, {0x4F, 0x80, 0x72, 0},
    {0xEE, 0x80, 0x72, 0},
};

s32 field_status_panel_built = 0; /* 800AF278: panel primitive buffers allocated */

/* A particle sprite (800af27c): half size, centre, and the four texture
 * corners (u, v) within the sprite page. */
typedef struct {
    u16 half_w;
    u16 half_h;
    u16 x;
    u16 y;
    u16 uv[4][2];
} ParticleSprite;

ParticleSprite field_effect_particle_sprites[21] = { /* 800AF27C */
    {0x40, 0x40, 0, 0, {{0, 0}, {0x40, 0}, {0, 0x40}, {0x40, 0x40}}},
    {0x40, 0x40, 0, 0xFFC0, {{0x40, 0}, {0x80, 0}, {0x40, 0x40}, {0x80, 0x40}}},
    {0x20, 0x20, 0, 0, {{0, 0x40}, {0x20, 0x40}, {0, 0x60}, {0x20, 0x60}}},
    {0x20, 0x20, 0, 0, {{0x20, 0x40}, {0x40, 0x40}, {0x20, 0x60}, {0x40, 0x60}}},
    {0x20, 0x20, 0, 0, {{0x40, 0x40}, {0x60, 0x40}, {0x40, 0x60}, {0x60, 0x60}}},
    {0xFE, 0x1F, 0x100, 0, {{0, 0x60}, {0xFF, 0x60}, {0, 0x80}, {0xFF, 0x80}}},
    {0x10, 0x20, 0, 0xFFE0, {{0xE0, 0}, {0xF0, 0}, {0xE0, 0x20}, {0xF0, 0x20}}},
    {0x10, 0x60, 0, 0xFFA0, {{0xF0, 0}, {0xFF, 0}, {0xF0, 0x60}, {0xFF, 0x60}}},
    {0x60, 0x10, 0xFFA0, 0, {{0xF0, 1}, {0xF0, 0x60}, {0xFF, 1}, {0xFF, 0x60}}},
    {0x40, 0x40, 0, 0, {{0x80, 0}, {0xC0, 0}, {0x80, 0x40}, {0xC0, 0x40}}},
    {0x20, 0x20, 0, 0, {{0x60, 0x40}, {0x80, 0x40}, {0x60, 0x60}, {0x80, 0x60}}},
    {0x40, 0x40, 0, 0, {{0, 0x80}, {0x40, 0x80}, {0, 0xC0}, {0x40, 0xC0}}},
    {0x40, 0x40, 0, 0, {{0x40, 0x80}, {0x80, 0x80}, {0x40, 0xC0}, {0x80, 0xC0}}},
    {0x20, 0x20, 0, 0, {{0x80, 0x40}, {0xA0, 0x40}, {0x80, 0x60}, {0xA0, 0x60}}},
    {0x20, 0x20, 0, 0, {{0xA0, 0x40}, {0xC0, 0x40}, {0xA0, 0x60}, {0xC0, 0x60}}},
    {0x20, 0x20, 0, 0, {{0xC0, 0}, {0xE0, 0}, {0xC0, 0x20}, {0xE0, 0x20}}},
    {0x20, 0x20, 0, 0, {{0xC0, 0x20}, {0xE0, 0x20}, {0xC0, 0x40}, {0xE0, 0x40}}},
    {0x40, 0x40, 0, 0xFFC0, {{0x80, 0x80}, {0xC0, 0x80}, {0x80, 0xC0}, {0xC0, 0xC0}}},
    {0x20, 0x20, 0, 0, {{0xC0, 0x40}, {0xE0, 0x40}, {0xC0, 0x60}, {0xE0, 0x60}}},
    {0x40, 0x40, 0, 0, {{0xC0, 0x80}, {0x100, 0x80}, {0xC0, 0xC0}, {0x100, 0xC0}}},
    {0x10, 0x10, 0, 0, {{0xE0, 0x20}, {0xF0, 0x20}, {0xE0, 0x30}, {0xF0, 0x30}}},
};

/* 800A4748: Reset the screen effect view with the image moved to (2c0, 100). */
void field_screen_save_copy(void) {
    field_screen_copy_to(0x2C0, 0x100);
}

/* 800A476C: Restore the projection distance and move the 320x224 screen image to
 * (x, y). */
void field_screen_copy_to(s32 x, s32 y) {
    RECT rect;

    rect.w = 0x140;
    rect.y = 0;
    rect.x = 0;
    rect.h = 0xE0;
    SetGeomScreen(0x200);
    MoveImage(&rect, x, y);
    field_sync_draw_and_vsync();
}

/* 800A47D4: Stop the screen effect and release its buffers. */
void field_distortion_stop(void) {
    field_work.unk2078 = 0;
    if (field_distortion_buffers_allocated != 0) {
        heap_free(field_work.effect_buffers[0]);
        heap_free(field_work.effect_buffers[1]);
        heap_free(field_work.effect_buffers[2]);
        heap_free(field_work.effect_buffers[3]);
        field_distortion_buffers_allocated = 0;
    }
}

/* 800A484C: Start the screen distortion: on first use allocate its buffers, build the
 * 20x17 grid of 16x16 textured quads (rows 14-16 sample the saved strips at
 * (3c0, 0)) and the screen/strip copy commands; unless resuming, read the
 * six targets and the step count from the operands. The strip loop reuses
 * the row counter and reads the strip x through a pointer to the table
 * (its own induction pointer; y is read from the table). */
void field_distortion_start(s32 resume) {
    RECT rect;
    s32 row;
    s32 column;
    POLY_FT4 *poly;
    DVECTOR *strips;
    POLY_FT4 *copy;
    s32 u;

    field_work.unk2078 = 1;
    if (field_distortion_buffers_allocated == 0) {
        field_work.effect_buffers[0] = heap_alloc(0x180, 0);
        field_work.effect_buffers[1] = heap_alloc(0x180, 0);
        field_work.effect_buffers[2] = heap_alloc(0x3840, 0);
        field_work.effect_buffers[3] = heap_alloc(0x3840, 0);
        field_distortion_buffers_allocated = 1;
        for (row = 0; row < 17; row++) {
            for (column = 0; column < 20; column++) {
                poly = (POLY_FT4 *)field_work.effect_buffers[2] + (column + row * 20);
                copy = (POLY_FT4 *)field_work.effect_buffers[3] + (column + row * 20);
                SetPolyFT4(poly);
                SetSemiTrans(poly, 0);
                poly->r0 = 0x80;
                poly->g0 = 0x80;
                poly->b0 = 0x80;
                poly->x0 = column * 16;
                poly->y0 = row * 16;
                poly->x1 = column * 16 + 16;
                poly->y1 = row * 16;
                poly->x2 = column * 16;
                poly->y2 = row * 16 + 16;
                poly->x3 = column * 16 + 16;
                poly->y3 = row * 16 + 16;
                if (row >= 14) {
                    u = (column * 16) & 0x3F;
                    poly->u0 = u;
                    poly->v0 = (column >> 2) * 16 + (row - 14) * 80;
                    poly->u1 = u + 16;
                    poly->v1 = (column >> 2) * 16 + (row - 14) * 80;
                    poly->u2 = u;
                    poly->v2 = (column >> 2) * 16 + ((row - 14) * 80 + 16);
                    poly->u3 = u + 16;
                    poly->v3 = (column >> 2) * 16 + ((row - 14) * 80 + 16);
                    poly->tpage = GetTPage(2, 0, 0x3C0, 0);
                    *copy = *poly;
                } else {
                    poly->u0 = (column * 16) & 0x3F;
                    poly->v0 = row * 16;
                    poly->u1 = ((column * 16) & 0x3F) + 16;
                    poly->v1 = row * 16;
                    poly->u2 = (column * 16) & 0x3F;
                    poly->v2 = row * 16 + 16;
                    poly->u3 = ((column * 16) & 0x3F) + 16;
                    poly->v3 = row * 16 + 16;
                    poly->tpage = GetTPage(2, 0, (column * 16) & 0xFFC0, 0);
                    *copy = *poly;
                    copy->tpage = GetTPage(2, 0, (column * 16) & 0xFFC0, 0x100);
                }
            }
        }
        rect.x = 0;
        rect.y = 0x20;
        rect.w = 0x140;
        rect.h = 0xC0;
        SetDrawMove(field_work.effect_buffers[0], &rect, 0, 0);
        rect.y = 0x120;
        SetDrawMove(field_work.effect_buffers[1], &rect, 0, 0x100);
        rect.w = 0x40;
        rect.h = 0x10;
        strips = field_distortion_strip_sources;
        for (row = 0; row < 15; row++) {
            rect.x = strips[row].vx;
            rect.y = field_distortion_strip_sources[row].vy;
            SetDrawMove((DR_MOVE *)field_work.effect_buffers[0] + (row + 1), &rect, 0x3C0, row * 16);
            rect.y = field_distortion_strip_sources[row].vy + 0x100;
            SetDrawMove((DR_MOVE *)field_work.effect_buffers[1] + (row + 1), &rect, 0x3C0, row * 16);
        }
    }
    if (resume == 0) {
        field_distortion_set_targets(field_event_read_imm_or_var(1), field_event_read_imm_or_var(3), field_event_read_imm_or_var(5), field_event_read_imm_or_var(7),
                      field_event_read_imm_or_var(9), field_event_read_imm_or_var(11), field_event_read_imm_or_var(13));
    }
    field_work.unk207A = 0;
}

/* 800A4CC4: Move the six screen effect values to the given whole targets over
 * `steps` frames. */
void field_distortion_set_targets(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 steps) {
    s32 step0;
    s32 step1;
    s32 step2;
    s32 step3;
    s32 step4;
    s32 step5;

    if (steps == 0) {
        steps = 1;
    }
    step0 = ((a << 16) - field_work.effect_value[0]) / steps;
    step1 = ((b << 16) - field_work.effect_value[1]) / steps;
    step2 = ((c << 16) - field_work.effect_value[2]) / steps;
    step3 = ((d << 16) - field_work.effect_value[3]) / steps;
    step4 = ((e << 16) - field_work.effect_value[4]) / steps;
    step5 = ((f << 16) - field_work.effect_value[5]) / steps;
    field_work.effect_steps = steps;
    field_work.effect_step[0] = step0;
    field_work.effect_step[1] = step1;
    field_work.effect_step[2] = step2;
    field_work.effect_step[3] = step3;
    field_work.effect_step[4] = step4;
    field_work.effect_step[5] = step5;
}

/* The distortion grid of the buffer being drawn: 20 x 17 textured quads. */
#define EFFECT_QUAD(row, column) \
    (((POLY_FT4 *)field_work.effect_buffers[2 + field_draw_buffer_index])[(row) * 20 + (column)])
/* The buffer's strip copy commands. */
#define EFFECT_MOVES ((DR_MOVE *)field_work.effect_buffers[field_draw_buffer_index])

/* 800A4DAC: Draw the screen distortion: step the six effect values (amplitude,
 * frequency and phase speed in x and y; once released, clear them when the
 * steps run out and stop), then bend the grid along two sine waves: rows
 * 14-16 hold the saved strips, rows 0-13 the screen, and row 13's lower
 * edge takes the next wave. The quad corners are addressed through the grid
 * index throughout; GCC substitutes the tested row/column constants
 * (column 0/19, row 13/16) into it. The step is shared by both branches
 * (one block that the released case jumps into). */
void field_distortion_draw(void) {
    s32 amplitude_x;
    s32 amplitude_y;
    s32 frequency_x;
    s32 frequency_y;
    s32 phase_x;
    s32 phase_y;
    s32 row;
    s32 column;
    s32 wave_x;
    s32 wave_y;
    POLY_FT4 *poly;
    s16 *phase;
    s32 speed_x;
    s32 speed_y;

    if (field_work.unk2078 == 0) {
        return;
    }
    if (field_work.unk207A == 0) {
        if (field_work.effect_steps > 0) {
        step:
            field_work.effect_value[0] += field_work.effect_step[0];
            field_work.effect_value[1] += field_work.effect_step[1];
            field_work.effect_value[2] += field_work.effect_step[2];
            field_work.effect_steps--;
            field_work.effect_value[3] += field_work.effect_step[3];
            field_work.effect_value[4] += field_work.effect_step[4];
            field_work.effect_value[5] += field_work.effect_step[5];
        }
    } else {
        if (field_work.effect_steps > 0) {
            goto step;
        }
        field_work.effect_value[5] = 0;
        field_work.effect_value[4] = 0;
        field_work.effect_value[3] = 0;
        field_work.effect_value[2] = 0;
        field_work.effect_value[1] = 0;
        field_work.effect_value[0] = 0;
        field_work.unk2078 = 0;
    }
    phase = EFFECT_PHASE;
    amplitude_x = WHOLE(field_work.effect_value[0]);
    amplitude_y = WHOLE(field_work.effect_value[1]);
    frequency_x = WHOLE(field_work.effect_value[2]);
    frequency_y = WHOLE(field_work.effect_value[3]);
    speed_x = WHOLE(field_work.effect_value[4]);
    speed_y = WHOLE(field_work.effect_value[5]);
    phase[0] += speed_x;
    phase[1] += speed_y;

    /* Rows 14-16 (the saved strips) continue the wave below the screen. */
    phase_y = EFFECT_PHASE[1] + frequency_y * 11;
    for (row = 14; row < 17; row++) {
        wave_y = (gpu_get_cos(phase_y) * amplitude_y) >> 12;
        phase_x = EFFECT_PHASE[0];
        phase_y += frequency_y;
        for (column = 0; column < 20; column++) {
            wave_x = (gpu_get_cos(phase_x) * amplitude_x) >> 12;
            phase_x += frequency_x;
            poly = &EFFECT_QUAD(row, column);
            if (column != 0) {
                EFFECT_QUAD(row, column).x0 = EFFECT_QUAD(row, column).x2 =
                    EFFECT_QUAD(row, column - 1).x1 = EFFECT_QUAD(row, column - 1).x3 =
                        column * 16 + wave_x;
                if (column == 19) {
                    EFFECT_QUAD(row, column).x1 = EFFECT_QUAD(row, column).x3 = wave_x + 0x140;
                }
            } else {
                EFFECT_QUAD(row, column).x0 = EFFECT_QUAD(row, column).x2 = 0;
            }
            EFFECT_QUAD(row, column).y0 = EFFECT_QUAD(row, column).y1 =
                EFFECT_QUAD(row - 1, column).y2 = EFFECT_QUAD(row - 1, column).y3 =
                    row * 16 + wave_y - 0x30;
            if (row == 16) {
                EFFECT_QUAD(row, column).y2 = EFFECT_QUAD(row, column).y3 = 0xE0;
            }
            addPrim(&field_current_draw_block->ot[1], poly);
        }
    }
    addPrim(&field_current_draw_block->ot[1], &EFFECT_MOVES[0]);

    /* Rows 0-13: the screen itself. */
    phase_y = EFFECT_PHASE[1];
    for (row = 0; row < 14; row++) {
        wave_y = (gpu_get_cos(phase_y) * amplitude_y) >> 12;
        phase_x = EFFECT_PHASE[0];
        phase_y += frequency_y;
        for (column = 0; column < 20; column++) {
            wave_x = (gpu_get_cos(phase_x) * amplitude_x) >> 12;
            phase_x += frequency_x;
            poly = &EFFECT_QUAD(row, column);
            if (row != 0) {
                if (row == 13) {
                    EFFECT_QUAD(row - 1, column).y2 = EFFECT_QUAD(row - 1, column).y3 =
                        EFFECT_QUAD(row, column).y0 = EFFECT_QUAD(row, column).y1 =
                            row * 16 + wave_y + 0x20;
                    wave_y = (gpu_get_cos(phase_y) * amplitude_y) >> 12;
                    EFFECT_QUAD(row, column).y2 = EFFECT_QUAD(row, column).y3 =
                        row * 16 + wave_y + 0x20;
                } else {
                    EFFECT_QUAD(row, column).y0 = EFFECT_QUAD(row, column).y1 =
                        EFFECT_QUAD(row - 1, column).y2 = EFFECT_QUAD(row - 1, column).y3 =
                            row * 16 + 0x20 + wave_y;
                }
            } else {
                EFFECT_QUAD(row, column).y0 = EFFECT_QUAD(row, column).y1 = 0x20;
            }
            if (column != 0) {
                EFFECT_QUAD(row, column).x0 = EFFECT_QUAD(row, column).x2 =
                    EFFECT_QUAD(row, column - 1).x1 = EFFECT_QUAD(row, column - 1).x3 =
                        column * 16 + wave_x;
                if (column == 19) {
                    EFFECT_QUAD(row, column).x1 = EFFECT_QUAD(row, column).x3 = 0x140;
                }
            } else {
                EFFECT_QUAD(row, column).x0 = EFFECT_QUAD(row, column).x2 = 0;
            }
            addPrim(&field_current_draw_block->ot[1], poly);
        }
    }
    for (row = 0; row < 15; row++) {
        addPrim(&field_current_draw_block->ot[1], &EFFECT_MOVES[row + 1]);
    }
    addPrim(&field_current_draw_block->ot[1], &field_texture_window_modes[field_draw_buffer_index][3]);
}

/* 800A55B8: Store three words at +14 of `object`. */
void field_store_three_words_at_14(s32 *object, s32 a, s32 b, s32 c) {
    object[5] = a;
    object[6] = b;
    object[7] = c;
}

/* 800A55C8: Release the two blocks at 800afe80 and 800b069c. */
void field_free_block_pair(void) {
    heap_free(field_block_pair_first);
    heap_free(field_block_pair_second);
}

/* 800A5600: Set the five screen pieces of the next draw buffer to grey `shade`. */
void field_screen_pieces_set_shade(s32 shade) {
    s32 i;

    for (i = 0; i < 5; i++) {
        (field_screen_pieces.quads[i] + ((field_draw_buffer_index + 1) & 1))->r0 = shade;
        (field_screen_pieces.quads[i] + ((field_draw_buffer_index + 1) & 1))->g0 = shade;
        (field_screen_pieces.quads[i] + ((field_draw_buffer_index + 1) & 1))->b0 = shade;
    }
}

/* 800A56A8: Fade screen channel 0 in from white over `frames` frames. */
void field_fade_from_white(s32 frames) {
    s32 step;

    step = -0x10000 / frames;
    field_work.fades[0].level[0] = field_work.fades[0].level[1] = field_work.fades[0].level[2] = 0xFF00;
    field_work.fades[0].steps = frames + 1;
    field_work.fades[0].abr = field_work.fades[0].active = 1;
    field_work.fades[0].step[0] = field_work.fades[0].step[1] = field_work.fades[0].step[2] = step;
}

/* 800A5710: Fade screen channel 0 up from black over `frames` frames. */
void field_fade_to_white(s32 frames) {
    s32 step;

    step = 0x10000 / frames;
    field_work.fades[0].level[0] = field_work.fades[0].level[1] = field_work.fades[0].level[2] = 0;
    field_work.fades[0].steps = frames + 1;
    field_work.fades[0].abr = field_work.fades[0].active = 1;
    field_work.fades[0].step[0] = field_work.fades[0].step[1] = field_work.fades[0].step[2] = step;
}

/* 800A5774: Set the mask bit of every pixel of the 64-pixel-wide VRAM column at
 * (x, y), `h` rows tall. */
void field_screen_set_stp_bits(s32 x, s32 y, s32 h) {
    RECT rect;
    u_long *pixels;
    u_long *p;
    s32 i;

    rect.x = x;
    rect.y = y;
    rect.w = 0x40;
    rect.h = h;
    pixels = heap_alloc(h << 7, 1);
    StoreImage(&rect, pixels);
    DrawSync(0);
    p = pixels;
    for (i = 0; i < h * 32; i += 8) {
        p[0] |= 0x80008000;
        p[1] |= 0x80008000;
        p[2] |= 0x80008000;
        p[3] |= 0x80008000;
        p[4] |= 0x80008000;
        p[5] |= 0x80008000;
        p[6] |= 0x80008000;
        p[7] |= 0x80008000;
        p += 8;
    }
    LoadImage(&rect, pixels);
    DrawSync(0);
    heap_free(pixels);
}

void field_screen_pieces_init(s32 semitrans, s32 abr);

/* 800A5884: Set up the screen pieces, draw them twice, mask the saved screen columns at
 * (2c0..3c0, 100) and draw twice more. */
void field_screen_pieces_show(s32 semitrans, s32 abr) {
    s32 i;
    s32 x;

    field_screen_pieces_init(semitrans, abr);
    for (i = 0; i < 2; i++) {
        field_draw_swap_and_clear_ots();
        field_screen_pieces_draw();
        field_draw_present_overlay();
    }
    x = 0x2C0;
    for (i = 0; i < 5; i++) {
        field_screen_set_stp_bits(x, 0x100, 0xE0);
        x += 0x40;
    }
    for (i = 0; i < 2; i++) {
        field_draw_swap_and_clear_ots();
        field_screen_pieces_draw();
        field_draw_present_overlay();
    }
}

/* 800A5924: Run the requested screen transition (800adb38) over 800adb3c frames:
 * 1-2 fade the screen pieces out, 3 fades them in and holds while it
 * stays requested, 4 dissolves the screen grid from the centre. */
void field_transition_run(void) {
    s32 i;
    s32 x;
    s32 level;

    if (field_transition_kind != 0) {
        console_close();
        field_dialogue_reset_portrait_slots();
        field_vram_column_save();
        if (field_transition_kind == 1 || field_transition_kind == 4) {
            field_screen_save_copy();
            DrawSync(0);
            field_draw_swap_and_clear_ots();
            field_sync_draw_and_vsync();
        }
        field_sync_draw_and_vsync();
    dispatch:
        switch (field_transition_kind) {
        case 4:
            field_screen_pieces_show(1, 1);
            field_screen_grid_build();
            field_faded_out = 1;
            field_fade_in(field_transition_frames);
            field_screen_grid_fade_radius = 0;
            for (i = 0; i < field_transition_frames; i++) {
                field_run_pre_frame();
                field_screen_grid_draw();
                field_run_frame();
                field_run_post_frame();
                field_screen_grid_fade_radius += 6;
            }
            DrawSync(0);
            field_screen_grid_release();
            break;
        case 1:
        case 2:
            field_screen_pieces_show(1, 1);
            field_faded_out = 1;
            field_fade_in(field_transition_frames);
            level = 0x800000;
            for (i = 0; i < field_transition_frames; i++) {
                field_run_pre_frame();
                field_screen_pieces_draw();
                field_run_frame();
                field_run_post_frame();
                field_screen_pieces_set_shade(level >> 16);
                level -= 0x800000 / field_transition_frames;
                if (level < 0) {
                    level = 0;
                }
            }
            break;
        case 3:
            field_screen_pieces_init(1, 1);
            for (i = 0, x = 0x2C0; i < 5; i++, x += 0x40) {
                field_screen_set_stp_bits(x, 0x100, 0xE0);
            }
            level = 0;
            field_fade_out(field_transition_frames);
            field_screen_pieces_set_shade(0);
            for (i = 0; i < field_transition_frames; i++) {
                field_run_pre_frame();
                field_screen_pieces_draw();
                field_run_frame();
                field_run_post_frame();
                field_screen_pieces_set_shade(level >> 16);
                level += 0x800000 / field_transition_frames;
            }
            for (;;) {
                if (field_transition_kind != 3) {
                    goto dispatch;
                }
                field_run_pre_frame();
                field_screen_pieces_draw();
                field_run_frame();
                field_run_post_frame();
            }
        }
        field_transition_kind = 0;
        field_vram_column_restore();
        field_load_text_palette();
    }
}

/* 800A5C40: Reload the field for a map change inside the field mode (800b0048 the
 * transition kind, 800afd14 its frames): stop effects, keep the map
 * read-ahead block across the heap reset, then per kind fade or dissolve
 * out, reload the components (80070cc8), restart the music and fade in. */
void field_reload_for_map_change(void) {
    RECT rect;
    u8 *ahead;
    s32 kind;
    s32 frames;
    s32 level;
    s32 i;

    console_close();
    field_effect_release_all_slots();
    field_sound_clear_emitters_stop_voices();
    field_dialogue_close_all_windows();
    if (field_map_change_kind != 6) {
        field_vram_column_save();
        if (field_map_change_kind != 4) {
            field_screen_save_copy();
        }
    }
    DrawSync(0);
    field_draw_swap_and_clear_ots();
    field_sync_draw_and_vsync();
    field_teardown();
    ahead = heap_alloc(mode_read_ahead_size, 0);
    memcpy(ahead, mode_read_ahead_block, mode_read_ahead_size);
    heap_unprotect_block(mode_read_ahead_block);
    heap_free(mode_read_ahead_block);
    if (field_map_change_kind != 6) {
        field_vram_column_relocate(1);
    }
    mode_read_ahead_block = heap_alloc(mode_read_ahead_size, 1);
    memcpy(mode_read_ahead_block, ahead, mode_read_ahead_size);
    heap_protect_block(mode_read_ahead_block);
    heap_free(ahead);
    switch (field_map_change_kind) {
    case 6:
        field_fade_out(field_map_change_frames);
        for (i = 0; i < field_map_change_frames; i++) {
            field_draw_swap_and_clear_ots();
            field_fade_update_channels(&field_current_draw_block->overlay_ot[0], field_draw_buffer_index);
            field_draw_present_overlay();
        }
        goto reload;
    case 0:
        field_screen_pieces_init(0, 0);
        field_fade_out(field_map_change_frames);
        for (i = 0; i < field_map_change_frames; i++) {
            field_draw_swap_and_clear_ots();
            field_fade_update_channels(&field_current_draw_block->overlay_ot[0], field_draw_buffer_index);
            field_screen_pieces_draw();
            field_draw_present_overlay();
        }
    reload:
        field_draw_swap_and_clear_ots();
        field_draw_present_overlay();
        mode_sync_party_files();
        mode_unpack_party_files();
        kind = field_map_change_kind;
        frames = field_map_change_frames;
        field_load_from_bundle();
        field_map_stream_start();
        field_map_stream_stop();
        field_map_change_kind = kind;
        field_map_change_frames = frames;
        if (mode_music_load_pending == -1) {
            field_music_change_track(mode_music_selected_track, 0);
        }
        field_fade_in(field_map_change_frames);
        break;
    case 1:
        field_screen_pieces_init(0, 0);
        field_fade_to_white(field_map_change_frames);
        for (i = 0; i < field_map_change_frames; i++) {
            field_draw_swap_and_clear_ots();
            field_fade_update_channels(&field_current_draw_block->overlay_ot[0], field_draw_buffer_index);
            field_screen_pieces_draw();
            field_draw_present_overlay();
        }
        field_sync_draw_and_vsync();
        mode_sync_party_files();
        mode_unpack_party_files();
        kind = field_map_change_kind;
        frames = field_map_change_frames;
        field_load_from_bundle();
        field_map_stream_start();
        field_map_stream_stop();
        field_map_change_kind = kind;
        field_map_change_frames = frames;
        if (mode_music_load_pending == -1) {
            field_music_change_track(mode_music_selected_track, 0);
        }
        field_fade_from_white(field_map_change_frames);
        break;
    case 2:
    case 4:
        field_screen_pieces_show(1, 1);
        mode_sync_party_files();
        mode_unpack_party_files();
        kind = field_map_change_kind;
        frames = field_map_change_frames;
        field_load_from_bundle();
        field_map_stream_start();
        if (field_map_stream_running == 1) {
            while (cd_get_pending_read_count() != 0) {
                field_draw_swap_and_clear_ots();
                field_screen_pieces_draw();
                field_draw_present_overlay();
                if (field_screen_pieces_scale < 0x22C0) {
                    field_screen_pieces_scale += 0x20;
                }
            }
            heap_free(field_map_stream_ring);
            field_map_stream_running = 0;
            field_brighten_text_strip();
        }
        field_dialogue_open_blocked = 1;
        field_map_change_kind = kind;
        field_map_change_frames = frames;
        if (mode_music_load_pending == -1) {
            field_music_change_track(mode_music_selected_track, 0);
        }
        level = 0x800000;
        field_fade_in(field_map_change_frames);
        for (i = 0; i < field_map_change_frames; i++) {
            field_run_pre_frame();
            field_screen_pieces_draw();
            field_run_frame();
            field_run_post_frame();
            field_screen_pieces_set_shade(level >> 16);
            level -= 0x800000 / field_map_change_frames;
            if (level < 0) {
                level = 0;
            }
            if (field_screen_pieces_scale < 0x22C0) {
                field_screen_pieces_scale += 0x20;
            }
        }
        break;
    case 3:
        field_screen_pieces_init(0, 0);
        field_map_stream_start();
        field_draw_swap_and_clear_ots();
        field_screen_pieces_draw();
        field_draw_present_overlay();
        mode_sync_party_files();
        mode_unpack_party_files();
        kind = field_map_change_kind;
        frames = field_map_change_frames;
        field_dialogue_open_blocked = 1;
        field_load_from_bundle();
        field_map_stream_stop();
        field_map_change_kind = kind;
        field_map_change_frames = frames;
        if (mode_music_load_pending == -1) {
            field_music_change_track(mode_music_selected_track, 0);
        }
        for (i = 0; i < 4; i++) {
            field_run_pre_frame();
            field_screen_pieces_draw();
            field_run_frame();
            field_run_post_frame();
        }
        break;
    case 5:
        field_screen_pieces_init(0, 0);
        field_map_stream_start();
        field_draw_swap_and_clear_ots();
        field_screen_pieces_draw();
        field_draw_present_overlay();
        mode_sync_party_files();
        mode_unpack_party_files();
        kind = field_map_change_kind;
        frames = field_map_change_frames;
        field_dialogue_open_blocked = 1;
        field_load_from_bundle();
        field_map_stream_stop();
        setRECT(&rect, 0x2C0, 0x100, 0x140, 0xFF);
        field_map_change_kind = kind;
        field_map_change_frames = frames;
        MoveImage(&rect, 0x140, 0xFF);
        if (mode_music_load_pending == -1) {
            field_music_change_track(mode_music_selected_track, 0);
        }
        for (i = 0; i < 4; i++) {
            field_run_pre_frame();
            field_screen_pieces_draw();
            field_run_frame();
            field_run_post_frame();
        }
        break;
    }
    if (field_map_change_kind != 6) {
        field_vram_column_restore();
    }
    field_map_change_kind = 2;
    field_map_change_frames = 0x20;
    field_dialogue_open_blocked = 0;
    field_load_text_palette();
    heap_coalesce();
}

/* 800A6408: Rotate and scale the screen pieces (when scaled) into their quads and
 * link the quads and draw modes of the current buffer. */
void field_screen_pieces_draw(void) {
    MATRIX m;
    VECTOR scale;
    long p;
    long flag;
    s32 i;

    gpu_build_rotation_matrix(&field_screen_pieces_rotation, &m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    scale.vx = field_screen_pieces_scale;
    scale.vy = field_screen_pieces_scale;
    scale.vz = field_screen_pieces_scale;
    ScaleMatrix(&m, &scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0; i < 5; i++) {
        if (field_screen_pieces_scale != 0x1000) {
            RotAverage4(&field_screen_pieces.corners[i][0], &field_screen_pieces.corners[i][1],
                        &field_screen_pieces.corners[i][2], &field_screen_pieces.corners[i][3],
                        (long *)&field_screen_pieces.quads[i][field_draw_buffer_index].x0,
                        (long *)&field_screen_pieces.quads[i][field_draw_buffer_index].x1,
                        (long *)&field_screen_pieces.quads[i][field_draw_buffer_index].x2,
                        (long *)&field_screen_pieces.quads[i][field_draw_buffer_index].x3, &p, &flag);
        }
        addPrim(&field_current_draw_block->overlay_ot[0], &field_screen_pieces.quads[i][field_draw_buffer_index]);
        addPrim(&field_current_draw_block->overlay_ot[0], &field_screen_pieces.modes[i][field_draw_buffer_index]);
    }
}

/* 800A663C: Set up the five 64x224 screen pieces (textured from the screen copy at
 * (2c0, 100)) at full scale with no rotation, with semi-transparency
 * `semitrans` and rate `abr`. */
void field_screen_pieces_init(s32 semitrans, s32 abr) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    SVECTOR *corners;
    s32 i;

    field_screen_pieces_scale = 0x1000;
    field_screen_pieces_rotation.vx = 0;
    field_screen_pieces_rotation.vy = 0;
    field_screen_pieces_rotation.vz = 0;
    for (i = 0; i < 5; i++) {
        quad = &field_screen_pieces.quads[i][0];
        copy = &field_screen_pieces.quads[i][1];
        corners = field_screen_pieces.corners[i];
        SetPolyFT4(quad);
        corners[0].vx = i * 0x20 - 0x50;
        corners[0].vy = -0x38;
        corners[0].vz = 0;
        corners[1].vx = i * 0x20 - 0x30;
        corners[1].vy = -0x38;
        corners[1].vz = 0;
        corners[2].vx = i * 0x20 - 0x50;
        corners[2].vy = 0x38;
        corners[2].vz = 0;
        corners[3].vx = i * 0x20 - 0x30;
        corners[3].vy = 0x38;
        corners[3].vz = 0;
        quad->x0 = i << 6;
        quad->y0 = 0;
        quad->x1 = (i << 6) + 0x40;
        quad->y1 = 0;
        quad->x2 = i << 6;
        quad->y2 = 0xDF;
        quad->x3 = (i << 6) + 0x40;
        quad->y3 = 0xDF;
        setRECT(&field_screen_pieces.windows[i][0], 0, 0, 0xFF, 0xFF);
        setRECT(&field_screen_pieces.windows[i][1], 0, 0, 0xFF, 0xFF);
        SetDrawMode(&field_screen_pieces.modes[i][0], 0, 0, GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100),
                    &field_screen_pieces.windows[i][0]);
        SetDrawMode(&field_screen_pieces.modes[i][1], 0, 0, GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100),
                    &field_screen_pieces.windows[i][1]);
        setRGB0(quad, 0x80, 0x80, 0x80);
        SetSemiTrans(quad, semitrans);
        quad->u0 = 0;
        quad->v0 = 0;
        quad->u1 = 0x40;
        quad->v1 = 0;
        quad->u2 = 0;
        quad->v2 = 0xDF;
        quad->u3 = 0x40;
        quad->v3 = 0xDF;
        quad->tpage = GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100);
        *copy = *quad;
    }
}

/* 800A6924: Present the current draw block: clear, set environments and draw its
 * overlay ordering table. */
void field_draw_present_overlay(void) {
    DrawSync(0);
    VSync(2);
    ClearImage(&field_current_draw_block->draw.clip, 0, 0, 0);
    PutDrawEnv(&field_current_draw_block->draw);
    PutDispEnv(&field_current_draw_block->disp);
    DrawOTag(&field_current_draw_block->overlay_ot[7]);
}

/* 800A6998: Darken corner `corner` of a grid quad by 6 (to 0) while it lies within
 * `radius` of the screen centre (a0, 70). */
void field_screen_grid_darken_corner(POLY_GT4 *quad, s32 corner, s32 radius, s16 *shade) {
    VECTOR offset;
    VECTOR squares;
    s32 centre_x;
    s32 centre_y;

    centre_x = 0xA0;
    centre_y = 0x70;
    offset.vz = 0;
    switch (corner) {
    case 0:
        offset.vx = centre_x - quad->x0;
        offset.vy = centre_y - quad->y0;
        Square0(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r0 = *shade;
            quad->g0 = *shade;
            quad->b0 = *shade;
        }
        break;
    case 1:
        offset.vx = centre_x - quad->x1;
        offset.vy = centre_y - quad->y1;
        Square0(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r1 = *shade;
            quad->g1 = *shade;
            quad->b1 = *shade;
        }
        break;
    case 2:
        offset.vx = centre_x - quad->x2;
        offset.vy = centre_y - quad->y2;
        Square0(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r2 = *shade;
            quad->g2 = *shade;
            quad->b2 = *shade;
        }
        break;
    case 3:
        offset.vx = centre_x - quad->x3;
        offset.vy = centre_y - quad->y3;
        Square0(&offset, &squares);
        if (SquareRoot0(squares.vx + squares.vy) >> 1 < radius) {
            *shade -= 6;
            if (*shade < 0) {
                *shade = 0;
            }
            quad->r3 = *shade;
            quad->g3 = *shade;
            quad->b3 = *shade;
        }
        break;
    }
}

/* 800A6C40: Fade the grid quads of the current buffer and link them, then the grid
 * draw mode, into the overlay ordering table. */
void field_screen_grid_draw(void) {
    POLY_GT4 *quad;
    s32 row;
    s32 column;

    for (row = 0; row < GRID_ROWS; row++) {
        for (column = 0; column < GRID_COLUMNS; column++) {
            quad = &field_screen_grid->quads[field_draw_buffer_index][row * GRID_COLUMNS + column];
            field_screen_grid_darken_corner(quad, 0, field_screen_grid_fade_radius, &field_screen_grid->shade[0][row * GRID_COLUMNS + column]);
            field_screen_grid_darken_corner(quad, 1, field_screen_grid_fade_radius, &field_screen_grid->shade[1][row * GRID_COLUMNS + column]);
            field_screen_grid_darken_corner(quad, 2, field_screen_grid_fade_radius, &field_screen_grid->shade[2][row * GRID_COLUMNS + column]);
            field_screen_grid_darken_corner(quad, 3, field_screen_grid_fade_radius, &field_screen_grid->shade[3][row * GRID_COLUMNS + column]);
            addPrim(&field_current_draw_block->overlay_ot[0], &field_screen_grid->quads[field_draw_buffer_index][row * GRID_COLUMNS + column]);
        }
    }
    addPrim(&field_current_draw_block->overlay_ot[0], &field_texture_window_modes[field_draw_buffer_index][4]);
}

/* 800A6E70: Build the screen grid: 16x16 half-bright quads textured from the
 * 15-bit screen copy at (2c0, 100), their corners at full shade. */
void field_screen_grid_build(void) {
    POLY_GT4 *quad;
    POLY_GT4 *copy;
    s32 row;
    s32 column;
    s32 k;

    field_screen_grid = heap_alloc(sizeof(ScreenGrid), 1);
    for (row = 0; row < GRID_ROWS; row++) {
        for (column = 0; column < GRID_COLUMNS; column++) {
            k = row * GRID_COLUMNS + column;
            quad = &field_screen_grid->quads[0][k];
            copy = &field_screen_grid->quads[1][k];
            field_screen_grid->shade[0][k] = 0x80;
            field_screen_grid->shade[1][k] = 0x80;
            field_screen_grid->shade[2][k] = 0x80;
            field_screen_grid->shade[3][k] = 0x80;
            SetPolyGT4(quad);
            setRGB0(quad, 0x80, 0x80, 0x80);
            quad->r1 = 0x80;
            quad->g1 = 0x80;
            quad->b1 = 0x80;
            quad->r2 = 0x80;
            quad->g2 = 0x80;
            quad->b2 = 0x80;
            quad->r3 = 0x80;
            quad->g3 = 0x80;
            quad->b3 = 0x80;
            quad->x0 = column * 16;
            quad->y0 = row * 16;
            quad->x1 = column * 16 + 16;
            quad->y1 = row * 16;
            quad->x2 = column * 16;
            quad->y2 = row * 16 + 16;
            quad->x3 = column * 16 + 16;
            quad->y3 = row * 16 + 16;
            quad->u0 = (column * 16) & 0x3F;
            quad->v0 = row * 16;
            quad->u1 = ((column * 16) & 0x3F) + 16;
            quad->v1 = row * 16;
            quad->u2 = (column * 16) & 0x3F;
            quad->v2 = row * 16 + 16;
            quad->u3 = ((column * 16) & 0x3F) + 16;
            quad->v3 = row * 16 + 16;
            quad->tpage = GetTPage(2, 1, 0x2C0 + column / 4 * 0x40, 0x100);
            SetSemiTrans(quad, 1);
            *copy = *quad;
        }
    }
}

/* 800A7064: Release the screen grid. */
void field_screen_grid_release(void) {
    heap_free(field_screen_grid);
}

/* 800A708C: Set up the movie player for a 320x224 picture. */
void field_movie_init_player(void) {
    heap_select_owner_tag(4, 0);
    if (field_movie_mode == 2) {
        movie_split_display = 1;
    } else {
        movie_split_display = 0;
    }
    movie_open(0x140, 0xE0, 0x80, 0x10, 0x20, 0x800, FIELD_MOVIE.depth24);
    field_movie_stopped = 0;
    heap_select_owner_tag(8, 0);
}

/* 800A7120: Movie frame callback: record the frame, select the draw block for the
 * decoded buffer (24-bit display when set). */
void field_movie_frame_callback(u16 frame, u16 x, u16 buffer) {
    field_movie_frame = frame;
    field_movie_awaiting_frame = 0;
    if (buffer == 0) {
        field_movie_decoded_block = 1;
    } else {
        field_movie_decoded_block = 0;
    }
    if (field_movie_mode == 0 && field_staff_roll_drawing == 0) {
        DrawSync(0);
        field_current_draw_block = &field_draw_blocks[field_movie_decoded_block];
        if ((s16)FIELD_MOVIE.depth24 == 1) {
            field_draw_blocks[field_movie_decoded_block & 1].disp.isrgb24 = 1;
        }
    }
}

/* 800A7218: Start the field movie with the current parameters. */
void field_movie_start(void) {
    s32 mode;

    field_movie_frame = 0;
    heap_select_owner_tag(4, 0);
    if (field_movie_stopped == 0) {
        cd_select_directory(0x18, 1);
        mode = 1;
        if (FIELD_MOVIE.sound_bank != 0xFF || (field_movie_fade_bits & 0x40)) {
            mode = 3;
        }
        movie_start(FIELD_MOVIE.file + 2, FIELD_MOVIE.unk2A, FIELD_MOVIE.sound_start,
                    FIELD_MOVIE.unk2E, 1, mode, FIELD_MOVIE.unk3A, FIELD_MOVIE.x, FIELD_MOVIE.y,
                    FIELD_MOVIE.source_x, FIELD_MOVIE.source_y, 0xE0, field_movie_frame_callback);
        cd_select_directory(4, 0);
    }
    heap_select_owner_tag(8, 0);
}

/* 800A732C: Run `frames` movie frames (with field sound) unless the movie stopped. */
void field_movie_run_frames(s32 frames) {
    s32 i;

    boot_check_soft_reset();
    if (field_movie_stopped == 0) {
        for (i = 0; i < frames; i++) {
            movie_poll();
            field_movie_play_due_sounds();
        }
    }
}

/* 800A7394: Keep the field running until the stream is idle and draw buffer 0 is
 * current, then wait for CD data. */
void field_movie_wait_disc_idle(void) {
    do {
        do {
            field_run_pre_frame();
            field_run_frame();
        } while (cd_get_pending_read_count() != 0);
    } while (field_draw_buffer_index != 0);
    CdDataSync(0);
}

/* 800A73E8: After a movie: reload the VRAM kept in party sprite blocks 1 and 2
 * (unless movie mode 2), then release both blocks. */
void field_movie_park_party_sprites(void) {
    RECT rect;

    if (field_movie_mode == 2) {
        heap_unprotect_block(mode_party_sprite_blocks[1]);
        heap_unprotect_block(mode_party_sprite_blocks[2]);
        heap_free(mode_party_sprite_blocks[1]);
        heap_free(mode_party_sprite_blocks[2]);
    } else {
        setRECT(&rect, 0x200, 0, 0x140, 0x80);
        LoadImage(&rect, mode_party_sprite_blocks[1]);
        DrawSync(0);
        setRECT(&rect, 0x200, 0x80, 0x140, 0x80);
        LoadImage(&rect, mode_party_sprite_blocks[2]);
        DrawSync(0);
        heap_unprotect_block(mode_party_sprite_blocks[1]);
        heap_unprotect_block(mode_party_sprite_blocks[2]);
        heap_free(mode_party_sprite_blocks[1]);
        heap_free(mode_party_sprite_blocks[2]);
    }
}

/* 800A74F8: Before a movie: allocate party sprite blocks 1 and 2 and save the VRAM
 * at (200, 0) in them; in movie mode 2 instead reload the sprites of party
 * slots 1 and 2 from their character files. */
void field_movie_restore_party_sprites(void) {
    RECT rect;
    FileRequest requests[4];
    s32 count;
    s32 i;

    if (field_movie_mode == 2) {
        mode_party_sprite_blocks[1] = heap_alloc(0x14000, 0);
        mode_party_sprite_blocks[2] = heap_alloc(0x14000, 0);
        heap_protect_block(mode_party_sprite_blocks[1]);
        heap_protect_block(mode_party_sprite_blocks[2]);
        cd_select_directory(4, 0);
        count = 0;
        for (i = 1; i < 3; i++) {
            if (mode_party_file_ids[i] != 0xFF) {
                requests[count].file = mode_party_file_ids[i] + 5;
                requests[count].destination = mode_party_file_blocks[i] = heap_alloc(cd_get_aligned_file_size(mode_party_file_ids[i] + 5), 1);
                count++;
            }
        }
        requests[count].destination = NULL;
        requests[count].file = 0;
        cd_read_file_list(requests, 0, 0);
        cd_sync_reads(0);
        for (i = 1; i < 3; i++) {
            if (mode_party_members[i] != 0xFF) {
                text_unpack_lzss(mode_party_file_blocks[i], mode_party_sprite_blocks[i]);
                heap_free(mode_party_file_blocks[i]);
            }
        }
        return;
    }
    mode_party_sprite_blocks[1] = heap_alloc(0x14000, 0);
    mode_party_sprite_blocks[2] = heap_alloc(0x14000, 0);
    heap_protect_block(mode_party_sprite_blocks[1]);
    heap_protect_block(mode_party_sprite_blocks[2]);
    setRECT(&rect, 0x200, 0, 0x140, 0x80);
    StoreImage(&rect, mode_party_sprite_blocks[1]);
    DrawSync(0);
    setRECT(&rect, 0x200, 0x80, 0x140, 0x80);
    StoreImage(&rect, mode_party_sprite_blocks[2]);
    DrawSync(0);
}

/* 800A7744: Next byte of the packed stream (four per word), scaled down by 8 but
 * at least 1 when nonzero. */
u32 field_screen_read_24bit_channel(void) {
    u32 value;

    if ((field_screen_convert_nibble_count & 3) == 0) {
        field_screen_convert_word = *field_screen_convert_stream;
        field_screen_convert_stream++;
    }
    field_screen_convert_nibble_count++;
    value = field_screen_convert_word & 0xFF;
    field_screen_convert_word >>= 8;
    if (value != 0) {
        value >>= 3;
        if (value == 0) {
            value = 1;
        }
    }
    return value;
}

/* 800A77C4: Convert the 24-bit screen (five 96-pixel columns at x 0) to 15-bit
 * pixels at (0..0x140, 100). Both callers pass 0, which it ignores. */
void field_screen_convert_24bit_to_15bit(s32 unused) {
    RECT rect;
    u32 *packed;
    u32 *pixels;
    u32 value;
    s32 i;
    s32 j;

    packed = heap_alloc(0xA800, 0);
    pixels = heap_alloc(0x7000, 0);
    for (i = 0; i < 5; i++) {
        rect.x = i * 0x60;
        rect.y = 0;
        rect.w = 0x60;
        rect.h = 0xE0;
        StoreImage(&rect, (u_long *)packed);
        DrawSync(0);
        field_screen_convert_stream = packed;
        field_screen_convert_pixels = pixels;
        field_screen_convert_nibble_count = 0;
        for (j = 0; j < 0x1C00; j++) {
            value = field_screen_read_24bit_channel();
            value |= field_screen_read_24bit_channel() << 5;
            value |= field_screen_read_24bit_channel() << 10;
            value |= field_screen_read_24bit_channel() << 16;
            value |= field_screen_read_24bit_channel() << 21;
            value |= field_screen_read_24bit_channel() << 26;
            *field_screen_convert_pixels = value;
            field_screen_convert_pixels++;
        }
        rect.x = i << 6;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0xE0;
        LoadImage(&rect, (u_long *)pixels);
        DrawSync(0);
    }
    heap_free(packed);
    heap_free(pixels);
}

/* 800A7948: While movie frames 687..18e2 play (when the sequence is enabled),
 * switch to 640-wide draw buffers and draw the file 0xab sequence over
 * the movie each frame, then restore the 320-wide buffers. Declared int
 * without a value, as the original's live result register shows. */
s32 field_staff_roll_run_over_movie(void) {
    RECT rect;

    if (mode_staff_roll_enabled == 0 || field_movie_frame < 0x687) {
        return;
    }
    if (field_movie_frame < 0x18E2) {
        field_staff_roll_drawing = 1;
        movie_load_restart = 0;
        setRECT(&rect, 0, 0, 0x500, 0x200);
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&field_draw_blocks[0].draw, 0, 0, 0x280, 0xE0);
        SetDefDrawEnv(&field_draw_blocks[1].draw, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&field_draw_blocks[0].disp, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&field_draw_blocks[1].disp, 0, 0, 0x280, 0xE0);
        field_draw_blocks[1].disp.isrgb24 = 0;
        field_draw_blocks[0].disp.isrgb24 = 0;
        PutDispEnv(&field_current_draw_block->disp);
        PutDrawEnv(&field_current_draw_block->draw);
        setRECT(&rect, 0x300, 0, 0x200, 0x100);
        ClearImage(&rect, 0, 0, 0);
        field_staff_roll_upload_font();
        VSync(0);
        DrawSync(0);
        while (field_movie_frame < 0x18E2) {
            boot_check_soft_reset();
            field_draw_switch_block();
            if (field_movie_frame < 0x18DE) {
                field_staff_roll_draw();
            }
            DrawSync(0);
            VSync(2);
            ClearImage(&field_current_draw_block->draw.clip, 0, 0, 0);
            PutDispEnv(&field_current_draw_block->disp);
            PutDrawEnv(&field_current_draw_block->draw);
            DrawOTag(&field_current_draw_block->overlay_ot[7]);
            field_staff_roll_advance();
            field_movie_run_frames(5);
        }
        field_staff_roll_drawing = 0;
        movie_load_restart = 1;
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&field_draw_blocks[0].draw, 0, 0, 0x140, 0xE0);
        SetDefDrawEnv(&field_draw_blocks[1].draw, 0, 0x100, 0x140, 0xE0);
        SetDefDispEnv(&field_draw_blocks[0].disp, 0, 0x100, 0x140, 0xE0);
        SetDefDispEnv(&field_draw_blocks[1].disp, 0, 0, 0x140, 0xE0);
        field_draw_blocks[1].disp.isrgb24 = 1;
        field_draw_blocks[0].disp.isrgb24 = 1;
        field_draw_blocks[1].disp.isinter = 0;
        field_draw_blocks[0].disp.isinter = 0;
        field_staff_roll_release();
    }
}

/* 800A7C58: Play the requested field movie (800c3a20..): move the movie library
 * (file 0xa9) into place, park the VRAM the movie uses, stop the field's
 * effects and the 801e module, then decode and present frames by movie
 * mode until it ends or is skipped; finally restore VRAM, the display and
 * the field stream. */
void field_movie_play(void) {
    RECT rect;
    u8 *data;
    u8 *library;
    s32 top;
    s32 i;

    field_movie_end_count = 0;
    field_movie_awaiting_frame = 0;
    field_movie_decoded_block = 0;
    data = heap_alloc(cd_get_aligned_file_size(0xA9), 0);
    cd_read_file(0xA9, data, 0, 0x80);
    field_movie_frame = 0;
    field_staff_roll_drawing = 0;
    field_movie_wait_disc_idle();
    cd_select_directory(0x18, 0);
    cd_seek_or_pause_if_idle(FIELD_MOVIE.file);
    cd_select_directory(4, 0);
    field_movie_wait_disc_idle();
    if (field_work.unk2264 != 0) {
        gear_model_shut_down();
        field_sync_and_flush_cache();
        field_sync_draw_and_vsync();
        heap_free(field_layer_module);
    }
    field_effect_release_all_slots();
    if (field_movie_mode != 2) {
        setRECT(&rect, 0x140, 0, 0xC0, 0x100);
        LoadImage(&rect, (u_long *)data);
        DrawSync(0);
        heap_free(data);
        data = heap_alloc(0x18000, 0);
        StoreImage(&rect, (u_long *)data);
    }
    top = field_heap_top;
    if (mode_field_standalone == 0) {
        library = heap_alloc((top & 0xFFFFFF) - 0x1D3008, 1);
        memcpy(library, data, cd_get_aligned_file_size(0xA9));
    } else {
        library = heap_alloc(8, 1);
    }
    heap_free(data);
    field_movie_load_sound_bank();
    field_staff_roll_start();
    field_movie_awaiting_frame = 1;
    field_movie_park_party_sprites();
    field_sync_and_flush_cache();
    heap_coalesce();
    field_movie_init_player();
    field_movie_start();
    field_movie_presenting = 1;
    do {
        VSync(0);
        field_movie_run_frames(3);
    } while (field_movie_awaiting_frame != 0);
    do {
        switch (field_movie_mode) {
        case 1:
            field_draw_switch_block();
            field_movie_run_overlay_frame();
            field_movie_run_frames(6);
            break;
        case 0:
            DrawSync(0);
            VSync(0);
            PutDispEnv(&field_current_draw_block->disp);
            PutDrawEnv(&field_current_draw_block->draw);
            field_movie_run_frames(3);
            DrawSync(0);
            VSync(0);
            PutDispEnv(&field_current_draw_block->disp);
            PutDrawEnv(&field_current_draw_block->draw);
            field_movie_run_frames(3);
            field_staff_roll_run_over_movie();
            break;
        case 2:
            field_run_pre_frame();
            field_run_frame();
            field_movie_run_frames(9);
            break;
        }
        if (field_monitor_absent == 0) {
            if (field_movie_mode == 2) {
                if ((field_pad_port0_repeated & 0x80) || field_movie_end_count != 0) {
                    break;
                }
            } else {
                field_pad_drain_queue();
                if (field_pad_port0_repeated & 0x20) {
                    break;
                }
            }
        } else if (field_movie_fade_bits & 0x80) {
            field_pad_drain_queue();
            if (field_pad_port0_repeated & 0x20) {
                sound_set_cd_volume(0, 10);
                for (i = 0; i < 5; i++) {
                    VSync(0);
                }
                break;
            }
        }
        if (field_movie_mode == 2 && field_movie_end_count != 0) {
            break;
        }
    } while ((s16)FIELD_MOVIE.unk3A != 0 || field_movie_frame < FIELD_MOVIE.unk2E);
    VSync(0);
    DrawSync(0);
    movie_close();
    field_sync_and_flush_cache();
    PutDispEnv(&field_current_draw_block->disp);
    PutDrawEnv(&field_current_draw_block->draw);
    VSync(0);
    DrawSync(0);
    heap_free(library);
    heap_coalesce();
    field_movie_restore_party_sprites();
    field_layer_load();
    setRECT(&rect, 0, field_movie_decoded_block << 8, 0x1E0, 0xE0);
    MoveImage(&rect, 0, field_movie_decoded_block << 8);
    DrawSync(0);
    VSync(0);
    field_current_draw_block = &field_draw_blocks[1];
    PutDispEnv(&field_draw_blocks[1].disp);
    PutDrawEnv(&field_current_draw_block->draw);
    if (field_movie_mode != 2) {
        field_screen_convert_24bit_to_15bit(0);
    }
    VSync(0);
    field_draw_blocks[0].disp.isrgb24 = 0;
    field_current_draw_block = &field_draw_blocks[0];
    PutDispEnv(&field_draw_blocks[0].disp);
    PutDrawEnv(&field_current_draw_block->draw);
    setRECT(&rect, 0, 0x100, 0x140, 0xE0);
    MoveImage(&rect, 0, 0);
    DrawSync(0);
    VSync(0);
    field_draw_blocks[1].disp.isrgb24 = 0;
    field_current_draw_block = &field_draw_blocks[1];
    PutDispEnv(&field_draw_blocks[1].disp);
    PutDrawEnv(&field_current_draw_block->draw);
    if (field_no_panorama_after_return != 0) {
        field_panorama_hidden = 1;
    } else {
        field_panorama_hidden = 0;
    }
    field_layer_start();
    if (field_movie_mode != 2) {
        cd_select_directory(4, 0);
        heap_select_owner_tag(8, 0);
        field_map_stream_running = 0;
        field_map_stream_start();
        field_map_stream_stop();
        field_transition_frames = 0x20;
        field_transition_kind = 1;
    }
    field_movie_release_sound_bank();
    FIELD_MOVIE.sound_bank = 0xFF;
    field_movie_mode = 0;
    field_movie_stopped = -1;
}

/* field.c's TIM upload, declared here for the call below: the calls in
 * field_event.c and the first in field_effect.c pass it unnarrowed
 * ints without a prototype, so no header declares it. */
void field_load_tim_at(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

/* 800A8314: Load file 0xaa and upload its image to (380, 0) with its CLUT at
 * (0, e8). */
void field_status_panel_load_image(void) {
    u32 *data;

    heap_select_owner_tag(8, 0);
    cd_select_directory(4, 0);
    data = heap_alloc(cd_get_aligned_file_size(0xAA), 1);
    cd_read_file(0xAA, data, 0, 0x80);
    cd_sync_reads(0);
    field_load_tim_at(data, 0x380, 0, 0, 0xE8, 0, 0);
    DrawSync(0);
    heap_free(data);
}

/* 800A83B4: Release the two primitive buffers once allocated. */
void field_status_panel_release(void) {
    if (field_status_panel_built != 0) {
        field_status_panel_built = 0;
        DrawSync(0);
        heap_free(field_status_panel_quads[0]);
        heap_free(field_status_panel_quads[1]);
    }
}

/* 800A8408: Texture panel piece `index` of the current buffer with its frame moved
 * by (du, dv), flipped vertically. The original passes the coordinates to
 * 8007a44c unconverted (no s16 prototype in scope: a separate unit). */
void field_status_panel_set_piece_uv(s32 index, s32 du, s32 dv) {
    s32 frame;
    s32 u;
    s32 v;
    s32 w;
    s32 h;
    s32 unused[6]; /* the original frame reserves an unused 24-byte local */

    frame = field_status_panel_pieces[index].frame;
    v = field_status_panel_texture_frames[frame].v + dv;
    w = field_status_panel_texture_frames[frame].w;
    u = field_status_panel_texture_frames[frame].u + du;
    h = field_status_panel_texture_frames[frame].h;
    field_set_quad_uvs_clamped(&field_status_panel_quads[field_draw_buffer_index][index], u, v + h - 1, u + w, v + h - 1, u, v - 1, u + w, v - 1);
}

/* 800A84C0: Draw the status panel: rotate the compass strip by the view angle, the
 * pitch strip by the camera pitch, their digits, the blinking marker and
 * the rest of the pieces. */
void field_status_panel_draw(void) {
    s32 pitch;
    s32 angle;
    s32 count;
    s32 i;

    if (field_status_panel_built != 0) {
        count = PANEL_PIECES;
        pitch = ratan2(field_compute_planar_length((field_view.target.vx - field_view.eye.vx) >> 16,
                                     (field_view.target.vz - field_view.eye.vz) >> 16),
                       (field_view.target.vy - field_view.eye.vy) >> 16) &
                0xFFF;
        angle = field_view.view_angle & 0xFFF;
        for (i = 0; i < 16; i++) {
            field_status_panel_set_piece_uv(i, (angle >> 4) & 0xF, 0);
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        for (; i < 29; i++) {
            field_status_panel_set_piece_uv(i, 0, (pitch >> 4) & 0xF);
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        for (; i < 34; i++) {
            field_status_panel_set_piece_uv(i, (angle & 0xF) * 8, 0);
            angle >>= 2;
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        for (; i < 39; i++) {
            field_status_panel_set_piece_uv(i, (pitch & 0xF) * 8, 0);
            pitch >>= 2;
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        for (; i < 40; i++) {
            if (!(field_status_panel_draw_count & 0xF)) {
                field_status_panel_blink_phase++;
            }
            if (field_status_panel_blink_phase >= 3) {
                field_status_panel_blink_phase = 0;
            }
            field_status_panel_set_piece_uv(i, 0, field_status_panel_blink_phase * 8);
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        for (; i < 42; i++) {
            field_status_panel_set_piece_uv(i, (field_status_panel_draw_count >> 2) & 0xF, 0);
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        for (; i < 43; i++) {
            if (!(field_status_panel_draw_count & 0x10)) {
                addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
            }
        }
        for (; i < count; i++) {
            addPrim(&field_current_draw_block->overlay_ot[4], &field_status_panel_quads[field_draw_buffer_index][i]);
        }
        field_status_panel_draw_count++;
    }
}

#define FRAME(n, field) (((u16 *)field_status_panel_texture_frames)[(n) * 4 + (field)])
enum { FRAME_U, FRAME_V, FRAME_W, FRAME_H };
#define PIECE(n, field) (((u16 *)field_status_panel_pieces)[(n) * 4 + (field)])
enum { PIECE_X, PIECE_Y, PIECE_FRAME, PIECE_FLAGS };

/* 800A8BA4: Build the status panel: load its image, allocate the quads of both
 * buffers and texture each piece from its frame (flip in flags bits 0-3;
 * bits 4-7 pick semi-transparency rate 1 or 2 on a 4-bit page, other values
 * keep the previous piece's rate, 0 before the first). The original passes
 * the coordinates to 8007a44c unconverted (no s16 prototype in scope: a
 * separate unit). Both tables are read one unsigned halfword field at a
 * time, so each field has its own base constant (layout + 6, layout and
 * frames + 4 stay in registers). The page flags, the position and the flip
 * flags are read through three copies of the piece index made after the
 * reads, so loop.c cannot turn the scaled reads into pointers, and after the
 * exit test, so the counter increment stays in place (before it the
 * increment gains a move). loop.c reduces the combined copies to a counter of
 * their own, the original's layout index beside the loop counter: each copy
 * is worth 1 against the add's cost of 2, so three is the minimum (one:
 * -6107 vs 171; two: 0 vs 172), and which read uses which copy cannot be
 * recovered. */
void field_status_panel_build(void) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    POLY_FT4 **prims;
    s32 mode;
    s32 i;
    s32 page_piece;
    s32 piece;
    s32 flip_piece;
    s32 x, y, frame, u, v, w, h;

    field_status_panel_load_image();
    mode = 0;
    field_status_panel_blink_phase = 0;
    heap_select_owner_tag(8, 0);
    prims = field_status_panel_quads;
    prims[0] = heap_alloc(PANEL_PIECES * sizeof(POLY_FT4), 0);
    prims[1] = heap_alloc(PANEL_PIECES * sizeof(POLY_FT4), 0);
    page_piece = piece = flip_piece = 0;
    i = 0;
    for (;;) {
        quad = &prims[0][i];
        copy = &prims[1][i];
        SetPolyFT4(quad);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad->clut = GetClut(0, 0xE8);
        switch ((PIECE(page_piece, PIECE_FLAGS) >> 4) & 0xF) {
        case 0:
            mode = 1;
            break;
        case 1:
            mode = 2;
            break;
        }
        quad->tpage = GetTPage(0, mode, 0x380, 0);
        SetSemiTrans(quad, 1);
        x = PIECE(piece, PIECE_X);
        y = PIECE(piece, PIECE_Y);
        frame = PIECE(piece, PIECE_FRAME);
        w = FRAME(frame, FRAME_W);
        h = FRAME(frame, FRAME_H);
        u = FRAME(frame, FRAME_U);
        v = FRAME(frame, FRAME_V);
        setXYWH(quad, x, y, w, h);
        switch (PIECE(flip_piece, PIECE_FLAGS) & 0xF) {
        case 0:
            field_set_quad_uvs_clamped(quad, u, v, u + w, v, u, v + h, u + w, v + h);
            break;
        case 1:
            field_set_quad_uvs_clamped(quad, u + w - 1, v, u - 1, v, u + w - 1, v + h, u - 1, v + h);
            break;
        case 2:
            field_set_quad_uvs_clamped(quad, u, v + h - 1, u + w, v + h - 1, u, v - 1, u + w, v - 1);
            break;
        case 3:
            field_set_quad_uvs_clamped(quad, u + w - 1, v + h - 1, u - 1, v + h - 1, u + w - 1, v - 1, u - 1, v - 1);
            break;
        }
        *copy = *quad;
        if (++i >= PANEL_PIECES) {
            break;
        }
        page_piece = piece = flip_piece = i;
    }
    field_status_panel_built = 1;
}

/* 800A8EAC: Set up a particle's quad in both buffers from sprite `sprite` with
 * semi-transparency rate `abr`.
 * The corners scale each term by 16 before subtracting/adding: x * 16
 * and y * 16 are computed once, but w * 16 and h * 16 twice (once for the
 * left/top edge, once for the right/bottom). Shifting in the differences
 * keeps GCC from folding x * 16 - w * 16 into (x - w) * 16, and the
 * (u16) view of the half sizes in the sums (a no-op on their values) keeps
 * CSE from sharing the second w * 16 / h * 16 with the first.
 * The second buffer's quad is taken as a pointer up front; the block copy
 * then starts from a copy of it (addiu v1,s0,0x78; move a2,v1). */
void field_effect_init_particle_quads(Particle *particle, s32 sprite, s32 abr) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    s32 half_w;
    s32 half_h;
    s32 x;
    s32 y;
    SVECTOR unused[11]; /* unused in the original; reserves 88 bytes */

    quad = &particle->quads[0];
    copy = &particle->quads[1];
    SetPolyFT4(quad);
    half_w = field_effect_particle_sprites[sprite].half_w;
    half_h = field_effect_particle_sprites[sprite].half_h;
    x = field_effect_particle_sprites[sprite].x;
    y = field_effect_particle_sprites[sprite].y;
    particle->corners[0].vz = 0;
    particle->corners[1].vz = 0;
    particle->corners[2].vz = 0;
    particle->corners[3].vz = 0;
    setRGB0(quad, 0x80, 0x80, 0x80);
    particle->corners[0].vx = (x << 4) - (half_w << 4);
    particle->corners[0].vy = (y << 4) - (half_h << 4);
    particle->corners[1].vx = (u16)half_w * 16 + (x << 4);
    particle->corners[1].vy = (y << 4) - (half_h << 4);
    particle->corners[2].vx = (x << 4) - (half_w << 4);
    particle->corners[2].vy = (u16)half_h * 16 + (y << 4);
    particle->corners[3].vx = (u16)half_w * 16 + (x << 4);
    particle->corners[3].vy = (u16)half_h * 16 + (y << 4);
    field_set_quad_uvs_clamped(quad, field_effect_particle_sprites[sprite].uv[0][0], field_effect_particle_sprites[sprite].uv[0][1] + 0x40,
                  field_effect_particle_sprites[sprite].uv[1][0] - 1, field_effect_particle_sprites[sprite].uv[1][1] + 0x40,
                  field_effect_particle_sprites[sprite].uv[2][0], field_effect_particle_sprites[sprite].uv[2][1] + 0x3F,
                  field_effect_particle_sprites[sprite].uv[3][0] - 1, field_effect_particle_sprites[sprite].uv[3][1] + 0x3F);
    SetSemiTrans(quad, 1);
    quad->tpage = GetTPage(0, abr, 0x3C0, 0x140);
    quad->clut = GetClut(0x100, 0xF7);
    *copy = *quad;
}

/* 800A90B4: Move the saved screen column into a new block allocated with `flags`. */
void field_vram_column_relocate(s32 flags) {
    ScreenColumn *copy;

    if (field_vram_column_saved == 1) {
        heap_select_owner_tag(8, 0);
        copy = heap_alloc(0x8000, flags);
        *copy = *field_vram_column_copy;
        heap_free(field_vram_column_copy);
        field_vram_column_copy = copy;
    }
}

/* 800A915C: Save the 64x256 VRAM column at (3c0, 100) once. */
void field_vram_column_save(void) {
    if (field_vram_column_saved != 1) {
        field_vram_column_saved = 1;
        heap_select_owner_tag(8, 0);
        field_vram_column_copy = heap_alloc(0x8000, 1);
        field_vram_column_rect.x = 0x3C0;
        field_vram_column_rect.y = 0x100;
        field_vram_column_rect.w = 0x40;
        field_vram_column_rect.h = 0x100;
        StoreImage(&field_vram_column_rect, field_vram_column_copy->words);
        DrawSync(0);
    }
}

/* 800A91F0: Restore the saved VRAM column and release it. */
void field_vram_column_restore(void) {
    if (field_vram_column_saved != 0) {
        field_vram_column_rect.x = 0x3C0;
        field_vram_column_saved = 0;
        field_vram_column_rect.y = 0x100;
        field_vram_column_rect.w = 0x40;
        field_vram_column_rect.h = 0x100;
        LoadImage(&field_vram_column_rect, field_vram_column_copy->words);
        DrawSync(0);
        heap_free(field_vram_column_copy);
    }
}
