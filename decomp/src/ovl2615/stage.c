/* Stage setup: the stage image list bounds, its images and actors, and
 * the stage lights. */
#include "battle_setup.h"

/* Relocate the stage image list and take the bounds of its pixel sections
 * (kind 0x1101: position, offset, size); returns the bounds' area. */
s32 func_801E70E8(s32 *images) {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
    s32 count;
    s32 i;
    u16 *p;
    s32 x;
    s32 y;
    s16 width;
    s16 height;

    func_8003342C(images);
    left = 0x800;
    top = 0x800;
    right = -0x800;
    bottom = -0x800;
    count = images[0];
    for (i = 0; i < count; i++) {
        p = (u16 *)images[i + 1];
        if (*p == 0x1101) {
            p += 2;
            x = *p++;
            y = *p++;
            x += *p++;
            y += *p++;
            if (x < left) {
                left = x;
            }
            if (y < top) {
                top = y;
            }
            x += p[0];
            y += p[1];
            if (right < x) {
                right = x;
            }
            if (bottom < y) {
                bottom = y;
            }
        }
    }
    width = right - left;
    height = bottom - top;
    D_800D2D30 = left;
    D_800D2D34 = top;
    D_800D2D2C = width;
    D_800C3EA8 = height;
    return width * height;
}

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/stage", func_801E7210);

/* Build the stage backdrop: its placement, the 9 x 9 floor grid spaced by
 * step, the tiles' texture page and palette, the fills and fades in the
 * given colours, and the draw modes. Returns NULL without memory. */
StageBackdrop *func_801E7914(s16 texX, s16 texY, s16 width, s16 height, s16 size, s16 step,
                             s16 v0A, s16 clutX, s16 clutY, s16 v10, s16 v12, VECTOR *position,
                             CVECTOR *colour, s16 v0C, s16 v0E) {
    DRAWENV env;
    RECT window;
    StageBackdrop *backdrop;
    s32 i;
    s32 j;
    s32 k;

    func_80032498(4, 0);
    backdrop = func_80031BDC(sizeof(StageBackdrop), 0);
    if (backdrop != NULL) {
        GetDrawEnv(&env);
        backdrop->x = 0;
        backdrop->y = 0;
        backdrop->width = width;
        backdrop->height = height;
        backdrop->v08 = size;
        backdrop->v0A = v0A;
        backdrop->v0C = v0C;
        backdrop->v0E = v0E;
        backdrop->v10 = v10;
        backdrop->v12 = v12;
        backdrop->position[0] = position->vx;
        backdrop->position[1] = position->vy;
        backdrop->position[2] = position->vz;
        for (i = 0; i < 2; i++) {
            backdrop->colours[i].r = colour[i].r;
            backdrop->colours[i].g = colour[i].g;
            backdrop->colours[i].b = colour[i].b;
        }
        k = 0;
        for (i = 0; i < 9; i++) {
            for (j = 0; j < 9; j++) {
                backdrop->grid[k].vx = j * step - step * 4;
                backdrop->grid[k].vy = 0;
                backdrop->grid[k].vz = i * step - step * 4;
                k++;
            }
        }
        for (i = 0; i < 2; i++) {
            SetPolyF4(&backdrop->fills[i]);
            backdrop->fills[i].r0 = colour->r;
            backdrop->fills[i].g0 = colour->g;
            backdrop->fills[i].b0 = colour->b;
            backdrop->fills[i].x0 = 0;
            backdrop->fills[i].y0 = 0;
            backdrop->fills[i].x1 = 320;
            backdrop->fills[i].y1 = 0;
            backdrop->fills[i].x2 = 0;
            backdrop->fills[i].x3 = 320;
        }
        for (i = 0; i < 2; i++) {
            SetPolyG4(&backdrop->fades[i]);
            SetSemiTrans(&backdrop->fades[i], 1);
            backdrop->fades[i].r2 = 0;
            backdrop->fades[i].g2 = 0;
            backdrop->fades[i].b2 = 0;
            backdrop->fades[i].r3 = 0;
            backdrop->fades[i].g3 = 0;
            backdrop->fades[i].b3 = 0;
            backdrop->fades[i].r0 = 255 - colour->r;
            backdrop->fades[i].g0 = 255 - colour->g;
            backdrop->fades[i].b0 = 255 - colour->b;
            backdrop->fades[i].r1 = 255 - colour->r;
            backdrop->fades[i].g1 = 255 - colour->g;
            backdrop->fades[i].b1 = 255 - colour->b;
            backdrop->fades[i].x0 = 0;
            backdrop->fades[i].x1 = 320;
            backdrop->fades[i].x2 = 0;
            backdrop->fades[i].x3 = 320;
        }
        colour++;
        for (i = 2; i < 4; i++) {
            SetPolyG4(&backdrop->fades[i]);
            backdrop->fades[i].r0 = colour->r;
            backdrop->fades[i].g0 = colour->g;
            backdrop->fades[i].b0 = colour->b;
            backdrop->fades[i].r1 = colour->r;
            backdrop->fades[i].g1 = colour->g;
            backdrop->fades[i].b1 = colour->b;
            colour++;
            backdrop->fades[i].r2 = colour->r;
            backdrop->fades[i].g2 = colour->g;
            backdrop->fades[i].b2 = colour->b;
            backdrop->fades[i].r3 = colour->r;
            backdrop->fades[i].g3 = colour->g;
            backdrop->fades[i].b3 = colour->b;
            colour--;
            backdrop->fades[i].x0 = 0;
            backdrop->fades[i].x1 = 320;
            backdrop->fades[i].x2 = 0;
            backdrop->fades[i].x3 = 320;
        }
        colour++;
        for (i = 2; i < 4; i++) {
            SetPolyF4(&backdrop->fills[i]);
            backdrop->fills[i].r0 = colour->r;
            backdrop->fills[i].g0 = colour->g;
            backdrop->fills[i].b0 = colour->b;
            backdrop->fills[i].x0 = 0;
            backdrop->fills[i].x1 = 320;
            backdrop->fills[i].x2 = 0;
            backdrop->fills[i].y2 = 240;
            backdrop->fills[i].x3 = 320;
            backdrop->fills[i].y3 = 240;
        }
        for (i = 0; i < 128; i++) {
            SetPolyFT4(&backdrop->tiles[i]);
            SetShadeTex(&backdrop->tiles[i], 1);
            backdrop->tiles[i].clut = GetClut(clutX, clutY);
            backdrop->tiles[i].tpage = GetTPage(0, 1, texX / 64 * 64, texY / 256 * 256);
        }
        for (i = 0; i < 2; i++) {
            window.x = texX % 64;
            window.y = texY % 256;
            window.w = size;
            window.h = size;
            SetDrawMode(&backdrop->modes[i], env.dfe, env.dtd, GetTPage(0, 1, 0, 0), &window);
        }
        for (i = 2; i < 4; i++) {
            SetDrawMode(&backdrop->modes[i], env.dfe, env.dtd, GetTPage(0, 1, 0, 0), &env.tw);
        }
    }
    return backdrop;
}

/* Register the stage actors and light entries; clear each light's active
 * flag. Without lights both pointers are cleared. */
void func_801E7EC4(void *actors, StageLight *lights, s32 count) {
    s32 i;

    D_800D3344 = actors;
    D_800D39CC = lights;
    D_800D3348 = count;
    D_800D2F64 = 1;
    if (lights != NULL) {
        for (i = 0; i < D_800D3348; i++) {
            D_800D39CC[i].active = 0;
        }
    }
    if (count == 0) {
        D_800D3344 = NULL;
        D_800D39CC = NULL;
    }
}
