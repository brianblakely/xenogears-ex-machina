#include "xem/reconstruction/menu_save.hpp"

#include "xem/reconstruction/program.hpp"

#include <algorithm>

namespace xem::reconstruction::menu {
namespace {

// Game data (8006d634) offsets, as the original addresses them.
constexpr std::uint32_t game = 0x8006d634;
constexpr std::uint32_t records = game + 0x26c; // 31 records of a4 bytes (8006d8a0)
constexpr std::uint32_t record_bytes = 0xa4;
constexpr std::uint32_t characters = 11;              // records 0..10, stored whole
constexpr std::uint32_t gears = game + 0x978;         // records 11..30 (8006dfac)
constexpr std::uint32_t pilot_bits = game + 0x16c4;   // u16 + pilot * 20 (8006ecf8)
constexpr std::uint32_t pilot_flags = game + 0x16da;  // u16 + pilot * 20 (8006ed0e)
constexpr std::uint32_t disc_word = game + 0x19d4;    // u16 (8006f008)
constexpr std::uint32_t game_globals = game + 0x2324; // 20 bytes (8006f958)

// Payload offsets.
constexpr std::uint32_t stored_characters = 0x290;
constexpr std::uint32_t stored_gears = 0x99c; // 20 entries of 3c bytes
constexpr std::uint32_t stored_gear_bytes = 0x3c;

void copy(MenuMemory &memory, std::uint32_t from, std::uint32_t to, std::uint32_t count) {
    const auto source = memory.bytes(from, count);
    const std::vector<std::uint8_t> bytes(source.begin(), source.end());
    std::ranges::copy(bytes, memory.bytes(to, count).begin());
}

std::uint32_t u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset] | (bytes[offset + 1] << 8));
}
std::uint32_t u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return u16(bytes, offset) | (u16(bytes, offset + 2) << 16);
}
void put16(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}
void put32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) {
    put16(bytes, offset, value & 0xffff);
    put16(bytes, offset + 2, value >> 16);
}

// Entry `index` of `stride` bytes of the table at `address`.
std::span<const std::uint8_t> entry(const MenuMemory &memory, std::uint32_t address,
                                    std::uint32_t index, std::uint32_t stride) {
    return memory.bytes(address + index * stride, stride);
}

std::span<std::uint8_t> gear_record(MenuMemory &memory, std::uint32_t j) {
    return memory.bytes(gears + j * record_bytes, record_bytes);
}

// 801e41c0(tables, j).
void recompute_engine(MenuMemory &memory, std::uint32_t tables, std::uint32_t j) {
    const auto record = gear_record(memory, j);
    const auto e = entry(memory, memory.u32(tables + 8), record[2], 0x18);
    put32(record, 0x60, u32(e, 4));
    put32(record, 0x64, u32(e, 4));
    record[0x98] = e[0x14];
    record[0x9e] = e[0x15];
    record[0x9d] = e[0x16];
    record[0x9f] = e[0x17];
    if (u32(record, 0x64) < u32(record, 0x60))
        put32(record, 0x60, u32(record, 0x64));
}

// 801e4258(tables, j).
void recompute_model(MenuMemory &memory, std::uint32_t tables, std::uint32_t j) {
    const auto record = gear_record(memory, j);
    const auto e = entry(memory, memory.u32(tables + 0x10), record[8], 0x14);
    put16(record, 0x70, u16(e, 8));
    put16(record, 0x72, u16(e, 10));
}

// 801e42ac(tables, j): the u16 at +38 is clamped to the new +3a.
void recompute_frame(MenuMemory &memory, std::uint32_t tables, std::uint32_t j) {
    const auto record = gear_record(memory, j);
    const auto e = entry(memory, memory.u32(tables + 0xc), record[3], 0x10);
    const auto previous = u16(record, 0x38);
    put16(record, 0x3a, u16(e, 6));
    record[0x3c] = e[0xc];
    record[0x3d] = e[0xd];
    record[0x3e] = e[0xe];
    record[0x3f] = e[0xe];
    if (u16(record, 0x3a) < previous)
        put16(record, 0x38, u16(record, 0x3a));
}

// 801e4928(j): ((u16 +44) / 120 - +75) / 2 with C truncation, 0 when the
// low halfword of the quotient is negative, as a byte.
std::uint8_t part_level(std::span<const std::uint8_t> record) {
    const auto difference = static_cast<std::int32_t>(u16(record, 0x44) / 120) - record[0x75];
    const auto half = difference / 2;
    const auto value = static_cast<std::int16_t>(half & 0xffff) < 0 ? 0 : half;
    return static_cast<std::uint8_t>(value & 0xff);
}

// 801e433c(tables, j): sum the three part entries (+9..+b) into the record and
// the pilot's bit words (game + 16c4 / 16da + pilot * 20).
void recompute_parts(MenuMemory &memory, std::uint32_t tables, std::uint32_t j) {
    const std::uint32_t pilot = memory.u8(gear_pilots + j);
    const auto bits = pilot_bits + pilot * 0x20;
    const auto flags = pilot_flags + pilot * 0x20;
    const auto record = gear_record(memory, j);

    for (const std::size_t offset :
         std::array<std::size_t, 7>{0x40, 0x42, 0x44, 0x48, 0x6e, 0x7e, 0x82})
        put16(record, offset, 0);
    std::fill_n(record.begin() + 0x4c, 4, std::uint8_t{0});  // +4c..4f
    std::fill_n(record.begin() + 0x50, 8, std::uint8_t{0});  // +50..57
    std::fill_n(record.begin() + 0x88, 16, std::uint8_t{0}); // +88..97
    put16(record, 0x86, u16(record, 0x86) & 0xf000);
    memory.put16(bits, memory.u16(bits) & 0xfb6f);

    const auto parts = memory.u32(tables + 0x14);
    for (std::size_t k = 0; k < 3; ++k) {
        const auto e = entry(memory, parts, record[9 + k], 0x1c);
        put16(record, 0x40, u16(record, 0x40) + e[0xd]);
        put16(record, 0x42, u16(record, 0x42) + e[0xe]);
        put16(record, 0x44, u16(record, 0x44) + u16(e, 6));
        record[0x4c] = static_cast<std::uint8_t>(record[0x4c] + e[0x18]);
        record[0x4d] = static_cast<std::uint8_t>(record[0x4d] + e[0x14]);
        record[0x54] = static_cast<std::uint8_t>(record[0x54] + e[0x1b]);
        for (std::size_t i = 0; i < 4; ++i)
            record[0x50 + i] = static_cast<std::uint8_t>(record[0x50 + i] + e[0x10 + i]);
        const auto mask = u16(e, 0x16);
        const auto set_if = [&](std::uint32_t first, std::uint32_t second, std::uint32_t bit) {
            const auto word = memory.u16(bits);
            if ((word & first) != 0 && (word & second) != 0)
                memory.put16(bits, word | bit);
        };
        switch (e[0x15]) { // Jump table 801c524c
        case 1:
            put16(record, 0x7e, u16(record, 0x7e) | mask);
            break;
        case 2:
            put16(record, 0x82, u16(record, 0x82) | mask);
            break;
        case 3:
            put16(record, 0x86, u16(record, 0x86) | mask);
            break;
        case 4:
            put16(record, 0x6e, u16(record, 0x6e) | mask);
            for (std::uint32_t b = 0; b < 16; ++b)
                if ((mask & (0x8000U >> b)) != 0)
                    record[0x88 + b] = static_cast<std::uint8_t>(record[0x88 + b] + e[0x1a]);
            break;
        case 5:
            record[0x4f] = static_cast<std::uint8_t>(record[0x4f] + e[0x16]);
            break;
        case 6:
            set_if(0x1000, 0x800, 0x400);
            break;
        case 7:
            set_if(0x200, 0x100, 0x80);
            break;
        case 8:
            set_if(0x40, 0x20, 0x10);
            break;
        case 9: // Continues into case 10 (801e4670 -> 801e4684).
            put16(record, 0x48, u16(record, 0x48) | mask);
            [[fallthrough]];
        case 10:
            record[0x56] = static_cast<std::uint8_t>(record[0x56] + e[0x16]);
            break;
        case 11:
            record[0x57] = static_cast<std::uint8_t>(record[0x57] + e[0x16]);
            break;
        default:
            break;
        }
    }
    record[0x4a] = part_level(record);
    if (record[0x4f] != 0)
        memory.put16(flags, memory.u16(flags) | 0x8000);
    else if (j == memory.u8(records + pilot * record_bytes + 0xa0))
        memory.put16(flags, memory.u16(flags) & 0x7fff);
}

std::uint8_t sum8(std::span<const std::uint8_t> bytes) {
    std::uint8_t sum = 0;
    for (const auto value : bytes)
        sum = static_cast<std::uint8_t>(sum + value);
    return sum;
}

std::uint32_t card_state(const Menu &menu) { return menu.memory.u32(menu.state() + state_card); }

} // namespace

NameCodec name_codec(const MenuMemory &memory) {
    const auto table = memory.u32(memory.u32(text_state) + 0x6c);
    return {memory.tail(table), static_cast<std::int32_t>(memory.u32(text_single_limit))};
}

std::uint16_t find_name_code(const NameCodec &codec, std::uint8_t first, std::uint8_t second) {
    for (std::uint32_t code = 0; code < searched_codes; ++code) {
        if (code * 2 + 2 > codec.table.size())
            throw SaveError("Name code search reads past the loaded code table");
        if (codec.table[code * 2] == first && codec.table[code * 2 + 1] == second)
            return static_cast<std::uint16_t>(code);
    }
    return missing_code;
}

EncodedText encode_text(const NameCodec &codec, std::span<const std::uint8_t> text) {
    std::size_t position = 0;
    const auto next = [&] {
        if (position >= text.size())
            throw SaveError("Name encoding reads past its known source bytes");
        return text[position++];
    };
    EncodedText encoded;
    for (auto byte = next(); byte != 0; byte = next()) {
        std::uint8_t first = 0, second = byte;
        if (static_cast<std::int32_t>(byte) >= codec.single_limit) {
            first = byte;
            second = next();
        }
        const auto code = find_name_code(codec, first, second);
        encoded.codes.push_back(code);
        if (code == missing_code) {
            encoded.result = -1;
            break;
        }
    }
    return encoded;
}

std::vector<std::uint8_t> decode_text(const NameCodec &codec, std::span<const std::uint8_t> codes,
                                      std::uint32_t count) {
    if (codes.size() < std::size_t{count} * 2)
        throw SaveError("Name decoding reads past its known source codes");
    std::vector<std::uint8_t> text;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto code = u16(codes, i * 2);
        if (code * 2 + 2 > codec.table.size())
            throw SaveError("Name code reads past the loaded code table");
        if (codec.table[code * 2] != 0)
            text.push_back(codec.table[code * 2]);
        text.push_back(codec.table[code * 2 + 1]);
    }
    text.push_back(0);
    return text;
}

void decode_names(Menu &menu, NameScratch &scratch) {
    auto &memory = menu.memory;
    const auto codec = name_codec(memory);
    for (std::uint32_t i = 0; i < name_count; ++i) {
        const auto name = memory.bytes(game + i * name_bytes, name_bytes);
        std::uint32_t count = 0;
        while (count < name_bytes / 2 && (name[count * 2] != 0 || name[count * 2 + 1] != 0))
            ++count;
        const auto text = decode_text(codec, name, count);
        if (text.size() > scratch.size())
            throw SaveError("A decoded name overruns 801cb184's buffer");
        std::ranges::copy(text, scratch.begin());
        std::ranges::copy(scratch, name.begin());
    }
}

void store_game_data(Menu &menu, std::uint32_t payload) {
    auto &memory = menu.memory;
    for (std::uint32_t i = 0; i < characters; ++i)
        copy(memory, records + i * record_bytes, payload + stored_characters + i * record_bytes,
             record_bytes);
    for (std::uint32_t j = 0; j < gear_count; ++j) {
        const auto record = gears + j * record_bytes;
        const auto stored = payload + stored_gears + j * stored_gear_bytes;
        copy(memory, record, stored, 0x28);
        memory.put32(stored + 0x28, memory.u32(record + 0x5c));
        memory.put16(stored + 0x34, memory.u16(record + 0x38));
        memory.put32(stored + 0x2c, memory.u32(record + 0x60));
        memory.put8(stored + 0x38, memory.u8(record + 0x99));
        memory.put8(stored + 0x39, memory.u8(record + 0x74));
        memory.put8(stored + 0x3a, memory.u8(record + 0x75));
    }
    copy(memory, game, payload + 0x24, 0xdc);
    copy(memory, game + 0xdc, payload + 0x100, 0x190);
    copy(memory, game + 0x1648, payload + 0xe4c, 0x78);
    copy(memory, game + 0x16c0, payload + 0xec4, 0x160);
    copy(memory, game + 0x1820, payload + 0x1024, 0x100);
    copy(memory, game + 0x1920, payload + 0x1124, 0xa38); // 8003f968
}

void serialize(Menu &menu, std::uint32_t payload, std::uint32_t digit, std::uint32_t disc,
               NameScratch &scratch) {
    auto &memory = menu.memory;
    for (std::uint32_t i = 0; i < 0x20; i += 2)
        memory.put16(game_globals + i, memory.u16(saved_globals + i));
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto id = menu.party_member(slot);
        if (id == 0xff) {
            memory.put8(payload + 0x1c + slot, 0xff);
            continue;
        }
        const auto record = records + id * record_bytes;
        memory.put8(payload + 0x1c + slot, id);
        memory.put16(payload + 4 + slot * 2, memory.u16(record + 0x4c));
        memory.put16(payload + 0xa + slot * 2, memory.u16(record + 0x4e));
        memory.put8(payload + 0x10 + slot, memory.u8(record + 0x50));
        memory.put8(payload + 0x13 + slot, memory.u8(record + 0x52));
        memory.put8(payload + 0x16 + slot, memory.u8(record + 0x62));
        memory.put8(payload + 0x19 + slot, memory.u8(record + 0x63));
    }
    memory.put8(payload + 0x1f, 0);
    memory.put8(payload + 0x23, digit & 0xff);
    memory.put32(payload, memory.u32(play_frames));

    // 801cbcb8: each name through a copy into a zeroed 20-byte buffer.
    const auto codec = name_codec(memory);
    for (std::uint32_t i = 0; i < name_count; ++i) {
        const auto name = memory.bytes(game + i * name_bytes, name_bytes);
        const std::array<std::uint8_t, name_bytes> source = [&] {
            std::array<std::uint8_t, name_bytes> bytes{};
            std::ranges::copy(name, bytes.begin());
            return bytes;
        }();
        const auto encoded = encode_text(codec, source);
        if (encoded.codes.size() > name_bytes / 2)
            throw SaveError("An encoded name overruns 801cba4c's buffer");
        std::ranges::fill(name, std::uint8_t{0});
        for (std::size_t k = 0; k < encoded.codes.size(); ++k)
            put16(name, k * 2, encoded.codes[k]);
    }
    // 801cbd2c: disc word.
    memory.put16(disc_word, memory.u8(disc_override) != 0 ? 1U : (disc - 1U) & 0xffffU);

    store_game_data(menu, payload);
    decode_names(menu, scratch);
}

std::uint32_t seal_payload(Menu &menu, std::uint32_t payload) {
    std::uint32_t sum = 0;
    for (const auto value : menu.memory.bytes(payload, checksum_offset))
        sum += value;
    menu.memory.put8(payload + checksum_offset, sum & 0xff);
    return sum;
}

LoadCheck check_loaded(const Menu &menu, std::uint32_t buffer) {
    LoadCheck check;
    check.sum = sum8(menu.memory.bytes(buffer + header_bytes, checksum_offset));
    check.stored = static_cast<std::uint8_t>(menu.memory.u8(buffer + file_bytes - 1));
    check.decision =
        check.sum == check.stored ? LoadDecision::accepted : LoadDecision::checksum_mismatch;
    return check;
}

void restore_game_data(Menu &menu, std::uint32_t payload, std::uint32_t tables) {
    auto &memory = menu.memory;
    for (std::uint32_t i = 0; i < characters; ++i)
        copy(memory, payload + stored_characters + i * record_bytes, records + i * record_bytes,
             record_bytes);
    copy(memory, payload + 0xec4, game + 0x16c0, 0x160);
    for (std::uint32_t j = 0; j < gear_count; ++j) {
        const auto record = gears + j * record_bytes;
        const auto stored = payload + stored_gears + j * stored_gear_bytes;
        copy(memory, stored, record, 0x28);
        memory.put32(record + 0x5c, memory.u32(stored + 0x28));
        recompute_engine(memory, tables, j);
        recompute_model(memory, tables, j);
        recompute_frame(memory, tables, j);
        recompute_parts(memory, tables, j);
        memory.put16(record + 0x38, memory.u16(stored + 0x34));
        memory.put32(record + 0x60, memory.u32(stored + 0x2c));
        memory.put8(record + 0x99, memory.u8(stored + 0x38));
        memory.put8(record + 0x74, memory.u8(stored + 0x39));
        memory.put8(record + 0x75, memory.u8(stored + 0x3a));
    }
    copy(memory, payload + 0x24, game, 0xdc);
    copy(memory, payload + 0x100, game + 0xdc, 0x190);
    copy(memory, payload + 0xe4c, game + 0x1648, 0x78);
    copy(memory, payload + 0x1024, game + 0x1820, 0x100);
    copy(memory, payload + 0x1124, game + 0x1920, 0xa38); // 8003f968
}

void apply_loaded(Menu &menu, std::uint32_t payload, NameScratch &scratch) {
    restore_game_data(menu, payload, menu.tables());
    auto &memory = menu.memory;
    memory.put32(play_frames, memory.u32(payload));
    for (std::uint32_t i = 0; i < 0x20; i += 2)
        memory.put16(saved_globals + i, memory.u16(game_globals + i));
    decode_names(menu, scratch);
}

namespace {
constexpr std::uint32_t slot_entries = 0x801e981c;

// A slot's card entry is loadable (801c9bcc / 801c9d34, mode 1).
bool loadable(const Menu &menu, std::uint32_t slot) {
    const auto &memory = menu.memory;
    const auto card = card_state(menu);
    const auto m = static_cast<std::int32_t>(memory.u32(slot_entries + slot * 4));
    const auto port = static_cast<std::uint32_t>(m / 16);
    const auto at = card + static_cast<std::uint32_t>(m);
    return memory.u8(card + 0x4fe4 + port) != 0 && memory.u8(at + 0x4fae) != 0xff &&
           memory.u8(at + 0x4f8e) != 0;
}
bool files_listed(const Menu &menu) {
    return (menu.memory.u32(card_state(menu) + 0x4f88) & 0xffff0000U) != 0;
}
// Only mode 1, the one the load passes, is reconstructed.
void require_load_mode(std::uint32_t mode, const char *operation, std::uint32_t address,
                       const char *symbol) {
    if (mode != 1)
        throw MissingDependency({operation, address, {}, {}}, symbol, false,
                                "Only the load mode (1) of the slot checks is recovered");
}
} // namespace

bool load_slot_valid(Menu &menu, std::uint32_t mode) {
    require_load_mode(mode, "menu_load_slot_valid", 0x801c9bcc, "symbol:801c9bcc");
    const auto slot = menu.memory.u32(card_state(menu) + 0x4f7c);
    const bool valid = files_listed(menu) && loadable(menu, slot);
    if (valid)
        menu.memory.put8(menu.state() + 0x4d8, 2);
    return valid;
}

std::uint32_t find_load_slot(Menu &menu, std::uint32_t mode) {
    require_load_mode(mode, "menu_load_find_slot", 0x801c9d34, "symbol:801c9d34");
    const auto card = card_state(menu);
    const std::uint32_t first = menu.memory.u8(card + 0x4fe4) == 0 ? 15 : 0;
    const std::uint32_t end = menu.memory.u8(card + 0x4fe5) == 0 ? 15 : 30;
    std::uint32_t found = 0xff;
    for (auto slot = first; slot < end; ++slot) {
        if (!files_listed(menu))
            break;
        if (loadable(menu, slot)) {
            found = slot;
            break;
        }
    }
    menu.memory.put8(menu.state() + 0x4d8, 2);
    return found;
}

CardPresence card_presence(const Menu &menu) {
    return menu.memory.u16(card_state(menu) + 0x4fe4) == 0 ? CardPresence::no_card
                                                           : CardPresence::present;
}

std::vector<std::uint8_t> save_file_block(const Menu &menu, std::uint32_t payload) {
    std::vector<std::uint8_t> block;
    const auto header = menu.memory.bytes(card_state(menu) + card_header, header_bytes);
    const auto body = menu.memory.bytes(payload, payload_bytes);
    block.insert(block.end(), header.begin(), header.end());
    block.insert(block.end(), body.begin(), body.end());
    return block;
}

std::string file_name(unsigned card, unsigned digit) {
    if (card > 1)
        throw SaveError("memory card port is not 0 or 1");
    const auto last = static_cast<char>(('0' + digit) & 0xff);
    if (digit > 0xff || last == 0)
        throw SaveError("file digit does not form a name character");
    return std::string(card == 0 ? "bu00:" : "bu10:") + "BASLUS-00664" + last;
}

std::uint8_t choose_file_digit(std::span<const std::uint8_t, 16> used, std::uint8_t existing) {
    if (existing != 0xff)
        return existing;
    for (std::uint8_t i = 0; i < file_indices; ++i)
        if (used[i] == 0)
            return i;
    return 0;
}

} // namespace xem::reconstruction::menu
