/*
 * The resident RGB555 pixel loops on the GTE (decomp/src/resident/
 * sprite_darken_pixels.s, sprite_blend_pixels.s) as portable C on the software
 * GTE. Each pixel makes the original's register transfers and its GPF, so the
 * GTE is left as the original leaves it: IR0 the level, IR1-IR3, MAC1-MAC3,
 * the colour FIFO and FLAG from the last pixel. Both loops also read the
 * halfword after the last pixel before they exit; that read has no effect.
 */
#include "common.h"
#include "xem/gte_ops.h"

#define XEM_POINTER(p) ((u32)(XemUintptr)(p))

/* IR0 = level in 4.12 (mtc2 keeps the low 16 bits, sign extended). */
static void xem_pixels_set_level(s32 level) {
    xem_gte_write_data(XEM_GTE_IR0, (u32)level << 7);
}

/* IR1-IR3 = the three values, GPF with sf=1 (IR = IR0 * IR >> 12), and the
 * results back. */
static void xem_pixels_scale(u32 red, u32 green, u32 blue, u32 *out) {
    xem_gte_write_data(XEM_GTE_IR1, red);
    xem_gte_write_data(XEM_GTE_IR2, green);
    xem_gte_write_data(XEM_GTE_IR3, blue);
    xem_gte_execute(XEM_GTE_GPF(1));
    out[0] = xem_gte_read_data(XEM_GTE_IR1);
    out[1] = xem_gte_read_data(XEM_GTE_IR2);
    out[2] = xem_gte_read_data(XEM_GTE_IR3);
}

/* Darken `count` pixels from `source` into `out` by level/32 (32 and above
 * copy; no lower clamp). Each colour field is scaled in place; bit 15 is
 * kept, and a nonzero pixel whose colour scales to 0 gets colour 1. The
 * count is tested as the original does (it stops when it decrements to -1). */
void sprite_darken_pixels(s32 count, s32 level, u16 *out, u16 *source) {
    u32 to = XEM_POINTER(out);
    u32 from = XEM_POINTER(source);
    u32 scaled[3];
    u32 pixel;
    u32 colour;

    if (level >= 32) {
        level = 32;
    }
    xem_pixels_set_level(level);
    while (--count != -1) {
        pixel = XEM_U16(from);
        from += 2;
        xem_pixels_scale(pixel & 0x1F, pixel & 0x3E0, pixel & 0x7C00, scaled);
        colour = (scaled[0] & 0x1F) | (scaled[1] & 0x3E0) | (scaled[2] & 0x7C00);
        if (pixel != 0 && colour == 0) {
            colour = 1;
        }
        XEM_U16(to) = (u16)((pixel & 0x8000) | colour);
        to += 2;
    }
}

/* Blend `count` pixels from `base` towards `target` by level/32 into `out`
 * (above 32 clamps to 32; no lower clamp): each field becomes base +
 * (IR & field mask) for IR the GTE-scaled signed difference, so a field that
 * decreases carries into the bit above it (5, 10 or 15). The count is tested
 * as the original does (it stops when it decrements to 0 from count + 1). */
void sprite_blend_pixels(s32 count, s32 level, u16 *out, u16 *base, u16 *target) {
    u32 to = XEM_POINTER(out);
    u32 from = XEM_POINTER(base);
    u32 toward = XEM_POINTER(target);
    u32 scaled[3];
    u32 b;
    u32 t;
    u32 red;
    u32 green;
    u32 blue;
    u32 n = (u32)count + 1;

    if (level >= 33) {
        level = 32;
    }
    xem_pixels_set_level(level);
    while (--n != 0) {
        t = XEM_U16(toward);
        b = XEM_U16(from);
        toward += 2;
        red = b & 0x1F;
        green = b & 0x3E0;
        blue = b & 0x7C00;
        xem_pixels_scale((t & 0x1F) - red, (t & 0x3E0) - green, (t & 0x7C00) - blue, scaled);
        red += scaled[0] & 0x1F;
        green += scaled[1] & 0x3E0;
        blue += scaled[2] & 0x7C00;
        XEM_U16(to) = (u16)(red | green | blue);
        from += 2;
        to += 2;
    }
}
