// Field overlay 800805f4 and 8008004c: the dialogue windows of a field
// frame. Each window (800c26b0 + 0x498 * w) keeps its own packets per draw
// buffer; drawing fills and links them into the frame's small table. The
// window's text advances glyph by glyph into its image and closes when its
// text ends and input releases it. Offsets below are relative to the window
// record; original addresses name correlations only.
#include "xem/reconstruction/field_text.hpp"
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_heap.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::uint32_t window_base(std::uint32_t w) {
    return field::DialogueWindow::base + w * field::DialogueWindow::stride;
}
[[noreturn]] void missing(std::string_view operation, std::uint32_t address, const char *id,
                          const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}
} // namespace

// Resident 80033cd0: the line a window waits on (+6b) while it waits for
// input (+10 bit 3).
std::uint32_t Program::dialogue_waiting(std::uint32_t window) {
    return (memory(window + 0x10, 2) & 8U) != 0 ? memory(window + 0x6b, 1) : 0U;
}

// Resident 80031798: link a four-word packet at `table`, replacing the
// entry's order byte.
void Program::link_text_packet(std::uint32_t table, std::uint32_t packet) {
    const auto previous = memory(table);
    set_memory(table, packet & 0xffffffU);
    set_memory(packet, previous | 0x04000000U);
}

// Resident 800320e8 of a block the window at `window` holds (dialogue_blocks).
void Program::release_dialogue_block(std::uint32_t window, std::uint32_t address,
                                     std::uint32_t call_site) {
    auto &blocks = field->dialogue_blocks.at((window - field::DialogueWindow::base) /
                                             field::DialogueWindow::stride);
    const auto block = std::ranges::find(blocks, address, &resident::HeapBlock::address);
    if (block == blocks.end())
        throw field::FieldFormatError("Released dialogue block is not held by its window");
    if (resident::heap_release(resident.heap, *block, call_site) == 0)
        blocks.erase(block);
}

// Resident 80034714: queue `text` as a page of the window (+8c list, +82
// pages) in an eight-byte block: the next page, then the text.
void Program::queue_dialogue_page(std::uint32_t window, std::uint32_t text) {
    set_memory(window + 0x82, memory(window + 0x82, 2) + 1, 2);
    const auto list = memory(window + 0x8c);
    resident.heap.allocation_class = 0x2a; // 800324b8
    auto block = resident::heap_allocate(resident.heap, 8, 2, 0x8003474c);
    if (!block)
        missing("queue_dialogue_page", 0x8003474c, "symbol:heap-quiet-null",
                "A quiet null page allocation is not reconstructed");
    const auto node = block->address;
    field->dialogue_blocks
        .at((window - field::DialogueWindow::base) / field::DialogueWindow::stride)
        .push_back(std::move(*block));
    set_memory(node + 4, text);
    set_memory(node, 0);
    if (list == 0) {
        set_memory(window + 0x8c, node);
        return;
    }
    auto last = list;
    while (memory(last) != 0)
        last = memory(last);
    set_memory(last, node);
}

// Resident 80033df0: advance the window's text (+1c) by up to +69 glyphs.
// A full line (cursor +0 past the width +a) first moves to the next line
// (+2, wrapping over the +c lines and scrolling from then on: flag 1, first
// shown line +16) and prepares that line's record. Byte 0 ends the text (or a
// substitution, flag 80, back to +20), 1 starts a new line, 2 and 3 wait for
// input; other codes below the lead limit are one-byte glyphs, others lead a
// two-byte glyph. Glyphs are drawn at the cursor into the window's image (+2c).
void Program::dialogue_glyphs(std::uint32_t window) {
    const auto half = [&](std::uint32_t offset) { return s16(memory(window + offset, 2)); };
    const auto set_half = [&](std::uint32_t offset, std::int32_t value) {
        set_memory(window + offset, u32(value) & 0xffffU, 2);
    };
    const auto flags = [&] { return memory(window + 0x10, 2); };
    const auto line = [&] { return memory(window + 0x28) + u32(half(2)) * 0x60; };
    auto count = static_cast<std::int32_t>(memory(window + 0x69, 1));
    if (half(0xa) < half(0)) {
        set_half(0, 0);
        set_half(2, half(2) + 1);
        set_half(0x18, half(0x18) + 1);
        if (half(2) >= half(0xc)) {
            set_half(2, 0);
            set_memory(window + 0x10, flags() | 1U, 2);
        }
        if ((flags() & 1U) != 0) {
            set_memory(memory(window + 0x28) + u32(half(0x16)) * 0x60 + 0x58, 0, 2);
            set_half(0x16, half(0x16) + 1);
            if (half(0x16) >= half(0xc))
                set_half(0x16, 0);
        }
        const auto lines = half(0xc) + 1;
        if (lines == 0)
            throw field::FieldFormatError(
                "Original MIPS division by zero is not a recovered result");
        const auto slot = half(0x18) % lines; // Signed remainder, as div leaves it.
        const auto row = memory(window + 0xe, 2) + u32(slot / 2 * 13);
        set_memory(line() + 0x5c, row, 1);
        set_memory(line() + 0x5e, resident.text_cluts[(slot & 1) != 0 ? 1 : 0], 2);
        set_memory(line() + 0x5a, u32(slot) & 1U, 1);
        set_memory(line() + 0x5b, u32(slot), 1);
        set_memory(line() + 0x52, row, 2);
    }
    --count;
    if (memory(window + 0x6c, 1) != 0) {
        set_memory(window + 0x6c, 0, 1);
        set_memory(window + 0x10, flags() & 0xfffbU, 2);
        return;
    }
    const field::TextFont font{s32(memory(0x8005934c)), memory(0x80059350), s32(memory(0x80059354)),
                               s32(memory(0x80059358)), memory(0x8005935c), memory(0x80059364)};
    for (; count != -1; --count) {
        const auto text = memory(window + 0x1c);
        const auto code = memory(text, 1);
        switch (code) {
        case 0:
            if ((flags() & 0x80U) == 0) {
                set_memory(window + 0x10, flags() | 8U, 2);
                set_memory(window + 0x6b, 1, 1);
                set_memory(window + 0x6c, 1, 1);
                return;
            }
            set_memory(window + 0x10, flags() & 0xff7fU, 2);
            set_memory(window + 0x1c, memory(window + 0x20) + 1);
            continue;
        case 1:
            set_half(0, 100);
            set_memory(window + 0x1c, text + 1);
            return;
        case 2:
            set_memory(window + 0x6b, 2, 1);
            set_memory(window + 0x10, flags() | 0x48U, 2);
            set_memory(window + 0x1c, memory(text + 1, 1) == 1 ? text + 2 : text + 1);
            return;
        case 3:
            set_memory(window + 0x6b, 3, 1);
            set_memory(window + 0x10, flags() | 8U, 2);
            set_memory(window + 0x1c, text + 1);
            return;
        case 0xf:
            missing("dialogue_glyphs", 0x80034090, "symbol:dialogue-text-control",
                    "Text control codes (byte 0f) are not recovered");
        default:
            break;
        }
        const bool single = static_cast<std::int32_t>(code) < font.lead_limit;
        const auto first = static_cast<std::uint16_t>(single ? 0 : code);
        const auto second = static_cast<std::uint16_t>(single ? code : memory(text + 1, 1));
        const auto width = field::glyph_width(font, first, second);
        const auto cursor = half(0);
        if (half(0xa) < cursor + s32(width)) {
            set_half(0, cursor + s32(width));
            return;
        }
        // 80034ffc: the glyph at the cursor, in the line's plane (+5a).
        const auto glyph = field::glyph_address(font, first, second);
        if (glyph == field::special_glyph)
            missing("dialogue_glyphs", 0x80035058, "symbol:dialogue-special-glyph",
                    "The resident special glyph (ff ff) is not owned");
        std::array<std::uint16_t, field::glyph_rows> rows{};
        for (std::uint32_t i = 0; i < field::glyph_rows; ++i)
            rows[i] = static_cast<std::uint16_t>(memory(glyph + i * 2, 2));
        const bool odd = memory(line() + 0x5a, 1) != 0;
        const auto cells = field::glyph_cells(rows, odd);
        const auto image = memory(window + 0x2c) + u32(cursor) * 2;
        const auto stride = u32(half(0x12)) * 2;
        for (std::uint32_t r = 0; r < cells.size(); ++r)
            for (std::uint32_t c = 0; c < 3; ++c) {
                const auto at = image + r * stride + c * 2;
                set_memory(at, (memory(at, 2) & field::glyph_keep(odd)) | cells[r][c], 2);
            }
        set_memory(window + 0x1c, text + (single ? 1U : 2U));
        set_half(0, cursor + s32(width));
        set_memory(line() + 0x58, u32(half(0)) & 0xffffU, 2);
    }
}

// Resident 80034888: the text lines of a window. Each line record (+28, 0x60
// bytes: two sprites per buffer, then the line's width +58, palette row +5c
// and clut +5e) is linked when it has glyphs; the line at +6e is lit.
void Program::draw_dialogue_text(FrameServices &services, std::uint32_t window, std::uint32_t table,
                                 std::uint32_t buffer) {
    auto flags = [&] { return memory(window + 0x10, 2); };
    if ((flags() & 4U) == 0) {
        if (memory(window + 0x82, 2) == 0)
            return;
        // 800348d8: the next queued page (+8c) becomes the text (+1c).
        const auto page = memory(window + 0x8c);
        set_memory(window + 0x1c, memory(page + 4));
        set_memory(window + 0x8c, memory(page));
        release_dialogue_block(window, page, 0x800348f0);
        set_memory(window + 0x82, memory(window + 0x82, 2) - 1, 2);
        set_memory(window + 0x10, (flags() & 2U) | 0x24U, 2);
        if (const auto speed = memory(window + 0x6a, 1); speed != 0) {
            set_memory(window + 0x68, speed, 1);
            set_memory(window + 0x6a, 0, 1);
        }
        set_memory(window + 0x88, 0, 2);
        set_memory(window + 0x86, 0, 2);
    }
    set_memory(window + 0x69,
               (flags() & 0x100U) != 0 ? memory(window + 0x68, 1) * 3 : memory(window + 0x68, 1),
               1);
    if ((flags() & 0x40U) != 0 && (flags() & 8U) == 0)
        set_memory(window + 0x10, (flags() & 0xffbfU) | 0x20U, 2);
    const auto lines = [&] { return memory(window + 0x28); };
    const auto count = [&] { return s16(memory(window + 0xc, 2)); };
    if ((flags() & 0x20U) != 0) {
        set_memory(window + 0x16, 0, 2);
        set_memory(window + 0x18, 0, 2);
        set_memory(window + 2, 0, 2);
        set_memory(window, 0, 2);
        set_memory(lines() + 0x5c, memory(window + 0xe, 2), 1);
        set_memory(lines() + 0x5e, memory(0x800595d4, 2), 2);
        set_memory(lines() + 0x5a, 0, 1);
        set_memory(lines() + 0x52, memory(window + 0xe, 2), 2);
        for (std::int32_t i = 0; i < count(); ++i)
            set_memory(lines() + u32(i) * 0x60 + 0x58, 0, 2);
        set_memory(window + 0x10, flags() & 0xffdeU, 2);
    }
    const auto packets = buffer * 0x28;
    // Both passes start at the first shown line (+16) and wrap.
    const auto pass = [&](std::uint32_t sprite, bool wide) {
        auto line = s16(memory(window + 0x16, 2));
        for (std::int32_t i = 0; i < count(); ++i, ++line) {
            if (line >= count())
                line = 0;
            const auto record = lines() + u32(line) * 0x60;
            const auto packet = record + packets + sprite;
            const auto code = memory(packet + 7, 1);
            set_memory(packet + 7, memory(window + 0x6e, 1) == u32(i) ? code & 0xfeU : code | 1U,
                       1);
            const auto width = s16(memory(record + 0x58, 2));
            if (wide ? width <= 0x40 : width == 0)
                continue;
            set_memory(packet + 0xd, memory(record + 0x5c, 1), 1);
            set_memory(packet + 0xe, memory(record + 0x5e, 2), 2);
            set_memory(packet + 0xa, memory(window + 6, 2) + u32(s16(memory(window + 0x14, 2)) * i),
                       2);
            set_memory(packet + 0x10,
                       wide           ? u32(width - 0x40) * 4
                       : width > 0x40 ? 0x100U
                                      : u32(width) * 4,
                       2);
            link_text_packet(table, packet);
        }
    };
    pass(0x14, true);
    add_primitive(table, window + 0x3c); // AddPrim (80043b48)
    pass(0, false);
    // Glyph reveal: +84 delays, +86 counts down to the next glyph (+88).
    if (const auto delay = memory(window + 0x84, 2); delay != 0) {
        set_memory(window + 0x84, delay - 1, 2);
    } else if (const auto wait = memory(window + 0x86, 2); wait != 0) {
        set_memory(window + 0x86, wait - 1, 2);
    } else {
        set_memory(window + 0x86, memory(window + 0x88, 2), 2);
        if ((flags() & 0x58U) == 0) {
            dialogue_glyphs(window);
            // LoadImage of the current line's glyph image (rectangle +50).
            load_image_at(services,
                          memory(window + 0x28) + u32(s16(memory(window + 2, 2))) * 0x60 + 0x50,
                          memory(window + 0x2c));
        }
    }
    if (const auto delay = memory(window + 0x84, 2); delay != 0) {
        set_memory(window + 0x84, delay - 1, 2);
        if (s16(delay - 1) == -1)
            set_memory(window + 0x10, flags() & 0xffefU, 2);
    }
    if ((flags() & 2U) == 0) {
        // The window's backing tile (+48 per buffer).
        const auto tile = window + buffer * 0x10;
        set_memory(tile + 0x50, u32(s16(memory(window + 4, 2)) - 7) |
                                    u32(s16(memory(window + 6, 2)) - 5) << 16U);
        const auto columns = u32(s16(memory(window + 0xa, 2) | 1U) * 4 + 0xd);
        set_memory(tile + 0x54, columns | u32(count() * s16(memory(window + 0x14, 2)) + 10) << 16U);
        add_primitive(table, tile + 0x48);
    }
    set_memory(window + 0x10, flags() & 0xfeffU, 2);
    add_primitive(table, window + 0x30);
}

// Field 8007e16c: the corners of a quad (+8..+22) at x, y of size w, h,
// mirrored when `mirror` is set.
void Program::dialogue_quad(std::uint32_t packet, std::int32_t x, std::int32_t y, std::int32_t w,
                            std::int32_t h, bool mirror) {
    if (!mirror) {
        --x;
        set_memory(packet + 8, u32(x), 2);
        set_memory(packet + 0x10, u32(x + w), 2);
        set_memory(packet + 0x18, u32(x), 2);
        set_memory(packet + 0x20, u32(x + w), 2);
    } else {
        set_memory(packet + 0x10, u32(x), 2);
        set_memory(packet + 8, u32(x + w), 2);
        set_memory(packet + 0x20, u32(x), 2);
        set_memory(packet + 0x18, u32(x + w), 2);
    }
    set_memory(packet + 0xa, u32(y), 2);
    set_memory(packet + 0x12, u32(y), 2);
    set_memory(packet + 0x1a, u32(y + h), 2);
    set_memory(packet + 0x22, u32(y + h), 2);
}

// Field 8007e1c0: a window's frame. The prompt cursor (+3ac per buffer) while
// the text waits, the eight border pieces (+1d4, 0xc8 per buffer), the
// portrait (+42c) and the background tile (+c4).
void Program::draw_dialogue_frame(std::uint32_t table, std::uint32_t buffer, std::uint32_t w) {
    const auto window = window_base(w);
    const auto half = [&](std::uint32_t offset) { return s16(memory(window + offset, 2)); };
    if (half(0x3f6) != 0)
        return;
    auto x = half(0x94);
    auto y = half(0x96);
    auto width = half(0x98);
    auto height = half(0x9a);
    if (const auto hold = half(0x3f0); hold != 0) {
        // 8007e28c: while the hold timer runs the window grows from its
        // centre, at least 16 pixels each way, and slides along +404/+408
        // (16.16) by +40c/+410.
        const auto steps = field->text_speed * 2;
        if (steps == 0)
            throw field::FieldFormatError(
                "Original MIPS division by zero is not a recovered result");
        const auto done = field->text_speed - hold;
        // mult keeps the low word of the product.
        const auto grown_w = s32(u32(s32(u32(width) << 16U) / steps) * u32(done));
        const auto grown_h = s32(u32(s32(u32(height) << 16U) / steps) * u32(done));
        x = x + width / 2 - (grown_w >> 16);
        y = y + height / 2 - (grown_h >> 16);
        width = s32(u32(grown_w) * 2) >> 16;
        height = s32(u32(grown_h) * 2) >> 16;
        if (width < 0x10) {
            x -= (0x10 - width) / 2;
            width = 0x10;
        }
        if (height < 0x10) {
            y -= (0x10 - height) / 2;
            height = 0x10;
        }
        set_memory(window + 0x404, memory(window + 0x404) + memory(window + 0x40c));
        const auto slide_y = memory(window + 0x408) + memory(window + 0x410);
        y += s32(slide_y) >> 16;
        set_memory(window + 0x408, slide_y);
        x += s16(memory(window + 0x406, 2));
    }
    const auto flags = [&] { return memory(window + 0x3f4, 2); };
    if (half(0x3ac) == 0 && memory(window + 0x3f8, 2) == 0 && half(0x3f0) == 0 &&
        (flags() & 0x40U) == 0 && half(0x364) != 0) {
        if (const auto delay = half(0x3f2); delay == 0) {
            // The prompt cursor at the waiting position; its texture window
            // steps through the animation frames (800adf04).
            const auto u = u32(s16(memory(window + 4, 2)) + s16(memory(window, 2)) * 4);
            const auto v = dialogue_line_y(window) + 4;
            const auto frame = 0x800adf04 + field->dialogue_cursor * 8;
            const std::array<std::int16_t, 4> area{static_cast<std::int16_t>(memory(frame, 2)),
                                                   static_cast<std::int16_t>(memory(frame + 2, 2)),
                                                   static_cast<std::int16_t>(memory(frame + 4, 2)),
                                                   static_cast<std::int16_t>(memory(frame + 6, 2))};
            const auto mode = window + 0x3b0 + buffer * 0xc;
            set_draw_mode(mode, gpu::texture_page(0, 0, 0x298, 0x1c0), area);
            const auto sprite = window + 0x3c8 + buffer * 0x14;
            set_memory(sprite + 8, u, 2);
            set_memory(sprite + 0xa, u32(v), 2);
            add_primitive(table, sprite);
            add_primitive(table, mode);
        } else {
            set_memory(window + 0x3f2, u32(delay - 1), 2);
        }
    } else {
        set_memory(window + 0x3f2, 2, 2);
    }
    // Border pieces: corners and edges (+1dc.. per buffer).
    const auto border = window + buffer * 0xc8;
    const auto left = x - 8;
    const auto top = y - 7;
    const auto right = x + width - 8;
    const auto bottom = y + height - 9;
    set_memory(border + 0x1dc, u32(left), 2);
    set_memory(border + 0x1de, u32(top), 2);
    set_memory(border + 0x206, u32(top), 2);
    set_memory(border + 0x22c, u32(left), 2);
    set_memory(border + 0x204, u32(right), 2);
    set_memory(border + 0x254, u32(right), 2);
    set_memory(border + 0x218, u32(left), 2);
    set_memory(border + 0x1f0, u32(right), 2);
    set_memory(border + 0x22e, u32(bottom), 2);
    set_memory(border + 0x256, u32(bottom), 2);
    set_memory(border + 0x21a, u32(y + 9), 2);
    set_memory(border + 0x1f2, u32(y + 9), 2);
    const auto span = std::max(height - 0x12, 0);
    set_memory(border + 0x222, u32(span), 2);
    set_memory(border + 0x1fa, u32(span), 2);
    set_memory(border + 0x242, u32(top), 2);
    set_memory(border + 0x240, u32(x + 8), 2);
    set_memory(border + 0x268, u32(x + 8), 2);
    set_memory(border + 0x26a, u32(bottom), 2);
    set_memory(border + 0x248, u32(width - 0x10), 2);
    set_memory(border + 0x270, u32(width - 0x10), 2);
    if ((flags() & 0x40U) == 0)
        for (std::uint32_t i = 0; i < 8; ++i) {
            add_primitive(table, border + 0x1d4 + i * 0x14);
            add_primitive(table, window + 0xe4 + buffer * 0x78 + i * 0xc);
        }
    // The portrait: up to 64x64 at the left (or right, flag 0x20) corner.
    const auto portrait_w = width - 4 < 0x40 ? width - 8 : 0x40;
    const auto portrait_h = height - 4 < 0x40 ? height - 8 : 0x40;
    const auto mirrored = (flags() & 0x20U) != 0;
    const auto portrait_x = mirrored ? x + width - portrait_w - 4 : x + 4;
    const auto portrait = window + 0x42c + buffer * 0x28;
    dialogue_quad(portrait, portrait_x, y + 4, portrait_w, portrait_h, mirrored);
    if (memory(window + 0x47c, 1) == 1) {
        add_primitive(table, portrait);
        add_primitive(table, window + 0x414 + buffer * 0xc);
    }
    if (half(0x364) == 0 && memory(window + 0x3f8, 2) == 0 && half(0x3f0) == 0)
        missing("draw_dialogue_frame", 0x8007ea94, "symbol:dialogue-choice-cursor",
                "The choice cursor of a displayed window is not recovered");
    const auto tile = window + buffer * 0x10;
    set_memory(tile + 0xcc, u32(x), 2);
    set_memory(tile + 0xce, u32(y + 1), 2);
    set_memory(tile + 0xd0, u32(width), 2);
    set_memory(tile + 0xd2, u32(height - 2), 2);
    if ((flags() & 0x40U) == 0) {
        add_primitive(table, tile + 0xc4);
        add_primitive(table, window + 0xac + buffer * 0xc);
    }
}

// Resident 800347c0: the screen row of the line a window is on (+2): rows
// after the first shown line (+16), wrapping over the line count (+c).
std::int32_t Program::dialogue_line_y(std::uint32_t window) {
    auto row = s16(memory(window + 2, 2)) - s16(memory(window + 0x16, 2));
    if (row < 0)
        row = s16(memory(window + 0xc, 2)) - 1;
    return s16(memory(window + 6, 2)) + row * s16(memory(window + 0x14, 2));
}

// SetDrawMode (800454dc) with dfe and dtd clear.
void Program::set_draw_mode(std::uint32_t packet, std::uint32_t tpage,
                            const std::array<std::int16_t, 4> &area) {
    set_memory(packet + 3, 2, 1);
    set_memory(packet + 4, gpu::draw_mode(resident.gpu_type, false, false, tpage));
    set_memory(packet + 8, gpu::texture_window(&area));
}

// Field 8007dcf8: the lit line (+6e) of a displayed window follows the
// choice (+36a over +368 options, from +366) moved by pad input (800c3900).
void Program::dialogue_choice(std::uint32_t w) {
    const auto window = window_base(w);
    if (s16(memory(window + 0x364, 2)) != 0 || s16(memory(window + 0x3f0, 2)) != 0)
        return;
    if (memory(window + 0x3f8, 2) != 0) {
        set_memory(window + 0x6e, 0xff, 1);
        return;
    }
    missing("dialogue_choice", 0x8007dd68, "symbol:dialogue-choice-input",
            "Moving the choice of a displayed window is not recovered");
}

// One window of 8008004c: its text (80034888), its buffer's packet list
// (800c2698 + 0x498 * w), frame (8007e1c0) and choice (8007dcf8). The first
// pass tests the waiting line before the text, the ranked pass after it.
void Program::draw_dialogue_window(FrameServices &services, std::uint32_t table, std::uint32_t w,
                                   bool first) {
    auto &state = *field;
    const auto window = window_base(w);
    const auto prompt = [&] {
        if (dialogue_waiting(window) != 0 && s16(memory(window + 0x364, 2)) != 0)
            set_memory(window + 0x3ac, 0, 2);
    };
    set_memory(window + 0x3ac, 0xffff, 2);
    if (memory(window + 0x3f0, 2) == 0) {
        if (first)
            prompt();
        if ((state.control_inputs.pressed_buttons & 0x20U) != 0 &&
            (first || memory(window + 0x3f8, 2) == 0)) {
            // Button 0x20 ends the window's wait: its owner keeps the choice
            // (+36a from +366) at actor +81, then 800345e0 clears the wait.
            set_memory(window + 0x364, 0xffff, 2);
            const auto owner =
                state.actors.at(static_cast<std::size_t>(s16(memory(window + 0x3fe, 2))));
            set_memory(memory(owner.descriptor_address + 0x4c) + 0x81,
                       memory(window + 0x36a, 1) + memory(window + 0x366, 1), 1);
            const auto flags = memory(window + 0x10, 2);
            set_memory(window + 0x10, flags & 0xfff7U, 2);
            if ((flags & 0x200U) != 0) {
                set_memory(window + 0x84, 0, 2);
                set_memory(window + 0x6c, 0, 1);
                set_memory(window + 0x10, memory(window + 0x10, 2) & 0xfdffU, 2);
            }
        }
        if (memory(window + 0x82, 2) == 0)
            queue_dialogue_page(window, memory(window + 0x90));
        draw_dialogue_text(services, window, table, state.draw_buffer);
        if (!first)
            prompt();
    }
    add_primitive(table, window - 0x18 + state.draw_buffer * 0xc);
    draw_dialogue_frame(table, state.draw_buffer, w);
    dialogue_choice(w);
}

// Field 8007f6f8: close window `w` unless it is busy. Its text stops
// (80034614, 800345e0, 800346a4), its queued pages and its line and glyph
// blocks are released (8003463c, 800346d4), and the window, its slot and its
// talk inhibition bit are freed.
void Program::close_dialogue(std::uint32_t w) {
    auto &state = *field;
    if (state.dialogue[w].half(field::DialogueWindow::busy) != 0)
        return;
    const auto window = window_base(w);
    const auto flags = [&] { return memory(window + 0x10, 2); };
    if (s16(memory(window + 0x84, 2)) == 0) {
        set_memory(window + 0x6c, 0, 1);
        set_memory(window + 0x10, flags() & 2U, 2);
    }
    set_memory(window + 0x10, flags() & 0xfff7U, 2);
    if ((flags() & 0x200U) != 0) {
        set_memory(window + 0x84, 0, 2);
        set_memory(window + 0x6c, 0, 1);
        set_memory(window + 0x10, flags() & 0xfdffU, 2);
    }
    set_memory(window + 0x6c, 0, 1);
    set_memory(window + 0x84, 0, 2);
    set_memory(window + 0x10, flags() & 2U, 2);
    for (auto page = memory(window + 0x8c); page != 0;) {
        const auto next = memory(page);
        release_dialogue_block(window, page, 0x80034674);
        page = next;
    }
    set_memory(window + 0x8c, 0);
    set_memory(window + 0x82, 0, 2);
    release_dialogue_block(window, memory(window + 0x28), 0x800346ec);
    release_dialogue_block(window, memory(window + 0x2c), 0x800346f8);
    auto &record = state.dialogue[w];
    record.set_half(field::DialogueWindow::status, 0xffff);
    record.set_half(field::DialogueWindow::busy, 0xffff);
    record.set_half(field::DialogueWindow::cleared, 0xffff);
    record.set_half(0x3f8, 0xffff);
    state.dialogue_slots[w] = -1;
    state.talk_inhibited = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(state.talk_inhibited) & ((1U << w) ^ 0xffU));
    record.set_half(field::DialogueWindow::owner, 0xff);
    record.set_half(0x3fa, 0);
}

// Field 800805f4: per dialogue window, close it (8007f6f8) when its hold
// timer (+3f0) ran out without a keep flag, or once it was cleared, then
// count the timer down.
void Program::frame_dialogue_timers() {
    auto &state = *field;
    for (std::size_t w = 0; w < state.dialogue.size(); ++w) {
        auto &window = state.dialogue[w];
        if (window.half(field::DialogueWindow::busy) != 0)
            continue;
        if (window.half(0x3f0) == 0 && (window.half(0x10) & 4) == 0)
            close_dialogue(static_cast<std::uint32_t>(w));
        if (window.half(field::DialogueWindow::cleared) == 0)
            close_dialogue(static_cast<std::uint32_t>(w));
        if (const auto hold = window.half(0x3f0); hold != 0)
            window.set_half(0x3f0, static_cast<std::uint16_t>(hold - 1));
    }
}

// Field 8008004c: the cursor animation counters, then every displayed window
// (+3fa set) is drawn in its order (+3f8), then the order is renumbered and
// the frame's text draw mode is linked into `table`.
void Program::frame_dialogue(FrameServices &services, std::uint32_t table) {
    auto &state = *field;
    ++state.dialogue_ticks;
    if ((state.dialogue_ticks & 3U) == 0)
        ++state.dialogue_cursor;
    if (s32(state.dialogue_cursor) > 4)
        state.dialogue_cursor = 0;
    // The last window with +3fa set is drawn first, the others by rank.
    std::size_t selected = 0xff;
    for (std::size_t w = 0; w < state.dialogue.size(); ++w)
        if (state.dialogue[w].half(0x3fa) != 0)
            selected = w;
    for (std::size_t w = 0; w < state.dialogue.size(); ++w) {
        const auto &window = state.dialogue[w];
        if (window.half(field::DialogueWindow::busy) == 0 && window.half(0x3fa) != 0)
            draw_dialogue_window(services, table, static_cast<std::uint32_t>(w), true);
    }
    std::array<std::uint32_t, 4> order{0xffff, 0xffff, 0xffff, 0xffff};
    std::uint32_t next = 0;
    for (std::uint32_t rank = 0; rank < 4; ++rank)
        for (std::size_t w = 0; w < state.dialogue.size(); ++w) {
            const auto &window = state.dialogue[w];
            const auto current = static_cast<std::uint16_t>(window.half(0x3f8));
            if (current == rank) {
                order[w] = next++;
                if (window.half(field::DialogueWindow::busy) == 0 && w != selected)
                    draw_dialogue_window(services, table, static_cast<std::uint32_t>(w), false);
            }
            if (current == 0xffff)
                order[w] = 0xffff;
        }
    for (std::size_t w = 0; w < state.dialogue.size(); ++w)
        state.dialogue[w].set_half(0x3f8, static_cast<std::uint16_t>(order[w]));
    add_primitive(table, 0x800b1df4 + state.draw_buffer * 0xc0);
}

} // namespace xem::reconstruction
