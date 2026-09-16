#include "xem/reconstruction/packed_field.hpp"

#include <algorithm>
#include <bit>
#include <string>
#include <string_view>
#include <utility>

namespace xem::reconstruction::field {
namespace {
using View = std::span<const std::uint8_t>;

View region(View data, std::size_t offset, std::size_t size, std::string_view label) {
    if (offset > data.size() || size > data.size() - offset)
        throw FieldFormatError(std::string(label) + ": range exceeds supplied component");
    return data.subspan(offset, size);
}

std::uint16_t word(View data, std::size_t offset) {
    const auto bytes = region(data, offset, 2, "u16le");
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[0]) |
                                      (static_cast<std::uint16_t>(bytes[1]) << 8U));
}

std::uint32_t dword(View data, std::size_t offset) {
    const auto bytes = region(data, offset, 4, "u32le");
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::int16_t signed_word(View data, std::size_t offset) {
    return std::bit_cast<std::int16_t>(word(data, offset));
}
} // namespace

PackedBlock decode_packed_block(View source_memory, std::size_t output_limit) {
    if (source_memory.size() < 4)
        throw PackedError("Missing u32le output length at source +0");
    const std::size_t size = dword(source_memory, 0);
    if (size > output_limit)
        throw PackedError("Declared output exceeds supplied bound");
    PackedBlock result;
    result.data.reserve(size);
    std::size_t position = 4;
    const auto read_byte = [&]() {
        if (position >= source_memory.size())
            throw PackedError("Source truncated at byte " + std::to_string(position));
        return source_memory[position++];
    };
    for (;;) {
        const auto flag_position = position;
        const auto flags = read_byte();
        if (result.data.size() == size) {
            result.token_bytes = flag_position;
            result.source_bytes_read = position;
            return result;
        }
        ++result.groups;
        for (unsigned token = 0; token < 8; ++token) {
            const auto token_position = position;
            const auto low = read_byte();
            if ((flags & (1U << token)) != 0) {
                const auto high = read_byte();
                const std::size_t distance =
                    static_cast<std::size_t>(low) | (static_cast<std::size_t>(high & 0x0fU) << 8U);
                const std::size_t length = (high >> 4U) + 3U;
                if (distance == 0 || distance > result.data.size())
                    throw PackedError("Invalid backward distance at byte " +
                                      std::to_string(token_position));
                if (length > size - result.data.size())
                    throw PackedError("Copy crosses output length at byte " +
                                      std::to_string(token_position));
                for (std::size_t i = 0; i < length; ++i)
                    result.data.push_back(result.data[result.data.size() - distance]);
            } else {
                if (result.data.size() >= size)
                    throw PackedError("Flag group crosses output length at byte " +
                                      std::to_string(token_position));
                result.data.push_back(low);
            }
        }
    }
}

std::span<const std::uint8_t> FieldComponent::logical_data() const {
    return region(decoded.data, 0, logical_size, "logical component");
}

std::array<FieldComponent, 9> decode_field_components(View source_memory) {
    (void)region(source_memory, 0, 0x154, "field header");
    std::array<FieldComponent, 9> result;
    for (std::uint32_t index = 0; index < result.size(); ++index) {
        auto &part = result[index];
        part.index = index;
        part.logical_size = dword(source_memory, 0x10c + 4U * index);
        part.source_offset = dword(source_memory, 0x130 + 4U * index);
        if (part.source_offset < 0x154 || part.source_offset >= source_memory.size())
            throw FieldFormatError("Component " + std::to_string(index) +
                                   ": invalid source offset");
        // Promote before addition: malformed u32 sizes must not wrap to a small allocation.
        const auto limit =
            std::min<std::uint64_t>(static_cast<std::uint64_t>(part.logical_size) + 16U, 0x200000);
        part.decoded = decode_packed_block(source_memory.subspan(part.source_offset),
                                           static_cast<std::size_t>(limit));
        if (part.decoded.data.size() < part.logical_size)
            throw FieldFormatError("Component " + std::to_string(index) +
                                   ": output is shorter than logical size");
    }
    return result;
}

EventProgram EventPackage::program() const & noexcept { return {bytecode, entries}; }

EventPackage parse_event_package(View logical_data) {
    const auto bits = region(logical_data, 0, 0x80, "variable type map");
    const std::size_t count = dword(logical_data, 0x80);
    if (count > (logical_data.size() - 0x84) / 64)
        throw FieldFormatError("Actor event table exceeds supplied component");
    const auto table = region(logical_data, 0x84, count * 64, "actor event table");
    const auto code = logical_data.subspan(0x84 + table.size());
    if (code.size() > 0x10000)
        throw FieldFormatError("Bytecode exceeds the original u16 PC address space");
    EventPackage result;
    std::copy(bits.begin(), bits.end(), result.variable_unsigned_bits.begin());
    result.bytecode_offset = 0x84 + table.size();
    result.bytecode.assign(code.begin(), code.end());
    result.entries.resize(count);
    for (std::size_t actor = 0; actor < count; ++actor) {
        for (std::size_t event = 0; event < 32; ++event) {
            const auto pc = word(table, actor * 64 + event * 2);
            if (pc >= code.size())
                throw FieldFormatError("Actor " + std::to_string(actor) + " event " +
                                       std::to_string(event) + ": PC outside bytecode");
            result.entries[actor][event] = pc;
        }
    }
    return result;
}

CollisionPackage parse_collision_package(View logical_data) {
    const std::size_t count = dword(logical_data, 0);
    if (count < 1 || count > 4)
        throw FieldFormatError("Unsupported collision layer count");
    const auto header_size = 0x18 + count * 8;
    (void)region(logical_data, 0, header_size, "collision header");
    std::array<std::uint32_t, 4> sizes{};
    for (std::size_t i = 0; i < sizes.size(); ++i)
        sizes[i] = dword(logical_data, 4 + i * 4);
    const auto attribute_offset = dword(logical_data, 0x14);
    if (attribute_offset > logical_data.size())
        throw FieldFormatError("Collision attributes exceed supplied component");
    CollisionPackage result;
    result.layers.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t triangle_offset = dword(logical_data, 0x18 + index * 8);
        const std::size_t vertex_offset = dword(logical_data, 0x1c + index * 8);
        const std::size_t vertex_end =
            index + 1 < count ? dword(logical_data, 0x18 + (index + 1) * 8) : attribute_offset;
        if (sizes[index] % 14 != 0)
            throw FieldFormatError("Triangle byte count is not a multiple of 14");
        const auto triangles =
            region(logical_data, triangle_offset, sizes[index], "collision triangles");
        if (triangle_offset < header_size || triangle_offset > vertex_offset ||
            sizes[index] > vertex_offset - triangle_offset)
            throw FieldFormatError("Overlapping collision header/triangle/vertex tables");
        if (vertex_end < vertex_offset)
            throw FieldFormatError("Reversed collision vertex span");
        const auto vertex_bytes = vertex_end - vertex_offset;
        if (vertex_bytes % 8 != 0)
            throw FieldFormatError("Vertex span is not a multiple of 8");
        const auto vertices =
            region(logical_data, vertex_offset, vertex_bytes, "collision vertices");
        CollisionLayer layer;
        layer.vertices.resize(vertices.size() / 8);
        for (std::size_t i = 0; i < layer.vertices.size(); ++i)
            for (std::size_t coordinate = 0; coordinate < 4; ++coordinate)
                layer.vertices[i][coordinate] = signed_word(vertices, i * 8 + coordinate * 2);
        layer.triangles.resize(triangles.size() / 14);
        for (std::size_t i = 0; i < layer.triangles.size(); ++i) {
            auto &triangle = layer.triangles[i];
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto vertex = signed_word(triangles, i * 14 + corner * 2);
                if (vertex < 0 || static_cast<std::size_t>(vertex) >= layer.vertices.size())
                    throw FieldFormatError("Collision triangle has invalid vertex index");
                triangle.vertices[corner] = vertex;
                triangle.adjacent_raw[corner] = word(triangles, i * 14 + 6 + corner * 2);
            }
            triangle.attribute_raw = word(triangles, i * 14 + 12);
        }
        result.layers.push_back(std::move(layer));
    }
    const auto attributes = logical_data.subspan(attribute_offset);
    result.attributes_raw.assign(attributes.begin(), attributes.end());
    for (std::size_t index = count; index < sizes.size(); ++index)
        result.reserved_triangle_sizes.push_back(sizes[index]);
    return result;
}

} // namespace xem::reconstruction::field
