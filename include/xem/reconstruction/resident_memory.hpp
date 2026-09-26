#pragma once

#include <cstdint>

namespace xem::reconstruction::resident {

// Memory at original addresses, as a mode's owner holds it (menu memory and
// its callee frames, battle memory, ...), for resident routines that several
// modes call. Reads and writes are little-endian, `width` 1, 2 or 4 bytes; an
// address the owner does not hold is an error of the owner.
class Memory {
  public:
    virtual ~Memory() = default;
    [[nodiscard]] virtual std::uint32_t read(std::uint32_t address, std::uint32_t width) const = 0;
    virtual void write(std::uint32_t address, std::uint32_t value, std::uint32_t width) = 0;
};

} // namespace xem::reconstruction::resident
