#pragma once

#include "xem/reconstruction/field_events.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace xem::reconstruction::field {

class PackedError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class FieldFormatError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct PackedBlock {
    std::vector<std::uint8_t> data;
    std::size_t token_bytes{};
    std::size_t source_bytes_read{};
    std::size_t groups{};
};

// Resident 80032eb4..80032f50, both EVID-REF-014 reference executables.
// The original tests output equality only between complete eight-token groups
// and reads the following flag before returning. token_bytes excludes that
// final read; source_bytes_read includes it. Overlapping copies advance a byte
// at a time. Unsafe original reads/writes fail explicitly within supplied bounds.
[[nodiscard]] PackedBlock decode_packed_block(std::span<const std::uint8_t> source_memory,
                                              std::size_t output_limit = 0x200000);

struct FieldComponent {
    std::uint32_t index{};
    std::uint32_t source_offset{};
    std::uint32_t logical_size{};
    PackedBlock decoded;
    [[nodiscard]] std::span<const std::uint8_t> logical_data() const;
};

// The field header at 80070cc8 supplies nine sizes (+10c) and offsets (+130).
// Each original allocation allows logical_size + 16 bytes; the packed output
// keeps its own size and padding. Source spans must contain measured bytes:
// streams may read across the next component and beyond the original file.
// Disc-sector padding and captured adjacent RAM are distinct valid inputs;
// this function never invents either. This is the structural decode stage,
// not the loader's remaining initialization, upload, or resource lifetime work.
[[nodiscard]] std::array<FieldComponent, 9>
decode_field_components(std::span<const std::uint8_t> source_memory);

struct EventPackage {
    std::array<std::uint8_t, 128> variable_unsigned_bits{};
    std::vector<std::array<std::uint16_t, 32>> entries;
    std::vector<std::uint8_t> bytecode;
    std::size_t bytecode_offset{};

    // Views remain valid while this package is alive and its vectors unchanged.
    [[nodiscard]] EventProgram program() const & noexcept;
    EventProgram program() const && = delete;
};

// Own the variable bitmap, actor entry table, and bytecode used by EventProgram.
// Parsing does not initialize variable values or execute any event instructions.
[[nodiscard]] EventPackage parse_event_package(std::span<const std::uint8_t> logical_data);

struct CollisionTriangle {
    std::array<std::int16_t, 3> vertices{};
    std::array<std::uint16_t, 3> adjacent_raw{};
    std::uint16_t attribute_raw{};
    bool operator==(const CollisionTriangle &) const = default;
};
struct CollisionLayer {
    std::vector<CollisionTriangle> triangles;
    std::vector<std::array<std::int16_t, 4>> vertices;
    bool operator==(const CollisionLayer &) const = default;
};
struct CollisionPackage {
    std::vector<CollisionLayer> layers;
    std::vector<std::uint8_t> attributes_raw;
    std::vector<std::uint32_t> reserved_triangle_sizes;
    bool operator==(const CollisionPackage &) const = default;
};

// Field 80070cc8/800711a8: four size words are present regardless of active
// layer count. Triangles are 14 bytes; vertices are four signed halfwords.
// Opaque adjacency/attribute fields are retained without traversal assumptions.
[[nodiscard]] CollisionPackage parse_collision_package(std::span<const std::uint8_t> logical_data);

} // namespace xem::reconstruction::field
