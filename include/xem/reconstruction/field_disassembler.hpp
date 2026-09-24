#pragma once

#include "xem/reconstruction/field_events.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace xem::reconstruction::field {

// Static structure of field event bytecode, recovered from the original
// dispatch tables of field overlay 38a1ce82: primary handlers at 800ae2a0
// (indexed by 800a1ec8) and extended handlers at 800ae6a0 (indexed after the
// FE prefix by 800869b8). This describes lengths, operand fields and control
// flow only; executing handlers live in field_events and field_script.

enum class OperandKind : std::uint8_t {
    none,
    byte,          // u8
    actor,         // u8 actor selector (8009cdb4: ff/fe/fd party slots, fb current actor)
    half,          // raw u16
    signed_half,   // s16 immediate
    word,          // u32
    variable,      // u16 variable reference
    immediate,     // u16: bit 15 selects a 15-bit immediate, otherwise a variable (800acdec)
    selected,      // u16: a flag bit selects a signed immediate, otherwise a variable (8009cf78..)
    target,        // u16 absolute bytecode PC
    message,       // u16 dialogue message id (8009c5a8)
    bit_reference, // u16: variable reference >> 4, bit & 15
};

struct OperandField {
    std::uint8_t offset{}; // From the instruction's first byte (the FE prefix when extended)
    OperandKind kind = OperandKind::none;
    std::uint8_t select_bit{};   // selected: flag bit that makes the operand immediate
    std::uint8_t select_flags{}; // selected: offset of the flag byte
};

enum class Flow : std::uint8_t {
    next, // Continues after the instruction (and at any target operand)
    end,  // Continues only at target operands, if any
    stay, // Leaves the PC unchanged: a primary instruction repeats; an extended
          // one leaves the PC on its extended byte, which then dispatches as primary
};

// Additional successors that original handlers produce on some paths.
enum class ExtraSuccessor : std::uint8_t {
    none,
    always,     // PC + extra_offset on a runtime-selected path
    party_slot, // PC + extra_offset when the actor selector at +1 is ff/fe/fd
    zero,       // PC + extra_offset when the operand at +1 may be zero
    jump_table, // PC + 3 + 3 * operand at +1; unresolved for a variable operand
    short_form, // PC + extra_offset inside the instruction: on a runtime-selected
                // path the handler consumes only that prefix, and the following
                // bytes then dispatch as their own instruction
};

struct InstructionSpec {
    std::uint8_t opcode{};
    std::uint32_t handler{};
    std::uint8_t length{};
    std::array<OperandField, 12> operands{};
    std::string_view name;
    Flow flow = Flow::next;
    ExtraSuccessor extra = ExtraSuccessor::none;
    std::uint8_t extra_offset{};
    // A form applies when (byte at form_offset & form_mask) == form_value.
    // form_offset 0 marks the opcode's default form.
    std::uint8_t form_offset{};
    std::uint8_t form_mask{};
    std::uint8_t form_value{};
};

// All forms recovered for an opcode, most specific first. Primary FE (the
// extended prefix) has no own form. Extended opcodes above e2 have none: the
// original table holds no handler address there.
[[nodiscard]] std::span<const InstructionSpec> primary_forms(std::uint8_t opcode) noexcept;
[[nodiscard]] std::span<const InstructionSpec> extended_forms(std::uint8_t opcode) noexcept;

class UnknownInstruction : public EventError {
  public:
    UnknownInstruction(std::uint16_t at, std::uint8_t value, bool is_extended);
    std::uint16_t pc; // First byte of the instruction (the FE prefix when extended)
    std::uint8_t opcode;
    bool extended;
};

class DisassemblyError : public EventError {
  public:
    DisassemblyError(std::uint16_t at, const std::string &message);
    std::uint16_t pc;
};

struct Operand {
    OperandField field;
    std::uint32_t raw{};
    bool variable{}; // The operand names a variable rather than an immediate
};

struct Instruction {
    std::uint16_t pc{};
    bool extended{};
    std::uint8_t opcode{}; // The extended byte when extended
    const InstructionSpec *spec{};
    std::uint16_t length{};
    std::vector<Operand> operands;
    std::vector<std::uint16_t> successors;
    bool unresolved{}; // A successor depends on a runtime variable (jump table)
    // The successor inside this instruction when its handler can consume only
    // a prefix (ExtraSuccessor::short_form).
    std::optional<std::uint16_t> short_successor;
};

// Decodes one instruction. Throws UnknownInstruction for an extended opcode
// without a recovered handler and DisassemblyError for truncated operands or
// successors outside the bytecode.
[[nodiscard]] Instruction decode_instruction(std::span<const std::uint8_t> bytecode,
                                             std::uint16_t pc);

struct EventEntry {
    std::uint16_t actor{};
    std::uint8_t event{};
    std::uint16_t pc{};
};

struct Disassembly {
    std::vector<EventEntry> entries;
    std::map<std::uint16_t, Instruction> instructions;
};

// Follows every successor from each nonzero actor event entry. Entry value 0
// is not followed: it fills the unused event slots of field 23, whose first
// code starts at 0x16. The original never tests for 0; requesting such an
// event would run from offset 0. Overlapping instructions are rejected,
// except code reached through a short form, which the original executes from
// inside the longer instruction's bytes.
[[nodiscard]] Disassembly disassemble_events(const EventProgram &program);

} // namespace xem::reconstruction::field
