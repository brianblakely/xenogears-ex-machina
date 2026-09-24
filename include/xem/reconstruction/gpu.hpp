#pragma once

#include <array>
#include <cstdint>
#include <map>
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

// GPU packet memory built in original layout: ordering tables and primitive
// packets that DrawOTag hands to the GPU. Packets link through 24-bit original
// addresses, so regions keep their original address. Each region is owned
// whole by the drawing state that builds it.
class PacketMemory {
  public:
    struct Region {
        std::string name;
        std::vector<std::uint8_t> bytes;
    };
    void add(std::string name, std::uint32_t address, std::vector<std::uint8_t> bytes);
    [[nodiscard]] bool contains(std::uint32_t address, std::size_t width) const;
    [[nodiscard]] std::uint32_t word(std::uint32_t address, std::size_t width = 4) const;
    void put(std::uint32_t address, std::uint32_t value, std::size_t width = 4);
    [[nodiscard]] const std::map<std::uint32_t, Region> &regions() const { return regions_; }
    bool operator==(const PacketMemory &) const = default;

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

// Resident libgpu statics the reconstructed calls read or write.
struct GpuLibrary {
    std::uint8_t queue_check{};  // 800568d1: nonzero checks the queue before running a call
    std::uint8_t debug{};        // 800568d2: levels above 1 print
    std::uint8_t interlace{};    // 800568d3: adds the interlace bit to display modes
    std::int16_t vram_width{};   // 800568d4
    std::int16_t vram_height{};  // 800568d6
    std::uint32_t started{};     // 800568d8: set when a call runs
    std::uint32_t force_queue{}; // 800568dc: nonzero queues every call
    std::array<std::uint8_t, 0x5c> draw_environment{};    // 800568e0: last PutDrawEnv
    std::array<std::uint8_t, 0x14> display_environment{}; // 8005693c: last PutDispEnv
    std::array<std::uint32_t, 3> last_call{};             // 800569c4: function, parameter, argument
    std::uint32_t queue_in{};                             // 800569d4
    std::uint32_t queue_out{};                            // 800569d8
    std::uint32_t saved_mask{};                           // 800569dc: interrupt mask around a call
    std::uint32_t alarm{};                                // 800569e8: VSync(-1) + f0 deadline
    std::uint32_t alarm_polls{};                          // 800569ec
    std::array<std::uint8_t, 0x44> packet{};              // 8005a238: packet built by _clr
    std::array<std::uint8_t, 0x100> control{};            // 8005a27c: last GP1 value per command
    std::vector<GpuCommand> commands;                     // In program order
    bool operator==(const GpuLibrary &) const = default;
};

} // namespace xem::reconstruction
