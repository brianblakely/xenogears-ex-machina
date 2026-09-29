// The live field's menu caller 800799d4 (overlay 38a1ce82...) and resident
// menu initialization 8001c634 (executable dc0b2dd7...). No mode dispatcher
// runs here: the field, sound driver, heap and persistent game data survive.
#include "xem/reconstruction/menu_overlay.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("The field menu requires live field state");
    return *program.field;
}
[[noreturn]] void missing(std::string_view operation, std::uint32_t address, const char *id,
                          const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::span<std::uint8_t> stack_window(Program &program, std::uint32_t base, std::uint32_t count) {
    if (!program.menu_call_state)
        missing("field_menu", 0x800799d4, "state:field-menu-call-abi",
                "The field menu needs a qualified initial owned stack and saved registers");
    auto &call = *program.menu_call_state;
    if (base < call.stack_base || std::uint64_t{base - call.stack_base} + count > call.stack.size())
        missing("field_menu", 0x800799d4, "state:field-menu-call-stack",
                "The qualified initial stack does not cover the original menu call frames");
    return std::span(call.stack).subspan(base - call.stack_base, count);
}
void observed(const ProgramObserver &observe, const Program &program, std::string_view operation,
              std::uint32_t address) {
    if (observe)
        observe(program, {operation, address, {}, {}}, true);
}
} // namespace

void Program::qualify_menu_call(std::uint32_t entry_sp, std::uint32_t stack_base,
                                std::span<const std::uint8_t> stack,
                                std::span<const std::uint32_t> saved_registers,
                                std::uint32_t frame_pointer, std::uint32_t return_address) {
    if (menu_call_state)
        throw field::FieldFormatError("A connected menu call ABI may be qualified only once");
    if (saved_registers.size() != 8 || stack.empty() || (entry_sp & 3U) != 0 ||
        (stack_base & 3U) != 0 || stack_base < 0x80000000U || entry_sp < stack_base ||
        std::uint64_t{stack_base} + stack.size() > 0x80200000ULL ||
        std::uint64_t{entry_sp} > std::uint64_t{stack_base} + stack.size())
        throw field::FieldFormatError("Invalid qualified menu call ABI");
    MenuCallState call{entry_sp, stack_base,    {stack.begin(), stack.end()},
                       {},       frame_pointer, return_address};
    std::ranges::copy(saved_registers, call.saved_registers.begin());
    menu_call_state = std::move(call);
}

// 80077d2c: unkeep all three blocks before releasing any; their original
// pointer words remain until 80077c88 overwrites them on restoration.
void Program::release_party_sprites() {
    for (const auto address : resident.party_sprite_blocks) {
        const auto header = resident.heap.headers.find(address - 8);
        if (header == resident.heap.headers.end())
            throw field::FieldFormatError("A party sprite block has no heap header");
        header->second[1] &= ~resident::heap_keep;
    }
    constexpr std::array<std::uint32_t, 3> sites{0x80077d70, 0x80077d80, 0x80077d90};
    for (std::size_t i = 0; i < sites.size(); ++i)
        static_cast<void>(release_owned_block(resident.party_sprite_blocks[i], sites[i]));
}

// 800798bc: the field/battle/menu entry flag, with the signed halfword
// override at 800b234c. The source's comparison is with ff, not -1.
void Program::field_entry_flag() {
    const auto &state = loaded(*this);
    std::uint8_t flag = 1;
    if (state.party_reassignment != 0) {
        const auto index = static_cast<std::size_t>(state.controlled_actor);
        if (index >= state.actors.size())
            throw field::FieldFormatError("The field-entry flag needs the controlled actor");
        flag = (state.actors[index].storage[0x14] & 0xc0U) != 0 ? 1 : 0;
    }
    resident.b_59179 = flag;
    if (const auto value = memory(0x800b234c, 2); s16(value) != 0xff)
        resident.b_59179 = static_cast<std::uint8_t>(value);
}

// 8007999c: synchronize drawing, enter the BIOS critical section, flush
// instruction-cache intent, then leave it. Services are explicit boundaries.
void Program::field_flush_cache(FrameServices &services, std::optional<std::uint32_t> caller_sp,
                                std::uint32_t return_address) {
    if (menu_call_state) {
        if (!caller_sp)
            missing("field_flush_cache", 0x8007999c, "state:field-flush-call-abi",
                    "The owned cache-flush caller stack needs its original SP");
        const auto &call = *menu_call_state;
        if (*caller_sp < 0x18U || *caller_sp - 8U < call.stack_base ||
            std::uint64_t{*caller_sp} - 4U > std::uint64_t{call.stack_base} + call.stack.size())
            missing("field_flush_cache", 0x8007999c, "state:field-flush-call-stack",
                    "The original cache-flush return slot crosses its qualified stack owner");
        set_memory(*caller_sp - 0x18U + 0x10U, return_address);
    }
    draw_and_vertical_sync(services);
    if (services.menu_card == nullptr)
        throw ServiceUnavailable("The field BIOS critical-section service");
    if (!services.flush_instruction_cache)
        throw ServiceUnavailable("The field BIOS instruction-cache flush service");
    static_cast<void>(services.menu_card->enter_critical_section());
    services.flush_instruction_cache();
    services.menu_card->exit_critical_section();
}

// 80079784: one original screen fade; table links, buffer and shade are
// computed from live field state, then the original GPU services run.
void Program::field_menu_fade(FrameServices &services, std::int32_t shade, std::uint32_t caller_sp,
                              std::uint32_t return_address) {
    auto &state = loaded(*this);
    set_memory(caller_sp - 0x18U + 0x10U, menu_call_state->saved_registers[0]);
    set_memory(caller_sp - 0x18U + 0x14U, return_address);
    swap_draw_buffer();
    const auto buffer = state.draw_buffer;
    const auto tile = 0x800afe54U + 0x10U * buffer;
    const auto value = (static_cast<std::uint32_t>(shade) << 2U) & 0xffU;
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(tile + 4 + i, value, 1);
    add_primitive(state.draw_block + 0xcc, tile);
    add_primitive(state.draw_block + 0xcc, 0x800afe24U + 0xcU * buffer);
    draw_and_vertical_sync(services);
    std::array<std::int16_t, 4> rect{};
    for (std::uint32_t i = 0; i < 4; ++i)
        rect[i] = static_cast<std::int16_t>(s16(memory(0x800afe4cU + 2U * i, 2)));
    move_image(services, rect, 0, static_cast<std::int32_t>(buffer << 8U));
    put_disp_env(state.draw_block + 0xb8);
    put_draw_env(services, state.draw_block);
    draw_otag(services, state.draw_block + 0xd0);
}

// 8001c634: allocate/clear state, initialize both environments and buffer
// controls, then enter 8001c1a8's ordinary dispatch into the shared Overlay.
void Program::initialize_field_menu(FrameServices &services, std::uint32_t entry_sp,
                                    const ProgramObserver &observe) {
    static_cast<void>(loaded(*this));
    static_cast<void>(stack_window(*this, entry_sp - 0x48U, 0x48U));
    auto &call = *menu_call_state;
    const auto setup_sp = entry_sp - 0x18U;
    set_memory(setup_sp + 0x14, 0x80079ea0);
    set_memory(setup_sp + 0x10, call.saved_registers[0]);
    if (!menu)
        menu.emplace();
    auto &regions = menu->regions;
    regions.try_emplace(menu::state_pointer, 4);
    const auto state = load_block(0x1e98, 0, 0x8001c644);
    regions[state] = std::move(resident.heap_contents.at(state));
    resident.heap_contents.erase(state);
    set_memory(menu::state_pointer, state);
    std::ranges::fill(regions.at(state), 0); // bzero(state, 1e98)
    set_memory(state + 0x325, 8, 1);
    resident::heap_select_tag(resident.heap, 2, 0);
    set_memory(state + 0x1d4, state + 0x120);
    set_memory(state + 0x1e94, 0, 1);
    set_memory(state + 0x1e95, 1, 1);
    set_memory(state + 0x2d8, 0);
    set_memory(state + 0x327, 0, 1);
    call.saved_registers[0] = 1;
    set_geometry_offset(160, 112);
    set_geometry_screen(512);
    set_default_display_environment(state + 0xc8, 0, 0xe0, 0x140, 0xe0);
    set_default_draw_environment(state + 0x6c, 0, 0, 0x140, 0xe0);
    set_default_display_environment(state + 0x17c, 0, 0, 0x140, 0xe0);
    set_default_draw_environment(state + 0x120, 0, 0xe0, 0x140, 0xe0);
    for (const auto env : {state + 0x6c, state + 0x120}) { // 8001bddc
        set_memory(env + 0x16, 1, 1);
        set_memory(env + 0x66, 10, 2);
        set_memory(env + 0x68, 0x100, 2);
        for (const auto at : {0x18U, 0x19U, 0x1aU, 0x1bU})
            set_memory(env + at, 0, 1);
        set_memory(env + 0x64, 0, 2);
        set_memory(env + 0x6a, 0xd8, 2);
    }
    if (resident.menu_effects != 0) {
        set_memory(state + 0x84, 1, 1);
        set_memory(state + 0x138, 1, 1);
    }
    for (const auto base : {state + 0x1d8, state + 0x218}) { // 8001beec
        for (const auto at : {0U, 2U, 4U})
            set_memory(base + at, 0, 2);
        set_memory(base + 8, 0);
        set_memory(base + 0xc, 0);
        set_memory(base + 0x10, 0x800);
    }
    set_memory(state + 0x2e8, 1);
    set_memory(state + 0x329, 0, 1);
    observed(observe, *this, "field_menu_initialized", 0x8001c6f8);
    vertical_sync(services);
    put_draw_env(services, state + 0x6c);
    put_draw_env(services, state + 0x120);
    put_disp_env(state + 0xc8);
    put_disp_env(state + 0x17c);
    set_disp_mask(1);
    // 8001c1a8's prologue. Saved S0..S5 retain the resident/field caller
    // assignments; the ordinary dispatch resets S0,S1,S2 before the call.
    const auto dispatch_sp = setup_sp - 0x30U;
    for (std::uint32_t i = 0; i < 6; ++i)
        set_memory(dispatch_sp + 0x10U + 4U * i, call.saved_registers[i]);
    set_memory(dispatch_sp + 0x28, 0x8001c750);
    call.saved_registers[0] = 0;
    call.saved_registers[1] = 0;
    call.saved_registers[2] = 1;
    if (resident.menu_effects != 0)
        missing("field_menu", 0x8001c1fc, "symbol:menu-debug-8001c1a8",
                "The diagnostic resident menu selection/loading path is not recovered");
    static_cast<void>(select_directory(0x10, 0));
    static_cast<void>(select_directory(0x10, 0));
    if (resident.menu_mode != 0 && resident.menu_mode != 2 && resident.menu_mode != 6)
        missing("field_menu", 0x8001c518, "symbol:menu-alternative-dispatch-8001c1a8",
                "This field menu request reaches another menu overlay entry");
    {
        const auto available = dispatch_sp - call.stack_base;
        const auto size = std::min(available, menu::Overlay::stack_bytes);
        auto bytes = stack_window(*this, dispatch_sp - size, size);
        if (bytes.empty())
            missing("field_menu", 0x8001c1a8, "state:field-menu-call-stack",
                    "The menu overlay needs an owned original callee stack");
        menu::Overlay overlay(*this, services, dispatch_sp, services.menu_card, bytes);
        overlay.set_saved_registers(call.saved_registers);
        observed(observe, *this, "field_menu_overlay_entry", 0x801c62a8);
        overlay.run_menu_mode();
    }
    if (resident.menu_mode == 2 || resident.menu_mode == 6)
        set_next_mode(1);
    for (std::uint32_t i = 0; i < 6; ++i)
        call.saved_registers[i] = memory(dispatch_sp + 0x10U + 4U * i);
    resident.menu_effects = 1;
    call.saved_registers[0] = memory(setup_sp + 0x10);
    observed(observe, *this, "field_menu_overlay_return", 0x80079ea0);
}

// 800a2488 -> 800ace24 -> 800ad978(0): actor zero's menu-return script,
// then detect changed party modes and refresh only the changed live actors.
void Program::field_menu_return(const ProgramObserver &observe) {
    auto &state = loaded(*this);
    store_original(0x800adb8c, 1, 4);
    run_actor0_script(3, observe);
    const auto modes = resident.party_modes();
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto at = 0x8006be2cU + 2U * i;
        set_memory(at, memory(at, 2) == modes[i] ? 0U : 1U, 2);
    }
    if (state.party_reassignment != 0)
        for (std::uint32_t i = 0; i < 3; ++i)
            if (state.party_indices[i] != 0xff && memory(0x8006be2cU + 2U * i, 2) == 1) {
                store_original(0x800afd1c, memory(0x8006f990U + 4U * i), 4);
                missing("field_menu_return", modes[i] == 0 ? 0x800ada2c : 0x800ada1c,
                        modes[i] == 0 ? "symbol:field-party-refresh-800acfd0"
                                      : "symbol:field-party-refresh-800ad4d4",
                        "Refreshing a changed party actor after the menu is not recovered");
            }
    store_original(0x800adb8c, 0, 4);
}

// 800799d4: synchronous entry and restoration around the menu. The caller
// stack, file list, VRAM buffers and all allocation/release sites follow
// the qualified original disassembly, not a post-load state snapshot.
void Program::field_menu(FrameServices &services, std::uint32_t entry_sp,
                         const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &request = state.control_inputs.jump_contact;
    static_cast<void>(stack_window(*this, entry_sp - 0xc0U, 0xc0U));
    auto &call = *menu_call_state;
    if (call.entry_sp != entry_sp)
        missing("field_menu", 0x800799d4, "state:field-menu-call-sp",
                "The menu call SP differs from the qualified initial calling convention");
    const auto sp = entry_sp - 0x78U;
    set_memory(sp + 0x70, call.frame_pointer);
    set_memory(sp + 0x74, call.return_address);
    for (std::uint32_t i = 0; i < 8; ++i)
        set_memory(sp + 0x50U + 4U * i, call.saved_registers[i]);
    if (request == 0x80 && state.script_flags_b21d0[0] != 0)
        return;
    call.frame_pointer = 0;
    set_memory(0x800afe57, 3, 1);
    set_memory(0x800afe58, 0x62000000);
    set_memory(0x800afe5c, 0);
    set_memory(0x800afe60, 0x00e00140);
    for (std::uint32_t i = 0; i < 16; i += 4)
        set_memory(0x800afe64U + i, memory(0x800afe54U + i));
    const auto tpage = gpu::texture_page(0, 2, 0, 0);
    draw_mode_packet(0x800afe24, tpage, nullptr);
    draw_mode_packet(0x800afe30, tpage, nullptr);
    set_memory(sp + 0x10, 0);
    release_party_sprites();
    observed(observe, *this, "field_menu_party_released", 0x80079b28);
    if (state.w_b2264 != 0)
        missing("field_menu", 0x80079b3c, "symbol:field-menu-gear-model",
                "Parking and restoring the gear model around a field menu is not recovered");
    const auto ceiling = state.w_adb30;
    set_memory(sp + 0x48, ceiling);
    static_cast<void>(select_directory(0x10, 0));
    const auto code_size = resident.w_4f370 == 1 ? file_words((request + 5) & 0x7fU)
                                                 : (ceiling & 0xffffffU) - 0x1c5008U;
    const auto code = load_block(code_size, 1, 0x80079bf8);
    call.saved_registers[7] = code;
    for (const auto at : {0x30U, 0x38U})
        set_memory(sp + at, 0, 2);
    set_memory(sp + 0x34, 0);
    set_memory(sp + 0x3c, 0);
    set_memory(sp + 0x20, 1, 2);
    resident.menu_resources = load_block(file_words(1), 1, 0x80079c28);
    set_memory(sp + 0x24, resident.menu_resources);
    set_memory(sp + 0x28, (request & 0x7fU) + 5U, 2);
    set_memory(sp + 0x2c, code);
    if ((request & 0x7fU) == 5 && resident.w_4f370 == 0) {
        set_memory(sp + 0x30, 0xc, 2);
        set_memory(sp + 0x34, 0x1dc000);
    }
    disc_wait(0);
    auto list = stack_window(*this, sp + 0x20,
                             (request & 0x7fU) == 5 && resident.w_4f370 == 0 ? 0x1aU : 0x12U);
    resident.disc_read.list = {sp + 0x20, {list.begin(), list.end()}};
    static_cast<void>(read_files(0));
    std::ranges::copy(resident.disc_read.list.bytes, list.begin());
    static_cast<void>(select_directory(4, 0));
    const auto put_rect = [&](std::uint32_t at, const std::array<std::int16_t, 4> &rect) {
        for (std::uint32_t i = 0; i < 4; ++i)
            set_memory(at + 2U * i, static_cast<std::uint16_t>(rect[i]), 2);
    };
    const auto move_tiles = [&](std::uint32_t from, std::uint32_t to) {
        for (std::uint32_t i = 0; i < 6; ++i) {
            std::array<std::int16_t, 4> rect{
                static_cast<std::int16_t>(memory(from + 4U * i, 2)),
                static_cast<std::int16_t>(memory(from + 4U * i + 2, 2)), 0x40, 0x20};
            put_rect(sp + 0x18, rect);
            move_image(services, rect, s16(memory(to + 4U * i, 2)),
                       s16(memory(to + 4U * i + 2, 2)));
            draw_sync(services);
        }
    };
    move_tiles(0x800adcb0, 0x800adcc8);
    call.saved_registers[0] = 0x800adcc8;
    call.saved_registers[1] = 0x800adcca;
    call.saved_registers[2] = 6;
    call.saved_registers[3] = 0x800adce0;
    call.saved_registers[4] = 0x800adce2;
    for (const auto [x, to] : {std::pair{0x3c0, 0x300}, {0x2c0, 0x280}}) { // 8007995c
        set_memory(sp + 0x10, static_cast<std::uint32_t>(to));
        set_memory(sp + 0x14, 0);
        // 8007995c's local rectangle and saved return address.
        put_rect(sp - 0x20U + 0x10U, {static_cast<std::int16_t>(x), 0x100, 0x40, 0x100});
        set_memory(sp - 0x20U + 0x18U, x == 0x3c0 ? 0x80079d40 : 0x80079d60);
        move_image(services, {static_cast<std::int16_t>(x), 0x100, 0x40, 0x100}, to, 0);
        draw_sync(services);
    }
    put_rect(0x800afe4c, {0x2c0, 0x100, 0x140, 0xe0});
    call.saved_registers[0] = 0x800afe4c;
    call.saved_registers[2] = 0;
    copy_screen(services, 0x2c0, 0x100);
    move_image(services, {0x2c0, 0x100, 0x140, 0xe0}, 0, 0x100);
    draw_sync(services);
    for (std::int32_t i = 0; i < 32; ++i)
        field_menu_fade(services, i, sp, 0x80079dbc);
    draw_and_vertical_sync(services);
    set_memory(0x800afe4c, 0, 2);
    set_memory(0x800afe4e, 0, 2);
    state.draw_buffer = (state.draw_buffer + 1U) % 2U; // 800796fc, no table clear
    state.draw_block = 0x800b249cU + state.draw_buffer * 0x80f4U;
    put_disp_env(state.draw_block + 0xb8);
    put_draw_env(services, state.draw_block);
    release_named_block();
    move_image(services, {0, 0, 0x140, 0xe0}, 0, 0xe0);
    draw_and_vertical_sync(services);
    disc_wait(0);
    std::ranges::copy(resident.disc_read.list.bytes, list.begin());
    resident.disc_read.list = {}; // The caller again owns the completed stack list.
    resident.departure_594d0 = 0;
    resident.menu_effects = 0;
    resident.menu_mode = static_cast<std::uint8_t>(request & 0x7fU);
    if (!menu)
        menu.emplace();
    auto &regions = menu->regions;
    regions.try_emplace(0x8006be2c, 6);
    const auto modes = resident.party_modes();
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(0x8006be2cU + 2U * i, modes[i], 2);
    field_entry_flag();
    regions.try_emplace(0x8005a4ac, 8);
    set_memory(0x8005a4ac, 0x800b2568);
    set_memory(0x8005a4b0, 0x800ba65c);
    // The resource file becomes the menu's block until load_resources frees it.
    regions[resident.menu_resources] =
        std::move(resident.heap_contents.at(resident.menu_resources));
    resident.heap_contents.erase(resident.menu_resources);
    regions[code] = std::move(resident.heap_contents.at(code));
    resident.heap_contents.erase(code);
    call.saved_registers[0] = 0x800afe4c;
    call.saved_registers[1] = 0x800adcca;
    call.saved_registers[2] = 3;
    call.saved_registers[3] = 0x800adce0;
    call.saved_registers[4] = 0x800adce2;
    field_flush_cache(services, sp, 0x80079e98);
    initialize_field_menu(services, sp, observe);
    resident.heap_contents[code] = std::move(regions.at(code));
    regions.erase(code);
    field_flush_cache(services, sp, 0x80079ea8);
    resident.sprite_models.depth_shift = 2;
    if (resident.departure_594d0 == 0 && (request & 0x7fU) == 2) {
        state.b_b02c8 = 1;
        resident.variables.write(0x46, 0);
        resident.variables.write(4, 4);
        resident.field_map = 4;
        set_memory(resident.game_state + 0x2320, 0, 2);
        set_memory(resident.game_state + 0x1932, 0, 2);
        set_memory(resident.game_state + 0x231a, 4, 2);
    }
    if (resident.departure_594d0 == 2) {
        state.b_b02c8 = 1;
        resident.variables.write(0x46, 2);
        const auto map = memory(resident.game_state + 0x231a, 2) & 0x3fffU;
        resident.variables.write(4, static_cast<std::int32_t>(map));
        if (map < 0x400)
            set_memory(resident.game_state + 0x2320, memory(resident.game_state + 0x1984, 2), 2);
    }
    draw_and_vertical_sync(services);
    put_rect(0x800afe4c, {0, 0xe0, 0x140, 0xe0});
    move_image(services, {0, 0xe0, 0x140, 0xe0}, 0x140, 0);
    draw_and_vertical_sync(services);
    set_memory(0x800afe4c, 0x140, 2);
    set_memory(0x800afe4e, 0, 2);
    move_image(services, {0x140, 0, 0x140, 0xe0}, 0, 0);
    move_image(services, {0x140, 0, 0x140, 0xe0}, 0, 0x100);
    draw_and_vertical_sync(services);
    put_disp_env(state.draw_block + 0xb8);
    put_draw_env(services, state.draw_block);
    set_memory(0x800afe4c, 0x2c0, 2);
    set_memory(0x800afe4e, 0x100, 2);
    call.saved_registers[0] = 0x100;
    call.saved_registers[1] = 0x40;
    call.saved_registers[2] = 0;
    field_menu_fade(services, 31, sp, 0x8007a04c);
    field_menu_fade(services, 31, sp, 0x8007a054);
    std::array<std::int16_t, 4> rect{0x300, 0, 0x40, 0x100};
    put_rect(sp + 0x18, rect);
    const auto first = load_block(0x8000, 1, 0x8007a06c);
    call.saved_registers[6] = first;
    store_image(services, rect, sp + 0x18, first);
    draw_sync(services);
    rect = {0x280, 0, 0x40, 0x100};
    put_rect(sp + 0x18, rect);
    const auto second = load_block(0x8000, 1, 0x8007a0a4);
    call.saved_registers[5] = second;
    store_image(services, rect, sp + 0x18, second);
    draw_sync(services);
    move_tiles(0x800adcc8, 0x800adcb0);
    call.saved_registers[0] = 0x800adce0;
    call.saved_registers[1] = 0x800adce2;
    call.saved_registers[2] = 6;
    call.saved_registers[3] = 0x800adcc8;
    call.saved_registers[4] = 0x800adcca;
    static_cast<void>(select_directory(4, 0));
    resident::heap_select_tag(resident.heap, 8, 0);
    state.reload.stream_pending = 0;
    start_field_stream();
    if (memory(0x800afe84) == 0) {
        for (std::int32_t i = 31; i >= 0; --i)
            field_menu_fade(services, i, sp, 0x8007a1ac);
        field_menu_fade(services, 0, sp, 0x8007a1c0);
        state.w_adb50 = 0;
    } else {
        for (std::int32_t i = 32; i < 63; ++i)
            field_menu_fade(services, i, sp, 0x8007a180);
        state.w_adb50 = 1;
    }
    finish_field_stream(services);
    draw_and_vertical_sync(services);
    rect = {0x2c0, 0x100, 0x40, 0x100};
    put_rect(sp + 0x18, rect);
    static_cast<void>(load_image(rect, sp + 0x18, second, &services));
    draw_sync(services);
    rect[0] = 0x3c0;
    put_rect(sp + 0x18, rect);
    static_cast<void>(load_image(rect, sp + 0x18, first, &services));
    draw_sync(services);
    static_cast<void>(release_owned_block(second, 0x8007a224));
    static_cast<void>(release_owned_block(first, 0x8007a22c));
    static_cast<void>(release_owned_block(code, 0x8007a234));
    static_cast<void>(select_directory(4, 0));
    set_geometry_offset(160, 112);
    set_geometry_screen(static_cast<std::int32_t>(memory(0x800af9f8)));
    allocate_party_sprites();
    if (request == 1) {
        resident.w_6fabc.fill(0xff);
        resident.party_sprite_load.needed = 0;
        resident.party_sprite_load.loaded = 0;
        prepare_party_sprites();
        decode_party_sprites();
        draw_and_vertical_sync(services);
        state.event_control.gate_values[2] = 0;
        store_original(0x800adb05, 1, 1);
    } else {
        for (std::uint32_t i = 0; i < 3; ++i) {
            const auto file = resident.w_6fabc[i];
            if (file == 0xff)
                continue;
            const auto packed = load_block(file_words(file + 5), 1, 0x8007a394);
            static_cast<void>(read_file(std::bit_cast<std::int32_t>(file + 5U), packed, 0, 0x80));
            disc_wait(0);
            const auto size = memory(packed);
            const auto source = [&](std::size_t offset) {
                return ram_byte(packed + static_cast<std::uint32_t>(offset));
            };
            auto output = owned_span(resident.party_sprite_blocks[i]);
            if (size > output.size())
                throw field::FieldFormatError("A menu-return sprite exceeds its original block");
            static_cast<void>(field::decode_packed_through(
                source,
                [&](std::size_t offset, std::uint8_t value) {
                    if (offset >= output.size())
                        throw field::FieldFormatError(
                            "A menu-return sprite decoder overruns its block");
                    output[offset] = value;
                },
                [&](std::size_t offset) {
                    if (offset >= output.size())
                        throw field::FieldFormatError(
                            "A menu-return sprite decoder reads outside its block");
                    return output[offset];
                }));
            static_cast<void>(release_owned_block(packed, 0x8007a3d4));
        }
        field_menu_return(observe);
        draw_and_vertical_sync(services);
    }
    request = 0xff;
    load_text_palette(services, sp);
    // 8004f350 is already the sprite upload subsystem's owned word.
    set_memory(0x8004f350, 0);
    for (std::uint32_t i = 0; i < 8; ++i)
        call.saved_registers[i] = memory(sp + 0x50U + 4U * i);
    call.frame_pointer = memory(sp + 0x70);
    call.return_address = memory(sp + 0x74);
    observed(observe, *this, "field_menu", 0x8007a448);
}

} // namespace xem::reconstruction
