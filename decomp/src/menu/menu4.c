/* menu4: text 8007E528-80081ECC, rodata 8006FE1C-800701B0, data
 * 80091230-8009178C, variables 800926D4-80092768 and 80095498-80095580.
 * The menu's text (the banner, font, cursor and colours), the selection
 * screen (the entry list, portraits and the two sides' wheels), the
 * settings, vibration and options pages, the choice menus and captions,
 * and the screen fades. Its jump tables lie at 4 mod 8 (8006FE1C-
 * 8007019C); its first function reads its variables and 80081D2C is the
 * last that does, so its end lies at 80081E00, 80081E6C or 80081ECC (the
 * two fades between touch only commons); the latest is kept. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/text.h"
#include "actor.h"
#include "bout.h"
#include "camera.h"
#include "display.h"
#include "effects.h"
#include "helpers.h"
#include "menus.h"
#include "mode.h"
#include "packets.h"
#include "resident_views.h"
#include "script.h"
#include "select.h"
#include "sound.h"
#include "text.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static POLY_FT4 *D_800926D4[2]; /* text quads, per draw buffer */
static s32 D_800926DC;
static u16 D_800926E0; /* text texture page */
static u16 D_800926E4; /* text CLUT */
static s16 D_800926E8;
static s16 D_800926EC;
static u8 D_800926F0; /* text colour r, g, b */
static u8 D_800926F4;
static u8 D_800926F8;
static u8 D_800926FC;
static s32 D_80092700; /* first side's pick */
static s32 D_80092704; /* second side's pick */
static u8 D_80092708;
static GridCell *D_8009270C;
static u32 D_80092710; /* bit 0/1: controller port 1/2 unavailable */
static s32 D_80092714;
static s32 D_80092718;
static s32 D_8009271C;
static s32 D_80092720;
static s32 D_80092724;
static s32 D_80092728;
static s32 D_8009272C; /* port 1 vibration entry selected */
static s32 D_80092730; /* port 2 vibration entry selected */
static Menu *D_80092734; /* menu being shown */
static Menu *D_80092738; /* menu to return to */
static u8 D_8009273C;
static u8 D_80092740;
static s32 D_80092744; /* caption of the selected line */
static s32 D_80092748; /* bit 0: both sides may pick the same entry; pad buttons held */
static u32 D_8009274C; /* pad buttons repeating this frame */
static u32 D_80092750; /* pad buttons pressed this frame */
static s32 D_80092754;
static u8 D_80092758;
static u8 D_8009275C;
static void *D_80092760; /* loaded image data */
static u8 D_80092764; /* stick is deflected */

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk). */
static DR_MOVE D_80095498[2];
static DR_TPAGE D_800954C8[2];
static SceneSprite D_800954D8[2];
/* The upper and lower captions, which share one pixel buffer
 * (func_80080F04). */
static Caption D_80095510[2];
static DR_TPAGE D_80095570[2];

/* Menu font glyphs (func_8007E8AC maps characters to these). */
Glyph D_80091230[] = {
    { 0x3C, 0x13, 0x15, 0x12 }, { 0x18, 0, 5, 0x12 }, { 0x20, 0, 0xC, 0x12 },
    { 0x30, 0, 0xC, 0x12 }, { 0x40, 0, 0xB, 0x12 }, { 0x4C, 0, 0xB, 0x12 },
    { 0x58, 0, 0xF, 0x12 }, { 0x68, 0, 0xA, 0x12 }, { 0x74, 0, 0xF, 0x12 },
    { 0x84, 0, 0xF, 0x12 }, { 0x94, 0, 9, 0x12 }, { 0xA0, 0, 0xA, 0x12 },
    { 0xAC, 0, 0xB, 0x12 }, { 0xB8, 0, 0xE, 0x12 }, { 0xC8, 0, 9, 0x12 },
    { 0xD4, 0, 9, 0x12 }, { 0xE0, 0, 0xB, 0x12 }, { 0xEC, 0, 9, 0x12 },
    { 0, 0x13, 3, 0x12 }, { 4, 0x13, 6, 0x12 }, { 0xC, 0x13, 9, 0x12 },
    { 0x18, 0x13, 6, 0x12 }, { 0x20, 0x13, 0xE, 0x12 }, { 0x30, 0x13, 9, 0x12 },
    { 0x3C, 0x13, 0x15, 0x12 }, { 0x54, 0x13, 0xB, 0x12 }, { 0x60, 0x13, 0x15, 0x13 },
    { 0x78, 0x13, 0xA, 0x12 }, { 0x84, 0x13, 8, 0x12 }, { 0x90, 0x13, 9, 0x12 },
    { 0x9C, 0x13, 8, 0x12 }, { 0xA8, 0x13, 0xB, 0x12 }, { 0xB4, 0x13, 0x10, 0x12 },
    { 0xC8, 0x13, 0xA, 0x12 }, { 0xD4, 0x13, 0xB, 0x12 }, { 0xE0, 0x13, 9, 0x12 },
    { 0xF8, 0, 4, 0x12 }, { 0xEC, 0x13, 3, 0x12 }, { 0xF0, 0x13, 7, 0x12 },
    { 0xF8, 0x13, 7, 0x12 }, { 0, 0, 0x16, 0x12 }, { 0, 0, 8, 0 },
    { 0x60, 0x13, 3, 7 },
};

s32 D_800912DC = 0x100; /* text width scale (0x100 = 1) */

/* Neighbour offsets and slide of the selection wheel portraits, per row. */
s16 D_800912E0[2][4] = { { 1, -1, 2, 0x24 }, { -1, -2, 1, -0x24 } };

s32 D_800912F0 = 0;

/* Handlers of the menu lines (D_800915AC, defined below): they take no
 * argument or the menu index. */
void func_800802A4();
void func_80080108();
void func_8008040C();
void func_80080144();
void func_80080780();
void func_80080920();
void func_800801F8();
void func_80080054();
void func_80080234();
void func_8007F854();
void func_80080090();
void func_800800CC();
void func_80080180();
void func_800801BC();
void func_8007F8E4();
void func_80080268();
void func_8007F9A0();
void func_8007FE48();
void func_8007FB0C();

/* Set the scene state, playing sound 0x24 when state 10 starts from 0. */
void func_8007E528(s32 state) {
    if (D_80092708 == 0 && state == 10) {
        func_8008EB4C(0x24);
    }
    D_80092708 = state;
}

/* While the scene state counts down, draw its sprite (when flag 2 is set). */
void func_8007E574(void *ot) {
    if (D_80092708 != 0) {
        if (D_800928E8 & 2) {
            AddPrim(ot, &D_800954D8[D_800928A0].sprite);
            AddPrim(ot, &D_800954D8[D_800928A0].tpage);
        }
        D_80092708--;
    }
}

/* The text quads used this frame. */
s32 func_8007E624(void) {
    return D_800926DC;
}

/* Allocate the text quads, load the font (with its palette's colours 0, 2
 * and 3 replaced) and the banner image, and build the banner sprite. */
void func_8007E634(MenuImageFile *files) {
    TIM_IMAGE image;
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
    palette = (s16 *)image.caddr;
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
    palette = (s16 *)image.caddr;
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

/* Move the text cursor. */
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

/* Set the text width scale (0x100 = 1). */
void func_8007E954(s32 value) {
    D_800912DC = value;
}

/* Draw one character at the text cursor (at most 101 quads a frame; '('
 * only advances) and move the cursor right by its scaled width. Declared
 * int without a return value, as the original's unfilled delay slot shows.
 * The vertices, UVs and colour word are written through casts of the packet
 * fields (not struct member stores), so no global load moves above them;
 * the texture page/CLUT and the length are member stores. */
s32 func_8007E964(s32 ch) {
    POLY_FT4 *quad;
    Glyph *glyph;
    s32 right;

    if (D_800926DC < 101) {
        quad = D_800926D4[D_800928A0];
        quad += D_800926DC;
        glyph = func_8007E8AC(ch);
        if (glyph != NULL) {
            if (ch != '(') {
                ch = glyph->width | 3; /* the quad width, in the same variable */
                *(u32 *)&quad->x0 = D_800926E8 | (D_800926EC << 16);
                right = D_800926E8 + ((ch * D_800912DC) >> 8);
                *(u32 *)&quad->x1 = right | (D_800926EC << 16);
                *(u32 *)&quad->x2 = D_800926E8 | ((D_800926EC + glyph->height) << 16);
                *(u32 *)&quad->x3 = right | ((D_800926EC + glyph->height) << 16);
                *(u16 *)&quad->u0 = glyph->u | (glyph->v << 8);
                *(u16 *)&quad->u1 = (glyph->u + ch) | (glyph->v << 8);
                *(u16 *)&quad->u2 = glyph->u | ((glyph->v + (glyph->height + 1)) << 8);
                *(u16 *)&quad->u3 = (glyph->u + ch) | ((glyph->v + (glyph->height + 1)) << 8);
                quad->tpage = D_800926E0;
                quad->clut = D_800926E4;
                setlen(quad, 9);
                *(u32 *)&quad->r0 = D_800926F0 | (D_800926F4 << 8) | (D_800926F8 << 16) | 0x2C000000;
                D_800926DC++;
            }
            D_800926E8 += ((glyph->width * D_800912DC) >> 8) + 2;
        }
    }
}

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

/* Build the list of the 49 entries (or, when filtering, of those whose
 * required level the current level reaches) and order it when filtering. */
void func_8007EEE8(s32 filter) {
    s32 level = D_8006D634.vars[0];
    ListEntry **list = func_80031BDC(0xC4, 1);
    MoveList *source;
    s32 i;

    D_800928EC = list;
    source = D_80092874;
    D_80092888 = 0;
    for (i = 0; i < 49; source++, i++) {
        if (!filter || source->level <= level) {
            D_800928EC[D_80092888++] = &D_80091964[i];
        }
    }
    if (filter) {
        func_8008895C();
    }
}

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

            cell->clut.x = 0x200;
            cell->clut.y = id--;
            cell->clut.w = 0x80;
            cell->clut.h = 1;
            cell->image.x = top;
            cell->image.y = left;
            cell->image.w = 0x1E;
            cell->image.h = 0x40;
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
    u = cell->image.x * 2;
    quad->uv0 = u | (cell->image.y << 8);
    quad->uv1 = (u + 0x3B) | (cell->image.y << 8);
    quad->uv2 = u | ((cell->image.y + 0x3F) << 8);
    quad->uv3 = (u + 0x3B) | ((cell->image.y + 0x3F) << 8);
    quad->clut = GetClut(cell->clut.x, cell->clut.y);
    quad->tpage = GetTPage(1, 0, cell->image.x & 0xFF80, cell->image.y);
    AddPrim(D_80092938, quad);
}

/* Draw the two-player selection: each side's pick, sliding in from its
 * previous one (the long way round wraps), with its neighbours when the
 * side is available, then "VS" and both names. The arguments are unused. */
void func_8007F258(void *packets, s32 arg) {
    VECTOR unused[2]; /* the original frame has 32 unused bytes */
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

/* Names of the game levels, then (after the frame rates and speeds) of the
 * entries of setting 10. GCC emits an initializer's string literals last to
 * first, after those of the code before it. */
char *D_800912F4[] = { "EASY", "NORMAL", "HARD" };

u8 D_80091300[] = { 0x1E, 0x14, 0xF, 0xC, 0xA, 6, 5, 4, 3, 2, 1 };

s32 D_8009130C[] = { 0x60, 0x80, 0xBB, 0x100, 0x180, 0x200, 0x300, 0x400 };

char *D_8009132C[] = {
    "BYSTANDER", "ON GUARD", "CONTROLLER2", "UP AND AT'EM",
    "SLOWPOKE", "MAGIC FIRER ", "MAGIC JUMPER", "KANGAROO",
    "GIVE CHASE", "RUN AWAY", "BACK DASH", "EASY BATTLE",
    "NORMAL BATTLE", "HARD BATTLE",
};

s32 D_80091364 = 0;

/* Hide both captions and forget the selected line's caption. */
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
    D_80092734 = NULL;
    func_8007F834();
    ACTOR_STANCE_BITS(&D_8009872C)->prev_stance = 3;
    ACTOR_STANCE_BITS(&D_80097010)->prev_stance = 3;
}

/* Leave the menus: camera mode 1, no menu shown, the captions hidden. */
void func_8007F8B4(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    func_80083C0C(1);
    D_80092734 = NULL;
    func_8007F834();
}

/* Close the system menu and go on: scene mode 3 when option 6 is set or
 * the first round was played, else mode 6 with the round count stepped
 * back (the round is played again). */
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
void func_8007F948(Menu *page, s32 entry) {
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

/* Draw the values column of the settings page, right-aligned, applying the
 * chosen speed as it is shown. */
void func_8007F9A0(Menu *page) {
    char text[8];

    func_8007E894(page->panel[0].x0 + page->panel[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007F948(page, 0);
    func_8007ECF0(func_8007F97C());
    func_8007F948(page, 1);
    sprintf(text, "%d", D_80099D98.speed + 1);
    D_80099D98.unkC = D_8009292C = D_8009130C[D_80099D98.speed];
    func_8007ECF0(text);
    func_8007F948(page, 2);
    sprintf(text, "%dFPS", D_80091300[D_80099D98.rate]);
    func_8007ECF0(text);
    func_8007F948(page, 3);
    func_8007ECF0(D_80099D98.com1 ? "COM" : "USER1");
    func_8007F948(page, 4);
    func_8007ECF0(D_80099D98.driven ? "COM" : "USER2");
    func_8007EE08(0);
}

/* Draw the values column of the second settings page; the chosen entry of
 * setting 10 is also passed to 80081100 as 0x15 + entry. */
void func_8007FB0C(Menu *page) {
    char text[8];

    func_8007E894(page->panel[0].x0 + page->panel[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007ECF0("");
    func_8007F948(page, 1);
    func_8007ECF0(D_8009132C[D_80099D98.command]);
    func_80081100(D_80099D98.command + 0x15, 1);
    func_8007F948(page, 2);
    sprintf(text, "%dFPS", D_80091300[D_80099D98.rate]);
    func_8007ECF0(text);
    func_8007EE08(0);
}

/* Draw the vibration page: per controller port, the vibration setting when
 * a type-4 controller without the "COM" setting is connected (the entry is
 * hidden otherwise). */
void func_8007FBEC(void) {
    Menu *page;
    s32 active;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    func_8007E894(0xA0, 0x8C);
    active = D_80092710 ^ 1;
    active &= 1;
    page = &D_800915AC[5];
    if (active && D_800915AC[5].cursor == 0) {
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
        D_800915AC[5].items[1].flags &= ~4;
    } else {
        D_800915AC[5].items[1].flags |= 4;
    }
    func_8007EE08(0);

    active = D_80092710 >> 1;
    active ^= 1;
    active &= 1;
    if (active && D_80092754 == 0) {
        active = 0;
    }
    page = &D_800915AC[6];
    if (active && D_800915AC[6].cursor == 0) {
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
        D_800915AC[6].items[1].flags &= ~4;
    } else {
        D_800915AC[6].items[1].flags |= 4;
    }
    func_8007EE08(0);
    func_8007F258(D_80092938, 1);
}

/* Draw the values column of the options page. */
void func_8007FE48(Menu *page) {
    char text[16];
    char *value;

    func_8007EE08(0);
    func_8007E894(page->panel[0].x0 + page->panel[0].w - 10, page->y);
    func_8007ECF0("");
    func_8007ECF0("");
    func_8007ECF0("");
    func_8007ECF0("");
    func_8007F948(page, 4);
    if (D_80099D98.option6 != 0) {
        sprintf(text, "%d", D_80099D98.option6);
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

/* Menu line handlers: step one setting with left/right (func_8007FF70):
 * the level, the speed, the frame rate, each port's vibration, each side's
 * computer control, option 6, rubber band battle and the opponent's
 * command. */
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

    func_8002954C(func_800289D0(6) + first * 2, data, 0x1000, 0, 0);
    other = data + 0x1000;
    func_8002954C(func_800289D0(6) + second * 2, other, 0x1000, 0, 0);
    D_80092700 = first;
    D_80092704 = second;
    D_80092714 = first;
    D_80092720 = second;
    D_80092710 = 3;
    func_80028A60(0);
    cell = &D_8009270C[first];
    LoadImage(&cell->clut, (u_long *)data);
    LoadImage(&cell->image, (u_long *)(data + 0x100));
    cell = &D_8009270C[second];
    LoadImage(&cell->clut, (u_long *)other);
    LoadImage(&cell->image, (u_long *)(data + 0x1100));
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
            LoadImage(&cell->clut, (u_long *)(D_800928D8 + (i << 12)));
            LoadImage(&cell->image, (u_long *)(D_800928D8 + (i << 12) + 0x100));
        }
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
    D_800928C8 = mode;
    if (mode == 4) {
        D_800915AC[5].parent = 3;
    } else {
        D_800915AC[5].parent = 4;
    }
    D_800915AC[6].parent = 5;
    if (mode == 3) {
        D_80091368[0].caption = 0x27;
        D_80091390[0].caption = 0x28;
    } else {
        D_80091368[0].caption = 0x25;
        D_80091390[0].caption = 0x26;
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

/* Menu line handler: hide the captions and end the menu screen (D_80092924). */
void func_800808F4(void) {
    D_80092924 = 1;
    func_8007F834();
}

/* Menu line handler: end the menu screen, restart the opening and give
 * both actor slots their first models again. */
void func_80080920(void) {
    D_80092924 = 1;
    func_800719F0();
    func_8008509C(0, 0);
    func_8008509C(1, 1);
}

/* Show a page, remembering the current one; 0xff returns to it. */
void func_80080964(s32 page) {
    Menu *previous;

    if (page == 0xFF) {
        D_80092734 = D_80092738;
        return;
    }
    previous = D_80092734;
    D_80092734 = &D_800915AC[page];
    D_80092738 = previous;
}

/* Whether page 3 is shown. */
s32 func_800809BC(void) {
    return D_80092734 == &D_800915AC[3];
}

/* Enter the settings/system menu at page 3 with every state reset. */
void func_800809D8(void) {
    func_80039FF8();
    D_80092734 = NULL;
    func_80080964(3);
    D_800915AC[3].cursor = 0;
    D_800915AC[4].cursor = 0;
    D_800928C8 = 0;
    D_80092758 = 0;
    func_8007F834();
    D_80092924 = 0;
    func_80080AA0(0);
    D_80092940 = 0;
    D_800928D8 = func_800891C0(6);
}

/* Release the loaded portraits unless they were uploaded (once). */
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
        LoadImage((RECT *)rect, D_80092760);
    }
}

/* Keep a copy of the shown screen: allocate the image buffer once, copy
 * the displayed buffer's area to (320,256) and read it back. */
void func_80080B58(void) {
    RECT area;

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
        D_80092734 = NULL;
        func_80080964(0);
        D_800915AC[0].cursor = 0;
        D_800915AC[2].cursor = 1;
    } else if (mode == 2) {
        D_80092734 = NULL;
        func_80080964(7);
        D_800915AC[7].cursor = 1;
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

/* Drop this frame's text quads. */
void func_80080D10(void) {
    D_800926DC = 0;
}

/* Link this frame's text quads and the menu overlay: the shown page's box
 * with its texture page and, while a page or the copy request is active, a
 * move of the kept screen copy into the draw buffer. */
void func_80080D20(void *ot) {
    POLY_FT4 *quad = D_800926D4[D_800928A0];
    RECT area;
    s32 i;

    for (i = 0; i < D_800926DC; i++, quad++) {
        AddPrim(ot, quad);
    }
    D_800926DC = 0;
    func_800811AC(ot);
    if ((D_80092734 != NULL && D_80092758 != 0) || D_800912F0 != 0) {
        if (D_80092734 != NULL) {
            AddPrim(ot, &D_80092734->panel[D_800928A0]);
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

    D_80095510[0].image = D_80095510[1].image = pixels;
    *(u32 *)&D_80095510[0].sprite[0].x0 = 0xB40000;
    *(u16 *)&D_80095510[0].sprite[0].u0 = 0x3000;
    SetSprt(&D_80095510[0].sprite[0]);
    SetShadeTex(&D_80095510[0].sprite[0], 1);
    D_80095510[0].sprite[0].h = 0xD;
    D_80095510[0].sprite[0].clut = D_800595D4;
    D_80095510[0].sprite[1] = D_80095510[0].sprite[0];
    *(u32 *)&D_80095510[1].sprite[0].x0 = 0xC30000;
    *(u16 *)&D_80095510[1].sprite[0].u0 = 0x3000;
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
    RECT rect;

    if (lower == 0) {
        if (text == D_8009273C) {
            return;
        }
        D_8009273C = text;
        func_80081094(&D_80095510[0], text, 0);
    } else {
        if (text == D_80092740) {
            return;
        }
        D_80092740 = text;
        func_80081094(&D_80095510[1], text, 1);
    }
    rect.x = 0x140;
    rect.y = 0x30;
    rect.w = 0x42;
    rect.h = 0xD;
    LoadImage(&rect, (void *)D_80095510[0].image);
}

/* Link the shown captions into the ordering table. */
void func_800811AC(void *ot) {
    Caption *caption;

    if (D_8009273C != 0) {
        caption = &D_80095510[0];
        caption->sprite[D_800928A0].w = caption->width;
        caption->sprite[D_800928A0].x0 = caption->x;
        AddPrim(ot, &caption->sprite[D_800928A0]);
    }
    if (D_80092740 != 0) {
        caption = &D_80095510[1];
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
    TILE *panel;
    s32 i;
    s32 widest;

    widest = 0;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        item->half_width = func_8007EB6C(item->text) / 2;
        widest = (widest < item->half_width) ? item->half_width : widest;
    }
    panel = &menu->panel[0];
    setlen(panel, 3);
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
    void (*handler)();
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

/* The lines of the menus; their texts are the literals of each table, emitted
 * last to first after the code before it ("" is func_8007FB0C's). */

/* Vibration choices per port; the selected one's caption is set at run
 * time. */
MenuItem D_80091368[2] = {
    { 1, 0x25, { 0 }, (s32)"", func_800802A4 },
    { 1, 0x29, { 0 }, (s32)"", func_80080108 },
};
MenuItem D_80091390[2] = {
    { 1, 0x26, { 0 }, (s32)"", func_8008040C },
    { 1, 0x29, { 0 }, (s32)"", func_80080144 },
};
MenuItem D_800913B8[] = {
    { 0, 1, { 0 }, (s32)"BONUS BATTLING", func_80080964, 4 },
    { 4, 0, { 0 }, (s32)"" },
    { 0, 2, { 0 }, (s32)"PRACTICE", func_80080780, 4 },
    { 0, 3, { 0 }, (s32)"TUTORIAL", func_80080920 },
    { 0, 5, { 0 }, (s32)"EXIT", (void (*)(s32))func_800851D4 },
};
MenuItem D_8009141C[] = {
    { 0, 7, { 0 }, (s32)"PLAYER1 VS COM", func_80080780, 1 },
    { 0, 8, { 0 }, (s32)"PLAYER1 VS PLAYER2", func_80080780, 2 },
    { 0, 9, { 0 }, (s32)"COM VS COM", func_80080780, 3 },
    { 4, 0, { 0 }, (s32)"" },
    { 3, 0xA, { 0 }, (s32)"NUM OF MATCHES", func_800801F8 },
    { 3, 0xB, { 0 }, (s32)"COM LEVEL", func_80080054 },
    { 3, 0xC, { 0 }, (s32)"RUBBER BAND", func_80080234 },
};
MenuItem D_800914A8[] = {
    { 0, 0xD, { 0 }, (s32)"CONTINUE BOUT", func_8007F854 },
    { 0, 0xE, { 0 }, (s32)"GIVE UP", func_80080964, 2 },
};
MenuItem D_800914D0[] = {
    { 1, 0xB, { 0 }, (s32)"GAME LEVEL", func_80080054 },
    { 1, 0x13, { 0 }, (s32)"MOTION SPEED", func_80080090 },
    { 1, 0x14, { 0 }, (s32)"FRAME RATE", func_800800CC },
    { 1, 0x23, { 0 }, (s32)"GEAR 1", func_80080180 },
    { 1, 0x24, { 0 }, (s32)"GEAR 2", func_800801BC },
};
MenuItem D_80091534[] = {
    { 0, 0x2A, { 0 }, (s32)"YES", func_8007F8E4 },
    { 0, 0x2A, { 0 }, (s32)"NO", func_80080964 },
};
MenuItem D_8009155C[] = {
    { 0, 0x10, { 0 }, (s32)"RETURN TO PRACTICE", func_8007F854 },
    { 1, 0x12, { 0 }, (s32)"AI", func_80080268 },
    { 1, 0x14, { 0 }, (s32)"FRAME RATE", func_800800CC },
    { 0, 0x11, { 0 }, (s32)"EXIT PRACTICE MODE", func_8007F8E4 },
};

/* The menus: lines, line count, menu returned to on cancel, extra drawing. */
Menu D_800915AC[8] = {
    { 0, { 0 }, D_800914A8, 2, 0 },
    { 0x20, { 0 }, D_800914D0, 5, 0, func_8007F9A0 },
    { 0, { 0 }, D_80091534, 2, 0 },
    { 0, { 0 }, D_800913B8, 5, 3 },
    { 0, { 0 }, D_8009141C, 7, 3, func_8007FE48 },
    { 0, { 0 }, D_80091368, 2, 3 },
    { 0, { 0 }, D_80091390, 2, 3 },
    { 1, { 0 }, D_8009155C, 4, 7, func_8007FB0C },
};

/* Unreferenced. */
const char D_80070198[] = "";

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
        D_80099D98.com1 = 0;
        D_80099D98.driven = 1;
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
        D_80099D98.com1 = 0;
        D_80099D98.driven = 0;
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
        D_80099D98.com1 = 1;
        D_80099D98.driven = 1;
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
        D_80099D98.com1 = 0;
        D_80099D98.driven = 1;
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
        D_80099D98.com1 = 0;
        D_80099D98.driven = 1;
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
    D_8009A0D8[0].background.y0 = 0x60;
    D_8009A0D8[1].background.y0 = 0x60;
    D_8009A0D8[0].background.r0 = 0x7F;
    D_8009A0D8[0].background.g0 = 0x7F;
    D_8009A0D8[0].background.b0 = 0x7F;
    D_8009A0D8[1].background.r0 = 0x7F;
    D_8009A0D8[1].background.g0 = 0x7F;
    D_8009A0D8[1].background.b0 = 0x7F;
    D_8009A0D8[0].background.h = D_8009286C - 0x60;
    D_8009A0D8[1].background.h = D_8009286C - 0x60;
}

/* Clear the fade tiles back to the full, black screen. */
void func_80081E6C(void) {
    D_8009A0D8[0].background.y0 = 0;
    D_8009A0D8[1].background.y0 = 0;
    D_8009A0D8[0].background.r0 = 0;
    D_8009A0D8[0].background.g0 = 0;
    D_8009A0D8[0].background.b0 = 0;
    D_8009A0D8[1].background.r0 = 0;
    D_8009A0D8[1].background.g0 = 0;
    D_8009A0D8[1].background.b0 = 0;
    D_8009A0D8[0].background.h = D_8009286C;
    D_8009A0D8[1].background.h = D_8009286C;
}
