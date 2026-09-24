#include "xem/reconstruction/field_disassembler.hpp"

#include <algorithm>
#include <format>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string>

namespace xem::reconstruction::field {
namespace {

// Handler addresses are in field overlay 38a1ce82 loaded at 8006faf0. Each
// length is the PC advance on the handler's completing path (extended lengths
// include the FE prefix); operand fields are the bytes the handler (or the
// helper it calls) reads relative to the PC. Rows marked "reviewed" in the
// Phase 1 disassembler report were read by hand, the rest were derived
// mechanically from single straight-line PC stores and cross-checked against
// the handlers' MIPS stores to actor +cc.
struct Row {
    InstructionSpec spec;
    [[nodiscard]] constexpr Row named(std::string_view name) const {
        auto result = *this;
        result.spec.name = name;
        return result;
    }
    [[nodiscard]] constexpr Row ends() const {
        auto result = *this;
        result.spec.flow = Flow::end;
        return result;
    }
    [[nodiscard]] constexpr Row stays() const {
        auto result = *this;
        result.spec.flow = Flow::stay;
        return result;
    }
    [[nodiscard]] constexpr Row when(std::uint8_t offset, std::uint8_t mask,
                                     std::uint8_t value) const {
        auto result = *this;
        result.spec.form_offset = offset;
        result.spec.form_mask = mask;
        result.spec.form_value = value;
        return result;
    }
    [[nodiscard]] constexpr Row extra(ExtraSuccessor kind, std::uint8_t offset) const {
        auto result = *this;
        result.spec.extra = kind;
        result.spec.extra_offset = offset;
        return result;
    }
    [[nodiscard]] constexpr Row also(std::uint8_t offset) const {
        return extra(ExtraSuccessor::always, offset);
    }
    [[nodiscard]] constexpr Row party_skip(std::uint8_t offset) const {
        return extra(ExtraSuccessor::party_slot, offset);
    }
    [[nodiscard]] constexpr Row zero_skip(std::uint8_t offset) const {
        return extra(ExtraSuccessor::zero, offset);
    }
    [[nodiscard]] constexpr Row jump_table() const { return extra(ExtraSuccessor::jump_table, 0); }
};

constexpr Row op(std::uint8_t opcode, std::uint32_t handler, std::uint8_t length,
                 std::initializer_list<OperandField> fields) {
    Row row{};
    row.spec.opcode = opcode;
    row.spec.handler = handler;
    row.spec.length = length;
    std::size_t i = 0;
    for (const auto &item : fields)
        row.spec.operands[i++] = item;
    return row;
}
constexpr OperandField field_of(std::uint8_t offset, OperandKind kind) { return {offset, kind}; }
constexpr OperandField byte(std::uint8_t offset) { return field_of(offset, OperandKind::byte); }
constexpr OperandField actor(std::uint8_t offset) { return field_of(offset, OperandKind::actor); }
constexpr OperandField half(std::uint8_t offset) { return field_of(offset, OperandKind::half); }
constexpr OperandField signed_half(std::uint8_t offset) {
    return field_of(offset, OperandKind::signed_half);
}
constexpr OperandField word(std::uint8_t offset) { return field_of(offset, OperandKind::word); }
constexpr OperandField variable(std::uint8_t offset) {
    return field_of(offset, OperandKind::variable);
}
constexpr OperandField immediate(std::uint8_t offset) {
    return field_of(offset, OperandKind::immediate);
}
constexpr OperandField target(std::uint8_t offset) { return field_of(offset, OperandKind::target); }
constexpr OperandField message(std::uint8_t offset) {
    return field_of(offset, OperandKind::message);
}
constexpr OperandField bit(std::uint8_t offset) {
    return field_of(offset, OperandKind::bit_reference);
}
constexpr OperandField sel(std::uint8_t offset, std::uint8_t select_bit, std::uint8_t flags) {
    return {offset, OperandKind::selected, select_bit, flags};
}

template <std::size_t Size>
constexpr std::array<InstructionSpec, Size> specs(const std::array<Row, Size> &rows) {
    std::array<InstructionSpec, Size> result{};
    for (std::size_t i = 0; i < Size; ++i)
        result[i] = rows[i].spec;
    return result;
}

// Primary table 800ae2a0. 13, fd and ff share 800a2fc0. FE (800869b8) is the
// extended prefix: it increments the PC and indexes 800ae6a0.
constexpr auto primary_table = specs(std::to_array<Row>({
    op(0x00, 0x800a1b70, 1, {}).named("end_slot").ends(),
    op(0x01, 0x800a1e74, 3, {target(1)}).named("jump").ends(),
    op(0x02, 0x800a1bd0, 8, {half(1), half(3), byte(5), target(6)}).named("branch_if_false"),
    op(0x03, 0x8009c104, 4, {message(1), byte(3)}),
    op(0x04, 0x800a1a8c, 1, {}).named("reset_idle_and_end").ends(),
    op(0x05, 0x800a17f4, 3, {target(1)}).named("call"),
    op(0x06, 0x800a1730, 5, {target(1)}).named("call_long"),
    op(0x07, 0x8009eb78, 3, {actor(1), byte(2)}),
    op(0x08, 0x8009ed68, 3, {actor(1), byte(2)}),
    op(0x09, 0x8009f0a0, 3, {actor(1), byte(2)}),
    op(0x0a, 0x8009533c, 4, {byte(1), target(2)}).named("call_zone"),
    op(0x0b, 0x800a1624, 3, {immediate(1)}),
    op(0x0c, 0x8009f5a8, 1, {}).named("loop_player_control").stays(),
    op(0x0d, 0x800a18b8, 1, {}).named("return").ends(),
    op(0x0e, 0x80092404, 1, {}),
    op(0x0f, 0x800923e4, 1, {}),
    op(0x10, 0x80098c00, 9, {byte(1), sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)})
        .when(1, 0xff, 0x00),
    op(0x10, 0x80098c00, 2, {byte(1)}),
    op(0x11, 0x80098c3c, 9, {byte(1), sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)})
        .when(1, 0xff, 0x00),
    op(0x11, 0x80098c3c, 4, {byte(1), immediate(2)}),
    op(0x12, 0x80093200, 9, {immediate(1), immediate(3), immediate(5), immediate(7)})
        .extra(ExtraSuccessor::short_form, 4),
    op(0x13, 0x800a2fc0, 1, {}),
    op(0x14, 0x80093c48, 1, {}),
    op(0x15, 0x80093c6c, 1, {}),
    op(0x16, 0x800a08b8, 3, {immediate(1)}),
    op(0x17, 0x8009e91c, 18,
       {sel(1, 0x80, 17), sel(3, 0x40, 17), sel(5, 0x20, 17), sel(7, 0x10, 17), sel(9, 0x08, 17),
        sel(11, 0x04, 17), sel(13, 0x02, 17), sel(15, 0x01, 17), byte(17)}),
    op(0x18, 0x8009e83c, 5, {byte(1), byte(2), byte(3), byte(4)}),
    op(0x19, 0x8009e4bc, 6, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5)}),
    op(0x1a, 0x8009e428, 2, {byte(1)}),
    op(0x1b, 0x8009e35c, 7, {sel(1, 0x80, 6), sel(3, 0x40, 6), byte(5), byte(6)}),
    op(0x1c, 0x8009e2c8, 4, {sel(1, 0x80, 3), byte(3)}),
    op(0x1d, 0x8009e248, 7, {signed_half(1), signed_half(3), signed_half(5)}),
    op(0x1e, 0x8009e208, 1, {}),
    op(0x1f, 0x8009e1a0, 2, {byte(1)}),
    op(0x20, 0x8009e10c, 3, {immediate(1)}),
    op(0x21, 0x8009e094, 3, {immediate(1)}),
    op(0x22, 0x8009df10, 1, {}),
    op(0x23, 0x8009e040, 1, {}),
    op(0x24, 0x8009ddec, 2, {actor(1)}),
    op(0x25, 0x8009de94, 2, {actor(1)}),
    op(0x26, 0x8009dd34, 3, {immediate(1)}).named("wait_countdown"),
    op(0x27, 0x8009dc4c, 2, {actor(1)}),
    op(0x28, 0x8009dbc8, 2, {actor(1)}),
    op(0x29, 0x8009dac4, 2, {actor(1)}).named("hide_actor"),
    op(0x2a, 0x8009da1c, 1, {}),
    op(0x2b, 0x8009da44, 1, {}),
    op(0x2c, 0x8009a130, 2, {byte(1)}),
    op(0x2d, 0x8009a024, 8, {actor(1), variable(2), variable(4), variable(6)}),
    op(0x2e, 0x80099fc4, 3, {half(1)}),
    op(0x2f, 0x80099ef8, 3, {variable(1)}),
    op(0x30, 0x80099f48, 3, {variable(1)}),
    op(0x31, 0x800961a0, 5, {half(1), target(3)}),
    op(0x32, 0x800961c8, 5, {half(1), target(3)}),
    op(0x33, 0x800961f0, 1, {}),
    op(0x34, 0x80096214, 5, {immediate(1), variable(3)}),
    op(0x35, 0x8009d9a4, 6, {variable(1), sel(3, 0x40, 5), byte(5)}).named("set_variable"),
    op(0x36, 0x8009d960, 3, {variable(1)}).named("set_variable_one"),
    op(0x37, 0x8009d91c, 3, {variable(1)}).named("set_variable_zero"),
    op(0x38, 0x8009d890, 6, {variable(1), sel(3, 0x40, 5), byte(5)}).named("add_variable"),
    op(0x39, 0x8009d804, 6, {variable(1), sel(3, 0x40, 5), byte(5)}).named("subtract_variable"),
    op(0x3a, 0x8009d644, 6, {variable(1), sel(3, 0x40, 5), byte(5)}),
    op(0x3b, 0x8009d408, 6, {variable(1), sel(3, 0x40, 5), byte(5)}),
    op(0x3c, 0x8009d340, 3, {variable(1)}).named("increment_variable"),
    op(0x3d, 0x8009d3a4, 3, {variable(1)}),
    op(0x3e, 0x8009d5b8, 6, {variable(1), sel(3, 0x40, 5), byte(5)}),
    op(0x3f, 0x8009d52c, 6, {variable(1), sel(3, 0x40, 5), byte(5)}),
    op(0x40, 0x8009d4a0, 6, {variable(1), sel(3, 0x40, 5), byte(5)}),
    op(0x41, 0x8009d2d0, 5, {variable(1), immediate(3)}),
    op(0x42, 0x8009d260, 5, {variable(1), immediate(3)}),
    op(0x43, 0x8009d198, 3, {variable(1)}),
    op(0x44, 0x80098184, 5, {immediate(1), immediate(3)}),
    op(0x45, 0x80097864, 8, {immediate(1), sel(3, 0x80, 7), immediate(5), byte(7)}),
    op(0x46, 0x80092808, 1, {}),
    op(0x47, 0x80092ea0, 6, {immediate(2), immediate(4)}),
    op(0x48, 0x80093cd0, 7, {half(1), variable(3), immediate(5)}),
    op(0x49, 0x80093d48, 8, {half(1), variable(3), immediate(5), byte(7)}),
    op(0x4a, 0x80099980, 6, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5)}),
    op(0x4b, 0x80098430, 8, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5), immediate(6)}),
    op(0x4c, 0x800979f0, 8, {half(1), half(3), byte(5), half(6)}),
    op(0x4d, 0x80097954, 10, {half(1), half(3), byte(5), half(6), immediate(8)}),
    op(0x4e, 0x80098370, 6, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5)}),
    op(0x4f, 0x80098274, 8, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5), immediate(6)}),
    op(0x50, 0x800977a4, 8, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5), sel(6, 0x20, 5)}),
    op(0x51, 0x800976a8, 10,
       {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5), sel(6, 0x20, 5), immediate(8)}),
    op(0x52, 0x800980fc, 2, {actor(1)}),
    op(0x53, 0x80098038, 4, {actor(1), immediate(2)}),
    op(0x54, 0x800975c0, 5, {actor(1), sel(2, 0x80, 4), byte(4)}),
    op(0x55, 0x8009749c, 7, {actor(1), sel(2, 0x80, 4), byte(4), immediate(5)}),
    op(0x56, 0x80093014, 10,
       {sel(1, 0x80, 9), sel(3, 0x40, 9), sel(5, 0x20, 9), sel(7, 0x10, 9), byte(9)}),
    op(0x57, 0x80099214, 2, {byte(1)}).when(1, 0x03, 0x03),
    op(0x57, 0x80099214, 11,
       {byte(1), sel(2, 0x80, 10), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10)}),
    op(0x58, 0x80094918, 4, {immediate(1), byte(3)}),
    op(0x59, 0x8009f4cc, 1, {}),
    op(0x5a, 0x8009524c, 1, {}),
    op(0x5b, 0x80095284, 1, {}).stays(),
    op(0x5c, 0x800a0228, 3, {immediate(1)}),
    op(0x5d, 0x8009a174, 2, {byte(1)}),
    op(0x5e, 0x8009a1ac, 1, {}),
    op(0x5f, 0x8009ad6c, 2, {byte(1)}),
    op(0x60, 0x8008fdd0, 1, {}),
    op(0x61, 0x8008fe2c, 8, {sel(1, 0x80, 7), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0x62, 0x8008ff04, 2, {actor(1)}),
    op(0x63, 0x8008ff90, 8, {sel(1, 0x80, 7), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0x64, 0x80090068, 1, {}),
    op(0x65, 0x800900c4, 8, {sel(1, 0x80, 7), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0x66, 0x8009019c, 2, {actor(1)}),
    op(0x67, 0x8009abfc, 4, {actor(1), immediate(2)}),
    op(0x68, 0x8009ac34, 4, {actor(1), immediate(2)}),
    op(0x69, 0x8009ac7c, 3, {immediate(1)}),
    op(0x6a, 0x8009acb4, 3, {immediate(1)}),
    op(0x6b, 0x8009ab5c, 3, {immediate(1)}),
    op(0x6c, 0x8009abac, 3, {immediate(1)}),
    op(0x6d, 0x8009a6ac, 8, {variable(1), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0x6e, 0x8009a768, 8, {variable(1), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0x6f, 0x8009a2a8, 2, {actor(1)}),
    op(0x70, 0x8009a1e4, 2, {byte(1)}),
    op(0x71, 0x80093568, 3, {immediate(1)}),
    op(0x72, 0x8008f724, 3, {immediate(1)}),
    op(0x73, 0x80086c34, 2, {byte(1)}).when(1, 0xff, 0x00),
    op(0x73, 0x80086c34, 8, {byte(1), immediate(2), immediate(4), immediate(6)})
        .when(1, 0xff, 0x01),
    op(0x73, 0x80086c34, 2, {byte(1)}).stays(),
    op(0x74, 0x8008f668, 3, {immediate(1)}),
    op(0x75, 0x8008f76c, 3, {immediate(1)}),
    op(0x76, 0x80093a68, 1, {}),
    op(0x77, 0x80093a98, 1, {}),
    op(0x78, 0x800973a4, 4, {byte(1), half(2)}),
    op(0x79, 0x80097264, 1, {}),
    op(0x7a, 0x800972ac, 1, {}),
    op(0x7b, 0x800969fc, 4, {sel(1, 0x80, 3), byte(3)}),
    op(0x7c, 0x80096f18, 4, {sel(1, 0x80, 3), byte(3)}),
    op(0x7d, 0x80097010, 4, {sel(1, 0x80, 3), byte(3)}),
    op(0x7e, 0x80097108, 4, {sel(1, 0x80, 3), byte(3)}),
    op(0x7f, 0x80095300, 3, {immediate(1)}),
    op(0x80, 0x80092664, 5, {byte(1), byte(2), immediate(3)}),
    op(0x81, 0x800926c8, 5, {byte(1), byte(2), immediate(3)}),
    op(0x82, 0x80093664, 5, {byte(1), byte(2), variable(3)}),
    op(0x83, 0x80092768, 5, {byte(1), byte(2), immediate(3)}),
    op(0x84, 0x80096644, 5, {immediate(1), target(3)}),
    op(0x85, 0x800966b4, 5, {immediate(1), target(3)}),
    op(0x86, 0x80096724, 5, {immediate(1), target(3)}).named("branch_variable_zero_unequal"),
    op(0x87, 0x80096790, 3, {immediate(1)}),
    op(0x88, 0x800967e8, 3, {variable(1)}),
    op(0x89, 0x80095e48, 6, {actor(1), immediate(2), target(4)}).named("branch_distance"),
    op(0x8a, 0x80095c00, 4, {actor(1), target(2)}),
    op(0x8b, 0x800962c0, 5, {immediate(1), target(3)}),
    op(0x8c, 0x8009631c, 3, {immediate(1)}),
    op(0x8d, 0x8009640c, 3, {immediate(1)}),
    op(0x8e, 0x80095f24, 7, {word(1), target(5)}),
    op(0x8f, 0x80095fb8, 3, {immediate(1)}),
    op(0x90, 0x8009601c, 3, {immediate(1)}),
    op(0x91, 0x800964b0, 4, {byte(1), target(2)}),
    op(0x92, 0x800a19b0, 1, {}).ends(),
    op(0x93, 0x800a1364, 3, {immediate(1)}),
    op(0x94, 0x800945d4, 5, {immediate(1), immediate(3)}),
    op(0x95, 0x80094650, 2, {byte(1)}),
    op(0x96, 0x8009468c, 1, {}),
    op(0x97, 0x8009a634, 3, {immediate(1)}),
    op(0x98, 0x800932d0, 5, {immediate(1), immediate(3)}),
    op(0x99, 0x8008fb98, 1, {}),
    op(0x9a, 0x8008fc4c, 3, {immediate(1)}).zero_skip(6),
    op(0x9b, 0x8008fd40, 5, {immediate(1), immediate(3)}),
    op(0x9c, 0x8009bb0c, 1, {}),
    op(0x9d, 0x8009a34c, 4, {immediate(1), byte(3)}),
    op(0x9e, 0x8009b9a0, 1, {}),
    op(0x9f, 0x8009ba0c, 1, {}),
    op(0xa0, 0x8009ba7c, 7, {immediate(1), immediate(3), immediate(5)}),
    op(0xa1, 0x8009a670, 3, {immediate(1)}),
    op(0xa2, 0x8009a58c, 2, {byte(1)}),
    op(0xa3, 0x80090228, 8, {sel(1, 0x80, 7), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0xa4, 0x8009a490, 4, {sel(1, 0x80, 3), byte(3)}),
    op(0xa5, 0x8009a534, 3, {variable(1)}),
    op(0xa6, 0x80097410, 3, {immediate(1)}).named("jump_table").ends().jump_table(),
    op(0xa7, 0x8009f5f4, 1, {}),
    op(0xa8, 0x8009d1f0, 5, {variable(1), immediate(3)}),
    op(0xa9, 0x8009bc98, 2, {byte(1)}),
    op(0xaa, 0x8009acec, 2, {byte(1)}),
    op(0xab, 0x80090300, 1, {}),
    op(0xac, 0x800903bc, 4, {byte(1), immediate(2)}),
    op(0xad, 0x80090b18, 7, {variable(1), variable(3), variable(5)}),
    op(0xae, 0x80090b9c, 7, {variable(1), variable(3), variable(5)}),
    op(0xaf, 0x80090c20, 4, {variable(1), byte(3)}),
    op(0xb0, 0x80090cb8, 4, {variable(1), byte(3)}),
    op(0xb1, 0x80090d50, 4, {variable(1), byte(3)}),
    op(0xb2, 0x8009a5e0, 2, {byte(1)}),
    op(0xb3, 0x8009731c, 3, {immediate(1)}),
    op(0xb4, 0x80097364, 3, {immediate(1)}),
    op(0xb5, 0x8009b8e4, 5, {immediate(1), immediate(3)}),
    op(0xb6, 0x8009b6ac, 5, {immediate(1), immediate(3)}),
    op(0xb7, 0x8009addc, 1, {}),
    op(0xb8, 0x8009ae0c, 1, {}),
    op(0xb9, 0x80096534, 4, {byte(1), target(2)}),
    op(0xba, 0x800965a8, 2, {byte(1)}),
    op(0xbb, 0x800965f4, 2, {byte(1)}),
    op(0xbc, 0x800a0d3c, 1, {}),
    op(0xbd, 0x80094a5c, 3, {immediate(1)}),
    op(0xbe, 0x80094acc, 3, {immediate(1)}),
    op(0xbf, 0x80094b3c, 3, {immediate(1)}),
    op(0xc0, 0x80094bac, 3, {immediate(1)}),
    op(0xc1, 0x80094c1c, 3, {immediate(1)}),
    op(0xc2, 0x80094c8c, 3, {immediate(1)}),
    op(0xc3, 0x800972f4, 1, {}),
    op(0xc4, 0x80093e30, 2, {byte(1)}),
    op(0xc5, 0x80093fc0, 2, {byte(1)}),
    op(0xc6, 0x800a1e9c, 1, {}),
    op(0xc7, 0x8009b824, 3, {immediate(1)}),
    op(0xc8, 0x8009b884, 3, {immediate(1)}),
    op(0xc9, 0x80095734, 4, {byte(1), target(2)}).named("branch_zone"),
    op(0xca, 0x8009a824, 8, {variable(1), sel(3, 0x40, 7), sel(5, 0x20, 7), byte(7)}),
    op(0xcb, 0x800958c0, 4, {byte(1), target(2)}),
    op(0xcc, 0x80095520, 4, {byte(1), target(2)}).named("call_zone_height"),
    op(0xcd, 0x8009da70, 1, {}),
    op(0xce, 0x8009da98, 1, {}),
    op(0xcf, 0x8009ce48, 5, {byte(1), byte(2), byte(3), byte(4)}),
    op(0xd0, 0x8009cee0, 11,
       {immediate(1), immediate(3), immediate(5), immediate(7), immediate(9)}),
    op(0xd1, 0x8009cf70, 1, {}).stays(),
    op(0xd2, 0x8009c0b4, 4, {message(1), byte(3)}),
    op(0xd3, 0x8009c0dc, 4, {message(1), byte(3)}),
    op(0xd4, 0x8009c01c, 5, {actor(1), message(2), byte(4)}).party_skip(6),
    op(0xd5, 0x80092628, 3, {half(1)}),
    op(0xd6, 0x800925a0, 3, {immediate(1)}),
    op(0xd7, 0x800946bc, 3, {immediate(1)}),
    op(0xd8, 0x80094710, 3, {immediate(1)}),
    op(0xd9, 0x80094764, 3, {immediate(1)}),
    op(0xda, 0x800921e8, 17,
       {half(1), half(3), half(5), half(7), half(9), half(11), half(13), half(15)}),
    op(0xdb, 0x80091f84, 5, {immediate(1), immediate(3)}),
    op(0xdc, 0x80092044, 5, {variable(1), variable(3)}),
    op(0xdd, 0x80091e00, 6, {sel(1, 0x80, 5), sel(3, 0x40, 5), byte(5)}),
    op(0xde, 0x8009d6d8, 6, {variable(1), sel(3, 0x40, 5), byte(5)}).named("multiply_variable"),
    op(0xdf, 0x8009d768, 6, {variable(1), sel(3, 0x40, 5), byte(5)}).named("divide_variable"),
    op(0xe0, 0x80091e98, 7, {actor(1), sel(2, 0x80, 6), sel(4, 0x40, 6), byte(6)}),
    op(0xe1, 0x80091bbc, 14,
       {sel(1, 0x80, 13), sel(3, 0x40, 13), sel(5, 0x20, 13), sel(7, 0x10, 13), sel(9, 0x08, 13),
        sel(11, 0x04, 13), byte(13)}),
    op(0xe2, 0x80096150, 5, {half(1), target(3)}),
    op(0xe3, 0x80096178, 5, {half(1), target(3)}),
    op(0xe4, 0x80091ad4, 1, {}).stays(),
    op(0xe5, 0x80091944, 17,
       {immediate(1), immediate(3), immediate(5), immediate(7), immediate(9), immediate(11),
        immediate(13), immediate(15)}),
    op(0xe6, 0x80091a08, 9, {signed_half(1), signed_half(3), signed_half(5), signed_half(7)}),
    op(0xe7, 0x80091a78, 7, {immediate(1), immediate(3), immediate(5)}),
    op(0xe8, 0x80094158, 7, {immediate(1), immediate(3), immediate(5)}),
    op(0xe9, 0x800943ac, 7, {immediate(1), immediate(3), immediate(5)}),
    op(0xea, 0x80092dfc, 6, {immediate(2), immediate(4)}),
    op(0xeb, 0x800910c0, 20,
       {sel(1, 0x80, 13), sel(3, 0x40, 13), sel(5, 0x20, 13), sel(7, 0x10, 13), sel(9, 0x08, 13),
        sel(11, 0x04, 13), byte(13), variable(14), variable(16), variable(18)}),
    op(0xec, 0x80091318, 15,
       {byte(1), sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8), variable(9),
        variable(11), variable(13)}),
    op(0xed, 0x800915c4, 8, {byte(1), variable(2), variable(4), variable(6)}),
    op(0xee, 0x80091720, 3, {byte(1), byte(2)}),
    op(0xef, 0x8008fa38, 3, {immediate(1)}),
    op(0xf0, 0x80090dec, 7, {variable(1), variable(3), variable(5)}),
    op(0xf1, 0x8008b248, 11,
       {immediate(1), immediate(3), immediate(5), immediate(7), immediate(9)}),
    op(0xf2, 0x8008f90c, 9, {immediate(1), immediate(3), immediate(5), immediate(7)}),
    op(0xf3, 0x80090e70, 7, {variable(1), variable(3), variable(5)}),
    op(0xf4, 0x8009be9c, 2, {byte(1)}),
    op(0xf5, 0x8009c12c, 4, {message(1), byte(3)}),
    op(0xf6, 0x8008e8c8, 2, {byte(1)}),
    op(0xf7, 0x8008e85c, 5, {immediate(1), immediate(3)}),
    op(0xf8, 0x8008e59c, 4, {byte(1), half(2)}),
    op(0xf9, 0x8008de64, 2, {actor(1)}),
    op(0xfa, 0x800947b0, 5, {byte(1), actor(2), immediate(3)}),
    op(0xfb, 0x8008d780, 5, {bit(1), target(3)}).named("branch_bit_clear"),
    op(0xfc, 0x8009bf8c, 5, {actor(1), message(2), byte(4)}).party_skip(6),
    op(0xfd, 0x800a2fc0, 1, {}),
    op(0xff, 0x800a2fc0, 1, {}),
}));

// Extended table 800ae6a0, entries 00..e2. Entries from e3 on are not handler
// addresses, so those opcodes are unknown.
constexpr auto extended_table = specs(std::to_array<Row>({
    op(0x00, 0x8008d2d8, 1, {}).stays(),
    op(0x01, 0x8009f424, 2, {}),
    op(0x02, 0x80095b3c, 5, {actor(2), target(3)}),
    op(0x03, 0x8008d0f4, 4, {immediate(2)}),
    op(0x04, 0x8008d26c, 4, {immediate(2)}),
    op(0x05, 0x80095cc4, 7, {actor(2), immediate(3), target(5)}),
    op(0x06, 0x80095d6c, 7, {actor(2), immediate(3), target(5)}),
    op(0x07, 0x8008d604, 3, {byte(2)}),
    op(0x08, 0x8008d180, 8, {immediate(2), immediate(4), immediate(6)}),
    op(0x09, 0x8008d078, 4, {immediate(2)}),
    op(0x0a, 0x8008d684, 4, {bit(2)}).named("set_variable_bit"),
    op(0x0b, 0x8008d700, 4, {bit(2)}),
    op(0x0c, 0x8008cfec, 14, {half(2), half(4), half(6), half(8), half(10), half(12)}),
    op(0x0d, 0x8008cf9c, 4, {immediate(2)}),
    op(0x0e, 0x8008c84c, 6, {immediate(2), immediate(4)}),
    op(0x0f, 0x8008c938, 7, {sel(2, 0x80, 6), sel(4, 0x40, 6), byte(6)}),
    op(0x10, 0x8008ca60, 6, {immediate(2), immediate(4)}),
    op(0x11, 0x8008cb4c, 7, {sel(2, 0x80, 6), sel(4, 0x40, 6), byte(6)}),
    op(0x12, 0x8008cc74, 4, {immediate(2)}),
    op(0x13, 0x8008cd48, 6, {immediate(2), immediate(4)}),
    op(0x14, 0x8008cdd4, 6, {immediate(2), immediate(4)}),
    op(0x15, 0x800a14f0, 6, {immediate(2), immediate(4)}),
    op(0x16, 0x8008c7d8, 2, {}),
    op(0x17, 0x8009aa00, 4, {actor(2), actor(3)}),
    op(0x18, 0x8008bdd8, 3, {byte(2)}).also(5),
    op(0x19, 0x8008c334, 3, {byte(2)}),
    op(0x1a, 0x8008b894, 2, {}),
    op(0x1b, 0x8008b5d4, 6, {signed_half(2), signed_half(4)}),
    op(0x1c, 0x80098a7c, 9, {sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)}),
    op(0x1d, 0x800984ec, 9, {sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)}),
    op(0x1e, 0x8009fb98, 3, {byte(2)}),
    op(0x1f, 0x8009fdd4, 2, {}),
    op(0x20, 0x8009fe4c, 3, {byte(2)}),
    op(0x21, 0x800a06e8, 4, {immediate(2)}),
    op(0x22, 0x8009b664, 4, {variable(2)}),
    op(0x23, 0x8009b398, 21,
       {sel(2, 0x80, 14), sel(4, 0x40, 14), sel(6, 0x20, 14), sel(8, 0x10, 14), sel(10, 0x08, 14),
        sel(12, 0x04, 14), byte(14), immediate(15), immediate(17), immediate(19)}),
    op(0x24, 0x8009b210, 2, {}),
    op(0x25, 0x8008d5c8, 3, {byte(2)}),
    op(0x26, 0x8008b2f0, 16,
       {immediate(2), immediate(4), immediate(6), immediate(8), immediate(10), immediate(12),
        immediate(14)}),
    op(0x27, 0x8008b328, 5, {byte(2), immediate(3)}).when(2, 0xff, 0x00),
    op(0x27, 0x8008b328, 3, {byte(2)}).when(2, 0xff, 0x01),
    op(0x27, 0x8008b328, 3, {byte(2)}).when(2, 0xff, 0x02),
    op(0x27, 0x8008b328, 3, {byte(2)}).when(2, 0xff, 0x03),
    op(0x27, 0x8008b328, 3, {byte(2)}).stays(),
    op(0x28, 0x8008e4ec, 4, {variable(2)}),
    op(0x29, 0x8008e518, 4, {variable(2)}),
    op(0x2a, 0x8008e544, 4, {variable(2)}),
    op(0x2b, 0x8008e570, 4, {variable(2)}),
    op(0x2c, 0x8008debc, 4, {variable(2)}),
    op(0x2d, 0x8008df44, 4, {variable(2)}),
    op(0x2e, 0x8008dfcc, 4, {variable(2)}),
    op(0x2f, 0x8008e054, 4, {variable(2)}),
    op(0x30, 0x8008e3e8, 6, {half(2), target(4)}),
    op(0x31, 0x8008e414, 6, {half(2), target(4)}),
    op(0x32, 0x8008e440, 6, {half(2), target(4)}),
    op(0x33, 0x8008e46c, 6, {half(2), target(4)}),
    op(0x34, 0x8008e298, 7, {half(2), actor(4), target(5)}),
    op(0x35, 0x8008e2ec, 7, {half(2), actor(4), target(5)}),
    op(0x36, 0x8008e340, 7, {half(2), actor(4), target(5)}),
    op(0x37, 0x8008e394, 7, {half(2), actor(4), target(5)}),
    op(0x38, 0x8008e1b4, 6, {variable(2), actor(4), actor(5)}),
    op(0x39, 0x8008d230, 4, {immediate(2)}),
    op(0x3a, 0x8008ced0, 4, {immediate(2)}),
    op(0x3b, 0x8008ce64, 4, {immediate(2)}),
    op(0x3c, 0x8008b180, 6, {immediate(2), immediate(4)}),
    op(0x3d, 0x8008aec8, 11,
       {sel(2, 0x80, 10), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10)}),
    op(0x3e, 0x8008afd8, 11,
       {sel(2, 0x80, 10), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10)}),
    op(0x3f, 0x8008b0e8, 8, {immediate(2), immediate(4), immediate(6)}),
    op(0x40, 0x80092148, 8, {immediate(2), immediate(4), half(6)}),
    op(0x41, 0x8009fc48, 4, {immediate(2)}),
    op(0x42, 0x8009fcac, 4, {immediate(2)}),
    op(0x43, 0x8009b15c, 2, {}),
    op(0x44, 0x8009b184, 2, {}),
    op(0x45, 0x8009a0fc, 3, {byte(2)}),
    op(0x46, 0x8008ae5c, 3, {byte(2)}),
    op(0x47, 0x8008b144, 4, {immediate(2)}),
    op(0x48, 0x8008b518, 9, {sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)}),
    op(0x49, 0x8008dafc, 2, {}),
    op(0x4a, 0x8008ace8, 4, {immediate(2)}),
    op(0x4b, 0x8008a9ac, 2, {}),
    op(0x4c, 0x8008a974, 3, {byte(2)}),
    op(0x4d, 0x8008a93c, 3, {byte(2)}),
    op(0x4e, 0x8008aa60, 2, {}),
    op(0x4f, 0x80093bb0, 2, {}),
    op(0x50, 0x80093bd4, 2, {}),
    op(0x51, 0x80093bfc, 2, {}),
    op(0x52, 0x80093c20, 2, {}),
    op(0x53, 0x80093ac8, 2, {}),
    op(0x54, 0x80093b10, 2, {}),
    op(0x55, 0x80093740, 2, {}),
    op(0x56, 0x80093930, 4, {immediate(2)}),
    op(0x57, 0x800937e0, 2, {}),
    op(0x58, 0x80093824, 4, {immediate(2)}),
    op(0x59, 0x800939a0, 4, {immediate(2)}),
    op(0x5a, 0x80093a04, 4, {immediate(2)}),
    op(0x5b, 0x8008b210, 4, {immediate(2)}),
    op(0x5c, 0x800a0fd8, 3, {byte(2)}).when(2, 0xff, 0x00),
    op(0x5c, 0x800a0fd8, 3, {byte(2)}).when(2, 0xff, 0x01),
    op(0x5c, 0x800a0fd8, 5, {byte(2), immediate(3)}).when(2, 0xff, 0x02),
    op(0x5c, 0x800a0fd8, 3, {byte(2)}).stays(),
    op(0x5d, 0x8008f6ac, 8, {immediate(2), immediate(4), immediate(6)}),
    op(0x5e, 0x8008f2d8, 4, {immediate(2)}),
    op(0x5f, 0x8008f1c8, 9, {byte(2), immediate(3), immediate(5), immediate(7)}),
    op(0x60, 0x8008ec30, 10, {immediate(2), immediate(4), immediate(6), immediate(8)}),
    op(0x61, 0x8008e9f8, 2, {}),
    op(0x62, 0x8008f444, 6, {immediate(2), immediate(4)}),
    op(0x63, 0x8008f4a0, 6, {immediate(2), immediate(4)}),
    op(0x64, 0x8008f5e4, 4, {immediate(2)}),
    op(0x65, 0x8008f4fc, 6, {immediate(2), immediate(4)}),
    op(0x66, 0x8008f558, 10, {immediate(2), immediate(4), immediate(6), immediate(8)}),
    op(0x67, 0x8008ee14, 20,
       {immediate(2), immediate(4), immediate(6), immediate(8), immediate(10), immediate(12),
        immediate(14), immediate(16), immediate(18)}),
    op(0x68, 0x80092c20, 7, {sel(2, 0x80, 6), sel(4, 0x40, 6), byte(6)}),
    op(0x69, 0x8008a6e0, 6, {variable(2), immediate(4)}),
    op(0x6a, 0x8008a604, 4, {immediate(2)}),
    op(0x6b, 0x8008a640, 6, {immediate(2), immediate(4)}),
    op(0x6c, 0x8008a5a0, 2, {}),
    op(0x6d, 0x8008fb28, 2, {}),
    op(0x6e, 0x8008fabc, 5, {sel(2, 0x80, 4), byte(4)}),
    op(0x6f, 0x8008b45c, 9, {sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)}),
    op(0x70, 0x80089f54, 4, {immediate(2)}),
    op(0x71, 0x8009899c, 4, {variable(2)}),
    op(0x72, 0x800988b8, 11,
       {variable(2), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10)}),
    op(0x73, 0x8009861c, 13,
       {variable(2), sel(4, 0x40, 12), sel(6, 0x20, 12), sel(8, 0x10, 12), sel(10, 0x08, 12),
        byte(12)}),
    op(0x74, 0x800985bc, 4, {variable(2)}),
    op(0x75, 0x800989f0, 5, {actor(2), variable(3)}),
    op(0x76, 0x80098738, 17,
       {variable(2), sel(4, 0x40, 16), sel(6, 0x20, 16), sel(8, 0x20, 16), sel(10, 0x10, 16),
        sel(12, 0x08, 16), sel(14, 0x08, 16), byte(16)}),
    op(0x77, 0x8008a2e8, 12,
       {byte(2), sel(5, 0x40, 11), sel(7, 0x20, 11), sel(9, 0x10, 11), byte(11)})
        .when(2, 0xff, 0x01),
    op(0x77, 0x8008a2e8, 3, {byte(2)}),
    op(0x78, 0x8008a4f0, 1, {}).stays(),
    op(0x79, 0x8008a4e8, 1, {}).stays(),
    op(0x7a, 0x8008a4e0, 1, {}).stays(),
    op(0x7b, 0x8008a518, 1, {}).stays(),
    op(0x7c, 0x8008a500, 1, {}).stays(),
    op(0x7d, 0x8008a508, 1, {}).stays(),
    op(0x7e, 0x8008a510, 1, {}).stays(),
    op(0x7f, 0x8008a244, 2, {}).named("wait_battle_request"),
    op(0x80, 0x80089fd0, 16, {half(2), half(4), half(6), half(8), half(10), half(12), half(14)}),
    op(0x81, 0x8008a08c, 9, {sel(2, 0x80, 8), sel(4, 0x40, 8), sel(6, 0x20, 8), byte(8)}),
    op(0x82, 0x8008a148, 26,
       {immediate(2), immediate(4), immediate(6), immediate(8), immediate(10), immediate(12),
        immediate(14), immediate(16), immediate(18), immediate(20), immediate(22), immediate(24)}),
    op(0x83, 0x80092fb4, 4, {immediate(2)}),
    op(0x84, 0x800933f8, 10, {immediate(2), immediate(6), immediate(8)}),
    op(0x85, 0x8008a2a0, 4, {variable(2)}),
    op(0x86, 0x80089f94, 3, {byte(2)}),
    op(0x87, 0x800936e4, 2, {}),
    op(0x88, 0x80089bf0, 19,
       {sel(2, 0x80, 18), sel(4, 0x40, 18), sel(6, 0x20, 18), sel(8, 0x10, 18), sel(10, 0x08, 18),
        sel(12, 0x04, 18), sel(14, 0x02, 18), sel(16, 0x01, 18), byte(18)}),
    op(0x89, 0x80089dcc, 12,
       {sel(2, 0x80, 10), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10),
        actor(11)}),
    op(0x8a, 0x80089f18, 4, {immediate(2)}),
    op(0x8b, 0x80089b54, 4, {variable(2)}),
    op(0x8c, 0x8008f3d0, 8, {immediate(2), immediate(4), immediate(6)}),
    op(0x8d, 0x8008f394, 4, {immediate(2)}),
    op(0x8e, 0x8008f348, 6, {immediate(2), immediate(4)}),
    op(0x8f, 0x80088790, 9, {actor(2), immediate(3), immediate(5), immediate(7)}),
    op(0x90, 0x80089004, 10, {immediate(2), immediate(4), immediate(6), immediate(8)}),
    op(0x91, 0x80089174, 15,
       {sel(2, 0x80, 14), sel(4, 0x40, 14), sel(6, 0x20, 14), sel(8, 0x10, 14), sel(10, 0x08, 14),
        sel(12, 0x04, 14), byte(14)}),
    op(0x92, 0x80089374, 15,
       {sel(2, 0x80, 14), sel(4, 0x40, 14), sel(6, 0x20, 14), sel(8, 0x10, 14), sel(10, 0x08, 14),
        sel(12, 0x04, 14), byte(14)}),
    op(0x93, 0x80089574, 12,
       {immediate(2), immediate(4), immediate(6), immediate(8), immediate(10)}),
    op(0x94, 0x800896d4, 11,
       {sel(2, 0x80, 10), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10)}),
    op(0x95, 0x80089880, 15,
       {sel(2, 0x80, 14), sel(4, 0x40, 14), sel(6, 0x20, 14), sel(8, 0x10, 14), sel(10, 0x08, 14),
        sel(12, 0x04, 14), byte(14)}),
    op(0x96, 0x80089a80, 2, {}),
    op(0x97, 0x80089ae4, 3, {byte(2)}),
    op(0x98, 0x800884cc, 4, {immediate(2)}),
    op(0x99, 0x8008848c, 3, {byte(2)}),
    op(0x9a, 0x8008f0b4, 10, {byte(2), actor(3), immediate(4), immediate(6), immediate(8)}),
    op(0x9b, 0x8008ef5c, 4, {immediate(2)}),
    op(0x9c, 0x8008efa0, 4, {immediate(2)}),
    op(0x9d, 0x8008f070, 4, {immediate(2)}),
    op(0x9e, 0x8008efe4, 10, {immediate(2), immediate(4), immediate(6), immediate(8)}),
    op(0x9f, 0x800883d4, 5, {byte(2), immediate(3)}),
    op(0xa0, 0x8008ea58, 13,
       {sel(2, 0x80, 12), sel(4, 0x40, 12), sel(6, 0x20, 12), sel(8, 0x10, 12), sel(10, 0x08, 12),
        byte(12)}),
    op(0xa1, 0x80088360, 6, {immediate(2), immediate(4)}),
    op(0xa2, 0x8008825c, 2, {}).named("wait_music_load"),
    op(0xa3, 0x800881e8, 3, {byte(2)}),
    op(0xa4, 0x80088198, 2, {}),
    op(0xa5, 0x80088c1c, 8, {immediate(2), immediate(4), immediate(6)}),
    op(0xa6, 0x800888a4, 6, {immediate(2), immediate(4)}),
    op(0xa7, 0x800889bc, 10, {immediate(2), immediate(4), immediate(6), immediate(8)}),
    op(0xa8, 0x80090a10, 8, {variable(2), variable(4), variable(6)}),
    op(0xa9, 0x80090a94, 8, {variable(2), variable(4), variable(6)}),
    op(0xaa, 0x8008db2c, 3, {actor(2)}),
    op(0xab, 0x8008dc74, 5, {sel(2, 0x80, 4), byte(4)}),
    op(0xac, 0x8008dd6c, 5, {sel(2, 0x80, 4), byte(4)}),
    op(0xad, 0x80096b58, 5, {half(2), byte(4)}),
    op(0xae, 0x80096af4, 8, {immediate(2), immediate(4), immediate(6)}),
    op(0xaf, 0x8008800c, 19,
       {sel(2, 0x80, 12), sel(4, 0x40, 12), sel(6, 0x20, 12), sel(8, 0x10, 12), sel(10, 0x08, 12),
        byte(12), variable(13), variable(15), variable(17)}),
    op(0xb0, 0x8008aacc, 3, {byte(2)}).when(2, 0xff, 0x01),
    op(0xb0, 0x8008aacc, 7, {byte(2), immediate(3), immediate(5)}),
    op(0xb1, 0x80087fd4, 2, {}),
    op(0xb2, 0x80096d28, 5, {byte(2), immediate(3)}),
    op(0xb3, 0x80096e20, 5, {byte(2), immediate(3)}),
    op(0xb4, 0x80096c40, 5, {half(2), byte(4)}),
    op(0xb5, 0x80087fa4, 2, {}),
    op(0xb6, 0x80087e98, 3, {actor(2)}),
    op(0xb7, 0x80087e5c, 4, {immediate(2)}),
    op(0xb8, 0x80087de0, 5, {byte(2), immediate(3)}),
    op(0xb9, 0x80087b5c, 10, {variable(2), variable(4), variable(6), variable(8)}),
    op(0xba, 0x80087c34, 11,
       {sel(2, 0x80, 10), sel(4, 0x40, 10), sel(6, 0x20, 10), sel(8, 0x10, 10), byte(10)}),
    op(0xbb, 0x80087d30, 4, {variable(2)}),
    op(0xbc, 0x80087d80, 5, {sel(2, 0x80, 4), byte(4)}),
    op(0xbd, 0x80088b68, 8, {immediate(2)}),
    op(0xbe, 0x80087c0c, 2, {}),
    op(0xbf, 0x80087848, 14,
       {immediate(2), immediate(4), immediate(6), immediate(8), immediate(10), immediate(12)}),
    op(0xc0, 0x80087800, 4, {variable(2)}),
    op(0xc1, 0x80088508, 8, {variable(2), variable(4), immediate(6)}),
    op(0xc2, 0x80088674, 10, {immediate(2), immediate(4), immediate(6), immediate(8)}),
    op(0xc3, 0x8009e014, 2, {}),
    op(0xc4, 0x8009df78, 3, {actor(2)}),
    op(0xc5, 0x80086f7c, 6, {immediate(2), immediate(4)}),
    op(0xc6, 0x8008bc80, 4, {immediate(2)}).also(6),
    op(0xc7, 0x800882b8, 6, {immediate(2), variable(4)}),
    op(0xc8, 0x80088cf8, 19,
       {sel(2, 0x80, 18), sel(4, 0x40, 18), sel(6, 0x20, 18), sel(8, 0x10, 18), sel(10, 0x08, 18),
        sel(12, 0x04, 18), sel(14, 0x02, 18), sel(16, 0x01, 18), byte(18)}),
    op(0xc9, 0x80088d18, 19,
       {sel(2, 0x80, 18), sel(4, 0x40, 18), sel(6, 0x20, 18), sel(8, 0x10, 18), sel(10, 0x08, 18),
        sel(12, 0x04, 18), sel(14, 0x02, 18), sel(16, 0x01, 18), byte(18)}),
    op(0xca, 0x800a0ee8, 3, {byte(2)}).when(2, 0xff, 0x00),
    op(0xca, 0x800a0ee8, 3, {byte(2)}).when(2, 0xff, 0x01),
    op(0xca, 0x800a0ee8, 3, {byte(2)}).stays(),
    op(0xcb, 0x800a0eb0, 2, {}),
    op(0xcc, 0x800a0e54, 2, {}),
    op(0xcd, 0x800a0dfc, 4, {variable(2)}),
    op(0xce, 0x800a0dc0, 4, {immediate(2)}),
    op(0xcf, 0x80093888, 6, {immediate(2), immediate(4)}),
    op(0xd0, 0x8008764c, 6, {immediate(2), immediate(4)}),
    op(0xd1, 0x8008754c, 2, {}),
    op(0xd2, 0x8008752c, 4, {}),
    op(0xd3, 0x80087420, 18,
       {immediate(2), immediate(4), immediate(6), immediate(8), immediate(10), immediate(12),
        variable(14), variable(16)}),
    op(0xd4, 0x80086fd0, 3, {byte(2)}).when(2, 0xff, 0x00),
    op(0xd4, 0x80086fd0, 11, {byte(2), immediate(3), immediate(5), immediate(7), immediate(9)})
        .when(2, 0xff, 0x01),
    op(0xd4, 0x80086fd0, 3, {byte(2)}).when(2, 0xff, 0x02),
    op(0xd4, 0x80086fd0, 11, {byte(2), immediate(3), immediate(5), immediate(7), immediate(9)})
        .when(2, 0xff, 0x03),
    op(0xd4, 0x80086fd0, 3, {byte(2)}).stays(),
    op(0xd5, 0x80087960, 6, {variable(2), variable(4)}),
    op(0xd6, 0x800879d0, 6, {variable(2), variable(4)}),
    op(0xd7, 0x80087ab8, 7, {half(2), half(4)}),
    op(0xd8, 0x80087a40, 3, {byte(2)}),
    op(0xd9, 0x80087a7c, 3, {byte(2)}),
    op(0xda, 0x80093790, 2, {}),
    op(0xdb, 0x80097200, 4, {immediate(2)}),
    op(0xdc, 0x800873c4, 6, {immediate(2), immediate(4)}),
    op(0xdd, 0x800871b0, 7, {byte(2), immediate(3), immediate(5)}).when(2, 0xff, 0x00),
    op(0xdd, 0x800871b0, 7, {byte(2), immediate(3), immediate(5)}).when(2, 0xff, 0x01),
    op(0xdd, 0x800871b0, 3, {byte(2)}).when(2, 0xff, 0x02),
    op(0xdd, 0x800871b0, 3, {byte(2)}).when(2, 0xff, 0x03),
    op(0xdd, 0x800871b0, 3, {byte(2)}).stays(),
    op(0xde, 0x80087148, 6, {immediate(2), immediate(4)}),
    op(0xdf, 0x80086e1c, 4, {immediate(2)}),
    op(0xe0, 0x80086de0, 3, {byte(2)}),
    op(0xe1, 0x80087580, 6, {immediate(2), immediate(4)}),
    op(0xe2, 0x80086d4c, 2, {}),
}));

template <std::size_t Size>
constexpr bool well_formed(const std::array<InstructionSpec, Size> &table) {
    for (std::size_t i = 0; i < Size; ++i) {
        const bool last = i + 1 == Size || table[i + 1].opcode != table[i].opcode;
        if (last != (table[i].form_offset == 0))
            return false; // Each opcode ends with exactly one default form
        if (i + 1 < Size && table[i + 1].opcode < table[i].opcode)
            return false;
    }
    return true;
}
static_assert(well_formed(primary_table) && well_formed(extended_table));
static_assert(primary_table.back().opcode == 0xff && extended_table.back().opcode == 0xe2);

template <std::size_t Size>
std::span<const InstructionSpec> forms_of(const std::array<InstructionSpec, Size> &table,
                                          std::uint8_t opcode) {
    const auto range = std::ranges::equal_range(table, opcode, {}, &InstructionSpec::opcode);
    return {range.begin(), range.end()};
}

std::size_t width(OperandKind kind) {
    switch (kind) {
    case OperandKind::none:
        return 0;
    case OperandKind::byte:
    case OperandKind::actor:
        return 1;
    case OperandKind::word:
        return 4;
    default:
        return 2;
    }
}

std::string describe(bool extended, std::uint8_t opcode, std::uint16_t pc) {
    return std::format("{} opcode {:02x} at PC {:04x}", extended ? "extended" : "primary", opcode,
                       pc);
}

} // namespace

std::span<const InstructionSpec> primary_forms(std::uint8_t opcode) noexcept {
    return forms_of(primary_table, opcode);
}

std::span<const InstructionSpec> extended_forms(std::uint8_t opcode) noexcept {
    return forms_of(extended_table, opcode);
}

UnknownInstruction::UnknownInstruction(std::uint16_t at, std::uint8_t value, bool is_extended)
    : EventError("Unknown " + describe(is_extended, value, at) +
                 (is_extended ? ": table 800ae6a0 holds no handler address for it" : "")),
      pc(at), opcode(value), extended(is_extended) {}

DisassemblyError::DisassemblyError(std::uint16_t at, const std::string &message)
    : EventError(message), pc(at) {}

Instruction decode_instruction(std::span<const std::uint8_t> bytecode, std::uint16_t pc) {
    const auto size = bytecode.size();
    if (pc >= size)
        throw DisassemblyError(pc, std::format("PC {:04x} outside the {}-byte bytecode", pc, size));
    Instruction result;
    result.pc = pc;
    result.extended = bytecode[pc] == 0xfe;
    if (result.extended && std::size_t{pc} + 1 >= size)
        throw DisassemblyError(pc, std::format("FE prefix at PC {:04x} ends the bytecode", pc));
    result.opcode = result.extended ? bytecode[pc + 1U] : bytecode[pc];
    const auto what = describe(result.extended, result.opcode, pc);
    const auto forms =
        result.extended ? extended_forms(result.opcode) : primary_forms(result.opcode);
    if (forms.empty())
        throw UnknownInstruction(pc, result.opcode, result.extended);
    for (const auto &form : forms) {
        if (form.form_offset == 0) {
            result.spec = &form;
            break;
        }
        if (std::size_t{pc} + form.form_offset >= size)
            throw DisassemblyError(pc, "Truncated form byte of " + what);
        if ((bytecode[pc + std::size_t{form.form_offset}] & form.form_mask) == form.form_value) {
            result.spec = &form;
            break;
        }
    }
    const auto &spec = *result.spec;
    // An extended handler that leaves the PC on its extended byte consumed only FE.
    result.length = result.extended && spec.flow == Flow::stay ? 1 : spec.length;
    if (std::size_t{pc} + result.length > size)
        throw DisassemblyError(pc,
                               std::format("Truncated operands of {}: {} bytes needed, {} remain",
                                           what, result.length, size - pc));
    const auto read = [&](std::size_t offset, std::size_t bytes) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < bytes; ++i)
            value |= static_cast<std::uint32_t>(bytecode[pc + offset + i]) << (8U * i);
        return value;
    };
    for (const auto &field : spec.operands) {
        if (field.kind == OperandKind::none || field.offset + width(field.kind) > result.length)
            continue;
        Operand operand{field, read(field.offset, width(field.kind)), false};
        if (field.kind == OperandKind::variable || field.kind == OperandKind::bit_reference)
            operand.variable = true;
        else if (field.kind == OperandKind::immediate)
            operand.variable = (operand.raw & 0x8000U) == 0;
        else if (field.kind == OperandKind::selected)
            operand.variable = (read(field.select_flags, 1) & field.select_bit) == 0;
        result.operands.push_back(operand);
    }

    std::vector<std::uint32_t> next;
    if (spec.flow == Flow::stay) {
        if (result.extended)
            next.push_back(pc + 1U);
    } else {
        if (spec.flow == Flow::next)
            next.push_back(pc + std::uint32_t{result.length});
        for (const auto &operand : result.operands)
            if (operand.field.kind == OperandKind::target)
                next.push_back(operand.raw);
        const auto first = [&] { return read(1, 2); };
        switch (spec.extra) {
        case ExtraSuccessor::none:
            break;
        case ExtraSuccessor::always:
            next.push_back(pc + std::uint32_t{spec.extra_offset});
            break;
        case ExtraSuccessor::short_form:
            result.short_successor = static_cast<std::uint16_t>(pc + spec.extra_offset);
            next.push_back(pc + std::uint32_t{spec.extra_offset});
            break;
        case ExtraSuccessor::party_slot:
            if (bytecode[pc + 1U] >= 0xfd)
                next.push_back(pc + std::uint32_t{spec.extra_offset});
            break;
        case ExtraSuccessor::zero:
            if ((first() & 0x8000U) == 0 || first() == 0x8000U)
                next.push_back(pc + std::uint32_t{spec.extra_offset});
            break;
        case ExtraSuccessor::jump_table:
            if ((first() & 0x8000U) == 0)
                result.unresolved = true;
            else
                next.push_back((pc + 3U + 3U * (first() & 0x7fffU)) & 0xffffU);
            break;
        }
    }
    for (const auto successor : next) {
        if (successor >= size)
            throw DisassemblyError(pc, std::format("Successor {:04x} of {} is outside the "
                                                   "{}-byte bytecode",
                                                   successor, what, size));
        const auto value = static_cast<std::uint16_t>(successor);
        if (std::ranges::find(result.successors, value) == result.successors.end())
            result.successors.push_back(value);
    }
    return result;
}

Disassembly disassemble_events(const EventProgram &program) {
    Disassembly result;
    std::vector<std::uint16_t> pending;
    for (std::size_t actor_index = 0; actor_index < program.entries.size(); ++actor_index) {
        for (std::size_t event = 0; event < 32; ++event) {
            const auto pc = program.entries[actor_index][event];
            if (pc == 0)
                continue;
            result.entries.push_back(
                {static_cast<std::uint16_t>(actor_index), static_cast<std::uint8_t>(event), pc});
            pending.push_back(pc);
        }
    }
    // PCs reached through a short form; their bytes also belong to the longer
    // instruction, so overlaps with them are original behavior.
    std::set<std::uint16_t> inner;
    const auto overlaps = [&](std::uint16_t pc, std::uint16_t length) {
        for (const auto &[at, other] : result.instructions) {
            if (inner.contains(pc) || inner.contains(at))
                continue;
            if (at < pc + std::size_t{length} && pc < at + std::size_t{other.length})
                return std::optional<std::uint16_t>{at};
        }
        return std::optional<std::uint16_t>{};
    };
    while (!pending.empty()) {
        const auto pc = pending.back();
        pending.pop_back();
        if (result.instructions.contains(pc))
            continue;
        auto instruction = decode_instruction(program.bytecode, pc);
        if (const auto other = overlaps(pc, instruction.length))
            throw DisassemblyError(pc, std::format("Instruction at {:04x} overlaps the "
                                                   "instruction at {:04x}",
                                                   pc, *other));
        if (instruction.short_successor)
            inner.insert(*instruction.short_successor);
        pending.insert(pending.end(), instruction.successors.begin(), instruction.successors.end());
        result.instructions.emplace(pc, std::move(instruction));
    }
    return result;
}

} // namespace xem::reconstruction::field
