#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"

/* Allocate the text quads, load the font (with its palette's colours 0, 2
 * and 3 replaced) and the banner image, and build the banner sprite. */
void func_8007E634(MenuFiles *files) {
    TimImage image;
    SceneSprite *banner;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s16 *palette;
    s32 i;

    D_800926D4[0] = func_80031BDC(0xFA0, 0);
    D_800926D4[1] = func_80031BDC(0xFA0, 0);
    for (i = 0; i < 100; i++) {
        ((u8 *)&D_800926D4[0][i].tag)[3] = 0;
        ((u8 *)&D_800926D4[1][i].tag)[3] = 0;
    }
    OpenTIM(files->font);
    ReadTIM(&image);
    palette = image.caddr;
    palette[2] = -0x6F9D;
    palette[0] = 0;
    palette[3] = -1;
    LoadImage(image.crect, image.caddr);
    LoadImage(image.prect, image.paddr);
    D_800926E4 = GetClut(image.crect->x, image.crect->y);
    D_800926E0 = GetTPage(0, 1, image.prect->x, image.prect->y);
    D_800926DC = 0;
    OpenTIM(files->banner);
    ReadTIM(&image);
    palette = image.caddr;
    palette[0] = 0;
    LoadImage(image.crect, image.caddr);
    LoadImage(image.prect, image.paddr);
    banner = &D_800954D8[0];
    setlen(&D_800954D8[0].sprite, 4);
    setcode(&D_800954D8[0].sprite, 0x65);
    SetDrawTPage(&banner->tpage, 0, 0, GetTPage(0, 1, image.prect->x, image.prect->y));
    D_800954D8[0].sprite.clut = GetClut(image.crect->x, image.crect->y);
    D_800954D8[0].sprite.x0 = 0x40;
    D_800954D8[0].sprite.y0 = 0xBE;
    D_800954D8[0].sprite.w = 0xC4;
    D_800954D8[0].sprite.h = 0xD;
    D_800954D8[0].sprite.u0 = image.prect->x * 4;
    D_800954D8[0].sprite.v0 = image.prect->y;
    D_800954D8[1] = *banner;
}

void func_8007E894(s32 x, s32 y) {
    D_800926E8 = x;
    D_800926EC = y;
}

/* The font glyph of a character: digits, capitals and a few punctuation
 * marks; NULL for anything else. */
Glyph *func_8007E8AC(s32 ch) {
    if (ch >= '0' && ch <= '9') {
        ch -= '0';
    } else if (ch >= 'A' && ch <= 'Z') {
        ch -= 'A' - 10;
    } else {
        switch (ch) {
        case '!':
            ch = 0x24;
            break;
        case ':':
            ch = 0x25;
            break;
        case '-':
            ch = 0x26;
            break;
        case '/':
            ch = 0x27;
            break;
        case '#':
            ch = 0x28;
            break;
        case ' ':
            ch = 0x29;
            break;
        case '\'':
            ch = 0x2A;
            break;
        default:
            return NULL;
        }
    }
    return &D_80091230[ch];
}

void func_8007E954(s32 value) {
    D_800912DC = value;
}

#ifdef NON_MATCHING
/* Draw one character at the text cursor (at most 101 quads a frame; '('
 * only advances) and move the cursor right by its scaled width. Declared
 * int without a return value, as the original's unfilled delay slot shows.
 * Does not match: the original loads the glyph width before storing the
 * first vertex, and the texture page/CLUT before the packet length. Reading
 * the width first and D_800912DC as aggregate (struct/array) memory makes
 * everything up to the third UV match (the scale load then stays behind the
 * first vertex store); the rest is the packet length/colour schedule. */
s32 func_8007E964(s32 ch) {
    PolyFT4Words *quad;
    Glyph *glyph;
    s32 right;

    if (D_800926DC < 101) {
        quad = D_800926D4[D_800928A0];
        quad += D_800926DC;
        glyph = func_8007E8AC(ch);
        if (glyph != NULL) {
            if (ch != '(') {
                quad->xy0 = D_800926E8 | (D_800926EC << 16);
                ch = glyph->width | 3; /* the quad width, in the same variable */
                right = D_800926E8 + ((ch * D_800912DC) >> 8);
                quad->xy1 = right | (D_800926EC << 16);
                quad->xy2 = D_800926E8 | ((D_800926EC + glyph->height) << 16);
                quad->xy3 = right | ((D_800926EC + glyph->height) << 16);
                quad->uv0 = glyph->u | (glyph->v << 8);
                quad->uv1 = (glyph->u + ch) | (glyph->v << 8);
                quad->uv2 = glyph->u | ((glyph->v + (glyph->height + 1)) << 8);
                quad->uv3 = (glyph->u + ch) | ((glyph->v + (glyph->height + 1)) << 8);
                quad->len = 9;
                quad->rgbc = D_800926F0 | (D_800926F4 << 8) | (D_800926F8 << 16) | 0x2C000000;
                quad->tpage = D_800926E0;
                quad->clut = D_800926E4;
                D_800926DC++;
            }
            D_800926E8 += ((glyph->width * D_800912DC) >> 8) + 2;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu4", func_8007E964);
#endif

/* Width of a text string in pixels at the current text scale. */
s32 func_8007EB6C(u8 *text) {
    s32 width = 0;

    while (*text != 0) {
        width += ((func_8007E8AC(*text++)->width * D_800912DC) >> 8) + 2;
    }
    return width;
}

/* Draw a line of text at the cursor and move the cursor to the next line. */
void func_8007EBE0(u8 *text) {
    s32 x = D_800926E8;

    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text centred on the cursor, then move to the next line. */
void func_8007EC54(u8 *text) {
    s32 x = D_800926E8;

    D_800926E8 -= func_8007EB6C(text) / 2;
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text ending at the cursor, then move to the next line. */
void func_8007ECF0(u8 *text) {
    s32 x = D_800926E8;

    D_800926E8 -= func_8007EB6C(text);
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text shifted left by an offset, then move to the next line. */
void func_8007ED84(u8 *text, s32 offset) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s32 x = D_800926E8;

    D_800926E8 = x - offset;
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Set the text colour: highlighted (fading red) or plain white. */
void func_8007EE08(s32 highlight) {
    if (highlight) {
        D_800926F0 = D_80059488 * 20;
        D_800926F4 = 0xFF;
        D_800926F8 = 0;
    } else {
        D_800926F0 = 0xFF;
        D_800926F4 = 0xFF;
        D_800926F8 = 0xFF;
    }
}

/* Set the text colour: highlighted (fading toward blue) or plain white.
 * Sample the VBlank counter separately for the red and green channels. */
void func_8007EE68(s32 highlight) {
    if (highlight) {
        s32 red = 0xFF - D_80059488 * 20;
        s32 green = 0xFF - D_80059488 * 20;

        D_800926F8 = 0xFF;
        D_800926F0 = red;
        D_800926F4 = green;
        return;
    }
    D_800926F0 = 0xFF;
    D_800926F4 = 0xFF;
    D_800926F8 = 0xFF;
}

#ifdef NON_MATCHING
/* Build the list of the 49 entries (or, when filtering, of those whose
 * required level the current level reaches) and order it when filtering.
 * Does not match: the source and entry pointers get swapped registers. */
void func_8007EEE8(s32 filter) {
    s32 level = D_8006EF64;
    ListEntry **list = func_80031BDC(0xC4, 1);
    MoveList *source;
    s32 i;

    source = D_80092874;
    D_800928EC = list;
    D_80092888 = 0;
    for (i = 0; i < 49; i++, source++) {
        if (!filter || source->level <= level) {
            list[D_80092888++] = &D_80091964[i];
        }
    }
    if (filter) {
        func_8008895C();
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu4", func_8007EEE8);
#endif

/* Allocate and lay out the 49 portrait slots: palette rows 511 down and
 * a 7x7 grid of image areas. */
void func_8007EFB4(void) {
    u8 unused[0x30]; /* never used; the original frame keeps its slot */
    GridCell *cell;
    s32 id;
    s32 row;
    s32 col;
    s16 top;

    cell = D_8009270C = func_80031BDC(0x3D4, 0);
    id = 0x1FF;
    for (row = 0; row < 7; row++) {
        top = row * 0x20 + 0x1A0;
        for (col = 0; col < 7; col++) {
            s16 left = col << 6;

            cell->clut_x = 0x200;
            cell->clut_y = id--;
            cell->clut_w = 0x80;
            cell->clut_h = 1;
            cell->image_x = top;
            cell->image_y = left;
            cell->image_w = 0x1E;
            cell->image_h = 0x40;
            cell++;
        }
    }
}

/* Draw the portrait of a list entry (the index wraps around the list) on
 * the left or right side. At fade 64 it is shown full size and unshaded;
 * below, it is shaded and shrunk by fade / 16, inset from x and grown from
 * a 0x34 by 0x38 base. */
void func_8007F05C(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade) {
    GridCell *cell;
    s32 inset;
    s32 top;
    s32 bottom;
    s32 left;
    s32 width;
    s32 height;
    s32 u;

    if (right_side) {
        x += 0xD3;
    } else {
        x += 0x33;
    }
    if (index > D_80092888 - 1) {
        index -= D_80092888;
    }
    if (index < 0) {
        index += D_80092888;
    }
    index = D_800928EC[index]->id;
    cell = &D_8009270C[index];
    if (fade == 0x40) {
        quad->len = 9;
        ((u8 *)&quad->rgbc)[3] = 0x2D;
        quad->xy0 = x | 0x300000;
        quad->xy1 = (x + 0x3C) | 0x300000;
        quad->xy2 = x | 0x700000;
        quad->xy3 = (x + 0x3C) | 0x700000;
    } else {
        fade += 0x40;
        quad->len = 9;
        quad->rgbc = fade | (fade << 8) | (fade << 16) | 0x2C000000;
        fade -= 0x40;
        fade >>= 4; /* from here on, how far the portrait shrinks */
        inset = fade - 4;
        left = x - inset;
        top = 0x34 - fade;
        quad->xy0 = left | (top << 16);
        width = 0x34;
        quad->xy1 = (left + width + fade * 2) | (top << 16);
        height = 0x38;
        bottom = top + height + fade * 2;
        quad->xy2 = left | (bottom << 16);
        quad->xy3 = (left + width + fade * 2) | (bottom << 16);
    }
    u = cell->image_x * 2;
    quad->uv0 = u | (cell->image_y << 8);
    quad->uv1 = (u + 0x3B) | (cell->image_y << 8);
    quad->uv2 = u | ((cell->image_y + 0x3F) << 8);
    quad->uv3 = (u + 0x3B) | ((cell->image_y + 0x3F) << 8);
    quad->clut = GetClut(cell->clut_x, cell->clut_y);
    quad->tpage = GetTPage(1, 0, cell->image_x & 0xFF80, cell->image_y);
    AddPrim(D_80092938, quad);
}

/* Draw the two-player selection: each side's pick, sliding in from its
 * previous one (the long way round wraps), with its neighbours when the
 * side is available, then "VS" and both names. The arguments are unused. */
void func_8007F258(void *packets, s32 arg) {
    Vector unused[2]; /* the original frame has 32 unused bytes */
    PolyFT4Words *quad = D_80099DA8[D_800928A0];
    s32 step;
    s32 row;

    step = D_80092714 - D_80092700;
    if (step != 0) {
        if (abs(step) >= 4) {
            step = -step;
        }
        D_80092718 = step > 0 ? -0x24 : 0x24;
        D_8009271C = D_80092718 = D_80092718; /* the original rereads it */
    }
    step = D_80092720 - D_80092704;
    if (step != 0) {
        if (abs(step) >= 4) {
            step = -step;
        }
        D_80092724 = step > 0 ? -0x24 : 0x24;
        D_80092728 = D_80092724 = D_80092724;
    }
    if (D_80092718 != 0) {
        D_80092718 = D_80092718 > 0 ? D_80092718 - 2 : D_80092718 + 2;
    }
    if (D_8009271C != 0) {
        D_8009271C = D_8009271C > 0 ? D_8009271C - 4 : D_8009271C + 4;
    }
    if (D_80092724 != 0) {
        D_80092724 = D_80092724 > 0 ? D_80092724 - 2 : D_80092724 + 2;
    }
    if (D_80092728 != 0) {
        D_80092728 = D_80092728 > 0 ? D_80092728 - 4 : D_80092728 + 4;
    }
    D_80092714 = D_80092700;
    D_80092720 = D_80092704;
    row = D_80092718 >= 0;
    func_8007F05C(D_80092700, quad++, 0, D_8009271C, ((0x24 - abs(D_8009271C)) << 6) / 36);
    if (!(D_80092710 & 1)) {
        func_8007F05C(D_80092700 + D_800912E0[row][0], quad++, 0, D_800912E0[row][3] + D_80092718,
                      (abs(D_80092718) << 6) / 36);
        func_8007F05C(D_80092700 + D_800912E0[row][1], quad++, 0, -0x24, 0);
        func_8007F05C(D_80092700 + D_800912E0[row][2], quad++, 0, 0x24, 0);
    }
    row = D_80092724 >= 0;
    func_8007F05C(D_80092704, quad++, 1, D_80092728, ((0x24 - abs(D_80092728)) << 6) / 36);
    if (!(D_80092710 & 2)) {
        func_8007F05C(D_80092704 + D_800912E0[row][0], quad++, 1, D_800912E0[row][3] + D_80092724,
                      (abs(D_80092724) << 6) / 36);
        func_8007F05C(D_80092704 + D_800912E0[row][1], quad++, 1, -0x24, 0);
        func_8007F05C(D_80092704 + D_800912E0[row][2], quad, 1, 0x24, 0);
    }
    func_8007E894(0xA0, 0x78);
    func_8007EC54("VS");
    func_8007E894(0x50, 0x78);
    if (func_800888E4(D_800928EC[D_80092700]->id)) {
        func_8007EE68(D_8009272C);
    } else {
        func_8007EE08(D_8009272C);
    }
    func_8007EC54(D_800928EC[D_80092700]->name);
    func_8007E894(0xF0, 0x78);
    if (func_800888E4(D_800928EC[D_80092704]->id)) {
        func_8007EE68(D_80092730);
    } else {
        func_8007EE08(D_80092730);
    }
    func_8007EC54(D_800928EC[D_80092704]->name);
    func_8007EE08(0);
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu4", D_8006FE8C);

void func_8007F834(void) {
    D_80092740 = 0;
    D_8009273C = 0;
    D_80092744 = 0;
}

/* Leave the settings screen: camera mode 1, and both actors' previous
 * stance effect state (unkD4 bits 2-3) set to 3. */
void func_8007F854(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    D_800912F0 = 1;
    func_80083C0C(1);
    D_80092734 = (Menu *)NULL;
    func_8007F834();
    ACTOR_STANCE_BITS(&D_8009872C)->prev_stance = 3;
    ACTOR_STANCE_BITS(&D_80097010)->prev_stance = 3;
}

void func_8007F8B4(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    func_80083C0C(1);
    D_80092734 = (Menu *)NULL;
    func_8007F834();
}

void func_8007F8E4(void) {
    func_80080C48(0);
    if (D_80099D98.option6 != 0 || D_80092950 == 1) {
        func_80083C0C(3);
    } else {
        D_80092950--;
        func_80083C0C(6);
    }
}

/* Highlight the text of a page's entry when it is under the cursor. */
void func_8007F948(MenuPage *page, s32 entry) {
    if (page->cursor == entry) {
        func_8007EE08(1);
    } else {
        func_8007EE08(0);
    }
}

/* Name of the chosen first setting. */
char *func_8007F97C(void) {
    return D_800912F4[D_80099D98.level];
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu4", D_8006FF5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu4", D_8006FF60);

/* Draw the values column of the settings page, right-aligned, applying the
 * chosen speed as it is shown. */
void func_8007F9A0(MenuPage *page) {
    char text[8];

    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007F948(page, 0);
    func_8007ECF0(func_8007F97C());
    func_8007F948(page, 1);
    sprintf(text, D_8006FF5C, D_80099D98.speed + 1);
    D_80099DA4 = D_8009292C = D_8009130C[D_80099D98.speed];
    func_8007ECF0(text);
    func_8007F948(page, 2);
    sprintf(text, D_8006FF60, D_80091300[D_80099D98.rate]);
    func_8007ECF0(text);
    func_8007F948(page, 3);
    func_8007ECF0(D_80099D98.com1 ? "COM" : "USER1");
    func_8007F948(page, 4);
    func_8007ECF0(D_80099D98.driven ? "COM" : "USER2");
    func_8007EE08(0);
}

/* Draw the values column of the second settings page; the chosen entry of
 * setting 10 is also passed to 80081100 as 0x15 + entry. */
void func_8007FB0C(MenuPage *page) {
    char text[8];

    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007ECF0(D_8006FF7C);
    func_8007F948(page, 1);
    func_8007ECF0(D_8009132C[D_80099D98.command]);
    func_80081100(D_80099D98.command + 0x15, 1);
    func_8007F948(page, 2);
    sprintf(text, D_8006FF60, D_80091300[D_80099D98.rate]);
    func_8007ECF0(text);
    func_8007EE08(0);
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu4", D_8006FF7C);

/* Draw the vibration page: per controller port, the vibration setting when
 * a type-4 controller without the "COM" setting is connected (the entry is
 * hidden otherwise). */
void func_8007FBEC(void) {
    MenuPage *page;
    s32 active;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    func_8007E894(0xA0, 0x8C);
    active = D_80092710 ^ 1;
    active &= 1;
    page = &((MenuPage *)D_800915AC)[5];
    if (active && ((MenuPage *)D_800915AC)[5].cursor == 0) {
        D_8009272C = 1;
    } else {
        D_8009272C = 0;
    }
    func_8007E894(0x50, 0x8C);
    if (func_80035734(0) == 4 && D_80099D98.com1 == 0) {
        if (active) {
            func_8007F948(page, 1);
        }
        func_8007EC54((D_80099D98.option4 & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        ((MenuPage *)D_800915AC)[5].item->flags &= ~4;
    } else {
        ((MenuPage *)D_800915AC)[5].item->flags |= 4;
    }
    func_8007EE08(0);

    active = D_80092710 >> 1;
    active ^= 1;
    active &= 1;
    if (active && D_80092754 == 0) {
        active = 0;
    }
    page = &((MenuPage *)D_800915AC)[6];
    if (active && ((MenuPage *)D_800915AC)[6].cursor == 0) {
        D_80092730 = 1;
    } else {
        D_80092730 = 0;
    }
    func_8007E894(0xF0, 0x8C);
    if (func_80035734(1) == 4 && D_80099D98.driven == 0) {
        if (active) {
            func_8007F948(page, 1);
        }
        func_8007EC54((D_80099D98.option5 & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        ((MenuPage *)D_800915AC)[6].item->flags &= ~4;
    } else {
        ((MenuPage *)D_800915AC)[6].item->flags |= 4;
    }
    func_8007EE08(0);
    func_8007F258(D_80092938, 1);
}

/* Draw the values column of the options page. */
void func_8007FE48(MenuPage *page) {
    char text[16];
    char *value;

    func_8007EE08(0);
    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007F948(page, 4);
    if (D_80099D98.option6 != 0) {
        sprintf(text, D_8006FF5C, D_80099D98.option6);
        value = text;
    } else {
        value = "#";
    }
    func_8007ECF0(value);
    func_8007F948(page, 5);
    func_8007ECF0(D_800912F4[D_80099D98.level]);
    func_8007F948(page, 6);
    func_8007ECF0(D_80092884 ? "ON" : "OFF");
    func_8007EE08(0);
}

/* Step a settings value with left/right: flag 4 reverses the direction,
 * flag 2 uses the repeating buttons, flag 1 wraps around (else clamps
 * silently). Plays the cursor sound when moved. */
s32 func_8007FF70(s32 value, s32 max, s32 flags) {
    s32 step = 1;
    u32 buttons;
    s32 moved;

    if (flags & 4) {
        step = -1;
    }
    moved = 0;
    if (flags & 2) {
        buttons = D_8009274C;
    } else {
        buttons = D_80092750;
    }
    if (buttons & 0x2000) {
        value += step;
    }
    if (buttons & 0x8000) {
        value -= step;
    }
    if (buttons & 0xA000) {
        moved = 1;
    }
    if (flags & 1) {
        if (value == -1) {
            value = max;
        }
        if (value > max) {
            value = 0;
        }
    } else {
        if (value == -1) {
            moved = 0;
            value = 0;
        }
        if (value > max) {
            moved = 0;
            value = max;
        }
    }
    if (moved) {
        func_8008EB4C(0x20);
    }
    return value;
}

void func_80080054(void) {
    D_80099D98.level = func_8007FF70(D_80099D98.level, 2, 0);
}

void func_80080090(void) {
    D_80099D98.speed = func_8007FF70(D_80099D98.speed, 7, 2);
}

void func_800800CC(void) {
    D_80099D98.rate = func_8007FF70(D_80099D98.rate, 4, 2);
}

void func_80080108(void) {
    D_80099D98.option4 = func_8007FF70(D_80099D98.option4, 1, 1);
}

void func_80080144(void) {
    D_80099D98.option5 = func_8007FF70(D_80099D98.option5, 1, 1);
}

void func_80080180(void) {
    D_80099D98.com1 = func_8007FF70(D_80099D98.com1, 1, 1);
}

void func_800801BC(void) {
    D_80099D98.driven = func_8007FF70(D_80099D98.driven, 1, 1);
}

void func_800801F8(void) {
    D_80099D98.option6 = func_8007FF70(D_80099D98.option6, 3, 2);
}

void func_80080234(void) {
    D_80092884 = func_8007FF70(D_80092884, 1, 1);
}

void func_80080268(void) {
    D_80099D98.command = func_8007FF70(D_80099D98.command, 13, 3);
}

/* First side's selection: cancel, move (skipping the other side's pick
 * unless shared picks are allowed or both already coincide) and confirm. */
void func_800802A4(void) {
    s32 same;

    if (D_80091364 == 0 && (D_8005948C & 0x40)) {
        D_80092710 &= ~1;
        func_80085134(0);
        func_8008EB4C(0x22);
    }
    if (!(D_80092710 & 1)) {
        same = D_80092700 == D_80092704;
        do {
            D_80092700 = func_8007FF70(D_80092700, D_80092888 - 1, 3);
        } while (D_80092700 == D_80092704 && !(D_80092748 & 1) && !same);
        if (D_80091364 == 0 && (D_8005948C & 0x20)) {
            D_80092710 |= 1;
            func_8008EB4C(0x21);
            func_8008509C(0, D_800928EC[D_80092700]->id);
        }
    }
}

/* Second side's selection; cancelling outside mode 2 leaves the screen. */
void func_8008040C(void) {
    s32 same;

    if (D_80092750 & 0x40) {
        if (D_800928C8 != 2) {
            D_80092710 = 0;
            func_80085134(1);
            func_8008EB4C(0x22);
            return;
        }
        D_80092710 &= ~2;
    }
    if (!(D_80092710 & 2)) {
        same = D_80092700 == D_80092704;
        do {
            D_80092704 = func_8007FF70(D_80092704, D_80092888 - 1, 3);
        } while (D_80092700 == D_80092704 && !(D_80092748 & 1) && !same);
        if (D_80092750 & 0x20) {
            D_80092710 |= 2;
            func_8008EB4C(0x21);
            func_8008509C(1, D_800928EC[D_80092704]->id);
        }
    }
}

/* Show both sides' picks; entries 4, 7, 11, 30 and 31 show as a plain
 * flag instead, depending on the other side's pick. */
void func_80080570(void) {
    s32 first = D_800928EC[D_80092700]->id;
    s32 second = D_800928EC[D_80092704]->id;

    switch (first) {
    case 4:
    case 7:
    case 11:
    case 30:
    case 31:
        first = second == 0;
        break;
    }
    func_8008509C(0, first);
    switch (second) {
    case 4:
    case 7:
    case 11:
    case 30:
    case 31:
        second = first != 1;
        break;
    }
    func_8008509C(1, second);
}

/* Load both picks' portraits (palette and image) into their VRAM slots and
 * mark both sides confirmed. */
void func_80080644(s32 first, s32 second) {
    u8 *data = func_80031BDC(0x2000, 0);
    u8 *other;
    GridCell *cell;

    func_8002954C((u16 *)func_800289D0(6) + first, data, 0x1000, 0, 0);
    other = data + 0x1000;
    func_8002954C((u16 *)func_800289D0(6) + second, other, 0x1000, 0, 0);
    D_80092700 = first;
    D_80092704 = second;
    D_80092714 = first;
    D_80092720 = second;
    D_80092710 = 3;
    func_80028A60(0);
    cell = &D_8009270C[first];
    LoadImage(&cell->clut_x, data);
    LoadImage(&cell->image_x, data + 0x100);
    cell = &D_8009270C[second];
    LoadImage(&cell->clut_x, other);
    LoadImage(&cell->image_x, data + 0x1100);
    func_80032C18(data, 2);
}

/* Enter the selection screen in a mode: upload every portrait once, set
 * the pages' entry counts and labels, and reset both sides. */
void func_80080780(s32 mode) {
    GridCell *cell;
    s32 i;

    if (D_80092940 == 0) {
        func_80028A60(0);
        cell = D_8009270C;
        for (i = 0; i < 49; i++, cell++) {
            LoadImage(&cell->clut_x, D_800928D8 + (i << 12));
            LoadImage(&cell->image_x, D_800928D8 + (i << 12) + 0x100);
        }
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
    D_800928C8 = mode;
    if (mode == 4) {
        ((MenuPage *)D_800915AC)[5].count = 3;
    } else {
        ((MenuPage *)D_800915AC)[5].count = 4;
    }
    ((MenuPage *)D_800915AC)[6].count = 5;
    if (mode == 3) {
        D_80091369 = 0x27;
        D_80091391 = 0x28;
    } else {
        D_80091369 = 0x25;
        D_80091391 = 0x26;
    }
    func_80083C0C(1);
    D_80092710 = 0;
    D_80092728 = 0;
    D_80092724 = 0;
    D_8009271C = 0;
    D_80092718 = 0;
    D_80092714 = D_80092700;
    D_80092720 = D_80092704;
    func_80080964(5);
}

void func_800808F4(void) {
    D_80092924 = 1;
    func_8007F834();
}

void func_80080920(void) {
    D_80092924 = 1;
    func_800719F0();
    func_8008509C(0, 0);
    func_8008509C(1, 1);
}

/* Show a page, remembering the current one; 0xff returns to it. */
void func_80080964(s32 page) {
    MenuPage *previous;

    if (page == 0xFF) {
        D_80092734 = (Menu *)((MenuPage *)D_80092738);
        return;
    }
    previous = ((MenuPage *)D_80092734);
    D_80092734 = (Menu *)&((MenuPage *)D_800915AC)[page];
    D_80092738 = (Menu *)previous;
}

/* Whether page 3 is shown. */
s32 func_800809BC(void) {
    return ((MenuPage *)D_80092734) == &((MenuPage *)D_800915AC)[3];
}

/* Enter the settings/system menu at page 3 with every state reset. */
void func_800809D8(void) {
    func_80039FF8();
    D_80092734 = (Menu *)NULL;
    func_80080964(3);
    ((MenuPage *)D_800915AC)[3].cursor = 0;
    ((MenuPage *)D_800915AC)[4].cursor = 0;
    D_800928C8 = 0;
    D_80092758 = 0;
    func_8007F834();
    D_80092924 = 0;
    func_80080AA0(0);
    D_80092940 = 0;
    D_800928D8 = func_800891C0(6);
}

void func_80080A58(void) {
    if (D_80092940 == 0) {
        func_80028A60(0);
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
}

/* Free the loaded image data (or just forget it). */
void func_80080AA0(s32 forget) {
    if (forget) {
        D_80092760 = NULL;
    }
    if (D_80092760 != NULL) {
        func_800320E8(D_80092760);
        D_80092760 = NULL;
    }
}

/* Unpack the loaded image data and upload it to VRAM (320,256)-(640,474). */
void func_80080AE8(void) {
    s16 rect[4];

    if (D_80092760 != NULL) {
        DrawSync(0);
        rect[0] = 0x140;
        rect[1] = 0x100;
        rect[2] = 0x140;
        rect[3] = 0xDA;
        func_8007313C(D_80092760, (u8 *)D_80092760 + 0x21E80);
        LoadImage(rect, D_80092760);
    }
}

/* Keep a copy of the shown screen: allocate the image buffer once, copy
 * the displayed buffer's area to (320,256) and read it back. */
void func_80080B58(void) {
    Rect area;

    if (D_80092760 == NULL) {
        func_80031BB4(1);
        D_80092760 = func_80031BDC(0x22100, 0);
        func_80031BB4(0);
    }
    DrawSync(0);
    area = D_8009A0D8[(D_800928A0 + 1) & 1].draw.clip;
    MoveImage(&area, 0x140, 0x100);
    if (D_80092760 != NULL) {
        StoreImage(&area, D_80092760);
    }
    DrawSync(0);
}

/* Open the system menu: mode 1 at page 0, mode 2 at page 7, else close. */
void func_80080C48(s32 mode) {
    func_80039FF8();
    func_8008EB4C(0x1F);
    if (mode == 1) {
        D_80092734 = (Menu *)NULL;
        func_80080964(0);
        ((MenuPage *)D_800915AC)[0].cursor = 0;
        ((MenuPage *)D_800915AC)[2].cursor = 1;
    } else if (mode == 2) {
        D_80092734 = (Menu *)NULL;
        func_80080964(7);
        ((MenuPage *)D_800915AC)[7].cursor = 1;
    } else {
        goto close;
    }
    func_80083C0C(0);
    D_80092758 = 1;
    D_800926FC = 0;
    D_8009275C = 1;
    func_80080B58();
    return;
close:
    func_8007F8B4();
}

void func_80080D10(void) {
    D_800926DC = 0;
}

/* Link this frame's text quads and the menu overlay: the shown page's box
 * with its texture page and, while a page or the copy request is active, a
 * move of the kept screen copy into the draw buffer. */
void func_80080D20(void *ot) {
    PolyFT4 *quad = D_800926D4[D_800928A0];
    Rect area;
    s32 i;

    for (i = 0; i < D_800926DC; i++, quad++) {
        AddPrim(ot, quad);
    }
    D_800926DC = 0;
    func_800811AC(ot);
    if ((((MenuPage *)D_80092734) != NULL && D_80092758 != 0) || D_800912F0 != 0) {
        if (((MenuPage *)D_80092734) != NULL) {
            AddPrim(ot, &((MenuPage *)D_80092734)->frame[D_800928A0]);
            SetDrawTPage(&D_800954C8[D_800928A0], 0, 0, GetTPage(0, 2, 0, 0));
            AddPrim(ot, &D_800954C8[D_800928A0]);
        }
        area.x = 0x140;
        area.y = 0x100;
        area.w = 0x140;
        area.h = 0xDA;
        SetDrawMove(&D_80095498[D_800928A0], &area, D_8009A0D8[D_800928A0].draw.clip.x,
                      D_8009A0D8[D_800928A0].draw.clip.y);
        AddPrim(ot, &D_80095498[D_800928A0]);
    }
    D_800912F0 = 0;
}

/* Set up the two semi-transparent sprite strips (at y 180 and 195) sharing
 * one pixel buffer, and their texture page. */
void func_80080F04(void) {
    u8 *pixels = func_80031BDC(0x6B4, 0);

    D_80095510[0].pixels = D_80095510[1].pixels = pixels;
    D_80095510[0].sprite[0].xy0 = 0xB40000;
    D_80095510[0].sprite[0].uv0 = 0x3000;
    SetSprt(&D_80095510[0].sprite[0]);
    SetShadeTex(&D_80095510[0].sprite[0], 1);
    D_80095510[0].sprite[0].h = 0xD;
    D_80095510[0].sprite[0].clut = D_800595D4;
    D_80095510[0].sprite[1] = D_80095510[0].sprite[0];
    D_80095510[1].sprite[0].xy0 = 0xC30000;
    D_80095510[1].sprite[0].uv0 = 0x3000;
    SetSprt(&D_80095510[1].sprite[0]);
    SetShadeTex(&D_80095510[1].sprite[0], 1);
    D_80095510[1].sprite[0].h = 0xD;
    D_80095510[1].sprite[0].clut = D_80059414;
    D_80095510[1].sprite[1] = D_80095510[1].sprite[0];
    SetDrawTPage(&D_80095570[0], 0, 0, GetTPage(0, 0, 0x140, 0x30));
    D_80095570[1] = D_80095570[0];
}

/* Render a caption's text into its image and centre it on the screen. */
void func_80081094(Caption *caption, s32 text, s32 arg) {
    s32 width;

    width = func_80034EAC(func_80033728(D_80092880, text), caption->image, 0x3F, arg);
    caption->width = width;
    caption->x = (0x140 - width) / 2;
}

/* Show a text in the upper (0) or lower (1) caption; re-render only when
 * the text changes. */
void func_80081100(s32 text, s32 lower) {
    Rect rect;

    if (lower == 0) {
        if (text == D_8009273C) {
            return;
        }
        D_8009273C = text;
        func_80081094((Caption *)&D_80095510, text, 0);
    } else {
        if (text == D_80092740) {
            return;
        }
        D_80092740 = text;
        func_80081094(&D_80095540, text, 1);
    }
    rect.x = 0x140;
    rect.y = 0x30;
    rect.w = 0x42;
    rect.h = 0xD;
    LoadImage(&rect, (void *)((Caption *)(Caption *)&D_80095510)->image);
}

/* Link the shown captions into the ordering table. */
void func_800811AC(void *ot) {
    Caption *caption;

    if (D_8009273C != 0) {
        caption = (Caption *)&D_80095510;
        caption->sprite[D_800928A0].w = caption->width;
        caption->sprite[D_800928A0].x0 = caption->x;
        AddPrim(ot, &caption->sprite[D_800928A0]);
    }
    if (D_80092740 != 0) {
        caption = &D_80095540;
        caption->sprite[D_800928A0].w = caption->width;
        caption->sprite[D_800928A0].x0 = caption->x;
        AddPrim(ot, &caption->sprite[D_800928A0]);
    }
    if (D_8009273C | D_80092740) {
        AddPrim(ot, ((u8 (*)[8])D_80095570)[D_800928A0]);
    }
}

/* Measure a menu's lines and size its panel around the widest one. */
void func_800812BC(Menu *menu) {
    MenuItem *item;
    TileRgb *panel;
    s32 i;
    s32 widest;

    widest = 0;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        item->half_width = func_8007EB6C(item->text) / 2;
        widest = (widest < item->half_width) ? item->half_width : widest;
    }
    panel = &menu->panel[0];
    ((PacketTag *)panel)->len = 3;
    panel->w = widest * 2 + 0x14;
    panel->x0 = 0x96 - widest;
    menu->cursor = 0;
    *(u32 *)&panel->r0 = 0x60102020;
    menu->y = 0x6D - menu->count * 10;
    panel->code |= 2;
    panel->h = menu->count * 20 + 0x14;
    panel->y0 = menu->y - 10;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        if (item->flags & 2) {
            item->half_width = widest;
        }
    }
    if (menu->title_width != 0) {
        panel->w += menu->title_width;
        panel->x0 -= menu->title_width >> 1;
        menu->x = 0xA0 - (menu->title_width >> 1) - widest;
    }
    menu->panel[1] = menu->panel[0];
    func_8007EE08(0);
}

/* Lay out all eight menus and reset the menu display. */
void func_800814AC(void) {
    u32 i;

    for (i = 0; i < 8; i++) {
        func_800812BC(&D_800915AC[i]);
    }
    D_80092734 = NULL;
    D_80092700 = 0;
    D_80092704 = 1;
    func_80080F04();
}

/* Open a menu: place the cursor and draw every line. */
void func_8008151C(Menu *menu) {
    s32 i;

    if (menu == NULL) {
        return;
    }
    D_80092734 = menu;
    if (menu == &D_800915AC[5]) {
        return;
    }
    if (menu->title_width != 0) {
        func_8007E894(menu->x, menu->y);
    } else {
        func_8007E894(0xA0, menu->y);
    }
    for (i = 0; i < menu->count; i++) {
        func_8007F948(menu, i);
        if (menu->title_width != 0) {
            func_8007EBE0(menu->items[i].text);
        } else {
            func_8007ED84(menu->items[i].text, menu->items[i].half_width);
        }
    }
    if (menu->draw != NULL) {
        menu->draw(menu);
    }
}

/* One frame of menu input from a pad port: caption, stick sound, confirm,
 * cancel and cursor movement (skipping disabled lines, wrapping). Declared
 * with a value it never returns, as the unfilled final delay slot shows. */
s32 func_8008162C(Menu *menu, s32 port) {
    MenuItem *item;
    void (*handler)(s32);
    s32 type;
    s32 x;
    s32 y;

    item = &menu->items[menu->cursor];
    if (port == 0 || menu == &D_800915AC[7]) {
        func_80081100(D_80092744, 0);
        D_80092744 = menu->items[menu->cursor].caption;
    }
    D_80091364 = port;
    type = 0;
    if (port == 1) {
        D_80092748 = D_80059574;
        D_8009274C = D_800594A8;
        D_80092750 = D_80059490;
        type = func_80035734(1);
        x = D_8005943C - 0x80;
        y = D_80059434 - 0x80;
    } else if (port == 0) {
        D_80092748 = D_80059570;
        D_8009274C = D_800594A4;
        D_80092750 = D_8005948C;
        type = func_80035734(0);
        x = D_80059438 - 0x80;
        y = D_80059430 - 0x80;
    }
    if (type == 3 || type == 4) {
        if (SquareRoot0(x * x + y * y) > 0x40) {
            if (D_80092764 == 0) {
                func_8008EB4C(0x24);
                D_80092764 = 1;
            }
            goto stick_done;
        }
    }
    D_80092764 = 0;
stick_done:
    handler = item->handler;
    if (handler != NULL) {
        if (item->flags & 1) {
            handler(item->arg);
        } else if (D_80092750 & 0x20) {
            func_8008EB4C(0x21);
            handler(item->arg);
        }
    }
    if (D_80092734 != NULL) {
        if ((D_80092750 & 0x40) && !(port == 1 && menu == &D_800915AC[6])) {
            if (D_80092734 == &D_800915AC[menu->parent] && menu->parent != 5 && menu->parent != 6) {
                func_8008EB4C(0x24);
            } else {
                func_80080964(menu->parent);
                func_8008EB4C(0x22);
            }
        }
        if (D_8009274C & 0x1000) {
            func_8008EB4C(0x1E);
            if (--menu->cursor < 0) {
                menu->cursor = menu->count - 1;
            }
            if (menu->items[menu->cursor].flags & 4) {
                menu->cursor--;
            }
        }
        if (D_8009274C & 0x4000) {
            func_8008EB4C(0x1E);
            menu->cursor++;
            if (menu->items[menu->cursor].flags & 4) {
                menu->cursor++;
            }
        }
        if (menu->cursor < 0) {
            menu->cursor = menu->count - 1;
        }
        if (menu->cursor >= menu->count) {
            menu->cursor = 0;
        }
        if (menu->items[menu->cursor].flags & 4) {
            menu->cursor--;
        }
    }
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu4", D_8007008C);

/* The controller menu pair (menus 5 and 6) for the current mode: run the
 * menu of the available port (both in mode 2, where an unavailable port's
 * menu returns to menu 5 instead of 4), note when neither port is
 * available, set which sides the pads drive, then draw. Mode 5 runs menu
 * 5 for port 1 without resetting D_80092754 and steps the second side's
 * pick for port 2. */
void func_80081A44(void) {
    switch (D_800928C8) {
    case 1:
        switch ((s32)D_80092710) {
        case 0:
            D_80092754 = 0;
            func_8008162C(&D_800915AC[5], 0);
            break;
        case 1:
            D_80092754 = 1;
            func_8008162C(&D_800915AC[6], 0);
            break;
        case 3:
            D_80092924 = 1;
            break;
        }
        D_80099D9D = 0;
        D_80099D9E = 1;
        break;
    case 2:
        D_80092754 = 1;
        if (D_80092710 & 1) {
            D_800915AC[5].parent = 5;
        } else {
            D_800915AC[5].parent = 4;
        }
        if (D_80092710 & 2) {
            D_800915AC[6].parent = 5;
        } else {
            D_800915AC[6].parent = 4;
        }
        func_8008162C(&D_800915AC[5], 0);
        func_8008162C(&D_800915AC[6], 1);
        if (D_80092710 == 3) {
            D_80092924 = 1;
        }
        D_80099D9D = 0;
        D_80099D9E = 0;
        break;
    case 3:
        switch ((s32)D_80092710) {
        case 0:
            D_80092754 = 0;
            func_8008162C(&D_800915AC[5], 0);
            break;
        case 1:
            D_80092754 = 1;
            func_8008162C(&D_800915AC[6], 0);
            break;
        case 3:
            D_80092924 = 1;
            break;
        }
        D_80099D9D = 1;
        D_80099D9E = 1;
        break;
    case 4:
        switch ((s32)D_80092710) {
        case 0:
            D_80092754 = 0;
            func_8008162C(&D_800915AC[5], 0);
            break;
        case 1:
            D_80092754 = 1;
            func_8008162C(&D_800915AC[6], 0);
            break;
        case 3:
            D_80092924 = 1;
            break;
        }
        D_80099D9D = 0;
        D_80099D9E = 1;
        break;
    case 5:
        switch ((s32)D_80092710) {
        case 0:
            func_8008162C(&D_800915AC[5], 0);
            break;
        case 1:
            if (D_80092704 == 3) {
                D_80092710 = 3;
            } else {
                D_80092704++;
                if (D_80092704 > D_80092888) {
                    D_80092704 -= D_80092888;
                }
                if (D_80092704 < 0) {
                    D_80092704 += D_80092888;
                }
            }
            break;
        case 3:
            D_80092924 = 1;
            break;
        }
        D_80099D9D = 0;
        D_80099D9E = 1;
        break;
    }
    func_8007FBEC();
}

/* One frame of the menu layer: pending refresh, the shown menu's input
 * (with the extra-speed button) and its drawing. */
void func_80081D2C(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    Menu *menu;

    if (D_8009275C != 0) {
        func_80080AE8();
        D_8009275C = 0;
    }
    func_80036420();
    menu = D_80092734;
    if (menu == &D_800915AC[5]) {
        func_80081A44();
        return;
    }
    if (menu != NULL) {
        if ((D_8005948C & 1) && D_800926FC < 5) {
            D_800926FC++;
            func_80080AE8();
        }
        func_8008162C(menu, D_800928FC);
    }
    func_8008151C(D_80092734);
}

/* Dim the screen below the top band with the half-grey fade tiles. */
void func_80081E00(void) {
    D_8009A1C0.y0 = 0x60;
    D_8009A2B8.y0 = 0x60;
    D_8009A1C0.r0 = 0x7F;
    D_8009A1C0.g0 = 0x7F;
    D_8009A1C0.b0 = 0x7F;
    D_8009A2B8.r0 = 0x7F;
    D_8009A2B8.g0 = 0x7F;
    D_8009A2B8.b0 = 0x7F;
    D_8009A1C0.h = D_8009286C - 0x60;
    D_8009A2B8.h = D_8009286C - 0x60;
}

/* Clear the fade tiles back to the full, black screen. */
void func_80081E6C(void) {
    D_8009A1C0.y0 = 0;
    D_8009A2B8.y0 = 0;
    D_8009A1C0.r0 = 0;
    D_8009A1C0.g0 = 0;
    D_8009A1C0.b0 = 0;
    D_8009A2B8.r0 = 0;
    D_8009A2B8.g0 = 0;
    D_8009A2B8.b0 = 0;
    D_8009A1C0.h = D_8009286C;
    D_8009A2B8.h = D_8009286C;
}

/* Build the menu backdrop packets: the sky gradient quads, the backdrop
 * texture pages, the six backdrop sprites; scale the map heights and set
 * up the map drawing pools. */
void func_80081ECC(void) {
    PolyG4 *sky;
    s16 *height;
    s32 i;

    func_800875EC();
    sky = &D_80095580[0];
    ((PacketTag *)sky)->len = 8;
    sky->code = 0x38;
    sky->r0 = 0x10;
    sky->g0 = 0x60;
    sky->b0 = 0x7F;
    *(u16 *)&sky->r1 = 0x6010;
    sky->b1 = 0x7F;
    *(u16 *)&sky->r2 = 0x7F7F;
    sky->b2 = 0x7F;
    *(u16 *)&sky->r3 = 0x7F7F;
    sky->b3 = 0x7F;
    *(u32 *)&sky->x0 = 0;
    *(u32 *)&sky->x1 = 0x140;
    *(u32 *)&sky->x2 = 0x600000;
    *(u32 *)&sky->x3 = 0x600140;
    D_80095580[1] = D_80095580[0];
    SetDrawTPage(&D_800955C8[0], 0, 0, GetTPage(2, 2, 0, 0x100));
    SetDrawTPage(&D_800955C8[1], 0, 0, GetTPage(2, 2, 0, 0));
    SetDrawTPage(&D_800955C8[2], 0, 0, GetTPage(2, 2, 0x100, 0x100));
    SetDrawTPage(&D_800955C8[3], 0, 0, GetTPage(2, 2, 0x100, 0));
    ((PacketTag *)&D_800955F8[0])->len = 4;
    *(u32 *)&D_800955F8[0].r0 = 0x64707070;
    D_800955F8[0].code &= ~1; /* texture not shaded */
    D_800955F8[0].code |= 2;  /* semi-transparent */
    *(u32 *)&D_800955F8[0].x0 = 0;
    *(u16 *)&D_800955F8[0].u0 = 0;
    *(u32 *)&D_800955F8[0].w = 0xDB0080;
    func_800732AC(&D_800955F8[1], &D_800955F8[0], sizeof(Sprite) * 5);
    D_800955F8[3].u0 = 0x80;
    D_800955F8[2].u0 = 0x80;
    D_800955F8[3].x0 = 0x80;
    D_800955F8[2].x0 = 0x80;
    D_800955F8[5].x0 = 0x100;
    D_800955F8[4].x0 = 0x100;
    D_800955F8[5].w = 0x40;
    D_800955F8[4].w = 0x40;
    height = (s16 *)D_800928DC;
    for (i = 0; i < 0x4000; i++) {
        *height *= 12;
        height += 2;
    }
    func_80087830();
}

/* Draw the large direction arrow at a map position (8.8 fixed point). */
void func_80082178(s32 x, s32 z, s32 direction) {
    s32 start_x;
    s32 start_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    x >>= 8;
    z >>= 8;
    last_x = start_x = x + ((func_8003F8B0(direction + 0x280) * 10) >> 12);
    last_z = start_z = z + ((func_8003F8CC(direction + 0x280) * 10) >> 12);
    angle = direction + 0x580;
    for (i = 0; i < 6; i++) {
        next_x = x + ((func_8003F8B0(angle) * 24) >> 12);
        next_z = z + ((func_8003F8CC(angle) * 24) >> 12);
        func_80087698(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x100;
    }
    next_x = x + ((func_8003F8B0(direction - 0x280) * 10) >> 12);
    next_z = z + ((func_8003F8CC(direction - 0x280) * 10) >> 12);
    func_80087698(last_x, last_z, next_x, next_z);
    func_80087698(start_x, start_z, next_x, next_z);
}

/* Draw the small direction arrow at a map position (8.8 fixed point). */
void func_80082300(s32 x, s32 z, s32 direction) {
    s32 start_x;
    s32 start_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    x >>= 8;
    z >>= 8;
    last_x = start_x = x + ((func_8003F8B0(direction + 0x100) * 16) >> 12);
    last_z = start_z = z + ((func_8003F8CC(direction + 0x100) * 16) >> 12);
    angle = direction + 0x78A;
    for (i = 0; i < 3; i++) {
        next_x = x + ((func_8003F8B0(angle) * 32) >> 12);
        next_z = z + ((func_8003F8CC(angle) * 32) >> 12);
        func_80087698(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x75;
    }
    next_x = x + ((func_8003F8B0(direction - 0x100) * 16) >> 12);
    next_z = z + ((func_8003F8CC(direction - 0x100) * 16) >> 12);
    func_80087698(last_x, last_z, next_x, next_z);
    func_80087698(start_x, start_z, next_x, next_z);
}

/* Copy the stored map position. */
void func_80082458(SVector *out) {
    *out = D_80092768;
}

/* Raise a ground corner by its square's kind: 1 by 0x100, 3 by 0x40. */
#define GROUND_KIND_LIFT(corner, x, z)                                          \
    switch (((u32 *)D_800928DC)[(z) * 128 + (x)] & 0x3000000) {                \
    case 0x1000000:                                                            \
        (corner).vy += 0xC0;                                                   \
    case 0x3000000:                                                            \
        (corner).vy += 0x40;                                                   \
    }

/* Ground height under a position: the plane through the triangle of its
 * 256-unit square that contains it (corners optionally raised by their
 * square's kind); the plane's normal is kept in D_80092768.
 * Once the triangle is copied, its wide plane point reuses the last
 * corner's scratch slot and the following eight bytes. */
s32 func_80082488(Vector *pos, s32 lift) {
    struct {
        s32 unused0[2];
        union {
            SVector corner[5]; /* four corners and room for the later Vector */
            struct {
                SVector unused[3];
                Vector point;
            } plane;
        } geometry;
        SVector tri[3];
        s32 unused1[2];
    } scratch;
    GroundSquare *square;
    s32 x;
    s32 z;
    s32 x0;
    s32 z0;

    x = pos->vx;
    z = pos->vz;
    x0 = x & ~0xFF;
    x >>= 8;
    z0 = z & ~0xFF;
    z >>= 8;
    square = (GroundSquare *)((z * 128 + x) * sizeof(GroundSquare) +
                             (s32)D_800928DC);
    scratch.geometry.corner[0].vx = x0;
    scratch.geometry.corner[0].vy = square[0].height;
    scratch.geometry.corner[0].vz = z0;
    scratch.geometry.corner[1].vx = x0 + 0x100;
    scratch.geometry.corner[1].vy = square[129].height;
    scratch.geometry.corner[1].vz = z0 + 0x100;
    scratch.geometry.corner[2].vx = x0 + 0x100;
    scratch.geometry.corner[2].vy = square[1].height;
    scratch.geometry.corner[2].vz = z0;
    scratch.geometry.corner[3].vx = x0;
    scratch.geometry.corner[3].vy = square[128].height;
    scratch.geometry.corner[3].vz = z0 + 0x100;
    if (lift) {
        GROUND_KIND_LIFT(scratch.geometry.corner[0], x, z);
        GROUND_KIND_LIFT(scratch.geometry.corner[1], x + 1, z + 1);
        GROUND_KIND_LIFT(scratch.geometry.corner[2], x + 1, z);
        GROUND_KIND_LIFT(scratch.geometry.corner[3], x, z + 1);
    }
    if ((scratch.geometry.corner[0].vz - scratch.geometry.corner[1].vz) * pos->vx +
            (scratch.geometry.corner[1].vx - scratch.geometry.corner[0].vx) * pos->vz +
            scratch.geometry.corner[0].vx * scratch.geometry.corner[1].vz -
            scratch.geometry.corner[1].vx * scratch.geometry.corner[0].vz < 0) {
        scratch.tri[0] = scratch.geometry.corner[0];
        scratch.tri[1] = scratch.geometry.corner[1];
        scratch.tri[2] = scratch.geometry.corner[2];
    } else {
        scratch.tri[0] = scratch.geometry.corner[0];
        scratch.tri[1] = scratch.geometry.corner[3];
        scratch.tri[2] = scratch.geometry.corner[1];
    }
    func_8002DB84(&scratch.tri[0], &scratch.tri[1], &scratch.tri[2], &D_80092768);
    {
        scratch.geometry.plane.point.vx = scratch.tri[0].vx;
        scratch.geometry.plane.point.vy = scratch.tri[0].vy;
        scratch.geometry.plane.point.vz = scratch.tri[0].vz;
        return pos->vy +
               (scratch.geometry.plane.point.vx * D_80092768.vx +
                scratch.geometry.plane.point.vy * D_80092768.vy +
                scratch.geometry.plane.point.vz * D_80092768.vz -
                (pos->vx * D_80092768.vx + pos->vy * D_80092768.vy +
                 pos->vz * D_80092768.vz)) / D_80092768.vy;
    }
}

/* Ground height of the map cell under a position (cells of 256 units). */
s32 func_80082880(SVector *pos) {
    Vector unused[3]; /* the original frame has 0x30 unused bytes */
    s16 x, z;

    x = pos->vx >> 8;
    z = pos->vz >> 8;
    return *(s16 *)&((s32 *)D_800928DC)[x + z * 128];
}

/* The map cell word under a position (cells of 256 units). */
s32 func_800828C4(Vector *pos) {
    s32 x = pos->vx >> 8;
    s32 z = pos->vz >> 8;

    return ((s32 *)D_800928DC)[z * 128 + x];
}

/* Keep a moving position inside the circular arena of the given radius
 * around the scene centre: when the step would leave it, turn the step
 * along the rim and shorten it until the end point is inside. */
void func_800828F8(Vector *pos, Vector *step, s32 radius) {
    Vector local;
    Vector next;
    Vector square;
    Matrix rim;
    Matrix back;
    SVector dir;
    s32 distance;

    local.vx = pos->vx + step->vx - 0x3F80;
    local.vz = pos->vz + step->vz - 0x3F80;
    func_8004A414(&local, &square);
    if (radius < SquareRoot0(square.vx + square.vz)) {
        VectorNormalS(&local, &dir);
        rim.m[2][1] = 0;
        rim.m[1][2] = 0;
        rim.m[1][0] = 0;
        rim.m[0][1] = 0;
        rim.m[1][1] = 0x1000;
        rim.m[2][2] = dir.vz;
        rim.m[0][0] = dir.vz;
        rim.m[0][2] = -dir.vx;
        rim.m[2][0] = dir.vx;
        ApplyMatrixLV(&rim, step, &local);
        func_8004A8EC(&rim, &back);
        SetRotMatrix(&back);
        local.vz = 0;
        for (;;) {
            func_8004998C(&local, step);
            next.vx = pos->vx + step->vx - 0x3F80;
            next.vz = pos->vz + step->vz - 0x3F80;
            func_8004A414(&next, &square);
            distance = SquareRoot0(square.vx + square.vz);
            if (radius >= distance) {
                break;
            }
            local.vz -= distance - radius - 8;
        }
    }
}

/* Apply the current stage's colours: sky gradient (top and bottom), back
 * and far (fog) colours, fade tiles and the GTE primitive colour. */
void func_80082A70(void) {
    Environment *env;
    s32 top_r;
    s32 top_g;
    s32 top_b;
    s32 bottom_r;
    s32 bottom_g;
    s32 bottom_b;

    env = &D_8009178C[D_800928B4];
    D_8009288C = env;
    top_r = env->top[0];
    top_g = env->top[1];
    top_b = env->top[2];
    D_8009291C = env->unk4;
    D_80092910 = env->unk5;
    D_80092908 = env->unk6;
    bottom_r = env->bottom[0];
    bottom_g = env->bottom[1];
    bottom_b = env->bottom[2];
    func_8002C6E0(env->back[0], env->back[1], env->back[2]);
    func_8004A10C(bottom_r, bottom_g, bottom_b);
    D_80095580[0].r0 = top_r;
    D_80095580[1].r0 = top_r;
    D_80095580[0].g0 = top_g;
    D_80095580[1].g0 = top_g;
    D_80095580[0].b0 = top_b;
    D_80095580[1].b0 = top_b;
    *(u16 *)&D_80095580[0].r1 = top_r | (top_g << 8);
    D_80095580[0].b1 = top_b;
    *(u16 *)&D_80095580[1].r1 = top_r | (top_g << 8);
    D_80095580[1].b1 = top_b;
    *(u16 *)&D_80095580[0].r2 = bottom_r | (bottom_g << 8);
    D_80095580[0].b2 = bottom_b;
    *(u16 *)&D_80095580[1].r2 = bottom_r | (bottom_g << 8);
    D_80095580[1].b2 = bottom_b;
    *(u16 *)&D_80095580[0].r3 = bottom_r | (bottom_g << 8);
    D_80095580[0].b3 = bottom_b;
    *(u16 *)&D_80095580[1].r3 = bottom_r | (bottom_g << 8);
    D_80095580[1].b3 = bottom_b;
    D_8009A1C0.r0 = bottom_r;
    D_8009A1C0.g0 = bottom_g;
    D_8009A1C0.b0 = bottom_b;
    D_8009A2B8.r0 = bottom_r;
    D_8009A2B8.g0 = bottom_g;
    D_8009A2B8.b0 = bottom_b;
    SetFogNearFar(0x800, 0x1800, 0xC0);
    D_80059598 = (D_80059598 & 0xFFFFFF) | 0x28000000;
    gte_ldrgb(&D_80059598);
}

/* Load the stage's floor texture (a TIM, palette made semi-transparent)
 * and build the two pools of 64 textured floor quads, alternating the two
 * halves of the texture. */
void func_80082C4C(StageFiles *files) {
    TimImage tim;
    PolyFT4 *quad;
    s16 *clut;
    s32 i;

    OpenTIM(files->floor_tim);
    ReadTIM(&tim);
    clut = (s16 *)tim.caddr;
    for (i = 0; i < 0x100; i++) {
        *clut++ |= 0x8000;
    }
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    D_800927A0 = GetClut(tim.crect->x, tim.crect->y);
    D_800927A4 = GetTPage(1, 0, tim.prect->x, tim.prect->y);
    D_800927A8 = (u8)tim.prect->y;
    D_80092788[0] = func_80031BDC(0xA00, 0);
    D_80092788[1] = func_80031BDC(0xA00, 0);
    quad = D_80092788[0];
    for (i = 0; i < 0x40; i += 2) {
        ((PacketTag *)&quad[0])->len = 9;
        quad[0].code = 0x2C;
        ((PacketTag *)&quad[1])->len = 9;
        quad[1].code = 0x2C;
        quad->clut = D_800927A0;
        quad->tpage = D_800927A4;
        quad->u0 = 0x7F;
        quad->v0 = D_800927A8 + 0x3F;
        quad->u1 = 0x7F;
        quad->v1 = D_800927A8;
        quad->u2 = 0x3F;
        quad->v2 = D_800927A8 + 0x3F;
        quad->u3 = 0x3F;
        quad->v3 = D_800927A8;
        quad++;
        quad->clut = D_800927A0;
        quad->tpage = D_800927A4;
        quad->u0 = 0x3F;
        quad->v0 = D_800927A8 + 0x3F;
        quad->u1 = 0x3F;
        quad->v1 = D_800927A8;
        quad->u2 = 0;
        quad->v2 = D_800927A8 + 0x3F;
        quad->u3 = 0;
        quad->v3 = D_800927A8;
        quad++;
    }
    func_800732AC(D_80092788[1], D_80092788[0], 0xA00);
}

/* Draw the arena wall: a ring of 32 two-storey textured segments around
 * the scene centre, starting behind the given position, depth-cued and
 * skipped when too far away. The wall's corners are taken relative to the
 * camera as 16-bit offsets. */
void func_80082E60(u32 *ot, Vector *pos) {
    Vector centre;
    SVector base0;
    SVector base1;
    SVector mid0;
    SVector mid1;
    SVector top0;
    SVector top1;
    s32 z[4];
    PolyFT4 *quad;
    PolyFT4 *next;
    s32 angle;
    s32 depth;
    s32 i;

    centre = *pos;
    i = 0;
    quad = D_80092788[D_800928A0];
    centre.vx -= 0x3F80;
    centre.vz -= 0x3F80;
    angle = ratan2(centre.vx, centre.vz) & 0xFFF0;
    angle -= 0x100;
    mid0.vy = mid1.vy = -0x290;
    base0.vy = base1.vy = 0;
    top0.vy = top1.vy = -0x520;
    base0.vx = ((func_8003F8B0(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vx - 0x3F80);
    base0.vz = ((func_8003F8CC(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vz - 0x3F80);
    angle += 0x10;
    for (; i < 32; i++) {
        top0.vx = mid0.vx = base0.vx;
        top0.vz = mid0.vz = base0.vz;
        top1.vx = mid1.vx = base1.vx = ((func_8003F8B0(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vx - 0x3F80);
        top1.vz = mid1.vz = base1.vz = ((func_8003F8CC(angle) * 0x3F80) >> 12) - (s16)(D_80096FA8.vz - 0x3F80);
        gte_ldv3(&base0, &base1, &mid0);
        gte_rtpt();
        gte_dpcs();
        gte_stsxy3(&quad[0].x0, &quad[0].x1, &quad[0].x2);
        gte_stsz3v(&z[0], &z[1], &z[2]);
        gte_ldv3(&mid1, &top0, &top1);
        gte_rtpt();
        next = &quad[1];
        depth = z[0];
        if (depth < z[1]) {
            depth = z[1];
        }
        if (depth <= z[2]) {
            depth = z[2];
        }
        *(u32 *)&next->x0 = *(u32 *)&quad[0].x2;
        gte_stsxy(&quad[0].x3);
        gte_stsxy3(&quad[0].x3, &next->x2, &next->x3);
        gte_stsz(&z[3]);
        *(u32 *)&next->x1 = *(u32 *)&quad[0].x3;
        if (depth <= z[3]) {
            depth = z[3];
        }
        if (depth < 0x1C00) {
            depth >>= 4;
            gte_strgb(&quad[0].r0);
            gte_strgb(&next->r0);
            ((PacketTag *)&quad[0])->len = 9;
            quad[0].code = 0x2C;
            ((PacketTag *)next)->len = 9;
            next->code = 0x2C;
            AddPrim(&ot[depth], &quad[0]);
            AddPrim(&ot[depth], next);
        }
        quad += 2;
        angle += 0x10;
        base0.vx = base1.vx;
        base0.vz = base1.vz;
    }
}

/* Put the look-at point somewhere random around the scene centre and set
 * the idle camera motion parameters. */
void func_800831C8(void) {
    s32 radius;
    s32 angle;

    radius = (rand() & 0x1FFF) + 0x800;
    angle = rand() % 0x600 + 0x500;
    D_8009871C.vx = ((func_8003F8B0(angle) * radius) >> 12) + 0x4000;
    D_8009871C.vz = ((func_8003F8CC(angle) * radius) >> 12) + 0x4000;
    D_8009871C.vy = -((rand() & 0x7FF) + 0x400);
    D_80092770 = 0x100;
    D_80092774 = 0x40;
    D_8009287C = 0x40;
    D_8009290C = 0x400;
}

/* Turn the idle camera with the left/right buttons. */
void func_800832C0(s32 buttons) {
    if (buttons & 0x8000) {
        D_800927AC += 0x20;
    }
    if (buttons & 0x2000) {
        D_800927AC -= 0x20;
    }
}

/* Idle orbit camera: move the eye toward a point between the two actors
 * (further toward the other actor late in the orbit, a third of the way
 * when smoothing) and swing the look-at point around it, kept inside the
 * arena and above the ground. */
void func_80083310(s32 smooth) {
    Vector look;
    Vector step;
    Vector offset;
    Vector unused;   /* the original frame has 0x18 unused bytes */
    SVector unused2;
    Actor *subject;
    Actor *other;
    s32 value; /* the other actor's share, then the orbit angle, then the ground */

    if (D_80092890 != 0) {
        subject = &D_80097010;
        other = &D_8009872C;
    } else {
        subject = &D_8009872C;
        other = &D_80097010;
    }
    ratan2(subject->pos.vx - other->pos.vx, subject->pos.vz - other->pos.vz);
    if (D_800928AC > 0xB0) {
        value = 0x100;
    } else if (D_800928AC > 0xA0) {
        value = (D_800928AC - 0xA0) << 4;
    } else {
        value = 0;
    }
    offset.vx = other->pos.vx;
    offset.vy = other->pos.vy;
    offset.vz = other->pos.vz;
    offset.vx -= subject->pos.vx;
    offset.vy -= subject->pos.vy;
    offset.vz -= subject->pos.vz;
    offset.vx *= value;
    offset.vy *= value;
    offset.vz *= value;
    offset.vx /= 256;
    offset.vy /= 256;
    offset.vz /= 256;
    offset.vx += subject->pos.vx;
    offset.vy += subject->pos.vy;
    offset.vz += subject->pos.vz;
    offset.vy -= 0xA0;
    offset.vx -= D_8009867C.vx;
    offset.vy -= D_8009867C.vy;
    offset.vz -= D_8009867C.vz;
    if (smooth) {
        offset.vx /= 3;
        offset.vy /= 3;
        offset.vz /= 3;
    }
    D_80092770 = 0xC00;
    D_8009867C.vx += offset.vx;
    D_8009867C.vy += offset.vy;
    D_8009867C.vz += offset.vz;
    value = D_800927AC + D_800928AC * D_800927B0;
    look.vx = (func_8003F8B0(value) * D_80092770) >> 12;
    look.vz = (func_8003F8CC(value) * D_80092770) >> 12;
    look.vy = -(D_800928AC * 6 + 0x200);
    look.vx += D_8009867C.vx;
    look.vy += D_8009867C.vy;
    look.vz += D_8009867C.vz;
    step.vx = look.vx - D_8009871C.vx;
    step.vz = look.vz - D_8009871C.vz;
    func_800828F8(&D_8009871C, &step, 0x3D00);
    D_8009871C.vx += step.vx;
    D_8009871C.vz += step.vz;
    value = func_80082488(&D_8009871C, 0);
    if (value < look.vy) {
        look.vy = value;
    }
    D_8009871C.vy = look.vy;
}

/* Start an idle camera orbit at a random angle, speed and direction. */
void func_8008369C(void) {
    D_800927AC = rand();
    D_800927B0 = rand() % 12 + 4;
    if (rand() & 1) {
        D_800927B0 = -D_800927B0;
    }
    func_80083310(0);
}

/* Frame two actors: put the eye between them, pick the side of the pair
 * the look-at point is nearer to, and move the look-at point toward a spot
 * beside the pair (further back when they are far apart), kept inside the
 * arena and above the ground. */
void func_80083738(Actor *first, Actor *second) {
    Vector side;
    Vector other_side;
    Vector unused[2]; /* the original frame has 0x20 unused bytes */
    s32 heading;
    s32 distance;
    s32 angle;
    s32 value; /* the second angle, then a side's distance, then the ground */

    heading = ratan2(first->pos.vx - second->pos.vx, first->pos.vz - second->pos.vz);
    distance = func_800887A4(&first->pos, &second->pos);
    angle = heading - 0x400;
    D_80092770 = distance * 2 / 3 + 0xC0;
    D_8009867C.vx = (first->pos.vx + second->pos.vx) / 2;
    D_8009867C.vy = (first->pos.vy + second->pos.vy) / 2 - 0xA0;
    D_8009867C.vz = (first->pos.vz + second->pos.vz) / 2;
    side.vx = D_8009867C.vx + ((func_8003F8B0(angle) * D_80092770) >> 12);
    side.vz = D_8009867C.vz + ((func_8003F8CC(angle) * D_80092770) >> 12);
    value = heading + 0x400;
    other_side.vx = D_8009867C.vx + ((func_8003F8B0(value) * D_80092770) >> 12);
    other_side.vz = D_8009867C.vz + ((func_8003F8CC(value) * D_80092770) >> 12);
    side.vx -= D_8009871C.vx;
    side.vy -= D_8009871C.vy;
    side.vz -= D_8009871C.vz;
    other_side.vx -= D_8009871C.vx;
    other_side.vy -= D_8009871C.vy;
    other_side.vz -= D_8009871C.vz;
    value = func_80088754(&side);
    if (func_80088754(&other_side) < value) {
        D_8009290C = 0x400;
        D_800928F4 = 0;
    } else {
        D_8009290C = -0x400;
        D_800928F4 = 1;
    }
    distance /= 4;
    if (distance > 0x300) {
        distance = 0x300;
    }
    side.vy = D_8009867C.vy - D_80092774 - distance;
    side.vx = D_8009867C.vx + ((func_8003F8B0(heading + D_8009290C) * D_80092770) >> 12);
    side.vz = D_8009867C.vz + ((func_8003F8CC(heading + D_8009290C) * D_80092770) >> 12);
    value = func_80082488(&side, 0) - 0x100;
    if (value < side.vy) {
        side.vy = value;
    }
    side.vx = (side.vx - D_8009871C.vx) / D_8009287C;
    side.vy = (side.vy - D_8009871C.vy) / D_8009287C;
    side.vz = (side.vz - D_8009871C.vz) / D_8009287C;
    D_8009277C = heading;
    func_800828F8(&D_8009871C, &side, 0x3D00);
    D_8009287C = 100;
    D_8009871C.vx += side.vx;
    D_8009871C.vy += side.vy;
    D_8009871C.vz += side.vz;
}

/* Read the camera's look-at point and eye. */
void func_80083B54(Vector *look, Vector *eye) {
    *look = D_8009871C;
    *eye = D_8009867C;
}

/* Clear the display area (one or both 320-wide buffers) and wait. */
void func_80083BB4(s32 both) {
    Rect rect;

    rect.x = 0;
    rect.y = 0;
    if (both) {
        rect.w = 0x280;
    } else {
        rect.w = 0x140;
    }
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Enter a camera/scene mode, running its setup. */
void func_80083C0C(s32 mode) {
    D_80092794 = mode;
    switch (mode) {
    case 3:
        func_80081E6C();
        break;
    case 4:
        func_8007A21C(D_8009294C);
        break;
    case 8:
        func_8007AC3C();
        break;
    case 6:
        if (D_8009872C.unkF2 < D_80097010.unkF2) {
            func_800725B0(&D_80097010);
        } else {
            func_800725B0(&D_8009872C);
        }
        break;
    }
}

/* The scene state word. */
s32 func_80083CD8(void) {
    return D_80092790;
}
