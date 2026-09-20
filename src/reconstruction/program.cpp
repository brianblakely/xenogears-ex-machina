#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/field_motion.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Program read exceeds its owned field storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Program write exceeds its owned field storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("Selected operation requires an initialized field");
    return *program.field;
}
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point,
              bool completed) {
    if (observe)
        observe(program, point, completed);
}
std::vector<field::SpriteResource> views(const std::vector<field::SpriteAllocation> &owned) {
    std::vector<field::SpriteResource> result;
    for (const auto &item : owned)
        result.push_back({item.address, item.bytes});
    return result;
}
std::uint32_t resource_word(const FieldState &state, std::uint32_t address) {
    std::optional<std::uint32_t> result;
    for (const auto &resource : state.resources) {
        if (address < resource.address)
            continue;
        const auto offset = static_cast<std::size_t>(address - resource.address);
        if (offset <= resource.bytes.size() && 4 <= resource.bytes.size() - offset) {
            const auto next = word(resource.bytes, offset);
            if (result && *result != next)
                throw field::FieldFormatError("Overlapping field sprite resources disagree");
            result = next;
        }
    }
    if (!result)
        throw field::FieldFormatError("Missing original field sprite bundle offset table");
    return *result;
}
void store_events(FieldActor &target, const field::EventActor &state) {
    put(target.storage, 0, state.flags);
    put(target.storage, 4, state.layer_flags);
    for (std::size_t i = 0; i < state.slots.size(); ++i) {
        const auto &slot = state.slots[i];
        const auto at = 0x8c + i * 8;
        put(target.storage, at, slot.resume_pc, 2);
        put(target.storage, at + 2, slot.countdown, 1);
        put(target.storage, at + 3, slot.event_tag, 1);
        put(target.storage, at + 4, slot.control_bits);
    }
    put(target.storage, 0xcc, state.pc, 2);
    put(target.storage, 0xce, state.selected_slot, 1);
    for (std::size_t i = 0; i < state.return_pcs.size(); ++i)
        put(target.storage, 0x78 + i * 2, state.return_pcs[i], 2);
    put(target.storage, 0x12c, state.model_and_bounds_flags);
}
void store_control(FieldActor &target, const field::ControlActor &state) {
    put(target.storage, 0x14, state.terrain_flags);
    for (std::size_t i = 0; i < state.integer_position.size(); ++i) {
        put(target.storage, 0x22 + i * 4, static_cast<std::uint16_t>(state.integer_position[i]), 2);
        put(target.storage, 0x68 + i * 2, static_cast<std::uint16_t>(state.cached_position[i]), 2);
    }
    put(target.storage, 0x104, state.direction, 2);
    put(target.storage, 0xe8, state.animation_mode, 2);
}
} // namespace

field::EventActor FieldActor::events() const { return field::original::read_event_actor(storage); }
field::ControlActor FieldActor::control() const {
    return field::original::read_control_actor(storage);
}

field::FieldSpriteEnvironment Program::sprite_environment() const {
    if (!field)
        throw field::FieldFormatError("Sprite environment requires a field");
    return {resident.sprite,
            resident.field_return_mode,
            field->sprite_gate,
            field->initialized_sprites,
            resident.allocation_class,
            resident.class_eight_context,
            resident.allocation_cursor,
            resident.sprite_tasks};
}
void Program::set_sprite_environment(const field::FieldSpriteEnvironment &environment) {
    auto &state = loaded(*this);
    resident.sprite = environment.sprite;
    resident.field_return_mode = environment.return_mode;
    state.sprite_gate = environment.field_gate;
    state.initialized_sprites = environment.initialized_count;
    resident.allocation_class = environment.allocation_class;
    resident.class_eight_context = environment.class_eight_context;
    resident.allocation_cursor = environment.allocation_cursor;
    resident.sprite_tasks = environment.tasks;
}

void Program::restore_field_data(const field::original::RestoreAllocation &allocate,
                                 const ProgramObserver &observe) {
    auto &state = loaded(*this);
    const SourcePoint point{"restore_field_data", 0x800a3474, {}, {}};
    observed(observe, *this, point, false);
    std::vector<field::original::ActorRestoreTarget> initial;
    for (const auto &actor : state.actors) {
        if (!actor.sprite.sprite.bytes.empty() || !actor.sprite.parts.bytes.empty())
            throw MissingDependency(point, "symbol:field-return-sprite-ownership", false,
                                    "Field replacement requires original sprite cleanup");
        field::original::ActorRestoreTarget target{};
        std::copy_n(actor.descriptor.begin() + 0x50, 8, target.descriptor_auxiliary.begin());
        target.descriptor_flags = word(actor.descriptor, 0x58);
        target.actor = actor.storage;
        initial.push_back(target);
    }
    const auto snapshot =
        field::original::parse_field_return(resident.field_snapshot, state.actors.size());
    for (const auto &actor : snapshot.actors)
        if ((actor.extension_110 || actor.extension_114) && !allocate)
            throw MissingDependency(
                {"restore_actor_extension", 0x80031bdc, {}, {}},
                "symbol:field-return-sprite-ownership", false,
                "Optional actor extensions require the original allocation service");
    const auto restored = field::original::restore_field_return_data(snapshot, initial, allocate);
    state.descriptor_count = restored.descriptor_count;
    state.snapshot_bytes_used = restored.bytes_used;
    state.globals = restored.globals;
    // These globals are outputs of the snapshot copy, not prepared factory inputs.
    state.sprite_gate = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(word(state.globals.field_state, 0x116, 2)));
    state.party_reassignment = word(state.globals.field_state, 0x1f0);
    state.party_processing_mode =
        static_cast<std::uint8_t>(word(state.globals.field_state, 0x154, 1));
    state.battle_mode_source = static_cast<std::uint8_t>(word(state.globals.field_state, 0x2de, 1));
    resident.variables.words = restored.variables;
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &actor = state.actors[i];
        const auto &result = restored.actors[i];
        actor.storage = result.actor;
        std::copy(result.descriptor_auxiliary.begin(), result.descriptor_auxiliary.end(),
                  actor.descriptor.begin() + 0x50);
        put(actor.descriptor, 0x58, result.descriptor_flags);
        actor.extension_110 = result.extension_110;
        actor.extension_114 = result.extension_114;
        actor.checkpoint = restored.pending_sprite_checkpoints[i];
    }
    observed(observe, *this, point, true);
}

void Program::restore_field(const field::SpriteAllocator &allocate,
                            const field::SpriteReleaser &release,
                            const field::original::RestoreAllocation &allocate_extension,
                            const ProgramObserver &observe) {
    auto &state = loaded(*this);
    if (resident.field_return_mode == 0)
        throw MissingDependency({"initialize_field_events", 0x800a28d4, {}, {}},
                                "symbol:field-event-initialization", false,
                                "Fresh event initialization is a different original branch");
    restore_field_data(allocate_extension, observe);
    const auto resources = views(state.resources);
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto frame_list = views(state.frame_list);
        for (const auto &owned : state.actors)
            if (!owned.sprite.sprite.bytes.empty())
                frame_list.push_back({owned.sprite.sprite.address, owned.sprite.sprite.bytes});
        const field::SpriteSources sources{
            resources,
            frame_list,
            state.trigonometry,
            state.replay_widths,
            {},
            [&](field::SpriteExecutionPoint at) {
                observed(observe, *this,
                         {at.operation, at.machine_address, i, {}, {}, at.command_pc}, false);
            }};
        auto &actor = state.actors[i];
        SourcePoint point{"create_field_sprite", 0x80076ac0, i, {}};
        observed(observe, *this, point, false);
        const auto tag = actor.storage[0x126];
        std::uint32_t resource = 0;
        if ((tag & 0x80U) == 0) {
            if (tag >= resident.party_sprite_resources.size())
                throw field::FieldFormatError("Missing selected party sprite resource");
            resource = resident.party_sprite_resources[tag];
        } else {
            const auto base = state.sprite_bundle_address;
            resource = base + resource_word(state, base + 4U + (tag & 0x7fU) * 4U);
        }
        const field::FieldSpriteArguments arguments{static_cast<std::uint32_t>(i),
                                                    actor.storage[0x127],
                                                    resource,
                                                    (word(actor.storage, 0x130) >> 28U) & 3U,
                                                    word(actor.storage, 0x134) & 15U,
                                                    tag,
                                                    (word(actor.storage, 0x134) >> 4U) & 1U};
        point.sprite_arguments = arguments;
        auto environment = sprite_environment();
        try {
            field::create_field_sprite(actor.sprite, actor.storage, actor.descriptor, arguments,
                                       environment, sources, allocate, release);
        } catch (...) {
            set_sprite_environment(environment);
            throw;
        }
        set_sprite_environment(environment);
        observed(observe, *this, point, true);
        const auto auxiliary = word(actor.storage, 0x12e, 2) & 3U;
        if ((tag & 0x80U) != 0 && (auxiliary == 1 || auxiliary == 2))
            throw MissingDependency({"restore_sprite_auxiliary", 0x8002303c, i, {}},
                                    "symbol:field-return-sprite-ownership", false,
                                    "Return caller requires the original sprite auxiliary binding");
    }
    if (state.party_reassignment != 0)
        throw MissingDependency({"restore_party_assignment", 0x800a28d4, {}, {}},
                                "symbol:field-return-sprite-ownership", false,
                                "Return caller requires party sprite reassignment");
}

void Program::dispatch(field::EventContext &context, std::uint8_t opcode,
                       const ProgramObserver &observe) {
    auto &state = loaded(*this);
    const auto index = static_cast<std::size_t>(context.current_actor_index);
    auto &actor = state.actors.at(index);
    const SourcePoint point{"event_instruction", 0x800a1ec8, index, context.current_actor->pc};
    const auto commit = [&] {
        store_events(actor, *context.current_actor);
        state.event_control = context.control;
        state.pass = context.pass;
    };
    commit();
    observed(observe, *this, point, false);
    try {
        switch (opcode) {
        case 5:
        case 6:
        case 0x0d:
            if (field::execute_event_call(context, opcode)) {
                commit();
                observed(observe, *this, {"event_diagnostic", 0, index, context.current_actor->pc},
                         true);
            }
            break;
        case 0x0c:
        case 0xa7: {
            auto control = actor.control();
            auto inputs = state.control_inputs;
            inputs.encounter.music_result = resident.music.gate;
            try {
                static_cast<void>(field::execute_control_event(context, opcode, control,
                                                               state.control_state, inputs,
                                                               resident.battle_request));
            } catch (...) {
                store_control(actor, control);
                throw;
            }
            store_control(actor, control);
            break;
        }
        case 0x21: {
            auto divisor = static_cast<std::uint16_t>(word(actor.storage, 0x76, 2));
            try {
                field::execute_motion_divisor(
                    context, divisor, {actor.sprite.sprite.address, actor.sprite.sprite.bytes});
            } catch (...) {
                put(actor.storage, 0x76, divisor, 2);
                throw;
            }
            put(actor.storage, 0x76, divisor, 2);
            break;
        }
        case 0x71:
            static_cast<void>(field::execute_battle_request(
                context, resident.battle_request, resident.music.gate, state.battle_mode_source));
            break;
        case 0x86:
            field::branch_battle_continuation(context);
            break;
        case 0xfe:
            field::run_extended_event(context, [&](auto &active, auto extended) {
                if (extended == 0x7f)
                    field::wait_battle_request_extended(active, resident.battle_request);
                else if (extended == 0xa2)
                    field::execute_music_extended_event(active, extended, resident.music);
                else
                    throw field::UnsupportedExtendedInstruction(active.current_actor->pc, extended);
            });
            break;
        default:
            field::execute_core_event(context, opcode);
        }
    } catch (...) {
        commit();
        throw;
    }
    commit();
    observed(observe, *this, point, true);
}

field::ScheduleResult Program::event_pass(const ProgramObserver &observe) {
    auto &state = loaded(*this);
    std::vector<field::EventActor> actors;
    for (const auto &item : state.actors)
        actors.push_back(item.events());
    std::vector<field::EventDescriptor> descriptors;
    for (std::size_t i = 0; i < actors.size(); ++i)
        descriptors.push_back({word(state.actors[i].descriptor, 0x58), &actors[i]});
    field::EventContext context{state.event_package.program(),
                                &resident.variables,
                                state.event_control,
                                state.pass,
                                nullptr,
                                nullptr,
                                0};
    field::SchedulerState scheduler{descriptors, static_cast<std::int32_t>(actors.size()),
                                    state.single_actor_mode, state.party_processing_mode,
                                    state.party_indices};
    const auto commit = [&] {
        for (std::size_t i = 0; i < actors.size(); ++i)
            store_events(state.actors[i], actors[i]);
        state.event_control = context.control;
        state.pass = context.pass;
    };
    try {
        const auto result =
            field::schedule_actor_events(context, scheduler, [&](auto &active, auto opcode) {
                commit();
                dispatch(active, opcode, observe);
            });
        commit();
        return result;
    } catch (...) {
        commit();
        throw;
    }
}

field::BatchResult Program::event_batch(std::size_t index, std::int32_t limit,
                                        const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &target = state.actors.at(index);
    auto actor = target.events();
    field::EventDescriptor descriptor{word(target.descriptor, 0x58), &actor};
    field::EventContext context{state.event_package.program(),
                                &resident.variables,
                                state.event_control,
                                state.pass,
                                &actor,
                                &descriptor,
                                static_cast<std::int32_t>(index)};
    const auto commit = [&] {
        store_events(target, actor);
        state.event_control = context.control;
        state.pass = context.pass;
    };
    try {
        const auto result = field::run_event_batch(
            context, limit, [&](auto &active, auto opcode) { dispatch(active, opcode, observe); });
        commit();
        return result;
    } catch (...) {
        commit();
        throw;
    }
}
} // namespace xem::reconstruction
