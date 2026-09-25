// The field main loop 80077e88 of field overlay 38a1ce82... leaving for a
// battle: its battle branch (80078334..80078494, 80078428..80078494) and,
// once it leaves, the loop's exit (80078abc..80078b34): the field's
// suspension, its teardown (800700b0), the releases after it and the field
// exit 8007954c with kind 0, which selects battle mode (2) and calls the mode
// dispatcher 80019acc.
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("Leaving the field requires loaded field state");
    return *program.field;
}
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t at) {
    return static_cast<std::uint32_t>(bytes[at]) | static_cast<std::uint32_t>(bytes[at + 1]) << 8U |
           static_cast<std::uint32_t>(bytes[at + 2]) << 16U |
           static_cast<std::uint32_t>(bytes[at + 3]) << 24U;
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
} // namespace

// 800a3f4c: save the field-return snapshot (8005a4e4): the descriptor
// count, the five fixed regions, each event actor's descriptor bytes, sprite
// checkpoint (80021ebc), record and extensions, the variable bank and the
// party modes (8005a408). 800afc50 is left after the written bytes.
void Program::save_field_return() {
    auto &state = loaded(*this);
    const auto read_block = [&](std::uint32_t address, auto &out) {
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = ram_byte(address + static_cast<std::uint32_t>(i));
    };
    field::original::FieldCaptureInput input;
    input.descriptor_count = memory(0x800afb0c);
    auto &globals = input.globals;
    read_original(*this, snapshot_regions[0].address, globals.object_state);
    read_original(*this, snapshot_regions[1].address, globals.transform_state);
    read_block(memory(0x800afb20), globals.collision_attributes);
    read_original(*this, snapshot_regions[2].address, globals.field_state);
    read_original(*this, snapshot_regions[3].address, globals.camera_state);
    const auto count = memory(0x800adbfc);
    if (count > state.actors.size())
        throw field::FieldFormatError("The snapshot's actor count exceeds the loaded actors");
    std::vector<field::original::ActorCaptureInput> actors(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto &actor = state.actors[i];
        auto &out = actors[i];
        std::copy_n(actor.descriptor.begin() + 0x50, 8, out.descriptor_auxiliary.begin());
        out.descriptor_flags = word(actor.descriptor, 0x58) & 0xffffU;
        out.actor = actor.storage;
        const auto sprite = word(actor.descriptor, 4);
        read_block(sprite, out.sprite.sprite);
        read_block(memory(sprite + 0x7c), out.sprite.sequencer);
        read_block(memory(sprite + 0x20), out.sprite.settings);
        out.extension_110 = actor.extension_110;
        out.extension_114 = actor.extension_114;
    }
    input.actors = actors;
    input.variables = resident.variables.words;
    input.party_modes = resident.party_modes();
    auto result = field::original::capture_field_return(input, resident.field_snapshot);
    resident.field_snapshot = std::move(result.storage);
    for (std::size_t i = 0; i < 3; ++i)
        resident.saved_party_modes[i] = result.saved_party_modes[i];
    state.snapshot_cursor = 0x8005a4e4U + static_cast<std::uint32_t>(result.bytes_used);
    // 800379c8 prints the snapshot's extent in a diagnostic build.
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"save_field_return", 0x800a4728, {}, {}},
                                "symbol:printf-800379c8", false,
                                "The diagnostic snapshot message is not reconstructed");
}

// 80078334..80078494 of 80077e88, reached with draw buffer 1, the disc idle
// and 80077e10 clear: release the read-ahead map data, save the current
// music once, close the dialogue windows, then either take the battle's
// music (800adbd0 1: the loop goes on) or leave (the snapshot unless
// 800adb18, the fade of the playing sequence after a music change).
bool Program::field_battle_start() {
    auto &state = loaded(*this);
    auto &heap = resident.heap;
    if (s32(resident.preload_slot) != -1) {
        // 800320b8 clears the block's keep flag, then 800320e8 releases it;
        // 8005a4e0 keeps naming the released block.
        auto &block = resident.preload_block;
        const auto found = heap.headers.find(block.address - 8);
        if (found == heap.headers.end())
            throw field::FieldFormatError("The read-ahead block has no heap header");
        found->second[1] &= ~resident::heap_keep;
        const auto address = block.address;
        if (resident::heap_release(heap, block, 0x80078360) != 0)
            throw field::FieldFormatError("The read-ahead block was not released");
        block = {address, {}};
    }
    if (!state.music_saved) {
        state.saved_music = resident.music.requested;
        state.music_saved = true;
    }
    close_dialogues(); // 8007ffe8
    if (state.w_adbd0 == 1) {
        resident.battle_request.mode = static_cast<std::uint8_t>(memory(0x800b2355, 1));
        state.saved_music = resident.music.requested;
        const auto music = static_cast<std::uint32_t>(s16(memory(0x800b2290, 2)));
        if (resident.music.loaded_sequence != music) {
            if (s32(resident.music.loaded_sequence) != -1)
                resident.music.reuse_sequence = 1;
            stop_music(); // 8001b66c
            resident.music.gate = 0xffffffffU;
            resident.music.requested = music;
            load_music(music); // 80085b20(music, 1)
        }
        state.w_adbd0 = 0;
        set_memory(0x800adbd4, 1);
        return false;
    }
    if (memory(0x800adb18) == 0) {
        ++resident.w_4f30c;
        save_field_return(); // 800a3f4c
    }
    if (memory(0x800adbd4) == 1)
        sequence_volume(resident.music.current_sequence, 0x7f, 0); // 8003a89c
    set_memory(0x800adbd4, 0);
    return true;
}

// 80078abc up to the teardown (800700b0): the battle-entry flag (800798bc),
// the particles' saved VRAM (800a91f0), the play record (800a31e8), the
// particles (800a9460), the emitters (800864f0), the dialogue windows
// (8007ffe8), then DrawSync(0) and VSync(0).
void Program::field_battle_leave(FrameServices &services) {
    auto &state = loaded(*this);
    // 800798bc: 1 without a reassigned party, else whether the controlled
    // actor has flag c0 (+14); 800b234c overrides it unless ff.
    std::uint8_t flag = 1;
    if (state.party_reassignment != 0) {
        const auto index = static_cast<std::size_t>(state.controlled_actor);
        if (index >= state.actors.size())
            throw field::FieldFormatError("The battle-entry flag needs the controlled actor");
        flag = (word(state.actors[index].storage, 0x14) & 0xc0U) != 0 ? 1 : 0;
    }
    resident.b_59179 = flag;
    if (const auto value = memory(0x800b234c, 2); s16(value) != 0xff)
        resident.b_59179 = static_cast<std::uint8_t>(value);
    restore_particle_vram(services); // 800a91f0
    record_play_state();             // 800a31e8
    stop_particles(services);        // 800a9460
    stop_emitters();                 // 800864f0
    close_dialogues();               // 8007ffe8
    draw_sync(services);             // 800445d0(0)
    vertical_sync(services);         // 8004b54c(0)
}

// 80078b04..80078b2c after the teardown: release the party sprite blocks
// (80077d2c), unlink and release the field's effect bank (80085988), clear
// 8004f31c and release the block 800adb30 names.
void Program::field_battle_release(std::uint32_t block) {
    auto &heap = resident.heap;
    const auto unkeep = [&](std::uint32_t address) { // 800320b8
        const auto found = heap.headers.find(address - 8);
        if (found == heap.headers.end())
            throw field::FieldFormatError("A released block has no heap header");
        found->second[1] &= ~resident::heap_keep;
    };
    for (const auto address : resident.party_sprite_blocks)
        unkeep(address);
    const std::array<std::uint32_t, 3> sites{0x80077d70, 0x80077d80, 0x80077d90};
    for (std::size_t i = 0; i < 3; ++i)
        static_cast<void>(release_owned_block(resident.party_sprite_blocks[i], sites[i]));
    const auto bank = resident.field_effect_bank;
    resident::unlink_effect_bank(resident.sound, bank); // 8003852c
    unkeep(bank);
    {
        // The bank is a driver object until its block is released.
        auto &objects = resident.sound.objects;
        const auto found = objects.find(bank);
        if (found == objects.end())
            throw field::FieldFormatError("The field's effect bank is not a driver object");
        resident::HeapBlock released{bank, std::move(found->second)};
        objects.erase(found);
        if (resident::heap_release(heap, released, 0x800859b8) != 0)
            throw field::FieldFormatError("The field's effect bank was not released");
    }
    resident.w_4f32c = 0xffffffffU;
    resident.party_sprite_load.loaded = 0;
    static_cast<void>(release_owned_block(block, 0x80078b24));
}

// 80078abc..80078b34: leave the field for battle mode. Returns true where
// the original calls the mode dispatcher 80019acc(0) from 8007954c.
bool Program::leave_field_for_battle(FrameServices &services, const ProgramObserver &observe) {
    const auto done = [&](std::string_view operation, std::uint32_t address) {
        if (observe)
            observe(*this, {operation, address, {}, {}}, true);
    };
    field_battle_leave(services);
    done("field_battle_leave", 0x800700b0);
    field_teardown(services); // 800700b0
    done("field_teardown", 0x80078b04);
    field_battle_release(loaded(*this).w_adb30);
    done("field_battle_release", 0x8007954c);
    return exit_field(0); // 8007954c(0)
}

} // namespace xem::reconstruction
