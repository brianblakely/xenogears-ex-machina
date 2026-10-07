/* Menu overlay unit 801E8070. Its rodata starts at 801C5278, where the jump
 * tables return to 0 mod 8 after 801E433C's odd-length table: the text
 * boundary lies between 801E433C and 801E8070. */
#include "menu.h"

/* Place label `selected` for layout `mode` (0: window row `row`, clearing the
 * other flags first; 1-3, 5, 6: 3D label vertices from the mode's tables;
 * 4: fixed at (4c, 12)) and mark it shown in `flags`. */
void func_801E8070(u8 count, MenuLabelSlot *labels, u8 *table, s32 *offsets, u8 *flags, u8 selected, u8 row,
                   u8 mode) {
    s32 first;

    first = 0;
    switch (mode) {
    case 0:
        func_801E8044(count, flags);
        (labels[selected].polys + D_800625A0->bufferIndex)->x0 = D_801E9A00[row + selected] + 0x16 + offsets[selected];
        (labels[selected].polys + D_800625A0->bufferIndex)->y0 = D_801E9A2C[row + selected] - 0x22;
        (labels[selected].polys + D_800625A0->bufferIndex)->x1 =
            D_801E9A00[row + selected] + 0x16 + offsets[selected] + labels[selected].width;
        (labels[selected].polys + D_800625A0->bufferIndex)->y1 = D_801E9A2C[row + selected] - 0x22;
        (labels[selected].polys + D_800625A0->bufferIndex)->x2 = D_801E9A00[row + selected] + 0x16 + offsets[selected];
        (labels[selected].polys + D_800625A0->bufferIndex)->y2 = D_801E9A2C[row + selected] - 0x15;
        (labels[selected].polys + D_800625A0->bufferIndex)->x3 =
            D_801E9A00[row + selected] + 0x16 + offsets[selected] + labels[selected].width;
        (labels[selected].polys + D_800625A0->bufferIndex)->y3 = D_801E9A2C[row + selected] - 0x15;
        break;
    case 1:
        func_801C851C(labels[selected].verts, D_801E9EC4[selected], D_801E9EE4, labels[selected].width, 0xd);
        break;
    case 5:
        first = 8;
    case 2:
        func_801C851C(labels[selected].verts, D_801E9EE8[first + selected], D_801E9F28[row], labels[selected].width,
                      0xd);
        break;
    case 3:
        func_801C851C(labels[selected].verts, 0x18, D_801E9F30[row], labels[selected].width, 0xd);
        break;
    case 4:
        (labels[selected].polys + D_800625A0->bufferIndex)->x0 = 0x4c;
        (labels[selected].polys + D_800625A0->bufferIndex)->y0 = 0x12;
        (labels[selected].polys + D_800625A0->bufferIndex)->x1 = labels->width + 0x4c;
        (labels[selected].polys + D_800625A0->bufferIndex)->y1 = 0x12;
        (labels[selected].polys + D_800625A0->bufferIndex)->x2 = 0x4c;
        (labels[selected].polys + D_800625A0->bufferIndex)->y2 = 0x1f;
        (labels[selected].polys + D_800625A0->bufferIndex)->x3 = labels->width + 0x4c;
        (labels[selected].polys + D_800625A0->bufferIndex)->y3 = 0x1f;
        break;
    case 6:
        func_801C851C(labels[selected].verts, D_801E9F68[selected], D_801E9F70[selected], labels[selected].width, 0xd);
        break;
    }
    labels[selected].count = D_800625A0->bufferIndex;
    flags[selected] = 1;
}

/* Open the command window: grow the cursor column one command per two
 * frames (the label column one behind), up to `count` commands. */
void func_801E8474(s32 count, MenuCommandImages *images) {
    s32 n;
    s32 i;

    D_800625A0->screenImages->captured = 0;
    D_800625A0->screenImages->refresh = 0;
    D_800625A0->party->redraw9 = 1;
    for (n = 1; n <= count; n++) {
        if (n != count) {
            D_800625A0->screenImages->count = 0;
            for (i = 0; i < n; i++) {
                D_800625A0->screenImages->count +=
                    func_8002675C(D_800625A0->sheet, images[i].cursor,
                                  &D_800625A0->screenImages->packets[D_800625A0->screenImages->count * 2],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
            }
            D_800625A0->screenImages->buffer = D_800625A0->bufferIndex;
        }
        D_800625A0->screenImages->count2 = 0;
        if (n != 1) {
            for (i = 0; i < n - 1; i++) {
                D_800625A0->screenImages->count2 +=
                    func_8002675C(D_800625A0->sheet, images[i].label,
                                  &D_800625A0->screenImages->packets2[D_800625A0->screenImages->count2 * 2],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
            }
            D_800625A0->screenImages->buffer2 = D_800625A0->bufferIndex;
        }
        for (i = 0; i < 2; i++) {
            func_801C7BF4();
        }
    }
}

/* Open the choice window of the command at `offset` past the top cursor:
 * grow its cursor and label columns one choice per two frames (until an
 * empty choice, ffff), counting the choices. */
#ifdef NON_MATCHING
void func_801E86C8(u8 offset) {
    s32 n;
    s32 i;
    u8 growing;

    D_800625A0->spriteLists->firstCount = 0;
    D_800625A0->spriteLists->secondCount = 0;
    D_800625A0->party->redrawA = 1;
    growing = 1;
    for (n = 1; n < 5; n++) {
        D_800625A0->spriteLists->firstCount = 0;
        D_800625A0->choiceCount = 0;
        for (i = 0; i < n; i++) {
            if (D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2] != 0xffff) {
                D_800625A0->spriteLists->firstCount +=
                    func_8002675C(D_800625A0->sheet, D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2],
                                  &D_800625A0->spriteLists->first[D_800625A0->spriteLists->firstCount * 2],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
                D_800625A0->choiceCount++;
            } else {
                growing = 0;
            }
        }
        D_800625A0->spriteLists->firstStart = D_800625A0->bufferIndex;
        if (growing) {
            for (i = 0; i < 2; i++) {
                func_801C7BF4();
            }
        }
        D_800625A0->spriteLists->secondCount = 0;
        for (i = 0; i < n; i++) {
            if (D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2] != 0xffff) {
                D_800625A0->spriteLists->secondCount +=
                    func_8002675C(D_800625A0->sheet, D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2 + 1],
                                  &D_800625A0->spriteLists->second[D_800625A0->spriteLists->secondCount * 2],
                                  D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
            }
        }
        D_800625A0->spriteLists->secondStart = D_800625A0->bufferIndex;
        if (growing) {
            for (i = 0; i < 2; i++) {
                func_801C7BF4();
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/slot39/asm/nonmatchings/slot39_801E8070", func_801E86C8);
#endif

/* Lay out the command cursor sprites for `count` commands (images of the
 * `images`, the chosen one `cursor` taking its lit image,
 * +d), then mark the command window for redraw. */
void func_801E8978(u8 count, u8 cursor, MenuCommandImages *images) {
    s32 i;
    s32 image;

    D_800625A0->screenImages->count = 0;
    D_800625A0->screenImages->count2 = 0;
    for (i = 0; i < count; i++) {
        if (i == cursor) {
            image = images[i].cursor + 0xd;
        } else {
            image = images[i].cursor;
        }
        D_800625A0->screenImages->count +=
            func_8002675C(D_800625A0->sheet, image,
                          &D_800625A0->screenImages->packets[D_800625A0->screenImages->count * 2],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
        D_800625A0->screenImages->count2 +=
            func_8002675C(D_800625A0->sheet, images[i].label,
                          &D_800625A0->screenImages->packets2[D_800625A0->screenImages->count2 * 2],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
    }
    D_800625A0->screenImages->buffer = D_800625A0->bufferIndex;
    D_800625A0->screenImages->buffer2 = D_800625A0->bufferIndex;
    func_801D1EE0(cursor, 1);
    D_800625A0->party->redraw4 = 1;
}

/* Lay out the choice cursor sprites of the command at `offset` past the top
 * cursor (the chosen one lit), then mark the window for redraw. */
void func_801E8B4C(u8 offset) {
    s32 i;
    s32 image;

    D_800625A0->spriteLists->firstCount = 0;
    D_800625A0->spriteLists->secondCount = 0;
    for (i = 0; i < D_800625A0->choiceCount; i++) {
        if (i == D_800625A0->choice) {
            image = D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2] + 0xd;
        } else {
            image = D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2];
        }
        D_800625A0->spriteLists->firstCount +=
            func_8002675C(D_800625A0->sheet, image,
                          &D_800625A0->spriteLists->first[D_800625A0->spriteLists->firstCount * 2],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
        D_800625A0->spriteLists->secondCount +=
            func_8002675C(D_800625A0->sheet, D_801EA1EC[(offset + D_800625A0->cursor) * 8 + i * 2 + 1],
                          &D_800625A0->spriteLists->second[D_800625A0->spriteLists->secondCount * 2],
                          D_800625A0->bufferIndex, 0xa0, 0x96, 0x1000);
    }
    D_800625A0->spriteLists->firstStart = D_800625A0->bufferIndex;
    D_800625A0->spriteLists->secondStart = D_800625A0->bufferIndex;
    func_801D1EE0(D_800625A0->choice + 7, 1);
    D_800625A0->party->redraw4 = 1;
}

/* Render the two name lines of name pair `image` (ff: blank) and upload them
 * to the label area of row `row` (rows pair up on one 40x13 image). */
void func_801E8DA8(u8 image, u8 row) {
    RECT rect;
    u8 *pixels;

    pixels = func_80031BDC(0x3f6, 0);
    bzero(pixels, 0x3f6);
    if (image != 0xff) {
        func_80034EAC(D_8006D634.names[image >> 1][0], pixels, 0x24, 0);
        func_80034EAC(D_8006D634.names[image >> 1][1], pixels, 0x24, 1);
    }
    rect.x = D_801EA578[row >> 1] + 0x180;
    rect.y = D_801EA5C4[row >> 1];
    rect.w = 0x28;
    rect.h = 0xd;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_800320E8(pixels);
}

/* Set `poly`'s blending for `mode`: 0 opaque, 1 additive-dim, 2 plain,
 * 3 dim. */
void func_801E8EAC(POLY_FT4 *poly, u8 mode) {
    u8 shade;

    SetShadeTex(poly, 0);
    switch (mode) {
    case 1:
        poly->tpage |= 0x20;
        SetSemiTrans(poly, 1);
    case 3:
        shade = 0x21;
        break;
    case 0:
        SetSemiTrans(poly, 0);
    case 2:
        shade = 0x80;
        break;
    default:
        return;
    }
    poly->r0 = shade;
    poly->g0 = shade;
    poly->b0 = shade;
}

/* Set the blending of portrait `index`'s quads of the current buffer: plain
 * (2), or dim (3) when the dim flag `mode` is set. Every edge list is walked
 * four pairs deep, so each pass also covers the list after it, as the
 * original does. */
void func_801E8F60(u8 index, u8 mode) {
    s32 i;

    /* The dim flag becomes the blending mode. */
    if (mode) {
        mode = 3;
    } else {
        mode = 2;
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->corner[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edge[0][i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edge[1][i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edge[2][i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->edge[3][i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    for (i = 0; i < 2; i++) {
        func_801E8EAC(&D_800625A0->portraits[index]->frameEnds[i * 2 + D_800625A0->portraits[index]->buffer], mode);
    }
    func_801E8EAC(&D_800625A0->portraits[index]->frameSide[D_800625A0->portraits[index]->buffer], mode);
}

/* Make `poly` semi-transparent, textured without shading, at neutral colour. */
void func_801E91C4(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Place `poly` at (x, y) with size (w, h) and texture origin (u, v). */
void func_801E920C(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h) {
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + w;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + h;
    poly->u3 = u + w;
    poly->v3 = v + h;
}

/* Initialise `poly` as an opaque textured quad at neutral colour. */
void func_801E927C(POLY_FT4 *poly) {
    SetPolyFT4(poly);
    SetSemiTrans(poly, 0);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* Unless 8002c3d8 reports ready, reset the drive and retry command 8 until it
 * succeeds. */
void func_801E92CC(void) {
    if (func_8002C3D8() == 0) {
        func_8002A498(0);
        func_80028A60(0);
        func_8002A428(0);
        func_80028A60(0);
        VSync(3);
        do {
            VSync(3);
        } while (CdControlB(8, 0, D_801EA8F4) == 0);
    }
}

/* Read `size` bytes of host file `name` into `buffer` (development PC link). */
void func_801E9340(char *name, void *buffer, s32 size) {
    s32 handle;

    handle = PCopen(name, 0, 0);
    func_8004C398(handle, buffer, size);
    PCclose(handle);
}

/* Check that disc `disc` is in the drive and load its directory: from the
 * host files on the development link, else after waiting for the lid to
 * close and the drive to settle, from the disc label and files 18 and 28.
 * Returns 0 when loaded, 2 when no disc label was read, 3 for the other
 * disc. */
s32 func_801E93A0(s32 disc) {
    DiscLabel label = { { 0 } };
    u8 pos[4];
    s32 result;
    s32 ok;

    func_80028A60(0);
    if (func_8002C3D8() != 0) {
        result = 0;
        if (disc == 1) {
            func_801E9340("c:\\work\\cdrom.mdg", D_8004FDF0, 0x8000);
            func_801E9340("c:\\work\\cdrom.fid", D_8004FDF4, 0x7a);
            func_801E9340("c:\\work\\cdrom.fnd", D_8004FE48, 0x40000);
        } else {
            func_801E9340("c:\\work\\cdrom2.mdg", D_8004FDF0, 0x8000);
            func_801E9340("c:\\work\\cdrom2.fid", D_8004FDF4, 0x7a);
            func_801E9340("c:\\work\\cdrom2.fnd", D_8004FE48, 0x40000);
        }
        return result;
    }
    CdIntToPos(0, pos);
    do {
        VSync(3);
        CdControlB(1, 0, D_801EA8F4);
    } while (!(D_801EA8F4[0] & 0x10));
    do {
        VSync(3);
        CdControlB(1, 0, D_801EA8F4);
    } while (D_801EA8F4[0] & 0x10);
    do {
        VSync(3);
        ok = CdControlB(1, 0, D_801EA8F4);
    } while (!(D_801EA8F4[0] & 2) || ok == 0);
retry:
    do {
        VSync(3);
    } while (CdControlB(0x13, 0, D_801EA8F4) == 0);
    do {
        VSync(3);
    } while (CdControlB(2, pos, D_801EA8F4) == 0);
    ok = CdControlB(0x15, 0, D_801EA8F4);
    if ((D_801EA8F4[0] & 1) && (D_801EA8F4[1] & 0x40)) {
        if (ok == 0) {
            return 2;
        }
    } else if (ok == 0) {
        goto retry;
    }
    func_8002A428(0xa0);
    func_80028A60(0);
    VSync(3);
    VSync(3);
    func_8002954C(0x17, &label, 0x10, 0, 0);
    func_80028A60(0);
    if (label.tag == 0x4e45585f) {
        if (label.disc == disc + '0') {
            func_8002954C(0x18, D_8004FDF0, 0x8000, 0, 0);
            result = 0;
            func_80028A60(0);
            func_8002954C(0x28, D_8004FDF4, 0x7a, 0, 0);
            func_80028A60(0);
        } else {
            result = 3;
        }
    } else {
        result = 2;
    }
    return result;
}
