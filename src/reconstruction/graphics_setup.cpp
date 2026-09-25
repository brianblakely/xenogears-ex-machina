// Resident libgpu/libgte setup calls of executable dc0b2dd7... and the
// battle mode's start (8001b6c4 up to 80070f40).
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

namespace xem::reconstruction {
namespace {
std::uint32_t half(std::int32_t value) { return static_cast<std::uint32_t>(value) & 0xffffU; }
} // namespace

// An environment store as the original makes it: battle memory when it holds
// the bytes, else the owned memory of set_memory.
void Program::store_owned(std::uint32_t address, std::uint32_t value, std::uint32_t width) {
    if (battle && battle->contains(address, width)) {
        if (width == 1)
            battle->put8(address, value);
        else
            battle->put16(address, value);
        return;
    }
    set_memory(address, value, width);
}

// 800439e0: display area x, y, w, h; screen area and flags cleared.
void Program::set_default_display_environment(std::uint32_t environment, std::int32_t x,
                                              std::int32_t y, std::int32_t width,
                                              std::int32_t height) {
    store_owned(environment, half(x), 2);
    store_owned(environment + 2, half(y), 2);
    store_owned(environment + 4, half(width), 2);
    for (const auto at : {8U, 0xaU, 0xcU, 0xeU})
        store_owned(environment + at, 0, 2);
    for (const auto at : {0x11U, 0x10U, 0x13U, 0x12U})
        store_owned(environment + at, 0, 1);
    store_owned(environment + 6, half(height), 2);
}

// 80043928: clip area x, y, w, h; the offset at x, y; texture window
// cleared; texture page 0a; dither 0; drawing to the display area allowed
// when the height fits the video mode (8004c308: 288 PAL, 256 NTSC lines);
// background off.
void Program::set_default_draw_environment(std::uint32_t environment, std::int32_t x,
                                           std::int32_t y, std::int32_t width,
                                           std::int32_t height) {
    const auto pal = resident.video_mode != 0;
    store_owned(environment, half(x), 2);
    store_owned(environment + 2, half(y), 2);
    store_owned(environment + 4, half(width), 2);
    for (const auto at : {0xcU, 0xeU, 0x10U, 0x12U})
        store_owned(environment + at, 0, 2);
    for (const auto at : {0x19U, 0x1aU, 0x1bU})
        store_owned(environment + at, 0, 1);
    store_owned(environment + 0x16, 1, 1);
    store_owned(environment + 6, half(height), 2);
    store_owned(environment + 0x17, height < (pal ? 289 : 257) ? 1U : 0U, 1);
    store_owned(environment + 8, half(x), 2);
    store_owned(environment + 0xa, half(y), 2);
    store_owned(environment + 0x14, 10, 2);
    store_owned(environment + 0x18, 0, 1);
}

// 80048bc4: 8004b4ac installs the GTE exception entries (BIOS memory) inside
// a critical section; the coprocessor is enabled; the screen and depth-cue
// constants take their defaults.
void Program::init_geometry(std::uint32_t return_address) {
    resident.geometry_return = return_address;
    resident.geometry_inner_return = 0x80048bd4;
    auto &gte = resident.gte;
    gte.set_control(29, 0x155);       // ZSF3
    gte.set_control(30, 0x100);       // ZSF4
    gte.set_control(26, 1000);        // H
    gte.set_control(27, 0xffffef9eU); // DQA
    gte.set_control(28, 0x01400000U); // DQB
    gte.set_control(24, 0);           // OFX
    gte.set_control(25, 0);           // OFY
}

void Program::set_geometry_offset(std::int32_t x, std::int32_t y) {
    resident.gte.set_control(24, static_cast<std::uint32_t>(x) << 16U);
    resident.gte.set_control(25, static_cast<std::uint32_t>(y) << 16U);
}

void Program::set_geometry_screen(std::int32_t h) {
    resident.gte.set_control(26, static_cast<std::uint32_t>(h));
}

void Program::set_battle_draw_modes(std::uint32_t environment) {
    store_owned(environment + 0x18, 1, 1);
    store_owned(environment + 0x16, 1, 1);
    store_owned(environment + 0x19, 0x3c, 1);
    store_owned(environment + 0x1a, 0x78, 1);
    store_owned(environment + 0x1b, 0x78, 1);
}

// 8001b6c4 up to its call of 80070f40.
void Program::battle_mode_start() {
    resident.b_5959c = 1;
    disc_wait(0);
    static_cast<void>(select_directory(0xc, 0));
    if (resident.debug_word != 0xffffffffU) // *8005917c
        throw MissingDependency({"battle_mode", 0x8001b704, {}, {}}, "symbol:debug-capture", false,
                                "The debug capture (8003747c, 800374e8) is not reconstructed");
    // 8001b844.
    reset_graph(1);
    // 800379d0 (three calls) returns at once.
    init_geometry(0x8001b89c);
    set_geometry_offset(0xa0, 0xb4);
    set_geometry_screen(0x200);
    set_default_display_environment(0x800c4a7c, 0, 0xe0, 0x140, 0xe0);
    set_default_draw_environment(0x800c4a20, 0, 0, 0x140, 0xe0);
    set_default_display_environment(0x800c8aec, 0, 0, 0x140, 0xe0);
    set_default_draw_environment(0x800c8a90, 0, 0xe0, 0x140, 0xe0);
    set_battle_draw_modes(0x800c4a20);
    set_battle_draw_modes(0x800c8a90);
}

} // namespace xem::reconstruction
