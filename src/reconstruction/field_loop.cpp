// The field main loop 80077e88 of field overlay 38a1ce82... between two field
// frames: from a frame's return (800782e4) to the call of the next frame
// (800782dc). The loop registers s1..s3 hold the constants 1, -1 and ff; s4
// and s5 are FieldState::combination_latched and music_saved.
//
// Interrupts that arrive between frames are platform inputs delivered where
// the original recorded them (Program::deliver_arrivals).
#include "xem/reconstruction/program.hpp"

#include <bit>

namespace xem::reconstruction {
namespace {
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("The field main loop requires loaded field state");
    return *program.field;
}
[[noreturn]] void unrecovered(std::string_view operation, std::uint32_t address, const char *id,
                              const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width) {
    if (at + width > bytes.size())
        throw field::FieldFormatError("Main-loop read outside its record");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
// The controlled actor's record (the descriptor's +4c pointer).
std::span<const std::uint8_t> controlled(const FieldState &state) {
    const auto index = static_cast<std::size_t>(state.controlled_actor);
    if (index >= state.actors.size())
        throw field::FieldFormatError("The main loop requires the controlled actor's record");
    return state.actors[index].storage;
}
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point) {
    if (observe)
        observe(program, point, true);
}
} // namespace

// 80078bc8: -1 while a battle menu, the disc, the music, a battle request,
// 800adb34 or a requested map change is pending; else 0.
std::int32_t Program::loop_disc_busy() {
    auto &state = loaded(*this);
    if (resident.battle_request.menu_gate != 0 || disc_busy() != 0 || resident.music.gate != 0 ||
        resident.battle_request.gate_90 != 0 || state.particles_paused != 0)
        return -1;
    return state.gate_adbc4 != 0xff ? -1 : 0;
}

// ClearOTagR 80044ad8(table, count) through libgpu service +2c (80045d5c):
// the ordering-table DMA (channel 6), then the first entry points at the
// libgpu terminator packet 8005698c.
void Program::clear_ordering_table(std::uint32_t table, std::uint32_t count) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        unrecovered("clear_ordering_table", 0x80044b18, "symbol:printf-80019964",
                    "libgpu request checking messages are not reconstructed");
    if (gpu.services != 0x80056888 || gpu.functions[11] != 0x80045d5c)
        unrecovered("clear_ordering_table", 0x80044b34, "symbol:gpu-services",
                    "Only the observed libgpu ordering-table service is recovered");
    const auto [address, block, control, dpcr] = gpu.otc_registers;
    io_write(dpcr, io_latch(dpcr, 4) | 0x08000000U, 4);
    io_write(control, 0, 4);
    io_write(address, table + count * 4U - 4U, 4);
    io_write(block, count, 4);
    io_write(control, 0x11000002U, 4);
    // The transfer (general PS1 DMA behavior, not game data): from the last
    // entry down, each entry links to the one before it; the first ends the
    // list.
    for (auto i = count; i-- > 1;)
        set_memory(table + 4U * i, (table + 4U * (i - 1U)) & 0xffffffU);
    set_memory(table, 0x00ffffffU);
    gpu_alarm(nullptr);
    constexpr std::uint32_t busy = 0x01000000U;
    if ((platform_read(resident.platform, 0x80045de4, 4) & busy) != 0)
        do {
            // 80046f30: the timeout prints and resets the GPU.
            if (s32(gpu.deadline) < s32(resident.vsync_counter) || 0xf0000 < s32(gpu.polls++))
                unrecovered("clear_ordering_table", 0x80046f30, "symbol:printf-80019964",
                            "The libgpu timeout message and reset are not reconstructed");
        } while ((platform_read(resident.platform, 0x80045e18, 4) & busy) != 0);
    set_memory(table, 0x8005698cU & 0xffffffU);
}

// 8007ae78(1, 80065848) after 8007af74(1): port 2's pointer record. A mouse
// (receive status 0, type 12) would first move the pointer.
void Program::pointer_state() {
    auto &state = loaded(*this);
    const auto pad = state.pointer_pads[1];
    const auto &buffers = resident.pad.buffers;
    if (pad != 0x800625fc && pad != 0x8006261e)
        unrecovered("pointer_state", 0x8007af88, "state:pointer-pad",
                    "The pointer record reads a buffer other than the controller buffers");
    const auto &buffer = buffers[pad == 0x800625fc ? 0 : 1];
    if (buffer[0] == 0 && buffer[1] == 0x12)
        unrecovered("pointer_state", 0x8007afb0, "symbol:field-mouse-pointer",
                    "Mouse pointer movement (8007af74) is not recovered");
    const auto divide = [](std::int32_t value, std::uint16_t divisor) {
        if (divisor == 0)
            throw field::FieldFormatError("The pointer record divides by zero");
        return value / static_cast<std::int32_t>(divisor);
    };
    auto &record = resident.pointer;
    record[0] = divide(state.pointer_x[1], state.pointer_divisors[0]);
    record[1] = divide(state.pointer_y[1], state.pointer_divisors[1]);
    record[2] = -0x100;
    record[3] = static_cast<std::int8_t>(buffer[4]);
    record[4] = static_cast<std::int8_t>(buffer[5]);
}

// 80074700: drain the input queue (80035cdc) into the frame's buttons: port 1
// held (800afe9c), port 2 held (800afea0), port 1 pressed (800c2694), port 2
// pressed (800c38f8), port 1 repeats (800c3900), port 2 repeats (800c3908).
void Program::drain_pad() {
    auto &state = loaded(*this);
    auto &queue = resident.input_queue;
    std::array<std::uint16_t, 6> buttons{};
    while (queue.dequeue())
        for (std::size_t i = 0; i < buttons.size(); ++i)
            buttons[i] |= i % 2 == 0 ? queue.current[i] & state.input_mask : queue.current[i];
    for (const auto i : {0U, 4U, 2U})
        buttons[i] &= state.position_result;
    queue.reset(); // 80035db0
    pointer_state();
    if (state.camera_cut != 0)
        buttons = {};
    if (resident.battle_request.field_active == 0)
        buttons[2] &= 0xff7fU;
    auto &inputs = state.control_inputs;
    inputs.held_buttons = buttons[0];
    state.held_buttons_2 = buttons[1];
    inputs.pressed_buttons = buttons[2];
    // The port 2 pressed and both repeat halfwords lie inside the fourth
    // dialogue window record as it is owned (+480, +488, +490).
    set_memory(0x800c38f8, buttons[3], 2);
    set_memory(0x800c3900, buttons[4], 2);
    set_memory(0x800c3908, buttons[5], 2);
}

// 800a31e8: the play record, unless 800b02c8 is 1.
void Program::record_play_state() {
    auto &state = loaded(*this);
    if (state.b_b02c8 == 1)
        return;
    state.held_history |= state.control_inputs.held_buttons;
    auto &data = resident.game_data;
    if (data.size() < game_data_bytes)
        throw field::FieldFormatError("The play record requires the game data");
    for (std::size_t slot = 0; slot < 3; ++slot)
        data[0x1d34 + slot] = static_cast<std::uint8_t>(state.party_characters[slot]);
    save_field_departure(); // 800a30fc
    resident.w_4f2f4 = 0;
    ++resident.w_4f318;
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        if (data[0x22b1 + slot] == 1)
            party_record(slot); // 8009fee4
    auto &variables = resident.variables;
    // Every 31st call steps variable 10 (low byte to 60, then the high byte),
    // or counts it down when 8004f328 has bit 4; bit 80 stops it.
    if (s32(resident.w_4f318) > 0x1e) {
        resident.w_4f318 = 0;
        if ((resident.w_4f328 & 0x80U) == 0) {
            const auto value = static_cast<std::uint32_t>(variables.read(10));
            auto low = value & 0xffU;
            auto high = (value >> 8U) & 0xffU;
            if ((resident.w_4f328 & 4U) == 0) {
                if (++low > 60) {
                    low = 0;
                    ++high;
                }
            } else if (low == 0) {
                if (high != 0) {
                    low = 0x3b;
                    --high;
                }
            } else {
                --low;
            }
            variables.write(10, static_cast<std::int32_t>(high << 8U | (low & 0xffU)));
        }
    }
    const auto &clock = resident.pad.clock; // frames, seconds, minutes, hours
    variables.write(12, clock[2] << 8U | clock[1]);
    variables.write(14, clock[3]);
    const auto actor = controlled(state);
    variables.write(0x1e, s16(word(actor, 0x22, 2)));
    variables.write(0x20, s16(word(actor, 0x2a, 2)));
    variables.write(0x22, s16(word(actor, 0x26, 2)));
}

bool Program::field_between_frames(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &request = resident.battle_request;
    auto &inputs = state.control_inputs;
    const auto &gates = state.event_control.gate_values; // 800adbe0, 800adbe4, 800adbec
    const auto flags = [&] { return word(controlled(state), 0, 4); };
    const auto repeats = [&] { return memory(0x800c3900, 2); }; // port 1 presses and repeats
    // 800a5924: a transition (800adb38) runs its own frames.
    if (state.transition != 0)
        unrecovered("field_transition", 0x800a5944, "symbol:field-transition-800a5924",
                    "The transition frames of 800a5924 are not recovered");
    // 800782ec: a battle request with draw buffer 1, the disc idle and
    // 80077e10 clear.
    if (state.draw_buffer == 1 && request.field_active == 0 && loop_disc_busy() == 0) {
        // 80077e10: -1 while 800adbd0 is 1, 800b2344 is zero and the
        // controlled actor has flag 800.
        const bool waiting = state.w_adbd0 == 1 && inputs.jump_mode == 0 && (flags() & 0x800U) != 0;
        if (!waiting && field_battle_start()) {
            observed(observe, *this, {"field_battle_start", 0x80078abc, {}, {}});
            // 80078abc: the loop ends; 8007954c calls the mode dispatcher.
            if (!leave_field_for_battle(services, observe))
                unrecovered("field_battle_exit", 0x800796e4, "symbol:field-exit-4f370",
                            "8004f370 keeping the field after a battle exit is not recovered");
            return false;
        }
    }
    field_map_change_step(services, observe); // 80078494..80078558
    // 80078558: leaving the field (exit kinds 1, 2 and 3) with draw buffer 1.
    if (state.draw_buffer == 1) {
        if (gates[1] == 0 && loop_disc_busy() == 0)
            unrecovered("field_exit", 0x80078590, "symbol:field-exit-1",
                        "Leaving the field with exit kind 1 is not recovered");
        if (state.gate_adbe8 == 0 && loop_disc_busy() == 0)
            unrecovered("field_exit", 0x8007860c, "symbol:field-exit-2",
                        "Leaving the field with exit kind 2 is not recovered");
        if (state.gate_adbd8 == 0 && loop_disc_busy() == 0)
            unrecovered("field_exit", 0x800786a8, "symbol:field-exit-3",
                        "Leaving the field with exit kind 3 is not recovered");
    }
    if (state.event_control.diagnostic_suppression == 0)
        unrecovered("field_debug_toggles", 0x80078708, "symbol:field-diagnostic-output",
                    "The debug toggles of an unsuppressed field are not recovered");
    // 80078810: the menu and action checks while no exit or battle is due.
    if (s32(state.gate_adbd8) == -1 && s32(request.field_active) == -1 && gates[1] == -1 &&
        loop_disc_busy() == 0 && gates[2] == -1) {
        const auto held = inputs.held_buttons;
        if ((held & 3U) == 0)
            state.combination_latched = false;
        if ((held & 1U) != 0 && state.pass.input_updated == 1 && (held & 2U) != 0 &&
            !state.combination_latched) {
            state.combination_latched = true;
            unrecovered("field_combination", 0x800788b8, "symbol:field-800798bc",
                        "The action of held buttons 1 and 2 (800798bc) is not recovered");
        }
        if ((repeats() & 0x100U) != 0 && state.pass.input_updated == 1)
            unrecovered("field_action", 0x80078978, "symbol:field-800aba98",
                        "The action of button 100 (800aba98) is not recovered");
        if (state.disc_idle_known != 0 && state.draw_buffer == 1)
            unrecovered("field_movie", 0x800789a8, "symbol:field-movie-800a7c58",
                        "Playing a requested movie from the main loop is not recovered");
        if (inputs.jump_contact != 0xff && state.draw_buffer == 0 && (flags() & 0x1800U) == 0)
            unrecovered("field_menu", 0x80078a28, "symbol:field-menu-800799d4",
                        "Opening the field menu (8007ffe8, 800799d4) is not recovered");
        if ((repeats() & 0x10U) != 0 && state.script_flags_b21d0[0] == 0 &&
            inputs.jump_contact == 0xff && state.pass.input_updated == 1) {
            inputs.jump_contact = 0x80;
            resident.b_59171 = static_cast<std::uint8_t>(state.script_flag_b236c);
        }
    }
    field_post_frame(); // 80078b5c
    observed(observe, *this, {"field_loop_tail", 0x80078b5c, {}, {}});
    // 80078174: 80035734(0) is zero without a controller in port 1.
    if (resident.pad.buffers[0][0] == 0xff)
        unrecovered("field_controller_wait", 0x80078184, "symbol:field-controller-wait",
                    "Waiting for a controller (80037ee4, 8001fab4) is not recovered");
    if ((repeats() & 0x800U) != 0 && (inputs.held_buttons & 0x40U) == 0 &&
        state.pause_inhibited == 0)
        unrecovered("field_pause", 0x80078238, "symbol:field-pause",
                    "Pausing the field (80037ee4, 8001fab4) is not recovered");
    if (state.event_control.diagnostic_suppression == 1)
        resident.variables.write(0x50, 1);
    // 80019ca0: the reset combination.
    if (resident.input_queue.current[0] == 0x90c)
        unrecovered("soft_reset", 0x80019cb8, "symbol:soft-reset-80019cd0",
                    "The soft reset (80019cd0) is not recovered");
    field_pre_frame(services); // 80077dac
    observed(observe, *this, {"field_between_frames", 0x800782dc, {}, {}});
    return true;
}

// 80078b5c: advance rand (8003fa38); 8004f308 at -1 takes the music
// load's next step (80085c90) for the requested music (8004f324).
void Program::field_post_frame() {
    auto &state = loaded(*this);
    resident.random_seed = resident.random_seed * 0x41c64e6dU + 12345U;
    deliver_arrivals(0x80078b88); // Recorded before the music poll's call
    if (s32(resident.music.gate) == -1)
        resident.music.gate = poll_music(resident.music.requested);
    if (state.camera_cut != 0)
        --state.camera_cut;
}

// 80073fe0 -> 80073f50: swap the draw buffer and clear its tables.
void Program::swap_draw_buffer() {
    auto &state = loaded(*this);
    if (state.event_control.diagnostic_suppression == 0)
        unrecovered("swap_draw_buffer", 0x80073f64, "symbol:break-80073f64",
                    "The diagnostic build's break is not a recovered result");
    const auto buffer = s32(state.draw_buffer) + 1;
    state.draw_buffer = static_cast<std::uint32_t>(buffer % 2);
    state.draw_block = 0x800b249cU + 0x80f4U * state.draw_buffer;
    clear_ordering_table(state.draw_block + 0x80d4U, 8);
    clear_ordering_table(state.draw_block + 0xccU, 0x1000);
    if (state.w_adb4c != 0)
        clear_ordering_table(state.draw_block + 0x40d0U, 0x1000);
}

void Program::field_pre_frame(FrameServices &services) {
    auto &state = loaded(*this);
    state.frame_start_hcount = take_service(services.hblank_counts, "VSync(1) in 80077dac");
    deliver_arrivals(0x80077db4);
    swap_draw_buffer(); // 80073fe0
    deliver_arrivals(0x80077dcc);
    drain_pad();
    deliver_arrivals(0x80074700);
    deliver_arrivals(0x800a31e8);
    if (state.event_control.diagnostic_suppression == 0)
        unrecovered("field_pre_frame", 0x80077df0, "symbol:field-debug-80281b00",
                    "The diagnostic overlay call 80281b00 is not recovered");
    record_play_state();
    deliver_arrivals(0x80077dac);
}

bool Program::field_loop_step(FrameServices &services, const ProgramObserver &observe) {
    if (!field_between_frames(services, observe))
        return false;
    field_frame(services, observe);
    return true;
}

} // namespace xem::reconstruction
