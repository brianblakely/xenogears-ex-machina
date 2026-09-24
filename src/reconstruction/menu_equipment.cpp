#include "xem/reconstruction/menu.hpp"

namespace xem::reconstruction::menu {
namespace {

// Character record fields.
constexpr std::uint32_t kind = 0x56;          // u8; 4 takes a second weapon (+6f, +72)
constexpr std::uint32_t weapon = 0x6a;        // u8
constexpr std::uint32_t accessories = 0x74;   // 3 x u8
constexpr std::uint32_t special_parts = 0x6f; // u8 per special part (+6f + part)
constexpr std::uint32_t gear_index = 0xa0;    // u8: gear record index
// Gear records: a4 bytes each from 8006dfb4; weapon +4, accessories +part.
constexpr std::uint32_t gear_records = 0x8006dfb4;

// Equipment screen state: the parts the screen replaced, kept while it
// previews its selection in the record.
constexpr std::uint32_t kept_weapon = 0x29c;
constexpr std::uint32_t kept_special = 0x2a1;   // + part
constexpr std::uint32_t kept_accessory = 0x2a5; // + part

// An inventory list: `length` ids with a count byte for each, `length` bytes
// before them.
struct Inventory {
    std::uint32_t ids;
    std::uint32_t length;
    [[nodiscard]] std::uint32_t counts() const { return ids - length; }
};
constexpr Inventory weapons{0x8006f3d0, 100};
constexpr Inventory accessory_list{0x8006f4fc, 200};
constexpr Inventory gear_weapons{0x8006f754, 100};
constexpr Inventory gear_accessories{0x8006f84e, 150};

// 801df4c0: the selected part leaves the inventory and the replaced part
// joins it (first free entry when absent); emptied entries lose their id and
// counts stop at 99.
void exchange(MenuMemory &memory, const Inventory &list, std::uint32_t selected,
              std::uint32_t replaced) {
    const auto counts = list.counts();
    if (selected != 0)
        for (std::uint32_t i = 0; i < list.length; ++i)
            if (memory.u8(list.ids + i) == selected) {
                memory.put8(counts + i, memory.u8(counts + i) - 1);
                break;
            }
    if (replaced != 0) {
        bool added = false;
        for (std::uint32_t i = 0; i < list.length; ++i)
            if (memory.u8(list.ids + i) == replaced) {
                memory.put8(counts + i, memory.u8(counts + i) + 1);
                added = true;
                break;
            }
        if (!added)
            for (std::uint32_t i = 0; i < list.length; ++i)
                if (memory.u8(list.ids + i) == 0) {
                    memory.put8(list.ids + i, replaced);
                    memory.put8(counts + i, 1);
                    break;
                }
    }
    for (std::uint32_t i = 0; i < list.length; ++i) {
        const auto count = memory.u8(counts + i);
        if (count == 0)
            memory.put8(list.ids + i, 0);
        else if (count >= 100)
            memory.put8(counts + i, 99);
    }
}

// A 16-byte weapon or accessory table entry.
std::uint32_t table_entry(const MenuMemory &memory, std::uint32_t tables, std::uint32_t table,
                          std::uint32_t id) {
    return memory.u32(tables + table) + id * 0x10;
}

// Copy weapon values (entry +8 u16, +a, +b, +c) to record +0..+4 or +18..+1c.
void copy_weapon(MenuMemory &memory, std::uint32_t entry, std::uint32_t target) {
    memory.put8(target + 4, memory.u8(entry + 0xc));
    memory.put16(target, memory.u16(entry + 8));
    memory.put8(target + 2, memory.u8(entry + 0xa));
    memory.put8(target + 3, memory.u8(entry + 0xb));
}

} // namespace

std::uint32_t swap_equipment(Menu &menu, std::uint32_t slot, std::uint32_t part,
                             std::uint32_t special, std::uint32_t gear) {
    auto &memory = menu.memory;
    part &= 0xff;
    const auto character = menu.party_member(slot & 0xff);
    const auto record = character_record(character);
    const auto screen = memory.u32(menu.state() + state_equip_screen);
    const auto gear_record = [&] { return gear_records + memory.u8(record + gear_index) * 0xa4; };

    std::uint32_t result = 0;
    std::uint32_t at;   // the equipped part in the record
    std::uint32_t kept; // the replaced part the screen kept
    Inventory list = (gear & 0xff) != 0 ? gear_weapons : weapons;
    if ((special & 0xff) != 0) {
        at = (gear & 0xff) != 0 ? gear_record() - 4 + part : record + special_parts + part;
        kept = memory.u8(screen + kept_special + part);
        const auto selected = memory.u8(at);
        if (selected == 0) {
            memory.put8(at, kept);
            return 0;
        }
        // Special parts carry a durability byte (8006f8ba / 8006f8ea per id);
        // a worn part is not returned to the inventory.
        const auto durability = ((gear & 0xff) != 0 ? 0x8006f8eaU : 0x8006f8baU) + selected;
        if (memory.u8(durability) < 100)
            kept = 0;
        memory.put8(durability, 100);
        exchange(memory, list, selected, kept);
        return 0;
    }
    if (part == 0) {
        at = (gear & 0xff) != 0 ? gear_record() + 4 : record + weapon;
        kept = memory.u8(screen + kept_weapon);
        const auto selected = memory.u8(at);
        // Without a selection the replaced weapon returns to the record.
        if (selected == 0) {
            memory.put8(at, kept);
            return 0;
        }
        if (character == 4)
            result = 1;
        exchange(memory, list, selected, kept);
        return result;
    }
    if ((gear & 0xff) != 0) {
        at = gear_record() + part;
        list = gear_accessories;
    } else {
        at = record + accessories - 1 + part;
        list = accessory_list;
    }
    kept = memory.u8(screen + kept_accessory + part);
    exchange(memory, list, memory.u8(at), kept);
    return result;
}

void equipment_bonuses(Menu &menu, std::uint32_t tables, std::uint32_t character) {
    auto &memory = menu.memory;
    const auto record = character_record(character);
    // Accessory bonus bytes +28..+31, flag words +32, +7e, +82, +86, +8a,
    // +8e and +a1 start from zero.
    for (std::uint32_t offset = 0x28; offset <= 0x31; ++offset)
        memory.put8(record + offset, 0);
    for (const auto offset : {0x32U, 0x7eU, 0x82U, 0x86U, 0x8aU, 0x8eU})
        memory.put16(record + offset, 0);
    memory.put8(record + 0xa1, 0);
    const auto add8 = [&](std::uint32_t offset, std::uint32_t value) {
        memory.put8(record + offset, memory.u8(record + offset) + value);
    };
    const auto or16 = [&](std::uint32_t offset, std::uint32_t value) {
        memory.put16(record + offset, memory.u16(record + offset) | value);
    };
    for (std::uint32_t i = 0; i < 3; ++i) {
        // Accessory entry: +8 adds to +2d; +9 selects what +a does (jump
        // table 801c5204); the bits of +c select the bytes its low byte adds to.
        const auto entry =
            table_entry(memory, tables, accessory_table, memory.u8(record + accessories + i));
        add8(0x2d, memory.u8(entry + 8));
        switch (memory.u8(entry + 9)) {
        case 1:
            or16(0x7e, memory.u16(entry + 0xa));
            break;
        case 2:
            or16(0x82, memory.u16(entry + 0xa));
            break;
        case 3:
            or16(0x86, memory.u16(entry + 0xa));
            break;
        case 4:
            or16(0x8a, memory.u16(entry + 0xa));
            break;
        case 5:
            or16(0x32, memory.u16(entry + 0xa));
            break;
        case 7:
            or16(0x8e, memory.u16(entry + 0xa));
            break;
        case 8:
        case 9:
            add8(0x30, memory.u8(entry + 0xa));
            break;
        case 10:
            add8(0xa1, memory.u8(entry + 0xa));
            break;
        default:
            break;
        }
        const auto selected = memory.u16(entry + 0xc);
        const auto value = memory.u8(entry + 0xc);
        constexpr std::pair<std::uint32_t, std::uint32_t> targets[] = {
            {0x8000, 0x28}, {0x4000, 0x29}, {0x2000, 0x2a}, {0x1000, 0x2b},
            {0x800, 0x2c},  {0x400, 0x2e},  {0x200, 0x2f},  {0x100, 0x2d}};
        for (const auto &[bit, offset] : targets)
            if ((selected & bit) != 0)
                add8(offset, value);
    }
    copy_weapon(memory, table_entry(memory, tables, weapon_table, memory.u8(record + weapon)),
                record);
    if (memory.u8(record + kind) == 4) {
        copy_weapon(memory, table_entry(memory, tables, weapon_table, memory.u8(record + 0x6f)),
                    record);
        copy_weapon(memory, table_entry(memory, tables, weapon_table, memory.u8(record + 0x72)),
                    record + 0x18);
    }
    if (memory.u8(record + 3) == 100)
        or16(0x8e, memory.u16(record));
}

void equipment_stats(Menu &menu, std::uint32_t tables, std::uint32_t character) {
    auto &memory = menu.memory;
    const auto record = character_record(character);
    const auto sum = [&](std::uint32_t a, std::uint32_t b) {
        return memory.u8(record + a) + memory.u8(record + b);
    };
    if (memory.u8(record + kind) == 4)
        memory.put16(tables + 0xb8, sum(4, 0x1c) * 6 / 10);
    else
        memory.put16(tables + 0xb8, memory.u8(record + 4) + sum(0x58, 0x28));
    memory.put16(tables + 0xba, sum(0x5e, 0x2e));
    memory.put16(tables + 0xbc, memory.u8(record + 0x2d) + sum(0x59, 0x29));
    memory.put16(tables + 0xbe, sum(0x5f, 0x2f));
    memory.put16(tables + 0xc0, sum(0x5b, 0x2b));
    memory.put16(tables + 0xc2, sum(0x5c, 0x2c));
    memory.put16(tables + 0xc4, sum(0x5a, 0x2a));
    const auto cap = [&](std::uint32_t offset, std::uint32_t limit, std::uint32_t value) {
        if (memory.u16(tables + offset) > limit)
            memory.put16(tables + offset, value);
    };
    cap(0xb8, 250, 250);
    cap(0xba, 99, 99);
    cap(0xbc, 250, 250);
    cap(0xbe, 99, 99);
    cap(0xc0, 250, 250);
    cap(0xc2, 250, 250);
    // A value above 20 becomes 16 (sltiu 21 / ori 10 at 801e3c10).
    cap(0xc4, 20, 16);
}

} // namespace xem::reconstruction::menu
