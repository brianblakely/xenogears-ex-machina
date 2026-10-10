/* Text, message windows and controllers (80032e7c-800366e0), GCC 2.7.2 at
 * -G0 (80032f54 matches only under 2.7.2): the handwritten packed-data
 * decoder (text_unpack_lzss_alloc.s), the font and system data resources, character
 * code decoding and encoding, the message windows and their glyph drawing,
 * then the controllers (buttons, sticks, state queue, actuators), the play
 * time, VRAM dumps to the PC file server and the vertical-blank callback. It
 * starts after the heap report unit (heap_host_report.c) and ends where
 * 80036718's jump table (0x80018b58, 0 mod 8) follows this unit's tables at
 * 4 mod 8: the next unit starts between 800365fc and 80036718, at the
 * console's output hook 800366e0 (console_and_sound_driver.c). */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libsn.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/text.h"
#include "resident/window.h"
#include "own_declarations.h"

/* This unit's own variables: those of up to 8 bytes in its .sbss
 * (8005934c), the larger window, text and controller queue buffers in its
 * .bss (80059fd8), as the original assembler placed them (SBSS_main2 in
 * slus_006.64.mk). */
static s32 text_font_two_byte_threshold;  /* 8005934C: font: first byte of a two-byte character */
static s32 text_font_two_byte_glyph_offset; /* 80059350 */
static s32 text_font_narrow_one_byte_count; /* 80059354 */
static s32 text_font_narrow_two_byte_limit; /* 80059358 */
static u8 *text_font_glyphs;  /* 8005935C: font glyph data */
static u8 **text_system_resources; /* 80059360: system data: resource table */
static s32 text_font_first_one_byte_code; /* 80059364 */
static u8 *text_system_data;  /* 80059368: system data block */
static u16 *text_font; /* 8005936C: font block: halfword 1 glyph offset, 2 first
                         * byte of a two-byte character */
static u8 pad_play_time_frames;   /* 80059370: play time frames */
static u32 pad_port0_last_held;  /* 80059374: held pad buttons of the last frame */
static u32 pad_port1_last_held; /* 80059378 */
static u32 pad_queue_count;  /* 8005937C: queued controller states */
static u32 pad_queue_write_index;  /* 80059380: queue write index */
static u32 pad_queue_read_index;  /* 80059384: queue read index */
static u8 pad_last_read_type;   /* 80059388: kind of the last read controller */
static u8 pad_unread_byte; /* 8005938C */
static s32 pad_vblank_polls_host;  /* 80059390: the vertical-blank callback polls the host */
/* The one-line layout window and its line. */
static Window window_single_line_window; /* 80059FD8 */
static WindowLine window_single_line_layout; /* 8005A068 */
/* Number character codes: color, 10 digits, 0xFFFF, and two that nothing
 * addresses (a word of its own would be a small variable, in .sbss). */
static u16 text_number_codes[14]; /* 8005A0C8 */
static u8 text_decoded_buffer[0x18]; /* 8005A0E4: decoded text */
/* Queued controller states (16 entries of the six state words). */
static u16 pad_queue_port0_held[16]; /* 8005A0FC */
static u16 pad_queue_port1_held[16]; /* 8005A11C */
static u16 pad_queue_port0_pressed[16]; /* 8005A13C */
static u16 pad_queue_port1_pressed[16]; /* 8005A15C */
static u16 pad_queue_port0_repeated[16]; /* 8005A17C */
static u16 pad_queue_port1_repeated[16]; /* 8005A19C */
static Actuator pad_actuators[2]; /* 8005A1BC */

/* The text palette: two 16-colour CLUTs. */
u16 text_palette[32] = { /* 80050190 */
    0x0000, 0xF7BD, 0xC086, 0xF7BD, 0x0000, 0xF7BD, 0xC086, 0xF7BD,
    0x0000, 0xF7BD, 0xC086, 0xF7BD, 0x0000, 0xF7BD, 0xC086, 0xF7BD,
    0x0000, 0x0000, 0x0000, 0x0000, 0xF7BD, 0xF7BD, 0xF7BD, 0xF7BD,
    0xC086, 0xC086, 0xC086, 0xC086, 0xF7BD, 0xF7BD, 0xF7BD, 0xF7BD,
};
/* The glyph of character pair 0xFF 0xFF: eleven rows of 12 bits, the
 * font block's 22-byte glyph format, which text_draw_glyph draws in place
 * of a font glyph. */
INCLUDE_ASSET(".data", text_special_glyph_rows, 0x800501D0, 0x16);
u16 pad_button_bits[8] = {0x20, 0x40, 0x10, 0x80, 0x4, 0x1, 0x8, 0x2}; /* 800501E8: button bits */
u8 pad_play_time_stopped = 0;              /* 800501F8: play time stopped at 100 hours */
void (*pad_vblank_hook)(void) = NULL; /* 800501FC: vertical-blank hook */
s32 pad_unread_reset_word = 1; /* 80050200 */
s32 pad_unread_init_word = 0; /* 80050204 */
s32 pad_queue_overflowed = 0; /* 80050208: queue overflowed */
u8 pad_dpad_stick_x_table[16] = { /* 8005020C */
    0x80, 0x80, 0xFF, 0xFF, 0x80, 0x80, 0xFF, 0x80, 0x00, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80,
};
u8 pad_dpad_stick_y_table[16] = { /* 8005021C */
    0x80, 0x00, 0x80, 0x00, 0xFF, 0x80, 0xFF, 0x80, 0x80, 0x00, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80,
};
s32 pad_port0_unchanged_frame_count = 0; /* 8005022C: frames the held buttons have not changed */
s32 pad_port1_unchanged_frame_count = 0; /* 80050230 */
s32 pad_unreferenced_word = 0; /* 80050234: nothing reads it */
u8 pad_button_assignment[8] = {0, 1, 2, 3, 4, 5, 6, 7}; /* 80050238: button assignment */

/* 80032E7C: Unpacked size of packed data (its first word). */
s32 text_get_lzss_unpacked_size(s32 *packed) {
    return *packed;
}

/* 80032E88 */
INCLUDE_ASM("decomp/src/resident", text_unpack_lzss_alloc);

/* 80032EB4 */
INCLUDE_ASM("decomp/src/resident", text_unpack_lzss);

/* 80032F54: Build a message window's two texture halves for each line and display
 * buffer. Lines share a glyph image in pairs, using alternating CLUTs;
 * two draw modes select the texture pages either side of the 256-pixel split. */
void window_open(Window *window, s16 vram_x, s16 vram_y, s16 x, u16 y,
                   u16 columns, u16 rows) {
    s32 i;
    s32 half_row;
    s32 plane;
    s32 uv;
    s16 texture_y;
    SPRT *sprite;

    window->unk4 = x;
    window->flags = 0;
    window->unk84 = 0;
    window->queue = NULL;
    window->queued = 0;
    window->unk14 = 14;
    window->unk68 = 1;
    window->unk69 = 1;
    window->width = columns;
    window->unk6C = 0;
    window->unk6A = 0;
    window->unk6D = 0;
    window->unk6B = 0;
    window->unk6E = 0xFF;
    window->unkE = vram_y;
    window->unk6 = y;
    window->lines = rows;
    window->width |= 1;
    window->unk8 = (u16)window->width * 4;
    window->stride = (u16)window->width + 3;
    heap_set_next_class(0x29);
    window->layout = heap_alloc(window->lines * sizeof(WindowLine), 2);
    heap_set_next_class(0x28);
    window->image = heap_alloc(window->stride * 28, 2);
    setlen(&window->tile[0], 3);
    *(u32 *)&window->tile[0].r0 = 0x60000000;
    *(u32 *)&window->tile[0].x0 = (window->unk4 - 7) | ((window->unk6 - 5) << 16);
    *(u32 *)&window->tile[0].w = (window->width * 4 + 13) |
                                 ((window->lines * window->unk14 + 10) << 16);
    SetSemiTrans(&window->tile[0], 1);
    window->tile[1] = window->tile[0];
    for (i = 0; i < window->lines; i++) {
        *(u32 *)&window->layout[i].sprite[0][0].x0 = window->unk4 |
                                        ((window->unk6 + window->unk14 * i) << 16);
        *(u32 *)&window->layout[i].sprite[0][1].x0 = (window->unk4 + 256) |
                                        ((window->unk6 + window->unk14 * i) << 16);
        half_row = i / 2;
        plane = i & 1;
        *(u32 *)&window->layout[i].sprite[0][0].w =
            window->unk8 < 257 ? window->unk8 | 0xD0000 : 0xD0100;
        *(u32 *)&window->layout[i].sprite[0][1].w =
            window->unk8 >= 257 ? (window->unk8 - 240) | 0xD0000 : 0xD0000;
        texture_y = vram_y + half_row * 13;
        uv = (vram_x & 0x3F) * 4 | ((texture_y & 0xFF) << 8);
        *(u16 *)&window->layout[i].sprite[0][0].u0 = uv;
        *(u16 *)&window->layout[i].sprite[0][1].u0 = uv;
        setSprt(&window->layout[i].sprite[0][0]);
        setSprt(&window->layout[i].sprite[0][1]);
        setShadeTex(&window->layout[i].sprite[0][0], 1);
        setShadeTex(&window->layout[i].sprite[0][1], 1);
        sprite = (SPRT *)(i * sizeof(WindowLine) + (s32)window->layout);
        window->layout[i].sprite[1][0] = *sprite;
        sprite = (SPRT *)(i * sizeof(WindowLine) + (s32)window->layout + sizeof(SPRT));
        window->layout[i].sprite[1][1] = *sprite;
        window->layout[i].rect.x = vram_x;
        window->layout[i].rect.y = texture_y;
        window->layout[i].rect.w = window->stride;
        window->layout[i].rect.h = 13;
        window->layout[i].width = 0;
        window->layout[i].clut = plane == 0 ? text_plane0_clut : text_plane1_clut;
        window->layout[i].row = vram_y + half_row * 13;
        window->layout[i].plane = plane;
        window->layout[i].slot = i;
    }
    SetDrawMode((DR_MODE *)window->unk30, 0, 0, GetTPage(0, 0, vram_x, vram_y), NULL);
    SetDrawMode((DR_MODE *)window->unk3C, 0, 0, GetTPage(0, 0, vram_x + 64, vram_y), NULL);
}

/* 8003342C: Turn a resource's offset table (count, then offsets) into pointers.
 * Returns the count. */
u32 text_relocate_offset_table(void *data) {
    u32 *table = data;
    u32 i;

    for (i = 1; i <= table[0]; i++) {
        table[i] += (u32)data;
    }
    return table[0];
}

/* 80033474: As 8003342c, without returning the count. */
void text_relocate_offset_table_no_count(void *data) {
    u32 *table = data;
    u32 i;

    for (i = 1; i <= table[0]; i++) {
        table[i] += (u32)data;
    }
}

/* 800334B8: The installed font block and system data block. */
u16 *text_get_font(void) {
    return text_font;
}

/* 800334C8 */
u8 *text_get_system_data(void) {
    return text_system_data;
}

/* 800334D8: Release the font. */
void text_release_font(void) {
    heap_unprotect_block(text_font);
    heap_free(text_font);
    text_font = NULL;
}

/* 80033518: Release the system data. */
void text_release_system_data(void) {
    heap_unprotect_block(text_system_data);
    heap_free(text_system_data);
    text_system_data = NULL;
}

/* 80033558: Install a loaded font block (protected from release): its header
 * halfwords are read in turn (glyph offset, then the character ranges). */
void text_install_font(u16 *font) {
    u16 *p;
    s32 offset;

    if (font == NULL) {
        heap_set_next_class(0x20);
        return;
    }
    heap_protect_block(font);
    text_font = font;
    text_font_glyphs = (u8 *)font;
    p = font + 1;
    offset = *p++;
    text_font_two_byte_threshold = *p++;
    text_font_two_byte_glyph_offset = *p++;
    text_font_narrow_one_byte_count = *p++;
    text_font_narrow_two_byte_limit = *p++;
    text_font_first_one_byte_code = *p;
    text_font_glyphs = (u8 *)font + offset;
}

/* 800335F4: Install a loaded system data block (protected from release). */
void text_install_system_data(u8 *data) {
    if (data == NULL) {
        heap_set_next_class(0x20);
        return;
    }
    heap_protect_block(data);
    text_system_data = data;
    text_system_resources = (u8 **)data;
    text_relocate_offset_table(data);
    text_system_resources++;
}

/* 80033668: Install a font block and a system data block. */
void text_install_font_and_system_data(u16 *font, u8 *data) {
    text_install_font(font);
    text_install_system_data(data);
}

/* 80033698: Upload the text palette to (x, y) and record its two CLUTs. */
void text_load_palette(s16 x, s16 y) {
    RECT rect;

    rect.w = 32;
    rect.x = x;
    rect.y = y;
    rect.h = 1;
    LoadImage(&rect, (u_long *)text_palette);
    text_plane0_clut = GetClut(x, y);
    text_plane1_clut = GetClut(x + 16, y);
}

/* 80033728: Entry `index` of a resource whose u16 offsets start at byte 4. */
u8 *text_get_resource_entry(u8 *resource, s32 index) {
    return resource + ((u16 *)resource)[index + 2];
}

/* 8003373C: First byte of entry `index` in a table of byte pairs after a header of
 * (count + 3) halfwords. */
u8 text_get_message_columns(u16 *table, s32 index) {
    u8 *entries = (u8 *)table;
    entries += *table * 2 + 6;
    entries += index * 2;
    return entries[0];
}

/* 80033760: Its second byte. */
u8 text_get_message_rows(u16 *table, s32 index) {
    u8 *entries = (u8 *)table;
    entries += *table * 2 + 6;
    entries += index * 2;
    return entries[1];
}

/* 80033784: Entry `index` of a resource table of the system data (8003373c's
 * form): of table `table`, or of the fixed table each of these names. */
u8 *text_get_system_resource_entry(s32 table, s32 index) {
    return text_get_resource_entry(text_system_resources[table], index);
}

/* 800337B8 */
u8 *text_get_resource16_entry(s32 index) {
    return text_get_resource_entry(text_system_resources[16], index);
}

/* 800337E8 */
u8 *text_get_accessory_name(s32 index) {
    return text_get_resource_entry(text_system_resources[17], index);
}

/* 80033818 */
u8 *text_get_item_name(s32 index) {
    return text_get_resource_entry(text_system_resources[22], index);
}

/* 80033848 */
u8 *text_get_weapon_name(s32 index) {
    return text_get_resource_entry(text_system_resources[23], index);
}

/* 80033878 */
u8 *text_get_resource24_entry(s32 index) {
    return text_get_resource_entry(text_system_resources[24], index);
}

/* 800338A8 */
u8 *text_get_resource25_entry(s32 index) {
    return text_get_resource_entry(text_system_resources[25], index);
}

/* 800338D8 */
u8 *text_get_battle_message(s32 index) {
    return text_get_resource_entry(text_system_resources[18], index);
}

/* 80033908 */
u8 *text_get_character_art_name(s32 index) {
    return text_get_resource_entry(text_system_resources[20], index);
}

/* 80033938 */
u8 *text_get_resource19_entry(s32 index) {
    return text_get_resource_entry(text_system_resources[19], index);
}

/* 80033968 */
u8 *text_get_resource21_entry(s32 index) {
    return text_get_resource_entry(text_system_resources[21], index);
}

/* 80033998 */
u8 *text_get_resource27_entry(s32 index) {
    return text_get_resource_entry(text_system_resources[27], index);
}

/* 800339C8 */
u8 *text_get_gear_resource_entry(s32 table, s32 index) {
    return text_get_resource_entry(text_system_resources[table + 28], index);
}

/* 800339FC */
u8 *text_get_gear_art_name(s32 index) {
    return text_get_resource_entry(text_system_resources[48], index);
}

/* 80033A2C */
u8 *text_get_gear_accessory_name(s32 index) {
    return text_get_resource_entry(text_system_resources[50], index);
}

/* 80033A5C */
u8 *text_get_gear_part_name(s32 index) {
    return text_get_resource_entry(text_system_resources[51], index);
}

/* 80033A8C */
u8 *text_get_gear_fuel_art_name(s32 index) {
    return text_get_resource_entry(text_system_resources[52], index);
}

/* 80033ABC: Decode 0xFFFF-terminated character codes into text bytes (text_decoded_buffer). */
void text_decode_codes_to_buffer(u16 *codes) {
    u8 *out = text_decoded_buffer;
    CharPair *pairs = (CharPair *)text_system_resources[27];
    CharPair *pair;
    u16 code;

    for (code = *codes; code != 0xFFFF; code = *codes) {
        pair = (CharPair *)(code * 2 + (u32)pairs);
        codes++;
        if (pair->first != 0) {
            *out++ = pair->first;
            *out++ = pair->second;
        } else {
            *out++ = pair->second;
        }
    }
    *out = 0;
}

/* 80033B34: Decode `count` character codes into text bytes at `out`. */
void text_decode_codes(u16 *codes, u8 *out, u32 count) {
    CharPair *pairs = (CharPair *)text_system_resources[27];
    CharPair *pair;

    while (count--) {
        pair = (CharPair *)(*codes * 2 + (u32)pairs);
        codes++;
        if (pair->first != 0) {
            *out++ = pair->first;
            *out++ = pair->second;
        } else {
            *out++ = pair->second;
        }
    }
    *out = 0;
}

/* 80033BAC: Character code of a byte pair, or 0x8000 when there is none. */
s32 text_find_char_code(u8 first, u8 second) {
    CharPair *pairs = (CharPair *)text_system_resources[27];
    CharPair *pair;
    s16 code;

    for (code = 0; code < 0x144; code++) {
        pair = (CharPair *)(code * 2 + (u32)pairs);
        if (pair->first == first && pair->second == second) {
            return code;
        }
    }
    return 0x8000;
}

/* 80033C20: Encode text into character codes. Returns -1 for a byte pair with no
 * code, else 0. */
s32 text_encode(u8 *text, u16 *codes) {
    u8 c;
    u8 first;
    u8 second;

    while ((c = *text++) != 0) {
        first = 0;
        if (c < text_font_two_byte_threshold) {
            second = c;
        } else {
            first = c;
            second = *text++;
        }
        *codes = text_find_char_code(first, second);
        if (*codes++ == 0x8000) {
            return -1;
        }
    }
    return 0;
}

/* 80033CD0: A window's byte 0x6b while its flag 8 is set, else 0. */
u8 window_get_wait_state(u8 *window) {
    return (*(u16 *)(window + 0x10) & 8) ? window[0x6B] : 0;
}

/* 80033CF0: Decode `value` as ten decimal digit codes in palette `color` (with a
 * sign code when `sign` is set) into text; leading zeros are dropped for
 * plain palettes. The leading-zero scan tests its end first in an
 * unrotated loop, as the original does. */
void text_format_number(u32 value, s32 color, s32 sign) {
    u32 divisor = 1000000000;
    u32 remaining = value;
    u16 *p;
    s32 i;

    color <<= 4;
    if (sign != 0) {
        sign = 11;
        if ((s32)remaining < 0) {
            remaining = -remaining;
            sign = 10;
        }
    }
    for (i = 0; i < 10; i++) {
        text_number_codes[i + 1] = remaining / divisor + color;
        remaining %= divisor;
        divisor /= 10;
    }
    text_number_codes[11] = 0xFFFF;
    p = text_number_codes;
    text_number_codes[0] = color;
    if ((color & 0xFFF0) == color) {
        while (1) {
            if (p == &text_number_codes[10]) {
                break;
            }
            if (*++p != color) {
                break;
            }
        }
    }
    if (sign != 0) {
        *--p = sign + color;
    }
    text_decode_codes_to_buffer(p);
}

/* 80033DD4: Insert `text` into a window's message: it continues there and returns
 * to the current position afterwards (flag 0x80). */
void window_insert_text(Window *window, u8 *text) {
    u8 *previous = window->text;

    window->text = text;
    window->resume = previous;
    window->flags |= 0x80;
}

/* 80033DF0: Reveal a window's next text bytes. Line images alternate between two glyph
 * planes; text controls pause, change reveal speed, insert resource/name/number
 * text and return to the byte after an inserted message's saved position.
 * Pointer increments below retain the control stream's original resume slots.
 * The control parameter reuses `first`. Resource controls resolve their
 * entries before sharing the text insertion and budget update. */
void window_reveal_text(Window *window) {
    s32 remaining = window->unk69;
    s32 line_slot;
    s32 current_line;
    u16 first;
    u16 second;
    u16 glyph_width;
    u8 byte;
    u8 old_speed;
    s32 text_bytes;
    s32 category;
    s32 palette;
    s32 sign;
    u8 *resource;
    u8 *cursor;
    s32 index;
    RECT unused; /* unused in the original; reserves 8 bytes */

    if (window->x > window->width) {
        window->x = 0;
        window->y++;
        window->unk18++;
        if (window->y >= window->lines) {
            window->y = 0;
            window->flags |= 1;
        }
        if (window->flags & 1) {
            window->layout[window->unk16].width = 0;
            if (++window->unk16 >= window->lines) {
                window->unk16 = 0;
            }
        }
        current_line = window->y;
        line_slot = window->unk18 % (window->lines + 1);
        window->layout[current_line].row = (line_slot / 2) * 13 + window->unkE;
        window->layout[current_line].clut = !(line_slot & 1) ? text_plane0_clut : text_plane1_clut;
        window->layout[current_line].plane = line_slot & 1;
        window->layout[current_line].slot = line_slot;
        window->layout[current_line].rect.y = window->unkE + (line_slot / 2) * 13;
    }
    remaining--;
    if (window->unk6C != 0) {
        window->unk6C = 0;
        window->flags &= ~4;
        return;
    }
    while (remaining != -1) {
        byte = *window->text;
        first = byte;
        /* 00 end(), 1 byte: end of text: return after an inserted text's
         * control; else state 1 (unk6B), flag 8 (wait) and unk6C, so once the
         * wait ends the window moves on to its next queued text. The pointer
         * stays on the 00. */
        if (first == 0) {
            if (window->flags & 0x80) {
                window->flags &= ~0x80;
                window->text = window->resume + 1;
                goto next_byte;
            }
            window->flags |= 8;
            window->unk6B = 1;
            window->unk6C = 1;
            return;
        }
        /* 03 pause(), 1 byte: state 3, flag 8: wait, keeping the lines. */
        if (first == 3) {
            window->unk6B = 3;
            window->flags |= 8;
            window->text++;
            return;
        }
        /* 0F nn: sub-code nn through the jump table at 80018a7c (cases 0-15); 16
         * and up match no case and leave the pointer on the 0F. */
        if (first == 15) {
            switch (window->text[1]) {
            /* 0F 00 wait(frames), 3 bytes: wait `frames` (unk84) and end this
             * step. */
            case 0:
                window->unk84 = window->text[2];
                window->text += 3;
                return;
            /* 0F 01 speed(speed), 3 bytes: glyphs per step = speed, saving the
             * old speed; 0 restores the saved speed. */
            case 1:
                first = window->text[2];
                if (first != 0) {
                    remaining += first;
                    window->unk6A = window->unk68;
                    window->unk68 = first;
                    window->unk69 = first;
                } else {
                    window->unk68 = window->unk6A;
                    window->unk69 = window->unk6A;
                    window->unk6A = 0;
                }
                window->text += 3;
                break;
            /* 0F 02 wait_done(frames), 3 bytes: wait `frames`; the following
             * step only clears window flag 4 (unk6C), so the window takes its
             * next queued text. */
            case 2:
                window->unk84 = window->text[2];
                window->text += 3;
                window->unk6C = 1;
                return;
            /* 0F 03 insert(resource, entry), 4 bytes: insert entry `entry` of
             * system resource `resource`. */
            case 3:
                first = window->text[2];
                second = window->text[3];
                window->text += 3;
                resource = text_system_resources[first];
                remaining++;
                resource = text_get_resource_entry(resource, second);
                goto resource_ready;
            /* 0F 04 insert_selection(), 2 bytes: insert entry selection & 0xFF
             * of resource 22, 23, 17, 51 or 50 for selection kinds 0x000-0x400
             * (another kind: the 04 is read as text). */
            case 4:
                cursor = window->text;
                cursor++;
                category = window->selection;
                second = category;
                category &= 0xFF00;
                window->text = cursor;
                switch (category) {
                case 0x000:
                    resource = text_system_resources[22];
                    second &= 0xFF;
                    resource = text_get_resource_entry(resource, second);
                    goto resource_ready;
                case 0x100:
                    resource = text_system_resources[23];
                    second &= 0xFF;
                    resource = text_get_resource_entry(resource, second);
                    goto resource_ready;
                case 0x200:
                    resource = text_system_resources[17];
                    second &= 0xFF;
                    resource = text_get_resource_entry(resource, second);
                    goto resource_ready;
                case 0x300:
                    resource = text_system_resources[51];
                    second &= 0xFF;
                    resource = text_get_resource_entry(resource, second);
                    goto resource_ready;
                case 0x400:
                    resource = text_system_resources[50];
                    second &= 0xFF;
                    resource = text_get_resource_entry(resource, second);
                    goto resource_ready;
                }
                break;
            /* 0F 05 insert_name(name), 3 bytes: insert character name `name`
             * (0x80 and up: the party member's; slot 0xFF: resource 26 entry 0). */
            case 5:
                first = window->text[2];
                window->text += 2;
                index = first;
                if (first >= 0x80) {
                    index = game_data.party[first - 0x80];
                    if (index == 0xFF) {
                        window_insert_text(window, text_get_resource_entry(text_system_resources[26], 0));
                    } else {
                        window_insert_text(window, game_data.names[index]);
                    }
                } else {
                    window_insert_text(window, game_data.names[index]);
                }
                remaining++;
                break;
            /* 0F 06 insert_23(entry), 3 bytes: insert entry `entry` of system
             * resource 23. */
            case 6:
                remaining++;
                first = window->text[2];
                window->text += 2;
                resource = text_system_resources[23];
                resource = text_get_resource_entry(resource, first);
                goto resource_ready;
            /* 0F 07 insert_24(entry), 3 bytes: insert entry `entry` of system
             * resource 24. */
            case 7:
                remaining++;
                first = window->text[2];
                window->text += 2;
                resource = text_system_resources[24];
                resource = text_get_resource_entry(resource, first);
                goto resource_ready;
            /* 0F 08 insert_25(entry), 3 bytes: insert entry `entry` of system
             * resource 25. */
            case 8:
                remaining++;
                first = window->text[2];
                window->text += 2;
                resource = text_system_resources[25];
                resource = text_get_resource_entry(resource, first);
                goto resource_ready;
            /* 0F 09 insert_number(value), 3 bytes: insert window value `value`
             * in decimal, palette 0. */
            case 9:
                palette = 0;
                sign = 0;
                cursor = window->text;
                goto insert_number;
            /* 0F 0A insert_number_1(value), 3 bytes: insert window value `value`
             * in decimal, palette 1. */
            case 10:
                palette = 1;
                cursor = window->text;
                sign = 0;
                goto insert_number;
            /* 0F 0B set_6d(value), 2 bytes: window byte 0x6D = value; the
             * pointer stops on the operand, which is read again as text. */
            case 11:
                window->unk6D = window->text[2];
                window->text += 2;
                break;
            /* 0F 0C insert_signed(value), 3 bytes: insert window value `value`
             * as signed decimal, palette 1. */
            case 12:
                palette = 1;
                cursor = window->text;
                sign = 1;
insert_number:
                first = cursor[2];
                cursor += 2;
                window->text = cursor;
                remaining++;
                text_format_number(window->values[first], palette, sign);
                window_insert_text(window, text_decoded_buffer);
                break;
            /* 0F 0D wait_done_skippable(frames), 3 bytes: as wait_done, also
             * setting window flag 0x200 (800345e0 then drops the wait and the
             * pending clear). */
            case 13:
                window->unk84 = window->text[2];
                window->text += 3;
                window->unk6C = 1;
                window->flags |= 0x200;
                return;
            /* 0F 0E slow(frames), 3 bytes: one glyph per step every `frames`
             * frames (unk86/unk88), saving the speed. */
            case 14:
                old_speed = window->unk68;
                window->unk68 = 1;
                window->unk69 = 1;
                window->unk6A = old_speed;
                window->unk86 = window->unk88 = window->text[2];
                window->text += 3;
                return;
            /* 0F 0F insert_button(action), 3 bytes: insert the name of the
             * button assigned to `action` (pad_button_assignment) from resource 49. */
            case 15:
                first = window->text[2];
                window->text += 2;
                resource = text_system_resources[49];
                second = pad_button_assignment[first];
                remaining++;
                resource = text_get_resource_entry(resource, second);
resource_ready:
                remaining--;
                window_insert_text(window, resource);
                goto check_budget;
            }
        } else if (first == 2) {
            /* 02 page(), 1 byte: state 2, flags 0x48: wait, then clear the
             * window; a following 01 is skipped. */
            window->unk6B = 2;
            window->flags |= 0x48;
            window->text++;
            if (*window->text == 1) {
                window->text++;
            }
            return;
        } else if (first == 1) {
            /* 01 newline(), 1 byte: x = 100 and end this step, so the next step
             * starts a new line. */
            window->x = 100;
            window->text++;
            return;
        } else {
            /* A glyph: one byte below text_font_two_byte_threshold (the font's two-byte
             * threshold), else that byte and the next. */
            text_bytes = 1;
            if (first < text_font_two_byte_threshold) {
                first = 0;
                second = byte;
            } else {
                second = window->text[1];
                text_bytes = 2;
            }
            glyph_width = text_get_glyph_width(first, second);
            if (window->x + glyph_width > window->width) {
                window->x += glyph_width;
                return;
            }
            text_draw_glyph(first, second, (u16 *)window->image + window->x,
                         window->stride, window->layout[window->y].plane);
            window->text = (u8 *)(text_bytes + (u32)window->text);
            window->x += glyph_width;
            window->layout[window->y].width = window->x;
        }
next_byte:
        remaining--;
check_budget:
        ;
    }
}

/* 800345E0: Clear flag 8; a window with flag 0x200 also drops its pending state. */
void window_end_wait(Window *window) {
    u16 flags = window->flags;

    window->flags = flags & ~8;
    if (flags & 0x200) {
        window->unk84 = 0;
        window->unk6C = 0;
        window->flags &= ~0x200;
    }
}

/* 80034614: Unless it is busy, reset a window to flag 2 only. */
void window_reset_if_idle(Window *window) {
    if (window->unk84 == 0) {
        window->unk6C = 0;
        window->flags &= 2;
    }
}

/* 8003463C: Unless it is busy, release a window's queued messages. */
void window_release_queue_if_idle(Window *window) {
    WindowQueue *entry;
    WindowQueue *current;

    if (window->unk84 == 0) {
        entry = window->queue;
        while (entry != NULL) {
            current = entry;
            entry = entry->next;
            heap_free(current);
        }
        window->queue = NULL;
        window->queued = 0;
    }
}

/* 800346A4: Reset a window and release its queue. */
void window_reset(Window *window) {
    window->unk6C = 0;
    window->unk84 = 0;
    window->flags &= 2;
    window_release_queue_if_idle(window);
}

/* 800346D4: Close a window: reset it and release its layout and image. */
void window_close(Window *window) {
    window_reset(window);
    heap_free(window->layout);
    heap_free(window->image);
}

/* 80034714: Queue `message` after the window's current one. Returns the queue
 * length. */
s16 window_queue_message(Window *window, s32 message) {
    WindowQueue *last = window->queue;
    WindowQueue *entry;

    window->queued++;
    heap_set_next_class(0x2A);
    entry = heap_alloc(sizeof(WindowQueue), 2);
    entry->message = message;
    entry->next = NULL;
    if (last == NULL) {
        window->queue = entry;
        return window->queued;
    }
    while (last->next != NULL) {
        last = last->next;
    }
    last->next = entry;
    return window->queued;
}

/* 800347AC: Screen x of the cursor. */
s32 window_get_cursor_x(Window *window) {
    return window->unk4 + window->x * 4;
}

/* 800347C0: Image row of the cursor line (wrapping to the last line). */
s32 window_get_cursor_line_y(Window *window) {
    s32 row = window->y - window->unk16;

    if (row < 0) {
        row = window->lines - 1;
    }
    return window->unk6 + row * window->unk14;
}

/* 80034800: Set the colour of every line's sprites. */
void window_set_color(Window *window, u8 r, u8 g, u8 b) {
    WindowLine *line;
    s32 i;

    for (i = 0; i < window->lines; i++) {
        line = &window->layout[i];
        line->sprite[0][0].r0 = line->sprite[0][1].r0 = line->sprite[1][0].r0 = line->sprite[1][1].r0 = r;
        line->sprite[0][0].g0 = line->sprite[0][1].g0 = line->sprite[1][0].g0 = line->sprite[1][1].g0 = g;
        line->sprite[0][0].b0 = line->sprite[0][1].b0 = line->sprite[1][0].b0 = line->sprite[1][1].b0 = b;
    }
}

/* 80034874: Highlight line `value` of a window (drawn unshaded); 8003487c clears it. */
void window_highlight_line(Window *window, u8 value) {
    window->unk6E = value;
}

/* 8003487C */
void window_clear_highlight(Window *window) {
    window->unk6E = 0xFF;
}

/* 80034888: Draw a window into `ot` for draw buffer `buffer`: start its next queued
 * message when the current one is done, link each line's two sprites
 * (from the first shown line, the highlighted line lit), reveal the next
 * glyphs when the wait is over and link its background. */
void window_draw_frame(Window *window, u_long *ot, s32 buffer) {
    WindowQueue *entry;
    s32 i;
    s32 line;

    if (!(window->flags & 4)) {
        if (window->queued == 0) {
            return;
        }
        entry = window->queue;
        window->text = (u8 *)entry->message;
        window->queue = window->queue->next;
        heap_free(entry);
        window->queued--;
        window->flags = (window->flags & 2) | 0x24;
        if (window->unk6A != 0) {
            window->unk68 = window->unk6A;
            window->unk69 = window->unk6A;
            window->unk6A = 0;
        }
        window->unk88 = 0;
        window->unk86 = 0;
        window->unk69 = window->unk68;
    }
    if (window->flags & 0x100) {
        window->unk69 = window->unk68 * 3;
    } else {
        window->unk69 = window->unk68;
    }
    if (window->flags & 0x40) {
        if (!(window->flags & 8)) {
            window->flags = (window->flags & ~0x40) | 0x20;
        }
    }
    if (window->flags & 0x20) {
        window->unk16 = 0;
        window->unk18 = 0;
        window->y = 0;
        window->x = 0;
        window->layout[0].row = window->unkE;
        window->layout[0].clut = text_plane0_clut;
        window->layout[0].plane = 0;
        window->layout[0].rect.y = window->unkE;
        for (line = 0; line < window->lines; line++) {
            window->layout[line].width = 0;
        }
        window->flags &= ~0x21;
    }

    line = window->unk16;
    for (i = 0; i < window->lines; line++, i++) {
        if (line >= window->lines) {
            line = 0;
        }
        setShadeTex(&window->layout[line].sprite[buffer][1], window->unk6E != i);
        if (window->layout[line].width > 0x40) {
            window->layout[line].sprite[buffer][1].v0 = window->layout[line].row;
            window->layout[line].sprite[buffer][1].clut = window->layout[line].clut;
            window->layout[line].sprite[buffer][1].y0 = window->unk6 + window->unk14 * i;
            window->layout[line].sprite[buffer][1].w = (window->layout[line].width - 0x40) * 4;
            gpu_ot_link_sprt(ot, &window->layout[line].sprite[buffer][1]);
        }
    }
    AddPrim(ot, window->unk3C);
    line = window->unk16;
    for (i = 0; i < window->lines; line++, i++) {
        if (line >= window->lines) {
            line = 0;
        }
        setShadeTex(&window->layout[line].sprite[buffer][0], window->unk6E != i);
        if (window->layout[line].width != 0) {
            window->layout[line].sprite[buffer][0].v0 = window->layout[line].row;
            window->layout[line].sprite[buffer][0].clut = window->layout[line].clut;
            window->layout[line].sprite[buffer][0].y0 = window->unk6 + window->unk14 * i;
            if (window->layout[line].width > 0x40) {
                window->layout[line].sprite[buffer][0].w = 0x100;
            } else {
                window->layout[line].sprite[buffer][0].w = window->layout[line].width * 4;
            }
            gpu_ot_link_sprt(ot, &window->layout[line].sprite[buffer][0]);
        }
    }

    if (window->unk84 != 0) {
        window->unk84--;
    } else if (window->unk86 != 0) {
        window->unk86--;
    } else {
        window->unk86 = window->unk88;
        if (!(window->flags & 0x58)) {
            window_reveal_text(window);
            LoadImage(&window->layout[window->y].rect, window->image);
        }
    }
    if (window->unk84 != 0) {
        if (--window->unk84 == -1) {
            window->flags &= ~0x10;
        }
    }
    if (!(window->flags & 2)) {
        *(u32 *)&window->tile[buffer].x0 = (window->unk4 - 7) | ((window->unk6 - 5) << 16);
        *(u32 *)&window->tile[buffer].w = ((s16)(window->width | 1) * 4 + 0xD) |
                                          ((window->lines * window->unk14 + 10) << 16);
        addPrim(ot, &window->tile[buffer]);
    }
    window->flags &= ~0x100;
    AddPrim(ot, window->unk30);
}

/* 80034EAC: Lay out one line of `text` into `image` in the layout window, `width`
 * made odd. Returns the laid-out width in pixels. */
s32 window_render_text_line(u8 *text, void *image, s16 width, s32 flags) {
    Window *window = &window_single_line_window;

    window->width = width;
    width |= 1;
    window->lines = 1;
    window->width = width;
    window->unk8 = width * 4;
    window->stride = width + 3;
    window->text = text;
    window->unk68 = 1;
    window->unk84 = 0;
    window->unk6C = 0;
    window->unk6A = 0;
    window->image = image;
    window->flags = 0;
    window->y = 0;
    window->x = 0;
    window->unk69 = 100;
    window->layout = &window_single_line_layout;
    window_single_line_layout.width = 0;
    window_single_line_layout.plane = flags & 1;
    window_reveal_text(window);
    return window->layout->width * 4;
}

/* 80034F98: Draw class of a character: 2 for a narrow glyph, else 3. */
s32 text_get_glyph_width(u16 first, u16 second) {
    if (first == 0) {
        if ((s32)((u32)second - (u32)text_font_first_one_byte_code) < text_font_narrow_one_byte_count) {
            return 2;
        }
        return 3;
    }
    if (first == text_font_two_byte_threshold && second < text_font_narrow_two_byte_limit) {
        return 2;
    }
    return 3;
}

/* Expand the font's twelve bits per row into the window's packed 4-bit
 * pixels. Each glyph has eleven rows; its three-row stencil also sets the
 * neighbouring pixels. The two glyph planes occupy opposite two-bit pairs
 * in each nibble, so clearing/drawing one preserves the other.
 *
 * Each row ORs an outline word into the rows above and below and the drawn
 * pixels with their outline into its own row. `carry` is the outline that a
 * column's first pixel and the previous column's last two pixels spread
 * into the column. `first` arrives as a full word; each use takes its low
 * half. */
#define DRAW_GLYPH_PLANE(keep, shift) do { \
    image[0] &= keep; \
    image[1] &= keep; \
    image[2] &= keep; \
    image += stride; \
    image[0] &= keep; \
    image[1] &= keep; \
    image[2] &= keep; \
    do { \
        u16 *upper = &image[-stride]; \
        u16 *lower = &image[stride]; \
        lower[0] &= keep; \
        lower[1] &= keep; \
        lower[2] &= keep; \
        bits = *glyph++; \
        edge = -((bits & 0x80) != 0) & (0x222 << shift); \
        if (bits & 0x40) edge |= 0x2220 << shift; \
        if (bits & 0x20) edge |= 0x2200 << shift; \
        if (!(bits & 0x10)) spread = edge; \
        else spread = edge | (0x2000 << shift); \
        upper[0] |= spread; \
        centre = -((bits & 0x80) != 0) & (0x212 << shift); \
        lower[0] |= spread; \
        if (bits & 0x40) centre |= 0x2120 << shift; \
        edge = centre; \
        if (bits & 0x20) edge |= 0x1200 << shift; \
        previous = image[0]; \
        if (bits & 0x10) image[0] = previous | (0x2000 << shift) | edge; \
        else image[0] = previous | edge; \
        if (!(bits & 8)) { \
            if (!(bits & 0x10)) carry = (bits >> (4 - shift)) & (2 << shift); \
            else carry = 0x22 << shift; \
        } else carry = 0x222 << shift; \
        edge = carry; \
        if (bits & 4) edge |= 0x2220 << shift; \
        if (bits & 2) edge |= 0x2200 << shift; \
        if (!(bits & 1)) spread = edge; \
        else spread = edge | (0x2000 << shift); \
        upper[1] |= spread; \
        lower[1] |= spread; \
        edge = (bits >> (4 - shift)) & (2 << shift); \
        if (bits & 0x10) edge |= 0x21 << shift; \
        if (bits & 8) edge |= 0x212 << shift; \
        if (bits & 4) edge |= 0x2120 << shift; \
        if (bits & 2) edge |= 0x1200 << shift; \
        previous = image[1]; \
        if (bits & 1) image[1] = previous | (0x2000 << shift) | edge; \
        else image[1] = previous | edge; \
        if (!(bits & 0x8000)) { \
            if (!(bits & 1)) carry = (bits << shift) & (2 << shift); \
            else carry = 0x22 << shift; \
        } else carry = 0x222 << shift; \
        edge = carry; \
        if (bits & 0x4000) edge |= 0x2220 << shift; \
        if (bits & 0x2000) edge |= 0x2200 << shift; \
        if (!(bits & 0x1000)) spread = edge; \
        else spread = edge | (0x2000 << shift); \
        upper[2] |= spread; \
        lower[2] |= spread; \
        edge = (bits << shift) & (2 << shift); \
        if (bits & 1) edge |= 0x21 << shift; \
        if (bits & 0x8000) edge |= 0x212 << shift; \
        if (bits & 0x4000) edge |= 0x2120 << shift; \
        if (bits & 0x2000) edge |= 0x1200 << shift; \
        previous = image[2]; \
        if (bits & 0x1000) image[2] = previous | (0x2000 << shift) | edge; \
        else image[2] = previous | edge; \
        image += stride; \
        row++; \
    } while (row < 11); \
} while (0)

/* 80034FFC: Draw the glyph of a character (a one-byte code when `first` is 0, the
 * special glyph for 0xff 0xff) into glyph plane `plane` of the line image at
 * `image`, `stride` halfwords per row, with its outline. */
void text_draw_glyph(s32 first, u16 second, u16 *image, s16 stride, s32 plane) {
    u16 *glyph;
    u16 bits;
    u16 previous;
    s32 edge;
    s32 spread;
    s32 centre;
    s32 carry;
    s32 row;

    if ((u16)first == 0) {
        glyph = (u16 *)(text_font_glyphs + (second - text_font_first_one_byte_code) * 22);
    } else if ((u16)first == 0xFF && second == 0xFF) {
        glyph = text_special_glyph_rows;
    } else {
        glyph = (u16 *)(text_font_glyphs + second * 22 + text_font_two_byte_glyph_offset +
                       ((u16)first - text_font_two_byte_threshold) * 0x1600);
    }
    row = 0;
    if (plane == 0) {
        DRAW_GLYPH_PLANE(0xCCCC, 0);
    } else {
        DRAW_GLYPH_PLANE(0x3333, 2);
    }
}

#undef DRAW_GLYPH_PLANE


/* 8003569C: Buttons held on controller `port` (active high), or 0 without a digital
 * or analog pad. */
s32 pad_read_buttons(s32 port) {
    PadBuffer *pad = &pad_receive_buffers[port];

    pad_last_read_type = 0;
    if (pad->status != 0) {
        return 0;
    }
    pad_last_read_type = pad->type & 0xF0;
    if (pad_last_read_type == 0x40 || pad_last_read_type == 0x50 || pad_last_read_type == 0x70) {
        return (u8)~pad->buttons[1] | ((pad->buttons[0] << 8) ^ 0xFF00);
    }
    return 0;
}

/* 80035734: Kind of controller on `port`: 0 none, 1 digital, 2 mouse, 3 analog stick,
 * 4 analog pad, -1 other. */
s32 pad_get_controller_kind(s32 port) {
    if (pad_receive_buffers[port].status == 0xFF) {
        return 0;
    }
    switch (pad_receive_buffers[port].type & 0xF0) {
    case 0x40:
        return 1;
    case 0x10:
        return 2;
    case 0x50:
        return 3;
    case 0x70:
        return 4;
    }
    return -1;
}

/* 800357C0: Remap the low button byte through the configured assignment. */
s16 pad_remap_buttons(s32 buttons) {
    s32 held = buttons;
    s32 i;

    buttons &= 0xFF00;
    for (i = 0; i < 8; i++) {
        if (held & pad_button_bits[i]) {
            buttons |= pad_button_bits[pad_button_assignment[i]];
        }
    }
    return buttons;
}

/* 8003582C: Swap the shoulder and face button bits between the two layouts. */
s16 pad_swap_button_layout(s32 buttons) {
    s16 result = buttons & ~0x9E;

    if (buttons & 8) {
        result |= 0x10;
    }
    if (buttons & 2) {
        result |= 4;
    }
    if (buttons & 0x10) {
        result |= 8;
    }
    if (buttons & 0x80) {
        result |= 2;
    }
    if (buttons & 4) {
        result |= 0x80;
    }
    return result;
}

/* 80035884: Stick positions (x, then y) of the directional buttons in `buttons`. */
u8 pad_get_dpad_stick_x(s32 buttons) {
    return pad_dpad_stick_x_table[(buttons >> 12) & 0xF];
}

/* 800358A0 */
u8 pad_get_dpad_stick_y(s32 buttons) {
    return pad_dpad_stick_y_table[(buttons >> 12) & 0xF];
}

/* The vertical blank count: the menu declares it volatile, so the shared
 * headers leave it out. */
extern s32 pad_vblank_count;

/* 800358BC: Read both controllers: held buttons (remapped; an analog stick's layout
 * swapped), the stick positions (the directional buttons' on a digital
 * pad), newly pressed buttons and the auto-repeating buttons (the newly
 * pressed ones; once the held buttons have not changed for 32 frames, all
 * held buttons every fourth frame). */
void pad_read_controllers(void) {
    pad_port0_held = pad_read_buttons(0);
    pad_port0_held = pad_remap_buttons((s16)pad_port0_held);
    if (pad_last_read_type != 0) {
        if (pad_last_read_type == 0x50) {
            pad_port0_held = pad_swap_button_layout((s16)pad_port0_held);
            goto analog0;
        }
        if (pad_last_read_type == 0x70) {
        analog0:
            pad_port0_right_stick_x = pad_receive_buffers[0].data[0];
            pad_port0_right_stick_y = pad_receive_buffers[0].data[1];
            pad_port0_left_stick_x = pad_receive_buffers[0].data[2];
            pad_port0_left_stick_y = pad_receive_buffers[0].data[3];
        } else {
            pad_port0_right_stick_y = 0;
            pad_port0_right_stick_x = 0;
            pad_port0_left_stick_x = pad_dpad_stick_x_table[pad_port0_held >> 12];
            pad_port0_left_stick_y = pad_dpad_stick_y_table[pad_port0_held >> 12];
        }
    } else {
        pad_port0_right_stick_y = 0;
        pad_port0_right_stick_x = 0;
        pad_port0_left_stick_y = 0;
        pad_port0_left_stick_x = 0;
    }
    pad_port0_pressed = pad_port0_held ^ pad_port0_last_held;
    pad_port0_pressed &= pad_port0_held;
    pad_port0_last_held = pad_port0_held;
    if (pad_port0_pressed) {
        pad_port0_unchanged_frame_count = 0;
    }
    pad_port0_repeated = pad_port0_held;
    if (pad_port0_unchanged_frame_count < 0x20) {
        pad_port0_unchanged_frame_count++;
        pad_port0_repeated = pad_port0_pressed;
    } else if (pad_vblank_count & 3) {
        pad_port0_repeated = pad_port0_pressed;
    }

    pad_port1_held = pad_read_buttons(1);
    pad_port1_held = pad_remap_buttons((s16)pad_port1_held);
    if (pad_last_read_type != 0) {
        if (pad_last_read_type == 0x50) {
            pad_port1_held = pad_swap_button_layout((s16)pad_port1_held);
            goto analog1;
        }
        if (pad_last_read_type == 0x70) {
        analog1:
            pad_port1_right_stick_x = pad_receive_buffers[1].data[0];
            pad_port1_right_stick_y = pad_receive_buffers[1].data[1];
            pad_port1_left_stick_x = pad_receive_buffers[1].data[2];
            pad_port1_left_stick_y = pad_receive_buffers[1].data[3];
        } else {
            pad_port1_right_stick_y = 0;
            pad_port1_right_stick_x = 0;
            pad_port1_left_stick_x = pad_dpad_stick_x_table[pad_port1_held >> 12];
            pad_port1_left_stick_y = pad_dpad_stick_y_table[pad_port1_held >> 12];
        }
    } else {
        pad_port1_right_stick_y = 0;
        pad_port1_right_stick_x = 0;
        pad_port1_left_stick_y = 0;
        pad_port1_left_stick_x = 0;
    }
    pad_port1_pressed = pad_port1_held ^ pad_port1_last_held;
    pad_port1_pressed &= pad_port1_held;
    pad_port1_last_held = pad_port1_held;
    if (pad_port1_pressed) {
        pad_port1_unchanged_frame_count = 0;
    }
    pad_port1_repeated = pad_port1_held;
    if (pad_port1_unchanged_frame_count < 0x20) {
        pad_port1_unchanged_frame_count++;
        pad_port1_repeated = pad_port1_pressed;
    } else if (pad_vblank_count & 3) {
        pad_port1_repeated = pad_port1_pressed;
    }
}

/* 80035C0C: Queue the current controller state (flag an overflow when full). */
void pad_queue_state(void) {
    s32 i;

    if (pad_queue_count < 16) {
        pad_queue_count++;
        i = pad_queue_write_index & 0xF;
        pad_queue_port0_held[i] = pad_port0_held;
        pad_queue_port1_held[i] = pad_port1_held;
        pad_queue_port0_pressed[i] = pad_port0_pressed;
        pad_queue_port1_pressed[i] = pad_port1_pressed;
        pad_queue_port0_repeated[i] = pad_port0_repeated;
        pad_queue_port1_repeated[i] = pad_port1_repeated;
        pad_queue_write_index++;
        return;
    }
    pad_queue_overflowed = 1;
}

/* 80035CDC: Take the oldest queued controller state as the current one. Returns the
 * count before, 0 when empty. */
u32 pad_dequeue_state(void) {
    u32 count = pad_queue_count;
    s32 i;

    if (count == 0) {
        return 0;
    }
    pad_queue_count = count - 1;
    i = pad_queue_read_index & 0xF;
    pad_queue_read_index++;
    pad_port0_held = pad_queue_port0_held[i];
    pad_port1_held = pad_queue_port1_held[i];
    pad_port0_pressed = pad_queue_port0_pressed[i];
    pad_port1_pressed = pad_queue_port1_pressed[i];
    pad_port0_repeated = pad_queue_port0_repeated[i];
    pad_port1_repeated = pad_queue_port1_repeated[i];
    return count;
}

/* 80035DA0: Number of queued controller states. */
u32 pad_get_queue_count(void) {
    return pad_queue_count;
}

/* 80035DB0: Clear the controller queue and states. */
void pad_clear_queue(void) {
    pad_queue_count = 0;
    pad_queue_write_index = 0;
    pad_queue_read_index = 0;
    pad_queue_overflowed = 0;
    pad_unread_reset_word = 1;
    pad_port1_unread_buttons_b = 0;
    pad_port0_unread_buttons_b = 0;
    pad_port1_unread_buttons_a = 0;
    pad_port0_unread_buttons_a = 0;
    pad_port1_unread_buttons_c = 0;
    pad_port0_unread_buttons_c = 0;
    pad_port1_repeated = 0;
    pad_port0_repeated = 0;
    pad_port1_pressed = 0;
    pad_port0_pressed = 0;
    pad_port1_held = 0;
    pad_port0_held = 0;
}

/* 80035E44: Advance the play time by one frame. */
void pad_advance_play_time(void) {
    if (pad_play_time_stopped == 0) {
        if (++pad_play_time_frames == 60) {
            pad_play_time_frames = 0;
            pad_play_time_seconds++;
        }
        if (pad_play_time_seconds == 60) {
            pad_play_time_seconds = 0;
            pad_play_time_minutes++;
        }
        if (pad_play_time_minutes == 60) {
            pad_play_time_minutes = 0;
            pad_play_time_hours++;
        }
        if (pad_play_time_hours == 100) {
            pad_play_time_stopped = 1;
        }
    }
}

/* 80035F1C: Save a VRAM rectangle as a 16-bit TIM file on the PC file server. */
void console_save_vram_as_tim(RECT *rect, char *name) {
    TimHeader header;
    s32 fd;

    StoreImage(rect, (u_long *)0x80700000);
    header.id = 0x10;
    header.flag = 2;
    header.bytes = rect->w * rect->h * 2 + 12;
    header.rect.x = rect->x;
    header.rect.y = rect->y;
    header.rect.w = rect->w;
    header.rect.h = rect->h;
    fd = PCcreat(name, 0);
    PCwrite(fd, (char *)&header, sizeof(header));
    PCwrite(fd, (char *)0x80700000, rect->w * rect->h * 2);
    PCclose(fd);
}

/* 80035FF8: Save a VRAM rectangle as a PPM (P6) image on the PC file server.
 * Returns 0, or -1 when the file cannot be created. */
s32 console_save_vram_as_ppm(RECT *rect, char *name) {
    char header[256];
    u16 *src;
    u8 *dst;
    s32 count;
    s32 i;
    s32 fd;

    StoreImage(rect, (u_long *)0x80600000);
    DrawSync(0);
    sprintf(header, "P6\r%d %d\r255\r", rect->w, rect->h);
    count = rect->w * rect->h;
    src = (u16 *)0x80600000;
    dst = (u8 *)0x80700000;
    i = count;
    while (i--) {
        *dst++ = (*src & 0x1F) << 3;
        *dst++ = (*src >> 2) & 0xF8;
        *dst++ = (*src++ >> 7) & 0xF8;
    }
    fd = PCcreat(name, 0);
    if (fd == -1) {
        return -1;
    }
    PCwrite(fd, header, strlen(header));
    PCwrite(fd, (char *)0x80700000, count * 3);
    PCclose(fd);
    return 0;
}

/* 8003611C: Stop both controllers' actuators and register their data with libpad. */
void pad_init_actuators(void) {
    pad_actuators[0].act[0] = 0;
    pad_actuators[0].timer = 0;
    pad_actuators[0].state = 0;
    pad_actuators[0].disabled = 0;
    pad_actuators[1] = pad_actuators[0];
    libapi_register_pad_send_buffers(pad_actuators[0].act, 4, pad_actuators[1].act, 4);
}

/* 80036188: Step one actuator: run while its timer lasts, then wind down. */
void pad_step_actuator(Actuator *actuator) {
    if (actuator->disabled == 0) {
        if (actuator->timer != 0) {
            actuator->act[0] = 1;
            actuator->act[1] = 0x40;
            actuator->act[2] = 1;
            actuator->act[3] = 0;
            actuator->state = 1;
            actuator->timer--;
        } else if (actuator->state == 1) {
            actuator->act[0] = 1;
            actuator->act[1] = 0x40;
            actuator->act[2] = 0;
            actuator->act[3] = 0;
            actuator->state = 2;
        } else if (actuator->state == 2) {
            actuator->act[0] = 0;
            actuator->state = 0;
        }
    }
}

/* 80036220: Step both controllers' actuators. */
void pad_step_actuators(void) {
    pad_step_actuator(&pad_actuators[0]);
    pad_step_actuator(&pad_actuators[1]);
}

/* 80036258: Run the actuator of `port` for `frames` frames. */
void pad_run_actuator(s32 port, s16 frames) {
    pad_actuators[port].timer = frames;
}

/* 80036270: Disable or enable the actuator of `port`. */
void pad_set_actuator_disabled(s32 port, u8 disabled) {
    pad_actuators[port].disabled = disabled;
}

/* 80036288: Start the controllers and reset the queue, actuators and assignment. */
void pad_start_controllers(void) {
    u8 *entry;
    s32 i;

    InitPAD((char *)&pad_receive_buffers[0], 0x22, (char *)&pad_receive_buffers[1], 0x22);
    StartPAD();
    ChangeClearPAD(0);
    pad_clear_queue();
    pad_init_actuators();
    pad_unread_init_word = 0;
    pad_unread_byte = 1;
    pad_vblank_polls_host = 0;
    for (i = 7, entry = &pad_button_assignment[7]; i >= 0; i--) {
        *entry-- = i;
    }
    pad_button_assignment[0] = 1;
    pad_button_assignment[2] = 3;
    pad_button_assignment[1] = 0;
    pad_button_assignment[3] = 2;
}

/* 8003633C: Set a controller byte that 80036288 sets to 1 (the field clears it); no
 * resident code reads it. */
void pad_set_unread_byte(u8 value) {
    pad_unread_byte = value;
}

/* 8003634C: Vertical-blank callback: counts frames, polls the controllers, input queue
 * and play clock, runs the installed hook, and on a development (host)
 * configuration with the debugger request set traps into the debugger.
 * The frame holds 40 bytes of locals that the code never touches. */
void pad_vblank_callback(void) {
    u8 unused[40];

    pad_vblank_count++;
    pad_read_controllers();
    pad_queue_state();
    pad_advance_play_time();
    pad_step_actuators();
    if (pad_vblank_hook != NULL) {
        pad_vblank_hook();
    }
    if (mode_disc_mode != -1 && pad_vblank_polls_host != 0) {
        pollhost();
    }
}

/* 800363E0: Set whether the vertical-blank callback polls the host and its hook, set
 * the word 80035db0 resets to 1 (no resident code reads it), and read the
 * queue overflow flag. */
void pad_set_host_polling(s32 value) {
    pad_vblank_polls_host = value;
}

/* 800363F0 */
void pad_set_vblank_hook(void (*value)(void)) {
    pad_vblank_hook = value;
}

/* 80036400 */
void pad_set_unread_reset_word(s32 value) {
    pad_unread_reset_word = value;
}

/* 80036410 */
s32 pad_has_queue_overflowed(void) {
    return pad_queue_overflowed;
}

/* 80036420: Merge every queued controller state into the current one (or reset
 * after an overflow). */
void pad_merge_queued_states(void) {
    u16 s0, s1, s2, s3, s4, s5;

    s0 = s1 = s2 = s3 = s4 = s5 = 0;

    if (pad_has_queue_overflowed() != 0) {
        pad_clear_queue();
    } else {
        while (pad_dequeue_state() != 0) {
            s0 |= pad_port0_held;
            s1 |= pad_port1_held;
            s2 |= pad_port0_pressed;
            s3 |= pad_port1_pressed;
            s4 |= pad_port0_repeated;
            s5 |= pad_port1_repeated;
        }
    }
    pad_port0_held = s0;
    pad_port1_held = s1;
    pad_port0_pressed = s2;
    pad_port1_pressed = s3;
    pad_port0_repeated = s4;
    pad_port1_repeated = s5;
}

/* 80036528: Print a controller receive buffer in hex, and a digital pad's buttons. */
void pad_print_buffer(PadBuffer *pad) {
    s32 count = (pad->type & 0xF) * 2 + 2;
    s32 i;

    for (i = 0; i < count; i++) {
        console_printf("%02x ", ((u8 *)pad)[i]);
    }
    console_printf("\n");
    if (pad->status == 0 && (pad->type & 0xF0) == 0x40) {
        console_printf("%04x\n", (~pad->buttons[1] & 0xFF) | ((pad->buttons[0] << 8) ^ 0xFF00));
    }
}

/* 800365FC: Print both controller buffers, the actuator values, the held buttons and
 * every queued pad entry. The final format occupies a full word-aligned
 * slot before the formatter's digit tables. */
void pad_print_state(void) {
    pad_print_buffer(&pad_receive_buffers[0]);
    pad_print_buffer(&pad_receive_buffers[1]);
    console_printf("vect0 %02x %02x\n", pad_port0_left_stick_x, pad_port0_left_stick_y);
    console_printf("vect1 %02x %02x\n", pad_port1_left_stick_x, pad_port1_left_stick_y);
    console_printf("PADD %04x %04x\n", pad_port0_held, pad_port1_held);
    while (pad_dequeue_state() != 0) {
        static const char queued_format[24] = "%04x %04x %04x %04x\n";

        console_printf((char *)queued_format, pad_read_buttons(0), pad_port0_held, pad_port0_pressed, pad_port0_repeated);
    }
}
