#include "xem/reconstruction/packed_field.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

// Authored synthetic fixtures check representation, integration, and safe failure.
// Independent original decoder outputs are compared by packed-field-probe.
namespace field = xem::reconstruction::field;
namespace {
using Bytes = std::vector<std::uint8_t>;

void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

template <typename Error, typename Call> void rejects(Call call, const char *message) {
    try {
        call();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error(message);
}

void put16(Bytes &data, std::size_t offset, std::uint16_t value) {
    data.at(offset) = static_cast<std::uint8_t>(value);
    data.at(offset + 1) = static_cast<std::uint8_t>(value >> 8U);
}

void put32(Bytes &data, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        data.at(offset + i) = static_cast<std::uint8_t>(value >> (8U * i));
}

Bytes literal_block(const Bytes &data) {
    check(data.size() % 8 == 0, "Fixture needs complete literal groups");
    Bytes result(4);
    put32(result, 0, static_cast<std::uint32_t>(data.size()));
    for (std::size_t start = 0; start < data.size(); start += 8) {
        result.push_back(0);
        for (std::size_t byte = 0; byte < 8; ++byte)
            result.push_back(data[start + byte]);
    }
    result.push_back(0xa5);
    return result;
}

Bytes field_bundle(std::array<Bytes, 9> components) {
    Bytes result(0x154);
    for (std::size_t index = 0; index < components.size(); ++index) {
        auto &bytes = components[index];
        put32(result, 0x10c + index * 4, static_cast<std::uint32_t>(bytes.size()));
        put32(result, 0x130 + index * 4, static_cast<std::uint32_t>(result.size()));
        bytes.resize((bytes.size() + 7U) & ~std::size_t{7}, 0xcc);
        const auto packed = literal_block(bytes);
        result.insert(result.end(), packed.begin(), packed.end());
    }
    return result;
}

Bytes event_rows() {
    Bytes result(0x104 + 64);
    result[0] = 0x81;
    result[127] = 0xff;
    put32(result, 0x80, 2);
    for (std::uint16_t i = 0; i < 64; ++i) {
        put16(result, 0x84 + i * 2U, i);
        result[0x104 + i] = static_cast<std::uint8_t>(i);
    }
    return result;
}

Bytes collision_data() {
    Bytes result(116 + 3);
    const std::array<std::uint32_t, 10> header{2, 14, 14, 0x10203040, 0, 116, 40, 54, 78, 92};
    for (std::size_t i = 0; i < header.size(); ++i)
        put32(result, i * 4, header[i]);
    for (std::size_t layer = 0; layer < 2; ++layer) {
        const std::size_t triangle = 40 + layer * 38;
        const std::array<std::uint16_t, 7> fields{0, 1, 2, 0xffff, 0x8001, 0, 0xf234};
        for (std::size_t i = 0; i < fields.size(); ++i)
            put16(result, triangle + i * 2, fields[i]);
        const auto y = layer == 0 ? std::int16_t{-32768} : std::int16_t{32767};
        const std::array<std::int16_t, 12> vertices{-30, y, -20, -1, 30, y, 0, 9, 0, y, 40, 2};
        for (std::size_t i = 0; i < vertices.size(); ++i)
            put16(result, triangle + 14 + i * 2, std::bit_cast<std::uint16_t>(vertices[i]));
    }
    result[116] = 0x80;
    result[117] = 0xff;
    result[118] = 0x12;
    return result;
}

void decoder_groups_and_bounds() {
    const Bytes content{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};
    const auto literals = literal_block(content);
    const auto block = field::decode_packed_block(literals);
    check(block.data == content && block.token_bytes == 22 && block.source_bytes_read == 23 &&
              block.groups == 2,
          "Complete literal groups and terminal prefetch must retain source extents");
    const auto empty = field::decode_packed_block(Bytes{0, 0, 0, 0, 0xff});
    check(empty.data.empty() && empty.token_bytes == 4 && empty.source_bytes_read == 5 &&
              empty.groups == 0,
          "Zero output must still read one flag");
    const Bytes copy{10, 0, 0, 0, 2, 'a', 1, 0, 'B', 'C', 'D', 'E', 'F', 'G', 0xa5};
    check(field::decode_packed_block(copy).data ==
              Bytes{'a', 'a', 'a', 'a', 'B', 'C', 'D', 'E', 'F', 'G'},
          "Flags are LSB-first and overlapping copies consume their new bytes");
    for (std::size_t length = 0; length < copy.size(); ++length)
        rejects<field::PackedError>(
            [&] { (void)field::decode_packed_block(std::span(copy).first(length)); },
            "Every incomplete input including terminal prefetch must fail");
    rejects<field::PackedError>([&] { (void)field::decode_packed_block(literals, 15); },
                                "Output allocation limit must be enforced");
    for (const auto distance : {0U, 2U}) {
        auto changed = copy;
        changed[6] = static_cast<std::uint8_t>(distance);
        rejects<field::PackedError>([&] { (void)field::decode_packed_block(changed); },
                                    "Copies cannot reference absent or current bytes");
    }
    auto overlong = copy;
    overlong[7] = 0xf0;
    rejects<field::PackedError>([&] { (void)field::decode_packed_block(overlong); },
                                "Copy may not cross the declared output size");
    overlong = literal_block(Bytes(8));
    put32(overlong, 0, 7);
    rejects<field::PackedError>([&] { (void)field::decode_packed_block(overlong); },
                                "Partial final flag groups may not silently truncate");

    Bytes prefix(4096);
    for (std::size_t i = 0; i < prefix.size(); ++i)
        prefix[i] = static_cast<std::uint8_t>(i);
    auto maximum = literal_block(prefix);
    maximum.pop_back();
    put32(maximum, 0, 4121);
    const Bytes last_group{1, 0xff, 0xff, '1', '2', '3', '4', '5', '6', '7', 0};
    maximum.insert(maximum.end(), last_group.begin(), last_group.end());
    auto expected = prefix;
    expected.insert(expected.end(), prefix.begin() + 1, prefix.begin() + 19);
    expected.insert(expected.end(), last_group.begin() + 3, last_group.end() - 1);
    check(field::decode_packed_block(maximum).data == expected,
          "Distance 4095 and copy length 18 must preserve all bit fields");

    const Bytes disc{8, 0, 0, 0, 0, 'A', 'B', 'C', 'D', 0, 0, 0, 0, 0};
    const Bytes ram{8, 0, 0, 0, 0, 'A', 'B', 'C', 'D', 'w', 'x', 'y', 'z', 0xa5};
    check(field::decode_packed_block(disc).data != field::decode_packed_block(ram).data,
          "Supplied adjacent memory must remain distinct from disc padding");
}

void component_bounds() {
    std::array<Bytes, 9> bytes;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = Bytes(5, static_cast<std::uint8_t>(i));
    const auto source = field_bundle(bytes);
    const auto components = field::decode_field_components(source);
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto logical = components[i].logical_data();
        check(std::equal(logical.begin(), logical.end(), bytes[i].begin(), bytes[i].end()) &&
                  components[i].decoded.data.size() == 8 && components[i].decoded.data[7] == 0xcc,
              "Logical component bytes and decoder padding must have separate extents");
    }
    rejects<field::PackedError>(
        [&] { (void)field::decode_field_components(std::span(source).first(source.size() - 1)); },
        "Field parser cannot invent final component padding");
    for (const auto [offset, value] : std::array<std::pair<std::size_t, std::uint32_t>, 4>{
             {{0x130, 0}, {0x130, 0xffffffff}, {0x10c, 9}, {0x10c, 0xffffffff}}}) {
        auto changed = source;
        put32(changed, offset, value);
        rejects<field::FieldFormatError>([&] { (void)field::decode_field_components(changed); },
                                         "Invalid offset or logical size must fail");
    }
    auto changed = source;
    put32(changed, components[0].source_offset, 22); // Logical 5 + original 16-byte allowance.
    rejects<field::PackedError>([&] { (void)field::decode_field_components(changed); },
                                "Component allocation allowance must constrain packed size");
    for (std::size_t size = 0; size < 0x154; ++size)
        rejects<field::FieldFormatError>(
            [&] { (void)field::decode_field_components(std::span(source).first(size)); },
            "Incomplete header must fail before component reads");
}

void event_bounds_and_connection() {
    const auto source = event_rows();
    const auto events = field::parse_event_package(source);
    check(events.bytecode_offset == 0x104 && events.program().entry(0, 31) == 31 &&
              events.program().entry(1, 31) == 63 && events.variable_unsigned_bits[127] == 0xff,
          "Actor entry stride, bytecode base, and bitmap must be retained");
    for (std::size_t size = 0; size < source.size(); ++size)
        rejects<field::FieldFormatError>(
            [&] { (void)field::parse_event_package(std::span(source).first(size)); },
            "Truncated actor table or out-of-range entry must fail");
    auto malformed = source;
    put32(malformed, 0x80, 0xffffffff);
    rejects<field::FieldFormatError>([&] { (void)field::parse_event_package(malformed); },
                                     "Actor count arithmetic may not wrap");
    malformed = source;
    malformed.resize(0x104 + 0x10001);
    rejects<field::FieldFormatError>([&] { (void)field::parse_event_package(malformed); },
                                     "Bytecode beyond the u16 PC space must fail");
    malformed.resize(0x104 + 0x10000);
    put16(malformed, 0x84, 0xffff);
    check(field::parse_event_package(malformed).entries[0][0] == 0xffff,
          "The final representable PC is valid when bytecode has 65536 bytes");
    check(field::parse_event_package(Bytes(0x84)).entries.empty(),
          "Empty actor packages must not invent an actor");

    std::array<Bytes, 9> components;
    components[1] = collision_data();
    auto &program = components[5];
    program.resize(0xc4);
    program[0] = 1;
    put32(program, 0x80, 1);
    // A signed immediate assignment stores FFFF; the decoded bitmap selects unsigned readback.
    const Bytes code{0x35, 0, 0, 0xff, 0xff, 0x40, 0};
    program.insert(program.end(), code.begin(), code.end());
    const auto decoded = field::decode_field_components(field_bundle(components));
    const auto loaded = field::parse_event_package(decoded[5].logical_data());
    const auto geometry = field::parse_collision_package(decoded[1].logical_data());
    field::EventVariables variables;
    variables.unsigned_bitmap = loaded.variable_unsigned_bits;
    field::EventActor actor;
    field::EventContext context;
    context.program = loaded.program();
    context.variables = &variables;
    context.current_actor = &actor;
    actor.pc = context.program.entry(0, 0);
    const auto batch = field::run_event_batch(context, 8, field::execute_core_event);
    check(variables.read(0) == 65535 && actor.pc == 6 && batch.dispatched == 2 &&
              batch.reason == field::BatchExit::handler_break && geometry.layers.size() == 2,
          "Decoded event program must execute through the real core alongside parsed geometry");
}

void collision_bounds() {
    const auto source = collision_data();
    const auto collision = field::parse_collision_package(source);
    check(collision.layers.size() == 2 &&
              collision.layers[0].vertices[0] ==
                  std::array<std::int16_t, 4>{-30, -32768, -20, -1} &&
              collision.layers[1].vertices[0][1] == 32767 &&
              collision.layers[0].triangles[0].adjacent_raw ==
                  std::array<std::uint16_t, 3>{0xffff, 0x8001, 0} &&
              collision.layers[0].triangles[0].attribute_raw == 0xf234 &&
              collision.reserved_triangle_sizes == std::vector<std::uint32_t>{0x10203040, 0} &&
              collision.attributes_raw == Bytes{0x80, 0xff, 0x12},
          "Signed coordinates, all four sizes, and opaque collision values must survive parsing");
    for (const auto [offset, value] :
         std::array<std::pair<std::size_t, std::uint32_t>, 8>{{{0, 0},
                                                               {0, 5},
                                                               {4, 13},
                                                               {0x18, 0},
                                                               {0x1c, 53},
                                                               {0x20, 79},
                                                               {0x14, 0xffffffff},
                                                               {0x14, 84}}}) {
        auto changed = source;
        put32(changed, offset, value);
        rejects<field::FieldFormatError>([&] { (void)field::parse_collision_package(changed); },
                                         "Malformed collision regions must fail");
    }
    for (const auto index : {std::uint16_t{0xffff}, std::uint16_t{3}}) {
        auto changed = source;
        put16(changed, 40, index);
        rejects<field::FieldFormatError>([&] { (void)field::parse_collision_package(changed); },
                                         "Negative or out-of-range vertex indices must fail");
    }
    for (std::size_t size = 0; size < 116; ++size)
        rejects<field::FieldFormatError>(
            [&] { (void)field::parse_collision_package(std::span(source).first(size)); },
            "Truncated collision table must fail");
}
} // namespace

// Source and output in one address space (80032eb4 in RAM): input is read when
// reached, so output written ahead of the reader is decoded as written.
void decoder_in_memory() {
    constexpr std::uint32_t base = 0x80100000;
    Bytes memory(32, 0xee);
    put32(memory, 0, 8);
    memory[4] = 0x00; // Eight literals.
    for (std::uint8_t i = 0; i < 8; ++i)
        memory[5 + i] = static_cast<std::uint8_t>(0x10 + i);
    memory[13] = 0x00; // Flag read after the last group.
    const Bytes original = memory;

    auto separate = memory;
    const auto apart = field::decode_packed_in_memory(separate, base, base, base + 20);
    check(apart.source_bytes_read == 14 && apart.data.empty(),
          "Positions are reported and output stays in memory");
    for (std::size_t i = 0; i < 8; ++i)
        check(separate[20 + i] == 0x10 + i, "Disjoint output holds the literals");

    // Output starting one byte after the first literal overwrites each literal
    // just before it is read, so every output byte repeats the first.
    auto overlapped = memory;
    static_cast<void>(field::decode_packed_in_memory(overlapped, base, base, base + 6));
    for (std::size_t i = 0; i < 8; ++i)
        check(overlapped[6 + i] == 0x10, "Overlapping output is decoded as written");
    check(field::decode_packed_block(std::span(original).first(14)).data ==
              Bytes{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
          "The separate decoder reads the original input");

    auto outside = memory;
    rejects<field::PackedError>(
        [&] { (void)field::decode_packed_in_memory(outside, base, base, base + 28); },
        "Output beyond the supplied memory is rejected");
    rejects<field::PackedError>(
        [&] { (void)field::decode_packed_in_memory(outside, base, base - 4, base + 20); },
        "A source below the supplied memory is rejected");
}

int main() {
    try {
        decoder_groups_and_bounds();
        decoder_in_memory();
        component_bounds();
        event_bounds_and_connection();
        collision_bounds();
        std::cout << "Packed decoder, field structures, and connected event tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
