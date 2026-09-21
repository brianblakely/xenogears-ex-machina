#include "xem/reconstruction/field_sprite_factory.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Function> void rejects(Function &&function) {
    try {
        function();
    } catch (const field::SpriteError &) {
        return;
    }
    throw std::runtime_error("Unqualified sprite operation must fail explicitly");
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    check(at <= bytes.size() && width <= bytes.size() - at, "Test write out of bounds");
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width = 4) {
    check(at <= bytes.size() && width <= bytes.size() - at, "Test read out of bounds");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (i * 8);
    return value;
}
struct Fixture {
    static constexpr std::uint32_t address = 0x80001000;
    static constexpr std::uint32_t resource = 0x80010000;
    field::SpriteAllocation incoming{address, std::vector<std::uint8_t>(356)};
    std::vector<std::uint8_t> data = std::vector<std::uint8_t>(512);
    std::vector<std::uint8_t> trig = std::vector<std::uint8_t>(16384);
    std::vector<std::uint8_t> widths = std::vector<std::uint8_t>(256);
    std::array<field::SpriteResource, 1> regions;
    field::SpriteEnvironment environment{1, 0, 19, 31, 0x80009900};
    Fixture() : regions{{{resource, data}}} {
        for (std::size_t i = 0; i < incoming.bytes.size(); ++i)
            incoming.bytes[i] = static_cast<std::uint8_t>(17 * i + 29);
        put(incoming.bytes, 0x82, 4096, 2);
        put(data, 4, 0x20);
        put(data, 8, 0x10);
        put(data, 12, 0x1f0);
        put(data, 0x10, 0x403, 2);
        put(data, 0x20, 1, 2);
        put(data, 0x22, 0x20, 2);
        put(data, 0x40, 2, 2);
        put(data, 0x42, 0x7e, 2);
        for (std::size_t i = 0; i < 5; ++i)
            put(data, 0x44 + i * 2, static_cast<std::uint32_t>(0xa0 - (0x44 + i * 2)), 2);
        put(data, 0xa0, 11, 2);
        data[0xc0] = 0x80;
        for (std::size_t i = 0; i < 4096; ++i)
            put(trig, i * 4 + 2, 4096, 2);
    }
    field::SpriteSources sources() { return {regions, {}, trig, widths, {}}; }
    field::SpriteConstruction construct() {
        field::SpriteConstruction result;
        field::construct_sprite(result, incoming, resource, {100, -1, 300, -4}, environment,
                                sources(), [](std::uint32_t size, std::uint32_t mode) {
                                    check(size == 48 && mode == 0, "Part allocation boundary");
                                    return field::SpriteAllocation{
                                        0x80002000, std::vector<std::uint8_t>(size, 0xa7)};
                                });
        return result;
    }
};
void field_factory_and_task_boundaries() {
    Fixture fixture;
    put(fixture.data, 0x12, 0x160, 2);
    put(fixture.data, 0xa0, 0, 2);
    fixture.data[0x171] = 11;
    fixture.data[0x172] = 22;
    fixture.data[0x173] = 33;
    std::array<std::uint8_t, 32> coordinates{};
    put(coordinates, 16, 300, 2);
    put(coordinates, 18, 65532, 2);
    const std::array<field::SpriteResource, 2> regions{
        {{Fixture::resource, fixture.data}, {0x800b1f78, coordinates}}};
    auto sources = fixture.sources();
    sources.resources = regions;
    std::array<std::uint8_t, 312> actor{};
    std::array<std::uint8_t, 92> descriptor{};
    put(actor, 0x20, 0x12348000);
    put(actor, 0x24, 0xfffd0000);
    put(actor, 0x28, 0xfffe8000);
    put(actor, 0x1a, 0x7777, 2);
    field::FieldSpriteEnvironment environment{fixture.environment,     1, 1, 7, {5, 99, 88},
                                              {0, 7, 0x11112222, 0, 9}};
    field::FieldSpriteArguments arguments{18, 2, Fixture::resource, 0, 0, 130, 1};
    std::vector<std::uint32_t> allocations, releases;
    const auto allocate = [&](std::uint32_t size, std::uint32_t mode) {
        check(mode == 0, "Field factory allocator mode");
        allocations.push_back(size);
        if (size == 356)
            return fixture.incoming;
        return field::SpriteAllocation{size == 768 ? 0x80003000U : 0x80002000U,
                                       std::vector<std::uint8_t>(size, 0xa7)};
    };
    const auto release = [&](std::uint32_t address) {
        check(get(descriptor, 4) == Fixture::address,
              "Factory publishes sprite before replacing parts");
        releases.push_back(address);
    };
    field::SpriteConstruction result;
    field::create_field_sprite(result, actor, descriptor, arguments, environment, sources, allocate,
                               release);
    check(
        allocations == std::vector<std::uint32_t>{356, 48} && releases.empty() &&
            get(descriptor, 4) == Fixture::address && (get(descriptor, 0x58) & 0x10000) &&
            get(result.sprite.bytes, 0x114, 2) == 300 &&
            get(result.sprite.bytes, 0x116, 2) == 65532 &&
            get(result.sprite.bytes, 0x118, 2) == 256 &&
            get(result.sprite.bytes, 0x11a, 2) == 482 && get(result.sprite.bytes, 0x108, 2) == 18 &&
            get(result.sprite.bytes, 0x68) == 0x80076a74,
        "Field factory preserves original wrapper coordinates, publication and callback identity");
    check(get(descriptor, 0x20) == 0x1234 && get(descriptor, 0x40) == 0x1234 &&
              get(descriptor, 0x24) == 0xfffffffd && get(descriptor, 0x44) == 0xfffffffd &&
              get(descriptor, 0x28) == 0xfffffffe && get(descriptor, 0x48) == 0xfffffffe &&
              get(result.sprite.bytes, 0) == 0x12348000 &&
              get(result.sprite.bytes, 0x84, 2) == 65533 && get(actor, 0x1a, 2) == 0x7777 &&
              environment.initialized_count == 8 && environment.heap.allocation_class == 8 &&
              environment.heap.class_eight_context == 0 && environment.heap.allocation_cursor == 0,
          "Return-mode factory retains actor bounds and publishes signed coarse/fixed positions");
    check(field::initial_sprite_bounds({result.sprite.address, result.sprite.bytes}, sources) ==
              std::array<std::int32_t, 3>{16, 24, 8},
          "Initial bounds use original frame bytes and current three-quarter sprite scale");

    descriptor.fill(0);
    arguments.mode = 1;
    environment.return_mode = 0;
    allocations.clear();
    result = {};
    field::create_field_sprite(result, actor, descriptor, arguments, environment, sources, allocate,
                               release);
    check(allocations == std::vector<std::uint32_t>{356, 48, 768} &&
              releases == std::vector<std::uint32_t>{0x80002000} &&
              result.parts.address == 0x80003000 &&
              result.parts.bytes == std::vector<std::uint8_t>(768, 0xa7) &&
              get(result.sprite.bytes, 0xe0) == 0x80003000 &&
              get(result.sprite.bytes, 0xe4) == 0x80003000 &&
              get(result.sprite.bytes, 0x114, 2) == 0x280 && get(actor, 0x1a, 2) == 0x40,
          "Mode-one factory executes original part release/reallocation and actor bounds");

    descriptor.fill(0);
    arguments.mode = 0;
    arguments.defer_initial_step = 0;
    environment.sprite.rate_control = 0;
    fixture.data[0xc0] = 0xc6;
    fixture.data[0xc1] = 255;
    fixture.data[0xc2] = 0x30;
    fixture.widths[0xc6] = 2;
    result = {};
    field::create_field_sprite(result, actor, descriptor, arguments, environment, sources, allocate,
                               release);
    check(get(actor, 0x1a, 2) == 66 && get(actor, 0xea, 2) == 255 && (get(actor, 4) & 0x1000000) &&
              get(result.sprite.bytes, 0x100, 2) == 255 &&
              get(result.sprite.bytes, 0x64) == Fixture::resource + 0xc3 &&
              environment.tasks.next == 0 && environment.tasks.current == 0x11112222,
          "Factory composes initial VM, empty task pass and sequencer sentinel actor effect");
    rejects([&] {
        field::SpriteConstruction partial;
        field::create_field_sprite(partial, actor, descriptor, arguments, environment, sources,
                                   allocate, release);
    });
    field::SpriteTaskState tasks{1, 9, 11, 0xdeadbeef, 13};
    field::advance_sprite_tasks(tasks, environment.sprite, {});
    check(tasks.wait_count == 0 && tasks.wait_flag == 0 && tasks.current == 11 && tasks.next == 13,
          "Task wait boundary decrements and clears only the original halfword flag");
    std::array<std::uint8_t, 28> node{};
    put(node, 8, 0x80012340);
    const std::array<field::SpriteResource, 1> task_regions{{{0x80005000, node}}};
    sources.resources = task_regions;
    tasks.head = 0x80005000;
    rejects([&] { field::advance_sprite_tasks(tasks, environment.sprite, sources); });
    check(tasks.current == 0x80005000 && tasks.next == 0,
          "Unknown task callback retains the original pre-call cursor stores and fails");
}

void interrupted_factory_retains_ownership() {
    Fixture fixture;
    put(fixture.data, 0x12, 0x160, 2);
    put(fixture.data, 0xa0, 0, 2);
    fixture.data[0x171] = 11;
    fixture.data[0x172] = 22;
    fixture.data[0x173] = 33;
    fixture.data[0xc0] = 0x97;
    const field::FieldSpriteEnvironment initial{{0, 0, 19, 31, 0x80009900}, 1, 1, 7, {5, 99, 88},
                                                {0, 7, 0x11112222, 0, 9}};
    const field::FieldSpriteArguments arguments{2, 2, Fixture::resource, 1, 0, 2, 0};
    std::array<std::uint8_t, 312> actor{};
    std::array<std::uint8_t, 92> descriptor{};
    auto environment = initial;
    field::SpriteConstruction result;
    std::vector<std::uint32_t> releases;
    bool reject_replacement = false;
    const auto allocate = [&](std::uint32_t size, std::uint32_t mode) {
        check(mode == 0, "Interrupted factory allocator mode");
        if (size == 356)
            return fixture.incoming;
        if (size == 768 && reject_replacement)
            throw field::SpriteError("Synthetic unavailable replacement allocation");
        return field::SpriteAllocation{size == 768 ? 0x80003000U : 0x80002000U,
                                       std::vector<std::uint8_t>(size, 0xa7)};
    };
    const auto release = [&](std::uint32_t address) { releases.push_back(address); };
    bool missing_command = false;
    try {
        field::create_field_sprite(result, actor, descriptor, arguments, environment,
                                   fixture.sources(), allocate, release);
    } catch (const field::UnrecoveredSpriteCommand &error) {
        missing_command = error.command_pc == Fixture::resource + 0xc0 && error.opcode == 0x97 &&
                          error.machine_address == 0x800248d4;
    }
    check(missing_command && result.sprite.address == Fixture::address &&
              result.sprite.bytes.size() == 356 && get(descriptor, 4) == result.sprite.address &&
              (get(descriptor, 0x58) & 0x10000) && result.parts.address == 0x80003000 &&
              result.parts.bytes.size() == 768 &&
              get(result.sprite.bytes, 0xe0) == result.parts.address &&
              get(result.sprite.bytes, 0x64) == Fixture::resource + 0xc0 &&
              get(result.sprite.bytes, 0x9e, 2) == 0 && result.environment == environment.sprite &&
              environment.initialized_count == 7 &&
              releases == std::vector<std::uint32_t>{0x80002000},
          "Missing command retains published sprite, replacement parts, timer and environment");
    const auto retained = result.sprite;
    rejects([&] {
        field::create_field_sprite(result, actor, descriptor, arguments, environment,
                                   fixture.sources(), allocate, release);
    });
    check(result.sprite == retained && releases.size() == 1,
          "Interrupted construction cannot silently overwrite its owned allocation");

    actor.fill(0);
    descriptor.fill(0);
    environment = initial;
    result = {};
    releases.clear();
    reject_replacement = true;
    rejects([&] {
        field::create_field_sprite(result, actor, descriptor, arguments, environment,
                                   fixture.sources(), allocate, release);
    });
    check(result.sprite.address == Fixture::address && get(descriptor, 4) == Fixture::address &&
              result.parts.address == 0 && result.parts.bytes.empty() &&
              releases == std::vector<std::uint32_t>{0x80002000} &&
              get(result.sprite.bytes, 0xe0) == 0x80002000 &&
              result.environment == environment.sprite && environment.initialized_count == 7,
          "Failed replacement retains sprite stores while marking released parts unowned");
}

void task_host_limit_preserves_cursor() {
    std::array<std::uint8_t, 56> nodes{};
    constexpr std::uint32_t first = 0x80005000;
    constexpr std::uint32_t second = first + 28;
    put(nodes, 24, second);
    const std::array<field::SpriteResource, 1> resources{{{first, nodes}}};
    field::SpriteSources sources{resources, {}, {}, {}, {}};
    field::SpriteTaskState state{0, 0, 0, first, 0};
    field::SpriteEnvironment environment{};
    struct HostExecutionLimit {};
    unsigned steps = 0;
    sources.observe_execution = [&](field::SpriteExecutionPoint point) {
        check(point.operation == "sprite_task" && point.machine_address == 0x8001c964 &&
                  !point.command_pc,
              "Task observation does not invent a bytecode PC");
        if (++steps == 2)
            throw HostExecutionLimit{};
    };
    bool stopped = false;
    try {
        field::advance_sprite_tasks(state, environment, sources);
    } catch (const HostExecutionLimit &) {
        stopped = true;
    }
    check(stopped && state.current == first && state.next == second,
          "Host interruption retains completed task-list cursor stores");

    sources.observe_execution = {};
    put(nodes, 24, first);
    rejects([&] { field::advance_sprite_tasks(state, environment, sources); });
    check(state.current == first && state.next == first,
          "Malformed task cycles remain input errors independent of host limits");
}

void owner_task_removal() {
    constexpr std::uint32_t owner = 0x80001000, first = 0x80002000;
    std::vector<std::uint8_t> sprite(356);
    put(sprite, 16, 0xe0000042);
    field::SpriteTaskState state{};
    state.pending_head = first;
    state.head = first + 4 * 28;
    state.next = first + 28;
    state.nodes.push_back({first, std::vector<std::uint8_t>(5 * 28)});
    auto &nodes = state.nodes[0].bytes;
    for (std::size_t i = 0; i < 5; ++i) {
        put(nodes, i * 28, owner);
        put(nodes, i * 28 + 20, 0x42);
        if (i < 3)
            put(nodes, i * 28 + 24, first + static_cast<std::uint32_t>((i + 1) * 28));
    }
    put(nodes, 2 * 28 + 20, 0x40000042); // Protected, even with matching generation.
    put(nodes, 3 * 28 + 20, 0x43);       // Reused owner address with another generation.
    field::SpriteSources sources{{}, {}, {}, {}, {}};
    field::remove_sprite_tasks(state, owner, {owner, sprite}, sources);
    check(state.pending_head == first + 2 * 28 && state.head == 0 && state.next == first + 2 * 28 &&
              get(nodes, 2 * 28 + 24) == first + 3 * 28,
          "Both lists remove consecutive matching generations, preserving protected/reused owners");

    // Middle unlink commits before an unsupported destructor; current is not rewritten.
    state.pending_head = first;
    state.head = 0;
    state.current = 0x55;
    put(nodes, 0, owner + 4);
    put(nodes, 24, first + 28);
    put(nodes, 28 + 12, 0x80022eb8);
    rejects([&] { field::remove_sprite_tasks(state, owner, {owner, sprite}, sources); });
    check(get(nodes, 24) == first + 2 * 28 && state.current == 0x55,
          "Removal preserves committed links at its destruction dependency");
    put(nodes, 24, first);
    rejects([&] { field::remove_sprite_tasks(state, owner, {owner, sprite}, sources); });
    state.pending_head = first + 0x1000;
    rejects([&] { field::remove_sprite_tasks(state, owner, {owner, sprite}, sources); });

    state = {};
    // An empty source list does not dereference an otherwise unavailable owner.
    field::remove_sprite_tasks(state, 0, {owner, sprite}, sources);
}

struct Transport {
    std::vector<std::uint8_t> data;
    std::size_t cursor = 0;
    std::uint32_t word() {
        const auto value = get(data, cursor);
        cursor += 4;
        return value;
    }
    std::vector<std::uint8_t> blob() {
        const auto size = word();
        check(cursor <= data.size() && size <= data.size() - cursor, "Truncated factory transport");
        const auto begin = data.begin() + static_cast<std::ptrdiff_t>(cursor);
        cursor += size;
        return {begin, begin + size};
    }
    field::SpriteAllocation block() {
        const auto address = word();
        return {address, blob()};
    }
    void word(std::uint32_t value) {
        const auto at = data.size();
        data.resize(at + 4);
        put(data, at, value);
    }
    void blob(std::span<const std::uint8_t> value) {
        check(value.size() <= std::numeric_limits<std::uint32_t>::max(),
              "Oversize factory transport");
        word(static_cast<std::uint32_t>(value.size()));
        data.insert(data.end(), value.begin(), value.end());
    }
    void block(const field::SpriteAllocation &value) {
        word(value.address);
        blob(value.bytes);
    }
    field::FieldSpriteEnvironment environment() {
        field::FieldSpriteEnvironment result;
        result.sprite.rate_control = std::bit_cast<std::int32_t>(word());
        result.sprite.platform_mode = static_cast<std::uint8_t>(word());
        result.sprite.variant = word();
        result.sprite.binding_control = static_cast<std::uint8_t>(word());
        result.sprite.frame_head = word();
        result.return_mode = word();
        result.field_gate = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(word()));
        result.initialized_count = word();
        result.heap.allocation_class = static_cast<std::uint16_t>(word());
        result.heap.class_eight_context = word();
        result.heap.allocation_cursor = word();
        result.tasks.wait_count = word();
        result.tasks.wait_flag = static_cast<std::uint16_t>(word());
        result.tasks.current = word();
        result.tasks.head = word();
        result.tasks.next = word();
        return result;
    }
    void environment(const field::FieldSpriteEnvironment &value) {
        word(static_cast<std::uint32_t>(value.sprite.rate_control));
        word(value.sprite.platform_mode);
        word(value.sprite.variant);
        word(value.sprite.binding_control);
        word(value.sprite.frame_head);
        word(value.return_mode);
        word(static_cast<std::uint16_t>(value.field_gate));
        word(value.initialized_count);
        word(value.heap.allocation_class);
        word(value.heap.class_eight_context);
        word(value.heap.allocation_cursor);
        word(value.tasks.wait_count);
        word(value.tasks.wait_flag);
        word(value.tasks.current);
        word(value.tasks.head);
        word(value.tasks.next);
    }
};

void original_transport(const char *input_path, const char *output_path) {
    std::ifstream stream(input_path, std::ios::binary);
    check(stream.is_open(), "Cannot open original factory input");
    Transport input{{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()}},
        output;
    const auto operation = input.word();
    field::FieldSpriteArguments arguments;
    arguments.actor_index = input.word();
    arguments.resource_slot = input.word();
    arguments.resource = input.word();
    arguments.mode = input.word();
    arguments.part_variant = input.word();
    arguments.tag = static_cast<std::uint8_t>(input.word());
    arguments.defer_initial_step = input.word();
    auto environment = input.environment();
    auto actor = input.blob(), descriptor = input.blob();
    std::vector<field::SpriteAllocation> allocations, resource_storage, list_storage;
    const auto blocks = [&](auto &storage) {
        const auto count = input.word();
        check(count <= 4096, "Excess original factory blocks");
        for (std::uint32_t i = 0; i < count; ++i)
            storage.push_back(input.block());
    };
    blocks(allocations);
    blocks(resource_storage);
    blocks(list_storage);
    std::vector<field::SpriteResource> resources, list;
    for (const auto &block : resource_storage)
        resources.push_back({block.address, block.bytes});
    for (const auto &block : list_storage)
        list.push_back({block.address, block.bytes});
    const auto trig = input.blob(), widths = input.blob();
    check(input.cursor == input.data.size(), "Trailing original factory inputs");
    field::SpriteSources sources{resources, list, trig, widths, {}};
    std::vector<std::uint32_t> requests, released;
    std::size_t next = 0;
    const auto allocate = [&](std::uint32_t size, std::uint32_t mode) {
        check(mode == 0 && next < allocations.size() && allocations[next].bytes.size() == size,
              "Original factory allocation extent differs");
        requests.push_back(size);
        return allocations[next++];
    };
    const auto release = [&](std::uint32_t address) { released.push_back(address); };
    if (operation == 0) {
        field::SpriteConstruction result;
        field::create_field_sprite(result, actor, descriptor, arguments, environment, sources,
                                   allocate, release);
        check(next == allocations.size(), "Original factory omitted an allocation");
        output.environment(environment);
        output.blob(actor);
        output.blob(descriptor);
        output.block(result.sprite);
        output.block(result.parts);
        output.word(static_cast<std::uint32_t>(requests.size()));
        for (const auto size : requests)
            output.word(size);
        output.word(static_cast<std::uint32_t>(released.size()));
        for (const auto address : released)
            output.word(address);
    } else if (operation == 1) {
        check(allocations.size() == 1, "Bounds transport requires its sprite window");
        const auto bounds =
            field::initial_sprite_bounds({allocations[0].address, allocations[0].bytes}, sources);
        for (const auto value : bounds)
            output.word(static_cast<std::uint32_t>(value));
    } else if (operation == 2) {
        field::advance_sprite_tasks(environment.tasks, environment.sprite, sources);
        output.environment(environment);
    } else {
        throw std::runtime_error("Unknown factory transport operation");
    }
    std::ofstream destination(output_path, std::ios::binary);
    destination.write(reinterpret_cast<const char *>(output.data.data()),
                      static_cast<std::streamsize>(output.data.size()));
    check(static_cast<bool>(destination), "Cannot write original factory output");
}

} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--original") {
            original_transport(argv[2], argv[3]);
            return 0;
        }
        check(argc == 1, "Usage: test-field-sprite-factory [--original INPUT OUTPUT]");
        field_factory_and_task_boundaries();
        interrupted_factory_retains_ownership();
        task_host_limit_preserves_cursor();
        owner_task_removal();
        std::cout << "Field sprite factory reconstruction passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
