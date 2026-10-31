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
 * With no language active (the original's English) every function is the
 * original's. The port keeps no data of its own.
 */
#include "common.h"
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

/* The active language's translation of `text`, or NULL. */
static const XemLocaleText *xem_locale_find(const u8 *text) {
    const XemLocaleLanguage *language = xem_locale_language();
    const XemLocaleText *texts;
    const XemLocaleText *entry;
    unsigned long long hash = 0xCBF29CE484222325ULL;
    unsigned long long found;
    u32 length;
    u32 i;
    u32 low;
    u32 high;
    u32 middle;

    if (language == NULL || (length = xem_text_length(text)) == 0) {
        return NULL;
    }
    for (i = 0; i < length; i++) {
        hash = (hash ^ text[i]) * 0x100000001B3ULL;
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
