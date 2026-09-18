#include "xem/reconstruction/field_control.hpp"
#include "xem/reconstruction/field_motion.hpp"

#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Operation> void rejects(Operation operation, const char *message) {
    bool failed = false;
    try {
        operation();
    } catch (const field::SpriteError &) {
        failed = true;
    } catch (const field::EventError &) {
        failed = true;
    } catch (const field::ActorInitializationError &) {
        failed = true;
    }
    check(failed, message);
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
struct Fixture {
    std::array<std::uint8_t, 0xb4> bytes;
    std::array<std::uint8_t, 0x4000> table{};
    Fixture() {
        bytes.fill(0xa5);
        put(bytes, 0x18, 16);
        put(bytes, 0x32, 7, 2);
        put(bytes, 0x82, 4096, 2);
        put(bytes, 0xac, 256U << 7U);
        pair(7, 4, -4);
    }
    field::SpriteWindow sprite() { return {0x80120000, bytes}; }
    void pair(std::size_t angle, std::int16_t sine, std::int16_t cosine) {
        put(table, angle * 4, static_cast<std::uint16_t>(sine), 2);
        put(table, angle * 4 + 2, static_cast<std::uint16_t>(cosine), 2);
    }
};

// Invented signed/width/branch boundaries. No original table or capture bytes
// are embedded; independent original replay is performed separately.
void mode_prefix() {
    field::MotionControl control{99, 0x40};
    field::FieldPassState pass{1, 91};
    auto result = field::begin_field_motion(0x01005800, 2, -7, control, pass);
    check(!result.active_body_mode && control.current_actor_index == -7 &&
              control.held_buttons == 0x40 && pass == field::FieldPassState{1, 91},
          "Inhibition must follow the sole actor-index store");
    check(field::begin_field_motion(0x4000, 0, 4, control, pass).active_body_mode == 2,
          "Owner, Circle and exact input-updated one select run");
    for (auto updated : {0U, 2U, 0xffffffffU}) {
        pass.input_updated = updated;
        check(field::begin_field_motion(0x4000, 2, 0, control, pass).active_body_mode == 1,
              "Other input-update words select walk");
    }
    pass.input_updated = 1;
    control.held_buttons = 0;
    check(field::begin_field_motion(0x4000, 2, 0, control, pass).active_body_mode == 1,
          "Circle must be held");
    control.held_buttons = 0x40;
    check(field::begin_field_motion(0, 2, 0, control, pass).active_body_mode == 1,
          "Ownership is required");
    for (auto flags : {0x800U, 0x1000U, 0x1800U}) {
        for (std::int16_t old : std::array<std::int16_t, 2>{1, 2})
            check(field::begin_field_motion(flags | 0x4000U, old, 0, control, pass)
                          .active_body_mode == old,
                  "Air flags preserve only old modes one and two");
        for (std::int16_t old : std::array<std::int16_t, 3>{-1, 0, 3})
            check(field::begin_field_motion(flags | 0x4000U, old, 0, control, pass)
                          .active_body_mode == 2,
                  "Other signed old modes do not override selection");
    }
}

void signed_speed_and_trigonometry() {
    check(field::animation_command_speed(255, 4097, 0) == -4096 &&
              field::animation_command_speed(1, 4097, 0) == 4096 &&
              field::animation_command_speed(255, 255, 0) == 0 &&
              field::animation_command_speed(128, 4096, 0) == -524288,
          "Signed operands and division truncation must match source");
    check(field::animation_command_speed(3, -4096, 0) == -12288 &&
              field::animation_command_speed(127, 4096, 2147483647) == 0 &&
              field::animation_command_speed(99, 123, -1) == 0 &&
              field::animation_command_speed(127, 32767, 100) == -116568576,
          "Each speed product and rate increment must retain low-word wrap");
    Fixture f;
    f.pair(4095, -32768, 32767);
    for (auto angle : {4095U, 8191U, 0xffffffffU})
        check(field::planar_trigonometry(f.table, angle) ==
                  field::PlanarTrigonometry{-32768, 32767},
              "Trig must mask twelve angle bits and sign-extend both halfwords");
    rejects(
        [&] { static_cast<void>(field::planar_trigonometry(std::span(f.table).first(16383), 0)); },
        "Incomplete original table must fail");
}

void vector_widths() {
    using V = field::SpritePlanarVector;
    check(field::sprite_planar_vector(-1, 3U << 7U, {0, 256}) == V{-85, -85, 0} &&
              field::sprite_planar_vector(15, 3U << 7U, {0, 256}) == V{0, 0, 0},
          "Speed shift floors before scalar division truncates");
    check(field::sprite_planar_vector(16, 256U << 7U, {4, -1}) == V{1, -1, -1} &&
              field::sprite_planar_vector(16, 256U << 7U, {-1, 4}) == V{1, 0, 0},
          "Z negation must precede its final arithmetic shift");
    check(field::sprite_planar_vector(1 << 27, 1U << 7U, {8, 8}) == V{-2147483647 - 1, 0, 0} &&
              field::sprite_planar_vector(1 << 26, 1U << 7U, {8, 8}) ==
                  V{1073741824, -33554432, -33554432},
          "Scalar, products and negation retain wrap boundaries");
    check(field::sprite_planar_vector(32, (256U << 7U) | 0xf800007fU, {256, 256}) ==
              field::sprite_planar_vector(32, 256U << 7U, {256, 256}),
          "Only the packed twelve-bit divisor participates");
    rejects([] { static_cast<void>(field::sprite_planar_vector(0, 0xf800007fU, {0, 0})); },
            "Zero divisor is explicitly unresolved");
}

void sprite_stores_and_speed_order() {
    Fixture f;
    auto expected = f.bytes;
    put(expected, 0x0c, 0xffffffffU);
    put(expected, 0x14, 0xffffffffU);
    check(field::rebuild_sprite_velocity(f.sprite(), f.table) ==
                  field::SpritePlanarVector{1, -1, -1} &&
              f.bytes == expected,
          "Vector writes preserve vertical velocity, direction, speed and every other byte");

    f = Fixture{};
    f.pair(7, 0, 4096);
    expected = f.bytes;
    put(expected, 0x18, 4096);
    put(expected, 0x0c, 4096);
    put(expected, 0x14, 0);
    check(field::apply_animation_speed(f.sprite(), 1, 0, f.table) ==
                  field::SpritePlanarVector{256, 4096, 0} &&
              f.bytes == expected,
          "A0 connects its new speed to the real vector routine without changing PC");

    f = Fixture{};
    put(f.bytes, 0xac, 0);
    expected = f.bytes;
    put(expected, 0x18, 4096);
    rejects([&] { static_cast<void>(field::apply_animation_speed(f.sprite(), 1, 0, {})); },
            "Unresolved vector inputs must not become a successful A0 command");
    check(f.bytes == expected, "Speed store must survive failure in the following vector call");
}

void field_velocity_boundaries() {
    Fixture f;
    auto expected = f.bytes;
    put(expected, 0x0c, 0xfffff000U);
    put(expected, 0x14, 0xfffff000U);
    const auto effect = field::update_field_party_velocity(f.sprite(), 7, 0x40, 0, f.table);
    check(effect ==
                  field::FieldPlanarVector{7, -4096, -4096, field::SpritePlanarVector{1, -1, -1}} &&
              f.bytes == expected,
          "Field X/Z quantization floors negative components and preserves other bytes");
    f = Fixture{};
    put(f.bytes, 0x18, 8192);
    f.pair(7, -32768, 32767);
    const auto positive = field::update_field_party_velocity(f.sprite(), 7, 0x40, 0, f.table);
    check(positive.x == 61440 && positive.z == 65536,
          "Positive field components retain their source quantization");

    f = Fixture{};
    put(f.bytes, 0x32, 0xffff, 2);
    put(f.bytes, 0xac, 0);
    expected = f.bytes;
    put(expected, 0x0c, 0);
    put(expected, 0x14, 0);
    check(field::update_field_party_velocity(f.sprite(), 0x8001, 0x40, std::nullopt, {}) ==
                  field::FieldPlanarVector{-1, 0, 0, std::nullopt} &&
              f.bytes == expected,
          "Stop sentinel precedes actor flags, divisor and trig while preserving angle");
    for (auto flags : {0x2000U, 0x80000U, 0x82000U})
        rejects(
            [&] {
                static_cast<void>(
                    field::update_field_party_velocity(f.sprite(), 1, 0x40, flags, {}));
            },
            "Alternate actor paths must fail explicitly");
    rejects(
        [&] {
            static_cast<void>(field::update_field_party_velocity(f.sprite(), 0x8000, 0, 0, {}));
        },
        "Descriptor ratio path precedes sentinel and remains unsupported");
    check(f.bytes == expected, "Unsupported branch selection must not mutate the sprite");
    rejects(
        [&] { static_cast<void>(field::update_field_party_velocity(f.sprite(), 13, 0x40, 0, {})); },
        "Direction store does not complete an unresolved vector call");
    put(expected, 0x32, 13, 2);
    check(f.bytes == expected, "Direction is stored before the nested vector error");
}

void committed_command_pc() {
    Fixture f;
    std::array<std::uint8_t, 256> widths{};
    widths[163] = 255;
    put(f.bytes, 0x64, 0xfffffffeU);
    auto expected = f.bytes;
    put(expected, 0x64, 253);
    check(field::store_sprite_command_pc(f.sprite(), 163, widths) == 253 && f.bytes == expected,
          "PC store uses unsigned width and wraps the post-handler word");
    check(field::store_sprite_command_pc(f.sprite(), 162, widths) == 253 && f.bytes == expected,
          "Source width zero is preserved without inventing handler behavior");
    rejects(
        [&] {
            static_cast<void>(
                field::store_sprite_command_pc(f.sprite(), 163, std::span(widths).first(255)));
        },
        "Incomplete source widths must fail");
}

void event_divisor_and_continuation() {
    Fixture f;
    std::vector<std::uint8_t> code{0x21, 0xff, 0xff, 0};
    field::EventActor actor;
    field::EventVariables variables;
    field::EventContext context;
    context.program = {code, {}};
    context.current_actor = &actor;
    context.variables = &variables;
    context.control.budget_mode = 1;
    std::uint16_t divisor = 0x1234;
    auto expected = f.bytes;
    put(expected, 0xac, 0x7ff80);
    const auto batch =
        field::run_event_batch(context, 8, [&](field::EventContext &current, std::uint8_t opcode) {
            if (opcode == 0x21)
                field::execute_motion_divisor(current, divisor, f.sprite());
            else
                field::execute_core_event(current, opcode);
        });
    check(divisor == 0x7fff && f.bytes == expected && actor.pc == 3 &&
              batch.reason == field::BatchExit::handler_break && batch.dispatched == 2,
          "Divisor handler composes with real event dispatch and end continuation");
    for (bool unsigned_value : {false, true}) {
        actor.pc = 0;
        code[1] = 0xff;
        code[2] = 7;
        variables.words[1023] = 0xff80;
        variables.unsigned_bitmap[127] = unsigned_value ? 0x80 : 0;
        put(f.bytes, 0xac, 0xf807007fU);
        field::execute_motion_divisor(context, divisor, f.sprite());
        check(divisor == 0xff80 && get(f.bytes, 0xac) == 0xf807c07fU && actor.pc == 3,
              "Typed odd variable alias retains full actor halfword and packed low twelve bits");
    }
    actor.pc = 0;
    code[1] = 0;
    code[2] = 0x80;
    context.variables = nullptr;
    field::execute_motion_divisor(context, divisor, f.sprite());
    check(divisor == 0 && (get(f.bytes, 0xac) & 0x7ff80U) == 0,
          "Zero setter is legal; its later division remains a distinct unsupported path");
    actor.pc = 0;
    code[1] = 7;
    rejects([&] { field::execute_motion_divisor(context, divisor, {0, {}}); },
            "Missing sprite state must reject after the actor store");
    check(divisor == 7 && actor.pc == 0,
          "Failed sprite call retains divisor but not continuation PC");

    code.assign(65536, 0);
    code[65533] = 0x21;
    code[65534] = 9;
    code[65535] = 0x80;
    context.program = {code, {}};
    actor.pc = 65533;
    field::execute_motion_divisor(context, divisor, f.sprite());
    check(divisor == 9 && actor.pc == 0, "Event PC halfword store wraps after valid operand reads");
    actor.pc = 65535;
    code[65535] = 0x21;
    rejects([&] { field::execute_motion_divisor(context, divisor, f.sprite()); },
            "Operand addresses must not wrap with the event PC");
    check(divisor == 9 && actor.pc == 65535, "Unavailable operands issue no divisor store");
    actor.pc = 0;
    rejects([&] { field::execute_motion_divisor(context, divisor, f.sprite()); },
            "Unrecognized field opcode must fail explicitly");
}

void impulse_boundaries() {
    Fixture f;
    put(f.bytes, 0xa8, 0);
    put(f.bytes, 0x82, 4097, 2);
    auto expected = f.bytes;
    put(expected, 0x10, static_cast<std::uint32_t>(-4096));
    check(field::apply_animation_impulse(f.sprite(), 255, 0) ==
                  field::SpriteImpulse{-4096, -1048576, false} &&
              f.bytes == expected,
          "Impulse sign extension, truncation and sole velocity store");
    std::array<std::uint8_t, 4> reference{};
    put(reference, 0, static_cast<std::uint32_t>(-5));
    put(f.bytes, 0xa8, 1);
    put(f.bytes, 0x7c, 0x80130000);
    put(f.bytes, 0xac, 7U << 7U);
    check(field::apply_animation_impulse(f.sprite(), 91, 987,
                                         field::SpriteResource{0x80130000, reference}) ==
              field::SpriteImpulse{-182, -1280, true},
          "Nonzero reference bypasses only operand scaling");
    expected = f.bytes;
    rejects([&] { static_cast<void>(field::apply_animation_impulse(f.sprite(), 1, 0)); },
            "Flagged reference must be supplied");
    rejects(
        [&] {
            static_cast<void>(field::apply_animation_impulse(
                f.sprite(), 1, 0, field::SpriteResource{0x80130004, reference}));
        },
        "Reference address must equal original pointer");
    check(f.bytes == expected, "Missing reference must not issue a velocity store");
    put(reference, 0, 0);
    put(f.bytes, 0xac, 256U << 7U);
    check(field::apply_animation_impulse(f.sprite(), 255, 0,
                                         field::SpriteResource{0x80130000, reference}) ==
              field::SpriteImpulse{-4096, -1048576, false},
          "Explicit zero reference falls through to operand scaling");
    put(f.bytes, 0xac, 0xf800007fU);
    rejects(
        [&] {
            static_cast<void>(field::apply_animation_impulse(
                f.sprite(), 255, 0, field::SpriteResource{0x80130000, reference}));
        },
        "Impulse zero divisor must fail");
    check(get(f.bytes, 0x10) == static_cast<std::uint32_t>(-4096),
          "Zero divisor preserves the scaled value stored before original division");
    put(f.bytes, 0xa8, 0);
    put(f.bytes, 0xac, 128);
    check(field::apply_animation_impulse(f.sprite(), 127, 2147483647).velocity == 0,
          "Rate increment and low-word impulse products wrap");
}

void vertical_boundaries() {
    using S = field::VerticalState;
    using B = field::VerticalBranch;
    auto result = field::field_vertical_step({655359, 0, 0, 0}, 1, 10, 0, 0, 0);
    check(result.state == S{655359, 1, 0x1000, 1} && result.branch == B::airborne,
          "Signed high halfword floor comparison preserves fractional Y");
    check(field::field_vertical_step({-65536, 0, 0x04000000, 91}, 7, 0, 2, 2, 0).state ==
                  S{0, 0, 0, 0} &&
              field::field_vertical_step({-65536, 0, 0x04000000, 91}, 7, 0, 2, 1, 0).state ==
                  S{-65536, 7, 0x1000, 7},
          "Layer change clears forced floor before comparison");
    for (auto terrain : {0U, 0x00400000U, 0x00020000U, 0x1000U}) {
        const auto marker = (terrain & 0x00420000U) != 0 ? 123 : 0;
        check(
            field::field_vertical_step({100000, -1, 0xffffffff, 123}, 99, 0, 0, 0, terrain).state ==
                S{0, -1, 0xfbbfefffU, marker},
            "Floor preserves upward velocity and terrain-selected marker");
    }
    result = field::field_vertical_step({0x7fff0000, 0x20000, 0, 0}, 0x7fffffff, 0, 0, 0, 0);
    check(result.state == S{-2147418112, -2147352577, 0x1000, -2147352577},
          "Position and gravity additions wrap before decisions");
}

void connected_jump() {
    Fixture f;
    std::array<std::uint8_t, 6> header{4, 0, 4, 0, 0, 0};
    const std::array<field::SpriteResource, 1> resources{{{0x800c0000, header}}};
    field::SpriteSources sources{resources, {}, {}, {}, std::nullopt};
    field::SpriteEnvironment environment{};
    field::install_sprite_gravity(f.sprite(), 0x800c0000, environment, sources);
    put(f.bytes, 0xa8, get(f.bytes, 0xa8) & ~1U);
    check(get(f.bytes, 0x1c) == 1024 &&
              field::apply_animation_impulse(f.sprite(), 255, 0).velocity == -4096,
          "Real animation header gravity composes with impulse handler");
    put(f.bytes, 0x84, 7, 2);
    std::array<std::uint8_t, 0x138> actor;
    actor.fill(0xa5);
    put(actor, 0, 0x400800);
    put(actor, 4, 0);
    put(actor, 8, 0, 2);
    put(actor, 0x10, 0, 2);
    put(actor, 0x24, 7U << 16U);
    put(actor, 0xf0, 99);
    field::CollisionPackage mesh;
    mesh.layers.resize(1);
    mesh.layers[0].triangles.resize(1);
    mesh.layers[0].triangles[0].attribute_raw = 0xab01;
    mesh.attributes_raw.resize(8);
    const auto untouched = actor;
    const auto sprite_untouched = f.bytes;
    const std::array<std::int32_t, 9> displacement{-4096, -7168, -9216, -10240, -10240,
                                                   -9216, -7168, -4096, 0};
    for (std::size_t i = 0; i < displacement.size(); ++i) {
        const auto result = field::apply_field_vertical(actor, f.sprite(), 0, mesh);
        check(result.state.y == (7 << 16) + displacement[i] &&
                  result.branch ==
                      (i == 8 ? field::VerticalBranch::floor : field::VerticalBranch::airborne),
              "Connected gravity/impulse/terrain/integration lands at original selected floor");
    }
    auto expected_actor = untouched;
    put(expected_actor, 0, 0x800);
    put(expected_actor, 0xf0, 0);
    auto expected_sprite = sprite_untouched;
    put(expected_sprite, 0x10, 0);
    check(actor == expected_actor && f.bytes == expected_sprite,
          "Connected jump retains every unrelated actor and sprite byte");
    put(f.bytes, 0x10, 9);
    put(actor, 8, 0xffff, 2);
    const auto old_y = get(actor, 0x24);
    rejects([&] { static_cast<void>(field::apply_field_vertical(actor, f.sprite(), 0, mesh)); },
            "Missing current terrain triangle must fail explicitly");
    check(get(actor, 0x24) == old_y + 9,
          "Terrain failure follows original Y integration delay-slot store");
    put(actor, 4, 8);
    static_cast<void>(field::apply_field_vertical(actor, f.sprite(), 0, mesh));
    check(get(actor, 0x24) == (7U << 16U),
          "Disabled terrain layer short-circuits invalid triangle and still lands");
}

void shared_scheduler_control_motion_pass() {
    const std::array<std::uint8_t, 1> code{0x0c};
    field::EventActor actor;
    actor.flags = 0x4000;
    for (auto &slot : actor.slots)
        slot.control_bits = 15U << 18U;
    actor.slots[0].control_bits = 2U << 18U;
    std::array<field::EventDescriptor, 1> descriptors{{{0x200, &actor}}};
    field::SchedulerState scheduler;
    scheduler.descriptors = descriptors;
    scheduler.event_actor_count = 1;
    field::EventContext context;
    context.program = {code, {}};
    context.control.gate_values = {1, 1, 1};
    context.pass = {7, 9};
    field::ControlActor controlled;
    field::ControlState control_state;
    field::ControlInputs inputs;
    inputs.held_buttons = 0x40;
    inputs.dialogue_status.fill(-1);
    field::BattleRequestState battle;
    const auto result = field::schedule_actor_events(
        context, scheduler, [&](field::EventContext &current, std::uint8_t opcode) {
            check(current.pass == field::FieldPassState{},
                  "Scheduler clears the shared pass state before control dispatch");
            static_cast<void>(field::execute_control_event(current, opcode, controlled,
                                                           control_state, inputs, battle));
        });
    field::MotionControl motion{99, inputs.held_buttons};
    check(result.dispatched_actors == 1 && context.pass.input_updated == 1 &&
              field::begin_field_motion(actor.flags, 0, 0, motion, context.pass).active_body_mode ==
                  2 &&
              context.pass == field::FieldPassState{1, 0},
          "Motion consumes the actual control write in the same pass without copying state");
    descriptors[0].flags = 0;
    const auto skipped = field::schedule_actor_events(context, scheduler, {});
    check(skipped.dispatched_actors == 0 && context.pass == field::FieldPassState{} &&
              field::begin_field_motion(actor.flags, 0, 0, motion, context.pass).active_body_mode ==
                  1,
          "Following pass resets input-updated even when no actor control is dispatched");
}
} // namespace

int main() {
    try {
        mode_prefix();
        signed_speed_and_trigonometry();
        vector_widths();
        sprite_stores_and_speed_order();
        field_velocity_boundaries();
        committed_command_pc();
        event_divisor_and_continuation();
        impulse_boundaries();
        vertical_boundaries();
        connected_jump();
        shared_scheduler_control_motion_pass();
        std::cout << "Field motion: eleven source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
