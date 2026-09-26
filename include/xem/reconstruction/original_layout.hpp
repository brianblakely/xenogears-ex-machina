#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace xem::reconstruction {

class Program;

// The original RAM layout of Program-owned globals: one entry per original
// global, read and written through the owning Program value. Original behavior
// that copies memory ranges (field-return snapshots) and analysis import/export
// share this table, so no range has a second representation.
struct OriginalGlobal {
    std::string name;
    std::uint32_t address;
    std::size_t width;
    bool resident; // Owned by ResidentState: valid without a loaded field.
    std::function<std::uint32_t(const Program &)> get;
    std::function<void(Program &, std::uint32_t)> set;
};
[[nodiscard]] const std::vector<OriginalGlobal> &original_globals();
// Entries of the interrupt-context state (interrupts.cpp), part of the table.
void add_interrupt_globals(std::vector<OriginalGlobal> &table);
void add_gpu_globals(std::vector<OriginalGlobal> &table);    // gpu_queue.cpp
void add_reload_globals(std::vector<OriginalGlobal> &table); // field_reload.cpp
void add_movie_globals(std::vector<OriginalGlobal> &table);  // field_movie_player.cpp

struct OriginalRegion {
    std::uint32_t address;
    std::uint32_t size;
};
// Fixed-address regions a field-return snapshot copies (800a3f4c/800a3474).
// The collision attribute block between the transform and field regions goes
// through the loader's pointer at 800afb20 and is not a fixed region.
inline constexpr std::array<OriginalRegion, 4> snapshot_regions{
    {{0x800b007c, 0x38}, {0x800afa54, 0x74}, {0x800b2078, 0x2e4}, {0x800af880, 0x1c8}}};

// Copy an original byte range into or out of Program state. Every byte must
// belong to exactly one entry lying wholly inside the range.
void write_original(Program &program, std::uint32_t address, std::span<const std::uint8_t> bytes);
void read_original(const Program &program, std::uint32_t address, std::span<std::uint8_t> bytes);

} // namespace xem::reconstruction
