#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace xem::reconstruction {

namespace gpu {
// Resident libgpu packet helpers (executable dc0b2dd7...). Pure functions.
// 80043a1c GetTPage(tp, abr, x, y).
[[nodiscard]] std::uint32_t texture_page(std::uint32_t tp, std::uint32_t abr, std::int32_t x,
                                         std::int32_t y);
// 800459dc get_mode(dfe, dtd, tpage) for the libgpu type at 800568d0.
[[nodiscard]] std::uint32_t draw_mode(std::uint8_t type, bool dfe, bool dtd, std::uint32_t tpage);
// 80045c10 get_tw(rect): the texture-window command; zero without a rectangle.
[[nodiscard]] std::uint32_t texture_window(const std::array<std::int16_t, 4> *rect);
} // namespace gpu

// Owned regions in original layout that drawing code addresses: ordering
// tables and primitive packets DrawOTag hands to the GPU (linked through
// 24-bit original addresses), and the records that describe them. Each region
// is owned whole.
class OriginalRegions {
  public:
    struct Region {
        std::string name;
        std::vector<std::uint8_t> bytes;
    };
    void add(std::string name, std::uint32_t address, std::vector<std::uint8_t> bytes);
    [[nodiscard]] bool contains(std::uint32_t address, std::size_t width) const;
    [[nodiscard]] std::uint32_t word(std::uint32_t address, std::size_t width = 4) const;
    void put(std::uint32_t address, std::uint32_t value, std::size_t width = 4);
    // The owned bytes from `address` to the end of its region; empty if none.
    [[nodiscard]] std::span<std::uint8_t> span(std::uint32_t address);
    // Drops the regions that start in [address, address + size).
    void remove(std::uint32_t address, std::size_t size);
    [[nodiscard]] const std::map<std::uint32_t, Region> &regions() const { return regions_; }
    bool operator==(const OriginalRegions &) const = default;

  private:
    [[nodiscard]] const Region *find(std::uint32_t address, std::size_t width,
                                     std::uint32_t &offset) const;
    std::map<std::uint32_t, Region> regions_;
};

// A libgpu call that reaches the GPU or its DMA channel, in program order. Not
// RAM: a native renderer receives these as commands.
struct GpuCommand {
    enum class Kind : std::uint8_t {
        clear_image,  // _clr: fill rect with color, packet at address
        load_image,   // _dws: copy words at address into VRAM rect
        store_image,  // _drs: copy VRAM rect into words at address
        move_image,   // MoveImage: copy VRAM rect to (x, y) in value
        draw_packets, // _cwc: send the packet list at address (DrawOTag, PutDrawEnv)
        control,      // _ctl: GP1 command in value (PutDispEnv)
    };
    Kind kind{};
    std::array<std::int16_t, 4> rect{};
    std::uint32_t address{};
    std::uint32_t value{};
    bool operator==(const GpuCommand &) const = default;
};

} // namespace xem::reconstruction
