#include "field_memory.hpp"

#include "xem/reconstruction/original_layout.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace xem::analysis {
namespace field = reconstruction::field;
using reconstruction::FieldState;
using reconstruction::Program;
namespace {
constexpr std::uint32_t event_actor_count = 0x800adbfc;
constexpr std::uint32_t descriptor_table = 0x800afb10;
constexpr std::uint32_t variable_bank = 0x800c3a68;
constexpr std::uint32_t event_package = 0x800adbf8;
constexpr std::uint32_t zone_table = 0x800adbf4;
constexpr std::uint32_t message_table = 0x800adbf0;
constexpr std::uint32_t collision_component = 0x800afb18;
constexpr std::uint32_t collision_attributes = 0x800afb20;
constexpr std::uint32_t collision_triangles = 0x800afb24;
constexpr std::uint32_t collision_vertices = 0x800afb34;
constexpr std::uint32_t history_ring = 0x800b14f0;
constexpr std::uint32_t published_index = 0x800afd1c;
constexpr std::uint32_t published_actor = 0x800b0078;
constexpr std::uint32_t published_descriptor = 0x800b06b8;
constexpr std::uint32_t trigonometry_table = 0x800523f0;
constexpr std::uint32_t square_root_table = 0x80056a00;
constexpr std::uint32_t reciprocal_table = 0x80056b94;
constexpr std::uint32_t angle_table = 0x80057030;
constexpr std::uint32_t replay_widths = 0x8004fc40;
constexpr std::uint32_t field_snapshot = 0x8005a4e4;
constexpr std::size_t field_snapshot_bytes = 0x3804;
constexpr std::size_t disc_file_table_bytes = 0x8000;
constexpr std::size_t disc_directory_table_bytes = 0x7a;
constexpr std::uint32_t pad_buffers = 0x800625fc;

// Field frame draw buffers: two 80f4-byte blocks (environments, ordering tables).
constexpr std::uint32_t draw_blocks = 0x800b249c;
constexpr std::size_t draw_block_bytes = 0x80f4;

constexpr std::uint32_t primitive_table = 0x8004fe50;

constexpr std::size_t actor_bytes = 0x138;
constexpr std::size_t descriptor_bytes = 0x5c;
constexpr std::size_t sprite_bytes = 0x164;

template <typename T> T convert(std::uint32_t raw) {
    if constexpr (std::is_signed_v<T>) {
        using U = std::make_unsigned_t<T>;
        return std::bit_cast<T>(static_cast<U>(raw));
    } else {
        return static_cast<T>(raw);
    }
}

template <std::size_t Size>
void copy_into(std::array<std::uint8_t, Size> &target, std::span<const std::uint8_t> source) {
    std::copy(source.begin(), source.end(), target.begin());
}
std::vector<std::uint8_t> copy_of(std::span<const std::uint8_t> bytes) {
    return {bytes.begin(), bytes.end()};
}
std::uint32_t bytes_word(std::span<const std::uint8_t> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8U * i);
    return value;
}
void loaded_copy(const OriginalMemory &memory, std::uint32_t pointer,
                 std::span<const std::uint8_t> component, const char *message) {
    if (!std::ranges::equal(memory.range(memory.word(pointer), component.size()), component))
        throw field::FieldFormatError(message);
}
std::vector<std::int16_t> halfwords(const OriginalMemory &memory, std::uint32_t address,
                                    std::uint32_t count) {
    std::vector<std::int16_t> result;
    for (std::uint32_t i = 0; i < count; ++i)
        result.push_back(convert<std::int16_t>(memory.word(address + i * 2, 2)));
    return result;
}
std::vector<std::uint8_t> halfword_bytes(const std::vector<std::int16_t> &values) {
    std::vector<std::uint8_t> result;
    for (const auto value : values) {
        result.push_back(static_cast<std::uint8_t>(value));
        result.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(value) >> 8U));
    }
    return result;
}
bool game_state_loaded(const reconstruction::ResidentState &resident) {
    const auto offset = (resident.game_state & 0x1fffffffU) + reconstruction::game_data_bytes;
    return (resident.game_state & 0xe0000000U) == 0x80000000U && offset <= ram_bytes;
}
// The collision code reads layer tables through component-relative offsets;
// the original reads loader-published pointers. They must describe one layout.
void check_collision_pointers(const OriginalMemory &memory, const FieldState &state) {
    const auto &component = state.collision_component;
    const auto base = state.collision_address;
    if (memory.word(collision_attributes) != base + bytes_word(component, 0x14))
        throw field::FieldFormatError("Collision attribute pointer differs from the component");
    const auto layers = bytes_word(component, 0);
    for (std::uint32_t layer = 0; layer < layers && layer < 4; ++layer)
        if (memory.word(collision_triangles + layer * 4) !=
                base + bytes_word(component, 0x18 + layer * 8) ||
            memory.word(collision_vertices + layer * 4) !=
                base + bytes_word(component, 0x1c + layer * 8))
            throw field::FieldFormatError("Collision layer pointers differ from the component");
}
} // namespace

std::span<const std::uint8_t> OriginalMemory::range(std::uint32_t address, std::size_t size) const {
    const auto offset = static_cast<std::size_t>(address & 0x1fffffffU);
    if ((address & 0xe0000000U) != 0x80000000U || offset > ram.size() || size > ram.size() - offset)
        throw field::FieldFormatError("Original address is outside the supplied RAM image");
    return std::span(ram).subspan(offset, size);
}
std::span<std::uint8_t> OriginalMemory::range(std::uint32_t address, std::size_t size) {
    const auto view = static_cast<const OriginalMemory &>(*this).range(address, size);
    return std::span(ram).subspan(static_cast<std::size_t>(view.data() - ram.data()), size);
}
std::uint32_t OriginalMemory::word(std::uint32_t address, std::size_t width) const {
    const auto bytes = range(address, width);
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(bytes[i]) << (8U * i);
    return result;
}
void OriginalMemory::put(std::uint32_t address, std::uint32_t value, std::size_t width) {
    auto bytes = range(address, width);
    for (std::size_t i = 0; i < width; ++i)
        bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
}

Program import_resident(const OriginalMemory &memory) {
    if (memory.ram.size() != ram_bytes)
        throw field::FieldFormatError("Original RAM image must contain exactly 2 MiB");
    Program program;
    auto &resident = program.resident;
    for (const auto &item : reconstruction::original_globals())
        if (item.resident)
            item.set(program, memory.word(item.address, item.width));
    resident.math.trigonometry = copy_of(memory.range(trigonometry_table, 0x4000));
    resident.math.square_root = halfwords(memory, square_root_table, 192);
    resident.math.reciprocal = halfwords(memory, reciprocal_table, 192);
    resident.math.angle = halfwords(memory, angle_table, 1025);
    resident.field_snapshot = copy_of(memory.range(field_snapshot, field_snapshot_bytes));
    // Disc file and directory tables, at the sizes 80028230 loads them with,
    // and the DMA service 8004b7a0 calls (*8005892c + 4).
    // Before boot sets these pointers they name no RAM.
    const auto in_ram = [](std::uint32_t address, std::size_t size) {
        return (address & 0xe0000000U) == 0x80000000U &&
               (address & 0x1fffffffU) + size <= ram_bytes;
    };
    auto &read = resident.disc_read;
    if (in_ram(read.file_table, disc_file_table_bytes))
        read.files = copy_of(memory.range(read.file_table, disc_file_table_bytes));
    if (in_ram(read.directory_table, disc_directory_table_bytes))
        read.directories = copy_of(memory.range(read.directory_table, disc_directory_table_bytes));
    if (in_ram(resident.cd.dma_services + 4, 4))
        resident.cd.dma_set_callback = memory.word(resident.cd.dma_services + 4);
    // Read-only input of the VSync callback and the pad status check: the
    // BIOS pad driver's receive buffers.
    for (std::uint32_t port = 0; port < 2; ++port)
        copy_into(resident.pad.buffers[port], memory.range(pad_buffers + port * 34, 34));
    // The game data is allocated during boot; before that there is none.
    if (game_state_loaded(resident))
        resident.game_data =
            copy_of(memory.range(resident.game_state, reconstruction::game_data_bytes));
    // The heap list: every header, and the bytes of free blocks.
    for (auto at = resident.heap.head - 8;;) {
        if (resident.heap.headers.size() > 0x10000)
            throw field::FieldFormatError("Original heap list does not terminate");
        const std::array<std::uint32_t, 2> header{memory.word(at), memory.word(at + 4)};
        if (!resident.heap.headers.emplace(at, header).second)
            throw field::FieldFormatError("Original heap list revisits a block");
        const auto tag = header[1] & reconstruction::resident::heap_tag_mask;
        if (tag == reconstruction::resident::heap_end_tag)
            break;
        if (header[0] <= at + 8)
            throw field::FieldFormatError("Original heap list does not ascend");
        if (tag == 0)
            resident.heap.held.emplace(at + 8, copy_of(memory.range(at + 8, header[0] - at - 16)));
        at = header[0] - 8;
    }
    // The mode block 800199cc cached (800592bc): its allocated heap extent. A
    // pointer that outlived a heap reset names bytes the heap holds.
    if (const auto block = resident.mode_block.address; block != 0) {
        const auto found = resident.heap.headers.find(block - 8);
        const auto tag = found == resident.heap.headers.end()
                             ? 0U
                             : found->second[1] & reconstruction::resident::heap_tag_mask;
        if (tag != 0 && tag != reconstruction::resident::heap_end_tag)
            resident.mode_block.bytes = copy_of(memory.range(block, found->second[0] - block - 8));
    }
    // The map data read ahead (8001b484) owns its block while a slot is
    // selected and the block is allocated (a slot can be selected while the
    // pointer still names a released block).
    if (resident.preload_slot != 0xffffffffU) {
        const auto &block = resident.preload_block.address;
        const auto found = resident.heap.headers.find(block - 8);
        if (found != resident.heap.headers.end() &&
            (found->second[1] & reconstruction::resident::heap_tag_mask) != 0)
            resident.preload_block.bytes =
                copy_of(memory.range(block, found->second[0] - block - 8));
    }
    // Sound driver objects, each owned whole. Objects in the sound pool (the
    // 6300 bytes at 80065b0c the driver initializes with 80038ec0) end at the
    // word 8 bytes before them (their pool header). Objects loaded into the
    // game heap own their allocated heap block: the block starting at the
    // object, or else the block containing it.
    auto &sound = resident.sound;
    const auto owned = [&](std::uint32_t address) {
        auto found = sound.objects.upper_bound(address);
        return found != sound.objects.begin() &&
               address - std::prev(found)->first < std::prev(found)->second.size();
    };
    const auto sound_object = [&](std::uint32_t address) {
        if (address == 0 || owned(address))
            return;
        if (address - 0x80065b0cU < 0x6300U) {
            const auto end = memory.word(address - 8);
            if (end <= address || end - 0x80065b0cU > 0x6300U)
                throw field::FieldFormatError("Sound pool object has no pool extent");
            sound.objects.emplace(address, copy_of(memory.range(address, end - address)));
            return;
        }
        for (const auto &[at, header] : resident.heap.headers) {
            const auto tag = header[1] & reconstruction::resident::heap_tag_mask;
            if (tag == 0 || tag == reconstruction::resident::heap_end_tag || address < at + 8 ||
                address >= header[0] - 8)
                continue;
            sound.objects.emplace(at + 8, copy_of(memory.range(at + 8, header[0] - 16 - at)));
            return;
        }
        throw field::FieldFormatError("Sound driver object is in neither the pool nor the heap");
    };
    const auto sound_list = [&](std::uint32_t head, std::uint32_t next) {
        for (auto at = head, count = 0U; at != 0; at = memory.word(at + next)) {
            if (++count > 0x1000)
                throw field::FieldFormatError("Sound driver list does not terminate");
            sound_object(at);
        }
    };
    sound_object(sound.effect_block);
    sound_list(sound.effect_banks, 0x1c);
    sound_list(sound.wave_banks, 0x2c);
    sound_list(sound.sequences, 0);
    for (const auto &[address, size] : reconstruction::resident::sound_statics)
        sound.statics.emplace(address, copy_of(memory.range(address, size)));
    for (const auto &[address, size] : reconstruction::resident::sound_constants)
        sound.constants.emplace(address, copy_of(memory.range(address, size)));
    // The SPU transfer queue (owned: transfers are queued and started),
    // unless a driver object already holds it.
    if (const auto queue = memory.word(0x80059458);
        in_ram(queue, reconstruction::resident::transfer_queue_bytes) && !owned(queue))
        sound.statics.emplace(
            queue, copy_of(memory.range(queue, reconstruction::resident::transfer_queue_bytes)));
    // Each sequence's event data (+8), a block whose third word is its byte
    // length; the tick only reads it. Every voice's event pointer must lie
    // inside it.
    for (auto sequence = sound.sequences; sequence != 0; sequence = memory.word(sequence)) {
        const auto data = memory.word(sequence + 8);
        if (data == 0)
            continue;
        const auto size = memory.word(data + 8);
        for (std::uint32_t voice = 0; voice < memory.word(sequence + 0x14, 1); ++voice) {
            const auto at = memory.word(sequence + 0x94 + voice * 0x158 + 0x14);
            if (at != 0 && (at < data || at - data >= size))
                throw field::FieldFormatError("A sequence voice reads outside its event data");
        }
        sound.constants.emplace(data, copy_of(memory.range(data, size)));
    }
    copy_into(sound.spu_blocks,
              memory.range(reconstruction::resident::spu_block_table, sound.spu_blocks.size()));
    sound.pitch_tables = copy_of(memory.range(reconstruction::resident::pitch_table_address,
                                              reconstruction::resident::pitch_table_bytes));
    // Child blocks of each sequence (next +4) and the sound-pool header list.
    for (auto sequence = sound.sequences; sequence != 0; sequence = memory.word(sequence))
        sound_list(memory.word(sequence + 4), 4);
    for (auto at = sound.pool; at != 0; at = sound.pool_headers.at(at)[3]) {
        if (sound.pool_headers.size() > 0x1000)
            throw field::FieldFormatError("Sound pool list does not terminate");
        if (!sound.pool_headers
                 .emplace(at, std::array{memory.word(at), memory.word(at + 4), memory.word(at + 8),
                                         memory.word(at + 12)})
                 .second)
            throw field::FieldFormatError("Sound pool list revisits a block");
    }
    return program;
}

Program import_field(const OriginalMemory &memory, std::span<const std::uint8_t> field_source,
                     std::span<const std::uint8_t> overlay,
                     std::span<const ResourceExtent> resources) {
    auto program = import_resident(memory);
    program.field = std::make_unique<FieldState>();
    auto &state = *program.field;
    auto &resident = program.resident;
    for (const auto &item : reconstruction::original_globals())
        if (!item.resident)
            item.set(program, memory.word(item.address, item.width));

    // The decoded overlay is the source of its constant tables; a byte is
    // readable only where the loaded copy equals it.
    if (overlay.empty())
        throw field::FieldFormatError("Field entries require the decoded field overlay");
    const auto loaded = memory.range(reconstruction::field_overlay_base, overlay.size());
    state.overlay = copy_of(overlay);
    state.overlay_verified.resize(overlay.size());
    for (std::size_t i = 0; i < overlay.size(); ++i)
        state.overlay_verified[i] = loaded[i] == overlay[i];

    const auto components = field::decode_field_components(field_source);
    const auto event_component = components[5].logical_data();
    loaded_copy(memory, event_package, event_component,
                "Loaded event package differs from the qualified field source");
    // Component 7 is the message table read through 800adbf0.
    const auto messages = components[7].logical_data();
    loaded_copy(memory, message_table, messages, "Loaded messages differ from the field source");
    state.messages = copy_of(messages);
    const auto zones = components[8].logical_data();
    loaded_copy(memory, zone_table, zones, "Loaded trigger zones differ from the field source");
    state.event_package = field::parse_event_package(event_component);
    state.zones = copy_of(zones);
    // The loaded collision component is live state (its attribute table can
    // change); the source decode only fixes its extent.
    state.collision_address = memory.word(collision_component);
    state.collision_component =
        copy_of(memory.range(state.collision_address, components[1].logical_size));
    state.collision = field::parse_collision_package(state.collision_component);
    check_collision_pointers(memory, state);
    state.replay_widths = copy_of(memory.range(replay_widths, 256));
    copy_into(state.history_ring, memory.range(history_ring, state.history_ring.size()));

    const auto count = memory.word(event_actor_count);
    if (count > 255 || count != state.event_package.entries.size())
        throw field::FieldFormatError("Original actor count differs from the event component");
    const auto table = memory.word(descriptor_table);
    state.actors.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto &actor = state.actors[i];
        actor.descriptor_address = table + i * static_cast<std::uint32_t>(descriptor_bytes);
        copy_into(actor.descriptor, memory.range(actor.descriptor_address, descriptor_bytes));
        actor.address = memory.word(actor.descriptor_address + 0x4c);
        copy_into(actor.storage, memory.range(actor.address, actor_bytes));
        if (const auto sprite = memory.word(actor.descriptor_address + 4); sprite != 0)
            actor.sprite.sprite = {sprite, copy_of(memory.range(sprite, sprite_bytes))};
        if (const auto instance = memory.word(actor.descriptor_address); instance != 0)
            actor.model = memory.word(instance + 4);
        if ((bytes_word(actor.storage, 0x134) & 0x80U) != 0)
            copy_into(actor.extension_110.emplace(),
                      memory.range(bytes_word(actor.storage, 0x110), 12));
        if ((bytes_word(actor.storage, 0x12c) & 0x1000U) != 0)
            copy_into(actor.extension_114.emplace(),
                      memory.range(bytes_word(actor.storage, 0x114), 16));
    }
    resident.variables.unsigned_bitmap = state.event_package.variable_unsigned_bits;
    for (std::uint32_t i = 0; i < resident.variables.words.size(); ++i)
        resident.variables.words[i] =
            static_cast<std::uint16_t>(memory.word(variable_bank + i * 2, 2));
    for (const auto &[address, size] : resources)
        state.resources.push_back({address, copy_of(memory.range(address, size))});
    for (std::uint32_t i = 0; i < 2; ++i) {
        const auto address = draw_blocks + i * static_cast<std::uint32_t>(draw_block_bytes);
        state.regions.add("draw_block", address, copy_of(memory.range(address, draw_block_bytes)));
    }
    // Compass quads (19 records of four vectors and a packet per buffer) and
    // the draw-mode packets (twelve bytes each, 16 per buffer).
    state.regions.add("compass", 0x800b06bc, copy_of(memory.range(0x800b06bc, 0x19 * 0x70)));
    state.regions.add("draw_modes", 0x800b1df4, copy_of(memory.range(0x800b1df4, 0x180)));
    // The sprite system's per-buffer arenas (packets and upload nodes).
    for (const auto arena : resident.sprite_arenas)
        if (arena != 0)
            state.regions.add("sprite_arena", arena,
                              copy_of(memory.range(arena, resident.sprite_arena_bytes)));
    // Descriptors after the event actors' (map pieces), and every model
    // instance: its 24-byte record and a packet buffer per draw buffer, sized
    // from its primitive groups by the resident primitive table (8004fe50).
    for (std::uint32_t i = count; i < state.descriptor_count; ++i) {
        auto &piece = state.pieces.emplace_back();
        piece.address = table + i * static_cast<std::uint32_t>(descriptor_bytes);
        copy_into(piece.descriptor, memory.range(piece.address, descriptor_bytes));
    }
    // A model's packet buffer size: its primitive groups by the resident
    // primitive table (8004fe50).
    const auto packet_bytes = [&](std::uint32_t model) {
        auto record = memory.word(model + 0x10);
        std::size_t size = 0;
        for (auto groups = memory.word(model + 6, 2); groups != 0; --groups) {
            const auto type = memory.word(record, 1);
            const auto primitives = convert<std::int16_t>(memory.word(record + 2, 2));
            if (type >= 17 || primitives < 0)
                throw field::FieldFormatError("Model primitive group outside the resident table");
            const auto entry = primitive_table + type * 0x28;
            size += static_cast<std::size_t>(primitives) * memory.word(entry + 0x24);
            record += 4 + static_cast<std::uint32_t>(primitives) * memory.word(entry + 0x1c);
        }
        return size;
    };
    std::set<std::uint32_t> instances;
    for (std::uint32_t i = 0; i < state.descriptor_count; ++i) {
        const auto instance = memory.word(table + i * static_cast<std::uint32_t>(descriptor_bytes));
        if (instance == 0 || !instances.insert(instance).second)
            continue;
        state.regions.add("model_instance", instance, copy_of(memory.range(instance, 0x24)));
        const auto size = packet_bytes(memory.word(instance + 4));
        for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
            const auto packets = memory.word(instance + 8 + buffer * 4);
            state.regions.add("model_packets", packets, copy_of(memory.range(packets, size)));
        }
    }
    // Sprite task nodes, each with the heap block it shares with its sprite,
    // and the packet buffers of task sprites that draw a model (sprite +20:
    // renderer, +2c/+30 packets, +34 model).
    auto &tasks = resident.sprite_tasks;
    for (auto head : {tasks.head, tasks.pending_head})
        for (auto node = head, visited = 0U; node != 0; node = memory.word(node + 0x18)) {
            if (++visited > 0x1000)
                throw field::FieldFormatError("Sprite task list does not terminate");
            const auto owned = std::ranges::any_of(tasks.nodes, [&](const auto &block) {
                return node >= block.address && node - block.address < block.bytes.size();
            });
            if (owned)
                continue;
            bool found = false;
            for (const auto &[at, header] : resident.heap.headers) {
                if (node < at + 8 || node >= header[0] - 8)
                    continue;
                tasks.nodes.push_back({at + 8, copy_of(memory.range(at + 8, header[0] - 16 - at))});
                found = true;
                break;
            }
            if (!found)
                throw field::FieldFormatError("Sprite task node is outside the heap");
            const auto sprite = memory.word(node + 4);
            const auto renderer = memory.word(sprite + 0x20);
            if (const auto model = memory.word(renderer + 0x34); model != 0) {
                const auto size = packet_bytes(model);
                for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
                    const auto packets = memory.word(renderer + 0x2c + buffer * 4);
                    state.regions.add("task_model_packets", packets,
                                      copy_of(memory.range(packets, size)));
                }
            }
        }
    // Each actor's ground shadow (descriptor +8): four corners and a packet
    // per draw buffer (800764b4).
    for (const auto &actor : state.actors)
        if (const auto shadow = memory.word(actor.descriptor_address + 8); shadow != 0)
            state.regions.add("shadow", shadow, copy_of(memory.range(shadow, 0x20 + 2 * 0x28)));
    // Each sprite's parts (renderer +30), the heap block its frames are
    // built into (8001dae8).
    std::vector<std::uint32_t> sprites;
    for (const auto &actor : state.actors)
        if (actor.sprite.sprite.address != 0)
            sprites.push_back(actor.sprite.sprite.address);
    for (auto head : {tasks.head, tasks.pending_head})
        for (auto node = head; node != 0; node = memory.word(node + 0x18))
            sprites.push_back(memory.word(node + 4));
    // The heap block holding `address`, unless already owned.
    const auto own_block = [&](const char *name, std::uint32_t address) {
        if (address == 0 || state.regions.contains(address, 1))
            return;
        const auto owned = std::ranges::any_of(tasks.nodes, [&](const auto &block) {
            return address >= block.address && address - block.address < block.bytes.size();
        });
        if (owned)
            return;
        const auto block = std::ranges::find_if(resident.heap.headers, [&](const auto &entry) {
            return address >= entry.first + 8 && address < entry.second[0] - 8;
        });
        if (block == resident.heap.headers.end())
            throw field::FieldFormatError("Owned heap record is outside the heap");
        const auto at = block->first + 8;
        state.regions.add(name, at, copy_of(memory.range(at, block->second[0] - 8 - at)));
    };
    for (const auto sprite : sprites)
        own_block("sprite_parts", memory.word(memory.word(sprite + 0x20) + 0x30));
    // The first window's packet list per buffer precedes the windows
    // (800c2698, 8008004c); later windows' lists end the window before.
    state.regions.add("dialogue_lists", field::DialogueWindow::base - 0x18,
                      copy_of(memory.range(field::DialogueWindow::base - 0x18, 0x18)));
    // The text line records (+28) of each dialogue window in use (80034888).
    for (std::uint32_t w = 0; w < state.dialogue.size(); ++w)
        if (state.dialogue[w].half(field::DialogueWindow::busy) == 0)
            own_block("dialogue_lines", memory.word(field::DialogueWindow::base +
                                                    w * field::DialogueWindow::stride + 0x28));
    return program;
}

namespace {
// Owned ranges written back over the entry image; overlaps are rejected.
struct Claims {
    OriginalMemory &memory;
    std::vector<OwnedRange> owned;
    std::map<std::uint32_t, std::size_t> claimed;
    void claim(std::string_view name, std::uint32_t address, std::size_t size) {
        const auto next = claimed.lower_bound(address);
        const bool overlaps =
            (next != claimed.end() && next->first < address + size) ||
            (next != claimed.begin() && std::prev(next)->first + std::prev(next)->second > address);
        if (overlaps) {
            const auto other = std::ranges::find_if(owned, [&](const OwnedRange &range) {
                return range.address < address + size && address < range.address + range.size;
            });
            std::ostringstream message;
            message << "Owned original ranges overlap at " << name << ' ' << std::hex << address
                    << " (" << other->name << ' ' << other->address << ')';
            throw field::FieldFormatError(message.str());
        }
        claimed.emplace(address, size);
        owned.push_back({name, address, size});
    }
    void bytes(std::string_view name, std::uint32_t address, std::span<const std::uint8_t> value) {
        std::ranges::copy(value, memory.range(address, value.size()).begin());
        claim(name, address, value.size());
    }
    void global(const reconstruction::OriginalGlobal &item, const Program &program) {
        memory.put(item.address, item.get(program), item.width);
        claim(item.name, item.address, item.width);
    }
};
void export_resident_into(const Program &program, Claims &out) {
    const auto &resident = program.resident;
    for (const auto &item : reconstruction::original_globals())
        if (item.resident)
            out.global(item, program);
    out.bytes("trigonometry", trigonometry_table, resident.math.trigonometry);
    out.bytes("square_root", square_root_table, halfword_bytes(resident.math.square_root));
    out.bytes("reciprocal", reciprocal_table, halfword_bytes(resident.math.reciprocal));
    out.bytes("angle", angle_table, halfword_bytes(resident.math.angle));
    if (resident.field_snapshot.size() != field_snapshot_bytes)
        throw field::FieldFormatError("Resident snapshot storage has the wrong extent");
    out.bytes("field_snapshot", field_snapshot, resident.field_snapshot);
    const auto &read = resident.disc_read;
    if (!read.files.empty())
        out.bytes("disc_file_records", read.file_table, read.files);
    if (!read.directories.empty())
        out.bytes("disc_directories", read.directory_table, read.directories);
    if (!read.ring.bytes.empty())
        out.bytes("disc_ring_header", read.ring.address, read.ring.bytes);
    if (!read.list.bytes.empty())
        out.bytes("disc_file_list", read.list.address, read.list.bytes);
    if (!read.ring_payload.bytes.empty())
        out.bytes("disc_ring_payload", read.ring_payload.address, read.ring_payload.bytes);
    for (const auto &block : resident.music_blocks)
        out.bytes("music_block", block.address, block.bytes);
    if (!resident.mode_block.bytes.empty())
        out.bytes("mode_block", resident.mode_block.address, resident.mode_block.bytes);
    if (!resident.preload_block.bytes.empty())
        out.bytes("preload_block", resident.preload_block.address, resident.preload_block.bytes);
    if (resident.cd.dma_set_callback) {
        out.memory.put(resident.cd.dma_services + 4, *resident.cd.dma_set_callback);
        out.claim("dma_set_callback", resident.cd.dma_services + 4, 4);
    }
    for (const auto &[address, bytes] : resident.sound.objects)
        out.bytes("sound_object", address, bytes);
    for (const auto &[address, bytes] : resident.sound.statics)
        out.bytes("sound_static", address, bytes);
    out.bytes("sound_spu_blocks", reconstruction::resident::spu_block_table,
              resident.sound.spu_blocks);
    if (!resident.sound.pitch_tables.empty())
        out.bytes("sound_pitch_tables", reconstruction::resident::pitch_table_address,
                  resident.sound.pitch_tables);
    for (const auto &[address, header] : resident.sound.pool_headers) {
        for (std::uint32_t i = 0; i < 4; ++i)
            out.memory.put(address + 4 * i, header[i]);
        out.claim("sound_pool_header", address, 16);
    }
    if (game_state_loaded(resident))
        out.bytes("game_data", resident.game_state, resident.game_data);
    for (const auto &[address, header] : resident.heap.headers) {
        out.memory.put(address, header[0]);
        out.memory.put(address + 4, header[1]);
        out.claim("heap_header", address, 8);
    }
    for (const auto &[address, held] : resident.heap.held)
        out.bytes("heap_held", address, held);
    for (const auto &block : resident.disc_transfers)
        out.bytes("disc_transfer", block.address, block.bytes);
}
} // namespace

void load_platform(Program &program, const char *platform, const char *disc) {
    auto &resident = program.resident;
    std::ifstream lines(platform);
    if (!lines)
        throw field::FieldFormatError("Cannot open the platform input file");
    const auto number = [](const std::string &text, int base) {
        std::size_t used = 0;
        const auto value = std::stoul(text, &used, base);
        if (used != text.size() || value > 0xffffffffUL)
            throw field::FieldFormatError("Malformed platform input number");
        return static_cast<std::uint32_t>(value);
    };
    for (std::string kind; lines >> kind;) {
        if (kind == "drive") {
            std::string lba;
            if (!(lines >> lba) || resident.drive.next)
                throw field::FieldFormatError("Malformed or repeated drive position");
            resident.drive.next = number(lba, 10);
        } else if (kind == "read") {
            std::string site, value;
            if (!(lines >> site >> value))
                throw field::FieldFormatError("Malformed platform read");
            resident.platform.push_back(
                {reconstruction::PlatformInput::Kind::read, number(site, 16), number(value, 16)});
        } else if (kind == "interrupt") {
            resident.platform.push_back({reconstruction::PlatformInput::Kind::interrupt, 0, 0});
        } else {
            throw field::FieldFormatError("Unknown platform input " + kind);
        }
    }
    if (!lines.eof())
        throw field::FieldFormatError("Malformed platform input file");
    // The drive received the Setmode libcd last recorded sending.
    resident.drive.mode = resident.cd.mode;
    if (disc != nullptr && *disc != 0) {
        const std::string path = disc;
        resident.drive.read_sector = [path](std::uint32_t lba) {
            std::ifstream stream(path, std::ios::binary);
            reconstruction::RawSector sector{};
            stream.seekg(static_cast<std::streamoff>(lba) * reconstruction::raw_sector_bytes);
            if (!stream.read(reinterpret_cast<char *>(sector.data()), sector.size()))
                throw reconstruction::PlatformInputError("Disc image has no sector " +
                                                         std::to_string(lba));
            return sector;
        };
    }
}

void attach_interrupt_memory(Program &program, const OriginalMemory &memory) {
    auto &resident = program.resident;
    auto &read = resident.disc_read;
    const auto &cd = resident.cd;
    const auto ring = resident.disc_stream.ring_buffer;
    const bool ring_active = cd.sync_callback == 0x8002b2f0 || cd.sync_callback == 0x8002b5d0 ||
                             cd.dma_callback == 0x8002ba58 || cd.dma_callback == 0x8002bb50;
    if (ring_active && ring != 0) {
        const auto count = memory.word(ring);
        if (count > 0x1000)
            throw field::FieldFormatError("Active disc ring has an implausible block count");
        const auto header_bytes = 0x24U + count * 8U;
        read.ring = {ring, copy_of(memory.range(ring, header_bytes))};
        read.ring_payload = {
            ring + header_bytes,
            copy_of(memory.range(ring + header_bytes, std::size_t{count} * 0x800))};
    }
    if (cd.sync_callback == 0x8002ac24 && read.w_fe0c != 0) {
        // Entries up to and including the terminating one (file or
        // destination zero).
        std::uint32_t entries = 0;
        for (auto at = read.w_fe0c;; at += 8) {
            if (++entries > 0x1000)
                throw field::FieldFormatError("Disc read list does not terminate");
            if (memory.word(at, 2) == 0 || memory.word(at + 4) == 0)
                break;
        }
        read.list = {read.w_fe0c, copy_of(memory.range(read.w_fe0c, entries * 8))};
    }
}

std::vector<OwnedRange> export_resident(const Program &program, OriginalMemory &memory) {
    Claims out{memory, {}, {}};
    export_resident_into(program, out);
    return std::move(out.owned);
}

Program import_battle(const OriginalMemory &memory) {
    auto program = import_resident(memory);
    auto &battle = program.battle.emplace();
    const auto overlay =
        memory.range(reconstruction::battle::overlay_base,
                     reconstruction::battle::overlay_end - reconstruction::battle::overlay_base);
    battle.regions.emplace(reconstruction::battle::overlay_base, copy_of(overlay));
    // Allocated heap blocks the battle reaches, each owned up to the next
    // header: those containing `address`, when one does.
    const auto &headers = program.resident.heap.headers;
    const auto own_block = [&](std::uint32_t address) {
        for (const auto &[at, header] : headers) {
            const auto tag = header[1] & reconstruction::resident::heap_tag_mask;
            if (tag == 0 || tag == reconstruction::resident::heap_end_tag || address < at + 8 ||
                address >= header[0] - 8)
                continue;
            // A block holding a sound-driver object belongs to the sound driver.
            if (program.resident.sound.objects.contains(at + 8))
                return true;
            if (!battle.regions.contains(at + 8))
                battle.regions.emplace(at + 8, copy_of(memory.range(at + 8, header[0] - 16 - at)));
            return true;
        }
        return false;
    };
    // Battle setup (80070f40) allocates UI and graphics state, UI state and
    // turn/menu state.
    for (const auto pointer : {0x800c3ea4U, 0x800d2d28U, 0x800c3eacU})
        if (!own_block(memory.word(pointer)))
            throw field::FieldFormatError("Battle state pointer does not name a heap block");
    // The enemy data file holding the enemy AI scripts (pointer 800c3dd0, set
    // by aux4 801e4958).
    static_cast<void>(own_block(memory.word(0x800c3dd0)));
    // The battle scene's formation data (pointer 800d3364, copied from
    // resident 8005949c): positions and the slot-relation table at +140.
    static_cast<void>(own_block(memory.word(0x800d3364)));
    // After combat: the growth table file (pointer 800d2c08) and the
    // post-battle module loaded at 801de000.
    static_cast<void>(own_block(memory.word(0x800d2c08)));
    static_cast<void>(own_block(reconstruction::battle::result_module_base));
    // The attack pages: the direction arrows (800c3e24), the combo sprites
    // (800d2db4), the glyph sprite table (800d2f5c), 800861d0's three text
    // blocks (800c3a70) and the blocks 8007fce8 and 8007fdec release.
    for (const auto pointer : {0x800c3e24U, 0x800d2db4U, 0x800d2f5cU, 0x800c3a70U, 0x800c3a74U,
                               0x800c3a78U, 0x800d367cU, 0x800c3de8U})
        static_cast<void>(own_block(memory.word(pointer)));
    // The result screens' window blocks (8008f8f4 allocates, 8008fa60 releases).
    for (std::uint32_t window = 0; window < 8; ++window)
        for (const auto table : {0x800d2e38U, 0x800d2d90U})
            static_cast<void>(own_block(memory.word(table + window * 4)));
    return program;
}

std::vector<OwnedRange> export_battle(const Program &program, OriginalMemory &memory) {
    if (!program.battle)
        throw field::FieldFormatError("Export requires battle memory");
    Claims out{memory, {}, {}};
    export_resident_into(program, out);
    for (const auto &[address, bytes] : program.battle->regions)
        out.bytes("battle_memory", address, bytes);
    return std::move(out.owned);
}

void import_menu_block(Program &program, const OriginalMemory &memory, std::uint32_t address) {
    // The allocated heap block containing `address`, owned whole; a block
    // holding a sound-driver object stays the driver's.
    auto &regions = program.menu.value().regions;
    for (const auto &[at, header] : program.resident.heap.headers) {
        const auto tag = header[1] & reconstruction::resident::heap_tag_mask;
        if (tag == 0 || tag == reconstruction::resident::heap_end_tag || address < at + 8 ||
            address >= header[0] - 8)
            continue;
        if (program.resident.sound.objects.contains(at + 8))
            throw field::FieldFormatError("A menu pointer names a sound-driver object");
        if (!regions.contains(at + 8))
            regions.emplace(at + 8, copy_of(memory.range(at + 8, header[0] - 16 - at)));
        return;
    }
    throw field::FieldFormatError("A menu pointer does not name a heap block");
}

Program import_menu(const OriginalMemory &memory) {
    namespace menu = reconstruction::menu;
    auto program = import_resident(memory);
    auto &regions = program.menu.emplace().regions;
    const auto own_block = [&](std::uint32_t address) {
        import_menu_block(program, memory, address);
    };
    // Resident words of the save and load, and the name codec's blocks.
    for (const auto [address, size] :
         {std::pair{menu::saved_globals, 0x20U}, std::pair{menu::text_state, 4U},
          std::pair{menu::text_single_limit, 4U}})
        regions.emplace(address, copy_of(memory.range(address, size)));
    const auto text = memory.word(menu::text_state);
    own_block(text);
    own_block(memory.word(text + 0x6c));
    own_block(menu::overlay_base);
    regions.emplace(menu::state_pointer, copy_of(memory.range(menu::state_pointer, 4)));
    const auto state = memory.word(menu::state_pointer);
    own_block(state);
    own_block(memory.word(state + menu::state_party));
    const auto tables = memory.word(state + menu::state_tables);
    own_block(tables);
    // The directory's table pointers (weapons +0 .. consumables +1c), when
    // loaded, and the equipment screen state while that screen is open.
    for (std::uint32_t offset = 0; offset <= menu::item_table; offset += 4)
        if (const auto table = memory.word(tables + offset); table != 0)
            own_block(table);
    if (const auto screen = memory.word(state + menu::state_equip_screen); screen != 0)
        own_block(screen);
    if (const auto card = memory.word(state + menu::state_card); card != 0)
        own_block(card);
    return program;
}

std::vector<OwnedRange> export_menu(const Program &program, OriginalMemory &memory) {
    if (!program.menu)
        throw field::FieldFormatError("Export requires menu memory");
    Claims out{memory, {}, {}};
    export_resident_into(program, out);
    for (const auto &[address, bytes] : program.menu->regions)
        out.bytes("menu_memory", address, bytes);
    return std::move(out.owned);
}

std::vector<OwnedRange> export_field(const Program &program, OriginalMemory &memory) {
    if (!program.field)
        throw field::FieldFormatError("Export requires field state");
    const auto &state = *program.field;
    const auto &resident = program.resident;
    Claims out{memory, {}, {}};
    export_resident_into(program, out);
    for (const auto &item : reconstruction::original_globals())
        if (!item.resident)
            out.global(item, program);
    for (const auto &actor : state.actors) {
        out.bytes("actor", actor.address, actor.storage);
        out.bytes("descriptor", actor.descriptor_address, actor.descriptor);
        if (!actor.sprite.sprite.bytes.empty())
            out.bytes("sprite", actor.sprite.sprite.address, actor.sprite.sprite.bytes);
        if (actor.extension_110)
            out.bytes("extension_110", bytes_word(actor.storage, 0x110), *actor.extension_110);
        if (actor.extension_114)
            out.bytes("extension_114", bytes_word(actor.storage, 0x114), *actor.extension_114);
    }
    std::vector<std::uint8_t> variables;
    for (const auto word : resident.variables.words) {
        variables.push_back(static_cast<std::uint8_t>(word));
        variables.push_back(static_cast<std::uint8_t>(word >> 8U));
    }
    out.bytes("variables", variable_bank, variables);
    out.bytes("history_ring", history_ring, state.history_ring);
    out.bytes("collision_component", state.collision_address, state.collision_component);
    out.bytes("replay_widths", replay_widths, state.replay_widths);
    out.bytes("messages", state.messages_address, state.messages);
    for (const auto &blocks : state.dialogue_blocks)
        for (const auto &block : blocks)
            out.bytes("dialogue_block", block.address, block.bytes);
    for (const auto &resource : state.resources)
        out.bytes("resource", resource.address, resource.bytes);
    for (const auto &[address, region] : state.regions.regions())
        out.bytes(region.name, address, region.bytes);
    for (const auto &piece : state.pieces)
        out.bytes("descriptor", piece.address, piece.descriptor);
    for (const auto &node : resident.sprite_tasks.nodes)
        out.bytes("sprite_task_block", node.address, node.bytes);
    if (state.published_actor) {
        const auto &actor = state.actors.at(*state.published_actor);
        memory.put(published_index, static_cast<std::uint32_t>(*state.published_actor));
        out.claim("published_index", published_index, 4);
        memory.put(published_actor, actor.address);
        out.claim("published_actor", published_actor, 4);
        memory.put(published_descriptor, actor.descriptor_address);
        out.claim("published_descriptor", published_descriptor, 4);
    }
    return std::move(out.owned);
}

} // namespace xem::analysis
