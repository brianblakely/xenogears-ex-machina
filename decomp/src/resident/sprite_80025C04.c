/* Resident sprite engine, fourth unit (0x80025c04-0x8002709c): GTE pixel
 * colour scaling and sprite sheet drawing. Built like the first unit
 * (sprite.c). Its rodata (the identity matrix of 80025fa8 at 0x800188cc)
 * follows the third unit's last jump table, which a CDK unit would emit
 * after its constants, and it addresses the queue globals of the third
 * unit (80059534, 80059580) absolutely where that unit uses $gp. It starts
 * after 80025a88, the last $gp user, and by 80025fa8; 80025c04 is chosen. */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "gte.h"

/* Corners of a sheet part being drawn (z 0x1000 until drawn). */
SVECTOR D_8004FDC0[4] = {{0, 0, 0x1000}, {0, 0, 0x1000}, {0, 0, 0x1000}, {0, 0, 0x1000}};

/* Scale `count` 15-bit pixels from `src` into `dst` by `scale` / 32 with
 * the GTE (through the scratchpad), clamping each component and keeping
 * the transparency bit. */
void func_80025C04(s32 count, s32 scale, u16 *dst, u16 *src) {
    ColourScratch *scratch = COLOUR_SCRATCH;

    gte_lddp(scale << 7);
    while (--count != -1) {
        scratch->in.vx = *src & 0x1F;
        scratch->in.vy = *src & 0x3E0;
        scratch->in.vz = *src & 0x7C00;
        gte_ldlvl(&scratch->in);
        gte_gpf12();
        gte_stlvl(&scratch->out);
        if (scratch->out.vx >= 0x20) {
            scratch->colour = 0x1F;
        } else {
            scratch->colour = scratch->out.vx & 0x1F;
        }
        if (scratch->out.vy > 0x3E0) {
            scratch->colour |= 0x3E0;
        } else {
            scratch->colour |= scratch->out.vy & 0x3E0;
        }
        if (scratch->out.vz > 0x7C00) {
            scratch->colour |= 0x7C00;
        } else {
            scratch->colour |= scratch->out.vz & 0x7C00;
        }
        scratch->colour |= *src++ & 0x8000;
        *dst++ = scratch->colour;
    }
}

/* Blend `count` 15-bit pixels towards a tinted copy of `src` with the
 * GTE: each source pixel is taken whole (mode 0), halved (1), quartered (2)
 * or made grey (3), offset by the tint and clamped; the result moves from
 * that towards the `base` pixel by `factor` / 32 (at most 1) and goes to
 * `dst`, except for transparent (zero) source pixels. */
void func_80025D4C(s32 count, u16 *src, u16 *base, u16 *dst, s32 red, s32 green, s32 blue, s32 mode,
                   s32 factor) {
    VECTOR delta;
    u16 pixel;
    s16 source_r;
    s16 source_g;
    s16 source_b;
    s16 r;
    s16 g;
    s16 b;
    s32 grey;
    u32 grey_green;
    u32 grey_blue;
    u16 colour;

    if (factor > 0x20) {
        factor = 0x20;
    }
    gte_lddp(factor << 7);
    blue <<= 10;
    green <<= 5;
    while (--count != -1) {
        pixel = *src;
        switch (mode) {
        case 0:
            source_r = pixel & 0x1F;
            source_g = pixel & 0x3E0;
            source_b = pixel & 0x7C00;
            break;
        case 1:
            source_r = (pixel & 0x1E) >> 1;
            source_g = (pixel & 0x3C0) >> 1;
            source_b = (pixel & 0x7800) >> 1;
            break;
        case 2:
            source_r = (pixel & 0x1C) >> 2;
            source_g = (pixel & 0x380) >> 2;
            source_b = (pixel & 0x7000) >> 2;
            break;
        case 3:
            grey_green = pixel & 0x3E0;
            grey_blue = pixel & 0x7C00;
            grey = (s32)((pixel & 0x1F) + (grey_green >> 5) + (grey_blue >> 10)) / 3;
            source_r = grey;
            source_g = grey << 5;
            source_b = grey << 10;
            break;
        }
        r = source_r + red;
        if (r < 0) {
            r = 0;
        }
        g = source_g + green;
        if (g < 0) {
            g = 0;
        }
        b = source_b + blue;
        if (b < 0) {
            b = 0;
        }
        if (r > 0x1F) {
            r = 0x1F;
        }
        if (g > 0x3E0) {
            g = 0x3E0;
        }
        if (b > 0x7C00) {
            b = 0x7C00;
        }
        delta.vx = (*base & 0x1F) - r;
        delta.vy = (*base & 0x3E0) - g;
        delta.vz = (*base & 0x7C00) - b;
        gte_ldlvl(&delta);
        gte_gpf12();
        gte_stlvl(&delta);
        if (pixel != 0) {
            colour = (u16)(r + ((u16)delta.vx & 0x1F)) | 0x8000;
            *dst = colour | (g + (delta.vy & 0x3E0)) | (b + (delta.vz & 0x7C00));
        }
        src++;
        base++;
        dst++;
    }
}

/* Draw entry `id` of a sprite sheet at screen (x, y), scaled and turned
 * by `angle`: each of its parts becomes a textured quad (every second one
 * of `prims`, from `index`) through the GTE with the geometry offset at
 * (x, y), mirrored by its flip bytes; unturned parts drawn mirrored lose a
 * texel at the edge. Returns the number of parts. */
s32 func_80025FA8(u16 *sheet, s32 id, POLY_FT4 *prims, s32 index, s16 x, s16 y, s16 scale_x, s16 scale_y,
                  s16 angle) {
    MATRIX matrix = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    VECTOR scale;
    long offset_x;
    long offset_y;
    long interpolation;
    long flag;
    long screen;
    s16 *entry;
    SheetPart *part;
    POLY_FT4 *poly;
    s32 i;
    s16 u;
    s16 v;
    s16 w;
    s16 h;
    s16 left;
    s16 top;

    scale.vx = scale_x;
    scale.vy = scale_y;
    scale.vz = 0x1000;
    PushMatrix();
    ScaleMatrixL(&matrix, &scale);
    RotMatrixZ(angle, &matrix);
    ReadGeomOffset(&offset_x, &offset_y);
    screen = ReadGeomScreen();
    SetGeomOffset(x, y);
    SetGeomScreen(0x1000);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    for (i = 0; i != entry[0]; i++) {
        poly = prims + i * 2 + index;
        part = &((SheetPart *)(entry + 2))[i];
        SetPolyFT4(poly);
        SetSemiTrans(poly, 0);
        SetShadeTex(poly, 1);
        poly->tpage = GetTPage(part->mode, 0, (s16)part->page_x, (s16)part->page_y);
        poly->clut = GetClut(part->clut_x, part->clut_y);
        w = part->w;
        h = part->h;
        left = part->x;
        top = part->y;
        if (!part->flip_x) {
            D_8004FDC0[0].vx = left;
            D_8004FDC0[1].vx = left + w;
            D_8004FDC0[2].vx = left + w;
            D_8004FDC0[3].vx = left;
        } else {
            D_8004FDC0[0].vx = left + w;
            D_8004FDC0[1].vx = left;
            D_8004FDC0[2].vx = left;
            D_8004FDC0[3].vx = left + w;
        }
        if (!part->flip_y) {
            D_8004FDC0[2].vy = top + h;
            D_8004FDC0[3].vy = top + h;
            D_8004FDC0[0].vy = top;
            D_8004FDC0[1].vy = top;
        } else {
            D_8004FDC0[2].vy = top;
            D_8004FDC0[3].vy = top;
            D_8004FDC0[0].vy = top + h;
            D_8004FDC0[1].vy = top + h;
        }
        RotTransPers4(&D_8004FDC0[0], &D_8004FDC0[1], &D_8004FDC0[2], &D_8004FDC0[3], (long *)&poly->x0,
                      (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &interpolation, &flag);
        u = part->u;
        v = part->v;
        w = part->w;
        h = part->h;
        if ((angle & 0xFFF) == 0xC00) {
            u--;
        }
        if ((angle & 0xFFF) == 0) {
            if (poly->x3 < poly->x0) {
                if (--u < 0) {
                    u = 0;
                    w--;
                }
            }
            if (poly->y3 < poly->y0) {
                if (--v < 0) {
                    v = 0;
                    h--;
                }
            }
        }
        poly->u0 = u;
        poly->v0 = v;
        poly->u1 = u + w;
        poly->v1 = v;
        poly->u2 = u;
        poly->v2 = v + h;
        poly->u3 = u + w;
        poly->v3 = v + h;
    }
    SetGeomOffset(offset_x, offset_y);
    SetGeomScreen(screen);
    PopMatrix();
    return entry[0];
}

/* The texture of sheet entry `id`: its first word, mode, CLUT position and
 * the VRAM position of its pixels (page plus column and row). */
void func_80026338(u16 *sheet, s32 id, s32 *first, s32 *mode, s32 *clut_x, s32 *clut_y, s32 *x, s32 *y) {
    s16 *entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    SheetPart *part = (SheetPart *)(entry + 2);
    s32 u;
    s32 column;

    *first = entry[0];
    u = part->u << 16;
    if (part->mode != 0) {
        column = u >> 18;
    } else {
        column = u >> 20;
    }
    *mode = part->mode;
    *clut_x = part->clut_x;
    *clut_y = part->clut_y;
    *x = (s16)(part->page_x & 0xFFC0) + column;
    *y = (s16)(part->page_y & 0xFF00) + part->v;
}

/* Draw entry `id` of a sprite sheet at screen (x, y) without the GTE:
 * each part becomes a textured quad (every second one of `prims`, from
 * `index`) placed and sized by `scale` / 4096, mirrored by the flip
 * arguments and by its own flip bytes; mirrored parts lose a texel at the
 * edge. Returns the number of parts. */
s32 func_800263E4(u16 *sheet, s32 id, POLY_FT4 *prims, s32 index, s16 x, s16 y, u16 scale, u8 flip_x, u8 flip_y) {
    s16 *entry;
    SheetPart *part;
    POLY_FT4 *poly;
    s32 i;
    s16 left;
    s16 top;
    s16 width;
    s16 height;
    s16 dx;
    s16 dy;
    s16 dw;
    s16 dh;
    s16 u;
    s16 v;
    s16 w;
    s16 h;
    s16 a;
    s16 b;

    entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    for (i = 0; i != entry[0]; i++) {
        poly = prims + i * 2 + index;
        part = &((SheetPart *)(entry + 2))[i];
        dx = left = (s16)part->x * scale / 4096;
        dy = top = (s16)part->y * scale / 4096;
        dw = width = (s16)part->w * scale / 4096;
        dh = height = (s16)part->h * scale / 4096;
        SetPolyFT4(poly);
        SetSemiTrans(poly, 0);
        SetShadeTex(poly, 1);
        poly->tpage = GetTPage(part->mode, 0, (s16)part->page_x, (s16)part->page_y);
        poly->clut = GetClut(part->clut_x, part->clut_y);
        if (flip_x) {
            dw = -width;
            dx = -left;
        }
        if (flip_y) {
            dy = -top;
            dh = -height;
        }
        u = part->u;
        v = part->v;
        w = part->w;
        h = part->h;
        if (!part->flip_x) {
            a = x + dx;
            b = dw + a;
            poly->x0 = a;
            poly->x1 = b;
            poly->x2 = a;
            poly->x3 = b;
        } else {
            a = x + dx;
            b = dw + a;
            poly->x0 = b;
            poly->x1 = a;
            poly->x2 = b;
            poly->x3 = a;
        }
        if (!part->flip_y) {
            a = y + dy;
            b = dh + a;
            poly->y0 = a;
            poly->y1 = a;
            poly->y2 = b;
            poly->y3 = b;
        } else {
            a = y + dy;
            b = dh + a;
            poly->y0 = b;
            poly->y1 = b;
            poly->y2 = a;
            poly->y3 = a;
        }
        if (poly->x3 < poly->x0) {
            if (--u < 0) {
                u = 0;
                w--;
            }
        }
        if (poly->y3 < poly->y0) {
            if (--v < 0) {
                v = 0;
                h--;
            }
        }
        poly->u0 = u;
        poly->v0 = v;
        poly->u1 = u + w;
        poly->v1 = v;
        poly->u2 = u;
        poly->v2 = v + h;
        poly->u3 = u + w;
        poly->v3 = v + h;
    }
    return entry[0];
}

/* 800263e4 without the flip arguments: each part mirrored by its own flip
 * bytes, losing a texel at the edge. Returns the number of parts. */
s32 func_8002675C(u16 *sheet, s32 id, POLY_FT4 *prims, s32 index, s16 x, s16 y, u16 scale) {
    s16 *entry;
    SheetPart *part;
    POLY_FT4 *poly;
    s32 i;
    s16 left;
    s16 top;
    s16 width;
    s16 height;
    s16 u;
    s16 v;
    s16 w;
    s16 h;
    s16 a;
    s16 b;

    entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    for (i = 0; i != entry[0]; i++) {
        poly = prims + i * 2 + index;
        part = &((SheetPart *)(entry + 2))[i];
        left = (s16)part->x * scale / 4096;
        top = (s16)part->y * scale / 4096;
        width = (s16)part->w * scale / 4096;
        height = (s16)part->h * scale / 4096;
        SetPolyFT4(poly);
        SetSemiTrans(poly, 0);
        SetShadeTex(poly, 1);
        poly->tpage = GetTPage(part->mode, 0, (s16)part->page_x, (s16)part->page_y);
        poly->clut = GetClut(part->clut_x, part->clut_y);
        u = part->u;
        v = part->v;
        w = part->w;
        h = part->h;
        if (!part->flip_x) {
            a = x + left;
            b = width + a;
            poly->x0 = a;
            poly->x1 = b;
            poly->x2 = a;
            poly->x3 = b;
        } else {
            a = x + left;
            b = width + a;
            poly->x0 = b;
            poly->x1 = a;
            poly->x2 = b;
            poly->x3 = a;
            if (--u < 0) {
                u = 0;
                w--;
            }
        }
        if (!part->flip_y) {
            a = y + top;
            b = height + a;
            poly->y0 = a;
            poly->y1 = a;
            poly->y2 = b;
            poly->y3 = b;
        } else {
            a = y + top;
            b = height + a;
            poly->y0 = b;
            poly->y1 = b;
            poly->y2 = a;
            poly->y3 = a;
            if (--v < 0) {
                v = 0;
                h--;
            }
        }
        poly->u0 = u;
        poly->v0 = v;
        poly->u1 = u + w;
        poly->v1 = v;
        poly->u2 = u;
        poly->v2 = v + h;
        poly->u3 = u + w;
        poly->v3 = v + h;
    }
    return entry[0];
}

/* Draw a sheet entry into every second SPRT from `index`, then place a
 * draw-mode packet in the following slot. The texture page comes from the
 * first sheet part, including when the entry has no parts. */
s32 func_80026A0C(u16 *sheet, s32 id, SPRT *prims, s32 index, s16 x, s16 y) {
    s16 *entry;
    SheetPart *part;
    SPRT *sprt;
    s32 i;
    s16 left, top;
    u16 u, v, w, h;

    entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    for (i = 0; i != entry[0]; i++) {
        sprt = prims + i * 2 + index;
        part = &((SheetPart *)(entry + 2))[i];
        SetSprt(sprt);
        sprt->clut = GetClut(part->clut_x, part->clut_y);
        SetSemiTrans(sprt, 0);
        SetShadeTex(sprt, 1);
        left = part->x;
        top = part->y;
        u = part->u;
        v = part->v;
        w = part->w;
        h = part->h;
        sprt->x0 = x + left;
        sprt->y0 = y + top;
        sprt->u0 = u;
        sprt->v0 = v;
        sprt->w = w;
        sprt->h = h;
    }
    part = (SheetPart *)(entry + 2);
    SetDrawMode((DR_MODE *)(prims + (i << 1) + index), 0, 0,
                GetTPage(part->mode, 0, (s16)part->page_x, (s16)part->page_y), 0);
    return entry[0] + 1;
}

void func_80026B9C(void) {
}

/* Queue the sheet entry as textured quads at (x, y), linking each quad at
 * `ot`. Its signed texture coordinates supply the offsets within its VRAM
 * page. Leave the queue untouched unless every part fits before its end. */
/* The texture column variable is reused for the part's u coordinate. */
void func_80026BA4(u16 *sheet, s32 id, s32 x, s32 y, u_long *ot) {
    s16 *entry;
    SheetPart *part;
    POLY_FT4 *poly;
    s32 i;
    s32 count;
    s32 v, w, h, left, top, mode;
    s32 page_x, page_y, column, texture_u;

    entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    count = entry[0];
    if ((u8 *)D_80059580 + count * sizeof(POLY_FT4) < D_80059534) {
        for (i = 0; i != count; i++) {
            poly = (POLY_FT4 *)D_80059580;
            D_80059580 = (SpriteQueueEntry *)(poly + 1);
            setlen(poly, 9);
            setcode(poly, 0x2d);
            part = &((SheetPart *)(entry + 2))[i];
            texture_u = part->u << 16;
            if (part->mode) {
                column = texture_u >> 18;
            } else {
                column = texture_u >> 20;
            }
            page_x = (s16)(part->page_x & 0xffc0) + column;
            page_y = (s16)(part->page_y & 0xff00) + part->v;
            v = part->v;
            mode = part->mode;
            w = (s16)part->w;
            h = (s16)part->h;
            left = (s16)part->x;
            top = (s16)part->y;
            column = (s16)part->u;
            poly->clut = GetClut(part->clut_x, part->clut_y);
            poly->tpage = GetTPage(mode, 0, page_x, page_y);
            setXYWH(poly, left + x, top + y, w, h);
            setUVWH(poly, column, v, w, h);
            AddPrim(ot, poly);
        }
    }
}

/* Fill the renderer's compact part records from the sheet entry, translated
 * by (x, y), with blend mode 1. Preserve each record's other fields. */
s32 func_80026DCC(u16 *sheet, s32 id, SpritePart *parts, s16 x, s16 y) {
    s16 *entry;
    SheetPart *part;
    s32 count;
    s32 i;
    s32 mode;
    s32 u, column;
    s32 page_x, page_y;

    entry = (s16 *)(sheet[id + 2] + (s32)sheet);
    count = entry[0];
    part = (SheetPart *)(entry + 2);
    for (i = 0; i != count; i++, part++, parts++) {
        u = part->u << 16;
        if (part->mode) {
            column = u >> 18;
        } else {
            column = u >> 20;
        }
        page_x = (s16)(part->page_x & 0xffc0) + column;
        page_y = (s16)(part->page_y & 0xff00) + part->v;
        mode = part->mode;
        parts->clut = GetClut(part->clut_x, part->clut_y);
        parts->tpage = GetTPage(mode, 1, page_x, page_y);
        parts->u = part->u;
        parts->v = part->v;
        parts->w = part->w;
        parts->h = part->h;
        parts->x = part->x + x;
        parts->y = part->y + y;
    }
    return count;
}

INCLUDE_ASM("decomp/src/resident", func_80026F44);

INCLUDE_ASM("decomp/src/resident", func_80026FE8);
