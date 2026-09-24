#include "xem/reconstruction/original_layout.hpp"

#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <map>
#include <type_traits>

namespace xem::reconstruction {
namespace {

template <typename T> T convert(std::uint32_t raw) {
    if constexpr (std::is_signed_v<T>) {
        using U = std::make_unsigned_t<T>;
        return std::bit_cast<T>(static_cast<U>(raw));
    } else {
        return static_cast<T>(raw);
    }
}
FieldState &f(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("Original field globals require field state");
    return *program.field;
}

std::vector<OriginalGlobal> build() {
    std::vector<OriginalGlobal> g;
    const auto entry = [&](std::string name, std::uint32_t address, std::size_t width,
                           bool resident, auto access) {
        g.push_back({std::move(name), address, width, resident,
                     [access](const Program &program) {
                         const auto &value = access(const_cast<Program &>(program));
                         return static_cast<std::uint32_t>(value);
                     },
                     [access](Program &program, std::uint32_t raw) {
                         auto &value = access(program);
                         value = convert<std::remove_reference_t<decltype(value)>>(raw);
                     }});
    };
    const auto add = [&](std::string name, std::uint32_t address, std::size_t width, auto access) {
        entry(std::move(name), address, width, false, access);
    };
    const auto add_resident = [&](std::string name, std::uint32_t address, std::size_t width,
                                  auto access) {
        entry(std::move(name), address, width, true, access);
    };
    // Event interpreter and scheduler.
    add("budget_mode", 0x800affec, 4,
        [](Program &p) -> auto & { return f(p).event_control.budget_mode; });
    add("break_requested", 0x800b00c0, 4,
        [](Program &p) -> auto & { return f(p).event_control.break_requested; });
    add("batch_limit", 0x800afc7c, 4,
        [](Program &p) -> auto & { return f(p).event_control.batch_limit; });
    add("post_initialization", 0x800adb1c, 4,
        [](Program &p) -> auto & { return f(p).event_control.post_initialization; });
    const std::array<std::uint32_t, 3> gates{0x800adbe0, 0x800adbe4, 0x800adbec};
    for (std::size_t i = 0; i < 3; ++i)
        add("event_gate", gates[i], 4,
            [i](Program &p) -> auto & { return f(p).event_control.gate_values[i]; });
    add("diagnostic_suppression", 0x800c268c, 4,
        [](Program &p) -> auto & { return f(p).event_control.diagnostic_suppression; });
    add("input_updated", 0x800adb68, 4,
        [](Program &p) -> auto & { return f(p).pass.input_updated; });
    add("unknown_c4268", 0x800c4268, 4,
        [](Program &p) -> auto & { return f(p).pass.unknown_c4268; });
    add("single_actor_mode", 0x800adb74, 4,
        [](Program &p) -> auto & { return f(p).single_actor_mode; });
    add("party_processing_mode", 0x800b21cc, 1,
        [](Program &p) -> auto & { return f(p).party_processing_mode; });
    for (std::uint32_t i = 0; i < 3; ++i) {
        add("party_indices", 0x8005a444 + i * 4, 4,
            [i](Program &p) -> auto & { return f(p).party_indices[i]; });
        add("history_index", 0x800b2360 + i * 4, 4,
            [i](Program &p) -> auto & { return f(p).history_indices[i]; });
        add("party_characters", 0x80062590 + i * 4, 4,
            [i](Program &p) -> auto & { return f(p).party_characters[i]; });
    }
    // Control inputs and state. 8004f308 is resident.music.gate; the control
    // handler receives it through ControlInputs at dispatch, never separately.
    add("held_buttons", 0x800afe9c, 2,
        [](Program &p) -> auto & { return f(p).control_inputs.held_buttons; });
    add("pressed_buttons", 0x800c2694, 2,
        [](Program &p) -> auto & { return f(p).control_inputs.pressed_buttons; });
    add("preserve_nonplayer_motion", 0x800b21ce, 1,
        [](Program &p) -> auto & { return f(p).control_inputs.preserve_nonplayer_motion; });
    add("jump_contact", 0x800adb64, 4,
        [](Program &p) -> auto & { return f(p).control_inputs.jump_contact; });
    add("jump_mode", 0x800b2344, 2,
        [](Program &p) -> auto & { return f(p).control_inputs.jump_mode; });
    add("repeat_delay", 0x800b2340, 2,
        [](Program &p) -> auto & { return f(p).control_inputs.repeat_delay; });
    add("alternate_directions", 0x800b2354, 1,
        [](Program &p) -> auto & { return f(p).control_inputs.alternate_directions; });
    for (std::uint32_t t = 0; t < 2; ++t)
        for (std::uint32_t i = 0; i < 16; ++i)
            add("direction_tables", 0x800adf68 + t * 32 + i * 2, 2, [t, i](Program &p) -> auto & {
                return f(p).control_inputs.direction_tables[t][i];
            });
    add("camera_angle", 0x800af98c, 2,
        [](Program &p) -> auto & { return f(p).control_inputs.camera_angle; });
    add("encounter_gate", 0x800b2298, 4,
        [](Program &p) -> auto & { return f(p).control_inputs.encounter.encounter_gate; });
    add("encounter_inhibition", 0x800b2176, 2,
        [](Program &p) -> auto & { return f(p).control_inputs.encounter.inhibition; });
    add("encounter_enabled", 0x800adb04, 1,
        [](Program &p) -> auto & { return f(p).control_inputs.encounter.enabled_byte; });
    add("stationary_counter", 0x800adb02, 2,
        [](Program &p) -> auto & { return f(p).control_state.stationary_counter; });
    add("repeat_remaining", 0x800b2342, 2,
        [](Program &p) -> auto & { return f(p).control_state.repeat_remaining; });
    add("latched_jump_setting", 0x800adb28, 4,
        [](Program &p) -> auto & { return f(p).control_state.latched_jump_setting; });
    // Battle request and music gate.
    add("battle_field_active", 0x800adbdc, 4,
        [](Program &p) -> auto & { return p.resident.battle_request.field_active; });
    add("battle_menu_gate", 0x800adb2c, 4,
        [](Program &p) -> auto & { return p.resident.battle_request.menu_gate; });
    add("battle_gate_90", 0x800adb90, 4,
        [](Program &p) -> auto & { return p.resident.battle_request.gate_90; });
    add("battle_pending", 0x800adb88, 4,
        [](Program &p) -> auto & { return p.resident.battle_request.pending; });
    add_resident("battle_selector", 0x80059508, 1,
                 [](Program &p) -> auto & { return p.resident.battle_request.selector; });
    add_resident("battle_mode", 0x8005954c, 1,
                 [](Program &p) -> auto & { return p.resident.battle_request.mode; });
    add_resident("battle_resident_flag", 0x800594f8, 1,
                 [](Program &p) -> auto & { return p.resident.battle_request.resident_flag; });
    add("battle_mode_source", 0x800b2356, 1,
        [](Program &p) -> auto & { return f(p).battle_mode_source; });
    // Resident pad-input queue.
    add_resident("input_count", 0x8005937c, 4,
                 [](Program &p) -> auto & { return p.resident.input_queue.count; });
    add_resident("input_write", 0x80059380, 4,
                 [](Program &p) -> auto & { return p.resident.input_queue.write; });
    add_resident("input_read", 0x80059384, 4,
                 [](Program &p) -> auto & { return p.resident.input_queue.read; });
    add_resident("input_overflow", 0x80050208, 4,
                 [](Program &p) -> auto & { return p.resident.input_queue.overflow; });
    add_resident("input_50200", 0x80050200, 4,
                 [](Program &p) -> auto & { return p.resident.input_queue.w50200; });
    constexpr std::array<std::uint32_t, 6> input_current{0x80059570, 0x80059574, 0x8005948c,
                                                         0x80059490, 0x800594a4, 0x800594a8};
    constexpr std::array<std::uint32_t, 6> input_other{0x800594dc, 0x800594e0, 0x800594e8,
                                                       0x800594ec, 0x800595c8, 0x800595cc};
    for (std::size_t i = 0; i < 6; ++i) {
        add_resident("input_current", input_current[i], 2,
                     [i](Program &p) -> auto & { return p.resident.input_queue.current[i]; });
        add_resident("input_other", input_other[i], 2,
                     [i](Program &p) -> auto & { return p.resident.input_queue.other[i]; });
    }
    add_resident("music_result", 0x8004f308, 4,
                 [](Program &p) -> auto & { return p.resident.music.gate; });
    add_resident("music_requested", 0x8004f324, 4,
                 [](Program &p) -> auto & { return p.resident.music.requested; });
    add_resident("music_sequence", 0x8004f338, 4,
                 [](Program &p) -> auto & { return p.resident.music.loaded_sequence; });
    add_resident("music_wave_bank", 0x8004f33c, 4,
                 [](Program &p) -> auto & { return p.resident.music.loaded_wave_bank; });
    add_resident("music_start_parameter", 0x8004f340, 4,
                 [](Program &p) -> auto & { return p.resident.music.start_parameter; });
    add_resident("music_wave_pending", 0x8004f354, 4,
                 [](Program &p) -> auto & { return p.resident.music.wave_pending; });
    add_resident("music_completed", 0x8004f36c, 4,
                 [](Program &p) -> auto & { return p.resident.music.completed; });
    add_resident("music_sequence_active", 0x8004f35c, 4,
                 [](Program &p) -> auto & { return p.resident.music.sequence_active; });
    add_resident("music_reuse_sequence", 0x8004f348, 4,
                 [](Program &p) -> auto & { return p.resident.music.reuse_sequence; });
    add_resident("music_wave_loaded_now", 0x8004f360, 4,
                 [](Program &p) -> auto & { return p.resident.music.wave_loaded_now; });
    add_resident("music_current_sequence", 0x80062528, 4,
                 [](Program &p) -> auto & { return p.resident.music.current_sequence; });
    add_resident("music_wave_transfer", 0x8006258c, 4,
                 [](Program &p) -> auto & { return p.resident.music.wave_transfer; });
    add_resident("music_cached_sequence", 0x8004f2fc, 4,
                 [](Program &p) -> auto & { return p.resident.music.cached_sequence; });
    add_resident("music_shared_wave_state", 0x8004f364, 4,
                 [](Program &p) -> auto & { return p.resident.music.shared_wave_state; });
    add_resident("music_shared_release_started", 0x8004f368, 4,
                 [](Program &p) -> auto & { return p.resident.music.shared_release_started; });
    add_resident("music_shared_release_flag", 0x8004f384, 2,
                 [](Program &p) -> auto & { return p.resident.music.shared_release_flag; });
    add_resident("music_active_shared_wave", 0x8006251c, 4,
                 [](Program &p) -> auto & { return p.resident.music.active_shared_wave; });
    // Field-overlay music globals.
    add("music_stream_descriptor", 0x800adbb8, 4,
        [](Program &p) -> auto & { return p.resident.music.stream.descriptor; });
    add("music_stream_consumer", 0x800afea4, 4,
        [](Program &p) -> auto & { return p.resident.music.stream.consumer; });
    add("music_deferred_sequence_read", 0x800afc54, 4,
        [](Program &p) -> auto & { return p.resident.music.deferred_sequence_read; });
    add("music_wave_chunk_index", 0x800b2370, 4,
        [](Program &p) -> auto & { return p.resident.music.wave_chunk_index; });
    add("music_wave_staging", 0x800c3a1c, 4,
        [](Program &p) -> auto & { return p.resident.music.wave_staging; });
    // Field update, motion, contact and followers.
    add("controlled_actor", 0x800b226c, 4,
        [](Program &p) -> auto & { return f(p).controlled_actor; });
    add("script_flag_b236c", 0x800b236c, 2,
        [](Program &p) -> auto & { return f(p).script_flag_b236c; });
    add("last_sound_effect", 0x800b21b8, 4,
        [](Program &p) -> auto & { return f(p).last_sound_effect; });
    add("history_reset", 0x800c3910, 4, [](Program &p) -> auto & { return f(p).history_reset; });
    add("followers_idle", 0x800b234e, 2, [](Program &p) -> auto & { return f(p).followers_idle; });
    add("talk_inhibited", 0x800b2174, 2, [](Program &p) -> auto & { return f(p).talk_inhibited; });
    add("gather_override", 0x800b2348, 2,
        [](Program &p) -> auto & { return f(p).gather_override; });
    add("touch_latch", 0x800adf64, 4, [](Program &p) -> auto & { return f(p).touch_latch; });
    add("position_result", 0x800adb00, 2,
        [](Program &p) -> auto & { return f(p).position_result; });
    add("forced_position", 0x800b21cf, 1,
        [](Program &p) -> auto & { return f(p).forced_position; });
    add("linked_floor_override", 0x800adb94, 4,
        [](Program &p) -> auto & { return f(p).linked_floor_override; });
    add("motion_counter", 0x800af858, 4, [](Program &p) -> auto & { return f(p).motion_counter; });
    for (std::uint32_t i = 0; i < 4; ++i)
        add("triangle_counts", 0x800afb44 + i * 4, 4,
            [i](Program &p) -> auto & { return f(p).triangle_counts[i]; });
    add("layer_count", 0x800afb54, 2, [](Program &p) -> auto & { return f(p).layer_count; });
    add("collision_mode", 0x800adb98, 4, [](Program &p) -> auto & { return f(p).collision_mode; });
    add("collision_enabled", 0x800adc0c, 4,
        [](Program &p) -> auto & { return f(p).collision_enabled; });
    add("animation_mode", 0x800b2346, 2, [](Program &p) -> auto & { return f(p).animation_mode; });
    add("motion_actor", 0x80065b08, 4, [](Program &p) -> auto & { return f(p).motion_actor; });
    add("terrain_angle", 0x800b2178, 2, [](Program &p) -> auto & { return f(p).terrain_angle; });
    add("terrain_scale", 0x800b218c, 2, [](Program &p) -> auto & { return f(p).terrain_scale; });
    for (std::uint32_t i = 0; i < 4; ++i)
        add("terrain_speeds", 0x800adfc4 + i * 2, 2,
            [i](Program &p) -> auto & { return f(p).terrain_speeds[i]; });
    for (std::uint32_t i = 0; i < 8; ++i)
        add("terrain_angles", 0x800adfa8 + i * 2, 2,
            [i](Program &p) -> auto & { return f(p).terrain_angles[i]; });
    // Whole 0x20-byte matrix records, three-word and three-halfword vectors.
    const auto matrix = [&](const std::string &name, std::uint32_t address, auto access) {
        for (std::uint32_t i = 0; i < 9; ++i)
            add(name, address + i * 2, 2,
                [access, i](Program &p) -> auto & { return access(p).r[i]; });
        add(name, address + 0x12, 2, [access](Program &p) -> auto & { return access(p).pad; });
        for (std::uint32_t i = 0; i < 3; ++i)
            add(name, address + 0x14 + i * 4, 4,
                [access, i](Program &p) -> auto & { return access(p).t[i]; });
    };
    const auto longs = [&](const std::string &name, std::uint32_t address, auto access) {
        for (std::uint32_t i = 0; i < 3; ++i)
            add(name, address + i * 4, 4,
                [access, i](Program &p) -> auto & { return access(p)[i]; });
    };
    const auto shorts = [&](const std::string &name, std::uint32_t address, auto access) {
        for (std::uint32_t i = 0; i < 3; ++i)
            add(name, address + i * 2, 2,
                [access, i](Program &p) -> auto & { return access(p)[i]; });
    };
    matrix("world_matrix", 0x800afaa4, [](Program &p) -> auto & { return f(p).world_matrix; });
    // Script-controlled fade, camera and flags.
    add("fade_mode", 0x800adc04, 4, [](Program &p) -> auto & { return f(p).fade.mode; });
    for (std::uint32_t i = 0; i < 32; ++i)
        add("fade_packets", 0x800b20dc + i, 1,
            [i](Program &p) -> auto & { return f(p).fade.packets[i]; });
    add("fade_started", 0x800adc08, 2, [](Program &p) -> auto & { return f(p).fade.started; });
    for (std::uint32_t i = 0; i < 6; ++i)
        add("fade", 0x800b20fc + i * 4, 4,
            [i](Program &p) -> auto & { return f(p).fade.words[i]; });
    for (std::uint32_t i = 0; i < 3; ++i)
        add("fade", 0x800b2114 + i * 2, 2,
            [i](Program &p) -> auto & { return f(p).fade.halves[i]; });
    add("camera_mode", 0x800af934, 2, [](Program &p) -> auto & { return f(p).camera.mode; });
    add("camera_target_a", 0x800af984, 4,
        [](Program &p) -> auto & { return f(p).camera.target_a; });
    add("camera_target_b", 0x800af988, 4,
        [](Program &p) -> auto & { return f(p).camera.target_b; });
    add("camera_flags", 0x800af9d8, 4, [](Program &p) -> auto & { return f(p).camera.flags; });
    add("camera_heading_half", 0x800af9e6, 2,
        [](Program &p) -> auto & { return f(p).camera.heading_half; });
    add("camera_heading_high", 0x800af9f0, 4,
        [](Program &p) -> auto & { return f(p).camera.heading_high; });
    add("camera_projection", 0x800af9f8, 4,
        [](Program &p) -> auto & { return f(p).camera.projection; });
    add("camera_elevation", 0x800af9fc, 2,
        [](Program &p) -> auto & { return f(p).camera.elevation; });
    add("camera_distance", 0x800af9fe, 2,
        [](Program &p) -> auto & { return f(p).camera.distance; });
    add("camera_heading", 0x800afa0c, 4, [](Program &p) -> auto & { return f(p).camera.heading; });
    add("camera_steps", 0x800afa1c, 2, [](Program &p) -> auto & { return f(p).camera.steps; });
    add("camera_start", 0x800afa20, 4, [](Program &p) -> auto & { return f(p).camera.start; });
    add("camera_step", 0x800afa24, 4, [](Program &p) -> auto & { return f(p).camera.step; });
    add("camera_counter", 0x800b21d8, 4, [](Program &p) -> auto & { return f(p).camera.counter; });
    // Move phase 800739c0: camera follow, view matrices and facing.
    const auto camera = [](Program &p) -> auto & { return f(p).camera; };
    matrix("camera_view", 0x800af85c, [camera](Program &p) -> auto & { return camera(p).view; });
    longs("camera_eye", 0x800af880, [camera](Program &p) -> auto & { return camera(p).eye; });
    longs("camera_target", 0x800af890, [camera](Program &p) -> auto & { return camera(p).target; });
    longs("camera_up", 0x800af8a0, [camera](Program &p) -> auto & { return camera(p).up; });
    longs("camera_eye_goal", 0x800af8b0,
          [camera](Program &p) -> auto & { return camera(p).eye_goal; });
    longs("camera_target_goal", 0x800af8c0,
          [camera](Program &p) -> auto & { return camera(p).target_goal; });
    longs("camera_shake_offset", 0x800af8e0,
          [camera](Program &p) -> auto & { return camera(p).shake_offset; });
    longs("camera_saved_target", 0x800af8f0,
          [camera](Program &p) -> auto & { return camera(p).saved_target; });
    longs("camera_point_actor_a", 0x800af900,
          [camera](Program &p) -> auto & { return camera(p).point_actor_a; });
    longs("camera_saved_eye", 0x800af910,
          [camera](Program &p) -> auto & { return camera(p).saved_eye; });
    longs("camera_point_actor_b", 0x800af920,
          [camera](Program &p) -> auto & { return camera(p).point_actor_b; });
    for (std::uint32_t i = 0; i < 24; ++i)
        add("direction_tables", 0x800aea34 + i * 2, 2,
            [i](Program &p) -> auto & { return f(p).direction_tables[i]; });
    add("camera_scripted_zoom", 0x800af930, 4,
        [](Program &p) -> auto & { return f(p).camera.scripted_zoom; });
    add("camera_scripted_elevation", 0x800af936, 2,
        [](Program &p) -> auto & { return f(p).camera.scripted_elevation; });
    add("camera_scripted_heading", 0x800af938, 2,
        [](Program &p) -> auto & { return f(p).camera.scripted_heading; });
    add("camera_scripted_scale", 0x800af93a, 2,
        [](Program &p) -> auto & { return f(p).camera.scripted_scale; });
    add("camera_scripted", 0x800af93c, 2,
        [](Program &p) -> auto & { return f(p).camera.scripted; });
    add("camera_target_steps", 0x800af93e, 2,
        [](Program &p) -> auto & { return f(p).camera.target_steps; });
    longs("camera_scripted_target", 0x800af940,
          [camera](Program &p) -> auto & { return camera(p).scripted_target; });
    longs("camera_target_step", 0x800af950,
          [camera](Program &p) -> auto & { return camera(p).target_step; });
    add("camera_eye_steps", 0x800af960, 2,
        [](Program &p) -> auto & { return f(p).camera.eye_steps; });
    longs("camera_scripted_eye", 0x800af964,
          [camera](Program &p) -> auto & { return camera(p).scripted_eye; });
    longs("camera_eye_step", 0x800af974,
          [camera](Program &p) -> auto & { return camera(p).eye_step; });
    add("camera_view_angle", 0x800af98e, 2,
        [](Program &p) -> auto & { return f(p).camera.view_angle; });
    matrix("camera_previous_view", 0x800af990,
           [camera](Program &p) -> auto & { return camera(p).previous_view; });
    matrix("camera_orbit", 0x800af9b0, [camera](Program &p) -> auto & { return camera(p).orbit; });
    shorts("camera_orbit_angles", 0x800af9d0,
           [camera](Program &p) -> auto & { return camera(p).orbit_angles; });
    for (std::uint32_t i = 0; i < 4; ++i)
        add("camera_bounds", 0x800af9dc + i * 2, 2,
            [i](Program &p) -> auto & { return f(p).camera.bounds[i]; });
    add("camera_heading_x", 0x800af9e4, 2,
        [](Program &p) -> auto & { return f(p).camera.heading_x; });
    add("camera_heading_z", 0x800af9e8, 2,
        [](Program &p) -> auto & { return f(p).camera.heading_z; });
    add("camera_heading_velocity", 0x800af9ec, 4,
        [](Program &p) -> auto & { return f(p).camera.heading_velocity; });
    for (std::uint32_t i = 0; i < 2; ++i)
        add("camera_heading_blocks", 0x800af9f4 + i, 1,
            [i](Program &p) -> auto & { return f(p).camera.heading_blocks[i]; });
    add("camera_heading_steps", 0x800af9f6, 2,
        [](Program &p) -> auto & { return f(p).camera.heading_steps; });
    add("camera_elevation_steps", 0x800afa00, 2,
        [](Program &p) -> auto & { return f(p).camera.elevation_steps; });
    add("camera_elevation_value", 0x800afa04, 4,
        [](Program &p) -> auto & { return f(p).camera.elevation_value; });
    add("camera_elevation_step", 0x800afa08, 4,
        [](Program &p) -> auto & { return f(p).camera.elevation_step; });
    add("camera_projection_steps", 0x800afa10, 2,
        [](Program &p) -> auto & { return f(p).camera.projection_steps; });
    add("camera_projection_value", 0x800afa14, 4,
        [](Program &p) -> auto & { return f(p).camera.projection_value; });
    add("camera_projection_step", 0x800afa18, 4,
        [](Program &p) -> auto & { return f(p).camera.projection_step; });
    add("camera_shake", 0x800afa28, 2, [](Program &p) -> auto & { return f(p).camera.shake; });
    add("camera_shake_time", 0x800afa2a, 2,
        [](Program &p) -> auto & { return f(p).camera.shake_time; });
    add("camera_shake_stop", 0x800afa2c, 2,
        [](Program &p) -> auto & { return f(p).camera.shake_stop; });
    longs("camera_shake_amplitude", 0x800afa30,
          [camera](Program &p) -> auto & { return camera(p).shake_amplitude; });
    longs("camera_shake_step", 0x800afa3c,
          [camera](Program &p) -> auto & { return camera(p).shake_step; });
    shorts("camera_world_angles", 0x800afa54,
           [camera](Program &p) -> auto & { return camera(p).world_angles; });
    shorts("camera_anchor", 0x800afa5c,
           [camera](Program &p) -> auto & { return camera(p).anchor; });
    matrix("camera_scaled_world", 0x800afa64,
           [camera](Program &p) -> auto & { return camera(p).scaled_world; });
    add("camera_scale", 0x800afac4, 4, [](Program &p) -> auto & { return f(p).camera.scale; });
    matrix("sprite_view", 0x800afc30, [](Program &p) -> auto & { return f(p).sprite_view; });
    shorts("sprite_view_angles", 0x800b2184,
           [](Program &p) -> auto & { return f(p).sprite_view_angles; });
    add("elevation_angle", 0x800b00b4, 4,
        [](Program &p) -> auto & { return f(p).elevation_angle; });
    add("camera_settle", 0x800adbac, 4, [](Program &p) -> auto & { return f(p).camera_settle; });
    add("camera_release", 0x800adbb0, 4, [](Program &p) -> auto & { return f(p).camera_release; });
    add("camera_floor_latched", 0x800adba8, 4,
        [](Program &p) -> auto & { return f(p).camera_floor_latched; });
    add("camera_cut", 0x800adc18, 4, [](Program &p) -> auto & { return f(p).camera_cut; });
    for (std::uint32_t i = 0; i < 8; ++i)
        add("heading_octants", 0x800adc1c + i, 1,
            [i](Program &p) -> auto & { return f(p).heading_octants[i]; });
    add("orientation_hold", 0x800adb05, 1,
        [](Program &p) -> auto & { return f(p).orientation_hold; });
    add("party_turn_speed", 0x800b21b4, 2,
        [](Program &p) -> auto & { return f(p).party_turn_speed; });
    add("camera_floor_fixed", 0x800b21cd, 1,
        [](Program &p) -> auto & { return f(p).camera_floor_fixed; });
    add("followed_actor", 0x800b233e, 2, [](Program &p) -> auto & { return f(p).followed_actor; });
    // Message windows.
    add("messages_address", 0x800adbf0, 4,
        [](Program &p) -> auto & { return f(p).messages_address; });
    for (std::uint32_t i = 0; i < 4; ++i)
        add("dialogue_slots", 0x800b068c + i * 4, 4,
            [i](Program &p) -> auto & { return f(p).dialogue_slots[i]; });
    add("dialogue_slot_cursor", 0x800ade90, 4,
        [](Program &p) -> auto & { return f(p).dialogue_slot_cursor; });
    for (std::uint32_t i = 0; i < 8; ++i)
        add("dialogue_vram", 0x800adf54 + i * 2, 2,
            [i](Program &p) -> auto & { return f(p).dialogue_vram[i]; });
    add("text_speed", 0x800b21d6, 2, [](Program &p) -> auto & { return f(p).text_speed; });
    add("dialogue_gate_afd04", 0x800afd04, 4,
        [](Program &p) -> auto & { return f(p).dialogue_gate_afd04; });
    add("disc_idle_known", 0x800adb70, 4,
        [](Program &p) -> auto & { return f(p).disc_idle_known; });
    // Movie request (extended 60) and the movie loop's inputs.
    const auto movie = [&](std::string name, std::uint32_t address, auto access) {
        add(std::move(name), address, 2,
            [access](Program &p) -> auto & { return access(f(p).movie.request); });
    };
    movie("movie_file", 0x800c3a20, [](auto &r) -> auto & { return r.movie; });
    for (std::size_t i = 0; i < 4; ++i)
        movie("movie_area", static_cast<std::uint32_t>(0x800c3a22 + 2 * i),
              [i](auto &r) -> auto & { return r.area[i]; });
    movie("movie_start_a1", 0x800c3a2a, [](auto &r) -> auto & { return r.start_a1; });
    movie("movie_start_a2", 0x800c3a2c, [](auto &r) -> auto & { return r.start_a2; });
    movie("movie_end_frame", 0x800c3a2e, [](auto &r) -> auto & { return r.end_frame; });
    movie("movie_layout", 0x800c3a30, [](auto &r) -> auto & { return r.layout; });
    for (std::size_t i = 0; i < 2; ++i)
        movie("movie_size", static_cast<std::uint32_t>(0x800c3a32 + 2 * i),
              [i](auto &r) -> auto & { return r.size[i]; });
    movie("movie_buffer_mark", 0x800c3a36, [](auto &r) -> auto & { return r.buffer_mark; });
    movie("movie_start_select", 0x800c3a38, [](auto &r) -> auto & { return r.start_select; });
    movie("movie_hold", 0x800c3a3a, [](auto &r) -> auto & { return r.hold; });
    add("movie_flags", 0x800adb80, 4, [](Program &p) -> auto & { return f(p).movie.flags; });
    add("movie_signals", 0x800adb84, 4, [](Program &p) -> auto & { return f(p).movie.signals; });
    add("movie_frame", 0x800b06a0, 4, [](Program &p) -> auto & { return f(p).movie.frame; });
    add("field_exit_mode", 0x800b0064, 4, [](Program &p) -> auto & { return f(p).exit_mode; });
    add_resident("gpu_type", 0x800568d0, 1,
                 [](Program &p) -> auto & { return p.resident.gpu_type; });
    add_resident("disc_error", 0x8004fdfc, 4,
                 [](Program &p) -> auto & { return p.resident.disc_error; });
    add_resident("disc_pending", 0x8004fe1c, 4,
                 [](Program &p) -> auto & { return p.resident.disc_pending; });
    // Resident disc stream ring and host-file flag.
    add_resident("disc_expected_sequence", 0x8004fe24, 2,
                 [](Program &p) -> auto & { return p.resident.disc_stream.expected_sequence; });
    add_resident("disc_ring", 0x8004fe30, 4,
                 [](Program &p) -> auto & { return p.resident.disc_stream.ring_buffer; });
    add_resident("disc_active_blocks", 0x8004fe40, 4,
                 [](Program &p) -> auto & { return p.resident.disc_stream.active_block_count; });
    add_resident("disc_host_file_table", 0x8004fe48, 4,
                 [](Program &p) -> auto & { return p.resident.disc_stream.host_file_table; });
    // Resident file reads (800295d8).
    const auto disc = [&](std::string name, std::uint32_t address, std::size_t width, auto member) {
        add_resident(std::move(name), address, width,
                     [member](Program &p) -> auto & { return p.resident.disc_read.*member; });
    };
    using Read = DiscReadState;
    disc("disc_file_table", 0x8004fdf0, 4, &Read::file_table);
    disc("disc_directory_table", 0x8004fdf4, 4, &Read::directory_table);
    disc("disc_size", 0x8004fdf8, 4, &Read::size);
    disc("disc_sector", 0x8004fe04, 4, &Read::sector);
    disc("disc_destination", 0x8004fe08, 4, &Read::destination);
    disc("disc_fe0c", 0x8004fe0c, 4, &Read::w_fe0c);
    disc("disc_fe10", 0x8004fe10, 4, &Read::w_fe10);
    disc("disc_directory", 0x8004fe14, 4, &Read::directory);
    disc("disc_read_directory", 0x8004fe18, 4, &Read::read_directory);
    disc("disc_fe26", 0x8004fe26, 2, &Read::h_fe26);
    disc("disc_fe28", 0x8004fe28, 2, &Read::h_fe28);
    disc("disc_ring_slots", 0x8004fe2c, 4, &Read::ring_slots);
    disc("disc_fe34", 0x8004fe34, 4, &Read::w_fe34);
    disc("disc_offset", 0x8004fe38, 4, &Read::offset);
    disc("disc_host_file", 0x8004fe4c, 4, &Read::host_file);
    disc("disc_file", 0x80059f0c, 4, &Read::file);
    disc("disc_59f60", 0x80059f60, 2, &Read::h_59f60);
    disc("disc_requests", 0x8005a488, 4, &Read::requests);
    disc("disc_5a4dc", 0x8005a4dc, 4, &Read::w_5a4dc);
    for (std::uint32_t i = 0; i < 3; ++i)
        add_resident("disc_59ef8", 0x80059ef8 + i * 4, 4,
                     [i](Program &p) -> auto & { return p.resident.disc_read.w_59ef8[i]; });
    for (std::uint32_t i = 0; i < 4; ++i) {
        add_resident("disc_location", 0x80059f10 + i, 1,
                     [i](Program &p) -> auto & { return p.resident.disc_read.location[i]; });
        add_resident("disc_59f18", 0x80059f18 + i, 1,
                     [i](Program &p) -> auto & { return p.resident.disc_read.b_59f18[i]; });
        add_resident("cd_position", 0x800564c4 + i, 1,
                     [i](Program &p) -> auto & { return p.resident.cd.position[i]; });
    }
    // CD library state.
    add_resident("cd_ready_callback", 0x800564a8, 4,
                 [](Program &p) -> auto & { return p.resident.cd.ready_callback; });
    add_resident("cd_sync_callback", 0x800564ac, 4,
                 [](Program &p) -> auto & { return p.resident.cd.sync_callback; });
    add_resident("cd_debug", 0x800564b4, 4,
                 [](Program &p) -> auto & { return p.resident.cd.debug; });
    add_resident("cd_status", 0x800564b8, 1,
                 [](Program &p) -> auto & { return p.resident.cd.status; });
    add_resident("cd_mode", 0x800564c8, 1, [](Program &p) -> auto & { return p.resident.cd.mode; });
    add_resident("cd_command", 0x800564c9, 1,
                 [](Program &p) -> auto & { return p.resident.cd.command; });
    add_resident("cd_sync_status", 0x80056788, 1,
                 [](Program &p) -> auto & { return p.resident.cd.sync_status; });
    add_resident("cd_ready_status", 0x80056789, 1,
                 [](Program &p) -> auto & { return p.resident.cd.ready_status; });
    for (std::uint32_t i = 0; i < 32; ++i) {
        add_resident("cd_setloc_first", 0x80056420 + i * 4, 4,
                     [i](Program &p) -> auto & { return p.resident.cd.setloc_first[i]; });
        add_resident("cd_clear_ready", 0x800565f0 + i * 4, 4,
                     [i](Program &p) -> auto & { return p.resident.cd.clear_ready[i]; });
        add_resident("cd_parameter_counts", 0x800566f0 + i * 4, 4,
                     [i](Program &p) -> auto & { return p.resident.cd.parameter_counts[i]; });
    }
    for (std::uint32_t i = 0; i < 3; ++i)
        add_resident("cd_registers", 0x80056770 + i * 4, 4,
                     [i](Program &p) -> auto & { return p.resident.cd.registers[i]; });
    add_resident("cd_interrupt_poll", 0x800578a6, 2,
                 [](Program &p) -> auto & { return p.resident.cd.interrupt_poll; });
    add_resident("dma_services", 0x8005892c, 4,
                 [](Program &p) -> auto & { return p.resident.cd.dma_services; });
    add_resident("dma_interrupt_register", 0x80058968, 4,
                 [](Program &p) -> auto & { return p.resident.cd.dma_interrupt_register; });
    add_resident("cd_dma_callback", 0x80058978, 4,
                 [](Program &p) -> auto & { return p.resident.cd.dma_callback; });
    // Resident sound driver.
    add_resident("sound_flags", 0x8005957c, 2,
                 [](Program &p) -> auto & { return p.resident.sound.flags; });
    add_resident("sound_effect_block", 0x800595d8, 4,
                 [](Program &p) -> auto & { return p.resident.sound.effect_block; });
    add_resident("sound_effect_run", 0x80059404, 4,
                 [](Program &p) -> auto & { return p.resident.sound.effect_run; });
    add_resident("sound_effect_banks", 0x80059440, 4,
                 [](Program &p) -> auto & { return p.resident.sound.effect_banks; });
    add_resident("sound_wave_banks", 0x80059558, 4,
                 [](Program &p) -> auto & { return p.resident.sound.wave_banks; });
    add_resident("sound_sequences", 0x80059564, 4,
                 [](Program &p) -> auto & { return p.resident.sound.sequences; });
    add_resident("sound_pool", 0x80059410, 4,
                 [](Program &p) -> auto & { return p.resident.sound.pool; });
    add_resident("sound_start_stamp", 0x80059504, 4,
                 [](Program &p) -> auto & { return p.resident.sound.start_stamp; });
    add_resident("sound_voice_changes", 0x80059554, 4,
                 [](Program &p) -> auto & { return p.resident.sound.voice_changes; });
    add_resident("sound_voice_holds", 0x800594fc, 4,
                 [](Program &p) -> auto & { return p.resident.sound.voice_holds; });
    for (std::size_t i = 0; i < 24; ++i)
        add_resident("sound_voice_owner", static_cast<std::uint32_t>(0x8006252c + 4 * i), 4,
                     [i](Program &p) -> auto & { return p.resident.sound.voice_owners[i]; });
    // Sound output volumes and mode (800386c4 and its callees).
    add_resident("sound_commits", 0x8005a3c0, 4,
                 [](Program &p) -> auto & { return p.resident.sound.commits; });
    for (std::size_t i = 0; i < 2; ++i) {
        add_resident("sound_master_pair", static_cast<std::uint32_t>(0x8005a3c4 + 2 * i), 2,
                     [i](Program &p) -> auto & { return p.resident.sound.master_pair[i]; });
        add_resident("sound_cd_pair", static_cast<std::uint32_t>(0x8005a3d0 + 2 * i), 2,
                     [i](Program &p) -> auto & { return p.resident.sound.cd_pair[i]; });
        add_resident("sound_reverb_pair", static_cast<std::uint32_t>(0x8005940c + 2 * i), 2,
                     [i](Program &p) -> auto & { return p.resident.sound.reverb_pair[i]; });
        add_resident("spu_reverb_output", static_cast<std::uint32_t>(0x800589bc + 2 * i), 2,
                     [i](Program &p) -> auto & { return p.resident.sound.spu_reverb_output[i]; });
    }
    add_resident("sound_master", 0x8005a3e8, 2,
                 [](Program &p) -> auto & { return p.resident.sound.master; });
    add_resident("sound_cd", 0x8005a3ea, 2,
                 [](Program &p) -> auto & { return p.resident.sound.cd; });
    add_resident("sound_reverb", 0x8005a3ec, 2,
                 [](Program &p) -> auto & { return p.resident.sound.reverb; });
    add_resident("sound_master_level", 0x8005a3f0, 4,
                 [](Program &p) -> auto & { return p.resident.sound.master_level; });
    add_resident("sound_master_step", 0x8005a3f4, 4,
                 [](Program &p) -> auto & { return p.resident.sound.master_step; });
    add_resident("sound_master_frames", 0x8005a3f8, 2,
                 [](Program &p) -> auto & { return p.resident.sound.master_frames; });
    add_resident("sound_master_target", 0x8005a3fa, 2,
                 [](Program &p) -> auto & { return p.resident.sound.master_target; });
    add_resident("sound_cd_level", 0x8005a3fc, 4,
                 [](Program &p) -> auto & { return p.resident.sound.cd_level; });
    add_resident("sound_cd_step", 0x8005a400, 4,
                 [](Program &p) -> auto & { return p.resident.sound.cd_step; });
    add_resident("sound_cd_frames", 0x8005a404, 2,
                 [](Program &p) -> auto & { return p.resident.sound.cd_frames; });
    add_resident("sound_cd_target", 0x8005a406, 2,
                 [](Program &p) -> auto & { return p.resident.sound.cd_target; });
    add_resident("sound_mode_voice", 0x80059518, 4,
                 [](Program &p) -> auto & { return p.resident.sound.mode_voice; });
    add_resident("sound_voice_releases", 0x80059550, 4,
                 [](Program &p) -> auto & { return p.resident.sound.voice_releases; });
    add_resident("spu_registers", 0x80058e08, 4,
                 [](Program &p) -> auto & { return p.resident.sound.spu_registers; });
    // Game-mode selection and the field exit.
    add_resident("next_mode", 0x80018088, 4,
                 [](Program &p) -> auto & { return p.resident.next_mode; });
    add_resident("mode_block", 0x800592bc, 4,
                 [](Program &p) -> auto & { return p.resident.mode_block.address; });
    add_resident("mode_loaded", 0x800592c0, 4,
                 [](Program &p) -> auto & { return p.resident.mode_loaded; });
    add_resident("field_exit_5942c", 0x8005942c, 1,
                 [](Program &p) -> auto & { return p.resident.b_5942c; });
    add_resident("field_exit_4f30c", 0x8004f30c, 4,
                 [](Program &p) -> auto & { return p.resident.w_4f30c; });
    add_resident("field_exit_4f310", 0x8004f310, 4,
                 [](Program &p) -> auto & { return p.resident.w_4f310; });
    add_resident("field_exit_4f370", 0x8004f370, 4,
                 [](Program &p) -> auto & { return p.resident.w_4f370; });
    add_resident("vsync_counter", 0x80058960, 4,
                 [](Program &p) -> auto & { return p.resident.vsync_counter; });
    add_resident("cd_sync_deadline", 0x8005a228, 4,
                 [](Program &p) -> auto & { return p.resident.cd_sync_deadline; });
    add_resident("cd_sync_polls", 0x8005a22c, 4,
                 [](Program &p) -> auto & { return p.resident.cd_sync_polls; });
    add_resident("cd_sync_label", 0x8005a230, 4,
                 [](Program &p) -> auto & { return p.resident.cd_sync_label; });
    add_resident("cd_dma_register", 0x800567b4, 4,
                 [](Program &p) -> auto & { return p.resident.cd_dma_register; });
    add_resident("text_clut_even", 0x800595d4, 2,
                 [](Program &p) -> auto & { return p.resident.text_cluts[0]; });
    add_resident("text_clut_odd", 0x80059414, 2,
                 [](Program &p) -> auto & { return p.resident.text_cluts[1]; });
    // Resident random seed and the PushMatrix stack.
    add_resident("random_seed", 0x8005a1fc, 4,
                 [](Program &p) -> auto & { return p.resident.random_seed; });
    add_resident("matrix_stack_depth", 0x80056d2c, 4,
                 [](Program &p) -> auto & { return p.resident.matrix_stack.depth; });
    for (std::uint32_t i = 0; i < 0x280; ++i)
        add_resident("matrix_stack", 0x80056d30 + i, 1,
                     [i](Program &p) -> auto & { return p.resident.matrix_stack.records[i]; });
    for (std::uint32_t i = 0; i < 2; ++i)
        add("script_flags_b21d0", 0x800b21d0 + i, 1,
            [i](Program &p) -> auto & { return f(p).script_flags_b21d0[i]; });
    // Dialogue windows, whole original records.
    for (std::uint32_t i = 0; i < 4; ++i)
        for (std::uint32_t at = 0; at < field::DialogueWindow::stride; ++at)
            add("dialogue", field::DialogueWindow::base + i * field::DialogueWindow::stride + at, 1,
                [i, at](Program &p) -> auto & { return f(p).dialogue[i].bytes[at]; });
    // Resident sprite environment.
    add_resident("sprite_frame_head", 0x80059190, 4,
                 [](Program &p) -> auto & { return p.resident.sprite.frame_head; });
    add_resident("sprite_rate", 0x80059198, 4,
                 [](Program &p) -> auto & { return p.resident.sprite.rate_control; });
    add_resident("sprite_platform", 0x800591ad, 1,
                 [](Program &p) -> auto & { return p.resident.sprite.platform_mode; });
    add_resident("sprite_binding", 0x800591b0, 1,
                 [](Program &p) -> auto & { return p.resident.sprite.binding_control; });
    add_resident("sprite_variant", 0x800591b8, 4,
                 [](Program &p) -> auto & { return p.resident.sprite.variant; });
    add_resident("sprite_texture_page", 0x80059310, 4,
                 [](Program &p) -> auto & { return p.resident.sprite.texture_page; });
    add_resident("sprite_texture_mode", 0x80050108, 4,
                 [](Program &p) -> auto & { return p.resident.sprite.texture_mode; });
    add("descriptor_count", 0x800afb0c, 4,
        [](Program &p) -> auto & { return f(p).descriptor_count; });
    add("snapshot_cursor", 0x800afc50, 4,
        [](Program &p) -> auto & { return f(p).snapshot_cursor; });
    for (std::uint32_t i = 0; i < 3; ++i)
        add_resident("saved_party_modes", 0x8005a408 + i * 4, 4,
                     [i](Program &p) -> auto & { return p.resident.saved_party_modes[i]; });
    add_resident("game_state", 0x8005a39c, 4,
                 [](Program &p) -> auto & { return p.resident.game_state; });
    // Resident heap globals ($gp-relative in 80031bdc). Globals in overlay data
    // (800adbxx above) are field entries: at boot the heap holds that memory.
    add_resident("heap_class", 0x80059318, 2,
                 [](Program &p) -> auto & { return p.resident.heap.allocation_class; });
    add_resident("heap_tag", 0x8005931c, 2,
                 [](Program &p) -> auto & { return p.resident.heap.tag; });
    add_resident("heap_head", 0x80059320, 4,
                 [](Program &p) -> auto & { return p.resident.heap.head; });
    add_resident("heap_dirty", 0x8005932c, 4,
                 [](Program &p) -> auto & { return p.resident.heap.dirty; });
    add_resident("heap_quiet", 0x80059330, 4,
                 [](Program &p) -> auto & { return p.resident.heap.quiet; });
    add_resident("heap_last_size", 0x8005933c, 4,
                 [](Program &p) -> auto & { return p.resident.heap.last_size; });
    add_resident("heap_last_caller", 0x80059340, 4,
                 [](Program &p) -> auto & { return p.resident.heap.last_caller; });
    add("sprite_gate", 0x800b218e, 2, [](Program &p) -> auto & { return f(p).sprite_gate; });
    add("party_reassignment", 0x800b2268, 4,
        [](Program &p) -> auto & { return f(p).party_reassignment; });
    // Remaining bytes of the regions a field-return snapshot copies.
    const auto covered = [&](std::uint32_t address) {
        return std::ranges::any_of(g, [&](const OriginalGlobal &item) {
            return address >= item.address && address - item.address < item.width;
        });
    };
    for (const auto &[address, size] : snapshot_regions)
        for (std::uint32_t at = address; at < address + size; ++at)
            if (!covered(at))
                add("uninterpreted", at, 1,
                    [at](Program &p) -> auto & { return f(p).uninterpreted[at]; });
    return g;
}

// Entries by address, for range copies.
const std::map<std::uint32_t, const OriginalGlobal *> &by_address() {
    static const auto index = [] {
        std::map<std::uint32_t, const OriginalGlobal *> result;
        for (const auto &item : original_globals())
            if (!result.emplace(item.address, &item).second)
                throw field::FieldFormatError("Duplicate original global " + item.name);
        // Each byte belongs to one entry: no entry may start inside another.
        const OriginalGlobal *previous = nullptr;
        for (const auto &[address, item] : result) {
            if (previous != nullptr && previous->address + previous->width > address)
                throw field::FieldFormatError("Overlapping original global " + item->name);
            previous = item;
        }
        return result;
    }();
    return index;
}
template <typename Visit> void each_entry(std::uint32_t address, std::size_t size, Visit visit) {
    const auto &index = by_address();
    for (std::size_t offset = 0; offset < size;) {
        const auto found = index.find(address + static_cast<std::uint32_t>(offset));
        if (found == index.end() || found->second->width > size - offset)
            throw field::FieldFormatError("Original range byte is not owned by one Program value");
        visit(*found->second, offset);
        offset += found->second->width;
    }
}
} // namespace

const std::vector<OriginalGlobal> &original_globals() {
    static const auto table = build();
    return table;
}

void write_original(Program &program, std::uint32_t address, std::span<const std::uint8_t> bytes) {
    each_entry(address, bytes.size(), [&](const OriginalGlobal &item, std::size_t offset) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < item.width; ++i)
            value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8U * i);
        item.set(program, value);
    });
}

void read_original(const Program &program, std::uint32_t address, std::span<std::uint8_t> bytes) {
    each_entry(address, bytes.size(), [&](const OriginalGlobal &item, std::size_t offset) {
        const auto value = item.get(program);
        for (std::size_t i = 0; i < item.width; ++i)
            bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
    });
}

} // namespace xem::reconstruction
