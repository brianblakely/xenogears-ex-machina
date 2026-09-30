/* Overlay 2600 (Disc 1 slot 2600; loaded at 0x801c5000): the character name
 * entry screen. A character grid (D_801CBEC0) is walked with the cursor, the
 * name is built as text codes and decoded for display, and the result is
 * stored in the character name table at 8006d634 + id * 0x14. The three
 * party portraits are loaded for the screen. Much of the drawing/list code is
 * the same as overlay 2598 (the party screen) but compiled into this image. */
#include "common.h"
#include "name_entry.h"

extern u16 D_801CC114[];
extern u8 D_801CBF98[];
extern s32 D_801CBEA0[]; /* marker home x */
extern s32 D_801CBEB0[]; /* marker home y */
extern s32 D_801CBF9C[]; /* file cursor -> slot */
extern s32 D_801CC014[]; /* slot x */
extern s32 D_801CC094[]; /* slot y */

/* Test character `index`'s bit (table D_801CC114) in `flags`. */
s32 func_801C5040(s32 flags, u8 index) {
    return D_801CC114[index] & flags;
}

/* Allocate (nonzero) or release the 0x5034-byte work block. */
void func_801C505C(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->work = block;
        func_8003F8E8(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->work);
    }
}

/* Allocate (nonzero) or release the menu flag block. */
void func_801C50C0(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6C, 0);
        D_800625A0->flags = block;
        func_8003F8E8(block, 0x6C);
    } else {
        func_800320E8(D_800625A0->flags);
    }
}

/* Allocate (nonzero) or release the 0x1194-byte block at state + 0x350. */
void func_801C5124(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->block_350 = block;
        func_8003F8E8(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->block_350);
    }
}

/* Allocate (nonzero) or release the 0x140C-byte block at state + 0x354. */
void func_801C5188(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140C, 0);
        D_800625A0->block_354 = block;
        func_8003F8E8(block, 0x140C);
    } else {
        func_800320E8(D_800625A0->block_354);
    }
}

/* Allocate (nonzero) or release the 0xCC-byte block at state + 0x330. */
void func_801C51EC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xCC, 0);
        D_800625A0->block_330 = block;
        func_8003F8E8(block, 0xCC);
    } else {
        func_800320E8(D_800625A0->block_330);
    }
}

/* Allocate (nonzero) or release the 0x15C-byte block at state + 0x348. */
void func_801C5250(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15C, 0);
        D_800625A0->backdrop = block;
        func_8003F8E8(block, 0x15C);
    } else {
        func_800320E8(D_800625A0->backdrop);
    }
}

/* Allocate (nonzero) or release the name entry block. */
void func_801C52B4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xDEC, 0);
        D_800625A0->entry = block;
        func_8003F8E8(block, 0xDEC);
    } else {
        func_800320E8(D_800625A0->entry);
    }
}

#ifdef NON_MATCHING
/* Load the screen's resources: the card icon TIM and file name into the work
 * block's save header, the palette data, sprite sheet and label texts, the
 * named character's entry length and three portraits (uploaded to the
 * portrait sprites' VRAM), and the menu sound bank when sound is on. */
void func_801C5318(void) {
    TIM_IMAGE tim;
    SpriteInfo info[3];
    MenuArchive *archive = D_8005945C;
    u8 *data;
    u8 use_table = 1;
    s32 i;
    u8 id;

    func_8003342C(archive);
    data = func_80032E88(archive->entry[0], 1);
    func_800471B4(data);
    func_800471C4(&D_800625A0->work->tim);
    strcpy(D_800625A0->work->file_name, "BISLPS-00800");
    D_800625A0->work->magic[0] = 'S';
    D_800625A0->work->magic[1] = 'C';
    D_800625A0->work->icon_type = 0x11;
    D_800625A0->work->blocks = 1;
    func_8003F8E8(D_800625A0->work->title, 0x5C);
    func_8003F99C(D_800625A0->work->clut, D_800625A0->work->tim.caddr, 0x20);
    func_8003F99C(D_800625A0->work->icon, D_800625A0->work->tim.paddr, 0x80);
    func_800320E8(data);
    data = func_80032E88(archive->entry[1], 1);
    func_8002DD20(data);
    func_800320E8(data);
    D_800625A0->sprite_sheet = func_80032E88(archive->entry[2], 0);
    D_800625A0->label_text = func_80032E88(archive->entry[3], 0);
    func_80026338(D_800625A0->sprite_sheet, 0x14B, &info[0].unk0, &info[0].tpage_mode,
                  &info[0].clut_x, &info[0].clut_y, &info[0].page_x, &info[0].page_y);
    func_80026338(D_800625A0->sprite_sheet, 0x14C, &info[1].unk0, &info[1].tpage_mode,
                  &info[1].clut_x, &info[1].clut_y, &info[1].page_x, &info[1].page_y);
    func_80026338(D_800625A0->sprite_sheet, 0x14D, &info[2].unk0, &info[2].tpage_mode,
                  &info[2].clut_x, &info[2].clut_y, &info[2].page_x, &info[2].page_y);
    D_800625A0->portrait_table = func_80032E88(archive->entry[5], 1);
    if (D_80059171 < 11) {
        D_800625A0->entry->max_length = 9;
        D_800625A0->flags->party[0] = D_80059171;
    } else {
        D_800625A0->entry->max_length = 10;
        if (D_80059171 - 11 == 9 && (D_8006F364 & 0x400)) {
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
    func_800320E8(D_800625A0->portrait_table);
    info[1].page_x += 12;
    data = func_80032E88(archive->entry[4], 1);
    for (i = 0; i < 3; i++) {
        id = D_800625A0->flags->party[i];
        if (id != 0xFF) {
            func_800471B4(data + id * 0xB20);
            func_800471C4(&tim);
            tim.crect->x = info[i].clut_x;
            tim.crect->y = info[i].clut_y;
            tim.prect->x = info[i].page_x;
            tim.prect->y = info[i].page_y;
            func_80044894(tim.crect, tim.caddr);
            func_80044894(tim.prect, tim.paddr);
        }
    }
    func_800445D0(0);
    func_800320E8(data);
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
#else
INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5318);
#endif

/* Reset the screen state, mark which characters may join, take the current
 * party (members that may not join become empty) and load the resources. */
void func_801C58B8(void) {
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
        if (func_801C5040(flags, i) & 0xFFFF) {
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
    func_801C5318();
}

/* Start building draw buffer 0. */
void func_801C5A30(void) {
    D_800625A0->buffer_index = 0;
}

/* Upload the text CLUT: 16 black entries except white entry 1 at (0, 0x1C0). */
void func_801C5A40(void) {
    RECT rect;
    u8 unused[8];
    u16 *clut = func_80031BDC(0x20, 0);

    func_8003F8E8(clut, 0x20);
    clut[1] = 0x7FFF;
    rect.x = 0;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.h = 1;
    func_80044894(&rect, clut);
    func_800445D0(0);
    func_800320E8(clut);
}

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5ABC);

/* Render `count` label texts (pairs of text ids) into VRAM, two per line,
 * and set up their quads. */
void func_801C5CFC(MenuLabel *labels, u8 *text_ids, s32 row, s32 count) {
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
        func_801C5ABC(&labels[i], i, row, 0);
        func_801C5ABC(&labels[i + 1], i + 1, row, 0);
        func_80044894(rect, D_800625A0->labels[0].image);
        func_800445D0(0);
    }
}

/* Set up the four command labels and the text CLUT. */
void func_801C5EAC(void) {
    func_80033698(0, 0x1D1);
    D_800625A0->labels[0].image = func_80031BDC(0x38E, 0);
    func_801C5CFC(D_800625A0->labels, D_801CBF98, 0, 4);
    func_801C5A40();
}

/* Look up the four cursor/frame sprites of the sheet. */
void func_801C5F08(void) {
    u8 unused[0x28];

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
void func_801C6010(void) {
    D_800625A0->flags->flag_4 = 0;
    D_800625A0->flags->flag_3 = 0;
}

/* Make `poly` a gouraud quad fading from (r, g, b) at the top to black. */
void func_801C6040(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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
        func_801C6040(&D_800625A0->backdrop->gradient[i], 0x80, 0x80, 0);
        func_80043BFC(&D_800625A0->backdrop->gradient[i], 1);
        func_80043DA0(&D_800625A0->backdrop->line_a[i]);
        (D_800625A0->backdrop->line_a + i)->r0 = 0;
        (D_800625A0->backdrop->line_a + i)->g0 = 0x40;
        (D_800625A0->backdrop->line_a + i)->b0 = 0;
        func_80043DA0(&D_800625A0->backdrop->line_b[i]);
        (D_800625A0->backdrop->line_b + i)->r0 = 0;
        (D_800625A0->backdrop->line_b + i)->g0 = 0x40;
        (D_800625A0->backdrop->line_b + i)->b0 = 0;
        func_80043C9C(&D_800625A0->backdrop->fade[i]);
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
        func_80043BFC(&D_800625A0->backdrop->fade[i], 1);
        func_800454DC(&D_800625A0->backdrop->mode_a[i], 0, 0, func_80043A1C(0, 0, 0x140, 0x80),
                      &window);
        func_800454DC(&D_800625A0->backdrop->mode_b[i], 0, 0, func_80043A1C(0, 2, 0x180, 0),
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
    func_80043BFC(poly, 1);
    func_80043C24(poly, 0);
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
    D_800625A0->flags->panel_20[index] = 0;
    D_800625A0->flags->panel_27[index] = 0;
    for (i = 0; i < 2; i++) {
        func_80043CC4(&panel->fill[i]);
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
        func_80043BFC(&panel->fill[i], 1);
        func_800454DC(&panel->fill_mode[i], 0, 0,
                      func_80043A1C(0, 0, D_800625A0->sprites[0].page_x,
                                    D_800625A0->sprites[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        func_80043CB0(&panel->edge[0][i]);
        func_80043C24(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xFF;
        (panel->edge[0] + i)->g0 = 0xFF;
        (panel->edge[0] + i)->b0 = 0xFF;
        (panel->edge[0] + i)->tpage =
            func_80043A1C(D_800625A0->sprites[0].tpage_mode, 0, D_800625A0->sprites[0].page_x,
                          D_800625A0->sprites[0].page_y);
        (panel->edge[0] + i)->clut =
            func_80043A58(D_800625A0->sprites[0].clut_x, D_800625A0->sprites[0].clut_y);
        func_80043CB0(&panel->edge[1][i]);
        func_80043C24(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xFF;
        (panel->edge[1] + i)->g0 = 0xFF;
        (panel->edge[1] + i)->b0 = 0xFF;
        (panel->edge[1] + i)->tpage =
            func_80043A1C(D_800625A0->sprites[1].tpage_mode, 0, D_800625A0->sprites[1].page_x,
                          D_800625A0->sprites[1].page_y);
        (panel->edge[1] + i)->clut =
            func_80043A58(D_800625A0->sprites[1].clut_x, D_800625A0->sprites[1].clut_y);
        func_80043CB0(&panel->edge[2][i]);
        func_80043C24(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xFF;
        (panel->edge[2] + i)->g0 = 0xFF;
        (panel->edge[2] + i)->b0 = 0xFF;
        (panel->edge[2] + i)->tpage =
            func_80043A1C(D_800625A0->sprites[2].tpage_mode, 0, D_800625A0->sprites[2].page_x,
                          D_800625A0->sprites[2].page_y);
        (panel->edge[2] + i)->clut =
            func_80043A58(D_800625A0->sprites[2].clut_x, D_800625A0->sprites[2].clut_y);
        func_80043CB0(&panel->edge[3][i]);
        func_80043C24(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xFF;
        (panel->edge[3] + i)->g0 = 0xFF;
        (panel->edge[3] + i)->b0 = 0xFF;
        (panel->edge[3] + i)->tpage =
            func_80043A1C(D_800625A0->sprites[3].tpage_mode, 0, D_800625A0->sprites[3].page_x,
                          D_800625A0->sprites[3].page_y);
        (panel->edge[3] + i)->clut =
            func_80043A58(D_800625A0->sprites[3].clut_x, D_800625A0->sprites[3].clut_y);
    }
}

/* Build panel `index`'s frame sprites for this buffer at (x, y) with height
 * `h` and place its top, bottom and side vectors. */
void func_801C67C4(u8 index, u16 x, u16 y, s32 unused, u16 h) {
    Panel *panel = D_800625A0->panels[index];

    func_8002675C(D_800625A0->sprite_sheet, 0x105, panel->frame_ends, D_800625A0->buffer_index,
                  x, y, 0x1000);
    func_800263E4(D_800625A0->sprite_sheet, 0x105, &panel->frame_ends[2],
                  D_800625A0->buffer_index, x, y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sprite_sheet, 0x106, panel->frame_side, D_800625A0->buffer_index,
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

    D_800625A0->flags->panel_20[index] = 0;
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
    D_800625A0->flags->panel_20[index] = 1;
}

/* Hide panel `index` and release its block and growth record. */
void func_801C7A18(u8 index) {
    D_800625A0->flags->panel_20[index] = 0;
    D_800625A0->flags->panel_27[index] = 0;
    func_800320E8(D_800625A0->panels[index]);
    func_800320E8(D_800625A0->growth[index]);
}

/* Open panel `index` (allocating it first unless it is 0 or 1): either
 * start its growth animation toward (x, y, w, h) or lay it out at once. */
void func_801C7AA4(u8 index, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 style, s32 param,
                   u8 framed) {
    PanelGrowth *growth;

    if (index >= 2) {
        D_800625A0->panels[index] = func_80031BDC(0x720, 0);
        func_8003F8E8(D_800625A0->panels[index], 0x720);
        D_800625A0->growth[index] = func_80031BDC(0x18, 0);
        func_8003F8E8(D_800625A0->growth[index], 0x18);
        func_801C64A8(index);
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
        func_801C7894(index, x, y, w, h, style, param, framed);
    }
}

/* Grow every opening panel by 32 in width and height per frame until it
 * reaches its size, laying it out centred on its final rectangle. */
void func_801C7C28(void) {
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
            func_801C7894(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->style, growth->param, growth->framed);
        }
    }
}

/* Project panel `index`'s two top edge pieces through the GTE and draw them. */
void func_801C7D74(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    func_8004A73C(&panel->edge_at[0][0][0], &panel->edge_at[0][0][1], &panel->edge_at[0][0][2],
                  &panel->edge_at[0][0][3], &(panel->edge[0] + panel->buffer)->x0,
                  &(panel->edge[0] + panel->buffer)->x1, &(panel->edge[0] + panel->buffer)->x2,
                  &(panel->edge[0] + panel->buffer)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[0][panel->buffer]);
    func_8004A73C(&panel->edge_at[0][1][0], &panel->edge_at[0][1][1], &panel->edge_at[0][1][2],
                  &panel->edge_at[0][1][3], &(panel->edge[0] + panel->buffer + 2)->x0,
                  &(panel->edge[0] + panel->buffer + 2)->x1, &(panel->edge[0] + panel->buffer + 2)->x2,
                  &(panel->edge[0] + panel->buffer + 2)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[0][panel->buffer + 2]);
}

/* Project panel `index`'s two bottom edge pieces through the GTE and draw them. */
void func_801C7F48(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    func_8004A73C(&panel->edge_at[1][0][0], &panel->edge_at[1][0][1], &panel->edge_at[1][0][2],
                  &panel->edge_at[1][0][3], &(panel->edge[1] + panel->buffer)->x0,
                  &(panel->edge[1] + panel->buffer)->x1, &(panel->edge[1] + panel->buffer)->x2,
                  &(panel->edge[1] + panel->buffer)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[1][panel->buffer]);
    func_8004A73C(&panel->edge_at[1][1][0], &panel->edge_at[1][1][1], &panel->edge_at[1][1][2],
                  &panel->edge_at[1][1][3], &(panel->edge[1] + panel->buffer + 2)->x0,
                  &(panel->edge[1] + panel->buffer + 2)->x1, &(panel->edge[1] + panel->buffer + 2)->x2,
                  &(panel->edge[1] + panel->buffer + 2)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[1][panel->buffer + 2]);
}

/* Project panel `index`'s two left edge pieces through the GTE and draw them. */
void func_801C811C(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    func_8004A73C(&panel->edge_at[2][0][0], &panel->edge_at[2][0][1], &panel->edge_at[2][0][2],
                  &panel->edge_at[2][0][3], &(panel->edge[2] + panel->buffer)->x0,
                  &(panel->edge[2] + panel->buffer)->x1, &(panel->edge[2] + panel->buffer)->x2,
                  &(panel->edge[2] + panel->buffer)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[2][panel->buffer]);
    func_8004A73C(&panel->edge_at[2][1][0], &panel->edge_at[2][1][1], &panel->edge_at[2][1][2],
                  &panel->edge_at[2][1][3], &(panel->edge[2] + panel->buffer + 2)->x0,
                  &(panel->edge[2] + panel->buffer + 2)->x1, &(panel->edge[2] + panel->buffer + 2)->x2,
                  &(panel->edge[2] + panel->buffer + 2)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[2][panel->buffer + 2]);
}

/* Project panel `index`'s two right edge pieces through the GTE and draw them. */
void func_801C82F0(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    func_8004A73C(&panel->edge_at[3][0][0], &panel->edge_at[3][0][1], &panel->edge_at[3][0][2],
                  &panel->edge_at[3][0][3], &(panel->edge[3] + panel->buffer)->x0,
                  &(panel->edge[3] + panel->buffer)->x1, &(panel->edge[3] + panel->buffer)->x2,
                  &(panel->edge[3] + panel->buffer)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[3][panel->buffer]);
    func_8004A73C(&panel->edge_at[3][1][0], &panel->edge_at[3][1][1], &panel->edge_at[3][1][2],
                  &panel->edge_at[3][1][3], &(panel->edge[3] + panel->buffer + 2)->x0,
                  &(panel->edge[3] + panel->buffer + 2)->x1, &(panel->edge[3] + panel->buffer + 2)->x2,
                  &(panel->edge[3] + panel->buffer + 2)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->edge[3][panel->buffer + 2]);
}

/* Project panel `index`'s fill through the GTE and draw it with its mode. */
void func_801C84C4(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;

    func_8004A73C(&panel->fill_at[0], &panel->fill_at[1], &panel->fill_at[2], &panel->fill_at[3],
                  &(panel->fill + panel->buffer)->x0, &(panel->fill + panel->buffer)->x1,
                  &(panel->fill + panel->buffer)->x2, &(panel->fill + panel->buffer)->x3, &depth,
                  &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->fill[panel->buffer]);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->fill_mode[panel->buffer]);
}

/* Project panel `index`'s four corner sprites through the GTE and draw them. */
void func_801C8600(s32 index) {
    Panel *panel = D_800625A0->panels[index];
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 4; i++) {
        func_8004A73C(&panel->corner_at[i * 4], &panel->corner_at[i * 4 + 1], &panel->corner_at[i * 4 + 2],
                      &panel->corner_at[i * 4 + 3], &panel->corner[i * 2 + panel->buffer].x0,
                      &panel->corner[i * 2 + panel->buffer].x1,
                      &panel->corner[i * 2 + panel->buffer].x2,
                      &panel->corner[i * 2 + panel->buffer].x3, &depth, &flag);
        func_80043B48(D_800625A0->draw_env->ot + panel->param,
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
        func_8004A73C(&panel->ends_at[i * 4], &panel->ends_at[i * 4 + 1], &panel->ends_at[i * 4 + 2],
                      &panel->ends_at[i * 4 + 3], &(panel->frame_ends + (i * 2 + panel->buffer))->x0,
                      &(panel->frame_ends + (i * 2 + panel->buffer))->x1,
                      &(panel->frame_ends + (i * 2 + panel->buffer))->x2,
                      &(panel->frame_ends + (i * 2 + panel->buffer))->x3, &depth, &flag);
        func_80043B48(D_800625A0->draw_env->ot + panel->param,
                      &panel->frame_ends[i * 2 + panel->buffer]);
    }
    func_8004A73C(&panel->side_at[0], &panel->side_at[1], &panel->side_at[2], &panel->side_at[3],
                  &(panel->frame_side + panel->buffer)->x0,
                  &(panel->frame_side + panel->buffer)->x1,
                  &(panel->frame_side + panel->buffer)->x2,
                  &(panel->frame_side + panel->buffer)->x3, &depth, &flag);
    func_80043B48(D_800625A0->draw_env->ot + panel->param, &panel->frame_side[panel->buffer]);
}

/* Draw every shown panel; style-0 panels are projected with an identity
 * rotation at depth 0x200. */
void func_801C8970(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    u8 unused[8];
    Panel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->flags->panel_20[i]) {
            panel = D_800625A0->panels[i];
            if (panel->style == 0) {
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
                func_801C8600(i);
                if (panel->framed) {
                    func_801C874C(i);
                }
                func_801C7D74(i);
                func_801C7F48(i);
                func_801C811C(i);
                func_801C82F0(i);
                func_801C84C4(i);
                func_800496AC();
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

    if (D_800625A0->flags->markers_on) {
        for (i = 0; i < 4; i++) {
            state = D_800625A0;
            if (state->markers->shown[i]) {
                if (state->markers->follow[i]) {
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x0 =
                        D_801CC014[D_801CBF9C[state->work->cursor]] + 8;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y0 =
                        D_801CC094[D_801CBF9C[state->work->cursor]] - 6;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x1 =
                        D_801CC014[D_801CBF9C[state->work->cursor]] + 24;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y1 =
                        D_801CC094[D_801CBF9C[state->work->cursor]] - 6;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x2 =
                        D_801CC014[D_801CBF9C[state->work->cursor]] + 8;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y2 =
                        D_801CC094[D_801CBF9C[state->work->cursor]] + 10;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->x3 =
                        D_801CC014[D_801CBF9C[state->work->cursor]] + 24;
                    (state->markers->poly + (i * 2 + state->markers->buffer[i]))->y3 =
                        D_801CC094[D_801CBF9C[state->work->cursor]] + 10;
                }
                func_80043B48(&D_800625A0->draw_env->ot[4],
                              &D_800625A0->markers->poly[i * 2 + D_800625A0->markers->buffer[i]]);
            }
        }
    }
}

/* Draw the shown command labels. */
void func_801C8E38(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->flags->label_shown[i]) {
            func_80043B48(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->labels[i].poly[D_800625A0->labels[i].buffer]);
        }
    }
}

/* Draw the shown list labels. */
void func_801C8EC8(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->flags->list_label_shown[i]) {
            func_80043B48(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->list_labels[i].poly[D_800625A0->list_labels[i].buffer]);
        }
    }
}

/* Draw the shown row labels, projecting the 3D ones through the GTE first. */
void func_801C8F58(void) {
    s32 depth;
    s32 flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->row_label_shown[i]) {
            if (D_800625A0->row_labels[i].projected) {
                MenuLabel *label = &D_800625A0->row_labels[i];

                func_8004A73C(&label->corners[0], &label->corners[1], &label->corners[2],
                              &label->corners[3],
                              &label->poly[D_800625A0->row_labels[i].buffer].x0,
                              &label->poly[D_800625A0->row_labels[i].buffer].x1,
                              &label->poly[D_800625A0->row_labels[i].buffer].x2,
                              &label->poly[D_800625A0->row_labels[i].buffer].x3, &depth, &flag);
                func_80043B48(&D_800625A0->draw_env->ot[4],
                              &D_800625A0->row_labels[i].poly[D_800625A0->row_labels[i].buffer]);
            } else {
                func_80043B48(&D_800625A0->draw_env->ot[4],
                              &D_800625A0->row_labels[i].poly[D_800625A0->row_labels[i].buffer]);
            }
        }
    }
}

/* Draw the shown name entry labels. */
void func_801C90D0(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->flags->entry_label_shown[i]) {
            func_80043B48(&D_800625A0->draw_env->ot[4],
                          &D_800625A0->entry_labels[i].poly[D_800625A0->entry_labels[i].buffer]);
        }
    }
}

/* Draw the shown message lines, projecting the 3D ones through the GTE. */
void func_801C9160(void) {
    s32 depth;
    s32 flag;
    s32 i;
    MenuLabel *line;

    if (D_800625A0->flags->b_2E) {
        for (i = 0; i < 3; i++) {
            line = D_800625A0->message_lines[i];
            if (line->projected) {
                func_8004A73C(&line->corners[0], &line->corners[1], &line->corners[2],
                              &line->corners[3], &line->poly[line->buffer].x0,
                              &line->poly[line->buffer].x1, &line->poly[line->buffer].x2,
                              &line->poly[line->buffer].x3, &depth, &flag);
                func_80043B48(&D_800625A0->draw_env->ot[4], &line->poly[line->buffer]);
            } else {
                func_80043B48(&D_800625A0->draw_env->ot[4], &line->poly[line->buffer]);
            }
        }
    }
}

/* Draw this buffer's fade quad and the second draw mode. */
void func_801C92BC(void) {
    func_80043B48(&D_800625A0->draw_env->ot[8],
                  &D_800625A0->backdrop->fade[D_800625A0->buffer_index]);
    func_80043B48(&D_800625A0->draw_env->ot[8],
                  &D_800625A0->backdrop->mode_b[D_800625A0->buffer_index]);
}

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9338);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C97FC);

/* Per-frame screen drawing: panels, markers, labels and the name entry while
 * active, then the fade. */
void func_801C983C(void) {
    if (D_800625A0->active) {
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
        func_80039DB8((D_800625A0->effect_bank->id << 16) | sound);
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

/* Advance the view motion (4/3 start zooming in/out, 2/1 run them) and load
 * the view rotation and translation into the GTE. */
void func_801C9AF4(void) {
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
    func_80049D9C(&D_800625A0->view_matrix, &D_800625A0->view_translation);
    func_80049EFC(&D_800625A0->view_matrix);
    func_80049F8C(&D_800625A0->view_matrix);
}

/* Run one menu frame: check the stack guard, read input, check the reset
 * combination, swap to the other draw buffer, draw the screen and present
 * it. */
void func_801C9C34(void) {
    MenuState *state;
    DrawEnv *env;
    s32 shown;

    if (*D_8005917C != -1) {
        __asm__ volatile("break 1024");
    }
    func_801C98E8();
    func_80019CA0();
    state = D_800625A0;
    env = &state->envs[0];
    if (state->draw_env == env) {
        env = &state->envs[1];
    }
    state->draw_env = env;
    state->buffer_index = state->buffer_index == 0;
    func_80044AD8(state->draw_env->ot, 16);
    func_801C9AF4();
    func_801C983C();
    shown = D_800625A0->buffer_index == 0;
    func_800445D0(0);
    func_8004B54C(0);
    func_80044C44(&D_800625A0->draw_env->draw);
    func_80044E9C(&D_800625A0->draw_env->disp);
    func_8004495C(&D_800625A0->block_350->screen, 0, shown * 0xE0);
    func_80044BD0(&D_800625A0->draw_env->ot[15]);
}

/* Allocate the markers and set them up for `mode`: 0 and 2 build all four
 * at their home positions (0 also turns them on following the cursor), 3
 * builds the first at the origin and turns them on, 1 leaves them empty. */
void func_801C9D5C(u8 mode) {
    s32 i;
    Markers *markers = func_80031BDC(0x14C, 0);

    D_800625A0->markers = markers;
    func_8003F8E8(markers, 0x14C);
    switch (mode) {
    case 0:
        D_800625A0->flags->markers_on = 1;
        D_800625A0->markers->follow[0] = 1;
        D_800625A0->markers->follow[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sprite_sheet, 0x108, D_800625A0->markers->poly + i * 2,
                          D_800625A0->buffer_index, D_801CBEA0[i], D_801CBEB0[i], 0x800);
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
void func_801C9F1C(void) {
    D_800625A0->flags->markers_on = 0;
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

        D_800625A0->message_lines[i] = block;
        func_8003F8E8(block, 0x80);
        if (!(i & 1)) {
            D_800625A0->message_lines[i]->image = func_80031BDC(0x5CA, 0);
            D_800625A0->message_lines[i]->rect.x = 0x140;
            D_800625A0->message_lines[i]->rect.y = (i / 2) * 13 + 0x4E;
            D_800625A0->message_lines[i]->rect.w = 0x3A;
            D_800625A0->message_lines[i]->rect.h = 13;
        } else {
            D_800625A0->message_lines[i]->image = D_800625A0->message_lines[i - 1]->image;
        }
    }
    for (i = 0; i < 3; i++) {
        line = D_800625A0->message_lines[i];
        line->width = func_80034EAC(func_80033728(D_800625A0->label_text, first + i), line->image,
                                    0x36, i % 2);
        func_801C5ABC(line, i, 0, 0);
        func_801C6408(line->corners, x, i * 16 + 0xA0, line->width, 13);
        (line->poly + D_800625A0->buffer_index)->u0 = 0;
        (line->poly + D_800625A0->buffer_index)->v0 = (i / 2) * 13 + 0x4E;
        (line->poly + D_800625A0->buffer_index)->u1 = line->width;
        (line->poly + D_800625A0->buffer_index)->v1 = (i / 2) * 13 + 0x4E;
        (line->poly + D_800625A0->buffer_index)->u2 = 0;
        (line->poly + D_800625A0->buffer_index)->v2 = (i / 2) * 13 + 0x5B;
        (line->poly + D_800625A0->buffer_index)->u3 = line->width;
        (line->poly + D_800625A0->buffer_index)->v3 = (i / 2) * 13 + 0x5B;
        line->buffer = D_800625A0->buffer_index;
        line->projected = 1;
    }
    func_80044894(&D_800625A0->message_lines[0]->rect, D_800625A0->message_lines[0]->image);
    func_80044894(&D_800625A0->message_lines[2]->rect, D_800625A0->message_lines[2]->image);
    func_800445D0(0);
    D_800625A0->flags->b_2E = 1;
    func_800320E8(D_800625A0->message_lines[0]->image);
    func_800320E8(D_800625A0->message_lines[2]->image);
    func_801C9C34();
    func_801C9C34();
}

/* Turn the message lines off, release their four blocks and run a frame. */
void func_801CA39C(void) {
    s32 i;

    D_800625A0->flags->b_2E = 0;
    for (i = 0; i < 4; i++) {
        func_800320E8(D_800625A0->message_lines[i]);
    }
    func_801C9C34();
}

/* Leave the screen: run the closing frames until buffer 0 is shown and
 * release everything (a frame passes around the sound bank release). */
void func_801CA400(void) {
    func_801C9C34();
    func_801C9C34();
    D_800625A0->active = 0;
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
    func_800320E8(D_800625A0->sprite_sheet);
    func_800320E8(D_800625A0->label_text);
    func_800320E8(D_800625A0->labels[0].image);
    if (D_80059178) {
        func_8003A094(D_800625A0->effect_bank);
        func_801C9C34();
        func_8003852C(D_800625A0->effect_bank);
        func_801C9C34();
        func_800320E8(D_800625A0->effect_bank);
    }
    func_801C52B4(0);
    func_800320E8(D_800625A0);
}

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA558);

/* Open the name entry: its panel and message, the cursor sprite and its
 * quad, the confirm/back/grid sprites, the two command labels, the
 * character grid, and show the entry. */
void func_801CADC8(void) {
    func_801C7AA4(3, 0x10, 0x9A, 0xC0, 0x3C, 1, 1, 4, 0);
    func_801C9FC0(0x1D);
    func_8002675C(D_800625A0->sprite_sheet, 0x14B, D_800625A0->entry->cursor,
                  D_800625A0->buffer_index, 0, 0, 0x1000);
    func_801C6408(D_800625A0->entry->cursor_at,
                  D_800625A0->entry->cursor[D_800625A0->buffer_index].x0 + 0x18,
                  D_800625A0->entry->cursor[D_800625A0->buffer_index].y0 + 0x9E,
                  D_800625A0->entry->cursor[D_800625A0->buffer_index].x1 -
                      D_800625A0->entry->cursor[D_800625A0->buffer_index].x0,
                  D_800625A0->entry->cursor[D_800625A0->buffer_index].y3 -
                      D_800625A0->entry->cursor[D_800625A0->buffer_index].y0);
    D_800625A0->entry->cursor_buffer = D_800625A0->buffer_index;
    func_8002675C(D_800625A0->sprite_sheet, 0xF9, D_800625A0->entry->confirm,
                  D_800625A0->buffer_index, 0xE8, 0xB6, 0x1000);
    func_8002675C(D_800625A0->sprite_sheet, 0xFC, D_800625A0->entry->back,
                  D_800625A0->buffer_index, 0xE0, 0xC6, 0x1000);
    D_800625A0->entry->frame_count =
        func_8002675C(D_800625A0->sprite_sheet, 0xF0, D_800625A0->entry->frame,
                      D_800625A0->buffer_index, 0xF4, 0x6E, 0x1000);
    D_800625A0->entry->parts_buffer = D_800625A0->buffer_index;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->x0 = 0xF8;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->y0 = 0xB6;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->x1 = D_800625A0->labels[0].width + 0xF8;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->y1 = 0xB6;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->x2 = 0xF8;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->y2 = 0xC3;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->x3 = D_800625A0->labels[0].width + 0xF8;
    (D_800625A0->labels[0].poly + D_800625A0->buffer_index)->y3 = 0xC3;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->x0 = 0xF0;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->y0 = 0xC6;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->x1 = D_800625A0->labels[3].width + 0xF0;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->y1 = 0xC6;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->x2 = 0xF0;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->y2 = 0xD3;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->x3 = D_800625A0->labels[3].width + 0xF0;
    (D_800625A0->labels[3].poly + D_800625A0->buffer_index)->y3 = 0xD3;
    D_800625A0->labels[3].buffer = D_800625A0->buffer_index;
    func_801CA558();
    D_800625A0->entry->shown = 1;
    D_800625A0->flags->entry_on = 1;
}

/* Render the name being entered (text codes `codes`) into VRAM at (0x180, 0xEA). */
void func_801CB1C4(u8 *codes) {
    RECT rect;
    u8 *image = func_80031BDC(0x3F6, 0);

    func_8003F8E8(image, 0x3F6);
    func_80034EAC(codes, image, 0x24, 0);
    rect.x = 0x180;
    rect.y = 0xEA;
    rect.w = 0x28;
    rect.h = 13;
    func_80044894(&rect, image);
    func_800445D0(0);
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

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB33C);

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
    state->block_350->screen.x = 0x2C0;
    state->block_350->screen.y = 0x100;
    state->block_350->screen.w = 0x140;
    state->block_350->screen.h = 0xE0;
    state->backdrop->b_15B = 0x40;
    func_801C58B8();
    func_801C5A30();
    func_801C5EAC();
    func_801C60BC();
    func_801C5F08();
    D_800625A0->active = 1;
    D_800625A0->sounds = 1;
    func_801CB33C();
    func_801CA400();
}
