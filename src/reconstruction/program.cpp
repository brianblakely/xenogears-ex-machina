#include "xem/reconstruction/program.hpp"

#include "xem/reconstruction/field_motion.hpp"
#include "xem/reconstruction/original_layout.hpp"

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
    return {resident.sprite,      resident.field_return_mode,
            field->sprite_gate,   field->initialized_sprites,
            resident.sprite_heap, resident.sprite_tasks};
}
void Program::set_sprite_environment(const field::FieldSpriteEnvironment &environment) {
    auto &state = loaded(*this);
    resident.sprite = environment.sprite;
    resident.field_return_mode = environment.return_mode;
    state.sprite_gate = environment.field_gate;
    state.initialized_sprites = environment.initialized_count;
    resident.sprite_heap = environment.heap;
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
    // 800a3474 leaves its read pointer after the consumed snapshot bytes.
    state.snapshot_cursor = 0x8005a4e4U + static_cast<std::uint32_t>(restored.bytes_used);
    // The snapshot's fixed regions land in the Program values that own those
    // original addresses (including the sprite gate and battle/party modes).
    const auto &globals = restored.globals;
    write_original(*this, snapshot_regions[0].address, globals.object_state);
    write_original(*this, snapshot_regions[1].address, globals.transform_state);
    write_original(*this, snapshot_regions[2].address, globals.field_state);
    write_original(*this, snapshot_regions[3].address, globals.camera_state);
    // The attribute block goes to the live table (*800afb20); the parsed package
    // owns it, and loaded component bytes hold the same table.
    auto &attributes = state.collision.attributes_raw;
    if (attributes.size() < globals.collision_attributes.size())
        attributes.resize(globals.collision_attributes.size());
    std::ranges::copy(globals.collision_attributes, attributes.begin());
    if (!state.collision_component.empty()) {
        const auto table = word(state.collision_component, 0x14);
        if (table > state.collision_component.size() ||
            state.collision_component.size() - table < globals.collision_attributes.size())
            throw field::FieldFormatError("Collision attribute table exceeds the loaded component");
        std::ranges::copy(globals.collision_attributes, state.collision_component.begin() + table);
    }
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

void Program::checkpoint_pass(const ProgramObserver &observe) {
    constexpr std::uint32_t snapshot_address = 0x8005a4e4;
    auto &state = loaded(*this);
    auto &snapshot = resident.field_snapshot;
    const SourcePoint point{"checkpoint_pass", 0x800a3c8c, {}, {}};
    observed(observe, *this, point, false);
    if (snapshot.size() < 0x95c)
        throw field::FieldFormatError("Checkpoint pass requires the resident snapshot");
    state.descriptor_count = snapshot[0];
    write_original(*this, snapshot_regions[1].address, std::span(snapshot).subspan(0x3c, 0x74));
    std::size_t cursor = 0x95c;
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &actor = state.actors[i];
        const auto at = cursor + 0xc;
        if (at > snapshot.size() || snapshot.size() - at < 48)
            throw field::FieldFormatError("Checkpoint pass walks past the resident snapshot");
        const auto checkpoint = std::span(snapshot).subspan(at, 48);
        const auto decision =
            field::select_sprite_checkpoint(actor.storage, checkpoint, resident.saved_party_modes,
                                            resident.party_modes(), state.party_reassignment);
        // The saved animation update is a store into snapshot memory.
        std::ranges::copy(decision.checkpoint, checkpoint.begin());
        if (decision.decision == field::SpriteRestoreDecision::restore)
            sprite_call(i, [&](field::SpriteWindow sprite, const field::SpriteSources &sources) {
                static_cast<void>(
                    field::restore_sprite_checkpoint(sprite, checkpoint, resident.sprite, sources));
            });
        cursor += decision.record_bytes;
    }
    state.snapshot_cursor = snapshot_address + static_cast<std::uint32_t>(cursor);
    observed(observe, *this, point, true);
}

void Program::restore_field(const field::SpriteAllocator &allocate,
                            const field::SpriteReleaser &release,
                            const field::original::RestoreAllocation &allocate_extension,
                            const ProgramObserver &observe,
                            const field::SpriteImageUploader &upload_image) {
    auto &state = loaded(*this);
    if (resident.field_return_mode == 0)
        throw MissingDependency({"initialize_field_events", 0x800a28d4, {}, {}},
                                "symbol:field-event-initialization", false,
                                "Fresh event initialization is a different original branch");
    restore_field_data(allocate_extension, observe);
    const auto resources = views(state.resources);
    std::vector<field::SpriteWindow> mutable_resources;
    for (auto &resource : state.resources)
        mutable_resources.push_back({resource.address, resource.bytes});
    field::SpriteServices services{allocate, release, upload_image, &resident.sprite_upload};
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto frame_list = views(state.frame_list);
        for (const auto &owned : state.actors)
            if (!owned.sprite.sprite.bytes.empty())
                frame_list.push_back({owned.sprite.sprite.address, owned.sprite.sprite.bytes});
        field::SpriteSources sources{
            resources,
            frame_list,
            resident.math.trigonometry,
            state.replay_widths,
            {},
            [&](field::SpriteExecutionPoint at) {
                observed(observe, *this,
                         {at.operation, at.machine_address, i, {}, {}, at.command_pc}, false);
            },
            {},
            &services};
        sources.mutable_resources = mutable_resources;
        sources.models = &resident.sprite_models;
        sources.allocator = &resident.heap;
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

void Program::script(field::EventContext &context,
                     const std::function<void(field::FieldWorld &)> &handler) {
    auto &state = loaded(*this);
    const auto index = static_cast<std::size_t>(context.current_actor_index);
    store_events(state.actors.at(index), *context.current_actor);
    std::vector<field::FieldRecord> actors;
    for (auto &actor : state.actors)
        actors.push_back({actor.storage,
                          actor.descriptor,
                          {actor.sprite.sprite.address, actor.sprite.sprite.bytes}});
    field::FieldWorld world{context.program,
                            *context.variables,
                            context.control,
                            actors,
                            index,
                            index,
                            state.party_indices,
                            state.controlled_actor,
                            state.zones,
                            state.dialogue,
                            state.script_flag_b236c,
                            resident.math,
                            state.collision,
                            state.triangle_counts,
                            state.layer_count,
                            state.fade,
                            state.camera,
                            state.control_inputs.encounter.inhibition,
                            state.script_flags_b21d0,
                            resident.gte_screen,
                            state.direction_tables};
    try {
        handler(world);
    } catch (...) {
        *context.current_actor = state.actors[index].events();
        throw;
    }
    // Handlers act on original-layout records; refresh the semantic copy.
    *context.current_actor = state.actors[index].events();
}

void Program::dispatch(field::EventContext &context, std::uint8_t opcode,
                       const ProgramObserver &observe) {
    auto &state = loaded(*this);
    const auto index = static_cast<std::size_t>(context.current_actor_index);
    auto &actor = state.actors.at(index);
    const SourcePoint point{
        "event_instruction", 0x800a1ec8, index, context.current_actor->pc, {}, {}, opcode};
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
            for (std::size_t w = 0; w < inputs.dialogue_status.size(); ++w)
                inputs.dialogue_status[w] = state.dialogue[w].half(field::DialogueWindow::status);
            inputs.jump_setting = state.history_indices[0]; // The same original word, 800b2360.
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
        case 0xfc:
            show_message(context);
            break;
        case 0x75:
            change_music(context);
            break;
        case 0x19:   // 8009e4bc: place at X/Z operands.
        case 0x1a:   // 8009e428: set the layer and re-place at the current position.
        case 0x1b: { // 8009e35c: set the layer and place at X/Z operands.
            script(context, [&](field::FieldWorld &world) {
                auto &self = world.actors[world.current].actor;
                const auto code = [&](std::uint32_t at) {
                    return world.program.byte(word(self, 0xcc, 2) + at);
                };
                const auto pc_now = word(self, 0xcc, 2);
                if (opcode == 0x1a) {
                    put(self, 0x10, code(1), 2);
                    snap_actor(world.current, static_cast<std::int16_t>(word(self, 0x22, 2)),
                               static_cast<std::int16_t>(word(self, 0x2a, 2)));
                    put(self, 0xcc, pc_now + 2, 2);
                    return;
                }
                const auto flag_at = opcode == 0x19 ? 5U : 6U;
                if (opcode == 0x1b)
                    put(self, 0x10, code(5), 2);
                const auto flags = code(flag_at);
                const auto x = field::read_selected(world, 1, flags, 0x80);
                const auto z = field::read_selected(world, 3, flags, 0x40);
                snap_actor(world.current, x, z);
                put(self, 4, word(self, 4) & 0xffdfffffU);
                put(self, 0, word(self, 0) & 0xfffeffffU);
                put(self, 0xcc, pc_now + (opcode == 0x19 ? 6U : 7U), 2);
            });
            break;
        }
        case 0xf5: // 8009c12c: a mode-3 message window for the current actor.
            script(context, [&](field::FieldWorld &world) {
                static_cast<void>(open_dialogue(world, context.pass,
                                                static_cast<std::uint32_t>(world.current), 3));
            });
            break;
        case 0xfe:
            field::run_extended_event(context, [&](auto &active, auto extended) {
                dispatch_extended(active, extended, point, observe);
            });
            break;
        case 0:
        case 1:
        case 2:
        case 4:
        case 0x26:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
            field::execute_core_event(context, opcode);
            break;
        default:
            script(context, [&](auto &world) { field::execute_script_instruction(world, opcode); });
        }
    } catch (...) {
        commit();
        throw;
    }
    commit();
    observed(observe, *this, point, true);
}

void Program::dispatch_extended(field::EventContext &active, std::uint8_t extended,
                                SourcePoint point, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto extended_point = point;
    extended_point.event_opcode = static_cast<std::uint16_t>(0xfe00U | extended);
    observed(observe, *this, extended_point, false);
    if (extended == 0x7f)
        field::wait_battle_request_extended(active, resident.battle_request);
    else if (extended == 0xa2)
        field::execute_music_extended_event(active, extended, resident.music);
    else if (extended == 0x24)
        script(active, [&](auto &world) { party_gather(world); });
    else if (extended == 0x62 || extended == 0x63) // 8008f444, 8008f4a0
        script(active, [&](field::FieldWorld &world) {
            auto &self = world.actors[world.current].actor;
            const auto channel = field::read_immediate15_or_variable(world, 3);
            const auto value = field::read_immediate15_or_variable(world, 1);
            resident::set_effect_pair(resident.sound, static_cast<std::uint32_t>(channel) << 1,
                                      extended == 0x62 ? 0x76U : 0x74U,
                                      static_cast<std::uint32_t>(value));
            put(self, 0xcc, word(self, 0xcc, 2) + 5, 2);
        });
    else if (extended == 0x65) // 8008f4fc
        script(active, [&](field::FieldWorld &world) {
            auto &self = world.actors[world.current].actor;
            const auto id = field::read_immediate15_or_variable(world, 1);
            const auto channel = field::read_immediate15_or_variable(world, 3);
            play_sound_effect(static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(channel));
            put(self, 0xcc, word(self, 0xcc, 2) + 5, 2);
        });
    else if (extended == 0x60) // 8008ec30
        script(active, [&](field::FieldWorld &world) {
            field::request_movie(world, state.movie, resident.battle_request.field_active,
                                 state.single_actor_mode, state.disc_idle_known);
        });
    else
        script(active, [&](auto &world) { field::execute_script_extended(world, extended); });
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
        if (context.current_actor != nullptr)
            state.published_actor = static_cast<std::size_t>(context.current_actor_index);
    };
    try {
        const auto result =
            field::schedule_actor_events(context, scheduler, [&](auto &active, auto opcode) {
                commit();
                dispatch(active, opcode, observe);
                // Script handlers may change any actor's original-layout record.
                for (std::size_t i = 0; i < actors.size(); ++i)
                    actors[i] = state.actors[i].events();
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

void Program::event_extended(std::size_t index, const ProgramObserver &observe) {
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
    const auto extended = context.program.byte(actor.pc);
    if (actor.pc == 0 || context.program.byte(actor.pc - 1U) != 0xfe)
        throw field::FieldFormatError("An extended event needs its FE prefix before the PC");
    const SourcePoint point{"event_instruction", 0x800a1ec8, index, actor.pc - 1U, {}, {},
                            std::uint8_t{0xfe}};
    try {
        dispatch_extended(context, extended, point, observe);
    } catch (...) {
        commit();
        throw;
    }
    commit();
    observed(observe, *this, point, true);
}
namespace {
// A battle computation over battle memory, with the resident game data
// addressable at its original address for the duration (moved, not copied).
template <typename Body> void with_battle(Program &program, Body body) {
    if (!program.battle)
        throw field::FieldFormatError("A battle entry requires loaded battle memory");
    auto &regions = program.battle->regions;
    auto &resident = program.resident;
    const bool data = resident.game_data.size() == game_data_bytes;
    if (data) {
        const auto start = resident.game_state;
        const auto end = start + game_data_bytes;
        for (const auto &[address, bytes] : regions)
            if (address < end && start < address + bytes.size())
                throw field::FieldFormatError("Game data overlaps battle memory");
        regions.emplace(start, std::move(resident.game_data));
    }
    try {
        battle::Battle context{*program.battle, resident.random_seed};
        body(context);
    } catch (...) {
        if (data)
            resident.game_data = std::move(regions.extract(resident.game_state).mapped());
        throw;
    }
    if (data)
        resident.game_data = std::move(regions.extract(resident.game_state).mapped());
}
} // namespace

void Program::commit_battle_action(std::uint32_t attacker, std::uint32_t targets,
                                   std::uint32_t animation) {
    with_battle(*this, [&](battle::Battle &context) {
        battle::commit_action(context, attacker, targets, animation);
    });
}

void Program::apply_battle_results(std::uint32_t queue) {
    with_battle(*this, [&](battle::Battle &context) { battle::apply_results(context, queue); });
}

void Program::update_battle_alive() {
    with_battle(*this, [](battle::Battle &context) { battle::update_alive(context); });
}

void Program::run_battle_enemy_script(std::uint32_t slot, std::uint32_t flag) {
    with_battle(*this,
                [&](battle::Battle &context) { battle::run_enemy_script(context, slot, flag); });
}

void Program::tick_battle_timers() {
    with_battle(*this, [](battle::Battle &context) { battle::atb_tick(context); });
}

void Program::reload_battle_timer() {
    with_battle(*this, [](battle::Battle &context) { battle::reload_turn_timer(context); });
}

void Program::grant_battle_rewards() {
    with_battle(*this, [](battle::Battle &context) { battle::grant_rewards(context); });
}

void Program::total_battle_rewards() {
    with_battle(*this, [](battle::Battle &context) { battle::total_rewards(context); });
}

void Program::add_battle_drops(std::uint32_t ids, std::uint32_t counts, std::uint32_t categories) {
    with_battle(*this, [&](battle::Battle &context) {
        battle::add_drops(context, ids, counts, categories);
    });
}

std::array<std::uint8_t, 3> ResidentState::party_modes() const {
    if (game_data.size() != game_data_bytes)
        throw field::FieldFormatError("Party modes require loaded game data");
    return {game_data[0x22b1], game_data[0x22b2], game_data[0x22b3]};
}

} // namespace xem::reconstruction
