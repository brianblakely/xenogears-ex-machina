#pragma once

#include "xem/reconstruction/program.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// Analysis-only correlation between an original field RAM image and Program.
// The native runtime never imports PS1 memory; these addresses belong to the
// Disc 1 resident dc0b2dd7... and field overlay 38a1ce82... identities only.
namespace xem::analysis {

inline constexpr std::size_t ram_bytes = 0x200000;

struct OriginalMemory {
    std::vector<std::uint8_t> ram;
    std::array<std::uint8_t, 1024> scratchpad{};

    [[nodiscard]] std::span<const std::uint8_t> range(std::uint32_t address,
                                                      std::size_t size) const;
    [[nodiscard]] std::span<std::uint8_t> range(std::uint32_t address, std::size_t size);
    [[nodiscard]] std::uint32_t word(std::uint32_t address, std::size_t width = 4) const;
    void put(std::uint32_t address, std::uint32_t value, std::size_t width = 4);
};

struct OwnedRange {
    std::string_view name;
    std::uint32_t address;
    std::size_t size;
};

// A mutable original resource allocation (sprite files, tables) whose extent
// was qualified by the caller from source data; its bytes come from the image.
struct ResourceExtent {
    std::uint32_t address;
    std::size_t size;
};

// Builds resident Program state (resident globals, math tables, the resident
// snapshot and the heap list) from the image; no field needs to be loaded.
[[nodiscard]] reconstruction::Program import_resident(const OriginalMemory &memory);
[[nodiscard]] std::vector<OwnedRange> export_resident(const reconstruction::Program &program,
                                                      OriginalMemory &memory);

// Builds battle-mode Program state: resident state plus the battle overlay
// region (8006faf0..800d39f0) and the heap blocks battle setup allocates.
[[nodiscard]] reconstruction::Program import_battle(const OriginalMemory &memory);
[[nodiscard]] std::vector<OwnedRange> export_battle(const reconstruction::Program &program,
                                                    OriginalMemory &memory);

// Builds field-mode Program state from the entry image. Field components are
// decoded from the separately qualified original source and must equal the
// loaded copies they describe; RAM is never parsed as a substitute format.
// `overlay` is the decoded field overlay image (loaded at 8006faf0).
[[nodiscard]] reconstruction::Program import_field(const OriginalMemory &memory,
                                                   std::span<const std::uint8_t> field_source,
                                                   std::span<const std::uint8_t> overlay,
                                                   std::span<const ResourceExtent> resources);

// Writes every Program-owned original correlation back over a copy of the entry
// image and lists the owned ranges. Comparison is performed by the caller.
[[nodiscard]] std::vector<OwnedRange> export_field(const reconstruction::Program &program,
                                                   OriginalMemory &memory);

} // namespace xem::analysis
