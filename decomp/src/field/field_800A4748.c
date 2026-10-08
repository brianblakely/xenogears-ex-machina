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
#include "field.h"
#include "field_anim.h"
#include "field_gte.h"
#include "field_motion.h"
#include "field_script.h"
#include "field_actor_events.h"
#include "field_screen.h"
#include "field_panel.h"

/* The saved strip sources (x, y) of the screen effects. */
DVECTOR D_800AEB24[15] = {
    {0, 0xB0}, {0x40, 0xB0}, {0x80, 0xB0}, {0xC0, 0xB0}, {0x100, 0xB0},
    {0, 0xC0}, {0x40, 0xC0}, {0x80, 0xC0}, {0xC0, 0xC0}, {0x100, 0xC0},
    {0, 0xD0}, {0x40, 0xD0}, {0x80, 0xD0}, {0xC0, 0xD0}, {0x100, 0xD0},
};

s32 D_800AEB60 = 0; /* panel frame counter */
s32 D_800AEB64 = 0; /* panel blinking frame 0..2 */

/* The status panel's texture frames (u, v, w, h) and its layout pieces
 * (x, y, frame, flags). */
PanelFrame D_800AEB68[117] = {
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
PanelPiece D_800AEF10[PANEL_PIECES] = {
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

s32 D_800AF278 = 0; /* panel primitive buffers allocated */

/* A particle sprite (800af27c): half size, centre, and the four texture
 * corners (u, v) within the sprite page. */
typedef struct {
    u16 half_w;
    u16 half_h;
    u16 x;
    u16 y;
    u16 uv[4][2];
} ParticleSprite;

ParticleSprite D_800AF27C[21] = {
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

void func_800A476C(s32 x, s32 y);

/* Reset the screen effect view with the image moved to (2c0, 100). */
void func_800A4748(void) {
    func_800A476C(0x2C0, 0x100);
}

/* Restore the projection distance and move the 320x224 screen image to
 * (x, y). */
void func_800A476C(s32 x, s32 y) {
    RECT rect;

    rect.w = 0x140;
    rect.y = 0;
    rect.x = 0;
    rect.h = 0xE0;
    SetGeomScreen(0x200);
    MoveImage(&rect, x, y);
    func_800775F8();
}

extern s32 D_800ADB24; /* screen effect buffers allocated */

/* Stop the screen effect and release its buffers. */
void func_800A47D4(void) {
    D_800B2078.unk2078 = 0;
    if (D_800ADB24 != 0) {
        func_800320E8(D_800B2078.effect_buffers[0]);
        func_800320E8(D_800B2078.effect_buffers[1]);
        func_800320E8(D_800B2078.effect_buffers[2]);
        func_800320E8(D_800B2078.effect_buffers[3]);
        D_800ADB24 = 0;
    }
}

/* Start the screen distortion: on first use allocate its buffers, build the
 * 20x17 grid of 16x16 textured quads (rows 14-16 sample the saved strips at
 * (3c0, 0)) and the screen/strip copy commands; unless resuming, read the
 * six targets and the step count from the operands. The strip loop reuses
 * the row counter and reads the strip x through a pointer to the table
 * (its own induction pointer; y is read from the table). */
void func_800A484C(s32 resume) {
    RECT rect;
    s32 row;
    s32 column;
    POLY_FT4 *poly;
    DVECTOR *strips;
    POLY_FT4 *copy;
    s32 u;

    D_800B2078.unk2078 = 1;
    if (D_800ADB24 == 0) {
        D_800B2078.effect_buffers[0] = func_80031BDC(0x180, 0);
        D_800B2078.effect_buffers[1] = func_80031BDC(0x180, 0);
        D_800B2078.effect_buffers[2] = func_80031BDC(0x3840, 0);
        D_800B2078.effect_buffers[3] = func_80031BDC(0x3840, 0);
        D_800ADB24 = 1;
        for (row = 0; row < 17; row++) {
            for (column = 0; column < 20; column++) {
                poly = (POLY_FT4 *)D_800B2078.effect_buffers[2] + (column + row * 20);
                copy = (POLY_FT4 *)D_800B2078.effect_buffers[3] + (column + row * 20);
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
        SetDrawMove(D_800B2078.effect_buffers[0], &rect, 0, 0);
        rect.y = 0x120;
        SetDrawMove(D_800B2078.effect_buffers[1], &rect, 0, 0x100);
        rect.w = 0x40;
        rect.h = 0x10;
        strips = D_800AEB24;
        for (row = 0; row < 15; row++) {
            rect.x = strips[row].vx;
            rect.y = D_800AEB24[row].vy;
            SetDrawMove((DR_MOVE *)D_800B2078.effect_buffers[0] + (row + 1), &rect, 0x3C0, row * 16);
            rect.y = D_800AEB24[row].vy + 0x100;
            SetDrawMove((DR_MOVE *)D_800B2078.effect_buffers[1] + (row + 1), &rect, 0x3C0, row * 16);
        }
    }
    if (resume == 0) {
        func_800A4CC4(func_800ACDEC(1), func_800ACDEC(3), func_800ACDEC(5), func_800ACDEC(7),
                      func_800ACDEC(9), func_800ACDEC(11), func_800ACDEC(13));
    }
    D_800B2078.unk207A = 0;
}

/* Move the six screen effect values to the given whole targets over
 * `steps` frames. */
void func_800A4CC4(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 steps) {
    s32 step0;
    s32 step1;
    s32 step2;
    s32 step3;
    s32 step4;
    s32 step5;

    if (steps == 0) {
        steps = 1;
    }
    step0 = ((a << 16) - D_800B2078.effect_value[0]) / steps;
    step1 = ((b << 16) - D_800B2078.effect_value[1]) / steps;
    step2 = ((c << 16) - D_800B2078.effect_value[2]) / steps;
    step3 = ((d << 16) - D_800B2078.effect_value[3]) / steps;
    step4 = ((e << 16) - D_800B2078.effect_value[4]) / steps;
    step5 = ((f << 16) - D_800B2078.effect_value[5]) / steps;
    D_800B2078.effect_steps = steps;
    D_800B2078.effect_step[0] = step0;
    D_800B2078.effect_step[1] = step1;
    D_800B2078.effect_step[2] = step2;
    D_800B2078.effect_step[3] = step3;
    D_800B2078.effect_step[4] = step4;
    D_800B2078.effect_step[5] = step5;
}

/* The distortion grid of the buffer being drawn: 20 x 17 textured quads. */
#define EFFECT_QUAD(row, column) \
    (((POLY_FT4 *)D_800B2078.effect_buffers[2 + D_800ADB08])[(row) * 20 + (column)])
/* The buffer's strip copy commands. */
#define EFFECT_MOVES ((DR_MOVE *)D_800B2078.effect_buffers[D_800ADB08])

/* Draw the screen distortion: step the six effect values (amplitude,
 * frequency and phase speed in x and y; once released, clear them when the
 * steps run out and stop), then bend the grid along two sine waves: rows
 * 14-16 hold the saved strips, rows 0-13 the screen, and row 13's lower
 * edge takes the next wave. The quad corners are addressed through the grid
 * index throughout; GCC substitutes the tested row/column constants
 * (column 0/19, row 13/16) into it. The step is shared by both branches
 * (one block that the released case jumps into). */
void func_800A4DAC(void) {
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

    if (D_800B2078.unk2078 == 0) {
        return;
    }
    if (D_800B2078.unk207A == 0) {
        if (D_800B2078.effect_steps > 0) {
        step:
            D_800B2078.effect_value[0] += D_800B2078.effect_step[0];
            D_800B2078.effect_value[1] += D_800B2078.effect_step[1];
            D_800B2078.effect_value[2] += D_800B2078.effect_step[2];
            D_800B2078.effect_steps--;
            D_800B2078.effect_value[3] += D_800B2078.effect_step[3];
            D_800B2078.effect_value[4] += D_800B2078.effect_step[4];
            D_800B2078.effect_value[5] += D_800B2078.effect_step[5];
        }
    } else {
        if (D_800B2078.effect_steps > 0) {
            goto step;
        }
        D_800B2078.effect_value[5] = 0;
        D_800B2078.effect_value[4] = 0;
        D_800B2078.effect_value[3] = 0;
        D_800B2078.effect_value[2] = 0;
        D_800B2078.effect_value[1] = 0;
        D_800B2078.effect_value[0] = 0;
        D_800B2078.unk2078 = 0;
    }
    phase = EFFECT_PHASE;
    amplitude_x = WHOLE(D_800B2078.effect_value[0]);
    amplitude_y = WHOLE(D_800B2078.effect_value[1]);
    frequency_x = WHOLE(D_800B2078.effect_value[2]);
    frequency_y = WHOLE(D_800B2078.effect_value[3]);
    speed_x = WHOLE(D_800B2078.effect_value[4]);
    speed_y = WHOLE(D_800B2078.effect_value[5]);
    phase[0] += speed_x;
    phase[1] += speed_y;

    /* Rows 14-16 (the saved strips) continue the wave below the screen. */
    phase_y = EFFECT_PHASE[1] + frequency_y * 11;
    for (row = 14; row < 17; row++) {
        wave_y = (func_8003F8CC(phase_y) * amplitude_y) >> 12;
        phase_x = EFFECT_PHASE[0];
        phase_y += frequency_y;
        for (column = 0; column < 20; column++) {
            wave_x = (func_8003F8CC(phase_x) * amplitude_x) >> 12;
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
            addPrim(&D_800C426C->ot[1], poly);
        }
    }
    addPrim(&D_800C426C->ot[1], &EFFECT_MOVES[0]);

    /* Rows 0-13: the screen itself. */
    phase_y = EFFECT_PHASE[1];
    for (row = 0; row < 14; row++) {
        wave_y = (func_8003F8CC(phase_y) * amplitude_y) >> 12;
        phase_x = EFFECT_PHASE[0];
        phase_y += frequency_y;
        for (column = 0; column < 20; column++) {
            wave_x = (func_8003F8CC(phase_x) * amplitude_x) >> 12;
            phase_x += frequency_x;
            poly = &EFFECT_QUAD(row, column);
            if (row != 0) {
                if (row == 13) {
                    EFFECT_QUAD(row - 1, column).y2 = EFFECT_QUAD(row - 1, column).y3 =
                        EFFECT_QUAD(row, column).y0 = EFFECT_QUAD(row, column).y1 =
                            row * 16 + wave_y + 0x20;
                    wave_y = (func_8003F8CC(phase_y) * amplitude_y) >> 12;
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
            addPrim(&D_800C426C->ot[1], poly);
        }
    }
    for (row = 0; row < 15; row++) {
        addPrim(&D_800C426C->ot[1], &EFFECT_MOVES[row + 1]);
    }
    addPrim(&D_800C426C->ot[1], &D_800B1E18[D_800ADB08]);
}

/* Store three words at +14 of `object`. */
void func_800A55B8(s32 *object, s32 a, s32 b, s32 c) {
    object[5] = a;
    object[6] = b;
    object[7] = c;
}

extern void *D_800AFE80;
extern void *D_800B069C;

/* Release the two blocks at 800afe80 and 800b069c. */
void func_800A55C8(void) {
    func_800320E8(D_800AFE80);
    func_800320E8(D_800B069C);
}

/* Set the five screen pieces of the next draw buffer to grey `shade`. */
void func_800A5600(s32 shade) {
    s32 i;

    for (i = 0; i < 5; i++) {
        (D_800B11AC.quads[i] + ((D_800ADB08 + 1) & 1))->r0 = shade;
        (D_800B11AC.quads[i] + ((D_800ADB08 + 1) & 1))->g0 = shade;
        (D_800B11AC.quads[i] + ((D_800ADB08 + 1) & 1))->b0 = shade;
    }
}

/* Fade screen channel 0 in from white over `frames` frames. */
void func_800A56A8(s32 frames) {
    s32 step;

    step = -0x10000 / frames;
    D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0xFF00;
    D_800B2078.fades[0].steps = frames + 1;
    D_800B2078.fades[0].abr = D_800B2078.fades[0].active = 1;
    D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = step;
}

/* Fade screen channel 0 up from black over `frames` frames. */
void func_800A5710(s32 frames) {
    s32 step;

    step = 0x10000 / frames;
    D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0;
    D_800B2078.fades[0].steps = frames + 1;
    D_800B2078.fades[0].abr = D_800B2078.fades[0].active = 1;
    D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = step;
}

/* Set the mask bit of every pixel of the 64-pixel-wide VRAM column at
 * (x, y), `h` rows tall. */
void func_800A5774(s32 x, s32 y, s32 h) {
    RECT rect;
    u32 *pixels;
    u32 *p;
    s32 i;

    rect.x = x;
    rect.y = y;
    rect.w = 0x40;
    rect.h = h;
    pixels = func_80031BDC(h << 7, 1);
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
    func_800320E8(pixels);
}

void func_800A663C(s32 semitrans, s32 abr);
void func_800A6408(void);
void func_800A6924(void);

/* Set up the screen pieces, draw them twice, mask the saved screen columns at
 * (2c0..3c0, 100) and draw twice more. */
void func_800A5884(s32 semitrans, s32 abr) {
    s32 i;
    s32 x;

    func_800A663C(semitrans, abr);
    for (i = 0; i < 2; i++) {
        func_80073FE0();
        func_800A6408();
        func_800A6924();
    }
    x = 0x2C0;
    for (i = 0; i < 5; i++) {
        func_800A5774(x, 0x100, 0xE0);
        x += 0x40;
    }
    for (i = 0; i < 2; i++) {
        func_80073FE0();
        func_800A6408();
        func_800A6924();
    }
}

#include "field_screen.h"

/* Run the requested screen transition (800adb38) over 800adb3c frames:
 * 1-2 fade the screen pieces out, 3 fades them in and holds while it
 * stays requested, 4 dissolves the screen grid from the centre. */
void func_800A5924(void) {
    s32 i;
    s32 x;
    s32 level;

    if (D_800ADB38 != 0) {
        func_8003748C();
        func_80070C84();
        func_800A915C();
        if (D_800ADB38 == 1 || D_800ADB38 == 4) {
            func_800A4748();
            DrawSync(0);
            func_80073FE0();
            func_800775F8();
        }
        func_800775F8();
    dispatch:
        switch (D_800ADB38) {
        case 4:
            func_800A5884(1, 1);
            func_800A6E70();
            D_800ADC08 = 1;
            func_80071E58(D_800ADB3C);
            D_800C3A40 = 0;
            for (i = 0; i < D_800ADB3C; i++) {
                func_80077DAC();
                func_800A6C40();
                func_8007554C();
                func_80078B5C();
                D_800C3A40 += 6;
            }
            DrawSync(0);
            func_800A7064();
            break;
        case 1:
        case 2:
            func_800A5884(1, 1);
            D_800ADC08 = 1;
            func_80071E58(D_800ADB3C);
            level = 0x800000;
            for (i = 0; i < D_800ADB3C; i++) {
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
                func_800A5600(level >> 16);
                level -= 0x800000 / D_800ADB3C;
                if (level < 0) {
                    level = 0;
                }
            }
            break;
        case 3:
            func_800A663C(1, 1);
            for (i = 0, x = 0x2C0; i < 5; i++, x += 0x40) {
                func_800A5774(x, 0x100, 0xE0);
            }
            level = 0;
            func_80071DCC(D_800ADB3C);
            func_800A5600(0);
            for (i = 0; i < D_800ADB3C; i++) {
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
                func_800A5600(level >> 16);
                level += 0x800000 / D_800ADB3C;
            }
            for (;;) {
                if (D_800ADB38 != 3) {
                    goto dispatch;
                }
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
            }
        }
        D_800ADB38 = 0;
        func_800A91F0();
        func_80077544();
    }
}

/* Reload the field for a map change inside the field mode (800b0048 the
 * transition kind, 800afd14 its frames): stop effects, keep the map
 * read-ahead block across the heap reset, then per kind fade or dissolve
 * out, reload the components (80070cc8), restart the music and fade in. */
void func_800A5C40(void) {
    RECT rect;
    u8 *ahead;
    s32 kind;
    s32 frames;
    s32 level;
    s32 i;

    func_8003748C();
    func_800A9460();
    func_800864F0();
    func_8007FFE8();
    if (D_800B0048 != 6) {
        func_800A915C();
        if (D_800B0048 != 4) {
            func_800A4748();
        }
    }
    DrawSync(0);
    func_80073FE0();
    func_800775F8();
    func_800700B0();
    ahead = func_80031BDC(D_8005A4C0, 0);
    memcpy(ahead, D_8005A4E0, D_8005A4C0);
    func_800320B8(D_8005A4E0);
    func_800320E8(D_8005A4E0);
    if (D_800B0048 != 6) {
        func_800A90B4(1);
    }
    D_8005A4E0 = func_80031BDC(D_8005A4C0, 1);
    memcpy(D_8005A4E0, ahead, D_8005A4C0);
    func_800320A4(D_8005A4E0);
    func_800320E8(ahead);
    switch (D_800B0048) {
    case 6:
        func_80071DCC(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80073FE0();
            func_80071CB4(&D_800C426C->overlay_ot[0], D_800ADB08);
            func_800A6924();
        }
        goto reload;
    case 0:
        func_800A663C(0, 0);
        func_80071DCC(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80073FE0();
            func_80071CB4(&D_800C426C->overlay_ot[0], D_800ADB08);
            func_800A6408();
            func_800A6924();
        }
    reload:
        func_80073FE0();
        func_800A6924();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        func_80070CC8();
        func_80070488();
        func_80070508();
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        func_80071E58(D_800AFD14);
        break;
    case 1:
        func_800A663C(0, 0);
        func_800A5710(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80073FE0();
            func_80071CB4(&D_800C426C->overlay_ot[0], D_800ADB08);
            func_800A6408();
            func_800A6924();
        }
        func_800775F8();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        func_80070CC8();
        func_80070488();
        func_80070508();
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        func_800A56A8(D_800AFD14);
        break;
    case 2:
    case 4:
        func_800A5884(1, 1);
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        func_80070CC8();
        func_80070488();
        if (D_800ADB60 == 1) {
            while (func_800286CC() != 0) {
                func_80073FE0();
                func_800A6408();
                func_800A6924();
                if (D_800C2684 < 0x22C0) {
                    D_800C2684 += 0x20;
                }
            }
            func_800320E8(D_800ADC14);
            D_800ADB60 = 0;
            func_80078C5C();
        }
        D_800AFD04 = 1;
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        level = 0x800000;
        func_80071E58(D_800AFD14);
        for (i = 0; i < D_800AFD14; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
            func_800A5600(level >> 16);
            level -= 0x800000 / D_800AFD14;
            if (level < 0) {
                level = 0;
            }
            if (D_800C2684 < 0x22C0) {
                D_800C2684 += 0x20;
            }
        }
        break;
    case 3:
        func_800A663C(0, 0);
        func_80070488();
        func_80073FE0();
        func_800A6408();
        func_800A6924();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        D_800AFD04 = 1;
        func_80070CC8();
        func_80070508();
        D_800B0048 = kind;
        D_800AFD14 = frames;
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        for (i = 0; i < 4; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
        }
        break;
    case 5:
        func_800A663C(0, 0);
        func_80070488();
        func_80073FE0();
        func_800A6408();
        func_800A6924();
        func_8001B044();
        func_8001B3A8();
        kind = D_800B0048;
        frames = D_800AFD14;
        D_800AFD04 = 1;
        func_80070CC8();
        func_80070508();
        setRECT(&rect, 0x2C0, 0x100, 0x140, 0xFF);
        D_800B0048 = kind;
        D_800AFD14 = frames;
        MoveImage(&rect, 0x140, 0xFF);
        if (D_8004F308 == -1) {
            func_80085B20(D_8004F324, 0);
        }
        for (i = 0; i < 4; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
        }
        break;
    }
    if (D_800B0048 != 6) {
        func_800A91F0();
    }
    D_800B0048 = 2;
    D_800AFD14 = 0x20;
    D_800AFD04 = 0;
    func_80077544();
    func_80031FF8();
}

/* Rotate and scale the screen pieces (when scaled) into their quads and
 * link the quads and draw modes of the current buffer. */
void func_800A6408(void) {
    MATRIX m;
    VECTOR scale;
    s32 p;
    s32 flag;
    s32 i;

    func_8003F738(&D_800B00B8, &m);
    m.t[2] = 0;
    m.t[1] = 0;
    m.t[0] = 0;
    scale.vx = D_800C2684;
    scale.vy = D_800C2684;
    scale.vz = D_800C2684;
    ScaleMatrix(&m, &scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0; i < 5; i++) {
        if (D_800C2684 != 0x1000) {
            RotAverage4(&D_800B11AC.corners[i][0], &D_800B11AC.corners[i][1], &D_800B11AC.corners[i][2],
                        &D_800B11AC.corners[i][3], (s32 *)&D_800B11AC.quads[i][D_800ADB08].x0,
                        (s32 *)&D_800B11AC.quads[i][D_800ADB08].x1, (s32 *)&D_800B11AC.quads[i][D_800ADB08].x2,
                        (s32 *)&D_800B11AC.quads[i][D_800ADB08].x3, &p, &flag);
        }
        addPrim(&D_800C426C->overlay_ot[0], &D_800B11AC.quads[i][D_800ADB08]);
        addPrim(&D_800C426C->overlay_ot[0], &D_800B11AC.modes[i][D_800ADB08]);
    }
}

/* Set up the five 64x224 screen pieces (textured from the screen copy at
 * (2c0, 100)) at full scale with no rotation, with semi-transparency
 * `semitrans` and rate `abr`. */
void func_800A663C(s32 semitrans, s32 abr) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    SVECTOR *corners;
    s32 i;

    D_800C2684 = 0x1000;
    D_800B00B8.vx = 0;
    D_800B00B8.vy = 0;
    D_800B00B8.vz = 0;
    for (i = 0; i < 5; i++) {
        quad = &D_800B11AC.quads[i][0];
        copy = &D_800B11AC.quads[i][1];
        corners = D_800B11AC.corners[i];
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
        setRECT(&D_800B11AC.windows[i][0], 0, 0, 0xFF, 0xFF);
        setRECT(&D_800B11AC.windows[i][1], 0, 0, 0xFF, 0xFF);
        SetDrawMode(&D_800B11AC.modes[i][0], 0, 0, GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100),
                    &D_800B11AC.windows[i][0]);
        SetDrawMode(&D_800B11AC.modes[i][1], 0, 0, GetTPage(2, abr, 0x2C0 + i * 0x40, 0x100),
                    &D_800B11AC.windows[i][1]);
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

/* Present the current draw block: clear, set environments and draw its
 * overlay ordering table. */
void func_800A6924(void) {
    DrawSync(0);
    VSync(2);
    ClearImage(&D_800C426C->draw.clip, 0, 0, 0);
    PutDrawEnv(&D_800C426C->draw);
    PutDispEnv(&D_800C426C->disp);
    DrawOTag(&D_800C426C->overlay_ot[7]);
}

/* Darken corner `corner` of a grid quad by 6 (to 0) while it lies within
 * `radius` of the screen centre (a0, 70). */
void func_800A6998(POLY_GT4 *quad, s32 corner, s32 radius, s16 *shade) {
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
        func_8004A414(&offset, &squares);
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
        func_8004A414(&offset, &squares);
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
        func_8004A414(&offset, &squares);
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
        func_8004A414(&offset, &squares);
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

/* Fade the grid quads of the current buffer and link them, then the grid
 * draw mode, into the overlay ordering table. */
void func_800A6C40(void) {
    POLY_GT4 *quad;
    s32 row;
    s32 column;

    for (row = 0; row < GRID_ROWS; row++) {
        for (column = 0; column < GRID_COLUMNS; column++) {
            quad = &D_800B00C4->quads[D_800ADB08][row * GRID_COLUMNS + column];
            func_800A6998(quad, 0, D_800C3A40, &D_800B00C4->shade[0][row * GRID_COLUMNS + column]);
            func_800A6998(quad, 1, D_800C3A40, &D_800B00C4->shade[1][row * GRID_COLUMNS + column]);
            func_800A6998(quad, 2, D_800C3A40, &D_800B00C4->shade[2][row * GRID_COLUMNS + column]);
            func_800A6998(quad, 3, D_800C3A40, &D_800B00C4->shade[3][row * GRID_COLUMNS + column]);
            addPrim(&D_800C426C->overlay_ot[0], &D_800B00C4->quads[D_800ADB08][row * GRID_COLUMNS + column]);
        }
    }
    addPrim(&D_800C426C->overlay_ot[0], &D_800B1E24[D_800ADB08]);
}

/* Build the screen grid: 16x16 half-bright quads textured from the
 * 15-bit screen copy at (2c0, 100), their corners at full shade. */
void func_800A6E70(void) {
    POLY_GT4 *quad;
    POLY_GT4 *copy;
    s32 row;
    s32 column;
    s32 k;

    D_800B00C4 = func_80031BDC(sizeof(ScreenGrid), 1);
    for (row = 0; row < GRID_ROWS; row++) {
        for (column = 0; column < GRID_COLUMNS; column++) {
            k = row * GRID_COLUMNS + column;
            quad = &D_800B00C4->quads[0][k];
            copy = &D_800B00C4->quads[1][k];
            D_800B00C4->shade[0][k] = 0x80;
            D_800B00C4->shade[1][k] = 0x80;
            D_800B00C4->shade[2][k] = 0x80;
            D_800B00C4->shade[3][k] = 0x80;
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

/* Release the screen grid. */
void func_800A7064(void) {
    func_800320E8(D_800B00C4);
}

extern s32 D_801D68B4;
extern void func_801D3538(s32 w, s32 h, s32, s32, s32, s32, s32 rgb24);

/* Set up the movie player for a 320x224 picture. */
void func_800A708C(void) {
    func_80032498(4, 0);
    if (D_800ADB74 == 2) {
        D_801D68B4 = 1;
    } else {
        D_801D68B4 = 0;
    }
    func_801D3538(0x140, 0xE0, 0x80, 0x10, 0x20, 0x800, D_800C3A36);
    D_800ADB6C = 0;
    func_80032498(8, 0);
}

/* Movie frame callback: record the frame, select the draw block for the
 * decoded buffer (24-bit display when set). */
void func_800A7120(u16 frame, s32 unused, u16 buffer) {
    D_800B06A0 = frame;
    D_800B00E4 = 0;
    if (buffer == 0) {
        D_800ADB78 = 1;
    } else {
        D_800ADB78 = 0;
    }
    if (D_800ADB74 == 0 && D_800AFE74 == 0) {
        DrawSync(0);
        D_800C426C = &D_800B249C[D_800ADB78];
        if ((s16)D_800C3A36 == 1) {
            D_800B249C[D_800ADB78 & 1].disp.isrgb24 = 1;
        }
    }
}

extern void func_801D37CC(s32 file, s32, s32, s32, s32, s32 mode, s32, s32, s32, s32, s32, s32 h,
                          void (*callback)(u16, s32, u16));

/* Start the field movie with the current parameters. */
void func_800A7218(void) {
    s32 mode;

    D_800B06A0 = 0;
    func_80032498(4, 0);
    if (D_800ADB6C == 0) {
        func_80028470(0x18, 1);
        mode = 1;
        if (D_800C3A38 != 0xFF || (D_800ADB80 & 0x40)) {
            mode = 3;
        }
        func_801D37CC(FIELD_MOVIE.file + 2, D_800C3A2A, D_800C3A2C, D_800C3A2E, 1, mode, D_800C3A3A, D_800C3A22,
                      D_800C3A24, D_800C3A26, D_800C3A28, 0xE0, func_800A7120);
        func_80028470(4, 0);
    }
    func_80032498(8, 0);
}

extern void func_80019CA0(void);
extern void func_801D3F7C(void);
void func_80085678(void);

/* Run `frames` movie frames (with field sound) unless the movie stopped. */
void func_800A732C(s32 frames) {
    s32 i;

    func_80019CA0();
    if (D_800ADB6C == 0) {
        for (i = 0; i < frames; i++) {
            func_801D3F7C();
            func_80085678();
        }
    }
}

/* Keep the field running until the stream is idle and draw buffer 0 is
 * current, then wait for CD data. */
void func_800A7394(void) {
    do {
        do {
            func_80077DAC();
            func_8007554C();
        } while (func_800286CC() != 0);
    } while (D_800ADB08 != 0);
    CdDataSync(0);
}

#include "field_movie.h"

/* After a movie: reload the VRAM kept in party sprite blocks 1 and 2
 * (unless movie mode 2), then release both blocks. */
void func_800A73E8(void) {
    RECT rect;

    if (D_800ADB74 == 2) {
        func_800320B8(D_8005A414[1]);
        func_800320B8(D_8005A41C);
        func_800320E8(D_8005A414[1]);
        func_800320E8(D_8005A41C);
    } else {
        setRECT(&rect, 0x200, 0, 0x140, 0x80);
        LoadImage(&rect, D_8005A414[1]);
        DrawSync(0);
        setRECT(&rect, 0x200, 0x80, 0x140, 0x80);
        LoadImage(&rect, D_8005A41C);
        DrawSync(0);
        func_800320B8(D_8005A414[1]);
        func_800320B8(D_8005A41C);
        func_800320E8(D_8005A414[1]);
        func_800320E8(D_8005A41C);
    }
}

/* Before a movie: allocate party sprite blocks 1 and 2 and save the VRAM
 * at (200, 0) in them; in movie mode 2 instead reload the sprites of party
 * slots 1 and 2 from their character files. */
void func_800A74F8(void) {
    RECT rect;
    MovieFileRequest requests[4];
    s32 count;
    s32 i;

    if (D_800ADB74 == 2) {
        D_8005A414[1] = func_80031BDC(0x14000, 0);
        D_8005A414[2] = func_80031BDC(0x14000, 0);
        func_800320A4(D_8005A414[1]);
        func_800320A4(D_8005A414[2]);
        func_80028470(4, 0);
        count = 0;
        for (i = 1; i < 3; i++) {
            if (D_8006FABC[i] != 0xFF) {
                requests[count].file = D_8006FABC[i] + 5;
                requests[count].destination = D_80065AFC[i] = func_80031BDC(func_800288EC(D_8006FABC[i] + 5), 1);
                count++;
            }
        }
        requests[count].destination = NULL;
        requests[count].file = 0;
        func_80029AFC(requests, 0, 0);
        func_80028A60(0);
        for (i = 1; i < 3; i++) {
            if (D_80062590[i] != 0xFF) {
                func_80032EB4(D_80065AFC[i], D_8005A414[i]);
                func_800320E8(D_80065AFC[i]);
            }
        }
        return;
    }
    D_8005A414[1] = func_80031BDC(0x14000, 0);
    D_8005A414[2] = func_80031BDC(0x14000, 0);
    func_800320A4(D_8005A414[1]);
    func_800320A4(D_8005A414[2]);
    setRECT(&rect, 0x200, 0, 0x140, 0x80);
    StoreImage(&rect, D_8005A414[1]);
    DrawSync(0);
    setRECT(&rect, 0x200, 0x80, 0x140, 0x80);
    StoreImage(&rect, D_8005A414[2]);
    DrawSync(0);
}

extern s32 D_800B14A8; /* nibble counter */
extern u32 *D_800C3904; /* packed stream */
extern u32 D_800C2688; /* current word */

/* Next byte of the packed stream (four per word), scaled down by 8 but
 * at least 1 when nonzero. */
u32 func_800A7744(void) {
    u32 value;

    if ((D_800B14A8 & 3) == 0) {
        D_800C2688 = *D_800C3904;
        D_800C3904++;
    }
    D_800B14A8++;
    value = D_800C2688 & 0xFF;
    D_800C2688 >>= 8;
    if (value != 0) {
        value >>= 3;
        if (value == 0) {
            value = 1;
        }
    }
    return value;
}

extern u32 *D_800C390C; /* converted pixels */

/* Convert the 24-bit screen (five 96-pixel columns at x 0) to 15-bit
 * pixels at (0..0x140, 100). Both callers pass 0, which it ignores. */
void func_800A77C4(s32 unused) {
    RECT rect;
    u32 *packed;
    u32 *pixels;
    u32 value;
    s32 i;
    s32 j;

    packed = func_80031BDC(0xA800, 0);
    pixels = func_80031BDC(0x7000, 0);
    for (i = 0; i < 5; i++) {
        rect.x = i * 0x60;
        rect.y = 0;
        rect.w = 0x60;
        rect.h = 0xE0;
        StoreImage(&rect, packed);
        DrawSync(0);
        D_800C3904 = packed;
        D_800C390C = pixels;
        D_800B14A8 = 0;
        for (j = 0; j < 0x1C00; j++) {
            value = func_800A7744();
            value |= func_800A7744() << 5;
            value |= func_800A7744() << 10;
            value |= func_800A7744() << 16;
            value |= func_800A7744() << 21;
            value |= func_800A7744() << 26;
            *D_800C390C = value;
            D_800C390C++;
        }
        rect.x = i << 6;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0xE0;
        LoadImage(&rect, pixels);
        DrawSync(0);
    }
    func_800320E8(packed);
    func_800320E8(pixels);
}

/* While movie frames 687..18e2 play (when the sequence is enabled),
 * switch to 640-wide draw buffers and draw the file 0xab sequence over
 * the movie each frame, then restore the 320-wide buffers. Declared int
 * without a value, as the original's live result register shows. */
s32 func_800A7948(void) {
    RECT rect;

    if (D_8004F300 == 0 || D_800B06A0 < 0x687) {
        return;
    }
    if (D_800B06A0 < 0x18E2) {
        D_800AFE74 = 1;
        D_801E89E0 = 0;
        setRECT(&rect, 0, 0, 0x500, 0x200);
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x280, 0xE0);
        SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x280, 0xE0);
        D_800B249C[1].disp.isrgb24 = 0;
        D_800B249C[0].disp.isrgb24 = 0;
        PutDispEnv(&D_800C426C->disp);
        PutDrawEnv(&D_800C426C->draw);
        setRECT(&rect, 0x300, 0, 0x200, 0x100);
        ClearImage(&rect, 0, 0, 0);
        func_800ACB90();
        VSync(0);
        DrawSync(0);
        while (D_800B06A0 < 0x18E2) {
            func_80019CA0();
            func_80073F50();
            if (D_800B06A0 < 0x18DE) {
                func_800AC99C();
            }
            DrawSync(0);
            VSync(2);
            ClearImage(&D_800C426C->draw.clip, 0, 0, 0);
            PutDispEnv(&D_800C426C->disp);
            PutDrawEnv(&D_800C426C->draw);
            DrawOTag(&D_800C426C->overlay_ot[7]);
            func_800ACCF4();
            func_800A732C(5);
        }
        D_800AFE74 = 0;
        D_801E89E0 = 1;
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x140, 0xE0);
        SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x140, 0xE0);
        SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x140, 0xE0);
        SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x140, 0xE0);
        D_800B249C[1].disp.isrgb24 = 1;
        D_800B249C[0].disp.isrgb24 = 1;
        D_800B249C[1].disp.isinter = 0;
        D_800B249C[0].disp.isinter = 0;
        func_800ACCB0();
    }
}

/* Play the requested field movie (800c3a20..): move the movie library
 * (file 0xa9) into place, park the VRAM the movie uses, stop the field's
 * effects and the 801e module, then decode and present frames by movie
 * mode until it ends or is skipped; finally restore VRAM, the display and
 * the field stream. */
void func_800A7C58(void) {
    RECT rect;
    u8 *data;
    u8 *library;
    s32 top;
    s32 i;

    D_800ADB84 = 0;
    D_800B00E4 = 0;
    D_800ADB78 = 0;
    data = func_80031BDC(func_800288EC(0xA9), 0);
    func_800295D8(0xA9, data, 0, 0x80);
    D_800B06A0 = 0;
    D_800AFE74 = 0;
    func_800A7394();
    func_80028470(0x18, 0);
    func_8002A2D0(FIELD_MOVIE.file);
    func_80028470(4, 0);
    func_800A7394();
    if (D_800B2078.unk2264 != 0) {
        func_801E7FD4();
        func_8007999C();
        func_800775F8();
        func_800320E8(D_800ADB20);
    }
    func_800A9460();
    if (D_800ADB74 != 2) {
        setRECT(&rect, 0x140, 0, 0xC0, 0x100);
        LoadImage(&rect, (u_long *)data);
        DrawSync(0);
        func_800320E8(data);
        data = func_80031BDC(0x18000, 0);
        StoreImage(&rect, (u_long *)data);
    }
    top = D_800ADB30;
    if (D_8004F370 == 0) {
        library = func_80031BDC((top & 0xFFFFFF) - 0x1D3008, 1);
        memcpy(library, data, func_800288EC(0xA9));
    } else {
        library = func_80031BDC(8, 1);
    }
    func_800320E8(data);
    func_80085788();
    func_800ACC58();
    D_800B00E4 = 1;
    func_800A73E8();
    func_8007999C();
    func_80031FF8();
    func_800A708C();
    func_800A7218();
    D_800ADB7C = 1;
    do {
        VSync(0);
        func_800A732C(3);
    } while (D_800B00E4 != 0);
    do {
        switch (D_800ADB74) {
        case 1:
            func_80073F50();
            func_80075910();
            func_800A732C(6);
            break;
        case 0:
            DrawSync(0);
            VSync(0);
            PutDispEnv(&D_800C426C->disp);
            PutDrawEnv(&D_800C426C->draw);
            func_800A732C(3);
            DrawSync(0);
            VSync(0);
            PutDispEnv(&D_800C426C->disp);
            PutDrawEnv(&D_800C426C->draw);
            func_800A732C(3);
            func_800A7948();
            break;
        case 2:
            func_80077DAC();
            func_8007554C();
            func_800A732C(9);
            break;
        }
        if (D_800C268C == 0) {
            if (D_800ADB74 == 2) {
                if ((D_800C3900 & 0x80) || D_800ADB84 != 0) {
                    break;
                }
            } else {
                func_80074700();
                if (D_800C3900 & 0x20) {
                    break;
                }
            }
        } else if (D_800ADB80 & 0x80) {
            func_80074700();
            if (D_800C3900 & 0x20) {
                func_80038D18(0, 10);
                for (i = 0; i < 5; i++) {
                    VSync(0);
                }
                break;
            }
        }
        if (D_800ADB74 == 2 && D_800ADB84 != 0) {
            break;
        }
    } while ((s16)FIELD_MOVIE.unk3A != 0 || D_800B06A0 < FIELD_MOVIE.unk2E);
    VSync(0);
    DrawSync(0);
    func_801D43B0();
    func_8007999C();
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    VSync(0);
    DrawSync(0);
    func_800320E8(library);
    func_80031FF8();
    func_800A74F8();
    func_80077884();
    setRECT(&rect, 0, D_800ADB78 << 8, 0x1E0, 0xE0);
    MoveImage(&rect, 0, D_800ADB78 << 8);
    DrawSync(0);
    VSync(0);
    D_800C426C = &D_800B249C[1];
    PutDispEnv(&D_800B249C[1].disp);
    PutDrawEnv(&D_800C426C->draw);
    if (D_800ADB74 != 2) {
        func_800A77C4(0);
    }
    VSync(0);
    D_800B249C[0].disp.isrgb24 = 0;
    D_800C426C = &D_800B249C[0];
    PutDispEnv(&D_800B249C[0].disp);
    PutDrawEnv(&D_800C426C->draw);
    setRECT(&rect, 0, 0x100, 0x140, 0xE0);
    MoveImage(&rect, 0, 0);
    DrawSync(0);
    VSync(0);
    D_800B249C[1].disp.isrgb24 = 0;
    D_800C426C = &D_800B249C[1];
    PutDispEnv(&D_800B249C[1].disp);
    PutDrawEnv(&D_800C426C->draw);
    if (D_800AFE84 != 0) {
        D_800ADB50 = 1;
    } else {
        D_800ADB50 = 0;
    }
    func_80077AB4();
    if (D_800ADB74 != 2) {
        func_80028470(4, 0);
        func_80032498(8, 0);
        D_800ADB60 = 0;
        func_80070488();
        func_80070508();
        D_800ADB3C = 0x20;
        D_800ADB38 = 1;
    }
    func_80085738();
    D_800C3A38 = 0xFF;
    D_800ADB74 = 0;
    D_800ADB6C = -1;
}

void func_80070340(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

/* Load file 0xaa and upload its image to (380, 0) with its CLUT at
 * (0, e8). */
void func_800A8314(void) {
    u32 *data;

    func_80032498(8, 0);
    func_80028470(4, 0);
    data = func_80031BDC(func_800288EC(0xAA), 1);
    func_800295D8(0xAA, data, 0, 0x80);
    func_80028A60(0);
    func_80070340(data, 0x380, 0, 0, 0xE8, 0, 0);
    DrawSync(0);
    func_800320E8(data);
}

extern POLY_FT4 *D_800AFC60[2];

/* Release the two primitive buffers once allocated. */
void func_800A83B4(void) {
    if (D_800AF278 != 0) {
        D_800AF278 = 0;
        DrawSync(0);
        func_800320E8(D_800AFC60[0]);
        func_800320E8(D_800AFC60[1]);
    }
}

/* Texture panel piece `index` of the current buffer with its frame moved
 * by (du, dv), flipped vertically. The original passes the coordinates to
 * 8007a44c unconverted (no s16 prototype in scope: a separate unit). */
void func_800A8408(s32 index, s32 du, s32 dv) {
    s32 frame;
    s32 u;
    s32 v;
    s32 w;
    s32 h;
    s32 unused[6]; /* the original frame reserves an unused 24-byte local */

    frame = D_800AEF10[index].frame;
    v = D_800AEB68[frame].v + dv;
    w = D_800AEB68[frame].w;
    u = D_800AEB68[frame].u + du;
    h = D_800AEB68[frame].h;
    func_8007A44C(&D_800AFC60[D_800ADB08][index], u, v + h - 1, u + w, v + h - 1, u, v - 1, u + w, v - 1);
}

/* Draw the status panel: rotate the compass strip by the view angle, the
 * pitch strip by the camera pitch, their digits, the blinking marker and
 * the rest of the pieces. */
void func_800A84C0(void) {
    s32 pitch;
    s32 angle;
    s32 count;
    s32 i;

    if (D_800AF278 != 0) {
        count = PANEL_PIECES;
        pitch = ratan2(func_80099A4C((D_800AF880.target.vx - D_800AF880.eye.vx) >> 16,
                                     (D_800AF880.target.vz - D_800AF880.eye.vz) >> 16),
                       (D_800AF880.target.vy - D_800AF880.eye.vy) >> 16) &
                0xFFF;
        angle = D_800AF880.view_angle & 0xFFF;
        for (i = 0; i < 16; i++) {
            func_800A8408(i, (angle >> 4) & 0xF, 0);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 29; i++) {
            func_800A8408(i, 0, (pitch >> 4) & 0xF);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 34; i++) {
            func_800A8408(i, (angle & 0xF) * 8, 0);
            angle >>= 2;
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 39; i++) {
            func_800A8408(i, (pitch & 0xF) * 8, 0);
            pitch >>= 2;
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 40; i++) {
            if (!(D_800AEB60 & 0xF)) {
                D_800AEB64++;
            }
            if (D_800AEB64 >= 3) {
                D_800AEB64 = 0;
            }
            func_800A8408(i, 0, D_800AEB64 * 8);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 42; i++) {
            func_800A8408(i, (D_800AEB60 >> 2) & 0xF, 0);
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        for (; i < 43; i++) {
            if (!(D_800AEB60 & 0x10)) {
                addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
            }
        }
        for (; i < count; i++) {
            addPrim(&D_800C426C->overlay_ot[4], &D_800AFC60[D_800ADB08][i]);
        }
        D_800AEB60++;
    }
}

#ifdef NON_MATCHING
/* Build the status panel: load its image, allocate the quads of both
 * buffers and texture each piece from its frame (flip in flags bits 0-3,
 * 4- or 8-bit page by bits 4-7). The original passes the coordinates to
 * 8007a44c unconverted (no s16 prototype in scope: a separate unit).
 * The selected layout record is consumed after the primitive setup calls;
 * its page-mode flags are read separately before those calls.
 * NON_MATCHING (48 edits): the original keeps layout + 6 and layout bases
 * and a separate layout index. This still reduces the layout addresses
 * and keeps the buffer-pointer table's address across the loop. */
/* The image frames are read one unsigned halfword field at a time. */
#define FRAME(n, field) (((u16 *)D_800AEB68)[(n) * 4 + (field)])
enum { FRAME_U, FRAME_V, FRAME_W, FRAME_H };
void func_800A8BA4(void) {
    POLY_FT4 *quad;
    POLY_FT4 *copy;
    s32 mode;
    s32 i;

    func_800A8314();
    mode = 0;
    D_800AEB64 = 0;
    func_80032498(8, 0);
    D_800AFC60[0] = func_80031BDC(PANEL_PIECES * sizeof(POLY_FT4), 0);
    D_800AFC60[1] = func_80031BDC(PANEL_PIECES * sizeof(POLY_FT4), 0);
    for (i = 0; i < PANEL_PIECES; i++) {
        quad = &D_800AFC60[0][i];
        copy = &D_800AFC60[1][i];
        SetPolyFT4(quad);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad->clut = GetClut(0, 0xE8);
        switch ((D_800AEF10[i].flags >> 4) & 0xF) {
        case 0:
            mode = 1;
            break;
        case 1:
            mode = 2;
            break;
        }
        quad->tpage = GetTPage(0, mode, 0x380, 0);
        SetSemiTrans(quad, 1);
        {
            PanelPiece *piece = &D_800AEF10[i];
            s32 x, y, frame, u, v, w, h;

            frame = piece->frame;
            x = piece->x;
            y = piece->y;
            w = FRAME(frame, FRAME_W);
            h = FRAME(frame, FRAME_H);
            u = FRAME(frame, FRAME_U);
            v = FRAME(frame, FRAME_V);
            quad->x0 = x;
            quad->y0 = y;
            quad->y1 = y;
            quad->x2 = x;
            quad->x1 = x + w;
            quad->y2 = y + h;
            quad->x3 = x + w;
            quad->y3 = y + h;
            switch (piece->flags & 0xF) {
            case 0:
                func_8007A44C(quad, u, v, u + w, v, u, v + h, u + w, v + h);
                break;
            case 1:
                func_8007A44C(quad, u + w - 1, v, u - 1, v, u + w - 1, v + h, u - 1, v + h);
                break;
            case 2:
                func_8007A44C(quad, u, v + h - 1, u + w, v + h - 1, u, v - 1, u + w, v - 1);
                break;
            case 3:
                func_8007A44C(quad, u + w - 1, v + h - 1, u - 1, v + h - 1, u + w - 1, v - 1, u - 1, v - 1);
                break;
            }
        }
        *copy = *quad;
    }
    D_800AF278 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800A4748", func_800A8BA4);
#endif


/* Set up a particle's quad in both buffers from sprite `sprite` with
 * semi-transparency rate `abr`.
 * The corners scale each term by 16 before subtracting/adding: x * 16
 * and y * 16 are computed once, but w * 16 and h * 16 twice (once for the
 * left/top edge, once for the right/bottom). Shifting in the differences
 * keeps GCC from folding x * 16 - w * 16 into (x - w) * 16, and the
 * (u16) view of the half sizes in the sums (a no-op on their values) keeps
 * CSE from sharing the second w * 16 / h * 16 with the first.
 * The second buffer's quad is taken as a pointer up front; the block copy
 * then starts from a copy of it (addiu v1,s0,0x78; move a2,v1). */
void func_800A8EAC(Particle *particle, s32 sprite, s32 abr) {
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
    half_w = D_800AF27C[sprite].half_w;
    half_h = D_800AF27C[sprite].half_h;
    x = D_800AF27C[sprite].x;
    y = D_800AF27C[sprite].y;
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
    func_8007A44C(quad, D_800AF27C[sprite].uv[0][0], D_800AF27C[sprite].uv[0][1] + 0x40,
                  D_800AF27C[sprite].uv[1][0] - 1, D_800AF27C[sprite].uv[1][1] + 0x40,
                  D_800AF27C[sprite].uv[2][0], D_800AF27C[sprite].uv[2][1] + 0x3F,
                  D_800AF27C[sprite].uv[3][0] - 1, D_800AF27C[sprite].uv[3][1] + 0x3F);
    SetSemiTrans(quad, 1);
    quad->tpage = GetTPage(0, abr, 0x3C0, 0x140);
    quad->clut = GetClut(0x100, 0xF7);
    *copy = *quad;
}

extern RECT D_800AFC28;
typedef struct {
    u32 words[0x2000];
} ScreenColumn; /* a 64x256 16-bit VRAM column */
extern ScreenColumn *D_800AFC70; /* saved screen column */

/* Move the saved screen column into a new block allocated with `flags`. */
void func_800A90B4(s32 flags) {
    ScreenColumn *copy;

    if (D_800ADB34 == 1) {
        func_80032498(8, 0);
        copy = func_80031BDC(0x8000, flags);
        *copy = *D_800AFC70;
        func_800320E8(D_800AFC70);
        D_800AFC70 = copy;
    }
}

/* Save the 64x256 VRAM column at (3c0, 100) once. */
void func_800A915C(void) {
    if (D_800ADB34 != 1) {
        D_800ADB34 = 1;
        func_80032498(8, 0);
        D_800AFC70 = func_80031BDC(0x8000, 1);
        D_800AFC28.x = 0x3C0;
        D_800AFC28.y = 0x100;
        D_800AFC28.w = 0x40;
        D_800AFC28.h = 0x100;
        StoreImage(&D_800AFC28, D_800AFC70->words);
        DrawSync(0);
    }
}

/* Restore the saved VRAM column and release it. */
void func_800A91F0(void) {
    if (D_800ADB34 != 0) {
        D_800AFC28.x = 0x3C0;
        D_800ADB34 = 0;
        D_800AFC28.y = 0x100;
        D_800AFC28.w = 0x40;
        D_800AFC28.h = 0x100;
        LoadImage(&D_800AFC28, D_800AFC70->words);
        DrawSync(0);
        func_800320E8(D_800AFC70);
    }
}
