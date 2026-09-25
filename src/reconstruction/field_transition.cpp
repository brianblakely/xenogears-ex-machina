// Map changes: the departure record (field 800a30fc), the read-ahead of the
// requested map's data (resident 8001b484 and 8001b53c) and the main-loop
// step that starts a requested change (field 80077e88, 80078494..80078558).
// The reload itself (800a5c40) is not reconstructed and stops explicitly.
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::uint32_t half(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset + 2 > data.size())
        throw field::FieldFormatError("Game data read outside its extent");
    return static_cast<std::uint32_t>(data[offset] | data[offset + 1] << 8U);
}
void put_half(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value) {
    if (offset + 2 > data.size())
        throw field::FieldFormatError("Game data write outside its extent");
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("A map change requires an initialized field");
    return *program.field;
}
} // namespace

// 8009744c: the controlled actor's facing (+106) as an octant, offset by two.
std::int32_t Program::facing_octant() const {
    const auto &state = *field;
    const auto controlled = static_cast<std::size_t>(state.controlled_actor);
    if (controlled >= state.actors.size())
        throw field::FieldFormatError("Facing octant requires the controlled actor's record");
    const auto facing = static_cast<std::int16_t>(half(state.actors[controlled].storage, 0x106));
    return (((facing + 0x100) >> 9) + 2) & 7;
}

// 8009a514: the camera angle (800af98c) as a reversed octant.
std::int32_t Program::camera_heading_octant() const {
    return (7 - ((field->control_inputs.camera_angle - 0x100) >> 9)) & 7;
}

void Program::save_field_departure() {
    auto &state = loaded(*this);
    auto &data = resident.game_data;
    if (data.size() < game_data_bytes)
        throw field::FieldFormatError("The departure record requires the game data");
    // Both reads precede the stores; +1932 and +1938 are variables 2 and 8 of
    // the bank the previous save copied to +1930.
    const auto entry = half(data, 0x1932);
    const auto octant = half(data, 0x1938);
    put_half(data, 0x231a, resident.field_map);
    put_half(data, 0x2322, resident.music.requested);
    put_half(data, 0x2320, entry);
    put_half(data, 0x231c, octant << 9U);
    auto &variables = resident.variables;
    variables.write(0x44, resident.departure_5941c);
    variables.write(0x46, resident.departure_594d0);
    // After a teardown (a battle exit) the controlled actor's record is
    // freed memory, which the original still reads through the descriptor
    // table.
    if (static_cast<std::size_t>(state.controlled_actor) < state.actors.size()) {
        variables.write(6, facing_octant());
    } else {
        const auto descriptor = state.reload.descriptor_table +
                                static_cast<std::uint32_t>(state.controlled_actor) * 0x5cU;
        std::uint32_t record = 0;
        for (std::uint32_t i = 0; i < 4; ++i)
            record |= static_cast<std::uint32_t>(ram_byte(descriptor + 0x4c + i)) << (8U * i);
        const auto facing = static_cast<std::int16_t>(ram_byte(record + 0x106) |
                                                      ram_byte(record + 0x107) << 8U);
        variables.write(6, (((facing + 0x100) >> 9) + 2) & 7);
    }
    variables.write(8, camera_heading_octant());
    variables.write(0x24, static_cast<std::int16_t>(state.camera.elevation));
    variables.write(0x3c, static_cast<std::int32_t>(resident.field_map));
    // 800a30b4: the party's character ids.
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        variables.write(static_cast<std::uint16_t>(0x3e + slot * 2), state.party_characters[slot]);
    for (std::size_t i = 0; i < 0x200; ++i)
        put_half(data, 0x1930 + i * 2, variables.words[i]);
}

std::int32_t Program::preload_field(std::uint32_t id, std::uint32_t slot) {
    if (resident.preload_slot == slot && resident.preload_id == id)
        return 0;
    if (disc_busy() != 0)
        return -1;
    disc_wait(0);
    auto &heap = resident.heap;
    const auto keep = [&](std::uint32_t address) -> std::uint32_t & {
        const auto found = heap.headers.find(address - 8);
        if (found == heap.headers.end())
            throw field::FieldFormatError("Read-ahead block has no heap header");
        return found->second[1];
    };
    if (resident.preload_slot != 0xffffffffU) {
        // 800320b8 clears the block's keep flag, then 800320e8 releases it.
        auto &block = resident.preload_block;
        if (block.bytes.empty())
            throw field::FieldFormatError("The read-ahead block's bytes are not owned");
        keep(block.address) &= ~resident::heap_keep;
        if (resident::heap_release(heap, block, 0x8001b500) != 0)
            throw field::FieldFormatError("The read-ahead block was not released");
        block.bytes.clear();
    }
    // 8001b53c: the file's size rounded up to words (800288ec), a last-fit
    // block marked kept (800320a4) and a read into it (800295d8).
    const auto file = id + 0xb8U;
    const auto size = std::bit_cast<std::int32_t>(file_size(std::bit_cast<std::int32_t>(file)));
    const auto rounded = size + 3 >= 0 ? size + 3 : size + 6;
    resident.preload_size = static_cast<std::uint32_t>(rounded >> 2) << 2U;
    auto block = resident::heap_allocate(heap, resident.preload_size, 1, 0x8001b560);
    if (!block)
        throw MissingDependency({"preload_allocation", 0x8001b574, {}, {}},
                                "state:quiet-heap-failure", false,
                                "A failed quiet allocation reaches 800320a4 with a null block");
    resident.preload_block = std::move(*block);
    keep(resident.preload_block.address) |= resident::heap_keep;
    static_cast<void>(
        read_file(std::bit_cast<std::int32_t>(file), resident.preload_block.address, 0, 0x80));
    resident.preload_slot = slot;
    resident.preload_id = id;
    return -1;
}

bool Program::start_map_change() {
    auto &state = loaded(*this);
    if (state.event_control.gate_values[2] != 0 || resident.music.gate != 0 ||
        state.gate_adbc4 != 0xff || resident.battle_request.gate_90 != 0)
        return false;
    if (preload_field((resident.field_map & 0xfffU) << 1U, 0) != 0 || disc_busy() != 0 ||
        static_cast<std::int16_t>(state.fade.channels[0].halves[2]) != 0)
        return false;
    state.control_inputs.encounter.enabled_byte = 0;
    save_field_departure();
    disc_wait(0);
    return true;
}

void Program::field_map_change_step(FrameServices &services, const ProgramObserver &observe) {
    if (!start_map_change())
        return;
    field_reload(services, field_loop_stack - 0x48, observe); // 800a5c40
    resident.input_queue.reset();                             // 80035db0
    loaded(*this).control_inputs.encounter.enabled_byte = 1;
}

} // namespace xem::reconstruction
