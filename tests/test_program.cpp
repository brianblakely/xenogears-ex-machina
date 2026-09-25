#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <iostream>

namespace game = xem::reconstruction;
namespace field = game::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t width) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}

game::Program events(std::vector<std::uint8_t> code, std::size_t count = 1) {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    auto &state = *program.field;
    state.event_package.bytecode = std::move(code);
    state.event_package.entries.resize(count);
    state.actors.resize(count);
    state.event_control.gate_values = {1, 1, 1};
    state.event_control.post_initialization = 1;
    for (auto &actor : state.actors) {
        put(actor.descriptor, 0x58, 0x200);
        for (std::size_t i = 1; i < 8; ++i)
            put(actor.storage, 0x90 + i * 8, 15U << 18U);
    }
    return program;
}

void state_continuity_and_moves() {
    auto program = events({0x35, 0, 0, 5, 0, 0x40, 0x26, 2, 0x80, 0x38, 0, 0, 3, 0, 0x40, 0});
    program.resident.random_seed = 0x12345678;
    program.resident.disc_stream.expected_sequence = 41;
    static_cast<void>(program.event_pass());
    auto moved = std::move(program);
    check(!program.field && moved.resident.variables.words[0] == 5,
          "Program move transfers owned mode storage and preserves resident variables");
    for (int pass = 0; pass < 3; ++pass)
        static_cast<void>(moved.event_pass());
    check(moved.resident.variables.words[0] == 8 && moved.field->actors[0].events().pc == 15,
          "Four passes compute the variable and wait continuation in one shared program");
    check(moved.field->actors[0].events().slots[0].resume_pc == 15,
          "Scheduler commits working PC to its owned selected slot");
    check(moved.resident.random_seed == 0x12345678 &&
              moved.resident.disc_stream.expected_sequence == 41,
          "Unrelated resident services survive event passes and a program move");
}

void connected_handlers_and_partial_state() {
    auto program = events({0x71, 0, 0x80, 0xfe, 0x7f, 0x86, 32, 0x80, 11, 0, 0, 0x03});
    program.field->battle_mode_source = 5;
    program.resident.battle_request.field_active = 0xffffffffU;
    program.resident.variables.words[0] = 27;
    static_cast<void>(program.event_pass());
    check(program.resident.battle_request.pending == 1 && program.resident.battle_request.mode == 5,
          "Real primary71 publishes resident transition request through shared dispatcher");
    check(program.field->actors[0].events().pc == 3,
          "A request returns at its entry boundary; it is not battle execution");
    static_cast<void>(program.event_pass());
    check(program.field->actors[0].events().pc == 3,
          "Request closes the scheduler gate; another pass does not bypass the transition");
    // Explicit synthetic return-service inputs, not a reconstruction of combat.
    program.field->event_control.gate_values[0] = 1;
    static_cast<void>(program.event_pass());
    check(program.field->actors[0].events().pc == 3,
          "Pending wait keeps its original prefix PC and does not invent elapsed time");
    program.resident.battle_request.pending = 0;
    static_cast<void>(program.event_pass());
    check(program.field->actors[0].events().pc == 5, "Ready wait computes the next batch PC");
    try {
        static_cast<void>(program.event_pass());
        check(false, "Unknown music handler must stop");
    } catch (const field::UnsupportedInstruction &error) {
        check(error.pc == 11 && error.opcode == 0x03,
              "Computed battle branch encounters its actual next instruction");
    }
    check(program.field->actors[0].events().pc == 11 &&
              program.field->actors[0].events().slots[0].resume_pc == 5,
          "Committed handler state survives; interrupted scheduler has not committed resume PC");
}

void map_change_request() {
    // 15; 98 map 22 entry 1; 5b.
    auto program = events({0x15, 0x98, 22, 0x80, 1, 0x80, 0x5b});
    auto &state = *program.field;
    auto &variables = program.resident.variables.words;
    state.actors[0].sprite.sprite.bytes.assign(0x20, 0xff);
    put(state.actors[0].storage, 0x106, 0x0400, 2);
    put(state.actors[0].storage, 0x30, 7);
    state.control_inputs.camera_angle = 0x300;
    program.resident.field_map = 0x4017;
    variables[9] = 5;
    program.resident.battle_request.field_active = 0;
    static_cast<void>(program.event_pass());
    check(state.actors[0].events().pc == 0, "15 waits while the field is inactive");
    program.resident.battle_request.field_active = 1;
    program.resident.music.gate = 0xffffffffU;
    static_cast<void>(program.event_pass());
    check(state.actors[0].events().pc == 1 && state.control_inputs.encounter.inhibition == -1 &&
              program.resident.field_map == 0x4017,
          "15 passes; 98 retries while music is pending");
    program.resident.music.gate = 0;
    static_cast<void>(program.event_pass());
    check(state.actors[0].events().pc == 6 && program.resident.field_map == 22 &&
              state.event_control.gate_values[2] == 0,
          "98 stores the requested map, closes gate ADBEC and breaks after its operands");
    check(variables[1] == 1 && variables[2] == 0x17 && variables[3] == 4 && variables[4] == 6 &&
              variables[9] == 6,
          "80092f44 records departure map, facing and camera octants and counts the change");
    static_cast<void>(program.event_pass());
    check(state.actors[0].events().pc == 6, "The closed gate blocks later event passes");
    state.event_control.gate_values[2] = 1;
    static_cast<void>(program.event_pass());
    check(state.actors[0].events().pc == 6 && get(state.actors[0].storage, 0x30, 4) == 0 &&
              get(state.actors[0].storage, 0x104, 2) == 0x8000 &&
              get(state.actors[0].sprite.sprite.bytes, 0xc, 4) == 0,
          "5b stops the actor and stays on its own PC");
}

void departure_record() {
    auto program = events({0});
    auto &state = *program.field;
    auto &resident = program.resident;
    auto &data = resident.game_data;
    data.assign(game::game_data_bytes, 0);
    put(data, 0x1932, 3, 2); // The previous save's variables 2 and 8.
    put(data, 0x1938, 5, 2);
    resident.field_map = 22;
    resident.music.requested = 0x41;
    resident.departure_5941c = 0x1234;
    resident.departure_594d0 = 0x56;
    put(state.actors[0].storage, 0x106, 0x0e00, 2);
    state.control_inputs.camera_angle = 0x100;
    state.camera.elevation = 0xfff0;
    state.party_characters = {0, 2, 0xff};
    auto &variables = resident.variables.words;
    variables[0x1ff] = 0x7777;
    variables[0x200] = 0x8888;
    program.save_field_departure();
    check(get(data, 0x231a, 2) == 22 && get(data, 0x2322, 2) == 0x41 && get(data, 0x2320, 2) == 3 &&
              get(data, 0x231c, 2) == 0xa00,
          "800a30fc records map, music, entry and heading from the previous save");
    check(variables[0x22] == 0x1234 && variables[0x23] == 0x56 && variables[3] == 1 &&
              variables[4] == 7 && variables[0x12] == 0xfff0 && variables[0x1e] == 22 &&
              variables[0x1f] == 0 && variables[0x20] == 2 && variables[0x21] == 0xff,
          "800a30fc stores the departure variables and the party");
    check(get(data, 0x1930 + 0x44, 2) == 0x1234 && get(data, 0x1930 + 0x3fe, 2) == 0x7777 &&
              get(data, 0x1930 + 0x400, 2) == 0,
          "The first 200 variables, after the stores, are copied to +1930");
    state.event_control.gate_values[2] = 1;
    check(!program.start_map_change(), "Without a requested map the step does nothing");
    state.event_control.gate_values[2] = 0;
    state.gate_adbc4 = 0;
    check(!program.start_map_change() && resident.preload_slot == 0xffffffffU,
          "A requested map waits while ADBC4 is not ff, before any read-ahead");
}

void collision_attribute_lane() {
    // 80 attribute 1 lane 2 = 0x134; 80 attribute 1 lane 4 (ignored).
    auto program = events({0x80, 1, 2, 0x34, 0x81, 0x80, 1, 4, 0, 0x80, 0x03});
    auto &state = *program.field;
    state.collision_component.assign(0x20, 0);
    put(state.collision_component, 0x14, 0x18);
    put(state.collision_component, 0x1c, 0xaabbccdd);
    state.collision.attributes_raw.assign(8, 0);
    put(state.collision.attributes_raw, 4, 0xaabbccdd);
    try {
        static_cast<void>(program.event_pass());
    } catch (const field::UnsupportedInstruction &error) {
        check(error.pc == 10, "80 advances five for every lane");
    }
    check(get(state.collision_component, 0x1c, 4) == 0xab34ccdd &&
              get(state.collision.attributes_raw, 4, 4) == 0xab34ccdd,
          "80 ORs the shifted value into the live attribute and its parsed copy");
}

void scheduler_observation_continuity() {
    auto program = events({0x26, 0, 0x80, 0}, 2);
    bool second_actor = false;
    static_cast<void>(program.event_pass([&](const auto &active, auto point, bool completed) {
        if (!completed && point.actor == 1) {
            second_actor = true;
            check(active.field->actors[0].events().slots[0].resume_pc == 3,
                  "Second actor observers see the first actor's scheduler commit");
        }
    }));
    check(second_actor, "Both actors ran in the same scheduler pass");
}

void extended_error_retains_prefix() {
    auto program = events({0xfe, 0xff});
    try {
        static_cast<void>(program.event_pass());
        check(false, "Unknown extended handler must stop");
    } catch (const field::UnsupportedExtendedInstruction &error) {
        check(error.pc == 1 && error.opcode == 0xff, "Extended PC is distinct from prefix PC");
    }
    check(program.field->actors[0].events().pc == 1,
          "FE's already-issued increment remains committed after interruption");
}

void sound_effect_dispatch() {
    // FE 62 value 0x40, channel 5; FE 63 value 0x20, channel 5; FE 65 effect 0,
    // channel 5 (a stop); then an unknown FE.
    auto program = events({0xfe, 0x62, 0x40, 0x80, 0x05, 0x80, 0xfe, 0x63, 0x20, 0x80,
                           0x05, 0x80, 0xfe, 0x65, 0x00, 0x80, 0x05, 0x80, 0xfe, 0xff});
    auto &sound = program.resident.sound;
    sound.effect_block = 0x80100000;
    auto &block = sound.objects[sound.effect_block];
    block.resize(game::resident::voice_records + 4 * game::resident::voice_stride);
    const auto voice = [](std::size_t index) {
        return game::resident::voice_records + index * game::resident::voice_stride;
    };
    block[voice(2)] = 1; // Pair (10 & fe) ^ 8 = voices 2, 3
    try {
        static_cast<void>(program.event_pass());
        check(false, "The trailing unknown extended handler must stop");
    } catch (const field::UnsupportedExtendedInstruction &error) {
        check(error.pc == 19, "Each sound-effect handler advances the PC by five");
    }
    check(get(block, voice(2) + 0x76, 2) == 0x4000 && get(block, voice(2) + 0x74, 2) == 0x2000 &&
              get(block, voice(2) + 2, 2) == 0x100,
          "62 targets +76 and 63 targets +74 of the doubled channel's pair");
    check(get(block, voice(3) + 0x76, 2) == 0 && get(block, voice(3) + 2, 2) == 0,
          "An idle voice is untouched");
    // 65 reads the effect at +1 and the channel at +3; channel 5 stops pair (10 & fe) ^ 8.
    check(get(block, voice(2), 2) == 0 && program.field->last_sound_effect == 0,
          "Effect zero only stops the channel's pair");
}

void original_snapshot_ownership() {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    program.resident.w_4f30c = 1;
    program.resident.variables.unsigned_bitmap[0] = 3;
    program.resident.random_seed = 93;
    field::original::FieldCaptureInput capture{};
    capture.descriptor_count = 71;
    capture.variables[0] = 27;
    put(capture.globals.field_state, 0x116, 0xfffc, 2);
    put(capture.globals.field_state, 0x1f0, 0);
    put(capture.globals.field_state, 0x154, 7, 1);
    put(capture.globals.field_state, 0x2de, 5, 1);
    const std::vector<std::uint8_t> previous(14340, 0xa5);
    program.resident.field_snapshot =
        field::original::capture_field_return(capture, previous).storage;
    const auto immutable = program.resident.field_snapshot;
    program.field->sprite_gate = 123;
    program.field->party_reassignment = 1;
    program.restore_field({}, {}); // Original zero-actor branch returns normally.
    check(program.field->descriptor_count == 71 && program.field->sprite_gate == -4 &&
              program.field->party_reassignment == 0 && program.field->battle_mode_source == 5 &&
              program.field->party_processing_mode == 7,
          "Return globals produce represented mode gates instead of prepared results");
    check(program.resident.variables.words[0] == 27 &&
              program.resident.variables.unsigned_bitmap[0] == 3 &&
              program.resident.random_seed == 93,
          "Snapshot restores variable values, preserves loaded types and unrelated RNG owner");
    check(program.field->snapshot_cursor ==
              0x8005a4e4U + static_cast<std::uint32_t>(program.field->snapshot_bytes_used),
          "Restore leaves 800afc50 after the consumed snapshot bytes");
    check(program.resident.field_snapshot == immutable,
          "Original snapshot stays immutable and is not a native persistence image");
    program.resident.field_snapshot.resize(10);
    try {
        program.restore_field_data();
        check(false, "Malformed snapshot must fail");
    } catch (const field::original::ReturnFormatError &) {
    }
    check(program.resident.variables.words[0] == 27,
          "Invalid input preserves previous resident state");
}

void control_shares_pass_and_owner() {
    auto program = events({0x0c});
    put(program.field->actors[0].storage, 0, 0x4000);
    program.field->control_inputs.held_buttons = 0x40;
    for (auto &window : program.field->dialogue)
        window.set_half(field::DialogueWindow::status, 0xffff); // No window displayed.
    // Source early encounter return is computed from inactive field, not a fake service result.
    static_cast<void>(program.event_pass());
    check(program.field->pass.input_updated == 1 && program.field->actors[0].events().pc == 0,
          "Control handler and scheduler share input_updated and preserve wrapper PC");
}

// Invented one-actor field with a single walkable triangle and a constant
// trig table (cosine 4096). Values are chosen so each result follows by hand.
game::Program move_field() {
    auto program = events({0x00});
    auto &state = *program.field;
    state.event_control.diagnostic_suppression = 1;
    auto &actor = state.actors[0];
    actor.sprite.sprite.address = 0x80100000;
    actor.sprite.sprite.bytes.assign(0x164, 0);
    put(actor.sprite.sprite.bytes, 0x7c, 0x80110000);
    put(actor.descriptor, 0x58, 0x100); // Facing applies (f40); motion (f80 == 200) does not.
    state.resources.push_back({0x80110000, std::vector<std::uint8_t>(64)});
    state.layer_count = 1;
    state.collision.layers.resize(1);
    state.collision.layers[0].triangles.push_back({{0, 2, 1}, {0xffff, 0xffff, 0xffff}, 0});
    state.collision.layers[0].vertices = {
        {-1000, 0, -1000, 0}, {1000, 0, -1000, 0}, {0, 0, 1000, 0}};
    state.collision.attributes_raw = {0, 0, 0x80, 0}; // Walkable (bit 800000).
    state.triangle_counts[0] = 1;
    auto &math = program.resident.math;
    math.trigonometry.assign(0x4000, 0);
    for (std::size_t angle = 0; angle < 4096; ++angle)
        put(math.trigonometry, angle * 4 + 2, 4096, 2);
    math.angle.assign(1025, 0);
    math.square_root.assign(192, 0);
    math.reciprocal.assign(192, 0x1000);
    state.orientation_hold = 1;
    return program;
}

void move_phase_camera_and_facing() {
    auto program = move_field();
    auto &state = *program.field;
    auto &a = state.actors[0].storage;
    put(a, 0x108, 0x100, 2); // Facing 100 toward 300 at speed 80: 180, no snap.
    put(a, 0x106, 0x300, 2);
    put(a, 0x11e, 0x80, 2);
    state.camera.heading_blocks = {1, 2};
    state.heading_octants[0] = 1; // Octant 0 blocked for mask 1: turn right.
    const field::GteMatrix entry{{1, 2, 3, 4, 5, 6, 7, 8, 9}, {10, 11, 12}};
    program.resident.gte.transform = entry;
    program.field_move();
    check(get(a, 0x108, 2) == 0x180, "80073930 turns facing by speed without passing the target");
    check(state.camera.heading == 0x200 && state.camera.heading_steps == 7 &&
              state.camera.heading_half == 0x40,
          "A blocked octant starts a right turn and the first step advances it");
    // Mode 0 sets both divisors to 8; eye and target move 1/8 toward y -0x200000.
    check(state.camera.target_a == 8 && state.camera.target_b == 8 && state.camera_settle == 1,
          "Mode 0 settles follow divisors");
    check(state.camera.target[1] == -0x40000 && state.camera.eye[1] == -0x40000,
          "Follow divides the goal distance by the divisor");
    check(program.resident.gte.transform.r == state.camera.scaled_world.r &&
              program.resident.gte.transform.t == state.camera.scaled_world.t,
          "The scaled world matrix is left loaded");
    const auto words = field::gte_words(entry);
    check(program.resident.matrix_stack.depth == 0 &&
              program.resident.matrix_stack.records[0] == 1 &&
              program.resident.matrix_stack.records[28] == words[7],
          "PushMatrix records the registers loaded at the orbit rotation");

    auto snapped = move_field();
    snapped.field->camera_cut = 1;
    put(snapped.field->actors[0].storage, 0x106, 0x1300, 2);
    snapped.field_move();
    check(get(snapped.field->actors[0].storage, 0x108, 2) == 0x300,
          "A camera cut snaps facing to the masked target");

    auto malformed = move_field();
    malformed.field->layer_count = 0;
    bool rejected = false;
    try {
        malformed.field_move();
    } catch (const field::FieldFormatError &) {
        rejected = true;
    }
    check(rejected, "A camera floor query without a loaded layer is rejected");
}

void original_layout_round_trip() {
    // No original byte has two owners in the layout table.
    std::vector<std::pair<std::uint32_t, const game::OriginalGlobal *>> starts;
    for (const auto &item : game::original_globals())
        starts.emplace_back(item.address, &item);
    std::ranges::sort(starts, {}, &decltype(starts)::value_type::first);
    for (std::size_t i = 1; i < starts.size(); ++i) {
        const auto &before = *starts[i - 1].second;
        if (before.address + before.width > starts[i].first)
            throw std::runtime_error("Original globals " + before.name + " and " +
                                     starts[i].second->name + " overlap");
    }
    auto program = events({0x00});
    // Every snapshot region byte has exactly one owner and survives a round trip.
    for (const auto [address, size] : game::snapshot_regions) {
        std::vector<std::uint8_t> bytes(size), back(size);
        for (std::size_t i = 0; i < size; ++i)
            bytes[i] = static_cast<std::uint8_t>(i * 7 + (address & 0xff));
        game::write_original(program, address, bytes);
        game::read_original(program, address, back);
        check(bytes == back, "Snapshot region bytes round-trip through Program values");
    }
    // Semantic owners receive the bytes at their original addresses.
    const std::array<std::uint8_t, 4> eye{0x78, 0x56, 0x34, 0x12};
    game::write_original(program, 0x800af880, eye);
    check(program.field->camera.eye[0] == 0x12345678, "Camera eye X owns 800af880");
    const std::array<std::uint8_t, 2> gate{0xfc, 0xff};
    game::write_original(program, 0x800b218e, gate);
    check(program.field->sprite_gate == -4, "The sprite gate owns 800b218e");
    bool rejected = false;
    try {
        const std::array<std::uint8_t, 2> half{};
        game::write_original(program, 0x800af880, half); // Straddles the eye word.
    } catch (const field::FieldFormatError &) {
        rejected = true;
    }
    check(rejected, "A range that splits an owned value is rejected");
    rejected = false;
    try {
        const std::array<std::uint8_t, 1> byte{};
        game::write_original(program, 0x80010000, byte);
    } catch (const field::FieldFormatError &) {
        rejected = true;
    }
    check(rejected, "An unowned original byte is rejected");
}

// Invented tables for field 80085b20 (primary 75): row 3 of 800adfcc names
// wave bank 2 with the shared-wave flag; directory 1c starts after file 10.
game::Program music_program(std::uint8_t id) {
    auto program = events({0x75, id, 0x80, 0xfe, 0xff});
    auto &resident = program.resident;
    resident.battle_request.field_active = 1;
    resident.cd_dma_register = 0x1f8010b8;
    resident.vsync_counter = 100;
    auto &read = resident.disc_read;
    read.files.assign(0x8000, 0);
    read.directories.assign(0x7a, 0);
    put(read.directories, 0x1c * 2, 11, 2);
    put(read.directories, 4 * 2, 5, 2);
    put(read.files, (0x17 + 10 - 1) * 7, 0x12345, 3); // File 2 * 2 + 13.
    put(read.files, (0x17 + 10 - 1) * 7 + 3, 0x4000, 4);
    auto &cd = resident.cd;
    cd.sync_status = 2;
    cd.parameter_counts[2] = 3;
    cd.registers = {0x1f801800, 0x1f801801, 0x1f801802};
    cd.dma_set_callback = 0x8004c21c;
    cd.dma_interrupt_register = 0x1f8010f4;
    // One free heap block 80100008..80110000, then the end block.
    auto &heap = resident.heap;
    heap.head = 0x80100008;
    heap.tag = 2;
    heap.headers[0x80100000] = {0x80110008, 0};
    heap.headers[0x80110000] = {0, game::resident::heap_end_tag};
    heap.held[0x80100008].assign(0xfff8, 0xcc);
    auto &overlay = program.field->overlay;
    const auto row = 0x800adfccU - game::field_overlay_base + 6;
    overlay.assign(row + 2, 0);
    overlay[row] = 2;
    overlay[row + 1] = 1;
    program.field->overlay_verified.assign(overlay.size(), true);
    auto &music = resident.music;
    music.requested = 9;
    music.loaded_wave_bank = 7;
    music.shared_release_started = 1;
    music.shared_wave_state = 1;
    return program;
}

void music_load() {
    auto program = music_program(3);
    try {
        static_cast<void>(program.event_pass());
        check(false, "The trailing unknown extended handler must stop");
    } catch (const field::UnsupportedExtendedInstruction &error) {
        check(error.pc == 4, "Primary 75 advances the PC by three after the load");
    }
    const auto &resident = program.resident;
    const auto &music = resident.music;
    const auto &read = resident.disc_read;
    const auto ring = music.stream.descriptor;
    check(music.requested == 3 && music.gate == 0xffffffffU && music.deferred_sequence_read == 1 &&
              music.shared_wave_state == 0 && music.loaded_wave_bank == 0xffffffffU,
          "The load stops the music, clears the shared wave and defers the sequence read");
    check(ring != 0 && music.stream.consumer == 0x800859dc &&
              resident.battle_request.menu_gate == 1 && music.wave_pending == 1 &&
              music.wave_chunk_index == 0,
          "80085560 starts the wave stream with the 800859dc consumer");
    check(read.ring.address == ring && read.ring.bytes.size() == 0x64 &&
              get(read.ring.bytes, 0, 4) == 8 && get(read.ring.bytes, 8, 2) == 8 &&
              resident.disc_stream.ring_buffer == ring,
          "8002a260 allocates eight blocks and selects their ring header");
    check(read.ring_payload.address == ring + 0x64 && read.ring_payload.bytes.size() == 0x4000 &&
              resident.music_blocks.size() == 1 &&
              resident.music_blocks[0].address == music.wave_staging &&
              resident.music_blocks[0].bytes.size() == 0x2000 &&
              resident.heap.last_caller == 0x80085be8,
          "The stream payload and the 800c3a1c staging stay owned heap blocks");
    check(read.file == 0x17 && read.sector == 0x12345 && read.destination == ring + 0x64 &&
              resident.disc_pending == 1 && resident.cd.command == 2 &&
              resident.cd.sync_callback == 0x8002b2f0,
          "The ring read of file bank * 2 + 13 is issued in directory 1c");
    check(read.directory == 4, "The load restores directory 4");

    // The read ends with no chunk delivered: the poll's stream step finds
    // the ring empty and the disc idle, returns the stream buffer and the
    // staging to the heap, then issues the deferred sequence read.
    program.resident.disc_pending = 0;
    program.resident.disc_error = 0;
    const auto staging = music.wave_staging;
    check(program.poll_music(3) == 0xffffffffU, "The poll stays pending for the sequence read");
    check(read.ring.bytes.empty() && read.ring_payload.bytes.empty() &&
              resident.music_blocks.empty() && resident.battle_request.menu_gate == 0,
          "800854d0 and 80085c90 release the stream buffer and the staging");
    check(resident.heap.headers.at(ring - 8)[1] == 0x84000000U &&
              resident.heap.headers.at(staging - 8)[1] == 0x84000000U &&
              resident.heap.held.contains(ring) && resident.heap.held.contains(staging),
          "Both blocks are free heap blocks again");
    check(music.wave_pending == 0 && music.wave_loaded_now == 1 && music.loaded_wave_bank == 2 &&
              music.sequence_pending == 1 && music.deferred_sequence_read == 0,
          "The wave bank counts as loaded and the sequence read is pending");
}

void music_load_without_stream() {
    // Id ff only stops: gate zero, no directory change.
    auto stop = music_program(0xff);
    stop.resident.disc_read.directory = 77;
    try {
        static_cast<void>(stop.event_pass());
    } catch (const field::UnsupportedExtendedInstruction &) {
    }
    check(stop.resident.music.gate == 0 && stop.resident.disc_read.directory == 77 &&
              stop.resident.music.shared_wave_state == 1,
          "Music ff stops and clears the gate without reading the table");
    // The loaded bank starts no stream.
    auto same = music_program(3);
    same.resident.music.loaded_wave_bank = 2;
    try {
        static_cast<void>(same.event_pass());
    } catch (const field::UnsupportedExtendedInstruction &) {
    }
    // stop_music forgets the loaded bank before the comparison.
    check(same.resident.music.stream.descriptor != 0,
          "8001b66c resets the loaded bank, so the stream still starts");
    // 80086024 releases the shared wave only once.
    auto release = music_program(3);
    release.resident.music.shared_release_started = 0;
    release.resident.music.active_shared_wave = 0x80120000;
    bool released = false;
    try {
        static_cast<void>(release.event_pass());
    } catch (const game::resident::SoundError &) {
        released = true;
    }
    check(released && release.resident.music.shared_release_flag == 1 &&
              release.resident.music.shared_release_started == 0,
          "80086024 sets 8004f384 and then releases *8006251c through 80038310");
    // An unverified row fails instead of reading the source.
    auto unverified = music_program(3);
    unverified.field->overlay_verified.assign(unverified.field->overlay.size(), false);
    bool rejected = false;
    try {
        static_cast<void>(unverified.event_pass());
    } catch (const field::FieldFormatError &) {
        rejected = true;
    }
    check(rejected && unverified.resident.music.stream.descriptor == 0,
          "A row whose loaded copy differs from the source is not read");
}

void input_queue_reset() {
    xem::reconstruction::InputQueue queue{3, 4, 5, 1, 0, {1, 2, 3, 4, 5, 6}, {7, 8, 9, 10, 11, 12}};
    queue.reset();
    check(queue.count == 0 && queue.write == 0 && queue.read == 0 && queue.overflow == 0 &&
              queue.w50200 == 1 && queue.current == std::array<std::uint16_t, 6>{} &&
              queue.other == std::array<std::uint16_t, 6>{},
          "80035db0 clears the queue and input halfwords and sets 80050200");
}
} // namespace

int main() {
    try {
        state_continuity_and_moves();
        connected_handlers_and_partial_state();
        map_change_request();
        collision_attribute_lane();
        departure_record();
        scheduler_observation_continuity();
        extended_error_retains_prefix();
        sound_effect_dispatch();
        original_snapshot_ownership();
        control_shares_pass_and_owner();
        input_queue_reset();
        move_phase_camera_and_facing();
        original_layout_round_trip();
        music_load();
        music_load_without_stream();
        std::cout << "15 connected program regression groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
