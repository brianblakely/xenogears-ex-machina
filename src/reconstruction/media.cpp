// Sound output mode, the field movie loop's decisions and the field's exit
// to the mode dispatcher.
#include "xem/reconstruction/program.hpp"

namespace xem::reconstruction {
namespace {
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("Selected operation requires an initialized field");
    return *program.field;
}
} // namespace

void Program::set_sound_mode(std::int32_t mode) {
    auto &sound = resident.sound;
    resident::select_sound_mode(sound, mode);
    // libspu 8004e574: the reverb output volume (SPU 184/186) and its copy.
    if (sound.spu_registers == 0)
        throw resident::SoundError("The libspu register base is not set");
    for (std::uint32_t side = 0; side < 2; ++side) {
        resident.hardware_writes.push_back(
            {sound.spu_registers + 0x184 + 2 * side, sound.reverb_pair[side], 2});
        sound.spu_reverb_output[side] = sound.reverb_pair[side];
    }
    resident::mark_sequences(sound, 0x100);
    if ((sound.flags & 0x4000U) != 0)
        throw MissingDependency({"sound_cd_mix", 0x8003885c, {}, {}}, "symbol:sound-cd-mix", false,
                                "Driver flag 4000 selects the CD mix 8003885c");
    resident::update_mode_voice(sound);
}

void Program::set_next_mode(std::uint32_t mode) {
    resident.next_mode = mode;
    if (mode == resident.mode_loaded)
        return;
    if (resident.mode_block.address != 0) {
        // 800320e8. The pointer can outlive its heap block (observed inside
        // the mode dispatcher's call with mode 0); the release then rewrites
        // the stale flags word in heap-held bytes. A kept block (-1) stays in
        // memory; the pointer is forgotten either way.
        static_cast<void>(resident::heap_release(resident.heap, resident.mode_block, 0x800199a0));
        resident.mode_block = {};
    }
    resident.mode_loaded = 0xffffffffU;
}

field::MoviePad Program::movie_pad() const {
    if (!field)
        throw field::FieldFormatError("The movie loop requires an initialized field");
    return field::movie_pad(field->movie, field->single_actor_mode,
                            field->event_control.diagnostic_suppression);
}

field::MovieStep Program::movie_decision(field::MovieServices &services) {
    const auto &state = loaded(*this);
    // The drain's halfword 800c3900 lies inside the fourth dialogue window
    // record as it is owned (800c26b0 + 3 * 498 + 488).
    constexpr std::size_t at =
        0x800c3900 - field::DialogueWindow::base - 3 * field::DialogueWindow::stride;
    const auto &window = state.dialogue[3].bytes;
    const auto buttons = static_cast<std::uint16_t>(window[at] | (window[at + 1] << 8U));
    const auto step = field::movie_step(state.movie, state.single_actor_mode,
                                        state.event_control.diagnostic_suppression, buttons);
    if (step == field::MovieStep::skip) {
        resident::set_cd_volume(resident.sound, 0, 10);
        for (int wait = 0; wait < 5; ++wait)
            services.wait_vertical_blank();
    }
    return step;
}

bool Program::exit_field(std::uint32_t kind) {
    auto &state = loaded(*this);
    resident.b_5942c = 0;
    if (kind == 0) {
        // Battle: record the departure (800a30fc), keep the music to return
        // to (8004f324 and game data +2322) and the map (+1932 into +2320),
        // then select battle mode (2).
        save_field_departure();
        auto &data = resident.game_data;
        if (data.size() < game_data_bytes)
            throw field::FieldFormatError("The battle exit requires the game data");
        const auto music = state.saved_music;
        const auto map = static_cast<std::uint32_t>(data[0x1932] | data[0x1933] << 8U);
        resident.music.requested = music;
        data[0x2322] = static_cast<std::uint8_t>(music);
        data[0x2323] = static_cast<std::uint8_t>(music >> 8U);
        data[0x2320] = static_cast<std::uint8_t>(map);
        data[0x2321] = static_cast<std::uint8_t>(map >> 8U);
        if (resident.w_4f370 != 0)
            return false;
        set_next_mode(2);
        return true;
    }
    if (kind != 3)
        throw MissingDependency({"field_exit", 0x8007954c, {}, {}}, "symbol:field-exit", false,
                                "Only the battle (0) and map-change (3) exits are recovered");
    resident.w_4f310 = 0;
    resident.w_4f30c = 0;
    if (resident.w_4f370 != 0)
        return false;
    if ((state.exit_mode & 0x80U) != 0)
        throw MissingDependency({"field_exit_prepare", 0x8001bb50, {}, {}},
                                "symbol:field-exit-8001bb50", false,
                                "Exit mode bit 80 calls the unrecovered 8001bb50");
    set_next_mode(state.exit_mode & 0x7fU);
    return true;
}

void Program::finish_battle_mode(std::uint32_t outcome, std::uint32_t mode_flag) {
    // The persistent map selector 8006f94e and its neighbours, in game data.
    const auto halfword = [&](std::uint32_t address) -> std::uint8_t * {
        const auto offset = address - resident.game_state;
        if (resident.game_data.size() != game_data_bytes || offset + 2 > game_data_bytes)
            throw field::FieldFormatError("The battle epilogue requires the game data");
        return resident.game_data.data() + offset;
    };
    outcome &= 0xffU;
    if (outcome == 1 || outcome == 0x40 || outcome == 0x21) {
        std::uint32_t mode = 0;
        if ((mode_flag & 0xffU) != 0) {
            mode = 6;
        } else if (resident.b_5947c != 0) {
            mode = 2;
        } else {
            const auto *map = halfword(0x8006f94e);
            mode = ((map[0] | static_cast<std::uint32_t>(map[1]) << 8U) & 0x7ffU) < 0x400 ? 1 : 3;
        }
        set_next_mode(mode);
    } else if (outcome == 0x81) {
        resident.w_4f30c = 0; // 8001ac94
        for (const auto [address, value] : {std::pair{0x8006f94eU, 0x1eaU},
                                            {0x8006f950U, 0U},
                                            {0x8006f952U, 0U},
                                            {0x8006f954U, 0U}}) {
            auto *bytes = halfword(address);
            bytes[0] = static_cast<std::uint8_t>(value);
            bytes[1] = static_cast<std::uint8_t>(value >> 8U);
        }
        set_next_mode(1);
    }
    if (resident.b_5947c == 0)
        resident.battle_request.resident_flag = 1;
}

} // namespace xem::reconstruction
