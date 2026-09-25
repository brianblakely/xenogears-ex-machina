// Field overlay 8008110c and the per-actor motion stage it drives. Original
// addresses name correlations only; owned records are Program state.
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_motion.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>
#include <string>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Field update read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Field update write exceeds owned storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t product(std::int32_t a, std::int32_t b) { return s32(u32(a) * u32(b)); }
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point,
              bool completed) {
    if (observe)
        observe(program, point, completed);
}

// Field 80080968.
std::uint32_t terrain_attribute(const FieldState &state, const FieldActor &actor) {
    const std::array<std::uint16_t, 4> triangles{
        static_cast<std::uint16_t>(word(actor.storage, 8, 2)),
        static_cast<std::uint16_t>(word(actor.storage, 10, 2)),
        static_cast<std::uint16_t>(word(actor.storage, 12, 2)),
        static_cast<std::uint16_t>(word(actor.storage, 14, 2))};
    return field::actor_terrain_attribute(word(actor.storage, 4),
                                          static_cast<std::int16_t>(word(actor.storage, 0x10, 2)),
                                          triangles, state.collision);
}

// Field 8008492c: -1 when previous motion or collision state prevents idling.
std::int32_t idle_predicate(const FieldState &state, const FieldActor &actor) {
    const auto &a = actor.storage;
    if ((word(a, 0x14) & 0x420000U) != 0 || state.collision_mode != 0 || word(a, 0x30) != 0 ||
        word(a, 0x34) != 0 || word(a, 0x38) != 0 || state.collision_enabled != 1 ||
        a[0x74] != 0xff || (word(a, 0) & 0x401800U) != 0)
        return -1;
    const auto layer = s16(word(a, 0x10, 2));
    if (layer >= 0 && layer <= 2 && (word(a, 4) & (1U << static_cast<std::uint32_t>(layer))) != 0)
        return -1;
    return 0;
}

std::uint32_t packed(std::int32_t x, std::int32_t z) { return (u32(x) << 16U) + u32(z); }

// Field 80082494: optional four-point bound through the actor +114 extension.
std::int32_t motion_bounds(const FieldActor &actor, const field::FieldVector &candidate) {
    if ((word(actor.storage, 0x12c) & 0x1000U) == 0)
        return 0;
    if (!actor.extension_114)
        throw field::FieldFormatError("Motion bounds require the actor +114 extension");
    const auto &bounds = *actor.extension_114;
    const auto point = packed(s32(word(actor.storage, 0x20) + u32(candidate[0])) >> 16,
                              s32(word(actor.storage, 0x28) + u32(candidate[2])) >> 16);
    std::array<std::uint32_t, 4> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i)
        vertices[i] = packed(s16(word(bounds, i * 4, 2)), s16(word(bounds, i * 4 + 2, 2)));
    for (std::size_t i = 0; i < 3; ++i)
        if (field::field_packed_area({vertices[i], vertices[i + 1], point}) < 0)
            return -1;
    return field::field_packed_area({vertices[3], vertices[0], point}) >> 31;
}

// Resident 8004a414: Square0 loads signed halfwords into GTE IR registers.
std::int32_t square(std::int32_t value) {
    const auto component = s16(u32(value));
    return component * component;
}

// A read through an original pointer into owned sprites (for example the
// sequencer that sprite +7c designates) or owned resources.
std::uint32_t resource_word(const FieldState &state, std::uint32_t address, std::size_t width) {
    for (const auto &actor : state.actors) {
        const auto &sprite = actor.sprite.sprite;
        if (address >= sprite.address && address - sprite.address < sprite.bytes.size())
            return word(sprite.bytes, address - sprite.address, width);
    }
    for (const auto &resource : state.resources)
        if (address >= resource.address && address - resource.address < resource.bytes.size())
            return word(resource.bytes, address - resource.address, width);
    throw field::FieldFormatError("Original resource read at " + std::to_string(address) +
                                  " is outside the owned resources");
}

std::uint32_t planar_angle(std::int32_t z, std::int32_t x, const field::MathTables &math) {
    return (0U - u32(field::collision_atan(z, x, math.angle).angle)) & 0xfffU;
}
} // namespace

// Field 80082620: terrain push, conveyor and linked-platform additive motion.
void Program::field_pre_motion(std::size_t index) {
    auto &state = *field;
    auto &actor = state.actors.at(index);
    auto &a = actor.storage;
    auto &sprite = actor.sprite.sprite.bytes;
    const auto layer = s16(word(a, 0x10, 2));
    std::uint32_t terrain = 0;
    if (((word(a, 4) >> ((u32(layer) + 3U) & 31U)) & 1U) == 0 && state.party_processing_mode == 0)
        terrain = word(a, 0x14);
    // Field 8007b614 into the function's local vector.
    const auto speed =
        s32(u32(state.terrain_speeds.at((terrain >> 9U) & 3U)) * 16U * u32(state.terrain_scale)) >>
        12;
    const auto direction =
        (state.terrain_angles.at((terrain >> 11U) & 7U) + u32(state.terrain_angle)) & 0xfffU;
    const auto trig = field::planar_trigonometry(resident.math.trigonometry, direction);
    const std::array<std::int32_t, 3> conveyor{product(trig.cosine, speed), 0,
                                               s32(0U - u32(product(trig.sine, speed)))};
    std::int32_t push_x = 0, push_z = 0;
    const auto add = [&](std::size_t offset, std::int32_t value) {
        put(a, offset, word(a, offset) + u32(value));
    };
    bool apply_conveyor = false;
    if ((word(a, 0) & 0x41800U) != 0) {
        apply_conveyor = (terrain & 0x4000U) != 0;
    } else {
        if ((terrain & 0x420000U) != 0) {
            if (sprite.size() < 0x20)
                throw field::FieldFormatError("Terrain push requires the actor sprite");
            const auto scale = s32(word(a, 0xf0));
            field::FieldVector slope{
                s32(0U - u32(product(s32(word(a, 0x50)), s32(word(a, 0x54))))) >> 15, 0,
                s32(0U - u32(product(s32(word(a, 0x58)), s32(word(a, 0x54))))) >> 15};
            if (slope[0] == 0)
                slope[0] = 1;
            slope[1] = 1;
            if (slope[2] == 0)
                slope[2] = 1;
            auto normal = field::normalize_field_vector(slope, resident.math.reciprocal);
            for (auto &component : normal)
                if (component == 0)
                    component = 1;
            push_x = s32(u32(product(normal[0], scale >> 17)) << 4U);
            push_z = s32(u32(product(normal[2], scale >> 17)) << 4U);
            const std::int32_t limit = (terrain & 0x400000U) != 0 ? 0x18 : 0x0c;
            const auto current = s32(word(a, 0xf0));
            put(a, 0xf0,
                (current >> 16) < limit ? word(a, 0xf0) + word(sprite, 0x1c) : u32(limit) << 16U);
            put(sprite, 0x10, u32(s32(word(a, 0xf0)) >> 1));
        }
        if ((terrain & 0x400000U) != 0) {
            add(0x40, push_x);
            put(a, 0x104, word(a, 0x104, 2) | 0x8000U, 2);
            add(0x48, push_z);
        }
        if (a[0x74] == 0xff) {
            if ((terrain & 0x20000U) != 0) {
                add(0x40, push_x);
                add(0x48, push_z);
            }
            apply_conveyor = (terrain & 0x8000U) != 0;
        }
    }
    if (apply_conveyor) {
        add(0x40, conveyor[0]);
        add(0x44, conveyor[1]);
        add(0x48, conveyor[2]);
    }
    if (a[0x74] != 0xff) {
        const auto linked = static_cast<std::size_t>(a[0x74]);
        const auto &platform = state.actors.at(linked);
        if ((word(platform.storage, 4) & 0xc0U) == 0xc0U) {
            if ((word(a, 0x134) & 0x80U) == 0)
                throw MissingDependency({"field_platform_extension", 0x80031bdc, index, {}},
                                        "symbol:field-heap-allocation", false,
                                        "Linked platform motion allocates through the game heap");
            if (!actor.extension_110)
                throw field::FieldFormatError("Actor +134 bit 80 requires the +110 extension");
            auto &extension = *actor.extension_110;
            const auto delta_y = s16(word(platform.descriptor, 0x52, 2) - word(extension, 2, 2));
            put(extension, 2, word(platform.descriptor, 0x52, 2), 2);
            const auto self_x = s32(word(a, 0x20)), self_z = s32(word(a, 0x28));
            const auto other_x = s32(word(platform.storage, 0x20));
            const auto other_z = s32(word(platform.storage, 0x28));
            if ((word(a, 0x104, 2) & 0x8000U) == 0) {
                // Field 800825ac -> 80099a4c: planar distance of the integer positions.
                const auto dx = s16(word(platform.storage, 0x22, 2)) - s16(word(a, 0x22, 2));
                const auto dz = s16(word(platform.storage, 0x2a, 2)) - s16(word(a, 0x2a, 2));
                put(extension, 8, u32(field::script_distance(dx, 0, dz, resident.math.square_root)),
                    2);
            }
            const auto radius = s16(word(extension, 8, 2));
            const auto angle = s16(u32(field::collision_atan(other_z - self_z, other_x - self_x,
                                                             resident.math.angle)
                                           .angle)) -
                               delta_y - 0x800;
            const auto around = field::planar_trigonometry(resident.math.trigonometry, u32(angle));
            add(0x40,
                s32(u32(other_x) + (u32(product(around.cosine, radius)) << 4U) - u32(self_x)));
            add(0x48, s32(u32(other_z) + (u32(product(around.sine, radius)) << 4U) - u32(self_z)));
        }
    }
    if ((word(a, 4) & 0x22000U) == 0x22000U)
        throw MissingDependency({"field_object_motion", 0x80082b0c, index, {}},
                                "symbol:field-object-table-801e8670", false,
                                "Object-table motion reads the unrecovered 801e8670 table");
}

// Field 800821f4 for the actor bound to this descriptor.
void Program::field_animation(std::size_t index, std::int32_t animation) {
    auto &state = *field;
    auto &actor = state.actors.at(index);
    if ((word(actor.descriptor, 0x58, 2) & 0x40U) == 0)
        return;
    auto &a = actor.storage;
    if (animation != 3 && state.control_inputs.jump_mode == 0)
        put(a, 0, word(a, 0) & ~0x800U);
    if (animation == 0xff)
        animation = 0;
    if (animation != state.animation_mode)
        put(a, 0, word(a, 0) & ~0x800U);
    const auto layer_flags = word(a, 4);
    if ((layer_flags & 0x2000U) != 0)
        throw MissingDependency({"field_model_animation", 0x801e8330, index, {}},
                                "symbol:field-model-animation-801e8330", false,
                                "Model animation dispatch 801e8330 is not recovered");
    if ((layer_flags & 0x1000000U) != 0)
        return;
    select_animation(index, animation);
}

void Program::select_animation(std::size_t index, std::int32_t animation) {
    sprite_call(index, [&](field::SpriteWindow sprite, const field::SpriteSources &sources) {
        field::select_sprite_animation(sprite, animation, resident.sprite, sources);
    });
}

void Program::sprite_call(
    std::size_t index,
    const std::function<void(field::SpriteWindow, const field::SpriteSources &)> &call) {
    auto &actor = field->actors.at(index);
    with_sprite_sources([&](const field::SpriteSources &sources) {
        call({actor.sprite.sprite.address, actor.sprite.sprite.bytes}, sources);
    });
}

// Field 80082bb8.
void Program::field_actor_motion(std::size_t index) {
    auto &state = *field;
    auto &actor = state.actors.at(index);
    auto &a = actor.storage;
    const field::SpriteWindow sprite{actor.sprite.sprite.address, actor.sprite.sprite.bytes};
    if (sprite.bytes.size() < 0xb4)
        throw field::FieldFormatError("Actor motion requires the descriptor sprite");
    auto requested = static_cast<std::uint16_t>(word(a, 0x104, 2));
    field::MotionControl control{state.motion_actor, state.control_inputs.held_buttons};
    const auto prefix =
        field::begin_field_motion(word(a, 0), static_cast<std::int16_t>(word(a, 0xe8, 2)),
                                  static_cast<std::int32_t>(index), control, state.pass);
    state.motion_actor = control.current_actor_index;
    if (!prefix.active_body_mode)
        return;
    auto mode = static_cast<std::int32_t>(*prefix.active_body_mode);
    if (a[0xe3] > 8)
        --a[0xe3];
    auto additive = word(a, 0x40) | word(a, 0x44) | word(a, 0x48);
    if (idle_predicate(state, actor) == -1)
        additive = 1;
    field::FieldVector local{};
    bool stop = false;
    if ((requested & 0x8000U) == 0 || additive != 0 || (word(a, 0) & 0x40800U) != 0) {
        if ((requested & 0x8000U) == 0) {
            static_cast<void>(field::update_field_velocity(
                sprite, requested, static_cast<std::uint16_t>(word(actor.descriptor, 0x58, 2)), a,
                resident.math.trigonometry));
            for (std::size_t axis = 0; axis < 3; ++axis)
                local[axis] = s32(word(sprite.bytes, 0x0c + axis * 4) + word(a, 0x40 + axis * 4));
            put(a, 0x106, requested, 2);
        } else {
            for (std::size_t axis = 0; axis < 3; ++axis)
                local[axis] = s32(word(a, 0x40 + axis * 4));
            requested = static_cast<std::uint16_t>(word(a, 0x106, 2) & 0xfffU);
        }
        if (motion_bounds(actor, local) != 0) {
            stop = true;
        } else {
            if (local[0] != 0 || local[2] != 0)
                requested =
                    static_cast<std::uint16_t>(planar_angle(local[2], local[0], resident.math));
            std::int32_t result = -1;
            const auto layer = s16(word(a, 0x10, 2));
            if (layer < 0 || layer >= 4)
                throw field::FieldFormatError("Unqualified original motion layer");
            if (s16(word(a, 8 + static_cast<std::size_t>(layer) * 2, 2)) != -1) {
                const auto saved = word(a, 0);
                if (static_cast<std::int32_t>(index) == state.controlled_actor)
                    for (const auto slot : {state.party_indices[1], state.party_indices[2]})
                        if (slot != 0xff)
                            put(a, 0,
                                word(a, 0) |
                                    (word(state.actors.at(static_cast<std::size_t>(slot)).storage,
                                          0) &
                                     0x600U));
                std::array<std::uint8_t, 12> velocity{};
                for (std::size_t axis = 0; axis < 3; ++axis)
                    put(velocity, axis * 4, u32(local[axis]));
                // The original edge buffer is uninitialized stack; a projection
                // using it without a preceding edge write is not reconstructed.
                std::array<std::uint8_t, 16> edge{};
                field::CollisionSweepTrace trace;
                result = field::apply_field_collision_sweep(
                    a, velocity, edge, static_cast<std::int16_t>(requested),
                    state.collision_component, resident.math.collision(), state.collision_mode,
                    state.party_processing_mode, &trace);
                if (trace.projection) {
                    bool written = false;
                    for (const auto &query : trace.queries) {
                        written = written || query.result.edge.has_value();
                        if (query.result.value == -1)
                            break;
                    }
                    if (!written)
                        throw MissingDependency(
                            {"field_motion_sweep", 0x80082e00, index, {}},
                            "behavior:field-motion-uninitialized-edge", false,
                            "Sweep projection would read the uninitialized original stack edge");
                }
                for (std::size_t axis = 0; axis < 3; ++axis)
                    local[axis] = s32(word(velocity, axis * 4));
                put(a, 0, (word(a, 0) & ~0x600U) | (saved & 0x600U));
            }
            stop = result == -1;
        }
    } else {
        mode = s16(word(a, 0xe6, 2));
        put(a, 0x104, word(a, 0x104, 2) | 0x8000U, 2);
        stop = true;
    }
    if (stop) {
        put(a, 0xf0, 0x10000);
        for (const std::size_t offset : {0x40U, 0x44U, 0x48U})
            put(a, offset, 0);
        local = {};
        put(sprite.bytes, 0x0c, 0);
        put(sprite.bytes, 0x14, 0);
        put(a, 0x106, word(a, 0x106, 2) | 0x8000U, 2);
    }
    put(a, 4, word(a, 4) & ~0x1000U);
    std::int32_t animation = 0;
    if ((word(a, 0) & 0x800U) == 0) {
        if ((word(a, 0x104, 2) & 0x8000U) != 0)
            mode = s16(word(a, 0xe6, 2));
        animation = mode;
        if ((terrain_attribute(state, actor) & 0x200000U) != 0) {
            animation = 6;
            if ((word(a, 0x104, 2) & 0x8000U) != 0 && s16(word(a, 0xe8, 2)) == 6)
                put(a, 4, word(a, 4) | 0x1000U);
        }
    } else {
        animation = state.animation_mode;
        if (state.control_inputs.jump_mode == 0)
            put(sprite.bytes, 0x18,
                word(sprite.bytes, 6, 2) == word(sprite.bytes, 0x84, 2)
                    ? 0U
                    : u32(product(s16(word(sprite.bytes, 0x82, 2)), mode == 2 ? 0x60 : 0x30)));
    }
    const auto override = s16(word(a, 0xea, 2));
    if (override != 0xff)
        animation = override;
    if (s16(word(a, 0xe8, 2)) != animation && (word(a, 0) & 0x2000000U) == 0) {
        put(a, 0xe8, u32(animation), 2);
        field_animation(index, animation);
    }
    if ((word(a, 0x14) & 0x100U) != 0) {
        local[0] >>= 1;
        local[2] >>= 1;
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        put(a, 0x30 + axis * 4, u32(local[axis]));
        put(a, 0x40 + axis * 4, 0);
    }
}

// Field 80081c54: the controlled actor's 72-byte history record.
void Program::field_history(std::size_t index) {
    auto &state = *field;
    if (static_cast<std::int32_t>(index) != state.controlled_actor ||
        state.party_processing_mode != 0)
        return;
    if (state.history_indices[0] >= 32)
        throw field::FieldFormatError("Unqualified original party history index");
    const auto &actor = state.actors.at(index);
    const auto &a = actor.storage;
    const auto &sprite = actor.sprite.sprite.bytes;
    const auto record = std::span(state.history_ring).subspan(state.history_indices[0] * 72U, 72);
    for (const auto [target, source] : std::array<std::pair<std::size_t, std::size_t>, 6>{
             {{0, 0}, {4, 4}, {0x30, 0x50}, {0x34, 0x54}, {0x38, 0x58}, {0x40, 0x14}}})
        put(record, target, word(a, source));
    for (const auto [target, source] :
         std::array<std::pair<std::size_t, std::size_t>, 8>{{{8, 0x22},
                                                             {10, 0x26},
                                                             {12, 0x2a},
                                                             {0x12, 0xe8},
                                                             {0x16, 8},
                                                             {0x18, 10},
                                                             {0x1a, 12},
                                                             {0x1c, 14}}})
        put(record, target, word(a, source, 2), 2);
    put(record, 0x10, word(sprite, 0x84, 2), 2);
    put(record, 0x14, word(a, 0x106, 2) & 0xfffU, 2);
    for (const auto [target, source] : std::array<std::pair<std::size_t, std::size_t>, 3>{
             {{0x20, 0x0c}, {0x24, 0x10}, {0x28, 0x14}}})
        put(record, target, word(sprite, source));
    record[0x44] = a[0x10];
    state.history_indices[0] = (state.history_indices[0] - 1U) & 31U;
    state.history_reset = 0;
}

// Field 80084a40: layer selection, position commit or rollback, vertical
// integration and headroom, followed by the history store.
std::int32_t Program::field_position(std::size_t index, std::int32_t linked_floor,
                                     std::uint32_t link_status) {
    auto &state = *field;
    auto &actor = state.actors.at(index);
    auto &a = actor.storage;
    auto &sprite = actor.sprite.sprite.bytes;
    if (sprite.size() < 0xb4)
        throw field::FieldFormatError("Position integration requires the actor sprite");
    const bool controlled = static_cast<std::int32_t>(index) == state.controlled_actor;
    if (controlled)
        state.position_result = 0xffff;
    if ((word(a, 0) & 0x1000000U) != 0 || (word(a, 4) & 0x200000U) != 0 ||
        (word(a, 0) & 0x10000U) != 0)
        return -1;
    if (!(controlled && state.forced_position == 1) && word(sprite, 0x10) == 0 &&
        idle_predicate(state, actor) == 0 && word(sprite, 0x84, 2) == word(a, 0x26, 2))
        return -1;
    const auto count = static_cast<std::int32_t>(state.layer_count) - 1;
    if (count < 0 || count > 4)
        throw field::FieldFormatError("Unqualified original position layer count");
    const auto old_layer = s16(word(a, 0x10, 2));
    if (old_layer < 0 || old_layer >= 4)
        throw field::FieldFormatError("Unqualified original position layer");
    const auto old_x = word(a, 0x20), old_y = word(a, 0x24), old_z = word(a, 0x28);
    std::array<std::uint8_t, 8> old_triangles{};
    std::copy_n(a.begin() + 8, 8, old_triangles.begin());
    std::array<std::int32_t, 4> floors{}, uppers{}, ids{};
    std::array<std::int16_t, 4> triangles{};
    std::array<field::FieldVector, 4> normals{};
    for (std::int32_t i = 0; i < 4; ++i) {
        floors[static_cast<std::size_t>(i)] = 0x7fffffff;
        uppers[static_cast<std::size_t>(i)] = 0x7fffffff;
        ids[static_cast<std::size_t>(i)] = i;
    }
    std::int32_t completed = 0;
    for (std::int32_t layer = 0; layer < count; ++layer) {
        const auto slot = static_cast<std::size_t>(layer);
        const auto query =
            field::query_layer_floor(state.collision_component, resident.math.reciprocal, a, layer,
                                     state.party_processing_mode);
        if (query.normal)
            normals[slot] = *query.normal;
        if (query.triangle)
            triangles[slot] = *query.triangle;
        if (query.floor)
            floors[slot] = *query.floor;
        if (query.upper)
            uppers[slot] = *query.upper;
        if (query.value != 0)
            break;
        ++completed;
    }
    for (std::size_t i = 0; i < 3; ++i)
        if ((word(a, 4) & (1U << i)) != 0) {
            floors[i] = 0x7fffffff;
            uppers[i] = 0x7fffffff;
        }
    const auto old_floor = floors[static_cast<std::size_t>(old_layer)];
    for (int pass = 0; pass < 2; ++pass)
        for (std::size_t i = 0; i < 2; ++i)
            if (floors[i + 1] < floors[i]) {
                std::swap(floors[i], floors[i + 1]);
                std::swap(uppers[i], uppers[i + 1]);
                std::swap(ids[i], ids[i + 1]);
            }
    const auto restore_planar = [&] {
        put(a, 0x20, old_x);
        put(a, 0x10, u32(old_layer), 2);
        put(a, 0xf0, 0);
        put(a, 0x28, old_z);
        std::copy(old_triangles.begin(), old_triangles.end(), a.begin() + 8);
    };
    const auto publish = [&](bool all_axes) {
        for (std::size_t axis = 0; axis < 3; ++axis)
            put(sprite, axis * 4, word(a, 0x20 + axis * 4));
        for (std::size_t axis = 0; axis < 3; ++axis)
            if (all_axes || axis == 1)
                put(actor.descriptor, 0x20 + axis * 4, u32(s16(word(a, 0x22 + axis * 4, 2))));
    };
    const auto finish = [&] {
        field_history(index);
        return 0;
    };
    if (completed == count) {
        for (std::int32_t i = 0; i < count; ++i)
            put(a, 8 + static_cast<std::size_t>(i) * 2, u32(triangles[static_cast<std::size_t>(i)]),
                2);
        const auto y = s16(word(a, 0x26, 2));
        std::int32_t choice = 0;
        if (y < old_floor || (word(a, 0) & 0x1800U) != 0) {
            for (; choice < count; ++choice)
                if (y <= floors[static_cast<std::size_t>(choice)]) {
                    put(a, 0x10, u32(ids[static_cast<std::size_t>(choice)]), 2);
                    break;
                }
        } else {
            for (; choice < count; ++choice)
                if (s16(word(a, 0x10, 2)) == ids[static_cast<std::size_t>(choice)])
                    break;
        }
        if ((terrain_attribute(state, actor) & 4U) != 0 && choice != 0 &&
            s16(word(a, 0x10, 2)) <= count)
            put(a, 0x10, u32(ids.at(static_cast<std::size_t>(choice - 1))), 2);
        const auto attribute = terrain_attribute(state, actor);
        if ((((word(a, 0) >> 8U) & 7U) & (attribute >> 5U)) != 0 || (attribute & 0x800000U) != 0) {
            if (state.event_control.diagnostic_suppression == 0)
                throw MissingDependency({"field_position_diagnostic", 0x80084f04, index, {}},
                                        "symbol:field-diagnostic-output", false,
                                        "Unsuppressed position ERROR diagnostic is not recovered");
            if (controlled)
                state.position_result = 0xfff;
            put(a, 0x24, word(a, 0x24) + word(sprite, 0x10));
            restore_planar();
            const auto integer_y = s16(word(a, 0x26, 2));
            const auto floor = s16(word(sprite, 0x84, 2));
            if (integer_y < floor) {
                put(sprite, 0x10, word(sprite, 0x10) + word(sprite, 0x1c));
            } else {
                if (s32(word(sprite, 0x10)) > 0)
                    put(sprite, 0x10, 0);
                put(a, 0, word(a, 0) & 0xffbfefffU);
                put(a, 0x24, u32(floor) << 16U);
            }
            publish(false);
            return finish();
        }
        put(a, 0x20, word(a, 0x20) + word(a, 0x30));
        put(a, 0x28, word(a, 0x28) + word(a, 0x38));
        const auto layer = s16(word(a, 0x10, 2));
        for (std::int32_t i = 0; i < count; ++i)
            if (layer == ids[static_cast<std::size_t>(i)]) {
                put(sprite, 0x84, u32(floors[static_cast<std::size_t>(i)]), 2);
                break;
            }
        if (layer < 0 || layer >= count)
            throw MissingDependency({"field_position_layer", 0x80084fac, index, {}},
                                    "behavior:field-position-uninitialized-normal", false,
                                    "Position selected a layer whose normal was not queried");
        const auto normal = field::normalize_field_vector(normals[static_cast<std::size_t>(layer)],
                                                          resident.math.reciprocal);
        for (std::size_t axis = 0; axis < 3; ++axis)
            put(a, 0x50 + axis * 4, u32(normal[axis]));
    } else {
        put(a, 0xf0, 0);
    }
    if (state.collision_mode == 0) {
        if (link_status != 0) {
            if (s16(word(sprite, 0x84, 2)) < s32(u32(linked_floor) + 10U))
                a[0x74] = 0xff;
            put(sprite, 0x84, u32(linked_floor), 2);
            put(a, 0x24, u32(linked_floor) << 16U);
        }
    } else if (link_status < 2) {
        put(sprite, 0x84, u32(linked_floor), 2);
    }
    if ((word(a, 0) & 0x40000U) != 0) {
        put(a, 0x24, u32(s16(word(a, 0xec, 2))) << 16U);
        put(sprite, 0x10, 0);
    }
    static_cast<void>(field::apply_field_vertical(a, {actor.sprite.sprite.address, sprite},
                                                  static_cast<std::int16_t>(old_layer),
                                                  state.collision));
    const auto y = s16(word(a, 0x26, 2));
    const auto head = s32(u32(y) - word(a, 0x1a, 2));
    std::int32_t blocked = 0;
    for (; blocked < count; ++blocked) {
        const auto floor = floors[static_cast<std::size_t>(blocked)];
        const auto upper = uppers[static_cast<std::size_t>(blocked)];
        if (floor < y && head < upper && floor != upper)
            break;
    }
    bool clear = blocked == count;
    if (clear) {
        const auto layer = s16(word(a, 0x10, 2));
        const auto triangle = s16(word(a, 8 + static_cast<std::size_t>(layer) * 2, 2));
        if (layer < 0 || static_cast<std::size_t>(layer) >= state.collision.layers.size() ||
            triangle < 0 ||
            static_cast<std::size_t>(triangle) >=
                state.collision.layers[static_cast<std::size_t>(layer)].triangles.size())
            throw field::FieldFormatError("Unqualified original position headroom triangle");
        const auto raw = state.collision.layers[static_cast<std::size_t>(layer)]
                             .triangles[static_cast<std::size_t>(triangle)]
                             .attribute_raw;
        const auto extent = static_cast<std::int32_t>(static_cast<std::int8_t>(raw >> 8U)) * 4;
        clear = extent >= 0 || s32(u32(extent) + u32(s16(word(sprite, 0x84, 2)))) <= head;
    }
    if (clear) {
        publish(true);
        put(a, 0x14, terrain_attribute(state, actor));
    } else {
        restore_planar();
        if (word(sprite, 0x84, 2) != word(a, 0x26, 2))
            put(sprite, 0x10, word(sprite, 0x10) + word(sprite, 0x1c));
        if (s32(word(sprite, 0x10)) < 0) {
            put(sprite, 0x10, 0);
            put(a, 0x24, old_y);
        }
        publish(false);
    }
    return finish();
}

// Field 80083288 (POLYCHECK): the lowest floor of a descriptor's collision
// model under the candidate X/Z, and the normal of the last containing polygon.
std::optional<Program::PolygonHit> Program::polygon_contact(std::size_t index, std::int32_t x,
                                                            std::int32_t z) {
    const auto &state = *field;
    const auto &actor = state.actors.at(index);
    const auto variant = word(actor.storage, 0x12c) & 3U;
    if (variant == 0)
        throw MissingDependency({"field_polygon_camera", 0x80083324, index, {}},
                                "symbol:field-polygon-camera-transform", false,
                                "Camera-composed polygon transforms are not recovered");
    if (actor.model == 0)
        throw field::FieldFormatError("Polygon contact requires the descriptor's model instance");
    field::GteVector angles{};
    angles.at(variant - 1) = static_cast<std::int16_t>(word(actor.storage, 0x70, 2));
    auto local = field::rotation_matrix(angles, resident.math.trigonometry);
    field::GteMatrix descriptor{};
    for (std::size_t i = 0; i < 9; ++i)
        descriptor.r[i] = static_cast<std::int16_t>(word(actor.descriptor, 0xc + i * 2, 2));
    field::multiply_rotation(descriptor, local);
    for (std::size_t axis = 0; axis < 3; ++axis)
        local.t[axis] = s32(word(actor.descriptor, 0x20 + axis * 4));
    const auto transform = field::compose_matrix(state.world_matrix, local);
    resident.gte.transform = transform; // SetRotMatrix and SetTransMatrix
    const auto groups = resource_word(state, actor.model + 6, 2);
    const auto vertices = resource_word(state, actor.model + 8, 4);
    auto cursor = resource_word(state, actor.model + 0x10, 4);
    const auto point = packed(x, z);
    const auto vertex = [&](std::uint32_t item) {
        const auto at = vertices + item * 8U;
        const field::GteVector source{static_cast<std::int16_t>(resource_word(state, at, 2)),
                                      static_cast<std::int16_t>(resource_word(state, at + 2, 2)),
                                      static_cast<std::int16_t>(resource_word(state, at + 4, 2))};
        const auto moved = field::rotate_translate(transform, source);
        return field::FieldVector{moved[0], moved[1], moved[2]};
    };
    const auto flat = [](const field::FieldVector &v) { return packed(v[0], v[2]); };
    std::int32_t lowest = 0x7fffffff;
    field::FieldVector normal{};
    const auto height = [&](const std::array<field::FieldVector, 3> &triangle) {
        const auto [value, triangle_normal] =
            field::field_height_and_normal(triangle, x, z, resident.math.reciprocal);
        normal = triangle_normal;
        lowest = std::min(lowest, static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
    };
    const auto inside = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        return field::field_packed_area({a, b, c}) >= 0;
    };
    for (std::uint32_t group = 0; group < groups; ++group) {
        const auto header = resource_word(state, cursor, 4);
        cursor += 4;
        const auto type = header & 0xffU;
        if (type == 0xc4 || type == 0xc8)
            continue;
        const auto count = header >> 16U;
        for (std::uint32_t item = 0; item < count; ++item, cursor += 8) {
            const auto first = resource_word(state, cursor, 4);
            const auto second = resource_word(state, cursor + 4, 4);
            const auto v0 = vertex(first & 0xffffU), v1 = vertex(first >> 16U);
            const auto v2 = vertex(second & 0xffffU);
            const auto p0 = flat(v0), p1 = flat(v1), p2 = flat(v2);
            if ((header & 8U) == 0) {
                if (inside(p0, p1, point) && inside(p1, p2, point) && inside(p2, p0, point) &&
                    inside(p0, p1, p2))
                    height({v0, v1, v2});
            } else {
                const auto v3 = vertex(second >> 16U);
                const auto p3 = flat(v3);
                if (inside(p0, p1, point) && inside(p1, p3, point) && inside(p3, p2, point) &&
                    inside(p2, p0, point) && inside(p0, p1, p2))
                    height(inside(p1, p2, point) ? std::array{v0, v1, v2} : std::array{v1, v3, v2});
            }
        }
    }
    if (lowest == 0x7fffffff)
        return std::nullopt;
    return PolygonHit{lowest, normal};
}

// Field 80084158: the controlled actor's contacts, riding and pushing, then
// its own position integration.
void Program::field_contact(std::size_t index) {
    auto &state = *field;
    auto &actor = state.actors.at(index);
    auto &a = actor.storage;
    const auto cx = s16(u32(s32(word(a, 0x20) + word(a, 0x30)) >> 16));
    const auto cz = s16(u32(s32(word(a, 0x28) + word(a, 0x38)) >> 16));
    const auto y = s16(word(a, 0x26, 2));
    const auto head = s32(u32(y) - word(a, 0x1a, 2));
    const auto entry_flags = word(a, 0);
    const auto entry_link = a[0x74];
    const auto riding_blocked = entry_flags & 0x40800U;
    bool linked = false;
    std::uint32_t link_status = 0;
    std::int32_t linked_floor = 0x7fffffff;
    for (std::size_t u = 0; u < state.actors.size(); ++u) {
        if (u == index)
            continue;
        auto &other = state.actors[u].storage;
        const auto flags = word(other, 0);
        if ((flags & 1U) != 0)
            continue;
        const auto layer_flags = word(other, 4);
        put(other, 4, layer_flags & 0xffff3effU);
        std::int32_t top = 0, bottom = 0;
        if ((layer_flags & 0x80U) != 0) {
            const auto hit = polygon_contact(u, cx, cz);
            if (!hit) {
                put(other, 4, word(other, 4) & 0xff3fffffU);
                continue;
            }
            if (state.event_control.diagnostic_suppression == 0)
                throw MissingDependency({"field_polygon_diagnostic", 0x80084324, u, {}},
                                        "symbol:field-diagnostic-output", false,
                                        "Unsuppressed POLYCHECK diagnostic is not recovered");
            put(other, 4, word(other, 4) | 0x100U);
            top = hit->floor;
            bottom = s32(u32(top) + word(other, 0x1a, 2));
            if (a[0x74] == u) {
                for (std::size_t axis = 0; axis < 3; ++axis)
                    put(a, 0x50 + axis * 4, u32(hit->normal[axis]));
                put(other, 4, word(other, 4) | 0x4000U);
            }
        } else {
            bool overlap = false;
            if ((flags & 0x2000U) == 0) {
                const auto dx = (s32(word(other, 0x20) + word(other, 0x30)) >> 16) - cx;
                const auto dz = (s32(word(other, 0x28) + word(other, 0x38)) >> 16) - cz;
                const auto radius = s32(word(a, 0x1e, 2) + word(other, 0x1e, 2));
                overlap = s32(u32(square(dx)) + u32(square(dz))) < square(radius);
            } else {
                // Field 8008237c: rectangle from actor half extents +18/+1c.
                const auto ox = s16(word(other, 0x22, 2)), oz = s16(word(other, 0x2a, 2));
                const auto hx = s32(word(other, 0x18, 2)), hz = s32(word(other, 0x1c, 2));
                const auto point = packed(cx, cz);
                const auto left = s32(u32(ox) - u32(hx)), right = s32(u32(ox) + u32(hx));
                const auto near = s32(u32(oz) - u32(hz)), far = s32(u32(oz) + u32(hz));
                const std::array<std::uint32_t, 4> corners{packed(left, far), packed(right, far),
                                                           packed(right, near), packed(left, near)};
                overlap = true;
                for (std::size_t i = 0; i < 4 && overlap; ++i)
                    overlap =
                        field::field_packed_area({corners[i], corners[(i + 1) % 4], point}) >= 0;
                if (overlap && state.event_control.diagnostic_suppression == 0)
                    throw MissingDependency(
                        {"field_rectangle_diagnostic", 0x80082484, u, {}},
                        "symbol:field-diagnostic-output", false,
                        "Unsuppressed rectangle-contact diagnostic is not recovered");
            }
            if (!overlap) {
                put(other, 4, word(other, 4) & 0xff3fffffU);
                continue;
            }
            if ((word(a, 0x14) & 0x400000U) != 0) {
                if (state.event_control.diagnostic_suppression == 0)
                    throw MissingDependency({"field_contact_diagnostic", 0x80084488, u, {}},
                                            "symbol:field-diagnostic-output", false,
                                            "Unsuppressed HITOFF diagnostic is not recovered");
                continue;
            }
            if (((flags | entry_flags) & 0x80U) != 0 || state.party_processing_mode != 0)
                continue;
            bottom = s16(word(other, 0x26, 2));
            top = s32(u32(bottom) - word(other, 0x1a, 2));
        }
        const auto ride = [&] {
            put(other, 4, word(other, 4) | 0x800000U);
            for (std::size_t axis = 0; axis < 3; ++axis)
                put(a, 0x40 + axis * 4, word(other, 0x30 + axis * 4));
            link_status = 2;
            linked_floor = top;
            if (riding_blocked == 0) {
                a[0x74] = static_cast<std::uint8_t>(u);
                linked = true;
            }
        };
        if (a[0x74] == u && riding_blocked == 0) {
            ride();
        } else if (bottom < head || y < top) {
            const auto current = word(other, 4);
            put(other, 4, current & ~0x100U);
            if (y < top) {
                put(other, 4, (current & ~0x100U) | 0x800000U);
                linked_floor = std::min(linked_floor, top);
            } else {
                put(other, 4, current & 0xff7ffeffU);
            }
            put(other, 4, word(other, 4) | 0x400000U);
            continue;
        } else if (y < top + 0x10 || (word(other, 4) & 0x800000U) != 0) {
            ride();
        } else {
            bool pushed = false;
            if ((flags & 0x10U) == 0) {
                if (other[0xe3] < 0x30)
                    other[0xe3] = static_cast<std::uint8_t>(other[0xe3] + 2);
                if (other[0xe3] > 0x20) {
                    const auto quarter = [](std::uint32_t value) {
                        auto v = s32(value);
                        return (v < 0 ? v + 3 : v) >> 2;
                    };
                    put(other, 0x40, word(other, 0x40) + u32(quarter(word(a, 0x30))));
                    put(other, 0x48, word(other, 0x48) + u32(quarter(word(a, 0x38))));
                    for (const std::size_t offset : {0x30U, 0x34U, 0x38U})
                        put(a, offset, 0);
                    put(a, 0x40, u32(quarter(word(a, 0x30))));
                    put(a, 0x44, 0);
                    put(a, 0x48, u32(quarter(word(a, 0x38))));
                    pushed = true;
                }
            }
            if (!pushed) {
                for (const std::size_t offset : {0x40U, 0x44U, 0x48U, 0x30U, 0x34U, 0x38U})
                    put(other, offset, 0);
                for (const std::size_t offset : {0x30U, 0x34U, 0x38U, 0x40U, 0x44U, 0x48U})
                    put(a, offset, 0);
            }
        }
        put(other, 4, word(other, 4) | 0x400000U);
    }
    if (state.collision_mode != 0) {
        linked = false;
        ++link_status;
        linked_floor = state.linked_floor_override;
    }
    if (!linked) {
        a[0x74] = 0xff;
    } else {
        auto &platform = state.actors.at(a[0x74]);
        put(platform.storage, 4, word(platform.storage, 4) | 0x8000U);
        if (entry_link == 0xff) {
            if ((word(a, 0x134) & 0x80U) == 0)
                throw MissingDependency({"field_ride_extension", 0x80031bdc, index, {}},
                                        "symbol:field-heap-allocation", false,
                                        "Riding a platform allocates through the game heap");
            if (!actor.extension_110)
                throw field::FieldFormatError("Actor +134 bit 80 requires the +110 extension");
            auto &extension = *actor.extension_110;
            for (std::size_t i = 0; i < 3; ++i)
                put(extension, i * 2, word(platform.descriptor, 0x50 + i * 2, 2), 2);
            const auto dx = s16(word(platform.storage, 0x22, 2)) - s16(word(a, 0x22, 2));
            const auto dz = s16(word(platform.storage, 0x2a, 2)) - s16(word(a, 0x2a, 2));
            put(extension, 8, u32(field::script_distance(dx, 0, dz, resident.math.square_root)), 2);
        }
    }
    if ((word(a, 0) & 0x10000U) == 0 && (word(a, 4) & 0x200000U) == 0)
        static_cast<void>(field_position(index, linked_floor, link_status));
    if (s16(resource_word(state, word(actor.sprite.sprite.bytes, 0x7c) + 0xc, 2)) == 1) {
        // 800848d8: the input reset, then actor +0 bit 0800 is cleared.
        resident.input_queue.reset();
        put(a, 0, word(a, 0) & ~0x800U);
    }
}

// Field 8008237c: rectangle from half extents +18/+1c plus a margin.
bool Program::rectangle_contains(std::size_t index, std::int32_t x, std::int32_t z,
                                 std::int32_t margin) const {
    const auto &other = field->actors.at(index).storage;
    const auto ox = s16(word(other, 0x22, 2)), oz = s16(word(other, 0x2a, 2));
    const auto hx = s32(word(other, 0x18, 2)), hz = s32(word(other, 0x1c, 2));
    const auto left = ox - hx - margin, right = ox + hx + margin;
    const auto near = oz - hz - margin, far = oz + hz + margin;
    const std::array<std::uint32_t, 4> corners{packed(left, far), packed(right, far),
                                               packed(right, near), packed(left, near)};
    const auto point = packed(x, z);
    for (std::size_t i = 0; i < 4; ++i)
        if (field::field_packed_area({corners[i], corners[(i + 1) % 4], point}) < 0)
            return false;
    if (field->event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_rectangle_diagnostic", 0x80082484, index, {}},
                                "symbol:field-diagnostic-output", false,
                                "Unsuppressed rectangle-contact diagnostic is not recovered");
    return true;
}

// Field 8008399c: talk (event 2) and touch (event 3) triggers around the
// controlled actor, including facing, and insertion into a free event slot.
void Program::field_interactions(std::size_t index) {
    auto &state = *field;
    auto &player = state.actors.at(index).storage;
    const auto py = s16(word(player, 0x26, 2));
    const auto head = py - s32(word(player, 0x1a, 2));
    const auto touch_radius = s32(word(player, 0x1e, 2)) + 8;
    const auto talk_radius = s32(word(player, 0x1e, 2)) + 0x20;
    const auto facing = word(player, 0x106, 2) & 0xfffU;
    const auto px = s16(word(player, 0x22, 2)), pz = s16(word(player, 0x2a, 2));
    const bool talk_pressed = (state.control_inputs.pressed_buttons & 0x20U) != 0;
    bool talked = false;
    for (std::size_t u = 0; u < state.actors.size(); ++u) {
        auto &other = state.actors[u].storage;
        const auto flags = word(other, 0);
        std::uint32_t event = 0xff, priority = 7;
        if ((flags & 1U) != 0 || player[0x74] == u)
            continue;
        const auto top = s16(word(other, 0x26, 2)) + s16(word(other, 0x62, 2));
        const auto offset = [&] {
            return std::array{s16(word(other, 0x22, 2)) - px + s16(word(other, 0x60, 2)),
                              s16(word(other, 0x2a, 2)) - pz + s16(word(other, 0x64, 2))};
        };
        const auto face = [&](std::uint32_t octant) {
            put(other, 0x12c, (word(other, 0x12c) & ~0xe00U) | (octant << 9U));
        };
        const auto angle = [&] {
            const auto [dx, dz] = offset();
            return field::collision_atan(dz, dx, resident.math.angle).angle;
        };
        // Talk: a0 = -atan, its octant is (a0 >> 9) & 7; touch uses -(atan >> 9).
        const auto talk_octant = [&](std::int32_t a) { return (u32(-a) >> 9U) & 7U; };
        const auto touch_octant = [&](std::int32_t a) { return u32(-(a >> 9)) & 7U; };
        const auto facing_away = [&](std::int32_t a) {
            const auto relative = (facing - (u32(-a) & 0xfffU)) & 0xfffU;
            return relative - 0x2bcU < 0xa89U;
        };
        const auto debug_flag = [&] {
            if (state.event_control.diagnostic_suppression == 0)
                throw MissingDependency({"field_interaction_debug", 0x80083d5c, u, {}},
                                        "symbol:field-diagnostic-output", false,
                                        "Unsuppressed interaction debug flag is not recovered");
        };
        const auto layer_flags = word(other, 4);
        if ((layer_flags & 0x180U) != 0) {
            if ((layer_flags & 0x100U) == 0) {
                state.touch_latch = 0;
            } else if (talk_pressed && !talked && (layer_flags & 0x4000000U) == 0) {
                if ((flags & 0x220000U) == 0 && state.talk_inhibited == 0) {
                    talked = true;
                    event = 2;
                    priority = 3;
                    put(other, 0x12c, (word(other, 0x12c) & ~0xe00U) | (u32(-angle()) & 0xe00U));
                }
            } else if ((flags & 0xa20000U) == 0) {
                event = 3;
                priority = 4;
                face(touch_octant(angle()));
                if (state.touch_latch == 0 && (flags & 0x8000000U) != 0) {
                    state.touch_latch = 1;
                    put(state.actors.at(index).sprite.sprite.bytes, 0x10, 0);
                }
            }
        }
        const auto insert = [&] {
            if (event == 0xff)
                return;
            for (std::size_t slot = 0; slot < 8; ++slot)
                if (other[0x8f + slot * 8] == event)
                    return;
            for (std::size_t slot = 0; slot < 8; ++slot) {
                const auto control = word(other, 0x90 + slot * 8);
                if (((control >> 18U) & 15U) == 15U && ((control >> 22U) & 1U) == 0) {
                    put(other, 0x8c + slot * 8,
                        state.event_package.program().entry(static_cast<std::int32_t>(u), event),
                        2);
                    other[0x8f + slot * 8] = static_cast<std::uint8_t>(event);
                    put(other, 0x90 + slot * 8, (control & 0xffc3ffffU) | (priority << 18U));
                    const auto direction = word(other, 0x106, 2) | 0x8000U;
                    put(other, 0x106, direction, 2);
                    put(other, 0x104, direction, 2);
                    return;
                }
            }
        };
        if ((flags & 0x2000U) != 0) {
            if (top < head || py < top - s32(word(other, 0x1a, 2)) || u == index ||
                !rectangle_contains(u, px, pz, 0x10)) {
                insert();
                continue;
            }
            if (talk_pressed && !talked && (word(other, 4) & 0x4000000U) == 0) {
                if ((flags & 0x220000U) != 0 || state.talk_inhibited != 0) {
                    insert();
                    continue;
                }
                const auto a = angle();
                if ((word(other, 4) & 0x40000U) != 0 && facing_away(a)) {
                    insert();
                    continue;
                }
                talked = true;
                event = 2;
                priority = 3;
                face(talk_octant(a));
                debug_flag();
            } else if ((flags & 0xa20000U) == 0) {
                event = 3;
                priority = 4;
                face(touch_octant(angle()));
                debug_flag();
            }
            insert();
            continue;
        }
        const auto [dx, dz] = offset();
        const auto reach = square(talk_radius + s32(word(other, 0x1e, 2)));
        const auto distance = s32(u32(square(dx)) + u32(square(dz)));
        if (!(distance < reach) || top < head || py < top - s32(word(other, 0x1a, 2)) ||
            u == index) {
            insert();
            continue;
        }
        if (talk_pressed && !talked && (word(other, 4) & 0x4000000U) == 0) {
            if ((flags & 0x220000U) != 0) {
                insert();
                continue;
            }
            const auto a = field::collision_atan(dz, dx, resident.math.angle).angle;
            if (facing_away(a) || state.talk_inhibited != 0) {
                insert();
                continue;
            }
            talked = true;
            event = 2;
            priority = 3;
            face(talk_octant(a));
        } else if ((flags & 0xa20000U) == 0 &&
                   distance < square(touch_radius + s32(word(other, 0x1e, 2)))) {
            event = 3;
            priority = 4;
            face(touch_octant(field::collision_atan(dz, dx, resident.math.angle).angle));
        }
        insert();
    }
}

// Field 800815f0: party followers replay the leader's history ring.
void Program::field_followers() {
    auto &state = *field;
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &actor = state.actors[i];
        auto &a = actor.storage;
        if ((word(a, 0) & 0x1000000U) == 0 ||
            static_cast<std::int32_t>(i) == state.controlled_actor ||
            (word(actor.descriptor, 0x58, 2) & 0x20U) != 0)
            continue;
        const auto idle = [&] {
            auto animation = s16(word(a, 0xe6, 2));
            put(a, 0xe8, u32(animation), 2);
            if (animation < 0)
                put(a, 0xe8, 0, 2);
        };
        if (state.followers_idle != 0) {
            if (word(a, 0xe8, 2) != word(a, 0xe6, 2)) {
                idle();
                field_animation(i, s16(word(a, 0xe8, 2)));
            }
            continue;
        }
        // Field 8009fa00: the party slot holding this actor's character id.
        const auto character = s16(word(a, 0xe4, 2));
        std::optional<std::size_t> slot;
        if (character != 0xff)
            for (std::size_t k = 0; k < 3; ++k) {
                if (state.party_characters[k] == 0xff)
                    break;
                if (state.party_characters[k] == character) {
                    slot = k;
                    break;
                }
            }
        if (!slot)
            continue;
        auto &follower = state.history_indices[*slot];
        const auto record = [&] {
            if (follower >= 32)
                throw field::FieldFormatError("Unqualified original follower history index");
            return std::span(state.history_ring).subspan(follower * 72U, 72);
        };
        auto &sprite = actor.sprite.sprite.bytes;
        static_cast<void>(field::update_field_velocity(
            {actor.sprite.sprite.address, sprite},
            static_cast<std::uint16_t>(word(record(), 0x14, 2)),
            static_cast<std::uint16_t>(word(actor.descriptor, 0x58, 2)), a,
            resident.math.trigonometry));
        const auto recorded_flags = word(record(), 0);
        bool copy = false;
        if (state.forced_position == 1) {
            follower = (state.history_indices[0] + 1U) & 31U;
            copy = true;
        } else {
            bool settle = (recorded_flags & 0x800U) != 0;
            if (!settle) {
                put(a, 4, word(a, 4) & ~0x1000U);
                if ((word(a, 0x14) & 0x420000U) != 0) {
                    settle = true;
                } else if (state.history_reset != 0xffffffffU) {
                    const auto lag = *slot == 1 ? 10U : 20U;
                    if (((state.history_indices[0] + lag) & 31U) != follower)
                        continue;
                    settle = true;
                } else if (word(sprite, 0x84, 2) != word(a, 0x26, 2)) {
                    settle = true;
                } else {
                    if (s16(word(a, 0xe8, 2)) == 6) {
                        put(a, 4, word(a, 4) | 0x1000U);
                        continue;
                    }
                    if (word(a, 0xe8, 2) == word(a, 0xe6, 2))
                        continue;
                    idle();
                    field_animation(i, s16(word(a, 0xe8, 2)));
                    continue;
                }
            }
            if (settle) {
                if (follower != state.history_indices[0]) {
                    copy = true;
                } else {
                    put(a, 0xe8, word(a, 0xe6, 2), 2);
                    put(a, 0, word(a, 0) & ~0x800U);
                    if (s16(word(a, 0xe6, 2)) < 0)
                        put(a, 0xe8, 0, 2);
                    field_animation(i, s16(word(a, 0xe8, 2)));
                    continue;
                }
            }
        }
        if (!copy)
            continue;
        // The flag comes from the record read before any forced index change.
        put(a, 0, (recorded_flags & 0x800U) != 0 ? word(a, 0) | 0x800U : word(a, 0) & ~0x800U);
        const auto animation = s16(word(record(), 0x12, 2));
        if (s16(word(a, 0xe8, 2)) != animation) {
            put(a, 0xe8, u32(animation), 2);
            if (animation < 0)
                put(a, 0xe8, 0, 2);
            field_animation(i, s16(word(a, 0xe8, 2)));
        }
        const auto after = record();
        for (std::size_t k = 0; k < 4; ++k)
            put(a, 8 + k * 2, word(after, 0x16 + k * 2, 2), 2);
        put(a, 0x10, after[0x44], 2);
        for (std::size_t k = 0; k < 3; ++k) {
            put(a, 0x50 + k * 4, word(after, 0x30 + k * 4));
            put(sprite, 0x0c + k * 4, word(after, 0x20 + k * 4));
        }
        for (std::size_t k = 0; k < 3; ++k) {
            const auto value = s16(word(after, 8 + k * 2, 2));
            put(actor.descriptor, 0x20 + k * 4, u32(value));
            put(sprite, k * 4, u32(value) << 16U);
            put(a, 0x20 + k * 4, u32(value) << 16U);
        }
        put(sprite, 0x84, word(after, 0x10, 2), 2);
        const auto direction = word(after, 0x14, 2);
        put(a, 0x106, direction, 2);
        put(a, 0x104, direction, 2);
        follower = (follower - 1U) & 31U;
    }
}

// Field 8009e574: place an actor on its located floors at integer (x, z).
void Program::snap_actor(std::size_t index, std::int32_t x, std::int32_t z) {
    auto &state = *field;
    auto &actor = state.actors.at(index);
    auto &a = actor.storage;
    auto &sprite = actor.sprite.sprite.bytes;
    if (sprite.size() < 0x88)
        throw field::FieldFormatError("Placing an actor requires its sprite");
    const auto count = static_cast<std::int32_t>(state.layer_count) - 1;
    std::array<std::optional<field::FloorLocation>, 4> floors{};
    for (std::int32_t layer = 0; layer < count; ++layer) {
        const auto slot = static_cast<std::size_t>(layer);
        if (slot >= state.collision.layers.size())
            throw field::FieldFormatError("Placement layer outside the collision package");
        floors[slot] =
            field::locate_initial_floor(state.collision.layers[slot], state.triangle_counts[slot],
                                        s16(u32(x)), s16(u32(z)), resident.math.reciprocal);
        put(a, 8 + slot * 2, floors[slot]->triangle, 2);
    }
    put(a, 0x14, terrain_attribute(state, actor));
    const auto layer = s16(word(a, 0x10, 2));
    // The original reads its stack buffers at the current layer; a layer that
    // was not queried would read uninitialized stack.
    if (layer < 0 || layer >= 4 || !floors[static_cast<std::size_t>(layer)])
        throw MissingDependency({"field_snap_layer", 0x8009e648, index, {}},
                                "behavior:field-snap-uninitialized-layer", false,
                                "Placement selected a layer whose floor was not queried");
    const auto &found = *floors[static_cast<std::size_t>(layer)];
    for (std::size_t axis = 0; axis < 3; ++axis)
        put(a, 0x50 + axis * 4, u32(found.normal[axis]));
    const auto height = found.point[1];
    const std::array<std::int32_t, 3> position{x, s16(u32(height)), z};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        put(actor.descriptor, 0x20 + axis * 4, u32(position[axis]));
        put(actor.descriptor, 0x40 + axis * 4, u32(position[axis]));
    }
    put(sprite, 0x84, u32(height), 2);
    put(a, 0x20, u32(x) << 16U);
    put(a, 0x28, u32(z) << 16U);
    put(a, 0x24, u32(position[1]) << 16U);
    put(a, 0x72, u32(height), 2);
    for (std::size_t axis = 0; axis < 3; ++axis)
        put(sprite, axis * 4, word(a, 0x20 + axis * 4));
    for (const std::size_t offset : {0x40U, 0x44U, 0x48U, 0x30U, 0x34U, 0x38U, 0xd0U, 0xd4U, 0xd8U})
        put(a, offset, 0);
    for (const std::size_t offset : {0x62U, 0x60U, 0x64U})
        put(a, offset, 0, 2);
    for (const std::size_t offset : {0x0cU, 0x10U, 0x14U})
        put(sprite, offset, 0);
    put(a, 0xf0, 0);
    put(a, 0xec, 0, 2);
    put(a, 0x72, word(a, 0x26, 2), 2);
    put(a, 0, (word(a, 0) & 0xfffbffffU) | 0x400000U);
}

// Field 8009aee0: one party member's gather step toward (x, z). Zero when
// the member is absent, disabled or has arrived; -1 while still walking.
std::int32_t Program::gather_member(std::size_t slot, std::int32_t x, std::int32_t z,
                                    std::uint32_t facing) {
    auto &state = *field;
    const auto member = state.party_indices.at(slot);
    if (member == 0xff)
        return 0;
    auto &actor = state.actors.at(static_cast<std::size_t>(member));
    if ((word(actor.descriptor, 0x58, 2) & 0x20U) != 0)
        return 0;
    auto &a = actor.storage;
    auto &sprite = actor.sprite.sprite.bytes;
    if (sprite.size() < 0x20)
        throw field::FieldFormatError("Party gathering requires the member sprite");
    if (word(sprite, 0x18) == 0) {
        const auto divisor = word(a, 0x76, 2);
        if (divisor == 0)
            throw field::FieldFormatError("Original gather speed division by zero");
        put(sprite, 0x18, 0x4000000U / divisor);
    }
    // 80099a8c: Square0 and square root of the step length alone.
    const auto step =
        field::script_distance(s32(word(sprite, 0x18)) >> 15, 0, 0, resident.math.square_root);
    const auto dx = x - s16(word(a, 0x22, 2)), dz = z - s16(word(a, 0x2a, 2));
    const auto distance = field::script_distance(dx, 0, dz, resident.math.square_root);
    put(a, 0, word(a, 0) | 0x400000U);
    if (step + 1 < distance) {
        const bool stationary = word(a, 0x68, 2) == word(a, 0x22, 2) &&
                                word(a, 0x6a, 2) == word(a, 0x26, 2) &&
                                word(a, 0x6c, 2) == word(a, 0x2a, 2);
        put(a, 0x6e, stationary ? word(a, 0x6e, 2) + 1U : 0U, 2);
        const auto direction = planar_angle(dz, dx, resident.math);
        put(a, 0x104, direction, 2);
        put(a, 0x106, direction, 2);
        if (s16(word(a, 0x6e, 2)) < 0x41 && state.gather_override == 0)
            return -1;
        snap_actor(static_cast<std::size_t>(member), x, z);
    }
    std::uint32_t direction = 0;
    if ((word(a, 0) & 0x8000U) != 0)
        direction = word(a, 0x11c, 2);
    else if (facing == 0xff)
        direction = word(a, 0x106, 2);
    else if (facing < 24)
        direction = state.direction_tables[facing]; // 800aea34 + facing * 2
    else
        throw field::FieldFormatError("Gather facing indexes past the owned direction tables");
    put(a, 0x104, direction | 0x8000U, 2);
    put(a, 0x106, direction | 0x8000U, 2);
    put(a, 0x20, u32(x) << 16U);
    put(a, 0x28, u32(z) << 16U);
    put(a, 0x6e, 0, 2);
    put(a, 0, word(a, 0) & 0xfddff7ffU);
    return 0;
}

// Extended 24 (8009b210): gather the party at the leader, retrying the
// instruction each pass until every member has arrived.
void Program::party_gather(field::FieldWorld &world) {
    auto &state = *field;
    const auto &leader = state.actors.at(static_cast<std::size_t>(state.party_indices[0])).storage;
    const auto x = s16(word(leader, 0x22, 2)), z = s16(word(leader, 0x2a, 2));
    std::uint32_t arrived = 0;
    for (std::size_t slot = 0; slot < 3; ++slot)
        if (gather_member(slot, x, z, 0xff) == 0)
            arrived |= 1U << slot;
    world.control.break_requested = 1;
    auto &self = world.actors[world.current].actor;
    const auto pc = word(self, 0xcc, 2);
    if (arrived == 7) {
        put(self, 0xcc, pc + 1U, 2);
        state.party_processing_mode = 0;
        state.gather_override = 0;
        // 8009b338: reset all history indices and refill the ring.
        state.history_indices = {0, 0, 0};
        state.control_inputs.preserve_nonplayer_motion = 0;
        for (int i = 0; i < 32; ++i)
            field_history(static_cast<std::size_t>(state.controlled_actor));
    } else {
        state.party_processing_mode = 1;
        put(self, 0xcc, pc - 1U, 2);
    }
}

void Program::field_update(const ProgramObserver &observe) {
    if (!field)
        throw field::FieldFormatError("Field update requires an initialized field");
    auto &state = *field;
    observed(observe, *this, {"field_update", 0x8008110c, {}, {}}, false);
    state.history_reset = 0xffffffffU;
    static_cast<void>(event_pass(observe));
    observed(observe, *this, {"field_update_events", 0x8008113c, {}, {}}, true);
    for (auto &actor : state.actors)
        for (std::size_t axis = 0; axis < 3; ++axis)
            put(actor.storage, 0x68 + axis * 2, word(actor.storage, 0x22 + axis * 4, 2), 2);
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_update_diagnostic", 0x800811e8, {}, {}},
                                "symbol:field-diagnostic-output", false,
                                "Unsuppressed EVENT_CODE diagnostic output is not recovered");
    state.motion_counter = 0;
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &actor = state.actors[i];
        if ((word(actor.descriptor, 0x58, 2) & 0xf80U) != 0x200U)
            continue;
        const auto flags = word(actor.storage, 0);
        const auto layer_flags = word(actor.storage, 4);
        const SourcePoint point{"field_actor_motion", 0x80082bb8, i, {}};
        observed(observe, *this, point, false);
        if ((flags & 0x10001U) == 0) {
            if ((layer_flags & 0x600U) == 0x200U)
                continue;
            put(actor.storage, 0x14, terrain_attribute(state, actor));
            field_pre_motion(i);
            field_actor_motion(i);
        } else if ((layer_flags & 0x1000000U) != 0 && (flags & 0x10000U) == 0) {
            if (word(actor.storage, 0xe8, 2) != word(actor.storage, 0xea, 2)) {
                put(actor.storage, 0xea, 2, 2);
                put(actor.storage, 0xe8, 2, 2);
                select_animation(i, 2);
            }
        } else if ((layer_flags & 0x200000U) != 0 &&
                   word(actor.storage, 0xe8, 2) != word(actor.storage, 0xea, 2)) {
            const auto animation = s16(word(actor.storage, 0xea, 2));
            put(actor.storage, 0xe8, u32(animation), 2);
            field_animation(i, animation);
        }
        observed(observe, *this, point, true);
    }
    observed(observe, *this, {"field_update_motion", 0x80081394, {}, {}}, true);
    const auto controlled = static_cast<std::size_t>(state.controlled_actor);
    field_contact(controlled);
    observed(observe, *this, {"field_update_contact", 0x800813ec, {}, {}}, true);
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &actor = state.actors[i];
        const auto descriptor_flags = word(actor.descriptor, 0x58, 2);
        if ((descriptor_flags & 0xf00U) == 0 || (word(actor.storage, 4) & 0x600U) == 0x200U ||
            (descriptor_flags & 0xf80U) != 0x200U || (word(actor.storage, 0) & 0x10001U) != 0 ||
            i == controlled)
            continue;
        static_cast<void>(field_position(i, 0x7fffffff, 0));
        if (s16(resource_word(state, word(actor.sprite.sprite.bytes, 0x7c) + 0xc, 2)) == 1)
            put(actor.storage, 0, word(actor.storage, 0) & ~0x800U);
    }
    observed(observe, *this, {"field_update_positions", 0x8008150c, {}, {}}, true);
    if (state.control_inputs.encounter.inhibition == 0 && state.party_processing_mode == 0)
        field_interactions(controlled);
    state.collision_enabled = 1;
    observed(observe, *this, {"field_update_interactions", 0x80081598, {}, {}}, true);
    field_followers();
    observed(observe, *this, {"field_update_followers", 0x800815a8, {}, {}}, true);
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_update_diagnostic", 0x800815c4, {}, {}},
                                "symbol:field-diagnostic-output", false,
                                "Unsuppressed MOV_CHECK diagnostic output is not recovered");
}

} // namespace xem::reconstruction
