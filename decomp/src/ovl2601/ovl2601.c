/*
 * ovl2601 (Disc 1 slot 2601, Disc 2 slot 2596; loaded at 801c5000): a
 * stand-alone memory-card screen built from the same card code as the menu's
 * save overlay (slot 2597), but for the file prefix "BISLPS-00800" instead of
 * this game's "BASLUS-00664". It loads its own menu resources (sprite sheet,
 * label text, icon TIM, sound bank), keeps the card directory and file heads
 * in the menu state's card block and draws the file list and details panel.
 */
#include "menu_card.h"

/* Place a textured quad at (x, y) of size w x h showing texels (u, v)..(u + w, v + h). */
void func_801C5040(POLY_FT4 *poly, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
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
u16 func_801C50B0(u16 mask, u8 id) {
    return D_801D21F0[id] & mask;
}

/* The party bit of member `id`. */
u16 func_801C50CC(u8 id) {
    return D_801D21F0[id];
}

/* Split `value` into nine decimal digits (menu state +31c), leading zeros blanked (ff). */
void func_801C50E8(u32 value) {
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
void func_801C5194(u8 allocate) {
    if (allocate) {
        D_800625A0->card = func_80031BDC(sizeof(CardState), 0);
        func_8003F8E8(D_800625A0->card, sizeof(CardState));
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate (nonzero) or release the screen flag block. */
void func_801C51F8(u8 allocate) {
    if (allocate) {
        D_800625A0->flags = func_80031BDC(sizeof(ScreenFlags), 0);
        func_8003F8E8(D_800625A0->flags, sizeof(ScreenFlags));
    } else {
        func_800320E8(D_800625A0->flags);
    }
}

/* Allocate (nonzero) or release the block at menu state +350. */
void func_801C525C(u8 allocate) {
    if (allocate) {
        D_800625A0->unk350 = func_80031BDC(0x1194, 0);
        func_8003F8E8(D_800625A0->unk350, 0x1194);
    } else {
        func_800320E8(D_800625A0->unk350);
    }
}

/* Allocate (nonzero) or release the block at menu state +354. */
void func_801C52C0(u8 allocate) {
    if (allocate) {
        D_800625A0->unk354 = func_80031BDC(0x140C, 0);
        func_8003F8E8(D_800625A0->unk354, 0x140C);
    } else {
        func_800320E8(D_800625A0->unk354);
    }
}

/* Allocate (nonzero) or release the unpacked resource table. */
void func_801C5324(u8 allocate) {
    if (allocate) {
        D_800625A0->resources = func_80031BDC(0xCC, 0);
        func_8003F8E8(D_800625A0->resources, 0xCC);
    } else {
        func_800320E8(D_800625A0->resources);
    }
}

/* Allocate (nonzero) or release the cursor block. */
void func_801C5388(u8 allocate) {
    if (allocate) {
        D_800625A0->cursor = func_80031BDC(sizeof(CursorBlock), 0);
        func_8003F8E8(D_800625A0->cursor, sizeof(CursorBlock));
    } else {
        func_800320E8(D_800625A0->cursor);
    }
}

/* Allocate (nonzero) or release the block at menu state +1e20. */
void func_801C53EC(u8 allocate) {
    if (allocate) {
        D_800625A0->unk1E20 = func_80031BDC(0xDEC, 0);
        func_8003F8E8(D_800625A0->unk1E20, 0xDEC);
    } else {
        func_800320E8(D_800625A0->unk1E20);
    }
}

/* Allocate (nonzero) or release the block at menu state +450. */
void func_801C5450(u8 allocate) {
    if (allocate) {
        D_800625A0->unk450 = func_80031BDC(0x4788, 0);
        func_8003F8E8(D_800625A0->unk450, 0x4788);
    } else {
        func_800320E8(D_800625A0->unk450);
    }
}

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C54B4);

/* Reset the screen state, note which party members are available and load the resources. */
void func_801C58F4(void) {
    u16 available;
    s32 i;
    s32 id;

    D_800625A0->top_cursor = 4;
    D_800625A0->unk337 = 0xFF;
    D_800625A0->poll_timer = 0x3C;
    D_800625A0->unk334 = 0;
    D_800625A0->unk335 = 0;
    available = D_8006F364 & D_8006F366 & 0x7FF;
    for (i = 0; i < 16; i++) {
        if (func_801C50B0(available, i)) {
            D_800625A0->member_present[i] = 1;
        } else {
            D_800625A0->member_present[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = D_8006F368[i];
        if (id != 0xFF && D_800625A0->member_present[id] != 0) {
            D_800625A0->flags->members[i] = id;
        } else {
            D_800625A0->flags->members[i] = 0xFF;
        }
    }
    func_801C54B4();
}

/* Start building draw buffer 0. */
void func_801C5A6C(void) {
    D_800625A0->buffer = 0;
}

/* Set up both buffers' quads of label `index` (two columns, 13-pixel rows from `row`). */
#ifdef NON_MATCHING
void func_801C5A7C(Label *label, s32 index, s32 row, s32 mode) {
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
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C5A7C);
#endif

/* Render `count` labels (text ids in pairs) into VRAM and set up their quads. */
void func_801C5CBC(Label *labels, u8 *text_ids, s32 row, s32 count) {
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
        func_801C5A7C(&labels[i], i, row, 0);
        func_801C5A7C(&labels[i + 1], i + 1, row, 0);
        func_80044894(rect, D_800625A0->labels[0].pixels);
        func_800445D0(0);
    }
}

/* Upload a 16-colour palette with only colour 1 set (7fff, white) at (0, 1c0). */
void func_801C5E6C(void) {
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
void func_801C5EE8(void) {
    func_80033698(0, 0x1D1);
    D_800625A0->labels[0].pixels = func_80031BDC(0x38E, 0);
    func_801C5CBC(D_800625A0->labels, D_801D2018, 0, 4);
    func_801C5E6C();
}

/* Look up the four sprite sheet entries the screen draws. */
void func_801C5F44(void) {
    POLY_FT4 unused; /* never referenced; the original frame reserves 28h bytes */
    SheetEntry *e;

    e = &D_800625A0->sheet_entries[0];
    func_80026338(D_800625A0->sprite_sheet, 0xFE, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &D_800625A0->sheet_entries[1];
    func_80026338(D_800625A0->sprite_sheet, 0x103, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &D_800625A0->sheet_entries[2];
    func_80026338(D_800625A0->sprite_sheet, 0x100, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &D_800625A0->sheet_entries[3];
    func_80026338(D_800625A0->sprite_sheet, 0x101, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
}

/* Draw the cursor at `position`; with `frame` also place its shade and edge lines. */
void func_801C604C(s32 position, u8 frame) {
    func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->cursor, D_800625A0->buffer, D_801D2194[position], D_801D21B0[position],
                  0x1000);
    D_800625A0->cursor->sprite_buffer = D_800625A0->buffer;
    if (frame) {
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x0 = D_801D2194[position] + 0x14;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y0 = D_801D21B0[position] - 0x24;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x1 = D_801D2194[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y1 = D_801D21B0[position] - 0x24;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x2 = D_801D2194[position] + 0x14;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y2 = D_801D21B0[position] - 0x14;
        (D_800625A0->cursor->shade + D_800625A0->buffer)->x3 = D_801D2194[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->shade + D_800625A0->buffer)->y3 = D_801D21B0[position] - 0x14;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->x0 = D_801D2194[position] + 0x14;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->y0 = D_801D21B0[position] - 0x24;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->x1 = D_801D2194[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->upper + D_800625A0->buffer)->y1 = D_801D21B0[position] - 0x24;
        (D_800625A0->cursor->upper + D_800625A0->buffer)->x2 = D_801D2194[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->upper + D_800625A0->buffer)->y2 = D_801D21B0[position] - 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->x0 = D_801D2194[position] + 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->y0 = D_801D21B0[position] - 0x24;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->x1 = D_801D2194[position] + 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->y1 = D_801D21B0[position] - 0x14;
        (D_800625A0->cursor->lower + D_800625A0->buffer)->x2 = D_801D2194[position] + (D_800625A0->cursor->width + 0x14);
        (D_800625A0->cursor->lower + D_800625A0->buffer)->y2 = D_801D21B0[position] - 0x14;
        D_800625A0->cursor->shade_buffer = D_800625A0->buffer;
        D_800625A0->flags->cursor_shown = 1;
    }
}

/* Hide the cursor. */
void func_801C6430(void) {
    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
}

/* Initialise a gouraud quad fading from (r, g, b) on the top edge to black on the bottom. */
void func_801C6460(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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
void func_801C64DC(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    func_801C6430();
    for (i = 0; i < 2; i++) {
        func_801C6460(&D_800625A0->cursor->shade[i], 0x80, 0x80, 0);
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

/* Unpack (mode 0) or release (mode 10h) the screen's resources from file 2. */
void func_801C6828(u8 mode) {
    void **list;

    if (mode < 0x10) {
        list = func_80031BDC(func_800288EC(2), 1);
        func_800295D8(2, list, 0, 0x80);
        func_80028A60(0);
        func_8003342C(list);
    }
    switch (mode) {
    case 0:
        D_800625A0->resources[0] = func_80032E88(list[2], 0);
        D_800625A0->resources[1] = func_80032E88(list[3], 0);
        D_800625A0->resources[7] = func_80032E88(list[1], 0);
        *(void **)(D_800625A0->unk450 + 0x4634) = func_80032E88(list[0x28], 0);
        *(void **)(D_800625A0->unk450 + 0x4638) = func_80032E88(list[0x29], 0);
        *(void **)(D_800625A0->unk450 + 0x4630) = func_80032E88(list[0x2A], 0);
        break;
    case 0x10:
        func_800320E8(D_800625A0->resources[0]);
        func_800320E8(D_800625A0->resources[1]);
        func_800320E8(D_800625A0->resources[7]);
        func_800320E8(*(void **)(D_800625A0->unk450 + 0x4634));
        func_800320E8(*(void **)(D_800625A0->unk450 + 0x4638));
        func_800320E8(*(void **)(D_800625A0->unk450 + 0x4630));
        break;
    }
    if (mode < 0x10) {
        func_800320E8(list);
    }
}

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C6A6C);

/* Set a quad's four corners for the rectangle (x, y, w, h), centred on the screen. */
void func_801C6E90(SVECTOR *quad, u16 x, u16 y, u16 w, u16 h) {
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
void func_801C6EE8(POLY_FT4 *poly) {
    func_80043BFC(poly, 1);
    func_80043C24(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Draw the scroll bar at (x, y): the thumb position follows `top` of `count` rows. */
void func_801C6F30(s32 x, s32 y, s32 height, s32 count, s32 top) {
    s32 offset;

    offset = 0;
    if (D_800625A0->flags->scroll_shown == 0) {
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
    func_801C6E90(D_800625A0->scroll->quad, x, y + offset, 8, height);
    D_800625A0->scroll->buffer = D_800625A0->buffer;
    D_800625A0->flags->scroll_shown = 1;
}

/* Remove the scroll bar. */
void func_801C70B8(void) {
    D_800625A0->flags->scroll_shown = 0;
    func_800320E8(D_800625A0->scroll);
}

/* Create marker `index`, starting on its first frame. */
void func_801C70FC(u8 index) {
    D_800625A0->markers[index] = func_80031BDC(sizeof(Marker), 0);
    func_8003F8E8(D_800625A0->markers[index], sizeof(Marker));
    D_800625A0->markers[index]->frame = 4;
    D_800625A0->markers[index]->timer = 0;
}

/* Animate marker `index` and draw it beside row `row` (at its previous place when `fixed`). */
#ifdef NON_MATCHING
void func_801C7178(s32 row, s32 unused, u8 fixed, u8 index) {
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
        func_801C6E90(marker->quad, poly->x0 + 0x1C, poly->y0 + y, poly->x1 - poly->x0,
                      poly->y3 - poly->y0);
        marker->buffer = D_800625A0->buffer;
        D_800625A0->flags->marker_shown[index] = 1;
    } else {
        D_800625A0->flags->marker_shown[index] = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C7178);
#endif

/* Remove marker `index`. */
void func_801C7314(u8 index) {
    func_800320E8(D_800625A0->markers[index]);
    D_800625A0->flags->marker_shown[index] = 0;
}

/* Initialise panel `index`'s background, draw modes and edge sprite parts. */
void func_801C7370(u8 index) {
    Panel *panel;
    RECT window;
    u8 i;

    panel = D_800625A0->panels[index];
    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    D_800625A0->flags->panel_shown[index] = 0;
    D_800625A0->flags->panel_growing[index] = 0;
    for (i = 0; i < 2; i++) {
        func_80043CC4(&panel->back[i]);
        (panel->back + i)->r0 = 0x68;
        (panel->back + i)->g0 = 0x68;
        (panel->back + i)->b0 = 0x68;
        (panel->back + i)->r1 = 0x68;
        (panel->back + i)->g1 = 0x68;
        (panel->back + i)->b1 = 0x68;
        (panel->back + i)->r2 = 0x68;
        (panel->back + i)->g2 = 0x68;
        (panel->back + i)->b2 = 0x68;
        (panel->back + i)->r3 = 0x68;
        (panel->back + i)->g3 = 0x68;
        (panel->back + i)->b3 = 0x68;
        func_80043BFC(&panel->back[i], 1);
        func_800454DC(&panel->mode[i], 0, 0,
                      func_80043A1C(0, 0, D_800625A0->sheet_entries[0].page_x, D_800625A0->sheet_entries[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        func_80043CB0(&panel->parts[i + 8]);
        func_80043C24(&panel->parts[i + 8], 1);
        (panel->parts + i + 8)->r0 = 0xFF;
        (panel->parts + i + 8)->g0 = 0xFF;
        (panel->parts + i + 8)->b0 = 0xFF;
        (panel->parts + i + 8)->tpage = func_80043A1C(D_800625A0->sheet_entries[0].mode, 0,
                                                      D_800625A0->sheet_entries[0].page_x,
                                                      D_800625A0->sheet_entries[0].page_y);
        (panel->parts + i + 8)->clut = func_80043A58(D_800625A0->sheet_entries[0].clut_x,
                                                     D_800625A0->sheet_entries[0].clut_y);
        func_80043CB0(&panel->parts[i + 12]);
        func_80043C24(&panel->parts[i + 12], 1);
        (panel->parts + i + 12)->r0 = 0xFF;
        (panel->parts + i + 12)->g0 = 0xFF;
        (panel->parts + i + 12)->b0 = 0xFF;
        (panel->parts + i + 12)->tpage = func_80043A1C(D_800625A0->sheet_entries[1].mode, 0,
                                                       D_800625A0->sheet_entries[1].page_x,
                                                       D_800625A0->sheet_entries[1].page_y);
        (panel->parts + i + 12)->clut = func_80043A58(D_800625A0->sheet_entries[1].clut_x,
                                                      D_800625A0->sheet_entries[1].clut_y);
        func_80043CB0(&panel->parts[i + 16]);
        func_80043C24(&panel->parts[i + 16], 1);
        (panel->parts + i + 16)->r0 = 0xFF;
        (panel->parts + i + 16)->g0 = 0xFF;
        (panel->parts + i + 16)->b0 = 0xFF;
        (panel->parts + i + 16)->tpage = func_80043A1C(D_800625A0->sheet_entries[2].mode, 0,
                                                       D_800625A0->sheet_entries[2].page_x,
                                                       D_800625A0->sheet_entries[2].page_y);
        (panel->parts + i + 16)->clut = func_80043A58(D_800625A0->sheet_entries[2].clut_x,
                                                      D_800625A0->sheet_entries[2].clut_y);
        func_80043CB0(&panel->parts[i + 20]);
        func_80043C24(&panel->parts[i + 20], 1);
        (panel->parts + i + 20)->r0 = 0xFF;
        (panel->parts + i + 20)->g0 = 0xFF;
        (panel->parts + i + 20)->b0 = 0xFF;
        (panel->parts + i + 20)->tpage = func_80043A1C(D_800625A0->sheet_entries[3].mode, 0,
                                                       D_800625A0->sheet_entries[3].page_x,
                                                       D_800625A0->sheet_entries[3].page_y);
        (panel->parts + i + 20)->clut = func_80043A58(D_800625A0->sheet_entries[3].clut_x,
                                                      D_800625A0->sheet_entries[3].clut_y);
    }
}

/* Place panel `index`'s scroll bar (top arrow, bottom arrow, track) at its right edge. */
void func_801C768C(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8002675C(D_800625A0->sprite_sheet, 0x105, &panel->parts[26], D_800625A0->buffer, x, y, 0x1000);
    func_800263E4(D_800625A0->sprite_sheet, 0x105, &panel->parts[28], D_800625A0->buffer, x,
                  y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sprite_sheet, 0x106, &panel->parts[24], D_800625A0->buffer, x, y + 8,
                  0x1000);
    func_801C6E90(panel->quads[14], x, y, 8, 8);
    func_801C6E90(panel->quads[15], x, y + h, 8, -8);
    func_801C6E90(panel->quads[13], x, y + 8, 8, h - 8);
}

/* Build panel `index`'s four corners around (x, y, w, h). */
void func_801C77F0(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel;
    s32 i;

    panel = D_800625A0->panels[index];
    panel->part_count = 0;
    panel->part_count += func_8002675C(D_800625A0->sprite_sheet, 0xFD, &panel->parts[panel->part_count * 2],
                                       D_800625A0->buffer, 0, 0, 0x1000);
    panel->part_count += func_8002675C(D_800625A0->sprite_sheet, 0xFF, &panel->parts[panel->part_count * 2],
                                       D_800625A0->buffer, 0, 0, 0x1000);
    panel->part_count += func_8002675C(D_800625A0->sprite_sheet, 0x102, &panel->parts[panel->part_count * 2],
                                       D_800625A0->buffer, 0, 0, 0x1000);
    panel->part_count += func_8002675C(D_800625A0->sprite_sheet, 0x104, &panel->parts[panel->part_count * 2],
                                       D_800625A0->buffer, 0, 0, 0x1000);
    func_801C6E90(panel->quads[0], x - 8, y + 8, 0x10, -0x10);
    func_801C6E90(panel->quads[1], x + w + 8, y + 8, -0x10, -0x10);
    func_801C6E90(panel->quads[2], x - 8, y + h - 8, 0x10, 0x10);
    func_801C6E90(panel->quads[3], x + w + 8, y + h - 8, -0x10, 0x10);
    for (i = 0; i < 4; i++) {
        func_801C6EE8(&panel->parts[i * 2 + D_800625A0->buffer]);
    }
}

/* Build panel `index`'s top edge, two pieces across the width. */
void func_801C7A38(u8 index, u16 x, u16 y, u16 w) {
    Panel *panel;
    s32 half;
    s32 i;

    panel = D_800625A0->panels[index];
    (panel->parts + D_800625A0->buffer + 8)->u0 = 0;
    (panel->parts + D_800625A0->buffer + 8)->v0 = 0x84;
    (panel->parts + D_800625A0->buffer + 8)->u1 = 7;
    (panel->parts + D_800625A0->buffer + 8)->v1 = 0x84;
    (panel->parts + D_800625A0->buffer + 8)->u2 = 0;
    (panel->parts + D_800625A0->buffer + 8)->v2 = 0x94;
    (panel->parts + D_800625A0->buffer + 8)->u3 = 7;
    (panel->parts + D_800625A0->buffer + 8)->v3 = 0x94;
    (panel->parts + D_800625A0->buffer + 10)->u0 = 0;
    (panel->parts + D_800625A0->buffer + 10)->v0 = 0x84;
    (panel->parts + D_800625A0->buffer + 10)->u1 = 7;
    (panel->parts + D_800625A0->buffer + 10)->v1 = 0x84;
    (panel->parts + D_800625A0->buffer + 10)->u2 = 0;
    (panel->parts + D_800625A0->buffer + 10)->v2 = 0x94;
    (panel->parts + D_800625A0->buffer + 10)->u3 = 7;
    (panel->parts + D_800625A0->buffer + 10)->v3 = 0x94;
    half = (w - 0x10) / 2;
    func_801C6E90(panel->quads[4], x + 8, y - 8, half, 0x10);
    func_801C6E90(panel->quads[5], x + (half + 8), y - 8, half, 0x10);
    for (i = 0; i < 2; i++) {
        func_801C6EE8(&panel->parts[i * 2 + D_800625A0->buffer + 8]);
    }
}

/* Build panel `index`'s bottom edge, two pieces across the width. */
void func_801C7D7C(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel;
    s32 half;
    s32 i;

    panel = D_800625A0->panels[index];
    (panel->parts + D_800625A0->buffer + 12)->u0 = 8;
    (panel->parts + D_800625A0->buffer + 12)->v0 = 0x84;
    (panel->parts + D_800625A0->buffer + 12)->u1 = 0xF;
    (panel->parts + D_800625A0->buffer + 12)->v1 = 0x84;
    (panel->parts + D_800625A0->buffer + 12)->u2 = 8;
    (panel->parts + D_800625A0->buffer + 12)->v2 = 0x94;
    (panel->parts + D_800625A0->buffer + 12)->u3 = 0xF;
    (panel->parts + D_800625A0->buffer + 12)->v3 = 0x94;
    (panel->parts + D_800625A0->buffer + 14)->u0 = 8;
    (panel->parts + D_800625A0->buffer + 14)->v0 = 0x84;
    (panel->parts + D_800625A0->buffer + 14)->u1 = 0xF;
    (panel->parts + D_800625A0->buffer + 14)->v1 = 0x84;
    (panel->parts + D_800625A0->buffer + 14)->u2 = 8;
    (panel->parts + D_800625A0->buffer + 14)->v2 = 0x94;
    (panel->parts + D_800625A0->buffer + 14)->u3 = 0xF;
    (panel->parts + D_800625A0->buffer + 14)->v3 = 0x94;
    half = (w - 0x10) / 2;
    func_801C6E90(panel->quads[6], x + 8, y + h - 8, half, 0x10);
    func_801C6E90(panel->quads[7], x + (half + 8), y + h - 8, half, 0x10);
    for (i = 0; i < 2; i++) {
        func_801C6EE8(&panel->parts[i * 2 + D_800625A0->buffer + 12]);
    }
}

/* Build panel `index`'s left edge, two pieces down the height. */
void func_801C80C8(u8 index, u16 x, u16 y, u16 h) {
    Panel *panel;
    s32 half;
    s32 i;

    panel = D_800625A0->panels[index];
    (panel->parts + D_800625A0->buffer + 16)->u0 = 0x10;
    (panel->parts + D_800625A0->buffer + 16)->v0 = 0x84;
    (panel->parts + D_800625A0->buffer + 16)->u1 = 0x20;
    (panel->parts + D_800625A0->buffer + 16)->v1 = 0x84;
    (panel->parts + D_800625A0->buffer + 16)->u2 = 0x10;
    (panel->parts + D_800625A0->buffer + 16)->v2 = 0x8B;
    (panel->parts + D_800625A0->buffer + 16)->u3 = 0x20;
    (panel->parts + D_800625A0->buffer + 16)->v3 = 0x8B;
    (panel->parts + D_800625A0->buffer + 18)->u0 = 0x10;
    (panel->parts + D_800625A0->buffer + 18)->v0 = 0x84;
    (panel->parts + D_800625A0->buffer + 18)->u1 = 0x20;
    (panel->parts + D_800625A0->buffer + 18)->v1 = 0x84;
    (panel->parts + D_800625A0->buffer + 18)->u2 = 0x10;
    (panel->parts + D_800625A0->buffer + 18)->v2 = 0x8B;
    (panel->parts + D_800625A0->buffer + 18)->u3 = 0x20;
    (panel->parts + D_800625A0->buffer + 18)->v3 = 0x8B;
    half = (h - 0x10) / 2;
    func_801C6E90(panel->quads[8], x - 8, y + 8, 0x10, half);
    func_801C6E90(panel->quads[9], x - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        func_801C6EE8(&panel->parts[i * 2 + D_800625A0->buffer + 16]);
    }
}

/* Build panel `index`'s right edge, two pieces down the height. */
void func_801C8410(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel;
    s32 half;
    s32 i;

    panel = D_800625A0->panels[index];
    (panel->parts + D_800625A0->buffer + 20)->u0 = 0x10;
    (panel->parts + D_800625A0->buffer + 20)->v0 = 0x8C;
    (panel->parts + D_800625A0->buffer + 20)->u1 = 0x20;
    (panel->parts + D_800625A0->buffer + 20)->v1 = 0x8C;
    (panel->parts + D_800625A0->buffer + 20)->u2 = 0x10;
    (panel->parts + D_800625A0->buffer + 20)->v2 = 0x93;
    (panel->parts + D_800625A0->buffer + 20)->u3 = 0x20;
    (panel->parts + D_800625A0->buffer + 20)->v3 = 0x93;
    (panel->parts + D_800625A0->buffer + 22)->u0 = 0x10;
    (panel->parts + D_800625A0->buffer + 22)->v0 = 0x8C;
    (panel->parts + D_800625A0->buffer + 22)->u1 = 0x20;
    (panel->parts + D_800625A0->buffer + 22)->v1 = 0x8C;
    (panel->parts + D_800625A0->buffer + 22)->u2 = 0x10;
    (panel->parts + D_800625A0->buffer + 22)->v2 = 0x93;
    (panel->parts + D_800625A0->buffer + 22)->u3 = 0x20;
    (panel->parts + D_800625A0->buffer + 22)->v3 = 0x93;
    half = (h - 0x10) / 2;
    func_801C6E90(panel->quads[10], x + w - 8, y + 8, 0x10, half);
    func_801C6E90(panel->quads[11], x + w - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        func_801C6EE8(&panel->parts[i * 2 + D_800625A0->buffer + 20]);
    }
}

/* Lay out panel `index` at (x, y, w, h) for this buffer and mark it shown. */
void func_801C875C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 style, s32 unk718, u8 has_bar) {
    Panel *panel;

    panel = D_800625A0->panels[index];
    D_800625A0->flags->panel_shown[index] = 0;
    func_801C6E90(panel->quads[12], x, y, w, h);
    func_801C77F0(index, x, y, w, h);
    func_801C7A38(index, x, y, w);
    func_801C7D7C(index, x, y, w, h);
    func_801C80C8(index, x, y, h);
    func_801C8410(index, x, y, w, h);
    if (has_bar) {
        func_801C768C(index, x, y, w, h);
    }
    panel->has_bar = has_bar;
    panel->style = style;
    panel->unk718 = unk718;
    panel->buffer = D_800625A0->buffer;
    D_800625A0->flags->panel_shown[index] = 1;
}

/* Remove panel `index`. */
void func_801C88E0(u8 index) {
    D_800625A0->flags->panel_shown[index] = 0;
    D_800625A0->flags->panel_growing[index] = 0;
    func_800320E8(D_800625A0->panels[index]);
    func_800320E8(D_800625A0->growth[index]);
}

/* Open panel `index` at (x, y, w, h): at once, or growing from its centre when `grow`. */
void func_801C896C(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 style, s32 unk718,
                   u8 has_bar) {
    PanelGrowth *growth;

    if (index >= 2) {
        D_800625A0->panels[index] = func_80031BDC(sizeof(Panel), 0);
        func_8003F8E8(D_800625A0->panels[index], sizeof(Panel));
        D_800625A0->growth[index] = func_80031BDC(sizeof(PanelGrowth), 0);
        func_8003F8E8(D_800625A0->growth[index], sizeof(PanelGrowth));
        func_801C7370(index);
    }
    growth = D_800625A0->growth[index];
    if (grow) {
        growth->index = index;
        growth->done = 0;
        growth->x = x;
        growth->y = y;
        growth->w = w;
        growth->h = h;
        growth->cur_w = 0;
        growth->cur_h = 0;
        D_800625A0->flags->panel_growing[index] = 1;
        growth->style = style;
        growth->unk0C = unk718;
    } else {
        func_801C875C(index, x, y, w, h, style, unk718, has_bar);
    }
}

/* Grow each opening panel by 20h in both directions until it reaches its size. */
void func_801C8AF0(void) {
    PanelGrowth *growth;
    s32 i;
    u8 finished;

    for (i = 0; i < 7; i++) {
        growth = D_800625A0->growth[i];
        if (D_800625A0->flags->panel_growing[i] != 0 && growth->done == 0) {
            finished = 0;
            if (growth->cur_w + 0x20 >= growth->w) {
                growth->cur_w = growth->w;
                finished = 1;
            } else {
                growth->cur_w += 0x20;
            }
            if (growth->cur_h + 0x20 >= growth->h) {
                growth->cur_h = growth->h;
                finished++;
            } else {
                growth->cur_h += 0x20;
            }
            if (finished == 2) {
                growth->done = 1;
            }
            func_801C875C(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->style, growth->unk0C, growth->has_bar);
        }
    }
}

/* Project `count` quads and link their packets (every other one from `first`) into OT entry 4. */
#ifdef NON_MATCHING
void func_801C8C3C(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first) {
    s32 depth;
    s32 flag;
    POLY_FT4 *poly;
    s32 i;

    for (i = 0; i < count; i++) {
        poly = &packets[first];
        func_8004A73C(&quads[i * 4], &quads[i * 4 + 1], &quads[i * 4 + 2], &quads[i * 4 + 3],
                      &poly->x0, &poly->x1, &poly->x2, &poly->x3, &depth, &flag);
        first += 2;
        func_80043B48(D_800625A0->draw_env + 0x80, poly);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8C3C);
#endif

/* Link `count` packets (every other one from `first`) into OT entry 4. */
#ifdef NON_MATCHING
void func_801C8D58(s32 count, POLY_FT4 *packets, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        func_80043B48(D_800625A0->draw_env + 0x80, packets + first);
        first += 2;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8D58);
#endif

/* Draw the scroll bar when shown. */
void func_801C8DDC(void) {
    ScrollBar *scroll;

    if (D_800625A0->flags->scroll_shown != 0) {
        scroll = D_800625A0->scroll;
        func_801C8C3C(1, scroll->quad, scroll->sprite, scroll->buffer);
    }
}

/* Draw the cursor's label draw mode, and the cursor when shown. */
void func_801C8E28(void) {
    func_80043B48(D_800625A0->draw_env + 0x80,
                  &D_800625A0->cursor->mode_label[D_800625A0->cursor->shade_buffer]);
    if (D_800625A0->flags->unk4 != 0) {
        func_80043B48(D_800625A0->draw_env + 0x80,
                      &D_800625A0->cursor->sprite[D_800625A0->cursor->sprite_buffer]);
    }
}

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C8EB8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C908C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9260);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9434);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9608);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9744);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9890);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9AB4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9C2C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801C9F7C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA00C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA09C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA214);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA22C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA388);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA404);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CA444);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAB0C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAB80);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CABF4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAC7C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CACC8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CAED4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB014);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB13C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB2FC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB340);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB370);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB384);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB7F4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CB894);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBA50);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBB08);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBC88);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CBCF0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC024);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC278);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC54C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC720);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CC97C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCAD8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCD28);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCE1C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CCFF4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD404);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD5D0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD6F8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD7E4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CD8D0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CDBA0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CDD14);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CE480);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CE8D8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CE91C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CEB3C);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CF2A0);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CF678);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CF780);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801CFF58);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D05BC);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D0C18);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D0E68);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1658);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D18A8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D18E8);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1928);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1968);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1B18);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1CA4);

INCLUDE_ASM(".local/decomp/ovl2601/asm/nonmatchings/ovl2601", func_801D1F10);
