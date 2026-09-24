// Private analysis tool: prints the reachable event bytecode of one field.
// Input is the field's source slot bytes as read from the original disc; the
// output is original-derived and must stay local.
#include "xem/reconstruction/field_disassembler.hpp"
#include "xem/reconstruction/packed_field.hpp"

#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {

std::string operand_text(const field::Operand &operand) {
    using Kind = field::OperandKind;
    const auto raw = operand.raw;
    switch (operand.field.kind) {
    case Kind::byte:
        return std::format("{:02x}", raw);
    case Kind::actor:
        return std::format("actor:{:02x}", raw);
    case Kind::word:
        return std::format("{:08x}", raw);
    case Kind::signed_half:
        return std::format("{}", static_cast<std::int16_t>(raw));
    case Kind::target:
        return std::format("->{:04x}", raw);
    case Kind::message:
        return std::format("message:{:04x}", raw);
    case Kind::bit_reference:
        return std::format("bit:var{:04x}.{}", raw >> 4U, raw & 15U);
    case Kind::variable:
        return std::format("var{:04x}", raw);
    case Kind::immediate:
        return operand.variable ? std::format("var{:04x}", raw) : std::format("#{}", raw & 0x7fffU);
    case Kind::selected:
        return operand.variable ? std::format("var{:04x}", raw)
                                : std::format("#{}", static_cast<std::int16_t>(raw));
    case Kind::half:
    case Kind::none:
        break;
    }
    return std::format("{:04x}", raw);
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: xem-event-disassembler FIELD-SOURCE.bin\n";
        return 2;
    }
    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) {
        std::cerr << "Cannot open " << argv[1] << '\n';
        return 2;
    }
    const std::vector<std::uint8_t> source{std::istreambuf_iterator<char>(stream), {}};
    try {
        const auto components = field::decode_field_components(source);
        const auto package = field::parse_event_package(components[5].logical_data());
        const auto program = package.program();
        const auto result = field::disassemble_events(program);
        std::map<std::string, std::size_t> counts;
        std::size_t unresolved = 0;
        for (const auto &[pc, instruction] : result.instructions) {
            const auto key =
                std::format("{}{:02x}", instruction.extended ? "fe " : "", instruction.opcode);
            ++counts[key];
            unresolved += instruction.unresolved ? 1U : 0U;
            std::string bytes;
            for (std::size_t i = 0; i < instruction.length; ++i)
                bytes += std::format("{:02x}", program.bytecode[pc + i]);
            std::string operands;
            for (const auto &operand : instruction.operands)
                operands += ' ' + operand_text(operand);
            std::string next;
            for (const auto successor : instruction.successors)
                next += std::format(" {:04x}", successor);
            std::cout << std::format("{:04x}  {:<24} {:<6} {:08x} {:<22}{}  =>{}{}\n", pc, bytes,
                                     key, instruction.spec->handler, instruction.spec->name,
                                     operands, next, instruction.unresolved ? " (unresolved)" : "");
        }
        std::size_t covered = 0;
        for (const auto &[pc, instruction] : result.instructions)
            covered += instruction.length;
        std::cout << std::format("actors {} entries {} instructions {} covered bytes {}/{} "
                                 "unresolved {}\nopcodes {}:",
                                 package.entries.size(), result.entries.size(),
                                 result.instructions.size(), covered, program.bytecode.size(),
                                 unresolved, counts.size());
        for (const auto &[key, count] : counts)
            std::cout << std::format(" [{}]x{}", key, count);
        std::cout << "\nunknown opcodes 0\n";
    } catch (const field::UnknownInstruction &error) {
        std::cout << "unknown opcode: " << error.what() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cout << "error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
