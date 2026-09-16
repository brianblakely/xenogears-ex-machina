#include "xem/reconstruction/packed_field.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

// Private original-data bridge: stdout contains original-derived data. Callers
// must retain it locally. This executable invokes the reusable source directly;
// it contains no original payload or alternative decoder/parser implementation.
namespace field = xem::reconstruction::field;
namespace {

void hex(std::span<const std::uint8_t> bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::cout << '"';
    for (const auto byte : bytes)
        std::cout << digits[byte >> 4U] << digits[byte & 15U];
    std::cout << '"';
}

template <typename Range, typename Write> void array(const Range &values, Write write) {
    std::cout << '[';
    bool first = true;
    for (const auto &value : values) {
        if (!first)
            std::cout << ',';
        first = false;
        write(value);
    }
    std::cout << ']';
}

template <typename Range> void numbers(const Range &values) {
    array(values, [](auto value) { std::cout << value; });
}

void packed(const field::PackedBlock &block) {
    std::cout << "{\"data\":";
    hex(block.data);
    std::cout << ",\"token_bytes\":" << block.token_bytes
              << ",\"source_bytes_read\":" << block.source_bytes_read
              << ",\"groups\":" << block.groups << '}';
}

void events(const field::EventPackage &package) {
    std::cout << "{\"variable_unsigned_bits\":";
    hex(package.variable_unsigned_bits);
    std::cout << ",\"entries\":";
    array(package.entries, [](const auto &row) { numbers(row); });
    std::cout << ",\"bytecode\":";
    hex(package.bytecode);
    std::cout << ",\"bytecode_offset\":" << package.bytecode_offset << '}';
}

void collision(const field::CollisionPackage &package) {
    std::cout << "{\"layers\":";
    array(package.layers, [](const auto &layer) {
        std::cout << "{\"triangles\":";
        array(layer.triangles, [](const auto &triangle) {
            std::cout << "{\"vertices\":";
            numbers(triangle.vertices);
            std::cout << ",\"adjacent_raw\":";
            numbers(triangle.adjacent_raw);
            std::cout << ",\"attribute_raw\":" << triangle.attribute_raw << '}';
        });
        std::cout << ",\"vertices\":";
        array(layer.vertices, [](const auto &vertex) { numbers(vertex); });
        std::cout << '}';
    });
    std::cout << ",\"attributes_raw\":";
    hex(package.attributes_raw);
    std::cout << ",\"reserved_triangle_sizes\":";
    numbers(package.reserved_triangle_sizes);
    std::cout << '}';
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error(
                "Usage: packed-field-probe packed|field|events|collision INPUT");
        std::ifstream input(argv[2], std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open supplied input");
        const std::vector<std::uint8_t> source{std::istreambuf_iterator<char>(input),
                                               std::istreambuf_iterator<char>()};
        if (input.bad())
            throw std::runtime_error("Failed reading supplied input");
        const std::string_view mode(argv[1]);
        if (mode == "packed") {
            packed(field::decode_packed_block(source));
        } else if (mode == "field") {
            array(field::decode_field_components(source), [](const auto &part) {
                std::cout << "{\"index\":" << part.index
                          << ",\"source_offset\":" << part.source_offset
                          << ",\"logical_size\":" << part.logical_size << ",\"decoded\":";
                packed(part.decoded);
                std::cout << '}';
            });
        } else if (mode == "events") {
            events(field::parse_event_package(source));
        } else if (mode == "collision") {
            collision(field::parse_collision_package(source));
        } else {
            throw std::runtime_error("Unknown probe mode");
        }
        std::cout << '\n';
        if (!std::cout)
            throw std::runtime_error("Failed writing probe result");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
