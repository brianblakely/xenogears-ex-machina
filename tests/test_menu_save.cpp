// An invented menu memory image, code table, names and data tables exercise
// the name codec, the save payload and its side effects on the game data, the
// load decision, restore and slot selection. They describe no original
// content.
#include "xem/reconstruction/menu_save.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>
#include <vector>

namespace game = xem::reconstruction;
namespace menu = game::menu;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call> void rejects(Call call, const char *message, std::string_view reason) {
    bool rejected = false;
    try {
        call();
    } catch (const std::exception &error) {
        rejected = std::string_view(error.what()).find(reason) != std::string_view::npos;
    }
    check(rejected, message);
}

constexpr std::uint32_t state = 0x80100000;
constexpr std::uint32_t party = 0x80101000;
constexpr std::uint32_t tables = 0x80102000;
constexpr std::uint32_t engines = 0x80103000;
constexpr std::uint32_t frames = 0x80103100;
constexpr std::uint32_t models = 0x80103200;
constexpr std::uint32_t parts = 0x80103300;
constexpr std::uint32_t card = 0x80110000;
constexpr std::uint32_t text = 0x80120000;
constexpr std::uint32_t code_table = 0x80121000;
constexpr std::uint32_t payload = 0x80130000;
constexpr std::uint32_t buffer = 0x80140000;
constexpr std::uint32_t game_data = 0x8006d634;
constexpr std::uint32_t gear_base = game_data + 0x978;
std::uint32_t gear(std::uint32_t j) { return gear_base + j * 0xa4; }

// Codes: 0 'x' (a zero code reads as a terminator), 1 'a', 2 'b', 3 the two-byte
// pair (90, 41); the rest unused (ff ff).
constexpr std::uint8_t lead = 0x90;

struct Sample {
    menu::MenuMemory memory;
    game::resident::SoundDriver sound;
    menu::Menu context{memory, sound};

    Sample() {
        memory.regions[menu::state_pointer].resize(4);
        memory.regions[menu::overlay_base].resize(0x2e4a0);
        memory.regions[state].resize(0x500);
        memory.regions[party].resize(0x40);
        memory.regions[tables].resize(0x20);
        memory.regions[engines].resize(2 * 0x18);
        memory.regions[frames].resize(2 * 0x10);
        memory.regions[models].resize(2 * 0x14);
        memory.regions[parts].resize(4 * 0x1c);
        memory.regions[card].resize(0x5000);
        memory.regions[text].resize(0x70);
        memory.regions[code_table].assign(menu::searched_codes * 2, 0xff);
        memory.regions[payload].resize(menu::payload_bytes);
        memory.regions[buffer].resize(0x2100);
        memory.regions[game_data].resize(0x2358);
        memory.regions[menu::saved_globals].resize(0x20);
        memory.regions[menu::text_state].resize(4);
        memory.regions[menu::text_single_limit].resize(4);
        memory.put32(menu::state_pointer, state);
        memory.put32(state + menu::state_party, party);
        memory.put32(state + menu::state_tables, tables);
        memory.put32(state + menu::state_card, card);
        memory.put32(tables + 8, engines);
        memory.put32(tables + 0xc, frames);
        memory.put32(tables + 0x10, models);
        memory.put32(tables + 0x14, parts);
        memory.put32(menu::text_state, text);
        memory.put32(text + 0x6c, code_table);
        memory.put32(menu::text_single_limit, 0x80);
        const std::array<std::uint8_t, 8> codes{0, 'x', 0, 'a', 0, 'b', lead, 0x41};
        std::ranges::copy(codes, memory.regions[code_table].begin());
        // Gear pilot map 801e9808: every gear flown by character 0.
        for (std::uint32_t j = 0; j < menu::gear_count; ++j)
            memory.put8(menu::gear_pilots + j, 0);
        // Slot map 801e981c: slots 0..14 are port 0 entries 0..14, 15..29
        // port 1 entries 10..1e.
        for (std::uint32_t s = 0; s < 30; ++s)
            memory.put32(0x801e981c + s * 4, s < 15 ? s : s + 1);
    }
};

std::vector<std::uint8_t> bytes(std::string_view source) { return {source.begin(), source.end()}; }

void test_codec() {
    Sample sample;
    const auto codec = menu::name_codec(sample.memory);
    check(menu::find_name_code(codec, 0, 'b') == 2, "one-byte character code");
    check(menu::find_name_code(codec, lead, 0x41) == 3, "two-byte character code");
    check(menu::find_name_code(codec, 0, 'z') == menu::missing_code, "unknown pair");

    auto encoded = menu::encode_text(codec, bytes(std::string("ab\x90\x41\0", 5)));
    check(encoded.result == 0 && encoded.codes == std::vector<std::uint16_t>{1, 2, 3},
          "encode one- and two-byte characters");
    encoded = menu::encode_text(codec, bytes(std::string("azb\0", 4)));
    check(encoded.result == -1 && encoded.codes == std::vector<std::uint16_t>{1, 0x8000},
          "an unknown character stores 8000 and stops");
    // A lead byte takes the terminator as its second byte and reading goes on.
    encoded = menu::encode_text(codec, bytes(std::string("\x90\0a", 3)));
    check(encoded.result == -1 && encoded.codes.size() == 1, "lead byte before the terminator");
    rejects([&] { (void)menu::encode_text(codec, bytes("ab")); }, "unterminated source",
            "past its known source");

    const std::array<std::uint8_t, 6> codes{1, 0, 3, 0, 2, 0};
    check(menu::decode_text(codec, codes, 3) == std::vector<std::uint8_t>{'a', lead, 0x41, 'b', 0},
          "decode codes");
    check(menu::decode_text(codec, codes, 0) == std::vector<std::uint8_t>{0}, "empty decode");
    const std::array<std::uint8_t, 2> far{0x00, 0x01}; // Code 100: table bytes ff ff.
    check(menu::decode_text(codec, far, 1).size() == 3, "unused code decodes its table bytes");
    const std::array<std::uint8_t, 2> beyond{0x00, 0x10};
    rejects([&] { (void)menu::decode_text(codec, beyond, 1); }, "code past the table",
            "past the loaded code table");
}

void put_name(Sample &sample, std::uint32_t index, std::string_view name) {
    auto target = sample.memory.bytes(game_data + index * menu::name_bytes, menu::name_bytes);
    std::ranges::fill(target, std::uint8_t{0x5a}); // Stale bytes after the terminator.
    std::ranges::copy(name, target.begin());
    target[name.size()] = 0;
}

void test_decode_names() {
    Sample sample;
    for (std::uint32_t i = 0; i < menu::name_count; ++i) {
        auto name = sample.memory.bytes(game_data + i * menu::name_bytes, menu::name_bytes);
        std::ranges::fill(name, std::uint8_t{0});
    }
    // Name 0 "ab", name 1 "b"; the rest empty.
    sample.memory.put16(game_data, 1);
    sample.memory.put16(game_data + 2, 2);
    sample.memory.put16(game_data + 20, 2);
    menu::NameScratch scratch;
    for (std::size_t i = 0; i < scratch.size(); ++i)
        scratch[i] = static_cast<std::uint8_t>(0xc0 + i);
    menu::decode_names(sample.context, scratch);
    const auto name0 = sample.memory.bytes(game_data, 20);
    check(name0[0] == 'a' && name0[1] == 'b' && name0[2] == 0 && name0[3] == 0xc3 &&
              name0[19] == 0xd3,
          "the buffer's earlier bytes follow the first name");
    const auto name1 = sample.memory.bytes(game_data + 20, 20);
    check(name1[0] == 'b' && name1[1] == 0 && name1[2] == 0 && name1[3] == 0xc3,
          "later names keep the previous name's bytes");
    check(scratch[0] == 0 && scratch[3] == 0xc3, "the buffer returns to the caller");
}

void test_serialize() {
    Sample sample;
    auto &memory = sample.memory;
    for (std::uint32_t i = 0; i < 0x2358; ++i)
        memory.put8(game_data + i, (i * 7 + 3) & 0xff);
    for (std::uint32_t i = 0; i < menu::name_count; ++i)
        put_name(sample, i,
                 i % 2 == 0 ? "ab"
                            : "\x90\x41"
                              "a");
    memory.put8(party + 0x30, 0);
    memory.put8(party + 0x31, 0xff);
    memory.put8(party + 0x32, 2);
    for (std::uint32_t i = 0; i < 0x20; ++i)
        memory.put8(menu::saved_globals + i, 0x60 + i);
    menu::NameScratch scratch{};
    scratch.fill(0x77);
    const auto before = std::vector<std::uint8_t>(memory.regions[game_data]);
    menu::serialize(sample.context, payload, 5, 2, 0x01020304, scratch);
    const auto p = [&](std::uint32_t offset) { return memory.u8(payload + offset); };

    check(memory.u32(payload) == 0x01020304, "play counter at payload 0");
    const auto record0 = game_data + 0x26c;
    const auto record2 = record0 + 2 * 0xa4;
    check(p(0x1c) == 0 && p(0x1d) == 0xff && p(0x1e) == 2, "party ids");
    check(memory.u16(payload + 4) == memory.u16(record0 + 0x4c) &&
              memory.u16(payload + 8) == memory.u16(record2 + 0x4c),
          "party +4c words");
    check(memory.u16(payload + 0xa) == memory.u16(record0 + 0x4e), "party +4e words");
    check(p(0x10) == memory.u8(record0 + 0x50) && p(0x15) == memory.u8(record2 + 0x52),
          "party +50/+52 bytes");
    check(p(0x16) == memory.u8(record0 + 0x62) && p(0x1b) == memory.u8(record2 + 0x63),
          "party +62/+63 bytes");
    check(memory.u16(payload + 6) == 0 && p(0x11) == 0, "empty party slot stays zero");
    check(p(0x1f) == 0 && p(0x23) == 5, "summary byte 1f and file digit");

    // Stored names are the encoded ones; the game data gets them decoded back.
    check(memory.u16(payload + 0x24) == 1 && memory.u16(payload + 0x26) == 2 &&
              memory.u16(payload + 0x28) == 0,
          "name 0 stored encoded");
    check(memory.u16(payload + 0x24 + 20) == 3 && memory.u16(payload + 0x24 + 22) == 1,
          "name 1 stored encoded");
    check(memory.u8(game_data) == 'a' && memory.u8(game_data + 2) == 0 &&
              memory.u8(game_data + 3) == 0x77,
          "names decoded back with the caller's stack bytes");
    check(memory.u8(game_data + 20) == lead && memory.u8(game_data + 22) == 'a' &&
              memory.u8(game_data + 23) == 0 && memory.u8(game_data + 24) == 0x77,
          "later names keep the buffer's tail");
    for (std::uint32_t i = 0; i < 0x20; ++i)
        check(memory.u8(game_data + 0x2324 + i) == 0x60 + i, "globals copied to game + 2324");
    check(memory.u16(game_data + 0x19d4) == 1, "disc word is disc - 1");
    check(memory.u16(payload + 0xe4c + 0x19d4 - 0x1648) == 1, "stored disc word");
    for (std::uint32_t i = 0x26c; i < 0x978; ++i)
        check(p(0x24 + i) == before[i], "character records at payload 290");
    for (std::uint32_t j = 0; j < menu::gear_count; ++j) {
        const auto stored = 0x99c + j * 0x3c;
        const auto r = 0x978 + j * 0xa4;
        for (std::uint32_t i = 0; i < 0x28; ++i)
            check(p(stored + i) == before[r + i], "gear prefix");
        check(p(stored + 0x28) == before[r + 0x5c] && p(stored + 0x2c) == before[r + 0x60] &&
                  p(stored + 0x34) == before[r + 0x38] && p(stored + 0x38) == before[r + 0x99] &&
                  p(stored + 0x39) == before[r + 0x74] && p(stored + 0x3a) == before[r + 0x75],
              "gear fields");
        check(p(stored + 0x30) == 0 && p(stored + 0x3b) == 0, "unstored gear bytes zero");
    }
    for (std::uint32_t i = 0x1648; i < 0x2358; ++i)
        if (i < 0x19d4 || i >= 0x19d6)
            if (i < 0x2324 || i >= 0x2344)
                check(p(0xe4c + i - 0x1648) == before[i], "tail at payload e4c");

    memory.put8(menu::disc_override, 1);
    menu::serialize(sample.context, payload, 5, 2, 0x01020304, scratch);
    check(memory.u16(game_data + 0x19d4) == 1 && memory.u16(payload + 0xe4c + 0x38c) == 1,
          "override stores disc word 1");
    memory.put8(menu::disc_override, 0);
    menu::serialize(sample.context, payload, 5, 0, 0x01020304, scratch);
    check(memory.u16(game_data + 0x19d4) == 0xffff, "disc 0 wraps");

    const auto sum = menu::seal_payload(sample.context, payload);
    std::uint32_t total = 0;
    for (std::uint32_t i = 0; i < menu::checksum_offset; ++i)
        total += p(i);
    const auto expected = total & 0xff;
    check(sum == total && total > 0xff && p(menu::checksum_offset) == expected,
          "seal stores the low byte and returns the whole sum");

    for (std::uint32_t i = 0; i < 0x100; ++i)
        memory.put8(card + menu::card_header + i, i);
    const auto block = menu::save_file_block(sample.context, payload);
    check(block.size() == menu::file_bytes && block[0x10] == 0x10 && block[0x100 + 0x23] == 5 &&
              block[0x1fff] == expected,
          "card block is header then payload");

    put_name(sample, 3, "abababababab"); // Twelve codes overrun the buffer.
    rejects([&] { menu::serialize(sample.context, payload, 5, 2, 0x01020304, scratch); },
            "long name", "overruns 801cba4c");
    put_name(sample, 3, "ab");
    auto name = memory.bytes(game_data + 4 * 20, 20);
    std::ranges::fill(name, std::uint8_t{'a'});
    rejects([&] { menu::serialize(sample.context, payload, 5, 2, 0x01020304, scratch); },
            "unterminated name", "past its known source");
}

void test_check() {
    Sample sample;
    for (std::uint32_t i = 0; i < 0x2000; ++i)
        sample.memory.put8(buffer + i, (i * 5) & 0xff);
    std::uint8_t sum = 0;
    for (std::uint32_t i = 0x100; i < 0x1fff; ++i)
        sum = static_cast<std::uint8_t>(sum + ((i * 5) & 0xff));
    sample.memory.put8(buffer + 0x1fff, sum);
    auto result = menu::check_loaded(sample.context, buffer);
    check(result.sum == sum && result.stored == sum &&
              result.decision == menu::LoadDecision::accepted,
          "matching checksum accepted");
    sample.memory.put8(buffer + 0x10, 0); // Header bytes are not checked.
    check(menu::check_loaded(sample.context, buffer).decision == menu::LoadDecision::accepted,
          "header not checked");
    sample.memory.put8(buffer + 0x180, sample.memory.u8(buffer + 0x180) ^ 1);
    result = menu::check_loaded(sample.context, buffer);
    check(result.decision == menu::LoadDecision::checksum_mismatch && result.stored == sum &&
              result.sum != sum,
          "corrupt payload byte is a checksum mismatch");
}

void setup_tables(Sample &sample) {
    auto &m = sample.memory;
    m.put32(engines + 0x18 + 4, 0x12345678);
    for (std::uint32_t i = 0; i < 4; ++i)
        m.put8(engines + 0x18 + 0x14 + i, 0x11 * (i + 1));
    m.put16(frames + 0x10 + 6, 0x0203);
    m.put8(frames + 0x10 + 0xc, 9);
    m.put8(frames + 0x10 + 0xd, 8);
    m.put8(frames + 0x10 + 0xe, 7);
    m.put16(models + 0x14 + 8, 0x0a0b);
    m.put16(models + 0x14 + 10, 0x0c0d);
    const auto part = [](std::uint32_t index) { return parts + index * 0x1c; };
    // Part 1: sums and effect 9 (continues into effect 10).
    m.put8(part(1) + 0xd, 5);
    m.put8(part(1) + 0xe, 6);
    m.put16(part(1) + 6, 400);
    m.put8(part(1) + 0x18, 1);
    m.put8(part(1) + 0x14, 2);
    m.put8(part(1) + 0x1b, 3);
    for (std::uint32_t i = 0; i < 4; ++i)
        m.put8(part(1) + 0x10 + i, i + 1);
    m.put8(part(1) + 0x15, 9);
    m.put16(part(1) + 0x16, 0x0102);
    // Part 2: effect 4 with two bits.
    m.put16(part(2) + 6, 600);
    m.put8(part(2) + 0x15, 4);
    m.put16(part(2) + 0x16, 0xc000);
    m.put8(part(2) + 0x1a, 7);
    // Part 3: effect 5.
    m.put8(part(3) + 0x15, 5);
    m.put8(part(3) + 0x16, 1);
}

void test_restore() {
    Sample sample;
    auto &m = sample.memory;
    setup_tables(sample);
    for (std::uint32_t i = 0; i < menu::payload_bytes; ++i)
        m.put8(buffer + 0x100 + i, (i * 3 + 1) & 0xff);
    for (std::uint32_t j = 0; j < menu::gear_count; ++j) {
        const auto stored = buffer + 0x100 + 0x99c + j * 0x3c;
        m.put8(stored + 2, 1);
        m.put8(stored + 3, 1);
        m.put8(stored + 8, 1);
        m.put8(stored + 9, 1);
        m.put8(stored + 10, 2);
        m.put8(stored + 11, 3);
    }
    for (std::uint32_t i = 0; i < 0x2358; ++i)
        m.put8(game_data + i, (i * 13 + 101) & 0xff);
    m.put8(gear(0) + 0x75, 3); // Read by the level (801e4928) before +75 is restored.
    m.put8(gear(1) + 0x75, 20);
    const auto previous = std::vector<std::uint8_t>(m.regions[game_data]);
    const auto loaded = buffer + 0x100;
    menu::NameScratch scratch{};
    // Names: the first is code 1 ('a'), the rest empty.
    for (std::uint32_t i = 0; i < menu::name_count; ++i)
        for (std::uint32_t k = 0; k < 20; ++k)
            m.put8(loaded + 0x24 + i * 20 + k, 0);
    m.put8(loaded + 0x24, 1);
    std::uint32_t play_frames = 0;
    menu::apply_loaded(sample.context, loaded, play_frames, scratch);

    const auto p = [&](std::uint32_t offset) { return m.u8(loaded + offset); };
    check(play_frames == m.u32(loaded), "play counter restored");
    for (std::uint32_t i = 0; i < 0x20; ++i)
        check(m.u8(menu::saved_globals + i) == p(0xe4c + 0x2324 - 0x1648 + i),
              "globals restored from game + 2324");
    check(m.u8(game_data) == 'a' && m.u8(game_data + 1) == 0, "names decoded");
    for (std::uint32_t i = 0x26c; i < 0x978; ++i)
        check(m.u8(game_data + i) == p(0x24 + i), "characters restored");
    for (std::uint32_t i = 0x1648; i < 0x2358; ++i)
        if (i != 0x16c4 && i != 0x16c5 && i != 0x16da && i != 0x16db)
            check(m.u8(game_data + i) == p(0xe4c + i - 0x1648), "tail restored");
    const auto stored_word = [&](std::uint32_t offset) {
        return static_cast<std::uint32_t>(p(0xe4c + offset - 0x1648) | p(0xe4c + offset - 0x1647)
                                                                           << 8);
    };
    check(m.u16(game_data + 0x16c4) == (stored_word(0x16c4) & 0xfb6f), "pilot bits cleared");
    check(m.u16(game_data + 0x16da) == (stored_word(0x16da) | 0x8000), "effect 5 flag");
    for (std::uint32_t j = 0; j < menu::gear_count; ++j) {
        const auto r = gear(j);
        const auto stored = 0x99c + j * 0x3c;
        for (std::uint32_t i = 0; i < 0x28; ++i)
            check(m.u8(r + i) == p(stored + i), "stored gear prefix");
        check(m.u16(r + 0x38) == (p(stored + 0x34) | p(stored + 0x35) << 8) &&
                  m.u8(r + 0x99) == p(stored + 0x38) && m.u8(r + 0x75) == p(stored + 0x3a),
              "stored gear fields restored last");
        check(m.u8(r + 0x5a) == previous[r - game_data + 0x5a], "unstored gear byte kept");
        check(m.u32(r + 0x64) == 0x12345678, "engine +64");
        check(m.u8(r + 0x98) == 0x11 && m.u8(r + 0x9e) == 0x22 && m.u8(r + 0x9d) == 0x33 &&
                  m.u8(r + 0x9f) == 0x44,
              "engine bytes");
        check(m.u16(r + 0x70) == 0x0a0b && m.u16(r + 0x72) == 0x0c0d, "model words");
        check(m.u16(r + 0x3a) == 0x0203 && m.u8(r + 0x3f) == 7, "frame fields");
        check(m.u16(r + 0x40) == 5 && m.u16(r + 0x42) == 6 && m.u16(r + 0x44) == 1000, "part sums");
        check(m.u16(r + 0x48) == 0x0102 && m.u8(r + 0x56) == 2, "effect 9 into effect 10");
        check(m.u16(r + 0x6e) == 0xc000 && m.u8(r + 0x88) == 7 && m.u8(r + 0x8a) == 0,
              "effect 4 bits");
    }
    // The level uses the +75 it finds before the stored one lands.
    check(m.u8(gear(0) + 0x4a) == 2, "level (1000 / 120 - 3) / 2");
    check(m.u8(gear(1) + 0x4a) == 0, "negative level is zero");

    m.put8(buffer + 0x100 + 0x99c + 9, 9); // Part 9 is outside the table.
    rejects([&] { menu::restore_game_data(sample.context, loaded, tables); }, "part outside",
            "outside its owned regions");
}

void test_slots() {
    Sample sample;
    auto &m = sample.memory;
    // No files listed: nothing loadable.
    m.put8(card + 0x4fe4, 1);
    m.put8(card + 0x4fe5, 1);
    check(menu::card_presence(sample.context) == menu::CardPresence::present, "cards present");
    check(!menu::load_slot_valid(sample.context, 1), "cursor slot without files");
    check(menu::find_load_slot(sample.context, 1) == 0xff && m.u8(state + 0x4d8) == 2, "no data");
    // Port 1 lists a game file at entry 12 (slot 17); port 0 an other file.
    m.put8(card + 0x4f8b, 1);
    for (std::uint32_t e = 0; e < 0x20; ++e)
        m.put8(card + 0x4fae + e, 0xff);
    m.put8(card + 0x4fae + 3, 0);
    m.put8(card + 0x4fae + 0x12, 0);
    m.put8(card + 0x4f8e + 0x12, 1);
    check(menu::find_load_slot(sample.context, 1) == 17, "first loadable slot");
    m.put32(card + 0x4f7c, 17);
    m.put8(state + 0x4d8, 0);
    check(menu::load_slot_valid(sample.context, 1) && m.u8(state + 0x4d8) == 2, "cursor loadable");
    m.put8(card + 0x4fe5, 0);
    check(menu::find_load_slot(sample.context, 1) == 0xff, "absent port 1 is not searched");
    check(!menu::load_slot_valid(sample.context, 1), "cursor on an absent port");
    m.put8(card + 0x4fe4, 0);
    check(menu::card_presence(sample.context) == menu::CardPresence::no_card, "no card");
    rejects([&] { (void)menu::find_load_slot(sample.context, 2); }, "other slot modes",
            "load mode");
}

void test_names() {
    check(menu::file_name(0, 0) == "bu00:BASLUS-006640", "card 0 name");
    check(menu::file_name(1, 14) == "bu10:BASLUS-00664>", "card 1 digit 14");
    rejects([] { (void)menu::file_name(2, 0); }, "card 2", "port");
    rejects([] { (void)menu::file_name(0, 0xd0); }, "digit making a zero character",
            "name character");
    std::array<std::uint8_t, 16> used{};
    check(menu::choose_file_digit(used, 0xff) == 0, "first free digit");
    used[0] = used[1] = 1;
    check(menu::choose_file_digit(used, 0xff) == 2, "skips used digits");
    check(menu::choose_file_digit(used, 1) == 1, "overwrite keeps its digit");
    used.fill(1);
    used[15] = 0;
    check(menu::choose_file_digit(used, 0xff) == 0, "index 15 is not searched");
}
} // namespace

int main() {
    try {
        test_codec();
        test_decode_names();
        test_serialize();
        test_check();
        test_restore();
        test_slots();
        test_names();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "menu save reconstruction tests passed\n";
    return 0;
}
