#include "xem/reconstruction/field_script.hpp"

#include "xem/reconstruction/field_actor.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <optional>

namespace xem::reconstruction::field {
namespace {
std::uint32_t read(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width) {
    if (offset > data.size() || width > data.size() - offset)
        throw EventError("Script handler read exceeds original record storage");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return value;
}
void write(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
           std::size_t width) {
    if (offset > data.size() || width > data.size() - offset)
        throw EventError("Script handler write exceeds original record storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }

FieldRecord &at(FieldWorld &world, std::int64_t index) {
    if (index < 0 || static_cast<std::size_t>(index) >= world.actors.size())
        throw EventError("Script actor selector outside the event actor table");
    return world.actors[static_cast<std::size_t>(index)];
}
std::uint32_t pc(const FieldWorld &world) {
    return read(world.actors[world.current].actor, 0xcc, 2);
}
void set_pc(FieldWorld &world, std::uint32_t value) {
    write(world.actors[world.current].actor, 0xcc, value, 2);
}
std::uint16_t u16(const FieldWorld &world, std::uint32_t offset) {
    return world.program.word(pc(world) + offset);
}
// 800acdec: bit 15 selects an unsigned 15-bit immediate, otherwise a variable.
std::int32_t immediate15_or_variable(const FieldWorld &world, std::uint32_t offset) {
    const auto raw = u16(world, offset);
    return (raw & 0x8000U) != 0 ? static_cast<std::int32_t>(raw & 0x7fffU)
                                : world.variables.read(raw);
}
// 8009cfbc and its siblings: one selector bit chooses a signed immediate.
std::int32_t selected(const FieldWorld &world, std::uint32_t offset, std::uint32_t mode,
                      std::uint32_t bit) {
    const auto raw = u16(world, offset);
    return (mode & bit) != 0 ? s16(raw) : world.variables.read(raw);
}
std::int16_t position(const FieldRecord &actor, std::size_t axis) {
    return static_cast<std::int16_t>(read(actor.actor, 0x22 + axis * 4, 2));
}
// Resident 8004a414 (Square0) truncates each component into a GTE IR register.
std::uint32_t square(std::int32_t value) {
    const auto component = s16(static_cast<std::uint32_t>(value));
    return static_cast<std::uint32_t>(component * component);
}

// 80095734: controlled actor X/Z against a four-vertex component-8 zone.
void zone_test(FieldWorld &world) {
    const auto record = static_cast<std::size_t>(world.program.byte(pc(world) + 1U)) * 24U;
    if (record > world.zones.size() || world.zones.size() - record < 24)
        throw EventError("Trigger zone outside field component 8");
    const auto zone = world.zones.subspan(record, 24);
    const auto packed = [](std::int32_t x, std::int32_t z) {
        return (static_cast<std::uint32_t>(z) << 16U) + static_cast<std::uint32_t>(x);
    };
    std::array<std::uint32_t, 4> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i)
        vertices[i] = packed(s16(read(zone, i * 6, 2)), s16(read(zone, i * 6 + 4, 2)));
    const auto &controlled = at(world, world.controlled);
    const auto point = packed(position(controlled, 0), position(controlled, 2));
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (field_packed_area({vertices[i], vertices[(i + 1) % 4], point}) < 0) {
            set_pc(world, u16(world, 2));
            ++world.control.batch_limit;
            return;
        }
    }
    set_pc(world, pc(world) + 4U);
}

// 80095e48: distance from the current descriptor's actor to a selected actor.
void proximity_branch(FieldWorld &world) {
    const auto target = resolve_script_actor(world, 1);
    if (target != 0xff) {
        const auto &self = at(world, static_cast<std::int64_t>(world.current_descriptor));
        const auto &other = at(world, target);
        const auto distance = script_distance(
            position(self, 0) - position(other, 0), position(self, 1) - position(other, 1),
            position(self, 2) - position(other, 2), world.math.square_root);
        if (distance < immediate15_or_variable(world, 2)) {
            set_pc(world, pc(world) + 6U);
            return;
        }
    }
    set_pc(world, u16(world, 4));
}

// 8009dac4: hide an actor, disable its event descriptor and release a
// dialogue window still owned by the current actor.
void hide_actor(FieldWorld &world) {
    const auto target = resolve_script_actor(world, 1);
    if (target != 0xff) {
        auto &actor = at(world, target);
        write(actor.actor, 0, read(actor.actor, 0, 4) | 1U, 4);
        write(actor.actor, 4, read(actor.actor, 4, 4) | 0x100000U, 4);
        write(actor.descriptor, 0x58, read(actor.descriptor, 0x58, 2) | 0x20U, 2);
        for (auto &window : world.dialogue) {
            if (window.half(DialogueWindow::owner) == static_cast<std::int32_t>(world.current) &&
                window.half(DialogueWindow::busy) == 0) {
                window.set_half(DialogueWindow::cleared, 0);
                break;
            }
        }
    }
    set_pc(world, pc(world) + 2U);
}

// Field 8007b1c4 on one collision layer; returns the located point's height.
// Callers differ: some store the located triangle in the actor's layer slot.
std::int32_t floor_height(FieldWorld &world, FieldRecord &actor, std::int32_t layer, std::int32_t x,
                          std::int32_t z, bool store_triangle) {
    if (layer < 0 || layer >= 4 || static_cast<std::size_t>(layer) >= world.collision.layers.size())
        throw EventError("Script floor query outside the recovered collision layers");
    const auto slot = static_cast<std::size_t>(layer);
    const auto found =
        locate_initial_floor(world.collision.layers[slot], world.triangle_counts[slot],
                             s16(static_cast<std::uint32_t>(x)), s16(static_cast<std::uint32_t>(z)),
                             world.math.reciprocal);
    if (store_triangle)
        write(actor.actor, 8 + slot * 2, found.triangle, 2);
    return s16(static_cast<std::uint32_t>(found.point[1]));
}
std::uint32_t word(const FieldRecord &actor, std::size_t offset) {
    return read(actor.actor, offset, 4);
}
void put(FieldRecord &actor, std::size_t offset, std::uint32_t value) {
    write(actor.actor, offset, value, 4);
}
std::int32_t quotient(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0)
        throw EventError("Original MIPS division by zero is not a recovered result");
    if (numerator == INT32_MIN && denominator == -1)
        return numerator;
    return numerator / denominator;
}

// 80099214: scripted arc movement. Setup (low bits 0) computes per-step
// velocity; mode 3 advances one step per call, then resolves the target from
// the setup instruction eleven bytes earlier; exact selector 0f re-queries floors.
void scripted_arc(FieldWorld &world) {
    auto &self = world.actors[world.current];
    if (self.sprite.bytes.size() < 0x20)
        throw EventError("Scripted arc requires the current actor sprite");
    auto &sprite = self.sprite.bytes;
    const auto origin = pc(world);
    put(self, 0, word(self, 0) | 0x10000U);
    const auto mode = world.program.byte(origin + 1U);
    const auto finish = [&] {
        for (std::size_t axis = 0; axis < 3; ++axis)
            write(self.descriptor, 0x20 + axis * 4,
                  static_cast<std::uint32_t>(s16(read(self.actor, 0x22 + axis * 4, 2))), 4);
        for (std::size_t axis = 0; axis < 3; ++axis)
            write(sprite, axis * 4, word(self, 0x20 + axis * 4), 4);
        write(self.actor, 0x102, read(self.actor, 0x102, 2) + 1U, 2);
        world.control.break_requested = 1;
    };
    // Setup stores the queried layer; the final step stores the located triangle.
    const auto target = [&](std::uint32_t selector, bool setup) {
        const auto x = selected(world, 2, selector, 0x80);
        const auto z = selected(world, 4, selector, 0x40);
        std::int32_t y = 0;
        if ((world.program.byte(pc(world) + 1U) & 0x80U) == 0) {
            y = selected(world, 6, selector, 0x20);
        } else {
            const auto layer = selected(world, 6, selector, 0x20);
            y = floor_height(world, self, layer, x, z, !setup);
            if (setup)
                write(self.actor, 0x10, static_cast<std::uint32_t>(layer), 2);
        }
        return std::array{x, y, z};
    };
    switch (mode & 3U) {
    case 0: {
        auto steps = selected(world, 8, world.program.byte(origin + 10U), 0x10);
        if (steps == 0)
            steps = 1;
        const auto [x, y, z] = target(world.program.byte(origin + 10U), true);
        const auto gravity = s32(read(sprite, 0x1c, 4));
        const auto fall =
            s32(static_cast<std::uint32_t>(gravity) * static_cast<std::uint32_t>(steps));
        auto velocity = -quotient(fall, 2);
        write(sprite, 0x10, static_cast<std::uint32_t>(velocity), 4);
        velocity += quotient(s32((static_cast<std::uint32_t>(y) << 16U) - word(self, 0x24)), steps);
        write(sprite, 0x10, static_cast<std::uint32_t>(velocity), 4);
        const auto x_step =
            quotient(s32((static_cast<std::uint32_t>(x) << 16U) - word(self, 0x20)), steps + 1);
        const auto z_step =
            quotient(s32((static_cast<std::uint32_t>(z) << 16U) - word(self, 0x28)), steps + 1);
        put(self, 0xd4, 0);
        write(self.actor, 0xe0, static_cast<std::uint32_t>(steps), 2);
        write(self.actor, 0x102, 0, 2);
        set_pc(world, origin + 0xbU);
        put(self, 0xd0, static_cast<std::uint32_t>(x_step));
        put(self, 0xd8, static_cast<std::uint32_t>(z_step));
        world.control.break_requested = 1;
        return;
    }
    case 3:
        break;
    default:
        throw EventError("Scripted arc distance modes 1 and 2 are not yet recovered");
    }
    if (mode == 0x0f) {
        for (std::int32_t layer = 0; layer < world.layer_count - 1; ++layer)
            static_cast<void>(
                floor_height(world, self, layer, position(self, 0), position(self, 2), true));
        put(self, 0, word(self, 0) & 0xfffeffffU);
        put(self, 4, word(self, 4) & 0xffdfffffU);
        set_pc(world, origin + 2U);
        world.control.break_requested = 1;
        return;
    }
    if (s16(read(self.actor, 0x102, 2)) < s16(read(self.actor, 0xe0, 2))) {
        put(self, 0x20, word(self, 0x20) + word(self, 0xd0));
        put(self, 0x28, word(self, 0x28) + word(self, 0xd8));
        put(self, 0x24, word(self, 0x24) + read(sprite, 0x10, 4));
        write(sprite, 0x10, read(sprite, 0x10, 4) + read(sprite, 0x1c, 4), 4);
        if ((word(self, 0xd0) != 0 || word(self, 0xd8) != 0) && (word(self, 0) & 0x8000U) == 0) {
            const auto angle =
                collision_atan(s32(word(self, 0xd8)), s32(word(self, 0xd0)), world.math.angle);
            const auto direction =
                ((0U - static_cast<std::uint32_t>(angle.angle)) & 0xfffU) | 0x8000U;
            write(self.actor, 0x104, direction, 2);
            write(self.actor, 0x106, direction, 2);
        }
    } else {
        set_pc(world, (origin - 0xbU) & 0xffffU);
        const auto [x, y, z] = target(world.program.byte(pc(world) + 10U), false);
        write(sprite, 0x10, 0, 4);
        put(self, 0x20, static_cast<std::uint32_t>(x) << 16U);
        put(self, 0x24, static_cast<std::uint32_t>(y) << 16U);
        put(self, 0x28, static_cast<std::uint32_t>(z) << 16U);
        put(self, 0, word(self, 0) & 0xfffeffffU);
        set_pc(world, pc(world) + 0xdU);
        put(self, 4, word(self, 4) & 0xffdfffffU);
    }
    finish();
}

// 8009cd18: the first window owned by the current actor and not busy.
std::optional<std::size_t> owned_window(const FieldWorld &world) {
    for (std::size_t i = 0; i < world.dialogue.size(); ++i)
        if (world.dialogue[i].half(DialogueWindow::owner) ==
                static_cast<std::int32_t>(world.current) &&
            world.dialogue[i].half(DialogueWindow::busy) == 0)
            return i;
    return std::nullopt;
}

// 8009bb0c: wait for the current actor's dialogue window to finish.
void wait_dialogue(FieldWorld &world) {
    auto &self = world.actors[world.current];
    const auto window = owned_window(world);
    if (!window) {
        world.control.batch_limit += 8;
        // 800a3074 stores the byte directly into the bank's halfword.
        world.variables.words[0x14 >> 1] = self.actor[0x81];
        set_pc(world, pc(world) + 1U);
        return;
    }
    auto &state = world.dialogue[*window];
    const auto &speaker = at(world, state.half(DialogueWindow::speaker));
    if ((read(speaker.actor, 4, 4) & 0x200U) != 0) {
        const auto progress = read(self.actor, 0x84, 4);
        const auto step = (progress >> 16U) != 0 ? progress >> 16U : progress & 0xffffU;
        if ((step & 1U) == 0) {
            const auto slot = self.actor[0xce];
            if (slot >= 8)
                throw EventError("Dialogue wait selected an invalid event slot");
            const auto control = read(self.actor, 0x90 + slot * 8U, 4);
            if (((control >> 18U) & 15U) != 7U) {
                // Opcode 00's body (800a1b70) ends the selected slot.
                write(self.actor, 0x90 + slot * 8U, control | 0x003c0000U, 4);
                self.actor[0x8f + slot * 8U] = 255;
                world.control.break_requested = 1;
                world.control.budget_mode = 1;
            }
            state.set_half(DialogueWindow::cleared, 0);
        }
    }
    world.control.break_requested = 1;
}

// 8009524c -> 80095284: stop the current actor's planar and additive motion.
void stop_actor(FieldWorld &world) {
    auto &self = world.actors[world.current];
    if (self.sprite.bytes.size() < 0x1c)
        throw EventError("Stopping an actor requires its sprite");
    world.control.break_requested = 1;
    for (const std::size_t offset : {0x30U, 0x34U, 0x38U, 0x40U, 0x44U, 0x48U})
        write(self.actor, offset, 0, 4);
    const auto direction = read(self.actor, 0x104, 2) | 0x8000U;
    write(self.actor, 0x106, direction, 2);
    write(self.actor, 0x104, direction, 2);
    for (const std::size_t offset : {0x0cU, 0x14U, 0x18U})
        write(self.sprite.bytes, offset, 0, 4);
    set_pc(world, pc(world) + 1U);
}

// 8009eb78: request an event (tag low five bits, priority high three) on
// another actor. With no free slot the PC stays, so the request retries.
void request_event(FieldWorld &world) {
    const auto target = resolve_script_actor(world, 1);
    if (target != 0xff) {
        auto &self = world.actors[world.current];
        auto &other = at(world, target);
        const auto code = world.program.byte(pc(world) + 2U);
        const auto tag = static_cast<std::uint8_t>(code & 0x1fU);
        if ((read(other.actor, 4, 4) & 0x100000U) != 0) {
            const auto slot = self.actor[0xce];
            const auto linked = self.actor[0xcf];
            if (slot >= 8 || linked >= 8)
                throw EventError("Event request selected an invalid slot");
            write(self.actor, 0x90 + slot * 8U, read(self.actor, 0x90 + slot * 8U, 4) & 0xfffcffffU,
                  4);
            write(other.actor, 0x90 + linked * 8U,
                  read(other.actor, 0x90 + linked * 8U, 4) & 0xffbfffffU, 4);
        } else {
            // 8009eb48: an existing slot with this tag suppresses the request.
            bool present = false;
            for (std::size_t slot = 0; slot < 8; ++slot)
                present = present || other.actor[0x8f + slot * 8] == tag;
            if (!present) {
                std::optional<std::size_t> free;
                for (std::size_t slot = 0; slot < 8 && !free; ++slot) {
                    const auto control = read(other.actor, 0x90 + slot * 8, 4);
                    if (((control >> 18U) & 15U) == 15U && ((control >> 22U) & 1U) == 0)
                        free = slot;
                }
                if (!free)
                    return;
                write(other.actor, 0x8c + *free * 8, world.program.entry(target, tag), 2);
                write(other.actor, 0x90 + *free * 8,
                      (read(other.actor, 0x90 + *free * 8, 4) & 0xffc3ffffU) |
                          ((static_cast<std::uint32_t>(code) >> 5U) << 18U),
                      4);
                other.actor[0x8f + *free * 8] = tag;
            }
        }
    }
    set_pc(world, pc(world) + 3U);
}

// 8009a34c: begin a camera distance transition toward an operand.
void camera_transition(FieldWorld &world) {
    auto &camera = world.camera;
    camera.steps = world.program.byte(pc(world) + 3U);
    if (camera.steps == 0) {
        camera.steps = 1;
        camera.counter += 2;
    }
    const auto target = immediate15_or_variable(world, 1);
    const auto delta = s32(0U - (static_cast<std::uint32_t>(camera.distance - target) << 16U));
    camera.step = delta / static_cast<std::int16_t>(camera.steps);
    camera.start = s32(static_cast<std::uint32_t>(camera.distance) << 16U);
    camera.flags |= 1U;
    set_pc(world, pc(world) + 4U);
}

// 8008fc4c: camera mode handshake. Busy modes retry; a zero operand in mode
// one ends the mode and, as in the original, advances the PC twice.
void camera_mode(FieldWorld &world) {
    auto &camera = world.camera;
    if (camera.mode == 1) {
        const auto value = immediate15_or_variable(world, 1);
        if (value == 0) {
            camera.mode = 0;
            camera.flags &= 0x7fffU;
            set_pc(world, pc(world) + 3U);
            camera.counter = 2;
        } else {
            camera.mode = 2;
            camera.target_a = value;
            camera.target_b = value;
        }
    } else if (camera.mode != 0) {
        return;
    } else {
        camera.flags &= 0x7fffU;
    }
    set_pc(world, pc(world) + 3U);
}

// 8009ba7c: camera elevation, heading octant and projection distance.
void camera_setup(FieldWorld &world) {
    auto &camera = world.camera;
    camera.elevation = static_cast<std::uint16_t>(immediate15_or_variable(world, 3));
    const auto octant = static_cast<std::uint32_t>(immediate15_or_variable(world, 1) + 4) & 7U;
    camera.heading = octant << 9U;
    camera.heading_half = static_cast<std::uint16_t>(octant << 9U);
    camera.heading_high = octant << 25U;
    camera.projection = immediate15_or_variable(world, 5);
    // 8004a14c SetGeomScreen: CTC2 to H keeps the low halfword.
    world.gte_screen.h = static_cast<std::uint16_t>(camera.projection);
    set_pc(world, pc(world) + 7U);
}

// 8007d93c(0): a full-screen semi-transparent tile, via the resident setTile
// (80043d64) and SetSemiTrans (80043bfc) byte edits, copied to its second
// buffer, and the fade counters reset.
void prepare_fade(FieldFade &fade) {
    auto &packet = fade.packets;
    packet[3] = 3;
    packet[7] = 0x60;
    packet[7] = static_cast<std::uint8_t>(packet[7] | 2U);
    write(packet, 0xc, 0x140, 2);
    write(packet, 0xe, 0xe0, 2);
    write(packet, 0x8, 0, 2);
    write(packet, 0xa, 0, 2);
    std::copy_n(packet.begin(), 16, packet.begin() + 16);
    fade.halves[1] = 0;
    fade.halves[2] = 0;
    fade.words[2] = 0;
    fade.words[1] = 0;
    fade.words[0] = 0;
    fade.halves[0] = 2;
}

// 8009731c -> 8007d93c, 80071e58: prepare the tile and start a fade-out once.
void start_fade_out(FieldWorld &world) {
    prepare_fade(world.fade);
    const auto frames = immediate15_or_variable(world, 1);
    auto &fade = world.fade;
    if (fade.started != 0) {
        fade.started = 0;
        if (fade.mode == 2) {
            if (frames == 0)
                throw EventError("Original fade division by zero is not a recovered result");
            const auto rate = static_cast<std::uint32_t>(-0x10000 / frames);
            fade.words = {0xff00, 0xff00, 0xff00, rate, rate, rate};
            fade.halves = {2, 1, static_cast<std::uint16_t>(frames)};
        }
    }
    set_pc(world, pc(world) + 3U);
}

// 80097364 -> 80071dcc: start a timed screen fade once.
void start_fade(FieldWorld &world) {
    const auto frames = immediate15_or_variable(world, 1);
    auto &fade = world.fade;
    if (fade.started != 1) {
        fade.started = 1;
        if (fade.mode == 2) {
            if (frames == 0)
                throw EventError("Original fade division by zero is not a recovered result");
            const auto rate = static_cast<std::uint32_t>(0xff00 / frames);
            fade.words = {0, 0, 0, rate, rate, rate};
            fade.halves = {2, 1, static_cast<std::uint16_t>(frames)};
        }
    }
    set_pc(world, pc(world) + 3U);
}

// 8009d6d8 / 8009d768: variable multiply and divide; divisor zero becomes one.
void arithmetic(FieldWorld &world, bool divide) {
    const auto reference = u16(world, 1);
    const auto left = world.variables.read(reference);
    auto right = selected(world, 3, world.program.byte(pc(world) + 5U), 0x40);
    std::int32_t value = 0;
    if (divide) {
        if (right == 0)
            right = 1;
        value = left / right; // Both operands are 16-bit, so MIPS div cannot overflow.
    } else {
        value = s32(static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }
    world.variables.write(reference, value);
    set_pc(world, pc(world) + 6U);
}
} // namespace

std::int32_t resolve_script_actor(const FieldWorld &world, std::uint32_t offset) {
    const auto selector = world.program.byte(pc(world) + offset);
    switch (selector) {
    case 0xff:
        return world.party[0];
    case 0xfe:
        return world.party[1];
    case 0xfd:
        return world.party[2];
    case 0xfb:
        return static_cast<std::int32_t>(world.current);
    default:
        return selector;
    }
}

std::int32_t script_distance(std::int32_t dx, std::int32_t dy, std::int32_t dz,
                             std::span<const std::int16_t> square_root) {
    const auto sum = square(dx) + square(dy) + square(dz);
    return collision_sqrt(s32(sum), square_root).value;
}

std::int32_t read_immediate15_or_variable(const FieldWorld &world, std::uint32_t offset) {
    return immediate15_or_variable(world, offset);
}
std::int32_t read_selected(const FieldWorld &world, std::uint32_t offset, std::uint32_t flags,
                           std::uint32_t bit) {
    return selected(world, offset, flags, bit);
}

void execute_script_instruction(FieldWorld &world, std::uint8_t opcode) {
    if (world.current >= world.actors.size())
        throw EventError("Script handler requires the current event actor");
    if (world.program.byte(pc(world)) != opcode)
        throw EventError("Dispatched script opcode disagrees with the working PC");
    switch (opcode) {
    case 0x07:
        request_event(world);
        return;
    case 0x29:
        hide_actor(world);
        return;
    case 0x9d:
        camera_transition(world);
        return;
    case 0xa0:
        camera_setup(world);
        return;
    case 0x9a:
        camera_mode(world);
        return;
    case 0x87: // 80096790
        world.control.batch_limit += 0x20;
        world.variables.write(0, immediate15_or_variable(world, 1));
        set_pc(world, pc(world) + 3U);
        return;
    case 0xb4:
        start_fade(world);
        return;
    case 0xb3:
        start_fade_out(world);
        return;
    case 0x2c: { // 8009a130
        auto &self = world.actors[world.current].actor;
        write(self, 4, read(self, 4, 4) & 0xfeffffffU, 4);
        const auto animation = world.program.byte(pc(world) + 1U);
        set_pc(world, pc(world) + 2U);
        write(self, 0xea, animation, 2);
        return;
    }
    case 0x57:
        scripted_arc(world);
        return;
    case 0x5a:
        stop_actor(world);
        return;
    case 0x9c:
        wait_dialogue(world);
        return;
    case 0x3c: {
        const auto reference = u16(world, 1);
        world.variables.write(reference, world.variables.read(reference) + 1);
        set_pc(world, pc(world) + 3U);
        return;
    }
    case 0x89:
        proximity_branch(world);
        return;
    case 0x5f: { // 8009ad6c: face a table direction; before initialization, at once.
        const auto index = world.program.byte(pc(world) + 1U) + 16U; // 800aea54
        if (index >= world.direction_tables.size())
            throw EventError("Direction operand indexes past the owned direction table");
        const auto direction = world.direction_tables[index] | 0x8000U;
        auto &self = world.actors[world.current].actor;
        write(self, 0x104, direction, 2);
        write(self, 0x106, direction, 2);
        if (world.control.post_initialization == 0)
            write(self, 0x108, direction, 2);
        set_pc(world, pc(world) + 2U);
        return;
    }
    case 0x61:   // 8008fe2c: set the target point.
    case 0x63:   // 8008ff90: set actor point A.
    case 0x65: { // 800900c4: set the eye point.
        const auto flags = world.program.byte(pc(world) + 7U);
        const auto first = selected(world, 1, flags, 0x80);
        const auto second = selected(world, 3, flags, 0x40);
        const auto third = selected(world, 5, flags, 0x20);
        auto &point = opcode == 0x61   ? world.camera.saved_target
                      : opcode == 0x63 ? world.camera.point_actor_a
                                       : world.camera.saved_eye;
        const auto at = [](std::int32_t value) {
            return s32(static_cast<std::uint32_t>(value) << 16U);
        };
        // X, then Z from the second operand, then Y from the third.
        point[0] = at(first);
        point[2] = at(second);
        point[1] = at(third);
        world.control.batch_limit += 1;
        set_pc(world, pc(world) + 8U);
        return;
    }
    case 0x62:   // 8008ff04: point A from an actor's position.
    case 0x66: { // 8009019c: point B from an actor's position.
        auto actor = resolve_script_actor(world, 1);
        if (actor == 0xff)
            actor = world.party[0]; // 8009cd7c
        if (actor < 0 || static_cast<std::size_t>(actor) >= world.actors.size())
            throw EventError("Camera point actor outside the event actors");
        const auto &source = world.actors[static_cast<std::size_t>(actor)].actor;
        auto &point = opcode == 0x62 ? world.camera.point_actor_a : world.camera.point_actor_b;
        for (std::size_t axis = 0; axis < 3; ++axis)
            point[axis] = s32(read(source, 0x20 + axis * 4, 4));
        world.control.batch_limit += 1;
        set_pc(world, pc(world) + 2U);
        return;
    }
    case 0x64: // 80090068: save the camera eye goal.
        world.camera.saved_eye = world.camera.eye_goal;
        world.control.batch_limit += 1;
        set_pc(world, pc(world) + 1U);
        return;
    case 0x18: { // 8009e83c: nonzero bytes set bounds +18, +1c, +1a, +1e (doubled).
        auto &self = world.actors[world.current].actor;
        const std::array<std::size_t, 4> fields{0x18, 0x1c, 0x1a, 0x1e};
        for (std::size_t i = 0; i < 4; ++i)
            if (const auto value =
                    world.program.byte(pc(world) + 1U + static_cast<std::uint32_t>(i));
                value != 0)
                write(self, fields[i], static_cast<std::uint32_t>(value) << 1U, 2);
        set_pc(world, pc(world) + 5U);
        return;
    }
    case 0x1e: { // 8009e208: take the floor from the actor's own height.
        auto &self = world.actors[world.current].actor;
        const auto height = read(self, 0x26, 2);
        write(self, 0xec, 0, 2);
        write(self, 0, (read(self, 0, 4) & 0xfffbffffU) | 0x400000U, 4);
        set_pc(world, pc(world) + 1U);
        write(self, 0x72, height, 2);
        return;
    }
    case 0x1f: { // 8009e1a0: layer flags bits 0-2 and 3-5 from one byte.
        auto &self = world.actors[world.current].actor;
        const auto value = world.program.byte(pc(world) + 1U);
        const auto flags = read(self, 4, 4);
        set_pc(world, pc(world) + 2U);
        write(self, 4, (flags & ~0x3fU) | (value & 7U) | ((value >> 1U) & 0x38U), 4);
        return;
    }
    case 0x20: { // 8009e10c: actor flags from a packed operand.
        const auto value = static_cast<std::uint32_t>(immediate15_or_variable(world, 1));
        auto bits = (value & 1U) << 7U;
        if ((value & 4U) != 0)
            bits |= 0x20U;
        if ((value & 8U) != 0)
            bits |= 0x10U;
        if ((value & 0x10U) != 0)
            bits |= 8U;
        if ((value & 0x20U) != 0)
            bits |= 4U;
        if ((value & 0x40U) != 0)
            bits |= 0x8000000U;
        auto &self = world.actors[world.current].actor;
        write(self, 0, (read(self, 0, 4) & 0xf7ffff43U) | bits, 4);
        set_pc(world, pc(world) + 3U);
        return;
    }
    case 0x2a: { // 8009da1c
        auto &self = world.actors[world.current].actor;
        write(self, 0, read(self, 0, 4) | 0x20000U, 4);
        set_pc(world, pc(world) + 1U);
        return;
    }
    case 0x84: { // 80096644: jump unless variable 0 is below the operand.
        const auto limit = immediate15_or_variable(world, 1);
        if (world.variables.read(0) < limit)
            set_pc(world, pc(world) + 5U);
        else
            set_pc(world, u16(world, 3));
        return;
    }
    case 0xa3: { // 80090228: set actor point B.
        const auto flags = world.program.byte(pc(world) + 7U);
        const auto at = [](std::int32_t value) {
            return s32(static_cast<std::uint32_t>(value) << 16U);
        };
        world.camera.point_actor_b[0] = at(selected(world, 1, flags, 0x80));
        world.camera.point_actor_b[2] = at(selected(world, 3, flags, 0x40));
        world.camera.point_actor_b[1] = at(selected(world, 5, flags, 0x20));
        world.control.batch_limit += 1;
        set_pc(world, pc(world) + 8U);
        return;
    }
    case 0xf8: { // 8008e59c: set or clear halves of actor +0 or +4.
        const auto value = static_cast<std::uint32_t>(u16(world, 2));
        const auto selector = world.program.byte(pc(world) + 1U);
        auto &self = world.actors[world.current].actor;
        if (selector < 8) {
            const auto offset = (selector & 2U) != 0 ? 4U : 0U;
            const auto bits = (selector & 1U) != 0 ? value << 16U : value;
            const auto current = read(self, offset, 4);
            write(self, offset, selector < 4 ? current | bits : current & ~bits, 4);
        }
        set_pc(world, pc(world) + 4U);
        return;
    }
    case 0xfb: { // 8008d780: jump unless a variable bit is set.
        const auto reference = u16(world, 1);
        const auto value = world.variables.read(static_cast<std::uint16_t>(reference >> 4U));
        if ((static_cast<std::uint32_t>(value) & (1U << (reference & 15U))) == 0)
            set_pc(world, u16(world, 3));
        else
            set_pc(world, pc(world) + 5U);
        return;
    }
    case 0xac: { // 800903bc: start a scripted target (0, 2) or eye (1, 3) move.
        const auto selector = world.program.byte(pc(world) + 1U);
        const auto mode = selector & 0xfU;
        auto &c = world.camera;
        const bool eye = (mode & 1U) != 0;
        auto &from = eye ? c.saved_eye : c.saved_target;
        const auto &to = eye ? c.point_actor_b : c.point_actor_a;
        auto &moving = eye ? c.scripted_eye : c.scripted_target;
        auto &step = eye ? c.eye_step : c.target_step;
        auto &steps = eye ? c.eye_steps : c.target_steps;
        auto &follow = eye ? c.eye : c.target;
        if (mode <= 1) {
            const auto count = static_cast<std::uint32_t>(immediate15_or_variable(world, 2));
            steps = static_cast<std::int16_t>(count);
            if ((count & 0xffffU) == 0) {
                steps = static_cast<std::int16_t>(steps + 1);
                (eye ? c.target_b : c.target_a) = 1;
            }
            for (std::size_t axis = 0; axis < 3; ++axis)
                step[axis] = quotient(s32(static_cast<std::uint32_t>(to[axis]) -
                                          static_cast<std::uint32_t>(from[axis])),
                                      steps);
        } else if (mode <= 3) {
            GteLong away{};
            for (std::size_t axis = 0; axis < 3; ++axis)
                away[axis] = s32(static_cast<std::uint32_t>(from[axis]) -
                                 static_cast<std::uint32_t>(to[axis])) >>
                             16;
            const auto direction = normalize_field_vector(away, world.math.reciprocal);
            const auto distance =
                script_distance(away[0], away[1], away[2], world.math.square_root);
            const auto speed = immediate15_or_variable(world, 2);
            for (std::size_t axis = 0; axis < 3; ++axis)
                step[axis] = s32(static_cast<std::uint32_t>(direction[axis]) *
                                 static_cast<std::uint32_t>(speed) * 0xfffffff0U);
            steps = static_cast<std::int16_t>(quotient(distance, speed));
        }
        if (mode <= 3) {
            moving = from;
            c.scripted |= eye ? 2U : 1U;
            if ((selector & 0x80U) != 0)
                follow = from;
        }
        set_pc(world, pc(world) + 4U);
        return;
    }
    case 0x60: // 8008fdd0: save the camera target goal.
        world.camera.saved_target = world.camera.target_goal;
        world.control.batch_limit += 1;
        set_pc(world, pc(world) + 1U);
        return;
    case 0x99: // 8008fb98: enter scripted camera mode from the current view.
        world.camera.mode = 1;
        world.control.batch_limit += 4;
        set_pc(world, pc(world) + 1U);
        world.camera.scripted_scale = 0x1000;
        world.camera.target_a = 0xc;
        world.camera.target_b = 0xc;
        world.camera.scripted_heading = world.camera.heading_half;
        world.camera.flags |= 0x8000U;
        world.camera.scripted_elevation = world.camera.elevation;
        world.camera.scripted_zoom =
            s32(static_cast<std::uint32_t>(world.camera.projection) *
                static_cast<std::uint32_t>(static_cast<std::int32_t>(world.camera.distance))) >>
            12;
        return;
    case 0xef: { // 8008fa38: wait while selected scripted camera moves run.
        const auto wait = static_cast<std::uint32_t>(immediate15_or_variable(world, 1));
        std::uint32_t running = world.camera.target_steps != 0 ? 3U : 2U;
        if (world.camera.eye_steps == 0)
            running &= 1U;
        world.control.break_requested = 1;
        if ((running & wait) == 0)
            set_pc(world, pc(world) + 3U);
        return;
    }
    case 0xf2: { // 8008f90c: camera shake toward the given amplitudes.
        const auto x = immediate15_or_variable(world, 1);
        const auto z = immediate15_or_variable(world, 3);
        const auto y = immediate15_or_variable(world, 5);
        auto steps = immediate15_or_variable(world, 7);
        if (steps == 0)
            steps = 1;
        set_pc(world, pc(world) + 9U);
        auto &c = world.camera;
        const auto toward = [&](std::int32_t target, std::int32_t from) {
            return quotient(
                s32((static_cast<std::uint32_t>(target) << 16U) - static_cast<std::uint32_t>(from)),
                steps);
        };
        const auto step_x = toward(x, c.shake_amplitude[0]);
        const auto step_y = toward(y, c.shake_amplitude[1]);
        // The original subtracts the Y amplitude for Z as well.
        const auto step_z = toward(z, c.shake_amplitude[1]);
        c.shake_time = static_cast<std::int16_t>(steps);
        c.shake = 1;
        c.shake_step = {step_x, step_y, step_z};
        if (x == 0 && y == 0 && z == 0) {
            c.shake_time = static_cast<std::int16_t>(steps + 2);
            c.shake_stop = 1;
        } else {
            c.shake_stop = 0;
        }
        return;
    }
    case 0xd9: { // 80094764: set actor +12c bits 0-1 and store +70.
        auto &self = world.actors[world.current].actor;
        write(self, 0x12c, read(self, 0x12c, 4) | 3U, 4);
        const auto value = immediate15_or_variable(world, 1);
        write(self, 0x70, static_cast<std::uint32_t>(value), 2);
        set_pc(world, pc(world) + 3U);
        return;
    }
    case 0xc9:
        zone_test(world);
        return;
    case 0xde:
    case 0xdf:
        arithmetic(world, opcode == 0xdf);
        return;
    default:
        throw UnsupportedInstruction(static_cast<std::uint16_t>(pc(world)), opcode);
    }
}

void execute_script_extended(FieldWorld &world, std::uint8_t opcode) {
    if (world.program.byte(pc(world)) != opcode)
        throw EventError("Dispatched extended opcode disagrees with the working PC");
    switch (opcode) {
    case 0x53: // 80093ac8
        world.encounter_inhibition = 0;
        world.script_flags_b21d0 = {0, 0};
        world.camera.flags &= 0x3fffU;
        set_pc(world, pc(world) + 1U);
        return;
    case 0x99: // 8008848c
        world.script_flag_b236c = world.program.byte(pc(world) + 1U) ^ 1U;
        set_pc(world, pc(world) + 2U);
        return;
    default:
        throw UnsupportedExtendedInstruction(static_cast<std::uint16_t>(pc(world)), opcode);
    }
}

} // namespace xem::reconstruction::field
