#include "menu_card.h"

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C511C);

/* Place a textured quad at (x, y) of size w x h showing texels (u, v)..(u + w, v + h). */
void func_801C51B8(POLY_FT4 *poly, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->u0 = u;
    poly->u2 = u;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->v0 = v;
    poly->u1 = u + w;
    poly->v1 = v;
    poly->v2 = v + h;
    poly->u3 = u + w;
    poly->v3 = v + h;
}

/* Test member `id`'s bit of a party bit mask. */
u16 func_801C5228(u16 mask, u8 id) {
    return D_801D6C68[id] & mask;
}

/* The party bit of member `id`. */
u16 func_801C5244(u8 id) {
    return D_801D6C68[id];
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5260);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C527C);

/* Split `value` into nine decimal digits (menu state +31c), leading zeros blanked (ff). */
void func_801C5298(u32 value) {
    s32 i;
    u32 divisor;

    divisor = 100000000;
    for (i = 0; i < 9; i++) {
        D_800625A0->digits[i] = value / divisor;
        value %= divisor;
        divisor /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (D_800625A0->digits[i] != 0) {
            if (D_800625A0->digits[i - 1] == 0) {
                D_800625A0->digits[i - 1] = 0xFF;
            }
            break;
        }
        D_800625A0->digits[i - 1] = 0xFF;
    }
}

/* Allocate (nonzero) or release the card state block. */
void func_801C5344(u8 allocate) {
    if (allocate) {
        D_800625A0->card = func_80031BDC(sizeof(CardState), 0);
        func_8003F8E8(D_800625A0->card, sizeof(CardState));
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate (nonzero) or release the party block. */
void func_801C53A8(u8 allocate) {
    if (allocate) {
        D_800625A0->party = func_80031BDC(sizeof(PartyBlock), 0);
        func_8003F8E8(D_800625A0->party, sizeof(PartyBlock));
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate (nonzero) or release the block at menu state +350. */
void func_801C540C(u8 allocate) {
    if (allocate) {
        D_800625A0->unk350 = func_80031BDC(0x1194, 0);
        func_8003F8E8(D_800625A0->unk350, 0x1194);
    } else {
        func_800320E8(D_800625A0->unk350);
    }
}

/* Allocate (nonzero) or release the block at menu state +354. */
void func_801C5470(u8 allocate) {
    if (allocate) {
        D_800625A0->unk354 = func_80031BDC(0x140C, 0);
        func_8003F8E8(D_800625A0->unk354, 0x140C);
    } else {
        func_800320E8(D_800625A0->unk354);
    }
}

/* Allocate (nonzero) or release the unpacked resource table. */
void func_801C54D4(u8 allocate) {
    if (allocate) {
        D_800625A0->resources = func_80031BDC(0xCC, 0);
        func_8003F8E8(D_800625A0->resources, 0xCC);
    } else {
        func_800320E8(D_800625A0->resources);
    }
}

/* Allocate (nonzero) or release the cursor block. */
void func_801C5538(u8 allocate) {
    if (allocate) {
        D_800625A0->cursor = func_80031BDC(sizeof(CursorBlock), 0);
        func_8003F8E8(D_800625A0->cursor, sizeof(CursorBlock));
    } else {
        func_800320E8(D_800625A0->cursor);
    }
}

/* Allocate (nonzero) or release the block at menu state +1e20. */
void func_801C559C(u8 allocate) {
    if (allocate) {
        D_800625A0->unk1E20 = func_80031BDC(0xDEC, 0);
        func_8003F8E8(D_800625A0->unk1E20, 0xDEC);
    } else {
        func_800320E8(D_800625A0->unk1E20);
    }
}

/* Allocate (nonzero) or release the block at menu state +450. */
void func_801C5600(u8 allocate) {
    if (allocate) {
        D_800625A0->unk450 = func_80031BDC(0x4788, 0);
        func_8003F8E8(D_800625A0->unk450, 0x4788);
    } else {
        func_800320E8(D_800625A0->unk450);
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5664);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C56C8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5B08);

/* Start building draw buffer 0. */
void func_801C5C98(void) {
    D_800625A0->buffer = 0;
}

/* Set up both buffers' quads of label `index` (two columns, 13-pixel rows from `row`). */
#ifdef NON_MATCHING
void func_801C5CA8(Label *label, s32 index, s32 row, s32 mode) {
    POLY_FT4 *poly;
    s32 i;
    s32 semi;
    s32 column;
    s32 line;
    s32 u;
    s32 v;

    i = 0;
    column = index & 1;
    line = index / 2;
    u = (line & 1) << 7;
    poly = label->poly;
    for (i = 0; i < 2; i++, poly++) {
        semi = 0;
        func_80043CB0(poly);
        func_80043BFC(poly, 0);
        func_80043C24(poly, 0);
        poly->r0 = 0x80;
        poly->g0 = 0x80;
        poly->b0 = 0x80;
        if ((u8)mode == 0) {
            label->highlight = column;
            poly->tpage = func_80043A1C(0, 0, 0x140, 0);
            poly->u0 = u;
            v = ((index + row) / 4) * 13;
            poly->v0 = v;
            poly->u1 = u + label->width;
            poly->v1 = v;
            poly->u2 = u;
            poly->v2 = v + 13;
            poly->u3 = u + label->width;
            poly->v3 = v + 13;
        } else {
            if (!(mode & 0x80)) {
                semi = 0x20;
                func_80043BFC(poly, 1);
                poly->r0 = semi;
                poly->g0 = semi;
                poly->b0 = semi;
            }
            label->highlight = (mode & 0x7F) - 1;
            poly->tpage = semi | func_80043A1C(0, 0, 0x180, 0x80);
            poly->u0 = column * 0x60;
            v = line * 13 + row;
            poly->v0 = v;
            poly->v1 = v;
            poly->u2 = column * 0x60;
            poly->v2 = v + 13;
            poly->u1 = column * 0x60 + label->width;
            poly->v3 = v + 13;
            poly->u3 = column * 0x60 + label->width;
        }
        if (label->highlight) {
            poly->clut = D_80059414;
        } else {
            poly->clut = D_800595D4;
        }
    }
    label->dirty = 0;
}
#else
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C5CA8);
#endif

/* Render `count` labels (text ids in pairs) into VRAM and set up their quads. */
void func_801C5EE8(Label *labels, u8 *text_ids, s32 row, s32 count) {
    RECT *rect;
    s32 i;

    for (i = 0; i < count; i += 2) {
        labels[i].width = func_80034EAC(func_80033728(D_800625A0->label_text, text_ids[i]),
                                        D_800625A0->labels[0].pixels, 0x18, 0);
        labels[i + 1].width = func_80034EAC(func_80033728(D_800625A0->label_text, text_ids[i + 1]),
                                            D_800625A0->labels[0].pixels, 0x18, 1);
        rect = &labels[i].rect;
        rect->x = (((i / 2) & 1) << 5) + 0x140;
        rect->y = ((i + row) / 4) * 13;
        rect->w = 0x1C;
        rect->h = 13;
        labels[i + 1].rect = *rect;
        func_801C5CA8(&labels[i], i, row, 0);
        func_801C5CA8(&labels[i + 1], i + 1, row, 0);
        func_80044894(rect, D_800625A0->labels[0].pixels);
        func_800445D0(0);
    }
}

/* Upload a 16-colour palette with only colour 1 set (7fff, white) at (0, 1c0). */
void func_801C6098(void) {
    RECT rect;
    RECT unused; /* never referenced; the original frame reserves it */
    u16 *palette;

    palette = func_80031BDC(0x20, 0);
    func_8003F8E8(palette, 0x20);
    palette[1] = 0x7FFF;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.x = 0;
    rect.h = 1;
    func_80044894(&rect, palette);
    func_800445D0(0);
    func_800320E8(palette);
}

/* Load the text palettes and render the four command labels. */
void func_801C6114(void) {
    func_80033698(0, 0x1D1);
    D_800625A0->labels[0].pixels = func_80031BDC(0x38E, 0);
    func_801C5EE8(D_800625A0->labels, D_801D6A80, 0, 4);
    func_801C6098();
}

/* Look up the four sprite sheet entries the screen draws. */
void func_801C6170(void) {
    POLY_FT4 unused; /* never referenced; the original frame reserves 28h bytes */
    SheetEntry *e;

    e = &D_800625A0->sheet_entries[0];
    func_80026338(D_800625A0->sprite_sheet, 0xFE, &e->u, &e->v, &e->w, &e->h, &e->x, &e->y);
    e = &D_800625A0->sheet_entries[1];
    func_80026338(D_800625A0->sprite_sheet, 0x103, &e->u, &e->v, &e->w, &e->h, &e->x, &e->y);
    e = &D_800625A0->sheet_entries[2];
    func_80026338(D_800625A0->sprite_sheet, 0x100, &e->u, &e->v, &e->w, &e->h, &e->x, &e->y);
    e = &D_800625A0->sheet_entries[3];
    func_80026338(D_800625A0->sprite_sheet, 0x101, &e->u, &e->v, &e->w, &e->h, &e->x, &e->y);
}

/* Draw the cursor at `position`; with `frame` also place its shade and edge lines. */
void func_801C6278(s32 position, u8 frame) {
    func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->cursor, D_800625A0->buffer, D_801D6BFC[position], D_801D6C20[position],
                  0x1000);
    D_800625A0->cursor->sprite_buffer = D_800625A0->buffer;
    if (frame) {
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x0 = D_801D6BFC[position] + 0x14;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y0 = D_801D6C20[position] - 0x24;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x1 = D_801D6BFC[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y1 = D_801D6C20[position] - 0x24;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x2 = D_801D6BFC[position] + 0x14;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y2 = D_801D6C20[position] - 0x14;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x3 = D_801D6BFC[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y3 = D_801D6C20[position] - 0x14;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->x0 = D_801D6BFC[position] + 0x14;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->y0 = D_801D6C20[position] - 0x24;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->x1 = D_801D6BFC[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->upper + D_800625A0->buffer)->y1 = D_801D6C20[position] - 0x24;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->x2 = D_801D6BFC[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->upper + D_800625A0->buffer)->y2 = D_801D6C20[position] - 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->x0 = D_801D6BFC[position] + 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->y0 = D_801D6C20[position] - 0x24;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->x1 = D_801D6BFC[position] + 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->y1 = D_801D6C20[position] - 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->x2 = D_801D6BFC[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->lower + D_800625A0->buffer)->y2 = D_801D6C20[position] - 0x14;
        D_800625A0->cursor->shade_buffer = D_800625A0->buffer;
        D_800625A0->party->cursor_shown = 1;
    }
}

/* Hide the cursor. */
void func_801C665C(void) {
    D_800625A0->party->unk4 = 0;
    D_800625A0->party->cursor_shown = 0;
}

/* Initialise a gouraud quad fading from (r, g, b) on the top edge to black on the bottom. */
void func_801C668C(POLY_G4 *poly, u8 r, u8 g, u8 b) {
    func_80043CC4(poly);
    poly->r0 = r;
    poly->g0 = g;
    poly->b0 = b;
    poly->r1 = r;
    poly->g1 = g;
    poly->b1 = b;
    poly->r2 = 0;
    poly->g2 = 0;
    poly->b2 = 0;
    poly->r3 = 0;
    poly->g3 = 0;
    poly->b3 = 0;
}

/* Initialise both buffers' cursor shade, edge lines, screen quad and draw modes. */
void func_801C6708(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    func_801C665C();
    for (i = 0; i < 2; i++) {
        func_801C668C(&D_800625A0->cursor->shade[i], 0x80, 0x80, 0);
        func_80043BFC(&D_800625A0->cursor->shade[i], 1);
        func_80043DA0(&D_800625A0->cursor->upper[i]);
        (D_800625A0->cursor->upper + i)->r0 = 0;
        (D_800625A0->cursor->upper + i)->g0 = 0x40;
        (D_800625A0->cursor->upper + i)->b0 = 0;
        func_80043DA0(&D_800625A0->cursor->lower[i]);
        (D_800625A0->cursor->lower + i)->r0 = 0;
        (D_800625A0->cursor->lower + i)->g0 = 0x40;
        (D_800625A0->cursor->lower + i)->b0 = 0;
        func_80043C9C(&D_800625A0->cursor->screen[i]);
        (D_800625A0->cursor->screen + i)->x0 = 0;
        (D_800625A0->cursor->screen + i)->y0 = 0;
        (D_800625A0->cursor->screen + i)->x1 = 0x140;
        (D_800625A0->cursor->screen + i)->y1 = 0;
        (D_800625A0->cursor->screen + i)->x2 = 0;
        (D_800625A0->cursor->screen + i)->y2 = 0xE0;
        (D_800625A0->cursor->screen + i)->x3 = 0x140;
        (D_800625A0->cursor->screen + i)->y3 = 0xE0;
        (D_800625A0->cursor->screen + i)->r0 = 0x80;
        (D_800625A0->cursor->screen + i)->g0 = 0x80;
        (D_800625A0->cursor->screen + i)->b0 = 0x80;
        func_80043BFC(&D_800625A0->cursor->screen[i], 1);
        func_800454DC(&D_800625A0->cursor->mode_label[i], 0, 0, func_80043A1C(0, 0, 0x140, 0x80),
                      &window);
        func_800454DC(&D_800625A0->cursor->mode_sprite[i], 0, 0, func_80043A1C(0, 2, 0x180, 0),
                      &window);
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6A54);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C6E74);

/* Set a quad's four corners for the rectangle (x, y, w, h), centred on the screen. */
void func_801C7604(SVECTOR *quad, u16 x, u16 y, u16 w, u16 h) {
    quad[0].vx = x - 0xA0;
    quad[0].vy = y - 0x70;
    quad[0].vz = 0;
    quad[1].vx = x + w - 0xA0;
    quad[1].vy = y - 0x70;
    quad[1].vz = 0;
    quad[2].vx = x - 0xA0;
    quad[2].vy = y + h - 0x70;
    quad[2].vz = 0;
    quad[3].vx = x + w - 0xA0;
    quad[3].vy = y + h - 0x70;
    quad[3].vz = 0;
}

/* Make a textured quad semi-transparent, unshaded and neutral grey. */
void func_801C765C(POLY_FT4 *poly) {
    func_80043BFC(poly, 1);
    func_80043C24(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Draw the scroll bar at (x, y): the thumb position follows `top` of `count` rows. */
void func_801C76A4(s32 x, s32 y, s32 height, s32 count, s32 top) {
    s32 offset;

    offset = 0;
    if (D_800625A0->party->scroll_shown == 0) {
        D_800625A0->scroll = func_80031BDC(sizeof(ScrollBar), 0);
        func_8003F8E8(D_800625A0->scroll, sizeof(ScrollBar));
    }
    if (count < 9) {
        height = 100;
    } else {
        offset = (top * 100) / (count - 8);
        offset = offset * 4000 / 10000;
    }
    func_8002675C(D_800625A0->sprite_sheet, 0x107, D_800625A0->scroll, D_800625A0->buffer, x, y,
                  0x1000);
    func_801C7604(D_800625A0->scroll->quad, x, y + offset, 8, height);
    D_800625A0->scroll->buffer = D_800625A0->buffer;
    D_800625A0->party->scroll_shown = 1;
}

/* Remove the scroll bar. */
void func_801C782C(void) {
    D_800625A0->party->scroll_shown = 0;
    func_800320E8(D_800625A0->scroll);
}

/* Create marker `index`, starting on its first frame. */
void func_801C7870(u8 index) {
    D_800625A0->markers[index] = func_80031BDC(sizeof(Marker), 0);
    func_8003F8E8(D_800625A0->markers[index], sizeof(Marker));
    D_800625A0->markers[index]->frame = 4;
    D_800625A0->markers[index]->timer = 0;
}

/* Animate marker `index` and draw it beside row `row` (at its previous place when `fixed`). */
#ifdef NON_MATCHING
void func_801C78EC(s32 row, s32 unused, u8 fixed, u8 index) {
    Marker *marker;
    POLY_FT4 *poly;
    s32 visible;
    s32 y;

    visible = 1;
    marker = D_800625A0->markers[index];
    if (++marker->timer >= 6) {
        if (--marker->frame < 0) {
            marker->frame = 4;
        }
        marker->timer = 0;
    }
    if (!fixed) {
        y = row * 13 + 0x32;
    }
    visible = 1;
    if (visible) {
        func_8002675C(D_800625A0->sprite_sheet, marker->frame + 0x15B, marker, D_800625A0->buffer, 0,
                      0, 0x1000);
        poly = &marker->sprite[D_800625A0->buffer];
        func_801C7604(marker->quad, poly->x0 + 0x1C, poly->y0 + y, poly->x1 - poly->x0,
                      poly->y3 - poly->y0);
        marker->buffer = D_800625A0->buffer;
        D_800625A0->party->marker_shown[index] = 1;
    } else {
        D_800625A0->party->marker_shown[index] = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C78EC);
#endif

/* Remove marker `index`. */
void func_801C7A88(u8 index) {
    func_800320E8(D_800625A0->markers[index]);
    D_800625A0->party->marker_shown[index] = 0;
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7AE4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7E00);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C7F64);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C81AC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C84F0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C883C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C8B84);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C8ED0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9054);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C90E0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9264);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C93B0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C94CC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9550);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C959C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C962C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9690);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9864);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9A38);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9C0C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9DE0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C9F1C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA068);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA28C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA404);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA754);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA7E4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA874);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA9EC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CAA7C);

void func_801CABD8(void) {
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CABE0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CAC20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB2E8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB35C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB3D0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB498);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB4E4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB690);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBA2C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBDA0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBE60);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC1C4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC31C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC4DC);

void func_801CC520(void) {
}

void func_801CC528(void) {
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC530);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC9A0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCA40);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCC18);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCD20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCE90);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCEBC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CCEE8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CD310);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CD564);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CD838);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CDA0C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CDC68);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CDD74);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE024);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE1D0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE2E8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE32C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE7E0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CE82C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CEA68);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CEEA8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF184);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF33C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF38C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF448);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CF9BC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CFAB8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CFC60);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CFF18);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0054);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0220);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0348);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0398);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D04E8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D05EC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D06D8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0C20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0D4C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D0EC8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D1078);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D1304);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D18F8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D1F20);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2054);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2784);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D27C4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2804);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2950);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D2B74);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3558);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3A3C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3A80);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D3C78);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D44FC);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D4888);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D498C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5398);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D573C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D57A8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5828);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5D38);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5EB8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D5F94);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6150);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D61B8);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6250);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D62A4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6334);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D6738);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801D690C);
