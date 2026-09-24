// An invented menu memory image exercises consumable effects and use, the
// equipment exchange, bonus and shown-stat rules and the explicit unsupported
// paths. It describes no original content.
#include "xem/reconstruction/menu.hpp"
#include "xem/reconstruction/program.hpp"

#include <iostream>
#include <string_view>

namespace game = xem::reconstruction;
namespace menu = game::menu;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

constexpr std::uint32_t state = 0x80100000;
constexpr std::uint32_t party = 0x80101000;
constexpr std::uint32_t tables = 0x80102000;
constexpr std::uint32_t items = 0x80103000;
constexpr std::uint32_t weapons = 0x80104000;
constexpr std::uint32_t accessories = 0x80105000;
constexpr std::uint32_t screen = 0x80106000;
constexpr std::uint32_t game_data = 0x8006d634;

struct Sample {
    menu::MenuMemory memory;
    game::resident::SoundDriver sound;
    menu::Menu context{memory, sound};

    Sample() {
        memory.regions[menu::state_pointer].resize(4);
        memory.regions[menu::overlay_base].resize(0x2e4a0);
        memory.regions[state].resize(0x400);
        memory.regions[party].resize(0x40);
        memory.regions[tables].resize(0xd0);
        memory.regions[items].resize(0x100);
        memory.regions[weapons].resize(0x100);
        memory.regions[accessories].resize(0x100);
        memory.regions[screen].resize(0x2b0);
        memory.regions[game_data].resize(0x2358);
        put32(menu::state_pointer, state);
        put32(state + menu::state_party, party);
        put32(state + menu::state_tables, tables);
        put32(state + menu::state_equip_screen, screen);
        put32(tables + menu::weapon_table, weapons);
        put32(tables + menu::accessory_table, accessories);
        put32(tables + menu::item_table, items);
        // Party slot bits 801e96a8 and members 1, 2 and an empty slot.
        for (std::uint32_t slot = 0; slot < 3; ++slot)
            memory.put16(0x801e96a8 + slot * 2, 1U << slot);
        memory.put8(party + 0x30, 1);
        memory.put8(party + 0x31, 2);
        memory.put8(party + 0x32, 0xff);
    }
    void put32(std::uint32_t address, std::uint32_t value) {
        memory.put16(address, value & 0xffff);
        memory.put16(address + 2, value >> 16);
    }
    [[nodiscard]] std::uint32_t record(std::uint32_t character) const {
        return menu::character_record(character);
    }
    void item(std::uint32_t id, std::uint32_t amount, std::uint32_t effects,
              std::uint32_t select = 0) {
        memory.put8(items + id * 0x10 + 8, amount);
        memory.put16(items + id * 0x10 + 0xa, effects);
        memory.put16(items + id * 0x10 + 0xc, select);
    }
    void vitals(std::uint32_t character, std::uint32_t hp, std::uint32_t max_hp, std::uint32_t ep,
                std::uint32_t max_ep) {
        memory.put16(record(character) + 0x4c, hp);
        memory.put16(record(character) + 0x4e, max_hp);
        memory.put16(record(character) + 0x50, ep);
        memory.put16(record(character) + 0x52, max_ep);
    }
};

void effects() {
    Sample sample;
    auto &memory = sample.memory;
    const auto fei = sample.record(1);
    sample.item(3, 1, 0x8000); // HP 50
    sample.vitals(1, 20, 56, 5, 10);
    check(menu::apply_item_effect(sample.context, tables, 1, 3) == 0 &&
              memory.u16(fei + 0x4c) == 56,
          "An HP item adds 50 per amount, capped at the maximum");
    check(menu::apply_item_effect(sample.context, tables, 1, 3) == 1 &&
              memory.u16(fei + 0x4c) == 56,
          "An HP item on full HP restores nothing");

    sample.item(4, 2, 0xc000); // HP and EP
    sample.vitals(1, 56, 56, 1, 10);
    check(menu::apply_item_effect(sample.context, tables, 1, 4) == 0 &&
              memory.u16(fei + 0x50) == 10,
          "An HP and EP item counts as used unless both are full");
    sample.vitals(1, 56, 56, 10, 10);
    check(menu::apply_item_effect(sample.context, tables, 1, 4) == 1,
          "An HP and EP item on a full character restores nothing");

    // Permanent raises: stat +58 and the HP and EP maximums, capped.
    sample.item(5, 5, 4, 0x8000 | 0x800 | 0x400);
    memory.put8(fei + 0x58, 198);
    sample.vitals(1, 1, 997, 1, 97);
    check(menu::apply_item_effect(sample.context, tables, 1, 5) == 0 &&
              memory.u8(fei + 0x58) == 200 && memory.u16(fei + 0x4e) == 999 &&
              memory.u16(fei + 0x52) == 99,
          "Permanent raises stop at 200, 999 and 99");

    // The +78 counter: down by the low byte, stopping at zero, or up to 200.
    sample.item(6, 0, 2, 7);
    memory.put8(fei + 0x78, 5);
    static_cast<void>(menu::apply_item_effect(sample.context, tables, 1, 6));
    check(memory.u8(fei + 0x78) == 0, "Lowering the counter below zero stops at zero");
    sample.item(7, 0, 2, 0x8000 | 150);
    memory.put8(fei + 0x78, 100);
    static_cast<void>(menu::apply_item_effect(sample.context, tables, 1, 7));
    check(memory.u8(fei + 0x78) == 200, "Raising the counter stops at 200");

    sample.item(8, 1, 1);
    bool missing = false;
    try {
        static_cast<void>(menu::apply_item_effect(sample.context, tables, 1, 8));
    } catch (const game::MissingDependency &error) {
        missing = error.dependency == "symbol:801e5058";
    }
    check(missing, "Effect flag 1 stops at its unreconstructed routine");
}

void use() {
    Sample sample;
    auto &memory = sample.memory;
    sample.item(3, 1, 0x8000);
    memory.put8(menu::item_counts + 4, 2);
    memory.put8(menu::item_ids + 4, 3);
    sample.vitals(1, 56, 56, 0, 0);
    sample.vitals(2, 10, 100, 0, 0);
    menu::use_item(sample.context, 4, 3);
    check(memory.u16(sample.record(2) + 0x4c) == 60 && memory.u8(menu::item_counts + 4) == 1,
          "Using an item on the party applies it to each target and consumes one");
    sample.vitals(2, 100, 100, 0, 0);
    menu::use_item(sample.context, 4, 3);
    check(memory.u8(menu::item_counts + 4) == 1 && memory.u8(menu::item_ids + 4) == 3,
          "An item no target takes is not consumed");
    sample.vitals(1, 1, 56, 0, 0);
    menu::use_item(sample.context, 4, 1);
    check(memory.u8(menu::item_counts + 4) == 0 && memory.u8(menu::item_ids + 4) == 0,
          "The last item leaves an empty inventory entry");
}

void equipment() {
    Sample sample;
    auto &memory = sample.memory;
    const auto fei = sample.record(1);
    constexpr std::uint32_t ids = 0x8006f4fc;
    constexpr std::uint32_t counts = ids - 200;
    // Removal: the screen cleared slot 1 and kept part 9.
    memory.put8(fei + 0x74, 0);
    memory.put8(screen + 0x2a6, 9);
    memory.put8(ids, 5);
    memory.put8(counts, 1);
    check(menu::swap_equipment(sample.context, 0, 1, 0, 0) == 0 && memory.u8(ids + 1) == 9 &&
              memory.u8(counts + 1) == 1,
          "A removed accessory joins the first free inventory entry");
    // Equipping it again gives it up; the empty entry loses its id.
    memory.put8(fei + 0x74, 9);
    memory.put8(screen + 0x2a6, 0);
    static_cast<void>(menu::swap_equipment(sample.context, 0, 1, 0, 0));
    check(memory.u8(ids + 1) == 0 && memory.u8(counts + 1) == 0 && memory.u8(ids) == 5,
          "An equipped accessory leaves the inventory");
    // Counts stop at 99.
    memory.put8(counts, 99);
    memory.put8(fei + 0x74, 0);
    memory.put8(screen + 0x2a6, 5);
    static_cast<void>(menu::swap_equipment(sample.context, 0, 1, 0, 0));
    check(memory.u8(counts) == 99, "Inventory counts stop at 99");
    // Without a selected weapon the kept one returns to the record.
    memory.put8(fei + 0x6a, 0);
    memory.put8(screen + 0x29c, 7);
    check(menu::swap_equipment(sample.context, 0, 0, 0, 0) == 0 && memory.u8(fei + 0x6a) == 7,
          "An empty weapon selection restores the kept weapon");
    // Character 4 reports a replaced weapon.
    memory.put8(party + 0x30, 4);
    memory.put8(sample.record(4) + 0x6a, 2);
    memory.put8(0x8006f3d0, 2);
    memory.put8(0x8006f3d0 - 100, 1);
    check(menu::swap_equipment(sample.context, 0, 0, 0, 0) == 1 && memory.u8(0x8006f3d0) == 0 &&
              memory.u8(0x8006f3d1) == 7,
          "Character 4's weapon change is reported");
}

void bonuses() {
    Sample sample;
    auto &memory = sample.memory;
    const auto fei = sample.record(1);
    // Accessory 2: +8 adds 3 to +2d, kind 8 adds +a to +30, +c adds its low
    // byte to +28 and +2e.
    memory.put8(fei + 0x74, 2);
    memory.put8(accessories + 0x20 + 8, 3);
    memory.put8(accessories + 0x20 + 9, 8);
    memory.put8(accessories + 0x20 + 0xa, 4);
    memory.put16(accessories + 0x20 + 0xc, 0x8000 | 0x400 | 6);
    // Accessory 1: kind 7 sets flags in +8e.
    memory.put8(fei + 0x75, 1);
    memory.put8(accessories + 0x10 + 9, 7);
    memory.put16(accessories + 0x10 + 0xa, 0x0101);
    memory.put8(fei + 0x28, 77); // cleared first
    // Weapon 3.
    memory.put8(fei + 0x6a, 3);
    memory.put16(weapons + 0x30 + 8, 0x1234);
    memory.put8(weapons + 0x30 + 0xa, 5);
    memory.put8(weapons + 0x30 + 0xb, 100);
    memory.put8(weapons + 0x30 + 0xc, 40);
    menu::equipment_bonuses(sample.context, tables, 1);
    check(memory.u8(fei + 0x2d) == 3 && memory.u8(fei + 0x30) == 4 && memory.u8(fei + 0x28) == 6 &&
              memory.u8(fei + 0x2e) == 6,
          "Accessories add their bonuses from zero");
    check(memory.u16(fei) == 0x1234 && memory.u8(fei + 4) == 40 && memory.u8(fei + 2) == 5 &&
              memory.u16(fei + 0x8e) == (0x0101 | 0x1234),
          "The weapon values are copied and a +3 of 100 adds +0 to the +8e flags");

    memory.put8(fei + 0x58, 250);
    memory.put8(fei + 0x5a, 30);
    menu::equipment_stats(sample.context, tables, 1);
    check(memory.u16(tables + 0xb8) == 250 && memory.u16(tables + 0xc4) == 16 &&
              memory.u16(tables + 0xba) == 6,
          "Shown stats sum base and bonus bytes; the +c4 value above 20 shows 16");

    // Kind 4 takes (+4 + +1c) * 6 / 10 from its two weapons.
    memory.put8(fei + 0x56, 4);
    memory.put8(fei + 0x6f, 3);
    memory.put8(fei + 0x72, 3);
    menu::equipment_bonuses(sample.context, tables, 1);
    menu::equipment_stats(sample.context, tables, 1);
    check(memory.u8(fei + 0x1c) == 40 && memory.u16(tables + 0xb8) == 48,
          "Kind 4 combines both weapons");
}
} // namespace

int main() {
    try {
        effects();
        use();
        equipment();
        bonuses();
        std::cout << "Menu actions: four source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
