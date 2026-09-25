// Invented state exercises the battle loading task's states (801e6fec and
// the states after it). It describes no original content or observation.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/program.hpp"

#include <iostream>

namespace game = xem::reconstruction;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call> void missing(Call call, const char *message) {
    bool stopped = false;
    try {
        call();
    } catch (const game::MissingDependency &) {
        stopped = true;
    }
    check(stopped, message);
}

constexpr std::uint32_t node = 0x80100008;
constexpr std::uint32_t other = 0x801000a8;

// Battle memory 800c3000..800d3a00 and the setup module's words; the loading
// task's node (98h bytes) is the first of two registered tasks in a heap
// block followed by a free block and the end block.
game::Program loader_program(std::uint32_t state) {
    game::Program program;
    auto &resident = program.resident;
    program.battle.emplace();
    auto &memory = *program.battle;
    memory.regions.emplace(0x800c3000, std::vector<std::uint8_t>(0x10a00));
    memory.regions.emplace(0x801e9600, std::vector<std::uint8_t>(0x100));
    auto &heap = resident.heap;
    heap.head = 0x80100008;
    heap.headers = {{0x80100000, {0x801000a8, 0x80000000 | 2U << 21U}},
                    {0x801000a0, {0x80100108, 0x80000000 | 2U << 21U}},
                    {0x80100100, {0x80108008, 0x84000000}},
                    {0x80108000, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80100108, std::vector<std::uint8_t>(0x7ef8, 0x11)}};
    auto &tasks = resident.sprite_tasks;
    std::vector<std::uint8_t> bytes(0x98);
    const auto put = [&](std::vector<std::uint8_t> &to, std::uint32_t at, std::uint32_t value) {
        for (std::uint32_t i = 0; i < 4; ++i)
            to[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    put(bytes, 8, state);
    put(bytes, 0xc, 0x8001ce44);
    put(bytes, 0x14, 5);
    put(bytes, 0x18, other);
    std::vector<std::uint8_t> second(0x58);
    tasks.nodes = {{node, bytes}, {other, second}};
    tasks.head = node;
    tasks.primary_count = 2;
    return program;
}
// The loading task's node bytes (the first task node).
std::uint32_t node_word(const game::Program &program, std::uint32_t at) {
    const auto &bytes = program.resident.sprite_tasks.nodes.at(0).bytes;
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes.at(at + i)) << (8U * i);
    return value;
}
void set_node(game::Program &program, std::uint32_t at, std::uint32_t value) {
    auto &bytes = program.resident.sprite_tasks.nodes.at(0).bytes;
    for (std::uint32_t i = 0; i < 4; ++i)
        bytes.at(at + i) = static_cast<std::uint8_t>(value >> (8U * i));
}

void settle() {
    auto program = loader_program(0x801e6c80);
    auto &memory = *program.battle;
    game::FrameServices services;
    set_node(program, 0x90, 2);
    program.battle_loader_step(node, services);
    check(node_word(program, 0x90) == 1 && memory.u8(0x800ccc58) == 0,
          "The settling count runs down first");
    set_node(program, 0x90, 0);
    // Member 0's sprite is still moving (+6 differs from +84).
    memory.put32(0x800ccb3c, 0x800c3100);
    memory.put16(0x800c3106, 3);
    program.battle_loader_step(node, services);
    check(memory.u8(0x800ccc58) == 0 && program.resident.sprite_tasks.head == node,
          "A moving member holds the loading");
    // A member with a model (+8) is not waited for.
    memory.put8(0x800c3eb8, 1);
    program.battle_loader_step(node, services);
    const auto &resident = program.resident;
    check(memory.u8(0x800ccc58) == 1 && resident.sprite_tasks.head == other &&
              resident.sprite_tasks.primary_count == 1 && resident.sprite_tasks.nodes.size() == 1,
          "Settled sprites end the loading and the task");
    check((resident.heap.headers.at(0x80100000)[1] & game::resident::heap_tag_mask) == 0 &&
              resident.heap.held.contains(node),
          "The task's node is released");
}

void wait_and_sound() {
    auto program = loader_program(0x801e6d34);
    auto &memory = *program.battle;
    game::FrameServices services;
    memory.put32(0x800c35d8, 1);
    program.battle_loader_step(node, services);
    check(node_word(program, 8) == 0x801e6d34, "Member model tasks hold the wait");
    memory.put32(0x800c35d8, 0);
    program.battle_loader_step(node, services);
    check(node_word(program, 8) == 0x801e6c80 && node_word(program, 0x90) == 0x10,
          "The wait arms 16 settling frames");

    // The sound state: the image file (+28, the second block) is released
    // once no SPU transfer runs; a member with a model needs 800bb760.
    const auto sound_program = [] {
        auto program = loader_program(0x801e6d6c);
        auto &tasks = program.resident.sprite_tasks;
        tasks.nodes.pop_back();
        tasks.primary_count = 1;
        set_node(program, 0x18, 0);
        program.battle->regions.emplace(other, std::vector<std::uint8_t>(0x58, 0x22));
        set_node(program, 0x28, other);
        return program;
    };
    auto model = sound_program();
    model.battle->put8(0x800c3eb8, 1);
    missing([&] { model.battle_loader_step(node, services); },
            "A member with a model stops at 800bb760");
    auto sound = sound_program();
    sound.battle_loader_step(node, services);
    check(node_word(sound, 8) == 0x801e6d34 && !sound.battle->regions.contains(other) &&
              sound.resident.heap.held.contains(other),
          "The image file is released");
}

void busy_states() {
    game::FrameServices services;
    for (const auto state : {0x801e6fecU, 0x801e6e48U}) {
        auto program = loader_program(state);
        program.resident.disc_error = 1;
        program.battle_loader_step(node, services);
        check(node_word(program, 8) == state, "A busy disc keeps the state");
    }
    auto members = loader_program(0x801e6f00);
    members.resident.disc_error = 1;
    auto &read = members.resident.disc_read;
    read.directories.assign(0x7a, 0);
    read.directories[0x2c * 2] = 0x40;
    members.battle_loader_step(node, services);
    check(node_word(members, 8) == 0x801e6f00 && read.directory == 0x3f,
          "The member state selects directory 2c even while the disc is busy");
    auto unknown = loader_program(0x801e7000);
    missing([&] { unknown.battle_loader_step(node, services); }, "An unknown state stops");
}

// 801e6314 over an invented enemy set file: an entry with a model, and an
// entry naming an image list before any was uploaded, stop.
void enemy_rows() {
    game::FrameServices services;
    for (const auto [flag, images] : {std::pair{1U, 0x40U}, std::pair{0U, 0U}}) {
        auto program = loader_program(0x801e6fec);
        auto &resident = program.resident;
        resident.cd_dma_register = 0x1f8010b8;
        constexpr std::uint32_t data = 0x800d3000;
        auto &memory = *program.battle;
        memory.put8(data, 1);
        memory.put32(data + 4, 0x40);
        memory.put32(data + 8, 0x14);
        memory.put32(data + 12, images);
        memory.put8(data + 16, static_cast<std::uint8_t>(flag));
        set_node(program, 0x20, data);
        missing([&] { program.battle_loader_step(node, services); },
                "Unreconstructed enemy set entries stop");
    }
}
} // namespace

int main() {
    try {
        settle();
        wait_and_sound();
        busy_states();
        enemy_rows();
        std::cout << "Battle loader: four groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
