#include "xem/reconstruction/menu.hpp"

#include "xem/reconstruction/program.hpp"

namespace xem::reconstruction::menu {
namespace {

std::uint8_t *byte_at(std::map<std::uint32_t, std::vector<std::uint8_t>> &regions,
                      std::uint32_t address, std::uint32_t size) {
    auto found = regions.upper_bound(address);
    if (found != regions.begin()) {
        --found;
        if (address - found->first + std::uint64_t{size} <= found->second.size())
            return found->second.data() + (address - found->first);
    }
    throw MenuError("Menu code reaches memory outside its owned regions");
}

// Character record fields.
constexpr std::uint32_t hp = 0x4c;           // u16
constexpr std::uint32_t max_hp = 0x4e;       // u16
constexpr std::uint32_t ep = 0x50;           // u16
constexpr std::uint32_t max_ep = 0x52;       // u16
constexpr std::uint32_t item_counter = 0x78; // u8

// 801c865c: the party slot bit (table 801e96a8) within `targets`.
bool targeted(const Menu &menu, std::uint32_t targets, std::uint32_t slot) {
    return (menu.memory.u16(0x801e96a8 + (slot & 0xff) * 2) & targets & 0xffff) != 0;
}

// 801c8574: when the menu plays sounds, sound effect `id` of the menu's
// effect bank (state + 2e4) through 80039db8.
void play_sound(Menu &menu, std::uint32_t id) {
    if (menu.entering_sound)
        menu.entering_sound();
    if (menu.memory.u8(menu.state() + state_sound) == 0)
        return;
    resident::start_bank_effect(menu.sound, menu.memory.u32(menu.state() + state_effect_bank), id);
}

} // namespace

std::uint32_t MenuMemory::u8(std::uint32_t address) const {
    return *byte_at(const_cast<MenuMemory *>(this)->regions, address, 1);
}
std::uint32_t MenuMemory::u16(std::uint32_t address) const {
    const auto *bytes = byte_at(const_cast<MenuMemory *>(this)->regions, address, 2);
    return static_cast<std::uint32_t>(bytes[0] | bytes[1] << 8);
}
std::uint32_t MenuMemory::u32(std::uint32_t address) const {
    const auto *bytes = byte_at(const_cast<MenuMemory *>(this)->regions, address, 4);
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 |
           static_cast<std::uint32_t>(bytes[2]) << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
}
void MenuMemory::put8(std::uint32_t address, std::uint32_t value) {
    *byte_at(regions, address, 1) = static_cast<std::uint8_t>(value);
}
void MenuMemory::put16(std::uint32_t address, std::uint32_t value) {
    auto *bytes = byte_at(regions, address, 2);
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
}
void MenuMemory::put32(std::uint32_t address, std::uint32_t value) {
    put16(address, value & 0xffff);
    put16(address + 2, value >> 16);
}
std::span<std::uint8_t> MenuMemory::bytes(std::uint32_t address, std::uint32_t size) {
    return {byte_at(regions, address, size), size};
}
std::span<const std::uint8_t> MenuMemory::bytes(std::uint32_t address, std::uint32_t size) const {
    return {byte_at(const_cast<MenuMemory *>(this)->regions, address, size), size};
}
std::span<const std::uint8_t> MenuMemory::tail(std::uint32_t address) const {
    auto found = regions.upper_bound(address);
    if (found == regions.begin() || (--found, address - found->first >= found->second.size()))
        throw MenuError("Menu code reaches memory outside its owned regions");
    return std::span<const std::uint8_t>(found->second).subspan(address - found->first);
}

std::uint32_t apply_item_effect(Menu &menu, std::uint32_t tables, std::uint32_t character,
                                std::uint32_t item) {
    auto &memory = menu.memory;
    const auto record = character_record(character);
    // Item entry: amount +8, effect flags +a, stat selection or change +c.
    const auto entry = memory.u32(tables + item_table) + (item & 0xff) * 0x10;
    const auto amount = memory.u8(entry + 8);
    const auto effects = memory.u16(entry + 0xa);
    const auto add8 = [&](std::uint32_t offset, std::uint32_t value) {
        memory.put8(record + offset, memory.u8(record + offset) + value);
    };
    const auto add16 = [&](std::uint32_t offset, std::uint32_t value) {
        memory.put16(record + offset, memory.u16(record + offset) + value);
    };
    const auto cap8 = [&](std::uint32_t offset, std::uint32_t limit) {
        if (memory.u8(record + offset) > limit)
            memory.put8(record + offset, limit);
    };
    const auto cap16 = [&](std::uint32_t offset, std::uint32_t limit, std::uint32_t value) {
        if (memory.u16(record + offset) > limit)
            memory.put16(record + offset, value);
    };

    // HP (50 per amount) and EP (10 per amount); already full is "nothing".
    bool hp_full = false, ep_full = false;
    if ((effects & 0x8000) != 0) {
        if (memory.u16(record + hp) == memory.u16(record + max_hp))
            hp_full = true;
        else
            add16(hp, amount * 50);
    }
    if ((effects & 0x4000) != 0) {
        if (memory.u16(record + ep) == memory.u16(record + max_ep))
            ep_full = true;
        else
            add16(ep, amount * 10);
    }
    if (memory.u16(record + max_hp) < memory.u16(record + hp))
        memory.put16(record + hp, memory.u16(record + max_hp));
    if (memory.u16(record + max_ep) < memory.u16(record + ep))
        memory.put16(record + ep, memory.u16(record + max_ep));

    // Permanent raises of the stats selected by +c, capped at 200, 999 and 99.
    if ((effects & 4) != 0) {
        const auto selected = memory.u16(entry + 0xc);
        if ((selected & 0x8000) != 0)
            add8(0x58, amount);
        if ((selected & 0x4000) != 0)
            add8(0x59, amount);
        if ((selected & 0x2000) != 0)
            add8(0x5b, amount);
        if ((selected & 0x1000) != 0)
            add8(0x5c, amount);
        if ((selected & 0x800) != 0)
            add16(max_hp, amount);
        if ((selected & 0x400) != 0)
            add16(max_ep, amount);
        for (const auto offset : {0x58U, 0x59U, 0x5bU, 0x5cU})
            cap8(offset, 200);
        cap16(max_hp, 999, 999);
        cap16(max_ep, 99, 99);
    }

    // The +78 counter moves by the low byte of +c: up (capped at 200) when +c
    // bit 8000 is set, otherwise down, stopping at zero.
    if ((effects & 2) != 0) {
        const auto change = memory.u16(entry + 0xc);
        const auto step = change & 0xff;
        if ((change & 0x8000) != 0) {
            add8(item_counter, step);
            cap8(item_counter, 200);
        } else if (memory.u8(record + item_counter) < step) {
            memory.put8(record + item_counter, 0);
        } else {
            add8(item_counter, 0x100 - step);
        }
    }

    if ((effects & 1) != 0 && (amount == 1 || amount == 2)) {
        const auto callee = amount == 1 ? 0x801e5058U : 0x801e5178U;
        throw MissingDependency({"menu_item_effect", callee, {}, {}},
                                amount == 1 ? "symbol:801e5058" : "symbol:801e5178", false,
                                "Item effect flag 1 reaches an unreconstructed menu routine");
    }

    const bool restores_hp = (effects & 0x8000) != 0;
    const bool restores_ep = (effects & 0x4000) != 0;
    if (restores_hp && restores_ep)
        return hp_full ? ep_full : 0;
    if (restores_hp)
        return hp_full;
    return restores_ep ? ep_full : 0;
}

void use_item(Menu &menu, std::uint32_t index, std::uint32_t targets) {
    auto &memory = menu.memory;
    bool used = false;
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        if (targeted(menu, targets, slot) &&
            apply_item_effect(menu, menu.tables(), menu.party_member(slot),
                              memory.u8(item_ids + index)) == 0)
            used = true;
    if (!used) {
        play_sound(menu, 4);
        return;
    }
    play_sound(menu, 0x37);
    const auto count = (memory.u8(item_counts + index) - 1) & 0xff;
    memory.put8(item_counts + index, count);
    if (count == 0)
        memory.put8(item_ids + index, 0);
}

} // namespace xem::reconstruction::menu
