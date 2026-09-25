// Field overlay 800805f4 and 8008004c: the dialogue windows of a field
// frame. Each window (800c26b0 + 0x498 * w) keeps its own packets per draw
// buffer; drawing fills and links them into the frame's small table. Offsets
// below are relative to the window record; original addresses name
// correlations only.
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/program.hpp"

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

// Resident 80034888: the text lines of a window. Each line record (+28, 0x60
// bytes: two sprites per buffer, then the line's width +58, palette row +5c
// and clut +5e) is linked when it has glyphs; the line at +6e is lit.
void Program::draw_dialogue_text(std::uint32_t window, std::uint32_t table, std::uint32_t buffer) {
    auto flags = [&] { return memory(window + 0x10, 2); };
    if ((flags() & 4U) == 0) {
        if (memory(window + 0x82, 2) == 0)
            return;
        missing("draw_dialogue_text", 0x800348d8, "symbol:dialogue-next-page",
                "Advancing a window to its next text page is not recovered");
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
        if ((flags() & 0x58U) == 0)
            missing("draw_dialogue_text", 0x80033df0, "symbol:dialogue-glyph-render",
                    "Rendering the next glyph and its upload are not recovered");
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
    const auto x = half(0x94);
    const auto y = half(0x96);
    const auto width = half(0x98);
    const auto height = half(0x9a);
    if (half(0x3f0) != 0)
        missing("draw_dialogue_frame", 0x8007e28c, "symbol:dialogue-window-opening",
                "The opening animation of a dialogue window is not recovered");
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
void Program::draw_dialogue_window(std::uint32_t table, std::uint32_t w, bool first) {
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
            missing("draw_dialogue_window", 0x80034714, "symbol:dialogue-page-start",
                    "Starting a window's next page is not recovered");
        draw_dialogue_text(window, table, state.draw_buffer);
        if (!first)
            prompt();
    }
    add_primitive(table, window - 0x18 + state.draw_buffer * 0xc);
    draw_dialogue_frame(table, state.draw_buffer, w);
    dialogue_choice(w);
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
        const bool expired = window.half(0x3f0) == 0 && (window.half(0x10) & 4) == 0;
        if (expired || window.half(field::DialogueWindow::cleared) == 0)
            throw MissingDependency({"field_frame_dialogue_timers", 0x8007f6f8, w, {}},
                                    "symbol:dialogue-window-close", false,
                                    "Closing a dialogue window from the frame is not recovered");
        if (const auto hold = window.half(0x3f0); hold != 0)
            window.set_half(0x3f0, static_cast<std::uint16_t>(hold - 1));
    }
}

// Field 8008004c: the cursor animation counters, then every displayed window
// (+3fa set) is drawn in its order (+3f8), then the order is renumbered and
// the frame's text draw mode is linked into `table`.
void Program::frame_dialogue(std::uint32_t table) {
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
            draw_dialogue_window(table, static_cast<std::uint32_t>(w), true);
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
                    draw_dialogue_window(table, static_cast<std::uint32_t>(w), false);
            }
            if (current == 0xffff)
                order[w] = 0xffff;
        }
    for (std::size_t w = 0; w < state.dialogue.size(); ++w)
        state.dialogue[w].set_half(0x3f8, static_cast<std::uint16_t>(order[w]));
    add_primitive(table, 0x800b1df4 + state.draw_buffer * 0xc0);
}

} // namespace xem::reconstruction
