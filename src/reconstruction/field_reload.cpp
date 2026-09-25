// The field reload 800a5c40: a map change inside the field mode, from the
// old field's teardown to the new field's fade-in. Original addresses name
// correlations only; owned records are Program state.
#include "xem/reconstruction/field_script.hpp"
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <optional>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Reload read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("The field reload requires field state");
    return *program.field;
}
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point,
              bool completed) {
    if (observe)
        observe(program, point, completed);
}
// MIPS DIV quotient; division by zero has no defined result.
std::int32_t divide(std::int32_t dividend, std::int32_t divisor, std::uint32_t site) {
    if (divisor == 0)
        throw MissingDependency({"division_by_zero", site, {}, {}}, "state:division-by-zero", false,
                                "An original division by zero has no recovered result");
    if (dividend == std::numeric_limits<std::int32_t>::min() && divisor == -1)
        return dividend;
    return dividend / divisor;
}
constexpr std::uint32_t transition_packets = 0x800b1274; // 5 records, a 28h packet per buffer
constexpr std::uint32_t transition_modes = 0x800b11ac;   // 5 records, a 0ch draw mode per buffer
constexpr std::uint32_t transition_corners = 0x800b1404; // 5 records of four vectors
} // namespace

// 8009fee4: record party slot `slot`'s map and position in its variables
// (2a, 30, 36 map | height bits; then x and z).
void Program::party_record(std::uint32_t slot) {
    auto &state = loaded(*this);
    const auto index = state.party_indices.at(slot);
    if (index == 0xff)
        return;
    const auto &actor = state.actors.at(static_cast<std::size_t>(index)).storage;
    const auto height = u32(s16(word(actor, 0x10, 2))) << 14U;
    auto &variables = resident.variables;
    const auto base = static_cast<std::uint16_t>(0x2a + 6 * slot);
    variables.write(base, s32((resident.field_map & 0xfffU) | height));
    variables.write(static_cast<std::uint16_t>(base + 2), s16(word(actor, 0x22, 2)));
    variables.write(static_cast<std::uint16_t>(base + 4), s16(word(actor, 0x2a, 2)));
}

// 800a6408: rotate and scale the five quads by 800b00b8 and the zoom, place
// them unless the zoom is 1000h, and link each quad after its draw mode at
// the head of the current buffer's small table.
void Program::reload_transition_draw() {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    auto m = field::rotation_matrix(reload.angles, resident.math.trigonometry);
    m.t = {};
    field::scale_matrix(m, {reload.zoom, reload.zoom, reload.zoom});
    resident.gte.transform = m; // SetRotMatrix, SetTransMatrix
    for (std::uint32_t i = 0; i < 5; ++i) {
        const auto buffer = state.draw_buffer;
        const auto packet = transition_packets + 0x28U * buffer + 0x50U * i;
        if (reload.zoom != 0x1000)
            static_cast<void>(rot_average4(transition_corners + 0x20U * i, packet)); // 8004a7bc
        const auto table = state.draw_block + 0x80d4U;
        add_primitive(table, packet);
        add_primitive(table, transition_modes + 0xcU * buffer + 0x18U * i);
    }
}

// 800a5600: the RGB bytes of the next buffer's five quads.
void Program::reload_transition_shade(std::uint32_t value) {
    auto &state = loaded(*this);
    for (std::uint32_t i = 0; i < 5; ++i)
        for (std::uint32_t c = 4; c < 7; ++c)
            set_memory(transition_packets + 0x28U * ((state.draw_buffer + 1U) & 1U) + 0x50U * i + c,
                       value & 0xffU, 1);
}

std::int32_t Program::field_reload_fade_frame(FrameServices &services, std::int32_t shade,
                                              const ProgramObserver &observe) {
    auto &reload = loaded(*this).reload;
    field_pre_frame(services);
    reload_transition_draw();
    field_frame(services, observe);
    field_post_frame();
    reload_transition_shade(u32(shade >> 16));
    shade -= divide(0x800000, s32(reload.fade_frames), 0x800a617c);
    if (shade < 0)
        shade = 0;
    if (reload.zoom < 0x22c0)
        reload.zoom += 0x20;
    return shade;
}

void Program::field_reload_fade_in(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    field::begin_fade_out(state.fade, s32(reload.fade_frames)); // 80071e58
    std::int32_t shade = 0x800000;
    for (std::int32_t i = 0; i < s32(reload.fade_frames); ++i) {
        shade = field_reload_fade_frame(services, shade, observe);
        observed(observe, *this, {"field_reload_fade_frame", 0x800a61c8, {}, {}}, true);
    }
}

// 800a63a0 onward for a reload that is not type 6: load the saved VRAM back
// (800a91f0), reset the reload type and fade length, then reload the text
// palettes (80077544) and coalesce the heap (80031ff8).
void Program::field_reload_finish(FrameServices &services, std::uint32_t frame) {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    if (state.background_mode == 6)
        throw MissingDependency({"field_reload_finish", 0x800a63ac, {}, {}},
                                "symbol:field-reload-type-6", false,
                                "Reload type 6 keeps its VRAM save; not recovered");
    if (state.particles_paused != 0) {
        // 800a91f0.
        reload.vram_rect = {0x3c0, 0x100, 0x40, 0x100};
        state.particles_paused = 0;
        if (reload.vram_save.bytes.empty() || reload.vram_save.address != reload.vram_save_address)
            throw field::FieldFormatError("The saved VRAM block is not owned");
        auto rect = reload.vram_rect;
        static_cast<void>(load_image(rect, 0x800afc28, reload.vram_save_address, &services));
        reload.vram_rect = rect;
        draw_sync(services);
        if (resident::heap_release(resident.heap, reload.vram_save, 0x800a925c) != 0)
            throw field::FieldFormatError("The saved VRAM block was not released");
        reload.vram_save = {};
    }
    state.background_mode = 2;
    reload.fade_frames = 0x20;
    state.dialogue_gate_afd04 = 0;
    // 80077544: without the diagnostic build, 80033698(100, f0) loads the
    // text palette row and stores both palettes' CLUT ids.
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_reload_finish", 0x80077558, {}, {}},
                                "symbol:field-debug-8003747c", false,
                                "The diagnostic capture setup is not recovered");
    std::array<std::int16_t, 4> rect{0x100, 0xf0, 0x20, 1};
    // The rectangle is on 80033698's stack: frame - 38h (80077544) - 28h + 10h.
    static_cast<void>(load_image(rect, frame - 0x38U - 0x28U + 0x10U, 0x80050190, &services));
    const auto clut = [](std::uint32_t x, std::uint32_t y) { // 80043a58 GetClut
        return static_cast<std::uint16_t>(y << 6U | (x >> 4U & 0x3fU));
    };
    resident.text_cluts = {clut(0x100, 0xf0), clut(0x110, 0xf0)};
    resident::heap_coalesce(resident.heap);
}

// VSync(0): wait for the next vertical blank; libetc keeps root counter 1
// and the vertical-blank counter (80057844, 80057848) at its return.
void Program::vertical_sync(FrameServices &services) {
    if (services.vblank_waits.empty())
        throw ServiceUnavailable("VSync(0) result");
    const auto wait = services.vblank_waits.front();
    services.vblank_waits.pop_front();
    resident.vsync_hcount = wait[0];
    resident.vsync_previous = wait[1];
    deliver_stage_arrivals();
}

void Program::store_image(FrameServices &services, std::array<std::int16_t, 4> &rect,
                          std::uint32_t address, std::uint32_t destination) {
    auto &gpu = resident.gpu;
    gpu_check_rect();
    if (gpu.services != 0x80056888 || gpu.functions[2] != 0x8004668c ||
        gpu.functions[7] != 0x800462dc)
        throw MissingDependency({"gpu_services", 0x80044930, {}, {}}, "symbol:gpu-services", false,
                                "Only the observed libgpu StoreImage service is recovered");
    static_cast<void>(gpu_enqueue(0x800462dc, address, &rect, 8, destination, &services));
}

// MoveImage (8004495c): the VRAM copy packet at 80056978 (source corner,
// destination, size), sent through _cwc.
void Program::move_image(FrameServices &services, const std::array<std::int16_t, 4> &rect,
                         std::int32_t x, std::int32_t y) {
    auto &gpu = resident.gpu;
    gpu_check_rect();
    if (rect[2] == 0 || rect[3] == 0)
        return;
    const auto put = [&](std::size_t at, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i)
            gpu.move_packet[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    const auto pair = [](std::int32_t low, std::int32_t high) {
        return (u32(high) << 16U) | (u32(low) & 0xffffU);
    };
    put(0xc, pair(x, y));
    put(0x8, pair(rect[0], rect[1]));
    put(0x10, pair(rect[2], rect[3]));
    if (gpu.services != 0x80056888 || gpu.functions[2] != 0x8004668c ||
        gpu.functions[6] != 0x800465ec)
        throw MissingDependency({"gpu_services", 0x800449f0, {}, {}}, "symbol:gpu-services", false,
                                "Only the observed libgpu MoveImage service is recovered");
    static_cast<void>(gpu_enqueue(0x800465ec, 0x80056978, nullptr, 0x14, 0, &services));
}

void Program::field_reload_teardown(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    auto &heap = resident.heap;
    const auto done = [&](std::string_view operation, std::uint32_t address) {
        deliver_stage_arrivals();
        observed(observe, *this, {operation, address, {}, {}}, true);
    };
    const auto release = [&](resident::HeapBlock &block, std::uint32_t site) {
        if (resident::heap_release(heap, block, site) != 0)
            throw field::FieldFormatError("A reload block was not released");
        block = {};
    };
    const auto keep = [&](std::uint32_t address) -> std::uint32_t & {
        const auto found = heap.headers.find(address - 8);
        if (found == heap.headers.end())
            throw field::FieldFormatError("Reload block has no heap header");
        return found->second[1];
    };
    // 8003748c: release the block 80059394 names (unless 800593a0 is set).
    if (resident.w_59394 != 0)
        throw MissingDependency({"field_reload", 0x800374a0, {}, {}}, "symbol:block-80059394",
                                false, "The block 80059394 names is not recovered");
    resident.w_593a0 = 0;
    // 800a9460: stop the 64 particle emitters (800a92ac), then DrawSync and VSync.
    for (std::size_t slot = 0; slot < state.particle_slots.size(); ++slot) {
        if (state.particle_slots[slot] == 1)
            throw MissingDependency({"field_reload", 0x800a92dc, {}, {}}, "symbol:field-particles",
                                    false, "Releasing an active particle emitter is not recovered");
        state.particle_slots[slot] = 0;
        reload.particle_ids[slot] = -1;
    }
    draw_sync(services);
    vertical_sync(services);
    // 800864f0: forget the positional emitters and stop the effect pairs
    // whose bit in 800b233c is clear.
    for (auto &emitter : state.emitters) {
        emitter[1] = 0xffff;
        emitter[0] = 0xffff;
    }
    for (std::uint32_t pair = 0; pair < 4; ++pair) {
        if ((reload.effects_kept & 1U) == 0)
            resident::stop_effect_pair(resident.sound, pair * 2);
        reload.effects_kept = static_cast<std::uint16_t>(reload.effects_kept >> 1U);
    }
    // 8007ffe8: close each open dialogue window (8007f6f8).
    for (std::uint32_t w = 0; w < 4; ++w)
        if (state.dialogue[w].half(0x3f6) == 0)
            close_dialogue(w);
    done("reload_suspend", 0x800a5c70);
    if (state.background_mode != 6) {
        // 800a915c: pause particles and save 40h x 100h of VRAM at (3c0, 100).
        if (state.particles_paused != 1) {
            state.particles_paused = 1;
            heap.tag = 8; // 80032498(8, 0)
            heap.tag_words[8] = 0;
            heap.quiet = 0;
            auto block = resident::heap_allocate(heap, 0x8000, 1, 0x800a918c);
            if (!block)
                throw field::FieldFormatError("The VRAM save allocation failed");
            reload.vram_save = std::move(*block);
            reload.vram_save_address = reload.vram_save.address;
            reload.vram_rect = {0x3c0, 0x100, 0x40, 0x100};
            auto rect = reload.vram_rect;
            store_image(services, rect, 0x800afc28, reload.vram_save.address);
            reload.vram_rect = rect;
            draw_sync(services);
        }
        if (state.background_mode != 4) {
            // 800a4748 -> 800a476c(2c0, 100): copy the displayed 140h x e0h
            // screen to (2c0, 100), then DrawSync and VSync.
            resident.gte.screen.h = 0x200; // SetGeomScreen
            move_image(services, {0, 0, 0x140, 0xe0}, 0x2c0, 0x100);
            draw_sync(services);
            vertical_sync(services);
        }
    }
    draw_sync(services);
    done("reload_save_screen", 0x800a5cb0);
    swap_draw_buffer();
    draw_sync(services);
    vertical_sync(services);
    done("reload_next_buffer", 0x800a5cc0);
    field_teardown(services);
    done("field_teardown", 0x800a5cc8);
    // Move the read-ahead map data: copy it to a first-fit block, release it,
    // move the VRAM save (800a90b4), then copy it back to a last-fit block.
    const auto size = resident.preload_size;
    auto &preload = resident.preload_block;
    if (preload.bytes.size() < size)
        throw field::FieldFormatError("The read-ahead block's bytes are not owned");
    auto copy = resident::heap_allocate(heap, size, 0, 0x800a5cd0);
    if (!copy)
        throw field::FieldFormatError("The read-ahead copy allocation failed");
    std::copy_n(preload.bytes.begin(), size, copy->bytes.begin());
    // The copy is held only here: raw heap contents until it is released.
    const auto copy_address = copy->address;
    resident.heap_contents[copy_address] = std::move(copy->bytes);
    keep(preload.address) &= ~resident::heap_keep;
    const auto preload_address = preload.address;
    release(preload, 0x800a5d0c);
    preload.address = preload_address; // 8005a4e0 keeps naming the released block
    done("reload_copy_preload", 0x800a5d14);
    if (state.background_mode != 6 && state.particles_paused == 1) {
        heap.tag = 8;
        heap.tag_words[8] = 0;
        heap.quiet = 0;
        auto moved = resident::heap_allocate(heap, 0x8000, 1, 0x800a90e4);
        if (!moved)
            throw field::FieldFormatError("The VRAM save move allocation failed");
        std::copy_n(reload.vram_save.bytes.begin(), 0x8000, moved->bytes.begin());
        release(reload.vram_save, 0x800a9138);
        reload.vram_save = std::move(*moved);
        reload.vram_save_address = reload.vram_save.address;
    }
    auto back = resident::heap_allocate(heap, size, 1, 0x800a5d38);
    if (!back)
        throw field::FieldFormatError("The read-ahead allocation failed");
    std::copy_n(resident.heap_contents.at(copy_address).begin(), size, back->bytes.begin());
    preload = std::move(*back);
    keep(preload.address) |= resident::heap_keep;
    static_cast<void>(release_owned_block(copy_address, 0x800a5d6c));
    if (state.background_mode != 2)
        throw MissingDependency({"field_reload", 0x800a5d9c, {}, {}},
                                "symbol:field-reload-type-" + std::to_string(state.background_mode),
                                false, "Only reload type 2 is recovered");
    done("reload_teardown", 0x800a6020);
}

std::span<std::uint8_t> Program::owned_span(std::uint32_t address) {
    address = 0x80000000U | (address & 0x1fffffU);
    auto &contents = resident.heap_contents;
    if (const auto after = contents.upper_bound(address); after != contents.begin()) {
        auto &[at, bytes] = *std::prev(after);
        if (address - at < bytes.size())
            return std::span(bytes).subspan(address - at);
    }
    if (field) {
        auto &state = *field;
        const auto window = [&](std::uint32_t base, std::span<std::uint8_t> bytes) {
            return address >= base && address - base < bytes.size() ? bytes.subspan(address - base)
                                                                    : std::span<std::uint8_t>{};
        };
        for (auto &actor : state.actors) {
            if (actor.extension_110)
                if (const auto bytes = window(word(actor.storage, 0x110), *actor.extension_110);
                    !bytes.empty())
                    return bytes;
            if (actor.extension_114)
                if (const auto bytes = window(word(actor.storage, 0x114), *actor.extension_114);
                    !bytes.empty())
                    return bytes;
            if (const auto bytes = window(actor.sprite.parts.address, actor.sprite.parts.bytes);
                !bytes.empty())
                return bytes;
        }
        for (auto &piece : state.pieces)
            if (const auto bytes = window(piece.address, piece.descriptor); !bytes.empty())
                return bytes;
        if (const auto bytes = window(state.collision_address, state.collision_component);
            !bytes.empty())
            return bytes;
        if (const auto bytes = window(state.messages_address, state.messages); !bytes.empty())
            return bytes;
        for (auto &blocks : state.dialogue_blocks)
            for (auto &block : blocks)
                if (const auto bytes = window(block.address, block.bytes); !bytes.empty())
                    return bytes;
        if (const auto bytes = state.regions.span(address); !bytes.empty())
            return bytes;
        for (auto &resource : state.resources)
            if (const auto bytes = window(resource.address, resource.bytes); !bytes.empty())
                return bytes;
        auto &saved = state.reload.vram_save;
        if (const auto bytes = window(saved.address, saved.bytes); !bytes.empty())
            return bytes;
    }
    return record_block(address);
}

namespace {
// The allocated heap block holding `address`: its data address and size.
std::optional<std::pair<std::uint32_t, std::uint32_t>> allocated_block(const resident::Heap &heap,
                                                                       std::uint32_t address) {
    const auto after = heap.headers.upper_bound(address - 8);
    if (after == heap.headers.begin())
        return {};
    const auto &[header, words] = *std::prev(after);
    const auto tag = words[1] & resident::heap_tag_mask;
    const auto data = header + 8;
    if (tag == 0 || tag == resident::heap_end_tag || address < data || address >= words[0] - 8)
        return {};
    return std::pair{data, words[0] - 8 - data};
}
} // namespace

std::uint32_t Program::release_owned_block(std::uint32_t address, std::uint32_t site) {
    auto &heap = resident.heap;
    const auto header = heap.headers.find(address - 8);
    resident::HeapBlock block{address, {}};
    std::uint32_t size = 0;
    if (header != heap.headers.end() && (header->second[1] & resident::heap_tag_mask) != 0 &&
        (header->second[1] & resident::heap_keep) == 0) {
        size = header->second[0] - 8 - address;
        block.bytes.resize(size);
        for (std::uint32_t at = 0; at < size;) {
            const auto bytes = owned_span(address + at);
            if (bytes.empty())
                throw field::FieldFormatError("A released heap block holds unowned bytes");
            const auto count = std::min<std::size_t>(bytes.size(), size - at);
            std::copy_n(bytes.begin(), count, block.bytes.begin() + at);
            at += static_cast<std::uint32_t>(count);
        }
        auto &contents = resident.heap_contents;
        for (auto run = contents.lower_bound(address);
             run != contents.end() && run->first < address + size;) {
            if (run->first + run->second.size() > address + size)
                throw field::FieldFormatError("Heap contents cross a released block");
            run = contents.erase(run);
        }
    }
    if (resident::heap_release(heap, block, site) < 0)
        return 0;
    return size;
}

// 80044110 ResetGraph(1): the service at +34, _reset (80046c58), empties the
// request queue and resets the GPU and its DMA channel with interrupts
// masked.
void Program::reset_graph(std::uint32_t mode) {
    auto &gpu = resident.gpu;
    if ((mode & 7U) != 1)
        throw MissingDependency({"reset_graph", 0x80044110, {}, {}}, "symbol:libgpu-reset-graph",
                                false, "Only ResetGraph(1) is recovered");
    if (gpu.debug >= 2)
        throw MissingDependency({"gpu_debug_print", 0x80044244, {}, {}}, "symbol:printf-80019964",
                                false, "libgpu request checking prints are not reconstructed");
    if (gpu.services != 0x80056888)
        throw MissingDependency({"gpu_services", 0x8004426c, {}, {}}, "symbol:gpu-services", false,
                                "Only the observed libgpu reset service is recovered");
    const auto mask_register = resident.interrupts.registers[1];
    gpu.reset_mask = io_latch(mask_register, 2); // SetIntrMask(0)
    io_write(mask_register, 0, 2);
    gpu.tail = 0;
    gpu.head = gpu.tail;
    const auto dpcr = gpu.otc_registers[3];
    io_write(gpu.registers[4], 0x401, 4);
    io_write(dpcr, io_latch(dpcr, 4) | 0x800U, 4);
    io_write(gpu.registers[1], 0x02000000U, 4);
    io_write(gpu.registers[1], 0x01000000U, 4);
    io_write(mask_register, gpu.reset_mask & 0xffffU, 2); // SetIntrMask(saved)
}

// 8001c8dc: run each task's destruction callback until both task lists are
// empty. Main tasks of child sprites end with 80022eb8, their auxiliary
// headers with 8001cb48.
void Program::destroy_sprite_tasks() {
    auto &tasks = resident.sprite_tasks;
    const auto get = [&](std::uint32_t address) { return memory(address); };
    const auto set = [&](std::uint32_t address, std::uint32_t value) {
        set_memory(address, value);
    };
    // 8001cb48: unlink an auxiliary header from the pending list.
    const auto unlink_pending = [&](std::uint32_t node) {
        std::uint32_t previous = 0;
        auto at = tasks.pending_head;
        for (; at != 0; previous = at, at = get(at + 0x18))
            if (at == node) {
                if (previous != 0)
                    set(previous + 0x18, get(at + 0x18));
                else
                    tasks.pending_head = get(at + 0x18);
                if (tasks.next == node)
                    tasks.next = get(node + 0x18);
                break;
            }
        if (at == 0)
            ++tasks.auxiliary_count;
        --tasks.auxiliary_count;
    };
    // 8001cd94: unlink a main task and count it out.
    const auto unlink_main = [&](std::uint32_t node) {
        std::uint32_t previous = 0;
        for (auto at = tasks.head; at != 0; previous = at, at = get(at + 0x18))
            if (at == node) {
                if (previous != 0)
                    set(previous + 0x18, get(at + 0x18));
                else
                    tasks.head = get(at + 0x18);
                if (tasks.next == node)
                    tasks.next = get(node + 0x18);
                break;
            }
        if (s32(get(node + 0x14)) < 0)
            --tasks.active_flags;
        --tasks.primary_count;
    };
    std::size_t guard = 0;
    while (tasks.head != 0) {
        if (++guard > 0x1000)
            throw field::FieldFormatError("The sprite task list does not empty");
        const auto node = tasks.head;
        if (get(node + 0xc) != 0x80022eb8)
            throw MissingDependency({"destroy_sprite_tasks", get(node + 0xc), {}, {}},
                                    "symbol:sprite-task-destruction", false,
                                    "This task destruction callback is not recovered");
        // 80022eb8.
        const auto sprite = get(node + 4);
        const auto renderer = get(sprite + 0x20);
        if (renderer != 0 && get(renderer + 0x2c) != 0) {
            // 80025180: queue the packet block for release with this buffer.
            auto &r = resident;
            const auto entry = r.sprite_arena_cursor;
            r.sprite_arena_cursor = entry + 8;
            if (entry != 0) {
                set(entry, get(renderer + 0x2c));
                set(entry + 4, r.sprite_releases.at(r.sprite_buffer));
                r.sprite_releases.at(r.sprite_buffer) = entry;
            }
        }
        const bool framed = (get(sprite + 0x3c) & 3U) == 1;
        if (framed && get(renderer + 0x34) != 0)
            throw MissingDependency({"destroy_sprite_tasks", 0x80022f2c, {}, {}},
                                    "symbol:sprite-task-model", false,
                                    "Releasing a framed task sprite's model is not recovered");
        if ((get(sprite + 0xac) >> 5 & 1U) != 0)
            throw MissingDependency({"destroy_sprite_tasks", 0x8001ce74, {}, {}},
                                    "symbol:sprite-child-removal", false,
                                    "Removing a task sprite's children is not recovered");
        if ((get(sprite + 0xb0) >> 11 & 1U) != 0)
            throw MissingDependency({"destroy_sprite_tasks", 0x8001d034, {}, {}},
                                    "symbol:sprite-owner-release", false,
                                    "Releasing a task sprite's owned tasks is not recovered");
        if (framed)
            throw MissingDependency({"destroy_sprite_tasks", 0x8001d3f4, {}, {}},
                                    "symbol:sprite-frame-unlink", false,
                                    "Unlinking a framed task sprite is not recovered");
        unlink_main(node);
        unlink_pending(node + 0x1c);
        static_cast<void>(release_owned_block(node, 0x80022fa0));
    }
    while (tasks.pending_head != 0) {
        if (++guard > 0x2000)
            throw field::FieldFormatError("The pending task list does not empty");
        if (get(tasks.pending_head + 0xc) != 0x8001cb48)
            throw MissingDependency({"destroy_sprite_tasks", get(tasks.pending_head + 0xc), {}, {}},
                                    "symbol:sprite-task-destruction", false,
                                    "This pending task callback is not recovered");
        unlink_pending(tasks.pending_head);
    }
}

// 80025044: this buffer's queued image uploads and clears.
void Program::flush_sprite_uploads(FrameServices &services) {
    auto &uploads = resident.sprite_uploads.at(resident.sprite_buffer);
    for (auto rect = uploads; rect != 0; rect = memory(rect + 0xc)) {
        if (const auto source = memory(rect + 8); source != 0)
            load_image_at(services, rect, source);
        else
            clear_image(services, rect, 0);
    }
    uploads = 0;
}

// 8008083c: release event actor `index`: its extensions, its resource
// (+118) and resource block (+120), the actor, the shadow and the sprite
// (800230a8).
void Program::release_actor(std::uint32_t index) {
    auto &state = loaded(*this);
    const auto release = [&](std::uint32_t address, std::uint32_t site) {
        static_cast<void>(release_owned_block(address, site));
    };
    if (s32(index) >= s32(state.reload.event_actors))
        return;
    const auto descriptor = state.reload.descriptor_table + index * 0x5cU;
    const auto actor = memory(descriptor + 0x4c);
    if ((memory(actor + 0x134) & 0x80U) != 0)
        release(memory(actor + 0x110), 0x8008089c);
    if ((memory(actor + 0x12c) & 0x1000U) != 0)
        release(memory(actor + 0x114), 0x800808bc);
    if ((memory(descriptor + 0x58, 2) & 0x2000U) != 0)
        release(memory(actor + 0x118), 0x800808ec);
    if (s16(memory(actor + 0x124, 2)) != -1)
        release(memory(actor + 0x120), 0x80080908);
    release(actor, 0x80080910);
    release(memory(descriptor + 8), 0x8008092c);
    // 800230a8.
    const auto sprite = memory(descriptor + 4);
    if ((memory(sprite + 0xa8) & 1U) == 1)
        if (const auto block = memory(memory(sprite + 0x7c) + 0x18); block != 0)
            release(block, 0x800230e4);
    // 8001d3f4: unlink the sprite from the frame list (renderer +38 links).
    auto &head = resident.sprite.frame_head;
    std::uint32_t previous = 0;
    for (auto at = head; at != 0; previous = at, at = memory(memory(at + 0x20) + 0x38))
        if (at == sprite) {
            const auto next = memory(memory(at + 0x20) + 0x38);
            if (previous != 0)
                set_memory(memory(previous + 0x20) + 0x38, next);
            else
                head = next;
            break;
        }
    release(memory(memory(sprite + 0x20) + 0x2c), 0x80023100);
    release(sprite, 0x80023108);
}

// 800700b0: reset the GPU, destroy the sprite tasks, flush both sprite
// buffers, then release every actor, model instance and loaded component.
// The field's records in heap blocks become raw heap contents first: each
// released block takes every byte it holds.
void Program::field_teardown(FrameServices &services) {
    auto &state = loaded(*this);
    auto &heap = resident.heap;
    auto &reload = state.reload;
    const auto release = [&](std::uint32_t address, std::uint32_t site) {
        static_cast<void>(release_owned_block(address, site));
    };
    {
        auto &contents = resident.heap_contents;
        const auto take = [&](std::uint32_t address, std::span<const std::uint8_t> bytes) {
            if (bytes.empty() || !allocated_block(heap, address))
                return false;
            contents[address] = {bytes.begin(), bytes.end()};
            return true;
        };
        for (auto &actor : state.actors) {
            take(actor.address, actor.storage);
            take(actor.descriptor_address, actor.descriptor);
            take(actor.sprite.sprite.address, actor.sprite.sprite.bytes);
            take(actor.sprite.parts.address, actor.sprite.parts.bytes);
            if (actor.extension_110)
                take(word(actor.storage, 0x110), *actor.extension_110);
            if (actor.extension_114)
                take(word(actor.storage, 0x114), *actor.extension_114);
        }
        state.actors.clear();
        for (auto &piece : state.pieces)
            take(piece.address, piece.descriptor);
        state.pieces.clear();
        if (take(state.collision_address, state.collision_component))
            state.collision_component.clear();
        if (take(state.messages_address, state.messages))
            state.messages.clear();
        std::vector<std::uint32_t> regions;
        for (const auto &[address, region] : state.regions.regions())
            if (take(address, region.bytes))
                regions.push_back(address);
        for (const auto address : regions)
            state.regions.remove(address, 1);
        std::erase_if(state.resources, [&](const field::SpriteAllocation &resource) {
            return take(resource.address, resource.bytes);
        });
        std::erase_if(resident.sprite_tasks.nodes, [&](const field::SpriteAllocation &node) {
            return take(node.address, node.bytes);
        });
        state.event_package = {};
        state.zones.clear();
        state.collision = {};
    }
    reset_graph(1);
    destroy_sprite_tasks();
    for (std::uint32_t i = 1; i <= 2; ++i) {
        flush_sprite_uploads(services);
        draw_sync(services);
        // 800250e0: select a buffer and release the blocks queued for it.
        const auto buffer = (state.draw_buffer + i) & 1U;
        auto &r = resident;
        auto pending = r.sprite_releases.at(buffer);
        r.sprite_buffer = buffer;
        r.sprite_arena_cursor = r.sprite_arenas.at(buffer);
        r.sprite_arena_start = r.sprite_arenas.at(buffer);
        r.sprite_arena_end = r.sprite_arenas.at(buffer) + r.sprite_arena_bytes;
        for (; pending != 0; pending = memory(pending + 4))
            release(memory(pending), 0x8002513c);
        r.sprite_releases.at(buffer) = 0;
        flush_sprite_uploads(services);
        draw_sync(services);
        // 80024fb8: release the arena block; the frame list empties.
        release(r.sprite_arenas[0], 0x80024fc4);
        r.sprite.frame_head = 0;
    }
    for (std::uint32_t i = 0; s32(i) < s32(state.descriptor_count); ++i) {
        release_actor(i);
        const auto descriptor = reload.descriptor_table + i * 0x5cU;
        const auto flags = memory(descriptor + 0x58, 2);
        if ((flags & 0x40U) != 0)
            continue;
        const auto instance = memory(descriptor);
        if ((flags & 0x2000U) != 0)
            throw MissingDependency({"field_teardown", 0x800306d0, {}, {}},
                                    "symbol:field-model-animation", false,
                                    "Releasing an animated model instance is not recovered");
        // 8002cbbc: a model with packets of its own (+0 bit 0) releases them.
        const auto model = memory(instance + 4);
        if ((memory(model, 2) & 1U) != 0) {
            release(memory(model + 0x18), 0x8002cbe4);
            set_memory(model, memory(model, 2) & 0xfffeU, 2);
        }
        release(memory(instance + 8), 0x80070194);
        release(memory(descriptor), 0x800701b0);
    }
    // 800a47d4.
    state.distortion = 0;
    if (reload.w_adb24 != 0)
        throw MissingDependency({"field_teardown", 0x800a47f4, {}, {}},
                                "symbol:field-screen-distortion", false,
                                "Releasing the distortion buffers is not recovered");
    release(reload.descriptor_table, 0x800701e0);
    release(reload.zones_address, 0x800701f0);
    release(state.messages_address, 0x80070200);
    release(reload.events_address, 0x80070210);
    release(state.collision_address, 0x80070220);
    release(reload.geometry_address, 0x80070230);
    release(state.sprite_bundle_address, 0x80070240);
    if (state.h_b00b2 != 0)
        throw MissingDependency({"field_teardown", 0x80027d40, {}, {}}, "symbol:field-80027d40",
                                false, "The release of 80027d40 is not recovered");
    if (state.h_afea8 > 0)
        throw MissingDependency({"field_teardown", 0x8002800c, {}, {}}, "symbol:field-800920d8",
                                false, "Releasing the 800920d8 objects is not recovered");
    if (resident.w_59394 != 0)
        throw MissingDependency({"field_teardown", 0x800374a0, {}, {}}, "symbol:block-80059394",
                                false, "The block 80059394 names is not recovered");
    resident.w_593a0 = 0;
    state.h_afea8 = 0;
    if (state.w_b2264 != 0)
        throw MissingDependency({"field_teardown", 0x801e7fd4, {}, {}}, "symbol:field-801e7fd4",
                                false, "The release of the 801e7fd4 module is not recovered");
    state.w_b2264 = 0;
    // 8003218c(3): release every block of owner tag 3.
    for (auto at = heap.head - 8;
         (heap.headers.at(at)[1] & resident::heap_tag_mask) != resident::heap_end_tag;) {
        const auto next = heap.headers.at(at)[0] - 8;
        if (((heap.headers.at(at)[1] >> 21U) & 15U) == 3)
            release(at + 8, 0x800321ec);
        at = next;
    }
    if (state.w_af278 != 0)
        throw MissingDependency({"field_teardown", 0x800a83c8, {}, {}}, "symbol:field-800a84c0",
                                false, "Releasing the 800a84c0 buffers is not recovered");
}

// 800a663c(1, 1): the five transition quads, each a textured quad over a
// 40h-wide column of the screen copy at (2c0, 100), unrotated at zoom 1000h,
// semi-transparent (abr 1), with a draw mode per buffer.
void Program::reload_transition_setup() {
    auto &reload = loaded(*this).reload;
    reload.zoom = 0x1000;
    reload.angles = {0, 0, 0};
    const auto half = [&](std::uint32_t address, std::int32_t value) {
        set_memory(address, u32(value) & 0xffffU, 2);
    };
    const auto byte = [&](std::uint32_t address, std::uint32_t value) {
        set_memory(address, value & 0xffU, 1);
    };
    for (std::uint32_t i = 0; i < 5; ++i) {
        const auto packet = transition_packets + 0x50U * i;
        const auto corners = transition_corners + 0x20U * i;
        const auto x = s32(0x2c0U + 0x40U * i);
        const auto left = s32(0x20U * i) - 0x50, right = s32(0x20U * i) - 0x30;
        const auto u = s32(0x40U * i);
        byte(packet + 3, 9); // SetPolyFT4 (80043cb0)
        byte(packet + 7, 0x2c);
        half(corners + 2, -0x38);
        half(corners + 0x12, 0x38);
        half(corners + 0x1a, 0x38);
        half(corners + 0, left);
        half(corners + 4, 0);
        half(corners + 8, right);
        half(corners + 0xa, -0x38);
        half(corners + 0xc, 0);
        half(corners + 0x10, left);
        half(corners + 0x14, 0);
        half(corners + 0x18, right);
        half(corners + 0x1c, 0);
        half(packet + 0x1a, 0xdf);
        half(packet + 0x22, 0xdf);
        const auto windows = 0x800b1224U + 0x10U * i;
        half(packet + 8, u);
        half(packet + 0xa, 0);
        half(packet + 0x10, 0x40 + u);
        half(packet + 0x12, 0);
        half(packet + 0x18, u);
        half(packet + 0x20, 0x40 + u);
        half(windows + 4, 0xff);
        half(windows + 6, 0xff);
        half(windows + 0, 0);
        half(windows + 2, 0);
        half(windows + 0xc, 0xff);
        half(windows + 8, 0);
        half(windows + 0xa, 0);
        half(windows + 0xe, 0xff);
        const auto tpage = gpu::texture_page(2, 1, x, 0x100); // GetTPage (80043a1c)
        set_draw_mode(transition_modes + 0x18U * i, tpage, {0, 0, 0xff, 0xff});
        set_draw_mode(transition_modes + 0x18U * i + 0xc, tpage, {0, 0, 0xff, 0xff});
        for (std::uint32_t c = 4; c < 7; ++c)
            byte(packet + c, 0x80);
        byte(packet + 7, memory(packet + 7, 1) | 2U); // SetSemiTrans (80043bfc)
        half(packet + 0x16, s32(tpage));
        byte(packet + 0x1d, 0xdf);
        byte(packet + 0xc, 0);
        byte(packet + 0xd, 0);
        byte(packet + 0x14, 0x40);
        byte(packet + 0x15, 0);
        byte(packet + 0x1c, 0);
        byte(packet + 0x24, 0x40);
        byte(packet + 0x25, 0xdf);
        for (std::uint32_t at = 0; at < 0x28; at += 4)
            set_memory(packet + 0x28 + at, memory(packet + at));
    }
}

// 800a6924: present the buffer: DrawSync, VSync(2), clear to black, the
// environments, then its small table.
void Program::reload_present(FrameServices &services) {
    auto &state = loaded(*this);
    draw_sync(services);
    vertical_sync(services); // VSync(2)
    clear_image(services, state.draw_block, 0);
    put_draw_env(services, state.draw_block);
    put_disp_env(state.draw_block + 0xb8);
    draw_otag(services, state.draw_block + 0x80f0);
}

// 800a5884(1, 1): two presented frames of the transition quads, five
// columns of the screen copy made semi-transparent (800a5774: StoreImage,
// bit 15 of every pixel, LoadImage), then two more frames.
void Program::reload_screen_fade(FrameServices &services, std::uint32_t frame) {
    reload_transition_setup();
    const auto show = [&] {
        for (std::uint32_t i = 0; i < 2; ++i) {
            swap_draw_buffer();
            reload_transition_draw();
            reload_present(services);
        }
    };
    show();
    for (std::uint32_t i = 0; i < 5; ++i) {
        // 800a5774(2c0 + 40i, 100, e0); its rectangle is on its stack.
        const auto rect_address = frame - 0x20U - 0x30U + 0x10U;
        std::array<std::int16_t, 4> rect{static_cast<std::int16_t>(0x2c0 + 0x40 * i), 0x100, 0x40,
                                         0xe0};
        const auto block = load_block(0xe0U << 7U, 1, 0x800a57a0);
        store_image(services, rect, rect_address, block);
        draw_sync(services);
        auto &pixels = resident.heap_contents.at(block);
        for (std::uint32_t at = 0; at < (0xe0U << 5U) * 4U; at += 4) {
            pixels[at + 1] |= 0x80;
            pixels[at + 3] |= 0x80;
        }
        static_cast<void>(load_image(rect, rect_address, block, &services));
        draw_sync(services);
        static_cast<void>(release_owned_block(block, 0x800a5864));
    }
    show();
}

// 8001b044: wait for the disc, then read the party sprite files the map
// needs unless that set is loaded (8001ad4c, 8001aeb8); a return with a
// reload mode (8004f30c) takes 8001b158.
void Program::prepare_party_sprites() {
    auto &load = resident.party_sprite_load;
    // 8001ad1c: poll the disc status, then 80028a60(0).
    while (disc_busy() != 0)
        if (!deliver_interrupt())
            throw MissingDependency({"prepare_party_sprites", 0x8001ad24, {}, {}},
                                    "interrupt:disc-read-completion", false,
                                    "Waiting for the disc needs the interrupt arrivals");
    disc_wait(0);
    if (load.pending == 1)
        throw MissingDependency({"prepare_party_sprites", 0x8001b08c, {}, {}},
                                "symbol:party-sprite-decode", false,
                                "Decoding read-ahead party sprites first is not recovered");
    if (resident.w_4f30c != 0)
        throw MissingDependency({"prepare_party_sprites", 0x8001b158, {}, {}},
                                "symbol:party-sprite-return", false,
                                "The return path of the party sprites (8001b158) is not recovered");
    load.needed = (resident.field_map & 0xc000U) != 0 ? 1 : 0;
    load.pending = 0;
    if (load.loaded != (load.needed == 0 ? 1U : 2U))
        throw MissingDependency(
            {"prepare_party_sprites", load.needed == 0 ? 0x8001ad4cU : 0x8001aeb8U, {}, {}},
            "symbol:party-sprite-read", false, "Reading another party sprite set is not recovered");
}

// 8001b3a8: decode the party sprite files read ahead (8004f374).
void Program::decode_party_sprites() {
    if (resident.party_sprite_load.pending != 0)
        throw MissingDependency({"decode_party_sprites", 0x8001b3d4, {}, {}},
                                "symbol:party-sprite-decode", false,
                                "Decoding read-ahead party sprites is not recovered");
}

// 80070488: start streaming the map's image file (map * 2 + b9 of the
// selected directory 4) into a four-block ring, once.
void Program::start_field_stream() {
    auto &reload = loaded(*this).reload;
    if (reload.stream_pending != 0)
        return;
    reload.stream_pending = 1;
    reload.stream_ring = allocate_disc_ring(4, 1);
    static_cast<void>(
        read_stream(s32(((resident.field_map & 0xfffU) << 1U) + 0xb9U), reload.stream_ring, 0, {}));
}

// 80078c5c: with 800b2344 set, the 100h x 20h VRAM strip at (0, 1e0) gets
// 0c63 added to each nonzero pixel's bits (a StoreImage, LoadImage round
// trip through a 4000h block).
void Program::brighten_text_strip(FrameServices &services, std::uint32_t frame) {
    auto &state = loaded(*this);
    if (state.control_inputs.jump_mode == 0)
        return;
    set_memory(0x800b24b2, 0, 1);
    set_memory(0x800ba5a6, 0, 1);
    const auto block = load_block(0x4000, 0, 0x80078c88);
    std::array<std::int16_t, 4> rect{0, 0x1e0, 0x100, 0x20};
    const auto rect_address = frame + 0x10;
    store_image(services, rect, rect_address, block);
    draw_sync(services);
    auto &bytes = resident.heap_contents.at(block);
    for (std::uint32_t at = 0; at < 0x4000; at += 4) {
        auto pixels = static_cast<std::uint32_t>(bytes[at]) |
                      static_cast<std::uint32_t>(bytes[at + 1]) << 8U |
                      static_cast<std::uint32_t>(bytes[at + 2]) << 16U |
                      static_cast<std::uint32_t>(bytes[at + 3]) << 24U;
        if ((pixels & 0xffffU) != 0)
            pixels |= 0xc63U;
        if ((pixels & 0xffff0000U) != 0)
            pixels |= 0x0c630000U;
        for (std::uint32_t i = 0; i < 4; ++i)
            bytes[at + i] = static_cast<std::uint8_t>(pixels >> (8U * i));
    }
    static_cast<void>(load_image(rect, rect_address, block, &services));
    draw_sync(services);
    static_cast<void>(release_owned_block(block, 0x80078d28));
}

void Program::field_reload(FrameServices &services, std::uint32_t frame,
                           const ProgramObserver &observe) {
    auto &state = loaded(*this);
    auto &reload = state.reload;
    const auto done = [&](std::string_view operation, std::uint32_t address) {
        deliver_stage_arrivals();
        observed(observe, *this, {operation, address, {}, {}}, true);
    };
    // Arrivals since the last frame's exit precede the reload.
    deliver_arrivals(0x80077db4);
    field_reload_teardown(services, observe);
    reload_screen_fade(services, frame);
    done("reload_screen_fade", 0x800a602c);
    prepare_party_sprites();
    done("reload_party_sprites", 0x800a6034);
    decode_party_sprites();
    done("reload_party_decode", 0x800a603c);
    // The reload type and fade length survive the load's reset.
    const auto kept_mode = state.background_mode;
    const auto kept_fade = reload.fade_frames;
    load_field(services, frame - 0xa0, observe); // 80070cc8
    done("reload_load", 0x800a6054);
    start_field_stream(); // 80070488
    done("reload_stream", 0x800a605c);
    if (reload.stream_pending == 1) {
        // Show the transition quads, zooming in, until the stream is read.
        for (;;) {
            if (disc_busy() == 0)
                break;
            swap_draw_buffer();
            reload_transition_draw();
            reload_present(services);
            if (reload.zoom < 0x22c0)
                reload.zoom += 0x20;
        }
        release_music_buffer(reload.stream_ring, 0x800a60c8);
        reload.stream_pending = 0;
        brighten_text_strip(services, frame - 0x20); // 80078c5c
    }
    state.dialogue_gate_afd04 = 1;
    state.background_mode = kept_mode;
    reload.fade_frames = kept_fade;
    if (s32(resident.music.gate) == -1)
        load_music(resident.music.requested); // 80085b20
    done("reload_resume", 0x800a6120);
    field_reload_fade_in(services, observe);
    deliver_arrivals(0x80077db4); // Since the last fade frame's exit.
    done("reload_fade_in", 0x800a63a0);
    field_reload_finish(services, frame);
    done("reload", 0x800a6400);
}

void add_reload_globals(std::vector<OriginalGlobal> &table) {
    const auto entry = [&](std::string name, std::uint32_t address, std::size_t width,
                           bool resident, auto access) {
        table.push_back({std::move(name), address, width, resident,
                         [access](const Program &program) {
                             const auto value = access(const_cast<Program &>(program));
                             using T = std::remove_cvref_t<decltype(value)>;
                             return static_cast<std::uint32_t>(
                                 static_cast<std::make_unsigned_t<T>>(value));
                         },
                         [access](Program &program, std::uint32_t raw) {
                             auto &value = access(program);
                             using T = std::remove_reference_t<decltype(value)>;
                             value = static_cast<T>(static_cast<std::make_unsigned_t<T>>(raw));
                         }});
    };
    const auto add = [&](std::string name, std::uint32_t address, std::size_t width, auto access) {
        entry(std::move(name), address, width, false,
              [access](Program &p) -> auto & { return access(loaded(p).reload); });
    };
    const auto resident = [&](std::string name, std::uint32_t address, std::size_t width,
                              auto access) {
        entry(std::move(name), address, width, true, access);
    };
    resident("w_59394", 0x80059394, 4, [](Program &p) -> auto & { return p.resident.w_59394; });
    resident("w_593a0", 0x800593a0, 4, [](Program &p) -> auto & { return p.resident.w_593a0; });
    add("transition_zoom", 0x800c2684, 4, [](ReloadState &r) -> auto & { return r.zoom; });
    for (std::uint32_t i = 0; i < 3; ++i)
        add("transition_angles", 0x800b00b8 + 2 * i, 2,
            [i](ReloadState &r) -> auto & { return r.angles[i]; });
    add("fade_frames", 0x800afd14, 4, [](ReloadState &r) -> auto & { return r.fade_frames; });
    add("stream_pending", 0x800adb60, 4, [](ReloadState &r) -> auto & { return r.stream_pending; });
    add("stream_ring", 0x800adc14, 4, [](ReloadState &r) -> auto & { return r.stream_ring; });
    for (std::uint32_t i = 0; i < 4; ++i)
        add("vram_rect", 0x800afc28 + 2 * i, 2,
            [i](ReloadState &r) -> auto & { return r.vram_rect[i]; });
    add("vram_save", 0x800afc70, 4, [](ReloadState &r) -> auto & { return r.vram_save_address; });
    for (std::uint32_t i = 0; i < 64; ++i)
        add("particle_ids", 0x800b0108 + 2 * i, 2,
            [i](ReloadState &r) -> auto & { return r.particle_ids[i]; });
    add("effects_kept", 0x800b233c, 2, [](ReloadState &r) -> auto & { return r.effects_kept; });
    add("descriptor_table", 0x800afb10, 4,
        [](ReloadState &r) -> auto & { return r.descriptor_table; });
    add("event_actors", 0x800adbfc, 4, [](ReloadState &r) -> auto & { return r.event_actors; });
    add("zones_address", 0x800adbf4, 4, [](ReloadState &r) -> auto & { return r.zones_address; });
    add("events_address", 0x800adbf8, 4, [](ReloadState &r) -> auto & { return r.events_address; });
    add("geometry_address", 0x800afb14, 4,
        [](ReloadState &r) -> auto & { return r.geometry_address; });
    add("w_adb24", 0x800adb24, 4, [](ReloadState &r) -> auto & { return r.w_adb24; });
    add("event_bytecode", 0x800adc00, 4, [](ReloadState &r) -> auto & { return r.event_bytecode; });
    add("collision_attributes", 0x800afb20, 4,
        [](ReloadState &r) -> auto & { return r.collision_attributes; });
    for (std::uint32_t i = 0; i < 4; ++i) {
        add("collision_triangles", 0x800afb24 + 4 * i, 4,
            [i](ReloadState &r) -> auto & { return r.collision_triangles[i]; });
        add("collision_vertices", 0x800afb34 + 4 * i, 4,
            [i](ReloadState &r) -> auto & { return r.collision_vertices[i]; });
    }
    add("attribute_words", 0x800afd10, 4,
        [](ReloadState &r) -> auto & { return r.attribute_words; });
    for (std::uint32_t i = 0; i < 10; ++i)
        resident("light_colors", 0x80059f84 + 2 * i, 2,
                 [i](Program &p) -> auto & { return p.resident.light_colors[i]; });
    for (std::uint32_t i = 0; i < 3; ++i)
        resident("window_color", 0x800594d4 + i, 1,
                 [i](Program &p) -> auto & { return p.resident.window_color[i]; });
    entry("collision_address", 0x800afb18, 4, false,
          [](Program &p) -> auto & { return loaded(p).collision_address; });
    entry("sprite_bundle_address", 0x800afb1c, 4, false,
          [](Program &p) -> auto & { return loaded(p).sprite_bundle_address; });
    for (std::uint32_t i = 0; i < 0x14; ++i)
        resident("gpu_move_packet", 0x80056978 + i, 1,
                 [i](Program &p) -> auto & { return p.resident.gpu.move_packet[i]; });
    resident("gpu_reset_mask", 0x800569e4, 4,
             [](Program &p) -> auto & { return p.resident.gpu.reset_mask; });
    resident("party_sprites_pending", 0x8004f374, 4,
             [](Program &p) -> auto & { return p.resident.party_sprite_load.pending; });
    resident("party_sprites_loaded", 0x8004f31c, 4,
             [](Program &p) -> auto & { return p.resident.party_sprite_load.loaded; });
    resident("party_sprites_needed", 0x8004f320, 4,
             [](Program &p) -> auto & { return p.resident.party_sprite_load.needed; });
    resident("gpu_tim_cursor", 0x8005a37c, 4,
             [](Program &p) -> auto & { return p.resident.gpu.tim_cursor; });
    for (std::uint32_t i = 0; i < 2; ++i)
        resident("image_upload", 0x800592f0 + 4 * i, 4,
                 [i](Program &p) -> auto & { return p.resident.image_upload[i]; });
}

} // namespace xem::reconstruction
