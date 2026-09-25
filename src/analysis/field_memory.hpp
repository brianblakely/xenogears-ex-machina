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
// Resident state and the loaded battle overlay with its BSS (8006faf0 to
// 800d39f0), before battle setup allocates anything.
[[nodiscard]] reconstruction::Program import_battle_overlay(const OriginalMemory &memory);
[[nodiscard]] reconstruction::Program import_battle(const OriginalMemory &memory);
[[nodiscard]] std::vector<OwnedRange> export_battle(const reconstruction::Program &program,
                                                    OriginalMemory &memory);

// Builds menu-mode Program state: resident state plus the heap block holding
// the menu overlay (801c5000), the menu state pointer word 800625a0 and the
// heap blocks the menu actions reach through it (menu state, party list, data
// table directory and its tables, equipment screen state, card state), the
// resident words the save and load use (globals 8005a3a0, text state pointer
// 80059360 and one-byte limit 8005934c; the play counter 80059488 is resident
// state) and the heap blocks of the text state and its name code table.
[[nodiscard]] reconstruction::Program import_menu(const OriginalMemory &memory);
// Adds the allocated heap block containing `address` (a save or load buffer)
// to menu memory.
void import_menu_block(reconstruction::Program &program, const OriginalMemory &memory,
                       std::uint32_t address);
[[nodiscard]] std::vector<OwnedRange> export_menu(const reconstruction::Program &program,
                                                  OriginalMemory &memory);

// Builds field-mode Program state from the entry image. Field components are
// decoded from the separately qualified original source and must equal the
// loaded copies they describe; RAM is never parsed as a substitute format.
// `overlay` is the decoded field overlay image (loaded at 8006faf0).
[[nodiscard]] reconstruction::Program import_field(const OriginalMemory &memory,
                                                   std::span<const std::uint8_t> field_source,
                                                   std::span<const std::uint8_t> overlay,
                                                   std::span<const ResourceExtent> resources);

// Builds the state of a field between the reload's teardown and the load of
// the next map (80070cc8): field globals, the decoded overlay and the fixed
// field regions, with no loaded components, actors or records.
[[nodiscard]] reconstruction::Program import_unloaded_field(const OriginalMemory &memory,
                                                            std::span<const std::uint8_t> overlay);

// Supplies recorded platform inputs to the Program. `platform` has one input
// per line: `drive LBA` (the sector the drive delivers next), `read SITE
// VALUE` (hex) or `interrupt`. `disc` is the raw 2352-byte-sector track the
// drive service reads; empty when the case needs none.
void load_platform(reconstruction::Program &program, const char *platform, const char *disc);
// Attaches the RAM that interrupt-context disc callbacks read: the active
// stream ring header and the list of a list read. While a music wave bank
// streams (8004f354 1) it also attaches the stream buffer (header and
// payload) until the stream ends, and the wave staging block (800c3a1c).
void attach_interrupt_memory(reconstruction::Program &program, const OriginalMemory &memory);
// Adds `size` bytes at `address` that a disc read placed and a call reads
// (as a disc transfer block).
void import_disc_data(reconstruction::Program &program, const OriginalMemory &memory,
                      std::uint32_t address, std::uint32_t size);

// Owns the bytes of allocated heap blocks that no other Program value owns,
// as resident heap contents: a field teardown releases whole blocks.
void import_heap_contents(reconstruction::Program &program, const OriginalMemory &memory);

// Writes every Program-owned original correlation back over a copy of the entry
// image and lists the owned ranges. Comparison is performed by the caller.
[[nodiscard]] std::vector<OwnedRange> export_field(const reconstruction::Program &program,
                                                   OriginalMemory &memory);

} // namespace xem::analysis
