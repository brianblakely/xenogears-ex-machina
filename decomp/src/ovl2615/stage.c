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

INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/stage", func_801E7914);

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
