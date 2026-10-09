/* Overlay 2600 (Disc 1 slot 2600; loaded at 0x801c5000): the character name
 * entry screen. A character grid (D_801CBEC0) is walked with the cursor, the
 * name is built as text codes and decoded for display, and the result is
 * stored in the character name table at 8006d634 + id * 0x14. The three
 * party portraits are loaded for the screen. Much of the drawing/list code is
 * the same as overlay 2598 (the party screen) but compiled into this image. */
#include "common.h"
#include "name_entry.h"

/* The four cursor markers' home positions. */
s32 D_801CBEA0[4] = {0, 0, 180, 276}; /* x */
s32 D_801CBEB0[4] = {0, 0, 200, 200}; /* y */

/* The name entry grid: 36 entries of six text codes (five shown, then 0x0F
 * or 0xFF). */
u8 D_801CBEC0[216] = {
    0x20, 0x21, 0x22, 0x23, 0x24, 0x0F, 0x34, 0x35, 0x36, 0x37, 0x38, 0x0F,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x0F, 0x51, 0x52, 0x53, 0x54, 0x55, 0x0F,
    0x3A, 0x3B, 0x3C, 0x61, 0x62, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F, 0x25, 0x26, 0x27, 0x28, 0x29, 0x0F,
    0x39, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x42, 0x43, 0x44, 0x45, 0x46, 0x0F,
    0x56, 0x0F, 0xCF, 0xCF, 0xCF, 0xCF, 0x63, 0x64, 0x65, 0xCF, 0xCF, 0xCF,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x0F,
    0x47, 0x48, 0x49, 0x4A, 0x4B, 0x0F, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x0F,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF,
    0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0xFF,
    0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0xFF, 0xCF, 0xCF, 0xCF, 0xCF, 0x1F, 0xFF,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xC0, 0xCF, 0xCF, 0xCF, 0x1F, 0xFF,
};

/* The four command labels' text ids. */
u8 D_801CBF98[4] = {9, 10, 11, 12};

/* File cursor -> slot (slots 15 and 31 are skipped). */
s32 D_801CBF9C[30] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};
/* Slot positions. */
s32 D_801CC014[32] = { /* x */
    32, 40, 48, 56, 64, 72, 80, 88,
    96, 104, 112, 120, 128, 136, 144, 320,
    176, 184, 192, 200, 208, 216, 224, 232,
    240, 248, 256, 264, 272, 280, 288, 320,
};
s32 D_801CC094[32] = { /* y */
    14, 34, 54, 14, 34, 54, 14, 34,
    54, 14, 34, 54, 14, 34, 54, 256,
    14, 34, 54, 14, 34, 54, 14, 34,
    54, 14, 34, 54, 14, 34, 54, 256,
};

/* Each character's bit in the party flags. */
u16 D_801CC114[16] = {
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
};

/* Test character `index`'s bit (table D_801CC114) in `flags`. */
s32 func_801C5040(s32 flags, u8 index) {
    return D_801CC114[index] & flags;
}

/* Allocate (nonzero) or release the 0x5034-byte work block. */
void func_801C505C(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->card = block;
        bzero(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate (nonzero) or release the menu flag block. */
void func_801C50C0(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6C, 0);
        D_800625A0->flags = block;
        bzero(block, 0x6C);
    } else {
        func_800320E8(D_800625A0->flags);
    }
}

/* Allocate (nonzero) or release the 0x1194-byte block at state + 0x350. */
void func_801C5124(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->images = block;
        bzero(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->images);
    }
}

/* Allocate (nonzero) or release the 0x140C-byte block at state + 0x354. */
void func_801C5188(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140C, 0);
        D_800625A0->lists = block;
        bzero(block, 0x140C);
    } else {
        func_800320E8(D_800625A0->lists);
    }
}

/* Allocate (nonzero) or release the 0xCC-byte block at state + 0x330. */
void func_801C51EC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xCC, 0);
        D_800625A0->tables = block;
        bzero(block, 0xCC);
    } else {
        func_800320E8(D_800625A0->tables);
    }
}

/* Allocate (nonzero) or release the 0x15C-byte block at state + 0x348. */
void func_801C5250(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15C, 0);
        D_800625A0->prims = block;
        bzero(block, 0x15C);
    } else {
        func_800320E8(D_800625A0->prims);
    }
}

/* Allocate (nonzero) or release the name entry block. */
void func_801C52B4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xDEC, 0);
        D_800625A0->name_entry = block;
        bzero(block, 0xDEC);
    } else {
        func_800320E8(D_800625A0->name_entry);
    }
}

/* Load the screen's resources: the card icon TIM and file name into the work
 * block's save header, the palette data, sprite sheet and label texts, the
 * named character's entry length and three portraits (uploaded to the
 * portrait sprites' VRAM), and the menu sound bank when sound is on. */
void func_801C5318(void) {
    enum {
        ENTRY_UNUSED, ENTRY_MODE, ENTRY_CLUT_X, ENTRY_CLUT_Y,
        ENTRY_PAGE_X, ENTRY_PAGE_Y, ENTRY_WORDS
    };
    TIM_IMAGE tim;
    s32 entries[3 * ENTRY_WORDS];
    MenuResources *archive = D_8005945C;
    u8 *data;
    u8 use_table = 1;
    s32 i;
    u8 id;

    func_8003342C(archive);
    data = func_80032E88(archive->files[0], 1);
    OpenTIM(data);
    ReadTIM(&D_800625A0->card->tim);
    strcpy(D_800625A0->card->file_name, "BISLPS-00800");
    D_800625A0->card->magic[0] = 'S';
    D_800625A0->card->magic[1] = 'C';
    D_800625A0->card->icon_type = 0x11;
    D_800625A0->card->blocks = 1;
    bzero(D_800625A0->card->title, 0x5C);
    memmove(D_800625A0->card->clut, D_800625A0->card->tim.caddr, 0x20);
    memmove(D_800625A0->card->icon, D_800625A0->card->tim.paddr, 0x80);
    func_800320E8(data);
    data = func_80032E88(archive->files[1], 1);
    func_8002DD20(data);
    func_800320E8(data);
    D_800625A0->sheet = func_80032E88(archive->files[2], 0);
    D_800625A0->label_text = func_80032E88(archive->files[3], 0);
    func_80026338(D_800625A0->sheet, 0x14B,
                  &entries[ENTRY_UNUSED], &entries[ENTRY_MODE],
                  &entries[ENTRY_CLUT_X], &entries[ENTRY_CLUT_Y],
                  &entries[ENTRY_PAGE_X], &entries[ENTRY_PAGE_Y]);
    func_80026338(D_800625A0->sheet, 0x14C,
                  &entries[ENTRY_WORDS + ENTRY_UNUSED], &entries[ENTRY_WORDS + ENTRY_MODE],
                  &entries[ENTRY_WORDS + ENTRY_CLUT_X], &entries[ENTRY_WORDS + ENTRY_CLUT_Y],
                  &entries[ENTRY_WORDS + ENTRY_PAGE_X], &entries[ENTRY_WORDS + ENTRY_PAGE_Y]);
    func_80026338(D_800625A0->sheet, 0x14D,
                  &entries[2 * ENTRY_WORDS + ENTRY_UNUSED], &entries[2 * ENTRY_WORDS + ENTRY_MODE],
                  &entries[2 * ENTRY_WORDS + ENTRY_CLUT_X], &entries[2 * ENTRY_WORDS + ENTRY_CLUT_Y],
                  &entries[2 * ENTRY_WORDS + ENTRY_PAGE_X], &entries[2 * ENTRY_WORDS + ENTRY_PAGE_Y]);
    D_800625A0->portrait_table = func_80032E88(archive->files[5], 1);
    if (D_80059171 < 11) {
        D_800625A0->name_entry->max_length = 9;
        D_800625A0->flags->party[0] = D_80059171;
    } else {
        D_800625A0->name_entry->max_length = 10;
        if (D_80059171 - 11 == 9 && (D_8006D634.joined & 0x400)) {
            D_800625A0->flags->party[0] = 10;
            use_table = 0;
        }
        if (use_table) {
            D_800625A0->flags->party[0] = D_800625A0->portrait_table[D_80059171 - 11];
        }
    }
    D_800625A0->portraits[0] = D_800625A0->portrait_table[D_80059171 * 3 + 0x20];
    D_800625A0->portraits[1] = D_800625A0->portrait_table[D_80059171 * 3 + 0x21];
    D_800625A0->portraits[2] = D_800625A0->portrait_table[D_80059171 * 3 + 0x22];
    i = 0;
    func_800320E8(D_800625A0->portrait_table);
    entries[ENTRY_WORDS + ENTRY_PAGE_X] += 12;
    data = func_80032E88(archive->files[4], 1);
    for (; i < 3; i++) {
        id = D_800625A0->flags->party[i];
        if (id != 0xFF) {
            OpenTIM(data + id * 0xB20);
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
    func_800320E8(data);
    if (D_80059178) {
        func_80028470(0x10, 2);
        D_8006259C = func_80031BDC(func_800288EC(5), 0);
        func_800295D8(5, D_8006259C, 0, 0x80);
        func_80028A60(0);
        func_80028470(0x10, 0);
        func_80038428(D_8006259C);
    }
    D_800625A0->effects = D_8006259C;
    func_800320E8(archive);
}

/* Reset the screen state, mark which characters may join, take the current
 * party (members that may not join become empty) and load the resources. */
void func_801C58B8(void) {
    s32 i;
    u16 flags;
    s32 id;

    D_800625A0->cursor = 4;
    D_800625A0->cursor_shown = 0xFF;
    D_800625A0->card_poll_timer = 60;
    D_800625A0->cards_present = 0;
    D_800625A0->unknown335 = 0;
    flags = D_8006D634.joined & D_8006D634.available & 0x7FF;
    for (i = 0; i < 16; i++) {
        if (func_801C5040(flags, i) & 0xFFFF) {
            D_800625A0->present[i] = 1;
        } else {
            D_800625A0->present[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = D_8006D634.party[i];
        if (id != 0xFF && D_800625A0->present[id]) {
            D_800625A0->flags->party[i] = id;
        } else {
            D_800625A0->flags->party[i] = 0xFF;
        }
    }
    func_801C5318();
}

/* Start building draw buffer 0. */
void func_801C5A30(void) {
    D_800625A0->buffer_index = 0;
}

/* Upload the text CLUT: 16 black entries except white entry 1 at (0, 0x1C0). */
void func_801C5A40(void) {
    RECT rect;
    RECT unused; /* unused in the original; reserves 8 bytes */
    u16 *clut = func_80031BDC(0x20, 0);

    bzero(clut, 0x20);
    clut[1] = 0x7FFF;
    rect.x = 0;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, clut);
    DrawSync(0);
    func_800320E8(clut);
}

/* Set up label `index`'s two quads: mode 0 maps the text rendered for the
 * command column at row + index; otherwise the list layout, dimmed unless
 * bit 7 is set, with the highlight from the low bits. Old-style definition:
 * mode arrives as a promoted int and is narrowed where it is tested. */
void func_801C5ABC(label, index, row, mode)
    MenuLabel *label;
    s32 index;
    s32 row;
    u8 mode;
{
    POLY_FT4 *poly;
    s32 i;
    u8 dim;

    for (i = 0; i < 2; i++) {
        poly = &label->polys[i];
        dim = 0;
        SetPolyFT4(poly);
        SetSemiTrans(poly, 0);
        SetShadeTex(poly, 0);
        poly->r0 = 0x80;
        poly->g0 = 0x80;
        poly->b0 = 0x80;
        if (mode == 0) {
            label->highlight = index & 1;
            poly->tpage = GetTPage(0, 0, 0x140, 0);
            poly->u0 = ((index / 2) & 1) << 7;
            poly->v0 = ((index + row) / 4) * 13;
            poly->u1 = (((index / 2) & 1) << 7) + label->width;
            poly->v1 = ((index + row) / 4) * 13;
            poly->u2 = ((index / 2) & 1) << 7;
            poly->v2 = ((index + row) / 4) * 13 + 13;
            poly->u3 = (((index / 2) & 1) << 7) + label->width;
            poly->v3 = ((index + row) / 4) * 13 + 13;
        } else {
            if (!(mode & 0x80)) {
                dim = 0x20;
                SetSemiTrans(poly, 1);
                poly->r0 = dim;
                poly->g0 = dim;
                poly->b0 = dim;
            }
            label->highlight = (mode & 0x7F) - 1;
            poly->tpage = GetTPage(0, 0, 0x180, 0x80) | dim;
            poly->u0 = (index & 1) * 0x60;
            poly->v0 = (index / 2) * 13 + row;
            poly->u1 = (index & 1) * 0x60 + label->width;
            poly->v1 = (index / 2) * 13 + row;
            poly->u2 = (index & 1) * 0x60;
            poly->v2 = (index / 2) * 13 + row + 13;
            poly->u3 = (index & 1) * 0x60 + label->width;
            poly->v3 = (index / 2) * 13 + row + 13;
        }
        label->polys[i].clut = label->highlight ? D_80059414 : D_800595D4;
    }
    label->projected = 0;
}

/* Render `count` label texts (pairs of text ids) into VRAM, two per line,
 * and set up their quads. */
void func_801C5CFC(MenuLabel *labels, u8 *text_ids, s32 row, s32 count) {
    s32 i;
    RECT *rect;

    for (i = 0; i < count; i += 2) {
        labels[i].width = func_80034EAC(func_80033728(D_800625A0->label_text, text_ids[i]),
                                        D_800625A0->labels[0].pixels, 0x18, 0);
        rect = &labels[i].rect;
        labels[i + 1].width =
            func_80034EAC(func_80033728(D_800625A0->label_text, text_ids[i + 1]),
                          D_800625A0->labels[0].pixels, 0x18, 1);
        labels[i].rect.x = (((i / 2) & 1) << 5) + 0x140;
        labels[i].rect.y = ((i + row) / 4) * 13;
        labels[i].rect.w = 0x1C;
        labels[i].rect.h = 13;
        labels[i + 1].rect = labels[i].rect;
        func_801C5ABC(&labels[i], i, row, 0);
        func_801C5ABC(&labels[i + 1], i + 1, row, 0);
        LoadImage(rect, D_800625A0->labels[0].pixels);
        DrawSync(0);
    }
}

/* Set up the four command labels and the text CLUT. */
void func_801C5EAC(void) {
    func_80033698(0, 0x1D1);
    D_800625A0->labels[0].pixels = func_80031BDC(0x38E, 0);
    func_801C5CFC(D_800625A0->labels, D_801CBF98, 0, 4);
    func_801C5A40();
}

/* Look up the four cursor/frame sprites of the sheet. */
void func_801C5F08(void) {
    s32 unused[10]; /* unused in the original; reserves 40 bytes */
    func_80026338(D_800625A0->sheet, 0xFE, &D_800625A0->sheet_entries[0].first,
                  &D_800625A0->sheet_entries[0].mode, &D_800625A0->sheet_entries[0].clut_x,
                  &D_800625A0->sheet_entries[0].clut_y, &D_800625A0->sheet_entries[0].page_x,
                  &D_800625A0->sheet_entries[0].page_y);
    func_80026338(D_800625A0->sheet, 0x103, &D_800625A0->sheet_entries[1].first,
                  &D_800625A0->sheet_entries[1].mode, &D_800625A0->sheet_entries[1].clut_x,
                  &D_800625A0->sheet_entries[1].clut_y, &D_800625A0->sheet_entries[1].page_x,
                  &D_800625A0->sheet_entries[1].page_y);
    func_80026338(D_800625A0->sheet, 0x100, &D_800625A0->sheet_entries[2].first,
                  &D_800625A0->sheet_entries[2].mode, &D_800625A0->sheet_entries[2].clut_x,
                  &D_800625A0->sheet_entries[2].clut_y, &D_800625A0->sheet_entries[2].page_x,
                  &D_800625A0->sheet_entries[2].page_y);
    func_80026338(D_800625A0->sheet, 0x101, &D_800625A0->sheet_entries[3].first,
                  &D_800625A0->sheet_entries[3].mode, &D_800625A0->sheet_entries[3].clut_x,
                  &D_800625A0->sheet_entries[3].clut_y, &D_800625A0->sheet_entries[3].page_x,
                  &D_800625A0->sheet_entries[3].page_y);
}

/* Clear the party list's flags 3 and 4. */
void func_801C6010(void) {
    D_800625A0->flags->sprite_shown = 0;
    D_800625A0->flags->cursor_shown = 0;
}

/* Make `poly` a gouraud quad fading from (r, g, b) at the top to black. */
void func_801C6040(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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

/* Set up the backdrop primitives of both draw buffers: the gradient, the
 * full-screen fade quad, the two green frame lines and the draw modes. */
void func_801C60BC(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    func_801C6010();
    for (i = 0; i < 2; i++) {
        func_801C6040(&D_800625A0->prims->shade[i], 0x80, 0x80, 0);
        SetSemiTrans(&D_800625A0->prims->shade[i], 1);
        SetLineF3(&D_800625A0->prims->upper[i]);
        (D_800625A0->prims->upper + i)->r0 = 0;
        (D_800625A0->prims->upper + i)->g0 = 0x40;
        (D_800625A0->prims->upper + i)->b0 = 0;
        SetLineF3(&D_800625A0->prims->lower[i]);
        (D_800625A0->prims->lower + i)->r0 = 0;
        (D_800625A0->prims->lower + i)->g0 = 0x40;
        (D_800625A0->prims->lower + i)->b0 = 0;
        SetPolyF4(&D_800625A0->prims->fade[i]);
        (D_800625A0->prims->fade + i)->x0 = 0;
        (D_800625A0->prims->fade + i)->y0 = 0;
        (D_800625A0->prims->fade + i)->x1 = 0x140;
        (D_800625A0->prims->fade + i)->y1 = 0;
        (D_800625A0->prims->fade + i)->x2 = 0;
        (D_800625A0->prims->fade + i)->y2 = 0xE0;
        (D_800625A0->prims->fade + i)->x3 = 0x140;
        (D_800625A0->prims->fade + i)->y3 = 0xE0;
        (D_800625A0->prims->fade + i)->r0 = 0x80;
        (D_800625A0->prims->fade + i)->g0 = 0x80;
        (D_800625A0->prims->fade + i)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->prims->fade[i], 1);
        SetDrawMode(&D_800625A0->prims->mode_label[i], 0, 0, GetTPage(0, 0, 0x140, 0x80),
                      &window);
        SetDrawMode(&D_800625A0->prims->mode_sprite[i], 0, 0, GetTPage(0, 2, 0x180, 0),
                      &window);
    }
}

/* Place a quad's four vertices around the screen centre (160, 112). */
void func_801C6408(SVECTOR *v, u16 x, u16 y, u16 w, u16 h) {
    v[0].vx = x - 160;
    v[0].vy = y - 112;
    v[0].vz = 0;
    v[1].vx = x + w - 160;
    v[1].vy = y - 112;
    v[1].vz = 0;
    v[2].vx = x - 160;
    v[2].vz = 0;
    v[3].vx = x + w - 160;
    v[3].vz = 0;
    v[2].vy = y + h - 112;
    v[3].vy = y + h - 112;
}

/* Make `poly` semi-transparent and untinted. */
void func_801C6460(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Set up panel `index`'s primitives: its translucent grey fill and draw
 * modes, and the textured edge strips from the four frame sprites. */
void func_801C64A8(u8 index) {
    Panel *panel = D_800625A0->panels[index];
    RECT window;
    u8 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    D_800625A0->flags->panels_shown[index] = 0;
    D_800625A0->flags->panels_growing[index] = 0;
    for (i = 0; i < 2; i++) {
        SetPolyG4(&panel->fill[i]);
        (panel->fill + i)->r0 = 0x68;
        (panel->fill + i)->g0 = 0x68;
        (panel->fill + i)->b0 = 0x68;
        (panel->fill + i)->r1 = 0x68;
        (panel->fill + i)->g1 = 0x68;
        (panel->fill + i)->b1 = 0x68;
        (panel->fill + i)->r2 = 0x68;
        (panel->fill + i)->g2 = 0x68;
        (panel->fill + i)->b2 = 0x68;
        (panel->fill + i)->r3 = 0x68;
        (panel->fill + i)->g3 = 0x68;
        (panel->fill + i)->b3 = 0x68;
        SetSemiTrans(&panel->fill[i], 1);
        SetDrawMode(&panel->fill_mode[i], 0, 0,
                      GetTPage(0, 0, D_800625A0->sheet_entries[0].page_x,
                                    D_800625A0->sheet_entries[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->edge[0][i]);
        SetShadeTex(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xFF;
        (panel->edge[0] + i)->g0 = 0xFF;
        (panel->edge[0] + i)->b0 = 0xFF;
        (panel->edge[0] + i)->tpage =
            GetTPage(D_800625A0->sheet_entries[0].mode, 0, D_800625A0->sheet_entries[0].page_x,
                          D_800625A0->sheet_entries[0].page_y);
        (panel->edge[0] + i)->clut =
            GetClut(D_800625A0->sheet_entries[0].clut_x, D_800625A0->sheet_entries[0].clut_y);
        SetPolyFT4(&panel->edge[1][i]);
        SetShadeTex(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xFF;
        (panel->edge[1] + i)->g0 = 0xFF;
        (panel->edge[1] + i)->b0 = 0xFF;
        (panel->edge[1] + i)->tpage =
            GetTPage(D_800625A0->sheet_entries[1].mode, 0, D_800625A0->sheet_entries[1].page_x,
                          D_800625A0->sheet_entries[1].page_y);
        (panel->edge[1] + i)->clut =
            GetClut(D_800625A0->sheet_entries[1].clut_x, D_800625A0->sheet_entries[1].clut_y);
        SetPolyFT4(&panel->edge[2][i]);
        SetShadeTex(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xFF;
        (panel->edge[2] + i)->g0 = 0xFF;
        (panel->edge[2] + i)->b0 = 0xFF;
        (panel->edge[2] + i)->tpage =
            GetTPage(D_800625A0->sheet_entries[2].mode, 0, D_800625A0->sheet_entries[2].page_x,
                          D_800625A0->sheet_entries[2].page_y);
        (panel->edge[2] + i)->clut =
            GetClut(D_800625A0->sheet_entries[2].clut_x, D_800625A0->sheet_entries[2].clut_y);
        SetPolyFT4(&panel->edge[3][i]);
        SetShadeTex(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xFF;
        (panel->edge[3] + i)->g0 = 0xFF;
        (panel->edge[3] + i)->b0 = 0xFF;
        (panel->edge[3] + i)->tpage =
            GetTPage(D_800625A0->sheet_entries[3].mode, 0, D_800625A0->sheet_entries[3].page_x,
                          D_800625A0->sheet_entries[3].page_y);
        (panel->edge[3] + i)->clut =
            GetClut(D_800625A0->sheet_entries[3].clut_x, D_800625A0->sheet_entries[3].clut_y);
    }
}

/* Build panel `index`'s frame sprites for this buffer at (x, y) with height
 * `h` and place its top, bottom and side vectors. */
void func_801C67C4(u8 index, u16 x, u16 y, s32 unused, u16 h) {
    Panel *panel = D_800625A0->panels[index];

    func_8002675C(D_800625A0->sheet, 0x105, panel->frame_ends, D_800625A0->buffer_index,
                  x, y, 0x1000);
    func_800263E4(D_800625A0->sheet, 0x105, &panel->frame_ends[2],
                  D_800625A0->buffer_index, x, y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sheet, 0x106, panel->frame_side, D_800625A0->buffer_index,
                  x, y + 8, 0x1000);
    func_801C6408(&panel->ends_at[0], x, y, 8, 8);
    func_801C6408(&panel->ends_at[4], x, y + h, 8, -8);
    func_801C6408(panel->side_at, x, y + 8, 8, h - 8);
}

/* Build panel `index`'s four corner sprites for this buffer and place them
 * around the rectangle (x, y, w, h). */
void func_801C6928(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel = D_800625A0->panels[index];
    s32 i;

    panel->corner_parts = 0;
    panel->corner_parts += func_8002675C(D_800625A0->sheet, 0xFD, panel->corner,
                                         D_800625A0->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        func_8002675C(D_800625A0->sheet, 0xFF, &panel->corner[panel->corner_parts * 2],
                      D_800625A0->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        func_8002675C(D_800625A0->sheet, 0x102, &panel->corner[panel->corner_parts * 2],
                      D_800625A0->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        func_8002675C(D_800625A0->sheet, 0x104, &panel->corner[panel->corner_parts * 2],
                      D_800625A0->buffer_index, 0, 0, 0x1000);
    func_801C6408(&panel->corner_at[0], x - 8, y + 8, 16, -16);
    func_801C6408(&panel->corner_at[4], x + w + 8, y + 8, -16, -16);
    func_801C6408(&panel->corner_at[8], x - 8, y + h - 8, 16, 16);
    func_801C6408(&panel->corner_at[12], x + w + 8, y + h - 8, -16, 16);
    for (i = 0; i < 4; i++) {
        func_801C6460(&panel->corner[i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s top edge pieces for this buffer and place them in two
 * halves along the top of (x, y, w). */
void func_801C6B70(u8 index, u16 x, u16 y, u16 w) {
    Panel *panel = D_800625A0->panels[index];
    s32 half;
    s32 i;

    (panel->edge[0] + D_800625A0->buffer_index)->u0 = 0;
    (panel->edge[0] + D_800625A0->buffer_index)->v0 = 0x84;
    (panel->edge[0] + D_800625A0->buffer_index)->u1 = 7;
    (panel->edge[0] + D_800625A0->buffer_index)->v1 = 0x84;
    (panel->edge[0] + D_800625A0->buffer_index)->u2 = 0;
    (panel->edge[0] + D_800625A0->buffer_index)->v2 = 0x94;
    (panel->edge[0] + D_800625A0->buffer_index)->u3 = 7;
    (panel->edge[0] + D_800625A0->buffer_index)->v3 = 0x94;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->u0 = 0;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->v0 = 0x84;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->u1 = 7;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->v1 = 0x84;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->u2 = 0;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->v2 = 0x94;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->u3 = 7;
    (panel->edge[0] + D_800625A0->buffer_index + 2)->v3 = 0x94;
    half = (w - 16) / 2;
    func_801C6408(panel->edge_at[0][0], x + 8, y - 8, half, 16);
    func_801C6408(panel->edge_at[0][1], x + (half + 8), y - 8, half, 16);
    for (i = 0; i < 2; i++) {
        func_801C6460(&panel->edge[0][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s bottom edge pieces for this buffer and place them in
 * two halves along the bottom of (x, y, w, h). */
void func_801C6EB4(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel = D_800625A0->panels[index];
    s32 half;
    s32 i;

    (panel->edge[1] + D_800625A0->buffer_index)->u0 = 8;
    (panel->edge[1] + D_800625A0->buffer_index)->v0 = 0x84;
    (panel->edge[1] + D_800625A0->buffer_index)->u1 = 0xF;
    (panel->edge[1] + D_800625A0->buffer_index)->v1 = 0x84;
    (panel->edge[1] + D_800625A0->buffer_index)->u2 = 8;
    (panel->edge[1] + D_800625A0->buffer_index)->v2 = 0x94;
    (panel->edge[1] + D_800625A0->buffer_index)->u3 = 0xF;
    (panel->edge[1] + D_800625A0->buffer_index)->v3 = 0x94;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->u0 = 8;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->v0 = 0x84;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->u1 = 0xF;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->v1 = 0x84;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->u2 = 8;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->v2 = 0x94;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->u3 = 0xF;
    (panel->edge[1] + D_800625A0->buffer_index + 2)->v3 = 0x94;
    half = (w - 16) / 2;
    func_801C6408(panel->edge_at[1][0], x + 8, y + h - 8, half, 16);
    func_801C6408(panel->edge_at[1][1], x + (half + 8), y + h - 8, half, 16);
    for (i = 0; i < 2; i++) {
        func_801C6460(&panel->edge[1][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s left edge pieces for this buffer and place them in two
 * halves down the left of (x, y, h). */
void func_801C7200(u8 index, u16 x, u16 y, u16 h) {
    Panel *panel = D_800625A0->panels[index];
    s32 half;
    s32 i;

    (panel->edge[2] + D_800625A0->buffer_index)->u0 = 0x10;
    (panel->edge[2] + D_800625A0->buffer_index)->v0 = 0x84;
    (panel->edge[2] + D_800625A0->buffer_index)->u1 = 0x20;
    (panel->edge[2] + D_800625A0->buffer_index)->v1 = 0x84;
    (panel->edge[2] + D_800625A0->buffer_index)->u2 = 0x10;
    (panel->edge[2] + D_800625A0->buffer_index)->v2 = 0x8B;
    (panel->edge[2] + D_800625A0->buffer_index)->u3 = 0x20;
    (panel->edge[2] + D_800625A0->buffer_index)->v3 = 0x8B;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->u0 = 0x10;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->v0 = 0x84;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->u1 = 0x20;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->v1 = 0x84;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->u2 = 0x10;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->v2 = 0x8B;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->u3 = 0x20;
    (panel->edge[2] + D_800625A0->buffer_index + 2)->v3 = 0x8B;
    half = (h - 16) / 2;
    func_801C6408(panel->edge_at[2][0], x - 8, y + 8, 16, half);
    func_801C6408(panel->edge_at[2][1], x - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        func_801C6460(&panel->edge[2][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s right edge pieces for this buffer and place them in two
 * halves down the right of (x, y, w, h). */
void func_801C7548(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel = D_800625A0->panels[index];
    s32 half;
    s32 i;

    (panel->edge[3] + D_800625A0->buffer_index)->u0 = 0x10;
    (panel->edge[3] + D_800625A0->buffer_index)->v0 = 0x8C;
    (panel->edge[3] + D_800625A0->buffer_index)->u1 = 0x20;
    (panel->edge[3] + D_800625A0->buffer_index)->v1 = 0x8C;
    (panel->edge[3] + D_800625A0->buffer_index)->u2 = 0x10;
    (panel->edge[3] + D_800625A0->buffer_index)->v2 = 0x93;
    (panel->edge[3] + D_800625A0->buffer_index)->u3 = 0x20;
    (panel->edge[3] + D_800625A0->buffer_index)->v3 = 0x93;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->u0 = 0x10;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->v0 = 0x8C;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->u1 = 0x20;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->v1 = 0x8C;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->u2 = 0x10;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->v2 = 0x93;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->u3 = 0x20;
    (panel->edge[3] + D_800625A0->buffer_index + 2)->v3 = 0x93;
    half = (h - 16) / 2;
    func_801C6408(panel->edge_at[3][0], x + w - 8, y + 8, 16, half);
    func_801C6408(panel->edge_at[3][1], x + w - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        func_801C6460(&panel->edge[3][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Lay out panel `index` at (x, y, w, h) for this buffer: fill, corners and
 * edges, the frame sprites when `framed`, and mark it shown. */
void func_801C7894(u8 index, u16 x, u16 y, u16 w, u16 h, u8 style, s32 param, u8 framed) {
    Panel *panel = D_800625A0->panels[index];

    D_800625A0->flags->panels_shown[index] = 0;
    func_801C6408(panel->fill_at, x, y, w, h);
    func_801C6928(index, x, y, w, h);
    func_801C6B70(index, x, y, w);
    func_801C6EB4(index, x, y, w, h);
    func_801C7200(index, x, y, h);
    func_801C7548(index, x, y, w, h);
    if (framed) {
        func_801C67C4(index, x, y, w, h);
    }
    panel->framed = framed;
    panel->style = style;
    panel->param = param;
    panel->buffer = D_800625A0->buffer_index;
    D_800625A0->flags->panels_shown[index] = 1;
}

/* Hide panel `index` and release its block and growth record. */
void func_801C7A18(u8 index) {
    D_800625A0->flags->panels_shown[index] = 0;
    D_800625A0->flags->panels_growing[index] = 0;
    func_800320E8(D_800625A0->panels[index]);
    func_800320E8(D_800625A0->growth[index]);
}

/* Open panel `index` (allocating it first unless it is 0 or 1): either
 * start its growth animation toward (x, y, w, h) or lay it out at once. */
void func_801C7AA4(u8 index, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 style, s32 param,
                   u8 framed) {
    MenuGrowth *growth;

    if (index >= 2) {
        D_800625A0->panels[index] = func_80031BDC(0x720, 0);
        bzero(D_800625A0->panels[index], 0x720);
        D_800625A0->growth[index] = func_80031BDC(0x18, 0);
        bzero(D_800625A0->growth[index], 0x18);
        func_801C64A8(index);
    }
    growth = D_800625A0->growth[index];
    if (animate) {
        growth->index = index;
        growth->done = 0;
        growth->x = x;
        growth->y = y;
        growth->w = w;
        growth->h = h;
        growth->cur_w = 0;
        growth->cur_h = 0;
        D_800625A0->flags->panels_growing[index] = 1;
        growth->flat = style;
        growth->ot_entry = param;
    } else {
        func_801C7894(index, x, y, w, h, style, param, framed);
    }
}

/* Grow every opening panel by 32 in width and height per frame until it
 * reaches its size, laying it out centred on its final rectangle. */
void func_801C7C28(void) {
    s32 i;
    MenuGrowth *growth;
    u8 done;

    for (i = 0; i < 7; i++) {
        growth = D_800625A0->growth[i];
        if (D_800625A0->flags->panels_growing[i] && !growth->done) {
            done = 0;
            if (growth->cur_w + 32 >= growth->w) {
                growth->cur_w = growth->w;
                done++;
            } else {
                growth->cur_w = growth->cur_w + 32;
            }
            if (growth->cur_h + 32 >= growth->h) {
                growth->cur_h = growth->h;
                done++;
            } else {
                growth->cur_h = growth->cur_h + 32;
            }
            if (done == 2) {
                growth->done = 1;
            }
            func_801C7894(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->flat, growth->ot_entry, growth->framed);
        }
    }
}

/* Project panel `index`'s two top edge pieces through the GTE and draw them. */
void func_801C7D74(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[0][0][0], &panel->edge_at[0][0][1], &panel->edge_at[0][0][2],
                  &panel->edge_at[0][0][3], &(panel->edge[0] + panel->buffer)->x0,
                  &(panel->edge[0] + panel->buffer)->x1, &(panel->edge[0] + panel->buffer)->x2,
                  &(panel->edge[0] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[0][panel->buffer]);
    RotTransPers4(&panel->edge_at[0][1][0], &panel->edge_at[0][1][1], &panel->edge_at[0][1][2],
                  &panel->edge_at[0][1][3], &(panel->edge[0] + panel->buffer + 2)->x0,
                  &(panel->edge[0] + panel->buffer + 2)->x1, &(panel->edge[0] + panel->buffer + 2)->x2,
                  &(panel->edge[0] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[0][panel->buffer + 2]);
}

/* Project panel `index`'s two bottom edge pieces through the GTE and draw them. */
void func_801C7F48(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[1][0][0], &panel->edge_at[1][0][1], &panel->edge_at[1][0][2],
                  &panel->edge_at[1][0][3], &(panel->edge[1] + panel->buffer)->x0,
                  &(panel->edge[1] + panel->buffer)->x1, &(panel->edge[1] + panel->buffer)->x2,
                  &(panel->edge[1] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[1][panel->buffer]);
    RotTransPers4(&panel->edge_at[1][1][0], &panel->edge_at[1][1][1], &panel->edge_at[1][1][2],
                  &panel->edge_at[1][1][3], &(panel->edge[1] + panel->buffer + 2)->x0,
                  &(panel->edge[1] + panel->buffer + 2)->x1, &(panel->edge[1] + panel->buffer + 2)->x2,
                  &(panel->edge[1] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[1][panel->buffer + 2]);
}

/* Project panel `index`'s two left edge pieces through the GTE and draw them. */
void func_801C811C(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[2][0][0], &panel->edge_at[2][0][1], &panel->edge_at[2][0][2],
                  &panel->edge_at[2][0][3], &(panel->edge[2] + panel->buffer)->x0,
                  &(panel->edge[2] + panel->buffer)->x1, &(panel->edge[2] + panel->buffer)->x2,
                  &(panel->edge[2] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[2][panel->buffer]);
    RotTransPers4(&panel->edge_at[2][1][0], &panel->edge_at[2][1][1], &panel->edge_at[2][1][2],
                  &panel->edge_at[2][1][3], &(panel->edge[2] + panel->buffer + 2)->x0,
                  &(panel->edge[2] + panel->buffer + 2)->x1, &(panel->edge[2] + panel->buffer + 2)->x2,
                  &(panel->edge[2] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[2][panel->buffer + 2]);
}

/* Project panel `index`'s two right edge pieces through the GTE and draw them. */
void func_801C82F0(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[3][0][0], &panel->edge_at[3][0][1], &panel->edge_at[3][0][2],
                  &panel->edge_at[3][0][3], &(panel->edge[3] + panel->buffer)->x0,
                  &(panel->edge[3] + panel->buffer)->x1, &(panel->edge[3] + panel->buffer)->x2,
                  &(panel->edge[3] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[3][panel->buffer]);
    RotTransPers4(&panel->edge_at[3][1][0], &panel->edge_at[3][1][1], &panel->edge_at[3][1][2],
                  &panel->edge_at[3][1][3], &(panel->edge[3] + panel->buffer + 2)->x0,
                  &(panel->edge[3] + panel->buffer + 2)->x1, &(panel->edge[3] + panel->buffer + 2)->x2,
                  &(panel->edge[3] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->edge[3][panel->buffer + 2]);
}

/* Project panel `index`'s fill through the GTE and draw it with its mode. */
void func_801C84C4(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->fill_at[0], &panel->fill_at[1], &panel->fill_at[2], &panel->fill_at[3],
                  &(panel->fill + panel->buffer)->x0, &(panel->fill + panel->buffer)->x1,
                  &(panel->fill + panel->buffer)->x2, &(panel->fill + panel->buffer)->x3, &depth,
                  &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->fill[panel->buffer]);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->fill_mode[panel->buffer]);
}

/* Project panel `index`'s four corner sprites through the GTE and draw them. */
void func_801C8600(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 4; i++) {
        RotTransPers4(&panel->corner_at[i * 4], &panel->corner_at[i * 4 + 1], &panel->corner_at[i * 4 + 2],
                      &panel->corner_at[i * 4 + 3], &panel->corner[i * 2 + panel->buffer].x0,
                      &panel->corner[i * 2 + panel->buffer].x1,
                      &panel->corner[i * 2 + panel->buffer].x2,
                      &panel->corner[i * 2 + panel->buffer].x3, &depth, &flag);
        AddPrim(D_800625A0->current->ot + panel->param,
                      &panel->corner[i * 2 + panel->buffer]);
    }
}

/* Project panel `index`'s frame sprites (top, bottom, side) and draw them. */
void func_801C874C(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 2; i++) {
        RotTransPers4(&panel->ends_at[i * 4], &panel->ends_at[i * 4 + 1], &panel->ends_at[i * 4 + 2],
                      &panel->ends_at[i * 4 + 3], &(panel->frame_ends + (i * 2 + panel->buffer))->x0,
                      &(panel->frame_ends + (i * 2 + panel->buffer))->x1,
                      &(panel->frame_ends + (i * 2 + panel->buffer))->x2,
                      &(panel->frame_ends + (i * 2 + panel->buffer))->x3, &depth, &flag);
        AddPrim(D_800625A0->current->ot + panel->param,
                      &panel->frame_ends[i * 2 + panel->buffer]);
    }
    RotTransPers4(&panel->side_at[0], &panel->side_at[1], &panel->side_at[2], &panel->side_at[3],
                  &(panel->frame_side + panel->buffer)->x0,
                  &(panel->frame_side + panel->buffer)->x1,
                  &(panel->frame_side + panel->buffer)->x2,
                  &(panel->frame_side + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->current->ot + panel->param, &panel->frame_side[panel->buffer]);
}

/* Draw every shown panel; style-0 panels are projected with an identity
 * rotation at depth 0x200. */
void func_801C8970(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    Panel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->flags->panels_shown[i]) {
            panel = D_800625A0->panels[i];
            if (panel->style == 0) {
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
                func_801C8600(i);
                if (panel->framed) {
                    func_801C874C(i);
                }
                func_801C7D74(i);
                func_801C7F48(i);
                func_801C811C(i);
                func_801C82F0(i);
                func_801C84C4(i);
                PopMatrix();
            } else {
                func_801C8600(i);
                if (panel->framed) {
                    func_801C874C(i);
                }
                func_801C7D74(i);
                func_801C7F48(i);
                func_801C811C(i);
                func_801C82F0(i);
                func_801C84C4(i);
            }
        }
    }
}

/* Draw the four cursor markers when markers are on; a marker that follows
 * the file cursor is first moved to the selected slot's position. */
void func_801C8AE8(void) {
    s32 i;
    MenuState *state;

    if (D_800625A0->flags->markers_shown) {
        for (i = 0; i < 4; i++) {
            state = D_800625A0;
            if (state->markers->shown[i]) {
                if (state->markers->at_cursor[i]) {
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x0 =
                        D_801CC014[D_801CBF9C[state->card->cursor]] + 8;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y0 =
                        D_801CC094[D_801CBF9C[state->card->cursor]] - 6;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x1 =
                        D_801CC014[D_801CBF9C[state->card->cursor]] + 24;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y1 =
                        D_801CC094[D_801CBF9C[state->card->cursor]] - 6;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x2 =
                        D_801CC014[D_801CBF9C[state->card->cursor]] + 8;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y2 =
                        D_801CC094[D_801CBF9C[state->card->cursor]] + 10;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x3 =
                        D_801CC014[D_801CBF9C[state->card->cursor]] + 24;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y3 =
                        D_801CC094[D_801CBF9C[state->card->cursor]] + 10;
                }
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->markers->polys[i * 2 + D_800625A0->markers->buffer[i]]);
            }
        }
    }
}

/* Draw the shown command labels. */
void func_801C8E38(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->flags->labels_shown[i]) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->labels[i].polys[D_800625A0->labels[i].buffer]);
        }
    }
}

/* Draw the shown list labels. */
void func_801C8EC8(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->flags->list_labels_shown[i]) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->list_labels[i].polys[D_800625A0->list_labels[i].buffer]);
        }
    }
}

/* Draw the shown row labels, projecting the 3D ones through the GTE first. */
void func_801C8F58(void) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->row_labels_shown[i]) {
            if (D_800625A0->row_labels[i].projected) {
                MenuLabel *label = &D_800625A0->row_labels[i];

                RotTransPers4(&label->verts[0], &label->verts[1], &label->verts[2],
                              &label->verts[3],
                              &label->polys[D_800625A0->row_labels[i].buffer].x0,
                              &label->polys[D_800625A0->row_labels[i].buffer].x1,
                              &label->polys[D_800625A0->row_labels[i].buffer].x2,
                              &label->polys[D_800625A0->row_labels[i].buffer].x3, &depth, &flag);
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->row_labels[i].polys[D_800625A0->row_labels[i].buffer]);
            } else {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->row_labels[i].polys[D_800625A0->row_labels[i].buffer]);
            }
        }
    }
}

/* Draw the shown name entry labels. */
void func_801C90D0(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->extra_labels_shown[i]) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->extra_labels[i].polys[D_800625A0->extra_labels[i].buffer]);
        }
    }
}

/* Draw the shown message lines, projecting the 3D ones through the GTE. */
void func_801C9160(void) {
    s32 depth;
    s32 flag;
    s32 i;
    MenuLabel *line;

    if (D_800625A0->flags->messages_shown) {
        for (i = 0; i < 3; i++) {
            line = D_800625A0->message_labels[i];
            if (line->projected) {
                RotTransPers4(&line->verts[0], &line->verts[1], &line->verts[2],
                              &line->verts[3], &line->polys[line->buffer].x0,
                              &line->polys[line->buffer].x1, &line->polys[line->buffer].x2,
                              &line->polys[line->buffer].x3, &depth, &flag);
                AddPrim(&D_800625A0->current->ot[4], &line->polys[line->buffer]);
            } else {
                AddPrim(&D_800625A0->current->ot[4], &line->polys[line->buffer]);
            }
        }
    }
}

/* Draw this buffer's fade quad and the second draw mode. */
void func_801C92BC(void) {
    AddPrim(&D_800625A0->current->ot[8],
                  &D_800625A0->prims->fade[D_800625A0->buffer_index]);
    AddPrim(&D_800625A0->current->ot[8],
                  &D_800625A0->prims->mode_sprite[D_800625A0->buffer_index]);
}

/* Draw the name entry: the projected cursor and name quads, the confirm,
 * back and frame sprites, the grid lines, the blinking caret after the name
 * and the 36 grid characters. */
void func_801C9338(void) {
    s32 depth;
    s32 flag;
    s32 i;

    if (D_800625A0->flags->name_entry_shown) {
        if (D_800625A0->name_entry->shown) {
            RotTransPers4(&D_800625A0->name_entry->cursor_at[0], &D_800625A0->name_entry->cursor_at[1],
                          &D_800625A0->name_entry->cursor_at[2], &D_800625A0->name_entry->cursor_at[3],
                          &(D_800625A0->name_entry->cursor + D_800625A0->name_entry->cursor_buffer)->x0,
                          &(D_800625A0->name_entry->cursor + D_800625A0->name_entry->cursor_buffer)->x1,
                          &(D_800625A0->name_entry->cursor + D_800625A0->name_entry->cursor_buffer)->x2,
                          &(D_800625A0->name_entry->cursor + D_800625A0->name_entry->cursor_buffer)->x3,
                          &depth, &flag);
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->name_entry->cursor[D_800625A0->name_entry->cursor_buffer]);
            RotTransPers4(&D_800625A0->name_entry->name_at[0], &D_800625A0->name_entry->name_at[1],
                          &D_800625A0->name_entry->name_at[2], &D_800625A0->name_entry->name_at[3],
                          &(D_800625A0->name_entry->name + D_800625A0->name_entry->name_buffer)->x0,
                          &(D_800625A0->name_entry->name + D_800625A0->name_entry->name_buffer)->x1,
                          &(D_800625A0->name_entry->name + D_800625A0->name_entry->name_buffer)->x2,
                          &(D_800625A0->name_entry->name + D_800625A0->name_entry->name_buffer)->x3,
                          &depth, &flag);
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->name_entry->name[D_800625A0->name_entry->name_buffer]);
        }
        if (D_800625A0->name_entry->grid_shown) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->name_entry->confirm[D_800625A0->name_entry->parts_buffer]);
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->name_entry->back[D_800625A0->name_entry->parts_buffer]);
            for (i = 0; i < D_800625A0->name_entry->frame_count; i++) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->name_entry->frame[i * 2 + D_800625A0->name_entry->parts_buffer]);
            }
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->name_entry->line_a[D_800625A0->name_entry->lines_buffer]);
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->name_entry->line_b[D_800625A0->name_entry->lines_buffer]);
            if (++D_800625A0->name_entry->blink >= 61) {
                D_800625A0->name_entry->blink = 0;
            }
            if (D_800625A0->name_entry->blink < 30) {
                (D_800625A0->name_entry->caret + D_800625A0->buffer_index)->x0 =
                    D_800625A0->name_entry->length * 8 + 0x50;
                (D_800625A0->name_entry->caret + D_800625A0->buffer_index)->y0 = 0xC6;
                (D_800625A0->name_entry->caret + D_800625A0->buffer_index)->x1 =
                    D_800625A0->name_entry->length * 8 + 0x58;
                (D_800625A0->name_entry->caret + D_800625A0->buffer_index)->y1 = 0xC6;
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->name_entry->caret[D_800625A0->buffer_index]);
            }
            for (i = 0; i < 36; i++) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->name_entry->chars[i * 2 + D_800625A0->name_entry->chars_buffer]);
            }
        }
    }
}

/* Draw all labels, the name entry labels and the message lines. */
void func_801C97FC(void) {
    func_801C8E38();
    func_801C8EC8();
    func_801C8F58();
    func_801C90D0();
    func_801C9160();
}

/* Per-frame screen drawing: panels, markers, labels and the name entry while
 * active, then the fade. */
void func_801C983C(void) {
    if (D_800625A0->drawing) {
        func_801C7C28();
        func_801C8AE8();
        func_801C97FC();
        func_801C9338();
        func_801C8970();
    }
    func_801C92BC();
}

/* Play menu sound `sound` from the loaded effect bank when sounds are on. */
void func_801C989C(u8 sound) {
    if (D_800625A0->sounds) {
        func_80039DB8((D_800625A0->effects->id << 16) | sound);
    }
}

/* Read this frame's input into the input code (8: none). Without a pad the
 * sound is paused (keeping the vsync count) until one is connected; an
 * overflowed queue is reset; otherwise entries are dequeued until one holds
 * a button the screen uses (directions and cancel play their sounds). */
void func_801C98E8(void) {
    u8 code = 8;
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;

    do {
        if (func_80035734(0) == 0) {
            if (paused == 0) {
                paused++;
                func_80037EE4();
                vsyncs = D_80059488;
            }
        } else {
            waiting--;
            if (paused) {
                func_80037E8C();
                D_80059488 = vsyncs;
            }
        }
    } while (waiting);
    if (func_80036410()) {
        func_80035DB0();
    } else {
        while (func_80035CDC()) {
            if (D_800594A4 & 0x2000) {
                code = 0;
                func_801C989C(1);
                break;
            }
            if (D_800594A4 & 0x4000) {
                code = 1;
                func_801C989C(1);
                break;
            }
            if (D_800594A4 & 0x8000) {
                code = 2;
                func_801C989C(1);
                break;
            }
            if (D_800594A4 & 0x1000) {
                code = 3;
                func_801C989C(1);
                break;
            }
            if (D_8005948C & 0x20) {
                code = 4;
                break;
            }
            if (D_8005948C & 0x40) {
                code = 5;
                func_801C989C(3);
                break;
            }
            if (D_8005948C & 0x80) {
                code = 6;
                break;
            }
            if (D_8005948C & 0x10) {
                code = 7;
                break;
            }
            if (D_8005948C & 4) {
                code = 10;
                break;
            }
            if (D_8005948C & 8) {
                code = 9;
                break;
            }
            if (D_8005948C & 0x800) {
                code = 11;
                break;
            }
            if (D_8005948C & 0x100) {
                code = 12;
                D_800625A0->debug_show = D_800625A0->debug_show == 0;
                break;
            }
            if (D_8005948C & 1) {
                D_800625A0->debug_value++;
                break;
            }
        }
    }
    D_800625A0->input = code;
}

/* Advance the view motion (4/3 start zooming in/out, 2/1 run them) and load
 * the view rotation and translation into the GTE. */
void func_801C9AF4(void) {
    switch (D_800625A0->view_motion) {
    case 4:
        D_800625A0->offset.vz = 0x200;
        D_800625A0->angles.vz = 0;
        D_800625A0->angles.vy = 0;
        D_800625A0->angles.vx = 0;
        D_800625A0->offset.vy = 0;
        D_800625A0->offset.vx = 0;
        D_800625A0->view_motion = 2;
        break;
    case 3:
        D_800625A0->offset.vz = 0x800;
        D_800625A0->angles.vz = 0;
        D_800625A0->angles.vy = 0;
        D_800625A0->angles.vx = 0;
        D_800625A0->offset.vy = 0;
        D_800625A0->offset.vx = 0;
        D_800625A0->view_motion = 1;
        break;
    case 2:
        D_800625A0->angles.vy -= 0x60;
        D_800625A0->offset.vz += 0x40;
        if (D_800625A0->offset.vz >= 0xE00) {
            D_800625A0->view_motion = 0;
        }
        break;
    case 1:
        D_800625A0->angles.vx += 0x7C;
        D_800625A0->offset.vz -= 0x30;
        if (D_800625A0->offset.vz < 0x200) {
            D_800625A0->offset.vz = 0x200;
            D_800625A0->angles.vz = 0;
            D_800625A0->angles.vx = 0;
            D_800625A0->angles.vy = 0;
            D_800625A0->view_motion = 0;
        }
        break;
    }
    func_8003F738(&D_800625A0->angles, &D_800625A0->matrix);
    TransMatrix(&D_800625A0->matrix, &D_800625A0->offset);
    SetRotMatrix(&D_800625A0->matrix);
    SetTransMatrix(&D_800625A0->matrix);
}

/* Run one menu frame: check the stack guard, read input, check the reset
 * combination, swap to the other draw buffer, draw the screen and present
 * it. */
void func_801C9C34(void) {
    MenuState *state;
    MenuBuffer *env;
    s32 shown;

    if (*D_8005917C != -1) {
        __asm__ volatile("break 1024");
    }
    func_801C98E8();
    func_80019CA0();
    state = D_800625A0;
    env = &state->buffers[0];
    if (state->current == env) {
        env = &state->buffers[1];
    }
    state->current = env;
    state->buffer_index = state->buffer_index == 0;
    ClearOTagR(state->current->ot, 16);
    func_801C9AF4();
    func_801C983C();
    shown = D_800625A0->buffer_index == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&D_800625A0->current->draw);
    PutDispEnv(&D_800625A0->current->disp);
    MoveImage(&D_800625A0->images->screen, 0, shown * 0xE0);
    DrawOTag(&D_800625A0->current->ot[15]);
}

/* Allocate the markers and set them up for `mode`: 0 and 2 build all four
 * at their home positions (0 also turns them on following the cursor), 3
 * builds the first at the origin and turns them on, 1 leaves them empty. */
void func_801C9D5C(u8 mode) {
    s32 i;
    MenuMarkers *markers = func_80031BDC(0x14C, 0);

    D_800625A0->markers = markers;
    bzero(markers, 0x14C);
    switch (mode) {
    case 0:
        D_800625A0->flags->markers_shown = 1;
        D_800625A0->markers->at_cursor[0] = 1;
        D_800625A0->markers->at_cursor[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sheet, 0x108, D_800625A0->markers->polys + i * 2,
                          D_800625A0->buffer_index, D_801CBEA0[i], D_801CBEB0[i], 0x800);
            D_800625A0->markers->buffer[i] = D_800625A0->buffer_index;
        }
        break;
    case 3:
        func_8002675C(D_800625A0->sheet, 0x108, D_800625A0->markers->polys,
                      D_800625A0->buffer_index, 0, 0, 0x800);
        D_800625A0->markers->buffer[0] = D_800625A0->buffer_index;
        D_800625A0->flags->markers_shown = 1;
        break;
    case 1:
        break;
    }
}

/* Hide the markers, let one frame pass and release them. */
void func_801C9F1C(void) {
    D_800625A0->flags->markers_shown = 0;
    func_801C9C34();
    func_800320E8(D_800625A0->markers);
}

/* Start view motion 3 with its sound. */
void func_801C9F60(void) {
    D_800625A0->view_motion = 3;
    func_801C989C(0x5B);
}

/* Start view motion 4 with its sound. */
void func_801C9F90(void) {
    D_800625A0->view_motion = 4;
    func_801C989C(0x5C);
}

/* Open the message: allocate four line labels (pairs share one render
 * buffer), render texts `first`..`first + 2` into VRAM, set up their 3D
 * quads and show them. */
void func_801C9FC0(u8 first) {
    u16 x = 0x48;
    s32 i;
    MenuLabel *line;

    for (i = 0; i < 4; i++) {
        void *block = func_80031BDC(0x80, 0);

        D_800625A0->message_labels[i] = block;
        bzero(block, 0x80);
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
    for (i = 0; i < 3; i++) {
        line = D_800625A0->message_labels[i];
        line->width = func_80034EAC(func_80033728(D_800625A0->label_text, first + i), line->pixels,
                                    0x36, i % 2);
        func_801C5ABC(line, i, 0, 0);
        func_801C6408(line->verts, x, i * 16 + 0xA0, line->width, 13);
        (line->polys + D_800625A0->buffer_index)->u0 = 0;
        (line->polys + D_800625A0->buffer_index)->v0 = (i / 2) * 13 + 0x4E;
        (line->polys + D_800625A0->buffer_index)->u1 = line->width;
        (line->polys + D_800625A0->buffer_index)->v1 = (i / 2) * 13 + 0x4E;
        (line->polys + D_800625A0->buffer_index)->u2 = 0;
        (line->polys + D_800625A0->buffer_index)->v2 = (i / 2) * 13 + 0x5B;
        (line->polys + D_800625A0->buffer_index)->u3 = line->width;
        (line->polys + D_800625A0->buffer_index)->v3 = (i / 2) * 13 + 0x5B;
        line->buffer = D_800625A0->buffer_index;
        line->projected = 1;
    }
    LoadImage(&D_800625A0->message_labels[0]->rect, D_800625A0->message_labels[0]->pixels);
    LoadImage(&D_800625A0->message_labels[2]->rect, D_800625A0->message_labels[2]->pixels);
    DrawSync(0);
    D_800625A0->flags->messages_shown = 1;
    func_800320E8(D_800625A0->message_labels[0]->pixels);
    func_800320E8(D_800625A0->message_labels[2]->pixels);
    func_801C9C34();
    func_801C9C34();
}

/* Turn the message lines off, release their four blocks and run a frame. */
void func_801CA39C(void) {
    s32 i;

    D_800625A0->flags->messages_shown = 0;
    for (i = 0; i < 4; i++) {
        func_800320E8(D_800625A0->message_labels[i]);
    }
    func_801C9C34();
}

/* Leave the screen: run the closing frames until buffer 0 is shown and
 * release everything (a frame passes around the sound bank release). */
void func_801CA400(void) {
    func_801C9C34();
    func_801C9C34();
    D_800625A0->drawing = 0;
    func_801C9C34();
    do {
        func_801C9C34();
    } while (D_800625A0->buffer_index != 0);
    func_801C505C(0);
    func_801C50C0(0);
    func_801C5124(0);
    func_801C5188(0);
    func_801C51EC(0);
    func_801C5250(0);
    func_800320E8(D_800625A0->sheet);
    func_800320E8(D_800625A0->label_text);
    func_800320E8(D_800625A0->labels[0].pixels);
    if (D_80059178) {
        func_8003A094(D_800625A0->effects);
        func_801C9C34();
        func_8003852C(D_800625A0->effects);
        func_801C9C34();
        func_800320E8(D_800625A0->effects);
    }
    func_801C52B4(0);
    func_800320E8(D_800625A0);
}

/* Build the name entry grid: set up the 72 character quads and the name
 * quad, render the 36 grid entries (five codes each from D_801CBEC0) into
 * VRAM and place them in 4 columns of 9, map and place the name quad, and
 * set up the green grid lines and the caret. */
void func_801CA558(void) {
    u16 codes[8];
    u8 text[16];
    RECT rect;
    u8 *image = func_80031BDC(0x2BE, 0);
    s32 i;
    s32 k;
    s32 column;
    s32 row;

    for (i = 0; i < 74; i++) {
        SetPolyFT4(&D_800625A0->name_entry->chars[i]);
        (D_800625A0->name_entry->chars + i)->r0 = 0x80;
        (D_800625A0->name_entry->chars + i)->g0 = 0x80;
        (D_800625A0->name_entry->chars + i)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->name_entry->chars[i], 0);
        SetShadeTex(&D_800625A0->name_entry->chars[i], 1);
        D_800625A0->name_entry->chars[i].clut = D_800595D4;
        D_800625A0->name_entry->chars[i].tpage = GetTPage(0, 0, 0x180, 0);
    }
    for (i = 0; i < 36; i++) {
        for (k = 0; k < 5; k++) {
            codes[k] = D_801CBEC0[i * 6 + k];
        }
        func_80033B34(codes, text, 5);
        bzero(image, 0x2BE);
        func_80034EAC(text, image, 0x18, 0);
        rect.x = (i % 2) * 32 + 0x180;
        rect.y = (i / 2) * 13;
        rect.w = 0x1C;
        rect.h = 13;
        LoadImage(&rect, image);
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->u0 = (i % 2) << 7;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->v0 = (i / 2) * 13;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->u1 = ((i % 2) << 7) + 0x3C;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->v1 = (i / 2) * 13;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->u2 = (i % 2) << 7;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->v2 = (i / 2) * 13 + 13;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->u3 = ((i % 2) << 7) + 0x3C;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->v3 = (i / 2) * 13 + 13;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->x0 = (i / 9) * 0x30 + 0x44;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->y0 = (i % 9) * 16 + 0x2E;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->x1 = (i / 9) * 0x30 + 0x80;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->y1 = (i % 9) * 16 + 0x2E;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->x2 = (i / 9) * 0x30 + 0x44;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->y2 = (i % 9) * 16 + 0x3B;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->x3 = (i / 9) * 0x30 + 0x80;
        (D_800625A0->name_entry->chars + (i * 2 + D_800625A0->buffer_index))->y3 = (i % 9) * 16 + 0x3B;
        DrawSync(0);
    }
    func_800320E8(image);
    D_800625A0->name_entry->chars_buffer = D_800625A0->buffer_index;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->u0 = 0;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->v0 = 0xEA;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->u1 = D_800625A0->name_entry->max_length * 8;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->v1 = 0xEA;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->u2 = 0;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->v2 = 0xF7;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->u3 = D_800625A0->name_entry->max_length * 8;
    (D_800625A0->name_entry->name + D_800625A0->buffer_index)->v3 = 0xF7;
    func_801C6408(D_800625A0->name_entry->name_at,
                  D_800625A0->name_entry->name[D_800625A0->buffer_index].x0 + 0x50,
                  D_800625A0->name_entry->name[D_800625A0->buffer_index].y0 + 0xB6,
                  D_800625A0->name_entry->max_length * 8, 13);
    D_800625A0->name_entry->name_buffer = D_800625A0->buffer_index;
    for (i = 0; i < 2; i++) {
        SetLineF3(&D_800625A0->name_entry->line_a[i]);
        (D_800625A0->name_entry->line_a + i)->r0 = 0;
        (D_800625A0->name_entry->line_a + i)->g0 = 0xFF;
        (D_800625A0->name_entry->line_a + i)->b0 = 0;
        SetLineF3(&D_800625A0->name_entry->line_b[i]);
        (D_800625A0->name_entry->line_b + i)->r0 = 0;
        (D_800625A0->name_entry->line_b + i)->g0 = 0xFF;
        (D_800625A0->name_entry->line_b + i)->b0 = 0;
        SetLineF2(&D_800625A0->name_entry->caret[i]);
        (D_800625A0->name_entry->caret + i)->r0 = 0;
        (D_800625A0->name_entry->caret + i)->g0 = 0x80;
        (D_800625A0->name_entry->caret + i)->b0 = 0;
    }
}

/* Open the name entry: its panel and message, the cursor sprite and its
 * quad, the confirm/back/grid sprites, the two command labels, the
 * character grid, and show the entry. */
void func_801CADC8(void) {
    func_801C7AA4(3, 0x10, 0x9A, 0xC0, 0x3C, 1, 1, 4, 0);
    func_801C9FC0(0x1D);
    func_8002675C(D_800625A0->sheet, 0x14B, D_800625A0->name_entry->cursor,
                  D_800625A0->buffer_index, 0, 0, 0x1000);
    func_801C6408(D_800625A0->name_entry->cursor_at,
                  D_800625A0->name_entry->cursor[D_800625A0->buffer_index].x0 + 0x18,
                  D_800625A0->name_entry->cursor[D_800625A0->buffer_index].y0 + 0x9E,
                  D_800625A0->name_entry->cursor[D_800625A0->buffer_index].x1 -
                      D_800625A0->name_entry->cursor[D_800625A0->buffer_index].x0,
                  D_800625A0->name_entry->cursor[D_800625A0->buffer_index].y3 -
                      D_800625A0->name_entry->cursor[D_800625A0->buffer_index].y0);
    D_800625A0->name_entry->cursor_buffer = D_800625A0->buffer_index;
    func_8002675C(D_800625A0->sheet, 0xF9, D_800625A0->name_entry->confirm,
                  D_800625A0->buffer_index, 0xE8, 0xB6, 0x1000);
    func_8002675C(D_800625A0->sheet, 0xFC, D_800625A0->name_entry->back,
                  D_800625A0->buffer_index, 0xE0, 0xC6, 0x1000);
    D_800625A0->name_entry->frame_count =
        func_8002675C(D_800625A0->sheet, 0xF0, D_800625A0->name_entry->frame,
                      D_800625A0->buffer_index, 0xF4, 0x6E, 0x1000);
    D_800625A0->name_entry->parts_buffer = D_800625A0->buffer_index;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->x0 = 0xF8;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->y0 = 0xB6;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->x1 = D_800625A0->labels[0].width + 0xF8;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->y1 = 0xB6;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->x2 = 0xF8;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->y2 = 0xC3;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->x3 = D_800625A0->labels[0].width + 0xF8;
    (D_800625A0->labels[0].polys + D_800625A0->buffer_index)->y3 = 0xC3;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->x0 = 0xF0;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->y0 = 0xC6;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->x1 = D_800625A0->labels[3].width + 0xF0;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->y1 = 0xC6;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->x2 = 0xF0;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->y2 = 0xD3;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->x3 = D_800625A0->labels[3].width + 0xF0;
    (D_800625A0->labels[3].polys + D_800625A0->buffer_index)->y3 = 0xD3;
    D_800625A0->labels[3].buffer = D_800625A0->buffer_index;
    func_801CA558();
    D_800625A0->name_entry->shown = 1;
    D_800625A0->flags->name_entry_shown = 1;
}

/* Render the name being entered (text codes `codes`) into VRAM at (0x180, 0xEA). */
void func_801CB1C4(u8 *codes) {
    RECT rect;
    u8 *image = func_80031BDC(0x3F6, 0);

    bzero(image, 0x3F6);
    func_80034EAC(codes, image, 0x24, 0);
    rect.x = 0x180;
    rect.y = 0xEA;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, image);
    DrawSync(0);
    func_800320E8(image);
}

/* Start a name entry: fill the entry buffer `codes` with blanks (code 0x1C)
 * ending in the terminator, and copy the character's current name. */
void func_801CB25C(u8 *codes, u8 *name) {
    s32 i;

    for (i = 0; i < 20; i += 2) {
        codes[i] = 0x1C;
        codes[i + 1] = 0;
        name[i] = D_8006D634.names[D_80059171][i];
        name[i + 1] = (D_8006D634.names[D_80059171] + 1)[i];
    }
    codes[18] = 0x1F;
    codes[19] = 0;
}

/* Number of two-byte text codes before the terminator (0x1F 0x00). */
u8 func_801CB2F0(u8 *codes) {
    s32 i;

    for (i = 0; i < 20; i += 2) {
        if (codes[i] == 0x1F && codes[i + 1] == 0) {
            break;
        }
    }
    return i / 2;
}

/* The name entry loop: zoom the view in, open the grid, move the cursor over
 * the character grid (skipping blank cells) and edit the name until it is
 * confirmed with a non-empty name; then store it for the three characters
 * of the portrait list (trailing blanks cut), and close the screen.
 * Confirming (11) is written first in the switch and again where a blank
 * cell is picked (4); cross-jumping later merges the two copies into the
 * one after case 4, but the copies give 15 and `running` the references
 * that put them in s5 and fp (0xCF stays without a register and is
 * reloaded at each compare), and make 15 the first constant loop.c hoists. */
void func_801CB33C(void) {
    u8 codes[24];
    u8 name[24];
    s32 prev_col = 0xFF;
    s32 prev_row = 0xFF;
    s32 col = 22;
    s32 row = 4;
    u8 dirty;
    u8 running;
    u8 c;
    s32 index;
    s32 i;
    s32 k;
    s32 last;

    func_801CB25C(codes, name);
    dirty = 0;
    func_801CADC8();
    func_801CB1C4(name);
    running = 1;
    func_801C9F60();
    while (D_800625A0->view_motion) {
        func_801C9C34();
    }
    func_801C7AA4(2, 0x38, 0x26, 0xD0, 0x60, 1, 0, 4, 0);
    while (!D_800625A0->growth[2]->done) {
        func_801C9C34();
    }
    D_800625A0->name_entry->grid_shown = 1;
    D_800625A0->flags->labels_shown[0] = 1;
    D_800625A0->flags->labels_shown[3] = 1;
    func_801C9D5C(1);
    D_800625A0->flags->markers_shown = 1;
    while (running) {
        if (col != prev_col || row != prev_row) {
            func_8002675C(D_800625A0->sheet, 0x108, D_800625A0->markers,
                          D_800625A0->buffer_index, col * 8 + 0x48, row * 16 + 0x36, 0x800);
            (D_800625A0->name_entry->line_a + D_800625A0->buffer_index)->x0 = col * 8 + 0x44;
            (D_800625A0->name_entry->line_a + D_800625A0->buffer_index)->y0 = row * 16 + 0x2E;
            (D_800625A0->name_entry->line_a + D_800625A0->buffer_index)->x1 = col * 8 + 0x4C;
            (D_800625A0->name_entry->line_a + D_800625A0->buffer_index)->y1 = row * 16 + 0x2E;
            (D_800625A0->name_entry->line_a + D_800625A0->buffer_index)->x2 = col * 8 + 0x4C;
            (D_800625A0->name_entry->line_a + D_800625A0->buffer_index)->y2 = row * 16 + 0x3A;
            (D_800625A0->name_entry->line_b + D_800625A0->buffer_index)->x0 = col * 8 + 0x44;
            (D_800625A0->name_entry->line_b + D_800625A0->buffer_index)->y0 = row * 16 + 0x2E;
            (D_800625A0->name_entry->line_b + D_800625A0->buffer_index)->x1 = col * 8 + 0x44;
            (D_800625A0->name_entry->line_b + D_800625A0->buffer_index)->y1 = row * 16 + 0x3A;
            (D_800625A0->name_entry->line_b + D_800625A0->buffer_index)->x2 = col * 8 + 0x4C;
            (D_800625A0->name_entry->line_b + D_800625A0->buffer_index)->y2 = row * 16 + 0x3A;
            D_800625A0->name_entry->lines_buffer = D_800625A0->buffer_index;
            prev_col = col;
            D_800625A0->markers->buffer[0] = D_800625A0->buffer_index;
            prev_row = row;
            D_800625A0->markers->shown[0] = 1;
        }
        if (dirty) {
            func_801CB1C4(name);
            dirty = 0;
        }
        func_801C9C34();
        switch (D_800625A0->input) {
        case 11:
            if (codes[0] != 0xF) {
                running = 0;
            }
            break;
        case 5:
            if (D_800625A0->name_entry->length) {
                D_800625A0->name_entry->length--;
            }
            codes[D_800625A0->name_entry->length * 2] = 0xF;
            codes[D_800625A0->name_entry->length * 2 + 1] = 0;
            func_80033B34(codes, name, func_801CB2F0(codes));
            dirty = 1;
            break;
        case 4:
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = D_801CBEC0[index];
            if (c != 0x1F) {
                if (D_800625A0->name_entry->length < D_800625A0->name_entry->max_length) {
                    if (c == 0xCF || c == 0xF) {
                        D_801CBEC0[index] = 0xC3;
                    }
                    codes[D_800625A0->name_entry->length * 2] = D_801CBEC0[index];
                    codes[D_800625A0->name_entry->length * 2 + 1] = 0;
                    func_80033B34(codes, name, func_801CB2F0(codes));
                    dirty = 1;
                    D_800625A0->name_entry->length++;
                    func_801C989C(2);
                } else {
                    func_801C989C(4);
                }
                break;
            }
            if (codes[0] != 0xF) {
                running = 0;
            }
            break;
        case 0:
            col++;
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = D_801CBEC0[index];
            if (c == 0xFF) {
                col = 0;
            } else if (c == 0xF || c == 0xCF) {
                col++;
            }
            break;
        case 2:
            if (--col < 0) {
                col = 22;
            } else {
                index = (row + (col / 6) * 9) * 6 + col % 6;
                c = D_801CBEC0[index];
                if (c == 0xF || c == 0xCF) {
                    col--;
                }
            }
            break;
        case 1:
            if (++row >= 5) {
                row = 0;
            }
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = D_801CBEC0[index];
            if (c == 0xCF) {
                row = 0;
            } else if (c == 0xF) {
                row++;
            }
            break;
        case 3:
            if (--row < 0) {
                row = 4;
            }
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = D_801CBEC0[index];
            if (c == 0xF || c == 0xCF) {
                row--;
            }
            break;
        case 9:
            if (++D_800625A0->name_entry->length > D_800625A0->name_entry->max_length) {
                D_800625A0->name_entry->length--;
            }
            break;
        case 10:
            if (D_800625A0->name_entry->length) {
                D_800625A0->name_entry->length--;
            }
            break;
        }
    }
    for (i = 0, last = 0; i < 3; i++) {
        for (k = 0; k < 20; k++) {
            D_8006D634.names[D_800625A0->portraits[i]][k] = 0;
        }
        for (k = 0; k < 18; k++) {
            D_8006D634.names[D_800625A0->portraits[i]][k] = name[k];
            if (name[k] == 0) {
                break;
            }
            if (name[k] != 0x4F) {
                last = k;
            }
        }
        for (k = last + 1; k < 20; k++) {
            D_8006D634.names[D_800625A0->portraits[i]][k] = 0;
        }
    }
    D_800625A0->flags->labels_shown[0] = 0;
    D_800625A0->flags->labels_shown[3] = 0;
    func_801C9F1C();
    D_800625A0->name_entry->grid_shown = 0;
    func_801C7A18(2);
    func_801C9F90();
    while (D_800625A0->offset.vz < 0x600) {
        func_801C9C34();
    }
    D_800625A0->flags->name_entry_shown = 0;
    func_801CA39C();
    func_801C7A18(3);
}

/* Overlay entry: allocate and set up the name entry screen, run it and leave. */
void func_801CBDBC(void) {
    MenuState *state;

    func_801C505C(1);
    func_801C50C0(1);
    func_801C5124(1);
    func_801C5188(1);
    func_801C51EC(1);
    func_801C5250(1);
    func_801C52B4(1);
    state = D_800625A0;
    state->images->screen.x = 0x2C0;
    state->images->screen.y = 0x100;
    state->images->screen.w = 0x140;
    state->images->screen.h = 0xE0;
    state->prims->width = 0x40;
    func_801C58B8();
    func_801C5A30();
    func_801C5EAC();
    func_801C60BC();
    func_801C5F08();
    D_800625A0->drawing = 1;
    D_800625A0->sounds = 1;
    func_801CB33C();
    func_801CA400();
}
