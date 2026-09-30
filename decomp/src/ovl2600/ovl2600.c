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

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C5318);

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

    func_8002675C(D_800625A0->sprite_sheet, 0x105, &panel->frame_top, D_800625A0->buffer_index,
                  x, y, 0x1000);
    func_800263E4(D_800625A0->sprite_sheet, 0x105, &panel->frame_bottom,
                  D_800625A0->buffer_index, x, y + h - 8, 0x1000, 0, 1);
    func_8002675C(D_800625A0->sprite_sheet, 0x106, &panel->frame_side, D_800625A0->buffer_index,
                  x, y + 8, 0x1000);
    func_801C6408(panel->top, x, y, 8, 8);
    func_801C6408(panel->bottom, x, y + h, 8, -8);
    func_801C6408(panel->side, x, y + 8, 8, h - 8);
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
    func_801C6408(panel->corner_at[0], x - 8, y + 8, 16, -16);
    func_801C6408(panel->corner_at[1], x + w + 8, y + 8, -16, -16);
    func_801C6408(panel->corner_at[2], x - 8, y + h - 8, 16, 16);
    func_801C6408(panel->corner_at[3], x + w + 8, y + h - 8, -16, 16);
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

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7894);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7A18);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7AA4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7C28);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7D74);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C7F48);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C811C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C82F0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C84C4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8600);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C874C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8970);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8AE8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8E38);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8EC8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C8F58);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C90D0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9160);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C92BC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9338);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C97FC);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C983C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C989C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C98E8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9AF4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9C34);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9D5C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9F1C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9F60);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9F90);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801C9FC0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA39C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA400);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CA558);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CADC8);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB1C4);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB25C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB2F0);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CB33C);

INCLUDE_ASM(".local/decomp/ovl2600/asm/nonmatchings/ovl2600", func_801CBDBC);
