// The battle's loading task (the setup module's task 801e6fec, which 800b81bc
// registers with its argument, the formation's enemy set file): its states
// run once per battle frame until the party and enemy sprites, the battle
// images and the effect bank are loaded, then it sets 800ccc58, which ends
// the loading loop of 80070f40 (80071248).
#include "xem/reconstruction/field_motion.hpp"
#include "xem/reconstruction/field_sprite.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/sound_driver.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace xem::reconstruction {
namespace {
constexpr std::uint32_t placements = 0x800c3eb0; // 0x1c per slot: +6 type, +7, +8, +a, +e x, +10 z
constexpr std::uint32_t placement_stride = 0x1c;
constexpr std::uint32_t sprite_rows = 0x800ccbd4;    // 12 per row: data, image x, y, variant
constexpr std::uint32_t slot_sprites = 0x800ccb3c;   // sprite per slot
constexpr std::uint32_t slot_tasks = 0x800ccb68;     // sprite task per slot
constexpr std::uint32_t enemy_copy = 0x800d39c8;     // 801e6314's copy of the enemy set data
constexpr std::uint32_t loaded = 0x800ccc58;         // set when loading ends
constexpr std::uint32_t member_files = 0x801e95bc;   // 8 per type: file (dir 2d), sequencer word
constexpr std::uint32_t member_widths = 0x801e962c;  // byte per type: image columns
constexpr std::uint32_t member_cursor = 0x801e9638;  // u16: the next member image column
constexpr std::uint32_t battle_images = 0x801e96b4;  // the image file 801e6dc8 uploads
constexpr std::uint32_t shared_binding = 0x8006be10; // 80022224's binding of file 2
constexpr std::uint32_t replay_widths = 0x8004fc40;

// Task states (the node's +8 callback).
constexpr std::uint32_t load_start = 0x801e6fec;
constexpr std::uint32_t load_members = 0x801e6f00;
constexpr std::uint32_t load_images = 0x801e6e48;
constexpr std::uint32_t load_sound = 0x801e6d6c;
constexpr std::uint32_t load_wait = 0x801e6d34;
constexpr std::uint32_t load_settle = 0x801e6c80;

std::int32_t s16(std::uint32_t value) { return static_cast<std::int16_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
std::uint32_t placement(std::uint32_t slot) { return placements + slot * placement_stride; }
std::uint32_t row(std::uint32_t index) { return sprite_rows + index * 12; }
// 800288ec: a byte size rounded up to words, as a signed MIPS quotient.
std::uint32_t word_size(std::uint32_t size) {
    const auto rounded = static_cast<std::int32_t>(size + 3U);
    const auto adjusted = rounded >= 0 ? rounded : static_cast<std::int32_t>(size + 6U);
    return static_cast<std::uint32_t>(adjusted >> 2) << 2U;
}
MissingDependency missing(const char *operation, std::uint32_t address, const char *symbol,
                          const char *reason) {
    return MissingDependency({operation, address, {}, {}}, symbol, false, reason);
}
} // namespace

std::uint32_t Program::battle_block(std::uint32_t size, std::uint32_t mode, std::uint32_t site) {
    auto block = resident::heap_allocate(resident.heap, size, mode, site);
    if (!block)
        throw battle::BattleError("A quiet null allocation in the battle loader");
    const auto address = block->address;
    battle->regions.emplace(address, std::move(block->bytes));
    return address;
}

// 800320e8 of an allocated block whatever Program value holds its bytes:
// battle memory, heap contents or task nodes, wholly inside the block.
void Program::release_battle_heap(std::uint32_t address, std::uint32_t site) {
    const auto header = resident.heap.headers.find(address - 8);
    if (header == resident.heap.headers.end())
        throw battle::BattleError("A released battle block is not a heap block");
    const auto end = header->second[0] - 8;
    resident::HeapBlock block{address, std::vector<std::uint8_t>(end - address)};
    std::vector<bool> filled(block.bytes.size());
    const auto gather = [&](auto &owners) {
        for (auto run = owners.lower_bound(address); run != owners.end() && run->first < end;) {
            if (run->first + run->second.size() > end)
                throw battle::BattleError("An owner crosses a released battle block");
            std::ranges::copy(run->second, block.bytes.begin() + (run->first - address));
            std::fill_n(filled.begin() + (run->first - address), run->second.size(), true);
            run = owners.erase(run);
        }
    };
    gather(battle->regions);
    gather(resident.heap_contents);
    auto &nodes = resident.sprite_tasks.nodes;
    for (auto node = nodes.begin(); node != nodes.end();) {
        if (node->address < address || node->address >= end) {
            ++node;
            continue;
        }
        if (node->address + node->bytes.size() > end)
            throw battle::BattleError("A task node crosses a released battle block");
        std::ranges::copy(node->bytes, block.bytes.begin() + (node->address - address));
        std::fill_n(filled.begin() + (node->address - address), node->bytes.size(), true);
        node = nodes.erase(node);
    }
    if (!std::ranges::all_of(filled, [](bool value) { return value; }))
        throw battle::BattleError("A released battle block holds unowned bytes");
    if (resident::heap_release(resident.heap, block, site) == -1)
        battle->regions.emplace(address, std::move(block.bytes)); // kept
}

// Owned task bytes become another owner's (a read list inside a task node).
std::vector<std::uint8_t> Program::take_task_bytes(std::uint32_t address, std::uint32_t size) {
    auto &nodes = resident.sprite_tasks.nodes;
    const auto node = std::ranges::find_if(nodes, [&](const auto &piece) {
        return address >= piece.address && address - piece.address + size <= piece.bytes.size();
    });
    if (node == nodes.end())
        throw battle::BattleError("Taken task bytes are not one task node's");
    const auto offset = address - node->address;
    auto bytes = std::move(node->bytes);
    const auto start = node->address;
    nodes.erase(node);
    std::vector<std::uint8_t> taken(bytes.begin() + offset, bytes.begin() + offset + size);
    if (offset != 0)
        nodes.push_back({start, {bytes.begin(), bytes.begin() + offset}});
    if (offset + size < bytes.size())
        nodes.push_back({address + size, {bytes.begin() + offset + size, bytes.end()}});
    return taken;
}

// A new list read's list replaces the last one, whose bytes go back to
// what holds them: the task node sharing its heap block, else battle memory.
void Program::return_task_list() {
    auto &list = resident.disc_read.list;
    if (list.address == 0 || list.bytes.empty())
        return;
    const auto &headers = resident.heap.headers;
    const auto header = headers.upper_bound(list.address);
    const auto in_node = header != headers.begin() &&
                         std::ranges::any_of(resident.sprite_tasks.nodes, [&](const auto &piece) {
                             const auto &[at, words] = *std::prev(header);
                             return piece.address >= at + 8 && piece.address < words[0] - 8;
                         });
    if (in_node)
        resident.sprite_tasks.nodes.push_back({list.address, std::move(list.bytes)});
    else
        battle->regions.emplace(list.address, std::move(list.bytes));
    list = {};
}

// 8001cc18: register `node` on the task list under `owner` (zero: none, whose
// generation is the word at 00000010).
void Program::register_task(std::uint32_t owner, std::uint32_t node) {
    auto &tasks = resident.sprite_tasks;
    set_memory(node, owner);
    const auto head = tasks.head;
    const auto generation = owner == 0 ? resident.null_owner_generation : memory(owner + 0x10);
    set_memory(node + 0xc, 0x8001cd94);
    set_memory(node + 8, 0);
    const auto serial = tasks.serial & 0x1fffffffU;
    tasks.head = node;
    set_memory(node + 0x14, generation & 0x1fffffffU);
    ++tasks.serial;
    set_memory(node + 0x10, (memory(node + 0x10) & 0xe0000000U) | serial);
    set_memory(node + 0x18, head);
    if (tasks.creation_flags == 0) {
        set_memory(node + 0x14, memory(node + 0x14) & 0x7fffffffU);
    } else {
        ++tasks.active_flags;
        set_memory(node + 0x14, memory(node + 0x14) | 0x80000000U);
    }
    ++tasks.primary_count;
}

// 8001ca58: register `node` on the pending (drawing) list under `owner`.
void Program::register_pending_task(std::uint32_t owner, std::uint32_t node) {
    auto &tasks = resident.sprite_tasks;
    set_memory(node, owner);
    const auto serial = tasks.serial & 0x1fffffffU;
    ++tasks.serial;
    set_memory(node + 0x18, tasks.pending_head);
    tasks.pending_head = node;
    const auto generation = memory(owner + 0x10);
    set_memory(node + 0x10, (memory(node + 0x10) & 0xe0000000U) | serial);
    set_memory(node + 8, 0);
    set_memory(node + 0xc, 0x8001cb48);
    ++tasks.auxiliary_count;
    set_memory(node + 0x14, generation & 0x1fffffffU);
}

// 8001cd94: unlink `node` from the task list.
void Program::unlink_task(std::uint32_t node) {
    auto &tasks = resident.sprite_tasks;
    std::uint32_t previous = 0;
    for (auto at = tasks.head; at != 0; at = memory(at + 0x18)) {
        if (at != node) {
            previous = at;
            continue;
        }
        const auto next = memory(node + 0x18);
        if (previous == 0)
            tasks.head = next;
        else
            set_memory(previous + 0x18, next);
        if (tasks.next == node)
            tasks.next = next;
        break;
    }
    if (static_cast<std::int32_t>(memory(node + 0x14)) < 0)
        --tasks.active_flags;
    --tasks.primary_count;
}

// 8001d1d8(size, owner, update, draw, destroy): a task node of `size` bytes
// (allocation mode 800591af) with a drawing node at +1c; both name the node
// (+4, +20) until the caller places its object there.
std::uint32_t Program::create_task(std::uint32_t size, std::uint32_t owner, std::uint32_t update,
                                   std::uint32_t draw, std::uint32_t destroy) {
    auto &tasks = resident.sprite_tasks;
    auto block = resident::heap_allocate(resident.heap, size, tasks.allocation_mode, 0x8001d208);
    if (!block)
        throw battle::BattleError("A quiet null allocation in 8001d1d8");
    const auto node = block->address;
    tasks.nodes.push_back({node, std::move(block->bytes)});
    register_task(owner, node);
    register_pending_task(node, node + 0x1c);
    set_memory(node + 8, update);
    set_memory(node + 0x1c + 8, draw);
    set_memory(node + 0xc, destroy != 0 ? destroy : 0x8001d19c);
    set_memory(node + 4, node);
    set_memory(node + 0x20, node);
    return node;
}

void Program::with_battle_sprites(const std::function<void(const field::SpriteSources &)> &call) {
    std::vector<field::SpriteResource> resources;
    for (const auto &[address, bytes] : battle->regions)
        resources.push_back({address, bytes});
    std::vector<field::SpriteResource> frames;
    for (const auto &node : resident.sprite_tasks.nodes)
        frames.push_back({node.address, node.bytes});
    std::array<std::uint8_t, 256> widths{};
    for (std::uint32_t i = 0; i < widths.size(); ++i)
        widths[i] = static_cast<std::uint8_t>(memory(replay_widths + i, 1));
    field::SpriteSources sources{};
    sources.resources = resources;
    sources.frame_list = frames;
    sources.trigonometry = resident.math.trigonometry;
    sources.replay_widths = widths;
    sources.tasks = &resident.sprite_tasks;
    sources.models = &resident.sprite_models;
    sources.heap = &resident.sprite_heap;
    sources.allocator = &resident.heap;
    call(sources);
}

// 800ba984 as 801e67a4 calls it: a sprite task (8001d1d8(19c, 0, 800bac50,
// 800bab0c, 800babdc)) whose sprite at +38 is built from `data` by 800242f4
// (8002435c with variant `variant`) at image (x, y) with the palette row
// `palette` (slot + 1c0), scaled 2000h, then showing `animation`.
std::uint32_t Program::create_battle_sprite_task(std::uint32_t data, std::uint32_t palette,
                                                 std::int16_t x, std::int16_t y,
                                                 std::uint32_t animation, std::uint32_t variant) {
    const auto node = create_task(0x19c, 0, 0x800bac50, 0x800bab0c, 0x800babdc);
    const auto sprite = node + 0x38;
    set_memory(node + 4, sprite);
    set_memory(node + 0x20, sprite);
    set_memory(node + 0x1c, 0);
    auto window = record_block(sprite);
    field::SpriteAllocation incoming{sprite, {window.begin(), window.end()}};
    auto environment = resident.sprite;
    environment.variant = variant;
    field::SpriteConstruction built;
    const field::SpriteAllocator allocate = [&](std::uint32_t size, std::uint32_t mode) {
        auto block = resident::heap_allocate(resident.heap, size, mode, 0x80024474);
        if (!block)
            throw battle::BattleError("A quiet null allocation in 8002435c");
        return field::SpriteAllocation{block->address, std::move(block->bytes)};
    };
    with_battle_sprites([&](const field::SpriteSources &sources) {
        field::construct_sprite(built, std::move(incoming), data,
                                {0, static_cast<std::int16_t>(palette), x, y}, environment, sources,
                                allocate);
    });
    resident.sprite = built.environment;
    resident.sprite.variant = 0;
    std::ranges::copy(built.sprite.bytes, window.begin());
    if (!built.parts.bytes.empty())
        battle->regions.emplace(built.parts.address, std::move(built.parts.bytes));
    set_memory(sprite + 0x6c, node);
    set_memory(sprite, 0);
    set_memory(sprite + 4, 0);
    set_memory(sprite + 8, 0);
    set_memory(sprite + 0xb0, animation, 1);
    set_memory(sprite + 0x3c, memory(sprite + 0x3c) | 4U);
    set_memory(sprite + 0x32, 0, 2);
    with_battle_sprites([&](const field::SpriteSources &sources) {
        field::SpriteWindow view{sprite, record_block(sprite)};
        // 80022000(sprite, 2000h).
        if (const auto renderer = memory(sprite + 0x20); renderer != 0) {
            set_memory(sprite + 0x2c, 0x2000, 2);
            for (const auto at : {10U, 8U, 6U})
                set_memory(renderer + at, 0x2000, 2);
            set_memory(sprite + 0x3c, memory(sprite + 0x3c) | 0x10000000U);
        }
        set_memory(sprite + 0x82, 0x2000, 2);
        set_memory(sprite + 0x78, 0);
        field::select_sprite_animation(view, s16(animation), resident.sprite, sources);
    });
    return node;
}

// 801e67a4(slot, row, animation): slot's sprite from sprite row `row` at its
// image position, placed and facing as its placement record says.
void Program::create_slot_sprite(std::uint32_t slot, std::uint32_t index, std::uint32_t animation) {
    const auto at = row(index);
    const auto x = static_cast<std::int16_t>(memory(at + 4, 2));
    const auto y = static_cast<std::int16_t>(memory(at + 6, 2));
    const auto node =
        create_battle_sprite_task(memory(at), slot + 0x1c0, x, y, animation, memory(at + 8));
    const auto sprite = memory(node + 4);
    const auto binding = memory(sprite + 0x24);
    set_memory(binding + 6, u32(y), 2);
    set_memory(binding + 4, u32(x), 2);
    set_memory(memory(sprite + 0x7c) + 0xe, memory(binding + 4));
    set_memory(slot_sprites + slot * 4, sprite);
    set_memory(slot_tasks + slot * 4, node);
    set_memory(sprite + 0xa8, (memory(sprite + 0xa8) & 0x3fffffffU) | slot << 30U);
    set_memory(sprite + 0xac, (memory(sprite + 0xac) & ~3U) | ((slot >> 2U) & 3U));
    const auto record = placement(slot);
    // 80021d3c: the position.
    set_memory(sprite + 8, u32(s16(memory(record + 0x10, 2))) << 16U);
    set_memory(sprite, u32(s16(memory(record + 0xe, 2))) << 16U);
    const auto angle = static_cast<std::int16_t>(memory(record + 0xa, 1) != 0 ? 0x800 : 0);
    with_battle_sprites([&](const field::SpriteSources &sources) {
        field::SpriteWindow view{sprite, record_block(sprite)};
        field::select_sprite_orientation(view, angle, resident.sprite, sources); // 800223b0
        // 80021fe0: the heading, then the velocity (80022974).
        set_memory(sprite + 0x32, u32(angle), 2);
        static_cast<void>(field::rebuild_sprite_velocity(view, sources.trigonometry));
    });
    if (memory(record + 7, 1) != 0)
        set_memory(sprite + 0x9e, 0, 2);
}

// 801e6314(data): the enemy set file's sprite rows (slots 3..) after its
// table (a count, a first image column, then 12-byte entries: data offset,
// image list offset or an earlier list's index, model flag +8, variant +b).
// The data after the table is copied (800d39c8); each image list is uploaded
// by 80022a70 at row 100h, 40h columns per image from the first column.
void Program::place_enemy_rows(FrameServices &services, std::uint32_t data, std::uint32_t frame) {
    const auto count = memory(data, 1);
    auto column = memory(data + 1, 1) * 0x40U + 0x140U;
    const auto table = count * 12U + 8U;
    const auto size = memory(data + 4) - table;
    const auto copy = battle_block(size, 0, 0x801e63b4);
    set_memory(enemy_copy, copy);
    for (std::uint32_t i = 0; i < size; ++i) // 8003f968 memcpy
        set_memory(copy + i, memory(data + table + i, 1), 1);
    const auto base = copy - table;
    std::array<std::optional<std::uint32_t>, 64> columns{};
    std::uint32_t images = 0;
    for (std::uint32_t k = 0; k < count; ++k) {
        const auto entry = data + 8 + k * 12;
        const auto images_at = memory(entry + 4);
        if (memory(entry + 8, 1) != 0)
            throw missing("place_enemy_rows", 0x800a8bf0, "symbol:battle-enemy-models",
                          "Enemy set entries with models (800a8bf0, 800bb350) are not "
                          "reconstructed");
        auto index = images_at;
        if (images_at >= 8) {
            // 80022a70(data + offset, column, 100) on 801e6314's frame.
            load_images_across(services, data + images_at, column, 0x100, frame - 0x38);
            index = images;
            columns.at(images++) = column;
            column += memory(data + images_at) * 0x40U;
            if (static_cast<std::int32_t>(column) > 0x2c0)
                column = 0;
        }
        if (!columns.at(index))
            throw missing("place_enemy_rows", 0x801e6690, "symbol:battle-enemy-image-index",
                          "An enemy entry names an image list not yet uploaded");
        const auto at = row(3 + k);
        set_memory(at + 6, 0x100, 2);
        set_memory(at, memory(entry) + base);
        set_memory(at + 4, *columns.at(index), 2);
        set_memory(at + 8, memory(entry + 11, 1));
    }
}

// 801e693c(list): the members' sprite files (directory 2d; file by type) by
// a list read at `list` in the task node.
void Program::read_member_files(std::uint32_t list) {
    static_cast<void>(select_directory(0x2c, 1));
    std::uint32_t entries = 0;
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto record = placement(member);
        const auto type = memory(record + 6, 1);
        if (type >= 0x11 || memory(record + 8, 1) != 0)
            continue;
        const auto file = memory(member_files + type * 8);
        const auto entry = list + entries * 8;
        ++entries;
        set_memory(entry, file, 2);
        const auto block =
            battle_block(word_size(file_size(static_cast<std::int32_t>(file))), 0, 0x801e69cc);
        set_memory(entry + 4, block);
        set_memory(row(member), block);
        set_memory(row(member) + 8, 0);
    }
    set_memory(list + entries * 8, 0, 2);
    set_memory(list + entries * 8 + 4, 0);
    return_task_list();
    resident.disc_read.list = {list, take_task_bytes(list, entries * 8 + 2)};
    static_cast<void>(read_files(0)); // 80029afc(list, 0, 0)
}

// 801e6ac4: each member's sprite (801e67a4 with the member's row: image at
// column 100h + the running cursor, row 1c0h), its sequencer word and a
// 300h-byte part block in place of the constructed one.
void Program::create_member_sprites() {
    for (std::uint32_t member = 0; member < 3; ++member) {
        const auto record = placement(member);
        const auto type = memory(record + 6, 1);
        if (type >= 0x11 || memory(record + 8, 1) != 0)
            continue;
        const auto cursor = memory(member_cursor, 2);
        set_memory(row(member) + 6, 0x1c0, 2);
        set_memory(row(member) + 4, cursor + 0x100, 2);
        set_memory(member_cursor, cursor + memory(member_widths + type, 1), 2);
        create_slot_sprite(member, member, 1);
        const auto sprite = memory(slot_sprites + member * 4);
        set_memory(memory(sprite + 0x7c), memory(member_files + type * 8 + 4));
        const auto renderer = memory(sprite + 0x20);
        release_battle_heap(memory(renderer + 0x2c), 0x801e6ba4);
        set_memory(renderer + 0x2c, battle_block(0x300, 0, 0x801e6bb0));
    }
    if (memory(0x800d36b8, 1) != 0)
        return;
    for (std::uint32_t member = 0; member < 3; ++member)
        if (memory(slot_sprites + member * 4) != 0)
            throw missing("create_member_sprites", 0x800ba8f4, "symbol:battle-ground-placement",
                          "Placing member sprites on the stage floor (800ba8f4) is not "
                          "reconstructed");
}

// One run of the loading task's state (the node's +8) for `node`.
void Program::battle_loader_step(std::uint32_t node, FrameServices &services) {
    const auto state = memory(node + 8);
    switch (state) {
    case load_start: {
        if (disc_busy() != 0) // 800286cc
            return;
        // A 4000h stack for 801e6314 and 801e6710.
        const auto stack = load_block(0x4000, 1, 0x801e700c);
        resident.switched_stacks.push_back({stack, 0x4000});
        const auto data = memory(node + 0x20);
        place_enemy_rows(services, data, stack + 0x3ff8 - 0xc8);
        // 801e6710: each enemy slot's sprite from its type's row.
        for (std::uint32_t slot = 3; slot < 11; ++slot) {
            const auto type = memory(placement(slot) + 6, 1);
            if (type < 8 && memory(row(type + 3)) != 0)
                create_slot_sprite(slot, type + 3, 1);
        }
        release_battle_heap(stack, 0x801e704c);
        draw_sync(services); // 800445d0(0)
        release_battle_heap(data, 0x801e7060);
        set_memory(node + 8, load_members);
        read_member_files(node + 0x30);
        return;
    }
    case load_members: {
        const auto busy = disc_busy();
        static_cast<void>(select_directory(0x2c, 0));
        if (busy != 0)
            return;
        create_member_sprites();
        // The battle files 1-3 of directory 2c by a list read at +50.
        const auto list = node + 0x50;
        const auto images = battle_block(word_size(file_size(1)), 1, 0x801e6f44);
        set_memory(list + 4, images);
        set_memory(node + 0x28, images);
        set_memory(list, 1, 2);
        const auto shared = battle_block(word_size(file_size(2)), 0, 0x801e6f68);
        set_memory(node + 0x5c, shared);
        set_memory(node + 0x24, shared);
        set_memory(0x800d2d54, shared);
        set_memory(list + 8, 2, 2);
        const auto effects = battle_block(word_size(file_size(3)), 0, 0x801e6f94);
        set_memory(node + 0x64, effects);
        set_memory(node + 0x2c, effects);
        set_memory(list + 0x10, 3, 2);
        set_memory(list + 0x1c, 0);
        set_memory(list + 0x18, 0, 2);
        return_task_list();
        resident.disc_read.list = {list, take_task_bytes(list, 0x1a)};
        static_cast<void>(read_files(0)); // 80029afc(list, 0, 0)
        set_memory(node + 8, load_images);
        return;
    }
    case load_images: {
        if (disc_busy() != 0)
            return;
        return_task_list();
        set_memory(battle_images, memory(node + 0x28));
        // 801e6dc8: 8002dde4(images) on a 2000h stack, then DrawSync(0).
        {
            const auto stack = load_block(0x2000, 1, 0x801e6dd8);
            resident.switched_stacks.push_back({stack, 0x2000});
            upload_image_sections(services, memory(battle_images), stack + 0x1efc - 0x48 + 0x10);
            draw_sync(services);
            static_cast<void>(release_owned_block(stack, 0x801e6e2c));
        }
        // 80022224(8006be10, file 2, (380, 0), (0, 1d1)).
        const auto file = memory(node + 0x24);
        set_memory(shared_binding + 4, 0x380);
        set_memory(shared_binding + 8, 0x01d10000);
        set_memory(shared_binding + 12, file + memory(file + 12));
        set_memory(shared_binding, file + memory(file + 8));
        resident.sprite.binding_control = 0;
        const auto directory = file + memory(file + 4);
        set_memory(shared_binding + 16, directory);
        if (resident.sprite.platform_mode != 0)
            if (const auto bits = (memory(directory, 2) >> 6) & 63; bits != 0)
                resident.sprite.platform_directory_bits = static_cast<std::uint8_t>(bits);
        // 80038428: file 3 becomes the linked effect bank.
        const auto bank = memory(node + 0x2c);
        auto bytes = std::move(battle->regions.at(bank));
        battle->regions.erase(bank);
        resident.sound.objects.emplace(bank, std::move(bytes));
        resident::link_effect_bank(resident.sound, bank);
        resident.sound.system_bank = bank;
        set_memory(node + 8, load_sound);
        set_memory(0x800c3d6c, 1, 1); // 800b14b8
        return;
    }
    case load_sound:
        if ((sound_wait(0) & 0xffff) != 0) // 8003bdfc(0)
            return;
        release_battle_heap(memory(node + 0x28), 0x801e6d94);
        // 801e6a4c: members with a model (+8) start 800bb760's task.
        for (std::uint32_t member = 0; member < 3; ++member) {
            const auto record = placement(member);
            if (memory(record + 6, 1) < 0x11 && memory(record + 8, 1) != 0)
                throw missing("battle_loader_step", 0x800bb760, "symbol:battle-member-models",
                              "Member models (800bb760) are not reconstructed");
        }
        set_memory(node + 8, load_wait);
        return;
    case load_wait:
        if (memory(0x800c35d8) != 0)
            return;
        set_memory(node + 0x90, 0x10);
        set_memory(node + 8, load_settle);
        return;
    case load_settle: {
        if (const auto count = memory(node + 0x90); count != 0) {
            set_memory(node + 0x90, count - 1);
            return;
        }
        for (std::uint32_t member = 0; member < 3; ++member) {
            if (memory(placement(member) + 8, 1) != 0)
                continue;
            const auto sprite = memory(slot_sprites + member * 4);
            if (sprite != 0 && s16(memory(sprite + 6, 2)) != s16(memory(sprite + 0x84, 2)))
                return;
        }
        set_memory(loaded, 1, 1);
        // 8001ce44: unlink the task and release its node.
        unlink_task(node);
        release_battle_heap(node, 0x8001ce54);
        return;
    }
    default:
        throw missing("battle_loader_step", state, "symbol:battle-loader-state",
                      "This loading task state is not reconstructed");
    }
}

// 8002dde4(data, 0, 0, 0, 0, 0, 0): each image section (1100 palette, 1101
// pixels) at its own position, the rectangle at `rect`.
void Program::upload_image_sections(FrameServices &services, std::uint32_t data,
                                    std::uint32_t rect) {
    const auto count = memory(data);
    auto at = data + (count + 1) * 4;
    for (std::uint32_t section = 0;
         static_cast<std::int32_t>(section) < static_cast<std::int32_t>(count); ++section) {
        const auto kind = memory(at);
        if (kind != 0x1100 && kind != 0x1101)
            return;
        std::array<std::int16_t, 4> area{
            static_cast<std::int16_t>(s16(memory(at + 4, 2)) + s16(memory(at + 8, 2))),
            static_cast<std::int16_t>(s16(memory(at + 6, 2)) + s16(memory(at + 10, 2))),
            static_cast<std::int16_t>(memory(at + 12, 2)),
            static_cast<std::int16_t>(memory(at + 14, 2))};
        static_cast<void>(load_image(area, rect, at + 16, &services));
        at += 16 + u32(static_cast<std::int32_t>(area[2]) * area[3] * 2);
    }
}

} // namespace xem::reconstruction
