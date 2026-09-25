// Resident libgpu packet helpers and owned original regions.
#include "xem/reconstruction/gpu.hpp"

#include <stdexcept>

namespace xem::reconstruction {
namespace gpu {
namespace {
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
} // namespace

std::uint32_t texture_page(std::uint32_t tp, std::uint32_t abr, std::int32_t x, std::int32_t y) {
    return (tp & 3U) << 7U | (abr & 3U) << 5U | (u32(y) & 0x100U) >> 4U | (u32(x) & 0x3ffU) >> 6U |
           (u32(y) & 0x200U) << 2U;
}

std::uint32_t draw_mode(std::uint8_t type, bool dfe, bool dtd, std::uint32_t tpage) {
    const auto low = static_cast<std::uint8_t>(type - 1U) < 2;
    std::uint32_t high = 0xe1000000U;
    if (dtd)
        high |= low ? 0x800U : 0x200U;
    auto mode = tpage & (low ? 0x27ffU : 0x9ffU);
    if (dfe)
        mode |= low ? 0x1000U : 0x400U;
    return high | mode;
}

std::uint32_t texture_window(const std::array<std::int16_t, 4> *rect) {
    if (rect == nullptr)
        return 0;
    const auto &r = *rect;
    const auto x = (static_cast<std::uint32_t>(r[0]) & 0xffU) >> 3U;
    const auto y = (static_cast<std::uint32_t>(r[1]) & 0xffU) >> 3U;
    const auto w = (u32(-r[2]) & 0xffU) >> 3U;
    const auto h = (u32(-r[3]) & 0xffU) >> 3U;
    return 0xe2000000U | y << 15U | x << 10U | h << 5U | w;
}
} // namespace gpu

void OriginalRegions::add(std::string name, std::uint32_t address,
                          std::vector<std::uint8_t> bytes) {
    const auto size = bytes.size();
    const auto next = regions_.lower_bound(address);
    if ((next != regions_.end() && next->first < address + size) ||
        (next != regions_.begin() &&
         std::prev(next)->first + std::prev(next)->second.bytes.size() > address))
        throw std::invalid_argument("Original regions overlap");
    regions_.emplace(address, Region{std::move(name), std::move(bytes)});
}

const OriginalRegions::Region *OriginalRegions::find(std::uint32_t address, std::size_t width,
                                                     std::uint32_t &offset) const {
    auto found = regions_.upper_bound(address);
    if (found == regions_.begin())
        return nullptr;
    --found;
    offset = address - found->first;
    if (offset > found->second.bytes.size() || width > found->second.bytes.size() - offset)
        return nullptr;
    return &found->second;
}

bool OriginalRegions::contains(std::uint32_t address, std::size_t width) const {
    std::uint32_t offset = 0;
    return find(address, width, offset) != nullptr;
}

std::uint32_t OriginalRegions::word(std::uint32_t address, std::size_t width) const {
    std::uint32_t offset = 0;
    const auto *region = find(address, width, offset);
    if (region == nullptr)
        throw std::out_of_range("Read outside owned original regions");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(region->bytes[offset + i]) << (8U * i);
    return value;
}

void OriginalRegions::put(std::uint32_t address, std::uint32_t value, std::size_t width) {
    std::uint32_t offset = 0;
    auto *region = const_cast<Region *>(find(address, width, offset));
    if (region == nullptr)
        throw std::out_of_range("Write outside owned original regions");
    for (std::size_t i = 0; i < width; ++i)
        region->bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}

} // namespace xem::reconstruction
