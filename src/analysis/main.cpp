#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace game = xem::reconstruction;
namespace field = game::field;
namespace {
// Private analysis transport. JSON qualification/comparison lives in Python;
// std::istream handles a small bounded binary input without a new JSON library.
class Input {
  public:
    explicit Input(const char *path) : stream(path, std::ios::binary) {
        if (!stream)
            throw std::runtime_error("Cannot open starting input");
        std::array<char, 8> magic{};
        stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        if (std::string_view(magic.data(), magic.size()) != "XEMRUN01")
            throw std::runtime_error("Invalid analysis transport header");
    }
    std::uint32_t word() {
        std::array<unsigned char, 4> data{};
        stream.read(reinterpret_cast<char *>(data.data()), 4);
        if (!stream)
            throw std::runtime_error("Truncated starting input");
        return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8U) |
               (static_cast<std::uint32_t>(data[2]) << 16U) |
               (static_cast<std::uint32_t>(data[3]) << 24U);
    }
    std::uint32_t count(std::uint32_t maximum) {
        const auto value = word();
        if (value > maximum)
            throw std::runtime_error("Starting input exceeds declared storage bounds");
        return value;
    }
    std::vector<std::uint8_t> blob(std::uint32_t maximum = 0x200000) {
        const auto size = count(maximum);
        total += size;
        if (total > 64U * 1024U * 1024U)
            throw std::runtime_error("Starting input exceeds 64 MiB analysis storage budget");
        std::vector<std::uint8_t> bytes(size);
        stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
        if (!stream)
            throw std::runtime_error("Truncated starting byte range");
        return bytes;
    }
    template <std::size_t Size> std::array<std::uint8_t, Size> block() {
        const auto bytes = blob(static_cast<std::uint32_t>(Size));
        if (bytes.size() != Size)
            throw std::runtime_error("Starting block has the wrong byte length");
        std::array<std::uint8_t, Size> result;
        std::copy(bytes.begin(), bytes.end(), result.begin());
        return result;
    }
    field::SpriteAllocation resource() { return {word(), blob()}; }
    void finish() {
        if (stream.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("Trailing starting input bytes");
    }

  private:
    std::ifstream stream;
    std::uint64_t total{};
};

std::int32_t signed_word(std::uint32_t word) { return std::bit_cast<std::int32_t>(word); }
std::int16_t signed_half(std::uint32_t word) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(word));
}
std::string hex(std::span<const std::uint8_t> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto value : bytes) {
        result += digits[value >> 4U];
        result += digits[value & 15U];
    }
    return result;
}
std::string quote(std::string_view text) {
    std::ostringstream out;
    out << '"';
    for (const auto raw : text) {
        const auto character = static_cast<unsigned char>(raw);
        if (character == '\\' || character == '"')
            out << '\\' << static_cast<char>(character);
        else if (character < 0x20)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<unsigned>(character);
        else
            out << static_cast<char>(character);
    }
    out << '"';
    return out.str();
}
std::array<std::uint32_t, 16> environment(const game::Program &program) {
    const auto e = program.sprite_environment();
    return {static_cast<std::uint32_t>(e.sprite.rate_control),
            e.sprite.platform_mode,
            e.sprite.variant,
            e.sprite.binding_control,
            e.sprite.frame_head,
            e.return_mode,
            static_cast<std::uint16_t>(e.field_gate),
            e.initialized_count,
            program.resident.heap.tag,
            e.heap.class_eight_context,
            program.resident.heap.quiet,
            e.tasks.wait_count,
            e.tasks.wait_flag,
            e.tasks.current,
            e.tasks.head,
            e.tasks.next};
}
template <typename Range> void numbers(std::ostream &out, const Range &items) {
    out << '[';
    bool comma = false;
    for (const auto value : items) {
        if (comma)
            out << ',';
        comma = true;
        out << +value;
    }
    out << ']';
}
void allocation(std::ostream &out, const field::SpriteAllocation &value) {
    out << "{\"address\":" << value.address << ",\"bytes\":" << quote(hex(value.bytes)) << '}';
}
void actor_storage(std::ostream &out, const game::FieldActor &actor) {
    out << "{\"actor\":" << quote(hex(actor.storage))
        << ",\"descriptor\":" << quote(hex(actor.descriptor)) << '}';
}
void globals(std::ostream &out, const game::Program &program) {
    const auto region = [&](std::size_t index) {
        const auto [address, size] = game::snapshot_regions[index];
        std::vector<std::uint8_t> bytes(size);
        game::read_original(program, address, bytes);
        return hex(bytes);
    };
    const auto &attributes = program.field->collision.attributes_raw;
    out << "{\"globals-007c\":" << quote(region(0)) << ",\"globals-fa54\":" << quote(region(1))
        << ",\"collision-attributes\":"
        << quote(hex(std::span(attributes).first(std::min<std::size_t>(attributes.size(), 0x400))))
        << ",\"globals-2078\":" << quote(region(2)) << ",\"globals-f880\":" << quote(region(3))
        << '}';
}
std::string variables(const game::Program &program) {
    std::array<std::uint8_t, 2048> bytes;
    for (std::size_t i = 0; i < program.resident.variables.words.size(); ++i) {
        bytes[i * 2] = static_cast<std::uint8_t>(program.resident.variables.words[i]);
        bytes[i * 2 + 1] = static_cast<std::uint8_t>(program.resident.variables.words[i] >> 8U);
    }
    return hex(bytes);
}
void snapshot(std::ostream &out, const game::Program &program) {
    if (!program.field) {
        out << "null";
        return;
    }
    const auto &state = *program.field;
    out << "{\"actors\":[";
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        if (i != 0)
            out << ',';
        actor_storage(out, state.actors[i]);
    }
    out << "],\"variables\":" << quote(variables(program)) << ",\"environment\":";
    numbers(out, environment(program));
    out << ",\"task_pending_head\":" << program.resident.sprite_tasks.pending_head;
    out << ",\"texture_page\":" << program.resident.sprite.texture_page
        << ",\"texture_mode\":" << program.resident.sprite.texture_mode;
    out << ",\"heap_class_five_context\":" << program.resident.sprite_heap.class_five_context
        << ",\"heap_tag\":" << program.resident.heap.allocation_class;
    const auto &model = program.resident.sprite_models;
    out << ",\"model_state\":{\"material_page\":" << model.material_page
        << ",\"material_palette\":" << model.material_palette
        << ",\"palette_base\":" << model.palette_base << ",\"palette_mode\":" << model.palette_mode
        << ",\"primitive_count\":" << model.primitive_count << ",\"output\":" << model.output
        << ",\"shading\":" << model.shading << ",\"geometry\":" << model.geometry
        << ",\"normals\":" << model.normals << ",\"vertices\":" << model.vertices
        << ",\"auxiliary\":" << model.auxiliary << "},\"model_buffers\":[";
    for (std::size_t i = 0; i < model.buffers.size(); ++i) {
        if (i != 0)
            out << ',';
        allocation(out, model.buffers[i]);
    }
    out << "],\"resources\":[";
    for (std::size_t i = 0; i < state.resources.size(); ++i) {
        if (i != 0)
            out << ',';
        allocation(out, state.resources[i]);
    }
    out << ']';
    const auto &tasks = program.resident.sprite_tasks;
    out << ",\"task_serial\":" << tasks.serial << ",\"task_primary_count\":" << tasks.primary_count
        << ",\"task_auxiliary_count\":" << tasks.auxiliary_count
        << ",\"task_active_flags\":" << tasks.active_flags
        << ",\"child_creation_flags\":" << +tasks.creation_flags
        << ",\"child_allocation_mode\":" << +tasks.allocation_mode << ",\"task_nodes\":[";
    for (std::size_t i = 0; i < tasks.nodes.size(); ++i) {
        if (i != 0)
            out << ',';
        allocation(out, tasks.nodes[i]);
    }
    out << ']';
    out << ",\"event_control\":";
    const auto &control = state.event_control;
    numbers(out,
            std::array{control.budget_mode, control.break_requested, control.batch_limit,
                       control.post_initialization, control.gate_values[0], control.gate_values[1],
                       control.gate_values[2], control.diagnostic_suppression});
    out << ",\"pass\":";
    numbers(out, std::array{state.pass.input_updated, state.pass.unknown_c4268});
    const auto &battle = program.resident.battle_request;
    out << ",\"battle_request\":";
    numbers(out, std::array<std::uint32_t, 8>{battle.field_active, battle.menu_gate, battle.gate_90,
                                              battle.pending, battle.selector, battle.mode,
                                              battle.resident_flag, state.battle_mode_source});
    out << ",\"sprites\":[";
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        if (i != 0)
            out << ',';
        out << "{\"sprite\":";
        allocation(out, state.actors[i].sprite.sprite);
        out << ",\"parts\":";
        allocation(out, state.actors[i].sprite.parts);
        out << '}';
    }
    out << "]}";
}
void point_json(std::ostream &out, game::SourcePoint point) {
    out << "\"operation\":" << quote(point.operation)
        << ",\"machine_address\":" << point.machine_address << ",\"actor\":";
    if (point.actor)
        out << *point.actor;
    else
        out << "null";
    out << ",\"event_pc\":";
    if (point.event_pc)
        out << *point.event_pc;
    else
        out << "null";
    out << ",\"sprite_bytecode_pc\":";
    if (point.sprite_pc)
        out << *point.sprite_pc;
    else
        out << "null";
}
struct HostBudget : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct ServiceResult {
    std::uint32_t mode;
    field::SpriteAllocation allocation;
};
struct Services {
    std::vector<ServiceResult> incoming;
    std::size_t cursor{};
    std::unordered_map<std::uint32_t, std::size_t> live;
    std::vector<std::uint32_t> requested;
    std::vector<std::uint32_t> released;
    std::vector<field::SpriteImageUpload> uploads;

    field::SpriteAllocation allocate(std::uint32_t size, std::uint32_t mode) {
        if (cursor == incoming.size())
            throw InputError("Missing declared original heap result at allocation " +
                             std::to_string(cursor));
        const auto &next = incoming[cursor];
        if (next.mode != mode || next.allocation.bytes.size() != size)
            throw InputError("Original heap result disagrees with computed allocation request");
        const auto address = next.allocation.address;
        if ((size != 0 && address == 0) ||
            static_cast<std::uint64_t>(address) + size > (1ULL << 32U))
            throw InputError("Invalid original allocation extent");
        for (const auto &[other, length] : live)
            if (static_cast<std::uint64_t>(address) < static_cast<std::uint64_t>(other) + length &&
                static_cast<std::uint64_t>(other) < static_cast<std::uint64_t>(address) + size)
                throw InputError("Original heap result overlaps a live allocation");
        live[address] = size;
        requested.push_back(size);
        ++cursor;
        return next.allocation;
    }
    void release(std::uint32_t address) {
        if (live.erase(address) != 1)
            throw InputError("Original release does not refer to a live allocation");
        released.push_back(address);
    }
};
constexpr std::array<std::string_view, 4> entries{"field_return", "field_return_data", "event_pass",
                                                  "event_batch"};
} // namespace

int main(int argc, char **argv) {
    game::Program program;
    Services services;
    std::string status = "invalid_input", reason, dependency;
    std::string entry_result = "null";
    std::string_view entry = "unknown";
    game::SourcePoint point{"prepare_start", 0, {}, {}};
    std::optional<std::uint8_t> opcode;
    std::optional<std::uint32_t> sprite_pc;
    std::vector<std::string> checkpoints;
    std::size_t report_bytes = 0;
    std::uint32_t work = 0, completed_entries = 0;
    bool executing = false;
    try {
        if (argc != 2)
            throw InputError("Usage: xem-analysis-runner INPUT.bin (use tools.analysis.execution "
                             "for JSON cases)");
        Input in(argv[1]);
        const auto selected = in.count(static_cast<std::uint32_t>(entries.size() - 1));
        entry = entries[selected];
        const auto budget = in.count(1000000);
        const auto repeats = in.count(10000);
        if (repeats == 0 || (selected < 2 && repeats != 1))
            throw InputError(
                "A return entry runs once; event entries require at least one invocation");
        const auto actor_index = in.count(255);
        const auto batch_limit = signed_word(in.word());
        program.field = std::make_unique<game::FieldState>();
        auto &state = *program.field;
        program.resident.field_snapshot = in.blob();
        const auto count = in.count(255);
        state.actors.resize(count);
        for (auto &actor : state.actors) {
            actor.storage = in.block<0x138>();
            actor.descriptor = in.block<0x5c>();
        }
        field::FieldSpriteEnvironment env{};
        env.sprite.rate_control = signed_word(in.word());
        env.sprite.platform_mode = static_cast<std::uint8_t>(in.count(255));
        env.sprite.variant = in.word();
        env.sprite.binding_control = static_cast<std::uint8_t>(in.count(255));
        env.sprite.frame_head = in.word();
        env.return_mode = in.word();
        env.field_gate = signed_half(in.count(65535));
        env.initialized_count = in.word();
        program.resident.heap.tag = static_cast<std::uint16_t>(in.count(65535));
        env.heap.class_eight_context = in.word();
        program.resident.heap.quiet = in.word();
        env.tasks.wait_count = in.word();
        env.tasks.wait_flag = static_cast<std::uint16_t>(in.count(65535));
        env.tasks.current = in.word();
        env.tasks.head = in.word();
        env.tasks.next = in.word();
        env.tasks.pending_head = in.word();
        for (auto n = in.count(4096); n != 0; --n)
            env.tasks.nodes.push_back(in.resource());
        env.tasks.serial = in.word();
        env.tasks.primary_count = in.word();
        env.tasks.auxiliary_count = in.word();
        env.tasks.active_flags = in.word();
        env.tasks.creation_flags = static_cast<std::uint8_t>(in.count(255));
        env.tasks.allocation_mode = static_cast<std::uint8_t>(in.count(255));
        env.sprite.texture_page = in.word();
        env.sprite.texture_mode = in.word();
        env.heap.class_five_context = in.word();
        program.resident.heap.allocation_class = static_cast<std::uint16_t>(in.count(65535));
        auto &model = program.resident.sprite_models;
        model.material_page = static_cast<std::uint16_t>(in.count(65535));
        model.material_palette = static_cast<std::uint16_t>(in.count(65535));
        model.palette_base = in.word();
        model.palette_mode = in.word();
        model.primitive_count = in.word();
        model.output = in.word();
        model.shading = in.word();
        model.geometry = in.word();
        model.normals = in.word();
        model.vertices = in.word();
        model.auxiliary = in.word();
        program.set_sprite_environment(env);
        state.sprite_bundle_address = in.word();
        state.party_reassignment = in.word();
        for (auto n = in.count(128); n != 0; --n)
            program.resident.party_sprite_resources.push_back(in.word());
        for (auto n = in.count(4096); n != 0; --n) {
            const auto mode = in.word();
            services.incoming.push_back({mode, in.resource()});
        }
        for (auto n = in.count(65536); n != 0; --n)
            state.resources.push_back(in.resource());
        for (auto n = in.count(4096); n != 0; --n)
            state.frame_list.push_back(in.resource());
        program.resident.math.trigonometry = in.blob(0x4000);
        state.replay_widths = in.blob(256);
        const auto event_component = in.blob();
        if (!event_component.empty()) {
            state.event_package = field::parse_event_package(event_component);
            program.resident.variables.unsigned_bitmap = state.event_package.variable_unsigned_bits;
        }
        const auto values = in.block<2048>();
        for (std::size_t i = 0; i < 1024; ++i)
            program.resident.variables.words[i] =
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(values[i * 2]) |
                                           (static_cast<std::uint16_t>(values[i * 2 + 1]) << 8U));
        auto &control = state.event_control;
        control.budget_mode = signed_word(in.word());
        control.break_requested = signed_word(in.word());
        control.batch_limit = signed_word(in.word());
        control.post_initialization = signed_word(in.word());
        for (auto &gate : control.gate_values)
            gate = signed_word(in.word());
        control.diagnostic_suppression = signed_word(in.word());
        auto &battle = program.resident.battle_request;
        battle.field_active = in.word();
        battle.menu_gate = in.word();
        battle.gate_90 = in.word();
        battle.pending = in.word();
        battle.selector = static_cast<std::uint8_t>(in.count(255));
        battle.mode = static_cast<std::uint8_t>(in.count(255));
        battle.resident_flag = static_cast<std::uint8_t>(in.count(255));
        state.battle_mode_source = static_cast<std::uint8_t>(in.count(255));
        program.resident.music.gate = in.word();
        in.finish();
        if (selected >= 2 && (event_component.empty() || count == 0 || actor_index >= count))
            throw InputError(
                "Event entry requires owned actor storage and a parsed event component");
        executing = true;
        std::size_t requested_start = 0, released_start = 0;
        const game::ProgramObserver observer = [&](const game::Program &active,
                                                   game::SourcePoint at, bool completed) {
            point = at;
            if (!completed) {
                if (work == budget)
                    throw HostBudget(
                        "Analysis operation budget exhausted before the next operation");
                ++work;
                if (at.operation == "create_field_sprite") {
                    requested_start = services.requested.size();
                    released_start = services.released.size();
                }
                return;
            }
            std::ostringstream out;
            out << '{';
            point_json(out, at);
            if (at.operation == "restore_field_data") {
                out << ",\"actors\":[";
                for (std::size_t i = 0; i < active.field->actors.size(); ++i) {
                    if (i != 0)
                        out << ',';
                    actor_storage(out, active.field->actors[i]);
                }
                out << "],\"globals\":";
                globals(out, active);
                out << ",\"variables\":" << quote(variables(active))
                    << ",\"descriptor_count\":" << active.field->descriptor_count
                    << ",\"bytes_used\":" << active.field->snapshot_bytes_used;
            } else if (at.operation == "create_field_sprite") {
                const auto &actor = active.field->actors.at(*at.actor);
                out << ",\"actor_bytes\":" << quote(hex(actor.storage))
                    << ",\"descriptor\":" << quote(hex(actor.descriptor)) << ",\"sprite\":";
                allocation(out, actor.sprite.sprite);
                out << ",\"parts\":";
                allocation(out, actor.sprite.parts);
                out << ",\"environment\":";
                numbers(out, environment(active));
                const auto &a = *at.sprite_arguments;
                out << ",\"arguments\":";
                numbers(out, std::array<std::uint32_t, 7>{a.actor_index, a.resource_slot,
                                                          a.resource, a.mode, a.part_variant, a.tag,
                                                          a.defer_initial_step});
                out << ",\"requested\":";
                numbers(out, std::span(services.requested).subspan(requested_start));
                out << ",\"released\":";
                numbers(out, std::span(services.released).subspan(released_start));
            } else {
                out << ",\"state\":";
                snapshot(out, active);
            }
            out << '}';
            auto checkpoint = out.str();
            report_bytes += checkpoint.size();
            if (report_bytes > 64U * 1024U * 1024U)
                throw HostBudget(
                    "Analysis checkpoint output exceeded 64 MiB; reduce the selected boundary");
            checkpoints.push_back(std::move(checkpoint));
        };
        for (std::uint32_t i = 0; i < repeats; ++i) {
            if (selected == 0)
                program.restore_field(
                    [&](auto size, auto mode) { return services.allocate(size, mode); },
                    [&](auto address) { services.release(address); }, {}, observer,
                    [&](const auto &request) { services.uploads.push_back(request); });
            else if (selected == 1)
                program.restore_field_data({}, observer);
            else if (selected == 2) {
                const auto result = program.event_pass(observer);
                std::ostringstream out;
                out << "{\"visited\":" << result.visited
                    << ",\"dispatched_actors\":" << result.dispatched_actors
                    << ",\"diagnostic_requests\":" << result.diagnostic_requests
                    << ",\"gate_stopped_pass\":" << (result.gate_stopped_pass ? "true" : "false")
                    << '}';
                entry_result = out.str();
            } else {
                const auto result = program.event_batch(actor_index, batch_limit, observer);
                constexpr std::array<std::string_view, 5> reasons{
                    "nonpositive_limit", "control_gate", "handler_break", "limit", "safeguard"};
                std::ostringstream out;
                out << "{\"batch_exit\":"
                    << quote(reasons.at(static_cast<std::size_t>(result.reason)))
                    << ",\"dispatched\":" << result.dispatched << ",\"diagnostic_requested\":"
                    << (result.diagnostic_requested ? "true" : "false") << '}';
                entry_result = out.str();
            }
            ++completed_entries;
        }
        status = "completed_boundary";
        reason = "Selected recovered entry returned normally";
    } catch (const HostBudget &error) {
        status = "host_budget_exhausted";
        reason = error.what();
    } catch (const game::MissingDependency &error) {
        status = error.recovered ? "dependency_not_connected" : "dependency_needs_recovery";
        reason = error.what();
        point = error.point;
        dependency = error.dependency;
    } catch (const field::UnrecoveredSpriteCommand &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        opcode = error.opcode;
        sprite_pc = error.command_pc;
        point.operation = "ordinary_sprite_command";
        point.sprite_pc = error.command_pc;
        point.machine_address = error.machine_address;
        dependency = "instruction:sprite:0x" + hex(std::span(&error.opcode, 1));
    } catch (const field::UnsupportedInstruction &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        point.event_pc = error.pc;
        opcode = error.opcode;
        dependency = "instruction:primary:0x" + hex(std::span(&error.opcode, 1));
    } catch (const field::UnsupportedExtendedInstruction &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        point.event_pc = error.pc;
        opcode = error.opcode;
        dependency = "instruction:extended:0x" + hex(std::span(&error.opcode, 1));
    } catch (const field::UnrecoveredEncounter &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        dependency = "symbol:field-control-encounter";
    } catch (const field::UnrecoveredSpriteBehavior &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        dependency = "symbol:field-return-sprite-ownership";
    } catch (const field::SpriteInputError &error) {
        status = "invalid_input";
        reason = error.what();
    } catch (const InputError &error) {
        status = "invalid_input";
        reason = error.what();
    } catch (const field::original::ReturnFormatError &error) {
        status = "invalid_input";
        reason = error.what();
    } catch (const field::FieldFormatError &error) {
        status = "invalid_input";
        reason = error.what();
    } catch (const std::exception &error) {
        status = executing ? "reconstruction_error" : "invalid_input";
        reason = error.what();
    }
    std::cout << "{\"entry\":" << quote(entry) << ",\"status\":" << quote(status)
              << ",\"reason\":" << quote(reason) << ",\"dependency\":" << quote(dependency)
              << ",\"operations\":" << work << ",\"completed_entries\":" << completed_entries
              << ",\"entry_result\":" << entry_result
              << ",\"continuation\":\"restart_from_immutable_input\",\"location\":{";
    point_json(std::cout, point);
    std::cout << "},\"opcode\":";
    if (opcode)
        std::cout << +*opcode;
    else
        std::cout << "null";
    std::cout << ",\"sprite_bytecode_pc\":";
    if (sprite_pc)
        std::cout << *sprite_pc;
    else
        std::cout << "null";
    std::cout << ",\"checkpoints\":[";
    for (std::size_t i = 0; i < checkpoints.size(); ++i) {
        if (i != 0)
            std::cout << ',';
        std::cout << checkpoints[i];
    }
    std::cout << "],\"uploads\":[";
    for (std::size_t i = 0; i < services.uploads.size(); ++i) {
        if (i != 0)
            std::cout << ',';
        const auto &request = services.uploads[i];
        std::cout << "{\"rectangle\":";
        numbers(std::cout, request.rectangle);
        std::cout << ",\"source_address\":" << request.source_address
                  << ",\"bytes\":" << quote(hex(request.bytes)) << '}';
    }
    std::cout << "],\"partial_state\":";
    snapshot(std::cout, program);
    std::cout << "}\n";
    return status == "completed_boundary" ? 0 : 1;
}
