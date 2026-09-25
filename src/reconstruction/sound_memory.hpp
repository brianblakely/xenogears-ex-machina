// Byte access of the resident sound driver (executable dc0b2dd7...) to its
// memory by original address, shared by the driver tick (sound_tick.cpp) and
// the music load calls (sound_load.cpp).
#pragma once

#include "xem/reconstruction/program.hpp"

#include <optional>
#include <string>
#include <type_traits>

namespace xem::reconstruction::detail {
using resident::SoundDriver;
using resident::SoundError;

inline std::string sound_hex(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(8, '0');
    for (int i = 7; i >= 0; --i, value >>= 4)
        text[static_cast<std::size_t>(i)] = digits[value & 15U];
    return text;
}

// Byte access to the driver's memory: its objects, statics, named globals and
// read-only constants.
class SoundMemory {
  public:
    // `transfers` are disc DMA blocks the driver reads (sequence event data
    // a disc read placed), or null.
    explicit SoundMemory(SoundDriver &driver,
                         const std::vector<resident::HeapBlock> *transfers = nullptr)
        : d(driver), transfers(transfers) {}
    std::uint32_t get(std::uint32_t address, std::uint32_t size) {
        if (const auto value = named(address, size))
            return *value;
        std::uint32_t value = 0;
        if (typed(address, size, [&](auto &field, std::uint32_t byte, std::uint32_t i) {
                value |= ((static_cast<std::uint32_t>(field) >> (8U * byte)) & 0xffU) << (8U * i);
            }))
            return value;
        const auto *bytes = find(address, size, false);
        for (std::uint32_t i = 0; i < size; ++i)
            value |= static_cast<std::uint32_t>(bytes[i]) << (8U * i);
        return value;
    }
    void put(std::uint32_t address, std::uint32_t value, std::uint32_t size) {
        if (put_named(address, value, size))
            return;
        if (typed(address, size, [&](auto &field, std::uint32_t byte, std::uint32_t i) {
                using Field = std::remove_reference_t<decltype(field)>;
                const auto kept = static_cast<std::uint32_t>(field) & ~(0xffU << (8U * byte));
                field = static_cast<Field>(kept | (((value >> (8U * i)) & 0xffU) << (8U * byte)));
            }))
            return;
        auto *bytes = find(address, size, true);
        for (std::uint32_t i = 0; i < size; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
    }
    std::uint32_t u8(std::uint32_t a) { return get(a, 1); }
    std::uint32_t u16(std::uint32_t a) { return get(a, 2); }
    std::uint32_t u32(std::uint32_t a) { return get(a, 4); }
    void w8(std::uint32_t a, std::uint32_t v) { put(a, v, 1); }
    void w16(std::uint32_t a, std::uint32_t v) { put(a, v, 2); }
    void w32(std::uint32_t a, std::uint32_t v) { put(a, v, 4); }
    SoundDriver &d;

  private:
    const std::vector<resident::HeapBlock> *transfers;
    static std::uint8_t *in(std::map<std::uint32_t, std::vector<std::uint8_t>> &blocks,
                            std::uint32_t address, std::uint32_t size) {
        auto found = blocks.upper_bound(address);
        if (found == blocks.begin())
            return nullptr;
        --found;
        if (address - found->first + std::uint64_t{size} > found->second.size())
            return nullptr;
        return found->second.data() + (address - found->first);
    }
    std::uint8_t *find(std::uint32_t address, std::uint32_t size, bool write) {
        if (auto *bytes = in(d.objects, address, size))
            return bytes;
        if (auto *bytes = in(d.statics, address, size))
            return bytes;
        if (!write) {
            if (auto *bytes = in(d.constants, address, size))
                return bytes;
            if (address - resident::pitch_table_address + std::uint64_t{size} <=
                d.pitch_tables.size())
                return d.pitch_tables.data() + (address - resident::pitch_table_address);
            if (transfers != nullptr)
                for (const auto &block : *transfers)
                    if (address - block.address + std::uint64_t{size} <= block.bytes.size())
                        return const_cast<std::uint8_t *>(block.bytes.data()) +
                               (address - block.address);
        }
        throw SoundError("Sound driver reaches memory outside its owned objects at " +
                         sound_hex(address));
    }
    // The volume fields of the common-attribute block (8005a3c0..8005a407),
    // which the volume setters also use, reached here by address: calls
    // `use(field, byte of the field, byte of the access)` for each byte of
    // the access. False when no byte is a field's; an access that is only
    // partly one is an error.
    template <class Use> bool typed(std::uint32_t address, std::uint32_t size, Use &&use) {
        std::uint32_t hits = 0;
        for (std::uint32_t i = 0; i < size; ++i) {
            const auto at = address + i;
            const auto visit = [&](auto &field, std::uint32_t base) {
                if (at - base >= sizeof(field))
                    return false;
                use(field, at - base, i);
                return true;
            };
            const auto pair = [&](std::array<std::uint16_t, 2> &fields, std::uint32_t base) {
                return visit(fields[0], base) || visit(fields[1], base + 2);
            };
            hits += visit(d.commits, 0x8005a3c0) || pair(d.master_pair, 0x8005a3c4) ||
                    pair(d.cd_pair, 0x8005a3d0) || visit(d.master, 0x8005a3e8) ||
                    visit(d.cd, 0x8005a3ea) || visit(d.reverb, 0x8005a3ec) ||
                    visit(d.master_level, 0x8005a3f0) || visit(d.master_step, 0x8005a3f4) ||
                    visit(d.master_frames, 0x8005a3f8) || visit(d.master_target, 0x8005a3fa) ||
                    visit(d.cd_level, 0x8005a3fc) || visit(d.cd_step, 0x8005a400) ||
                    visit(d.cd_frames, 0x8005a404) || visit(d.cd_target, 0x8005a406);
        }
        if (hits != 0 && hits != size)
            throw SoundError("Sound driver access straddles a driver field at " +
                             sound_hex(address));
        return hits != 0;
    }
    std::uint32_t *word(std::uint32_t address) {
        switch (address) {
        case 0x80059504:
            return &d.start_stamp;
        case 0x80059554:
            return &d.voice_changes;
        case 0x800594fc:
            return &d.voice_holds;
        case 0x800595d8:
            return &d.effect_block;
        case 0x80059564:
            return &d.sequences;
        case 0x80059558:
            return &d.wave_banks;
        case 0x80059404:
            return &d.effect_run;
        case 0x80059440:
            return &d.effect_banks;
        case 0x80059410:
            return &d.pool;
        default:
            if (address - 0x8006252cU < 24 * 4 && (address & 3U) == 0)
                return &d.voice_owners[(address - 0x8006252cU) / 4];
            return nullptr;
        }
    }
    std::optional<std::uint32_t> named(std::uint32_t address, std::uint32_t size) {
        if (address == 0x8005957c) {
            if (size != 2)
                throw SoundError("Sound flags read with another width");
            return d.flags;
        }
        if (auto *value = word(address)) {
            if (size != 4)
                throw SoundError("Driver word read with another width");
            return *value;
        }
        return std::nullopt;
    }
    bool put_named(std::uint32_t address, std::uint32_t value, std::uint32_t size) {
        if (address == 0x8005957c) {
            if (size != 2)
                throw SoundError("Sound flags written with another width");
            d.flags = static_cast<std::uint16_t>(value);
            return true;
        }
        if (auto *target = word(address)) {
            if (size != 4)
                throw SoundError("Driver word written with another width");
            *target = value;
            return true;
        }
        return false;
    }
};

} // namespace xem::reconstruction::detail
