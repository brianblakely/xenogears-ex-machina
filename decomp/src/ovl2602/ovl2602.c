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

/* Allocate (nonzero) or release the screen flag block. */
void func_801C53A8(u8 allocate) {
    if (allocate) {
        D_800625A0->flags = func_80031BDC(sizeof(ScreenFlags), 0);
        func_8003F8E8(D_800625A0->flags, sizeof(ScreenFlags));
    } else {
        func_800320E8(D_800625A0->flags);
    }
}

/* Allocate (nonzero) or release the image packet block. */
void func_801C540C(u8 allocate) {
    if (allocate) {
        D_800625A0->images = func_80031BDC(sizeof(ImageBlock), 0);
        func_8003F8E8(D_800625A0->images, sizeof(ImageBlock));
    } else {
        func_800320E8(D_800625A0->images);
    }
}

/* Allocate (nonzero) or release the list packet block. */
void func_801C5470(u8 allocate) {
    if (allocate) {
        D_800625A0->lists = func_80031BDC(sizeof(ListBlock), 0);
        func_8003F8E8(D_800625A0->lists, sizeof(ListBlock));
    } else {
        func_800320E8(D_800625A0->lists);
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

    column = index & 1;
    line = index / 2;
    u = (line & 1) << 7;
    poly = label->poly;
    i = 0;
loop:
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
        i++;
        poly++;
    if (i < 2) {
        goto loop;
    }
    label->projected = 0;
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
    func_80026338(D_800625A0->sprite_sheet, 0xFE, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &D_800625A0->sheet_entries[1];
    func_80026338(D_800625A0->sprite_sheet, 0x103, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &D_800625A0->sheet_entries[2];
    func_80026338(D_800625A0->sprite_sheet, 0x100, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &D_800625A0->sheet_entries[3];
    func_80026338(D_800625A0->sprite_sheet, 0x101, &e->unk0, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
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
        D_800625A0->flags->cursor_shown = 1;
    }
}

/* Hide the cursor. */
void func_801C665C(void) {
    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
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
    func_801C7604(D_800625A0->scroll->quad, x, y + offset, 8, height);
    D_800625A0->scroll->buffer = D_800625A0->buffer;
    D_800625A0->flags->scroll_shown = 1;
}

/* Remove the scroll bar. */
void func_801C782C(void) {
    D_800625A0->flags->scroll_shown = 0;
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
        D_800625A0->flags->marker_shown[index] = 1;
    } else {
        D_800625A0->flags->marker_shown[index] = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C78EC);
#endif

/* Remove marker `index`. */
void func_801C7A88(u8 index) {
    func_800320E8(D_800625A0->markers[index]);
    D_800625A0->flags->marker_shown[index] = 0;
}

/* Initialise panel `index`'s background, draw modes and edge sprite parts. */
void func_801C7AE4(u8 index) {
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
void func_801C7E00(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8002675C(D_800625A0->sprite_sheet, 0x105, &panel->parts[26], D_800625A0->buffer, x, y, 0x1000);
    func_800263E4(D_800625A0->sprite_sheet, 0x105, &panel->parts[28], D_800625A0->buffer, x,
                  y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sprite_sheet, 0x106, &panel->parts[24], D_800625A0->buffer, x, y + 8,
                  0x1000);
    func_801C7604(&panel->quads[56], x, y, 8, 8);
    func_801C7604(&panel->quads[60], x, y + h, 8, -8);
    func_801C7604(&panel->quads[52], x, y + 8, 8, h - 8);
}

/* Build panel `index`'s four corners around (x, y, w, h). */
void func_801C7F64(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    func_801C7604(&panel->quads[0], x - 8, y + 8, 0x10, -0x10);
    func_801C7604(&panel->quads[4], x + w + 8, y + 8, -0x10, -0x10);
    func_801C7604(&panel->quads[8], x - 8, y + h - 8, 0x10, 0x10);
    func_801C7604(&panel->quads[12], x + w + 8, y + h - 8, -0x10, 0x10);
    for (i = 0; i < 4; i++) {
        func_801C765C(&panel->parts[i * 2 + D_800625A0->buffer]);
    }
}

/* Build panel `index`'s top edge, two pieces across the width. */
void func_801C81AC(u8 index, u16 x, u16 y, u16 w) {
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
    func_801C7604(&panel->quads[16], x + 8, y - 8, half, 0x10);
    func_801C7604(&panel->quads[20], x + (half + 8), y - 8, half, 0x10);
    for (i = 0; i < 2; i++) {
        func_801C765C(&panel->parts[i * 2 + D_800625A0->buffer + 8]);
    }
}

/* Build panel `index`'s bottom edge, two pieces across the width. */
void func_801C84F0(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    func_801C7604(&panel->quads[24], x + 8, y + h - 8, half, 0x10);
    func_801C7604(&panel->quads[28], x + (half + 8), y + h - 8, half, 0x10);
    for (i = 0; i < 2; i++) {
        func_801C765C(&panel->parts[i * 2 + D_800625A0->buffer + 12]);
    }
}

/* Build panel `index`'s left edge, two pieces down the height. */
void func_801C883C(u8 index, u16 x, u16 y, u16 h) {
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
    func_801C7604(&panel->quads[32], x - 8, y + 8, 0x10, half);
    func_801C7604(&panel->quads[36], x - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        func_801C765C(&panel->parts[i * 2 + D_800625A0->buffer + 16]);
    }
}

/* Build panel `index`'s right edge, two pieces down the height. */
void func_801C8B84(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    func_801C7604(&panel->quads[40], x + w - 8, y + 8, 0x10, half);
    func_801C7604(&panel->quads[44], x + w - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        func_801C765C(&panel->parts[i * 2 + D_800625A0->buffer + 20]);
    }
}

/* Lay out panel `index` at (x, y, w, h) for this buffer and mark it shown. */
void func_801C8ED0(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar) {
    Panel *panel;

    panel = D_800625A0->panels[index];
    D_800625A0->flags->panel_shown[index] = 0;
    func_801C7604(&panel->quads[48], x, y, w, h);
    func_801C7F64(index, x, y, w, h);
    func_801C81AC(index, x, y, w);
    func_801C84F0(index, x, y, w, h);
    func_801C883C(index, x, y, h);
    func_801C8B84(index, x, y, w, h);
    if (has_bar) {
        func_801C7E00(index, x, y, w, h);
    }
    panel->has_bar = has_bar;
    panel->flat = flat;
    panel->ot_entry = ot_entry;
    panel->buffer = D_800625A0->buffer;
    D_800625A0->flags->panel_shown[index] = 1;
}

/* Remove panel `index`. */
void func_801C9054(u8 index) {
    D_800625A0->flags->panel_shown[index] = 0;
    D_800625A0->flags->panel_growing[index] = 0;
    func_800320E8(D_800625A0->panels[index]);
    func_800320E8(D_800625A0->growth[index]);
}

/* Open panel `index` at (x, y, w, h): at once, or growing from its centre when `grow`. */
void func_801C90E0(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry,
                   u8 has_bar) {
    PanelGrowth *growth;

    if (index >= 2) {
        D_800625A0->panels[index] = func_80031BDC(sizeof(Panel), 0);
        func_8003F8E8(D_800625A0->panels[index], sizeof(Panel));
        D_800625A0->growth[index] = func_80031BDC(sizeof(PanelGrowth), 0);
        func_8003F8E8(D_800625A0->growth[index], sizeof(PanelGrowth));
        func_801C7AE4(index);
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
        growth->flat = flat;
        growth->ot_entry = ot_entry;
    } else {
        func_801C8ED0(index, x, y, w, h, flat, ot_entry, has_bar);
    }
}

/* Grow each opening panel by 20h in both directions until it reaches its size. */
void func_801C9264(void) {
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
            func_801C8ED0(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->flat, growth->ot_entry, growth->has_bar);
        }
    }
}

/* Project `count` quads and link their packets (every other one from `first`) into OT entry 4. */
void func_801C93B0(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < count; i++) {
        func_8004A73C(&quads[i * 4], &quads[i * 4 + 1], &quads[i * 4 + 2], &quads[i * 4 + 3],
                      &packets[first + i * 2].x0, &packets[first + i * 2].x1,
                      &packets[first + i * 2].x2, &packets[first + i * 2].x3, &depth, &flag);
        func_80043B48(&D_800625A0->draw_env->ot[4], &packets[first + i * 2]);
    }
}

/* Link `count` packets (every other one from `first`) into OT entry 4. */
void func_801C94CC(s32 count, POLY_FT4 *packets, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        func_80043B48(&D_800625A0->draw_env->ot[4], &packets[first + i * 2]);
    }
}

/* Draw the scroll bar when shown. */
void func_801C9550(void) {
    ScrollBar *scroll;

    if (D_800625A0->flags->scroll_shown != 0) {
        scroll = D_800625A0->scroll;
        func_801C93B0(1, scroll->quad, scroll->sprite, scroll->buffer);
    }
}

/* Draw the cursor's label draw mode, and the cursor when shown. */
void func_801C959C(void) {
    func_80043B48(&D_800625A0->draw_env->ot[4],
                  &D_800625A0->cursor->mode_label[D_800625A0->cursor->shade_buffer]);
    if (D_800625A0->flags->unk4 != 0) {
        func_80043B48(&D_800625A0->draw_env->ot[4],
                      &D_800625A0->cursor->sprite[D_800625A0->cursor->sprite_buffer]);
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801C962C);

/* Project and link panel `index`'s top edge. */
void func_801C9690(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8004A73C(&panel->quads[16], &panel->quads[17], &panel->quads[18], &panel->quads[19],
                  &panel->parts[panel->buffer + 8].x0, &panel->parts[panel->buffer + 8].x1,
                  &panel->parts[panel->buffer + 8].x2, &panel->parts[panel->buffer + 8].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 8]);
    func_8004A73C(&panel->quads[20], &panel->quads[21], &panel->quads[22], &panel->quads[23],
                  &panel->parts[panel->buffer + 10].x0, &panel->parts[panel->buffer + 10].x1,
                  &panel->parts[panel->buffer + 10].x2, &panel->parts[panel->buffer + 10].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 10]);
}

/* Project and link panel `index`'s bottom edge. */
void func_801C9864(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8004A73C(&panel->quads[24], &panel->quads[25], &panel->quads[26], &panel->quads[27],
                  &panel->parts[panel->buffer + 12].x0, &panel->parts[panel->buffer + 12].x1,
                  &panel->parts[panel->buffer + 12].x2, &panel->parts[panel->buffer + 12].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 12]);
    func_8004A73C(&panel->quads[28], &panel->quads[29], &panel->quads[30], &panel->quads[31],
                  &panel->parts[panel->buffer + 14].x0, &panel->parts[panel->buffer + 14].x1,
                  &panel->parts[panel->buffer + 14].x2, &panel->parts[panel->buffer + 14].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 14]);
}

/* Project and link panel `index`'s left edge. */
void func_801C9A38(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8004A73C(&panel->quads[32], &panel->quads[33], &panel->quads[34], &panel->quads[35],
                  &panel->parts[panel->buffer + 16].x0, &panel->parts[panel->buffer + 16].x1,
                  &panel->parts[panel->buffer + 16].x2, &panel->parts[panel->buffer + 16].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 16]);
    func_8004A73C(&panel->quads[36], &panel->quads[37], &panel->quads[38], &panel->quads[39],
                  &panel->parts[panel->buffer + 18].x0, &panel->parts[panel->buffer + 18].x1,
                  &panel->parts[panel->buffer + 18].x2, &panel->parts[panel->buffer + 18].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 18]);
}

/* Project and link panel `index`'s right edge. */
void func_801C9C0C(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8004A73C(&panel->quads[40], &panel->quads[41], &panel->quads[42], &panel->quads[43],
                  &panel->parts[panel->buffer + 20].x0, &panel->parts[panel->buffer + 20].x1,
                  &panel->parts[panel->buffer + 20].x2, &panel->parts[panel->buffer + 20].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 20]);
    func_8004A73C(&panel->quads[44], &panel->quads[45], &panel->quads[46], &panel->quads[47],
                  &panel->parts[panel->buffer + 22].x0, &panel->parts[panel->buffer + 22].x1,
                  &panel->parts[panel->buffer + 22].x2, &panel->parts[panel->buffer + 22].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 22]);
}

/* Project and link panel `index`'s background and its draw mode. */
void func_801C9DE0(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    func_8004A73C(&panel->quads[48], &panel->quads[49], &panel->quads[50], &panel->quads[51],
                  &(panel->back + panel->buffer)->x0, &(panel->back + panel->buffer)->x1,
                  &(panel->back + panel->buffer)->x2, &(panel->back + panel->buffer)->x3, &depth,
                  &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->back[panel->buffer]);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->mode[panel->buffer]);
}

/* Project and link panel `index`'s four corners. */
void func_801C9F1C(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;
    s32 i;

    panel = D_800625A0->panels[index];
    for (i = 0; i < 4; i++) {
        func_8004A73C(&panel->quads[i * 4], &panel->quads[i * 4 + 1], &panel->quads[i * 4 + 2], &panel->quads[i * 4 + 3],
                      &(panel->parts + (i * 2 + panel->buffer))->x0,
                      &(panel->parts + (i * 2 + panel->buffer))->x1,
                      &(panel->parts + (i * 2 + panel->buffer))->x2,
                      &(panel->parts + (i * 2 + panel->buffer))->x3, &depth, &flag);
        func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[i * 2 + panel->buffer]);
    }
}

/* Project and link panel `index`'s scroll bar: both arrows, then the track. */
void func_801CA068(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;
    s32 i;

    panel = D_800625A0->panels[index];
    for (i = 0; i < 2; i++) {
        func_8004A73C(&panel->quads[i * 4 + 56], &panel->quads[i * 4 + 57], &panel->quads[i * 4 + 58],
                      &panel->quads[i * 4 + 59], &panel->parts[i * 2 + panel->buffer + 26].x0, &panel->parts[i * 2 + panel->buffer + 26].x1, &panel->parts[i * 2 + panel->buffer + 26].x2, &panel->parts[i * 2 + panel->buffer + 26].x3, &depth, &flag);
        func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[i * 2 + panel->buffer + 26]);
    }
    func_8004A73C(&panel->quads[52], &panel->quads[53], &panel->quads[54], &panel->quads[55],
                  &panel->parts[panel->buffer + 24].x0, &panel->parts[panel->buffer + 24].x1,
                  &panel->parts[panel->buffer + 24].x2, &panel->parts[panel->buffer + 24].x3,
                  &depth, &flag);
    func_80043B48(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 24]);
}

/* Draw every shown panel; panels that are not flat get their own 3D matrices. */
void func_801CA28C(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    SVECTOR unused; /* never referenced; the original frame reserves it */
    Panel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->flags->panel_shown[i] != 0) {
            panel = D_800625A0->panels[i];
            if (panel->flat == 0) {
                func_8004960C();
                rotation.vz = 0;
                rotation.vy = 0;
                rotation.vx = 0;
                translation.vy = 0;
                translation.vx = 0;
                translation.vz = 0x200;
                func_8003F738(&rotation, &matrix);
                func_80049D9C(&matrix, &translation);
                func_80049EFC(&matrix);
                func_80049F8C(&matrix);
                func_801C9F1C(i);
                if (panel->has_bar) {
                    func_801CA068(i);
                }
                func_801C9690(i);
                func_801C9864(i);
                func_801C9A38(i);
                func_801C9C0C(i);
                func_801C9DE0(i);
                func_800496AC();
            } else {
                func_801C9F1C(i);
                if (panel->has_bar) {
                    func_801CA068(i);
                }
                func_801C9690(i);
                func_801C9864(i);
                func_801C9A38(i);
                func_801C9C0C(i);
                func_801C9DE0(i);
            }
        }
    }
}

/* Link the shown markers; markers that follow the file cursor move to its slot first. */
void func_801CA404(void) {
    s32 i;

    if (D_800625A0->flags->marks_shown != 0) {
        for (i = 0; i < 4; i++) {
            if (D_800625A0->marks->shown[i] != 0) {
                if (D_800625A0->marks->at_cursor[i] != 0) {
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x0 =
                        D_801D6AFC[D_801D6A84[D_800625A0->card->cursor_slot]] + 8;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y0 =
                        D_801D6B7C[D_801D6A84[D_800625A0->card->cursor_slot]] - 6;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x1 =
                        D_801D6AFC[D_801D6A84[D_800625A0->card->cursor_slot]] + 0x18;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y1 =
                        D_801D6B7C[D_801D6A84[D_800625A0->card->cursor_slot]] - 6;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x2 =
                        D_801D6AFC[D_801D6A84[D_800625A0->card->cursor_slot]] + 8;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y2 =
                        D_801D6B7C[D_801D6A84[D_800625A0->card->cursor_slot]] + 0xA;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x3 =
                        D_801D6AFC[D_801D6A84[D_800625A0->card->cursor_slot]] + 0x18;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y3 =
                        D_801D6B7C[D_801D6A84[D_800625A0->card->cursor_slot]] + 0xA;
                }
                func_80043B48(&D_800625A0->draw_env->ot[4],
                              D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]));
            }
        }
    }
}

/* Link the shown command labels. */
void func_801CA754(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->flags->label_shown[i] != 0) {
            func_80043B48(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->labels[i].poly[D_800625A0->labels[i].buffer]);
        }
    }
}

/* Link the shown list labels. */
void func_801CA7E4(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->flags->list_label_shown[i] != 0) {
            func_80043B48(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->list_labels[i].poly[D_800625A0->list_labels[i].buffer]);
        }
    }
}

/* Link the shown info labels, projecting the 3D ones first. */
void func_801CA874(void) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->info_label_shown[i] != 0) {
            if (D_800625A0->info_labels[i].projected) {
                func_8004A73C(&D_800625A0->info_labels[i].quad[0], &D_800625A0->info_labels[i].quad[1], &D_800625A0->info_labels[i].quad[2], &D_800625A0->info_labels[i].quad[3],
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x0,
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x1,
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x2,
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x3, &depth, &flag);
                func_80043B48(&D_800625A0->draw_env->ot[4], &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer]);
            } else {
                func_80043B48(&D_800625A0->draw_env->ot[4], &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer]);
            }
        }
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CA9EC);

/* Link the message labels while the message is shown, projecting the 3D ones first. */
void func_801CAA7C(void) {
    s32 depth;
    s32 flag;
    Label *label;
    s32 i;

    if (D_800625A0->flags->message_shown != 0) {
        for (i = 0; i < 3; i++) {
            label = D_800625A0->message_labels[i];
            if (label->projected) {
                func_8004A73C(&label->quad[0], &label->quad[1], &label->quad[2], &label->quad[3],
                              &label->poly[label->buffer].x0, &label->poly[label->buffer].x1,
                              &label->poly[label->buffer].x2, &label->poly[label->buffer].x3, &depth,
                              &flag);
                func_80043B48(&D_800625A0->draw_env->ot[4], &label->poly[label->buffer]);
            } else {
                func_80043B48(&D_800625A0->draw_env->ot[4], &label->poly[label->buffer]);
            }
        }
    }
}

void func_801CABD8(void) {
}

/* Link every label group. */
void func_801CABE0(void) {
    func_801CA754();
    func_801CA7E4();
    func_801CA874();
    func_801CA9EC();
    func_801CAA7C();
}

/* Link both image packet groups, first applying a changed dimming (semi-transparent, 20h grey). */
void func_801CAC20(void) {
    s32 i;

    if (D_800625A0->flags->images_shown != 0) {
        if (D_800625A0->images->dim != D_800625A0->images->dimmed) {
            if (D_800625A0->images->dim != 0) {
                for (i = 0; i < D_800625A0->images->count2; i++) {
                    func_80043BFC(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 1);
                    func_80043C24(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 0);
                    D_800625A0->images->packets2[i * 2 + D_800625A0->images->buffer2].tpage |= 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->r0 = 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->g0 = 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->b0 = 0x20;
                }
                for (i = 0; i < D_800625A0->images->count; i++) {
                    func_80043BFC(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 1);
                    func_80043C24(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 0);
                    D_800625A0->images->packets[i * 2 + D_800625A0->images->buffer].tpage |= 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->r0 = 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->g0 = 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->b0 = 0x20;
                }
            } else {
                for (i = 0; i < D_800625A0->images->count2; i++) {
                    func_80043BFC(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 0);
                    func_80043C24(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 0);
                    D_800625A0->images->packets2[i * 2 + D_800625A0->images->buffer2].tpage |= 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->r0 = 0x80;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->g0 = 0x80;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->b0 = 0x80;
                }
                for (i = 0; i < D_800625A0->images->count; i++) {
                    func_80043BFC(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 0);
                    func_80043C24(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 0);
                    D_800625A0->images->packets[i * 2 + D_800625A0->images->buffer].tpage |= 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->r0 = 0x80;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->g0 = 0x80;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->b0 = 0x80;
                }
            }
            D_800625A0->images->dimmed = D_800625A0->images->dim;
        }
        func_801C94CC(D_800625A0->images->count2, D_800625A0->images->packets2, D_800625A0->images->buffer2);
        func_801C94CC(D_800625A0->images->count, D_800625A0->images->packets, D_800625A0->images->buffer);
    }
}

/* Link both list packet groups when shown. */
void func_801CB2E8(void) {
    if (D_800625A0->flags->lists_shown != 0) {
        func_801C94CC(D_800625A0->lists->count2, D_800625A0->lists->packets2, D_800625A0->lists->buffer2);
        func_801C94CC(D_800625A0->lists->count, D_800625A0->lists->packets, D_800625A0->lists->buffer);
    }
}

/* Project and link the shown markers. */
void func_801CB35C(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (D_800625A0->flags->marker_shown[i] != 0) {
            func_801C93B0(1, D_800625A0->markers[i]->quad, D_800625A0->markers[i]->sprite,
                          D_800625A0->markers[i]->buffer);
        }
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB3D0);

/* Play menu sound `sound` of the effect bank when sounds are on. */
void func_801CB498(u8 sound) {
    if (D_800625A0->sounds != 0) {
        func_80039DB8((D_800625A0->effect_bank->id << 16) | sound);
    }
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB4E4);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CB690);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBA2C);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBDA0);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CBE60);

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC1C4);

/* Create the marker block: both yes/no markers at the cursor (0), the four markers (2) or one (3). */
void func_801CC31C(u8 mode) {
    s32 i;

    D_800625A0->marks = func_80031BDC(sizeof(MarkerBlock), 0);
    func_8003F8E8(D_800625A0->marks, sizeof(MarkerBlock));
    switch (mode) {
    case 0:
        D_800625A0->flags->marks_shown = 1;
        D_800625A0->marks->at_cursor[0] = 1;
        D_800625A0->marks->at_cursor[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sprite_sheet, 0x108, &D_800625A0->marks->packets[i * 2],
                          D_800625A0->buffer, D_801D6A60[i], D_801D6A70[i], 0x800);
            D_800625A0->marks->buffer[i] = D_800625A0->buffer;
        }
        break;
    case 3:
        func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->marks->packets, D_800625A0->buffer,
                      0, 0, 0x800);
        D_800625A0->marks->buffer[0] = D_800625A0->buffer;
        D_800625A0->flags->marks_shown = 1;
        break;
    case 1:
        break;
    }
}

/* Hide the markers, let a frame pass, and release them. */
void func_801CC4DC(void) {
    D_800625A0->flags->marks_shown = 0;
    func_801CC1C4();
    func_800320E8(D_800625A0->marks);
}

void func_801CC520(void) {
}

void func_801CC528(void) {
}

INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/ovl2602", func_801CC530);

/* Close the message panel and release its labels, then let a frame pass. */
void func_801CC9A0(void) {
    s32 i;

    if (D_800625A0->flags->panel_shown[4] != 0) {
        func_801C9054(4);
        D_800625A0->flags->message_shown = 0;
        for (i = 0; i < 4; i++) {
            func_800320E8(D_800625A0->message_labels[i]);
        }
    }
    D_800625A0->flags->unk5B = 0;
    func_801CC1C4();
}

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
