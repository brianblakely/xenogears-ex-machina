// Field event initialization and the event instructions that set actors up:
// sprites made by the field factory (80076ac0), party members, entry points
// and placement. Original addresses name correlations only; owned records
// are Program state.
#include "xem/reconstruction/field_actor.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Event initialization read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Event initialization write exceeds owned storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t s32of(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("Event initialization requires field state");
    return *program.field;
}
void add_pc(std::span<std::uint8_t> actor, std::uint32_t step) {
    put(actor, 0xcc, word(actor, 0xcc, 2) + step, 2);
}
} // namespace

// Field 80076ac0 over the resident heap: allocations at their original call
// sites; releases return the bytes their owner holds (replaced parts, the
// two stacks of an image upload). Image uploads (8002dde4) run LoadImage on
// the stack 8001fb30 switched to, with the event caller's services.
void Program::create_actor_sprite(std::size_t index, const field::FieldSpriteArguments &arguments) {
    auto &state = loaded(*this);
    auto &actor = state.actors.at(index);
    const field::SpriteAllocator allocate = [&](std::uint32_t bytes, std::uint32_t mode,
                                                std::uint32_t site) {
        auto block = resident::heap_allocate(resident.heap, bytes, mode, site);
        if (!block || block->bytes.size() != bytes)
            throw field::FieldFormatError("A sprite allocation failed");
        // 8001fe64 and 8001fb30 each run on a stack at +1efc of their block.
        if (site == 0x8001fe68 || site == 0x8001fb40)
            resident.switched_stacks.push_back({block->address + 0x1f00 - 0x800, 0x804});
        return field::SpriteAllocation{block->address, std::move(block->bytes)};
    };
    auto &upload = resident.sprite_upload;
    const field::SpriteReleaser release = [&](std::uint32_t address) {
        const auto give_back = [&](const field::SpriteAllocation &owner, std::uint32_t site) {
            resident::HeapBlock block{address, owner.bytes};
            if (resident::heap_release(resident.heap, block, site) != 0)
                throw field::FieldFormatError("A sprite block was not released");
        };
        if (address == actor.sprite.parts.address && !actor.sprite.parts.bytes.empty())
            give_back(actor.sprite.parts, 0x80023360);
        else if (address == upload.inner_stack.address && !upload.inner_stack.bytes.empty())
            give_back(upload.inner_stack, 0x8001fb88);
        else if (address == upload.outer_stack.address && !upload.outer_stack.bytes.empty())
            give_back(upload.outer_stack, 0x8001fed0);
        else
            throw MissingDependency({"create_actor_sprite", 0x800320e8, index, {}},
                                    "symbol:sprite-release-owner", false,
                                    "A sprite release of another owner's block is not recovered");
    };
    const field::SpriteImageUploader upload_image = [&](const field::SpriteImageUpload &request) {
        if (event_services_ == nullptr)
            throw MissingDependency({"create_actor_sprite", 0x80044894, index, {}},
                                    "platform:event-gpu-services", false,
                                    "An event's LoadImage needs its caller's platform services");
        // 8002dde4's rectangle is at +10 of its 48h-byte frame on the inner
        // stack.
        auto rect = request.rectangle;
        static_cast<void>(load_image(rect, upload.inner_stack.address + 0x1efc - 0x48 + 0x10,
                                     request.source_address, event_services_));
    };
    field::SpriteServices services{allocate, release, upload_image, &upload};
    std::vector<std::uint32_t> models;
    for (const auto &buffer : resident.sprite_models.buffers)
        models.push_back(buffer.address);
    with_sprite_sources(
        [&](const field::SpriteSources &input) {
            auto sources = input;
            sources.services = &services;
            auto environment = sprite_environment();
            try {
                field::create_field_sprite(actor.sprite, actor.storage, actor.descriptor, arguments,
                                           environment, sources, allocate, release);
            } catch (...) {
                set_sprite_environment(environment);
                throw;
            }
            set_sprite_environment(environment);
        },
        index);
    // A new model buffer's second half is a copy of its first (8002cb54),
    // including what stack frames left in bytes the packets do not write.
    for (const auto &buffer : resident.sprite_models.buffers) {
        if (std::ranges::find(models, buffer.address) != models.end())
            continue;
        const auto half = static_cast<std::uint32_t>(buffer.bytes.size() / 2);
        const auto windows = resident.switched_stacks;
        for (const auto &[low, size] : windows) {
            const auto from = std::max(low, buffer.address);
            const auto to = std::min(low + size, buffer.address + half);
            if (from < to)
                resident.switched_stacks.push_back({from + half, to - from});
        }
    }
}

// Field 800a0c94: the current actor's descriptor and sprite take its position.
void Program::sync_actor_position(std::size_t index) {
    auto &actor = loaded(*this).actors.at(index);
    auto &a = actor.storage;
    for (const std::uint32_t axis : {0U, 4U, 8U}) {
        const auto position = u32(s16(word(a, 0x22 + axis, 2)));
        put(actor.descriptor, 0x20 + axis, position);
        put(actor.descriptor, 0x40 + axis, position);
    }
    auto &sprite = actor.sprite.sprite.bytes;
    if (sprite.size() < 0x88)
        throw field::FieldFormatError("Positioning an actor requires its sprite");
    put(sprite, 0, word(a, 0x20));
    put(sprite, 4, word(a, 0x24));
    put(sprite, 0x10, 0);
    put(sprite, 8, word(a, 0x28));
    put(sprite, 0x84, word(a, 0x26, 2), 2);
    put(a, 0x72, word(a, 0x26, 2), 2);
}

// The bundle's sprite resource `slot`: its offset table after the count.
std::uint32_t Program::bundle_sprite(std::uint32_t slot) {
    const auto bundle = loaded(*this).sprite_bundle_address;
    return bundle + memory(bundle + 4 + 4 * slot);
}

// Field 8009fa54: place the current actor at map entry `entry` of the
// events' entry table (the bytecode's first byte is ff when present):
// x, z, layer, and the facing and camera heading, each ff taking the
// departure's saved octant (variables 8 and 6). Returns 0.
void Program::place_at_entry(std::size_t index, std::int32_t entry) {
    auto &state = loaded(*this);
    const auto &code = state.event_package.bytecode;
    if (code.empty() || code[0] != 0xff)
        return;
    const auto at = u32(entry) * 7U;
    const auto half = [&](std::uint32_t offset) { // 8009e330
        if (offset + 1 >= code.size())
            throw field::FieldFormatError("A map entry lies outside the event bytecode");
        return s16(static_cast<std::uint32_t>(code[offset]) |
                   static_cast<std::uint32_t>(code[offset + 1]) << 8U);
    };
    const auto byte = [&](std::uint32_t offset) -> std::uint32_t {
        if (offset >= code.size())
            throw field::FieldFormatError("A map entry lies outside the event bytecode");
        return code[offset];
    };
    auto &a = state.actors.at(index).storage;
    put(a, 0x10, byte(at + 5), 2);
    snap_actor(index, half(at + 1), half(at + 3));
    auto facing = byte(at + 6);
    std::uint32_t heading = ((facing + 4U) & 7U) << 9U;
    if (facing == 0xff)
        heading = ((u32(resident.variables.read(8)) + 4U) & 7U) << 9U;
    const auto camera = heading & 0xffffU;
    set_memory(0x800af9e6, camera, 2);
    set_memory(0x800afa0c, u32(s16(camera)));
    set_memory(0x800af9f0, camera << 16U);
    const auto turn = byte(at + 7);
    auto direction = (((turn - 2U) & 7U) << 9U) | 0x8000U;
    if (turn == 0xff)
        direction = (((u32(resident.variables.read(6)) - 2U) & 7U) << 9U) | 0x8000U;
    for (const std::uint32_t offset : {0x104U, 0x106U, 0x108U})
        put(a, offset, direction, 2);
}

// 800a0d3c (primary bc): the bundle's first sprite for the current actor.
void Program::event_default_sprite(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        const auto index = world.current;
        create_actor_sprite(
            index, {static_cast<std::uint32_t>(index), 0, bundle_sprite(0), 0, 0, 0x80, 1});
        sync_actor_position(index);
        auto &a = loaded(*this).actors.at(index).storage;
        add_pc(a, 1);
        put(a, 0, word(a, 0) | 0x100U);
        put(a, 4, word(a, 4) | 0x800U);
    });
}

// 800a1624 (primary 0b): bundle sprite `operand 1` for the current actor.
void Program::event_bundle_sprite(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        const auto index = world.current;
        auto &actor = loaded(*this).actors.at(index);
        put(actor.descriptor, 0x58, (word(actor.descriptor, 0x58, 2) & 0xf07fU) | 0x200U, 2);
        const auto slot = u32(field::read_immediate15_or_variable(world, 1));
        create_actor_sprite(index, {static_cast<std::uint32_t>(index), slot, bundle_sprite(slot), 0,
                                    0, static_cast<std::uint8_t>(slot | 0x80U), 0});
        sync_actor_position(index);
        auto &a = actor.storage;
        add_pc(a, 3);
        put(a, 0, (word(a, 0) | 0x100U) & ~0x80U);
        put(a, 4, word(a, 4) & ~0x800U);
        put(actor.descriptor, 0x58, word(actor.descriptor, 0x58, 2) & 0xffdfU, 2);
    });
}

// 800a08b8 (primary 16): the current actor becomes party character `operand
// 1` (ff, fe, fd: the characters of party slots 2, 1, 0). A character in the
// party takes its slot's sprite and, unless it is in the party, the first
// party sprite; party members start at the map entry (variable 2).
void Program::event_party_member(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &state = loaded(*this);
        const auto index = world.current;
        auto &actor = state.actors.at(index);
        auto &a = actor.storage;
        auto character = field::read_immediate15_or_variable(world, 1);
        switch (character) { // 8008cf3c
        case 0xff:
            character = state.party_characters[2];
            break;
        case 0xfe:
            character = state.party_characters[1];
            break;
        case 0xfd:
            character = state.party_characters[0];
            break;
        case 0xfc:
            character = 0xff;
            break;
        default:
            break;
        }
        std::int32_t slot = -1; // 8009fa00
        if (character != 0xff)
            for (std::int32_t k = 0; k < 3; ++k) {
                const auto present = state.party_characters[static_cast<std::size_t>(k)];
                if (present == 0xff)
                    break;
                if (present == character) {
                    slot = k;
                    break;
                }
            }
        put(a, 0xe4, u32(character), 2);
        put(actor.descriptor, 0x58, (word(actor.descriptor, 0x58, 2) & 0xf07fU) | 0x200U, 2);
        const auto self = static_cast<std::uint32_t>(index);
        if (slot == -1) {
            create_actor_sprite(index,
                                {self, 0, resident.party_sprite_resources.at(0), 1, 0, 0, 1});
            put(a, 0, word(a, 0) | 1U);
            world.control.budget_mode = 1;
            world.control.break_requested = 1;
            put(a, 4, word(a, 4) | 0x100000U);
        } else {
            const auto k = static_cast<std::size_t>(slot);
            if (slot == 0) {
                state.controlled_actor = static_cast<std::int32_t>(index);
                state.followed_actor = static_cast<std::uint16_t>(index);
                put(a, 0, (word(a, 0) | 0x4400U) & ~0x80U);
            }
            state.party_indices.at(k) = static_cast<std::int32_t>(index);
            if (state.party_reassignment == 0) {
                create_actor_sprite(index, {self, static_cast<std::uint32_t>(slot),
                                            resident.party_sprite_resources.at(k), 1, 0,
                                            static_cast<std::uint8_t>(slot), 1});
                put(a, 0, (word(a, 0) | 0x400U) & ~0x300U);
            } else {
                throw MissingDependency({"event_party_member", 0x800a09a4, index, {}},
                                        "symbol:field-party-reassignment", false,
                                        "Party sprites of a reassigned party are not recovered");
            }
            put(actor.descriptor, 0x58, word(actor.descriptor, 0x58, 2) & 0xffdfU, 2);
            state.h_afd20 = 0xff40;
            place_at_entry(index, resident.variables.read(2));
            sync_actor_position(index);
            put(a, 4, word(a, 4) & ~0x800U);
        }
        put(a, 0, word(a, 0) | 0x20000U);
        put(a, 4, word(a, 4) | 0x400U);
        add_pc(a, 3);
    });
}

// 8009e248 (primary 1d): place the current actor at (x, z) with height y.
void Program::event_place_height(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        const auto index = world.current;
        auto &a = loaded(*this).actors.at(index).storage;
        const auto operand = [&](std::uint32_t offset) { // 800acd7c
            const auto at = word(a, 0xcc, 2) + offset;
            return s16(static_cast<std::uint32_t>(world.program.byte(at)) |
                       static_cast<std::uint32_t>(world.program.byte(at + 1)) << 8U);
        };
        snap_actor(index, operand(1), operand(3));
        const auto height = operand(5); // 8009e810
        put(a, 0x24, u32(height) << 16U);
        put(a, 0xec, u32(height), 2);
        put(a, 0x72, u32(height), 2);
        put(a, 0, word(a, 0) | 0x40000U);
        add_pc(a, 7);
    });
}

// 8009e040 (primary 23): descriptor flag 20 of the current actor.
void Program::event_descriptor_hidden(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &actor = loaded(*this).actors.at(world.current);
        put(actor.descriptor, 0x58, word(actor.descriptor, 0x58, 2) | 0x20U, 2);
        add_pc(actor.storage, 1);
    });
}

// 8009dc4c (primary 27): stop actor `operand 1`: clear its motion, mark it
// (+0 bit 0) and face its current direction; the current actor's idle
// dialogue window forgets its clear.
void Program::event_stop_actor(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &state = loaded(*this);
        const auto target = field::resolve_script_actor(world, 1);
        if (target != 0xff) {
            auto &t = state.actors.at(static_cast<std::size_t>(target)).storage;
            for (const std::uint32_t offset : {0x30U, 0x34U, 0x38U, 0x40U, 0x44U, 0x48U})
                put(t, offset, 0);
            put(t, 0, word(t, 0) | 1U);
            const auto direction = word(t, 0x104, 2) | 0x8000U;
            put(t, 0x106, direction, 2);
            put(t, 0x104, direction, 2);
            for (auto &window : world.dialogue) // 8009cd18
                if (window.half(field::DialogueWindow::owner) ==
                        static_cast<std::int32_t>(world.current) &&
                    window.half(field::DialogueWindow::busy) == 0) {
                    window.set_half(field::DialogueWindow::cleared, 0);
                    break;
                }
        }
        add_pc(state.actors.at(world.current).storage, 2);
    });
}

// Field 8008cf3c: ff, fe and fd name the characters of party slots 2, 1
// and 0; fc names none (ff).
std::int32_t Program::party_character(std::int32_t selector) const {
    const auto &state = *field;
    switch (selector) {
    case 0xff:
        return state.party_characters[2];
    case 0xfe:
        return state.party_characters[1];
    case 0xfd:
        return state.party_characters[0];
    case 0xfc:
        return 0xff;
    default:
        return selector;
    }
}

// 80091a08 (primary e6): the camera's four bounds; the last is negated.
void Program::event_camera_bounds(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &a = loaded(*this).actors.at(world.current).storage;
        const auto operand = [&](std::uint32_t offset) { // 800acd7c
            const auto at = word(a, 0xcc, 2) + offset;
            return s16(static_cast<std::uint32_t>(world.program.byte(at)) |
                       static_cast<std::uint32_t>(world.program.byte(at + 1)) << 8U);
        };
        set_memory(0x800af9dc, u32(operand(1)) & 0xffffU, 2);
        set_memory(0x800af9de, u32(operand(3)) & 0xffffU, 2);
        set_memory(0x800af9e0, u32(operand(5)) & 0xffffU, 2);
        set_memory(0x800af9e2, u32(-operand(7)) & 0xffffU, 2);
        add_pc(a, 9);
    });
}

// 800966b4 (primary 85): jump to `operand 3` unless `operand 1` is below
// variable 0.
void Program::event_branch_below(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &a = loaded(*this).actors.at(world.current).storage;
        const auto limit = field::read_immediate15_or_variable(world, 1);
        const auto pc = word(a, 0xcc, 2);
        if (limit < resident.variables.read(0))
            put(a, 0xcc, pc + 5, 2);
        else // 800acdb8
            put(a, 0xcc, u32(world.program.byte(pc + 3)) | u32(world.program.byte(pc + 4)) << 8U,
                2);
    });
}

// 8009ac7c, 8009a904 (primary 69): face table direction `operand 1`;
// before initialization completes the facing is also the resting one.
void Program::event_face_direction(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &state = loaded(*this);
        auto &a = state.actors.at(world.current).storage;
        const auto index = field::read_immediate15_or_variable(world, 1);
        if (index < 0 || static_cast<std::size_t>(index) >= state.direction_tables.size())
            throw field::FieldFormatError("A facing indexes past the direction tables");
        const auto direction = state.direction_tables[static_cast<std::size_t>(index)] | 0x8000U;
        if (world.control.post_initialization == 0)
            for (const std::uint32_t offset : {0x104U, 0x106U, 0x108U})
                put(a, offset, direction, 2);
        put(a, 0x104, direction, 2);
        put(a, 0x106, direction, 2);
        add_pc(a, 3);
    });
}

// 8008e85c, 8008e718 (primary f7): the random-encounter table: the gate
// (800b2298, kept at 800b2294), and `operand 3` distinct steps (at most 20h)
// drawn at random below the gate + 1, each plus one, at 800b22a0.
void Program::event_encounter_table(field::EventContext &context) {
    script(context, [&](field::FieldWorld &world) {
        auto &state = loaded(*this);
        auto &gate = state.control_inputs.encounter.encounter_gate;
        gate = u32(field::read_immediate15_or_variable(world, 1));
        auto count = field::read_immediate15_or_variable(world, 3);
        if (count >= 33)
            count = 0x20;
        set_memory(0x800b229c, u32(count));
        set_memory(0x800b2294, gate);
        if (count == 0) {
            gate = 0;
        } else {
            constexpr std::uint32_t table = 0x800b22a0;
            for (std::uint32_t k = 0; k < 32; ++k)
                set_memory(table + 2 * k, 0xffff, 2);
            for (std::int32_t n = 0; n < count; ++n) {
                std::uint32_t step = 0;
                for (bool taken = true; taken;) {
                    const auto next = field::advance_field_random(resident.random_seed);
                    resident.random_seed = next.seed;
                    step = u32(static_cast<std::int32_t>(next.value) * s32of(gate + 1) >> 15) &
                           0xffffU;
                    taken = false;
                    for (std::uint32_t k = 0; k < 32 && !taken; ++k)
                        taken = memory(table + 2 * k, 2) == step;
                }
                set_memory(table + 2 * u32(n), step, 2);
            }
            for (std::int32_t n = 0; n < count; ++n)
                set_memory(table + 2 * u32(n), (memory(table + 2 * u32(n), 2) + 1U) & 0xffffU, 2);
        }
        add_pc(state.actors.at(world.current).storage, 5);
    });
}

// Extended instructions of actor setup; the working PC is the extended byte.
bool Program::event_setup_extended(field::EventContext &context, std::uint8_t extended) {
    switch (extended) {
    case 0x07: // 8008d604: layer flag 400 cleared (0) or set (1).
        script(context, [&](field::FieldWorld &world) {
            auto &a = loaded(*this).actors.at(world.current).storage;
            const auto value = world.program.byte(word(a, 0xcc, 2) + 1);
            if (value == 0)
                put(a, 4, word(a, 4) & ~0x400U);
            else if (value == 1)
                put(a, 4, word(a, 4) | 0x400U);
            add_pc(a, 2);
        });
        return true;
    case 0x09: // 8008d078: layer flag 800 from `operand 1`.
        script(context, [&](field::FieldWorld &world) {
            auto &a = loaded(*this).actors.at(world.current).storage;
            if (field::read_immediate15_or_variable(world, 1) == 0)
                put(a, 4, word(a, 4) & ~0x800U);
            else
                put(a, 4, word(a, 4) | 0x800U);
            add_pc(a, 3);
        });
        return true;
    case 0x0d: // 8008cf9c: the actor's portrait character (+80).
        script(context, [&](field::FieldWorld &world) {
            auto &a = loaded(*this).actors.at(world.current).storage;
            a[0x80] = static_cast<std::uint8_t>(
                party_character(field::read_immediate15_or_variable(world, 1)));
            add_pc(a, 3);
        });
        return true;
    case 0x15: // 800a14f0: bundle sprite `operand 1` with part variant `operand 3`.
        script(context, [&](field::FieldWorld &world) {
            const auto index = world.current;
            auto &actor = loaded(*this).actors.at(index);
            put(actor.descriptor, 0x58, (word(actor.descriptor, 0x58, 2) & 0xf07fU) | 0x200U, 2);
            const auto slot = u32(field::read_immediate15_or_variable(world, 1));
            const auto resource = bundle_sprite(slot);
            const auto variant = u32(field::read_immediate15_or_variable(world, 3));
            create_actor_sprite(index, {static_cast<std::uint32_t>(index), slot, resource, 0,
                                        variant, static_cast<std::uint8_t>(slot | 0x80U), 1});
            sync_actor_position(index);
            auto &a = actor.storage;
            add_pc(a, 5);
            put(a, 0, (word(a, 0) | 0x100U) & ~0x80U);
            put(a, 4, word(a, 4) & ~0x800U);
            put(actor.descriptor, 0x58, word(actor.descriptor, 0x58, 2) & 0xffdfU, 2);
        });
        return true;
    case 0x1c: // 80098a7c: place the actor at (x, z, y) without a floor query.
        script(context, [&](field::FieldWorld &world) {
            auto &actor = loaded(*this).actors.at(world.current);
            auto &a = actor.storage;
            put(a, 0, word(a, 0) | 0x10000U);
            put(a, 4, word(a, 4) | 0x200000U);
            const auto flags = world.program.byte(word(a, 0xcc, 2) + 7);
            put(a, 0x20, u32(field::read_selected(world, 1, flags, 0x80)) << 16U);
            put(a, 0x28, u32(field::read_selected(world, 3, flags, 0x40)) << 16U);
            put(a, 0x24, u32(field::read_selected(world, 5, flags, 0x20)) << 16U);
            for (const std::uint32_t axis : {0U, 4U, 8U})
                put(actor.descriptor, 0x20 + axis, u32(s16(word(a, 0x22 + axis, 2))));
            auto &sprite = actor.sprite.sprite.bytes;
            if (sprite.size() < 0xc)
                throw field::FieldFormatError("Placing an actor requires its sprite");
            for (const std::uint32_t axis : {0U, 4U, 8U})
                put(sprite, axis, word(a, 0x20 + axis));
            add_pc(a, 8);
        });
        return true;
    case 0x0a: // 8008d684: set a variable bit.
        script(context, [&](field::FieldWorld &world) {
            auto &a = loaded(*this).actors.at(world.current).storage;
            const auto pc = word(a, 0xcc, 2);
            const auto raw = u32(world.program.byte(pc + 1)) | u32(world.program.byte(pc + 2))
                                                                   << 8U;
            const auto reference = static_cast<std::uint16_t>(raw >> 4U);
            resident.variables.write(reference,
                                     resident.variables.read(reference) | (1 << (raw & 15U)));
            add_pc(a, 3);
        });
        return true;
    case 0x3b: // 8008ce64: clear the character's bit in game data +1d32.
        script(context, [&](field::FieldWorld &world) {
            const auto character = party_character(field::read_immediate15_or_variable(world, 1));
            if (character != 0xff) {
                auto &data = resident.game_data;
                if (data.size() < 0x1d34 || character < 0 || character >= 16)
                    throw field::FieldFormatError("A character bit outside the game data");
                const auto bits =
                    (u32(data[0x1d32]) | u32(data[0x1d33]) << 8U) & ~(1U << u32(character));
                data[0x1d32] = static_cast<std::uint8_t>(bits);
                data[0x1d33] = static_cast<std::uint8_t>(bits >> 8U);
            }
            add_pc(loaded(*this).actors.at(world.current).storage, 3);
        });
        return true;
    case 0x42: // 8009fcac: party slot `operand 1` (at most 2) leaves its
               // mode, and its record variables restart at this map.
        script(context, [&](field::FieldWorld &world) {
            auto slot = field::read_immediate15_or_variable(world, 1);
            if (slot >= 3)
                slot = 2;
            auto &data = resident.game_data;
            const auto at = static_cast<std::int64_t>(0x22b1) + slot;
            if (at < 0 || at >= static_cast<std::int64_t>(data.size()))
                throw field::FieldFormatError("A party mode outside the game data");
            data[static_cast<std::size_t>(at)] = 0;
            if (slot >= 0) { // 8009fd10
                const auto base = static_cast<std::uint16_t>(0x2a + 6 * slot);
                resident.variables.write(base,
                                         static_cast<std::int32_t>(resident.field_map & 0xfffU));
                resident.variables.write(static_cast<std::uint16_t>(base + 2), 0);
                resident.variables.write(static_cast<std::uint16_t>(base + 4), 0);
            }
            add_pc(loaded(*this).actors.at(world.current).storage, 3);
        });
        return true;
    case 0x4a: // 8008ace8: read the actor's data file (77a + `operand 1`).
        script(context, [&](field::FieldWorld &world) {
            auto &state = loaded(*this);
            auto &a = state.actors.at(world.current).storage;
            world.control.break_requested = 1;
            if (resident.battle_request.gate_90 != 0 || resident.battle_request.menu_gate != 0 ||
                disc_idle_query() != 0) {
                add_pc(a, 0xffff); // Retry the whole instruction.
                return;
            }
            if (s16(word(a, 0x124, 2)) != -1) {
                static_cast<void>(release_owned_block(word(a, 0x120), 0x8008ad68));
                put(a, 0x124, 0xffff, 2);
            }
            const auto id = field::read_immediate15_or_variable(world, 1);
            static_cast<void>(select_directory(4, 0));
            const auto file = id + 0x77a;
            const auto size = file_bytes(file); // 800288ec
            put(a, 0x124, u32(file), 2);
            const auto block = load_block(size + 8, 0, 0x8008adb4);
            put(a, 0x120, block);
            static_cast<void>(read_file(file, block, 0, 0x80));
            if (world.control.post_initialization == 0)
                disc_wait(0);
            resident.battle_request.gate_90 = 1;
            add_pc(a, 3);
        });
        return true;
    case 0x4b: // 8008a9ac: once the disc is idle, the sprite takes the data.
        script(context, [&](field::FieldWorld &world) {
            auto &actor = loaded(*this).actors.at(world.current);
            auto &a = actor.storage;
            world.control.break_requested = 1;
            if (disc_idle_query() != 0) {
                add_pc(a, 0xffff);
                return;
            }
            resident.battle_request.gate_90 = 0;
            auto &sprite = actor.sprite.sprite.bytes;
            if (sprite.size() < 0x50)
                throw field::FieldFormatError("Actor data requires the actor's sprite");
            put(sprite, 0x4c, word(a, 0x120)); // 80021bf0
            add_pc(a, 1);
        });
        return true;
    default:
        return false;
    }
}

} // namespace xem::reconstruction
