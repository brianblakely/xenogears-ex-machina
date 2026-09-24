#pragma once

#include "xem/reconstruction/field_collision.hpp"
#include "xem/reconstruction/field_events.hpp"
#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_sprite.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace xem::reconstruction::field {

// Resident read-only lookup tables, copied from the original executable image:
// sine/cosine pairs 800523f0, square root 80056a00, reciprocal 80056b94 and
// arctangent 80057030. They are original data supplied by the caller.
struct MathTables {
    std::vector<std::uint8_t> trigonometry;
    std::vector<std::int16_t> square_root, reciprocal, angle;
    [[nodiscard]] CollisionTables collision() const noexcept {
        return {reciprocal, square_root, angle, trigonometry};
    }
};

// One of four original dialogue windows, in original layout at 800c26b0 +
// 0x498 * index. Offsets name the fields recovered handlers use.
struct DialogueWindow {
    static constexpr std::uint32_t base = 0x800c26b0;
    static constexpr std::uint32_t stride = 0x498;
    static constexpr std::size_t status = 0x364;  // 800c2a14: zero while displayed
    static constexpr std::size_t busy = 0x3f6;    // 800c2aa6
    static constexpr std::size_t cleared = 0x3fc; // 800c2aac: cleared when its owner hides
    static constexpr std::size_t owner = 0x3fe;   // 800c2aae: owning event actor index
    static constexpr std::size_t speaker = 0x400; // 800c2ab0: actor whose flag 200 is tested
    std::array<std::uint8_t, stride> bytes{};
    [[nodiscard]] std::int16_t half(std::size_t offset) const {
        return static_cast<std::int16_t>(bytes.at(offset) | bytes.at(offset + 1) << 8U);
    }
    void set_half(std::size_t offset, std::uint16_t value) {
        bytes.at(offset) = static_cast<std::uint8_t>(value);
        bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8U);
    }
    bool operator==(const DialogueWindow &) const = default;
};

// Screen fade parameters written by 80071dcc/80071e58 and the fade tile
// packets built by 8007d93c (800adc04..800b2118 block).
struct FieldFade {
    std::array<std::uint8_t, 32> packets{}; // 800b20dc: two 16-byte GPU tile packets
    std::uint32_t mode{};                   // 800adc04; a fade starts only in mode 2
    std::int16_t started{};                 // 800adc08
    std::array<std::uint32_t, 6> words{};   // 800b20fc, 2100, 2104, 2108, 210c, 2110
    std::array<std::uint16_t, 3> halves{};  // 800b2114, 2116, 2118
    bool operator==(const FieldFade &) const = default;
};

// Field camera state. Event instructions (a0, 9d, fe 53) write parameters; the
// move phase (800739c0 and its callees) consumes and advances the rest.
// Positions are 16.16 fixed-point words.
struct FieldCamera {
    std::int16_t mode{};          // 800af934
    std::int32_t target_a{};      // 800af984, target follow divisor
    std::int32_t target_b{};      // 800af988, eye follow divisor
    std::uint32_t flags{};        // 800af9d8
    std::uint16_t heading_half{}; // 800af9e6
    std::uint32_t heading_high{}; // 800af9f0, heading accumulator
    std::int32_t projection{};    // 800af9f8; also loaded into the GTE H register
    std::uint16_t elevation{};    // 800af9fc
    std::int16_t distance{};      // 800af9fe
    std::uint32_t heading{};      // 800afa0c, heading goal
    std::uint16_t steps{};        // 800afa1c, distance interpolation
    std::int32_t start{};         // 800afa20
    std::int32_t step{};          // 800afa24
    std::uint32_t counter{};      // 800b21d8

    GteMatrix view{};                             // 800af85c
    GteLong eye{};                                // 800af880
    GteLong target{};                             // 800af890
    GteLong up{};                                 // 800af8a0
    GteLong eye_goal{};                           // 800af8b0
    GteLong target_goal{};                        // 800af8c0
    GteLong shake_offset{};                       // 800af8e0
    GteLong saved_target{};                       // 800af8f0: target goal saved by primary 60
    GteLong point_actor_a{};                      // 800af900: an actor position (primary 62/63)
    GteLong saved_eye{};                          // 800af910: eye goal saved by primary 64/65
    GteLong point_actor_b{};                      // 800af920: an actor position (primary 66)
    std::int32_t scripted_zoom{};                 // 800af930: projection * distance >> 12
    std::uint16_t scripted_elevation{};           // 800af936
    std::uint16_t scripted_heading{};             // 800af938
    std::uint16_t scripted_scale{};               // 800af93a
    std::uint16_t scripted{};                     // 800af93c: bit 0 target, bit 1 eye
    std::int16_t target_steps{};                  // 800af93e
    GteLong scripted_target{};                    // 800af940
    GteLong target_step{};                        // 800af950
    std::int16_t eye_steps{};                     // 800af960
    GteLong scripted_eye{};                       // 800af964
    GteLong eye_step{};                           // 800af974
    std::int16_t view_angle{};                    // 800af98e
    GteMatrix previous_view{};                    // 800af990
    GteMatrix orbit{};                            // 800af9b0
    GteVector orbit_angles{};                     // 800af9d0
    std::array<std::int16_t, 4> bounds{};         // 800af9dc: x, z, x extent, z extent
    std::int16_t heading_x{};                     // 800af9e4; 800af9e6 is heading_half
    std::int16_t heading_z{};                     // 800af9e8
    std::int32_t heading_velocity{};              // 800af9ec
    std::array<std::uint8_t, 2> heading_blocks{}; // 800af9f4
    std::int16_t heading_steps{};                 // 800af9f6
    std::uint16_t elevation_steps{};              // 800afa00
    std::int32_t elevation_value{};               // 800afa04
    std::int32_t elevation_step{};                // 800afa08
    std::int16_t projection_steps{};              // 800afa10
    std::int32_t projection_value{};              // 800afa14
    std::int32_t projection_step{};               // 800afa18
    std::int16_t shake{};                         // 800afa28
    std::int16_t shake_time{};                    // 800afa2a
    std::int16_t shake_stop{};                    // 800afa2c
    GteLong shake_amplitude{};                    // 800afa30
    GteLong shake_step{};                         // 800afa3c
    GteVector world_angles{};                     // 800afa54
    GteVector anchor{};                           // 800afa5c
    GteMatrix scaled_world{};                     // 800afa64; t (800afa78) is the anchor in view
    std::int32_t scale{};                         // 800afac4
    bool operator==(const FieldCamera &) const = default;
};

// Original-layout records of an event actor used by handlers that address
// several actors. Addresses are correlations, never host pointers.
struct FieldRecord {
    std::span<std::uint8_t, 0x138> actor;
    std::span<std::uint8_t, 0x5c> descriptor;
    SpriteWindow sprite{};
};

// Shared field state for handlers beyond the core event context. The caller
// must write its semantic EventActor copies to these records before a call
// and reload them afterwards: these handlers act on original-layout records.
struct FieldWorld {
    EventProgram program;
    EventVariables &variables;
    EventControl &control;
    std::span<FieldRecord> actors;
    std::size_t current{};               // 800afd1c / 800b0078
    std::size_t current_descriptor{};    // 800b06b8
    std::array<std::int32_t, 3> party{}; // 8005a444..8005a44c
    std::int32_t controlled{};           // 800b226c
    std::span<const std::uint8_t> zones; // Field component 8 (800adbf4), 24-byte records
    std::span<DialogueWindow, 4> dialogue;
    std::uint16_t &script_flag_b236c; // Written by extended 99; consumer unrecovered
    const MathTables &math;
    const CollisionPackage &collision;
    std::array<std::int32_t, 4> triangle_counts{}; // 800afb44: per-layer active triangles
    std::int16_t layer_count{};                    // 800afb54
    FieldFade &fade;
    FieldCamera &camera;
    std::int16_t &encounter_inhibition;              // 800b2176
    std::array<std::uint8_t, 2> &script_flags_b21d0; // 800b21d0/800b21d1
    GteScreen &gte_screen;                           // GTE OFX/OFY/H
    // Overlay constants: 16 facings at 800aea34, then 8 directions at 800aea54.
    const std::array<std::uint16_t, 24> &direction_tables;
};

// Primary 07, 29, 2c, 3c, 57, 5a, 5f, 60, 61, 62, 63, 64, 65, 66, 87, 89, 99, 9a, 9c, 9d, a0, b3,
// b4, c9, d9, de, df, ef and f2; unsupported opcodes throw UnsupportedInstruction. Every handler
// advances or replaces the working PC at actor +cc exactly as the original does, including its
// batch-limit bump.
// Primary 5b is 5a without its final PC advance.
void execute_script_instruction(FieldWorld &world, std::uint8_t opcode);
// Extended FE xx after run_extended_event has advanced the PC to xx.
void execute_script_extended(FieldWorld &world, std::uint8_t opcode);

// Field 800acdec: operand at PC + offset; bit 15 selects an unsigned 15-bit
// immediate, otherwise a variable reference.
[[nodiscard]] std::int32_t read_immediate15_or_variable(const FieldWorld &world,
                                                        std::uint32_t offset);
// Fields 8009cf78 / 8009cfbc / 8009d000: `bit` of `flags` selects a signed
// immediate at PC + offset, otherwise a variable reference there.
[[nodiscard]] std::int32_t read_selected(const FieldWorld &world, std::uint32_t offset,
                                         std::uint32_t flags, std::uint32_t bit);
// Field 8009cdb4: ff/fe/fd select party slots, fb the current actor.
[[nodiscard]] std::int32_t resolve_script_actor(const FieldWorld &world, std::uint32_t offset);
// Field 80099a04: GTE Square0 of signed halfword components, sum, 80048c4c.
[[nodiscard]] std::int32_t script_distance(std::int32_t dx, std::int32_t dy, std::int32_t dz,
                                           std::span<const std::int16_t> square_root);

} // namespace xem::reconstruction::field
