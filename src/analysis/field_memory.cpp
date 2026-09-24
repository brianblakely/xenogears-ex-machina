#include "field_memory.hpp"

#include "xem/reconstruction/original_layout.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <functional>
#include <map>
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
    // Read-only inputs of the VSync callback: the BIOS pad driver's receive
    // buffers and the first word of the executable's text.
    for (std::uint32_t port = 0; port < 2; ++port)
        copy_into(resident.pad.buffers[port], memory.range(pad_buffers + port * 34, 34));
    resident.pad.text_word = memory.word(0x80010000);
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
    // The SPU transfer queue the transfer callback reads.
    if (const auto queue = memory.word(0x80059458);
        in_ram(queue, reconstruction::resident::transfer_queue_bytes))
        sound.constants.emplace(
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
        if (overlaps)
            throw field::FieldFormatError("Owned original ranges overlap at " + std::string(name));
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
    if (!read.ring_payload.bytes.empty())
        out.bytes("disc_ring_payload", read.ring_payload.address, read.ring_payload.bytes);
    for (const auto &block : resident.music_blocks)
        out.bytes("music_block", block.address, block.bytes);
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
    // Image data of LoadImage requests waiting in the libgpu queue.
    auto &gpu = resident.gpu;
    for (auto at = gpu.tail; at != gpu.head; at = (at + 1U) & 63U) {
        const auto entry = at * 0x60U;
        const auto word = [&](std::uint32_t offset) {
            std::uint32_t value = 0;
            for (std::uint32_t i = 0; i < 4; ++i)
                value |= static_cast<std::uint32_t>(gpu.queue.at(entry + offset + i)) << (8U * i);
            return value;
        };
        if (word(0) != 0x800460a0 || word(4) != 0x8006be40U + entry)
            continue;
        const auto size = word(0x10);
        const auto pixels = std::size_t{size & 0xffffU} * (size >> 16U);
        if (pixels > 1024 * 512)
            throw field::FieldFormatError("Queued image request exceeds VRAM");
        gpu.sources.push_back({word(8), copy_of(memory.range(word(8), (pixels + 1) / 2 * 4))});
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
    // After combat: the growth table file (pointer 800d2c08) and the
    // post-battle module loaded at 801de000.
    static_cast<void>(own_block(memory.word(0x800d2c08)));
    static_cast<void>(own_block(reconstruction::battle::result_module_base));
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
