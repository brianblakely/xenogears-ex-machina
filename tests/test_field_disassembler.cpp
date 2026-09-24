// Invented bytecode exercises the recovered instruction lengths, operand
// fields and control flow. It describes no original game content.
#include "xem/reconstruction/field_disassembler.hpp"

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Call> Error expect(Call call, const char *message) {
    try {
        call();
    } catch (const Error &error) {
        return error;
    }
    throw std::runtime_error(message);
}
using Code = std::vector<std::uint8_t>;
std::vector<std::uint16_t> successors(const Code &code, std::uint16_t pc) {
    return field::decode_instruction(code, pc).successors;
}

void fixed_primary_and_operands() {
    // 35: set_variable var 0010 := immediate -5 (flag 40 at +5).
    const Code code{0x35, 0x10, 0x00, 0xfb, 0xff, 0x40, 0x00};
    const auto instruction = field::decode_instruction(code, 0);
    check(!instruction.extended && instruction.opcode == 0x35 && instruction.length == 6,
          "Primary 35 is six bytes");
    check(instruction.spec->handler == 0x8009d9a4 && instruction.spec->name == "set_variable",
          "Primary 35 names its original handler");
    check(instruction.operands.size() == 3, "Primary 35 has variable, selected and flag fields");
    check(instruction.operands[0].field.kind == field::OperandKind::variable &&
              instruction.operands[0].raw == 0x10 && instruction.operands[0].variable,
          "Destination is a variable reference");
    check(instruction.operands[1].field.kind == field::OperandKind::selected &&
              !instruction.operands[1].variable && instruction.operands[1].raw == 0xfffb,
          "Flag 40 selects an immediate source");
    check(instruction.successors == std::vector<std::uint16_t>{6}, "Primary 35 falls through");

    const Code variable_source{0x35, 0x10, 0x00, 0x20, 0x00, 0x00, 0x00};
    check(field::decode_instruction(variable_source, 0).operands[1].variable,
          "A clear flag selects a variable source");

    // 26 wait_countdown with a 15-bit immediate.
    const Code wait{0x26, 0x05, 0x80, 0x00};
    const auto countdown = field::decode_instruction(wait, 0);
    check(countdown.length == 3 && !countdown.operands[0].variable &&
              (countdown.operands[0].raw & 0x7fffU) == 5,
          "Bit 15 marks an immediate");
}

void control_flow() {
    // 02 branch: fallthrough and target; 01 jump: target only; 00: none.
    const Code code{0x02, 0x01, 0x00, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x01, 0x0c, 0x00, 0x00, 0x00};
    check(successors(code, 0) == std::vector<std::uint16_t>{8, 11}, "Branch has two successors");
    check(successors(code, 8) == std::vector<std::uint16_t>{12}, "Jump continues at its target");
    check(successors(code, 11).empty(), "End slot has no successor");

    // 05 call continues at its target and at the return point.
    const Code call{0x05, 0x04, 0x00, 0x00, 0x0d};
    check(successors(call, 0) == std::vector<std::uint16_t>{3, 4}, "Call returns after itself");
    check(successors(call, 4).empty(), "Return has no static successor");

    // 5b never advances; fe 00 leaves the PC on its 00 byte.
    const Code stall{0x5b, 0xfe, 0x00};
    check(successors(stall, 0).empty(), "Primary 5b repeats itself");
    const auto empty = field::decode_instruction(stall, 1);
    check(empty.extended && empty.length == 1 && empty.successors == std::vector<std::uint16_t>{2},
          "An empty extended handler dispatches its extended byte as primary");
}

void operand_selected_forms() {
    // 73: +1 == 0 is two bytes, 1 is eight, anything else never advances.
    const Code short_form{0x73, 0x00, 0x00};
    check(field::decode_instruction(short_form, 0).length == 2, "73 form 0 is two bytes");
    const Code long_form{0x73, 0x01, 0x01, 0x80, 0x02, 0x80, 0x03, 0x80, 0x00};
    const auto wide = field::decode_instruction(long_form, 0);
    check(wide.length == 8 && wide.operands.size() == 4, "73 form 1 carries three immediates");
    const Code idle{0x73, 0x07, 0x00};
    check(successors(idle, 0).empty(), "Other 73 forms never advance");

    // 57: low bits 3 is the two-byte continuation of an eleven-byte setup.
    const Code arc{0x57, 0x03, 0x00};
    check(field::decode_instruction(arc, 0).length == 2, "57 continuation is two bytes");

    // fe b0: FE+2 == 1 is three bytes, otherwise seven.
    const Code release{0xfe, 0xb0, 0x01, 0x00};
    check(field::decode_instruction(release, 0).length == 3, "fe b0 form 1 is three bytes");
    const Code load{0xfe, 0xb0, 0x02, 0x03, 0x80, 0x05, 0x00, 0x00};
    const auto loaded = field::decode_instruction(load, 0);
    check(loaded.length == 7, "Other fe b0 forms are seven bytes");
    // 8008aacc reads its operands through 800acdec(2) and (4), relative to the
    // extended byte: FE+3 and FE+5.
    check(loaded.operands.size() == 3 && loaded.operands[0].raw == 2 &&
              !loaded.operands[1].variable && (loaded.operands[1].raw & 0x7fffU) == 3 &&
              loaded.operands[2].variable && loaded.operands[2].raw == 5,
          "fe b0 operands follow the form byte");

    // fe 27 with an unrecognized form leaves the PC on its extended byte.
    const Code other{0xfe, 0x27, 0x09, 0x00};
    const auto stay = field::decode_instruction(other, 0);
    check(stay.length == 1 && stay.successors == std::vector<std::uint16_t>{1},
          "Unrecognized fe 27 forms stay on the extended byte");
}

void runtime_alternatives() {
    // fc with a party-slot selector may skip six bytes; a fixed actor cannot.
    const Code party{0xfc, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    check(successors(party, 0) == std::vector<std::uint16_t>{5, 6}, "Party selector adds PC+6");
    const Code fixed{0xfc, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    check(successors(fixed, 0) == std::vector<std::uint16_t>{5}, "Actor selector continues at 5");

    // 9a: a zero or variable operand may advance six bytes.
    const Code zero{0x9a, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00};
    check(successors(zero, 0) == std::vector<std::uint16_t>{3, 6}, "Zero operand adds PC+6");
    const Code nonzero{0x9a, 0x02, 0x80, 0x00};
    check(successors(nonzero, 0) == std::vector<std::uint16_t>{3}, "Nonzero immediate advances 3");

    // a6 jump table: resolved only for an immediate index.
    const Code table{0xa6, 0x01, 0x80, 0x01, 0x0c, 0x00, 0x01, 0x0c, 0x00, 0x01, 0x0c, 0x00, 0x00};
    check(successors(table, 0) == std::vector<std::uint16_t>{6}, "Immediate index selects entry");
    const Code indexed{0xa6, 0x01, 0x00, 0x00};
    const auto unresolved = field::decode_instruction(indexed, 0);
    check(unresolved.unresolved && unresolved.successors.empty(), "Variable index is unresolved");
}

void errors() {
    const Code unknown{0x36, 0x00, 0x00, 0xfe, 0xe5, 0x00};
    const auto error = expect<field::UnknownInstruction>(
        [&] { (void)field::decode_instruction(unknown, 3); }, "Extended e5 has no handler");
    check(error.pc == 3 && error.opcode == 0xe5 && error.extended, "Unknown error is precise");
    check(std::string(error.what()).find("extended opcode e5 at PC 0003") != std::string::npos,
          "Unknown error names namespace, opcode and PC");
    check(field::extended_forms(0xe3).empty() && !field::extended_forms(0xe2).empty(),
          "Extended table ends at e2");
    check(field::primary_forms(0xfe).empty(), "FE is only the extended prefix");

    const Code truncated{0x00, 0x35, 0x10, 0x00};
    const auto cut = expect<field::DisassemblyError>(
        [&] { (void)field::decode_instruction(truncated, 1); }, "Truncated operands are rejected");
    check(cut.pc == 1, "Truncation names the instruction PC");
    const Code prefix{0x00, 0xfe};
    (void)expect<field::DisassemblyError>([&] { (void)field::decode_instruction(prefix, 1); },
                                          "A final FE prefix is rejected");

    const Code far{0x01, 0x40, 0x00, 0x00};
    const auto jump = expect<field::DisassemblyError>(
        [&] { (void)field::decode_instruction(far, 0); }, "Out-of-range jump is rejected");
    check(jump.pc == 0 && std::string(jump.what()).find("0040") != std::string::npos,
          "Jump error names the target");
    const Code fallthrough{0x36, 0x00, 0x00};
    (void)expect<field::DisassemblyError>([&] { (void)field::decode_instruction(fallthrough, 0); },
                                          "Falling past the bytecode is rejected");
}

void reachable_package() {
    // Actor 0: entry 0 at 1 calls 7 and jumps back; entry 1 at 11. Unused entries are 0.
    const Code code{0xff, 0x05, 0x07, 0x00, 0x01, 0x0b, 0x00, 0x36, 0x02, 0x00, 0x0d, 0x00};
    std::array<std::array<std::uint16_t, 32>, 1> entries{};
    entries[0][0] = 1;
    entries[0][1] = 11;
    const field::EventProgram program{code, entries};
    const auto result = field::disassemble_events(program);
    check(result.entries.size() == 2, "Only nonzero entries are followed");
    std::vector<std::uint16_t> pcs;
    for (const auto &[pc, instruction] : result.instructions)
        pcs.push_back(pc);
    check(pcs == std::vector<std::uint16_t>{1, 4, 7, 10, 11}, "Calls, returns and jumps reached");

    // A jump into the middle of an earlier instruction is an overlap.
    const Code overlap{0xff, 0x35, 0x10, 0x00, 0x01, 0x80, 0x00, 0x01, 0x03, 0x00};
    std::array<std::array<std::uint16_t, 32>, 1> start{};
    start[0][0] = 1;
    const field::EventProgram bad{overlap, start};
    (void)expect<field::DisassemblyError>([&] { (void)field::disassemble_events(bad); },
                                          "Overlapping instructions are rejected");

    // 12 is nine bytes, but when its callee declines the handler consumes only
    // four and the next bytes dispatch as an instruction: that inner code is
    // reachable and is not an overlap error.
    const Code shortened{0xff, 0x12, 0x01, 0x80, 0x00, 0x00, 0x02, 0x80, 0x03, 0x80, 0x00};
    std::array<std::array<std::uint16_t, 32>, 1> twelve{};
    twelve[0][0] = 1;
    const auto walked = field::disassemble_events({shortened, twelve});
    check(walked.instructions.at(1).short_successor == std::uint16_t{5} &&
              walked.instructions.contains(5) && walked.instructions.contains(10),
          "Opcode 12's short form reaches the code inside it");

    // Unknown extended opcodes reached from an entry surface with their PC.
    const Code reach{0xff, 0x00, 0xfe, 0xf0};
    std::array<std::array<std::uint16_t, 32>, 1> into{};
    into[0][0] = 2;
    const field::EventProgram unknown{reach, into};
    const auto error = expect<field::UnknownInstruction>(
        [&] { (void)field::disassemble_events(unknown); }, "Reachable unknown opcode throws");
    check(error.pc == 2 && error.opcode == 0xf0, "Reachable unknown opcode is precise");
}

void table_invariants() {
    std::size_t primary = 0, extended = 0;
    for (unsigned opcode = 0; opcode < 256; ++opcode) {
        const auto code = static_cast<std::uint8_t>(opcode);
        for (const auto &form : field::primary_forms(code)) {
            ++primary;
            for (const auto &operand : form.operands)
                check(operand.kind == field::OperandKind::none || operand.offset > 0,
                      "Primary operands follow the opcode");
        }
        for (const auto &form : field::extended_forms(code)) {
            ++extended;
            for (const auto &operand : form.operands)
                check(operand.kind == field::OperandKind::none || operand.offset > 1,
                      "Extended operands follow the extended byte");
        }
    }
    check(primary >= 255 && extended >= 227, "Every table handler has a recovered form");
}
} // namespace

int main() {
    try {
        fixed_primary_and_operands();
        control_flow();
        operand_selected_forms();
        runtime_alternatives();
        errors();
        reachable_package();
        table_invariants();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "field disassembler checks passed\n";
    return 0;
}
