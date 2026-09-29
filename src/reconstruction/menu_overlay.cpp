// Menu overlay 82f84a24... (801c5000): menu-mode memory, callee frames and
// the resident services the overlay calls.
#include "xem/reconstruction/menu_overlay.hpp"

#include "xem/reconstruction/menu_save.hpp"
#include "xem/reconstruction/resident_text.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction::menu {
namespace {
std::uint32_t ram(std::uint32_t address) {
    if ((address & 0x1fffffffU) >= 0x200000U)
        throw MenuError("Menu code reaches an address outside main RAM");
    return 0x80000000U | (address & 0x1fffffU);
}
struct LendGpuCaller {
    std::optional<FrameCallAbi> &slot;
    std::optional<FrameCallAbi> previous;
    ~LendGpuCaller() { slot = previous; }
};
} // namespace

Overlay::Overlay(Program &owner, FrameServices &frame_services, std::uint32_t stack_pointer,
                 CardBios *bios, std::span<std::uint8_t> qualified_stack)
    : program(owner), services(frame_services), card(bios), sp_(stack_pointer),
      entry_sp_(stack_pointer), qualified_stack_(qualified_stack) {
    if (qualified_stack_.size() > stack_bytes || qualified_stack_.size() > entry_sp_ ||
        (qualified_stack_.size() & 3U) != 0)
        throw MenuError("A qualified menu stack must fit the bounded entry stack window");
    if (!program.menu)
        throw MenuError("The menu overlay requires menu memory");
    if (!qualified_stack_.empty() && !program.menu->stack.empty())
        throw MenuError("A borrowed menu stack cannot overlap a separate menu stack owner");
    if (program.menu->resident_memory || program.menu->resident_tail)
        throw MenuError("Menu memory already has a borrowed resident owner");
    auto &resident = program.resident;
    if (resident.game_data.size() != game_data_bytes)
        throw MenuError("The menu overlay requires the game data");
    game_start_ = resident.game_state;
    auto &regions = program.menu->regions;
    for (const auto &[address, bytes] : regions)
        if (address < game_start_ + game_data_bytes && game_start_ < address + bytes.size())
            throw MenuError("Game data overlaps menu memory");
    regions.emplace(game_start_, std::move(resident.game_data));
    services_at_start_ = services_left();
    program.menu->stack_base =
        entry_sp_ - static_cast<std::uint32_t>(qualified_stack_.empty() ? stack_bytes
                                                                        : qualified_stack_.size());
    if (qualified_stack_.empty())
        program.menu->stack.assign(stack_bytes, 0);
    program.menu->resident_memory = this;
    program.menu->resident_tail = [&](std::uint32_t address) {
        return program.record_block(address);
    };
}

std::size_t Overlay::services_left() const {
    return services.hblank_counts.size() + services.vblank_waits.size() +
           services.vblank_counts.size() + services.alarm_polls.size() +
           services.gpu_status.size() + services.dma_busy.size() + services.interrupt_masks.size() +
           services.gpu_info.size() + program.resident.gpu.vram_reads.size();
}

std::uint32_t Overlay::events() const {
    return positions_ + static_cast<std::uint32_t>(services_at_start_ - services_left());
}

void Overlay::catch_up() {
    const auto passed = events();
    using Kind = PlatformInput::Kind;
    const auto &inputs = program.resident.platform;
    while (!inputs.empty() &&
           (inputs.front().kind == Kind::interrupt || inputs.front().kind == Kind::tick) &&
           inputs.front().site <= passed)
        static_cast<void>(program.deliver_interrupt());
}

void Overlay::pass_position(bool deliver) {
    ++positions_;
    if (deliver)
        catch_up();
}

void Overlay::entering_sound() {
    // The qualified hook is 801c8574's entry, before 801c85a8 starts the
    // effect. Deliver earlier arrivals first; later arrivals must see the
    // voice setup that 8003b644 brackets with DisableEvent/EnableEvent.
    catch_up();
    if (sound_positions)
        pass_position(false);
}

Overlay::~Overlay() {
    auto &regions = program.menu->regions;
    if (auto found = regions.find(game_start_); found != regions.end())
        program.resident.game_data = std::move(regions.extract(found).mapped());
    program.menu->stack.clear();
    program.menu->resident_memory = nullptr;
    program.menu->resident_tail = {};
}

void Overlay::missing(std::string_view operation, std::uint32_t address, const char *id,
                      const char *reason) {
    throw MissingDependency({operation, address, {}, {}}, id, false, reason);
}

// ---- Memory ---------------------------------------------------------------

void Overlay::set_stack_image(std::span<const std::uint8_t> image) {
    if (!qualified_stack_.empty())
        throw MenuError("A connected menu stack cannot be replaced at its menu boundary");
    auto &stack = program.menu->stack;
    if (image.size() != stack.size())
        throw MenuError("The stack image must cover the modeled stack");
    std::ranges::copy(image, stack.begin());
}

void Overlay::set_saved_registers(std::span<const std::uint32_t> registers) {
    if (registers.size() != saved_registers_.size())
        throw MenuError("The saved-register image must contain S0 through S7");
    std::ranges::copy(registers, saved_registers_.begin());
}

std::uint8_t *Overlay::stack_byte(std::uint32_t address) const {
    const auto base = program.menu->stack_base;
    if (address < base || address >= entry_sp_)
        return nullptr;
    const auto bytes = qualified_stack_.empty() ? std::span(program.menu->stack) : qualified_stack_;
    return bytes.data() + (address - base);
}

std::uint8_t *Overlay::menu_byte(std::uint32_t address, std::uint32_t width) const {
    auto &regions = program.menu->regions;
    auto found = regions.upper_bound(address);
    if (found == regions.begin())
        return nullptr;
    --found;
    if (address - found->first + std::uint64_t{width} > found->second.size())
        return nullptr;
    return found->second.data() + (address - found->first);
}

std::uint32_t Overlay::read(std::uint32_t address, std::uint32_t width) const {
    address = ram(address);
    if (const auto *bytes = menu_byte(address, width)) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i)
            value |= static_cast<std::uint32_t>(bytes[i]) << (8U * i);
        return value;
    }
    if (stack_byte(address) != nullptr) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i) {
            const auto *byte = stack_byte(address + i);
            if (byte == nullptr)
                throw MenuError("A stack read crosses the entry stack pointer");
            value |= static_cast<std::uint32_t>(*byte) << (8U * i);
        }
        return value;
    }
    return program.memory(address, width);
}

void Overlay::write(std::uint32_t address, std::uint32_t value, std::uint32_t width) {
    address = ram(address);
    if (auto *bytes = menu_byte(address, width)) {
        for (std::uint32_t i = 0; i < width; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
        return;
    }
    if (stack_byte(address) != nullptr) {
        for (std::uint32_t i = 0; i < width; ++i) {
            auto *byte = stack_byte(address + i);
            if (byte == nullptr)
                throw MenuError("A stack write crosses the entry stack pointer");
            *byte = static_cast<std::uint8_t>(value >> (8U * i));
        }
        return;
    }
    program.set_memory(address, value, width);
}

std::uint32_t Overlay::u8(std::uint32_t address) const { return read(address, 1); }
std::uint32_t Overlay::u16(std::uint32_t address) const { return read(address, 2); }
std::uint32_t Overlay::u32(std::uint32_t address) const { return read(address, 4); }
std::int32_t Overlay::s8(std::uint32_t address) const {
    return static_cast<std::int8_t>(read(address, 1));
}
std::int32_t Overlay::s16(std::uint32_t address) const {
    return static_cast<std::int16_t>(read(address, 2));
}
void Overlay::put8(std::uint32_t address, std::uint32_t value) { write(address, value, 1); }
void Overlay::put16(std::uint32_t address, std::uint32_t value) { write(address, value, 2); }
void Overlay::put32(std::uint32_t address, std::uint32_t value) { write(address, value, 4); }

Overlay::StackFrame::StackFrame(Overlay &overlay, std::uint32_t bytes)
    : overlay_(overlay), saved_sp_(overlay.sp_), saved_size_(overlay.frame_size_),
      saved_registers_(overlay.saved_registers_) {
    if (bytes > overlay_.sp_ || overlay_.sp_ - bytes < overlay_.program.menu->stack_base)
        throw MenuError("Menu callee frames exceed the qualified stack");
    overlay_.sp_ -= bytes;
    overlay_.frame_size_ = bytes;
}

Overlay::StackFrame::~StackFrame() {
    overlay_.sp_ = saved_sp_;
    overlay_.frame_size_ = saved_size_;
    overlay_.saved_registers_ = saved_registers_;
}

Overlay::Frame Overlay::frame(std::uint32_t bytes) const {
    if (bytes > frame_size_)
        throw MenuError("Menu locals exceed the function's original frame");
    return {sp_};
}

// ---- Resident services -----------------------------------------------------

std::uint32_t Overlay::allocate(std::uint32_t size, std::uint32_t mode, std::uint32_t site) {
    // 80031bdc..80031c20: the heap frame and the caller/register stores it
    // retains. The original caller word is reduced to its 25-bit word offset.
    const auto stack_frame = enter(0x28);
    const auto locals = frame(0x28);
    put32(locals[0x18], saved_registers_[0]);
    put32(locals[0x1c], saved_registers_[1]);
    put32(locals[0x20], site + 8);
    put32(locals[0x10], (site & 0x1ffffffU) >> 2U);
    auto block = resident::heap_allocate(program.resident.heap, size, mode, site);
    if (!block)
        missing("heap_allocate", site, "symbol:heap-fatal-80019acc",
                "A failed menu allocation reaches the resident fatal handler");
    const auto address = block->address;
    program.menu->regions[address] = std::move(block->bytes);
    return address;
}

void Overlay::release(std::uint32_t block, std::uint32_t site) {
    // 800320e8..800320f4: the 20h frame and delay-slot RA store execute
    // for both null and non-null pointers. The caller supplies its JAL PC.
    const auto stack_frame = enter(0x20);
    const auto locals = frame(0x20);
    put32(locals[0x18], site + 8);
    if (block == 0 && program.resident.heap.quiet == 0)
        put32(locals[0x10], site + 8); // 80032108..80032110, before the fatal handler.
    auto &regions = program.menu->regions;
    resident::HeapBlock taken{block, {}};
    const auto found = regions.find(block);
    if (block != 0) {
        if (found == regions.end()) {
            const auto header = program.resident.heap.headers.find(block - 8);
            if (header != program.resident.heap.headers.end() &&
                (header->second[1] & resident::heap_tag_mask) != 0)
                throw MenuError("The menu releases a live block it does not own");
            // 801da518 repeats data-set 10's releases. 800320e8 rewrites
            // the flags even after the bytes have returned to the heap;
            // its stale-pointer path also validates heap-held ownership.
            static_cast<void>(resident::heap_release(program.resident.heap, taken, site));
            return;
        }
        taken.bytes = std::move(found->second);
        regions.erase(found);
    }
    if (resident::heap_release(program.resident.heap, taken, site) < 0 && block != 0)
        regions[block] = std::move(taken.bytes); // A kept block stays with the menu.
}

std::uint32_t Overlay::bzero(std::uint32_t address, std::int32_t count) {
    if (address == 0 || count <= 0)
        return 0;
    for (auto at = address; count > 0; --count)
        put8(at++, 0);
    return address;
}

std::uint32_t Overlay::memcpy(std::uint32_t to, std::uint32_t from, std::int32_t count) {
    if (to == 0)
        return 0;
    for (auto at = to; count > 0; --count)
        put8(at++, u8(from++));
    return to;
}

// V0 is the count when nothing is copied, else the last byte copied.
std::uint32_t Overlay::memmove(std::uint32_t to, std::uint32_t from, std::int32_t count) {
    auto result = static_cast<std::uint32_t>(count);
    if (to < from) {
        for (; count > 0; --count)
            put8(to++, result = u8(from++));
    } else {
        for (auto i = count; i-- > 0;)
            put8(to + static_cast<std::uint32_t>(i),
                 result = u8(from + static_cast<std::uint32_t>(i)));
    }
    return result;
}

std::uint32_t Overlay::strlen(std::uint32_t text) const {
    if (text == 0)
        return 0;
    std::uint32_t length = 0;
    while (u8(text + length) != 0)
        ++length;
    return length;
}

std::uint32_t Overlay::strcpy(std::uint32_t to, std::uint32_t from) {
    if (to == 0 || from == 0)
        return 0;
    auto at = to;
    std::uint32_t byte = 0;
    do {
        byte = u8(from++);
        put8(at++, byte);
    } while (byte != 0);
    return to;
}

std::uint32_t Overlay::strcat(std::uint32_t to, std::uint32_t from) {
    if (to == 0 || from == 0)
        return 0;
    if (to + strlen(to) == from + strlen(from))
        return 0;
    auto at = to + strlen(to);
    std::uint32_t byte = 0;
    do {
        byte = u8(from++);
        put8(at++, byte);
    } while (byte != 0);
    return to;
}

// 8001bd40: ff when `low` is ff, 0 when `high` is 0, `low` when they are
// equal, otherwise low + rand() % (high - low + 1) (a byte; the full rand byte
// for spans of ff or more). rand is 8003fa38.
std::uint32_t Overlay::random_range(std::uint32_t low, std::uint32_t high) {
    const auto first = low & 0xffU;
    const auto last = high & 0xffU;
    if (first == 0xff)
        return 0xff;
    if (last == 0)
        return 0;
    if (first == last)
        return first;
    auto &seed = program.resident.random_seed;
    seed = seed * 0x41c64e6dU + 12345U;
    const auto value = (seed >> 16U) & 0x7fffU & 0xffU;
    const auto span = static_cast<std::int32_t>(last) - static_cast<std::int32_t>(first);
    if (span >= 0xff)
        return value;
    // Signed division; a zero divisor leaves the dividend in HI.
    const auto divisor = span + 1;
    const auto remainder = divisor == 0 ? static_cast<std::int32_t>(value)
                                        : static_cast<std::int32_t>(value) % divisor;
    return (low + static_cast<std::uint32_t>(remainder)) & 0xffU;
}

// ---- libgpu ------------------------------------------------------------------

std::uint32_t Overlay::get_tpage(std::uint32_t tp, std::uint32_t abr, std::int32_t x,
                                 std::int32_t y) {
    return gpu::texture_page(tp, abr, x, y);
}

std::uint32_t Overlay::get_clut(std::int32_t x, std::int32_t y) {
    return ((static_cast<std::uint32_t>(y) << 6U) |
            ((static_cast<std::uint32_t>(x >> 4)) & 0x3fU)) &
           0xffffU;
}

void Overlay::add_prim(std::uint32_t table_entry, std::uint32_t packet) {
    put32(packet, (u32(packet) & 0xff000000U) | (u32(table_entry) & 0xffffffU));
    put32(table_entry, (u32(table_entry) & 0xff000000U) | (packet & 0xffffffU));
}

void Overlay::set_semi_trans(std::uint32_t packet, std::uint32_t on) {
    put8(packet + 7, on != 0 ? u8(packet + 7) | 2U : u8(packet + 7) & 0xfdU);
}

void Overlay::set_shade_tex(std::uint32_t packet, std::uint32_t on) {
    put8(packet + 7, on != 0 ? u8(packet + 7) | 1U : u8(packet + 7) & 0xfeU);
}

void Overlay::set_poly_f4(std::uint32_t packet) {
    put8(packet + 3, 5);
    put8(packet + 7, 0x28);
}

void Overlay::set_poly_ft4(std::uint32_t packet) {
    put8(packet + 3, 9);
    put8(packet + 7, 0x2c);
}

void Overlay::set_poly_g4(std::uint32_t packet) {
    put8(packet + 3, 8);
    put8(packet + 7, 0x38);
}

void Overlay::set_line_f3(std::uint32_t packet) {
    put8(packet + 3, 5);
    put8(packet + 7, 0x48);
    put32(packet + 0x14, 0x55555555U);
}

// LoadImage runs at once when the queue is idle and clamps the caller's
// rectangle in place (as Program::load_image_at); the rectangle may be on a
// callee frame.
void Overlay::load_image(std::uint32_t rect, std::uint32_t data) {
    catch_up();
    std::array<std::int16_t, 4> area{};
    for (std::uint32_t i = 0; i < 4; ++i)
        area[i] = static_cast<std::int16_t>(s16(rect + 2 * i));
    static_cast<void>(program.load_image(area, rect, data, &services));
    put16(rect + 4, static_cast<std::uint16_t>(area[2]));
    put16(rect + 6, static_cast<std::uint16_t>(area[3]));
}
void Overlay::draw_sync() {
    catch_up();
    program.draw_sync(services, [this] { catch_up(); });
}
void Overlay::vsync() {
    catch_up();
    program.vertical_sync(services);
}
void Overlay::put_draw_env(std::uint32_t environment) {
    catch_up();
    program.put_draw_env(services, environment);
}
void Overlay::put_disp_env(std::uint32_t environment) { program.put_disp_env(environment); }
void Overlay::move_image(std::uint32_t rect, std::int32_t x, std::int32_t y) {
    // 8004495c..80044980: MoveImage preserves its caller's S0..S2,
    // then keeps the rectangle, y and x in those registers.
    const auto stack_frame = enter(0x20);
    const auto locals = frame(0x20);
    for (std::uint32_t i = 0; i < 3; ++i)
        put32(locals[0x10 + i * 4], saved_registers_[i]);
    saved_registers_[0] = rect;
    saved_registers_[1] = static_cast<std::uint32_t>(y);
    saved_registers_[2] = static_cast<std::uint32_t>(x);
    catch_up();
    const std::array<std::int16_t, 4> area{
        static_cast<std::int16_t>(s16(rect)), static_cast<std::int16_t>(s16(rect + 2)),
        static_cast<std::int16_t>(s16(rect + 4)), static_cast<std::int16_t>(s16(rect + 6))};
    LendGpuCaller lend{program.gpu_enqueue_caller_, program.gpu_enqueue_caller_};
    program.gpu_enqueue_caller_ = FrameCallAbi{sp_, saved_registers_, 0x80044a04};
    program.move_image(services, area, x, y);
}
void Overlay::draw_otag(std::uint32_t table) {
    // 80044bd8..80044be0: DrawOTag preserves S0 and uses it for the table.
    const auto stack_frame = enter(0x18);
    put32(frame(0x18)[0x10], saved_registers_[0]);
    saved_registers_[0] = table;
    catch_up();
    LendGpuCaller lend{program.gpu_enqueue_caller_, program.gpu_enqueue_caller_};
    program.gpu_enqueue_caller_ = FrameCallAbi{sp_, saved_registers_, 0x80044c30};
    program.draw_otag(services, table);
}
void Overlay::clear_otag_r(std::uint32_t table, std::uint32_t count) {
    catch_up(); // its DMA busy reads are platform inputs after earlier arrivals
    const auto stack_frame = enter(0x20);
    program.clear_ordering_table(table, count);
}

void Overlay::set_draw_mode(std::uint32_t packet, std::uint32_t dfe, std::uint32_t dtd,
                            std::uint32_t tpage, std::uint32_t window) {
    put8(packet + 3, 2);
    put32(packet + 4,
          gpu::draw_mode(program.resident.gpu_type, dfe != 0, dtd != 0, tpage & 0xffffU));
    std::array<std::int16_t, 4> area{};
    if (window != 0)
        for (std::uint32_t i = 0; i < 4; ++i)
            area[i] = static_cast<std::int16_t>(s16(window + 2 * i));
    put32(packet + 8, gpu::texture_window(window != 0 ? &area : nullptr));
}

void Overlay::open_tim(std::uint32_t data) { program.resident.gpu.tim_cursor = data; }

// ReadTIM through 80047518: a TIM starts with word 10h; flag 8 in the mode
// word announces a CLUT block before the image block. Each block is its byte
// size, then the rectangle and the words. With request checking at level 2
// libgpu prints the header (not reconstructed).
std::uint32_t Overlay::read_tim(std::uint32_t image) {
    auto &cursor = program.resident.gpu.tim_cursor;
    auto at = cursor;
    if (u32(at) != 0x10)
        return 0;
    at += 4;
    put32(image, u32(at));
    at += 4;
    if (program.resident.gpu.debug == 2)
        missing("read_tim", 0x80047570, "symbol:printf-80019964",
                "libgpu TIM messages are not reconstructed");
    std::uint32_t clut_words = 0;
    if ((u32(image) & 8U) != 0) {
        put32(image + 4, at + 4);
        put32(image + 8, at + 12);
        clut_words = u32(at) >> 2U;
        at += clut_words * 4U;
    } else {
        put32(image + 4, 0);
        put32(image + 8, 0);
    }
    put32(image + 12, at + 4);
    put32(image + 16, at + 12);
    cursor += (clut_words + (u32(at) >> 2U) + 2U) * 4U;
    return image;
}

std::int32_t Overlay::select_directory(std::uint32_t base, std::uint32_t index) {
    return program.select_directory(base, index);
}

std::uint32_t Overlay::file_words(std::uint32_t file) { return program.file_words(file); }

std::int32_t Overlay::read_file(std::uint32_t file, std::uint32_t destination, std::uint32_t offset,
                                std::uint32_t mode) {
    return program.read_file(static_cast<std::int32_t>(file), destination, offset, mode);
}

void Overlay::disc_wait(std::uint32_t once) {
    const auto stack_frame = enter(0x18); // 80028a60
    program.disc_wait(once);
    // The final 800286cc call at 80028a7c occurs on either path, after
    // the wait. Its S0 and return-PC stores remain in the reused stack.
    const auto poll_frame = enter(0x18);
    put32(frame(0x18)[0x10], saved_registers_[0]); // 800286d0
    put32(frame(0x18)[0x14], 0x80028a84);          // 800286e4
}

std::uint32_t Overlay::current_disc() const { return program.current_disc(); }

// 80033698: the 20h-entry palette row at 80050190 goes to VRAM (x, y) from a
// rectangle on 80033698's stack; the even and odd text CLUTs are (x, y) and
// (x + 10h, y).
void Overlay::load_text_palette(std::uint32_t x, std::uint32_t y) {
    const auto stack_frame = enter(0x28);
    auto rect = frame(0x28);
    const auto area = rect[0x10];
    put16(area, x);
    put16(area + 2, y);
    put16(area + 4, 0x20);
    put16(area + 6, 1);
    load_image(area, 0x80050190);
    program.resident.text_cluts = {
        static_cast<std::uint16_t>(
            get_clut(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y))),
        static_cast<std::uint16_t>(
            get_clut(static_cast<std::int16_t>(x + 0x10U), static_cast<std::int16_t>(y)))};
}

std::uint32_t Overlay::layout_text(std::uint32_t text, std::uint32_t image, std::uint32_t width,
                                   std::uint32_t plane) {
    return resident::layout_text_line(*this, text, image, width, plane, [&](std::uint32_t window) {
        program.dialogue_glyphs(window);
    });
}

void Overlay::decode_text(std::uint32_t codes, std::uint32_t text, std::uint32_t count) {
    std::vector<std::uint8_t> words(std::size_t{count} * 2U);
    for (std::uint32_t i = 0; i < words.size(); ++i)
        words[i] = static_cast<std::uint8_t>(u8(codes + i));
    const auto bytes = menu::decode_text(name_codec(*program.menu), words, count);
    for (std::uint32_t i = 0; i < bytes.size(); ++i)
        put8(text + i, bytes[i]);
}

void Overlay::play_sound(std::uint32_t id) {
    entering_sound();
    if (u8(at(state_sound)) == 0)
        return;
    resident::start_bank_effect(program.resident.sound, u32(at(state_effect_bank)), id);
}

std::uint32_t Overlay::pad_kind(std::uint32_t port) const {
    const auto &buffer = program.resident.pad.buffers.at(port);
    if (buffer[0] == 0xff)
        return 0;
    switch (buffer[1] & 0xf0U) {
    case 0x40:
        return 1;
    case 0x10:
        return 2;
    case 0x50:
        return 3;
    case 0x70:
        return 4;
    default:
        return 0xffffffffU;
    }
}

} // namespace xem::reconstruction::menu
