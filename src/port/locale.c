/*
 * The game's text in the player's language (docs/runtime.md, Languages).
 * The host installs every language's translations at XEM_LOCALE, outside
 * the PS1's addresses and outside snapshots, as it places the Kanji font,
 * and says which language is active (xem_core::locale). A message window
 * takes its text only through window_queue_message, window_render_text_line
 * and the insertions (window_insert_text; docs/scripts/text-control.md):
 * there each text the game passes is named by the FNV-1a 64 hash of its
 * bytes through its closing 00, walked as window_reveal_text walks them,
 * and the translation with that hash, if the active language has one, is
 * passed instead. The game's own bytes and pointers stay as they are; a
 * window's text pointer then points at the translation, which stays
 * installed whichever language is active later, so a window keeps
 * revealing the text it began.
 *
 * A field message's window takes its columns and rows from the message's
 * table (text_get_message_columns, text_get_message_rows): a translation
 * that needs more gets them. The letters a language adds to the font are
 * two-byte glyphs from 0xFF20 (0xFF xx is a two-byte glyph the original
 * text never uses but for 0xFFFF): text_get_glyph_width gives them the
 * narrow letters' 2 columns, and text_draw_glyph draws each through the
 * original's own drawing of glyph 0xFFFF, whose rows (text_special_glyph_rows)
 * hold the letter while it draws: a letter of the game's font with rows
 * taken out or blanked and an accent drawn over it, the host's recipe. The
 * rows are put back before it returns, and nothing waits in between.
 *
 * The menus' command labels (Status, Equip, Buy ...) are not text: they are
 * sheet entries 0x123-0x149 of the menu's sprite sheet, words drawn in its
 * textures. When a menu has loaded its resources (the five loaders below,
 * each the original's first), each such entry the active language
 * translates (named by the hash of the entry's bytes, its parts' places in
 * the textures) is drawn with the game's text renderer in the game's font
 * into texture page (448, 256), which no menu uses, and the entry made one
 * part showing it with the text palette the menus load at (0, 0x1D1). The
 * line image is a heap block, as the menus' own are: the transfer to VRAM
 * reads RAM. The stat panels' words (Attack, Defense, Weight ...) are sheet
 * entries 0xDE-0xF8 composed of the sheet's small font, a part a letter: a
 * translated one is spelled anew with the same font's letter cells, as many
 * parts as letters, ending where the original ended (the panels align them
 * right); a word ending in % keeps the original's % (its rightmost part).
 *
 * With no language active (the original's English) every function is the
 * original's. The port keeps no data of its own.
 */
#include "common.h"
#include "psyq/libgpu.h"
#include "resident/heap.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "resident/window.h"
#include "xem/memory.h"

/* The host's translations (xem_core::locale::BASE), its layout below. */
#define XEM_LOCALE 0x20000000
#define XEM_LOCALE_MAGIC 0x31434C58 /* "XLC1" */
/* A language's first added glyph's second byte (0xFF20). */
#define XEM_LOCALE_FIRST_GLYPH 0x20
/* The longest text the port names: the original's are under 2 KiB. */
#define XEM_TEXT_LIMIT 4096
#define XEM_RAM 0x80000000
#define XEM_RAM_END 0x80200000

typedef struct {
    u32 magic;
    u32 active;      /* 0: the original's text; n: language n */
    u32 languages;
    u32 glyphs;      /* address of `glyph_count` XemLocaleGlyph */
    u32 glyph_count;
    u32 reserved[3];
} XemLocaleHeader;

/* Language n (1-based) follows the header at n - 1. */
typedef struct {
    u32 texts;       /* address of `count` XemLocaleText, ascending by hash */
    u32 count;
} XemLocaleLanguage;

typedef struct {
    u32 hash_low;
    u32 hash_high;
    u32 text;        /* the translation's bytes, through its 00 */
    u8 columns;      /* its widest line, in the window's 4-pixel columns */
    u8 rows;         /* its longest page's lines */
    u16 reserved;
} XemLocaleText;

typedef struct {
    u8 base;         /* the one-byte glyph it is drawn from */
    u8 reserved;
    u16 drop;        /* rows taken out of the base (bit n row n), those above moving down */
    u16 clear;       /* rows blanked */
    u16 accent[11];  /* rows drawn over it */
} XemLocaleGlyph;

s16 xem_original_window_queue_message(Window *window, s32 message);
s32 xem_original_window_render_text_line(u8 *text, void *image, s16 width, s32 flags);
void xem_original_window_insert_text(Window *window, u8 *text);
u8 xem_original_text_get_message_columns(u16 *table, s32 index);
u8 xem_original_text_get_message_rows(u16 *table, s32 index);
s32 xem_original_text_get_glyph_width(u16 first, u16 second);
void xem_original_text_draw_glyph(s32 first, u16 second, u16 *image, s16 stride, s32 plane);
u16 *text_get_font(void);
u8 *text_get_resource_entry(u8 *resource, s32 index);
/* resident/menu.h's MenuState *menu_state_current, read as its address
 * (the header's layout checks hold for the game's 32-bit pointers only):
 * the menu's sprite sheet is at +0x2DC. */
extern u32 menu_state_current;
#define XEM_MENU_SHEET 0x2DC
void xem_original_menu_load_resources(void);
void xem_original_item_shop_load_resources(void);
void xem_original_gear_shop_load_resources(void);
void xem_original_member_change_load_resources(void);
void xem_original_name_entry_load_resources(void);

static const XemLocaleHeader *xem_locale_header(void) {
    const XemLocaleHeader *header = (const XemLocaleHeader *)(XemUintptr)XEM_LOCALE;

    return header->magic == XEM_LOCALE_MAGIC ? header : NULL;
}

/* The active language's translations, or NULL for the original's text. */
static const XemLocaleLanguage *xem_locale_language(void) {
    const XemLocaleHeader *header = xem_locale_header();

    if (header == NULL || header->active == 0 || header->active > header->languages) {
        return NULL;
    }
    return (const XemLocaleLanguage *)(header + 1) + (header->active - 1);
}

/* The bytes of a text in RAM through its closing 00, as window_reveal_text
 * walks it (tools/translations.py text_bytes): a two-byte glyph from 0xFE
 * on, a control 0F with its sub-code's operands. 0 for a text the port does
 * not name: outside RAM, or without its 00 within XEM_TEXT_LIMIT bytes and
 * the RAM's end. */
u32 xem_text_length(const u8 *text) {
    u32 address = (u32)(XemUintptr)text;
    u32 limit;
    u32 at;
    u8 byte;

    if (address < XEM_RAM || address >= XEM_RAM_END) {
        return 0;
    }
    limit = XEM_RAM_END - address;
    if (limit > XEM_TEXT_LIMIT) {
        limit = XEM_TEXT_LIMIT;
    }
    for (at = 0; at < limit;) {
        byte = text[at];
        if (byte == 0) {
            return at + 1;
        }
        if (byte == 0x0F) {
            if (at + 1 >= limit) {
                return 0;
            }
            byte = text[at + 1];
            at += byte == 0x04 || byte == 0x0B ? 2 : byte == 0x03 ? 4 : 3;
        } else if (byte >= 0xFE) {
            at += 2;
        } else {
            at += 1;
        }
    }
    return 0;
}

/* The active language's translation of the `length` bytes at `bytes`, by
 * their FNV-1a 64 hash, or NULL. */
static const XemLocaleText *xem_locale_lookup(const u8 *bytes, u32 length) {
    const XemLocaleLanguage *language = xem_locale_language();
    const XemLocaleText *texts;
    const XemLocaleText *entry;
    unsigned long long hash = 0xCBF29CE484222325ULL;
    unsigned long long found;
    u32 i;
    u32 low;
    u32 high;
    u32 middle;

    if (language == NULL || length == 0) {
        return NULL;
    }
    for (i = 0; i < length; i++) {
        hash = (hash ^ bytes[i]) * 0x100000001B3ULL;
    }
    texts = (const XemLocaleText *)(XemUintptr)language->texts;
    low = 0;
    high = language->count;
    while (low < high) {
        middle = low + (high - low) / 2;
        entry = &texts[middle];
        found = (unsigned long long)entry->hash_high << 32 | entry->hash_low;
        if (found == hash) {
            return entry;
        }
        if (found < hash) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return NULL;
}

/* The active language's translation of `text`, or NULL. */
static const XemLocaleText *xem_locale_find(const u8 *text) {
    return xem_locale_language() != NULL ? xem_locale_lookup(text, xem_text_length(text)) : NULL;
}

static u8 *xem_translate(u8 *text) {
    const XemLocaleText *found = xem_locale_find(text);

    return found != NULL ? (u8 *)(XemUintptr)found->text : text;
}

s16 window_queue_message(Window *window, s32 message) {
    return xem_original_window_queue_message(window, (s32)(u32)(XemUintptr)xem_translate((u8 *)(XemUintptr)(u32)message));
}

s32 window_render_text_line(u8 *text, void *image, s16 width, s32 flags) {
    return xem_original_window_render_text_line(xem_translate(text), image, width, flags);
}

void window_insert_text(Window *window, u8 *text) {
    xem_original_window_insert_text(window, xem_translate(text));
}

u8 text_get_message_columns(u16 *table, s32 index) {
    u8 columns = xem_original_text_get_message_columns(table, index);
    const XemLocaleText *found = xem_locale_find(text_get_resource_entry((u8 *)table, index));

    return found != NULL && found->columns > columns ? found->columns : columns;
}

u8 text_get_message_rows(u16 *table, s32 index) {
    u8 rows = xem_original_text_get_message_rows(table, index);
    const XemLocaleText *found = xem_locale_find(text_get_resource_entry((u8 *)table, index));

    return found != NULL && found->rows > rows ? found->rows : rows;
}

/* The recipe of glyph (first, second) when it is a language's added glyph:
 * every installed language's, whichever is active, so a window goes on
 * revealing a translation begun before the player changed language. */
static const XemLocaleGlyph *xem_locale_glyph(u16 first, u16 second) {
    const XemLocaleHeader *header;

    if (first != 0xFF || second < XEM_LOCALE_FIRST_GLYPH || second == 0xFF) {
        return NULL;
    }
    header = xem_locale_header();
    if (header == NULL || second - XEM_LOCALE_FIRST_GLYPH >= header->glyph_count) {
        return NULL;
    }
    return (const XemLocaleGlyph *)(XemUintptr)header->glyphs + (second - XEM_LOCALE_FIRST_GLYPH);
}

s32 text_get_glyph_width(u16 first, u16 second) {
    if (xem_locale_glyph(first, second) != NULL) {
        return 2;
    }
    return xem_original_text_get_glyph_width(first, second);
}

void text_draw_glyph(s32 first, u16 second, u16 *image, s16 stride, s32 plane) {
    const XemLocaleGlyph *glyph = xem_locale_glyph((u16)first, second);
    u16 *font = text_get_font();
    const u16 *base;
    u16 saved[11];
    s32 from;
    s32 to;
    s32 row;

    if (glyph == NULL || font == NULL) {
        xem_original_text_draw_glyph(first, second, image, stride, plane);
        return;
    }
    /* text_install_font: halfword 1 the glyphs' offset, 6 the first
     * one-byte glyph's code; 22 bytes a glyph. */
    base = (const u16 *)((u8 *)font + font[1] + (glyph->base - font[6]) * 22);
    for (row = 0; row < 11; row++) {
        saved[row] = text_special_glyph_rows[row];
    }
    to = 10;
    for (from = 10; from >= 0; from--) {
        if (!((glyph->drop >> from) & 1)) {
            text_special_glyph_rows[to--] = base[from];
        }
    }
    while (to >= 0) {
        text_special_glyph_rows[to--] = 0;
    }
    for (row = 0; row < 11; row++) {
        if ((glyph->clear >> row) & 1) {
            text_special_glyph_rows[row] = 0;
        }
        text_special_glyph_rows[row] |= glyph->accent[row];
    }
    xem_original_text_draw_glyph(0xFF, 0xFF, image, stride, plane);
    for (row = 0; row < 11; row++) {
        text_special_glyph_rows[row] = saved[row];
    }
}

/* The menus' command labels: their sheet entries, where they are drawn, the
 * widest a label may be (in the renderer's 4-pixel columns, a line image of
 * that many columns plus 3 halfwords a row) and the rows a line takes. */
#define XEM_LABEL_FIRST 0x123
#define XEM_LABEL_LAST 0x149
#define XEM_LABEL_PAGE_X 448
#define XEM_LABEL_PAGE_Y 256
#define XEM_LABEL_PAGE_WIDTH 64 /* halfwords: 256 4-bit texels */
#define XEM_LABEL_COLUMNS 59
#define XEM_LABEL_STRIDE (XEM_LABEL_COLUMNS + 3)
#define XEM_LABEL_ROWS 13
#define XEM_LABEL_CLUT_X 0
#define XEM_LABEL_CLUT_Y 0x1D1

/* Draw each command label the active language translates into texture page
 * (448, 256), left to right and down in 13-row lines, and make its entry
 * one part showing it, where the original's first part was (two rows up, as
 * the font's 13 rows centre on the original's 9). */
void xem_locale_menu_labels(u16 *sheet) {
    u16 *image = NULL;
    const XemLocaleText *found;
    SheetPart *part;
    SheetPart *first;
    s16 *entry;
    RECT rect;
    s32 id;
    s32 width;
    s32 halfwords;
    s32 row;
    s32 column;
    s32 at_x = 0;
    s32 at_y = 0;
    s16 left;
    s16 top;

    if (sheet == NULL || xem_locale_language() == NULL) {
        return;
    }
    for (id = XEM_LABEL_FIRST; id <= XEM_LABEL_LAST; id++) {
        entry = (s16 *)((u8 *)sheet + sheet[id + 2]);
        if (entry[0] < 1 || entry[0] > 8) {
            continue;
        }
        found = xem_locale_lookup((const u8 *)entry, 4 + sizeof(SheetPart) * entry[0]);
        if (found == NULL) {
            continue;
        }
        if (image == NULL) {
            image = heap_alloc(XEM_LABEL_STRIDE * XEM_LABEL_ROWS * 2, 0);
        }
        for (row = 0; row < XEM_LABEL_STRIDE * XEM_LABEL_ROWS; row++) {
            image[row] = 0;
        }
        width = xem_original_window_render_text_line((u8 *)(XemUintptr)found->text, image, XEM_LABEL_COLUMNS, 0);
        halfwords = (width + 3) / 4;
        if (halfwords == 0 || halfwords > XEM_LABEL_PAGE_WIDTH) {
            continue;
        }
        if (at_x + halfwords > XEM_LABEL_PAGE_WIDTH) {
            at_x = 0;
            at_y += XEM_LABEL_ROWS;
        }
        if (at_y + XEM_LABEL_ROWS > 256) {
            break;
        }
        /* The line image's rows, each `halfwords` wide, one after another. */
        for (row = 0; row < XEM_LABEL_ROWS; row++) {
            for (column = 0; column < halfwords; column++) {
                image[row * halfwords + column] = image[row * XEM_LABEL_STRIDE + column];
            }
        }
        rect.x = XEM_LABEL_PAGE_X + at_x;
        rect.y = XEM_LABEL_PAGE_Y + at_y;
        rect.w = halfwords;
        rect.h = XEM_LABEL_ROWS;
        LoadImage(&rect, (u_long *)image);
        DrawSync(0); /* the transfer reads the line image, which the next label reuses */
        first = (SheetPart *)(entry + 2);
        left = first->x;
        top = first->y;
        for (part = first + 1; part < first + entry[0]; part++) {
            if ((s16)part->x < left) {
                left = part->x;
            }
            if ((s16)part->y < top) {
                top = part->y;
            }
        }
        first->u = at_x * 4;
        first->v = at_y;
        first->w = width;
        first->h = XEM_LABEL_ROWS;
        first->x = left;
        first->y = top - 2;
        first->mode = 0;
        first->clut_x = XEM_LABEL_CLUT_X;
        first->clut_y = XEM_LABEL_CLUT_Y;
        first->page_x = XEM_LABEL_PAGE_X;
        first->page_y = XEM_LABEL_PAGE_Y;
        first->flip_x = 0;
        first->flip_y = 0;
        entry[0] = 1;
        at_x += halfwords;
    }
    if (image != NULL) {
        heap_free(image);
    }
}

/* The stat panels' words and the small font's cells: digits, capitals and
 * small letters from these entries, the full stop at 0x91. A letter's
 * advance in pixels, as the original words space them. */
#define XEM_WORD_FIRST 0xDE
#define XEM_WORD_LAST 0xF8
#define XEM_WORD_PARTS 12
#define XEM_SMALL_DIGITS 0x00
#define XEM_SMALL_CAPITALS 0x0A
#define XEM_SMALL_LETTERS 0x24
#define XEM_SMALL_STOP 0x91
#define XEM_SMALL_ADVANCE 6
static const u8 xem_small_capital_advances[26] = {7, 7, 7, 7, 6, 6, 7, 7, 4, 6, 7, 6, 8, 7, 7, 7, 7, 7, 6, 6, 7, 7, 8, 7, 7, 6};
static const u8 xem_small_letter_advances[26] = {6, 6, 6, 6, 6, 5, 6, 6, 3, 4, 6, 3, 8, 6, 6, 6, 6, 5, 6, 6, 6, 6, 8, 7, 6, 6};

/* The small font's cell for a one-byte glyph of the text and its advance,
 * or -1 for a glyph it lacks. */
static s32 xem_small_cell(u8 code, s32 *advance) {
    if (code >= 0x16 && code <= 0x1F) {
        *advance = XEM_SMALL_ADVANCE;
        return XEM_SMALL_DIGITS + code - 0x16;
    }
    if (code >= 0x20 && code <= 0x39) {
        *advance = xem_small_capital_advances[code - 0x20];
        return XEM_SMALL_CAPITALS + code - 0x20;
    }
    if (code >= 0x3D && code <= 0x56) {
        *advance = xem_small_letter_advances[code - 0x3D];
        return XEM_SMALL_LETTERS + code - 0x3D;
    }
    if (code == 0x14) {
        *advance = 3;
        return XEM_SMALL_STOP;
    }
    return -1;
}

/* A letter followed by i or l (narrow letters whose ink sits right of their
 * cells' left edges) comes 2 pixels closer, as the original words set them. */
static s32 xem_small_kern(u8 code, u8 next) {
    return (next == 0x45 || next == 0x48) && code != 0x45 && code != 0x48 ? -2 : 0;
}

/* Spell each stat panel word the active language translates with the
 * small font's cells. */
void xem_locale_menu_words(u16 *sheet) {
    const XemLocaleText *found;
    const u8 *text;
    SheetPart *first;
    SheetPart *part;
    s16 *entry;
    s32 id;
    s32 length;
    s32 total;
    s32 advance;
    s32 right;
    s32 x;
    s32 k;
    s32 cell;
    s32 letters;
    s32 rightmost;
    s16 y;
    SheetPart percent;

    if (sheet == NULL || xem_locale_language() == NULL) {
        return;
    }
    for (id = XEM_WORD_FIRST; id <= XEM_WORD_LAST; id++) {
        entry = (s16 *)((u8 *)sheet + sheet[id + 2]);
        if (entry[0] < 1 || entry[0] > XEM_WORD_PARTS) {
            continue;
        }
        found = xem_locale_lookup((const u8 *)entry, 4 + sizeof(SheetPart) * entry[0]);
        if (found == NULL) {
            continue;
        }
        text = (const u8 *)(XemUintptr)found->text;
        for (length = 0; length <= entry[0] && text[length] != 0; length++) {
        }
        if (length == 0 || length > entry[0]) {
            continue;
        }
        letters = text[length - 1] == 0x5A ? length - 1 : length; /* a closing % */
        for (k = 0, total = 0; k < letters && xem_small_cell(text[k], &advance) >= 0; k++) {
            total += advance + (k + 1 < letters ? xem_small_kern(text[k], text[k + 1]) : 0);
        }
        if (k < letters) {
            continue;
        }
        first = (SheetPart *)(entry + 2);
        rightmost = 0;
        for (k = 1; k < entry[0]; k++) {
            if ((s16)first[k].x > (s16)first[rightmost].x) {
                rightmost = k;
            }
        }
        percent = first[rightmost];
        right = letters < length ? (s16)percent.x : (s16)percent.x + XEM_SMALL_ADVANCE;
        x = right - total;
        y = first->y;
        for (k = 0; k < letters; k++) {
            cell = xem_small_cell(text[k], &advance);
            part = first + k;
            *part = *(SheetPart *)((s16 *)((u8 *)sheet + sheet[cell + 2]) + 2);
            part->x = x;
            part->y = y;
            x += advance + (k + 1 < letters ? xem_small_kern(text[k], text[k + 1]) : 0);
        }
        if (letters < length) {
            first[letters] = percent;
        }
        entry[0] = length;
    }
}

static u16 *xem_menu_sheet(void) {
    return menu_state_current != 0 ? (u16 *)(XemUintptr)XEM_U32(menu_state_current + XEM_MENU_SHEET) : NULL;
}

void menu_load_resources(void) {
    xem_original_menu_load_resources();
    xem_locale_menu_labels(xem_menu_sheet());
    xem_locale_menu_words(xem_menu_sheet());
}

void item_shop_load_resources(void) {
    xem_original_item_shop_load_resources();
    xem_locale_menu_labels(xem_menu_sheet());
    xem_locale_menu_words(xem_menu_sheet());
}

void gear_shop_load_resources(void) {
    xem_original_gear_shop_load_resources();
    xem_locale_menu_labels(xem_menu_sheet());
    xem_locale_menu_words(xem_menu_sheet());
}

void member_change_load_resources(void) {
    xem_original_member_change_load_resources();
    xem_locale_menu_labels(xem_menu_sheet());
    xem_locale_menu_words(xem_menu_sheet());
}

void name_entry_load_resources(void) {
    xem_original_name_entry_load_resources();
    xem_locale_menu_labels(xem_menu_sheet());
    xem_locale_menu_words(xem_menu_sheet());
}
