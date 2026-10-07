/*
 * ovl2601 (Disc 1 slot 2601, Disc 2 slot 2596; loaded at 801c5000): the item
 * shop screen. Its entry (801ccd28) builds its own menu state blocks and
 * resources, then runs a three-command screen: leave, sell (four lists: a
 * member's equipment and the three inventories) and buy (the shop's weapons,
 * armour and items, chosen by the shop number 80059171, with prices, the
 * party's gold and the members who can use each). It is built from the menu's
 * shared screen code: the 3D panels, labels, cursor and yes/no prompt, and
 * the card state block whose save header it still fills in, with the file
 * name prefix "BISLPS-00800" (unused by the shop itself; ovl2600 and ovl2602
 * carry the same string).
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
        bzero(D_800625A0->card, sizeof(CardState));
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate (nonzero) or release the screen flag block. */
void func_801C51F8(u8 allocate) {
    if (allocate) {
        D_800625A0->flags = func_80031BDC(sizeof(ScreenFlags), 0);
        bzero(D_800625A0->flags, sizeof(ScreenFlags));
    } else {
        func_800320E8(D_800625A0->flags);
    }
}

/* Allocate (nonzero) or release the image packet block. */
void func_801C525C(u8 allocate) {
    if (allocate) {
        D_800625A0->images = func_80031BDC(sizeof(ImageBlock), 0);
        bzero(D_800625A0->images, sizeof(ImageBlock));
    } else {
        func_800320E8(D_800625A0->images);
    }
}

/* Allocate (nonzero) or release the list packet block. */
void func_801C52C0(u8 allocate) {
    if (allocate) {
        D_800625A0->lists = func_80031BDC(sizeof(ListBlock), 0);
        bzero(D_800625A0->lists, sizeof(ListBlock));
    } else {
        func_800320E8(D_800625A0->lists);
    }
}

/* Allocate (nonzero) or release the unpacked resource table. */
void func_801C5324(u8 allocate) {
    if (allocate) {
        D_800625A0->resources = func_80031BDC(0xCC, 0);
        bzero(D_800625A0->resources, 0xCC);
    } else {
        func_800320E8(D_800625A0->resources);
    }
}

/* Allocate (nonzero) or release the cursor block. */
void func_801C5388(u8 allocate) {
    if (allocate) {
        D_800625A0->cursor = func_80031BDC(sizeof(CursorBlock), 0);
        bzero(D_800625A0->cursor, sizeof(CursorBlock));
    } else {
        func_800320E8(D_800625A0->cursor);
    }
}

/* Allocate (nonzero) or release the block at menu state +1e20. */
void func_801C53EC(u8 allocate) {
    if (allocate) {
        D_800625A0->unk1E20 = func_80031BDC(0xDEC, 0);
        bzero(D_800625A0->unk1E20, 0xDEC);
    } else {
        func_800320E8(D_800625A0->unk1E20);
    }
}

/* Allocate (nonzero) or release the shop screen's packet block. */
void func_801C5450(u8 allocate) {
    if (allocate) {
        D_800625A0->details = func_80031BDC(sizeof(DetailBlock), 0);
        bzero(D_800625A0->details, sizeof(DetailBlock));
    } else {
        func_800320E8(D_800625A0->details);
    }
}

/* Load the screen's resources: the card header (prefix, icon), text images, sprite sheet, labels, the party's portraits, the sound bank and the shop tables. */
void func_801C54B4(void) {
    enum {
        ENTRY_UNUSED, ENTRY_MODE, ENTRY_CLUT_X, ENTRY_CLUT_Y,
        ENTRY_PAGE_X, ENTRY_PAGE_Y, ENTRY_WORDS
    };
    TIM_IMAGE tim;
    s32 entries[3 * ENTRY_WORDS];
    MenuResources *res;
    u32 *packed;
    s32 i;
    s32 id;

    res = D_8005945C;
    func_8003342C(res);
    packed = func_80032E88(res->files[0], 1);
    OpenTIM(packed);
    ReadTIM(&D_800625A0->card->icon);
    *(CardPrefix *)D_800625A0->card->game_prefix = D_801C5000;
    D_800625A0->card->save_magic[0] = 'S';
    D_800625A0->card->save_magic[1] = 'C';
    D_800625A0->card->save_icon_flag = 0x11;
    D_800625A0->card->save_blocks = 1;
    bzero(D_800625A0->card->save_title, sizeof(D_800625A0->card->save_title));
    memmove(D_800625A0->card->save_palette, D_800625A0->card->icon.caddr, 0x20);
    i = 0;
    memmove(D_800625A0->card->save_icon, D_800625A0->card->icon.paddr, 0x80);
    func_800320E8(packed);
    packed = func_80032E88(res->files[1], 1);
    func_8002DD20(packed);
    func_800320E8(packed);
    D_800625A0->sprite_sheet = func_80032E88(res->files[2], 0);
    D_800625A0->label_text = func_80032E88(res->files[3], 0);
    func_80026338(D_800625A0->sprite_sheet, 0x14B,
                  &entries[ENTRY_UNUSED], &entries[ENTRY_MODE],
                  &entries[ENTRY_CLUT_X], &entries[ENTRY_CLUT_Y],
                  &entries[ENTRY_PAGE_X], &entries[ENTRY_PAGE_Y]);
    func_80026338(D_800625A0->sprite_sheet, 0x14C,
                  &entries[ENTRY_WORDS + ENTRY_UNUSED], &entries[ENTRY_WORDS + ENTRY_MODE],
                  &entries[ENTRY_WORDS + ENTRY_CLUT_X], &entries[ENTRY_WORDS + ENTRY_CLUT_Y],
                  &entries[ENTRY_WORDS + ENTRY_PAGE_X], &entries[ENTRY_WORDS + ENTRY_PAGE_Y]);
    func_80026338(D_800625A0->sprite_sheet, 0x14D,
                  &entries[2 * ENTRY_WORDS + ENTRY_UNUSED], &entries[2 * ENTRY_WORDS + ENTRY_MODE],
                  &entries[2 * ENTRY_WORDS + ENTRY_CLUT_X], &entries[2 * ENTRY_WORDS + ENTRY_CLUT_Y],
                  &entries[2 * ENTRY_WORDS + ENTRY_PAGE_X], &entries[2 * ENTRY_WORDS + ENTRY_PAGE_Y]);
    entries[ENTRY_WORDS + ENTRY_PAGE_X] += 0xC;
    packed = func_80032E88(res->files[4], 1);
    for (; i < 3; i++) {
        id = D_800625A0->flags->members[i];
        if (id != 0xFF) {
            OpenTIM((u8 *)packed + id * 0xB20);
            ReadTIM(&tim);
            tim.crect->x = entries[i * ENTRY_WORDS + ENTRY_CLUT_X];
            tim.crect->y = entries[i * ENTRY_WORDS + ENTRY_CLUT_Y];
            tim.prect->x = entries[i * ENTRY_WORDS + ENTRY_PAGE_X];
            tim.prect->y = entries[i * ENTRY_WORDS + ENTRY_PAGE_Y];
            LoadImage(tim.crect, tim.caddr);
            LoadImage(tim.prect, tim.paddr);
        }
    }
    DrawSync(0);
    func_800320E8(packed);
    if (D_80059178 != 0) {
        func_80028470(0x10, 2);
        D_8006259C = func_80031BDC(func_800288EC(5), 0);
        func_800295D8(5, D_8006259C, 0, 0x80);
        func_80028A60(0);
        func_80028470(0x10, 0);
        func_80038428(D_8006259C);
    }
    D_800625A0->effect_bank = D_8006259C;
    D_800625A0->unk1E2C = func_80032E88(res->files[6], 0);
    func_800320E8(res);
}

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

    i = 0;
    column = index & 1;
    line = index / 2;
    u = (line & 1) << 7;
    poly = label->poly;
loop:
        semi = 0;
        SetPolyFT4(poly);
        SetSemiTrans(poly, 0);
        SetShadeTex(poly, 0);
        poly->r0 = 0x80;
        poly->g0 = 0x80;
        poly->b0 = 0x80;
        if ((u8)mode == 0) {
            label->highlight = column;
            poly->tpage = GetTPage(0, 0, 0x140, 0);
            poly->u0 = u;
            poly->v0 = ((index + row) / 4) * 13;
            poly->u1 = u + label->width;
            poly->v1 = ((index + row) / 4) * 13;
            poly->u2 = u;
            poly->v2 = ((index + row) / 4) * 13 + 13;
            poly->u3 = u + label->width;
            poly->v3 = ((index + row) / 4) * 13 + 13;
        } else {
            if (!(mode & 0x80)) {
                semi = 0x20;
                SetSemiTrans(poly, 1);
                poly->r0 = semi;
                poly->g0 = semi;
                poly->b0 = semi;
            }
            label->highlight = (u8)(mode & 0x7F) - 1;
            poly->tpage = semi | GetTPage(0, 0, 0x180, 0x80);
            poly->u0 = column * 0x60;
            poly->v0 = line * 13 + row;
            poly->u1 = column * 0x60 + label->width;
            poly->v1 = line * 13 + row;
            poly->u2 = column * 0x60;
            poly->v2 = line * 13 + row + 13;
            poly->u3 = column * 0x60 + label->width;
            poly->v3 = line * 13 + row + 13;
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
        LoadImage(rect, D_800625A0->labels[0].pixels);
        DrawSync(0);
    }
}

/* Upload a 16-colour palette with only colour 1 set (7fff, white) at (0, 1c0). */
void func_801C5E6C(void) {
    RECT rect;
    RECT unused; /* unused in the original; reserves 8 bytes */
    u16 *palette;

    palette = func_80031BDC(0x20, 0);
    bzero(palette, 0x20);
    palette[1] = 0x7FFF;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.x = 0;
    rect.h = 1;
    LoadImage(&rect, palette);
    DrawSync(0);
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
    s32 unused[10]; /* unused in the original; reserves 40 bytes */
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
    SetPolyG4(poly);
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
        SetSemiTrans(&D_800625A0->cursor->shade[i], 1);
        SetLineF3(&D_800625A0->cursor->upper[i]);
        (D_800625A0->cursor->upper + i)->r0 = 0;
        (D_800625A0->cursor->upper + i)->g0 = 0x40;
        (D_800625A0->cursor->upper + i)->b0 = 0;
        SetLineF3(&D_800625A0->cursor->lower[i]);
        (D_800625A0->cursor->lower + i)->r0 = 0;
        (D_800625A0->cursor->lower + i)->g0 = 0x40;
        (D_800625A0->cursor->lower + i)->b0 = 0;
        SetPolyF4(&D_800625A0->cursor->screen[i]);
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
        SetSemiTrans(&D_800625A0->cursor->screen[i], 1);
        SetDrawMode(&D_800625A0->cursor->mode_label[i], 0, 0, GetTPage(0, 0, 0x140, 0x80),
                      &window);
        SetDrawMode(&D_800625A0->cursor->mode_sprite[i], 0, 0, GetTPage(0, 2, 0x180, 0),
                      &window);
    }
}

/* Unpack (mode 0) or release (mode 10h) the item tables and pictures from file 2. */
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
        D_800625A0->resources->equipment = func_80032E88(list[2], 0);
        D_800625A0->resources->accessories = func_80032E88(list[3], 0);
        D_800625A0->resources->items = func_80032E88(list[1], 0);
        D_800625A0->details->resources[1] = func_80032E88(list[0x28], 0);
        D_800625A0->details->resources[2] = func_80032E88(list[0x29], 0);
        D_800625A0->details->resources[0] = func_80032E88(list[0x2A], 0);
        break;
    case 0x10:
        func_800320E8(D_800625A0->resources->equipment);
        func_800320E8(D_800625A0->resources->accessories);
        func_800320E8(D_800625A0->resources->items);
        func_800320E8(D_800625A0->details->resources[1]);
        func_800320E8(D_800625A0->details->resources[2]);
        func_800320E8(D_800625A0->details->resources[0]);
        break;
    }
    if (mode < 0x10) {
        func_800320E8(list);
    }
}

/* Collect the shop's items (id and kind), unpack the item tables and set up the bars and frame lines. */
void func_801C6A6C(void) {
    u8 *entry;
    s32 j;
    s32 i;

    j = 0;
    entry = D_800625A0->unk1E2C + D_80059171 * 0x5C;
    for (i = 0; i < 0x30; i++) {
        D_800625A0->shop_items[i] = 0;
        D_800625A0->shop_kinds[i] = 0;
    }
    for (i = 0; i < 0x5A; i++) {
        if (entry[i] != 0) {
            D_800625A0->shop_items[j] = entry[i];
            D_800625A0->shop_kinds[j] = i / 30;
            j++;
        }
    }
    D_801D1F50 = j;
    func_801C6828(0);
    for (i = 0; i < 9; i++) {
        for (j = 0; j < 2; j++) {
            SetLineF3(D_800625A0->details->bar_upper + (i * 2 + j));
            (D_800625A0->details->bar_upper + (i * 2 + j))->r0 = 0xFF;
            (D_800625A0->details->bar_upper + (i * 2 + j))->g0 = 0;
            (D_800625A0->details->bar_upper + (i * 2 + j))->b0 = 0;
            SetLineF3(D_800625A0->details->bar_lower + (i * 2 + j));
            (D_800625A0->details->bar_lower + (i * 2 + j))->r0 = 0xFF;
            (D_800625A0->details->bar_lower + (i * 2 + j))->g0 = 0;
            (D_800625A0->details->bar_lower + (i * 2 + j))->b0 = 0;
            (D_800625A0->details->bar_upper + (i * 2 + j))->x0 = D_801D21CC[i];
            (D_800625A0->details->bar_upper + (i * 2 + j))->y0 = 0xA6;
            (D_800625A0->details->bar_upper + (i * 2 + j))->x1 = D_801D21CC[i] + 0x18;
            (D_800625A0->details->bar_upper + (i * 2 + j))->y1 = 0xA6;
            (D_800625A0->details->bar_upper + (i * 2 + j))->x2 = D_801D21CC[i] + 0x18;
            (D_800625A0->details->bar_upper + (i * 2 + j))->y2 = 0xBC;
            (D_800625A0->details->bar_lower + (i * 2 + j))->x0 = D_801D21CC[i];
            (D_800625A0->details->bar_lower + (i * 2 + j))->y0 = 0xA6;
            (D_800625A0->details->bar_lower + (i * 2 + j))->x1 = D_801D21CC[i];
            (D_800625A0->details->bar_lower + (i * 2 + j))->y1 = 0xBC;
            (D_800625A0->details->bar_lower + (i * 2 + j))->x2 = D_801D21CC[i] + 0x18;
            (D_800625A0->details->bar_lower + (i * 2 + j))->y2 = 0xBC;
        }
        D_800625A0->details->bar_shown[i] = 0;
    }
    for (j = 0; j < 2; j++) {
        SetLineF2(&D_800625A0->details->frame[j]);
        (D_800625A0->details->frame + j)->r0 = 0xFF;
        (D_800625A0->details->frame + j)->g0 = 0xFF;
        (D_800625A0->details->frame + j)->b0 = 0xFF;
        (D_800625A0->details->frame + j)->x0 = D_801D2250 - 8;
        (D_800625A0->details->frame + j)->y0 = D_801D2254 + 9;
        (D_800625A0->details->frame + j)->x1 = D_801D2250 + 0x4E;
        (D_800625A0->details->frame + j)->y1 = D_801D2254 + 9;
    }
}

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
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
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
        bzero(D_800625A0->scroll, sizeof(ScrollBar));
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
    bzero(D_800625A0->markers[index], sizeof(Marker));
    D_800625A0->markers[index]->frame = 4;
    D_800625A0->markers[index]->timer = 0;
}

/* Animate marker `index` beside row `row`. Both shop callers pass fixed = 0;
 * the original leaves the row offset uninitialized for a nonzero fixed value. */
void func_801C7178(s32 row, s32 unused, u8 fixed, u8 index) {
    Marker *marker;
    POLY_FT4 *poly;
    s32 visible;
    s32 y;

    marker = D_800625A0->markers[index];
    if (++marker->timer >= 6) {
        if (--marker->frame < 0) {
            marker->frame = 4;
        }
        marker->timer = 0;
    }
    if (fixed) {
        visible = 1;
    } else {
        y = row * 13 + 0x32;
        visible = 1;
    }
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
        SetPolyG4(&panel->back[i]);
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
        SetSemiTrans(&panel->back[i], 1);
        SetDrawMode(&panel->mode[i], 0, 0,
                      GetTPage(0, 0, D_800625A0->sheet_entries[0].page_x, D_800625A0->sheet_entries[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->parts[i + 8]);
        SetShadeTex(&panel->parts[i + 8], 1);
        (panel->parts + i + 8)->r0 = 0xFF;
        (panel->parts + i + 8)->g0 = 0xFF;
        (panel->parts + i + 8)->b0 = 0xFF;
        (panel->parts + i + 8)->tpage = GetTPage(D_800625A0->sheet_entries[0].mode, 0,
                                                      D_800625A0->sheet_entries[0].page_x,
                                                      D_800625A0->sheet_entries[0].page_y);
        (panel->parts + i + 8)->clut = GetClut(D_800625A0->sheet_entries[0].clut_x,
                                                     D_800625A0->sheet_entries[0].clut_y);
        SetPolyFT4(&panel->parts[i + 12]);
        SetShadeTex(&panel->parts[i + 12], 1);
        (panel->parts + i + 12)->r0 = 0xFF;
        (panel->parts + i + 12)->g0 = 0xFF;
        (panel->parts + i + 12)->b0 = 0xFF;
        (panel->parts + i + 12)->tpage = GetTPage(D_800625A0->sheet_entries[1].mode, 0,
                                                       D_800625A0->sheet_entries[1].page_x,
                                                       D_800625A0->sheet_entries[1].page_y);
        (panel->parts + i + 12)->clut = GetClut(D_800625A0->sheet_entries[1].clut_x,
                                                      D_800625A0->sheet_entries[1].clut_y);
        SetPolyFT4(&panel->parts[i + 16]);
        SetShadeTex(&panel->parts[i + 16], 1);
        (panel->parts + i + 16)->r0 = 0xFF;
        (panel->parts + i + 16)->g0 = 0xFF;
        (panel->parts + i + 16)->b0 = 0xFF;
        (panel->parts + i + 16)->tpage = GetTPage(D_800625A0->sheet_entries[2].mode, 0,
                                                       D_800625A0->sheet_entries[2].page_x,
                                                       D_800625A0->sheet_entries[2].page_y);
        (panel->parts + i + 16)->clut = GetClut(D_800625A0->sheet_entries[2].clut_x,
                                                      D_800625A0->sheet_entries[2].clut_y);
        SetPolyFT4(&panel->parts[i + 20]);
        SetShadeTex(&panel->parts[i + 20], 1);
        (panel->parts + i + 20)->r0 = 0xFF;
        (panel->parts + i + 20)->g0 = 0xFF;
        (panel->parts + i + 20)->b0 = 0xFF;
        (panel->parts + i + 20)->tpage = GetTPage(D_800625A0->sheet_entries[3].mode, 0,
                                                       D_800625A0->sheet_entries[3].page_x,
                                                       D_800625A0->sheet_entries[3].page_y);
        (panel->parts + i + 20)->clut = GetClut(D_800625A0->sheet_entries[3].clut_x,
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
    func_801C6E90(&panel->quads[56], x, y, 8, 8);
    func_801C6E90(&panel->quads[60], x, y + h, 8, -8);
    func_801C6E90(&panel->quads[52], x, y + 8, 8, h - 8);
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
    func_801C6E90(&panel->quads[0], x - 8, y + 8, 0x10, -0x10);
    func_801C6E90(&panel->quads[4], x + w + 8, y + 8, -0x10, -0x10);
    func_801C6E90(&panel->quads[8], x - 8, y + h - 8, 0x10, 0x10);
    func_801C6E90(&panel->quads[12], x + w + 8, y + h - 8, -0x10, 0x10);
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
    func_801C6E90(&panel->quads[16], x + 8, y - 8, half, 0x10);
    func_801C6E90(&panel->quads[20], x + (half + 8), y - 8, half, 0x10);
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
    func_801C6E90(&panel->quads[24], x + 8, y + h - 8, half, 0x10);
    func_801C6E90(&panel->quads[28], x + (half + 8), y + h - 8, half, 0x10);
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
    func_801C6E90(&panel->quads[32], x - 8, y + 8, 0x10, half);
    func_801C6E90(&panel->quads[36], x - 8, y + (half + 8), 0x10, half);
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
    func_801C6E90(&panel->quads[40], x + w - 8, y + 8, 0x10, half);
    func_801C6E90(&panel->quads[44], x + w - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        func_801C6EE8(&panel->parts[i * 2 + D_800625A0->buffer + 20]);
    }
}

/* Lay out panel `index` at (x, y, w, h) for this buffer and mark it shown. */
void func_801C875C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar) {
    Panel *panel;

    panel = D_800625A0->panels[index];
    D_800625A0->flags->panel_shown[index] = 0;
    func_801C6E90(&panel->quads[48], x, y, w, h);
    func_801C77F0(index, x, y, w, h);
    func_801C7A38(index, x, y, w);
    func_801C7D7C(index, x, y, w, h);
    func_801C80C8(index, x, y, h);
    func_801C8410(index, x, y, w, h);
    if (has_bar) {
        func_801C768C(index, x, y, w, h);
    }
    panel->has_bar = has_bar;
    panel->flat = flat;
    panel->ot_entry = ot_entry;
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
void func_801C896C(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry,
                   u8 has_bar) {
    PanelGrowth *growth;

    if (index >= 2) {
        D_800625A0->panels[index] = func_80031BDC(sizeof(Panel), 0);
        bzero(D_800625A0->panels[index], sizeof(Panel));
        D_800625A0->growth[index] = func_80031BDC(sizeof(PanelGrowth), 0);
        bzero(D_800625A0->growth[index], sizeof(PanelGrowth));
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
        growth->flat = flat;
        growth->ot_entry = ot_entry;
    } else {
        func_801C875C(index, x, y, w, h, flat, ot_entry, has_bar);
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
                          growth->cur_h, growth->flat, growth->ot_entry, growth->has_bar);
        }
    }
}

/* Project `count` quads and link their packets (every other one from `first`) into OT entry 4. */
void func_801C8C3C(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < count; i++) {
        RotTransPers4(&quads[i * 4], &quads[i * 4 + 1], &quads[i * 4 + 2], &quads[i * 4 + 3],
                      &packets[first + i * 2].x0, &packets[first + i * 2].x1,
                      &packets[first + i * 2].x2, &packets[first + i * 2].x3, &depth, &flag);
        AddPrim(&D_800625A0->draw_env->ot[4], &packets[first + i * 2]);
    }
}

/* Link `count` packets (every other one from `first`) into OT entry 4. */
void func_801C8D58(s32 count, POLY_FT4 *packets, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(&D_800625A0->draw_env->ot[4], &packets[first + i * 2]);
    }
}

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
    AddPrim(&D_800625A0->draw_env->ot[4],
                  &D_800625A0->cursor->mode_label[D_800625A0->cursor->shade_buffer]);
    if (D_800625A0->flags->unk4 != 0) {
        AddPrim(&D_800625A0->draw_env->ot[4],
                      &D_800625A0->cursor->sprite[D_800625A0->cursor->sprite_buffer]);
    }
}

/* Project and link panel `index`'s top edge. */
void func_801C8EB8(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    RotTransPers4(&panel->quads[16], &panel->quads[17], &panel->quads[18], &panel->quads[19],
                  &panel->parts[panel->buffer + 8].x0, &panel->parts[panel->buffer + 8].x1,
                  &panel->parts[panel->buffer + 8].x2, &panel->parts[panel->buffer + 8].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 8]);
    RotTransPers4(&panel->quads[20], &panel->quads[21], &panel->quads[22], &panel->quads[23],
                  &panel->parts[panel->buffer + 10].x0, &panel->parts[panel->buffer + 10].x1,
                  &panel->parts[panel->buffer + 10].x2, &panel->parts[panel->buffer + 10].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 10]);
}

/* Project and link panel `index`'s bottom edge. */
void func_801C908C(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    RotTransPers4(&panel->quads[24], &panel->quads[25], &panel->quads[26], &panel->quads[27],
                  &panel->parts[panel->buffer + 12].x0, &panel->parts[panel->buffer + 12].x1,
                  &panel->parts[panel->buffer + 12].x2, &panel->parts[panel->buffer + 12].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 12]);
    RotTransPers4(&panel->quads[28], &panel->quads[29], &panel->quads[30], &panel->quads[31],
                  &panel->parts[panel->buffer + 14].x0, &panel->parts[panel->buffer + 14].x1,
                  &panel->parts[panel->buffer + 14].x2, &panel->parts[panel->buffer + 14].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 14]);
}

/* Project and link panel `index`'s left edge. */
void func_801C9260(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    RotTransPers4(&panel->quads[32], &panel->quads[33], &panel->quads[34], &panel->quads[35],
                  &panel->parts[panel->buffer + 16].x0, &panel->parts[panel->buffer + 16].x1,
                  &panel->parts[panel->buffer + 16].x2, &panel->parts[panel->buffer + 16].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 16]);
    RotTransPers4(&panel->quads[36], &panel->quads[37], &panel->quads[38], &panel->quads[39],
                  &panel->parts[panel->buffer + 18].x0, &panel->parts[panel->buffer + 18].x1,
                  &panel->parts[panel->buffer + 18].x2, &panel->parts[panel->buffer + 18].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 18]);
}

/* Project and link panel `index`'s right edge. */
void func_801C9434(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    RotTransPers4(&panel->quads[40], &panel->quads[41], &panel->quads[42], &panel->quads[43],
                  &panel->parts[panel->buffer + 20].x0, &panel->parts[panel->buffer + 20].x1,
                  &panel->parts[panel->buffer + 20].x2, &panel->parts[panel->buffer + 20].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 20]);
    RotTransPers4(&panel->quads[44], &panel->quads[45], &panel->quads[46], &panel->quads[47],
                  &panel->parts[panel->buffer + 22].x0, &panel->parts[panel->buffer + 22].x1,
                  &panel->parts[panel->buffer + 22].x2, &panel->parts[panel->buffer + 22].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 22]);
}

/* Project and link panel `index`'s background and its draw mode. */
void func_801C9608(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;

    panel = D_800625A0->panels[index];
    RotTransPers4(&panel->quads[48], &panel->quads[49], &panel->quads[50], &panel->quads[51],
                  &(panel->back + panel->buffer)->x0, &(panel->back + panel->buffer)->x1,
                  &(panel->back + panel->buffer)->x2, &(panel->back + panel->buffer)->x3, &depth,
                  &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->back[panel->buffer]);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->mode[panel->buffer]);
}

/* Project and link panel `index`'s four corners. */
void func_801C9744(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;
    s32 i;

    panel = D_800625A0->panels[index];
    for (i = 0; i < 4; i++) {
        RotTransPers4(&panel->quads[i * 4], &panel->quads[i * 4 + 1], &panel->quads[i * 4 + 2], &panel->quads[i * 4 + 3],
                      &(panel->parts + (i * 2 + panel->buffer))->x0,
                      &(panel->parts + (i * 2 + panel->buffer))->x1,
                      &(panel->parts + (i * 2 + panel->buffer))->x2,
                      &(panel->parts + (i * 2 + panel->buffer))->x3, &depth, &flag);
        AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[i * 2 + panel->buffer]);
    }
}

/* Project and link panel `index`'s scroll bar: both arrows, then the track. */
void func_801C9890(s32 index) {
    s32 depth;
    s32 flag;
    Panel *panel;
    s32 i;

    panel = D_800625A0->panels[index];
    for (i = 0; i < 2; i++) {
        RotTransPers4(&panel->quads[i * 4 + 56], &panel->quads[i * 4 + 57], &panel->quads[i * 4 + 58],
                      &panel->quads[i * 4 + 59], &panel->parts[i * 2 + panel->buffer + 26].x0, &panel->parts[i * 2 + panel->buffer + 26].x1, &panel->parts[i * 2 + panel->buffer + 26].x2, &panel->parts[i * 2 + panel->buffer + 26].x3, &depth, &flag);
        AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[i * 2 + panel->buffer + 26]);
    }
    RotTransPers4(&panel->quads[52], &panel->quads[53], &panel->quads[54], &panel->quads[55],
                  &panel->parts[panel->buffer + 24].x0, &panel->parts[panel->buffer + 24].x1,
                  &panel->parts[panel->buffer + 24].x2, &panel->parts[panel->buffer + 24].x3,
                  &depth, &flag);
    AddPrim(&D_800625A0->draw_env->ot[panel->ot_entry], &panel->parts[panel->buffer + 24]);
}

/* Draw every shown panel; panels that are not flat get their own 3D matrices. */
void func_801C9AB4(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    Panel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->flags->panel_shown[i] != 0) {
            panel = D_800625A0->panels[i];
            if (panel->flat == 0) {
                PushMatrix();
                rotation.vz = 0;
                rotation.vy = 0;
                rotation.vx = 0;
                translation.vy = 0;
                translation.vx = 0;
                translation.vz = 0x200;
                func_8003F738(&rotation, &matrix);
                TransMatrix(&matrix, &translation);
                SetRotMatrix(&matrix);
                SetTransMatrix(&matrix);
                func_801C9744(i);
                if (panel->has_bar) {
                    func_801C9890(i);
                }
                func_801C8EB8(i);
                func_801C908C(i);
                func_801C9260(i);
                func_801C9434(i);
                func_801C9608(i);
                PopMatrix();
            } else {
                func_801C9744(i);
                if (panel->has_bar) {
                    func_801C9890(i);
                }
                func_801C8EB8(i);
                func_801C908C(i);
                func_801C9260(i);
                func_801C9434(i);
                func_801C9608(i);
            }
        }
    }
}

/* Link the shown markers; markers that follow the file cursor move to its slot first. */
void func_801C9C2C(void) {
    s32 i;

    if (D_800625A0->flags->marks_shown != 0) {
        for (i = 0; i < 4; i++) {
            if (D_800625A0->marks->shown[i] != 0) {
                if (D_800625A0->marks->at_cursor[i] != 0) {
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x0 =
                        D_801D2094[D_801D201C[D_800625A0->card->cursor_slot]] + 8;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y0 =
                        D_801D2114[D_801D201C[D_800625A0->card->cursor_slot]] - 6;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x1 =
                        D_801D2094[D_801D201C[D_800625A0->card->cursor_slot]] + 0x18;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y1 =
                        D_801D2114[D_801D201C[D_800625A0->card->cursor_slot]] - 6;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x2 =
                        D_801D2094[D_801D201C[D_800625A0->card->cursor_slot]] + 8;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y2 =
                        D_801D2114[D_801D201C[D_800625A0->card->cursor_slot]] + 0xA;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->x3 =
                        D_801D2094[D_801D201C[D_800625A0->card->cursor_slot]] + 0x18;
                    (D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]))->y3 =
                        D_801D2114[D_801D201C[D_800625A0->card->cursor_slot]] + 0xA;
                }
                AddPrim(&D_800625A0->draw_env->ot[4],
                              D_800625A0->marks->packets + (i * 2 + D_800625A0->marks->buffer[i]));
            }
        }
    }
}

/* Link the shown command labels. */
void func_801C9F7C(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->flags->label_shown[i] != 0) {
            AddPrim(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->labels[i].poly[D_800625A0->labels[i].buffer]);
        }
    }
}

/* Link the shown list labels. */
void func_801CA00C(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->flags->list_label_shown[i] != 0) {
            AddPrim(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->list_labels[i].poly[D_800625A0->list_labels[i].buffer]);
        }
    }
}

/* Link the shown info labels, projecting the 3D ones first. */
void func_801CA09C(void) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->info_label_shown[i] != 0) {
            if (D_800625A0->info_labels[i].projected) {
                RotTransPers4(&D_800625A0->info_labels[i].quad[0], &D_800625A0->info_labels[i].quad[1], &D_800625A0->info_labels[i].quad[2], &D_800625A0->info_labels[i].quad[3],
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x0,
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x1,
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x2,
                              &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer].x3, &depth, &flag);
                AddPrim(&D_800625A0->draw_env->ot[4], &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer]);
            } else {
                AddPrim(&D_800625A0->draw_env->ot[4], &D_800625A0->info_labels[i].poly[D_800625A0->info_labels[i].buffer]);
            }
        }
    }
}

/* An empty loop over six entries; whatever it drew was removed. */
void func_801CA214(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
    }
}

/* Link the message labels while the message is shown, projecting the 3D ones first. */
void func_801CA22C(void) {
    s32 depth;
    s32 flag;
    Label *label;
    s32 i;

    if (D_800625A0->flags->message_shown != 0) {
        for (i = 0; i < 3; i++) {
            label = D_800625A0->message_labels[i];
            if (label->projected) {
                RotTransPers4(&label->quad[0], &label->quad[1], &label->quad[2], &label->quad[3],
                              &label->poly[label->buffer].x0, &label->poly[label->buffer].x1,
                              &label->poly[label->buffer].x2, &label->poly[label->buffer].x3, &depth,
                              &flag);
                AddPrim(&D_800625A0->draw_env->ot[4], &label->poly[label->buffer]);
            } else {
                AddPrim(&D_800625A0->draw_env->ot[4], &label->poly[label->buffer]);
            }
        }
    }
}

/* Link the full-screen quad and its draw mode into OT entry 8. */
void func_801CA388(void) {
    AddPrim(&D_800625A0->draw_env->ot[8], D_800625A0->cursor->screen + D_800625A0->buffer);
    AddPrim(&D_800625A0->draw_env->ot[8], D_800625A0->cursor->mode_sprite + D_800625A0->buffer);
}

/* Link every label group. */
void func_801CA404(void) {
    func_801C9F7C();
    func_801CA00C();
    func_801CA09C();
    func_801CA214();
    func_801CA22C();
}

/* Link both image packet groups, first applying a changed dimming (semi-transparent, 20h grey). */
void func_801CA444(void) {
    s32 i;

    if (D_800625A0->flags->images_shown != 0) {
        if (D_800625A0->images->dim != D_800625A0->images->dimmed) {
            if (D_800625A0->images->dim != 0) {
                for (i = 0; i < D_800625A0->images->count2; i++) {
                    SetSemiTrans(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 1);
                    SetShadeTex(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 0);
                    D_800625A0->images->packets2[i * 2 + D_800625A0->images->buffer2].tpage |= 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->r0 = 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->g0 = 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->b0 = 0x20;
                }
                for (i = 0; i < D_800625A0->images->count; i++) {
                    SetSemiTrans(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 1);
                    SetShadeTex(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 0);
                    D_800625A0->images->packets[i * 2 + D_800625A0->images->buffer].tpage |= 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->r0 = 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->g0 = 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->b0 = 0x20;
                }
            } else {
                for (i = 0; i < D_800625A0->images->count2; i++) {
                    SetSemiTrans(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 0);
                    SetShadeTex(D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2), 0);
                    D_800625A0->images->packets2[i * 2 + D_800625A0->images->buffer2].tpage |= 0x20;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->r0 = 0x80;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->g0 = 0x80;
                    (D_800625A0->images->packets2 + (i * 2 + D_800625A0->images->buffer2))->b0 = 0x80;
                }
                for (i = 0; i < D_800625A0->images->count; i++) {
                    SetSemiTrans(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 0);
                    SetShadeTex(D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer), 0);
                    D_800625A0->images->packets[i * 2 + D_800625A0->images->buffer].tpage |= 0x20;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->r0 = 0x80;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->g0 = 0x80;
                    (D_800625A0->images->packets + (i * 2 + D_800625A0->images->buffer))->b0 = 0x80;
                }
            }
            D_800625A0->images->dimmed = D_800625A0->images->dim;
        }
        func_801C8D58(D_800625A0->images->count2, D_800625A0->images->packets2, D_800625A0->images->buffer2);
        func_801C8D58(D_800625A0->images->count, D_800625A0->images->packets, D_800625A0->images->buffer);
    }
}

/* Link both list packet groups when shown. */
void func_801CAB0C(void) {
    if (D_800625A0->flags->lists_shown != 0) {
        func_801C8D58(D_800625A0->lists->count2, D_800625A0->lists->packets2, D_800625A0->lists->buffer2);
        func_801C8D58(D_800625A0->lists->count, D_800625A0->lists->packets, D_800625A0->lists->buffer);
    }
}

/* Project and link the shown markers. */
void func_801CAB80(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (D_800625A0->flags->marker_shown[i] != 0) {
            func_801C8C3C(1, D_800625A0->markers[i]->quad, D_800625A0->markers[i]->sprite,
                          D_800625A0->markers[i]->buffer);
        }
    }
}

/* Build the frame's packets: every element while the screen is drawn, then the screen quad. */
void func_801CABF4(void) {
    if (D_800625A0->drawing != 0) {
        func_801C8AF0();
        func_801C9C2C();
        func_801CA404();
        func_801C8E28();
        func_801CAB80();
        func_801CCFF4();
        func_801C8DDC();
        func_801CA444();
        func_801CAB0C();
        func_801C9AB4();
    }
    func_801CA388();
}

/* Play menu sound `sound` of the effect bank when sounds are on. */
void func_801CAC7C(u8 sound) {
    if (D_800625A0->sounds != 0) {
        func_80039DB8((D_800625A0->effect_bank->id << 16) | sound);
    }
}

/* Wait for a controller (sound paused meanwhile), then decode the frame's input into +325. */
void func_801CACC8(void) {
    s32 saved;
    u8 waiting;
    u8 paused;
    u8 code;

    code = 8;
    waiting = 1;
    paused = 0;
    do {
        if (func_80035734(0) == 0) {
            if (!paused) {
                paused++;
                func_80037EE4();
                saved = D_80059488;
            }
        } else {
            waiting--;
            if (paused) {
                func_80037E8C();
                D_80059488 = saved;
            }
        }
    } while (waiting);
    if (func_80036410() != 0) {
        func_80035DB0();
    } else {
        while (func_80035CDC() != 0) {
            if (D_800594A4 & 0x2000) {
                code = 0;
                func_801CAC7C(1);
                break;
            } else if (D_800594A4 & 0x4000) {
                code = 1;
                func_801CAC7C(1);
                break;
            } else if (D_800594A4 & 0x8000) {
                code = 2;
                func_801CAC7C(1);
                break;
            } else if (D_800594A4 & 0x1000) {
                code = 3;
                func_801CAC7C(1);
                break;
            } else if (D_8005948C & 0x20) {
                code = 4;
                break;
            } else if (D_8005948C & 0x40) {
                code = 5;
                func_801CAC7C(3);
                break;
            } else if (D_8005948C & 0x80) {
                code = 6;
                break;
            } else if (D_8005948C & 0x10) {
                code = 7;
                break;
            } else if (D_8005948C & 4) {
                code = 0xA;
                break;
            } else if (D_8005948C & 8) {
                code = 9;
                break;
            } else if (D_8005948C & 0x800) {
                code = 0xB;
                break;
            } else if (D_8005948C & 0x100) {
                code = 0xC;
                D_800625A0->select_toggle = D_800625A0->select_toggle == 0;
                break;
            } else if (D_8005948C & 1) {
                D_800625A0->unk1E95++;
                break;
            }
        }
    }
    D_800625A0->input = code;
}

/* Advance the view's zoom (+329) and load the view matrices. */
void func_801CAED4(void) {
    switch (D_800625A0->view_motion) {
    case 4:
        D_800625A0->view_translation.vz = 0x200;
        D_800625A0->view_rotation.vz = 0;
        D_800625A0->view_rotation.vy = 0;
        D_800625A0->view_rotation.vx = 0;
        D_800625A0->view_translation.vy = 0;
        D_800625A0->view_translation.vx = 0;
        D_800625A0->view_motion = 2;
        break;
    case 3:
        D_800625A0->view_translation.vz = 0x800;
        D_800625A0->view_rotation.vz = 0;
        D_800625A0->view_rotation.vy = 0;
        D_800625A0->view_rotation.vx = 0;
        D_800625A0->view_translation.vy = 0;
        D_800625A0->view_translation.vx = 0;
        D_800625A0->view_motion = 1;
        break;
    case 2:
        D_800625A0->view_rotation.vy -= 0x60;
        D_800625A0->view_translation.vz += 0x40;
        if (D_800625A0->view_translation.vz >= 0xE00) {
            D_800625A0->view_motion = 0;
        }
        break;
    case 1:
        D_800625A0->view_rotation.vx += 0x7C;
        D_800625A0->view_translation.vz -= 0x30;
        if (D_800625A0->view_translation.vz < 0x200) {
            D_800625A0->view_translation.vz = 0x200;
            D_800625A0->view_rotation.vz = 0;
            D_800625A0->view_rotation.vx = 0;
            D_800625A0->view_rotation.vy = 0;
            D_800625A0->view_motion = 0;
        }
        break;
    }
    func_8003F738(&D_800625A0->view_rotation, &D_800625A0->view_matrix);
    TransMatrix(&D_800625A0->view_matrix, &D_800625A0->view_translation);
    SetRotMatrix(&D_800625A0->view_matrix);
    SetTransMatrix(&D_800625A0->view_matrix);
}

/* Run one frame: input, buffer swap, view, packets, then present the finished buffer. */
void func_801CB014(void) {
    s32 shown;

    if (*D_8005917C != -1) {
        __asm__ volatile("break 1024");
    }
    func_801CACC8();
    func_80019CA0();
    D_800625A0->draw_env =
        D_800625A0->draw_env == &D_800625A0->envs[0] ? &D_800625A0->envs[1] : &D_800625A0->envs[0];
    D_800625A0->buffer = D_800625A0->buffer == 0;
    ClearOTagR(D_800625A0->draw_env->ot, 16);
    func_801CAED4();
    func_801CABF4();
    shown = D_800625A0->buffer == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(D_800625A0->draw_env->draw);
    PutDispEnv(D_800625A0->draw_env->disp);
    MoveImage(&D_800625A0->images->screen, 0, shown * 224);
    DrawOTag(&D_800625A0->draw_env->ot[15]);
}

/* Create the marker block: both yes/no markers at the cursor (0), the four markers (2) or one (3). */
void func_801CB13C(u8 mode) {
    s32 i;

    D_800625A0->marks = func_80031BDC(sizeof(MarkerBlock), 0);
    bzero(D_800625A0->marks, sizeof(MarkerBlock));
    switch (mode) {
    case 0:
        D_800625A0->flags->marks_shown = 1;
        D_800625A0->marks->at_cursor[0] = 1;
        D_800625A0->marks->at_cursor[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sprite_sheet, 0x108, &D_800625A0->marks->packets[i * 2],
                          D_800625A0->buffer, D_801D1FF8[i], D_801D2008[i], 0x800);
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
void func_801CB2FC(void) {
    D_800625A0->flags->marks_shown = 0;
    func_801CB014();
    func_800320E8(D_800625A0->marks);
}

/* Start zooming the view out, with its sound. */
void func_801CB340(void) {
    D_800625A0->view_motion = 3;
    func_801CAC7C(0x5B);
}

/* Start zooming the view in. */
void func_801CB370(void) {
    D_800625A0->view_motion = 4;
}

/* Open the message panel and show three lines of label text from entry `first`. */
void func_801CB384(u8 first) {
    PanelGrowth *growth;
    Label *label;
    s32 i;
    s32 x;

    x = 0x50;
    func_801C896C(4, 0x42, 0x46, 0xBC, 0x40, 1, 1, 4, 0);
    growth = D_800625A0->growth[4];
    while (growth->done == 0) {
        func_801CB014();
    }
    for (i = 0; i < 4; i++) {
        D_800625A0->message_labels[i] = func_80031BDC(sizeof(Label), 0);
        bzero(D_800625A0->message_labels[i], sizeof(Label));
        if (!(i & 1)) {
            D_800625A0->message_labels[i]->pixels = func_80031BDC(0x5CA, 0);
            D_800625A0->message_labels[i]->rect.x = 0x140;
            D_800625A0->message_labels[i]->rect.y = (i / 2) * 13 + 0x4E;
            D_800625A0->message_labels[i]->rect.w = 0x3A;
            D_800625A0->message_labels[i]->rect.h = 13;
        } else {
            D_800625A0->message_labels[i]->pixels = D_800625A0->message_labels[i - 1]->pixels;
        }
    }
    i = 0;
    do {
        label = D_800625A0->message_labels[i];
        label->width = func_80034EAC(func_80033728(D_800625A0->label_text, first + i), label->pixels,
                                     0x36, i % 2);
        func_801C5A7C(label, i, 0, 0);
        func_801C6E90(label->quad, x, i * 16 + 0x50, label->width, 13);
        (label->poly + D_800625A0->buffer)->u0 = 0;
        (label->poly + D_800625A0->buffer)->v0 = (i / 2) * 13 + 0x4E;
        (label->poly + D_800625A0->buffer)->u1 = label->width;
        (label->poly + D_800625A0->buffer)->v1 = (i / 2) * 13 + 0x4E;
        (label->poly + D_800625A0->buffer)->u2 = 0;
        (label->poly + D_800625A0->buffer)->v2 = (i / 2) * 13 + 0x5B;
        (label->poly + D_800625A0->buffer)->u3 = label->width;
        (label->poly + D_800625A0->buffer)->v3 = (i / 2) * 13 + 0x5B;
        label->buffer = D_800625A0->buffer;
        label->projected = 1;
        i++;
    } while (i < 3);
    LoadImage(&D_800625A0->message_labels[0]->rect, D_800625A0->message_labels[0]->pixels);
    LoadImage(&D_800625A0->message_labels[2]->rect, D_800625A0->message_labels[2]->pixels);
    DrawSync(0);
    D_800625A0->flags->message_shown = 1;
    func_800320E8(D_800625A0->message_labels[0]->pixels);
    func_800320E8(D_800625A0->message_labels[2]->pixels);
    if (D_800625A0->flags->unk5B == 2) {
        D_800625A0->flags->unk5B = 1;
    }
    func_801CB014();
    func_801CB014();
}

/* Close the message panel and release its labels, then let a frame pass. */
void func_801CB7F4(void) {
    s32 i;

    if (D_800625A0->flags->panel_shown[4] != 0) {
        func_801C88E0(4);
        D_800625A0->flags->message_shown = 0;
        for (i = 0; i < 4; i++) {
            func_800320E8(D_800625A0->message_labels[i]);
        }
    }
    D_800625A0->flags->unk5B = 0;
    func_801CB014();
}

/* Let the player choose yes or no (1 = yes); without `wait` the choice ends after 60 idle frames. */
u8 func_801CB894(u8 wait) {
    u8 choosing;
    u8 yes;
    u8 timer;

    choosing = 1;
    yes = 0;
    timer = 60;
    while (choosing) {
        if (!wait) {
            D_800625A0->marks->shown[2] = 0;
            D_800625A0->marks->shown[3] = 0;
            if (D_800625A0->input != 8) {
                break;
            }
            if (--timer == 0) {
                break;
            }
        }
        func_801CB014();
        switch (D_800625A0->input) {
        case 4:
            func_801CAC7C(2);
            choosing = 0;
            break;
        case 5:
            yes = 0;
            choosing = 0;
            break;
        case 2:
            D_800625A0->marks->shown[2] = 1;
            yes = 1;
            D_800625A0->marks->shown[3] = 0;
            break;
        case 0:
            D_800625A0->marks->shown[2] = 0;
            yes = 0;
            D_800625A0->marks->shown[3] = 1;
            break;
        }
    }
    D_800625A0->marks->shown[2] = 0;
    D_800625A0->marks->shown[3] = 0;
    return yes;
}

/* Ask message `message` as a yes/no question; a yes is confirmed by `confirm` unless it is ff. */
u8 func_801CBA50(u8 message, u8 confirm, u8 wait) {
    u8 answer;

    func_801CB384(message);
    D_800625A0->marks->shown[3] = 1;
    answer = func_801CB894(wait);
    func_801CB7F4();
    if (confirm != 0xFF && answer) {
        func_801CB384(confirm);
        D_800625A0->marks->shown[3] = 1;
        answer = func_801CB894(wait);
        func_801CB7F4();
    }
    return answer;
}

/* Close the screen: stop drawing, release every block and resource, then the menu state itself. */
void func_801CBB08(void) {
    func_801CB014();
    func_801CB014();
    D_800625A0->drawing = 0;
    func_801CB014();
    do {
        func_801CB014();
    } while (D_800625A0->buffer != 0);
    func_801C5194(0);
    func_801C51F8(0);
    func_801C525C(0);
    func_801C52C0(0);
    func_801C5324(0);
    func_801C5388(0);
    func_801C6828(0x10);
    func_801C5450(0);
    func_800320E8(D_800625A0->sprite_sheet);
    func_800320E8(D_800625A0->label_text);
    func_800320E8(D_800625A0->labels[0].pixels);
    if (D_80059178 != 0) {
        func_8003A094(D_800625A0->effect_bank);
        func_801CB014();
        func_8003852C(D_800625A0->effect_bank);
        func_801CB014();
        func_800320E8(D_800625A0->effect_bank);
    }
    func_800320E8(D_800625A0->unk1E2C);
    func_801C53EC(0);
    func_800320E8(D_800625A0);
}

/* Render `count` labels (render != 0) or clear their shown flags. */
void func_801CBC88(u8 render, u8 count, Label *labels, u8 *text_ids, u8 *shown) {
    s32 i;

    if (render) {
        func_801C5CBC(labels, text_ids, 2, count);
    } else {
        for (i = 0; i < count; i++) {
            shown[i] = 0;
        }
    }
}

/* Show label `index`: in list row `row` (mode 0, offset by its column) or at the info position (mode 1). */
void func_801CBCF0(u8 count, Label *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row,
                   u8 mode) {
    switch (mode) {
    case 0:
        func_801CBC88(0, count, labels, text_ids, shown);
        (labels[index].poly + D_800625A0->buffer)->x0 = D_801D2194[row + index] + 0x16 + offsets[index];
        (labels[index].poly + D_800625A0->buffer)->y0 = D_801D21B0[row + index] - 0x22;
        (labels[index].poly + D_800625A0->buffer)->x1 =
            labels[index].width + (D_801D2194[row + index] + 0x16 + offsets[index]);
        (labels[index].poly + D_800625A0->buffer)->y1 = D_801D21B0[row + index] - 0x22;
        (labels[index].poly + D_800625A0->buffer)->x2 = D_801D2194[row + index] + 0x16 + offsets[index];
        (labels[index].poly + D_800625A0->buffer)->y2 = D_801D21B0[row + index] - 0x15;
        (labels[index].poly + D_800625A0->buffer)->x3 =
            labels[index].width + (D_801D2194[row + index] + 0x16 + offsets[index]);
        (labels[index].poly + D_800625A0->buffer)->y3 = D_801D21B0[row + index] - 0x15;
        break;
    case 1:
        (labels[index].poly + D_800625A0->buffer)->x0 = 0xEC;
        (labels[index].poly + D_800625A0->buffer)->y0 = 0x7E;
        (labels[index].poly + D_800625A0->buffer)->x1 = labels->width + 0xEC;
        (labels[index].poly + D_800625A0->buffer)->y1 = 0x7E;
        (labels[index].poly + D_800625A0->buffer)->x2 = 0xEC;
        (labels[index].poly + D_800625A0->buffer)->y2 = 0x8B;
        (labels[index].poly + D_800625A0->buffer)->x3 = labels->width + 0xEC;
        (labels[index].poly + D_800625A0->buffer)->y3 = 0x8B;
        break;
    }
    labels[index].buffer = D_800625A0->buffer;
    shown[index] = 1;
}

/* Reveal `count` image pairs one step at a time (two frames each), the second of each pair one step behind. */
void func_801CC024(s32 count, s32 *ids) {
    s32 step;
    s32 i;

    D_800625A0->images->dim = 0;
    D_800625A0->images->dimmed = 0;
    D_800625A0->flags->images_shown = 1;
    for (step = 1; step <= count; step++) {
        if (step != count) {
            D_800625A0->images->count = 0;
            for (i = 0; i < step; i++) {
                D_800625A0->images->count +=
                    func_8002675C(D_800625A0->sprite_sheet, ids[i * 2],
                                  D_800625A0->images->packets + D_800625A0->images->count * 2,
                                  D_800625A0->buffer, 0xA0, 0x96, 0x1000);
            }
            D_800625A0->images->buffer = D_800625A0->buffer;
        }
        D_800625A0->images->count2 = 0;
        if (step != 1) {
            for (i = 0; i < step - 1; i++) {
                D_800625A0->images->count2 +=
                    func_8002675C(D_800625A0->sprite_sheet, ids[i * 2 + 1],
                                  D_800625A0->images->packets2 + D_800625A0->images->count2 * 2,
                                  D_800625A0->buffer, 0xA0, 0x96, 0x1000);
            }
            D_800625A0->images->buffer2 = D_800625A0->buffer;
        }
        for (i = 0; i < 2; i++) {
            func_801CB014();
        }
    }
}

/* Reveal the list pictures of the current command (up to four pairs), two frames per step. */
void func_801CC278(u8 menu) {
    s32 animate;
    s32 step;
    s32 i;
    s32 command;
    s32 *ids;
    s32 *paired;

    D_800625A0->images->dim = 0;
    animate = 1;
    step = 1;
    D_800625A0->images->dimmed = 0;
    command = menu;
    D_800625A0->lists->count = 0;
    ids = D_801D1F6C;
    D_800625A0->lists->count2 = 0;
    paired = ids + 1;
    D_800625A0->flags->lists_shown = 1;
    for (; step < 5; step++) {
        D_800625A0->lists->count = 0;
        D_800625A0->list_count = 0;
        for (i = 0; i < step; i++) {
            s32 offset;

            offset = i * 2;
            offset += (command + D_800625A0->top_cursor) * 8;
            if (ids[offset] != 0xFFFF) {
                D_800625A0->lists->count +=
                    func_8002675C(D_800625A0->sprite_sheet, ids[offset],
                                  D_800625A0->lists->packets + D_800625A0->lists->count * 2,
                                  D_800625A0->buffer, 0xA0, 0x96, 0x1000);
                D_800625A0->list_count++;
            } else {
                animate = 0;
            }
        }
        D_800625A0->lists->buffer = D_800625A0->buffer;
        if (animate) {
            for (i = 0; i < 2; i++) {
                func_801CB014();
            }
        }
        D_800625A0->lists->count2 = 0;
        for (i = 0; i < step; i++) {
            s32 offset;

            offset = i * 2;
            offset += (command + D_800625A0->top_cursor) * 8;
            if (ids[offset] != 0xFFFF) {
                D_800625A0->lists->count2 += func_8002675C(
                    D_800625A0->sprite_sheet, paired[offset],
                    D_800625A0->lists->packets2 + D_800625A0->lists->count2 * 2, D_800625A0->buffer, 0xA0,
                    0x96, 0x1000);
            }
        }
        D_800625A0->lists->buffer2 = D_800625A0->buffer;
        if (animate) {
            for (i = 0; i < 2; i++) {
                func_801CB014();
            }
        }
    }
}

/* Draw `count` image pairs with pair `selected` highlighted (+0dh), and put the cursor on it. */
void func_801CC54C(u8 count, u8 selected, s32 *ids) {
    s32 id;
    s32 i;

    D_800625A0->images->count = 0;
    D_800625A0->images->count2 = 0;
    for (i = 0; i < count; i++) {
        if (i == selected) {
            id = ids[i * 2] + 0xD;
        } else {
            id = ids[i * 2];
        }
        D_800625A0->images->count +=
            func_8002675C(D_800625A0->sprite_sheet, id,
                          D_800625A0->images->packets + D_800625A0->images->count * 2, D_800625A0->buffer,
                          0xA0, 0x96, 0x1000);
        D_800625A0->images->count2 +=
            func_8002675C(D_800625A0->sprite_sheet, ids[i * 2 + 1],
                          D_800625A0->images->packets2 + D_800625A0->images->count2 * 2,
                          D_800625A0->buffer, 0xA0, 0x96, 0x1000);
    }
    D_800625A0->images->buffer = D_800625A0->buffer;
    D_800625A0->images->buffer2 = D_800625A0->buffer;
    func_801C604C(selected, 1);
    D_800625A0->flags->unk4 = 1;
}

/* Draw the current command's list pictures with the chosen one highlighted (+0dh), and put the cursor on it. */
void func_801CC720(u8 menu) {
    s32 id;
    s32 i;

    D_800625A0->lists->count = 0;
    D_800625A0->lists->count2 = 0;
    for (i = 0; i < D_800625A0->list_count; i++) {
        if (i == D_800625A0->list_cursor) {
            id = D_801D1F6C[(menu + D_800625A0->top_cursor) * 8 + i * 2] + 0xD;
        } else {
            id = D_801D1F6C[(menu + D_800625A0->top_cursor) * 8 + i * 2];
        }
        D_800625A0->lists->count +=
            func_8002675C(D_800625A0->sprite_sheet, id,
                          D_800625A0->lists->packets + D_800625A0->lists->count * 2, D_800625A0->buffer,
                          0xA0, 0x96, 0x1000);
        D_800625A0->lists->count2 += func_8002675C(
            D_800625A0->sprite_sheet, D_801D1F6C[(menu + D_800625A0->top_cursor) * 8 + i * 2 + 1],
            D_800625A0->lists->packets2 + D_800625A0->lists->count2 * 2, D_800625A0->buffer, 0xA0, 0x96,
            0x1000);
    }
    D_800625A0->lists->buffer = D_800625A0->buffer;
    D_800625A0->lists->buffer2 = D_800625A0->buffer;
    func_801C604C(D_800625A0->list_cursor + 3, 1);
    D_800625A0->flags->unk4 = 1;
}

/* Run the chosen top command (0 leaves); afterwards restore the command screen. Returns 0 to leave. */
u8 func_801CC97C(void) {
    u8 running;
    u8 redraw;

    running = 1;
    switch (D_800625A0->top_cursor) {
    case 0:
        running = 0;
        break;
    case 1:
        redraw = func_801D1CA4();
        break;
    case 2:
        redraw = func_801CF780();
        break;
    }
    if (redraw) {
        func_801CB370();
        func_801CBC88(1, 4, D_800625A0->list_labels, D_801D1FCC, D_800625A0->flags->list_label_shown);
    }
    func_801D1F10();
    D_800625A0->images->dim = 0;
    D_800625A0->images->dimmed = 1;
    D_800625A0->flags->unk4 = 1;
    D_800625A0->flags->cursor_shown = 1;
    D_800625A0->unk337 = 0xFF;
    D_800625A0->flags->lists_shown = 0;
    return running;
}

/* The command screen: move between leave, sell and buy and run the chosen one until leaving. */
void func_801CCAD8(void) {
    u8 running;

    running = 1;
    D_800625A0->top_cursor = 2;
    func_801CC024(4, D_801D1F54);
    func_801CBC88(1, 4, D_800625A0->list_labels, D_801D1FCC, D_800625A0->flags->list_label_shown);
    do {
        func_801CB014();
        switch (D_800625A0->input) {
        case 4:
            func_801CAC7C(2);
            D_800625A0->images->dim = 1;
            func_801C6430();
            func_801CBC88(0, 4, D_800625A0->list_labels, D_801D1FCC, D_800625A0->flags->list_label_shown);
            D_800625A0->cursor->width = 0x4C;
            running = func_801CC97C();
            D_800625A0->cursor->width = 0x40;
            break;
        case 5:
            running = 0;
            break;
        case 1:
            if (D_800625A0->top_cursor != 0) {
                D_800625A0->top_cursor--;
            } else {
                D_800625A0->top_cursor = 2;
            }
            break;
        case 3:
            if (++D_800625A0->top_cursor >= 3) {
                D_800625A0->top_cursor = 0;
            }
            break;
        }
        if (D_800625A0->top_cursor != D_800625A0->unk337) {
            func_801CC54C(3, D_800625A0->top_cursor, D_801D1F54);
            func_801CBCF0(4, D_800625A0->list_labels, D_801D1FCC, D_801D1FD8,
                          D_800625A0->flags->list_label_shown, D_800625A0->top_cursor, 0, 0);
            D_800625A0->unk337 = D_800625A0->top_cursor;
        }
    } while (running);
}

/* Overlay entry: build the card screen, run it, and tear it down. */
void func_801CCD28(void) {
    func_801C5194(1);
    func_801C51F8(1);
    func_801C525C(1);
    func_801C52C0(1);
    func_801C5324(1);
    func_801C5388(1);
    func_801C53EC(1);
    func_801C5450(1);
    D_800625A0->images->screen.x = 0x2C0;
    D_800625A0->images->screen.y = 0x100;
    D_800625A0->images->screen.w = 0x140;
    D_800625A0->images->screen.h = 0xE0;
    D_800625A0->cursor->width = 0x40;
    func_801C58F4();
    func_801C5A6C();
    func_801C5EE8();
    func_801C64DC();
    func_801C5F44();
    func_801C6A6C();
    D_800625A0->drawing = 1;
    D_800625A0->sounds = 1;
    func_801CCAD8();
    func_801CBB08();
}
