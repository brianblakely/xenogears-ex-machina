/* Overlay 2598 (Disc 1 slot 2598, Disc 2 slot 2593; loaded at 0x801c5000):
 * the party member selection screen. It lists the three party slots and the
 * characters that may join (flags 8006f364 & 8006f366), lets the player swap
 * members between the two lists, and writes the chosen party back to
 * 8006f368. It shares the menu state (*D_800625A0) with the other menu
 * overlays. */
#include "common.h"
#include "party_menu.h"

extern u16 D_801CB57C[];

void func_801C9A08(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height);
void func_801C9F80(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height);
void func_801CA24C(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height);
void func_801CAD14(void);
void func_801CA690(u8 first);
u8 func_801CAB48(u8 list, s32 row, s32 page, u8 prev_list, s32 prev_row, s32 prev_page);
extern u8 D_801CB400[];
extern s32 D_801CB180[]; /* marker home x */
extern s32 D_801CB190[]; /* marker home y */
extern s32 D_801CB1A0[]; /* marker x by list row */
extern s32 D_801CB1C4[]; /* marker y by list row */
extern s32 D_801CB1E8[]; /* member panel layout x */
extern s32 D_801CB22C[]; /* party panel layout x */
extern s32 D_801CB274[]; /* member panel layout y */
extern s32 D_801CB2B8[]; /* party panel layout y */
extern s32 D_801CB344[]; /* name texture x (in 4-pixel units) by character pair */
extern s32 D_801CB390[]; /* name texture y by character pair */
extern s32 D_801CB3DC[]; /* status panel layout sprites (0xFFFF none) */
extern s32 D_801CB404[]; /* file cursor -> slot */
extern s32 D_801CB47C[]; /* slot x */
extern s32 D_801CB4FC[]; /* slot y */

/* Test character `index`'s bit (table D_801CB57C) in `flags`. */
s32 func_801C5018(s32 flags, u8 index) {
    return D_801CB57C[index] & flags;
}

/* Allocate (nonzero) or release the 0x5034-byte work block. */
void func_801C5034(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->work = block;
        bzero(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->work);
    }
}

/* Allocate (nonzero) or release the menu flag block. */
void func_801C5098(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6C, 0);
        D_800625A0->flags = block;
        bzero(block, 0x6C);
    } else {
        func_800320E8(D_800625A0->flags);
    }
}

/* Allocate (nonzero) or release the 0x1194-byte block at state + 0x350. */
void func_801C50FC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->block_350 = block;
        bzero(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->block_350);
    }
}

/* Allocate (nonzero) or release the 0x140C-byte block at state + 0x354. */
void func_801C5160(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140C, 0);
        D_800625A0->block_354 = block;
        bzero(block, 0x140C);
    } else {
        func_800320E8(D_800625A0->block_354);
    }
}

/* Allocate (nonzero) or release the 0xCC-byte block at state + 0x330. */
void func_801C51C4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xCC, 0);
        D_800625A0->block_330 = block;
        bzero(block, 0xCC);
    } else {
        func_800320E8(D_800625A0->block_330);
    }
}

/* Allocate (nonzero) or release the 0x15C-byte block at state + 0x348. */
void func_801C5228(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15C, 0);
        D_800625A0->backdrop = block;
        bzero(block, 0x15C);
    } else {
        func_800320E8(D_800625A0->backdrop);
    }
}

/* Allocate (nonzero) or release the six member and three party panels. */
void func_801C528C(u8 allocate) {
    s32 i;

    if (allocate) {
        for (i = 0; i < 6; i++) {
            void *block = func_80031BDC(0xBEC, 0);
            D_800625A0->member_panels[i] = block;
            bzero(block, 0xBEC);
        }
        for (i = 0; i < 3; i++) {
            void *block = func_80031BDC(0xBEC, 0);
            D_800625A0->party_panels[i] = block;
            bzero(block, 0xBEC);
        }
    } else {
        for (i = 0; i < 6; i++) {
            func_800320E8(D_800625A0->member_panels[i]);
        }
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->party_panels[i]);
        }
    }
}

/* Load the screen's resources: the card icon TIM into the work block's save
 * header, the palette data, the sprite sheet and label texts, and the menu
 * sound bank when sound is on. */
void func_801C5390(void) {
    u8 unused[0x60]; /* unused in the original; reserves 96 bytes */
    MenuArchive *archive = D_8005945C;
    void *data;

    func_8003342C(archive);
    data = func_80032E88(archive->entry[0], 1);
    OpenTIM(data);
    ReadTIM(&D_800625A0->work->tim);
    D_800625A0->work->magic[0] = 'S';
    D_800625A0->work->magic[1] = 'C';
    D_800625A0->work->icon_type = 0x11;
    D_800625A0->work->blocks = 1;
    bzero(D_800625A0->work->title, 0x5C);
    memmove(D_800625A0->work->clut, D_800625A0->work->tim.caddr, 0x20);
    memmove(D_800625A0->work->icon, D_800625A0->work->tim.paddr, 0x80);
    func_800320E8(data);
    data = func_80032E88(archive->entry[1], 1);
    func_8002DD20(data);
    func_800320E8(data);
    D_800625A0->sprite_sheet = func_80032E88(archive->entry[2], 0);
    D_800625A0->label_text = func_80032E88(archive->entry[3], 0);
    if (D_80059178) {
        func_80028470(0x10, 2);
        D_8006259C = func_80031BDC(func_800288EC(5), 0);
        func_800295D8(5, D_8006259C, 0, 0x80);
        func_80028A60(0);
        func_80028470(0x10, 0);
        func_80038428(D_8006259C);
    }
    D_800625A0->effect_bank = D_8006259C;
    func_800320E8(archive);
}

/* Reset the screen state, mark which characters may join, take the current
 * party (members that may not join become empty) and load the resources. */
void func_801C559C(void) {
    s32 i;
    u16 flags;
    s32 id;

    D_800625A0->top_cursor = 4;
    D_800625A0->b_337 = 0xFF;
    D_800625A0->card_poll_timer = 60;
    D_800625A0->b_334 = 0;
    D_800625A0->b_335 = 0;
    flags = D_8006F364 & D_8006F366 & 0x7FF;
    for (i = 0; i < 16; i++) {
        if (func_801C5018(flags, i) & 0xFFFF) {
            D_800625A0->available[i] = 1;
        } else {
            D_800625A0->available[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = D_8006F368[i];
        if (id != 0xFF && D_800625A0->available[id]) {
            D_800625A0->flags->party[i] = id;
        } else {
            D_800625A0->flags->party[i] = 0xFF;
        }
    }
    func_801C5390();
}

/* Start building draw buffer 0. */
void func_801C5714(void) {
    D_800625A0->buffer_index = 0;
}

/* Upload the text CLUT: 16 black entries except white entry 1 at (0, 0x1C0). */
void func_801C5724(void) {
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
 * bit 7 is set, with the highlight from the low bits. */
#ifdef NON_MATCHING
void func_801C57A0(MenuLabel *label, s32 index, s32 row, u8 mode) {
    POLY_FT4 *poly = label->poly;
    s32 column = index & 1;
    s32 line = index / 2;
    s32 u = (line & 1) << 7;
    s32 i = 0;
    s32 dim;
    s32 v;
    s32 list_u;

loop:
    dim = 0;
    SetPolyFT4(poly);
    SetSemiTrans(poly, 0);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
    if (mode == 0) {
        label->highlight = column;
        poly->tpage = GetTPage(0, 0, 0x140, 0);
        poly->u0 = u;
        v = ((index + row) / 4) * 13;
        poly->v0 = v;
        poly->u1 = u + label->width;
        poly->v1 = v;
        poly->u2 = u;
        v += 13;
        poly->v2 = v;
        poly->u3 = u + label->width;
        poly->v3 = v;
    } else {
        if (!(mode & 0x80)) {
            dim = 0x20;
            SetSemiTrans(poly, 1);
            poly->r0 = dim;
            poly->g0 = dim;
            poly->b0 = dim;
        }
        label->highlight = (mode & 0x7F) - 1;
        poly->tpage = dim | GetTPage(0, 0, 0x180, 0x80);
        list_u = column * 0x60;
        v = line * 13 + row;
        poly->u0 = list_u;
        poly->v0 = v;
        poly->v1 = v;
        v += 13;
        poly->u2 = list_u;
        poly->v2 = v;
        poly->u1 = list_u + label->width;
        poly->v3 = v;
        poly->u3 = list_u + label->width;
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
INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801C57A0);
#endif

/* Render `count` label texts (pairs of text ids) into VRAM, two per line,
 * and set up their quads. */
void func_801C59E0(MenuLabel *labels, u8 *text_ids, s32 row, s32 count) {
    s32 i;
    RECT *rect;

    for (i = 0; i < count; i += 2) {
        labels[i].width = func_80034EAC(func_80033728(D_800625A0->label_text, text_ids[i]),
                                        D_800625A0->labels[0].image, 0x18, 0);
        rect = &labels[i].rect;
        labels[i + 1].width =
            func_80034EAC(func_80033728(D_800625A0->label_text, text_ids[i + 1]),
                          D_800625A0->labels[0].image, 0x18, 1);
        labels[i].rect.x = (((i / 2) & 1) << 5) + 0x140;
        labels[i].rect.y = ((i + row) / 4) * 13;
        labels[i].rect.w = 0x1C;
        labels[i].rect.h = 13;
        labels[i + 1].rect = labels[i].rect;
        func_801C57A0(&labels[i], i, row, 0);
        func_801C57A0(&labels[i + 1], i + 1, row, 0);
        LoadImage(rect, D_800625A0->labels[0].image);
        DrawSync(0);
    }
}

/* Set up the four command labels and the text CLUT. */
void func_801C5B90(void) {
    func_80033698(0, 0x1D1);
    D_800625A0->labels[0].image = func_80031BDC(0x38E, 0);
    func_801C59E0(D_800625A0->labels, D_801CB400, 0, 4);
    func_801C5724();
}

/* Look up the four cursor/frame sprites of the sheet. */
void func_801C5BEC(void) {
    s32 unused[10]; /* unused in the original; reserves 40 bytes */

    func_80026338(D_800625A0->sprite_sheet, 0xFE, &D_800625A0->sprites[0].unk0,
                  &D_800625A0->sprites[0].tpage_mode, &D_800625A0->sprites[0].clut_x,
                  &D_800625A0->sprites[0].clut_y, &D_800625A0->sprites[0].page_x,
                  &D_800625A0->sprites[0].page_y);
    func_80026338(D_800625A0->sprite_sheet, 0x103, &D_800625A0->sprites[1].unk0,
                  &D_800625A0->sprites[1].tpage_mode, &D_800625A0->sprites[1].clut_x,
                  &D_800625A0->sprites[1].clut_y, &D_800625A0->sprites[1].page_x,
                  &D_800625A0->sprites[1].page_y);
    func_80026338(D_800625A0->sprite_sheet, 0x100, &D_800625A0->sprites[2].unk0,
                  &D_800625A0->sprites[2].tpage_mode, &D_800625A0->sprites[2].clut_x,
                  &D_800625A0->sprites[2].clut_y, &D_800625A0->sprites[2].page_x,
                  &D_800625A0->sprites[2].page_y);
    func_80026338(D_800625A0->sprite_sheet, 0x101, &D_800625A0->sprites[3].unk0,
                  &D_800625A0->sprites[3].tpage_mode, &D_800625A0->sprites[3].clut_x,
                  &D_800625A0->sprites[3].clut_y, &D_800625A0->sprites[3].page_x,
                  &D_800625A0->sprites[3].page_y);
}

/* Clear the party list's flags 3 and 4. */
void func_801C5CF4(void) {
    D_800625A0->flags->flag_4 = 0;
    D_800625A0->flags->flag_3 = 0;
}

/* Make `poly` a gouraud quad fading from (r, g, b) at the top to black. */
void func_801C5D24(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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
void func_801C5DA0(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    func_801C5CF4();
    for (i = 0; i < 2; i++) {
        func_801C5D24(&D_800625A0->backdrop->gradient[i], 0x80, 0x80, 0);
        SetSemiTrans(&D_800625A0->backdrop->gradient[i], 1);
        SetLineF3(&D_800625A0->backdrop->line_a[i]);
        (D_800625A0->backdrop->line_a + i)->r0 = 0;
        (D_800625A0->backdrop->line_a + i)->g0 = 0x40;
        (D_800625A0->backdrop->line_a + i)->b0 = 0;
        SetLineF3(&D_800625A0->backdrop->line_b[i]);
        (D_800625A0->backdrop->line_b + i)->r0 = 0;
        (D_800625A0->backdrop->line_b + i)->g0 = 0x40;
        (D_800625A0->backdrop->line_b + i)->b0 = 0;
        SetPolyF4(&D_800625A0->backdrop->fade[i]);
        (D_800625A0->backdrop->fade + i)->x0 = 0;
        (D_800625A0->backdrop->fade + i)->y0 = 0;
        (D_800625A0->backdrop->fade + i)->x1 = 0x140;
        (D_800625A0->backdrop->fade + i)->y1 = 0;
        (D_800625A0->backdrop->fade + i)->x2 = 0;
        (D_800625A0->backdrop->fade + i)->y2 = 0xE0;
        (D_800625A0->backdrop->fade + i)->x3 = 0x140;
        (D_800625A0->backdrop->fade + i)->y3 = 0xE0;
        (D_800625A0->backdrop->fade + i)->r0 = 0x80;
        (D_800625A0->backdrop->fade + i)->g0 = 0x80;
        (D_800625A0->backdrop->fade + i)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->backdrop->fade[i], 1);
        SetDrawMode(&D_800625A0->backdrop->mode_a[i], 0, 0, GetTPage(0, 0, 0x140, 0x80),
                      &window);
        SetDrawMode(&D_800625A0->backdrop->mode_b[i], 0, 0, GetTPage(0, 2, 0x180, 0),
                      &window);
    }
}

/* Place a quad's four vertices around the screen centre (160, 112). */
void func_801C60EC(SVECTOR *v, u16 x, u16 y, u16 w, u16 h) {
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
void func_801C6144(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Set up panel `index`'s primitives: its translucent grey fill and draw
 * modes, and the textured edge strips from the four frame sprites. */
void func_801C618C(u8 index) {
    Panel *panel = D_800625A0->panels[index];
    RECT window;
    u8 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    D_800625A0->flags->panel_20[index] = 0;
    D_800625A0->flags->panel_27[index] = 0;
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
                      GetTPage(0, 0, D_800625A0->sprites[0].page_x,
                                    D_800625A0->sprites[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->edge[0][i]);
        SetShadeTex(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xFF;
        (panel->edge[0] + i)->g0 = 0xFF;
        (panel->edge[0] + i)->b0 = 0xFF;
        (panel->edge[0] + i)->tpage =
            GetTPage(D_800625A0->sprites[0].tpage_mode, 0, D_800625A0->sprites[0].page_x,
                          D_800625A0->sprites[0].page_y);
        (panel->edge[0] + i)->clut =
            GetClut(D_800625A0->sprites[0].clut_x, D_800625A0->sprites[0].clut_y);
        SetPolyFT4(&panel->edge[1][i]);
        SetShadeTex(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xFF;
        (panel->edge[1] + i)->g0 = 0xFF;
        (panel->edge[1] + i)->b0 = 0xFF;
        (panel->edge[1] + i)->tpage =
            GetTPage(D_800625A0->sprites[1].tpage_mode, 0, D_800625A0->sprites[1].page_x,
                          D_800625A0->sprites[1].page_y);
        (panel->edge[1] + i)->clut =
            GetClut(D_800625A0->sprites[1].clut_x, D_800625A0->sprites[1].clut_y);
        SetPolyFT4(&panel->edge[2][i]);
        SetShadeTex(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xFF;
        (panel->edge[2] + i)->g0 = 0xFF;
        (panel->edge[2] + i)->b0 = 0xFF;
        (panel->edge[2] + i)->tpage =
            GetTPage(D_800625A0->sprites[2].tpage_mode, 0, D_800625A0->sprites[2].page_x,
                          D_800625A0->sprites[2].page_y);
        (panel->edge[2] + i)->clut =
            GetClut(D_800625A0->sprites[2].clut_x, D_800625A0->sprites[2].clut_y);
        SetPolyFT4(&panel->edge[3][i]);
        SetShadeTex(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xFF;
        (panel->edge[3] + i)->g0 = 0xFF;
        (panel->edge[3] + i)->b0 = 0xFF;
        (panel->edge[3] + i)->tpage =
            GetTPage(D_800625A0->sprites[3].tpage_mode, 0, D_800625A0->sprites[3].page_x,
                          D_800625A0->sprites[3].page_y);
        (panel->edge[3] + i)->clut =
            GetClut(D_800625A0->sprites[3].clut_x, D_800625A0->sprites[3].clut_y);
    }
}

/* Build panel `index`'s frame sprites for this buffer at (x, y) with height
 * `h` and place its top, bottom and side vectors. */
void func_801C64A8(u8 index, u16 x, u16 y, s32 unused, u16 h) {
    Panel *panel = D_800625A0->panels[index];

    func_8002675C(D_800625A0->sprite_sheet, 0x105, panel->frame_ends, D_800625A0->buffer_index,
                  x, y, 0x1000);
    func_800263E4(D_800625A0->sprite_sheet, 0x105, &panel->frame_ends[2],
                  D_800625A0->buffer_index, x, y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sprite_sheet, 0x106, panel->frame_side, D_800625A0->buffer_index,
                  x, y + 8, 0x1000);
    func_801C60EC(&panel->ends_at[0], x, y, 8, 8);
    func_801C60EC(&panel->ends_at[4], x, y + h, 8, -8);
    func_801C60EC(panel->side_at, x, y + 8, 8, h - 8);
}

/* Build panel `index`'s four corner sprites for this buffer and place them
 * around the rectangle (x, y, w, h). */
void func_801C660C(u8 index, u16 x, u16 y, u16 w, u16 h) {
    Panel *panel = D_800625A0->panels[index];
    s32 i;

    panel->corner_parts = 0;
    panel->corner_parts += func_8002675C(D_800625A0->sprite_sheet, 0xFD, panel->corner,
                                         D_800625A0->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        func_8002675C(D_800625A0->sprite_sheet, 0xFF, &panel->corner[panel->corner_parts * 2],
                      D_800625A0->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        func_8002675C(D_800625A0->sprite_sheet, 0x102, &panel->corner[panel->corner_parts * 2],
                      D_800625A0->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        func_8002675C(D_800625A0->sprite_sheet, 0x104, &panel->corner[panel->corner_parts * 2],
                      D_800625A0->buffer_index, 0, 0, 0x1000);
    func_801C60EC(&panel->corner_at[0], x - 8, y + 8, 16, -16);
    func_801C60EC(&panel->corner_at[4], x + w + 8, y + 8, -16, -16);
    func_801C60EC(&panel->corner_at[8], x - 8, y + h - 8, 16, 16);
    func_801C60EC(&panel->corner_at[12], x + w + 8, y + h - 8, -16, 16);
    for (i = 0; i < 4; i++) {
        func_801C6144(&panel->corner[i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s top edge pieces for this buffer and place them in two
 * halves along the top of (x, y, w). */
void func_801C6854(u8 index, u16 x, u16 y, u16 w) {
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
    func_801C60EC(panel->edge_at[0][0], x + 8, y - 8, half, 16);
    func_801C60EC(panel->edge_at[0][1], x + (half + 8), y - 8, half, 16);
    for (i = 0; i < 2; i++) {
        func_801C6144(&panel->edge[0][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s bottom edge pieces for this buffer and place them in
 * two halves along the bottom of (x, y, w, h). */
void func_801C6B98(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    func_801C60EC(panel->edge_at[1][0], x + 8, y + h - 8, half, 16);
    func_801C60EC(panel->edge_at[1][1], x + (half + 8), y + h - 8, half, 16);
    for (i = 0; i < 2; i++) {
        func_801C6144(&panel->edge[1][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s left edge pieces for this buffer and place them in two
 * halves down the left of (x, y, h). */
void func_801C6EE4(u8 index, u16 x, u16 y, u16 h) {
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
    func_801C60EC(panel->edge_at[2][0], x - 8, y + 8, 16, half);
    func_801C60EC(panel->edge_at[2][1], x - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        func_801C6144(&panel->edge[2][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Map panel `index`'s right edge pieces for this buffer and place them in two
 * halves down the right of (x, y, w, h). */
void func_801C722C(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    func_801C60EC(panel->edge_at[3][0], x + w - 8, y + 8, 16, half);
    func_801C60EC(panel->edge_at[3][1], x + w - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        func_801C6144(&panel->edge[3][i * 2 + D_800625A0->buffer_index]);
    }
}

/* Lay out panel `index` at (x, y, w, h) for this buffer: fill, corners and
 * edges, the frame sprites when `framed`, and mark it shown. */
void func_801C7578(u8 index, u16 x, u16 y, u16 w, u16 h, u8 style, s32 param, u8 framed) {
    Panel *panel = D_800625A0->panels[index];

    D_800625A0->flags->panel_20[index] = 0;
    func_801C60EC(panel->fill_at, x, y, w, h);
    func_801C660C(index, x, y, w, h);
    func_801C6854(index, x, y, w);
    func_801C6B98(index, x, y, w, h);
    func_801C6EE4(index, x, y, h);
    func_801C722C(index, x, y, w, h);
    if (framed) {
        func_801C64A8(index, x, y, w, h);
    }
    panel->framed = framed;
    panel->style = style;
    panel->param = param;
    panel->buffer = D_800625A0->buffer_index;
    D_800625A0->flags->panel_20[index] = 1;
}

/* Hide panel `index` and release its block and growth record. */
void func_801C76FC(u8 index) {
    D_800625A0->flags->panel_20[index] = 0;
    D_800625A0->flags->panel_27[index] = 0;
    func_800320E8(D_800625A0->panels[index]);
    func_800320E8(D_800625A0->growth[index]);
}

/* Open panel `index` (allocating it first unless it is 0 or 1): either
 * start its growth animation toward (x, y, w, h) or lay it out at once. */
void func_801C7788(u8 index, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 style, s32 param,
                   u8 framed) {
    PanelGrowth *growth;

    if (index >= 2) {
        D_800625A0->panels[index] = func_80031BDC(0x720, 0);
        bzero(D_800625A0->panels[index], 0x720);
        D_800625A0->growth[index] = func_80031BDC(0x18, 0);
        bzero(D_800625A0->growth[index], 0x18);
        func_801C618C(index);
    }
    growth = D_800625A0->growth[index];
    if (animate) {
        growth->index = index;
        growth->open = 0;
        growth->x = x;
        growth->y = y;
        growth->w = w;
        growth->h = h;
        growth->cur_w = 0;
        growth->cur_h = 0;
        D_800625A0->flags->panel_27[index] = 1;
        growth->style = style;
        growth->param = param;
    } else {
        func_801C7578(index, x, y, w, h, style, param, framed);
    }
}

/* Grow every opening panel by 32 in width and height per frame until it
 * reaches its size, laying it out centred on its final rectangle. */
void func_801C790C(void) {
    s32 i;
    PanelGrowth *growth;
    u8 done;

    for (i = 0; i < 7; i++) {
        growth = D_800625A0->growth[i];
        if (D_800625A0->flags->panel_27[i] && !growth->open) {
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
                growth->open = 1;
            }
            func_801C7578(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->style, growth->param, growth->framed);
        }
    }
}

/* Draw the four cursor markers when markers are on; a marker that follows
 * the file cursor is first moved to the selected slot's position. */
void func_801C7A58(void) {
    s32 i;
    MenuState *state;

    if (D_800625A0->flags->markers_on) {
        for (i = 0; i < 4; i++) {
            state = D_800625A0;
            if (state->markers->shown[i]) {
                if (state->markers->follow[i]) {
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x0 =
                        D_801CB47C[D_801CB404[state->work->cursor]] + 8;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y0 =
                        D_801CB4FC[D_801CB404[state->work->cursor]] - 6;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x1 =
                        D_801CB47C[D_801CB404[state->work->cursor]] + 24;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y1 =
                        D_801CB4FC[D_801CB404[state->work->cursor]] - 6;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x2 =
                        D_801CB47C[D_801CB404[state->work->cursor]] + 8;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y2 =
                        D_801CB4FC[D_801CB404[state->work->cursor]] + 10;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x3 =
                        D_801CB47C[D_801CB404[state->work->cursor]] + 24;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y3 =
                        D_801CB4FC[D_801CB404[state->work->cursor]] + 10;
                }
                AddPrim(&D_800625A0->draw_env->ot[4],
                              &D_800625A0->markers->poly[i * 2 + D_800625A0->markers->buffer[i]]);
            }
        }
    }
}

/* Draw the shown command labels. */
void func_801C7DA8(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->flags->label_shown[i]) {
            AddPrim(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->labels[i].poly[D_800625A0->labels[i].buffer]);
        }
    }
}

/* Draw the shown list labels. */
void func_801C7E38(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->flags->list_label_shown[i]) {
            AddPrim(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->list_labels[i].poly[D_800625A0->list_labels[i].buffer]);
        }
    }
}

/* Draw the shown row labels, projecting the 3D ones through the GTE first. */
void func_801C7EC8(void) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->row_label_shown[i]) {
            if (D_800625A0->row_labels[i].projected) {
                MenuLabel *label = &D_800625A0->row_labels[i];

                RotTransPers4(&label->corners[0], &label->corners[1], &label->corners[2],
                              &label->corners[3],
                              &label->poly[D_800625A0->row_labels[i].buffer].x0,
                              &label->poly[D_800625A0->row_labels[i].buffer].x1,
                              &label->poly[D_800625A0->row_labels[i].buffer].x2,
                              &label->poly[D_800625A0->row_labels[i].buffer].x3, &depth, &flag);
                AddPrim(&D_800625A0->draw_env->ot[4],
                              &D_800625A0->row_labels[i].poly[D_800625A0->row_labels[i].buffer]);
            } else {
                AddPrim(&D_800625A0->draw_env->ot[4],
                              &D_800625A0->row_labels[i].poly[D_800625A0->row_labels[i].buffer]);
            }
        }
    }
}

/* Draw this buffer's fade quad and the second draw mode. */
void func_801C8040(void) {
    AddPrim(&D_800625A0->draw_env->ot[8],
                  &D_800625A0->backdrop->fade[D_800625A0->buffer_index]);
    AddPrim(&D_800625A0->draw_env->ot[8],
                  &D_800625A0->backdrop->mode_b[D_800625A0->buffer_index]);
}

/* Draw a shown status panel for this buffer: face, label, layout sprites and
 * the level/HP/EP digits, plus the extra sprites when `extra` is set. */
void func_801C80BC(StatusPanel *panel, u8 extra) {
    s32 i;

    if (panel->shown) {
        AddPrim(&D_800625A0->draw_env->ot[4], &panel->face[panel->buffer]);
        AddPrim(&D_800625A0->draw_env->ot[4], &panel->label[panel->buffer]);
        for (i = 0; i < panel->layout_count; i++) {
            AddPrim(&D_800625A0->draw_env->ot[4], &panel->layout[i * 2 + panel->buffer]);
        }
        for (i = 0; i < panel->level_count; i++) {
            AddPrim(&D_800625A0->draw_env->ot[4], &panel->level[i * 2 + panel->buffer]);
        }
        for (i = 0; i < panel->hp_count; i++) {
            AddPrim(&D_800625A0->draw_env->ot[4], &panel->hp[i * 2 + panel->buffer]);
        }
        for (i = 0; i < panel->hp_max_count; i++) {
            AddPrim(&D_800625A0->draw_env->ot[4], &panel->hp_max[i * 2 + panel->buffer]);
        }
        for (i = 0; i < panel->ep_count; i++) {
            AddPrim(&D_800625A0->draw_env->ot[4], &panel->ep[i * 2 + panel->buffer]);
        }
        for (i = 0; i < panel->ep_max_count; i++) {
            AddPrim(&D_800625A0->draw_env->ot[4], &panel->ep_max[i * 2 + panel->buffer]);
        }
        if (extra) {
            for (i = 0; i < panel->extra_count; i++) {
                AddPrim(&D_800625A0->draw_env->ot[4],
                              &panel->extra[i * 2 + panel->buffer]);
            }
        }
    }
}

/* Draw the six member panels and the three party panels (with extras). */
void func_801C83D0(void) {
    s32 i;

    if (D_800625A0->flags->status_on) {
        for (i = 0; i < 6; i++) {
            func_801C80BC(D_800625A0->member_panels[i], 0);
        }
        for (i = 0; i < 3; i++) {
            func_801C80BC(D_800625A0->party_panels[i], 1);
        }
    }
}

/* Draw all labels. */
void func_801C846C(void) {
    func_801C7DA8();
    func_801C7E38();
    func_801C7EC8();
}

/* Project panel `index`'s two top edge pieces through the GTE and draw them. */
void func_801C849C(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[0][0][0], &panel->edge_at[0][0][1], &panel->edge_at[0][0][2],
                  &panel->edge_at[0][0][3], &(panel->edge[0] + panel->buffer)->x0,
                  &(panel->edge[0] + panel->buffer)->x1, &(panel->edge[0] + panel->buffer)->x2,
                  &(panel->edge[0] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[0][panel->buffer]);
    RotTransPers4(&panel->edge_at[0][1][0], &panel->edge_at[0][1][1], &panel->edge_at[0][1][2],
                  &panel->edge_at[0][1][3], &(panel->edge[0] + panel->buffer + 2)->x0,
                  &(panel->edge[0] + panel->buffer + 2)->x1, &(panel->edge[0] + panel->buffer + 2)->x2,
                  &(panel->edge[0] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[0][panel->buffer + 2]);
}

/* Project panel `index`'s two bottom edge pieces through the GTE and draw them. */
void func_801C8670(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[1][0][0], &panel->edge_at[1][0][1], &panel->edge_at[1][0][2],
                  &panel->edge_at[1][0][3], &(panel->edge[1] + panel->buffer)->x0,
                  &(panel->edge[1] + panel->buffer)->x1, &(panel->edge[1] + panel->buffer)->x2,
                  &(panel->edge[1] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[1][panel->buffer]);
    RotTransPers4(&panel->edge_at[1][1][0], &panel->edge_at[1][1][1], &panel->edge_at[1][1][2],
                  &panel->edge_at[1][1][3], &(panel->edge[1] + panel->buffer + 2)->x0,
                  &(panel->edge[1] + panel->buffer + 2)->x1, &(panel->edge[1] + panel->buffer + 2)->x2,
                  &(panel->edge[1] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[1][panel->buffer + 2]);
}

/* Project panel `index`'s two left edge pieces through the GTE and draw them. */
void func_801C8844(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[2][0][0], &panel->edge_at[2][0][1], &panel->edge_at[2][0][2],
                  &panel->edge_at[2][0][3], &(panel->edge[2] + panel->buffer)->x0,
                  &(panel->edge[2] + panel->buffer)->x1, &(panel->edge[2] + panel->buffer)->x2,
                  &(panel->edge[2] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[2][panel->buffer]);
    RotTransPers4(&panel->edge_at[2][1][0], &panel->edge_at[2][1][1], &panel->edge_at[2][1][2],
                  &panel->edge_at[2][1][3], &(panel->edge[2] + panel->buffer + 2)->x0,
                  &(panel->edge[2] + panel->buffer + 2)->x1, &(panel->edge[2] + panel->buffer + 2)->x2,
                  &(panel->edge[2] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[2][panel->buffer + 2]);
}

/* Project panel `index`'s two right edge pieces through the GTE and draw them. */
void func_801C8A18(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->edge_at[3][0][0], &panel->edge_at[3][0][1], &panel->edge_at[3][0][2],
                  &panel->edge_at[3][0][3], &(panel->edge[3] + panel->buffer)->x0,
                  &(panel->edge[3] + panel->buffer)->x1, &(panel->edge[3] + panel->buffer)->x2,
                  &(panel->edge[3] + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[3][panel->buffer]);
    RotTransPers4(&panel->edge_at[3][1][0], &panel->edge_at[3][1][1], &panel->edge_at[3][1][2],
                  &panel->edge_at[3][1][3], &(panel->edge[3] + panel->buffer + 2)->x0,
                  &(panel->edge[3] + panel->buffer + 2)->x1, &(panel->edge[3] + panel->buffer + 2)->x2,
                  &(panel->edge[3] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->edge[3][panel->buffer + 2]);
}

/* Project panel `index`'s fill through the GTE and draw it with its mode. */
void func_801C8BEC(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    RotTransPers4(&panel->fill_at[0], &panel->fill_at[1], &panel->fill_at[2], &panel->fill_at[3],
                  &(panel->fill + panel->buffer)->x0, &(panel->fill + panel->buffer)->x1,
                  &(panel->fill + panel->buffer)->x2, &(panel->fill + panel->buffer)->x3, &depth,
                  &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->fill[panel->buffer]);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->fill_mode[panel->buffer]);
}

/* Project panel `index`'s four corner sprites through the GTE and draw them. */
void func_801C8D28(s32 index) {
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
        AddPrim(D_800625A0->draw_env->ot + panel->param,
                      &panel->corner[i * 2 + panel->buffer]);
    }
}

/* Project panel `index`'s frame sprites (top, bottom, side) and draw them. */
void func_801C8E74(s32 index) {
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
        AddPrim(D_800625A0->draw_env->ot + panel->param,
                      &panel->frame_ends[i * 2 + panel->buffer]);
    }
    RotTransPers4(&panel->side_at[0], &panel->side_at[1], &panel->side_at[2], &panel->side_at[3],
                  &(panel->frame_side + panel->buffer)->x0,
                  &(panel->frame_side + panel->buffer)->x1,
                  &(panel->frame_side + panel->buffer)->x2,
                  &(panel->frame_side + panel->buffer)->x3, &depth, &flag);
    AddPrim(D_800625A0->draw_env->ot + panel->param, &panel->frame_side[panel->buffer]);
}

/* Draw every shown panel; style-0 panels are projected with an identity
 * rotation at depth 0x200. */
void func_801C9098(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    Panel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->flags->panel_20[i]) {
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
                func_801C8D28(i);
                if (panel->framed) {
                    func_801C8E74(i);
                }
                func_801C849C(i);
                func_801C8670(i);
                func_801C8844(i);
                func_801C8A18(i);
                func_801C8BEC(i);
                PopMatrix();
            } else {
                func_801C8D28(i);
                if (panel->framed) {
                    func_801C8E74(i);
                }
                func_801C849C(i);
                func_801C8670(i);
                func_801C8844(i);
                func_801C8A18(i);
                func_801C8BEC(i);
            }
        }
    }
}

/* Per-frame screen drawing: panels, markers, labels and status panels while
 * active, then the fade. */
void func_801C9210(void) {
    if (D_800625A0->active) {
        func_801C790C();
        func_801C7A58();
        func_801C846C();
        func_801C83D0();
        func_801C9098();
    }
    func_801C8040();
}

/* Play menu sound `sound` from the loaded effect bank. */
void func_801C9270(u8 sound) {
    func_80039DB8((D_800625A0->effect_bank->id << 16) | sound);
}

/* Read this frame's input into the input code (8: none). Without a pad the
 * sound is paused (keeping the vsync count) until one is connected; an
 * overflowed queue is reset; otherwise entries are dequeued until one holds
 * a button the screen uses. */
void func_801C92AC(void) {
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
                break;
            }
            if (D_800594A4 & 0x4000) {
                code = 1;
                break;
            }
            if (D_800594A4 & 0x8000) {
                code = 2;
                break;
            }
            if (D_800594A4 & 0x1000) {
                code = 3;
                break;
            }
            if (D_8005948C & 0x20) {
                code = 4;
                break;
            }
            if (D_8005948C & 0x40) {
                code = 5;
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
                D_800625A0->b_1E94 = D_800625A0->b_1E94 == 0;
                break;
            }
            if (D_8005948C & 1) {
                D_800625A0->b_1E95++;
                break;
            }
        }
    }
    D_800625A0->input_code = code;
}

/* Run one menu frame: read input, check the reset combination, swap to the
 * other draw buffer, draw the screen and present it. */
void func_801C94A0(void) {
    MenuState *state;
    DrawEnv *env;
    s32 shown;

    func_801C92AC();
    func_80019CA0();
    state = D_800625A0;
    env = &state->envs[0];
    if (state->draw_env == env) {
        env = &state->envs[1];
    }
    state->draw_env = env;
    state->buffer_index = state->buffer_index == 0;
    ClearOTagR(state->draw_env->ot, 16);
    func_801C9210();
    shown = D_800625A0->buffer_index == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&D_800625A0->draw_env->draw);
    PutDispEnv(&D_800625A0->draw_env->disp);
    MoveImage(&D_800625A0->block_350->screen, 0, shown * 0xE0);
    DrawOTag(&D_800625A0->draw_env->ot[15]);
}

/* Render the names of characters index & ~1 and index | 1 (two halves of
 * one texture) into VRAM at list slot `slot`. */
void func_801C95A0(s32 index, s32 slot) {
    RECT rect;
    u8 *image = func_80031BDC(0x3F6, 0);

    bzero(image, 0x3F6);
    func_80034EAC(D_8006D634.names[(u8)index / 2 * 2], image, 0x24, 0);
    func_80034EAC(D_8006D634.names[(u8)index / 2 * 2 + 1], image, 0x24, 1);
    rect.x = D_801CB344[(u8)slot / 2] + 0x180;
    rect.y = D_801CB390[(u8)slot / 2];
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, image);
    DrawSync(0);
    func_800320E8(image);
}

/* Split `value` into nine decimal digits, blanking (0xFF) leading zeros. */
void func_801C969C(u32 value) {
    u32 divisor = 100000000;
    s32 i;

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
            return;
        }
        D_800625A0->digits[i - 1] = 0xFF;
    }
}

/* Leave the screen: store the chosen party (packed, 0xFF filled), run the
 * closing frames until buffer 0 is shown, and release everything. */
void func_801C9748(void) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 3; i++) {
        if (D_800625A0->flags->party[i] != 0xFF) {
            D_8006F368[count] = D_800625A0->flags->party[i];
            count++;
        }
    }
    for (; count < 3; count++) {
        D_8006F368[count] = 0xFF;
    }
    func_801C94A0();
    func_801C94A0();
    D_800625A0->active = 0;
    func_801C94A0();
    do {
        func_801C94A0();
    } while (D_800625A0->buffer_index != 0);
    func_801C5034(0);
    func_801C5098(0);
    func_801C50FC(0);
    func_801C5160(0);
    func_801C51C4(0);
    func_801C5228(0);
    func_800320E8(D_800625A0->sprite_sheet);
    func_800320E8(D_800625A0->label_text);
    func_800320E8(D_800625A0->labels[0].image);
    if (D_80059178) {
        func_8003A094(D_800625A0->effect_bank);
        func_8003852C(D_800625A0->effect_bank);
        func_800320E8(D_800625A0->effect_bank);
    }
    func_801C528C(0);
    func_800320E8(D_800625A0);
}

/* List the characters that may join and are not in the party (0xFF
 * filled) and render their names. */
void func_801C9908(void) {
    s32 id;
    s32 count;
    s32 j;
    u8 free;

    for (id = 0, count = 0; id < 11; id++) {
        free = 1;
        if (D_800625A0->available[id]) {
            for (j = 0; j < 3; j++) {
                if (D_800625A0->flags->party[j] == id) {
                    free = 0;
                    break;
                }
            }
            if (free) {
                D_800625A0->members[count++] = id;
            }
        }
    }
    for (; count < 11; count++) {
        D_800625A0->members[count] = 0xFF;
    }
    for (id = 0; id < 11; id += 2) {
        func_801C95A0((u8)id, (u8)id);
    }
}

/* Build a status panel's layout sprites and face for character `id` in row
 * `slot` (x/y tables, row height) and its name label quad. */
void func_801C9A08(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height) {
    s32 i;
    s32 face;

    panel->layout_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_801CB3DC[i] != 0xFFFF) {
            panel->layout_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_801CB3DC[i],
                              &panel->layout[panel->layout_count * 2], D_800625A0->buffer_index,
                              x[i], row_height * slot + y[i], 0x1000);
        }
    }
    face = id;
    func_8002675C(D_800625A0->sprite_sheet, face + 0x14E, panel->face, D_800625A0->buffer_index,
                  x[9], row_height * slot + y[9], 0x1000);
    SetPolyFT4(&panel->label[D_800625A0->buffer_index]);
    (panel->label + D_800625A0->buffer_index)->r0 = 0x80;
    (panel->label + D_800625A0->buffer_index)->g0 = 0x80;
    (panel->label + D_800625A0->buffer_index)->b0 = 0x80;
    SetSemiTrans(&panel->label[D_800625A0->buffer_index], 0);
    panel->label[D_800625A0->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
    panel->label[D_800625A0->buffer_index].clut = (face & 1) ? D_80059414 : D_800595D4;
    (panel->label + D_800625A0->buffer_index)->x0 = x[16];
    (panel->label + D_800625A0->buffer_index)->y0 = y[16] + row_height * slot;
    (panel->label + D_800625A0->buffer_index)->x1 = x[16] + 0x48;
    (panel->label + D_800625A0->buffer_index)->y1 = y[16] + row_height * slot;
    (panel->label + D_800625A0->buffer_index)->x2 = x[16];
    (panel->label + D_800625A0->buffer_index)->y2 = y[16] + row_height * slot + 13;
    (panel->label + D_800625A0->buffer_index)->x3 = x[16] + 0x48;
    (panel->label + D_800625A0->buffer_index)->y3 = y[16] + row_height * slot + 13;
    (panel->label + D_800625A0->buffer_index)->u0 = D_801CB344[id / 2] * 4;
    (panel->label + D_800625A0->buffer_index)->v0 = D_801CB390[id / 2];
    (panel->label + D_800625A0->buffer_index)->u1 = D_801CB344[id / 2] * 4 + 0x48;
    (panel->label + D_800625A0->buffer_index)->v1 = D_801CB390[id / 2];
    (panel->label + D_800625A0->buffer_index)->u2 = D_801CB344[id / 2] * 4;
    (panel->label + D_800625A0->buffer_index)->v2 = D_801CB390[id / 2] + 13;
    (panel->label + D_800625A0->buffer_index)->u3 = D_801CB344[id / 2] * 4 + 0x48;
    (panel->label + D_800625A0->buffer_index)->v3 = D_801CB390[id / 2] + 13;
}

/* Build a status panel's level digits and the green digits of the record's
 * next value (+0x63) for character `id` in row `slot`. */
void func_801C9F80(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height) {
    s32 i;
    s32 n;

    func_801C969C(D_8006D634.characters[id].level);
    panel->level_count = 0;
    for (i = 0; i < 3; i++) {
        if (D_800625A0->digits[6 + i] != 0xFF) {
            panel->level_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[6 + i],
                              &panel->level[panel->level_count * 2], D_800625A0->buffer_index,
                              i * 8 + x[10], row_height * slot + y[10], 0x1000);
        }
    }
    func_801C969C(D_8006D634.characters[id].b63);
    panel->next_count = 0;
    for (i = 0, n = 0; i < 3; i++) {
        if (D_800625A0->digits[6 + i] != 0xFF) {
            panel->next_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[6 + i],
                              &panel->next[panel->next_count * 2], D_800625A0->buffer_index,
                              n * 8 + x[11], row_height * slot + y[11], 0x1000);
            n++;
        }
    }
    for (i = 0; i < panel->next_count; i++) {
        SetShadeTex(&panel->next[i * 2 + D_800625A0->buffer_index], 0);
        (panel->next + (i * 2 + D_800625A0->buffer_index))->r0 = 0;
        (panel->next + (i * 2 + D_800625A0->buffer_index))->g0 = 0x80;
        (panel->next + (i * 2 + D_800625A0->buffer_index))->b0 = 0;
    }
}

/* Build a status panel's HP / maximum HP (three digits) and EP / maximum EP
 * (two digits) for character `id` in row `slot`. */
void func_801CA24C(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height) {
    s32 i;
    s32 n;

    func_801C969C(D_8006D634.characters[id].hp);
    panel->hp_count = 0;
    for (i = 0; i < 3; i++) {
        if (D_800625A0->digits[6 + i] != 0xFF) {
            panel->hp_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[6 + i],
                              &panel->hp[panel->hp_count * 2], D_800625A0->buffer_index,
                              i * 8 + x[12], row_height * slot + y[12], 0x1000);
        }
    }
    func_801C969C(D_8006D634.characters[id].hp_max);
    panel->hp_max_count = 0;
    for (i = 0, n = 0; i < 3; i++) {
        if (D_800625A0->digits[6 + i] != 0xFF) {
            panel->hp_max_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[6 + i],
                              &panel->hp_max[panel->hp_max_count * 2], D_800625A0->buffer_index,
                              n * 8 + x[13], row_height * slot + y[13], 0x1000);
            n++;
        }
    }
    func_801C969C(D_8006D634.characters[id].ep);
    panel->ep_count = 0;
    for (i = 0; i < 2; i++) {
        if (D_800625A0->digits[7 + i] != 0xFF) {
            panel->ep_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[7 + i],
                              &panel->ep[panel->ep_count * 2], D_800625A0->buffer_index,
                              i * 8 + x[14], row_height * slot + y[14], 0x1000);
        }
    }
    func_801C969C(D_8006D634.characters[id].ep_max);
    panel->ep_max_count = 0;
    for (i = 0, n = 0; i < 2; i++) {
        if (D_800625A0->digits[7 + i] != 0xFF) {
            panel->ep_max_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[7 + i],
                              &panel->ep_max[panel->ep_max_count * 2], D_800625A0->buffer_index,
                              n * 8 + x[15], row_height * slot + y[15], 0x1000);
            n++;
        }
    }
}

/* Build status panel `panel` for character `id` in row `slot` of a layout
 * (x/y tables, row height) and show it for this buffer. */
void func_801CA5C0(StatusPanel *panel, u8 id, u8 slot, s32 *x, s32 *y, s32 row_height,
                   u8 party) {
    func_801C9A08(panel, id, slot, x, y, row_height);
    func_801C9F80(panel, id, slot, x, y, row_height);
    func_801CA24C(panel, id, slot, x, y, row_height);
    panel->extra_count = 0;
    panel->shown = 1;
    panel->buffer = D_800625A0->buffer_index;
}

/* Build the three party panels and the six member panels of list page
 * `first` (hiding panels without a character). */
/* Build the three party panels and the six member panels of list page
 * `first` (hiding panels without a character). */
void func_801CA690(u8 first) {
    s32 i;
    s32 index;
    StatusPanel *panel;
    s32 *x;
    s32 *y;

    x = D_801CB22C;
    y = D_801CB2B8;
    for (i = 0; i < 3; i++) {
        panel = D_800625A0->party_panels[i];
        if (D_800625A0->flags->party[i] != 0xFF) {
            func_801CA5C0(panel, D_800625A0->flags->party[i], i, x, y, 0x38, 1);
        } else {
            panel->shown = 0;
        }
    }
    x = D_801CB1E8;
    y = D_801CB274;
    D_800625A0->flags->status_on = 1;
    for (i = 0; i < 6; i++) {
        index = first + i;
        if (index >= 11) {
            break;
        }
        panel = D_800625A0->member_panels[i];
        if (D_800625A0->members[index] != 0xFF) {
            func_801C94A0();
            func_801CA5C0(panel, D_800625A0->members[index], i, x, y, 0x20, 0);
        } else {
            panel->shown = 0;
        }
    }
}

/* Place cursor marker `marker` on row `row` of the party list (`list` 0)
 * or the member list and show the markers. */
void func_801CA810(u8 list, s32 row, u8 marker) {
    s32 base = list ? 3 : 0;

    if (marker == 0) {
        func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->markers->poly,
                      D_800625A0->buffer_index, D_801CB1A0[base + row], D_801CB1C4[base + row],
                      0x800);
    } else {
        func_8002675C(D_800625A0->sprite_sheet, 0x108, &D_800625A0->markers->poly[2],
                      D_800625A0->buffer_index, D_801CB1A0[base + row], D_801CB1C4[base + row],
                      0x800);
    }
    D_800625A0->markers->shown[marker] = 1;
    D_800625A0->markers->buffer[marker] = D_800625A0->buffer_index;
    D_800625A0->flags->markers_on = 1;
}

/* Allocate the markers and set them up for `mode`: 0 and 2 build all four
 * at their home positions (0 also turns them on following the cursor), 3
 * builds the first at the origin and turns them on, 1 leaves them empty. */
void func_801CA944(u8 mode) {
    s32 i;
    Markers *markers = func_80031BDC(0x14C, 0);

    D_800625A0->markers = markers;
    bzero(markers, 0x14C);
    switch (mode) {
    case 0:
        D_800625A0->flags->markers_on = 1;
        D_800625A0->markers->follow[0] = 1;
        D_800625A0->markers->follow[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->markers->poly + i * 2,
                          D_800625A0->buffer_index, D_801CB180[i], D_801CB190[i], 0x800);
            D_800625A0->markers->buffer[i] = D_800625A0->buffer_index;
        }
        break;
    case 3:
        func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->markers->poly,
                      D_800625A0->buffer_index, 0, 0, 0x800);
        D_800625A0->markers->buffer[0] = D_800625A0->buffer_index;
        D_800625A0->flags->markers_on = 1;
        break;
    case 1:
        break;
    }
}

/* Hide the markers, let one frame pass and release them. */
void func_801CAB04(void) {
    D_800625A0->flags->markers_on = 0;
    func_801C94A0();
    func_800320E8(D_800625A0->markers);
}

/* Swap the party member and the list member picked by the two cursor
 * positions unless either is empty or locked (D_8006F94C); an exchange that
 * would leave the party empty is undone. Returns whether they were swapped. */
u8 func_801CAB48(u8 list, s32 row, s32 page, u8 prev_list, s32 prev_row, s32 prev_page) {
    u8 party_ok;
    u8 member_ok;
    u8 swapped;
    u8 slot;
    u8 index;
    u8 id;
    u8 count;
    s32 i;

    swapped = 0;
    party_ok = 1;
    member_ok = 1;

    if (list == 0) {
        slot = row;
        index = prev_row + prev_page;
    } else {
        slot = prev_row;
        index = row + page;
    }
    if (D_800625A0->flags->party[slot] == 0xFF) {
        party_ok = 0;
    } else if (func_801C5018(D_8006F94C, D_800625A0->flags->party[slot]) & 0xFFFF) {
        party_ok = 0;
    }
    if (D_800625A0->members[index] == 0xFF) {
        member_ok = 0;
    } else if (func_801C5018(D_8006F94C, D_800625A0->members[index]) & 0xFFFF) {
        member_ok = 0;
    }
    if (party_ok && member_ok) {
        id = D_800625A0->flags->party[slot];
        D_800625A0->flags->party[slot] = D_800625A0->members[index];
        D_800625A0->members[index] = id;
        for (i = 0, count = 0; i < 3; i++) {
            if (D_800625A0->flags->party[i] != 0xFF) {
                count++;
            }
        }
        swapped = 1;
        if (count == 0) {
            id = D_800625A0->flags->party[slot];
            D_800625A0->flags->party[slot] = D_800625A0->members[index];
            swapped = 0;
            D_800625A0->members[index] = id;
        }
    }
    return swapped;
}

/* The party screen loop: open the list panel, then move the cursor between
 * the party (list 0) and the member list (list 1) and pick two entries to
 * swap them, until cancelled; finally store the party.
 * NON_MATCHING: the original tests `list` zero-extended (andi) but passes it
 * to func_801CA810/func_801CAB48 unextended, copies it unextended into
 * prev_list (which is extended when passed), and flips it from the extended
 * value (xori s2, s0, 1); no u8/int/u16/s16 typing of list/prev_list here
 * reproduces all three (u8 list with s32 prev_list leaves only the call
 * extensions and the flip/copy order). */
#ifdef NON_MATCHING
void func_801CAD14(void) {
    u8 running = 1;
    s32 row = 0;
    s32 page = 0;
    s32 shown_page;
    s32 list;
    u8 picking;
    u8 prev_list;
    s32 prev_row;
    s32 prev_page;
    s32 i;

    func_801C9908();
    func_801C7788(2, 0x78, 6, 0xB8, 0xC8, 1, 0, 4, 0);
    func_801CA944(1);
    shown_page = 0xFF;
    list = 0;
    picking = 0;
    while (D_800625A0->growth[2]->open == 0) {
        func_801C94A0();
    }
    while (running) {
        func_801C94A0();
        if (page != shown_page) {
            func_801CA690(page);
            shown_page = page;
        }
        func_801CA810(list, row, 0);
        switch (D_800625A0->input_code) {
        case 5:
            func_801C9270(3);
            if (picking) {
                picking = 0;
                D_800625A0->markers->shown[running] = 0;
            } else {
                running = 0;
            }
            break;
        case 4:
            if (!picking) {
                picking = 1;
                func_801CA810(list, row, 1);
                prev_row = row;
                row = 0;
                prev_list = list;
                list ^= 1;
                prev_page = page;
                func_801C9270(2);
            } else if (func_801CAB48(list, row, page, prev_list, prev_row, prev_page)) {
                picking = 0;
                shown_page = 0xFF;
                D_800625A0->markers->shown[running] = 0;
                func_801C9270(2);
            } else {
                func_801C9270(4);
            }
            break;
        case 1:
            if (!list) {
                func_801C9270(1);
                if (++row >= 3) {
                    row = 0;
                }
            } else {
                if (++row >= 6) {
                    row = 5;
                }
                func_801C9270(1);
            }
            break;
        case 3:
            if (!list) {
                func_801C9270(1);
                if (--row < 0) {
                    row = 2;
                }
            } else {
                if (--row < 0) {
                    row = 0;
                    if (--page < 0) {
                        page = 0;
                        break;
                    }
                }
                func_801C9270(1);
            }
            break;
        case 2:
            if (!picking && list) {
                func_801C9270(1);
                list = 0;
                row = 0;
            }
            break;
        case 0:
            if (!picking && !list) {
                func_801C9270(1);
                list = 1;
                row = 0;
            }
            break;
        }
    }
    func_801CAB04();
    func_801C76FC(2);
    for (i = 0; i < 3; i++) {
        D_8006F368[i] = D_800625A0->flags->party[i];
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2598/asm/nonmatchings/ovl2598", func_801CAD14);
#endif

/* Overlay entry: allocate and set up the party screen, run it and leave. */
void func_801CB0A8(void) {
    MenuState *state;

    func_801C5034(1);
    func_801C5098(1);
    func_801C50FC(1);
    func_801C5160(1);
    func_801C51C4(1);
    func_801C5228(1);
    func_801C528C(1);
    state = D_800625A0;
    state->block_350->screen.x = 0x2C0;
    state->block_350->screen.y = 0x100;
    state->block_350->screen.w = 0x140;
    state->block_350->screen.h = 0xE0;
    state->backdrop->b_15B = 0x40;
    func_801C559C();
    func_801C5714();
    func_801C5B90();
    func_801C5DA0();
    func_801C5BEC();
    D_800625A0->active = 1;
    func_801CAD14();
    func_801C9748();
}
