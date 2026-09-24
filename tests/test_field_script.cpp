// Invented bytecode, records and tables exercise source-derived handler
// boundaries. They describe no original game content or observed result.
#include "xem/reconstruction/field_script.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
// A window with the handler-visible fields: cleared, busy, owner, speaker.
field::DialogueWindow field_window(std::uint16_t cleared, std::uint16_t busy, std::uint16_t owner,
                                   std::uint16_t speaker = 0) {
    field::DialogueWindow result;
    result.set_half(field::DialogueWindow::cleared, cleared);
    result.set_half(field::DialogueWindow::busy, busy);
    result.set_half(field::DialogueWindow::owner, owner);
    result.set_half(field::DialogueWindow::speaker, speaker);
    return result;
}
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::uint32_t read(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8U * i);
    return value;
}
void write(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value,
           std::size_t width) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}

struct Fixture {
    std::vector<std::uint8_t> code = std::vector<std::uint8_t>(64);
    std::array<std::array<std::uint16_t, 32>, 3> entries{};
    field::EventVariables variables;
    field::EventControl control;
    std::array<std::array<std::uint8_t, 0x138>, 3> actors{};
    std::array<std::array<std::uint8_t, 0x5c>, 3> descriptors{};
    std::array<std::array<std::uint8_t, 0x40>, 3> sprites{};
    std::vector<field::FieldRecord> views;
    std::vector<std::uint8_t> zones = std::vector<std::uint8_t>(48);
    std::array<field::DialogueWindow, 4> dialogue{};
    std::uint16_t flag{};
    field::MathTables math;
    field::CollisionPackage collision;
    field::FieldFade fade;
    field::FieldCamera camera;
    std::int16_t inhibition{};
    std::array<std::uint8_t, 2> flags_b21d0{};
    field::GteScreen screen{};
    std::array<std::uint16_t, 24> tables{};
    field::FieldWorld world;

    Fixture()
        : world{{code, entries},
                variables,
                control,
                {},
                0,
                0,
                {1, 2, 255},
                1,
                zones,
                dialogue,
                flag,
                math,
                collision,
                {1, 0, 0, 0},
                2,
                fade,
                camera,
                inhibition,
                flags_b21d0,
                screen,
                tables} {
        math.square_root.assign(192, 4096);
        math.reciprocal.assign(192, 4096);
        math.angle.assign(1025, 0);
        for (std::size_t i = 0; i < math.angle.size(); ++i)
            math.angle[i] = static_cast<std::int16_t>(i / 2);
        for (std::uint32_t i = 0; i < 3; ++i)
            views.push_back({actors[i], descriptors[i], {0x80100000U + 0x40U * i, sprites[i]}});
        world.actors = views;
        field::CollisionLayer layer;
        layer.vertices = {{-100, -10, -100, 0}, {100, -10, -100, 0}, {0, -10, 100, 0}};
        layer.triangles = {{{0, 2, 1}, {}, 0}};
        collision.layers = {layer};
    }
    void position(std::size_t actor, std::int16_t x, std::int16_t y, std::int16_t z) {
        write(actors[actor], 0x22, static_cast<std::uint16_t>(x), 2);
        write(actors[actor], 0x26, static_cast<std::uint16_t>(y), 2);
        write(actors[actor], 0x2a, static_cast<std::uint16_t>(z), 2);
    }
    std::uint32_t pc() const { return read(actors[0], 0xcc, 2); }
    void run(std::initializer_list<std::uint8_t> bytes) {
        std::fill(code.begin(), code.end(), 0);
        std::copy(bytes.begin(), bytes.end(), code.begin());
        write(actors[0], 0xcc, 0, 2);
        field::execute_script_instruction(world, code[0]);
    }
};

void trigger_zone_and_batch_limit() {
    Fixture f;
    const std::array<std::int16_t, 12> square{-10, 0, -10, 10, 0, -10, 10, 0, 10, -10, 0, 10};
    for (std::size_t i = 0; i < square.size(); ++i)
        write(f.zones, 24 + i * 2, static_cast<std::uint16_t>(square[i]), 2);
    f.position(1, 0, 0, 0);
    f.control.batch_limit = 8;
    f.run({0xc9, 1, 0x20, 0});
    check(f.pc() == 4 && f.control.batch_limit == 8, "Inside zone advances four bytes");
    f.position(1, 30, 0, 0);
    f.run({0xc9, 1, 0x20, 0});
    check(f.pc() == 0x20 && f.control.batch_limit == 9, "Outside jumps and extends the batch");
    bool rejected = false;
    try {
        f.run({0xc9, 2, 0, 0});
    } catch (const field::EventError &) {
        rejected = true;
    }
    check(rejected, "A zone beyond component storage must fail explicitly");
}

void proximity_and_selectors() {
    Fixture f;
    f.position(0, 0, 0, 0);
    f.position(1, 3, 4, 0);
    f.run({0x89, 0xff, 0x06, 0x80, 0x30, 0});
    check(f.pc() == 6, "Party selector ff resolves slot one and a short distance advances");
    f.run({0x89, 0xff, 0x01, 0x80, 0x30, 0});
    check(f.pc() == 0x30, "Distance not below the bound jumps");
    f.run({0x89, 0xfd, 0x40, 0x80, 0x31, 0});
    check(f.pc() == 0x31, "Empty party slot 255 takes the jump without reading positions");
    check(field::script_distance(0x10003, 4, 0, f.math.square_root) ==
              field::script_distance(3, 4, 0, f.math.square_root),
          "Square0 keeps only signed halfword components");
}

void arithmetic_and_increment() {
    Fixture f;
    f.variables.write(4, -7);
    f.run({0xde, 4, 0, 3, 0, 0x40});
    check(f.variables.read(4) == -21 && f.pc() == 6, "Multiply by signed immediate");
    f.run({0xdf, 4, 0, 0, 0, 0x40});
    check(f.variables.read(4) == -21, "Zero divisor becomes one");
    f.variables.write(6, 4);
    f.run({0xdf, 4, 0, 6, 0, 0});
    check(f.variables.read(4) == -5, "Division truncates toward zero with a variable divisor");
    f.run({0x3c, 4, 0});
    check(f.variables.read(4) == -4 && f.pc() == 3, "Increment stores the low halfword");
}

void hide_actor_and_dialogue() {
    Fixture f;
    f.dialogue[0] = field_window(5, 0, 3); // Owned by another actor.
    f.dialogue[1] = field_window(6, 1, 0); // Busy window of the current actor.
    f.dialogue[2] = field_window(7, 0, 0);
    f.dialogue[3] = field_window(9, 0, 0);
    f.run({0x29, 2});
    check((read(f.actors[2], 0, 4) & 1U) != 0 && (read(f.actors[2], 4, 4) & 0x100000U) != 0 &&
              (read(f.descriptors[2], 0x58, 2) & 0x20U) != 0,
          "Hidden actor loses event and descriptor eligibility");
    check(f.dialogue[0].half(field::DialogueWindow::cleared) == 5 &&
              f.dialogue[1].half(field::DialogueWindow::cleared) == 6 &&
              f.dialogue[2].half(field::DialogueWindow::cleared) == 0 &&
              f.dialogue[3].half(field::DialogueWindow::cleared) == 9 && f.pc() == 2,
          "Only the first idle window owned by the current actor is released");
}

void extended_flag() {
    Fixture f;
    f.code[1] = 0x99;
    f.code[2] = 0;
    write(f.actors[0], 0xcc, 1, 2);
    field::execute_script_extended(f.world, 0x99);
    check(f.flag == 1 && f.pc() == 3, "Extended 99 stores its operand xor one");
}

void scripted_arc_lifecycle() {
    Fixture f;
    write(f.sprites[0], 0x1c, 0x100, 4);
    f.position(0, 0, 0, 0);
    // Setup: floor-query form (0x80), immediate X/Z/layer/steps.
    f.run({0x57, 0x80, 10, 0, 0, 0, 0, 0, 3, 0, 0xf0, 0x57, 0x03});
    check(f.pc() == 11 && f.control.break_requested == 1, "Setup advances eleven and breaks");
    check(read(f.actors[0], 0xe0, 2) == 3 && read(f.actors[0], 0x10, 2) == 0 &&
              read(f.actors[0], 8, 2) == 0,
          "Setup stores steps and layer but not the located triangle");
    check(static_cast<std::int32_t>(read(f.actors[0], 0xd0, 4)) == (10 << 16) / 4,
          "Planar steps divide by steps plus one");
    const auto velocity = static_cast<std::int32_t>(read(f.sprites[0], 0x10, 4));
    check(velocity == -(0x100 * 3 / 2) + (-10 * 65536) / 3, "Vertical impulse includes gravity");
    write(f.actors[0], 8, 0x55, 2);
    for (int step = 0; step < 3; ++step) {
        field::execute_script_instruction(f.world, 0x57);
        check(f.pc() == 11, "Stepping keeps the mode-three PC");
    }
    check((read(f.actors[0], 0x104, 2) & 0x8000U) != 0, "Moving steps publish a facing");
    field::execute_script_instruction(f.world, 0x57);
    check(f.pc() == 13 && read(f.actors[0], 0x20, 4) == (10U << 16U) &&
              static_cast<std::int16_t>(read(f.actors[0], 0x26, 2)) == -10 &&
              read(f.actors[0], 8, 2) == 0 && read(f.sprites[0], 0x10, 4) == 0,
          "Final step resolves the setup target and stores the located triangle");
    check((read(f.actors[0], 0, 4) & 0x10000U) == 0 && read(f.descriptors[0], 0x20, 4) == 10 &&
              read(f.sprites[0], 0, 4) == (10U << 16U),
          "Finish clears the motion flag and publishes descriptor and sprite positions");
    bool rejected = false;
    try {
        f.run({0x57, 0x81});
    } catch (const field::EventError &) {
        rejected = true;
    }
    check(rejected, "Unrecovered distance modes must fail explicitly");
}
void requests_and_waits() {
    Fixture f;
    for (auto &actor : f.actors)
        for (std::size_t slot = 0; slot < 8; ++slot)
            write(actor, 0x90 + slot * 8, 15U << 18U, 4);
    f.entries[2][5] = 0x1234;
    f.run({0x07, 2, 0x65});
    check(f.pc() == 3 && read(f.actors[2], 0x8c, 2) == 0x1234 && f.actors[2][0x8f] == 5 &&
              ((read(f.actors[2], 0x90, 4) >> 18U) & 15U) == 3,
          "Event request installs the entry, tag and priority in a free slot");
    for (std::size_t slot = 1; slot < 8; ++slot)
        write(f.actors[2], 0x90 + slot * 8, 3U << 18U, 4);
    f.run({0x07, 2, 0x66});
    check(f.pc() == 0, "A full target retries without advancing");
    write(f.actors[2], 4, 0x100000, 4);
    write(f.actors[0], 0x90, 0x30000U, 4);
    f.actors[0][0xcf] = 1;
    f.run({0x07, 2, 0x66});
    check(f.pc() == 3 && (read(f.actors[0], 0x90, 4) & 0x30000U) == 0 &&
              (read(f.actors[2], 0x98, 4) & 0x400000U) == 0,
          "A hidden target clears the waiting bits instead");
    for (auto &window : f.dialogue)
        window = field_window(0, 0, 7); // Owned by another actor.
    f.actors[0][0x81] = 42;
    f.control.batch_limit = 1;
    f.run({0x9c});
    check(f.pc() == 1 && f.variables.words[10] == 42 && f.control.batch_limit == 9,
          "No owned window stores +81 and extends the batch");
    f.dialogue[0] = field_window(9, 0, 0, 1);
    write(f.actors[1], 4, 0x200, 4);
    f.run({0x9c});
    check(f.pc() == 0 && f.dialogue[0].half(field::DialogueWindow::cleared) == 0 &&
              f.actors[0][0x8f] == 255 && f.control.break_requested == 1,
          "An even dialogue step ends a non-idle slot and releases the window");
}

void camera_and_fade() {
    Fixture f;
    f.camera.distance = 100;
    f.run({0x9d, 0x14, 0x80, 4});
    check(f.pc() == 4 && f.camera.steps == 4 && f.camera.start == (100 << 16) &&
              f.camera.step == (-(80 << 16)) / 4 && (f.camera.flags & 1U) != 0,
          "Camera transition divides the signed distance change by its steps");
    f.run({0x9d, 0x14, 0x80, 0});
    check(f.camera.steps == 1 && f.camera.counter == 2, "Zero steps become one and count");
    f.run({0xa0, 3, 0x80, 7, 0x80, 0x40, 0x81});
    check(f.pc() == 7 && f.camera.elevation == 7 && f.camera.heading == 7U << 9U &&
              f.camera.heading_high == 7U << 25U && f.camera.projection == 0x140 &&
              f.screen.h == 0x140,
          "Camera setup stores the octant three ways and loads the projection into H");
    f.camera.mode = 1;
    f.run({0x9a, 0, 0x80});
    check(f.pc() == 6 && f.camera.mode == 0 && f.camera.counter == 2,
          "Zero ends mode one and advances twice");
    f.camera.mode = 2;
    f.run({0x9a, 0, 0x80});
    check(f.pc() == 0, "A busy camera mode retries");
    f.fade.mode = 2;
    f.run({0xb4, 0x10, 0x80});
    check(f.pc() == 3 && f.fade.started == 1 && f.fade.words[3] == 0xff00 / 16 &&
              f.fade.halves == std::array<std::uint16_t, 3>{2, 1, 16},
          "Fade-in starts once with its rate");
    f.run({0xb3, 0x08, 0x80});
    check(f.fade.started == 0 && f.fade.words[0] == 0xff00 &&
              f.fade.words[3] == static_cast<std::uint32_t>(-0x10000 / 8) &&
              f.fade.packets[7] == 0x62 && f.fade.packets[16 + 7] == 0x62,
          "Fade-out prepares a semi-transparent tile in both buffers");
    f.run({0xb3, 0x00, 0x80});
    check(f.pc() == 3, "A repeated fade-out after the latch clears only prepares the tile");
}

void small_handlers() {
    Fixture f;
    write(f.actors[0], 4, 0x1000001, 4);
    f.run({0x2c, 5});
    check(f.pc() == 2 && read(f.actors[0], 0xea, 2) == 5 && read(f.actors[0], 4, 4) == 1,
          "Override store clears the 01000000 layer flag");
    f.control.batch_limit = 0;
    f.run({0x87, 0x09, 0x80});
    check(f.pc() == 3 && f.variables.read(0) == 9 && f.control.batch_limit == 0x20,
          "Primary 87 stores variable zero and extends the batch");
    write(f.sprites[0], 0x0c, 7, 4);
    write(f.actors[0], 0x40, 7, 4);
    write(f.actors[0], 0x104, 0x12, 2);
    f.run({0x5a});
    check(f.pc() == 1 && read(f.sprites[0], 0x0c, 4) == 0 && read(f.actors[0], 0x40, 4) == 0 &&
              read(f.actors[0], 0x106, 2) == 0x8012,
          "Stop clears motion and marks the direction idle");
    f.inhibition = 5;
    f.camera.flags = 0xffff;
    f.code[1] = 0x53;
    write(f.actors[0], 0xcc, 1, 2);
    field::execute_script_extended(f.world, 0x53);
    check(f.inhibition == 0 && f.camera.flags == 0x3fff && f.pc() == 2,
          "Extended 53 clears inhibition, flags and camera bits");
}
} // namespace

int main() {
    try {
        trigger_zone_and_batch_limit();
        proximity_and_selectors();
        arithmetic_and_increment();
        hide_actor_and_dialogue();
        extended_flag();
        scripted_arc_lifecycle();
        requests_and_waits();
        camera_and_fade();
        small_handlers();
        std::cout << "Field script reconstruction: nine source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
