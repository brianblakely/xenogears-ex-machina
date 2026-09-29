// Invented call-stack, field and heap bytes test ordering, bounds and ownership
// of the connected field-menu caller. They are not original-fidelity evidence.
#include "../src/analysis/field_memory.hpp"
#include "xem/reconstruction/menu_overlay.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace game = xem::reconstruction;
namespace menu = game::menu;
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Error, typename Body> void rejects(Body body, const char *message) {
    bool rejected = false;
    try {
        body();
    } catch (const Error &) {
        rejected = true;
    }
    check(rejected, message);
}
std::string dependency(const std::function<void()> &body) {
    try {
        body();
    } catch (const game::MissingDependency &error) {
        return error.dependency;
    }
    return {};
}
constexpr auto entry_sp = game::field_loop_stack;
constexpr auto stack_base = entry_sp - 0xc0U - menu::Overlay::stack_bytes;
constexpr std::array<std::uint32_t, 8> registers{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width = 4) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8U * i);
    return value;
}
void put(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    for (std::size_t i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
void qualify(game::Program &program) {
    std::vector<std::uint8_t> stack(menu::Overlay::stack_bytes + 0xc0U, 0xa5);
    program.qualify_menu_call(entry_sp, stack_base, stack, registers, 0x9876, 0x80078a38);
}
game::Program sample() {
    game::Program program;
    program.field = std::make_unique<game::FieldState>();
    auto &state = *program.field;
    state.event_control.diagnostic_suppression = 1;
    state.control_inputs.jump_contact = 0;
    state.regions.add("menu_modes", 0x800afe24, std::vector<std::uint8_t>(0x18, 0xa6));
    state.regions.add("menu_packets", 0x800afe4c, std::vector<std::uint8_t>(0x28, 0xa6));
    auto &resident = program.resident;
    resident.game_state = game::game_data_block;
    resident.game_data.assign(game::game_data_bytes, 0xb7);
    resident.field_snapshot.assign(0x3804, 0xd3);
    auto &heap = resident.heap;
    heap.tag = 8;
    heap.head = 0x80100008;
    heap.headers = {{0x80100000, {0x80140008, 0}}, {0x80140000, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80100008, std::vector<std::uint8_t>(0x3fff8, 0xcd)}};
    return program;
}

void qualification_and_inhibition() {
    auto program = sample();
    game::FrameServices services;
    check(dependency([&] { program.field_menu(services, entry_sp); }) ==
              "state:field-menu-call-abi",
          "An unqualified connected call stops before inventing stack bytes");
    check(program.resident.heap.last_caller == 0, "Missing ABI does not allocate or release");
    std::vector<std::uint8_t> image(0x80, 0x77);
    rejects<game::field::FieldFormatError>(
        [&] { program.qualify_menu_call(entry_sp, stack_base, image, {}, 0, 0); },
        "The initial qualification requires all eight saved registers");
    qualify(program);
    rejects<game::field::FieldFormatError>([&] { qualify(program); },
                                           "A menu boundary cannot re-import the initial owner");
    check(program.menu_call_state->stack.front() == 0xa5,
          "Qualification preserves the supplied stack tail");
    program.field->control_inputs.jump_contact = 0x80;
    program.field->script_flags_b21d0[0] = 1;
    const auto before = program.resident.heap.headers;
    program.field_menu(services, entry_sp);
    const auto &stack = program.menu_call_state->stack;
    const auto frame = entry_sp - 0x78U - stack_base;
    check(word(stack, frame + 0x50) == registers[0] && word(stack, frame + 0x6c) == registers[7] &&
              word(stack, frame + 0x70) == 0x9876 && word(stack, frame + 0x74) == 0x80078a38,
          "An inhibited triangle call still computes the original prologue stores");
    check(program.field->control_inputs.jump_contact == 0x80 &&
              program.resident.heap.headers == before && !program.menu &&
              program.menu_call_state->frame_pointer == 0x9876,
          "The inhibited source branch leaves field resources and menu state untouched");
    auto narrow = sample();
    narrow.qualify_menu_call(entry_sp, entry_sp - 0x80, image, registers, 0, 0);
    check(dependency([&] { narrow.field_menu(services, entry_sp); }) ==
              "state:field-menu-call-stack",
          "A bounded but insufficient original stack stops explicitly");
}

void release_before_missing_gear() {
    auto program = sample();
    qualify(program);
    auto &resident = program.resident;
    for (std::size_t i = 0; i < 3; ++i) {
        auto block = game::resident::heap_allocate(resident.heap, 0x20, 0, 0x1234);
        check(block.has_value(), "Synthetic party block allocation");
        resident.party_sprite_blocks[i] = block->address;
        resident.heap_contents[block->address] = std::move(block->bytes);
        resident.heap.headers.at(block->address - 8)[1] |= game::resident::heap_keep;
    }
    const auto party = resident.party_sprite_blocks;
    const auto field = program.field.get();
    resident.field_effect_bank = 0x12345678;
    program.field->w_b2264 = 1;
    std::vector<std::uint32_t> stages;
    game::FrameServices services;
    check(dependency([&] {
              program.field_menu(services, entry_sp,
                                 [&](const game::Program &, game::SourcePoint point, bool done) {
                                     if (done)
                                         stages.push_back(point.machine_address);
                                 });
          }) == "symbol:field-menu-gear-model",
          "The unsupported gear preservation path stops at its actual source dependency");
    check(stages == std::vector<std::uint32_t>{0x80079b28},
          "Party releases precede gear model preservation");
    check(program.field.get() == field && resident.party_sprite_blocks == party &&
              resident.field_effect_bank == 0x12345678 && !program.battle,
          "The caller preserves live field and sound ownership without restarting a mode");
    for (const auto address : party)
        check(!resident.heap_contents.contains(address) &&
                  (resident.heap.headers.at(address - 8)[1] &
                   (game::resident::heap_keep | game::resident::heap_tag_mask)) == 0,
              "All three unkept blocks return their exact bytes to the heap");
    check(resident.heap.last_caller == 0x1234 && resident.heap.held.at(party[0])[0] == 0xcd,
          "Successful releases preserve the allocation caller and return the freed contents");
    const auto packet = program.field->regions.span(0x800afe54);
    check(word(packet, 0) == 0x03a6a6a6 && word(packet, 4) == 0x62000000 &&
              word(packet, 0xc) == 0x00e00140 && word(packet, 0x10) == word(packet, 0),
          "Both fade tiles are computed, including the source header copy");
}

void resident_initialization_before_wait() {
    auto program = sample();
    qualify(program);
    const auto live_field = program.field.get();
    std::vector<std::uint32_t> stages;
    game::FrameServices services;
    rejects<game::ServiceUnavailable>(
        [&] {
            program.initialize_field_menu(
                services, entry_sp - 0x78,
                [&](const game::Program &, game::SourcePoint point, bool completed) {
                    if (completed)
                        stages.push_back(point.machine_address);
                });
        },
        "Initialization reaches the original VSync wait rather than accepting an absent result");
    check(stages == std::vector<std::uint32_t>{0x8001c6f8},
          "State and graphics initialization finish before waiting");
    check(program.menu.has_value(), "The resident caller creates menu ownership");
    const auto state = program.menu->u32(menu::state_pointer);
    check(program.menu->regions.contains(state) && !program.resident.heap_contents.contains(state),
          "The state allocation has one menu owner");
    check(program.menu->u8(state + 0x325) == 8 &&
              program.menu->u32(state + 0x1d4) == state + 0x120 &&
              program.menu->u8(state + 0x1e94) == 0 && program.menu->u8(state + 0x1e95) == 1 &&
              program.menu->u32(state + 0x1e8) == 0x800 &&
              program.menu->u32(state + 0x228) == 0x800 && program.menu->u32(state + 0x2e8) == 1,
          "Menu buffer, input and ordering controls follow resident initialization");
    check(program.menu->u16(state + 0x6c + 4) == 0x140 &&
              program.menu->u16(state + 0x6c + 0x6a) == 0xd8 &&
              program.menu->u16(state + 0x120 + 2) == 0xe0 &&
              program.menu->u16(state + 0xc8 + 2) == 0xe0 &&
              program.menu->u16(state + 0x17c + 2) == 0,
          "Both source display and draw environments are constructed independently");
    check(program.resident.heap.tag == 2 &&
              (program.resident.heap.headers.at(state - 8)[1] & game::resident::heap_tag_mask) ==
                  8U << 21U &&
              program.resident.heap.last_caller == 0x8001c644 &&
              program.field.get() == live_field && program.resident.game_data.front() == 0xb7,
          "State allocates under the incoming tag before selecting menu tag two; field/game data "
          "stay live");
}

void borrowed_stack_and_resident_bus() {
    auto program = sample();
    qualify(program);
    auto &regions = program.menu.emplace().regions;
    regions[menu::state_pointer] = {0, 0, 0x10, 0x80};
    regions[0x80100000].resize(0x100);
    program.resident.heap_contents[0x80150000] = std::vector<std::uint8_t>(0x20, 0x91);
    program.resident.menu_effects = 7;
    game::FrameServices services;
    auto stack = std::span(program.menu_call_state->stack).first(menu::Overlay::stack_bytes);
    {
        menu::Overlay overlay(program, services, stack_base + menu::Overlay::stack_bytes, nullptr,
                              stack);
        check(program.menu->stack.empty(), "A borrowed connected stack creates no second owner");
        check(program.menu->u8(0x80059178) == 7,
              "Shared menu helpers read live resident globals through their temporary bus");
        program.menu->put8(0x80059178, 3);
        program.menu->put32(0x8005a1fc, 0x12345678);
        check(program.resident.menu_effects == 3 && program.resident.random_seed == 0x12345678,
              "Resident writes keep their original scalar owner and width");
        program.menu->bytes(0x80150004, 4)[0] = 0x82;
        check(program.resident.heap_contents.at(0x80150000)[4] == 0x82 &&
                  program.menu->tail(0x80150004).size() == 0x1c && !regions.contains(0x80150000),
              "Resident resource bytes are borrowed without duplicating the block");
        rejects<menu::MenuError>([&] { static_cast<void>(program.menu->bytes(0x80150030, 1)); },
                                 "An unowned resident resource byte still fails explicitly");
        const auto frame = overlay.enter(0x18);
        overlay.put32(overlay.frame(0x18)[0x10], 0xdeadbeef);
        check(word(stack, stack.size() - 8) == 0xdeadbeef && stack.front() == 0xa5,
              "Stack writes evolve the qualified initial owner and preserve unused tails");
        rejects<menu::MenuError>([&] { overlay.set_stack_image(stack); },
                                 "A connected menu cannot replace its stack from another snapshot");
    }
    check(!program.menu->resident_memory && !program.menu->resident_tail &&
              program.menu->stack.empty() && program.resident.game_data.front() == 0xb7 &&
              word(stack, stack.size() - 8) == 0xdeadbeef,
          "Borrowed views end with the Overlay; resident data and stack changes keep their owners");
    rejects<menu::MenuError>([&] { static_cast<void>(program.menu->u8(0x80059178)); },
                             "The released ephemeral resident bus cannot be used later");
}

void disjoint_export() {
    auto program = sample();
    qualify(program);
    game::FrameServices services;
    rejects<game::ServiceUnavailable>(
        [&] { program.initialize_field_menu(services, entry_sp - 0x78); },
        "Synthetic export checkpoint ends at the required platform wait");
    xem::analysis::OriginalMemory image{std::vector<std::uint8_t>(xem::analysis::ram_bytes, 0xe9),
                                        {}};
    const auto exported = xem::analysis::export_field(program, image);
    const auto state = program.menu->u32(menu::state_pointer);
    check(image.word(menu::state_pointer) == state && image.word(state + 0x1e8) == 0x800 &&
              image.word(stack_base) == 0xa5a5a5a5,
          "Export writes menu and qualified stack state from their actual owners");
    std::size_t stack_bytes = 0;
    for (const auto &range : exported)
        if (range.address >= stack_base && range.address < entry_sp)
            stack_bytes += range.size;
    check(stack_bytes == program.menu_call_state->stack.size(),
          "Every qualified stack byte has exactly one exported owner");
    // An active disc read temporarily owns its source-sorted caller list;
    // export must split the stack claim around this exact range.
    constexpr auto list_address = entry_sp - 0x78U + 0x20U;
    program.resident.disc_read.list = {list_address, std::vector<std::uint8_t>(0x12, 0x6b)};
    {
        auto stack = std::span(program.menu_call_state->stack).first(menu::Overlay::stack_bytes);
        menu::Overlay overlay(program, services, stack_base + menu::Overlay::stack_bytes, nullptr,
                              stack);
        check(program.menu->u32(list_address) == 0x6b6b6b6b &&
                  program.menu->tail(list_address - 4).size() == 4,
              "The live bus uses the list owner and bounds the preceding stack span");
        program.menu->put32(list_address, 0x12345678);
    }
    check(program.resident.disc_read.list.bytes[0] == 0x78 &&
              program.menu_call_state->stack[list_address - stack_base] != 0x78,
          "A live-list store changes its temporary owner, not the lent stack");
    const auto with_list = xem::analysis::export_field(program, image);
    check(image.word(list_address) == 0x12345678 && !with_list.empty(),
          "Active disc list bytes are compared through their one temporary owner");
}

void original_frame_caller() {
    auto program = sample();
    qualify(program);
    game::FrameServices services;
    check(dependency([&] { program.field_frame(services); }) == "state:field-frame-call-abi",
          "An owned frame cannot guess its source caller SP or registers");
    const game::FrameCallAbi caller{entry_sp, registers, 0x81234560};
    rejects<game::ServiceUnavailable>(
        [&] { program.field_frame(services, {}, game::FrameStep::start, caller); },
        "Source prologue stores precede the first required platform timing result");
    const auto &stack = program.menu_call_state->stack;
    const auto frame = entry_sp - 0x28U - stack_base;
    check(word(stack, frame + 0x18) == registers[0] && word(stack, frame + 0x1c) == registers[1] &&
              word(stack, frame + 0x20) == caller.return_address,
          "8007554c preserves its derived caller S0, S1 and original return address");
    check(dependency([&] { program.field_frame(services, {}, game::FrameStep::draw, caller); }) ==
              "state:field-frame-start-counter",
          "A resumed frame cannot invent the source VSync counter retained in S1");
    auto invalid = caller;
    invalid.sp = 0x80000010;
    check(dependency([&] { program.field_frame(services, {}, game::FrameStep::start, invalid); }) ==
              "state:field-frame-call-stack",
          "A source callee cannot spill outside its once-qualified stack owner");
}

void queued_draw_environment() {
    auto program = sample();
    auto &resident = program.resident;
    resident.gpu.queued = 1;
    resident.gpu.width = 1024;
    resident.gpu.height = 512;
    resident.interrupts.registers[1] = 0x1f801074;
    resident.cd.dma_set_callback = 0x8004c21c;
    resident.cd.dma_interrupt_register = 0x1f8010f4;
    // The invented busy DMA prevents execution of the queued packet.
    resident.platform.push_back({game::PlatformInput::Kind::read, 0x80046980, 0x01000000});
    game::FrameServices services;
    services.vblank_counts = {10};
    services.interrupt_masks = {1};
    services.dma_busy = {0x01000000};
    constexpr auto environment = 0x80150000U;
    program.menu.emplace().regions[environment].assign(0x100, 0);
    std::fill_n(program.menu->regions.at(environment).begin() + 0x38, 0x24, 0x7b);
    menu::Overlay overlay(program, services, entry_sp);
    overlay.put_draw_env(environment);
    const auto &queue = resident.gpu.queue;
    check(resident.gpu.head == 1 && resident.gpu.tail == 0 &&
              word(queue, 4) == 0x8006be34U + 0xcU && word(queue, 8) == 0,
          "A queued draw environment uses its own copied packet, not the caller pointer");
    check(queue[0xc + 0x3f] == 0x7b,
          "80044cac's forty-hex-byte extent retains the caller packet's unused tail");
    const auto copied = word(queue, 0xc);
    overlay.put32(environment + 0x1c, 0x12345678);
    check(
        word(queue, 0xc) == copied && word(queue, 0xc) != 0x12345678 && resident.platform.empty() &&
            services.vblank_counts.empty() && services.interrupt_masks.empty() &&
            services.dma_busy.empty(),
        "Subsequent caller writes leave the computed queued copy and its input consumption intact");
    resident.gpu.services = 0x80056888;
    resident.gpu.functions[2] = 0x8004668c;
    resident.gpu.functions[6] = 0x800465ec;
    overlay.set_saved_registers(registers);
    resident.platform.push_back({game::PlatformInput::Kind::read, 0x80046980, 0x01000000});
    services.vblank_counts = {11};
    services.interrupt_masks = {1};
    constexpr auto table = environment + 0x80U;
    overlay.draw_otag(table);
    const auto runner = entry_sp - 0x18U - 0x28U - 0x18U;
    check(overlay.u32(runner + 0x10U) == table && overlay.u32(runner + 0x14U) == 0x80046930 &&
              resident.gpu.head == 2 && resident.gpu.tail == 0 && resident.platform.empty(),
          "The actual busy runner saves enqueue's parameter and its queued-branch return");
}

void compass_retained_matrix_padding() {
    struct Boundary {};
    auto program = sample();
    qualify(program);
    auto &state = *program.field;
    state.actors.resize(2);
    state.controlled_actor = 0;
    state.event_control.post_initialization = 1; // Invented blocked scheduler.
    state.party_processing_mode = 1;
    put(state.actors[0].storage, 0, 0x10000); // No position-integration branch.
    put(state.actors[1].storage, 0, 1);       // No contact candidate.
    state.actors[0].sprite.sprite.bytes.resize(0xb4);
    put(state.actors[0].sprite.sprite.bytes, 0x7c, 0x80180000);
    state.resources.push_back({0x80180000, std::vector<std::uint8_t>(0x10)});
    const game::FrameCallAbi caller{entry_sp, registers, 0x81234560};
    game::FrameServices services;
    services.hblank_counts = {3};
    rejects<Boundary>(
        [&] {
            program.field_frame(
                services,
                [](const game::Program &, game::SourcePoint point, bool completed) {
                    if (completed && point.machine_address == 0x800813ec)
                        throw Boundary{};
                },
                game::FrameStep::start, caller);
        },
        "The invented contact pass reaches its original completed boundary");
    const auto frame = entry_sp - 0x28U;
    const auto compass = frame - 0xf8U;
    const auto contact = frame - 0x48U - 0x50U - 0xc8U;
    auto &stack = program.menu_call_state->stack;
    check(word(stack, contact + 0x88U - stack_base) == 0xff3fffffU &&
              word(stack, contact + 0xa8U - stack_base) == 2,
          "Contact retains its source mask and the completed actor-count register");
    check(contact + 0x88U == compass + 0x20U && contact + 0xa8U == compass + 0x40U,
          "The later compass locals reuse the earlier contact callee bytes");
    // Invented tables and a one-unit axis camera produce an exact basis.
    state.overlay.assign(0x800adc60U - game::field_overlay_base, 0);
    state.overlay_verified.assign(state.overlay.size(), true);
    auto &math = program.resident.math;
    math.trigonometry.assign(0x4000, 0);
    for (std::size_t angle = 0; angle < 0x1000; ++angle)
        math.trigonometry[angle * 4 + 3] = 0x10; // sine zero, cosine 4096.
    math.square_root.assign(192, 4096);
    math.reciprocal.assign(192, 4096);
    state.camera.eye = {0, 0, -0x10000};
    state.camera.up = {0, 0x10000, 0};
    state.camera.previous_view.r = {-4096, 0, 0, 0, 4096, 0, 0, 0, -4096};
    state.script_flags_b21d0[1] = 1; // No quad packets in this source branch.
    state.compass_palette_rect[3] = 1;
    state.draw_block = 0x80160000;
    state.regions.add("invented_frame_table", state.draw_block, std::vector<std::uint8_t>(0x8100));
    state.regions.add("invented_compass_records", 0x800b06bc, std::vector<std::uint8_t>(0x18c4));
    auto &gpu = program.resident.gpu;
    gpu.services = 0x80056888;
    gpu.functions[2] = 0x8004668c;
    gpu.functions[8] = 0x800460a0;
    gpu.width = 1024;
    gpu.height = 512;
    program.resident.interrupts.registers[1] = 0x1f801074;
    services.vblank_counts = {7, 8};
    services.interrupt_masks = {1};
    services.alarm_polls = {0};
    rejects<Boundary>(
        [&] {
            program.field_frame(
                services,
                [](const game::Program &, game::SourcePoint point, bool completed) {
                    if (completed && point.machine_address == 0x80074108)
                        throw Boundary{};
                },
                game::FrameStep::compass, caller);
        },
        "The computed compass pass reaches its original completed boundary");
    check(word(stack, compass + 0x20U - stack_base) == 0xff3ff000U &&
              word(stack, compass + 0x40U - stack_base) == 0x1000,
          "Negative rotation and view outputs preserve their distinct inherited pad halves");
    check(word(stack, compass + 0x24U - stack_base) == 0 &&
              word(stack, compass + 0x28U - stack_base) == 0 &&
              word(stack, compass + 0x2cU - stack_base) == 0x1080 &&
              services.vblank_counts.empty() && services.interrupt_masks.empty() &&
              services.alarm_polls.empty(),
          "The original matrix copy computes translations and consumes only its GPU inputs");
    check(word(stack, compass + 0x60U - stack_base) == 0xfffff000U &&
              word(stack, compass + 0x80U - stack_base) == 0xfffff000U &&
              word(stack, compass + 0x8cU - stack_base) == 0x1080 &&
              word(stack, compass - 4U - stack_base) == 0x8007443c,
          "The hidden compass retains full-word products and its actual last identity return");
    state.script_flags_b21d0[1] = 0;
    put(state.overlay, 0x800adc40U - game::field_overlay_base, 7, 2);
    put(state.overlay, 0x800adc42U - game::field_overlay_base, 0xfff7, 2);
    program.resident.gte.set_control(28, 0x321000); // Constant invented IR0=321h.
    services.vblank_counts = {9, 10};
    services.interrupt_masks = {1};
    services.alarm_polls = {0};
    rejects<Boundary>(
        [&] {
            program.field_frame(
                services,
                [](const game::Program &, game::SourcePoint point, bool completed) {
                    if (completed && point.machine_address == 0x80074108)
                        throw Boundary{};
                },
                game::FrameStep::compass, caller);
        },
        "The shown invented compass computes all four letter placements and later quads");
    check(word(stack, compass + 0x60U - stack_base) == 0xffff1000U &&
              word(stack, compass + 0x64U - stack_base) == 7 &&
              word(stack, compass + 0x6cU - stack_base) == 0xfffffff7U &&
              word(stack, compass + 0x80U - stack_base) == 0xffff1000U &&
              word(stack, compass + 0x84U - stack_base) == 0xfffffff9U &&
              word(stack, compass + 0x8cU - stack_base) == 0x1089 &&
              word(stack, compass - 4U - stack_base) == 0x800744e8,
          "Letter rotation halfword copies preserve computed product padding and translations");
    check(word(stack, compass - 0x48U + 0x28U - stack_base) == 0x321 &&
              word(stack, compass - 0x48U + 0x2cU - stack_base) == 0,
          "RotAverage4 retains computed IR0 and projection flags before its average-depth command");
}

void computed_contact_position_locals() {
    struct Boundary {};
    auto program = sample();
    qualify(program);
    auto &state = *program.field;
    state.actors.resize(1);
    state.controlled_actor = 0;
    state.party_processing_mode = 1;
    state.event_control.post_initialization = 1;
    state.forced_position = 1;
    state.layer_count = 2;
    auto &actor = state.actors[0];
    actor.storage[0x74] = 0xff;
    put(actor.storage, 0x20, 4U << 16U);
    put(actor.storage, 0x24, 7U << 16U);
    put(actor.storage, 0x28, 4U << 16U);
    put(actor.storage, 0x30, 1U << 16U);
    put(actor.storage, 0x38, 1U << 16U);
    actor.sprite.sprite.address = 0x80181000;
    actor.sprite.sprite.bytes.resize(0xb4);
    put(actor.sprite.sprite.bytes, 0x84, 7, 2);
    put(actor.sprite.sprite.bytes, 0x7c, 0x80180000);
    state.resources.push_back({0x80180000, std::vector<std::uint8_t>(0x10)});
    // One invented flat triangle, with no neighbors and ordinary terrain.
    auto &mesh = state.collision_component;
    mesh.resize(0x9a);
    put(mesh, 0, 1);
    put(mesh, 4, 14);
    put(mesh, 0x14, 0x56);
    put(mesh, 0x18, 0x30);
    put(mesh, 0x1c, 0x3e);
    const std::array<game::field::FieldVector, 3> corners{{{0, 7, 0}, {0, 7, 16}, {16, 7, 0}}};
    for (std::uint32_t i = 0; i < 3; ++i) {
        put(mesh, 0x30U + 2U * i, i, 2);
        put(mesh, 0x36U + 2U * i, 0xffff, 2);
        for (std::uint32_t axis = 0; axis < 3; ++axis)
            put(mesh, 0x3eU + 8U * i + 2U * axis, static_cast<std::uint32_t>(corners[i][axis]), 2);
    }
    state.collision = game::field::parse_collision_package(mesh);
    program.resident.math.reciprocal.assign(192, 4096);
    game::FrameServices services;
    services.hblank_counts = {3};
    const game::FrameCallAbi caller{entry_sp, registers, 0x81234560};
    rejects<Boundary>(
        [&] {
            program.field_frame(
                services,
                [](const game::Program &, game::SourcePoint point, bool completed) {
                    if (completed && point.machine_address == 0x800813ec)
                        throw Boundary{};
                },
                game::FrameStep::start, caller);
        },
        "The initial invented field frame computes contact and position before its boundary");
    const auto &stack = program.menu_call_state->stack;
    const auto position = entry_sp - 0x28U - 0x48U - 0x50U - 0xc8U - 0x100U;
    const auto query = position - 0x80U;
    check(word(stack, position + 0x90U - stack_base) == 4U << 16U &&
              word(stack, position + 0x94U - stack_base) == 7U << 16U &&
              word(stack, position + 0x98U - stack_base) == 4U << 16U &&
              word(stack, position + 0xf4U - stack_base) == 0x7fffffff &&
              word(stack, query + 0x74U - stack_base) == 0x7fffffff,
          "Source position and query frames save live old coordinates and the unlinked floor");
    check(word(stack, query + 0x18U - stack_base) == 5 &&
              word(stack, query + 0x1cU - stack_base, 2) == 5 &&
              word(stack, query + 0x1eU - stack_base, 2) == 0xa5a5 &&
              word(stack, query + 0x48U - stack_base) == 0x00040004 &&
              word(actor.storage, 0x20) == 5U << 16U && word(actor.storage, 0x28) == 5U << 16U,
          "Computed predicted coordinates use source halfword widths and keep local padding");
}

void computed_frame_gpu_callers() {
    struct Boundary {};
    auto program = sample();
    qualify(program);
    auto &state = *program.field;
    state.actors.resize(1);
    state.controlled_actor = 0;
    state.event_control.post_initialization = 1;
    state.party_processing_mode = 1;
    state.orientation_hold = 1;
    put(state.actors[0].storage, 0, 0x10000);
    state.actors[0].storage[0x10d] = 0xff; // No positional sound emitter.
    state.actors[0].sprite.sprite.bytes.resize(0xb4);
    put(state.actors[0].sprite.sprite.bytes, 0x7c, 0x80180000);
    state.resources.push_back({0x80180000, std::vector<std::uint8_t>(0x10)});
    for (auto &emitter : state.emitters)
        emitter[1] = 0xffff;
    for (auto &dialogue : state.dialogue)
        dialogue.set_half(game::field::DialogueWindow::busy, 1);
    state.overlay.assign(0x800adc60U - game::field_overlay_base, 0);
    state.overlay_verified.assign(state.overlay.size(), true);
    auto &math = program.resident.math;
    math.trigonometry.assign(0x4000, 0);
    for (std::size_t angle = 0; angle < 0x1000; ++angle)
        math.trigonometry[angle * 4 + 3] = 0x10;
    math.square_root.assign(192, 4096);
    math.reciprocal.assign(192, 4096);
    math.angle.assign(1025, 0);
    state.camera.mode = 1; // No camera floor query in this invented prefix.
    state.camera.eye = state.camera.eye_goal = {0, 0, -0x10000};
    state.camera.up = {0, 0x10000, 0};
    state.camera.previous_view.r = {-4096, 0, 0, 0, 4096, 0, 0, 0, -4096};
    state.script_flags_b21d0[1] = 1;
    state.compass_palette_rect[3] = 1;
    state.draw_block = 0x80160000;
    state.clear_color = {0x12, 0x34, 0x56};
    state.regions.add("invented_frame_table", state.draw_block, std::vector<std::uint8_t>(0x8100));
    state.regions.add("invented_compass_cursor", 0x800b1df4, std::vector<std::uint8_t>(0x18c));
    state.regions.put(state.draw_block + 0x60U, 64, 2);
    state.regions.put(state.draw_block + 0x62U, 1, 2);
    state.regions.put(state.draw_block + 4U, 2000, 2);
    state.regions.put(state.draw_block + 6U, 700, 2);
    state.regions.put(state.draw_block + 0x18U, 1, 1); // Invented fill-background branch.
    auto &resident = program.resident;
    resident.vsync_counter = 0x7654;
    auto &gpu = resident.gpu;
    gpu.services = 0x80056888;
    gpu.functions[2] = 0x8004668c;
    gpu.functions[3] = 0x80045e44;
    gpu.functions[6] = 0x800465ec;
    gpu.functions[8] = 0x800460a0;
    gpu.width = 1024;
    gpu.height = 512;
    resident.interrupts.registers[1] = 0x1f801074;
    resident.cd.dma_set_callback = 0x8004c21c;
    resident.cd.dma_interrupt_register = 0x1f8010f4;
    // An invented prior clear request exercises DrawSync's actual runner
    // without importing any intermediate original gameplay state.
    gpu.head = 1;
    put(gpu.queue, 0, 0x80045e44);
    put(gpu.queue, 4, state.draw_block + 0x5cU);
    put(gpu.queue, 8, 0x123456);
    for (const auto site : {0x80046980U, 0x800469c8U, 0x80046a2cU, 0x80046bdcU})
        resident.platform.push_back(
            {game::PlatformInput::Kind::read, site, site == 0x80046a2cU ? 0x04000000U : 0U});
    game::FrameServices services;
    services.hblank_counts = {3, 4};
    services.vblank_counts = {0x7654, 0x7655, 0x7656, 0x7657, 0x7658};
    services.interrupt_masks = {1, 1, 1};
    services.alarm_polls = {0, 0};
    services.gpu_status = {0, 0};
    services.vblank_waits = {{5, 10}};
    const game::FrameCallAbi caller{entry_sp, registers, 0x81234560};
    const auto frame = entry_sp - 0x28U;
    auto &stack = program.menu_call_state->stack;
    bool cleared = false;
    bool viewed = false;
    rejects<Boundary>(
        [&] {
            program.field_frame(
                services,
                [&](const game::Program &, game::SourcePoint point, bool completed) {
                    if (!completed)
                        return;
                    if (point.machine_address == 0x80073c64) {
                        const auto view = frame - 0x48U - 0x70U;
                        check(word(stack, view + 0x28U - stack_base) == 0x1000 &&
                                  word(stack, view + 0x30U - stack_base) == 0x1000 &&
                                  word(stack, view + 0x44U - stack_base) == 0x1000 &&
                                  word(stack, view + 0x18U - stack_base) == 0xfffffffd &&
                                  word(stack, view + 0x54U - stack_base, 2) == 0xfffd,
                              "The shared view computation exposes normalized axes and moved eye");
                        for (const auto pad : {0x1cU, 0x2cU, 0x3cU, 0x4cU})
                            check(word(stack, view + pad - stack_base) == 0xa5a5a5a5,
                                  "Long-vector local padding retains its inherited owner bytes");
                        check(word(stack, view + 0x56U - stack_base, 2) == 0xa5a5 &&
                                  word(stack, view + 0x58U - stack_base) == 0x800af898 &&
                                  word(stack, view + 0x5cU - stack_base) == 0x7654 &&
                                  word(stack, view + 0x6cU - stack_base) == 0x80073c44,
                              "View padding and caller saves use exact source widths and live S1");
                        viewed = true;
                    }
                    if (point.machine_address == 0x80044764) {
                        const auto queue = frame - 0x28U - 0x28U;
                        check(word(stack, queue + 0x10U - stack_base) == 0x563400 &&
                                  word(stack, queue + 0x14U - stack_base) == 0x3400 &&
                                  word(stack, queue + 0x18U - stack_base) == 0x12 &&
                                  word(stack, queue + 0x1cU - stack_base) ==
                                      state.draw_block + 0x5cU,
                              "ClearImage passes source-shifted colour registers into enqueue");
                        check(word(stack, queue - 0x40U + 0x30U - stack_base) ==
                                      state.draw_block + 0x5cU &&
                                  word(stack, queue - 0x40U + 0x34U - stack_base) == 8 &&
                                  word(stack, queue - 0x40U + 0x38U - stack_base) == 0x800467a0,
                              "The actually executed clear operation preserves its exact caller");
                        cleared = true;
                    }
                    if (point.machine_address == 0x800445d0) {
                        const auto runner = frame - 0x18U - 0x18U - 0x18U;
                        const auto clear = runner - 0x40U;
                        check(word(stack, runner + 0x10U - stack_base) == 0 &&
                                  word(stack, runner + 0x14U - stack_base) == 0x80046ddc &&
                                  word(stack, clear + 0x30U - stack_base) == 0x800569c4 &&
                                  word(stack, clear + 0x34U - stack_base) == 0x7654 &&
                                  word(stack, clear + 0x38U - stack_base) == 0x80046ac8 &&
                                  gpu.head == gpu.tail && resident.platform.empty(),
                              "The queued clear inherits the executed runner frame and jalr RA");
                    }
                    if (point.machine_address == 0x80044c44) {
                        const auto queue = frame - 0x20U - 0x28U;
                        const auto alarm = queue - 0x18U;
                        const auto vsync = alarm - 0x20U;
                        check(word(stack, queue + 0x10U - stack_base) == state.draw_block + 0x1cU &&
                                  word(stack, queue + 0x14U - stack_base) == state.draw_block &&
                                  word(stack, queue + 0x18U - stack_base) == 0x800568d2 &&
                                  word(stack, queue + 0x20U - stack_base) == 0x80044cd8 &&
                                  word(stack, alarm + 0x10U - stack_base) == 0x800466b8 &&
                                  word(stack, vsync + 0x10U - stack_base) ==
                                      state.draw_block + 0x1cU &&
                                  word(stack, vsync + 0x14U - stack_base) == 0x40 &&
                                  word(stack, vsync + 0x18U - stack_base) == 0x80046f0c,
                              "PutDrawEnv computes its own queue and common alarm/VSync frame");
                        const auto environment = frame - 0x20U - 0x40U;
                        check(
                            word(stack, environment + 0x14U - stack_base, 2) == 1023 &&
                                word(stack, environment + 0x16U - stack_base, 2) == 511 &&
                                word(stack, frame - 0x18U - 0x18U - 0x18U - 0x20U + 0x14U -
                                                stack_base) == 0x7654,
                            "DrawSync retains live S1 and SetDrawEnv2 clamps its halfword locals");
                        throw Boundary{};
                    }
                },
                game::FrameStep::start, caller);
        },
        "The invented full computed prefix reaches the source draw-environment boundary");
    check(viewed && cleared && services.hblank_counts.empty() && services.vblank_counts.empty() &&
              services.interrupt_masks.empty() && services.alarm_polls.empty() &&
              services.gpu_status.empty() && services.vblank_waits.empty(),
          "The computed prefix consumes exactly the supplied independent GPU/device inputs");
    const auto retained = stack;
    services.vblank_counts = {12};
    services.interrupt_masks = {1};
    program.menu.emplace();
    auto borrowed = std::span(stack).first(menu::Overlay::stack_bytes);
    {
        menu::Overlay overlay(program, services, stack_base + menu::Overlay::stack_bytes, nullptr,
                              borrowed);
        overlay.put_draw_env(state.draw_block);
    }
    check(stack == retained,
          "An observer exception restores the lent enqueue ABI before a later isolated call");
    check(dependency([&] { program.field_move(); }) == "state:field-move-call-abi",
          "An owned isolated move cannot invent its source field-frame caller");
    auto move_caller = caller;
    move_caller.sp = frame;
    move_caller.saved_registers[1] = resident.vsync_counter;
    move_caller.return_address = 0x8007557c;
    state.camera_cut = 1;
    program.field_move({}, move_caller);
    check(word(stack, frame - 0x48U - 0x70U + 0x6cU - stack_base) == 0x80073b9c,
          "The camera-cut view branch derives its distinct original return address");
}
} // namespace

int main() {
    try {
        qualification_and_inhibition();
        release_before_missing_gear();
        resident_initialization_before_wait();
        borrowed_stack_and_resident_bus();
        disjoint_export();
        original_frame_caller();
        queued_draw_environment();
        compass_retained_matrix_padding();
        computed_contact_position_locals();
        computed_frame_gpu_callers();
        std::cout << "Field menu caller bounds, source ordering and ownership passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
