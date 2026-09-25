// Field primary FC (8009bf8c) and the message-window opening chain 8009c01c,
// 8009c5a8, 8007f8dc, 8007f814, 8007e114 and resident 80032f54. Original
// addresses name correlations only; windows are original-layout records.
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_heap.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <string>

namespace xem::reconstruction {
namespace {
using Window = field::DialogueWindow;

std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Message window read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Message window write exceeds owned storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
field::FieldRecord &record(field::FieldWorld &world, std::size_t index) {
    if (index >= world.actors.size())
        throw field::FieldFormatError("Message actor index outside the event actors");
    return world.actors[index];
}
std::uint32_t pc(const field::FieldRecord &actor) { return word(actor.actor, 0xcc, 2); }
void set_pc(field::FieldRecord &actor, std::uint32_t value) { put(actor.actor, 0xcc, value, 2); }
std::int32_t quotient(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0)
        throw field::FieldFormatError("Original MIPS division by zero is not a recovered result");
    if (numerator == INT32_MIN && denominator == -1)
        return numerator;
    return numerator / denominator;
}
// Field 8009a514: the camera heading's octant, counted the other way round.
std::uint32_t camera_octant(const FieldState &state) {
    return (7U - u32((s16(u32(state.control_inputs.camera_angle)) - 0x100) >> 9)) & 7U;
}
} // namespace

// Resident 800286cc: nonzero while the disc is in error, reading or pending.
std::uint32_t Program::disc_busy() {
    if (resident.disc_error != 0)
        return resident.disc_error;
    if (resident.disc_stream.host_file_table == 0) {
        // 8004293c CD_datasync(1): arm the timeout, then poll DMA3 once.
        resident.cd_sync_deadline = resident.vsync_counter + 0x3c0;
        resident.cd_sync_polls = 0;
        resident.cd_sync_label = 0x80018f30;
        if (static_cast<std::int32_t>(resident.cd_sync_deadline) <
            static_cast<std::int32_t>(resident.vsync_counter))
            throw MissingDependency({"cd_sync_timeout", 0x800429e4, {}, {}},
                                    "symbol:libcd-timeout-reset", false,
                                    "The CD_datasync timeout and reset path is not reconstructed");
        ++resident.cd_sync_polls;
        const auto offset = resident.cd_dma_register - 0x1f801000U;
        if (offset > resident.io.size() - 4)
            throw field::FieldFormatError("DMA3 control register outside the observed I/O page");
        const auto control = word(resident.io, offset);
        if ((control & 0x1000000U) != 0)
            return 1;
    }
    return resident.disc_pending != 0 ? 1U : 0U;
}

// Field 8008a558: -1 while battle menus or the disc are busy; otherwise waits
// (80028a60 with zero) until the disc is idle.
std::int32_t Program::disc_idle_query() {
    if (resident.battle_request.menu_gate != 0 || disc_busy() != 0)
        return -1;
    disc_wait(0);
    return 0;
}

// Field 8007f814: the screen position of a point `height` above a descriptor's
// origin. It leaves the composed transform loaded in the GTE.
std::array<std::int32_t, 2> Program::project_actor(std::size_t index, std::int16_t height) {
    const auto &descriptor = field->actors.at(index).descriptor;
    field::GteMatrix model{};
    for (std::size_t i = 0; i < 9; ++i)
        model.r[i] = static_cast<std::int16_t>(word(descriptor, 0xc + i * 2, 2));
    model.pad = static_cast<std::int16_t>(word(descriptor, 0x1e, 2));
    for (std::size_t i = 0; i < 3; ++i)
        model.t[i] = s32(word(descriptor, 0x20 + i * 4));
    resident.gte.transform = field::compose_matrix(field->camera.scaled_world, model);
    const auto screen =
        field::rot_trans_pers(resident.gte.transform, resident.gte.screen, {0, height, 0});
    return {screen[0], screen[1]};
}

// Field 8009c5a8. Returns -1 when the window cannot open yet (the caller
// retries the instruction), 0 after opening it and advancing the PC by four.
std::int32_t Program::open_dialogue(field::FieldWorld &world, field::FieldPassState &pass,
                                    std::uint32_t speaker, std::uint32_t mode) {
    auto &state = *field;
    auto &self = record(world, world.current);
    world.control.batch_limit += 0x20;
    const auto defer = [&] {
        world.control.break_requested = 1;
        return -1;
    };
    if (resident.battle_request.menu_gate != 0 || state.dialogue_gate_afd04 != 0 ||
        pass.unknown_c4268 != 0 || state.control_inputs.jump_contact != 0xff)
        return defer();
    if (state.disc_idle_known == 0 && disc_idle_query() != 0)
        return defer();
    if (self.actor[0x80] != 0xff)
        throw MissingDependency({"message_portrait", 0x8009c154, world.current, pc(self)},
                                "symbol:field-message-portrait-8009c154", false,
                                "Portrait message windows (8009c154) are not recovered");
    ++pass.unknown_c4268;
    // 8009cd18: a window still owned by this actor and not busy.
    for (auto &window : state.dialogue)
        if (window.half(Window::owner) == static_cast<std::int32_t>(world.current) &&
            window.half(Window::busy) == 0) {
            world.control.break_requested = 1;
            window.set_half(Window::cleared, 0);
            return -1;
        }
    world.control.batch_limit += 8;
    const auto message = static_cast<std::uint32_t>(world.program.byte(pc(self) + 1U)) |
                         static_cast<std::uint32_t>(world.program.byte(pc(self) + 2U)) << 8U;
    constexpr std::size_t age = 0x3f8;
    // 80080720 / 80080760 / 800807b4: take a free window, or give up the oldest.
    std::size_t index = 4;
    if (std::ranges::none_of(state.dialogue, [](const Window &w) { return w.half(age) == -1; })) {
        std::uint16_t oldest_age = 0;
        std::optional<std::size_t> oldest;
        for (std::size_t i = 0; i < 4; ++i) {
            const auto value = static_cast<std::uint16_t>(state.dialogue[i].half(age));
            if (value != 0xffff && oldest_age <= value) {
                oldest = i;
                oldest_age = value;
            }
        }
        if (!oldest)
            throw field::FieldFormatError("No message window is free or open");
        state.dialogue[*oldest].set_half(Window::cleared, 0);
        world.control.break_requested = 1;
        return -1;
    }
    for (auto &window : state.dialogue)
        if (window.half(age) != -1)
            window.set_half(age, static_cast<std::uint16_t>(window.half(age) + 1));
    for (std::size_t i = 0; i < 4; ++i)
        if (state.dialogue[i].half(age) == -1) {
            state.dialogue[i].set_half(age, 0);
            index = i;
            break;
        }
    constexpr std::size_t layout_flags = 0x3f4;
    std::uint32_t idle = 0, combined = 0;
    for (const auto &window : state.dialogue)
        if (window.half(Window::busy) == 0) {
            ++idle;
            combined |= u32(window.half(layout_flags));
        }
    // 8003373c / 80033760: message dimensions, one halfword per message.
    const auto &table = state.messages;
    const auto entry = (word(table, 0, 2) + message + 3U) * 2U;
    auto columns = static_cast<std::int32_t>(word(table, entry, 1));
    auto rows = static_cast<std::int32_t>(word(table, entry + 1, 1));
    const bool placed = mode == 0 || mode == 3;
    if (placed) {
        if (self.actor[0x82] != 0)
            columns = self.actor[0x82];
        if (self.actor[0x83] != 0)
            rows = self.actor[0x83];
    }
    const auto progress = word(self.actor, 0x84);
    const auto low = progress & 0xffffU;
    put(self.actor, 0x84, low);
    auto style = low;
    if (const auto override = world.program.byte(pc(self) + 3U); override != 0) {
        style = (progress & 0xff00U) | override;
        put(self.actor, 0x84, low | style << 16U);
    }
    const auto portrait = [&] { return self.actor[0x80] != 0xff && (style & 2U) == 0; };
    std::int32_t x = 0, y = 0x10;
    bool located = false;
    const auto place = [&](std::uint16_t kind, std::int32_t fixed_y, bool below) {
        state.dialogue[index].set_half(layout_flags, kind);
        if (!placed) {
            columns = 0x48;
            rows = 4;
            y = fixed_y;
            x = 0xa0;
            located = true;
            return;
        }
        const auto point = project_actor(speaker, -0x40);
        x = point[0];
        if (mode != 0) {
            // Mode 3 above the speaker keeps the 14 set in 8009c930's delay slot.
            x = 0xa0;
            y = kind == 1 ? 0x14 : fixed_y;
        } else {
            y = below ? point[1] + 0x30 : point[1] - rows * 14 - 0x24;
        }
        located = true;
        if (portrait()) {
            columns = std::max(columns, 0x18) + 0x11;
            rows = 4;
            y = fixed_y;
        }
    };
    const auto layout = (style >> 4U) & 3U;
    if (layout == 1) {
        place(1, 0x10, false);
    } else if (layout == 2) {
        place(0x81, 0x94, true);
    } else if (layout == 0) {
        const auto facing = ((word(self.actor, 0x12c) >> 9U) - camera_octant(state)) & 7U;
        const bool above =
            facing < 5 ? (combined & 0x80U) != 0 : (combined & 0x80U) == 0 && idle == 0;
        if (above)
            place(1, 0x10, false);
        else
            place(0x81, 0x94, true);
    }
    if (!located)
        throw MissingDependency({"message_layout", 0x8009ca64, world.current, pc(self)},
                                "symbol:message-layout-3-uninitialized", false,
                                "Message layout 3 reads an uninitialized stack word");
    auto left = x - 8 - columns * 2;
    if (left < 0xc)
        left = 0xc;
    if (left + 0x10 + columns * 4 >= 0x135)
        left = 0x124 - columns * 4;
    if (y < 0x10)
        y = 0x10;
    if (y + 8 + rows * 14 >= 0xd5)
        y = 0xcc - rows * 14;
    auto top = y;
    if (placed) {
        if (const auto value = s16(word(self.actor, 0x88, 2)); value != 0)
            left = value;
        if (const auto value = s16(word(self.actor, 0x8a, 2)); value != 0)
            top = value;
        if (self.actor[0x82] != 0)
            columns = self.actor[0x82];
        if (self.actor[0x83] != 0)
            rows = self.actor[0x83];
        if (portrait())
            rows = 4;
    }
    auto &window = state.dialogue[index];
    if ((style & 0x40U) != 0)
        window.set_half(layout_flags, static_cast<std::uint16_t>(window.half(layout_flags) | 0x40));
    std::uint32_t turned = 0;
    if ((style & 0xcU) == 0) {
        const auto &speaker_actor = state.actors.at(speaker).storage;
        const auto heading = s32(word(speaker_actor, 0x106, 2) << 16U) >> 25;
        turned = ((u32(heading) - camera_octant(state) + 1U) & 7U) < 4 ? 0U : 0x400U;
    } else if ((style & 4U) != 0) {
        turned = 0x400;
    }

    // 8007f8dc: window setup.
    const auto owner = world.current;
    const auto shown_x = static_cast<std::int16_t>(left);
    const auto shown_y = static_cast<std::int16_t>(top - 8);
    const auto owner_progress = word(state.actors.at(owner).storage, 0x84);
    auto flags = (owner_progress >> 16U) != 0 ? owner_progress >> 16U : owner_progress & 0xffffU;
    flags |= turned;
    for (std::size_t tries = 0; tries < 4; ++tries) {
        const auto slot = state.dialogue_slot_cursor & 3U;
        ++state.dialogue_slot_cursor;
        if (state.dialogue_slots[slot] == -1) {
            state.dialogue_slots[slot] = 0;
            break;
        }
    }
    for (std::size_t i = 0; i < 4; ++i) {
        const auto value = world.variables.read(static_cast<std::uint16_t>(0x16 + i * 2));
        put(window.bytes, 0x70 + i * 4, u32(value));
        if (i == 3)
            put(window.bytes, 0x80, u32(value), 2);
    }
    std::int32_t target_x = 0, target_y = 0;
    if (mode == 2) {
        target_x = 0xa0;
        target_y = shown_y + 0x20;
    } else if (mode == 3) {
        target_x = shown_x + 8 + columns * 2;
        target_y = shown_y + 8 + rows * 7;
    } else {
        const auto point = project_actor(speaker, -0x40);
        target_x = point[0];
        target_y = point[1];
    }
    if (state.actors.at(owner).storage[0x80] != 0xff && (flags & 2U) == 0)
        throw MissingDependency({"message_portrait", 0x8007f5ac, owner, pc(self)},
                                "symbol:field-message-portrait-8007f5ac", false,
                                "Portrait message windows (8007f5ac) are not recovered");
    window.bytes.at(0x47d) = 0x80;
    window.bytes.at(0x47c) = 0;
    window.set_half(Window::status, 0xffff);
    // 8007e114: the window rectangle.
    put(window.bytes, 0x94, u32(shown_x), 2);
    put(window.bytes, 0x96, u32(shown_y), 2);
    put(window.bytes, 0x98, u32(columns * 4 + 0x10), 2);
    put(window.bytes, 0x9a, u32(rows * 14 + 0x10), 2);

    // Resident 80032f54: the text record at the window base and its primitives.
    auto &text = window.bytes;
    const auto vram_x = state.dialogue_vram.at(index * 2);
    const auto vram_y = state.dialogue_vram.at(index * 2 + 1);
    put(text, 4, u32(shown_x + 8), 2);
    put(text, 0x10, 0, 2);
    put(text, 0x84, 0, 2);
    put(text, 0x8c, 0);
    put(text, 0x82, 0, 2);
    put(text, 0x14, 0xe, 2);
    text[0x68] = 1;
    text[0x69] = 1;
    text[0x6c] = 0;
    text[0x6a] = 0;
    text[0x6d] = 0;
    text[0x6b] = 0;
    text[0x6e] = 0xff;
    put(text, 0xe, vram_y, 2);
    put(text, 6, u32(shown_y + 8), 2);
    put(text, 0xc, u32(rows), 2);
    const auto width = (u32(columns) & 0xffffU) | 1U;
    put(text, 0xa, width, 2);
    put(text, 8, width << 2U, 2);
    put(text, 0x12, width + 3U, 2);
    const auto allocate = [&](std::uint16_t allocation_class, std::uint32_t bytes,
                              std::uint32_t call_site) {
        resident.heap.allocation_class = allocation_class; // 800324b8
        auto block = resident::heap_allocate(resident.heap, bytes, 2, call_site);
        if (!block)
            throw MissingDependency({"message_allocation", call_site, owner, pc(self)},
                                    "symbol:heap-quiet-null", false,
                                    "A quiet null message allocation is not reconstructed");
        return std::move(*block);
    };
    const auto text_rows = s16(word(text, 0xc, 2));
    auto rows_block = allocate(0x29, u32(text_rows * 0x60), 0x8003300c);
    put(text, 0x28, rows_block.address);
    auto glyph_block = allocate(0x28, u32(s16(word(text, 0x12, 2)) * 0x1c), 0x80033030);
    put(text, 0x2c, glyph_block.address);
    text[0x4b] = 3;
    put(text, 0x4c, 0x60000000);
    put(text, 0x50, u32(s16(word(text, 4, 2)) - 7) | u32(s16(word(text, 6, 2)) - 5) << 16U);
    put(text, 0x54,
        u32(s16(word(text, 0xa, 2)) * 4 + 0xd) | u32(text_rows * s16(word(text, 0x14, 2)) + 10)
                                                     << 16U);
    text[0x4f] = static_cast<std::uint8_t>(text[0x4f] | 2); // SetSemiTrans(+48, 1)
    for (std::size_t i = 0; i < 0x10; ++i)
        text[0x58 + i] = text[0x48 + i];
    auto &rows_bytes = rows_block.bytes;
    const auto spacing = s16(word(text, 0x14, 2));
    const auto span = s16(word(text, 8, 2));
    for (std::int32_t row = 0; row < text_rows; ++row) {
        const auto at = static_cast<std::size_t>(row) * 0x60;
        const auto line_y = u32(s16(word(text, 6, 2)) + spacing * row) << 16U;
        put(rows_bytes, at + 8, u32(s16(word(text, 4, 2))) | line_y);
        put(rows_bytes, at + 0x1c, u32(s16(word(text, 4, 2)) + 0x100) | line_y);
        put(rows_bytes, at + 0x10, span < 0x101 ? u32(span) | 0xd0000U : 0xd0100U);
        put(rows_bytes, at + 0x24, span < 0x101 ? 0xd0000U : u32(span - 0xf0) | 0xd0000U);
        const auto pair = row / 2;
        const auto line = vram_y + u32(pair * 13);
        const auto texture = (vram_x & 0x3fU) << 2U | (line & 0xffU) << 8U;
        put(rows_bytes, at + 0xc, texture, 2);
        put(rows_bytes, at + 0x20, texture, 2);
        rows_bytes[at + 3] = 4;
        rows_bytes[at + 7] = 100;
        rows_bytes[at + 0x17] = 4;
        rows_bytes[at + 0x1b] = 100;
        rows_bytes[at + 7] |= 1;
        rows_bytes[at + 0x1b] |= 1;
        for (std::size_t i = 0; i < 0x14; ++i)
            rows_bytes[at + 0x28 + i] = rows_bytes[at + i];
        for (std::size_t i = 0; i < 0x14; ++i)
            rows_bytes[at + 0x3c + i] = rows_bytes[at + 0x14 + i];
        put(rows_bytes, at + 0x50, vram_x, 2);
        put(rows_bytes, at + 0x52, line, 2);
        put(rows_bytes, at + 0x54, word(text, 0x12, 2), 2);
        put(rows_bytes, at + 0x56, 0xd, 2);
        put(rows_bytes, at + 0x58, 0, 2);
        put(rows_bytes, at + 0x5e, resident.text_cluts[(row & 1) != 0 ? 1 : 0], 2);
        rows_bytes[at + 0x5c] = static_cast<std::uint8_t>(line);
        rows_bytes[at + 0x5a] = static_cast<std::uint8_t>(row & 1);
        rows_bytes[at + 0x5b] = static_cast<std::uint8_t>(row);
    }
    // SetDrawMode(+30 and +3c, 0, 0, GetTPage(0, 0, x, y), NULL).
    for (const auto [offset, page_x] :
         {std::pair{0x30U, s16(vram_x)}, std::pair{0x3cU, s16(vram_x) + 0x40}}) {
        text[offset + 3] = 2;
        put(text, offset + 4,
            gpu::draw_mode(resident.gpu_type, false, false,
                           gpu::texture_page(0, 0, page_x, s16(vram_y))));
        put(text, offset + 8, 0);
    }
    state.dialogue_blocks.at(index) = {};
    state.dialogue_blocks[index].push_back(std::move(rows_block));
    state.dialogue_blocks[index].push_back(std::move(glyph_block));

    if ((flags & 0x400U) != 0)
        window.set_half(layout_flags, static_cast<std::uint16_t>(window.half(layout_flags) | 0x20));
    window.bytes.at(0x68) = state.text_speed == 8 ? 1 : 2;
    // 80033728: the message's address in the loaded table.
    put(window.bytes, 0x90, state.messages_address + word(table, message * 2U + 4U, 2));
    window.set_half(Window::busy, 0);
    put(window.bytes, 0x10, word(window.bytes, 0x10, 2) | 2U, 2);
    window.set_half(Window::owner, static_cast<std::uint16_t>(owner));
    window.set_half(Window::speaker, static_cast<std::uint16_t>(speaker));
    put(window.bytes, 0x3f0, u32(state.text_speed), 2);
    put(window.bytes, 0x3fa, (style & 0x800U) != 0 ? 1U : 0U, 2);
    const auto dx = s32(u32(target_x - (columns * 2 + 8) - shown_x) << 16U);
    const auto dy = s32(u32(target_y - (rows * 7 + 8) - shown_y) << 16U);
    put(window.bytes, 0x404, u32(dx));
    put(window.bytes, 0x408, u32(dy));
    if ((flags & 0x100U) == 0) {
        put(window.bytes, 0x40c, 0U - u32(quotient(dx, state.text_speed)));
        put(window.bytes, 0x410, 0U - u32(quotient(dy, state.text_speed)));
    } else {
        put(window.bytes, 0x3f0, 1, 2);
        put(window.bytes, 0x40c, 0U - u32(dx));
        put(window.bytes, 0x410, 0U - u32(dy));
    }
    if ((word(state.actors.at(speaker).storage, 4) & 0x200U) != 0 && (flags & 1U) == 0)
        window.set_half(Window::cleared, 0);

    // 8009ccf8 and the handler's tail.
    state.talk_inhibited = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(state.talk_inhibited) | 1U << (index & 31U));
    put(self.actor, 0x104, word(self.actor, 0x104, 2) | 0x8000U, 2);
    set_pc(self, pc(self) + 4);
    return 0;
}

void Program::play_sound_effect(std::uint32_t id, std::uint32_t channel) {
    const auto pair = (channel & 7) << 1;
    if (id == 0) {
        resident::stop_effect_pair(resident.sound, pair);
        return;
    }
    field->last_sound_effect = id;
    // 800855c8 with volume 7f and pan 40.
    resident::stop_effect_pair(resident.sound, pair);
    resident::start_effect(resident.sound, id, pair, 0x7f, 0x40);
}

// Resident 8001b66c: stop the playing sequence and forget the loaded pair.
void Program::stop_music() {
    auto &music = resident.music;
    if (music.completed != 0) {
        // 8001b5e8: stop the active sequence; release it unless it is kept
        // for reuse, in which case it becomes the cached sequence.
        if (music.sequence_active == 1) {
            resident::stop_sequence(resident.sound, music.current_sequence);
            if (music.reuse_sequence == 0)
                resident::release_sequence(resident.sound, music.current_sequence);
            else
                music.cached_sequence = music.current_sequence;
            music.sequence_active = 0;
            music.reuse_sequence = 0;
        }
        // 8001b5a8: release the transferred wave bank.
        if (music.wave_loaded_now == 1) {
            resident::release_wave_bank(resident.sound, music.wave_transfer);
            music.wave_loaded_now = 0;
        }
    }
    music.loaded_wave_bank = 0xffffffffU;
    music.loaded_sequence = 0xffffffffU;
    music.completed = 0;
}

// Field 80085eec: stop and release the cached sequence (8004f2fc).
void Program::release_cached_sequence() {
    auto &music = resident.music;
    if (music.cached_sequence == 0)
        return;
    resident::stop_sequence(resident.sound, music.cached_sequence);    // 80039c4c
    resident::release_sequence(resident.sound, music.cached_sequence); // 800399d4
    music.cached_sequence = 0;
}

// Field 8008f76c / 8008f7b8 (primary 75): request field music.
void Program::change_music(field::EventContext &context, std::uint32_t start_parameter) {
    auto &music = resident.music;
    if (resident.battle_request.field_active == 0) {
        context.control.break_requested = 1;
        return;
    }
    music.start_parameter = start_parameter;
    script(context, [&](field::FieldWorld &world) {
        auto &self = record(world, world.current);
        const auto id = u32(field::read_immediate15_or_variable(world, 1));
        if (world.control.post_initialization == 0) {
            release_cached_sequence(); // 80085eec
            if (id != music.requested) {
                stop_music();
                music.gate = 0xffffffffU;
            }
            music.requested = id;
            set_pc(self, pc(self) + 3);
            return;
        }
        if (disc_idle_query() != 0 || resident.battle_request.field_active == 0 ||
            music.wave_pending == 1 || music.gate == 0xffffffffU) {
            world.control.break_requested = 1;
            return;
        }
        if (id != music.requested) {
            stop_music();
            music.requested = id;
            music.gate = 0xffffffffU;
            load_music(id);
        }
        set_pc(self, pc(self) + 3);
    });
}

// A byte of the field overlay's constant data, from the qualified source.
std::uint8_t Program::overlay_byte(std::uint32_t address) const {
    const auto offset = static_cast<std::size_t>(address - field_overlay_base);
    if (!field || offset >= field->overlay.size())
        throw field::FieldFormatError("Field overlay read outside the qualified source image");
    if (offset >= field->overlay_verified.size() || !field->overlay_verified[offset])
        throw field::FieldFormatError("Loaded field overlay byte differs from its source");
    return field->overlay[offset];
}

// Field 8009bf8c and 8009c01c.
void Program::show_message(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &self = record(world, world.current);
        const auto speaker = field::resolve_script_actor(world, 1);
        if (speaker == 0xff) {
            set_pc(self, pc(self) + 6);
            return;
        }
        self.actor[0x80] = record(world, static_cast<std::size_t>(speaker)).actor[0x80];
        set_pc(self, pc(self) + 1);
        if (open_dialogue(world, context.pass, static_cast<std::uint32_t>(speaker), 0) == -1)
            set_pc(self, pc(self) - 1);
    });
}

} // namespace xem::reconstruction
