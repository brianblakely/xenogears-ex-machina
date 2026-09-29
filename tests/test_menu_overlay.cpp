// Invented menu-mode memory exercises the menu overlay's framework (memory,
// callee frames, heap, C library and libgpu helpers, arrival ordering) and
// some of its functions and their explicit unrecovered paths. It describes
// no original content.
#include "xem/reconstruction/menu_overlay.hpp"
#include "xem/reconstruction/menu_save.hpp"
#include "xem/reconstruction/resident_text.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace game = xem::reconstruction;
namespace menu = game::menu;
namespace {
void check(bool value, const char *message) {
    if (!value)
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

constexpr std::uint32_t state = 0x80100000;
constexpr std::uint32_t party = 0x80101000;
constexpr std::uint32_t card = 0x80102000;
constexpr std::uint32_t entry_sp = 0x801ffe00;

// A Program in menu mode: a menu state block, its party and card blocks,
// the game data, and a heap with one free block (80180008..80180fff).
game::Program sample() {
    game::Program program;
    auto &resident = program.resident;
    resident.game_state = 0x8006d634;
    resident.game_data.assign(game::game_data_bytes, 0);
    auto &regions = program.menu.emplace().regions;
    regions[menu::state_pointer] = {0x00, 0x00, 0x10, 0x80};
    regions[menu::overlay_base].resize(0x2e4a0); // the overlay image and statics
    regions[state].resize(0x2000);
    regions[party].resize(0x6c);
    regions[card].resize(0x5034);
    auto put32 = [&](std::uint32_t address, std::uint32_t value) {
        auto &bytes = regions[state];
        for (std::uint32_t i = 0; i < 4; ++i)
            bytes[address - state + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    put32(state + menu::state_party, party);
    put32(state + menu::state_card, card);
    auto &heap = resident.heap;
    heap.head = 0x80180008;
    heap.tag = 10;
    heap.headers = {{0x80180000, {0x80181008, 0}}, {0x80181000, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80180008, std::vector<std::uint8_t>(0xff8, 0xee)}};
    return program;
}

void memory_and_frames() {
    auto program = sample();
    game::FrameServices services;
    {
        menu::Overlay overlay(program, services, entry_sp);
        check(overlay.state() == state && overlay.at(0x33c) == state + 0x33c, "state pointer");
        // Game data is menu memory while the overlay runs.
        overlay.put16(0x8006d634 + 2, 0xbeef);
        check(overlay.u16(0x8006d636) == 0xbeef && overlay.s16(0x8006d636) == -0x4111,
              "game data access");
        // The stack keeps what callees left; frames follow the original SP.
        {
            const auto outer = overlay.enter(0x20);
            const auto locals = overlay.frame(0x20);
            check(locals.base == entry_sp - 0x20, "outer frame base");
            overlay.put32(locals[0x10], 0x12345678);
            {
                const auto inner = overlay.enter(0x18);
                check(overlay.frame(0x18).base == entry_sp - 0x38, "inner frame base");
                rejects<menu::MenuError>([&] { static_cast<void>(overlay.frame(0x20)); },
                                         "locals beyond the entered frame");
            }
            check(overlay.u32(locals[0x10]) == 0x12345678, "stack contents");
        }
        rejects<menu::MenuError>([&] { overlay.put8(0x80400000, 0); }, "address beyond RAM");
    }
    check(program.resident.game_data[2] == 0xef && program.resident.game_data[3] == 0xbe,
          "game data returns to the resident state");
    check(program.menu->stack.empty(), "the stack is released with the overlay");
}

void bounded_borrowed_stack() {
    auto program = sample();
    game::FrameServices services;
    // Invented stack bytes start immediately above this fixture's terminal
    // heap header. The heap header remains resident-owned, even though it
    // would lie inside a fixed 16 KiB window below the selected entry SP.
    constexpr auto heap_header = 0x80181000U;
    constexpr auto base = heap_header + 8U;
    std::vector<std::uint8_t> stack(0x40, 0xa5);
    const auto top = base + static_cast<std::uint32_t>(stack.size());
    {
        menu::Overlay overlay(program, services, top, nullptr, stack);
        check(program.menu->stack.empty() && program.menu->stack_base == base,
              "A shorter borrowed stack keeps its actual owner and lower bound");
        check(program.resident.heap.headers.at(heap_header)[1] == game::resident::heap_end_tag &&
                  program.resident.heap.held.at(0x80180008).back() == 0xee,
              "The terminal heap header remains outside the borrowed stack");
        {
            const auto outer = overlay.enter(0x28);
            const auto inner = overlay.enter(0x18);
            check(overlay.frame(0x18).base == base,
                  "Nested frames may reach the exact qualified lower bound");
            overlay.put32(base, 0x87654321);
            rejects<menu::MenuError>([&] { static_cast<void>(overlay.enter(4)); },
                                     "A further frame cannot borrow bytes from the heap header");
            check(overlay.frame(0x18).base == base && overlay.u32(base) == 0x87654321,
                  "A rejected frame preserves SP and the current qualified bytes");
            rejects<menu::MenuError>([&] { static_cast<void>(overlay.enter(0xffffffffU)); },
                                     "A frame size cannot underflow the stack pointer");
        }
        rejects<menu::MenuError>([&] { static_cast<void>(overlay.u32(top - 1)); },
                                 "A read cannot cross the borrowed stack's upper bound");
        rejects<menu::MenuError>([&] { overlay.set_stack_image(stack); },
                                 "A shorter borrowed owner cannot be replaced at the menu entry");
    }
    check(stack[0] == 0x21 && stack[3] == 0x87 && stack.back() == 0xa5 &&
              program.menu->stack.empty() &&
              program.resident.heap.headers.at(heap_header)[1] == game::resident::heap_end_tag &&
              program.resident.heap.held.at(0x80180008) == std::vector<std::uint8_t>(0xff8, 0xee),
          "Borrowed stack stores survive the Overlay without extending its owner");
    std::vector<std::uint8_t> unaligned(0x41, 0);
    rejects<menu::MenuError>(
        [&] { menu::Overlay overlay(program, services, top, nullptr, unaligned); },
        "A qualified stack must retain word-aligned original bounds");
    std::vector<std::uint8_t> oversized(menu::Overlay::stack_bytes + 4U, 0);
    rejects<menu::MenuError>(
        [&] { menu::Overlay overlay(program, services, top, nullptr, oversized); },
        "A borrowed owner cannot exceed the modeled original stack bound");
    check(program.resident.game_data.size() == game::game_data_bytes &&
              !program.menu->resident_memory && !program.menu->resident_tail,
          "Rejected stack qualifications do not take resident memory ownership");
}

void heap_and_library() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    const auto block = overlay.allocate(0x20, 0, 0x801c5f44);
    check(block == 0x80180008 && overlay.u8(block) == 0xee, "allocation hands the held bytes");
    check(overlay.bzero(block, 0x20) == block && overlay.u32(block + 0x1c) == 0, "bzero");
    check(overlay.bzero(block, 0) == 0 && overlay.bzero(0, 4) == 0, "bzero without bytes");
    overlay.put32(block, 0x00636261); // "abc"
    check(overlay.strlen(block) == 3, "strlen");
    check(overlay.strcpy(block + 8, block) == block + 8 && overlay.u32(block + 8) == 0x00636261,
          "strcpy");
    check(overlay.strcat(block + 8, block) == block + 8 && overlay.strlen(block + 8) == 6,
          "strcat");
    check(overlay.strcat(block, block) == 0, "strcat onto itself");
    check(overlay.memmove(block + 1, block, 3) == 0x61 && overlay.u8(block + 3) == 0x63,
          "memmove backward copy returns the last byte");
    check(overlay.memcpy(block + 0x10, block, 2) == block + 0x10, "memcpy");
    overlay.release(block, 0x801c60fc);
    check(!program.menu->regions.contains(block), "a released block leaves menu memory");
    rejects<game::resident::HeapError>([&] { overlay.release(0x80170000, 0x801c60fc); },
                                       "releasing a block the menu does not own");
    // rand (8003fa38) through 8001bd40.
    program.resident.random_seed = 1;
    check(overlay.random_range(0xff, 3) == 0xff && overlay.random_range(2, 0) == 0 &&
              overlay.random_range(7, 7) == 7,
          "random range special cases");
    const auto value = overlay.random_range(0, 0xff);
    check(program.resident.random_seed == 1U * 0x41c64e6dU + 12345U &&
              value == ((program.resident.random_seed >> 16U) & 0xffU),
          "random byte");
}

void packed_resource_header_boundary() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    const auto source = overlay.allocate(16, 0, 0x801c72fc);
    // Five literals and three distance-one copies produce fourteen bytes.
    // The next flag is read from the resident header at source + 16.
    const std::array<std::uint8_t, 16> packed{14,  0,   0, 0, 0xe0, 'a', 'b', 'c',
                                              'd', 'e', 1, 0, 1,    0,   1,   0};
    for (std::size_t i = 0; i < packed.size(); ++i)
        overlay.put8(source + static_cast<std::uint32_t>(i), packed[i]);
    const auto output = game::resident::unpack_to_new_block(
        overlay, source, 0, [&](std::uint32_t size, std::uint32_t mode) {
            return overlay.allocate(size, mode, game::resident::unpack_allocation_site);
        });
    const auto header = program.resident.heap.headers.at(source + 16);
    check(output == source + 24 && overlay.u8(output) == 'a' && overlay.u8(output + 4) == 'e' &&
              overlay.u8(output + 13) == 'e',
          "A packed resource terminates through the next resident-owned header");
    check(overlay.u32(source + 16) == header[0] && overlay.u32(source + 20) == header[1] &&
              !program.menu->regions.contains(source + 16),
          "Adjacent header reads use current typed heap words without transferring ownership");
    overlay.release(output, 0x801c60fc);
    overlay.put8(output + 3, 0x7b);
    check(program.resident.heap.held.at(output)[3] == 0x7b && overlay.u8(output + 3) == 0x7b,
          "Released bytes remain accessible through their single resident heap owner");
    rejects<std::exception>([&] { static_cast<void>(overlay.u8(0x80170000)); },
                            "An address without a qualified owner still stops");
}

void repeated_heap_releases() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    auto &heap = program.resident.heap;
    const auto first = overlay.allocate(0x20, 0, 0x801c5f44);
    const auto second = overlay.allocate(0x20, 0, 0x801c5f44);
    overlay.put32(first, 0x12345678);
    overlay.release(first, 0x801c60fc);
    const auto returned = heap.held.at(first);
    heap.dirty = 0;
    overlay.release(first, 0x801da56c);
    check(heap.dirty == 1 && heap.headers.at(first - 8)[1] == 0x84000000 &&
              heap.held.at(first) == returned && !program.menu->regions.contains(first),
          "A repeated release rewrites its free header while the heap retains the bytes");
    game::resident::heap_set_keep(heap, first, true);
    heap.dirty = 0;
    overlay.release(first, 0x801da56c);
    check(heap.dirty == 0 && !program.menu->regions.contains(first) &&
              heap.headers.at(first - 8)[1] == (0x84000000U | game::resident::heap_keep),
          "A kept free header does not create a new menu owner");
    game::resident::heap_set_keep(heap, first, false);
    overlay.release(second, 0x801da5a4);
    game::resident::heap_coalesce(heap);
    check(!heap.headers.contains(second - 8), "The second free header is now heap-held bytes");
    heap.dirty = 0;
    overlay.release(second, 0x801da5a4);
    const auto offset = second - 4 - first;
    const auto &held = heap.held.at(first);
    check(heap.dirty == 1 && held[offset] == 0 && held[offset + 1] == 0 && held[offset + 2] == 0 &&
              held[offset + 3] == 0x84 && !program.menu->regions.contains(second),
          "A stale release writes only the flags word held by the heap");
    auto other = game::resident::heap_allocate(heap, 0x20, 0, 0x80012340);
    check(other.has_value(), "Invented non-menu owner allocation");
    const auto owned_header = heap.headers.at(other->address - 8);
    const auto owned_bytes = other->bytes;
    rejects<menu::MenuError>([&] { overlay.release(other->address, 0x801da5a4); },
                             "A live allocation cannot be released by a menu without its bytes");
    check(heap.headers.at(other->address - 8) == owned_header && other->bytes == owned_bytes,
          "Rejecting a live unowned release preserves the existing owner and header");
}

void equipment_cleanup() {
    auto program = sample();
    auto &heap = program.resident.heap;
    heap.headers = {{0x80180000, {0x80185008, 0}}, {0x80185000, {0, game::resident::heap_end_tag}}};
    heap.held = {{0x80180008, std::vector<std::uint8_t>(0x4ff8, 0xee)}};
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    const auto title = overlay.allocate(0x74, 0, 0x801d3380);
    overlay.put32(state + 0x43c, title);
    overlay.put8(party + 0x49, 1);
    for (const auto index : {3U, 4U}) {
        overlay.put32(state + 0x364 + 4 * index, overlay.allocate(0x20, 0, 0x801d4ef4));
        overlay.put32(state + 0x380 + 4 * index, overlay.allocate(0x20, 0, 0x801d4f10));
        overlay.put8(party + 0x20 + index, 1);
        overlay.put8(party + 0x27 + index, 1);
    }
    const auto tables = overlay.allocate(0xcc, 0, 0x801c5f44);
    const auto loaded = overlay.allocate(0x1198, 0, 0x801c5f44);
    const auto table_item = overlay.allocate(0x20, 0, 0x801c77a4);
    const auto payload = overlay.allocate(0x20, 0, 0x801c7acc);
    overlay.put32(state + menu::state_tables, tables);
    overlay.put32(state + 0x42c, loaded);
    overlay.put32(tables + 0x1c, table_item);
    overlay.put32(loaded + 0x1180, payload);
    overlay.put32(payload, 0x89abcdef);
    overlay.put8(party + 0x48, 1);
    // 801da518 executes every source release, including data-set 10's
    // releases followed by repeated releases of payload and table_item.
    overlay.close_equipment_shared();
    check(heap.headers.at(loaded - 8)[1] == 0x84000000 &&
              heap.headers.at(table_item - 8)[1] == 0x84000000 &&
              heap.headers.at(payload - 8)[1] == 0x84000000 &&
              !program.menu->regions.contains(loaded) &&
              !program.menu->regions.contains(table_item) &&
              !program.menu->regions.contains(payload) && !program.menu->regions.contains(title),
          "Equipment cleanup returns its blocks to the resident heap including source repeats");
    check(overlay.u8(party + 0x48) == 0 && overlay.u8(party + 0x49) == 0 &&
              overlay.u8(party + 0x23) == 0 && overlay.u8(party + 0x24) == 0 &&
              overlay.u8(party + 0x2a) == 0 && overlay.u8(party + 0x2b) == 0 &&
              overlay.u32(tables + 0x1c) == table_item && overlay.u32(state + 0x42c) == loaded &&
              heap.held.at(payload)[0] == 0xef,
          "Cleanup clears only the source flags and retains the freed bytes and stale pointers");
}

void retained_release_spills() {
    {
        auto program = sample();
        auto block = game::resident::heap_allocate(program.resident.heap, 0x20, 0, 0x801c5f44);
        check(block.has_value(), "Invented release-frame allocation");
        const auto address = block->address;
        program.menu->regions[address] = std::move(block->bytes);
        game::FrameServices services;
        // The release's exact 20h callee frame is this owner's entire span.
        std::vector<std::uint8_t> stack(0x20, 0xa5);
        menu::Overlay overlay(program, services, entry_sp, nullptr, stack);
        constexpr auto base = entry_sp - 0x20;
        constexpr auto first_site = 0x801d3470U;
        overlay.release(address, first_site);
        check(overlay.u32(base + 0x18) == first_site + 8 &&
                  overlay.u32(base + 0x10) == 0xa5a5a5a5 &&
                  !program.menu->regions.contains(address),
              "A non-null release retains its supplied caller RA and leaves other locals intact");
        constexpr auto repeated_site = 0x801da56cU;
        overlay.release(address, repeated_site);
        check(overlay.u32(base + 0x18) == repeated_site + 8 &&
                  program.resident.heap.headers.at(address - 8)[1] == 0x84000000,
              "A repeated release reuses the same frame with its actual callsite");
        program.resident.heap.quiet = 1;
        constexpr auto null_site = 0x801da5a4U;
        overlay.release(0, null_site);
        check(overlay.u32(base + 0x18) == null_site + 8 && overlay.u32(base + 0x10) == 0xa5a5a5a5 &&
                  overlay.frame(0).base == entry_sp,
              "A quiet null release saves RA without the fatal path's extra local store");
        program.resident.heap.quiet = 0;
        constexpr auto fatal_site = 0x801d4ef4U;
        rejects<game::resident::HeapError>(
            [&] { overlay.release(0, fatal_site); },
            "A nonquiet null release still stops at the fatal handler");
        check(overlay.u32(base + 0x18) == fatal_site + 8 &&
                  overlay.u32(base + 0x10) == fatal_site + 8 &&
                  program.resident.heap.last_size == 0 &&
                  program.resident.heap.last_caller == fatal_site &&
                  overlay.frame(0).base == entry_sp,
              "The null fatal path retains both original RA stores before its explicit stop");
    }
    {
        auto program = sample();
        auto block = game::resident::heap_allocate(program.resident.heap, 0x20, 0, 0x801c5f44);
        check(block.has_value(), "Invented bounded-release allocation");
        const auto address = block->address;
        program.menu->regions[address] = std::move(block->bytes);
        const auto bytes = program.menu->regions.at(address);
        const auto header = program.resident.heap.headers.at(address - 8);
        game::FrameServices services;
        std::vector<std::uint8_t> stack(0x1c, 0xa5);
        menu::Overlay overlay(program, services, entry_sp, nullptr, stack);
        rejects<menu::MenuError>([&] { overlay.release(address, 0x801d3470); },
                                 "The full release frame must fit the qualified owner");
        check(stack == std::vector<std::uint8_t>(0x1c, 0xa5) &&
                  program.menu->regions.at(address) == bytes &&
                  program.resident.heap.headers.at(address - 8) == header &&
                  overlay.frame(0).base == entry_sp,
              "A rejected release frame preserves the stack, current SP and live heap owner");
    }
}

void gpu_helpers() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    const auto packet = state + 0x100;
    const auto table = state + 0x200;
    overlay.put32(table, 0x00ffffff);
    overlay.set_poly_ft4(packet);
    check(overlay.u8(packet + 3) == 9 && overlay.u8(packet + 7) == 0x2c, "SetPolyFT4");
    overlay.set_semi_trans(packet, 1);
    overlay.set_shade_tex(packet, 1);
    check(overlay.u8(packet + 7) == 0x2f, "semi-transparent raw texture");
    overlay.add_prim(table, packet);
    check((overlay.u32(table) & 0xffffffU) == (packet & 0xffffffU) &&
              (overlay.u32(packet) & 0xffffffU) == 0xffffffU,
          "AddPrim links the packet first");
    overlay.set_line_f3(packet + 0x40);
    check(overlay.u32(packet + 0x54) == 0x55555555U, "SetLineF3 terminator");
    check(menu::Overlay::get_clut(0x100, 0x1d1) == ((0x1d1U << 6U) | 0x10U), "GetClut");
    // 80035734: controller kind by the receive buffer.
    program.resident.pad.buffers[0][0] = 0;
    program.resident.pad.buffers[0][1] = 0x41;
    check(overlay.pad_kind(0) == 1, "digital pad");
    program.resident.pad.buffers[0][1] = 0x12;
    check(overlay.pad_kind(0) == 2, "mouse");
    program.resident.pad.buffers[0][1] = 0x23;
    check(overlay.pad_kind(0) == 0xffffffffU, "unknown kind");
    program.resident.pad.buffers[0][0] = 0xff;
    check(overlay.pad_kind(0) == 0, "no controller");
}

void retained_gpu_spills() {
    auto program = sample();
    auto &gpu = program.resident.gpu;
    gpu.services = 0x80056888;
    gpu.functions[2] = 0x8004668c;
    gpu.functions[6] = 0x800465ec;
    gpu.queued = 0;
    program.resident.interrupts.registers[1] = 0x1f801074;
    game::FrameServices services;
    services.vblank_counts = {10, 11};
    services.interrupt_masks = {1, 1};
    menu::Overlay overlay(program, services, entry_sp);
    const std::array<std::uint32_t, 8> registers{0x17, 0x29, 0x31, 0x43, 0, 0, 0, 0};
    overlay.set_saved_registers(registers);
    rejects<menu::MenuError>([&] { overlay.set_saved_registers({}); },
                             "saved registers require exactly S0..S7");
    constexpr auto rectangle = state + 0x100;
    constexpr auto table = state + 0x200;
    overlay.put16(rectangle + 4, 32);
    overlay.put16(rectangle + 6, 16);
    // A menu frame sends MoveImage followed by DrawOTag. Their nested
    // 8004668c frames leave argument and inherited-register data in the
    // locations the later new-game name buffer reuses.
    {
        const auto frame = overlay.enter(0x18);
        overlay.move_image(rectangle, 7, 123);
        check(overlay.u32(entry_sp - 0x40) == 0x80044a04,
              "MoveImage lends its source queue return PC to the shared helper");
        overlay.draw_otag(table);
        check(overlay.u32(entry_sp - 0x38) == 0x80044c30,
              "DrawOTag uses its own source queue return PC at the reused depth");
    }
    auto &regions = program.menu->regions;
    regions[menu::text_state].resize(4);
    regions[menu::text_single_limit].resize(4);
    overlay.put32(menu::text_state, state + 0x300);
    overlay.put32(state + 0x36c, state + 0x400);
    overlay.put8(state + 0x400, 0);
    overlay.put8(state + 0x401, 'Q');
    overlay.put16(state + 0x500, 0);
    {
        const auto command = overlay.enter(0x20);
        const auto new_game = overlay.enter(0x58);
        const auto text = overlay.frame(0x58)[0x28];
        overlay.decode_text(state + 0x500, text, 1);
        check(overlay.u8(text) == 'Q' && overlay.u8(text + 1) == 0,
              "the decoder writes its text and terminator");
        check(overlay.u32(text + 4) == 123 && overlay.u32(text + 8) == table &&
                  overlay.u32(text + 12) == registers[1] && overlay.u32(text + 16) == registers[2],
              "a short name preserves the source-derived GPU stack tail");
    }
    {
        const auto save_payload = overlay.enter(0x58);
        const auto decode_names = overlay.enter(0x58);
        const auto text = overlay.frame(0x58)[0x28];
        overlay.decode_text(state + 0x500, text, 1);
        check(overlay.u32(text + 4) == 0x14 && overlay.u32(text + 8) == table &&
                  overlay.u32(text + 12) == 0 && overlay.u32(text + 16) == 0x80046f0c,
              "save name decoding retains queue arguments and VSync's source return PC");
    }
    // Empty rectangles skip enqueue, leaving its deeper spill words alone.
    const auto before = overlay.u32(entry_sp - 0x44);
    overlay.put16(rectangle + 4, 0);
    {
        const auto frame = overlay.enter(0x18);
        overlay.move_image(rectangle, 9, 234);
    }
    check(overlay.u32(entry_sp - 0x44) == before,
          "an empty MoveImage does not enter the GPU queue helper");
    {
        const auto deadline = program.resident.gpu.deadline;
        const auto frame = overlay.enter(menu::Overlay::stack_bytes - 0x20);
        rejects<game::MissingDependency>([&] { overlay.draw_otag(table); },
                                         "the shared queue cannot cross the original stack owner");
        check(program.resident.gpu.deadline == deadline,
              "an unavailable source callee frame stops before consuming its alarm input");
    }
}

void retained_load_spills() {
    auto program = sample();
    program.resident.disc_stream.host_file_table = 1; // idle without a DMA3 read
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    constexpr auto payload = state + 0x700;
    constexpr auto compressed_file = state + 0x900;
    const std::array<std::uint32_t, 8> incoming{payload, 5, 2, 0, 0, 0, 0, 0};
    overlay.set_saved_registers(incoming);
    {
        const auto data_set = overlay.enter(0x28);
        const std::array<std::uint32_t, 8> data_registers{1, compressed_file, 2, 1, 0, 0, 0, 0};
        overlay.set_saved_registers(data_registers);
        overlay.disc_wait(0);
        {
            const auto unpack = overlay.enter(8);
            static_cast<void>(overlay.allocate(32, 0, 0x80032e94));
        }
    }
    {
        const auto apply_payload = overlay.enter(0x18);
        // Invalid payload bounds stop after the recovered restore prologue.
        rejects<menu::MenuError>([&] { overlay.restore_payload_game_data(0, 0); },
                                 "restore does not accept an unowned payload");
        const auto decode_names = overlay.enter(0x58);
        const auto text = overlay.frame(0x58)[0x28];
        check(overlay.u32(text + 4) == 0x80028a84 && overlay.u32(text + 8) == 1 &&
                  overlay.u32(text + 12) == compressed_file && overlay.u32(text + 16) == payload,
              "load name decoding retains disc, heap and restore source spills");
    }
}

void functions() {
    auto program = sample();
    game::FrameServices services;
    menu::Overlay overlay(program, services, entry_sp);
    // 801c7f34: 1 hour, 23 minutes, 45 seconds of VSyncs.
    overlay.split_play_time(((1 * 60 + 23) * 60 + 45) * 60);
    check(overlay.u32(state + 0x2f4) == 1 && overlay.u32(state + 0x2f8) == 2 &&
              overlay.u32(state + 0x2fc) == 3 && overlay.u32(state + 0x300) == 4 &&
              overlay.u32(state + 0x304) == 5 && overlay.u32(state + 0x2ec) == 0,
          "play time digits");
    // 801d22c4 hides both cursors.
    overlay.put8(party + 3, 1);
    overlay.put8(party + 4, 1);
    overlay.hide_cursors();
    check(overlay.u8(party + 3) == 0 && overlay.u8(party + 4) == 0, "hide cursors");
    // 801c7d78 decodes the queue's first menu button: pressed 4000 (code 1,
    // sound 1 with menu sounds off).
    auto &queue = program.resident.input_queue;
    program.resident.pad.buffers[0] = {};
    queue.count = 1;
    queue.ring[4][0] = 0x4000;
    overlay.decode_input();
    check(overlay.u8(state + 0x325) == 1, "input code");
    overlay.decode_input();
    check(overlay.u8(state + 0x325) == 8, "no input");
    // Unrecovered paths stop explicitly.
    program.resident.pad.buffers[0][0] = 0xff;
    rejects<game::MissingDependency>([&] { overlay.decode_input(); }, "controller wait");
    overlay.put8(state + 0x336, 2);
    rejects<game::MissingDependency>([&] { static_cast<void>(overlay.run_command(0)); },
                                     "menu command 2");
    rejects<game::MissingDependency>([&] { static_cast<void>(overlay.call(0x801c5000, {})); },
                                     "an entry that is not a function");
    check(overlay.call(0x801d22c4, {}) == 0, "dispatch by entry address");
}

void arrivals() {
    auto program = sample();
    game::FrameServices services;
    services.vblank_waits = {{1, 2}, {3, 4}};
    // One interrupt arrival after two events; the dispatcher is not set up,
    // so delivering it fails: nothing may deliver it earlier.
    program.resident.platform.push_back({game::PlatformInput::Kind::interrupt, 2, 0});
    menu::Overlay overlay(program, services, entry_sp);
    overlay.pass_position();
    check(overlay.events() == 1 && program.resident.platform.size() == 1, "arrival waits");
    overlay.vsync();
    check(overlay.events() == 2 && program.resident.platform.size() == 1,
          "a service result counts once consumed");
    rejects<std::exception>([&] { overlay.pass_position(); }, "the arrival is due");
}

void payload_positions() {
    {
        auto program = sample();
        game::FrameServices services;
        menu::Overlay overlay(program, services, entry_sp);
        overlay.payload_positions = true;
        bool observed = false;
        overlay.boundary = [&](std::string_view name) {
            check(name == "apply_loaded_payload_entry" && overlay.events() == 0,
                  "the payload entry precedes its position event");
            observed = true;
        };
        // The restored payload's bounds are deliberately invalid. Its entry
        // is still a position, without using any payload state as an input.
        rejects<menu::MenuError>([&] { overlay.apply_loaded_payload(0); },
                                 "invalid payload stops after its entry position");
        check(observed && overlay.events() == 1, "a payload entry counts exactly once");
    }
    {
        auto program = sample();
        game::FrameServices services;
        program.resident.platform.push_back({game::PlatformInput::Kind::interrupt, 1, 0});
        menu::Overlay overlay(program, services, entry_sp);
        overlay.payload_positions = true;
        rejects<menu::MenuError>([&] { overlay.apply_loaded_payload(0); },
                                 "invalid payload bounds stop before a later arrival");
        check(overlay.events() == 1 && program.resident.platform.size() == 1,
              "an arrival after payload entry waits for the next delivery point");
    }
    {
        auto program = sample();
        game::FrameServices services;
        program.resident.platform.push_back({game::PlatformInput::Kind::interrupt, 0, 0});
        menu::Overlay overlay(program, services, entry_sp);
        overlay.payload_positions = true;
        bool observed = false;
        overlay.boundary = [&](std::string_view) { observed = true; };
        // No dispatcher is initialized: the due arrival explicitly fails
        // before the observer or payload restore can run.
        rejects<std::exception>([&] { overlay.apply_loaded_payload(0); },
                                "an earlier arrival is delivered before payload entry");
        check(!observed && overlay.events() == 0 && program.resident.platform.empty(),
              "the earlier arrival precedes the payload position");
    }
}

void sound_positions() {
    {
        auto program = sample();
        game::FrameServices services;
        program.resident.platform.push_back({game::PlatformInput::Kind::interrupt, 1, 0});
        auto &sound = program.resident.sound;
        constexpr auto bank = 0x80110000U;
        constexpr auto effects = 0x80120000U;
        sound.flags = 0x800;
        sound.voice_limit = 2;
        sound.effect_banks = bank;
        sound.effect_block = effects;
        sound.objects[bank].resize(0x200);
        sound.objects[effects].resize(game::resident::voice_records +
                                      2U * game::resident::voice_stride);
        // Both invented effect table entries are zero: the actual sound
        // function computes two inactive voices, with no wave-bank input.
        menu::Overlay overlay(program, services, entry_sp);
        overlay.put8(state + menu::state_sound, 1);
        overlay.put32(state + menu::state_effect_bank, bank);
        overlay.sound_positions = true;
        overlay.play_sound(7);
        const auto &bytes = sound.objects.at(effects);
        check(sound.effect_run == 2 && bytes[game::resident::voice_records + 8] == 7 &&
                  bytes[0x10] == 0 && bytes[0x11] == 0x80,
              "Sound setup computes its original voice state before a later arrival");
        check(overlay.events() == 1 && program.resident.platform.size() == 1,
              "A sound entry counts once and keeps its later arrival pending");
        // The next delivery point reaches the deliberately absent dispatcher.
        rejects<std::exception>([&] { overlay.catch_up(); },
                                "The later arrival is delivered after sound setup");
        check(program.resident.platform.empty(), "The deferred arrival is consumed once");
    }
    {
        auto program = sample();
        game::FrameServices services;
        program.resident.platform.push_back({game::PlatformInput::Kind::interrupt, 0, 0});
        menu::Overlay overlay(program, services, entry_sp);
        overlay.sound_positions = true;
        rejects<std::exception>([&] { overlay.play_sound(7); },
                                "An earlier arrival precedes sound entry and computation");
        check(overlay.events() == 0 && program.resident.platform.empty(),
              "An earlier failed arrival does not advance the sound position");
    }
}
} // namespace

int main() {
    try {
        memory_and_frames();
        bounded_borrowed_stack();
        heap_and_library();
        packed_resource_header_boundary();
        repeated_heap_releases();
        equipment_cleanup();
        retained_release_spills();
        gpu_helpers();
        retained_gpu_spills();
        retained_load_spills();
        functions();
        arrivals();
        payload_positions();
        sound_positions();
    } catch (const std::exception &error) {
        std::cerr << "menu overlay test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "menu overlay tests passed\n";
    return 0;
}
